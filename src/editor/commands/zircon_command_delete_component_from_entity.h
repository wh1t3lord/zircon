#pragma once

#include "zircon_command_definitions.h"
#include "../../ecs/zircon_component_interface.h"

class zircon_session_editor_manager;
class zircon_factory;

class zircon_command_delete_component_from_entity
	: public Kotek::Core::ktkISDKRedoUndo
{
public:
	zircon_command_delete_component_from_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t id, 
		const char* p_component_name
	);

	// for command history
	zircon_command_delete_component_from_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory
	);

	~zircon_command_delete_component_from_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::entity_t id) noexcept override;

	kotek::enum_base_t GetCommandType() noexcept override;
	kotek::size_t Serialize(Kotek::core::ktkFileHandleType file) noexcept override;
	void Deserialize(const kotek::ktk::json::object& json) noexcept;

	eZirconComponentType get_component_type();

private:
	kotek::entity_t m_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	const char* m_p_component_name;
	kotek::ktk::json::value m_serialized_state_of_deleted_component;
	char m_serialized_component_as_string
		[ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
	unsigned char
		m_storage_json_memory[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON -
			ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
};
