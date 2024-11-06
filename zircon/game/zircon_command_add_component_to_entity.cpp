#include "zircon_command_add_component_to_entity.h"
#include "../ecs/components/zircon_factory.h"

zircon_command_add_component_to_entity::zircon_command_add_component_to_entity(
	zircon_factory_game* p_factory, entt::entity id,
	const char* component_string) :
	m_is_serialized{},
	m_id{id}, m_p_component_name{component_string}, m_p_factory{p_factory},
	m_serialized_state_of_deleted_component{},
	m_serialized_component_as_string{}, m_storage_json_memory{}
{
	KOTEK_ASSERT(p_factory, "you can't pass an invalid factory here");
	KOTEK_ASSERT(component_string, "you can't pass an invalid component name!");
	KOTEK_ASSERT(strlen(component_string), "you can't pass an empty string!");
}

zircon_command_add_component_to_entity::
	~zircon_command_add_component_to_entity()
{
}

void zircon_command_add_component_to_entity::Execute(void)
{
	KOTEK_ASSERT(this->m_p_component_name,
		"you must initialize this field from constructor");

	if (this->m_p_factory)
	{
		if (this->m_p_factory->IsValidEntity(this->m_id))
		{
			auto p_raw_data_of_component = this->m_p_factory->CreateComponentByName(
				this->m_id, this->m_p_component_name);

			if (this->m_is_serialized)
			{
				this->m_p_factory->DeserializeComponent(p_raw_data_of_component,
					this->m_serialized_state_of_deleted_component);
			}

			KOTEK_MESSAGE("[history][{}] [{}] for entity[{}]", this->GetName(),
				this->m_p_component_name,
				static_cast<kotek::uint32_t>(this->m_id));
		}
	}
}

void zircon_command_add_component_to_entity::Undo(void)
{
	KOTEK_ASSERT(this->m_p_component_name,
		"you must initialize this field from constructor");

	if (this->m_p_factory)
	{
		if (this->m_p_factory->IsValidEntity(this->m_id))
		{
			this->m_serialized_state_of_deleted_component =
				this->m_p_factory->SerializeComponentByNameToJSON(this->m_id,
					this->m_p_component_name, this->m_storage_json_memory,
					sizeof(this->m_storage_json_memory));

			this->m_is_serialized = true;

			this->m_p_factory->RemoveComponentByName(
				this->m_id, this->m_p_component_name);

			KOTEK_MESSAGE("[history] removed component[{}] from entity[{}]",
				this->m_p_component_name,
				static_cast<kotek::uint32_t>(this->m_id));
		}
	}
}
const char* zircon_command_add_component_to_entity::GetName()
{
	return "add component to entity";
}

kotek::uint32_t zircon_command_add_component_to_entity::GetEntityID(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_id);
}

void zircon_command_add_component_to_entity::SetEntityID(
	kotek::uint32_t id) noexcept
{
	this->m_id = static_cast<entt::entity>(id);
}

kotek::enum_base_t
zircon_command_add_component_to_entity::GetCommandType() noexcept
{
	return static_cast<kotek::enum_base_t>(kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_CreateComponentForEntity);
}

kotek::size_t zircon_command_add_component_to_entity::Serialize(
	kotek::uint32_t resource_handle_id,
	kotek::core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(resource_handle_id != kotek::size_t(-1),
		"you must pass a valid resource handl");
	KOTEK_ASSERT(p_resource_manager, "must be valid!");

	KOTEK_ASSERT(
		this->m_serialized_state_of_deleted_component.get_object().find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME) ==
			this->m_serialized_state_of_deleted_component.get_object().end(),
		"your component has a reserved field already! you should use different "
		"field for serialization not: [{}]",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME);

	KOTEK_ASSERT(
		this->m_serialized_state_of_deleted_component.get_object().find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME) ==
			this->m_serialized_state_of_deleted_component.get_object().end(),
		"your compnent has a reserved field already! you should use different "
		"field for serialization not: [{}]",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME);
	
	KOTEK_ASSERT(
		this->m_serialized_state_of_deleted_component.get_object().find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME) ==
			this->m_serialized_state_of_deleted_component.get_object().end(),
		"your component has a reserved field already! you shoud use different field for serialization not: [{}]");

	auto& object = this->m_serialized_state_of_deleted_component.get_object();

	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME] =
		this->GetCommandType();
	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME] =
		static_cast<kotek::uint32_t>(this->m_id);


	kotek::ktk::json::serializer sr;
	sr.reset(&this->m_serialized_state_of_deleted_component);

	kotek::size_t offset{};
	constexpr kotek::size_t _kValidationSize =
		sizeof(this->m_serialized_component_as_string);

	while (!sr.done())
	{
		KOTEK_ASSERT(offset <= _kValidationSize,
			"you must shrink your buffer for serializing component as string "
			"because you got overflow state! current buffer size for "
			"serialization as string is: {} offset: {}",
			_kValidationSize, offset);

		char buf[zircon_DEF_STREAM_JSON_STACK_SIZE]{};
		auto view = sr.read(buf, sizeof(buf));

		kotek::ktk::memory::memcpy(
			this->m_serialized_component_as_string + offset, buf, view.size());
		offset += view.size();
	}

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("[history][{}] serialized command: [{}] with size string: "
				  "[{}] and total offset with endl symbol: [{}]",
		this->GetName(), this->m_serialized_component_as_string, offset,
		offset + 2);
#endif

	if (p_resource_manager)
	{
		char offset_string[sizeof(
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)];
		std::memset(offset_string, ' ', sizeof(offset_string));

		auto null_symbol_index = kotek::ktk::sprintf(
			offset_string, sizeof(offset_string), "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		kotek::ktk::memory::memset(offset_string + null_symbol_index, ' ',
			sizeof(offset_string) - null_symbol_index);
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			' ';

		p_resource_manager->Write(
			resource_handle_id, offset_string, sizeof(offset_string));
		p_resource_manager->Write(resource_handle_id,
			kotek::core::eFileWritingControlCharacterType::kNewLine);

		p_resource_manager->Write(
			resource_handle_id, this->m_serialized_component_as_string);
		p_resource_manager->Write(resource_handle_id,
			kotek::core::eFileWritingControlCharacterType::kNewLine);

		null_symbol_index = kotek::ktk::sprintf(
			offset_string, sizeof(offset_string), "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		kotek::ktk::memory::memset(offset_string + null_symbol_index, ' ',
			sizeof(offset_string) - null_symbol_index);
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY;

		p_resource_manager->Write(
			resource_handle_id, offset_string, sizeof(offset_string));
		p_resource_manager->Write(resource_handle_id,
			kotek::core::eFileWritingControlCharacterType::kFlush);
	}

	return offset;
}

void zircon_command_add_component_to_entity::Deserialize(
	const kotek::ktk::json::object& json) noexcept
{
	KOTEK_ASSERT(
		json.find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME) !=
			json.end(),
		"must exist key in json-object: {}",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME);

	auto type = static_cast<kotek::core::eConsoleCommandIndex>(
		json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME)
			.to_number<kotek::enum_base_t>());

	KOTEK_ASSERT(
		static_cast<kotek::enum_base_t>(type) == this->GetCommandType(),
		"it is not {} command what expected to be based on field {}",
		this->GetName(),
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME);

	KOTEK_ASSERT(
		json.find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME) !=
			json.end(),
		"must exist key in json-object: {}",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME);

	this->m_id = static_cast<entt::entity>(
		json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME)
			.to_number<kotek::uint32_t>());


}
