#include "zircon_render_pass_library_manager.h"

#include "zircon_renderer.h"

#include <cstring>

namespace
{
	/// the renderer's seam wrappers: free FUNCTIONS (no state of their
	/// own — the no-static-storage rule) that forward to the owning
	/// manager instance passed as p_owner. The renderer installs them
	/// bound to the manager; every resolved export, counter and watcher
	/// field lives as a member of that instance
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
	zircon_create_render_pass_via_manager(
		void* p_owner, const char* p_pass_name)
	{
		KOTEK_ASSERT(p_owner,
			"the pass seam owner must be the pass library manager that "
			"installed the callback");

		if (!p_owner)
		{
			return nullptr;
		}

		return static_cast<zircon_render_pass_library_manager*>(p_owner)
			->create_pass(p_pass_name);
	}

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	void zircon_destroy_render_pass_via_manager(void* p_owner,
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass)
	{
		KOTEK_ASSERT(p_owner,
			"the pass seam owner must be the pass library manager that "
			"installed the callback");

		if (!p_owner)
		{
			return;
		}

		static_cast<zircon_render_pass_library_manager*>(p_owner)
			->destroy_pass(p_pass);
	}

	/// the no-assert load/probe primitives of the reload path (task Z3
	/// P3b): dll::shared_library::load/get KOTEK_ASSERT on failure,
	/// which would abort a Debug session on a half-written candidate —
	/// the reload contract instead logs and keeps the OLD library
	/// running. The returned handle is stored into the shared_library
	/// members (p_lib is the public owning handle), so unload/destructor
	/// semantics stay uniform
	void* zircon_load_pass_library_gracefully(
		const char* p_path) noexcept
	{
#ifdef KOTEK_USE_PLATFORM_WINDOWS
		return reinterpret_cast<void*>(LoadLibraryA(p_path));
#else
		// the hot-reload dev feature is Windows-only today
		(void)p_path;
		return nullptr;
#endif
	}

	void* zircon_get_pass_library_export_gracefully(
		void* p_library, const char* p_export_name) noexcept
	{
#ifdef KOTEK_USE_PLATFORM_WINDOWS
		if (!p_library)
		{
			return nullptr;
		}

		return reinterpret_cast<void*>(GetProcAddress(
			reinterpret_cast<HMODULE>(p_library), p_export_name));
#else
		(void)p_library;
		(void)p_export_name;
		return nullptr;
#endif
	}

	bool zircon_delete_pass_library_shadow(const char* p_path) noexcept
	{
#ifdef KOTEK_USE_PLATFORM_WINDOWS
		return DeleteFileA(p_path) != 0;
#else
		(void)p_path;
		return false;
#endif
	}

	/// the watcher's probe: (last-write-time, size) of the REAL pass
	/// library in one GetFileAttributesEx — false while the file is
	/// absent/inaccessible (a clean rebuild deletes before relinking)
	bool zircon_read_pass_library_file_state(
		kotek::uint64_t& out_last_write_time,
		kotek::uint64_t& out_file_size) noexcept
	{
#ifdef KOTEK_USE_PLATFORM_WINDOWS
		WIN32_FILE_ATTRIBUTE_DATA data;

		if (!GetFileAttributesExA(
				ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
				GetFileExInfoStandard, &data))
		{
			return false;
		}

		out_last_write_time =
			(static_cast<kotek::uint64_t>(
				 data.ftLastWriteTime.dwHighDateTime)
				<< 32) |
			data.ftLastWriteTime.dwLowDateTime;
		out_file_size =
			(static_cast<kotek::uint64_t>(data.nFileSizeHigh) << 32) |
			data.nFileSizeLow;

		return true;
#else
		(void)out_last_write_time;
		(void)out_file_size;
		return false;
#endif
	}

	void zircon_append_unsigned_to_shadow_path(
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>& path,
		unsigned value) noexcept
	{
		// 10 digits hold any 32-bit unsigned; no snprintf in src/ (house
		// container discipline)
		char digits[10];
		kotek::uint8_t digit_count = 0;

		do
		{
			digits[digit_count] = static_cast<char>('0' + (value % 10));
			value /= 10;
			++digit_count;
		} while (value != 0);

		while (digit_count > 0)
		{
			--digit_count;
			path.push_back(digits[digit_count]);
		}
	}

	/// the directory prefix of the real pass library path (everything up
	/// to and including the last slash), written into out_prefix
	void zircon_pass_library_dir_prefix(
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>&
			out_prefix) noexcept
	{
		out_prefix.clear();

		const char* p_real_path =
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH;

		const char* p_last_slash = nullptr;

		for (const char* p_cursor = p_real_path; *p_cursor != '\0';
		     ++p_cursor)
		{
			if (*p_cursor == '/' || *p_cursor == '\\')
			{
				p_last_slash = p_cursor;
			}
		}

		if (p_last_slash)
		{
			out_prefix.assign(p_real_path,
				static_cast<kotek::ktk::size_t>(
					p_last_slash - p_real_path + 1));
		}
	}

	/// builds "passes/.shadow_<pid>_<counter>.dll" — the name the real
	/// DLL is copied to before EVERY load (init and each reload), so the
	/// real file stays writable for the linker while the engine runs
	void zircon_build_pass_library_shadow_path(unsigned shadow_counter,
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>&
			out_path) noexcept
	{
		zircon_pass_library_dir_prefix(out_path);

		out_path.append(".shadow_");

#ifdef KOTEK_USE_PLATFORM_WINDOWS
		zircon_append_unsigned_to_shadow_path(
			out_path, static_cast<unsigned>(GetCurrentProcessId()));
#else
		out_path.push_back('0');
#endif

		out_path.push_back('_');
		zircon_append_unsigned_to_shadow_path(out_path, shadow_counter);
		out_path.append(".dll");
	}

	/// deletes the shadow leftovers of previous runs at init. A shadow
	/// still loaded by a LIVE process resists deletion (a loaded image
	/// can't be deleted on Windows), so the blanket sweep is safe
	void zircon_delete_stale_pass_library_shadows(void) noexcept
	{
#ifdef KOTEK_USE_PLATFORM_WINDOWS
		kotek::static_cstring_t<
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>
			search_pattern;

		zircon_pass_library_dir_prefix(search_pattern);
		search_pattern.append(".shadow_*.dll");

		WIN32_FIND_DATAA find_data;

		HANDLE h_search =
			FindFirstFileA(search_pattern.c_str(), &find_data);

		if (h_search == INVALID_HANDLE_VALUE)
		{
			return;
		}

		unsigned deleted_count = 0;

		do
		{
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
				0)
			{
				continue;
			}

			kotek::static_cstring_t<
				ZIRCON_DEF_RENDER_PASS_LIBRARY_SHADOW_PATH_MAX_LENGTH>
				shadow_path;

			zircon_pass_library_dir_prefix(shadow_path);
			shadow_path.append(find_data.cFileName);

			// failures are expected (a live process's own shadow) and
			// harmless (the next boot's sweep retries)
			if (DeleteFileA(shadow_path.c_str()) != 0)
			{
				++deleted_count;
			}
		} while (FindNextFileA(h_search, &find_data) != 0);

		FindClose(h_search);

		if (deleted_count > 0)
		{
			KOTEK_MESSAGE(
				"[passlib]: swept {} stale pass-library shadow(s) from "
				"previous runs",
				deleted_count);
		}
#endif
	}
#endif
} // namespace

zircon_render_pass_library_manager::zircon_render_pass_library_manager(
	void) :
	m_is_shared_library_active{}
{
}

zircon_render_pass_library_manager::
	~zircon_render_pass_library_manager(void)
{
}

void zircon_render_pass_library_manager::Initialize(
	zircon_renderer_bgfx* p_renderer, bool prefer_shared_library,
	zircon_render_pass_create_pfn_t p_static_create,
	zircon_render_pass_destroy_pfn_t p_static_destroy,
	zircon_passlib_get_count_pfn_t p_static_get_count,
	zircon_passlib_get_name_pfn_t p_static_get_name) noexcept
{
	KOTEK_ASSERT(p_renderer, "pass a valid renderer");
	KOTEK_ASSERT(p_static_create,
		"pass the statically-linked zircon_passlib_create");
	KOTEK_ASSERT(p_static_destroy,
		"pass the statically-linked zircon_passlib_destroy");

	if (!p_renderer || !p_static_create || !p_static_destroy)
	{
		return;
	}

	this->m_pfn_static_create = p_static_create;
	this->m_pfn_static_destroy = p_static_destroy;

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	KOTEK_ASSERT(p_static_get_count && p_static_get_name,
		"pass the statically-linked zircon_passlib_get_count/get_name");

	this->m_p_renderer = p_renderer;

	bool library_loaded = false;

	if (prefer_shared_library)
	{
		// the shadow-copy discipline (task Z3 P3b, the central design
		// point): the engine NEVER loads the real DLL — a loaded image is
		// write-locked on Windows, which would fail the next rebuild at
		// the linker. Every load copies the real file to a fresh shadow
		// name first; the real DLL stays writable at all times
		zircon_delete_stale_pass_library_shadows();

		this->m_shadow_counter = 1;
		zircon_build_pass_library_shadow_path(
			this->m_shadow_counter, this->m_active_shadow_path);

#ifdef KOTEK_USE_PLATFORM_WINDOWS
		if (CopyFileA(ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
				this->m_active_shadow_path.c_str(), FALSE) == 0)
		{
			KOTEK_MESSAGE_ERROR(
				"[passlib]: failed to copy '{}' to the shadow '{}' "
				"(GetLastError={}) — falling back to the "
				"statically-linked passes",
				ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
				this->m_active_shadow_path.c_str(), GetLastError());

			this->m_active_shadow_path.clear();
		}
		else
#endif
		{
			this->m_library.p_lib = zircon_load_pass_library_gracefully(
				this->m_active_shadow_path.c_str());

			library_loaded = this->m_library.p_lib != nullptr;

			if (!library_loaded)
			{
				KOTEK_MESSAGE_ERROR(
					"[passlib]: failed to load the shadow '{}' of '{}' — "
					"falling back to the statically-linked passes",
					this->m_active_shadow_path.c_str(),
					ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH);

				zircon_delete_pass_library_shadow(
					this->m_active_shadow_path.c_str());
				this->m_active_shadow_path.clear();
			}
		}

		if (library_loaded)
		{
			this->m_pfn_active_get_count =
				reinterpret_cast<zircon_passlib_get_count_pfn_t>(
					zircon_get_pass_library_export_gracefully(
						this->m_library.p_lib,
						"zircon_passlib_get_count"));
			this->m_pfn_active_get_name =
				reinterpret_cast<zircon_passlib_get_name_pfn_t>(
					zircon_get_pass_library_export_gracefully(
						this->m_library.p_lib, "zircon_passlib_get_name"));
			this->m_pfn_active_create =
				reinterpret_cast<zircon_render_pass_create_pfn_t>(
					zircon_get_pass_library_export_gracefully(
						this->m_library.p_lib, "zircon_passlib_create"));
			this->m_pfn_active_destroy =
				reinterpret_cast<zircon_render_pass_destroy_pfn_t>(
					zircon_get_pass_library_export_gracefully(
						this->m_library.p_lib, "zircon_passlib_destroy"));

			if (!this->m_pfn_active_get_count ||
				!this->m_pfn_active_get_name ||
				!this->m_pfn_active_create || !this->m_pfn_active_destroy)
			{
				KOTEK_MESSAGE_ERROR(
					"[passlib]: '{}' is missing at least one of the 4 "
					"C-ABI exports (get_count={}, get_name={}, "
					"create={}, destroy={}) — falling back to the "
					"statically-linked passes",
					this->m_active_shadow_path.c_str(),
					reinterpret_cast<void*>(this->m_pfn_active_get_count),
					reinterpret_cast<void*>(this->m_pfn_active_get_name),
					reinterpret_cast<void*>(this->m_pfn_active_create),
					reinterpret_cast<void*>(this->m_pfn_active_destroy));

				this->m_pfn_active_get_count = nullptr;
				this->m_pfn_active_get_name = nullptr;
				this->m_pfn_active_create = nullptr;
				this->m_pfn_active_destroy = nullptr;

				this->m_library.unload();
				zircon_delete_pass_library_shadow(
					this->m_active_shadow_path.c_str());
				this->m_active_shadow_path.clear();
				library_loaded = false;
			}
		}
	}

	if (library_loaded)
	{
		// one-time boot evidence: the loaded shadow, the resolved exports
		// and the registry the DLL exposes
		KOTEK_MESSAGE(
			"[passlib]: loaded shared pass library '{}' (shadow of '{}'; "
			"exports: get_count={}, get_name={}, create={}, destroy={})",
			this->m_active_shadow_path.c_str(),
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
			reinterpret_cast<void*>(this->m_pfn_active_get_count),
			reinterpret_cast<void*>(this->m_pfn_active_get_name),
			reinterpret_cast<void*>(this->m_pfn_active_create),
			reinterpret_cast<void*>(this->m_pfn_active_destroy));

		const unsigned pass_count = this->m_pfn_active_get_count();

		KOTEK_MESSAGE(
			"[passlib]: the shared pass library exposes {} registered "
			"passes:",
			pass_count);

		if (pass_count >
			ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT)
		{
			KOTEK_MESSAGE_WARNING(
				"[passlib]: pass count {} looks corrupt (>{})",
				pass_count,
				ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT);
		}

		for (unsigned i = 0;
		     i < pass_count &&
		     i < ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT;
		     ++i)
		{
			const char* p_name = this->m_pfn_active_get_name(i);

			KOTEK_MESSAGE("[passlib]:   [{}] {}", i,
				p_name ? p_name : "<nullptr>");
		}

		this->m_is_shared_library_active = true;

		this->refill_registry(
			this->m_pfn_active_get_count, this->m_pfn_active_get_name);
	}
	else
	{
		this->m_is_shared_library_active = false;

		// the window still reads the manager's executor-owned registry
		// (same content as its compile-time ctor tables — uniform
		// behavior across the two dev sub-modes)
		this->refill_registry(p_static_get_count, p_static_get_name);
	}

	// the manager owns the renderer's seam in every mode: creation AND
	// destruction route through this instance (create_pass/destroy_pass
	// pick the active source — the loaded library or the static twins)
	p_renderer->set_render_pass_create_callback(
		&zircon_create_render_pass_via_manager, this);
	p_renderer->set_render_pass_destroy_callback(
		&zircon_destroy_render_pass_via_manager, this);

	if (prefer_shared_library)
	{
		// the watcher runs even when the initial load fell back to the
		// static twin: a later-built DLL is picked up the same way
		this->m_watcher_stop.store(false);
		this->m_reload_requested.store(false);

		this->m_watcher_thread = kotek::mt::thread_t(
			&zircon_render_pass_library_manager::watch_pass_library_file,
			this);

		this->m_is_file_watcher_active = true;

		KOTEK_MESSAGE(
			"[passlib]: watching '{}' for changes (poll every {} ms, {} "
			"consecutive stable reads accept a change)",
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
			ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_POLL_INTERVAL_MS,
			ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_STABLE_READS_REQUIRED);
	}
#else
	if (prefer_shared_library)
	{
		KOTEK_MESSAGE_WARNING(
			"[passlib]: graphics_development was requested but this "
			"build has no ZIRCON_GRAPHICS_DEVELOPMENT support — using "
			"the statically-linked passes");
	}

	// the default configuration: creation flows through the same seam
	// (the manager's wrapper -> the statically-linked
	// zircon_passlib_create), destruction stays on
	// ktkRenderGraphSimplified::Shutdown — byte-identical to the pre-P3
	// behavior
	(void)p_static_destroy; // asserted above; only dev builds route
							// destruction through the seam
	(void)p_static_get_count; // the registry copies exist only in dev
	(void)p_static_get_name;  // builds (the window keeps its ctor tables)
	p_renderer->set_render_pass_create_callback(
		&zircon_create_render_pass_via_manager, this);

	this->m_is_shared_library_active = false;
#endif
}

void zircon_render_pass_library_manager::Shutdown(void) noexcept
{
#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	// join the watcher BEFORE any unload (the happens-before edge makes
	// the teardown clean; the thread only probes the real file and sets
	// the flag)
	if (this->m_watcher_thread.joinable())
	{
		this->m_watcher_stop.store(true);
		this->m_watcher_thread.join();
	}

	this->m_is_file_watcher_active = false;

	// neither handle can outlive finish_reload (the three phases run
	// inside one draw() call) — the cleanups below are defensive, not
	// expected paths
	if (this->m_candidate_library.p_lib)
	{
		this->m_candidate_library.unload();

		if (!this->m_candidate_shadow_path.empty())
		{
			zircon_delete_pass_library_shadow(
				this->m_candidate_shadow_path.c_str());
			this->m_candidate_shadow_path.clear();
		}
	}

	if (this->m_replaced_library.p_lib)
	{
		this->m_replaced_library.unload();

		if (!this->m_replaced_shadow_path.empty())
		{
			zircon_delete_pass_library_shadow(
				this->m_replaced_shadow_path.c_str());
			this->m_replaced_shadow_path.clear();
		}
	}

	if (this->m_library.p_lib)
	{
		// the reload-safety invariant: every library-created pass was
		// destroyed through the library before this unload
		KOTEK_ASSERT(this->m_passes_created_via_library ==
				this->m_passes_destroyed_via_library,
			"[passlib]: {} passes were created through the shared pass "
			"library but only {} were destroyed — unloading would leave "
			"live objects owned by a dead library",
			this->m_passes_created_via_library,
			this->m_passes_destroyed_via_library);

		KOTEK_MESSAGE(
			"[passlib]: unloading the shared pass library ({} passes "
			"created, {} destroyed)",
			this->m_passes_created_via_library,
			this->m_passes_destroyed_via_library);

		this->m_pfn_active_get_count = nullptr;
		this->m_pfn_active_get_name = nullptr;
		this->m_pfn_active_create = nullptr;
		this->m_pfn_active_destroy = nullptr;
		this->m_passes_created_via_library = 0;
		this->m_passes_destroyed_via_library = 0;

		this->m_library.unload();

		// the unload released our shadow — remove it
		if (!this->m_active_shadow_path.empty())
		{
			zircon_delete_pass_library_shadow(
				this->m_active_shadow_path.c_str());
			this->m_active_shadow_path.clear();
		}
	}
#endif

	this->m_is_shared_library_active = false;
}

bool zircon_render_pass_library_manager::is_shared_library_active(
	void) const noexcept
{
	return this->m_is_shared_library_active;
}

kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
zircon_render_pass_library_manager::create_pass(
	const char* p_pass_name) noexcept
{
#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	if (this->m_is_shared_library_active)
	{
		KOTEK_ASSERT(this->m_pfn_active_create,
			"the pass library exports must be resolved before creation");

		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass =
			this->m_pfn_active_create(p_pass_name);

		if (p_pass)
		{
			++this->m_passes_created_via_library;

			KOTEK_MESSAGE(
				"[passlib]: created '{}' through the shared pass library "
				"(total: {})",
				p_pass_name, this->m_passes_created_via_library);
		}

		return p_pass;
	}
#endif

	KOTEK_ASSERT(this->m_pfn_static_create,
		"the static pass creation function must be stored at Initialize");

	if (!this->m_pfn_static_create)
	{
		return nullptr;
	}

	return this->m_pfn_static_create(p_pass_name);
}

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT

void zircon_render_pass_library_manager::destroy_pass(
	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
		p_pass) noexcept
{
	if (!p_pass)
	{
		return;
	}

	if (this->m_is_shared_library_active)
	{
		KOTEK_ASSERT(this->m_pfn_active_destroy,
			"the pass library exports must be resolved before "
			"destruction");

		this->m_pfn_active_destroy(p_pass);

		++this->m_passes_destroyed_via_library;

		KOTEK_MESSAGE(
			"[passlib]: destroyed pass through the shared pass library "
			"(total: {})",
			this->m_passes_destroyed_via_library);
	}
	else
	{
		KOTEK_ASSERT(this->m_pfn_static_destroy,
			"the static pass destruction function must be stored at "
			"Initialize");

		if (this->m_pfn_static_destroy)
		{
			this->m_pfn_static_destroy(p_pass);
		}
	}
}

void zircon_render_pass_library_manager::request_reload(void) noexcept
{
	if (!this->m_is_file_watcher_active)
	{
		KOTEK_MESSAGE_WARNING(
			"[passlib]: reload requested but the pass library watcher is "
			"not active (the graphics_development feature is off) — "
			"nothing to do");
		return;
	}

	KOTEK_MESSAGE(
		"[passlib]: reload requested through the console command — the "
		"swap runs at the next frame boundary");

	this->m_reload_requested.store(true);
}

bool zircon_render_pass_library_manager::is_reload_requested(
	void) const noexcept
{
	return this->m_reload_requested.load();
}

bool zircon_render_pass_library_manager::prepare_reload_candidate(
	void) noexcept
{
	KOTEK_ASSERT(this->m_reload_requested.load(),
		"prepare runs only when a reload was requested");

	++this->m_shadow_counter;
	zircon_build_pass_library_shadow_path(
		this->m_shadow_counter, this->m_candidate_shadow_path);

#ifdef KOTEK_USE_PLATFORM_WINDOWS
	if (CopyFileA(ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
			this->m_candidate_shadow_path.c_str(), FALSE) == 0)
	{
		// the linker is mid-write or the file vanished — the watcher
		// re-fires once the write stays stable, so this is a skip, not a
		// terminal failure
		KOTEK_MESSAGE_ERROR(
			"[passlib]: reload FAILED — could not copy '{}' to the "
			"shadow '{}' (GetLastError={}); keeping the current pass "
			"library",
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
			this->m_candidate_shadow_path.c_str(), GetLastError());

		this->set_status(eZirconRenderPassLibraryStatus::kReloadFailed,
			"copy failed — keeping the current pass library");

		this->m_candidate_shadow_path.clear();
		this->m_reload_requested.store(false);
		return false;
	}
#endif

	this->m_candidate_library.p_lib = zircon_load_pass_library_gracefully(
		this->m_candidate_shadow_path.c_str());

	if (!this->m_candidate_library.p_lib)
	{
		KOTEK_MESSAGE_ERROR(
			"[passlib]: reload FAILED — the shadow '{}' did not load; "
			"keeping the current pass library",
			this->m_candidate_shadow_path.c_str());

		this->set_status(eZirconRenderPassLibraryStatus::kReloadFailed,
			"load failed — keeping the current pass library");

		zircon_delete_pass_library_shadow(
			this->m_candidate_shadow_path.c_str());
		this->m_candidate_shadow_path.clear();
		this->m_reload_requested.store(false);
		return false;
	}

	this->m_pfn_candidate_get_count =
		reinterpret_cast<zircon_passlib_get_count_pfn_t>(
			zircon_get_pass_library_export_gracefully(
				this->m_candidate_library.p_lib,
				"zircon_passlib_get_count"));
	this->m_pfn_candidate_get_name =
		reinterpret_cast<zircon_passlib_get_name_pfn_t>(
			zircon_get_pass_library_export_gracefully(
				this->m_candidate_library.p_lib,
				"zircon_passlib_get_name"));
	this->m_pfn_candidate_create =
		reinterpret_cast<zircon_render_pass_create_pfn_t>(
			zircon_get_pass_library_export_gracefully(
				this->m_candidate_library.p_lib, "zircon_passlib_create"));
	this->m_pfn_candidate_destroy =
		reinterpret_cast<zircon_render_pass_destroy_pfn_t>(
			zircon_get_pass_library_export_gracefully(
				this->m_candidate_library.p_lib,
				"zircon_passlib_destroy"));

	if (!this->m_pfn_candidate_get_count ||
		!this->m_pfn_candidate_get_name || !this->m_pfn_candidate_create ||
		!this->m_pfn_candidate_destroy)
	{
		KOTEK_MESSAGE_ERROR(
			"[passlib]: reload FAILED — the candidate '{}' is missing at "
			"least one of the 4 C-ABI exports; keeping the current pass "
			"library",
			this->m_candidate_shadow_path.c_str());

		this->set_status(eZirconRenderPassLibraryStatus::kReloadFailed,
			"resolve failed — keeping the current pass library");

		this->m_pfn_candidate_get_count = nullptr;
		this->m_pfn_candidate_get_name = nullptr;
		this->m_pfn_candidate_create = nullptr;
		this->m_pfn_candidate_destroy = nullptr;

		this->m_candidate_library.unload();
		zircon_delete_pass_library_shadow(
			this->m_candidate_shadow_path.c_str());
		this->m_candidate_shadow_path.clear();
		this->m_reload_requested.store(false);
		return false;
	}

	KOTEK_MESSAGE(
		"[passlib]: candidate pass library ready ('{}' — shadow of '{}')",
		this->m_candidate_shadow_path.c_str(),
		ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH);

	return true;
}

void zircon_render_pass_library_manager::commit_reload_candidate(
	void) noexcept
{
	KOTEK_ASSERT(this->m_candidate_library.p_lib,
		"commit runs only after a successful prepare");

	// every pass the old library created is dead by now (the renderer
	// destroyed them through the old exports) — stage the replaced
	// library for finish_reload and promote the candidate
	this->m_replaced_library = static_cast<kotek::dll::shared_library&&>(
		this->m_library);
	this->m_replaced_shadow_path = this->m_active_shadow_path;

	this->m_library = static_cast<kotek::dll::shared_library&&>(
		this->m_candidate_library);
	this->m_active_shadow_path = this->m_candidate_shadow_path;
	this->m_candidate_shadow_path.clear();

	this->m_pfn_active_get_count = this->m_pfn_candidate_get_count;
	this->m_pfn_active_get_name = this->m_pfn_candidate_get_name;
	this->m_pfn_active_create = this->m_pfn_candidate_create;
	this->m_pfn_active_destroy = this->m_pfn_candidate_destroy;

	this->m_pfn_candidate_get_count = nullptr;
	this->m_pfn_candidate_get_name = nullptr;
	this->m_pfn_candidate_create = nullptr;
	this->m_pfn_candidate_destroy = nullptr;

	// the renderer's callbacks need no re-pointing: they are bound to
	// this instance and route through the active pointers just switched
	this->m_is_shared_library_active = true;
}

void zircon_render_pass_library_manager::finish_reload(void) noexcept
{
	// the recreation through the new library has run — nothing references
	// the replaced library anymore (all its objects died before the
	// commit), so it unloads now and its shadow goes away
	if (this->m_replaced_library.p_lib)
	{
		KOTEK_MESSAGE(
			"[passlib]: unloading the replaced pass library ('{}')",
			this->m_replaced_shadow_path.c_str());

		this->m_replaced_library.unload();
	}

	if (!this->m_replaced_shadow_path.empty())
	{
		if (!zircon_delete_pass_library_shadow(
				this->m_replaced_shadow_path.c_str()))
		{
			KOTEK_MESSAGE_WARNING(
				"[passlib]: could not delete the replaced shadow '{}' — "
				"the next boot's sweep retries",
				this->m_replaced_shadow_path.c_str());
		}

		this->m_replaced_shadow_path.clear();
	}

	// re-enumerate the registry from the NEW library — a pass added in
	// the rebuild appears in the Render Passes window without an editor
	// restart (the generation bump is the window's signal)
	this->refill_registry(
		this->m_pfn_active_get_count, this->m_pfn_active_get_name);

	this->m_reload_requested.store(false);
}

kotek::uint32_t
zircon_render_pass_library_manager::get_registry_generation(
	void) const noexcept
{
	return this->m_registry_generation;
}

bool zircon_render_pass_library_manager::get_registry(
	const char* const*& out_p_editor_pass_names,
	kotek::uint8_t& out_editor_pass_count,
	const char* const*& out_p_game_pass_names,
	kotek::uint8_t& out_game_pass_count) const noexcept
{
	out_p_editor_pass_names = this->m_registry_editor_pass_name_ptrs.data();
	out_editor_pass_count = static_cast<kotek::uint8_t>(
		this->m_registry_editor_pass_name_ptrs.size());
	out_p_game_pass_names = this->m_registry_game_pass_name_ptrs.data();
	out_game_pass_count = static_cast<kotek::uint8_t>(
		this->m_registry_game_pass_name_ptrs.size());

	return this->m_registry_generation != 0;
}

eZirconRenderPassLibraryStatus
zircon_render_pass_library_manager::get_status(void) const noexcept
{
	return this->m_status;
}

const char* zircon_render_pass_library_manager::get_status_message(
	void) const noexcept
{
	return this->m_status_message.c_str();
}

void zircon_render_pass_library_manager::set_status(
	eZirconRenderPassLibraryStatus status, const char* p_message) noexcept
{
	this->m_status = status;

	this->m_status_message.clear();

	if (p_message)
	{
		// every writer passes a short string literal or a buffer
		// pre-formatted into the same capacity — far under the limit
		this->m_status_message.assign(p_message);
	}
}

void zircon_render_pass_library_manager::watch_pass_library_file(
	void) noexcept
{
	// the boot baseline: the real file as the manager found it — only
	// CHANGES after this point may request a reload (all state lives in
	// the manager's members, touched only by this thread)
	this->m_watcher_have_last = zircon_read_pass_library_file_state(
		this->m_watcher_last_write_time, this->m_watcher_last_file_size);

	this->m_watcher_accepted_write_time = this->m_watcher_last_write_time;
	this->m_watcher_accepted_file_size = this->m_watcher_last_file_size;
	this->m_watcher_have_accepted = this->m_watcher_have_last;
	this->m_watcher_stable_reads = 0;

	while (!this->m_watcher_stop.load())
	{
		kotek::mt::this_thread::sleep_for(kotek::chrono::milliseconds(
			ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_POLL_INTERVAL_MS));

		kotek::uint64_t current_write_time = 0;
		kotek::uint64_t current_file_size = 0;

		const bool have_current = zircon_read_pass_library_file_state(
			current_write_time, current_file_size);

		if (!have_current)
		{
			// a clean rebuild deletes the file before relinking — the
			// reappearance re-arms the stability count from scratch
			this->m_watcher_stable_reads = 0;
			this->m_watcher_have_last = false;
			continue;
		}

		if (!this->m_watcher_have_last ||
			current_write_time != this->m_watcher_last_write_time ||
			current_file_size != this->m_watcher_last_file_size)
		{
			// a new write (or the first appearance after a missing
			// period): (re)arm the stability count
			this->m_watcher_last_write_time = current_write_time;
			this->m_watcher_last_file_size = current_file_size;
			this->m_watcher_have_last = true;
			this->m_watcher_stable_reads = 1;
			continue;
		}

		if (this->m_watcher_stable_reads <
			ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_STABLE_READS_REQUIRED)
		{
			++this->m_watcher_stable_reads;
		}

		if (this->m_watcher_stable_reads >=
				ZIRCON_DEF_RENDER_PASS_LIBRARY_WATCHER_STABLE_READS_REQUIRED &&
			(!this->m_watcher_have_accepted ||
				current_write_time != this->m_watcher_accepted_write_time ||
				current_file_size != this->m_watcher_accepted_file_size))
		{
			this->m_watcher_accepted_write_time = current_write_time;
			this->m_watcher_accepted_file_size = current_file_size;
			this->m_watcher_have_accepted = true;

			// no logging from this thread — the frame-boundary protocol
			// logs the detection and the whole swap sequence
			this->m_reload_requested.store(true);
		}
	}
}

void zircon_render_pass_library_manager::refill_registry(
	zircon_passlib_get_count_pfn_t pfn_get_count,
	zircon_passlib_get_name_pfn_t pfn_get_name) noexcept
{
	this->m_registry_editor_pass_names.clear();
	this->m_registry_game_pass_names.clear();
	this->m_registry_editor_pass_name_ptrs.clear();
	this->m_registry_game_pass_name_ptrs.clear();

	if (pfn_get_count && pfn_get_name)
	{
		unsigned pass_count = pfn_get_count();

		if (pass_count >
			ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT)
		{
			KOTEK_MESSAGE_WARNING(
				"[passlib]: pass count {} looks corrupt (>{})",
				pass_count,
				ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT);

			pass_count =
				ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_PASS_COUNT;
		}

		for (unsigned i = 0; i < pass_count; ++i)
		{
			const char* p_name = pfn_get_name(i);

			if (!p_name || p_name[0] == '\0')
			{
				continue;
			}

			// the editor/game split mirrors the codegen's containsEditor
			// rule (generate_zircon.cpp): the registered class name
			// contains "editor" (generated names are lowercase
			// snake_case)
			auto& target_names =
				std::strstr(p_name, "editor") != nullptr
				? this->m_registry_editor_pass_names
				: this->m_registry_game_pass_names;

			if (target_names.size() >= target_names.capacity())
			{
				KOTEK_MESSAGE_ERROR(
					"[passlib]: the registry copy is full ({} passes) — "
					"raise ZIRCON_DEF_RENDER_PASS_LIBRARY_MAX_REGISTRY_"
					"PASS_COUNT",
					target_names.capacity());
				break;
			}

			kotek::static_cstring_t<
				ZIRCON_DEF_RENDER_PASS_LIBRARY_REGISTRY_NAME_MAX_LENGTH>
				name;

			name.assign(p_name);
			target_names.push_back(name);
		}
	}

	// the pointer tables the window reads: they index the manager's own
	// inline storage, which never moves — a refilled table keeps the same
	// address, only the contents change
	for (const auto& name : this->m_registry_editor_pass_names)
	{
		this->m_registry_editor_pass_name_ptrs.push_back(name.c_str());
	}

	for (const auto& name : this->m_registry_game_pass_names)
	{
		this->m_registry_game_pass_name_ptrs.push_back(name.c_str());
	}

	++this->m_registry_generation;
}

#endif
