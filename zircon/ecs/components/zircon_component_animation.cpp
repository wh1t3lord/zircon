#include "zircon_component_animation.h"

zircon_component_animation::zircon_component_animation(void) {}

zircon_component_animation::~zircon_component_animation(void) {}

void zircon_component_animation::Clear(void) noexcept {}

void zircon_component_animation::DrawImGui(
	Kotek::Core::ktkMainManager* main_manager) noexcept
{
	if (main_manager)
	{
		kotek::core::ktkIImguiWrapper* p_imgui_wrapper =
			main_manager->Get_ImguiWrapper();

		if (p_imgui_wrapper)
		{
			if (p_imgui_wrapper->BeginTabBar("ZirconComponentAnimationTabBar"))
			{
				if (p_imgui_wrapper->BeginTabItem("info"))
				{
					p_imgui_wrapper->EndTabItem();
				}

				if (p_imgui_wrapper->BeginTabItem("edit"))
				{
					p_imgui_wrapper->EndTabItem();
				}

				p_imgui_wrapper->EndTabBar();
			}
		}
	}
}
