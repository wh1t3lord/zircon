#include "zircon_ui_window_settings.h"
#include "../../core/zircon_config.h"
#include "../../core/zircon_localization_manager.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_settings::
	zircon_editor_ui_window_settings(zircon_config* p_config,
		zircon_localization_manager* p_localization) :
	m_is_window_show(false),
	m_p_config{p_config},
	m_p_localization{p_localization}
{
}

zircon_editor_ui_window_settings::
	~zircon_editor_ui_window_settings(void)
{
}

void zircon_editor_ui_window_settings::Initialize(void) {}

void zircon_editor_ui_window_settings::Shutdown(void) {}

const char* zircon_editor_ui_window_settings::translate(
	const char* p_key) const noexcept
{
	if (this->m_p_localization == nullptr)
	{
		KOTEK_ASSERT(false,
			"the settings window needs a valid "
			"zircon_localization_manager (the game manager injects it)");
		return p_key;
	}

	return this->m_p_localization->translate(
		eZirconLocalizationInstance::kEditor, p_key);
}

void zircon_editor_ui_window_settings::Draw(
	kotek::Core::ktkMainManager* p_main_manager
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
		// the window title is the imgui ID and the imgui.ini section
		// name: the "en" table keeps it byte-identical "Settings" so the
		// existing docking/layout stays valid, while a non-default
		// language simply opens its own ini section (imgui keys layout
		// by the title string — that is inherent to string-table
		// localization and applies to every migrated window)
		if (p_wrapper_imgui->Begin(this->translate("settings.window_title")))
		{
			if (p_wrapper_imgui->CollapsingHeader(
					this->translate("settings.header.features")))
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
								this->translate(
									"settings.feature.add_required_components"
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
								this->translate(
									"settings.feature.sphere_bounding_box_quality"
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
									this->translate(
										"settings.feature.sbb_quality"
									),
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

						// editor camera rotation representation (task
						// Z20): quaternion accumulation instead of
						// euler yaw/pitch; the session's camera driver
						// reads the flag every frame, so the toggle
						// applies live
						status = p_config->is_feature_enabled(
							eZirconSDKFeatures::
								kSDK_Feature_SDKCamera_Rotation_Quaternion
						);

						if (p_wrapper_imgui->Checkbox(
								this->translate(
									"settings.feature.sdk_camera_rotation_quaternion"
								),
								&status
							))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_SDKCamera_Rotation_Quaternion,
								status
							);
						}

						// the editor camera bootstrap entity (task
						// Z20, owner clarification: opt-out) —
						// auto-create the sdk_camera+sdk_input+
						// transform entity when an editor session's
						// world starts empty; OFF leaves the world
						// untouched (e.g. for a scene that always
						// brings its own camera)
						status = p_config->is_feature_enabled(
							eZirconSDKFeatures::
								kSDK_Feature_AddSdkCameraInputBootstrap_Automatically
						);

						if (p_wrapper_imgui->Checkbox(
								this->translate(
									"settings.feature.sdk_camera_input_bootstrap"
								),
								&status
							))
						{
							p_config->set_feature(
								eZirconSDKFeatures::
									kSDK_Feature_AddSdkCameraInputBootstrap_Automatically,
								status
							);
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
