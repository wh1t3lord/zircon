#include "zircon_command_add_component_to_entity.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../../world/zircon_world.h"
#include "../../ecs/zircon_factory.h"

zircon_command_add_component_to_entity::zircon_command_add_component_to_entity(
	zircon_session_editor_manager* p_manager_session_editor, entt::entity id,
	const char* component_string) :
	m_is_serialized{}, m_id{id}, m_p_component_name{component_string},
	m_p_manager_session_editor{p_manager_session_editor},
	m_serialized_state_of_deleted_component{},
	m_serialized_component_as_string{}, m_storage_json_memory{}
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"you must pass a valid pointer editor session manager!");
	KOTEK_ASSERT(component_string, "you can't pass an invalid component name!");
	KOTEK_ASSERT(strlen(component_string), "you can't pass an empty string!");
}

zircon_command_add_component_to_entity::zircon_command_add_component_to_entity(
	zircon_session_editor_manager* p_manager_session_editor) :
	m_is_serialized{}, m_id{entt::null}, m_p_component_name{},
	m_p_manager_session_editor{p_manager_session_editor},
	m_serialized_state_of_deleted_component{},
	m_serialized_component_as_string{}, m_storage_json_memory{}
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"you must pass a valid pointer of editor session manager!");
}

zircon_command_add_component_to_entity::
	~zircon_command_add_component_to_entity()
{
}

void zircon_command_add_component_to_entity::Execute(void)
{
	KOTEK_ASSERT(this->m_p_component_name,
		"you must initialize this field from constructor");
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"should be initialzed editor session manager here");

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid editor session manager pointer!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session editor#{}",
			this->m_p_manager_session_editor->get_current_session_id());
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	if (p_factory)
	{
		if (p_factory->IsValidEntity(this->m_id))
		{
			auto p_raw_data_of_component = p_factory->CreateComponentByName(
				this->m_id, this->m_p_component_name);

			if (this->m_is_serialized)
			{
				p_factory->DeserializeComponent(p_raw_data_of_component,
					this->m_serialized_state_of_deleted_component);
			}

			KOTEK_ASSERT(p_raw_data_of_component,
				"failed to create component to entity: {}",
				static_cast<kotek::uint32_t>(this->m_id));

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
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"should be initialzed game manager here");

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid game manager pointer!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session editor#{}",
			this->m_p_manager_session_editor->get_current_session_id());
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	if (p_factory)
	{
		if (p_factory->IsValidEntity(this->m_id))
		{
			this->m_serialized_state_of_deleted_component =
				p_factory->SerializeComponentByNameToJSON(this->m_id,
					this->m_p_component_name, this->m_storage_json_memory,
					sizeof(this->m_storage_json_memory));

			KOTEK_ASSERT(
				this->m_serialized_state_of_deleted_component.get_object().find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME) ==
					this->m_serialized_state_of_deleted_component.get_object()
						.end(),
				"your component has a reserved field already! you should use "
				"different "
				"field for serialization not: [{}]",
				ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME);

			KOTEK_ASSERT(
				this->m_serialized_state_of_deleted_component.get_object().find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME) ==
					this->m_serialized_state_of_deleted_component.get_object()
						.end(),
				"your compnent has a reserved field already! you should use "
				"different "
				"field for serialization not: [{}]",
				ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME);

			KOTEK_ASSERT(
				this->m_serialized_state_of_deleted_component.get_object().find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME) ==
					this->m_serialized_state_of_deleted_component.get_object()
						.end(),
				"your component has a reserved field already! you shoud use "
				"different "
				"field for serialization not: [{}]");

			this->m_is_serialized = true;

			p_factory->RemoveComponentByName(
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
	kotek::cfstream_t* p_file,
	kotek::core::ktkIResourceManager* p_resource_manager) noexcept
{
	KOTEK_ASSERT(p_file, "you must pass a valid resource handl");
	KOTEK_ASSERT(p_resource_manager, "must be valid!");
	KOTEK_ASSERT(this->m_p_component_name,
		"this must be initialized because it is issued as command from "
		"console");
	KOTEK_ASSERT(strlen(this->m_p_component_name), "must be not empty!");

	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"should be initialzed editor session manager here");

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid game manager pointer!");
		return kotek::size_t(-1);
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session editor#{}",
			this->m_p_manager_session_editor->get_current_session_id());
		return kotek::size_t(-1);
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return kotek::size_t(-1);
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

	kotek::cfstream_t& file = *p_file;

	if (p_factory)
	{
		if (this->m_is_serialized == false)
		{
			KOTEK_ASSERT(
				p_factory->IsValidEntity(this->m_id), "must be valid!");

			bool has_component =
				p_factory->HasComponent(this->m_id, this->m_p_component_name);

			if (has_component)
			{
				auto* p_raw_data_of_component = p_factory->GetComponentByName(
					this->m_id, this->m_p_component_name);

				this->m_serialized_state_of_deleted_component =
					p_factory->SerializeComponentByNameToJSON(this->m_id,
						this->m_p_component_name, this->m_storage_json_memory,
						sizeof(this->m_storage_json_memory));

				this->m_is_serialized = true;
			}
		}
		else
		{
			if (p_factory->IsValidEntity(this->m_id))
			{
				if (p_factory->HasComponent(
						this->m_id, this->m_p_component_name))
				{
					auto* p_raw_data_of_component =
						p_factory->GetComponentByName(
							this->m_id, this->m_p_component_name);

					this->m_serialized_state_of_deleted_component =
						p_factory->SerializeComponentByNameToJSON(this->m_id,
							this->m_p_component_name,
							this->m_storage_json_memory,
							sizeof(this->m_storage_json_memory));
				}
			}
		}
	}

	auto& object = this->m_serialized_state_of_deleted_component.get_object();

	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME] =
		this->GetCommandType();
	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME] =
		static_cast<kotek::uint32_t>(this->m_id);
	object[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME] =
		static_cast<kotek::enum_base_t>(
			p_factory->get_component_type_id_by_component_name(
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

		// p_resource_manager->Write(
		//	resource_handle_id, offset_string, sizeof(offset_string));
		file.write(offset_string, sizeof(offset_string));
		//	p_resource_manager->Write(resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kNewLine);
		file << std::endl;
		// p_resource_manager->Write(
		//	resource_handle_id, this->m_serialized_component_as_string);
		file << this->m_serialized_component_as_string;
		// p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		file << std::endl;
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

		//	p_resource_manager->Write(
		//		resource_handle_id, offset_string, sizeof(offset_string));
		file.write(offset_string, sizeof(offset_string));
		//	p_resource_manager->Write(resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kFlush);
		file.flush();
	}

	return offset;
}

void zircon_command_add_component_to_entity::Deserialize(
	const kotek::ktk::json::object& json) noexcept
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"should be initialzed game manager here");

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid game manager pointer!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session editor#{}",
			this->m_p_manager_session_editor->get_current_session_id());
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return;
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");

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
		p_factory
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
	this->m_is_serialized = true;
}

void zircon_command_add_component_to_entity::serialize_state()
{
	if (this->m_is_serialized == false)
	{
		KOTEK_ASSERT(this->m_p_manager_session_editor,
			"should be initialzed game manager here");

		if (!this->m_p_manager_session_editor)
		{
			KOTEK_MESSAGE_WARNING(
				"failed to execute due to invalid game manager pointer!");
			return;
		}

		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor->get_current_session_id());

		KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
			this->m_p_manager_session_editor->get_current_session_id());

		if (!p_session)
		{
			KOTEK_MESSAGE_WARNING(
				"failed to execute due to invalid session editor#{}",
				this->m_p_manager_session_editor->get_current_session_id());
			return;
		}

		zircon_world* p_world = p_session->get_world();

		KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());

		if (!p_world)
		{
			KOTEK_MESSAGE_WARNING("failed to execute due to invalid world in "
								  "session editor_{}#{}",
				p_session->get_session_name(), p_session->get_id());
			return;
		}

		zircon_factory* p_factory = p_world->get_factory();

		KOTEK_ASSERT(p_factory, "world must have valid factory!");
		KOTEK_ASSERT(p_factory->IsValidEntity(this->m_id), "entity must exist");
		KOTEK_ASSERT(this->m_p_component_name, "must be valid pointer");
		KOTEK_ASSERT(
			strlen(this->m_p_component_name), "must be not empty string");
		KOTEK_ASSERT(
			p_factory->HasComponent(this->m_id, this->m_p_component_name),
			"must exist otherwise wrong calling!");

		if (p_factory)
		{
			auto* p_raw_data_of_component = p_factory->GetComponentByName(
				this->m_id, this->m_p_component_name);

			this->m_serialized_state_of_deleted_component =
				p_factory->SerializeComponentByNameToJSON(this->m_id,
					this->m_p_component_name, this->m_storage_json_memory,
					sizeof(this->m_storage_json_memory));

			this->m_is_serialized = true;
		}
	}
}

bool zircon_command_add_component_to_entity::is_state_serialized()
	const noexcept
{
	return this->m_is_serialized;
}

zircon_component_type_t
zircon_command_add_component_to_entity::get_component_type()
{
	KOTEK_ASSERT(this->m_p_manager_session_editor,
		"should be initialzed editor session manager here");

	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid game manager pointer!");
		return zircon_component_type_t::kComponentTypeUnknown;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor->get_current_session_id());

	KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor->get_current_session_id());

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid session editor#{}",
			this->m_p_manager_session_editor->get_current_session_id());
		return zircon_component_type_t::kComponentTypeUnknown;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(p_world, "failed to obtain world in session editor_{}#{}",
		p_session->get_session_name(), p_session->get_id());

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid world in session editor_{}#{}",
			p_session->get_session_name(), p_session->get_id());
		return zircon_component_type_t::kComponentTypeUnknown;
	}

	zircon_factory* p_factory = p_world->get_factory();

	KOTEK_ASSERT(p_factory, "world must have valid factory!");
	KOTEK_ASSERT(this->m_p_component_name, "must be valid!");
	KOTEK_ASSERT(strlen(this->m_p_component_name), "must be not empty!");

	zircon_component_type_t result =
		zircon_component_type_t::kComponentTypeUnknown;

	if (p_factory)
	{
		if (this->m_p_component_name)
		{
			result = p_factory->get_component_type_id_by_component_name(
				this->m_p_component_name);
		}
	}

	return result;
}
