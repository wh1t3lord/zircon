#include "zircon_localization_manager.h"

#include <kotek.core.api/include/kotek_api.h>
#include <kotek.core.containers.json/include/kotek_core_containers_json.h>
#include <kotek.core.main_manager/include/kotek_main_manager.h>

#include <cstring>

namespace
{
	/// @brief \~english fnv1a-64 over the key bytes (without the
	/// terminator) — the same constants as the test-side
	/// zircon_test_fnv1a (zircon_unit_tests_command_history.cpp)
	kotek::uint64_t localization_fnv1a(const void* p_data, kotek::size_t size,
		kotek::uint64_t hash = 14695981039346656037ULL) noexcept
	{
		const unsigned char* p_bytes =
			reinterpret_cast<const unsigned char*>(p_data);

		for (kotek::size_t i = 0; i < size; ++i)
		{
			hash ^= p_bytes[i];
			hash *= 1099511628211ULL;
		}

		return hash;
	}

	/// the instance's folder under data_game/configs/locale/ — also the
	/// log name; only ever called with a validated enum
	/// (get_instance_data asserts first)
	const char* localization_instance_folder(
		eZirconLocalizationInstance instance) noexcept
	{
		switch (instance)
		{
		case eZirconLocalizationInstance::kEditor:
			return "editor";
		case eZirconLocalizationInstance::kGame:
			return "game";
		default:
			return "editor";
		}
	}

	/// a language tag becomes a FILE name (<lang>.json) — anything that
	/// could walk the path (separators, dots) or blow the fixed buffers
	/// is rejected here; 1..ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH
	/// chars of [a-zA-Z0-9_-]
	bool localization_is_valid_language_name(
		const char* p_language) noexcept
	{
		if (p_language == nullptr)
			return false;

		const kotek::size_t length = std::strlen(p_language);

		if (length == 0 ||
			length > ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH)
			return false;

		for (kotek::size_t i = 0; i < length; ++i)
		{
			const char c = p_language[i];
			const bool is_allowed = (c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_';

			if (is_allowed == false)
				return false;
		}

		return true;
	}
} // namespace

zircon_localization_manager::zircon_localization_manager(void) :
	m_p_filesystem{},
	m_instances{},
	m_file_buffer{},
	m_dom_scratch{}
{
}

zircon_localization_manager::~zircon_localization_manager(void) {}

void zircon_localization_manager::initialize(
	eZirconLocalizationInstance instance,
	kotek::core::ktkMainManager* p_main_manager) noexcept
{
	KOTEK_ASSERT(
		p_main_manager, "you must pass a valid ktkMainManager instance");

	if (p_main_manager == nullptr)
		return;

	kotek::core::ktkIFileSystem* p_filesystem =
		p_main_manager->GetFileSystem();

	KOTEK_ASSERT(
		p_filesystem, "the main manager has no filesystem registered");

	if (p_filesystem == nullptr)
		return;

	this->m_p_filesystem = p_filesystem;

	instance_data_t& data = this->get_instance_data(instance);

	// a set_language call that arrived before initialize already picked
	// the tag — only an untouched instance defaults to the built-in
	if (data.m_language.empty())
		data.m_language.assign(kZirconLocalization_DefaultLanguage);

	this->reload(instance);

	KOTEK_MESSAGE(
		"[localization]: {} instance initialized, language '{}', {} "
		"entries",
		localization_instance_folder(instance), data.m_language.c_str(),
		data.m_entries.size());
}

void zircon_localization_manager::shutdown(void) noexcept
{
	for (kotek::size_t i = 0; i < this->m_instances.size(); ++i)
	{
		instance_data_t& data = this->m_instances[i];

		data.m_language.clear();
		data.m_entries.clear();
		data.m_warned_missing.clear();
		data.m_is_initialized = false;
		data.m_warned_overflow_announced = false;
	}

	this->m_p_filesystem = nullptr;
}

const char* zircon_localization_manager::translate(
	eZirconLocalizationInstance instance, const char* p_key) noexcept
{
	if (p_key == nullptr || p_key[0] == '\0')
		return "";

	instance_data_t& data = this->get_instance_data(instance);

	const kotek::uint64_t key_hash =
		localization_fnv1a(p_key, std::strlen(p_key));

	// the chain: the active table -> the key itself echoed (single
	// residency: no second language is ever loaded)
	if (const entry_t* p_entry = find_entry(data.m_entries, key_hash))
		return p_entry->m_value.c_str();

	bool already_warned = false;

	for (kotek::size_t i = 0; i < data.m_warned_missing.size(); ++i)
	{
		if (data.m_warned_missing[i] == key_hash)
		{
			already_warned = true;
			break;
		}
	}

	if (already_warned == false)
	{
		if (data.m_warned_missing.size() <
			ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS)
		{
			data.m_warned_missing.push_back(key_hash);

			KOTEK_MESSAGE_WARNING(
				"[localization]: missing key '{}' ({} instance, language "
				"'{}') — the key is echoed as the text",
				p_key, localization_instance_folder(instance),
				data.m_language.c_str());
		}
		else if (data.m_warned_overflow_announced == false)
		{
			data.m_warned_overflow_announced = true;

			KOTEK_MESSAGE_WARNING(
				"[localization]: the missing-key dedupe set is full ({}) "
				"— further missing keys still echo on screen but stop "
				"logging; raise ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS",
				ZIRCON_DEF_LOCALIZATION_MAX_WARNED_KEYS);
		}
	}

	return p_key;
}

void zircon_localization_manager::set_language(
	eZirconLocalizationInstance instance, const char* p_language) noexcept
{
	instance_data_t& data = this->get_instance_data(instance);

	if (localization_is_valid_language_name(p_language) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: invalid language name '{}' — 1..{} chars of "
			"[a-zA-Z0-9_-] only; keeping '{}'",
			p_language ? p_language : "nullptr",
			ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH,
			data.m_language.c_str());
		return;
	}

	if (data.m_is_initialized &&
		std::strcmp(data.m_language.c_str(), p_language) == 0)
	{
		return;
	}

	// keep the previous tag until the new table proves usable — a
	// rejected load (missing/malformed file) rolls the tag back and the
	// WORKING table stays (never lose text mid-session)
	kotek::static_cstring_t<ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH>
		language_previous = data.m_language;

	data.m_language.assign(p_language);

	if (this->m_p_filesystem)
	{
		if (this->reload(instance) == false)
		{
			data.m_language = language_previous;

			KOTEK_MESSAGE_WARNING(
				"[localization]: language '{}' of the {} instance is "
				"unavailable — staying on '{}'",
				p_language, localization_instance_folder(instance),
				language_previous.c_str());
		}
	}
	// without a filesystem (never initialized) the tag is stored and
	// initialize() loads it
}

const char* zircon_localization_manager::get_language(
	eZirconLocalizationInstance instance) const noexcept
{
	return this->get_instance_data(instance).m_language.c_str();
}

bool zircon_localization_manager::reload(
	eZirconLocalizationInstance instance) noexcept
{
	instance_data_t& data = this->get_instance_data(instance);

	KOTEK_ASSERT(this->m_p_filesystem,
		"reload needs an initialized manager (no filesystem)");

	if (this->m_p_filesystem == nullptr)
		return false;

	// NO pre-clear: install_table_from_* replaces the table only after
	// the document proves well-formed, so a rejected load (missing or
	// malformed file) leaves the WORKING table in place
	const install_status status = this->install_table_from_file(
		instance, data.m_language.c_str(), data.m_entries);

	if (status == install_status::kRejected)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: the language '{}' of the {} instance is not "
			"available — the previous table stays (missing keys echo on "
			"screen)",
			data.m_language.c_str(),
			localization_instance_folder(instance));
		return false;
	}

	// the warned-once set re-arms only for a table that actually changed
	data.m_warned_missing.clear();
	data.m_warned_overflow_announced = false;

	data.m_is_initialized = true;

	KOTEK_MESSAGE(
		"[localization]: {} instance reloaded, language '{}', {} entries",
		localization_instance_folder(instance), data.m_language.c_str(),
		data.m_entries.size());

	return true;
}

bool zircon_localization_manager::load_language_from_text(
	eZirconLocalizationInstance instance, const char* p_language,
	const void* p_text, kotek::size_t text_size) noexcept
{
	instance_data_t& data = this->get_instance_data(instance);

	if (localization_is_valid_language_name(p_language) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: invalid language name '{}' — the in-memory "
			"table was not installed",
			p_language ? p_language : "nullptr");
		return false;
	}

	// installs into the instance's ONE resident table; a rejected
	// document leaves the previous state (table AND tag) untouched
	const install_status status = this->install_table_from_text(
		p_language, p_text, text_size, data.m_entries);

	if (status == install_status::kRejected)
		return false;

	data.m_language.assign(p_language);
	data.m_warned_missing.clear();
	data.m_warned_overflow_announced = false;

	return status == install_status::kInstalled;
}

kotek::uint32_t zircon_localization_manager::get_entry_count(
	eZirconLocalizationInstance instance) const noexcept
{
	return static_cast<kotek::uint32_t>(
		this->get_instance_data(instance).m_entries.size());
}

kotek::uint32_t zircon_localization_manager::get_warned_missing_count(
	eZirconLocalizationInstance instance) const noexcept
{
	return static_cast<kotek::uint32_t>(
		this->get_instance_data(instance).m_warned_missing.size());
}

zircon_localization_manager::instance_data_t&
zircon_localization_manager::get_instance_data(
	eZirconLocalizationInstance instance) noexcept
{
	kotek::size_t index = static_cast<kotek::size_t>(instance);

	KOTEK_ASSERT(index < this->m_instances.size(),
		"invalid localization instance {}",
		static_cast<kotek::uint32_t>(instance));

	if (index >= this->m_instances.size())
		index = 0;

	return this->m_instances[index];
}

const zircon_localization_manager::instance_data_t&
zircon_localization_manager::get_instance_data(
	eZirconLocalizationInstance instance) const noexcept
{
	kotek::size_t index = static_cast<kotek::size_t>(instance);

	KOTEK_ASSERT(index < this->m_instances.size(),
		"invalid localization instance {}",
		static_cast<kotek::uint32_t>(instance));

	if (index >= this->m_instances.size())
		index = 0;

	return this->m_instances[index];
}

zircon_localization_manager::install_status
zircon_localization_manager::install_table_from_file(
	eZirconLocalizationInstance instance, const char* p_language,
	table_t& out_table) noexcept
{
	KOTEK_ASSERT(this->m_p_filesystem,
		"install_table_from_file needs an initialized manager");

	if (this->m_p_filesystem == nullptr)
		return install_status::kRejected;

	ktk_filesystem_path path_to_file;
	this->m_p_filesystem->Make_Path(path_to_file,
		kotek::core::eFolderIndex::kFolderIndex_DataGame_Configs);

	path_to_file /= "locale";
	path_to_file /= localization_instance_folder(instance);

	kotek::static_cstring_t<
		ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH + 5>
		file_name;
	file_name.assign(p_language);
	file_name += ".json";
	path_to_file /= file_name.c_str();

	kotek::size_t file_size = 0;

	if (this->m_p_filesystem->Get_FileSize(path_to_file, file_size) ==
		false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: locale file '{}' does not exist",
			path_to_file);
		return install_status::kRejected;
	}

	if (file_size == 0)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: locale file '{}' is empty", path_to_file);
		return install_status::kRejected;
	}

	if (file_size > ZIRCON_DEF_LOCALIZATION_FILE_MAX_SIZE - 1)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: locale file '{}' is too big ({} > {}) — "
			"raise ZIRCON_DEF_LOCALIZATION_FILE_MAX_SIZE or shrink the "
			"table",
			path_to_file, file_size,
			ZIRCON_DEF_LOCALIZATION_FILE_MAX_SIZE - 1);
		return install_status::kRejected;
	}

	unsigned char* p_buffer = this->m_file_buffer.data();
	kotek::size_t read_size = this->m_file_buffer.size() - 1;

	if (this->m_p_filesystem->Read_File(
			path_to_file, p_buffer, read_size) == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: failed to read locale file '{}'",
			path_to_file);
		return install_status::kRejected;
	}

	return this->install_table_from_text(
		p_language, this->m_file_buffer.data(), read_size, out_table);
}

zircon_localization_manager::install_status
zircon_localization_manager::install_table_from_text(
	const char* p_language, const void* p_text, kotek::size_t text_size,
	table_t& out_table) noexcept
{
	KOTEK_ASSERT(p_language && p_language[0] != '\0',
		"pass a valid language name");
	KOTEK_ASSERT(p_text || text_size == 0,
		"a non-zero text size needs a valid text pointer");

	if (text_size == 0 || p_text == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: empty document for language '{}'",
			p_language);
		return install_status::kRejected;
	}

	// the backend-portable json subset only (the gltf loader's list):
	// parse(string_view, error_code&, storage_ptr), is_*/as_*,
	// object::find + begin/end + (*it).key()/.value()
	kotek::json::monotonic_resource resource(
		this->m_dom_scratch.data(), this->m_dom_scratch.size());

	kotek::json::error_code parse_error;

	kotek::json::value document = kotek::json::parse(
		kotek::json::string_view(
			reinterpret_cast<const char*>(p_text), text_size),
		parse_error, kotek::json::storage_ptr(&resource));

	if (parse_error)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: malformed json for language '{}' ({})",
			p_language, parse_error.message());
		return install_status::kRejected;
	}

	if (document.is_object() == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: the locale document for language '{}' is "
			"not an object",
			p_language);
		return install_status::kRejected;
	}

	const kotek::json::object& root = document.as_object();

	// the optional "language" field — a mismatch names a mislabeled
	// file (the contents are still used; the file name is the truth)
	{
		auto it_language = root.find("language");

		if (it_language != root.end())
		{
			if ((*it_language).value().is_string())
			{
				const auto& declared = (*it_language).value().as_string();
				const kotek::size_t expected_length =
					std::strlen(p_language);

				if (declared.size() != expected_length ||
					std::memcmp(declared.data(), p_language,
						expected_length) != 0)
				{
					// clamped echo: the declared tag is user data of
					// any length, the log buffer is fixed
					kotek::static_cstring_t<
						ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH>
						declared_short;
					declared_short.assign(declared.data(),
						declared.size() <=
								ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH
							? declared.size()
							: ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH);

					KOTEK_MESSAGE_WARNING(
						"[localization]: locale file for language '{}' "
						"declares '{}' — the file name wins",
						p_language, declared_short.c_str());
				}
			}
			else
			{
				KOTEK_MESSAGE_WARNING(
					"[localization]: 'language' of the locale document "
					"for '{}' must be a string, ignoring the field",
					p_language);
			}
		}
	}

	auto it_entries = root.find("entries");

	if (it_entries == root.end())
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: the locale document for language '{}' has "
			"no 'entries' object",
			p_language);
		return install_status::kRejected;
	}

	if ((*it_entries).value().is_object() == false)
	{
		KOTEK_MESSAGE_WARNING(
			"[localization]: 'entries' of the locale document for "
			"language '{}' must be an object",
			p_language);
		return install_status::kRejected;
	}

	const kotek::json::object& entries = (*it_entries).value().as_object();

	// the document proved well-formed — only now the table is replaced
	out_table.clear();

	bool lost_entries = false;

	for (auto it = entries.begin(); it != entries.end(); ++it)
	{
		const auto key = (*it).key();
		const kotek::json::value& value = (*it).value();

		if (key.empty())
		{
			KOTEK_MESSAGE_WARNING(
				"[localization]: empty key in language '{}', skipping",
				p_language);
			lost_entries = true;
			continue;
		}

		if (key.size() > ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH)
		{
			KOTEK_MESSAGE_WARNING(
				"[localization]: a key in language '{}' is too long ({} "
				"> {}), skipping — raise "
				"ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH",
				p_language, key.size(),
				ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH);
			lost_entries = true;
			continue;
		}

		// every log below echoes the key through a fixed buffer (the
		// json key is a view into the document, NOT null-terminated on
		// every backend)
		kotek::static_cstring_t<ZIRCON_DEF_LOCALIZATION_MAX_KEY_LENGTH>
			key_copy;
		key_copy.assign(key.data(), key.size());

		if (value.is_string() == false)
		{
			KOTEK_MESSAGE_WARNING(
				"[localization]: the value of key '{}' in language '{}' "
				"is not a string, skipping",
				key_copy.c_str(), p_language);
			lost_entries = true;
			continue;
		}

		const auto& text = value.as_string();

		if (text.size() > ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH)
		{
			KOTEK_MESSAGE_WARNING(
				"[localization]: the value of key '{}' in language '{}' "
				"is too long ({} > {}), skipping — raise "
				"ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH",
				key_copy.c_str(), p_language, text.size(),
				ZIRCON_DEF_LOCALIZATION_MAX_VALUE_LENGTH);
			lost_entries = true;
			continue;
		}

		const kotek::uint64_t key_hash =
			localization_fnv1a(key.data(), key.size());

		bool is_duplicate = false;
		const kotek::size_t position =
			find_position(out_table, key_hash, &is_duplicate);

		if (is_duplicate)
		{
			// the json DOM already collapses repeated keys (boost keeps
			// the last occurrence), so this fires on the fnv1a
			// equal-hash case — two DIFFERENT keys colliding; the first
			// inserted entry stays and the collision is loud
			KOTEK_MESSAGE_WARNING(
				"[localization]: duplicate key hash for '{}' in "
				"language '{}' — the first occurrence wins",
				key_copy.c_str(), p_language);
			lost_entries = true;
			continue;
		}

		if (out_table.size() >= ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES)
		{
			KOTEK_MESSAGE_WARNING(
				"[localization]: language '{}' exceeds the entry "
				"capacity — keeping the first {}, the rest is dropped; "
				"raise ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES",
				p_language, ZIRCON_DEF_LOCALIZATION_MAX_ENTRIES);
			lost_entries = true;
			break;
		}

		entry_t entry{};
		entry.m_key_hash = key_hash;
		entry.m_value.assign(text.data(), text.size());

		out_table.insert(out_table.begin() + position, entry);
	}

	return lost_entries ? install_status::kInstalledWithDrops
						: install_status::kInstalled;
}

kotek::size_t zircon_localization_manager::find_position(
	const table_t& table, kotek::uint64_t key_hash,
	bool* p_out_found) noexcept
{
	// lower_bound over the hash-sorted table (hand-rolled — the
	// wrappers-only rule, rule 1b)
	kotek::size_t first = 0;
	kotek::size_t count = table.size();

	while (count > 0)
	{
		const kotek::size_t half = count / 2;
		const kotek::size_t middle = first + half;

		if (table[middle].m_key_hash < key_hash)
		{
			first = middle + 1;
			count -= half + 1;
		}
		else
		{
			count = half;
		}
	}

	if (p_out_found)
	{
		*p_out_found = first < table.size() &&
			table[first].m_key_hash == key_hash;
	}

	return first;
}

const zircon_localization_manager::entry_t*
zircon_localization_manager::find_entry(
	const table_t& table, kotek::uint64_t key_hash) noexcept
{
	bool found = false;
	const kotek::size_t position =
		find_position(table, key_hash, &found);

	return found ? &table[position] : nullptr;
}
