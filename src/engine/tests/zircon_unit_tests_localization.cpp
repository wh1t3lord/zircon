#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../core/zircon_config.h"
		#include "../../core/zircon_localization_manager.h"

		#include <cstdio>
		#include <cstring>

		#ifndef ZIRCON_DEF_UNIT_TEST_LOCALIZATION
			#define ZIRCON_DEF_UNIT_TEST_LOCALIZATION 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_LOCALIZATION == 1

// functional proofs for task Z22 (plan Part A): the localization
// manager loads the shipped en tables, translates through the
// active -> en -> key-echo fallback chain, warns exactly once per
// missing key, degrades gracefully on user data (missing language file,
// malformed json, over-long entries, capacity overflow), keeps its two
// instances isolated, switches language at runtime, and persists both
// language tags through the real game_config.json

namespace
{
	/// @brief \~english the headless environment (the same shape as the
	/// Z19/Z20 fixtures minus the ecs): a real filesystem behind a real
	/// main manager — the manager under test is driven through the
	/// exact initialize(instance, p_main_manager) path the boot uses.
	/// The shipped fixtures on disk: data_game/configs/locale/
	/// {editor,game}/{en,qa}.json — "qa" is the QA pseudo-locale with
	/// known marker strings
	struct zircon_test_localization_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;
		kotek::core::ktkMainManager main_manager;
		zircon_localization_manager localization;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);

			this->main_manager.Set_FileSystem(&this->filesystem);
			this->main_manager.Set_FrameworkConfig(
				&this->framework_config);

			this->localization.initialize(
				eZirconLocalizationInstance::kEditor, &this->main_manager);
			this->localization.initialize(
				eZirconLocalizationInstance::kGame, &this->main_manager);
		}

		void shutdown(void)
		{
			this->localization.shutdown();
			this->filesystem.Shutdown();
		}
	};
} // namespace

TEST(Zircon_Core, LocalizationLoadsEnglishAndTranslatesKnownKeys)
{
	// heap allocated like every fixture that touches these classes (the
	// filesystem alone is ~600 KB, the manager's tables ~800 KB)
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	EXPECT_STREQ(env.localization.get_language(
					 eZirconLocalizationInstance::kEditor),
		"en");
	EXPECT_STREQ(
		env.localization.get_language(eZirconLocalizationInstance::kGame),
		"en");

	// the shipped editor/en.json carries 15 entries, game/en.json 5 —
	// exact counts pin the parse (living code: grow them with the file)
	EXPECT_EQ(env.localization.get_entry_count(
				  eZirconLocalizationInstance::kEditor),
		15u);
	EXPECT_EQ(
		env.localization.get_entry_count(eZirconLocalizationInstance::kGame),
		5u);

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.feature.sbb_quality"),
		"SBB quality");
	EXPECT_STREQ(
		env.localization.translate(
			eZirconLocalizationInstance::kGame, "game.paused"),
		"Paused");

	// the active language IS the default — no separate fallback table
	// is loaded (no double memory)
	EXPECT_EQ(env.localization.get_fallback_entry_count(
				  eZirconLocalizationInstance::kEditor),
		0u);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationMissingKeyEchoesKeyAndWarnsOnce)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// the key itself is the displayed text (missing strings
	// self-document) — and the warning fires exactly once per key
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "no.such.key.exists"),
		"no.such.key.exists");
	EXPECT_EQ(env.localization.get_warned_missing_count(
				  eZirconLocalizationInstance::kEditor),
		1u);

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "no.such.key.exists"),
		"no.such.key.exists");
	EXPECT_EQ(env.localization.get_warned_missing_count(
				  eZirconLocalizationInstance::kEditor),
		1u);

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "another.missing.key"),
		"another.missing.key");
	EXPECT_EQ(env.localization.get_warned_missing_count(
				  eZirconLocalizationInstance::kEditor),
		2u);

	// nullptr/empty keys are guard cases, never warnings nor crashes
	EXPECT_STREQ(
		env.localization.translate(eZirconLocalizationInstance::kEditor,
			nullptr),
		"");
	EXPECT_STREQ(
		env.localization.translate(eZirconLocalizationInstance::kEditor, ""),
		"");

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationWarnedSetCapsGracefully)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// 40 distinct missing keys against a 32-entry dedupe set: every
	// lookup still echoes its key, the set stops growing at the cap and
	// nothing asserts (the log stays sane by design)
	for (kotek::uint32_t i = 0; i < 40; ++i)
	{
		char key[64];
		snprintf(key, sizeof(key), "missing.key.%u", i);

		EXPECT_STREQ(env.localization.translate(
						 eZirconLocalizationInstance::kEditor, key),
			key);
	}

	EXPECT_EQ(env.localization.get_warned_missing_count(
				  eZirconLocalizationInstance::kEditor),
		static_cast<kotek::uint32_t>(
			ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS));

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationMissingLanguageFallsBackToEnglish)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// a language without a file: the selection sticks, the active table
	// stays empty, and every lookup resolves through the "en" fallback
	env.localization.set_language(
		eZirconLocalizationInstance::kEditor, "xx_missing");

	EXPECT_STREQ(env.localization.get_language(
					 eZirconLocalizationInstance::kEditor),
		"xx_missing");
	EXPECT_EQ(env.localization.get_entry_count(
				  eZirconLocalizationInstance::kEditor),
		0u);
	EXPECT_EQ(env.localization.get_fallback_entry_count(
				  eZirconLocalizationInstance::kEditor),
		15u);

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");

	// a key not even "en" knows still echoes itself
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "editor.only.missing"),
		"editor.only.missing");

	// a language name that would walk the path is rejected and changes
	// nothing (the tag becomes a file name — separators/dots are
	// refused by validation)
	env.localization.set_language(
		eZirconLocalizationInstance::kEditor, "../evil");
	EXPECT_STREQ(env.localization.get_language(
					 eZirconLocalizationInstance::kEditor),
		"xx_missing");

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationMalformedJsonIsGraceful)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// the in-memory seam (also the future embedded-defaults entry
	// point): malformed user data is a warning + false, never an
	// assert, and the previous table stays untouched
	const char* broken = "{ this is not json";
	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zz", broken,
		std::strlen(broken)));

	EXPECT_STREQ(env.localization.get_language(
					 eZirconLocalizationInstance::kEditor),
		"en");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");

	// well-formed json that is not a locale document
	const char* not_object = "[1, 2, 3]";
	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zz", not_object,
		std::strlen(not_object)));

	const char* no_entries = "{\"language\":\"zz\"}";
	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zz", no_entries,
		std::strlen(no_entries)));

	const char* entries_not_object =
		"{\"language\":\"zz\",\"entries\":[1]}";
	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zz", entries_not_object,
		std::strlen(entries_not_object)));

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationOverlongAndDuplicateEntriesAreSkipped)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// a 100-char key (cap 96) and a 300-char value (cap 256): both
	// entries are skipped with a warning each, the one valid entry
	// still installs, and the load reports the drops (false)
	char key_long[ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH + 8];
	memset(key_long, 'k', sizeof(key_long) - 1);
	key_long[sizeof(key_long) - 1] = '\0';

	char value_long[ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH + 48];
	memset(value_long, 'v', sizeof(value_long) - 1);
	value_long[sizeof(value_long) - 1] = '\0';

	char document[2048];
	const int written = snprintf(document, sizeof(document),
		"{\"language\":\"zz\",\"entries\":{\"%s\":\"x\",\"ok.key\":\"ok\","
		"\"over.value\":\"%s\",\"dup.key\":\"first\",\"dup.key\":"
		"\"second\"}}",
		key_long, value_long);
	ASSERT_GT(written, 0);
	ASSERT_LT(static_cast<kotek::size_t>(written), sizeof(document));

	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zz", document,
		static_cast<kotek::size_t>(written)));

	// ok.key + dup.key (a same-key duplicate is collapsed by the json
	// DOM before the table ever sees it — boost's parse keeps the LAST
	// occurrence, the own backend's choice is its own; either way the
	// table holds exactly one entry and the manager's insert-time
	// duplicate check is the equal-hash guard, not the json-level one)
	EXPECT_EQ(env.localization.get_entry_count(
				  eZirconLocalizationInstance::kEditor),
		2u);
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, "ok.key"),
		"ok");

	// the over-long key never entered the table — it echoes itself
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, key_long),
		key_long);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationCapacityOverflowIsLoudButGraceful)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// 520 entries against the 512 cap: the first 512 (file order)
	// install, the rest drop with one loud warning, the return is
	// false, and the table stays fully usable
	char document[16384];
	int used = snprintf(document, sizeof(document),
		"{\"language\":\"zzf\",\"entries\":{");
	ASSERT_GT(used, 0);

	for (kotek::uint32_t i = 0; i < 520; ++i)
	{
		const int appended = snprintf(document + used,
			sizeof(document) - static_cast<kotek::size_t>(used),
			"%s\"k%04u\":\"v%04u\"", i == 0 ? "" : ",", i, i);
		ASSERT_GT(appended, 0);
		used += appended;
	}

	const int closed = snprintf(document + used,
		sizeof(document) - static_cast<kotek::size_t>(used), "}}");
	ASSERT_GT(closed, 0);
	used += closed;

	EXPECT_FALSE(env.localization.load_language_from_text(
		eZirconLocalizationInstance::kEditor, "zzf", document,
		static_cast<kotek::size_t>(used)));

	EXPECT_EQ(env.localization.get_entry_count(
				  eZirconLocalizationInstance::kEditor),
		static_cast<kotek::uint32_t>(
			ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES));

	// the first entries survived; entry 519 (past the cap) echoes
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, "k0000"),
		"v0000");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, "k0511"),
		"v0511");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, "k0519"),
		"k0519");

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationInstancesAreIsolated)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	// the same key lives in both "qa" tables with different text — the
	// instances (and their folders) must not bleed into each other
	env.localization.set_language(
		eZirconLocalizationInstance::kEditor, "qa");
	env.localization.set_language(
		eZirconLocalizationInstance::kGame, "qa");

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "test.instance_marker"),
		"editor-qa");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kGame,
					 "test.instance_marker"),
		"game-qa");

	// an editor key is unknown to the game instance entirely (neither
	// the game qa table nor the game en table carries it) — it echoes
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kGame,
					 "settings.window_title"),
		"settings.window_title");

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationSetLanguageAndReloadSwitchText)
{
	zircon_test_localization_env& env = *new zircon_test_localization_env();
	env.initialize();

	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");

	// the runtime switch: the qa table replaces the active one and the
	// "en" table becomes the fallback
	env.localization.set_language(
		eZirconLocalizationInstance::kEditor, "qa");
	EXPECT_STREQ(env.localization.get_language(
					 eZirconLocalizationInstance::kEditor),
		"qa");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"[qa] Settings");

	// a key the qa table lacks falls back to "en" (not to the echo)
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor, "common.ok"),
		"OK");

	// reload re-reads the same files (the editor's cheap runtime path)
	env.localization.reload(eZirconLocalizationInstance::kEditor);
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"[qa] Settings");

	// switching back to the default drops the separate fallback table
	env.localization.set_language(
		eZirconLocalizationInstance::kEditor, "en");
	EXPECT_STREQ(env.localization.translate(
					 eZirconLocalizationInstance::kEditor,
					 "settings.window_title"),
		"Settings");
	EXPECT_EQ(env.localization.get_fallback_entry_count(
				  eZirconLocalizationInstance::kEditor),
		0u);

	env.shutdown();
	delete &env;
}

TEST(Zircon_Core, LocalizationConfigEditorLanguageRoundTrip)
{
	// heap allocated like every fixture that touches these classes (the
	// filesystem's reserved read buffer alone is ~1 MB of stack)
	kotek::core::ktkFrameworkConfig* p_framework_config =
		new kotek::core::ktkFrameworkConfig();
	kotek::core::ktkFileSystem* p_filesystem =
		new kotek::core::ktkFileSystem();
	p_filesystem->Initialize(p_framework_config);

	ktk_filesystem_path path_to_file;
	p_filesystem->Make_Path(
		path_to_file, kotek::core::eFolderIndex::kFolderIndex_DataUser);
	path_to_file /= kZirconConfig_FileName;

	if (p_filesystem->Is_Exists(path_to_file) == false)
	{
		p_filesystem->Shutdown();
		delete p_filesystem;
		delete p_framework_config;
		GTEST_SKIP() << "game_config.json is absent — the roundtrip "
						"needs the real file to preserve";
	}

	// backup the working copy — the serialize under test writes the
	// real data_user/game_config.json, the bytes go back at the end so
	// the test never drifts the user's settings
	kotek::array_t<unsigned char, 2048> backup{};
	kotek::ktk::size_t backup_size = backup.size();
	unsigned char* p_backup_data = backup.data();

	bool status =
		p_filesystem->Read_File(path_to_file, p_backup_data, backup_size);
	ASSERT_TRUE(status);
	ASSERT_LT(backup_size, backup.size());

	// the default is "en"
	{
		zircon_config config_default;
		EXPECT_STREQ(config_default.get_localization_editor_language(),
			"en");
	}

	// a non-default tag persists
	{
		zircon_config config_write;
		config_write.set_localization_editor_language("qa");
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.deserialize(p_filesystem);

		EXPECT_STREQ(
			config_read.get_localization_editor_language(), "qa");
		// no cross-talk with the neighboring language key
		EXPECT_STREQ(config_read.get_localization_game_language(), "en");
	}

	// "en" written back -> the read tag is "en" even though the
	// in-memory value below differs (proves the key is actually read)
	{
		zircon_config config_write;
		config_write.set_localization_editor_language("en");
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.set_localization_editor_language("fr");
		config_read.deserialize(p_filesystem);

		EXPECT_STREQ(
			config_read.get_localization_editor_language(), "en");
	}

	// restore the user's bytes
	status = p_filesystem->Write_File(path_to_file,
		reinterpret_cast<const char*>(backup.data()), backup_size);
	EXPECT_TRUE(status);

	p_filesystem->Shutdown();
	delete p_filesystem;
	delete p_framework_config;
}

TEST(Zircon_Core, LocalizationConfigGameLanguageRoundTrip)
{
	// same backup/restore discipline as the editor-key roundtrip above
	kotek::core::ktkFrameworkConfig* p_framework_config =
		new kotek::core::ktkFrameworkConfig();
	kotek::core::ktkFileSystem* p_filesystem =
		new kotek::core::ktkFileSystem();
	p_filesystem->Initialize(p_framework_config);

	ktk_filesystem_path path_to_file;
	p_filesystem->Make_Path(
		path_to_file, kotek::core::eFolderIndex::kFolderIndex_DataUser);
	path_to_file /= kZirconConfig_FileName;

	if (p_filesystem->Is_Exists(path_to_file) == false)
	{
		p_filesystem->Shutdown();
		delete p_filesystem;
		delete p_framework_config;
		GTEST_SKIP() << "game_config.json is absent — the roundtrip "
						"needs the real file to preserve";
	}

	kotek::array_t<unsigned char, 2048> backup{};
	kotek::ktk::size_t backup_size = backup.size();
	unsigned char* p_backup_data = backup.data();

	bool status =
		p_filesystem->Read_File(path_to_file, p_backup_data, backup_size);
	ASSERT_TRUE(status);
	ASSERT_LT(backup_size, backup.size());

	{
		zircon_config config_default;
		EXPECT_STREQ(
			config_default.get_localization_game_language(), "en");
	}

	{
		zircon_config config_write;
		config_write.set_localization_game_language("qa");
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.deserialize(p_filesystem);

		EXPECT_STREQ(config_read.get_localization_game_language(), "qa");
		EXPECT_STREQ(
			config_read.get_localization_editor_language(), "en");
	}

	{
		zircon_config config_write;
		config_write.set_localization_game_language("en");
		config_write.serialize(p_filesystem);

		zircon_config config_read;
		config_read.set_localization_game_language("fr");
		config_read.deserialize(p_filesystem);

		EXPECT_STREQ(config_read.get_localization_game_language(), "en");
	}

	status = p_filesystem->Write_File(path_to_file,
		reinterpret_cast<const char*>(backup.data()), backup_size);
	EXPECT_TRUE(status);

	p_filesystem->Shutdown();
	delete p_filesystem;
	delete p_framework_config;
}

		#endif
	#endif
#endif
