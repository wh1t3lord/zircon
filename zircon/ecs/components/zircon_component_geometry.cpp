#include "zircon_component_geometry.h"
#include "../../engine/zircon_game_manager.h"
#include "../../editor/ui/zircon_editor_ui_state.h"
#include "../../editor/zircon_session_editor.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_geometry::zircon_component_geometry(void) :
	m_is_enabled{true},
	m_component_type{kComponentTypezircon_component_geometry},
	m_is_use_model{true}, m_is_visible{true},
	m_geometry_type{kotek::core::eStaticGeometryType::kUnknown},
	m_p_geometry_name{}
#ifdef KOTEK_USE_SDK_IMGUI
	,
	m_vertex_count{}, m_index_count{}
#endif
{
}

zircon_component_geometry::~zircon_component_geometry(void) {}

kotek::size_t zircon_component_geometry::get_vertex_count(void) const noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	return this->m_vertex_count;
#else
	return 0;
#endif
}

void zircon_component_geometry::set_vertex_count(kotek::size_t count) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	this->m_vertex_count = count;
#endif
}

kotek::size_t zircon_component_geometry::get_index_count(void) const noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	return this->m_index_count;
#else
	return 0;
#endif
}

void zircon_component_geometry::set_index_count(kotek::size_t count) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	this->m_index_count = count;
#endif
}

const char* zircon_component_geometry::get_path(void) const noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	return this->m_path.c_str();
#endif
}

void zircon_component_geometry::set_path(
	const kotek::ktk::cstring& path) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	this->m_path = path;
#endif
}

bool zircon_component_geometry::is_visible(void) const noexcept
{
	return this->m_is_visible;
}

void zircon_component_geometry::set_visible(bool status) noexcept
{
	this->m_is_visible = status;
}

void zircon_component_geometry::draw_imgui(
	kotek::core::ktkMainManager* p_main_manager) noexcept
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
					this->m_p_geometry_name =
						kotek::core::helper::Translate_StaticGeometryType(
							this->m_geometry_type);

					if (p_wrapper_imgui->BeginCombo(
							"Static geometry", this->m_p_geometry_name))
					{
						for (kotek::ktk::enum_base_t i = 0; i <
							static_cast<kotek::ktk::enum_base_t>(
								kotek::core::eStaticGeometryType::kEndOfEnum);
							++i)
						{
							const auto& translated_name = kotek::core::helper::
								Translate_StaticGeometryType(static_cast<
									kotek::core::eStaticGeometryType>(i));

							if (p_wrapper_imgui->Selectable(translated_name,
									std::string_view(this->m_p_geometry_name) ==
										translated_name))
							{
								this->m_geometry_type = static_cast<
									kotek::core::eStaticGeometryType>(i);

								auto* p_game_manager =
									static_cast<zircon_game_manager*>(
										p_main_manager->GetGameManager());

								zircon_session_editor* p_session =
									p_game_manager->get_session_editor(
										p_game_manager
											->get_session_editor_id());

								KOTEK_ASSERT(p_session, "must be initialized");
								if (!p_session)
								{
									KOTEK_MESSAGE_WARNING(
										"failed to obtain session editor by "
									    "id: {}",
										p_game_manager
											->get_session_editor_id());
									return;
								}

								auto entity_id = p_session->get_ui_state()->get_selected_entity();

								p_main_manager->GetGameManager()
									->GetConsole()
									->Push_Command(
										static_cast<kotek::ktk::enum_base_t>(
											kotek::core::eConsoleCommandIndex::
												kConsoleCommand_ResourceManager_Load),
										{kotek::core::ktkLoadingRequest(
											kotek::core::
												eResourceThreadingPolicy::
													kAsync,
											kotek::core::
												eResourceLoadingPolicy::kStream,
											kotek::core::
												eResourceCachingPolicy::
													kWithoutCache,
											kotek::core::eResourceLoadingType::
												kModelStatic_Triangle,
											"",
											static_cast<kotek::uint32_t>(
												entity_id))});
							}
						}

						p_wrapper_imgui->EndCombo();
					}
				}
			}
		}
	}
}

kotek::json::value zircon_component_geometry::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_geometry::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_geometry>(data);
}

kotek::json::value zircon_component_geometry::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_geometry::get_component_type(
	void) const noexcept
{
	return m_component_type;
}

bool zircon_component_geometry::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_geometry::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

kotek::core::eStaticGeometryType
zircon_component_geometry::get_geometry_type() const noexcept
{
	return this->m_geometry_type;
}

void zircon_component_geometry::set_geometry_type(
	kotek::core::eStaticGeometryType type) noexcept
{
	this->m_geometry_type = type;
}
