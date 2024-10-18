#pragma once

#include "zircon_command_definitions.h"

class zircon_factory_game;

class zircon_command_delete_component_from_entity
	: public Kotek::Core::ktkISDKRedoUndo
{
public:
	zircon_command_delete_component_from_entity(zircon_factory_game* p_factory,
		entt::entity id, Kotek::ktk::cstring component_string);

	~zircon_command_delete_component_from_entity();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;

	kotek::uint32_t GetEntityID(void) const noexcept override;
	void SetEntityID(kotek::uint32_t id) noexcept override;

	Kotek::ktk::enum_base_t GetCommandType() noexcept override;
	Kotek::ktk::size_t Serialize(Kotek::ktk::uint32_t resource_handle_id,
		Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept override;

private:
	entt::entity m_id;
	zircon_factory_game* m_p_factory;
	Kotek::ktk::static_cstring<zircon_DEF_MAX_FILENAME_LENGTH_FOR_STREAMING>
		m_filename;
	// must be pointer to char, because it is contains as static in ecs classes
	// (see interface)
	Kotek::ktk::static_cstring<zircon_DEF_MAX_COMPONENT_NAME_SIZE>
		m_component_name;
};
