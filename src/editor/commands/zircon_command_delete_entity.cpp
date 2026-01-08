#include "zircon_command_delete_entity.h"
#include "../../ecs/zircon_factory.h"
#include "../../world/zircon_world.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"
#include "../commands/zircon_command_history.h"

zircon_command_delete_entity::zircon_command_delete_entity(
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::entity_t entity_to_delete
) :
	m_entity_created{entity_to_delete},
	m_entity_previous_id{kotek::ktk::kInvalidECSEntity},
	m_p_manager_session_editor{p_manager_session_editor},
	m_p_serialized_json_as_string{}, m_p_placement_new_memory{}
{
	KOTEK_ASSERT(
		p_manager_session_editor, "passed invalid game manager!"
	);
}

zircon_command_delete_entity::~zircon_command_delete_entity() {}

void zircon_command_delete_entity::Execute(void)
{
	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute command due "
		                      "to invalid game manager!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid session "
		    "editor#{}",
			this->m_p_manager_session_editor
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	zircon_editor_command_history* p_history =
		p_session->get_command_history();

	KOTEK_ASSERT(
		p_history,
		"failed to obtain command history manager in session "
	    "editor_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_history)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid command "
			"history in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	KOTEK_ASSERT(
		p_world->get_factory(), "world must contain factory!"
	);

	if (p_world)
	{
		// todo: reimpl
		// this->m_components =
		// this->m_p_factory->GetAllComponentsOfEntity(
		// this->m_entity_created);

		zircon_factory* p_factory = p_world->get_factory();

		this->m_components =
			p_factory->get_all_components_of_entity(
				this->m_entity_created
			);

		p_factory->RemoveEntity(this->m_entity_created);

		if (this->m_entity_created != entt::null)
		{
			this->m_entity_previous_id = this->m_entity_created;

			KOTEK_MESSAGE(
				"[history] removed entity: {}",
				static_cast<kotek::uint32_t>(
					this->m_entity_created
				)
			);
		}
	}
}

void zircon_command_delete_entity::Undo(void)
{
	if (!this->m_p_manager_session_editor)
	{
		KOTEK_MESSAGE_WARNING("failed to execute command due "
		                      "to invalid game manager!");
		return;
	}

	zircon_session_editor* p_session =
		this->m_p_manager_session_editor->get_session(
			this->m_p_manager_session_editor
				->get_current_session_id()
		);

	KOTEK_ASSERT(
		p_session,
		"failed to obtain session editor by id: {}",
		this->m_p_manager_session_editor
			->get_current_session_id()
	);

	if (!p_session)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid session "
		    "editor#{}",
			this->m_p_manager_session_editor
				->get_current_session_id()
		);
		return;
	}

	zircon_world* p_world = p_session->get_world();

	KOTEK_ASSERT(
		p_world,
		"failed to obtain world from session_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_world)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute command due to invalid world "
			"in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	zircon_editor_command_history* p_history =
		p_session->get_command_history();

	KOTEK_ASSERT(
		p_history,
		"failed to obtain command history manager in session "
	    "editor_{}#{}",
		p_session->get_session_name(),
		p_session->get_id()
	);

	if (!p_history)
	{
		KOTEK_MESSAGE_WARNING(
			"failed to execute due to invalid command "
			"history in session editor_{}#{}",
			p_session->get_session_name(),
			p_session->get_id()
		);
		return;
	}

	KOTEK_ASSERT(
		p_world->get_factory(), "world must contain factory!"
	);

	if (p_world)
	{
		this->m_entity_created = p_world->create_entity();

		if (this->m_entity_previous_id != entt::null &&
		    this->m_entity_created !=
		        this->m_entity_previous_id)
		{
			if (p_history)
			{
				p_history->update_dependent_commands(
					this->m_entity_previous_id,
					this->m_entity_created
				);

				if (this->m_components.empty() == false)
				{
					kotek::ktk::json::static_resource storage(
						this->m_p_placement_new_memory
					);

					for (zircon_component_type_t& type_id :
					     this->m_components)
					{
						kotek::ktk::json::value
							serialized_component(&storage);
						bool status =
							p_history
								->get_serialized_component_by_entity_and_component_type_id(
									serialized_component,
									this->m_entity_created,
									type_id
								);

						if (status)
						{
							p_world->get_factory()
								->create_component(
									this->m_entity_created,
									type_id,
									serialized_component
								);
						}
#ifdef KOTEK_DEBUG
						else
						{
							KOTEK_TRACE(
								"couldn't obtain component {} "
							    "from entity {}",
								static_cast<kotek::uint32_t>(
									type_id
								),
								static_cast<kotek::uint32_t>(
									this->m_entity_created
								)
							);
						}
#endif
					}
				}
			}
		}

		KOTEK_MESSAGE(
			"[history][undo] created entity: {}",
			static_cast<kotek::uint32_t>(this->m_entity_created)
		);
	}
}

const char* zircon_command_delete_entity::GetName()
{
	return "delete entity";
}

kotek::uint32_t zircon_command_delete_entity::GetEntityID(void
) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_entity_created);
}

void zircon_command_delete_entity::SetEntityID(
	kotek::uint32_t id
) noexcept
{
	this->m_entity_created = static_cast<entt::entity>(id);
}

kotek::enum_base_t
zircon_command_delete_entity::GetCommandType() noexcept
{
	return static_cast<kotek::ktk::enum_base_t>(
		kotek::core::eConsoleCommandIndex::
			kConsoleCommand_SDK_DeleteEntity
	);
}

kotek::size_t zircon_command_delete_entity::Serialize(
	kotek::core::ktkFileHandleType file
) noexcept
{
	KOTEK_ASSERT(
		file != kotek::core::kInvalidFileHandleType, "you must pass a valid resource manager handle!"
	);

	KOTEK_ASSERT(false, "todo: re-write please, also replace _file to FILE* handle");

	kotek::cfstream_t _file;

	kotek::ktk::json::static_resource storage(
		this->m_p_placement_new_memory
	);
	kotek::ktk::json::value out(&storage);

	auto& object = out.emplace_object();
	object
		[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME] =
			this->GetCommandType();
	object
		[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME] =
			static_cast<kotek::uint32_t>(this->m_entity_created
	        );
	auto& serializing_ids =
		object
			[ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_IDS_NAME]
				.emplace_array();

	for (const zircon_component_type_t& type_id :
	     this->m_components)
	{
		serializing_ids.push_back(
			static_cast<kotek::uint32_t>(type_id)
		);
	}

	kotek::ktk::json::serializer sr;
	sr.reset(&out);

	kotek::size_t offset{};

	while (!sr.done())
	{
		char buf[zircon_DEF_STREAM_JSON_STACK_SIZE]{};
		auto view = sr.read(buf, sizeof(buf));

		kotek::ktk::memory::memcpy(
			this->m_p_serialized_json_as_string + offset,
			buf,
			view.size()
		);
		offset += view.size();
	}

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE(
		"[history][{}] serialized command: [{}] with size "
	    "string: "
		"[{}] and total offset with endl symbol: [{}]",
		this->GetName(),
		this->m_p_serialized_json_as_string,
		offset,
		offset + 2
	);
#endif

	//if (p_resource_manager)
	{
		char offset_string[sizeof(
			zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS
		)];
		std::memset(offset_string, ' ', sizeof(offset_string));

		auto null_symbol_index =
			std::sprintf(offset_string, "%zu", offset + 2);

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

		// p_resource_manager->Write(
		//	resource_handle_id, offset_string,
		//sizeof(offset_string));
		_file.write(offset_string, sizeof(offset_string));
		// p_resource_manager->Write(resource_handle_id,
		//	kotek::core::eFileWritingControlCharacterType::kNewLine);
		_file << std::endl;
		//	p_resource_manager->Write(
		//		resource_handle_id,
		//this->m_p_serialized_json_as_string);
		_file << this->m_p_serialized_json_as_string;
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

		// sadly but safe version of sprintf fills buffer with
		// garbage values
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
		//sizeof(offset_string));
		_file.write(offset_string, sizeof(offset_string));
		//	p_resource_manager->Write(resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kFlush);
		_file.flush();
	}

	return offset;
}

void zircon_command_delete_entity::Deserialize(
	const Kotek::ktk::json::object& json
) noexcept
{
	KOTEK_ASSERT(
		json.find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME
		) != json.end(),
		"must exist key {}!",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME
	);

	auto type = static_cast<kotek::core::eConsoleCommandIndex>(
		json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME
	    )
			.to_number<kotek::enum_base_t>()
	);

	KOTEK_ASSERT(
		static_cast<kotek::enum_base_t>(type) ==
			this->GetCommandType(),
		"it is not {} command! Something is broken!",
		this->GetName()
	);

	this->m_entity_created = static_cast<entt::entity>(
		json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME
	    )
			.to_number<kotek::uint32_t>()
	);
	this->m_entity_previous_id = this->m_entity_created;

	KOTEK_ASSERT(
		json.find(
			ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_IDS_NAME
		) != json.end(),
		"you must serialize a such field: {} but the content "
	    "might be empty",
		ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_IDS_NAME
	);

	const auto& serialized_components =
		json.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_IDS_NAME
	    )
			.as_array();

	if (serialized_components.empty() == false)
	{
		for (auto& value : serialized_components)
		{
			zircon_component_type_t type_id =
				static_cast<zircon_component_type_t>(
					value.to_number<kotek::uint32_t>()
				);

			this->m_components.push_back(type_id);
		}
	}
}
