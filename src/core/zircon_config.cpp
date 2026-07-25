#include "zircon_config.h"

zircon_config::zircon_config(void) :
	m_is_session_editor{},
	m_current_session_id{kotek::uint8_t(-1)},
	m_features_game{eZirconGameFeatures::kGame_Feature_Unknown},
	m_features_sdk{eZirconSDKFeatures::kSDK_Feature_Unknown}
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

void zircon_config::initialize_default() noexcept
{
	this->set_feature(
		eZirconSDKFeatures::
			kSDK_Feature_AddRequiredComponents_Automatically,
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
