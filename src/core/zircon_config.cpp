#include "zircon_config.h"

#include <cstring>

namespace
{
	/// reads a comma-separated render-pass list key into a static string
	/// without materializing a std::string; an absent key or an empty
	/// value leaves the current (default) content untouched
	template <kotek::ktk::uint32_t _ParserBufferSize,
		kotek::ktk::uint32_t _JsonMemorySize, bool _Realloc,
		kotek::ktk::size_t _Size>
	void read_render_pass_list(
		const kotek::core::ktkResourceText<_ParserBufferSize, _JsonMemorySize,
			_Realloc>& file,
		const char* p_key,
		kotek::static_cstring_t<_Size>& out_list) noexcept
	{
		const auto& object = file.Get_Object();
		auto it = object.find(p_key);

		if (it == object.end())
		{
			return;
		}

		// (*it).value(): the own json backend's const iterator has no
		// operator-> (boost's does) — this spelling compiles against
		// both
		if (!(*it).value().is_string())
		{
			KOTEK_MESSAGE_WARNING(
				"config key '{}' must be a string, ignoring", p_key);
			return;
		}

		const auto& value = (*it).value().as_string();

		if (value.empty())
		{
			return;
		}

		KOTEK_ASSERT(value.size() <= _Size,
			"render pass list of key '{}' is too long ({} > {}), raise "
			"ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH",
			p_key, value.size(), _Size);

		out_list.assign(value.data(), value.size());
	}

	/// reads a language-tag config key into a static string (task Z22);
	/// an absent key or an empty value keeps the current (default)
	/// content, and a wrong-typed or over-long value is user data — a
	/// warning, never an assert
	template <kotek::ktk::uint32_t _ParserBufferSize,
		kotek::ktk::uint32_t _JsonMemorySize, bool _Realloc,
		kotek::ktk::size_t _Size>
	void read_localization_language(
		const kotek::core::ktkResourceText<_ParserBufferSize, _JsonMemorySize,
			_Realloc>& file,
		const char* p_key,
		kotek::static_cstring_t<_Size>& out_language) noexcept
	{
		const auto& object = file.Get_Object();
		auto it = object.find(p_key);

		if (it == object.end())
		{
			return;
		}

		// (*it).value(): the own json backend's const iterator has no
		// operator-> (boost's does) — this spelling compiles against
		// both
		if (!(*it).value().is_string())
		{
			KOTEK_MESSAGE_WARNING(
				"config key '{}' must be a string, ignoring", p_key);
			return;
		}

		const auto& value = (*it).value().as_string();

		if (value.empty())
		{
			return;
		}

		if (value.size() > _Size)
		{
			KOTEK_MESSAGE_WARNING(
				"config key '{}' is too long ({} > {}), ignoring", p_key,
				value.size(), _Size);
			return;
		}

		out_language.assign(value.data(), value.size());
	}
} // namespace

zircon_config::zircon_config(void) :
	m_is_session_editor{},
	m_current_session_id{kotek::uint8_t(-1)},
	m_features_game{eZirconGameFeatures::kGame_Feature_Unknown},
	// default-TRUE flags live in the ctor (not only in
	// initialize_default) so an existing config file that predates the
	// key keeps the default — deserialize only overwrites when the key
	// is actually present. kSDK_Feature_GraphicsDevelopment (task Z3
	// P3a) defaults TRUE only in ZIRCON_GRAPHICS_DEVELOPMENT builds —
	// that build exists to run passes from the DLL; every other build
	// keeps the static passes
	m_features_sdk{eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart |
		eZirconSDKFeatures::
			kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
#ifdef ZIRCON_USE_GRAPHICS_DEVELOPMENT
			| eZirconSDKFeatures::kSDK_Feature_GraphicsDevelopment
#endif
	},
	m_render_passes_editor{kZirconConfig_DefaultRenderPassesEditor},
	m_render_passes_game{kZirconConfig_DefaultRenderPassesGame},
	// task Z22: default-"en" tags live in the ctor so a config file that
	// predates the keys keeps the default — deserialize only overwrites
	// when the key is actually present (the absent-key idiom)
	m_localization_editor_language{kZirconLocalization_DefaultLanguage},
	m_localization_game_language{kZirconLocalization_DefaultLanguage}
{
}

zircon_config::~zircon_config(void) {}

void zircon_config::set_feature(
	eZirconSDKFeatures feature, bool status
) noexcept
{
	if (status)
	{
		KOTEK_SET_FLAG(this->m_features_sdk, feature);
	}
	else
	{
		KOTEK_REMOVE_FLAG(this->m_features_sdk, feature);
	}
}

void zircon_config::set_feature(
	eZirconSDKFeatures feature, int data
) noexcept
{
	if (this->is_feature_enabled(feature))
	{
		for (auto& entry : this->m_features_data_sdk)
		{
			if (entry.first == feature)
			{
				entry.second = data;
				return;
			}
		}

		KOTEK_ASSERT(
			this->m_features_data_sdk.size() <
				ZIRCON_DEF_CONFIG_MAX_FEATURE_DATA_COUNT,
			"zircon feature data table is full — raise "
			"ZIRCON_DEF_CONFIG_MAX_FEATURE_DATA_COUNT"
		);

		this->m_features_data_sdk.push_back({feature, data});
	}
}

void zircon_config::set_feature(
	eZirconGameFeatures feature, bool status
) noexcept
{
	if (status)
	{
		KOTEK_SET_FLAG(this->m_features_game, feature);
	}
	else
	{
		KOTEK_REMOVE_FLAG(this->m_features_game, feature);
	}
}

bool zircon_config::is_feature_enabled(
	eZirconSDKFeatures feature
) const
{
	return (this->m_features_sdk & feature) == feature;
}

bool zircon_config::is_feature_enabled(
	eZirconGameFeatures feature
) const
{
	return KOTEK_CHECK_FLAG(this->m_features_game, feature);
}

void zircon_config::serialize(
	kotek::Core::ktkIFileSystem* p_filesystem
) noexcept
{
	KOTEK_ASSERT(
		p_filesystem,
		"you can't pass an invalid filesystem here"
	);

	if (p_filesystem)
	{
		ktk_filesystem_path path_to_file;
		p_filesystem->Make_Path(
			path_to_file,
			kotek::core::eFolderIndex::kFolderIndex_DataUser
		);

		path_to_file /= kZirconConfig_FileName;

		kotek::core::ktkResourceText<ZIRCON_DEF_CONFIG_JSON_PARSER_MEMORY_SIZE,
			ZIRCON_DEF_CONFIG_JSON_MEMORY_SIZE, false>
			config(
			kZirconConfig_FileName
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically
			),
			this->is_feature_enabled(
				eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically
			)
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::
					kSDK_Feature_SphereBoundingBox_Quality
			),
			this->get_feature<int>(
				eZirconSDKFeatures::
					kSDK_Feature_SphereBoundingBox_Quality
			)
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart
			),
			this->is_feature_enabled(
				eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart
			)
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::kSDK_Feature_GraphicsDevelopment
			),
			this->is_feature_enabled(
				eZirconSDKFeatures::kSDK_Feature_GraphicsDevelopment
			)
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::
					kSDK_Feature_SDKCamera_Rotation_Quaternion
			),
			this->is_feature_enabled(
				eZirconSDKFeatures::
					kSDK_Feature_SDKCamera_Rotation_Quaternion
			)
		);

		config.Write(
			translate_zircon_sdk_features(
				eZirconSDKFeatures::
					kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
			),
			this->is_feature_enabled(
				eZirconSDKFeatures::
					kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
			)
		);

		config.Write(
			kZirconConfig_KeyRenderPassesEditor,
			this->m_render_passes_editor.c_str()
		);

		config.Write(
			kZirconConfig_KeyRenderPassesGame,
			this->m_render_passes_game.c_str()
		);

		config.Write(
			kZirconConfig_KeyLocalizationEditorLanguage,
			this->m_localization_editor_language.c_str()
		);

		config.Write(
			kZirconConfig_KeyLocalizationGameLanguage,
			this->m_localization_game_language.c_str()
		);

		// raw array is forced by kotek's template signature
		// (ktkResourceText::Serialize_ToString(char (&)[N], Size&) in
		// kotek.core.filesystem.file_text) — exempt from the no-raw-array
		// rule, like the interface-shaped string_view sites
		char text[1024];
		kotek::uint16_t text_real_length = 0;
		bool status = config.Serialize_ToString(text, text_real_length);
		KOTEK_ASSERT(status, "failed to serialize!");

		status = p_filesystem->Write_File(
			path_to_file, text, text_real_length
		);
		KOTEK_ASSERT(
			status, "failed to write to file: {}", path_to_file
		);
	}
}

void zircon_config::deserialize(
	kotek::Core::ktkIFileSystem* p_filesystem
) noexcept
{
	KOTEK_ASSERT(
		p_filesystem,
		"you must have a valid instance of file system (it is "
		"nullptr)"
	);

	if (p_filesystem)
	{
		ktk_filesystem_path path_to_file;
		p_filesystem->Make_Path(
			path_to_file,
			kotek::core::eFolderIndex::kFolderIndex_DataUser
		);

		path_to_file /= kZirconConfig_FileName;

		if (!p_filesystem->Is_Exists(path_to_file))
		{
			this->initialize_default();
		}
		else
		{
			kotek::core::ktkResourceText<
				ZIRCON_DEF_CONFIG_JSON_PARSER_MEMORY_SIZE,
				ZIRCON_DEF_CONFIG_JSON_MEMORY_SIZE, false>
				file;

			kotek::array_t<unsigned char, 1024> text{};

			kotek::ktk::size_t text_size = text.size();

			unsigned char* p_text = text.data();
			bool status = p_filesystem->Read_File(
				path_to_file, p_text, text_size
			);
			KOTEK_ASSERT(
				status, "failed to read file: {}", path_to_file
			);

			status = file.Create_FromMemory(
				text.data(), text_size
			);

			KOTEK_ASSERT(
				status,
				"failed to load from memory: {}",
				path_to_file
			);

			status = file.Get<
				bool>(translate_zircon_sdk_features(
				eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically
			));

			this->set_feature(
				eZirconSDKFeatures::
					kSDK_Feature_AddRequiredComponents_Automatically,
				status
			);

			int quality =
				file.Get<int>(translate_zircon_sdk_features(
					eZirconSDKFeatures::
						kSDK_Feature_SphereBoundingBox_Quality
				));

			this->set_feature(
				eZirconSDKFeatures::
					kSDK_Feature_SphereBoundingBox_Quality,
				quality
			);

			read_render_pass_list(
				file,
				kZirconConfig_KeyRenderPassesEditor,
				this->m_render_passes_editor
			);

			read_render_pass_list(
				file,
				kZirconConfig_KeyRenderPassesGame,
				this->m_render_passes_game
			);

			// task Z22: the localization instances' language tags —
			// absent/empty keys keep the ctor default ("en")
			read_localization_language(
				file,
				kZirconConfig_KeyLocalizationEditorLanguage,
				this->m_localization_editor_language
			);

			read_localization_language(
				file,
				kZirconConfig_KeyLocalizationGameLanguage,
				this->m_localization_game_language
			);

			// default-TRUE flag (Z3 P2a): read only when the key is
			// actually persisted — an absent key (a config written
			// before this flag existed) must keep the ctor default
			// instead of silently flipping to false
			{
				const auto& object = file.Get_Object();

				auto it = object.find(translate_zircon_sdk_features(
					eZirconSDKFeatures::
						kSDK_Feature_ShowPassManagerOnStart
				));

				if (it != object.end())
				{
					// (*it).value(): the own json backend's const
					// iterator has no operator-> (boost's does) —
					// this spelling compiles against both
					if ((*it).value().is_bool())
					{
						this->set_feature(
							eZirconSDKFeatures::
								kSDK_Feature_ShowPassManagerOnStart,
							(*it).value().as_bool()
						);
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"config key 'show_pass_manager_on_start' "
							"must be a bool, ignoring"
						);
					}
				}
			}

			// same absent-key-keeps-default rule (Z3 P3a): the ctor
			// default differs per build (TRUE in
			// ZIRCON_GRAPHICS_DEVELOPMENT builds, FALSE otherwise)
			{
				const auto& object = file.Get_Object();

				auto it = object.find(translate_zircon_sdk_features(
					eZirconSDKFeatures::kSDK_Feature_GraphicsDevelopment
				));

				if (it != object.end())
				{
					if ((*it).value().is_bool())
					{
						this->set_feature(
							eZirconSDKFeatures::
								kSDK_Feature_GraphicsDevelopment,
							(*it).value().as_bool()
						);
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"config key 'graphics_development' must be "
							"a bool, ignoring"
						);
					}
				}
			}

			// same absent-key-keeps-default rule (task Z20): the ctor
			// default is FALSE (euler), a config written before the flag
			// existed must not silently enable the quaternion driver
			{
				const auto& object = file.Get_Object();

				auto it = object.find(translate_zircon_sdk_features(
					eZirconSDKFeatures::
						kSDK_Feature_SDKCamera_Rotation_Quaternion
				));

				if (it != object.end())
				{
					if ((*it).value().is_bool())
					{
						this->set_feature(
							eZirconSDKFeatures::
								kSDK_Feature_SDKCamera_Rotation_Quaternion,
							(*it).value().as_bool()
						);
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"config key 'sdk_camera_rotation_quaternion' "
							"must be a bool, ignoring"
						);
					}
				}
			}

			// same absent-key-keeps-default rule (task Z20): the ctor
			// default is TRUE (the bootstrap is opt-out per the owner's
			// clarification) — a config that predates the key must keep
			// the bootstrap on
			{
				const auto& object = file.Get_Object();

				auto it = object.find(translate_zircon_sdk_features(
					eZirconSDKFeatures::
						kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
				));

				if (it != object.end())
				{
					if ((*it).value().is_bool())
					{
						this->set_feature(
							eZirconSDKFeatures::
								kSDK_Feature_AddSdkCameraInputBootstrap_Automatically,
							(*it).value().as_bool()
						);
					}
					else
					{
						KOTEK_MESSAGE_WARNING(
							"config key 'sdk_camera_input_bootstrap' "
							"must be a bool, ignoring"
						);
					}
				}
			}
		}
	}
}

bool zircon_config::is_current_session_editor(void) const
{
	return this->m_is_session_editor;
}

void zircon_config::set_current_session(
	kotek::uint8_t session_id, bool is_editor
) noexcept
{
	this->m_current_session_id = session_id;
	this->m_is_session_editor = is_editor;
}

const char* zircon_config::get_render_passes_editor(void) const noexcept
{
	return this->m_render_passes_editor.c_str();
}

const char* zircon_config::get_render_passes_game(void) const noexcept
{
	return this->m_render_passes_game.c_str();
}

void zircon_config::set_render_passes_editor(
	const char* p_comma_separated_names
) noexcept
{
	KOTEK_ASSERT(
		p_comma_separated_names,
		"pass a valid comma-separated pass-name list (empty string to "
		"clear, never nullptr)"
	);

	if (p_comma_separated_names)
	{
		KOTEK_ASSERT(
			std::strlen(p_comma_separated_names) <=
				ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH,
			"render pass list is too long, raise "
			"ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH"
		);

		this->m_render_passes_editor.assign(p_comma_separated_names);
	}
}

void zircon_config::set_render_passes_game(
	const char* p_comma_separated_names
) noexcept
{
	KOTEK_ASSERT(
		p_comma_separated_names,
		"pass a valid comma-separated pass-name list (empty string to "
		"clear, never nullptr)"
	);

	if (p_comma_separated_names)
	{
		KOTEK_ASSERT(
			std::strlen(p_comma_separated_names) <=
				ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH,
			"render pass list is too long, raise "
			"ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH"
		);

		this->m_render_passes_game.assign(p_comma_separated_names);
	}
}

const char* zircon_config::get_localization_editor_language(
	void) const noexcept
{
	return this->m_localization_editor_language.c_str();
}

const char* zircon_config::get_localization_game_language(
	void) const noexcept
{
	return this->m_localization_game_language.c_str();
}

void zircon_config::set_localization_editor_language(
	const char* p_language) noexcept
{
	KOTEK_ASSERT(
		p_language,
		"pass a valid language tag (never nullptr)"
	);

	if (p_language)
	{
		KOTEK_ASSERT(
			std::strlen(p_language) <=
				ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH,
			"language tag is too long, raise "
			"ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH"
		);

		this->m_localization_editor_language.assign(p_language);
	}
}

void zircon_config::set_localization_game_language(
	const char* p_language) noexcept
{
	KOTEK_ASSERT(
		p_language,
		"pass a valid language tag (never nullptr)"
	);

	if (p_language)
	{
		KOTEK_ASSERT(
			std::strlen(p_language) <=
				ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH,
			"language tag is too long, raise "
			"ZIRCON_DEF_LOCALIZATION_LANGUAGE_NAME_MAX_LENGTH"
		);

		this->m_localization_game_language.assign(p_language);
	}
}

void zircon_config::initialize_default() noexcept
{
	this->set_feature(
		eZirconSDKFeatures::
			kSDK_Feature_AddRequiredComponents_Automatically,
		true
	);

	this->set_feature(
		eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart, true
	);

	this->set_feature(
		eZirconSDKFeatures::
			kSDK_Feature_AddSdkCameraInputBootstrap_Automatically,
		true
	);
}

const char*
translate_zircon_sdk_features(eZirconSDKFeatures features)
{
	if (KOTEK_CHECK_FLAG(
			features,
			eZirconSDKFeatures::
				kSDK_Feature_AddRequiredComponents_Automatically
		))
	{
		return "add_required_components_automatically";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::
					 kSDK_Feature_SphereBoundingBox_Quality
			 ))
	{
		return "sphere_bounding_box_quality";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart
			 ))
	{
		return "show_pass_manager_on_start";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::kSDK_Feature_GraphicsDevelopment
			 ))
	{
		return "graphics_development";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::
					 kSDK_Feature_SDKCamera_Rotation_Quaternion
			 ))
	{
		return "sdk_camera_rotation_quaternion";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::
					 kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
			 ))
	{
		// 26 chars — the config's Write truncates keys at 32 (see
		// "add_required_components_automati" in a written
		// game_config.json), keep new keys under the limit
		return "sdk_camera_input_bootstrap";
	}
	else if (KOTEK_CHECK_FLAG(
				 features,
				 eZirconSDKFeatures::kSDK_Feature_Unknown
			 ))
	{
		return "unknown";
	}
	else
	{
		return "ENUM_zircon_SDK_FEATURES_UNDEFINED";
	}
}

const char*
translate_zircon_game_features(eZirconGameFeatures features)
{
	if (KOTEK_CHECK_FLAG(
			features,
			eZirconGameFeatures::kGame_Feature_Unknown
		))
	{
		return "unknown";
	}
	else
	{
		return "ENUM_zircon_GAME_FEATURES_UNDEFINED";
	}
}
