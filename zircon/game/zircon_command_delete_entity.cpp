#include "zircon_command_delete_entity.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_scene.h"
#include "zircon_command_history.h"

zircon_command_delete_entity::zircon_command_delete_entity(
	zircon_command_history* p_history, zircon_scene* p_scene,
	zircon_factory_game* p_factory, entt::entity entity_to_delete) :
	m_p_history{p_history},
	m_p_scene{p_scene}, m_p_factory{p_factory},
	m_entity_to_delete{entity_to_delete}, m_entity_previous_id{entt::null}
{
	KOTEK_ASSERT(this->m_p_scene, "you can't pass an invalid scene here");
	KOTEK_ASSERT(this->m_p_factory, "you can't pass an invalid factory here");
	KOTEK_ASSERT(this->m_p_history, "you can't pass an invalid pointer here");
}

zircon_command_delete_entity::~zircon_command_delete_entity() {}

void zircon_command_delete_entity::Execute(void)
{
	if (this->m_p_scene)
	{
		// todo: reimpl
		// this->m_components = this->m_p_factory->GetAllComponentsOfEntity(
		// this->m_entity_to_delete);

		this->m_p_scene->RemoveEntity(this->m_entity_to_delete);

		this->m_entity_previous_id = this->m_entity_to_delete;

		KOTEK_MESSAGE("[history] removed entity: {}", static_cast<kotek::uint32_t>(this->m_entity_to_delete));
	}
}

void zircon_command_delete_entity::Undo(void)
{
	if (this->m_p_scene)
	{
		this->m_entity_to_delete = this->m_p_scene->CreateEntity();

		// todo: reimpl!!
		// this->m_p_factory->CreateAllComponents(
		// this->m_entity_to_delete, this->m_components);

		if (this->m_entity_previous_id != entt::null &&
			this->m_entity_to_delete != this->m_entity_previous_id)
		{
			if (this->m_p_history)
			{
				this->m_p_history->update_dependent_commands(
					this->m_entity_previous_id, this->m_entity_to_delete);
			}
		}

		KOTEK_MESSAGE(
			"[history][undo] created entity: {}", static_cast<kotek::uint32_t>(this->m_entity_to_delete));
	}
}

const char* zircon_command_delete_entity::GetName()
{
	return "delete entity";
}

kotek::uint32_t zircon_command_delete_entity::GetEntityID(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_entity_to_delete);
}

void zircon_command_delete_entity::SetEntityID(kotek::uint32_t id) noexcept
{
	this->m_entity_to_delete = static_cast<entt::entity>(id);
}

Kotek::ktk::enum_base_t zircon_command_delete_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eConsoleCommandIndex::kConsoleCommand_SDK_DeleteEntity);
}

Kotek::ktk::size_t zircon_command_delete_entity::Serialize(
	Kotek::ktk::uint32_t resource_handle_id,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	return 0;
}
