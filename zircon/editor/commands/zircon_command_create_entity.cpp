#include "zircon_command_create_entity.h"
#include "../../ecs/components/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "zircon_command_history.h"

zircon_command_create_entity::zircon_command_create_entity(
	zircon_editor_command_history* p_history, zircon_world* p_scene) :
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

		KOTEK_MESSAGE("[history]: created entity: {}",
			static_cast<kotek::uint32_t>(this->m_created_entity));
	}
}

void zircon_command_create_entity::Undo()
{
	if (this->m_p_scene)
	{
		KOTEK_MESSAGE("[history][undo]: removed entity {}",
			static_cast<kotek::uint32_t>(this->m_created_entity));

		this->m_entity_previous_id = this->m_created_entity;

		this->m_p_scene->RemoveEntity(this->m_created_entity);
	}
}
const char* zircon_command_create_entity::GetName()
{
	return "create entity";
}
kotek::uint32_t zircon_command_create_entity::GetEntityID(void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_created_entity);
}
void zircon_command_create_entity::SetEntityID(kotek::uint32_t id) noexcept
{
	this->m_created_entity = static_cast<entt::entity>(id);
}

Kotek::ktk::enum_base_t zircon_command_create_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eConsoleCommandIndex::kConsoleCommand_SDK_CreateEntity);
}

Kotek::ktk::size_t zircon_command_create_entity::Serialize(
	kotek::cfstream_t* p_file,
	Kotek::Core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(p_file,
		"you must pass a valid resource manager pointer!");
	KOTEK_ASSERT(p_resource_manager,
		"you must pass a valid resource manager interface!");

#ifdef KOTEK_DEBUG
	unsigned char stack_memory[256];
#else
	unsigned char stack_memory[128];
#endif

	kotek::cfstream_t& file = *p_file;

	Kotek::ktk::json::static_resource storage(stack_memory);
	Kotek::ktk::json::value out(&storage);

	auto& object = out.emplace_object();
	object["command"] = this->GetCommandType();
	object["entity_id"] = static_cast<kotek::uint32_t>(this->m_created_entity);

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
	KOTEK_MESSAGE("[history][{}] serialized command: [{}] with size "
				  "string: [{}] and total offset with endl symbol: [{}]",
		this->GetName(), this->m_serialize_json_string_storage, offset,
		offset + 2);
#endif

	if (p_resource_manager)
	{
		char offset_string[sizeof(
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)];
		kotek::ktk::memory::memset(offset_string, ' ', sizeof(offset_string));

		auto null_symbol_index = kotek::ktk::sprintf(offset_string, sizeof(offset_string), "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		kotek::ktk::memory::memset(offset_string + null_symbol_index, ' ',
			sizeof(offset_string) - null_symbol_index);
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			' ';

	//	p_resource_manager->Write(
	//		resource_handle_id, offset_string, sizeof(offset_string));
		file.write(offset_string, sizeof(offset_string));


		//p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		file << std::endl;

		//p_resource_manager->Write(
		//	resource_handle_id, this->m_serialize_json_string_storage);
		file << this->m_serialize_json_string_storage;
		//p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		file << std::endl;

		null_symbol_index = kotek::ktk::sprintf(offset_string, sizeof(offset_string), "%zu", offset + 2);

		KOTEK_ASSERT(null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS);

		kotek::ktk::memory::memset(offset_string + null_symbol_index, ' ',
			sizeof(offset_string) - null_symbol_index);
		offset_string[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
			zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY;

		// storage + endl
		//p_resource_manager->Write(
		//	resource_handle_id, offset_string, sizeof(offset_string));
		file.write(offset_string, sizeof(offset_string));
		//p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kFlush);
		file.flush();
	}

	return offset;
}

void zircon_command_create_entity::Deserialize(
	const Kotek::ktk::json::object& json) noexcept
{
	KOTEK_ASSERT(json.find("entity_id") != json.end(),
		"must exist key entity_id! (is it create entity command at all?)");

	auto type =
		static_cast<Kotek::ktk::enum_base_t>(json.at("command").as_int64());

	KOTEK_ASSERT(type == this->GetCommandType(),
		"it is not create entity command! Something is broken!");

	this->m_created_entity = static_cast<entt::entity>(
		json.at("entity_id").to_number<kotek::uint32_t>());
}
