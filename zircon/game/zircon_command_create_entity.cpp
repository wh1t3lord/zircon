#include "zircon_command_create_entity.h"
#include "../ecs/components/zircon_factory.h"
#include "zircon_scene.h"
#include "zircon_command_history.h"

zircon_command_create_entity::zircon_command_create_entity(
	zircon_command_history* p_history, zircon_scene* p_scene) :
	m_created_entity{},
	m_entity_previous_id{entt::null}, m_p_history{p_history},
	m_p_scene{p_scene}, m_serialize_json_string_storage{}
{
	KOTEK_ASSERT(
		this->m_p_scene, "you must pass a valid pointer instance of scene");
	KOTEK_ASSERT(this->m_p_history, "you can't pass an invalid pointer here");
}

zircon_command_create_entity::~zircon_command_create_entity() {}

void zircon_command_create_entity::Execute()
{
	if (this->m_p_scene)
	{
		this->m_created_entity = this->m_p_scene->CreateEntity();

		if (this->m_entity_previous_id != entt::null &&
			this->m_created_entity != this->m_entity_previous_id)
		{
			if (this->m_p_history)
			{
				this->m_p_history->update_dependent_commands(
					this->m_entity_previous_id, this->m_created_entity);
			}
		}

		KOTEK_MESSAGE("[history]: created entity: {}", this->m_created_entity);
	}
}

void zircon_command_create_entity::Undo()
{
	if (this->m_p_scene)
	{
		KOTEK_MESSAGE(
			"[history][undo]: removed entity {}", this->m_created_entity);

		this->m_entity_previous_id = this->m_created_entity;

		this->m_p_scene->RemoveEntity(this->m_created_entity);
	}
}
const char* zircon_command_create_entity::GetName()
{
	return "create entity";
}
Kotek::ktk::entity_t zircon_command_create_entity::GetEntityID(
	void) const noexcept
{
	return this->m_created_entity;
}
void zircon_command_create_entity::SetEntityID(Kotek::ktk::entity_t id) noexcept
{
	this->m_created_entity = id;
}

Kotek::ktk::enum_base_t zircon_command_create_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eConsoleCommandIndex::kConsoleCommand_SDK_CreateEntity);
}

Kotek::ktk::size_t zircon_command_create_entity::Serialize(
	Kotek::ktk::uint32_t resource_handle_id,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(resource_handle_id != Kotek::ktk::size_t(-1),
		"you must pass a valid resource manager pointer!");
	KOTEK_ASSERT(p_resource_manager,
		"you must pass a valid resource manager interface!");

#ifdef KOTEK_DEBUG
	unsigned char stack_memory[256];
#else
	unsigned char stack_memory[128];
#endif

	Kotek::ktk::json::static_resource storage(stack_memory);
	Kotek::ktk::json::value out(&storage);

	auto& object = out.emplace_object();
	object["command"] = this->GetCommandType();

#ifdef KOTEK_DEBUG
	object["entity_id"] = this->m_created_entity;
#endif

	Kotek::ktk::json::serializer sr;
	sr.reset(&out);

	Kotek::ktk::size_t offset{};
	while (!sr.done())
	{
		char buf[16];
		auto view = sr.read(buf, sizeof(buf));

		Kotek::ktk::memory::memcpy(
			this->m_serialize_json_string_storage + offset, buf, view.size());

		offset += view.size();
	}

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("[history][create entity] serialized command: [{}] with size "
				  "string: [{}] and total offset with endl symbol: [{}]",
		this->m_serialize_json_string_storage, offset, offset + 2);
#endif

	if (p_resource_manager)
	{
		char offset_string[sizeof(
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)];
		std::memset(offset_string, ' ', sizeof(offset_string));

		auto null_symbol_index = std::sprintf(offset_string, "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		offset_string[null_symbol_index] = ' ';
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			' ';

		p_resource_manager->Write(
			resource_handle_id, offset_string, sizeof(offset_string));
		p_resource_manager->Write(resource_handle_id,
			Kotek::Core::eFileWritingControlCharacterType::kNewLine);

		p_resource_manager->Write(
			resource_handle_id, this->m_serialize_json_string_storage);
		p_resource_manager->Write(resource_handle_id,
			Kotek::Core::eFileWritingControlCharacterType::kNewLine);

		null_symbol_index = std::sprintf(offset_string, "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		offset_string[null_symbol_index] = ' ';
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY;

		// storage + endl
		p_resource_manager->Write(
			resource_handle_id, offset_string, sizeof(offset_string));
		p_resource_manager->Write(resource_handle_id,
			Kotek::Core::eFileWritingControlCharacterType::kFlush);
	}

	return offset;
}

void zircon_command_create_entity::Deserialize(
	const Kotek::ktk::json::object& json) noexcept
{
	KOTEK_ASSERT(json.find("entity_id") != json.end(),
		"must exist key entity_id! (is it create entity command at all?)");

#ifdef KOTEK_DEBUG
	auto type =
		static_cast<Kotek::ktk::enum_base_t>(json.at("command").as_int64());

	KOTEK_ASSERT(type == this->GetCommandType(),
		"it is not create entity command! Something is broken!");

	this->m_created_entity =
		json.at("entity_id").to_number<Kotek::ktk::entity_t>();
#endif
}
