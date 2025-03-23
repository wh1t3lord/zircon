#include "zircon_component_terrain_gcm.h"

zircon_component_terrain_gcm::zircon_component_terrain_gcm() {}

zircon_component_terrain_gcm::~zircon_component_terrain_gcm() {}

void zircon_component_terrain_gcm::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader(
				"Component Terrain Implementation - CBT"))
			{

			}
		}
	}
}
