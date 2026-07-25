#pragma once

#include "zircon_command_registry.h"

class zircon_world;
class zircon_editor_command_history;
class zircon_session_editor;
class zircon_session_editor_manager;
class zircon_factory;

class zircon_command_create_entity
	: public kotek::core::ktkISDKRedoUndo,
	  public zircon_interface_command_delta
{
public:
	zircon_command_create_entity(
		zircon_session_editor_manager* p_session_manager_editor,
		zircon_factory* p_factory
	);
	~zircon_command_create_entity();

	void Execute() override;
	void Undo() override;
	const char* GetName() override;

	kotek::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::entity_t id) noexcept override;

	kotek::ktk::enum_base_t GetCommandType() noexcept override;

	/// @brief \~english deprecated file serialization from the old
	/// streaming design, the journal uses Serialize_Delta instead
	kotek::ktk::size_t Serialize(
		kotek::core::ktkFileHandleType file
	) noexcept override;

	/// @brief \~english delta = the created entity id
	bool Serialize_Delta(
		zircon_command_delta_writer& writer
	) noexcept override;
	bool Deserialize_Delta(
		zircon_command_delta_reader& reader
	) noexcept override;

private:
	kotek::entity_t m_created_entity;
	kotek::entity_t m_entity_previous_id;
	zircon_session_editor_manager* m_p_editor_session_manager;
	zircon_factory* m_p_factory;
};
