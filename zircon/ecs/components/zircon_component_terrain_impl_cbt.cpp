#include "zircon_component_terrain_impl_cbt.h"

zircon_component_terrain_impl_cbt::zircon_component_terrain_impl_cbt() {}

zircon_component_terrain_impl_cbt::~zircon_component_terrain_impl_cbt() {}

void zircon_component_terrain_impl_cbt::DrawImGui(
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
