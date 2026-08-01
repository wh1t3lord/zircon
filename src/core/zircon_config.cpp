#include "zircon_config.h"

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

		if (!it->value().is_string())
		{
			KOTEK_MESSAGE_WARNING(
				"config key '{}' must be a string, ignoring", p_key);
			return;
		}

		const auto& value = it->value().as_string();

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
} // namespace

zircon_config::zircon_config(void) :
	m_is_session_editor{},
	m_current_session_id{kotek::uint8_t(-1)},
	m_features_game{eZirconGameFeatures::kGame_Feature_Unknown},
	// default-TRUE flags live in the ctor (not only in
	// initialize_default) so an existing config file that predates the
	// key keeps the default — deserialize only overwrites when the key
	// is actually present
	m_features_sdk{eZirconSDKFeatures::kSDK_Feature_ShowPassManagerOnStart},
	m_render_passes_editor{kZirconConfig_DefaultRenderPassesEditor},
	m_render_passes_game{kZirconConfig_DefaultRenderPassesGame}
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

		kotek::core::ktkResourceText<1024, 2048, false> config(
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
			kZirconConfig_KeyRenderPassesEditor,
			this->m_render_passes_editor.c_str()
		);

		config.Write(
			kZirconConfig_KeyRenderPassesGame,
			this->m_render_passes_game.c_str()
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
			kotek::core::ktkResourceText<1024, 2048, false>
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
					if (it->value().is_bool())
					{
						this->set_feature(
							eZirconSDKFeatures::
								kSDK_Feature_ShowPassManagerOnStart,
							it->value().as_bool()
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
