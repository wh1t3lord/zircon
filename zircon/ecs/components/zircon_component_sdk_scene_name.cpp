#include "zircon_component_sdk_scene_name.h"

zircon_component_sdk_scene_name::zircon_component_sdk_scene_name() {}

zircon_component_sdk_scene_name::~zircon_component_sdk_scene_name() {}

void zircon_component_sdk_scene_name::DrawImGui(
	kotek::core::ktkMainManager* main_manager) noexcept
{
	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->CollapsingHeader("Scene name"))
		{
			char buffer[ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH]{};

			kotek::ktk::memory::memcpy(buffer, this->m_name.data(),
				ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH);

			if (p_wrapper_imgui->InputText("Name", buffer,
					ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH))
			{
				this->m_name = buffer;
			}
		}
	}
}

const char* zircon_component_sdk_scene_name::get_name(
	void) const noexcept
{
	return this->m_name.c_str();
}

void zircon_component_sdk_scene_name::set_name(
    const kotek::static_cstring_view_t& name) noexcept
{
	this->m_name = name.data();
}
