#pragma once

class zircon_scene;
class zircon_command_history;

class zircon_command_create_entity : public Kotek::Core::ktkISDKRedoUndo
{
public:
	zircon_command_create_entity(
		zircon_command_history* p_history, zircon_scene* p_scene);
	~zircon_command_create_entity();

	void Execute() override;
	void Undo() override;
	const char* GetName() override;

	Kotek::ktk::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(Kotek::ktk::entity_t id) noexcept override;

	Kotek::ktk::enum_base_t GetCommandType() noexcept override;

	Kotek::ktk::size_t Serialize(Kotek::ktk::uint32_t resource_handle_id,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept override;

	void Deserialize(const Kotek::ktk::json::object& json_data) noexcept;

private:
	Kotek::ktk::entity_t m_created_entity;
	Kotek::ktk::entity_t m_entity_previous_id;
	zircon_command_history* m_p_history;
	zircon_scene* m_p_scene;
	// json object to string
	char m_serialize_json_string_storage[64];
};
