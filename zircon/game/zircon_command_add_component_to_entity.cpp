#include "zircon_command_add_component_to_entity.h"
#include "../ecs/components/zircon_factory.h"

zircon_command_add_component_to_entity::zircon_command_add_component_to_entity(
	zircon_factory_game* p_factory, Kotek::ktk::entity_t id,
	const char* component_string) :
	m_is_serialized{},
	m_p_factory{p_factory}, m_id{id}, m_component_name{component_string}
{
	KOTEK_ASSERT(p_factory, "you can't pass an invalid factory here");
	KOTEK_ASSERT(component_string, "you can't pass an invalid component name!");
	KOTEK_ASSERT(strlen(component_string), "you can't pass an empty string!");
	std::memset(
		this->m_storage_json_memory, 0, sizeof(this->m_storage_json_memory));
}

zircon_command_add_component_to_entity::
	~zircon_command_add_component_to_entity()
{
}

void zircon_command_add_component_to_entity::Execute(void)
{
	if (this->m_p_factory)
	{
		if (this->m_p_factory->IsValidEntity(this->m_id))
		{
			auto p_raw_data = this->m_p_factory->CreateComponentByName(
				this->m_id, this->m_component_name);

			if (this->m_is_serialized)
			{
				this->m_p_factory->DeserializeComponent(
					p_raw_data, this->m_serialized_state_of_deleted_component);
			}

			KOTEK_MESSAGE("[history] create component[{}] for entity[{}]",
				this->m_component_name, this->m_id);
		}
	}
}

void zircon_command_add_component_to_entity::Undo(void)
{
	if (this->m_p_factory)
	{
		if (this->m_p_factory->IsValidEntity(this->m_id))
		{
			this->m_serialized_state_of_deleted_component =
				this->m_p_factory->SerializeComponentByNameToJSON(this->m_id,
					this->m_component_name, this->m_storage_json_memory,
					sizeof(this->m_storage_json_memory));

			this->m_is_serialized = true;

			this->m_p_factory->RemoveComponentByName(
				this->m_id, this->m_component_name);

			KOTEK_MESSAGE("[history] removed component[{}] from entity[{}]",
				this->m_component_name, this->m_id);
		}
	}
}
const char* zircon_command_add_component_to_entity::GetName()
{
	return "add component to entity";
}

Kotek::ktk::entity_t zircon_command_add_component_to_entity::GetEntityID(
	void) const noexcept
{
	return this->m_id;
}

void zircon_command_add_component_to_entity::SetEntityID(
	Kotek::ktk::entity_t id) noexcept
{
	this->m_id = id;
}

Kotek::ktk::enum_base_t
zircon_command_add_component_to_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(Kotek::Core::
			eConsoleCommandIndex::kConsoleCommand_SDK_CreateComponentForEntity);
}

Kotek::ktk::size_t zircon_command_add_component_to_entity::Serialize(
	Kotek::ktk::uint32_t resource_handle_id,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	return 0;
}