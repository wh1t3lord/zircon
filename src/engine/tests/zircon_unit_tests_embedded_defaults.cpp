#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../zircon_resource_manager.h"
		#include "../../core/zircon_embedded_defaults.h"

		#include <cstdio>
		#include <cstring>
		#include <filesystem>

		#ifdef KOTEK_USE_FILESYSTEM_TYPE_PACK
			#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>
		#endif

// functional proofs for the filesystem plan's phase B4 (task Z23): the
// fault-tolerant defaults chain. Per embedded type: absent from pack AND
// from disk -> the embedded blob loads, the loud-once log fires EXACTLY
// once per (type, path) across two resolutions, no exit/fault/assert on
// the path (Debug runs with asserts live), and a second resolution
// reuses the resolved default; the negative control (a real file shadows
// the default — real bytes, no default, no log); the resource-manager
// integration (a missing text resource loads as is_default=true with
// valid empty-json content); the pack-hosted text resource loads through
// the dispatcher (the B3-noted native-only gate fix); the warned-set
// capacity (16) is loud-but-graceful on overflow. All lightweight
// (house rule 8a).

namespace
{
	/// @brief \~english the headless environment (the same shape as the
	/// B2b pack-boot fixture): a real filesystem behind a real framework
	/// config — the boot dispatcher, no engine session needed.
	/// Heap-allocated: the filesystem alone is ~600 KB, a stack env
	/// overflows the TestBody prologue (the Z20 fixture lesson)
	struct zircon_test_embedded_defaults_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	/// one full resolution that MUST land on the embedded default: the
	/// returned bytes ARE the embedded blob (same address — the default
	/// is reused, never copied), the size matches, the caller buffer is
	/// untouched, and the byte content equals the table entry
	void expect_default_resolution(
		kotek::core::ktkIFileSystem* p_fs,
		zircon_embedded_defaults& defaults,
		const kotek::static_path_t& path,
		eZirconEmbeddedDefaultType type)
	{
		const zircon_embedded_default_t& blob =
			zircon_embedded_defaults::get_embedded_default(type);

		kotek::uint8_t scratch[256] = {};
		kotek::size_t scratch_size = sizeof(scratch);
		zircon_resolved_or_default_t resolved{};

		EXPECT_TRUE(zircon_resolve_or_default(p_fs, defaults, path, type,
			scratch, scratch_size, resolved));

		EXPECT_TRUE(resolved.m_is_default);
		EXPECT_TRUE(resolved.m_p_bytes == blob.m_p_bytes);
		EXPECT_TRUE(resolved.m_size == blob.m_size);
		EXPECT_TRUE(scratch_size == blob.m_size);

		if (resolved.m_size == blob.m_size)
		{
			EXPECT_TRUE(
				memcmp(resolved.m_p_bytes, blob.m_p_bytes, blob.m_size) ==
				0);
		}
	}

	/// the negative control: a REAL file at path shadows the default —
	/// the real bytes land in the caller's buffer, no default, and the
	/// warned count is untouched
	void expect_real_shadows_default(
		kotek::core::ktkIFileSystem* p_fs,
		zircon_embedded_defaults& defaults,
		const kotek::static_path_t& path,
		eZirconEmbeddedDefaultType type,
		const void* p_real_bytes,
		kotek::size_t real_size,
		kotek::uint32_t warned_count_before)
	{
		kotek::uint8_t readback[512] = {};
		kotek::size_t readback_size = sizeof(readback);
		zircon_resolved_or_default_t resolved{};

		EXPECT_TRUE(zircon_resolve_or_default(p_fs, defaults, path, type,
			readback, readback_size, resolved));

		EXPECT_FALSE(resolved.m_is_default);
		EXPECT_TRUE(resolved.m_p_bytes == readback);
		EXPECT_TRUE(resolved.m_size == real_size);
		EXPECT_TRUE(readback_size == real_size);

		if (resolved.m_size == real_size)
		{
			EXPECT_TRUE(memcmp(resolved.m_p_bytes, p_real_bytes,
								real_size) == 0);
		}

		EXPECT_EQ(defaults.get_warned_count(), warned_count_before);
	}

	void make_tests_path(kotek::core::ktkIFileSystem* p_fs,
		kotek::static_path_t& out, const char* p_name)
	{
		p_fs->Make_Path(
			out, kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests);
		out /= p_name;
	}

	void remove_file_quiet(const kotek::static_path_t& path)
	{
		std::error_code ec;
		std::filesystem::remove(std::filesystem::path(path.c_str()), ec);
	}
} // namespace

TEST(Zircon_Core, EmbeddedDefaultTextureCheckerResolvesWhenAbsent)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	// the blob itself: 2x2 magenta/black, RGBA8 — the Source-style
	// missing-texture signal, a pixel block + metadata, NOT a file
	// container
	const zircon_embedded_default_t& blob =
		zircon_embedded_defaults::get_embedded_default(
			eZirconEmbeddedDefaultType::kTexture_Checker_Magenta);

	EXPECT_TRUE(blob.m_type ==
		eZirconEmbeddedDefaultType::kTexture_Checker_Magenta);
	EXPECT_EQ(blob.m_size, 16u);
	EXPECT_EQ(blob.m_texture_width, 2u);
	EXPECT_EQ(blob.m_texture_height, 2u);
	EXPECT_TRUE(blob.m_texture_format ==
		eZirconEmbeddedTextureFormat::kRGBA8_Unorm);

	ASSERT_EQ(blob.m_size, 16u);
	EXPECT_EQ(blob.m_p_bytes[0], 0xFF);  // magenta R
	EXPECT_EQ(blob.m_p_bytes[1], 0x00);  // magenta G
	EXPECT_EQ(blob.m_p_bytes[2], 0xFF);  // magenta B
	EXPECT_EQ(blob.m_p_bytes[3], 0xFF);  // opaque A
	EXPECT_EQ(blob.m_p_bytes[4], 0x00);  // black pixel
	EXPECT_EQ(blob.m_p_bytes[6], 0x00);
	EXPECT_EQ(blob.m_p_bytes[7], 0xFF);
	EXPECT_EQ(blob.m_p_bytes[12], 0xFF); // the diagonal magenta
	EXPECT_EQ(blob.m_p_bytes[14], 0xFF);

	kotek::static_path_t missing;
	make_tests_path(
		&env.filesystem, missing, "z23_definitely_missing_texture.ktx");
	remove_file_quiet(missing); // insurance: it must be absent

	zircon_embedded_defaults defaults;

	// absent from disk AND from any pack (the shipped tree mounts
	// nothing) -> the embedded blob loads
	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kTexture_Checker_Magenta);
	EXPECT_EQ(defaults.get_warned_count(), 1u);

	// the second resolution of the SAME missing resource reuses the
	// resolved default and does NOT re-log
	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kTexture_Checker_Magenta);
	EXPECT_EQ(defaults.get_warned_count(), 1u);
	EXPECT_FALSE(defaults.get_warned_overflow_announced());

	// negative control: a real file shadows the default
	const kotek::uint8_t real_pixels[] = {1, 2, 3, 4, 5, 6, 7, 8};

	ASSERT_TRUE(env.filesystem.Write_File(
		missing, real_pixels, sizeof(real_pixels)));

	expect_real_shadows_default(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kTexture_Checker_Magenta, real_pixels,
		sizeof(real_pixels), 1u);

	// a real file that does not fit the caller's buffer is a caller
	// error (the required size comes back), NEVER a default
	{
		kotek::uint8_t tiny[4] = {};
		kotek::size_t tiny_size = sizeof(tiny);
		zircon_resolved_or_default_t resolved{};

		EXPECT_FALSE(zircon_resolve_or_default(&env.filesystem, defaults,
			missing, eZirconEmbeddedDefaultType::kTexture_Checker_Magenta,
			tiny, tiny_size, resolved));
		EXPECT_EQ(tiny_size, sizeof(real_pixels));
	}

	remove_file_quiet(missing);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, EmbeddedDefaultShaderTokenResolvesWhenAbsent)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	// the shader default is a SEMANTIC TOKEN (documented in the header):
	// the render side maps these exact bytes to its built-in
	// unlit-magenta material per backend — no compiled blob can live in
	// zircon core (backend-specific, runtime-chosen)
	const zircon_embedded_default_t& blob =
		zircon_embedded_defaults::get_embedded_default(
			eZirconEmbeddedDefaultType::kShader_Unlit_Magenta);

	constexpr char k_expected_token[] =
		"ZIRCON_SHADER_DEFAULT_UNLIT_MAGENTA";

	EXPECT_EQ(blob.m_size,
		static_cast<kotek::uint32_t>(sizeof(k_expected_token) - 1));
	EXPECT_EQ(blob.m_texture_width, 0u);
	EXPECT_TRUE(blob.m_texture_format ==
		eZirconEmbeddedTextureFormat::kNone);
	ASSERT_EQ(blob.m_size, sizeof(k_expected_token) - 1);
	EXPECT_TRUE(memcmp(blob.m_p_bytes, k_expected_token,
					blob.m_size) == 0);

	kotek::static_path_t missing;
	make_tests_path(
		&env.filesystem, missing, "z23_definitely_missing_shader.bin");
	remove_file_quiet(missing);

	zircon_embedded_defaults defaults;

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kShader_Unlit_Magenta);
	EXPECT_EQ(defaults.get_warned_count(), 1u);

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kShader_Unlit_Magenta);
	EXPECT_EQ(defaults.get_warned_count(), 1u);
	EXPECT_FALSE(defaults.get_warned_overflow_announced());

	const kotek::uint8_t real_shader[] = {'f', 'a', 'k', 'e', 'd', 'x',
		'i', 'l'};

	ASSERT_TRUE(env.filesystem.Write_File(
		missing, real_shader, sizeof(real_shader)));

	expect_real_shadows_default(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kShader_Unlit_Magenta, real_shader,
		sizeof(real_shader), 1u);

	remove_file_quiet(missing);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, EmbeddedDefaultAudioSilenceResolvesWhenAbsent)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	// the silent buffer is a canonical PCM WAV: mono 8 kHz 16-bit, 32
	// zeroed samples behind a 44-byte header (108 bytes total)
	const zircon_embedded_default_t& blob =
		zircon_embedded_defaults::get_embedded_default(
			eZirconEmbeddedDefaultType::kAudio_Silence);

	ASSERT_EQ(blob.m_size, 108u);
	EXPECT_TRUE(memcmp(blob.m_p_bytes, "RIFF", 4) == 0);
	EXPECT_TRUE(memcmp(blob.m_p_bytes + 8, "WAVE", 4) == 0);
	EXPECT_TRUE(memcmp(blob.m_p_bytes + 12, "fmt ", 4) == 0);
	EXPECT_EQ(blob.m_p_bytes[20], 0x01); // PCM
	EXPECT_EQ(blob.m_p_bytes[22], 0x01); // mono
	EXPECT_EQ(blob.m_p_bytes[24], 0x40); // 8000 Hz little-endian
	EXPECT_EQ(blob.m_p_bytes[25], 0x1F);
	EXPECT_TRUE(memcmp(blob.m_p_bytes + 36, "data", 4) == 0);
	EXPECT_EQ(blob.m_p_bytes[40], 0x40); // 64 bytes of sample data

	bool samples_are_silent = true;

	for (kotek::size_t i = 44; i < blob.m_size; ++i)
	{
		if (blob.m_p_bytes[i] != 0x00)
			samples_are_silent = false;
	}

	EXPECT_TRUE(samples_are_silent);

	kotek::static_path_t missing;
	make_tests_path(
		&env.filesystem, missing, "z23_definitely_missing_sound.ogg");
	remove_file_quiet(missing);

	zircon_embedded_defaults defaults;

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kAudio_Silence);
	EXPECT_EQ(defaults.get_warned_count(), 1u);

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kAudio_Silence);
	EXPECT_EQ(defaults.get_warned_count(), 1u);
	EXPECT_FALSE(defaults.get_warned_overflow_announced());

	const kotek::uint8_t real_audio[] = {9, 9, 9, 9, 9, 9, 9, 9, 9};

	ASSERT_TRUE(env.filesystem.Write_File(
		missing, real_audio, sizeof(real_audio)));

	expect_real_shadows_default(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kAudio_Silence, real_audio,
		sizeof(real_audio), 1u);

	remove_file_quiet(missing);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, EmbeddedDefaultJsonEmptyResolvesWhenAbsent)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	// the two bytes "{}" — trivial, and exactly the type that makes
	// config/locale-style loads never fail
	const zircon_embedded_default_t& blob =
		zircon_embedded_defaults::get_embedded_default(
			eZirconEmbeddedDefaultType::kJson_Empty);

	ASSERT_EQ(blob.m_size, 2u);
	EXPECT_EQ(blob.m_p_bytes[0], '{');
	EXPECT_EQ(blob.m_p_bytes[1], '}');

	kotek::static_path_t missing;
	make_tests_path(
		&env.filesystem, missing, "z23_definitely_missing_config.json");
	remove_file_quiet(missing);

	zircon_embedded_defaults defaults;

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kJson_Empty);
	EXPECT_EQ(defaults.get_warned_count(), 1u);

	expect_default_resolution(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kJson_Empty);
	EXPECT_EQ(defaults.get_warned_count(), 1u);
	EXPECT_FALSE(defaults.get_warned_overflow_announced());

	const char real_json[] = "{\"z23\": true}";

	ASSERT_TRUE(env.filesystem.Write_File(
		missing, real_json, sizeof(real_json) - 1));

	expect_real_shadows_default(&env.filesystem, defaults, missing,
		eZirconEmbeddedDefaultType::kJson_Empty, real_json,
		sizeof(real_json) - 1, 1u);

	remove_file_quiet(missing);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, EmbeddedDefaultsWarnedSetCapsGracefully)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	zircon_embedded_defaults defaults;

	// 20 distinct missing json resources against the 16-entry dedupe
	// set: every one still resolves to the default, the set stops
	// growing at the cap, ONE suppression notice, nothing asserts
	for (kotek::uint32_t i = 0; i < 20; ++i)
	{
		char name[64];
		snprintf(name, sizeof(name), "z23_missing_%u.json", i);

		kotek::static_path_t missing;
		make_tests_path(&env.filesystem, missing, name);

		expect_default_resolution(&env.filesystem, defaults, missing,
			eZirconEmbeddedDefaultType::kJson_Empty);
	}

	EXPECT_EQ(defaults.get_warned_count(),
		static_cast<kotek::uint32_t>(
			ZIRCON_DEF_EMBEDDED_DEFAULTS_MAX_WARNED));
	EXPECT_TRUE(defaults.get_warned_overflow_announced());

	env.shutdown();
	delete &env;
}

TEST(Zircon_Game, ResourceManagerMissingTextResolvesToEmbeddedDefault)
{
	zircon_test_embedded_defaults_env& env =
		*new zircon_test_embedded_defaults_env();

	env.initialize();

	kotek::core::ktkMainManager main_manager;

	main_manager.Set_FileSystem(&env.filesystem);
	main_manager.Set_FrameworkConfig(&env.framework_config);

	zircon_resource_manager* p_rm = new zircon_resource_manager();

	p_rm->initialize(&main_manager);

	kotek::static_path_t missing;
	make_tests_path(
		&env.filesystem, missing, "z23_rm_definitely_missing.json");
	remove_file_quiet(missing); // the resource must be absent

	// PHASE A: the missing text resource resolves to the embedded
	// empty-json default — is_loaded AND is_default, never a fault
	kotek::shared_ptr_t<zircon_resource_t> result =
		p_rm->load(missing, eZirconResourceLoadingFlags::kSync);

	ASSERT_TRUE(result.get() != nullptr);

	const zircon_resource_desc_t* p_desc =
		p_rm->get_desc(result->desc_id);

	ASSERT_TRUE(p_desc != nullptr);
	EXPECT_TRUE(p_desc->is_loaded);
	EXPECT_TRUE(p_desc->is_default);
	EXPECT_TRUE(p_desc->type == eZirconResourceType::kText);

	// valid empty-json content: the view parses and holds no keys
	const zircon_view_handle_t* p_view =
		p_rm->get_view(result->view_id);

	ASSERT_TRUE(p_view != nullptr);
	ASSERT_TRUE(p_view->p_view != nullptr);

	const auto* p_text_view =
		static_cast<const kotek::core::ktkResourceViewText*>(
			p_view->p_view);

	EXPECT_FALSE(p_text_view->Is_KeyExist("anything"));
	EXPECT_FALSE(p_text_view->Is_KeyExist("z23"));

	// the loud-once fired exactly once through the manager's own
	// defaults instance
	EXPECT_EQ(p_rm->get_embedded_defaults().get_warned_count(), 1u);

	// the second load of the same missing resource reuses the resolved
	// default — still exactly one loud log
	kotek::shared_ptr_t<zircon_resource_t> result_second =
		p_rm->load(missing, eZirconResourceLoadingFlags::kSync);

	ASSERT_TRUE(result_second.get() != nullptr);

	const zircon_resource_desc_t* p_desc_second =
		p_rm->get_desc(result_second->desc_id);

	ASSERT_TRUE(p_desc_second != nullptr);
	EXPECT_TRUE(p_desc_second->is_loaded);
	EXPECT_TRUE(p_desc_second->is_default);
	EXPECT_EQ(p_rm->get_embedded_defaults().get_warned_count(), 1u);
	EXPECT_FALSE(p_rm->get_embedded_defaults()
	                 .get_warned_overflow_announced());

	result_second.reset();
	result.reset();

	// PHASE B (the negative control): a real json file at the same
	// manager shadows the default — real content, is_default=false, no
	// new log
	constexpr const char k_real_content[] =
		R"({"test_name": "ResourceManagerMissingTextResolvesToEmbeddedDefault"})";

	ASSERT_TRUE(env.filesystem.Write_File(
		missing, k_real_content, sizeof(k_real_content) - 1));

	kotek::shared_ptr_t<zircon_resource_t> result_real =
		p_rm->load(missing, eZirconResourceLoadingFlags::kSync);

	ASSERT_TRUE(result_real.get() != nullptr);

	const zircon_resource_desc_t* p_desc_real =
		p_rm->get_desc(result_real->desc_id);

	ASSERT_TRUE(p_desc_real != nullptr);
	EXPECT_TRUE(p_desc_real->is_loaded);
	EXPECT_FALSE(p_desc_real->is_default);

	const zircon_view_handle_t* p_view_real =
		p_rm->get_view(result_real->view_id);

	ASSERT_TRUE(p_view_real != nullptr);
	ASSERT_TRUE(p_view_real->p_view != nullptr);

	const auto* p_text_view_real =
		static_cast<const kotek::core::ktkResourceViewText*>(
			p_view_real->p_view);

	EXPECT_TRUE(p_text_view_real->Is_KeyExist("test_name"));
	EXPECT_EQ(p_rm->get_embedded_defaults().get_warned_count(), 1u);

	result_real.reset();

	remove_file_quiet(missing);

	p_rm->shutdown();

	env.shutdown();

	delete p_rm;
	delete &env;
}

	#ifdef KOTEK_USE_FILESYSTEM_TYPE_PACK
TEST(Zircon_Game, ResourceManagerPackHostedTextLoadsThroughDispatcher)
{
	// the B3-noted gate fix: the text branch used to pre-check existence
	// through the NATIVE backend only (Is_Exists), so a pack-hosted text
	// resource could never load. A tiny pack is built in-test through the
	// shared encoder (the pack-boot fixture's shape), a FRESH filesystem
	// mounts it at Initialize exactly like the engine boot, and the
	// resource manager's sync load must resolve the pack-only json
	// through the dispatcher — real content, is_default=false

	// PHASE A: no pack mounted — the marker is absent for the manager
	// (and resolves to the embedded default, the B4 behavior)
	kotek::static_path_t marker_path;
	kotek::static_path_t packs_folder;
	kotek::static_path_t pack_path;

	// pre-clean BEFORE any filesystem mounts (a crashed previous run's
	// pack must not be picked up at Initialize — the test environment's
	// root is the cwd, the same assumption every fixture makes)
	{
		std::error_code ec;
		std::filesystem::remove("data_game/packs/z23_rm_pack.kpack", ec);
	}

	{
		zircon_test_embedded_defaults_env& probe_env =
			*new zircon_test_embedded_defaults_env();

		probe_env.initialize();

		probe_env.filesystem.Make_Path(
			packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		pack_path = packs_folder;
		pack_path /= "z23_rm_pack.kpack";

		probe_env.filesystem.Make_Path(
			marker_path,
			kotek::core::eFolderIndex::kFolderIndex_DataGame_Configs);
		marker_path /= "z23_pack_marker.json";

		// the shipped tree mounts nothing now
		ASSERT_TRUE(
			probe_env.filesystem.Get_Pack()->Get_MountedPackCount() == 0);

		kotek::core::ktkMainManager main_manager;
		main_manager.Set_FileSystem(&probe_env.filesystem);
		main_manager.Set_FrameworkConfig(&probe_env.framework_config);

		zircon_resource_manager* p_rm = new zircon_resource_manager();
		p_rm->initialize(&main_manager);

		kotek::shared_ptr_t<zircon_resource_t> result =
			p_rm->load(marker_path, eZirconResourceLoadingFlags::kSync);

		ASSERT_TRUE(result.get() != nullptr);

		const zircon_resource_desc_t* p_desc =
			p_rm->get_desc(result->desc_id);

		ASSERT_TRUE(p_desc != nullptr);
		EXPECT_TRUE(p_desc->is_loaded);
		EXPECT_TRUE(p_desc->is_default);

		result.reset();

		p_rm->shutdown();
		delete p_rm;

		probe_env.shutdown();
		delete &probe_env;
	}

	// PHASE B: build the tiny pack and load the marker through the
	// dispatcher — BEFORE the gate fix this load could only fail (the
	// native-only Is_Exists never saw pack entries)
	constexpr const char k_pack_payload[] =
		R"({"test_name": "ResourceManagerPackHostedTextLoadsThroughDispatcher", "origin": "pack"})";

	const kotek::core::kpack_writer_entry_t entries[] = {
		{"data_game/configs/z23_pack_marker.json",
			reinterpret_cast<const kotek::uint8_t*>(k_pack_payload),
			sizeof(k_pack_payload) - 1,
			kotek::core::eKpackCompression::kZstd},
	};

	std::error_code ec;
	ASSERT_TRUE(std::filesystem::create_directories(
		std::filesystem::path(packs_folder.c_str()), ec));
	ASSERT_TRUE(kotek::core::kpack_write_file(pack_path.c_str(), entries, 1));

	{
		zircon_test_embedded_defaults_env& env =
			*new zircon_test_embedded_defaults_env();

		env.initialize();

		// the boot dispatcher's conventional mount fired at Initialize
		ASSERT_TRUE(env.filesystem.Get_Pack()->Get_MountedPackCount() == 1);

		kotek::core::ktkMainManager main_manager;
		main_manager.Set_FileSystem(&env.filesystem);
		main_manager.Set_FrameworkConfig(&env.framework_config);

		zircon_resource_manager* p_rm = new zircon_resource_manager();
		p_rm->initialize(&main_manager);

		kotek::shared_ptr_t<zircon_resource_t> result =
			p_rm->load(marker_path, eZirconResourceLoadingFlags::kSync);

		ASSERT_TRUE(result.get() != nullptr);

		const zircon_resource_desc_t* p_desc =
			p_rm->get_desc(result->desc_id);

		ASSERT_TRUE(p_desc != nullptr);
		EXPECT_TRUE(p_desc->is_loaded);
		EXPECT_FALSE(p_desc->is_default);
		EXPECT_TRUE(p_desc->type == eZirconResourceType::kText);

		// the pack's real bytes made it into the parsed DOM
		const zircon_view_handle_t* p_view =
			p_rm->get_view(result->view_id);

		ASSERT_TRUE(p_view != nullptr);
		ASSERT_TRUE(p_view->p_view != nullptr);

		const auto* p_text_view =
			static_cast<const kotek::core::ktkResourceViewText*>(
				p_view->p_view);

		ASSERT_TRUE(p_text_view->Is_KeyExist("origin"));

		const auto loaded_origin =
			p_text_view->Get<kotek::static_cstring_t<96>>("origin");

		EXPECT_TRUE(loaded_origin == "pack");

		// a chain hit never touches the defaults
		EXPECT_EQ(p_rm->get_embedded_defaults().get_warned_count(), 0u);

		result.reset();

		p_rm->shutdown();
		delete p_rm;

		env.shutdown();
		delete &env;
	}

	// PHASE C: the lifecycle — remove the pack and the folder; nothing
	// leaks into later boots (the pack-boot fixture owns the full
	// residue pin, this test just cleans up after itself)
	remove_file_quiet(pack_path);

	ec.clear();
	std::filesystem::remove(std::filesystem::path(packs_folder.c_str()), ec);
}
	#endif

	#endif
#endif
