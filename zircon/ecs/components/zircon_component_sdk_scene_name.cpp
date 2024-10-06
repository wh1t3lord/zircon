#include "zircon_component_sdk_scene_name.h"

zircon_component_sdk_scene_name::zircon_component_sdk_scene_name() {}

zircon_component_sdk_scene_name::~zircon_component_sdk_scene_name() {}

void zircon_component_sdk_scene_name::DrawImGui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->CollapsingHeader("Scene name"))
		{
			char buffer[128];

#ifdef KOTEK_USE_PLATFORM_WINDOWS
            memcpy_s(
                buffer, sizeof(buffer), this->m_name.c_str(), sizeof(buffer));
#else
            memcpy(buffer, this->m_name.c_str(), sizeof(buffer));
#endif

			if (p_wrapper_imgui->InputText("Name", buffer, 128))
			{
				this->m_name = buffer;
			}
		}
	}
}

const Kotek::ktk::cstring& zircon_component_sdk_scene_name::GetName(
	void) const noexcept
{
	return this->m_name;
}

void zircon_component_sdk_scene_name::SetName(
    const Kotek::ktk::cstring& name) noexcept
{
	this->m_name = name;
}
