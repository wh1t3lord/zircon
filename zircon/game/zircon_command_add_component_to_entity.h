#pragma once

#include "zircon_command_definitions.h"

class zircon_factory_game;
enum zircon_component_type_t;

class zircon_command_add_component_to_entity
	: public kotek::core::ktkISDKRedoUndo
{
public:
	zircon_command_add_component_to_entity(zircon_factory_game* p_factory,
		entt::entity id, const char* component_string);
	// for command history
	zircon_command_add_component_to_entity(zircon_factory_game* p_factory);

	~zircon_command_add_component_to_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::uint32_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::uint32_t id) noexcept override;

	kotek::enum_base_t GetCommandType() noexcept override;
	kotek::size_t Serialize(kotek::uint32_t resource_handle_id,
		kotek::core::ktkIResourceManager* p_resource_manager) noexcept override;
	void Deserialize(const kotek::ktk::json::object& json) noexcept;

	void serialize_state();
	bool is_state_serialized() const noexcept;
	zircon_component_type_t get_component_type();

private:
	bool m_is_serialized;
	entt::entity m_id;
	const char* m_p_component_name;
	zircon_factory_game* m_p_factory;
	kotek::ktk::json::value m_serialized_state_of_deleted_component;
	char m_serialized_component_as_string
		[ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
	unsigned char
		m_storage_json_memory[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON -
			ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE];
};
