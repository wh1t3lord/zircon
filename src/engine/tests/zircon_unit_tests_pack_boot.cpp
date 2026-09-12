#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG
		#ifdef KOTEK_USE_FILESYSTEM_TYPE_PACK

			#include <gtest/gtest.h>

			#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

			#include <cstdio>
			#include <cstring>
			#include <filesystem>

// functional proof for the filesystem plan's phase B2b (owner-approved
// plan Part B): the BOOT dispatcher reads pack-only content. A tiny pack
// is built in-test through the shared encoder (kpack_write_file — the same
// function the zircon_kpacker host tool links) into the conventional
// data_game/packs folder, a FRESH filesystem then mounts it at Initialize
// exactly like the engine boot, and the reads go through the same
// ktkIFileSystem::Read_File interface the engine uses. The pack file and
// folder are removed at the end and a third instance proves the
// mounted-then-unmounted lifecycle leaves no residue.
//
// COLLISION EXPECTATION (the shipped priority): B2a's documented boot
// behavior is that mounting a pack PREPENDS kPack to the effective
// priority list when the config never listed it ("native always last" —
// packs override the native dirs, the mod/patch story; pinned by kotek's
// test_b2a_pack_conventional_folder_mounts_newest_first). So a same-named
// file present in BOTH resolves from the PACK by default, and the
// native-wins shape is the user's EXPLICIT ["Native","Pack"] order — both
// are pinned below, mirroring B2a's fallthrough/priority suite.

namespace
{
	/// @brief \~english the headless environment (the same shape as the
	/// Z22 localization fixture): a real filesystem behind a real framework
	/// config — the boot dispatcher, no engine session needed
	struct zircon_test_pack_boot_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};
} // namespace

TEST(Zircon_Game, PackBootDispatcherReadsConventionalFolder)
{
	// heap allocated like every fixture that touches these classes (the
	// filesystem alone is ~600 KB — a stack env overflows the TestBody
	// prologue, the Z20 fixture lesson)
	zircon_test_pack_boot_env& probe_env = *new zircon_test_pack_boot_env();

	probe_env.initialize();

	ktk_filesystem_path data_game;
	probe_env.filesystem.Make_Path(
		data_game, kotek::core::eFolderIndex::kFolderIndex_DataGame
	);

	ktk_filesystem_path packs_folder = data_game;
	packs_folder /= kotek::core::kKpackPacksFolderName;

	ktk_filesystem_path pack_path = packs_folder;
	pack_path /= "b2b_boot_marker.kpack";

	ktk_filesystem_path tests_folder;
	probe_env.filesystem.Make_Path(
		tests_folder, kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests
	);

	ktk_filesystem_path shared_path = tests_folder;
	shared_path /= "b2b_shared.bin";

	// pre-clean (a crashed previous run must not pollute this one) — only
	// this test's own files are ever touched
	std::error_code ec;
	std::filesystem::remove(std::filesystem::path(pack_path.c_str()), ec);
	ec.clear();
	std::filesystem::remove(
		std::filesystem::path(packs_folder.c_str()), ec);
	ec.clear();

	// PHASE A: the baseline — the shipped tree mounts nothing and the
	// marker is absent everywhere (this is also the residue property the
	// previous run's phase C left behind)
	EXPECT_TRUE(
		probe_env.filesystem.Get_Pack()->Get_MountedPackCount() == 0);

	ktk_filesystem_path marker_path = data_game;
	marker_path /= "configs/b2b_pack_marker.json";

	{
		kotek::uint8_t scratch[64];
		kotek::uint8_t* p_scratch = scratch;
		kotek::size_t scratch_size = sizeof(scratch);

		EXPECT_FALSE(probe_env.filesystem.Read_File(
			marker_path, p_scratch, scratch_size));
	}

	probe_env.shutdown();
	delete &probe_env;

	// PHASE B: build the tiny pack through the shared encoder — a
	// pack-only marker json under data_game and a name that ALSO exists
	// natively (written below with different bytes)
	const char marker_payload[] = "{\"b2b\": \"pack-only-marker\"}\n";
	const char pack_shared_payload[] = "B2B-PACK-BYTES!!!!!!";
	const char native_shared_payload[] = "B2B-NATIVE-BYTES!!!!";

	const kotek::core::kpack_writer_entry_t entries[] = {
		{"data_game/configs/b2b_pack_marker.json",
		 reinterpret_cast<const kotek::uint8_t*>(marker_payload),
		 sizeof(marker_payload), kotek::core::eKpackCompression::kZstd},
		{"data_user/tests/b2b_shared.bin",
		 reinterpret_cast<const kotek::uint8_t*>(pack_shared_payload),
		 sizeof(pack_shared_payload),
		 kotek::core::eKpackCompression::kStored},
	};

	ASSERT_TRUE(std::filesystem::create_directories(
		std::filesystem::path(packs_folder.c_str()), ec));
	ASSERT_TRUE(kotek::core::kpack_write_file(
		pack_path.c_str(), entries, 2));

	zircon_test_pack_boot_env& env = *new zircon_test_pack_boot_env();
	env.initialize();

	// the boot dispatcher's conventional mount fired at Initialize
	EXPECT_TRUE(
		env.filesystem.Get_Pack()->Get_MountedPackCount() == 1);

	// and it prepended kPack (the shipped "native always last" override
	// semantics — see the file-header note)
	EXPECT_TRUE(
		env.framework_config.Get_FS_PriorityList()[0] ==
		static_cast<kotek::uint8_t>(
			kotek::core::eFileSystemPriorityType::kPack));

	// the native twin of the shared name
	ASSERT_TRUE(env.filesystem.Write_File(
		shared_path, native_shared_payload,
		sizeof(native_shared_payload)));

	// pack-only content reads through the same interface the engine
	// uses, byte-exact
	{
		kotek::uint8_t readback[64];
		kotek::uint8_t* p_readback = readback;
		kotek::size_t readback_size = sizeof(readback);

		EXPECT_TRUE(env.filesystem.Read_File(
			marker_path, p_readback, readback_size));
		EXPECT_TRUE(readback_size == sizeof(marker_payload));
		EXPECT_TRUE(
			memcmp(readback, marker_payload,
				sizeof(marker_payload)) == 0);
	}

	// the collision: the shipped default resolves it from the PACK (the
	// B2a prepend)
	{
		kotek::uint8_t readback[64];
		kotek::uint8_t* p_readback = readback;
		kotek::size_t readback_size = sizeof(readback);

		EXPECT_TRUE(env.filesystem.Read_File(
			shared_path, p_readback, readback_size));
		EXPECT_TRUE(readback_size == sizeof(pack_shared_payload));
		EXPECT_TRUE(
			memcmp(readback, pack_shared_payload,
				sizeof(pack_shared_payload)) == 0);
	}

	// the user's explicit ["Native","Pack"] order flips the override:
	// the loose native file shadows the pack entry (mirrors B2a's flip),
	// while the pack-only marker still answers (native has nothing)
	{
		kotek::uint8_t native_first[static_cast<kotek::uint8_t>(
			kotek::core::eFileSystemPriorityType::kEndOfEnum)] = {};
		native_first[0] = static_cast<kotek::uint8_t>(
			kotek::core::eFileSystemPriorityType::kNative);
		native_first[1] = static_cast<kotek::uint8_t>(
			kotek::core::eFileSystemPriorityType::kPack);
		env.framework_config.Set_FS_PriorityList(native_first);

		kotek::uint8_t readback[64];
		kotek::uint8_t* p_readback = readback;
		kotek::size_t readback_size = sizeof(readback);

		EXPECT_TRUE(env.filesystem.Read_File(
			shared_path, p_readback, readback_size));
		EXPECT_TRUE(readback_size == sizeof(native_shared_payload));
		EXPECT_TRUE(
			memcmp(readback, native_shared_payload,
				sizeof(native_shared_payload)) == 0);

		p_readback = readback;
		readback_size = sizeof(readback);

		EXPECT_TRUE(env.filesystem.Read_File(
			marker_path, p_readback, readback_size));
		EXPECT_TRUE(readback_size == sizeof(marker_payload));
	}

	// PHASE C: the lifecycle — shutdown (unmount), remove the pack and
	// the folder, and a fresh instance must mount NOTHING (no residue:
	// the next boot behaves as if the pack never existed)
	env.filesystem.Shutdown();

	ec.clear();
	std::filesystem::remove(
		std::filesystem::path(shared_path.c_str()), ec);
	ec.clear();
	std::filesystem::remove(std::filesystem::path(pack_path.c_str()), ec);
	ec.clear();
	std::filesystem::remove(
		std::filesystem::path(packs_folder.c_str()), ec);
	ec.clear();

	delete &env;

	zircon_test_pack_boot_env& env_after = *new zircon_test_pack_boot_env();
	env_after.initialize();

	EXPECT_TRUE(
		env_after.filesystem.Get_Pack()->Get_MountedPackCount() == 0);
	EXPECT_TRUE(
		env_after.framework_config.Get_FS_PriorityList()[0] ==
		static_cast<kotek::uint8_t>(
			kotek::core::eFileSystemPriorityType::kNative));

	{
		kotek::uint8_t scratch[64];
		kotek::uint8_t* p_scratch = scratch;
		kotek::size_t scratch_size = sizeof(scratch);

		EXPECT_FALSE(env_after.filesystem.Read_File(
			marker_path, p_scratch, scratch_size));
	}

	env_after.shutdown();
	delete &env_after;
}

		#endif
	#endif
#endif
