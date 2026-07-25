#pragma once

#include "zircon_command_definitions.h"
#include "zircon_command_registry.h"
#include "../../ecs/zircon_component_interface.h"

class zircon_session_editor_manager;
class zircon_factory;

class zircon_command_delete_component_from_entity
	: public kotek::Core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_delete_component_from_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_factory* p_factory,
		kotek::entity_t id,
		const char* p_component_name
	);

	// for command history (journal reconstruction)
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

	/// @brief \~english deprecated file serialization from the old
	/// streaming design, the journal uses Serialize_Delta instead
	kotek::size_t Serialize(kotek::core::ktkFileHandleType file) noexcept override;

	/// @brief \~english delta = entity id + component name +
	/// component state before the deletion
	bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept override;
	bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept override;

	eZirconComponentType get_component_type();

private:
	kotek::entity_t m_id;
	zircon_session_editor_manager* m_p_manager_session_editor;
	zircon_factory* m_p_factory;
	kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>
		m_component_name;
	kotek::ktk::json::value m_serialized_state_of_deleted_component;
};
