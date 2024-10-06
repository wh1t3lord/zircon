#pragma once

#include "zircon_command_definitions.h"

class zircon_factory_game;

class zircon_command_add_component_to_entity
	: public Kotek::Core::ktkISDKRedoUndo
{
public:
	zircon_command_add_component_to_entity(zircon_factory_game* p_factory,
		Kotek::ktk::entity_t id, const char* component_string);

	~zircon_command_add_component_to_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	Kotek::ktk::entity_t GetEntityID(void) const noexcept override;
	void SetEntityID(Kotek::ktk::entity_t id) noexcept override;

	Kotek::ktk::enum_base_t GetCommandType() noexcept override;
	Kotek::ktk::size_t Serialize(Kotek::ktk::uint32_t resource_handle_id,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept override;

private:
	bool m_is_serialized;
	Kotek::ktk::entity_t m_id;
	const char* m_component_name;
	zircon_factory_game* m_p_factory;
	Kotek::ktk::json::value m_serialized_state_of_deleted_component;
	unsigned char
		m_storage_json_memory[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];
};
