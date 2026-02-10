#pragma once

class zircon_world;
class zircon_editor_command_history;
class zircon_session_editor;
class zircon_session_editor_manager;
class zircon_factory;

class zircon_command_create_entity
	: public kotek::core::ktkISDKRedoUndo
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

	Kotek::ktk::enum_base_t GetCommandType() noexcept override;

	Kotek::ktk::size_t Serialize(
		kotek::core::ktkFileHandleType file
	) noexcept override;

	void Deserialize(const Kotek::ktk::json::object& json_data
	) noexcept;

private:
	kotek::entity_t m_created_entity;
	kotek::entity_t m_entity_previous_id;
	zircon_session_editor_manager* m_p_editor_session_manager;
	zircon_factory* m_p_factory;
	// json object to string
	char m_serialize_json_string_storage[64];
};
