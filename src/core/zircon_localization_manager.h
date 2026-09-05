#pragma once

#include "zircon_defs.h"

// ---------------------------------------------------------------------
// task Z22 (plan Part A): string-table localization. ONE engine, TWO
// instances of this class — the editor (SDK UI strings) and the game
// (layer-3 text) — selected by eZirconLocalizationInstance, never two
// classes. Files live at
// data_game/configs/locale/<editor|game>/<lang>.json:
//
//     { "language": "en", "entries": { "menu.file": "File", ... } }
//
// flat dotted keys, one file per language per instance. Lookup chain:
// active language -> built-in default ("en") -> the key itself echoed
// as the text (missing strings self-document on screen) + ONE warning
// per missing key (deduped). Formatting (args in strings) is out of
// scope v1 — the API takes/returns text unchanged; the design doesn't
// preclude a later format entry point.
// ---------------------------------------------------------------------

/// 512 entries cover an editor window set many times over (the shipped
/// en table is ~20 strings); raise only with measured data (rule 9)
#define ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES 512
/// dotted keys stay short and readable ("settings.feature.sbb_quality"
/// is 31 chars); 96 is three such segments deep
#define ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH 96
/// a UI string is a sentence, not a paragraph; 256 chars covers
/// tooltips and multi-line help
#define ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH 256
/// one warning per missing key, deduped through this set (a plain
/// sorted-less static_vector scan — 32 entries, rule 2); when full,
/// later misses still echo the key but stop logging
#define ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS 32
/// BCP-47 tags are short ("en", "en-US", "zh-Hans"); 16 covers
/// regional variants with margin. zircon_config persists the tag in
/// the SAME capacity (it includes this header), so a persisted value
/// always fits the manager
#define ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH 16
/// a full table at the caps above is 512 * (96 + 256 + ~12 of json
/// syntax) ~= 186 KB; 192 KB covers it with margin — bigger files are
/// rejected loudly at load (they can never produce a valid table)
#define ZIRCON_DEF_LOCALIZATION_FILE_MAX_SIZE 196608
/// inline scratch of the monotonic DOM resource; grows by bounded
/// allocation for texts that outgrow it (the gltf loader's documented
/// pattern) — shipped tables are a few KB and never allocate
#define ZIRCON_DEF_LOCALIZATION_JSON_DOM_SCRATCH_SIZE 65536

/// the built-in default/fallback language: every lookup chain ends in
/// this table before echoing the key (active -> en -> key)
constexpr const char* kZirconLocalization_DefaultLanguage = "en";

/// the two string-table instances (owner directive: split for
/// modularity) — the editor reads the SDK UI strings, the game reads
/// layer-3 text; same class, own folder each
enum class eZirconLocalizationInstance : kotek::uint8_t
{
	kEditor = 0,
	kGame,
	kCount
};

/// @brief \~english the localization manager (task Z22). Heap-allocated
/// by the game manager (the tables are ~280 KB per instance at the
/// caps — never place this class on a stack frame). All lookup paths
/// are allocation-free after load: tables are sorted static vectors of
/// (fnv1a-64 key hash -> value) driven by binary search; equal hashes
/// are treated as the same key (fnv1a-64 collisions across a 512-entry
/// table are below any measurable rate, and a collision degrades to a
/// wrong-but-stable string, never to corruption)
class zircon_localization_manager
{
public:
	zircon_localization_manager(void);
	~zircon_localization_manager(void);

	/// loads the instance's active language table (the persisted
	/// selection — the caller pushes it via set_language afterwards) and
	/// the "en" fallback; missing files degrade to the fallback chain
	/// with warnings, never an assert (user data is not a programmer
	/// error)
	void initialize(
		eZirconLocalizationInstance instance,
		kotek::core::ktkMainManager* p_main_manager) noexcept;

	void shutdown(void) noexcept;

	/// never returns nullptr: active table -> "en" fallback -> the key
	/// itself; non-const because the missing-key path maintains the
	/// warned-once dedupe set
	const char* translate(
		eZirconLocalizationInstance instance, const char* p_key) noexcept;

	/// validates the tag (a language name becomes a FILE name: 1..16
	/// chars of [a-zA-Z0-9_-], nothing else — path separators and dots
	/// are rejected), then reloads the instance's tables; a same-value
	/// call on an initialized instance is a no-op. The selection sticks
	/// even when the file is missing (display falls back to en)
	void set_language(eZirconLocalizationInstance instance,
		const char* p_language) noexcept;

	/// "" when the instance was never initialized
	const char* get_language(eZirconLocalizationInstance instance) const noexcept;

	/// re-reads the instance's files from disk (the editor's runtime
	/// switch — cheap at these sizes); also re-arms the warned-once set
	void reload(eZirconLocalizationInstance instance) noexcept;

	/// installs a language table from in-memory text instead of a file.
	/// Two consumers: the tests (malformed/overflow/over-long inputs
	/// without touching disk) and the future embedded-defaults chain
	/// (plan Part B4 — a compiled-in blob rides the same entry point).
	/// Returns true only when every entry was installed; on a malformed
	/// document the instance's previous state is kept untouched
	bool load_language_from_text(eZirconLocalizationInstance instance,
		const char* p_language, const void* p_text,
		kotek::size_t text_size) noexcept;

	/// diagnostics (the tests pin behavior through these; a future
	/// editor statistics window reads the same numbers)
	kotek::uint32_t get_entry_count(
		eZirconLocalizationInstance instance) const noexcept;
	kotek::uint32_t get_fallback_entry_count(
		eZirconLocalizationInstance instance) const noexcept;
	kotek::uint32_t get_warned_missing_count(
		eZirconLocalizationInstance instance) const noexcept;

private:
	struct entry_t
	{
		kotek::uint64_t m_key_hash;
		kotek::static_cstring_t<ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH>
			m_value;
	};

	using table_t =
		kotek::static_vector_t<entry_t, ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES>;

	struct instance_data_t
	{
		instance_data_t(void) :
			m_language{},
			m_entries{},
			m_entries_fallback_en{},
			m_warned_missing{},
			m_is_initialized{false},
			m_warned_overflow_announced{false}
		{
		}

		kotek::static_cstring_t<
			ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH>
			m_language;
		/// the active language's table, sorted by hash; when the active
		/// language IS "en" this is the only loaded table
		table_t m_entries;
		/// the "en" table, sorted by hash; EMPTY when the active
		/// language is "en" (no double load, no double memory)
		table_t m_entries_fallback_en;
		kotek::static_vector_t<kotek::uint64_t,
			ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS>
			m_warned_missing;
		bool m_is_initialized;
		bool m_warned_overflow_announced;
	};

	/// kInstalled = every entry made it; kInstalledWithDrops = the table
	/// is usable but some entries were dropped (duplicates, over-long
	/// keys/values, the 512-entry capacity — each named by its own
	/// warning); kRejected = the document/file was unusable and the
	/// target table was NOT touched
	enum class install_status : kotek::uint8_t
	{
		kInstalled,
		kInstalledWithDrops,
		kRejected
	};

private:
	instance_data_t& get_instance_data(
		eZirconLocalizationInstance instance) noexcept;
	const instance_data_t& get_instance_data(
		eZirconLocalizationInstance instance) const noexcept;

	/// file -> bytes -> install_table_from_text; kRejected on any
	/// failure (missing/empty/too-big file, malformed json — each with
	/// its own warning), out_table untouched on a malformed document
	install_status install_table_from_file(
		eZirconLocalizationInstance instance, const char* p_language,
		table_t& out_table) noexcept;

	/// parses the json text and fills out_table (cleared only after the
	/// document proved well-formed, so a malformed text never destroys a
	/// working table). Entries are inserted in sorted position as
	/// parsed; over-long keys/values are skipped with a warning each;
	/// more than ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES entries keeps the
	/// first 512 with one loud warning. (Duplicate keys: the json DOM
	/// collapses a repeated key before the table sees it — the
	/// insert-time duplicate check below is the equal-HASH guard, the
	/// fnv1a collision case, and names the key in its warning)
	install_status install_table_from_text(const char* p_language,
		const void* p_text, kotek::size_t text_size,
		table_t& out_table) noexcept;

	/// lower_bound over the hash-sorted table: the insertion position
	/// for key_hash; *p_out_found (when non-null) reports an exact hash
	/// match at that position
	static kotek::size_t find_position(const table_t& table,
		kotek::uint64_t key_hash, bool* p_out_found = nullptr) noexcept;

	static const entry_t* find_entry(
		const table_t& table, kotek::uint64_t key_hash) noexcept;

private:
	/// non-owning, captured at initialize; the filesystem outlives the
	/// manager (the game manager destroys localization before the
	/// engine's subsystems go down)
	kotek::core::ktkIFileSystem* m_p_filesystem;
	kotek::array_t<instance_data_t,
		static_cast<kotek::size_t>(eZirconLocalizationInstance::kCount)>
		m_instances;

	// shared load scratch: table loads are sequential (boot, a runtime
	// language switch), never concurrent — one buffer pair serves both
	// instances and both tables
	kotek::array_t<unsigned char, ZIRCON_DEF_LOCALIZATION_FILE_MAX_SIZE>
		m_file_buffer;
	kotek::array_t<unsigned char, ZIRCON_DEF_LOCALIZATION_JSON_DOM_SCRATCH_SIZE>
		m_dom_scratch;
};
