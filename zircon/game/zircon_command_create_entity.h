#pragma once

class zircon_scene;
class zircon_command_history;

class zircon_command_create_entity : public kotek::core::ktkISDKRedoUndo
{
public:
	zircon_command_create_entity(
		zircon_command_history* p_history, zircon_scene* p_scene);
	~zircon_command_create_entity();

	void Execute() override;
	void Undo() override;
	const char* GetName() override;

	kotek::uint32_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::uint32_t id) noexcept override;

	Kotek::ktk::enum_base_t GetCommandType() noexcept override;

	Kotek::ktk::size_t Serialize(Kotek::ktk::uint32_t resource_handle_id,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept override;

	void Deserialize(const Kotek::ktk::json::object& json_data) noexcept;

private:
	entt::entity m_created_entity;
	entt::entity m_entity_previous_id;
	zircon_command_history* m_p_history;
	zircon_scene* m_p_scene;
	// json object to string
	char m_serialize_json_string_storage[64];
};
