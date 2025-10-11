#include "zircon_ui_window_settings.h"
#include "../../core/zircon_config.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_settings::
	zircon_editor_ui_window_settings(zircon_config* p_config) :
	m_is_window_show(false), m_p_config{p_config}
{
}

zircon_editor_ui_window_settings::
	~zircon_editor_ui_window_settings(void)
{
}

void zircon_editor_ui_window_settings::Initialize(void) {}

void zircon_editor_ui_window_settings::Shutdown(void) {}

void zircon_editor_ui_window_settings::Draw(
	Kotek::Core::ktkMainManager* p_main_manager
)
{
	if (!this->m_is_window_show)
		return;

	KOTEK_ASSERT(
		this->m_p_config,
		"you must pass a valid pointer of zircon_config "
		"instance!"
	);

	auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->Begin("Settings"))
		{
			if (p_wrapper_imgui->CollapsingHeader("Features"))
			{
				if (p_main_manager->GetGameManager())
				{
					auto* p_config = this->m_p_config;

					if (p_config)
					{
						// required components
						bool status =
							p_config->is_feature_enabled(
								eZirconSDKFeatures::
									kSDK_Feature_AddRequiredComponents_Automatically
							);

						if (p_wrapper_imgui->Checkbox(
								translate_zircon_sdk_features(
									eZirconSDKFeatures::
										kSDK_Feature_AddRequiredComponents_Automatically
								),
								&status
							))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_AddRequiredComponents_Automatically,
								status
							);
						}

						// quality for sphere bounding box
						// generation
						status = p_config->is_feature_enabled(
							eZirconSDKFeatures::
								kSDK_Feature_SphereBoundingBox_Quality
						);

						if (p_wrapper_imgui->Checkbox(
								translate_zircon_sdk_features(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality
								),
								&status
							))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_SphereBoundingBox_Quality,
								status
							);
						}

						if (status)
						{
							int quality = 0;

							if (p_config->is_feature_enabled(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality
								))
							{
								quality = p_config->get_feature<
									int>(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality
								);
							}

							if (quality == 0)
							{
								quality = 3;
							}

							if (p_wrapper_imgui->DragInt(
									"SBB quality",
									&quality,
									1.0,
									3,
									50
								))
							{
								p_config->set_feature(
									eZirconSDKFeatures::
										kSDK_Feature_SphereBoundingBox_Quality,
									quality
								);
							}
						}
					}
				}
			}
		}

		p_wrapper_imgui->End();
	}
}

int zircon_editor_ui_window_settings::Get_ID(void) const
{
	return static_cast<int>(
		eZirconWindowIDs::kWindow_SDK_Settings
	);
}

void zircon_editor_ui_window_settings::Show(void)
{
	this->m_is_window_show = true;
}

void zircon_editor_ui_window_settings::Hide(void)
{
	this->m_is_window_show = false;
}

bool zircon_editor_ui_window_settings::Is_Shown(void) const
{
	return this->m_is_window_show;
}
