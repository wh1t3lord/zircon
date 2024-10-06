#include "zircon_command_delete_component_from_entity.h"
#include "../ecs/components/zircon_factory.h"

// TODO: remove all cstring to static_cstring containers!!!
zircon_command_delete_component_from_entity::
	zircon_command_delete_component_from_entity(zircon_factory_game* p_factory,
		Kotek::ktk::entity_t id, Kotek::ktk::cstring component_string) :
	m_id{id},
	m_p_factory{p_factory}
{
	KOTEK_ASSERT(this->m_p_factory, "you can't pass an invalid factory here");

	KOTEK_ASSERT(component_string.size() <= zircon_DEF_MAX_COMPONENT_NAME_SIZE,
		"overflow component_string > zircon_DEF_MAX_COMPONENT_NAME_SIZE({}) "
		"(suggestion: reduce naming in class)",
		zircon_DEF_MAX_COMPONENT_NAME_SIZE);

	this->m_component_name = component_string.c_str();

	KOTEK_ASSERT(this->m_component_name.empty() == false,
		"you can't pass an empty string here");
}

zircon_command_delete_component_from_entity::
	~zircon_command_delete_component_from_entity()
{
}

void zircon_command_delete_component_from_entity::Execute(void)
{
	if (this->m_p_factory)
	{
		if (this->m_component_name.empty() == false)
		{
			if (this->m_p_factory->IsValidEntity(this->m_id))
			{
				// todo: reimpl!
				// this->m_serialized_state_of_deleted_component =
				// this->m_p_factory->SerializeComponentByNameToJSON(
				// this->m_id, this->m_component_name);

				this->m_p_factory->RemoveComponentByName(
					this->m_id, this->m_component_name.c_str());

				KOTEK_MESSAGE("[history] removed component by name: {} "
							  "in entity {}",
					this->m_component_name.c_str(), this->m_id);
			}
		}
	}
}

void zircon_command_delete_component_from_entity::Undo(void)
{
	if (this->m_p_factory)
	{
		if (this->m_component_name.empty() == false)
		{
			if (this->m_p_factory->IsValidEntity(this->m_id))
			{
				auto* p_component = this->m_p_factory->CreateComponentByName(
					this->m_id, this->m_component_name.c_str());

				// todo: reimpl!
				//	this->m_p_factory->DeserializeComponent(
				//	p_component, this->m_serialized_state_of_deleted_component);

				KOTEK_MESSAGE("[history][undo] restored component by "
							  "name: {} for entity {}",
					this->m_component_name.c_str(), this->m_id);
			}
		}
	}
}
const char* zircon_command_delete_component_from_entity::GetName()
{
	return "delete component from entity";
}

Kotek::ktk::entity_t zircon_command_delete_component_from_entity::GetEntityID(
	void) const noexcept
{
	return this->m_id;
}

void zircon_command_delete_component_from_entity::SetEntityID(
	Kotek::ktk::entity_t id) noexcept
{
	this->m_id = id;
}

Kotek::ktk::enum_base_t
zircon_command_delete_component_from_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eConsoleCommandIndex::
			kConsoleCommand_SDK_DeleteComponentFromEntity);
}

Kotek::ktk::size_t zircon_command_delete_component_from_entity::Serialize(
	Kotek::ktk::uint32_t resource_handle_id,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	return 0;
}
