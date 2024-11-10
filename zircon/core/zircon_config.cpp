#include "zircon_config.h"

zircon_config::zircon_config(void) :
	m_features_game{eZirconGameFeatures::kGame_Feature_Unknown},
	m_features_sdk{eZirconSDKFeatures::kSDK_Feature_Unknown}
{
}

zircon_config::~zircon_config(void) {}

void zircon_config::set_feature(
	eZirconSDKFeatures feature, bool status) noexcept
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

void zircon_config::set_feature(eZirconSDKFeatures feature, int data) noexcept
{
	if (this->is_feature_enabled(feature))
	{
		this->m_features_data_sdk[feature] = data;
	}
}

void zircon_config::set_feature(
	eZirconGameFeatures feature, bool status) noexcept
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

bool zircon_config::is_feature_enabled(eZirconSDKFeatures feature) const
{
	return (this->m_features_sdk & feature) == feature;
}

bool zircon_config::is_feature_enabled(eZirconGameFeatures feature) const
{
	return KOTEK_CHECK_FLAG(this->m_features_game, feature);
}

void zircon_config::serialize(Kotek::Core::ktkIFileSystem* p_filesystem,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(p_filesystem, "you can't pass an invalid filesystem here");
	KOTEK_ASSERT(
		p_resource_manager, "you can't pass an invalid resource manager");
	KOTEK_ASSERT(p_resource_manager->Get_ResourceSaver(),
		"resource manager must have a valid resource saver instance!");

	if (p_filesystem)
	{
		if (p_resource_manager)
		{
			auto* p_saver = p_resource_manager->Get_ResourceSaver();

			if (p_saver)
			{
				auto path_to_file = p_filesystem->GetFolderByEnum(
					Kotek::Core::eFolderIndex::kFolderIndex_UserData);

				path_to_file /= kZirconConfig_FileName;

				Kotek::Core::ktkFileText config(kZirconConfig_FileName);

				config.Write(
					translate_zircon_sdk_features(eZirconSDKFeatures::
							kSDK_Feature_AddRequiredComponents_Automatically),
					this->is_feature_enabled(eZirconSDKFeatures::
							kSDK_Feature_AddRequiredComponents_Automatically));

				config.Write(translate_zircon_sdk_features(eZirconSDKFeatures::
									 kSDK_Feature_SphereBoundingBox_Quality),
					this->get_feature<int>(eZirconSDKFeatures::
							kSDK_Feature_SphereBoundingBox_Quality));

				p_saver->Save(path_to_file,
					kotek::core::ktkResourceHandle(&config, true));
			}
		}
	}
}

void zircon_config::deserialize(Kotek::Core::ktkIFileSystem* p_filesystem,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(p_filesystem,
		"you must have a valid instance of file system (it is nullptr)");
	KOTEK_ASSERT(p_resource_manager,
		"you must have a valid instance of resource manager (it is nullptr)");
	KOTEK_ASSERT(p_resource_manager->Get_ResourceSaver(),
		"you must have a valid instance of resource manager saver (it is "
		"nullptr)");

	if (p_filesystem)
	{
		if (p_resource_manager)
		{
			auto* p_loader = p_resource_manager->Get_ResourceLoader();

			if (p_loader)
			{
				auto path_to_file = p_filesystem->GetFolderByEnum(
					Kotek::Core::eFolderIndex::kFolderIndex_UserData);

				path_to_file /= kZirconConfig_FileName;

				if (!p_filesystem->IsValidPath(path_to_file))
				{
					this->initialize_default();
				}
				else
				{
					Kotek::Core::ktkFileText file;
					KOTEK_ASSERT(
						p_loader->Load(path_to_file,
							kotek::core::ktkResourceHandle(&file, true)),
						"failed to load file!");

					bool status = file.Get<bool>(translate_zircon_sdk_features(
						eZirconSDKFeatures::
							kSDK_Feature_AddRequiredComponents_Automatically));

					this->set_feature(
						eZirconSDKFeatures::
							kSDK_Feature_AddRequiredComponents_Automatically,
						status);

					int quality = file.Get<int>(
						translate_zircon_sdk_features(eZirconSDKFeatures::
								kSDK_Feature_SphereBoundingBox_Quality));

					this->set_feature(
						eZirconSDKFeatures::
							kSDK_Feature_SphereBoundingBox_Quality,
						quality);
				}
			}
		}
	}
}

void zircon_config::initialize_default() noexcept
{
	this->set_feature(
		eZirconSDKFeatures::kSDK_Feature_AddRequiredComponents_Automatically,
		true);
}

kotek::cstring_t translate_zircon_sdk_features(eZirconSDKFeatures features)
{
	if (KOTEK_CHECK_FLAG(features,
			eZirconSDKFeatures::
				kSDK_Feature_AddRequiredComponents_Automatically))
	{
		return "add_required_components_automatically";
	}
	else if (KOTEK_CHECK_FLAG(features,
				 eZirconSDKFeatures::kSDK_Feature_SphereBoundingBox_Quality))
	{
		return "sphere_bounding_box_quality";
	}
	else if (KOTEK_CHECK_FLAG(
				 features, eZirconSDKFeatures::kSDK_Feature_Unknown))
	{
		return "unknown";
	}
	else
	{
		return "ENUM_zircon_SDK_FEATURES_UNDEFINED";
	}
}

Kotek::ktk::cstring translate_zircon_game_features(eZirconGameFeatures features)
{
	KOTEK_ASSERT(false, "not implemented");

	return Kotek::ktk::cstring();
}
