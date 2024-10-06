#include "zircon_component_terrain.h"

zircon_component_terrain::zircon_component_terrain() {}

zircon_component_terrain::~zircon_component_terrain() {}

void zircon_component_terrain::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			p_wrapper_imgui->Text("Terrain");
		}
	}
}
