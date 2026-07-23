#pragma once

#include "zircon_command_definitions.h"
#include "zircon_command_registry.h"
#include "../../ecs/zircon_component_interface.h"

class zircon_factory;
class zircon_session_editor_manager;

class zircon_command_add_component_to_entity
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_add_component_to_entity(
		zircon_session_editor_manager* p_manager_session_editor,
		kotek::entity_t id, const char* component_string);
	// for command history (journal reconstruction)
	zircon_command_add_component_to_entity(
		zircon_session_editor_manager* p_manager_session_editor);

	~zircon_command_add_component_to_entity();

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
	/// component state (captured eagerly by Execute)
	bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept override;
	bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept override;

	void serialize_state();
	bool is_state_serialized() const noexcept;
	eZirconComponentType get_component_type();

private:
	bool m_is_serialized;
	kotek::entity_t m_id;
	kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>
		m_component_name;
	zircon_session_editor_manager* m_p_manager_session_editor;
	kotek::ktk::json::value m_serialized_state_of_deleted_component;
};
