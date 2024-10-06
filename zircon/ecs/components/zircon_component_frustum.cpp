#include "zircon_component_frustum.h"

zircon_component_frustum::zircon_component_frustum(void) {}

zircon_component_frustum::~zircon_component_frustum(void) {}

void zircon_component_frustum::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("Frustum"))
			{
			}
		}
	}
}
