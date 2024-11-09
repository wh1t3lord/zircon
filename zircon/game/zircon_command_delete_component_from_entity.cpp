#include "zircon_command_delete_component_from_entity.h"
#include "../ecs/components/zircon_factory.h"

// TODO: remove all cstring to static_cstring containers!!!
zircon_command_delete_component_from_entity::
	zircon_command_delete_component_from_entity(zircon_factory_game* p_factory,
		entt::entity id, const char* p_component_string) :
	m_id{id},
	m_p_factory{p_factory}, m_p_component_name{p_component_string},
	m_serialized_state_of_deleted_component{},
	m_serialized_component_as_string{}, m_storage_json_memory{}
{
	KOTEK_ASSERT(this->m_p_factory, "you can't pass an invalid factory here");
	KOTEK_ASSERT(
		this->m_p_component_name, "you can't pass an empty string here");
	KOTEK_ASSERT(
		strlen(this->m_p_component_name), "string must not be an empty!");
}

zircon_command_delete_component_from_entity::
	zircon_command_delete_component_from_entity(
		zircon_factory_game* p_factory) :
	m_id{entt::null},
	m_p_factory{p_factory}, m_p_component_name{},
	m_serialized_state_of_deleted_component{},
	m_serialized_component_as_string{}, m_storage_json_memory{}
{
}

zircon_command_delete_component_from_entity::
	~zircon_command_delete_component_from_entity()
{
}

void zircon_command_delete_component_from_entity::Execute(void)
{
	if (this->m_p_factory)
	{
		if (this->m_p_component_name)
		{
			if (this->m_p_factory->IsValidEntity(this->m_id))
			{
				this->m_serialized_state_of_deleted_component =
					this->m_p_factory->SerializeComponentByNameToJSON(
						this->m_id, this->m_p_component_name);

				this->m_p_factory->RemoveComponentByName(
					this->m_id, this->m_p_component_name);

				KOTEK_MESSAGE("[history] removed component by name: {} "
							  "in entity {}",
					this->m_p_component_name,
					static_cast<kotek::uint32_t>(this->m_id));
			}
		}
	}
}

void zircon_command_delete_component_from_entity::Undo(void)
{
	if (this->m_p_factory)
	{
		if (this->m_p_component_name)
		{
			if (this->m_p_factory->IsValidEntity(this->m_id))
			{
				auto* p_component = this->m_p_factory->CreateComponentByName(
					this->m_id, this->m_p_component_name);

				this->m_p_factory->DeserializeComponent(
					p_component, this->m_serialized_state_of_deleted_component);

				KOTEK_MESSAGE("[history][undo] restored component by "
							  "name: {} for entity {}",
					this->m_p_component_name,
					static_cast<kotek::uint32_t>(this->m_id));
			}
		}
	}
}
const char* zircon_command_delete_component_from_entity::GetName()
{
	return "delete component from entity";
}

kotek::uint32_t zircon_command_delete_component_from_entity::GetEntityID(
	void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_id);
}

void zircon_command_delete_component_from_entity::SetEntityID(
	kotek::uint32_t id) noexcept
{
	this->m_id = static_cast<entt::entity>(id);
}

Kotek::enum_base_t
zircon_command_delete_component_from_entity::GetCommandType() noexcept
{
	return static_cast<kotek::enum_base_t>(kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_DeleteComponentFromEntity);
}

kotek::size_t zircon_command_delete_component_from_entity::Serialize(
	kotek::uint32_t resource_handle_id,
	kotek::core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(resource_handle_id != kotek::size_t(-1),
		"you must pass a valid resource handl");
	KOTEK_ASSERT(p_resource_manager, "must be valid!");

	KOTEK_ASSERT(this->m_p_factory, "factory must be initialize");
	KOTEK_ASSERT(this->m_p_component_name,
		"this must be initialized because it is issued as command from "
		"console");
	KOTEK_ASSERT(strlen(this->m_p_component_name), "must be not empty!");

	auto& object = this->m_serialized_state_of_deleted_component.get_object();

	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME] =
		this->GetCommandType();
	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME] =
		static_cast<kotek::uint32_t>(this->m_id);
	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME] =
		static_cast<kotek::enum_base_t>(
			this->m_p_factory->get_component_type_id_by_component_name(
				this->m_p_component_name));

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

void zircon_command_delete_component_from_entity::Deserialize(
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

	this->m_p_component_name =
		this->m_p_factory
			->get_component_name_by_component_type_id(static_cast<
				zircon_component_type_t>(
				json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME)
					.to_number<kotek::enum_base_t>()))
			.data();

	KOTEK_ASSERT(
		this->m_p_component_name, "must be valid pointer from string_view!");
	KOTEK_ASSERT(strlen(this->m_p_component_name), "must be not empty string!");

	kotek::ktk::json::static_resource storage(this->m_storage_json_memory);
	kotek::ktk::json::value out(&storage);
	auto& object = out.emplace_object();
	object = json;
	this->m_serialized_state_of_deleted_component = out;
}

zircon_component_type_t
zircon_command_delete_component_from_entity::get_component_type()
{
	KOTEK_ASSERT(this->m_p_factory, "early calling, must be valid!");
	KOTEK_ASSERT(this->m_p_component_name, "must be valid!");
	KOTEK_ASSERT(strlen(this->m_p_component_name), "must be not empty!");

	zircon_component_type_t result =
		zircon_component_type_t::kComponentTypeUnknown;

	if (this->m_p_factory)
	{
		if (this->m_p_component_name)
		{
			result = this->m_p_factory->get_component_type_id_by_component_name(
				this->m_p_component_name);
		}
	}

	return result;
}
