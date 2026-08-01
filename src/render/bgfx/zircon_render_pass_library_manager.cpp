#include "zircon_render_pass_library_manager.h"

#include "zircon_renderer.h"

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
namespace
{
	/// sanity cap for the registry enumeration log (a sane library
	/// exposes a handful of passes; more means a corrupt export)
	constexpr unsigned _kMaxPassLibraryEntries = 64;

	/// the resolved C-ABI exports of the loaded pass library. The
	/// renderer's create/destroy callbacks are plain function pointers
	/// (no user data), so the installed wrappers reach the library
	/// through these file-statics — they are valid exactly while the
	/// library stays loaded, and the manager outlives every pass by
	/// construction (the renderer destroys all graphs before
	/// Shutdown() unloads)
	using zircon_passlib_get_count_pfn_t = unsigned (*)(void);
	using zircon_passlib_get_name_pfn_t = const char* (*)(unsigned);

	zircon_passlib_get_count_pfn_t g_pfn_passlib_get_count{};
	zircon_passlib_get_name_pfn_t g_pfn_passlib_get_name{};
	zircon_render_pass_create_pfn_t g_pfn_passlib_create{};
	zircon_render_pass_destroy_pfn_t g_pfn_passlib_destroy{};

	/// one-time boot evidence + the unload-safety invariant: every
	/// library-created pass must come back through the library destroy
	/// before the library unloads
	unsigned g_passes_created_via_library{};
	unsigned g_passes_destroyed_via_library{};

	kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
	zircon_create_render_pass_via_library(const char* p_pass_name)
	{
		KOTEK_ASSERT(g_pfn_passlib_create,
			"the pass library exports must be resolved before creation");

		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass =
			g_pfn_passlib_create(p_pass_name);

		if (p_pass)
		{
			++g_passes_created_via_library;

			KOTEK_MESSAGE(
				"[passlib]: created '{}' through the shared pass library "
				"(total: {})",
				p_pass_name, g_passes_created_via_library);
		}

		return p_pass;
	}

	void zircon_destroy_render_pass_via_library(
		kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass* p_pass)
	{
		KOTEK_ASSERT(g_pfn_passlib_destroy,
			"the pass library exports must be resolved before "
			"destruction");

		if (p_pass)
		{
			g_pfn_passlib_destroy(p_pass);

			++g_passes_destroyed_via_library;

			KOTEK_MESSAGE(
				"[passlib]: destroyed pass through the shared pass "
				"library (total: {})",
				g_passes_destroyed_via_library);
		}
	}
} // namespace
#endif

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
	zircon_render_pass_destroy_pfn_t p_static_destroy) noexcept
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

#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	bool library_loaded = false;

	if (prefer_shared_library)
	{
		library_loaded = this->m_library.load(
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH);

		if (library_loaded)
		{
			g_pfn_passlib_get_count =
				reinterpret_cast<zircon_passlib_get_count_pfn_t>(
					this->m_library.get("zircon_passlib_get_count"));
			g_pfn_passlib_get_name =
				reinterpret_cast<zircon_passlib_get_name_pfn_t>(
					this->m_library.get("zircon_passlib_get_name"));
			g_pfn_passlib_create =
				reinterpret_cast<zircon_render_pass_create_pfn_t>(
					this->m_library.get("zircon_passlib_create"));
			g_pfn_passlib_destroy =
				reinterpret_cast<zircon_render_pass_destroy_pfn_t>(
					this->m_library.get("zircon_passlib_destroy"));

			if (!g_pfn_passlib_get_count || !g_pfn_passlib_get_name ||
				!g_pfn_passlib_create || !g_pfn_passlib_destroy)
			{
				KOTEK_MESSAGE_ERROR(
					"[passlib]: '{}' is missing at least one of the 4 "
					"C-ABI exports (get_count={}, get_name={}, "
					"create={}, destroy={}) — falling back to the "
					"statically-linked passes",
					ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
					reinterpret_cast<void*>(g_pfn_passlib_get_count),
					reinterpret_cast<void*>(g_pfn_passlib_get_name),
					reinterpret_cast<void*>(g_pfn_passlib_create),
					reinterpret_cast<void*>(g_pfn_passlib_destroy));

				g_pfn_passlib_get_count = nullptr;
				g_pfn_passlib_get_name = nullptr;
				g_pfn_passlib_create = nullptr;
				g_pfn_passlib_destroy = nullptr;

				this->m_library.unload();
				library_loaded = false;
			}
		}
		else
		{
			KOTEK_MESSAGE_ERROR(
				"[passlib]: failed to load '{}' — falling back to the "
				"statically-linked passes",
				ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH);
		}
	}

	if (library_loaded)
	{
		// one-time boot evidence: the loaded path, the resolved exports
		// and the registry the DLL exposes
		KOTEK_MESSAGE(
			"[passlib]: loaded shared pass library '{}' (exports: "
			"get_count={}, get_name={}, create={}, destroy={})",
			ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH,
			reinterpret_cast<void*>(g_pfn_passlib_get_count),
			reinterpret_cast<void*>(g_pfn_passlib_get_name),
			reinterpret_cast<void*>(g_pfn_passlib_create),
			reinterpret_cast<void*>(g_pfn_passlib_destroy));

		const unsigned pass_count = g_pfn_passlib_get_count();

		KOTEK_MESSAGE(
			"[passlib]: the shared pass library exposes {} registered "
			"passes:",
			pass_count);

		if (pass_count > _kMaxPassLibraryEntries)
		{
			KOTEK_MESSAGE_WARNING(
				"[passlib]: pass count {} looks corrupt (>{})",
				pass_count, _kMaxPassLibraryEntries);
		}

		for (unsigned i = 0;
		     i < pass_count && i < _kMaxPassLibraryEntries; ++i)
		{
			const char* p_name = g_pfn_passlib_get_name(i);

			KOTEK_MESSAGE("[passlib]:   [{}] {}", i,
				p_name ? p_name : "<nullptr>");
		}

		p_renderer->set_render_pass_create_callback(
			&zircon_create_render_pass_via_library);
		p_renderer->set_render_pass_destroy_callback(
			&zircon_destroy_render_pass_via_library);

		this->m_is_shared_library_active = true;
	}
	else
	{
		// the static twin of the passes (dev builds always carry one):
		// same seam, same destroy-routing, no DLL
		p_renderer->set_render_pass_create_callback(p_static_create);
		p_renderer->set_render_pass_destroy_callback(p_static_destroy);

		this->m_is_shared_library_active = false;
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
	// (the statically-linked zircon_passlib_create), destruction stays
	// on ktkRenderGraphSimplified::Shutdown — byte-identical to the
	// pre-P3 behavior
	(void)p_static_destroy; // asserted above; only dev builds route
							// destruction through the seam
	p_renderer->set_render_pass_create_callback(p_static_create);

	this->m_is_shared_library_active = false;
#endif
}

void zircon_render_pass_library_manager::Shutdown(void) noexcept
{
#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
	if (this->m_library.p_lib)
	{
		// the reload-safety invariant: every library-created pass was
		// destroyed through the library before this unload
		KOTEK_ASSERT(
			g_passes_created_via_library == g_passes_destroyed_via_library,
			"[passlib]: {} passes were created through the shared pass "
			"library but only {} were destroyed — unloading would leave "
			"live objects owned by a dead library",
			g_passes_created_via_library, g_passes_destroyed_via_library);

		KOTEK_MESSAGE(
			"[passlib]: unloading the shared pass library ({} passes "
			"created, {} destroyed)",
			g_passes_created_via_library, g_passes_destroyed_via_library);

		g_pfn_passlib_get_count = nullptr;
		g_pfn_passlib_get_name = nullptr;
		g_pfn_passlib_create = nullptr;
		g_pfn_passlib_destroy = nullptr;
		g_passes_created_via_library = 0;
		g_passes_destroyed_via_library = 0;

		this->m_library.unload();
	}
#endif

	this->m_is_shared_library_active = false;
}

bool zircon_render_pass_library_manager::is_shared_library_active(
	void) const noexcept
{
	return this->m_is_shared_library_active;
}
