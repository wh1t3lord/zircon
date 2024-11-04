#include "zircon_component_geometry.h"
#include "../../game/zircon_game_manager.h"
#include "../../core/zircon_sdk_ui.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_geometry::zircon_component_geometry(void) :
	m_is_use_model{true},
	m_geometry_type{Kotek::Core::eStaticGeometryType::kUnknown},
	m_vertex_count{}, m_index_count{}
{
}

zircon_component_geometry::~zircon_component_geometry(void) {}

Kotek::ktk::size_t zircon_component_geometry::GetVertexCount(
	void) const noexcept
{
	return this->m_vertex_count;
}

void zircon_component_geometry::SetVertexCount(
	Kotek::ktk::size_t count) noexcept
{
	this->m_vertex_count = count;
}

Kotek::ktk::size_t zircon_component_geometry::GetIndexCount(void) const noexcept
{
	return this->m_index_count;
}

void zircon_component_geometry::SetIndexCount(Kotek::ktk::size_t count) noexcept
{
	this->m_index_count = count;
}

const Kotek::ktk::cstring& zircon_component_geometry::GetPath(
	void) const noexcept
{
	return this->m_path;
}

void zircon_component_geometry::SetPath(
	const Kotek::ktk::cstring& path) noexcept
{
	this->m_path = path;
}

bool zircon_component_geometry::is_visible(void) const noexcept
{
	return this->m_is_visible;
}

void zircon_component_geometry::set_visible(bool status) noexcept
{
	this->m_is_visible = status;
}

void zircon_component_geometry::Clear(void) noexcept
{
	this->m_index_count = {};
	this->m_vertex_count = {};
	this->m_path = "";
}
void zircon_component_geometry::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("Component geometry"))
			{
				p_wrapper_imgui->Checkbox("Use model", &this->m_is_use_model);
				p_wrapper_imgui->SameLine();

				if (this->m_is_use_model)
				{
					p_wrapper_imgui->Button("Load model");
				}
				else
				{
					this->m_geometry_name =
						Kotek::Core::helper::Translate_StaticGeometryType(
							this->m_geometry_type);

					if (p_wrapper_imgui->BeginCombo(
							"Static geometry", this->m_geometry_name.c_str()))
					{
						for (Kotek::ktk::enum_base_t i = 0; i <
							 static_cast<Kotek::ktk::enum_base_t>(
								 Kotek::Core::eStaticGeometryType::kEndOfEnum);
							 ++i)
						{
							const auto& translated_name = Kotek::Core::helper::
								Translate_StaticGeometryType(static_cast<
									Kotek::Core::eStaticGeometryType>(i));

							if (p_wrapper_imgui->Selectable(
									translated_name.c_str(),
									this->m_geometry_name == translated_name))
							{
								this->m_geometry_type = static_cast<
									Kotek::Core::eStaticGeometryType>(i);

								auto* p_game_manager =
									static_cast<zircon_manager_game*>(
										p_main_manager->GetGameManager());

								auto entity_id =
									p_game_manager->get_sdk_ui()
										->get_selected_entity();

								p_main_manager->GetGameManager()
									->GetConsole()
									->PushCommand(
										static_cast<Kotek::ktk::enum_base_t>(
											Kotek::Core::eConsoleCommandIndex::
												kConsoleCommand_ResourceManager_Load),
										{Kotek::Core::ktkLoadingRequest(
											Kotek::Core::
												eResourceLoadingPolicy::kAsync,
											Kotek::Core::
												eResourceCachingPolicy::
													kWithoutCache,
											Kotek::Core::eResourceLoadingType::
												kModelStatic_Triangle,
											"", static_cast<kotek::uint32_t>(entity_id))});
							}
						}

						p_wrapper_imgui->EndCombo();
					}
				}
			}
		}
	}
}
