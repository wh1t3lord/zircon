#include "zircon_ui_csg.h"

#include "../../ecs/zircon_factory.h"
#include "../../ecs/zircon_component_csg.h"
#include "../../ecs/zircon_component_csg_primitive.h"
#include "../../world/zircon_world.h"
#include "../../core/zircon_session_editor_interface.h"
#include "../../core/zircon_editor_command_history_interface.h"
#include "../commands/zircon_command_csg_create_primitive.h"
#include "../commands/zircon_command_csg_delete_primitive.h"
#include "../commands/zircon_command_csg_edit_primitive.h"
#include "../../core/zircon_editor_ui_state_interface.h"
#include "zircon_ui_test_harness.h"

#include <cmath>

namespace
{
	const char* zircon_csg_type_name(eZirconCsgPrimitiveType type) noexcept
	{
		switch (type)
		{
		case eZirconCsgPrimitiveType::kBox:
			return "box";
		case eZirconCsgPrimitiveType::kWedge:
			return "wedge";
		case eZirconCsgPrimitiveType::kCylinder:
			return "cylinder";
		default:
			return "unknown";
		}
	}
} // namespace

zircon_editor_ui_window_csg::zircon_editor_ui_window_csg(
	zircon_session_editor_manager_interface* p_manager_session_editor,
	kotek::core::ktkConsole* p_console, zircon_factory* p_factory,
	zircon_ui_test_widget_registry* p_widget_registry) :
	m_is_show_window{},
	m_selected_primitive_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_console{p_console}, m_p_factory{p_factory}, m_p_imgui_wrapper{},
	m_p_widget_registry{p_widget_registry},
	m_dimensions{2.0f, 2.0f, 2.0f}, m_position{0.0f, 0.0f, 0.0f},
	m_rotation{0.0f, 0.0f, 0.0f, 1.0f}, m_material_id{},
	m_is_subtractive{}
{
	KOTEK_ASSERT(p_manager_session_editor,
		"a valid editor session manager is required");
	KOTEK_ASSERT(p_console, "a valid console is required");
	KOTEK_ASSERT(p_factory, "a valid factory is required");
}

zircon_editor_ui_window_csg::~zircon_editor_ui_window_csg(void) {}

void zircon_editor_ui_window_csg::Initialize(void) {}

void zircon_editor_ui_window_csg::Shutdown(void) {}

zircon_session_editor_interface* zircon_editor_ui_window_csg::get_session(
	void) const noexcept
{
	if (!this->m_p_manager_session_editor)
		return nullptr;

	return this->m_p_manager_session_editor->get_session(
		this->m_p_manager_session_editor->get_current_session_id());
}

zircon_world* zircon_editor_ui_window_csg::get_world(void) const noexcept
{
	zircon_session_editor_interface* p_session = this->get_session();
	return p_session ? p_session->get_world() : nullptr;
}

zircon_component_csg* zircon_editor_ui_window_csg::get_selected_compound(
	void) const noexcept
{
	zircon_session_editor_interface* p_session = this->get_session();
	zircon_world* p_world = this->get_world();

	if (!p_session || !p_world || !p_session->get_ui_state())
		return nullptr;

	const kotek::entity_t id = p_session->get_ui_state()->get_selected_entity();

	if (!this->m_p_factory->is_valid_entity(
			p_world->get_ecs_context(), id))
	{
		return nullptr;
	}

	return static_cast<zircon_component_csg*>(
		this->m_p_factory->get_component_by_enum(p_world->get_ecs_context(), id,
			eZirconComponentType::kzircon_component_csg));
}

zircon_component_csg_primitive*
zircon_editor_ui_window_csg::get_selected_primitive(void) const noexcept
{
	zircon_component_csg* p_compound = this->get_selected_compound();
	zircon_world* p_world = this->get_world();

	if (!p_compound || !p_world ||
		!p_compound->has_primitive_entity(this->m_selected_primitive_id))
	{
		return nullptr;
	}

	return static_cast<zircon_component_csg_primitive*>(
		this->m_p_factory->get_component_by_enum(p_world->get_ecs_context(),
			this->m_selected_primitive_id,
			eZirconComponentType::kzircon_component_csg_primitive));
}

void zircon_editor_ui_window_csg::create_compound(void) noexcept
{
	zircon_session_editor_interface* p_session = this->get_session();
	zircon_world* p_world = this->get_world();

	if (!p_session || !p_world || !this->m_p_console)
		return;

	this->m_p_console->Execute_Command(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::kConsoleCommand_SDK_CreateEntity));

	kotek::entity_t newest{kotek::ktk::kInvalidECSEntity};

	for (kotek::uint32_t raw_id = 0;
		 raw_id < p_world->get_entity_count_max_limit(); ++raw_id)
	{
		const kotek::entity_t id{raw_id};

		if (this->m_p_factory->is_valid_entity(p_world->get_ecs_context(), id))
			newest = id;
	}

	if (ecs_is_invalid_entity(newest))
		return;

	this->m_p_console->Execute_Command(
		static_cast<kotek::ktk::enum_base_t>(
			kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_CreateComponentForEntityByName),
		{{"zircon_component_csg"},
			{static_cast<kotek::uint32_t>(newest.id)}});

	p_session->get_ui_state()->set_selected_entity(newest);
	this->m_selected_primitive_id = kotek::entity_t{kotek::ktk::kInvalidECSEntity};
}

void zircon_editor_ui_window_csg::create_primitive(
	eZirconCsgPrimitiveType type) noexcept
{
	zircon_component_csg* p_compound = this->get_selected_compound();
	zircon_session_editor_interface* p_session = this->get_session();

	if (!p_compound || !p_session)
		return;

	zircon_editor_command_history_interface* p_history = p_session->get_command_history();
	const kotek::entity_t compound_id =
		p_session->get_ui_state()->get_selected_entity();

	if (!p_history)
		return;

	const double dimensions[3] = {2.0, 2.0, 2.0};
	const double position[3] = {0.0, 0.0, 0.0};
	const double rotation[4] = {0.0, 0.0, 0.0, 1.0};

	unsigned char* p_memory = p_history->allocate_memory_for_command(
		sizeof(zircon_command_csg_create_primitive),
		"zircon_command_csg_create_primitive");

	auto* p_command = new (p_memory) zircon_command_csg_create_primitive(
		this->m_p_manager_session_editor, this->m_p_factory, compound_id, type,
		dimensions, position, rotation, 0,
		static_cast<kotek::uint16_t>(this->m_material_id));

	p_history->ExecuteCommand(p_command);
	this->m_selected_primitive_id = p_command->GetEntityID();
}

void zircon_editor_ui_window_csg::delete_selected_primitive(void) noexcept
{
	zircon_component_csg* p_compound = this->get_selected_compound();
	zircon_component_csg_primitive* p_primitive = this->get_selected_primitive();
	zircon_session_editor_interface* p_session = this->get_session();

	if (!p_compound || !p_primitive || !p_session)
		return;

	zircon_editor_command_history_interface* p_history = p_session->get_command_history();

	if (!p_history)
		return;

	unsigned char* p_memory = p_history->allocate_memory_for_command(
		sizeof(zircon_command_csg_delete_primitive),
		"zircon_command_csg_delete_primitive");

	auto* p_command = new (p_memory) zircon_command_csg_delete_primitive(
		this->m_p_manager_session_editor, this->m_p_factory,
		p_session->get_ui_state()->get_selected_entity(),
		this->m_selected_primitive_id);

	p_history->ExecuteCommand(p_command);
	this->m_selected_primitive_id = kotek::entity_t{kotek::ktk::kInvalidECSEntity};
}

void zircon_editor_ui_window_csg::commit_subtractive(
	bool is_subtractive) noexcept
{
	this->m_is_subtractive = is_subtractive;
	this->commit_edit();
}

void zircon_editor_ui_window_csg::commit_edit(void) noexcept
{
	zircon_component_csg_primitive* p_primitive = this->get_selected_primitive();
	zircon_session_editor_interface* p_session = this->get_session();

	if (!p_primitive || !p_session)
		return;

	zircon_editor_command_history_interface* p_history = p_session->get_command_history();

	if (!p_history)
		return;

	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;
	zircon_component_csg_primitive edited = *p_primitive;

	for (kotek::uint8_t axis = 0; axis < 3; ++axis)
	{
		edited.set_dimension(axis, traits_t::from_double(this->m_dimensions[axis]));
		edited.set_position_axis(axis,
			traits_t::from_double(this->m_position[axis]));
	}

	float rotation_length_squared = 0.0f;

	for (float component : this->m_rotation)
		rotation_length_squared += component * component;

	if (rotation_length_squared > 1e-8f)
	{
		const float inverse_length = 1.0f / std::sqrt(rotation_length_squared);

		for (kotek::uint8_t component = 0; component < 4; ++component)
		{
			edited.set_rotation_component(component,
				traits_t::from_double(this->m_rotation[component] * inverse_length));
		}
	}
	else
	{
		edited.set_rotation(traits_t::from_double(0.0), traits_t::from_double(0.0),
			traits_t::from_double(0.0), traits_t::from_double(1.0));
	}

	edited.set_material_id(static_cast<kotek::uint16_t>(this->m_material_id));
	edited.set_subtractive(this->m_is_subtractive);

	zircon_command_text_state state_after;
	state_after.capture(&edited);
	unsigned char* p_memory = p_history->allocate_memory_for_command(
		sizeof(zircon_command_csg_edit_primitive),
		"zircon_command_csg_edit_primitive");

	auto* p_command = new (p_memory) zircon_command_csg_edit_primitive(
		this->m_p_manager_session_editor, this->m_p_factory,
		p_session->get_ui_state()->get_selected_entity(),
		this->m_selected_primitive_id, state_after);

	p_history->ExecuteCommand(p_command);
}

void zircon_editor_ui_window_csg::track(const char* p_label) noexcept
{
	zircon_ui_test_track_widget(this->m_p_imgui_wrapper,
		this->m_p_widget_registry, zircon_ui_test_window_names::kCsg, p_label);
}

void zircon_editor_ui_window_csg::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	if (!this->m_is_show_window || !p_main_manager)
		return;

	kotek::core::ktkIImguiWrapper* p_wrapper = p_main_manager->Get_ImguiWrapper();

	if (!p_wrapper)
		return;

	this->m_p_imgui_wrapper = p_wrapper;
	p_wrapper->SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_Once);

	if (!p_wrapper->Begin("CSG"))
	{
		p_wrapper->End();
		return;
	}

	if (p_wrapper->Button("New compound"))
		this->create_compound();
	this->track("button:New compound");

	zircon_component_csg* p_compound = this->get_selected_compound();

	if (!p_compound)
	{
		p_wrapper->TextDisabled(
			"select an entity with zircon_component_csg, or create one");
		p_wrapper->End();
		return;
	}

	p_wrapper->SameLine();
	p_wrapper->Text("primitives: %u", p_compound->get_primitive_count());

	if (p_wrapper->Button("Add box"))
		this->create_primitive(eZirconCsgPrimitiveType::kBox);
	this->track("button:Add box");

	if (p_wrapper->Button("Add wedge"))
		this->create_primitive(eZirconCsgPrimitiveType::kWedge);
	this->track("button:Add wedge");

	if (p_wrapper->Button("Add cylinder"))
		this->create_primitive(eZirconCsgPrimitiveType::kCylinder);
	this->track("button:Add cylinder");

	p_wrapper->SeparatorText("brushes");

	{
		for (const kotek::entity_t id : p_compound->get_primitive_entities())
		{
			auto* p_primitive = static_cast<zircon_component_csg_primitive*>(
				this->m_p_factory->get_component_by_enum(
					this->get_world()->get_ecs_context(), id,
					eZirconComponentType::kzircon_component_csg_primitive));

			if (!p_primitive)
				continue;

			kotek::array_t<char, 96> label{};
			kotek::ktk::sprintf(label.data(), label.size(), "%s [%u]",
				zircon_csg_type_name(p_primitive->get_primitive_type()),
				static_cast<kotek::uint32_t>(id.id));

			if (p_wrapper->Selectable(
					label.data(), id.id == this->m_selected_primitive_id.id))
			{
				using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;
				this->m_selected_primitive_id = id;

				for (kotek::uint8_t axis = 0; axis < 3; ++axis)
				{
					this->m_dimensions[axis] = static_cast<float>(
						traits_t::to_double(p_primitive->get_dimension(axis)));
					this->m_position[axis] = static_cast<float>(
						traits_t::to_double(p_primitive->get_position_axis(axis)));
				}

				for (kotek::uint8_t component = 0; component < 4; ++component)
				{
					this->m_rotation[component] = static_cast<float>(
						traits_t::to_double(
							p_primitive->get_rotation_component(component)));
				}

				this->m_material_id = p_primitive->get_material_id();
				this->m_is_subtractive = p_primitive->is_subtractive();
			}

			kotek::array_t<char, 32> row_label{};
			kotek::ktk::sprintf(row_label.data(), row_label.size(), "row:%u",
				static_cast<kotek::uint32_t>(id.id));
			this->track(row_label.data());
		}

	}

	zircon_component_csg_primitive* p_primitive = this->get_selected_primitive();

	if (!p_primitive)
	{
		p_wrapper->TextDisabled("select a brush to edit it");
		p_wrapper->End();
		return;
	}

	if (p_wrapper->Checkbox("Subtractive", &this->m_is_subtractive))
		this->commit_subtractive(this->m_is_subtractive);
	this->track("checkbox:Subtractive");

	if (p_wrapper->DragInt("Material", &this->m_material_id, 1.0, 0, 65535) &&
		p_wrapper->IsItemDeactivatedAfterEdit())
	{
		this->commit_edit();
	}
	this->track("drag:Material");

	if (p_wrapper->DragFloat3("Dimensions (m)", this->m_dimensions, 0.05f,
			0.01f, 16384.0f) &&
		p_wrapper->IsItemDeactivatedAfterEdit())
	{
		this->commit_edit();
	}
	this->track("drag:Dimensions");

	if (p_wrapper->DragFloat3("Position (m)", this->m_position, 0.05f,
			-16384.0f, 16384.0f) &&
		p_wrapper->IsItemDeactivatedAfterEdit())
	{
		this->commit_edit();
	}
	this->track("drag:Position");

	if (p_wrapper->DragFloat4("Rotation (quat)", this->m_rotation, 0.01f,
			-1.0f, 1.0f) &&
		p_wrapper->IsItemDeactivatedAfterEdit())
	{
		this->commit_edit();
	}
	this->track("drag:Rotation");

	if (p_wrapper->Button("Delete selected brush"))
		this->delete_selected_primitive();
	this->track("button:Delete selected brush");

	p_wrapper->End();
}

int zircon_editor_ui_window_csg::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_Csg);
}

void zircon_editor_ui_window_csg::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_csg::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_csg::Is_Shown(void) const
{
	return this->m_is_show_window;
}
