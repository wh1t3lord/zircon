#include "zircon_ui_window_settings.h"
#include "../../engine/zircon_game_manager.h"
#include "../../core/zircon_config.h"
#include "zircon_editor_ui_state.h"

zircon_ui_window_settings::zircon_ui_window_settings(void) :
	m_is_window_show(false)
{
}

zircon_ui_window_settings::~zircon_ui_window_settings(void) {}

void zircon_ui_window_settings::initialize(void) {}

void zircon_ui_window_settings::shutdown(void) {}

void zircon_ui_window_settings::Draw(Kotek::Core::ktkMainManager* main_manager)
{
	if (!this->m_is_window_show)
		return;

	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();
	auto* p_game_manager =
		static_cast<zircon_manager_game*>(main_manager->GetGameManager());

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Settings"))
		{
			if (p_wrapper_imgui->CollapsingHeader("Features"))
			{
				if (p_game_manager)
				{
					auto* p_config = p_game_manager->get_config();

					if (p_config)
					{
						// required components
						bool status = p_config->is_feature_enabled(
							eZirconSDKFeatures::
								kSDK_Feature_AddRequiredComponents_Automatically);

						if (p_wrapper_imgui->Checkbox(
								translate_zircon_sdk_features(
									eZirconSDKFeatures::
										kSDK_Feature_AddRequiredComponents_Automatically)
									.c_str(),
								&status))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_AddRequiredComponents_Automatically,
								status);
						}

						// quality for sphere bounding box generation
						status =
							p_config->is_feature_enabled(eZirconSDKFeatures::
									kSDK_Feature_SphereBoundingBox_Quality);

						if (p_wrapper_imgui->Checkbox(
								translate_zircon_sdk_features(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality)
									.c_str(),
								&status))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_SphereBoundingBox_Quality,
								status);
						}

						if (status)
						{
							int quality = 0;

							if (p_config->is_feature_enabled(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality))
							{
								quality = p_config->get_feature<int>(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality);
							}

							if (quality == 0)
							{
								quality = 3;
							}

							if (p_wrapper_imgui->DragInt(
									"SBB quality", &quality, 1.0, 3, 50))
							{
								p_config->set_feature(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality,
									quality);
							}
						}
					}
				}
			}
		}

		p_wrapper_imgui->End();
	}
}

int zircon_ui_window_settings::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Settings);
}

void zircon_ui_window_settings::Show(void)
{
	this->m_is_window_show = true;
}

void zircon_ui_window_settings::Hide(void)
{
	this->m_is_window_show = false;
}

bool zircon_ui_window_settings::Is_Shown(void) const
{
	return this->m_is_window_show;
}
