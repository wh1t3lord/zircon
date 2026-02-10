#include "zircon_command_create_entity.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "zircon_command_history.h"

zircon_command_create_entity::zircon_command_create_entity(
	zircon_session_editor_manager* p_session_manager_editor,
	zircon_factory* p_factory
) :
	m_created_entity{},
	m_entity_previous_id{kotek::ktk::kInvalidECSEntity},
	m_p_editor_session_manager{p_session_manager_editor},
	m_p_factory{p_factory}, m_serialize_json_string_storage{}
{
	KOTEK_ASSERT(
		p_session_manager_editor,
		"must be valid pointer of editor session manager"
	);

	KOTEK_ASSERT(p_factory, "factory must be valid!");
}

zircon_command_create_entity::~zircon_command_create_entity() {}

void zircon_command_create_entity::Execute()
{
	if (!this->m_p_editor_session_manager)
	{
		KOTEK_MESSAGE_ERROR("failed to excute due to invalid "
		                    "editor session manager!");
		return;
	}

	if (!this->m_p_factory)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid factory"
		);
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_editor_session_manager->get_session(
			this->m_p_editor_session_manager
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_editor_session_manager
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_editor_session_manager
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world of session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	zircon_editor_command_history* p_history =
		p_session->get_command_history();

	KOTEK_ASSERT(
		p_history,
		"failed to execute due to invalid history command "
		"manager is nullptr "
		"in session editor_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_history)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid history "
			"command manager is nullptr "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid ecs context that "
			"created for world!"
		);
		return;
	}

	if (p_world)
	{
		this->m_created_entity =
			this->m_p_factory->create_entity(
				p_world->get_ecs_context()
			);

#ifdef KOTEK_USE_ECS_BACKEND_PICO
		if (this->m_entity_previous_id.id !=
		        kotek::ktk::kInvalidECSEntity.id &&
		    this->m_created_entity.id !=
		        this->m_entity_previous_id.id)
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
		if (this->m_entity_previous_id != entt::null &&
		    this->m_created_entity !=
		        this->m_entity_previous_id)
#endif
		{
			if (p_history)
			{
				p_history->update_dependent_commands(
					this->m_entity_previous_id,
					this->m_created_entity
				);
			}
		}

		KOTEK_MESSAGE(
			"[history]: created entity: {}",
			this->m_created_entity
		);
	}
}

void zircon_command_create_entity::Undo()
{
	if (!this->m_p_editor_session_manager)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to excute due to invalid game manager!"
		);
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_editor_session_manager->get_session(
			this->m_p_editor_session_manager
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_editor_session_manager
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session "
			"editor#{}",
			this->m_p_editor_session_manager
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world of session "
			"editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	if (p_world)
	{
		KOTEK_MESSAGE(
			"[history][undo]: removed entity {}",
			this->m_created_entity
		);

		this->m_entity_previous_id = this->m_created_entity;

		this->m_p_factory->destroy_entity(
			p_world->get_ecs_context(), this->m_created_entity
		);
	}
}
const char* zircon_command_create_entity::GetName()
{
	return "create entity";
}
kotek::entity_t zircon_command_create_entity::GetEntityID(void
) const noexcept
{
	return this->m_created_entity;
}
void zircon_command_create_entity::SetEntityID(
	kotek::entity_t id
) noexcept
{
	this->m_created_entity = id;
}

Kotek::ktk::enum_base_t
zircon_command_create_entity::GetCommandType() noexcept
{
	return static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eConsoleCommandIndex::
			kConsoleCommand_SDK_CreateEntity
	);
}

Kotek::ktk::size_t zircon_command_create_entity::Serialize(
	kotek::core::ktkFileHandleType file
) noexcept
{
	KOTEK_ASSERT(
		file != kotek::core::kInvalidFileHandleType,
		"you must pass a valid resource manager pointer!"
	);

#ifdef KOTEK_DEBUG
	unsigned char stack_memory[256];
#else
	unsigned char stack_memory[128];
#endif

	KOTEK_ASSERT(
		false,
		"todo: re-write please also replace _file to FILE* "
		"handle"
	);

	kotek::cfstream_t _file;

	Kotek::ktk::json::static_resource storage(stack_memory);
	Kotek::ktk::json::value out(&storage);

	auto& object = out.emplace_object();
	object["command"] = this->GetCommandType();
	object["entity_id"] =
		static_cast<kotek::uint32_t>(this->m_created_entity.id);

	Kotek::ktk::json::serializer sr;
	sr.reset(&out);

	Kotek::ktk::size_t offset{};
	while (!sr.done())
	{
		char buf[16];
		auto view = sr.read(buf, sizeof(buf));

		Kotek::ktk::memory::memcpy(
			this->m_serialize_json_string_storage + offset,
			buf,
			view.size()
		);

		offset += view.size();
	}

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE(
		"[history][{}] serialized command: [{}] with size "
		"string: [{}] and total offset with endl symbol: [{}]",
		this->GetName(),
		this->m_serialize_json_string_storage,
		offset,
		offset + 2
	);
#endif

	//	if (p_resource_manager)
	{
		char offset_string[sizeof(
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS
		)];
		kotek::ktk::memory::memset(
			offset_string, ' ', sizeof(offset_string)
		);

		auto null_symbol_index = kotek::ktk::sprintf(
			offset_string,
			sizeof(offset_string),
			"%zu",
			offset + 2
		);

		KOTEK_ASSERT(
			null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are "
			"out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS
		);

		kotek::ktk::memory::memset(
			offset_string + null_symbol_index,
			' ',
			sizeof(offset_string) - null_symbol_index
		);
		offset_string
			[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
				' ';

		//	p_resource_manager->Write(
		//		resource_handle_id, offset_string,
		// sizeof(offset_string));
		_file.write(offset_string, sizeof(offset_string));

		// p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		_file << std::endl;

		// p_resource_manager->Write(
		//	resource_handle_id,
		// this->m_serialize_json_string_storage);
		_file << this->m_serialize_json_string_storage;
		// p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		_file << std::endl;

		null_symbol_index = kotek::ktk::sprintf(
			offset_string,
			sizeof(offset_string),
			"%zu",
			offset + 2
		);

		KOTEK_ASSERT(
			null_symbol_index <=
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS,
			"overflow, number is {} digits and it means we are "
			"out of "
			"memory!",
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS
		);

		kotek::ktk::memory::memset(
			offset_string + null_symbol_index,
			' ',
			sizeof(offset_string) - null_symbol_index
		);
		offset_string
			[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] =
				zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY;

		// storage + endl
		// p_resource_manager->Write(
		//	resource_handle_id, offset_string,
		// sizeof(offset_string));
		_file.write(offset_string, sizeof(offset_string));
		// p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kFlush);
		_file.flush();
	}

	return offset;
}

void zircon_command_create_entity::Deserialize(
	const Kotek::ktk::json::object& json
) noexcept
{
	KOTEK_ASSERT(
		json.find("entity_id") != json.end(),
		"must exist key entity_id! (is it create entity "
		"command at all?)"
	);

	auto type = static_cast<Kotek::ktk::enum_base_t>(
		json.at("command").as_int64()
	);

	KOTEK_ASSERT(
		type == this->GetCommandType(),
		"it is not create entity command! Something is broken!"
	);

#ifdef KOTEK_USE_ECS_BACKEND_PICO
	this->m_created_entity.id =
		(json.at("entity_id")
	         .to_number<decltype(kotek::entity_t::id)>());
#elif defined(KOTEK_USE_ECS_BACKEND_ENTT)
	this->m_created_entity = static_cast<kotek::entity_t>(
		json.at("entity_id").to_number<kotek::uint32_t>()
	);
#endif
}
