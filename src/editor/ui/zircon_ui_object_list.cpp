#include "zircon_ui_object_list.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../../world/zircon_world_manager.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_editor_ui_state.h"

zircon_editor_ui_window_object_list::zircon_editor_ui_window_object_list(
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::core::ktkConsole* p_console,
	zircon_factory* p_factory) :
	m_is_show_window{}, m_amount_of_entites{}, m_selected_entity_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_console{p_console}, m_p_factory{p_factory}
{
	KOTEK_ASSERT(
		p_manager_session_editor, "valid pointer is expected"
	);
	KOTEK_ASSERT(p_console, "valid pointer is expected");
	KOTEK_ASSERT(p_factory, "valid pointer is expected");
}

zircon_editor_ui_window_object_list::~zircon_editor_ui_window_object_list(void) {}

void zircon_editor_ui_window_object_list::Initialize(void) {}

void zircon_editor_ui_window_object_list::Shutdown(void) {}

void zircon_editor_ui_window_object_list::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"you have to pass a valid session editor manager pointer!");
	KOTEK_ASSERT(
		this->m_p_console, "you have to pass a valid pointer of console!");

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING("initialize session editor!");
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world,
		"session editor_{}#{} must contain a valid pointer of zircon_world",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING("set world to session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING("factory that you've passed is invalid!");
		return;
	}

	// TODO(zircon): enumeration is capped by this fixed buffer; entities
	// beyond ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT are not listed
	kotek::entity_t ids[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
	kotek::uint32_t ids_count = this->m_p_factory->get_all_entities(
		p_world->get_ecs_context(), p_world->get_entity_count_max_limit(),
		ids, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);

	kotek::core::ktkIImguiWrapper* p_wrapper_imgui =
		p_main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		p_wrapper_imgui->ShowDemoWindow();

		if (!this->m_is_show_window)
			return;

		if (p_wrapper_imgui->Begin("Entity List"))
		{
			if (p_wrapper_imgui->Button("Add"))
			{
				if (this->m_p_console)
				{
					this->m_p_console->Execute_Command(
						static_cast<kotek::ktk::enum_base_t>(
							kotek::core::eConsoleCommandIndex::
								kConsoleCommand_SDK_CreateEntity));
				}
			}

			p_wrapper_imgui->SameLine();

			if (p_wrapper_imgui->Button("Delete"))
			{
				if (this->m_p_console)
				{
					zircon_factory* p_factory = p_world->get_factory();

					if (p_factory)
					{
						if (p_factory->is_valid_entity(
								p_world->get_ecs_context(),
								this->m_selected_entity_id))
						{
							this->m_p_console->Execute_Command(
								static_cast<kotek::ktk::enum_base_t>(
									kotek::core::eConsoleCommandIndex::
										kConsoleCommand_SDK_DeleteEntity),
								{{static_cast<kotek::uint32_t>(
									this->m_selected_entity_id.id)}});
						}
					}
				}
			}

			if (p_wrapper_imgui->BeginTable("", 1))
			{
				kotek::array_t<char, 32> table_column_name{};
				kotek::ktk::sprintf(table_column_name.data(),
					table_column_name.size(), "Entity ID [%zu]",
					this->m_amount_of_entites);
				p_wrapper_imgui->TableSetupColumn(
					table_column_name.data(), ImGuiTableColumnFlags_WidthStretch);
				p_wrapper_imgui->TableHeadersRow();

				kotek::size_t i = 0;
				for (kotek::uint32_t index = 0; index < ids_count; ++index)
				{
					kotek::entity_t id = ids[index];

					p_wrapper_imgui->TableNextRow();
					p_wrapper_imgui->TableSetColumnIndex(0);

					kotek::static_cstring_t<128> converted_id;

					bool has_sdk_name =
						p_world->get_factory()->has_component(
							p_world->get_ecs_context(), id,
							eZirconComponentType::
								kzircon_component_sdk_scene_name);
					if (has_sdk_name)
					{
						auto* p_component =
							static_cast<zircon_component_sdk_scene_name*>(
								p_world->get_factory()
									->get_component_by_enum(
										p_world->get_ecs_context(), id,
										eZirconComponentType::
											kzircon_component_sdk_scene_name));

						KOTEK_ASSERT(
							p_component, "must be valid component!");

						if (p_component)
						{
							converted_id = kotek::ktk::static_format<128>(
								"[{}]", p_component->get_name());
						}
					}
					else
					{
						converted_id = Kotek::ktk::static_format<128>(
							"{}", static_cast<kotek::uint32_t>(id.id));
					}

					if (p_wrapper_imgui->Selectable(converted_id.c_str(),
							id.id == this->m_selected_entity_id.id))
					{
						this->m_selected_entity_id = id;
						KOTEK_ASSERT(
							p_session->get_ui_state(), "must be valid!");
						p_session->get_ui_state()->set_selected_entity(
							this->m_selected_entity_id);
					}

					if (has_sdk_name)
					{
						p_wrapper_imgui->SameLine();
						p_wrapper_imgui->TextDisabled(
							"(%d)", static_cast<kotek::uint32_t>(id.id));
					}

					++i;
				}

				this->m_amount_of_entites = i;
				p_wrapper_imgui->EndTable();
			}
		}

		p_wrapper_imgui->End();
	}
}

int zircon_editor_ui_window_object_list::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_ObjectList);
}

void zircon_editor_ui_window_object_list::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_object_list::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_object_list::Is_Shown(void) const
{
	return this->m_is_show_window;
}
