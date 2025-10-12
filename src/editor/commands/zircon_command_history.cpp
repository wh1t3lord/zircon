#include "zircon_command_history.h"
#include "../../world/zircon_world.h"

#include "zircon_command_create_entity.h"
#include "zircon_command_delete_entity.h"
#include "zircon_command_add_component_to_entity.h"
#include "zircon_command_delete_component_from_entity.h"

constexpr const char* _kExchangeFileNameWithExtension = "exchange.json";
constexpr const char* _kTempFileNameWithExtension = "temp.json";
constexpr const char* _kTempFileName = "temp";
constexpr const char* _kExchangeFileName = "exchange";

#define ZIRCON_ENABLE_CH_TRACE

zircon_editor_command_history::zircon_editor_command_history(void) :
	m_is_changed{}, m_is_first_serialize_happened{}, m_is_action_issued{},
	m_p_manager_session_editor{}, m_p_file_temp{},
	m_p_file_exchange{}, m_p_filesystem{}, m_index{},
	m_cursor_index{-1}, m_max_index{}, m_file_index{}, m_current_file_offset{},
	m_after_frame_file_offset{}, m_before_frame_file_offset{},
	m_end_of_previous_frame{}, m_start_of_next_frame{},
	m_exchange_file_offset_after{}, m_current_amount_of_created_commands{}
{
	for (int i = 0; i < this->m_commands.size(); ++i)
	{
		this->m_commands[i] = nullptr;
	}

	auto element_size = sizeof(this->m_storage[0]);
	for (int i = 0; i < this->m_storage.size(); ++i)
	{
		memset(this->m_storage[i], 0, element_size);
	}

	memset(this->m_p_memory_for_stack_command_creation, 0,
		sizeof(this->m_p_memory_for_stack_command_creation));

	static_assert(
		zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY != ' ' &&
		"you can't use whitespace as delimiter because with whitespace we "
		"define right side (e.g. next entry) for moving cursor otherwise it is "
		"impossible to determine which direction we want to use in file for "
		"moving cursor that means we can't define operations like undo and "
		"redo, undo is moving to the beggining of file redo means moving to "
		"the end of file");
}
zircon_editor_command_history::~zircon_editor_command_history(void) {}

void zircon_editor_command_history::initialize(
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::core::ktkIFileSystem* p_filesystem)
{
	KOTEK_ASSERT(p_filesystem,
		"you must pass a valid pointer of file system interface!");
	KOTEK_ASSERT(p_manager_session_editor,
		"you must pass a valid session editor manager!");

	this->m_p_filesystem = p_filesystem;
	this->m_p_manager_session_editor = p_manager_session_editor;

	ktk_filesystem_path path_to_file;

	this->m_p_filesystem->Make_Path(
		path_to_file,
		kotek::core::eFolderIndex::
			kFolderIndex_DataUser_SDK_Scenes
	);

	//= this->m_p_filesystem->GetFolderByEnum(
	//		kotek::core::eFolderIndex::kFolderIndex_DataUser_SDK_Scenes);

	path_to_file /= "current";

	this->m_path_to_streaming_folder.append(
		reinterpret_cast<char*>(path_to_file.u8string().data()),
		path_to_file.u8string().size());

	bool is_valid_path = this->m_p_filesystem->Is_Exists(
		this->m_path_to_streaming_folder.c_str());

	if (is_valid_path == false)
	{
		this->m_p_filesystem->Create_Directory(
			this->m_path_to_streaming_folder.c_str(),
			kotek::core::eFolderVisibilityType::kVisible);
	}

	KOTEK_ASSERT(false, "todo: re-write please");
	/* todo: re-write please
	if (this->m_p_resource_manager)
	{
		kotek::core::ktkResourceFileStreamRequest request;
		request.path_to_file = path_to_file / _kTempFileNameWithExtension;
		request.resource_type =
			kotek::core::eResourceRequestResourceType::kText;
		request.operation_type =
			kotek::core::eResourceRequestOperationType::kSave;

		this->m_p_file_temp =
			this->m_p_resource_manager->Open_FileStream(request);

		request.path_to_file = path_to_file / _kExchangeFileNameWithExtension;

		this->m_p_file_exchange =
			this->m_p_resource_manager->Open_FileStream(request);

		KOTEK_ASSERT(this->m_p_file_temp,
			"no avaiable fstream from resource manager! Out of resource!!!");
		KOTEK_ASSERT(this->m_p_file_exchange,
			"no available fstream from resource manager! Out of resources!!!");
	}*/
}

void zircon_editor_command_history::shutdown(void)
{
	KOTEK_ASSERT(false, "todo: re-write please");

	/* todo: re-write please
	this->m_p_resource_manager->Close_FileStream(this->m_p_file_temp);
	this->m_p_resource_manager->Close_FileStream(this->m_p_file_exchange);*/

	for (const auto& path_file : kotek::ktk::filesystem::directory_iterator(
			 this->m_path_to_streaming_folder.c_str()))
	{
		kotek::ktk::filesystem::remove(path_file);
	}
}

void zircon_editor_command_history::ExecuteCommand(
	kotek::core::ktkISDKRedoUndo* p_command)
{
	KOTEK_ASSERT(p_command, "you can't send an invalid command here");

	//	this->m_index = this->m_cursor_index %
	//	static_cast<size_t>(zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE);

	this->m_commands[this->m_index] = p_command;

	if (p_command->GetCommandType() ==
		static_cast<kotek::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_DeleteComponentFromEntity))
	{
		// trying to find a component that relates to add component from entity
		// where entity is same as in delete component otherwise serialization
		// of add component is invalid because the serialization state was never
		// issued when frame moves next
		zircon_command_delete_component_from_entity* p_casted_command =
			static_cast<zircon_command_delete_component_from_entity*>(
				p_command);

		for (auto* p_cmd : this->m_commands)
		{
			if (p_cmd)
			{
				if (p_cmd->GetCommandType() ==
					static_cast<kotek::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateComponentForEntity))
				{
					if (p_cmd->GetEntityID() == p_command->GetEntityID())
					{
						// we found or add component version, so we need
						// serialize it but only that relates to the same
						// component and entity

						zircon_command_add_component_to_entity* p_casted_cmd =
							static_cast<
								zircon_command_add_component_to_entity*>(p_cmd);

						if (p_casted_command->get_component_type() ==
							p_casted_cmd->get_component_type())
						{
							p_casted_cmd->serialize_state();
						}
					}
				}
			}
		}
	}

	if (p_command->GetCommandType() ==
		static_cast<kotek::enum_base_t>(kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_DeleteEntity))
	{
		zircon_command_delete_entity* p_casted_command =
			static_cast<zircon_command_delete_entity*>(p_command);

		for (auto* p_cmd : this->m_commands)
		{
			if (p_cmd)
			{
				if (p_cmd->GetCommandType() ==
					static_cast<kotek::enum_base_t>(
						kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateComponentForEntity))
				{
					if (p_cmd->GetEntityID() == p_command->GetEntityID())
					{
						zircon_command_add_component_to_entity* p_casted_cmd =
							static_cast<
								zircon_command_add_component_to_entity*>(p_cmd);

						p_casted_cmd->serialize_state();
					}
				}
			}
		}
	}

	p_command->Execute();

	set_changed(true);
}

void zircon_editor_command_history::Undo()
{
#ifdef ZIRCON_ENABLE_CH_TRACE
	KOTEK_TRACE("undo: {}", this->m_cursor_index);
#endif

	if (this->m_cursor_index > -1)
	{
		KOTEK_ASSERT(this->m_commands[this->m_index], "something is wrong!");
		this->m_commands[this->m_index]->Undo();

		if (this->m_index > 0)
			--this->m_index;

		m_is_action_issued = true;

		if (this->m_current_file_offset > 0)
		{
			// TODO: реализовать удаление команд когда совершили действие после
			// undo
			if (this->m_index == 0)
			{
				if (this->m_cursor_index > 0 &&
					this->m_cursor_index %
							zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE ==
						0)
				{
					this->unload_content();

					//	this->reopen_current_file(this->m_file_resource_handle_id);

					this->m_p_file_temp =
						this->reopen_current_file(this->m_p_file_temp);

					if (this->m_p_file_temp == nullptr)
						return;

					this->insert_content(
						0, this->m_exchange_file_offset_after, 0);

					// this->m_p_resource_manager->Seekg(
					//	this->m_file_resource_handle_id,
					//	this->m_before_frame_file_offset,
					//	kotek::core::eFileSeekDirectionType::
					//		kSeekDirectionBegin);

					this->m_p_file_temp->seekg(
						this->m_before_frame_file_offset, std::ios_base::beg);

					//	auto file_size = this->m_p_resource_manager->Tellg(
					//	this->m_file_resource_handle_id);

					auto file_size = this->m_p_file_temp->tellg();

					char buffer[sizeof(
						zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

					this->m_index =
						zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE - 1;

					for (auto* p_command : this->m_commands)
					{
						if (p_command)
						{
							KOTEK_ASSERT(
								false, "todo: re-write please"
							);
						//	p_command->Serialize(this->m_p_file_temp);
						}
					}

					for (auto* p_command : this->m_commands)
					{
						if (p_command)
						{
							p_command->~ktkISDKRedoUndo();
						}
					}

					for (auto* p_placement_new_memory : this->m_storage)
					{
						std::memset(p_placement_new_memory, 0,
							sizeof(this->m_storage[0]));
					}

					std::memset(
						this->m_commands.data(), 0, sizeof(this->m_commands));

					auto total_count =
						zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
					const auto size_for_number = sizeof(
						zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS);
					char stream_buffer_for_json_data
						[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

					// this is only for placing memory for each restored command
					auto copy_index = this->m_index;

					// this->m_p_resource_manager->Seekg(
					//	this->m_file_resource_handle_id, 0,
					//	kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

					this->m_p_file_temp->seekg(0, std::ios_base::end);

					// auto file_size_after_serialize =
					//	this->m_p_resource_manager->Tellg(
					//		this->m_file_resource_handle_id);

					auto file_size_after_serialize =
						this->m_p_file_temp->tellg();

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_exchange_resource_handle_id, 0,
					//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

					this->m_p_file_exchange->seekg(0, std::ios_base::end);

					//	auto file_size_of_exchange =
					//		this->m_p_resource_manager->Tellg(
					//			this->m_file_exchange_resource_handle_id);

					auto file_size_of_exchange =
						this->m_p_file_exchange->tellg();

					this->insert_content(this->m_exchange_file_offset_after,
						file_size_of_exchange, file_size_after_serialize);

					this->m_after_frame_file_offset =
						this->m_before_frame_file_offset;
					this->m_current_file_offset =
						this->m_after_frame_file_offset;

					for (int i = 0; i < total_count; ++i)
					{
						std::memset(buffer, 0, sizeof(buffer));
						this->m_current_file_offset -= size_for_number;

						// this->m_p_resource_manager->Seekg(
						//	this->m_file_resource_handle_id,
						//	this->m_current_file_offset,
						//	kotek::core::eFileSeekDirectionType::
						//		kSeekDirectionBegin);

						this->m_p_file_temp->seekg(
							this->m_current_file_offset, std::ios_base::beg);

						// this->m_p_resource_manager->Read(
						//	this->m_file_resource_handle_id, buffer,
						//	sizeof(buffer));

						this->m_p_file_temp->read(buffer, sizeof(buffer));

						if (buffer
								[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
							zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
						{
							// making a suggestion that we need to read next
							// because we're on right side

							this->m_current_file_offset -= size_for_number;

							// this->m_p_resource_manager->Seekg(
							//	this->m_file_resource_handle_id,
							//	this->m_current_file_offset,
							//	kotek::core::eFileSeekDirectionType::
							//		kSeekDirectionBegin);

							this->m_p_file_temp->seekg(
								this->m_current_file_offset,
								std::ios_base::beg);

							//	this->m_p_resource_manager->Read(
							//		this->m_file_resource_handle_id, buffer,
							//		sizeof(buffer));

							this->m_p_file_temp->read(buffer, sizeof(buffer));

							KOTEK_ASSERT(
								buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
									zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
								"something is broken, maybe corrupted data, "
								"unable parse data!");
						}

						auto offset_for_json_data = std::atoi(buffer);

						KOTEK_ASSERT(offset_for_json_data > 0,
							"something is not right!");

						this->m_current_file_offset -= offset_for_json_data;
						// removing endl size
						auto real_size_for_json_data = offset_for_json_data - 2;

						// this->m_p_resource_manager->Seekg(
						//	this->m_file_resource_handle_id,
						//	this->m_current_file_offset,
						//	kotek::core::eFileSeekDirectionType::
						//	kSeekDirectionBegin);

						this->m_p_file_temp->seekg(
							this->m_current_file_offset, std::ios_base::beg);

						kotek::json::stream_parser parser;
						kotek::json::static_resource storage_ptr(
							this->m_p_memory_for_stack_parser);
						parser.reset(&storage_ptr);

						if (real_size_for_json_data >
							zircon_DEF_STREAM_JSON_STACK_SIZE)
						{
							int counter{real_size_for_json_data};
							auto prev_size{this->m_current_file_offset};

							while (counter > 0)
							{
								if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
								{
									//	this->m_p_resource_manager->Read(
									//		this->m_file_resource_handle_id,
									//		stream_buffer_for_json_data,
									//		sizeof(stream_buffer_for_json_data));

									this->m_p_file_temp->read(
										stream_buffer_for_json_data,
										sizeof(stream_buffer_for_json_data));

									this->m_current_file_offset +=
										zircon_DEF_STREAM_JSON_STACK_SIZE;
									//	this->m_p_resource_manager->Seekg(
									//		this->m_file_resource_handle_id,
									//		this->m_current_file_offset,
									//		kotek::core::eFileSeekDirectionType::
									//			kSeekDirectionBegin);

									this->m_p_file_temp->seekg(
										this->m_current_file_offset,
										std::ios_base::beg);
								}
								else
								{
									//	this->m_p_resource_manager->Read(
									//		this->m_file_resource_handle_id,
									//		stream_buffer_for_json_data,
									// counter);
									this->m_p_file_temp->read(
										stream_buffer_for_json_data, counter);
									this->m_current_file_offset += counter;
								}

								if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
								{
									parser.write(stream_buffer_for_json_data,
										sizeof(stream_buffer_for_json_data));
								}
								else
								{
									parser.write(
										stream_buffer_for_json_data, counter);
								}

								counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
								std::memset(stream_buffer_for_json_data, 0,
									sizeof(stream_buffer_for_json_data));
							}
							this->m_current_file_offset -=
								real_size_for_json_data;
							KOTEK_ASSERT(
								this->m_current_file_offset == prev_size,
								"wrong calculations! after parsing you have to "
								"get exactly the same size as you minused "
								"offset_for_json_data!");

							if (this->m_current_file_offset > 0)
								this->m_current_file_offset -= 2;

							auto status = parser.done();

							KOTEK_ASSERT(status, "must be valid json!");
						}
						else
						{
							//	this->m_p_resource_manager->Read(
							//		this->m_file_resource_handle_id,
							//		stream_buffer_for_json_data,
							//		real_size_for_json_data);
							this->m_p_file_temp->read(
								stream_buffer_for_json_data,
								real_size_for_json_data);
							parser.write(stream_buffer_for_json_data);
							auto status = parser.done();
							KOTEK_ASSERT(
								status, "must be valid json in stream buffer!");
						}

						kotek::json::value json_data = parser.release();

						KOTEK_ASSERT(json_data.is_object(), "must be object!");

						auto& json = json_data.as_object();

						KOTEK_ASSERT(json.find("command") != json.end(),
							"must exist a such key, because we can't "
							"understand which command we need to restore!");

						kotek::core::eConsoleCommandIndex type =
							static_cast<kotek::core::eConsoleCommandIndex>(
								json.at("command").as_int64());

						switch (type)
						{
						case kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateEntity:
						{
							auto placement_storage =
								this->m_storage[copy_index];
							zircon_command_create_entity* p_command =
								new (placement_storage)
									zircon_command_create_entity(
										this->m_p_manager_session_editor);
							p_command->Deserialize(json);

							this->m_commands[copy_index] = p_command;

							--copy_index;
							break;
						}
						case kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_DeleteEntity:
						{
							auto placement_storage =
								this->m_storage[copy_index];
							zircon_command_delete_entity* p_command =
								new (placement_storage)
									zircon_command_delete_entity(
										this->m_p_manager_session_editor,
										entt::null);
							p_command->Deserialize(json);

							this->m_commands[copy_index] = p_command;

							--copy_index;
							break;
						}
						case kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateComponentForEntity:
						{
							auto placement_storage =
								this->m_storage[copy_index];
							zircon_command_add_component_to_entity* p_command =
								new (placement_storage)
									zircon_command_add_component_to_entity(
										this->m_p_manager_session_editor);
							p_command->Deserialize(json);

							this->m_commands[copy_index] = p_command;

							--copy_index;
							break;
						}
						case kotek::core::eConsoleCommandIndex::
							kConsoleCommand_SDK_DeleteComponentFromEntity:
						{
							auto placement_storage =
								this->m_storage[copy_index];

							zircon_command_delete_component_from_entity*
								p_command = new (placement_storage)
									zircon_command_delete_component_from_entity(
										this->m_p_manager_session_editor);

							p_command->Deserialize(json);

							this->m_commands[copy_index] = p_command;
							--copy_index;
							break;
						}
						default:
						{
							KOTEK_ASSERT(false,
								"can't be!!! data corruption?! or you forgot "
								"to add a new type");
							break;
						}
						}
					}

					KOTEK_ASSERT(this->m_current_file_offset >= sizeof(buffer),
						"something is really wrong because I need to get "
						"offset equal or higher than 0");

					this->m_current_file_offset -= sizeof(buffer);
					this->m_before_frame_file_offset =
						this->m_current_file_offset;
				}
			}
		}

		--this->m_cursor_index;
		set_changed(true);
	}
}

void zircon_editor_command_history::Redo()
{
	/*
	if (this->m_storage.empty() == false &&
	    this->m_index < this->m_storage.size())
	{
	    this->m_storage[this->m_index]->Execute();
	    ++this->m_index;
	}
	*/

#ifdef ZIRCON_ENABLE_CH_TRACE
	KOTEK_TRACE("redo: {}", this->m_cursor_index);
#endif

	if (this->m_cursor_index < this->m_max_index - 1 ||
		(this->m_cursor_index == -1 && this->m_max_index > 0))
	{
		m_is_action_issued = true;

		constexpr auto index_when_moving_to_next_frame =
			zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE - 1;

		auto real_cursor_index =
			this->m_cursor_index % zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;

#ifdef KOTEK_DEBUG
		if (real_cursor_index == index_when_moving_to_next_frame)
		{
			KOTEK_ASSERT(this->m_index == index_when_moving_to_next_frame,
				"something is wrong with calculations, because m_index must be "
				"equal to index_when_moving_to_next_frame as well as "
				"m_cursor_index to the same "
				"value (index_when_moving_to_next_frame)");
		}
#endif

		if (this->m_index == index_when_moving_to_next_frame)
		{
			KOTEK_ASSERT(real_cursor_index == index_when_moving_to_next_frame,
				"something is  broken because it is expected that when m_index "
				"== index_when_moving_to_next_frame then it means "
				"m_cursor_index must be equal to the same value it doesn't so "
				"it can't be");

			if (real_cursor_index == index_when_moving_to_next_frame)
			{
				this->unload_content();

				this->m_p_file_temp =
					this->reopen_current_file(this->m_p_file_temp);

				this->insert_content(0, this->m_exchange_file_offset_after, 0);

				// this->m_p_resource_manager->Seekg(
				//	this->m_file_resource_handle_id,
				//	this->m_before_frame_file_offset,
				//	kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

				this->m_p_file_temp->seekg(
					this->m_before_frame_file_offset, std::ios_base::beg);

				// auto file_size = this->m_p_resource_manager->Tellg(
				//	this->m_file_resource_handle_id);

				auto file_size = this->m_p_file_temp->tellg();

				for (auto* p_command : this->m_commands)
				{
					if (p_command)
					{
						KOTEK_ASSERT(
							false, "todo: re-write please"
						);
					//	p_command->Serialize(
					//		this->m_p_file_temp);
					}
				}

				for (auto* p_command : this->m_commands)
				{
					if (p_command)
					{
						p_command->~ktkISDKRedoUndo();
					}
				}

				for (auto* p_placement_new_memory : this->m_storage)
				{
					std::memset(
						p_placement_new_memory, 0, sizeof(this->m_storage[0]));
				}

				std::memset(
					this->m_commands.data(), 0, sizeof(this->m_commands));

				char stream_buffer_for_json_data
					[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

				char buffer[sizeof(
					zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

				// this->m_p_resource_manager->Seekg(
				//	this->m_file_resource_handle_id, 0,
				//	kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

				this->m_p_file_temp->seekg(0, std::ios_base::end);

				// auto file_size_after_serialize =
				//	this->m_p_resource_manager->Tellg(
				//		this->m_file_resource_handle_id);

				auto file_size_after_serialize = this->m_p_file_temp->tellg();

				// this->m_p_resource_manager->Seekg(
				//	this->m_file_exchange_resource_handle_id, 0,
				//	kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

				this->m_p_file_exchange->seekg(0, std::ios_base::end);

				// auto file_size_of_exchange =
				// this->m_p_resource_manager->Tellg(
				//	this->m_file_exchange_resource_handle_id);

				auto file_size_of_exchange = this->m_p_file_exchange->tellg();

				this->insert_content(this->m_exchange_file_offset_after,
					file_size_of_exchange, file_size_after_serialize);

				this->m_before_frame_file_offset = file_size_after_serialize;

				this->m_current_file_offset = this->m_before_frame_file_offset;
				int command_count{};
				for (int i = 0; i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
					++i)
				{
					std::memset(buffer, 0, sizeof(buffer));
					// this->m_p_resource_manager->Seekg(
					//	this->m_file_resource_handle_id,
					//	this->m_current_file_offset,
					//	kotek::core::eFileSeekDirectionType::
					//		kSeekDirectionBegin);
					this->m_p_file_temp->seekg(
						this->m_current_file_offset, std::ios_base::beg);
					// this->m_p_resource_manager->Read(
					//	this->m_file_resource_handle_id, buffer,
					//	sizeof(buffer));
					this->m_p_file_temp->read(buffer, sizeof(buffer));
					if (buffer
							[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
					{
						auto current_offset = this->m_current_file_offset;
						current_offset += sizeof(buffer);

						// this->m_p_resource_manager->Seekg(
						//	this->m_file_resource_handle_id, 0,
						//	kotek::core::eFileSeekDirectionType::
						//		kSeekDirectionEnd);
						this->m_p_file_temp->seekg(0, std::ios_base::end);

						//	auto file_size = this->m_p_resource_manager->Tellg(
						//		this->m_file_resource_handle_id);

						auto file_size = this->m_p_file_temp->tellg();

						// we reached end of file, so we can't move further,
						// making leaving from this stack...
						if (current_offset == file_size)
						{
							this->m_current_file_offset =
								this->m_before_frame_file_offset;
							//	this->m_p_resource_manager->Seekg(
							//		this->m_file_resource_handle_id,
							//		this->m_current_file_offset,
							//		kotek::core::eFileSeekDirectionType::
							//			kSeekDirectionBegin);
							this->m_p_file_temp->seekg(
								this->m_current_file_offset,
								std::ios_base::beg);
							this->m_current_file_offset -= sizeof(buffer);

							break;
						}
						else
						{
							// everything is fine we can move further
							this->m_current_file_offset += sizeof(buffer);
							// this->m_p_resource_manager->Seekg(
							//	this->m_file_resource_handle_id,
							//	this->m_current_file_offset,
							//	kotek::core::eFileSeekDirectionType::
							//		kSeekDirectionBegin);
							this->m_p_file_temp->seekg(
								this->m_current_file_offset,
								std::ios_base::beg);
							// this->m_p_resource_manager->Read(
							//	this->m_file_resource_handle_id, buffer,
							//	sizeof(buffer));
							this->m_p_file_temp->read(buffer, sizeof(buffer));
						}
					}

					KOTEK_ASSERT(
						buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
							zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
						"that means we are reading for going to back (to "
						"the "
						"beginning of file) that's not correct and data "
						"might "
						"be corrupted, check everything again!");

					auto offset_for_json_data = std::atoi(buffer);

					KOTEK_ASSERT(offset_for_json_data > 0,
						"bad cast or something is broken when data was "
						"written "
						"to file!");

					this->m_current_file_offset += sizeof(buffer) + 2;

					// getting real json exact string size for reading
					auto real_size_for_json_data = offset_for_json_data - 2;

					// this->m_p_resource_manager->Seekg(
					//	this->m_file_resource_handle_id,
					//	this->m_current_file_offset,
					//	kotek::core::eFileSeekDirectionType::
					//		kSeekDirectionBegin);

					this->m_p_file_temp->seekg(
						this->m_current_file_offset, std::ios_base::beg);

					kotek::json::stream_parser parser;
					kotek::json::static_resource storage_ptr(
						this->m_p_memory_for_stack_parser);
					parser.reset(&storage_ptr);

					if (real_size_for_json_data >
						zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						int counter{real_size_for_json_data};
						auto prev_size{this->m_current_file_offset};

						while (counter > 0)
						{
							if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
							{
								// this->m_p_resource_manager->Read(
								//	this->m_file_resource_handle_id,
								//	stream_buffer_for_json_data,
								//	sizeof(stream_buffer_for_json_data));
								this->m_p_file_temp->read(
									stream_buffer_for_json_data,
									sizeof(stream_buffer_for_json_data));
								this->m_current_file_offset +=
									zircon_DEF_STREAM_JSON_STACK_SIZE;
								// this->m_p_resource_manager->Seekg(
								//	this->m_file_resource_handle_id,
								//	this->m_current_file_offset,
								//	kotek::core::eFileSeekDirectionType::
								//		kSeekDirectionBegin);
								this->m_p_file_temp->seekg(
									this->m_current_file_offset,
									std::ios_base::beg);
							}
							else
							{
								//	this->m_p_resource_manager->Read(
								//		this->m_file_resource_handle_id,
								//		stream_buffer_for_json_data, counter);
								this->m_p_file_temp->read(
									stream_buffer_for_json_data, counter);
								this->m_current_file_offset += counter;
							}

							if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
							{
								parser.write(stream_buffer_for_json_data,
									sizeof(stream_buffer_for_json_data));
							}
							else
							{
								parser.write(
									stream_buffer_for_json_data, counter);
							}

							counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
							std::memset(stream_buffer_for_json_data, 0,
								sizeof(stream_buffer_for_json_data));
						}

						KOTEK_ASSERT((this->m_current_file_offset -
										 real_size_for_json_data) == prev_size,
							"wrong calculations! after parsing you have to "
							"get exactly the same size as you minused "
							"offset_for_json_data! curret_offset:[{}] "
							"real_size_for_json_data:[{}] dif:[{}] "
							"prev_size:[{}]",
							this->m_current_file_offset,
							real_size_for_json_data,
							(this->m_current_file_offset -
								real_size_for_json_data),
							prev_size);

						if (this->m_current_file_offset > 0)
							this->m_current_file_offset += 2;

						auto status = parser.done();

						KOTEK_ASSERT(status, "must be valid json!");
					}
					else
					{
						// this->m_p_resource_manager->Read(
						//	this->m_file_resource_handle_id,
						//	stream_buffer_for_json_data,
						//	real_size_for_json_data);
						this->m_p_file_temp->read(stream_buffer_for_json_data,
							real_size_for_json_data);
						parser.write(stream_buffer_for_json_data);
						auto status = parser.done();
						KOTEK_ASSERT(
							status, "must be valid json in stream buffer!");

						this->m_current_file_offset += real_size_for_json_data;

						if (this->m_current_file_offset > 0)
							this->m_current_file_offset += 2;
					}

					kotek::json::value json_data = parser.release();

					KOTEK_ASSERT(json_data.is_object(), "must be object!");

					auto& json = json_data.as_object();

					KOTEK_ASSERT(json.find("command") != json.end(),
						"must exist a such key, because we can't "
						"understand which command we need to restore!");

					kotek::core::eConsoleCommandIndex type =
						static_cast<kotek::core::eConsoleCommandIndex>(
							json.at("command").as_int64());

					switch (type)
					{
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_CreateEntity:
					{
						auto placement_storage = this->m_storage[i];
						zircon_command_create_entity* p_command =
							new (placement_storage)
								zircon_command_create_entity(
									this->m_p_manager_session_editor);
						p_command->Deserialize(json);

						this->m_commands[i] = p_command;
						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_DeleteEntity:
					{
						auto placement_storage = this->m_storage[i];
						zircon_command_delete_entity* p_command =
							new (placement_storage)
								zircon_command_delete_entity(
									this->m_p_manager_session_editor, entt::null);

						p_command->Deserialize(json);

						this->m_commands[i] = p_command;
						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_CreateComponentForEntity:
					{
						auto placement_storage = this->m_storage[i];
						zircon_command_add_component_to_entity* p_command =
							new (placement_storage)
								zircon_command_add_component_to_entity(
									this->m_p_manager_session_editor);

						p_command->Deserialize(json);

						this->m_commands[i] = p_command;
						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_DeleteComponentFromEntity:
					{
						auto placement_storage = this->m_storage[i];
						zircon_command_delete_component_from_entity* p_command =
							new (placement_storage)
								zircon_command_delete_component_from_entity(
									this->m_p_manager_session_editor);

						p_command->Deserialize(json);

						this->m_commands[i] = p_command;
						break;
					}
					default:
					{
						KOTEK_ASSERT(false,
							"can't be!!! data corruption?! or you forgot to "
							"add a new type");
						break;
					}
					}

					++command_count;
				}

				// in order to save the logic when we move cursor further
				this->m_index = 0;
				this->m_current_file_offset += sizeof(buffer);
				this->m_after_frame_file_offset = this->m_current_file_offset;
			}
		}

		// todo: think about this logical carefully, i think something is wrong
		// here
		if (this->m_cursor_index < kotek::ptrdiff_t(this->m_max_index) - 1)
		{
			++this->m_cursor_index;
			this->m_index = this->m_cursor_index %
				zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;

			KOTEK_ASSERT(this->m_commands[this->m_index], "something is wrong");
			auto* p_command = this->m_commands[this->m_index];
			if (p_command)
				p_command->Execute();

			set_changed(true);
		}
		else
		{
			int a = 0;
		}
	}
}

void zircon_editor_command_history::set_changed(bool status) noexcept
{
	this->m_is_changed = status;
}

bool zircon_editor_command_history::is_changed() const noexcept
{
	return this->m_is_changed;
}

const kotek::array_t<kotek::core::ktkISDKRedoUndo*,
	zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
zircon_editor_command_history::GetCommands(void) const noexcept
{
	return this->m_commands;
}

void zircon_editor_command_history::update_dependent_commands(
	entt::entity id_what_will_be_deleted,
	entt::entity id_that_replaces_what_will_be_deleted) noexcept
{
	for (auto* p_command : this->m_commands)
	{
		if (p_command)
		{
			if (p_command->GetEntityID() ==
				static_cast<kotek::uint32_t>(id_what_will_be_deleted))
			{
				p_command->SetEntityID(static_cast<kotek::uint32_t>(
					id_that_replaces_what_will_be_deleted));
			}
		}
	}

	// update info in serialized commands
	// also update offsets before and after
	// and offsets for reading in file

	this->update_dependent_serialized_commands(
		id_what_will_be_deleted, id_that_replaces_what_will_be_deleted);
}

unsigned char* zircon_editor_command_history::allocate_memory_for_command(
	kotek::size_t size_of_class, const char* p_debug_type_name) noexcept
{
	KOTEK_ASSERT(size_of_class != 0 && size_of_class != kotek::size_t(-1),
		"you can't pass a invalid size!");
	KOTEK_ASSERT(size_of_class <= zircon_DEF_MAXIMUM_COMMAND_SIZE,
		"you passed size larger than standard we can't allocate!");

	unsigned char* pResultPlacementNewBuffer{};

	constexpr size_t _kLimitSize =
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE - 1;

	this->clear_content_when_action_issued();

	++this->m_cursor_index;
	++this->m_max_index;

#ifdef ZIRCON_ENABLE_CH_TRACE
	KOTEK_TRACE("command: {} ({})", this->m_max_index, p_debug_type_name);
#endif

	this->m_index = this->m_cursor_index %
		static_cast<size_t>(zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE);
	// update new data
	if (this->m_cursor_index > 0 &&
		this->m_cursor_index % zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE == 0)
	{
		for (auto* p_command : this->m_commands)
		{
			KOTEK_ASSERT(p_command, "must be valid!");
			KOTEK_ASSERT(false, "todo: re-write please");
		//	p_command->Serialize(
		//		this->m_p_file_temp);
		}

		for (auto* p_command : this->m_commands)
		{
			KOTEK_ASSERT(p_command, "must be valid");
			p_command->~ktkISDKRedoUndo();
		}

		for (auto* p_placement_new_memory : this->m_storage)
		{
			std::memset(p_placement_new_memory, 0, sizeof(this->m_storage[0]));
		}

		std::memset(this->m_commands.data(), 0, sizeof(this->m_commands));

		auto pStart = this->m_storage[this->m_index];
		pResultPlacementNewBuffer = pStart;

		this->m_current_file_offset = this->m_p_file_temp->tellg();

		this->m_end_of_previous_frame = this->m_start_of_next_frame;
		this->m_start_of_next_frame = this->m_current_file_offset;

		this->m_before_frame_file_offset = this->m_start_of_next_frame;
	}
	else
	{
		auto pStart = this->m_storage[this->m_index];
		pResultPlacementNewBuffer = pStart;
	}

	return pResultPlacementNewBuffer;
}

kotek::size_t zircon_editor_command_history::get_current_index(void) const
{
	return this->m_index;
}

kotek::ptrdiff_t zircon_editor_command_history::get_cursor_index(void) const
{
	return this->m_cursor_index;
}

bool zircon_editor_command_history::
	get_serialized_component_by_entity_and_component_type_id(
		kotek::ktk::json::value&
			constructed_value_on_stack_based_on_placement_new_memory,
		entt::entity id, zircon_component_type_t type_id)
{
	KOTEK_ASSERT(this->m_p_file_temp, "early calling, must be initialized!");

	bool result{};

//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())

		{
			//	kotek::size_t restored_offset =
			// this->m_p_resource_manager->Tellg(
			//		this->m_file_resource_handle_id);
			kotek::size_t restored_offset = this->m_p_file_temp->tellg();

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0, kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);
			this->m_p_file_temp->seekg(0, std::ios_base::end);

			//	kotek::size_t file_size = this->m_p_resource_manager->Tellg(
			//		this->m_file_resource_handle_id);
			kotek::size_t file_size = this->m_p_file_temp->tellg();

			kotek::size_t current_offset{};

			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			char buffer[sizeof(
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

			this->m_p_file_exchange =
				this->reopen_exchange_file(this->m_p_file_exchange);

			// this->m_p_resource_manager->Seekg(
			//	this->m_file_exchange_resource_handle_id, 0,
			//	kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_exchange->seekg(0, std::ios_base::beg);

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0,
			// kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(0, std::ios_base::beg);

			while (current_offset < file_size)
			{
				std::memset(buffer, 0, sizeof(buffer));

				kotek::size_t current_section_offset_begin{current_offset};
				kotek::size_t current_section_offset_end{};

				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id, buffer,
				// sizeof(buffer));

				this->m_p_file_temp->read(buffer, sizeof(buffer));

				if (buffer
						[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
					zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
				{
					auto test_current_offset = current_offset;
					test_current_offset += sizeof(buffer);

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, 0,
					//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

					this->m_p_file_temp->seekg(0, std::ios_base::end);

					//	auto file_size = this->m_p_resource_manager->Tellg(
					//		this->m_file_resource_handle_id);

					auto file_size = this->m_p_file_temp->tellg();

					// we reached end of file, so we can't move further,
					// making leaving from this stack...
					if (test_current_offset == file_size)
					{
						break;
					}
					else
					{
						// everything is fine we can move further
						current_offset += sizeof(buffer);
						// this->m_p_resource_manager->Seekg(
						//	this->m_file_resource_handle_id, current_offset,
						//	kotek::core::eFileSeekDirectionType::
						//		kSeekDirectionBegin);

						this->m_p_file_temp->seekg(
							current_offset, std::ios_base::beg);

						// this->m_p_resource_manager->Read(
						//	this->m_file_resource_handle_id, buffer,
						//	sizeof(buffer));
						this->m_p_file_temp->read(buffer, sizeof(buffer));
					}
				}

				KOTEK_ASSERT(
					buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
					"that means we are reading for going to back (to "
					"the "
					"beginning of file) that's not correct and data "
					"might "
					"be corrupted, check everything again!");

				auto offset_for_json_data = std::atoi(buffer);

				current_offset += sizeof(buffer) + 2;

				auto real_size_for_json_data = offset_for_json_data - 2;

				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, current_offset,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

				this->m_p_file_temp->seekg(current_offset, std::ios_base::beg);

				kotek::json::value json_data;

				bool is_contain =
					this->check_json_entry_has_entity_id_and_component_type_id(
						id, type_id, real_size_for_json_data, current_offset,
						json_data);

				if (is_contain)
				{
					constructed_value_on_stack_based_on_placement_new_memory =
						json_data;
					result = true;
					break;
				}
				else
				{
					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, current_offset,
					//		kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);

					this->m_p_file_temp->seekg(
						current_offset, std::ios_base::beg);

					if (current_offset + sizeof(buffer) == file_size)
					{
						break;
					}
				}
			}

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		restored_offset,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(restored_offset, std::ios_base::beg);
		}
	}

	return result;
}

void zircon_editor_command_history::unload_content()
{
	// я нахожусь между первым и последним фреймом
	if (this->m_cursor_index > 9 &&
		this->m_cursor_index < (this->m_max_index - 1) -
				((this->m_max_index - 1) %
					zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE))
	{
		this->unload_content_before();
		this->unload_content_after(false);
	}
	// я на самом первом фрейме (самое начало)
	else if (this->m_cursor_index <= 9 && this->m_max_index > 9)
	{
		this->m_exchange_file_offset_after = 0;
		this->unload_content_after(true);
	}
	// я на самом последнем фрейме (самый конец)
	else if (this->m_cursor_index > 9 &&
		this->m_cursor_index ==
			(this->m_max_index - 1) -
				((this->m_max_index - 1) %
					zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE))
	{
		this->m_exchange_file_offset_after = 0;
		this->unload_content_before();
	}
#ifdef KOTEK_DEBUG
	else
	{
		KOTEK_ASSERT(false, "unhanlded situation!");
	}
#endif
}

void zircon_editor_command_history::unload_content_before()
{
	//	this->m_p_resource_manager->Close_Saver(
	//		this->m_file_exchange_resource_handle_id);

	KOTEK_ASSERT(false, "todo: re-write please");
	//this->m_p_resource_manager->Close_FileStream(this->m_p_file_exchange);

	//	kotek::core::ktkResourceWritingRequest request;
	//	request.Set_ResourceType(kotek::core::eResourceWritingType::kText);

	const auto& path_to_exchange =
		this->get_full_path_of_file(_kExchangeFileNameWithExtension);

	//	request.Set_Path(path_to_exchange);
	//	request.Set_ID(this->m_file_exchange_resource_handle_id);

	//	this->m_p_resource_manager->Open(request);

	KOTEK_ASSERT(false, "todo: re-write please");
	/* todo: re-write please
	kotek::core::ktkResourceFileStreamRequest request;

	request.resource_type = kotek::core::eResourceRequestResourceType::kText;
	request.path_to_file = path_to_exchange;
	request.operation_type = kotek::core::eResourceRequestOperationType::kSave;

	this->m_p_file_exchange =
		this->m_p_resource_manager->Open_FileStream(request);*/

	auto file_size = this->m_before_frame_file_offset;

	//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id, 0,
	//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

	this->m_p_file_temp->seekg(0, std::ios_base::beg);

	kotek::size_t current_size{};
	kotek::size_t size_reading{};
	kotek::size_t size_writing{};

	while (current_size < file_size)
	{
		char buffer[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

		if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE > file_size)
		{
			size_reading = (file_size - current_size);
			size_writing = size_reading;
		}
		else
		{
			size_reading = zircon_DEF_STREAM_JSON_STACK_SIZE;
			size_writing = zircon_DEF_STREAM_JSON_STACK_SIZE;
		}

		//	this->m_p_resource_manager->Read(
		//		this->m_file_resource_handle_id, buffer, size_reading);

		this->m_p_file_temp->read(buffer, size_reading);

		// control character is '\n' because FILE interprets is as a complex
		// char so it is two values not one as we write in code
		kotek::size_t control_character_amount_of_repetitions{};
		bool is_contain_control_character = this->is_contain_control_character(
			buffer, control_character_amount_of_repetitions, size_reading);

		if (size_writing < zircon_DEF_STREAM_JSON_STACK_SIZE &&
			buffer[size_writing] == '\0')
		{
			if (is_contain_control_character)
				size_writing -= control_character_amount_of_repetitions;

			KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
		}
		else
		{
			if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE == file_size)
			{
				if (is_contain_control_character)
					size_writing -= control_character_amount_of_repetitions;

				KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
			}
		}

		//	this->m_p_resource_manager->Write(
		//		this->m_file_exchange_resource_handle_id, buffer, size_writing);
		this->m_p_file_exchange->write(buffer, size_writing);
		//	this->m_p_resource_manager->Write(
		//		this->m_file_exchange_resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kFlush);
		this->m_p_file_exchange->flush();

		current_size += zircon_DEF_STREAM_JSON_STACK_SIZE +
			control_character_amount_of_repetitions;

		//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
		//		current_size,
		//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
		this->m_p_file_temp->seekg(current_size, std::ios_base::beg);
	}

	//	this->m_p_resource_manager->Seekg(this->m_file_exchange_resource_handle_id,
	//		0, kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

	this->m_p_file_exchange->seekg(0, std::ios_base::end);

	//	this->m_exchange_file_offset_after = this->m_p_resource_manager->Tellg(
	//		this->m_file_exchange_resource_handle_id);
	this->m_exchange_file_offset_after = this->m_p_file_exchange->tellg();
}

void zircon_editor_command_history::unload_content_after(bool is_need_to_reopen)
{
	KOTEK_ASSERT(this->m_p_file_exchange->is_open(),
		"you must write some data that goes BEFORE this method");
	KOTEK_ASSERT(this->m_p_file_temp->is_open(),
		"something is wrong, that file must be opened before calling this "
		"method!");

	if (is_need_to_reopen)
	{
		//	this->m_p_resource_manager->Close_Saver(
		//		this->m_file_exchange_resource_handle_id);

		KOTEK_ASSERT(false, "todo: re-write please");
	//	this->m_p_resource_manager->Close_FileStream(this->m_p_file_exchange);

		//	kotek::core::ktkResourceWritingRequest request;
		//	request.Set_ResourceType(kotek::core::eResourceWritingType::kText);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kExchangeFileNameWithExtension);

		//	request.Set_Path(path_to_exchange);
		//	request.Set_ID(this->m_file_exchange_resource_handle_id);

		//	this->m_p_resource_manager->Open(request);

		KOTEK_ASSERT(false, "todo: re-write please");
		/* todo: re-write please
		kotek::core::ktkResourceFileStreamRequest request;
		request.resource_type =
			kotek::core::eResourceRequestResourceType::kText;
		request.path_to_file = path_to_exchange;
		request.operation_type =
			kotek::core::eResourceRequestOperationType::kSave;

		this->m_p_file_exchange =
			this->m_p_resource_manager->Open_FileStream(request);*/
	}

	//	if (this->m_p_resource_manager->Is_Open(
	//			this->m_file_exchange_resource_handle_id))
	if (this->m_p_file_exchange->is_open())
	{
		//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
		// 0, 		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

		this->m_p_file_temp->seekg(0, std::ios_base::end);

		// kotek::size_t file_size =
		//	this->m_p_resource_manager->Tellg(this->m_file_resource_handle_id);

		kotek::size_t file_size = this->m_p_file_temp->tellg();

		//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
		//		this->m_after_frame_file_offset,
		//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

		this->m_p_file_temp->seekg(
			this->m_after_frame_file_offset, std::ios_base::beg);

		if (file_size == kotek::size_t(-1))
			return;

		kotek::size_t current_size{this->m_after_frame_file_offset};
		kotek::size_t size_reading{};
		kotek::size_t size_writing{};

		while (current_size < file_size)
		{
			char buffer[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE > file_size)
			{
				size_reading = (file_size - current_size);
				size_writing = size_reading;
			}
			else
			{
				size_reading = zircon_DEF_STREAM_JSON_STACK_SIZE;
				size_writing = zircon_DEF_STREAM_JSON_STACK_SIZE;
			}

			//	this->m_p_resource_manager->Read(
			//		this->m_file_resource_handle_id, buffer, size_reading);

			this->m_p_file_temp->read(buffer, size_reading);

			// control character is '\n' because FILE interprets is as a complex
			// char so it is two values not one as we write in code
			kotek::size_t control_character_amount_of_repetitions{};
			bool is_contain_control_character =
				this->is_contain_control_character(buffer,
					control_character_amount_of_repetitions, size_reading);

			if (size_writing < zircon_DEF_STREAM_JSON_STACK_SIZE &&
				buffer[size_writing] == '\0')
			{
				if (is_contain_control_character)
					size_writing -= control_character_amount_of_repetitions;

				KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
			}
			else
			{
				if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE ==
					file_size)
				{
					if (is_contain_control_character)
						size_writing -= control_character_amount_of_repetitions;

					KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
				}
			}

			//	this->m_p_resource_manager->Write(
			//		this->m_file_exchange_resource_handle_id, buffer,
			// size_writing);
			this->m_p_file_exchange->write(buffer, size_writing);
			//	this->m_p_resource_manager->Write(
			//		this->m_file_exchange_resource_handle_id,
			//		kotek::core::eFileWritingControlCharacterType::kFlush);
			this->m_p_file_exchange->flush();

			current_size += zircon_DEF_STREAM_JSON_STACK_SIZE +
				control_character_amount_of_repetitions;

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		current_size,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(current_size, std::ios_base::beg);
		}
	}
}

void zircon_editor_command_history::insert_content(kotek::size_t from_offset,
	kotek::size_t to_offset, kotek::size_t cursor_offset_current)
{
	if (to_offset == size_t(-1))
		return;

	if (from_offset == to_offset)
		return;

	// this->m_p_resource_manager->Seekg(this->m_file_exchange_resource_handle_id,
	//	0, kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

	this->m_p_file_exchange->seekg(0, std::ios_base::end);

	kotek::size_t current_file_exchange_size{from_offset};
	kotek::size_t current_file_current_size{cursor_offset_current};

	// потому что мы очистили наш файл путем его "переоткрытия"
	// this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
	//	cursor_offset_current,
	//	kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

	this->m_p_file_temp->seekg(cursor_offset_current, std::ios_base::beg);

	kotek::size_t size_writing{};
	kotek::size_t size_reading{};

	while (current_file_exchange_size < to_offset)
	{
		char buffer[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

		if (current_file_exchange_size + zircon_DEF_STREAM_JSON_STACK_SIZE >
			to_offset)
		{
			size_reading = (to_offset - current_file_exchange_size);
			size_writing = size_reading;
		}
		else
		{
			size_reading = zircon_DEF_STREAM_JSON_STACK_SIZE;
			size_writing = zircon_DEF_STREAM_JSON_STACK_SIZE;
		}

		//	this->m_p_resource_manager->Seekg(
		//		this->m_file_exchange_resource_handle_id,
		//		current_file_exchange_size,
		//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

		this->m_p_file_exchange->seekg(
			current_file_exchange_size, std::ios_base::beg);

		//	this->m_p_resource_manager->Read(
		//		this->m_file_exchange_resource_handle_id, buffer, size_reading);

		this->m_p_file_exchange->read(buffer, size_reading);

		kotek::size_t amount_of_repeats{};
		bool is_contain_control_character = this->is_contain_control_character(
			buffer, amount_of_repeats, size_reading);

		if (size_writing < zircon_DEF_STREAM_JSON_STACK_SIZE &&
			buffer[size_writing] == '\0')
		{
			if (is_contain_control_character)
				size_writing -= amount_of_repeats;

			KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
		}
		else
		{
			if (current_file_exchange_size +
					zircon_DEF_STREAM_JSON_STACK_SIZE ==
				to_offset)
			{
				if (is_contain_control_character)
					size_writing -= amount_of_repeats;

				KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
			}
		}
		// if (is_contain_control_character)
		// size_writing -= amount_of_repeats;

		//	this->m_p_resource_manager->Write(
		//		this->m_file_resource_handle_id, buffer, size_writing);
		this->m_p_file_temp->write(buffer, size_writing);
		//	this->m_p_resource_manager->Write(this->m_file_resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kFlush);
		this->m_p_file_temp->flush();

		current_file_exchange_size +=
			zircon_DEF_STREAM_JSON_STACK_SIZE + amount_of_repeats;
	}
}

void zircon_editor_command_history::insert_content_exchange(
	kotek::size_t offset_cursor_exchange,
	kotek::size_t read_until_offset_in_file,
	kotek::size_t cursor_offset_of_file)
{
	if (read_until_offset_in_file == size_t(-1))
		return;

	if (offset_cursor_exchange == read_until_offset_in_file)
		return;

	kotek::size_t current_file_exchange_size{offset_cursor_exchange};
	kotek::size_t current_file_current_size{cursor_offset_of_file};

	// потому что мы очистили наш файл путем его "переоткрытия"
	// this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
	//	cursor_offset_of_file,
	//	kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

	this->m_p_file_temp->seekg(cursor_offset_of_file, std::ios_base::beg);

	kotek::size_t size_writing{};
	kotek::size_t size_reading{};

	while (current_file_exchange_size < read_until_offset_in_file)
	{
		char buffer[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

		if (current_file_exchange_size + zircon_DEF_STREAM_JSON_STACK_SIZE >
			read_until_offset_in_file)
		{
			size_reading =
				(read_until_offset_in_file - current_file_exchange_size);
			size_writing = size_reading;
		}
		else
		{
			size_reading = zircon_DEF_STREAM_JSON_STACK_SIZE;
			size_writing = zircon_DEF_STREAM_JSON_STACK_SIZE;
		}

		//		this->m_p_resource_manager->Seekg(
		//			this->m_file_exchange_resource_handle_id,
		//			current_file_exchange_size,
		//			kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

		//	this->m_p_resource_manager->Read(
		//		this->m_file_resource_handle_id, buffer, size_reading);

		this->m_p_file_temp->read(buffer, size_reading);

		kotek::size_t amount_of_repeats{};
		bool is_contain_control_character = this->is_contain_control_character(
			buffer, amount_of_repeats, size_reading);

		if (size_writing < zircon_DEF_STREAM_JSON_STACK_SIZE &&
			buffer[size_writing] == '\0')
		{
			if (is_contain_control_character)
				size_writing -= amount_of_repeats;

			KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
		}
		else
		{
			// fix in case
			if (current_file_exchange_size +
					zircon_DEF_STREAM_JSON_STACK_SIZE ==
				read_until_offset_in_file)
			{
				if (is_contain_control_character)
					size_writing -= amount_of_repeats;

				KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
			}
		}
		// if (is_contain_control_character)
		// size_writing -= amount_of_repeats;

		//	this->m_p_resource_manager->Write(
		//		this->m_file_exchange_resource_handle_id, buffer, size_writing);
		this->m_p_file_exchange->write(buffer, size_writing);
		//	this->m_p_resource_manager->Write(
		//		this->m_file_exchange_resource_handle_id,
		//		kotek::core::eFileWritingControlCharacterType::kFlush);
		this->m_p_file_exchange->flush();

		current_file_exchange_size +=
			zircon_DEF_STREAM_JSON_STACK_SIZE + amount_of_repeats;
	}
}

bool zircon_editor_command_history::is_contain_control_character(
	const char* p_buffer,
	kotek::ktk::size_t& how_much_time_control_character_repeats,
	kotek::ktk::size_t size_of_buffer, char control_character)
{
	KOTEK_ASSERT(p_buffer, "you passed an invalid buffer!");
	KOTEK_ASSERT(size_of_buffer,
		"you must have a valid length that determines the buffer!");

	bool result{};
	how_much_time_control_character_repeats = 0;

	for (kotek::size_t i = 0; i < size_of_buffer; ++i)
	{
		if (p_buffer[i] == control_character)
		{
			how_much_time_control_character_repeats++;
			result = true;
		}
#ifdef KOTEK_DEBUG
		else if (p_buffer[i] == '\t' || p_buffer[i] == '\b' ||
			p_buffer[i] == '\r' || p_buffer[i] == '\f')
		{
			KOTEK_ASSERT(false,
				"control character must be '\n' but not any other control "
				"character that contains json of commands! Can't be something "
				"is wrong!");
		}
#endif
	}

	return result;
}

kotek::static_path_t zircon_editor_command_history::get_full_path_of_file(
	const char* filename_with_extension)
{
	KOTEK_ASSERT(this->m_path_to_streaming_folder.empty() == false,
		"early calling you should intialize the path of streaming folder!");
	return kotek::static_path_t(this->m_path_to_streaming_folder) /
		filename_with_extension;
}

kotek::cfstream_t* zircon_editor_command_history::reopen_current_file(
	kotek::cfstream_t* p_file)
{
	KOTEK_ASSERT(p_file, "must be valid!");

	kotek::cfstream_t* p_result{};
//	if (this->m_p_resource_manager)
	{
		KOTEK_ASSERT(false, "todo: re-write please");
		/* todo: re-write please
		this->m_p_resource_manager->Close_FileStream(p_file);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kTempFileNameWithExtension);

		kotek::core::ktkResourceFileStreamRequest request;

		request.path_to_file = path_to_exchange;
		request.resource_type =
			kotek::core::eResourceRequestResourceType::kText;
		request.operation_type =
			kotek::core::eResourceRequestOperationType::kSave;

		p_result = this->m_p_resource_manager->Open_FileStream(request);*/
		KOTEK_ASSERT(
			p_result, "must return a valid otherwise out of resources!");
	}

	return p_result;
}

kotek::cfstream_t* zircon_editor_command_history::reopen_exchange_file(
	kotek::cfstream_t* p_file)
{
	KOTEK_ASSERT(p_file, "must be valid!");

	kotek::cfstream_t* p_result{};
//	if (this->m_p_resource_manager)
	{
		KOTEK_ASSERT(false, "todo: re-write please");
		/* todo: re-write please 
		this->m_p_resource_manager->Close_FileStream(p_file);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kExchangeFileNameWithExtension);

		kotek::core::ktkResourceFileStreamRequest request;
		request.path_to_file = path_to_exchange;
		request.resource_type =
			kotek::core::eResourceRequestResourceType::kText;
		request.operation_type =
			kotek::core::eResourceRequestOperationType::kSave;

		p_result = this->m_p_resource_manager->Open_FileStream(request);*/
		KOTEK_ASSERT(
			p_result, "must return a valid otherwise out of resources!");
	}

	return p_result;
}

void zircon_editor_command_history::clear_content_when_action_issued()
{
	if (this->m_is_action_issued)
	{
		bool is_need_to_file_clear{};
		bool is_need_to_buffer_clear{};

		kotek::size_t index{this->m_index};
		if (this->m_cursor_index <
			static_cast<kotek::ptrdiff_t>(this->m_max_index - 1))
		{
			is_need_to_file_clear = true;
			if (this->m_cursor_index >= 0)
			{
				if (index + 1 < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE)
				{
					index += 1;
					is_need_to_buffer_clear = true;
				}
			}
			else
			{
				is_need_to_buffer_clear = true;
			}
		}
		else
		{
			this->m_is_action_issued = false;
			return;
		}

		// how much we deleted in buffer
		kotek::size_t command_count_from_buffer{};

		if (is_need_to_buffer_clear)
		{
			for (kotek::size_t i = index;
				i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE; ++i)
			{
				if (this->m_commands[i])
				{
					this->m_commands[i]->~ktkISDKRedoUndo();
					this->m_commands[i] = nullptr;
					++command_count_from_buffer;
				}
			}

#ifdef ZIRCON_ENABLE_CH_TRACE
			KOTEK_TRACE("before[max_index] = {} before[cursor_index] = {}",
				this->m_max_index, this->m_cursor_index);
#endif

			this->m_max_index -= command_count_from_buffer;

#ifdef ZIRCON_ENABLE_CH_TRACE
			KOTEK_TRACE("after[max_index] = {} after[cursor_index] = {} "
						"c_from_buffer = {}",
				this->m_max_index, this->m_cursor_index,
				command_count_from_buffer);
#endif

			for (kotek::size_t i = index;
				i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE; ++i)
			{
				auto* p_ptr = this->m_storage[i];
				std::memset(p_ptr, 0, sizeof(this->m_storage[0]));
			}

			if (is_need_to_file_clear)
			{
				if (this->m_cursor_index >
					static_cast<kotek::ptrdiff_t>(this->m_max_index - 1))
				{
					is_need_to_file_clear = false;
				}
			}
		}

		if (is_need_to_file_clear)
		{
			kotek::size_t delete_from_offset =
				this->get_offset_of_current_index_in_file();
			kotek::size_t command_count_from_file =
				this->get_count_of_commands_in_file(delete_from_offset);

			kotek::ptrdiff_t expected_count =
				this->m_max_index - this->m_cursor_index;

			KOTEK_ASSERT(static_cast<kotek::ptrdiff_t>(
							 command_count_from_file) <= expected_count,
				"something is wrong!");

			KOTEK_ASSERT(command_count_from_file > 0 ||
					(command_count_from_file == 0 &&
						this->m_cursor_index >= 0) ||
					(command_count_from_file == 0 && this->m_cursor_index < 0),
				"if you make a diff between you will get a negative value and "
				"casted to size_t will cause overflow!!! So that means "
				"something is wrong");

			kotek::size_t diff{};

			if (command_count_from_file == 0 && this->m_cursor_index < 0)
				diff = this->m_max_index;
			else if (command_count_from_file == 0 && this->m_cursor_index > 0)
				diff = 0;
			else
			{
				// it is good and it means we didn't serialize our data from
				// current storage buffer
				if (this->m_max_index - command_count_from_file ==
					this->m_cursor_index + 1)
				{
#ifdef ZIRCON_ENABLE_CH_TRACE
					KOTEK_TRACE(
						"serialized commands: {}", command_count_from_file);
#endif

					diff = command_count_from_file;
				}
				else if ((static_cast<kotek::ptrdiff_t>(this->m_max_index) -
							 command_count_from_file) <= this->m_cursor_index &&
					((this->m_cursor_index + 1) -
							(static_cast<kotek::ptrdiff_t>(this->m_max_index) -
								command_count_from_file) <=
						zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE))
				{
					// the difference between cursor_index + 1 aand max_index -
					// count_from_file tells us how many commands were
					// serialized in command_count_from_buffer variable

#ifdef ZIRCON_ENABLE_CH_TRACE
					KOTEK_TRACE(
						"serialized commands in command_count_from_buffer: {}",
						((this->m_cursor_index + 1) -
							(static_cast<kotek::ptrdiff_t>(this->m_max_index) -
								command_count_from_file)));
#endif

					diff = command_count_from_file -
						((this->m_cursor_index + 1) -
							(static_cast<kotek::ptrdiff_t>(this->m_max_index) -
								command_count_from_file));

					KOTEK_ASSERT(
						this->m_max_index - diff == this->m_cursor_index + 1,
						"can't be!");
				}
				else
				{
					diff = command_count_from_file - command_count_from_buffer;

					KOTEK_ASSERT(
						(this->m_max_index - 1) - diff == this->m_cursor_index,
						"something is wrong and you should detail your "
						"heusristic. it means that some part of "
						"command_count_from_buffer has in "
						"command_count_from_file and we need to understand "
						"which buffers were serialized in file. So like "
						"command_from_buffer = 5 but command_from_file = 12 "
						"but suppose 3 were serialized from buffer and it "
						"means real amount is only 2 not 5 and we should "
						"command_from_file - 2 not 5");
				}
			}

			this->m_max_index -= (diff);

#ifdef ZIRCON_ENABLE_CH_TRACE
			KOTEK_TRACE("clear: max_index {} cursor_index {} c_from_file {} "
						"c_from_buffer {} diff {}",
				this->m_max_index, this->m_cursor_index,
				command_count_from_file, command_count_from_buffer, diff);
#endif

			if (delete_from_offset > 0)
			{
				if (this->m_max_index > 0)
				{
					this->move_content_from_file_to_exchange(
						0, delete_from_offset);
					this->move_content_from_exchange_to_file(
						0, delete_from_offset);
				}
				else
				{
					this->m_p_file_temp =
						this->reopen_current_file(this->m_p_file_temp);
					this->m_p_file_exchange =
						this->reopen_exchange_file(this->m_p_file_exchange);
				}
			}
		}

		KOTEK_ASSERT((this->m_max_index == 0 && this->m_cursor_index < 0) ||
				(this->m_max_index > this->m_cursor_index),
			"can't be! max_index is always bigger than cursor!");

		this->m_is_action_issued = false;
	}
}

kotek::size_t
zircon_editor_command_history::get_offset_of_current_index_in_file()
{
	kotek::size_t result{};
//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())
		{
			// мое начало текущего фрейма в файле
			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			char buffer[sizeof(
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

			int command_count{};
			kotek::size_t current_file_offset =
				this->m_before_frame_file_offset;

			if (this->m_cursor_index < 0)
			{
				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, 0,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

				this->m_p_file_temp->seekg(0, std::ios_base::end);

				//	auto file_size = this->m_p_resource_manager->Tellg(
				//		this->m_file_resource_handle_id);

				auto file_size = this->m_p_file_temp->tellg();

				result = file_size;
			}
			else
			{
				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, 0,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

				this->m_p_file_temp->seekg(0, std::ios_base::end);

				//	auto file_size = this->m_p_resource_manager->Tellg(
				//		this->m_file_resource_handle_id);

				auto file_size = this->m_p_file_temp->tellg();

				if (current_file_offset == file_size)
				{
					// this->m_p_resource_manager->Seekg(
					//	this->m_file_resource_handle_id,
					//	this->m_current_file_offset,
					//		kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);
					this->m_p_file_temp->seekg(
						this->m_current_file_offset, std::ios_base::beg);
					return file_size;
				}

				for (int i = 0; i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
					++i)
				{
					std::memset(buffer, 0, sizeof(buffer));
					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id,
					// current_file_offset,
					// kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);
					this->m_p_file_temp->seekg(
						current_file_offset, std::ios_base::beg);
					//	this->m_p_resource_manager->Read(
					//		this->m_file_resource_handle_id, buffer,
					//		sizeof(buffer));
					this->m_p_file_temp->read(buffer, sizeof(buffer));

					if (buffer
							[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
					{
						auto current_offset = current_file_offset;
						current_offset += sizeof(buffer);

						//	this->m_p_resource_manager->Seekg(
						//		this->m_file_resource_handle_id, 0,
						//		kotek::core::eFileSeekDirectionType::
						//			kSeekDirectionEnd);
						this->m_p_file_temp->seekg(0, std::ios_base::end);

						//	auto file_size = this->m_p_resource_manager->Tellg(
						//		this->m_file_resource_handle_id);

						auto file_size = this->m_p_file_temp->tellg();

						// we reached end of file, so we can't move further,
						// making leaving from this stack...
						if (current_offset == file_size)
						{
							current_file_offset =
								this->m_before_frame_file_offset;
							current_file_offset -= sizeof(buffer);

							break;
						}
						else
						{
							// everything is fine we can move further
							current_file_offset += sizeof(buffer);
							//	this->m_p_resource_manager->Seekg(
							//		this->m_file_resource_handle_id,
							//		current_file_offset,
							//		kotek::core::eFileSeekDirectionType::
							//			kSeekDirectionBegin);

							this->m_p_file_temp->seekg(
								current_file_offset, std::ios_base::beg);

							//	this->m_p_resource_manager->Read(
							//		this->m_file_resource_handle_id, buffer,
							//		sizeof(buffer));
							this->m_p_file_temp->read(buffer, sizeof(buffer));
						}
					}

					KOTEK_ASSERT(
						buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
							zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
						"that means we are reading for going to back (to "
						"the "
						"beginning of file) that's not correct and data "
						"might "
						"be corrupted, check everything again!");

					auto offset_for_json_data = std::atoi(buffer);

					KOTEK_ASSERT(offset_for_json_data > 0,
						"bad cast or something is broken when data was "
						"written "
						"to file!");

					current_file_offset += sizeof(buffer) + 2;

					// getting real json exact string size for reading
					auto real_size_for_json_data = offset_for_json_data - 2;

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id,
					// current_file_offset,
					// kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);
					this->m_p_file_temp->seekg(
						current_file_offset, std::ios_base::beg);

					kotek::json::stream_parser parser;
					kotek::json::static_resource storage_ptr(
						this->m_p_memory_for_stack_parser);
					parser.reset(&storage_ptr);

					if (real_size_for_json_data >
						zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						int counter{real_size_for_json_data};
						auto prev_size{current_file_offset};

						while (counter > 0)
						{
							if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
							{
								//	this->m_p_resource_manager->Read(
								//		this->m_file_resource_handle_id,
								//		stream_buffer_for_json_data,
								//			sizeof(stream_buffer_for_json_data));
								this->m_p_file_temp->read(
									stream_buffer_for_json_data,
									sizeof(stream_buffer_for_json_data));
								current_file_offset +=
									zircon_DEF_STREAM_JSON_STACK_SIZE;
								//	this->m_p_resource_manager->Seekg(
								//		this->m_file_resource_handle_id,
								//		current_file_offset,
								//		kotek::core::eFileSeekDirectionType::
								//			kSeekDirectionBegin);
								this->m_p_file_temp->seekg(
									current_file_offset, std::ios_base::beg);
							}
							else
							{
								//	this->m_p_resource_manager->Read(
								//		this->m_file_resource_handle_id,
								//		stream_buffer_for_json_data, counter);
								this->m_p_file_temp->read(
									stream_buffer_for_json_data, counter);
								current_file_offset += counter;
							}

							if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
							{
								parser.write(stream_buffer_for_json_data,
									sizeof(stream_buffer_for_json_data));
							}
							else
							{
								parser.write(
									stream_buffer_for_json_data, counter);
							}

							counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
							std::memset(stream_buffer_for_json_data, 0,
								sizeof(stream_buffer_for_json_data));
						}

						KOTEK_ASSERT((current_file_offset -
										 real_size_for_json_data) == prev_size,
							"wrong calculations! after parsing you have to "
							"get exactly the same size as you minused "
							"offset_for_json_data! curret_offset:[{}] "
							"real_size_for_json_data:[{}] dif:[{}] "
							"prev_size:[{}]",
							current_file_offset, real_size_for_json_data,
							(current_file_offset - real_size_for_json_data),
							prev_size);

						if (current_file_offset > 0)
							current_file_offset += 2;

						auto status = parser.done();

						KOTEK_ASSERT(status, "must be valid json!");
					}
					else
					{
						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id,
						//		stream_buffer_for_json_data,
						//		real_size_for_json_data);
						this->m_p_file_temp->read(stream_buffer_for_json_data,
							real_size_for_json_data);
						parser.write(stream_buffer_for_json_data);
						auto status = parser.done();
						KOTEK_ASSERT(
							status, "must be valid json in stream buffer!");
					}

					kotek::json::value json_data = parser.release();

					KOTEK_ASSERT(json_data.is_object(), "must be object!");

					++command_count;

					if (this->m_index == command_count - 1)
					{
						current_file_offset += sizeof(buffer);
						result = current_file_offset;
						break;
					}
				}
			}

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		this->m_current_file_offset,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(
				this->m_current_file_offset, std::ios_base::beg);
		}
	}

	return result;
}

kotek::size_t zircon_editor_command_history::get_count_of_commands_in_file(
	kotek::size_t start_offset)
{
	kotek::size_t result{};
//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())
		{
			// мое начало текущего фрейма в файле
			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			char buffer[sizeof(
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

			int command_count{};
			kotek::size_t current_file_offset = start_offset;

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0, kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

			this->m_p_file_temp->seekg(0, std::ios_base::end);

			//	auto file_size = this->m_p_resource_manager->Tellg(
			//		this->m_file_resource_handle_id);

			auto file_size = this->m_p_file_temp->tellg();

			if (start_offset == file_size)
			{
				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id,
				//		this->m_before_frame_file_offset,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
				this->m_p_file_temp->seekg(
					this->m_before_frame_file_offset, std::ios_base::beg);

				return result;
			}

			kotek::ptrdiff_t expected_count =
				static_cast<kotek::ptrdiff_t>(this->m_max_index) -
				this->m_cursor_index;

			KOTEK_ASSERT(expected_count >= 0, "can't be negative!");

			for (kotek::size_t i = 0; i < expected_count; ++i)
			{
				std::memset(buffer, 0, sizeof(buffer));
				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, current_file_offset,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
				this->m_p_file_temp->seekg(
					current_file_offset, std::ios_base::beg);
				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id, buffer,
				// sizeof(buffer));

				this->m_p_file_temp->read(buffer, sizeof(buffer));

				if (buffer
						[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
					zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
				{
					auto current_offset = current_file_offset;
					current_offset += sizeof(buffer);

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, 0,
					//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

					this->m_p_file_temp->seekg(0, std::ios_base::end);

					//	auto file_size = this->m_p_resource_manager->Tellg(
					//		this->m_file_resource_handle_id);

					auto file_size = this->m_p_file_temp->tellg();

					// we reached end of file, so we can't move further,
					// making leaving from this stack...
					if (current_offset == file_size)
					{
						break;
					}
					else
					{
						// everything is fine we can move further
						current_file_offset += sizeof(buffer);
						//	this->m_p_resource_manager->Seekg(
						//			this->m_file_resource_handle_id,
						//		current_file_offset,
						//		kotek::core::eFileSeekDirectionType::
						//			kSeekDirectionBegin);

						this->m_p_file_temp->seekg(
							current_file_offset, std::ios_base::beg);

						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id, buffer,
						//		sizeof(buffer));
						this->m_p_file_temp->read(buffer, sizeof(buffer));
					}
				}

				KOTEK_ASSERT(
					buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
					"that means we are reading for going to back (to "
					"the "
					"beginning of file) that's not correct and data "
					"might "
					"be corrupted, check everything again!");

				auto offset_for_json_data = std::atoi(buffer);

				KOTEK_ASSERT(offset_for_json_data > 0,
					"bad cast or something is broken when data was "
					"written "
					"to file!");

				current_file_offset += sizeof(buffer) + 2;

				// getting real json exact string size for reading
				auto real_size_for_json_data = offset_for_json_data - 2;

				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, current_file_offset,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

				this->m_p_file_temp->seekg(
					current_file_offset, std::ios_base::beg);

				kotek::json::stream_parser parser;
				kotek::json::static_resource storage_ptr(
					this->m_p_memory_for_stack_parser);
				parser.reset(&storage_ptr);

				if (real_size_for_json_data > zircon_DEF_STREAM_JSON_STACK_SIZE)
				{
					int counter{real_size_for_json_data};
					auto prev_size{current_file_offset};

					while (counter > 0)
					{
						if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
						{
							//	this->m_p_resource_manager->Read(
							//		this->m_file_resource_handle_id,
							//		stream_buffer_for_json_data,
							//		sizeof(stream_buffer_for_json_data));
							this->m_p_file_temp->read(
								stream_buffer_for_json_data,
								sizeof(stream_buffer_for_json_data));
							current_file_offset +=
								zircon_DEF_STREAM_JSON_STACK_SIZE;
							//	this->m_p_resource_manager->Seekg(
							//		this->m_file_resource_handle_id,
							//		current_file_offset,
							//		kotek::core::eFileSeekDirectionType::
							//			kSeekDirectionBegin);
							this->m_p_file_temp->seekg(
								current_file_offset, std::ios_base::beg);
						}
						else
						{
							//	this->m_p_resource_manager->Read(
							//		this->m_file_resource_handle_id,
							//		stream_buffer_for_json_data, counter);
							this->m_p_file_temp->read(
								stream_buffer_for_json_data, counter);
							current_file_offset += counter;
						}

						if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
						{
							parser.write(stream_buffer_for_json_data,
								sizeof(stream_buffer_for_json_data));
						}
						else
						{
							parser.write(stream_buffer_for_json_data, counter);
						}

						counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
						std::memset(stream_buffer_for_json_data, 0,
							sizeof(stream_buffer_for_json_data));
					}

					KOTEK_ASSERT((current_file_offset -
									 real_size_for_json_data) == prev_size,
						"wrong calculations! after parsing you have to "
						"get exactly the same size as you minused "
						"offset_for_json_data! curret_offset:[{}] "
						"real_size_for_json_data:[{}] dif:[{}] "
						"prev_size:[{}]",
						current_file_offset, real_size_for_json_data,
						(current_file_offset - real_size_for_json_data),
						prev_size);

					if (current_file_offset > 0)
						current_file_offset += 2;

					auto status = parser.done();

					KOTEK_ASSERT(status, "must be valid json!");
				}
				else
				{
					//	this->m_p_resource_manager->Read(
					//		this->m_file_resource_handle_id,
					//		stream_buffer_for_json_data,
					// real_size_for_json_data);
					this->m_p_file_temp->read(
						stream_buffer_for_json_data, real_size_for_json_data);
					parser.write(stream_buffer_for_json_data);
					auto status = parser.done();
					KOTEK_ASSERT(
						status, "must be valid json in stream buffer!");
				}

				kotek::json::value json_data = parser.release();

				KOTEK_ASSERT(json_data.is_object(), "must be object!");

				++command_count;
			}

			result = command_count;

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		this->m_before_frame_file_offset,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(
				this->m_before_frame_file_offset, std::ios_base::beg);
		}
	}

	return result;
}

void zircon_editor_command_history::move_content_from_file_to_exchange(
	kotek::size_t start_offset_in_file, kotek::size_t end_offset_in_file)
{
//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())
		{
			// clear content in exchange file
			this->m_p_file_exchange =
				this->reopen_exchange_file(this->m_p_file_exchange);

			auto file_size = end_offset_in_file;

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0,
			// kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

			this->m_p_file_temp->seekg(0, std::ios_base::beg);

			kotek::size_t current_size{};
			kotek::size_t size_reading{};
			kotek::size_t size_writing{};

			while (current_size < file_size)
			{
				char buffer[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

				if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE >
					file_size)
				{
					size_reading = (file_size - current_size);
					size_writing = size_reading;
				}
				else
				{
					size_reading = zircon_DEF_STREAM_JSON_STACK_SIZE;
					size_writing = zircon_DEF_STREAM_JSON_STACK_SIZE;
				}

				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id, buffer, size_reading);

				this->m_p_file_temp->read(buffer, size_reading);

				// control character is '\n' because FILE interprets is as a
				// complex char so it is two values not one as we write in code
				kotek::size_t control_character_amount_of_repetitions{};
				bool is_contain_control_character =
					this->is_contain_control_character(buffer,
						control_character_amount_of_repetitions, size_reading);

				if (size_writing < zircon_DEF_STREAM_JSON_STACK_SIZE &&
					buffer[size_writing] == '\0')
				{
					if (is_contain_control_character)
						size_writing -= control_character_amount_of_repetitions;

					KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
				}
				else
				{
					if (current_size + zircon_DEF_STREAM_JSON_STACK_SIZE ==
						file_size)
					{
						if (is_contain_control_character)
							size_writing -=
								control_character_amount_of_repetitions;

						KOTEK_ASSERT(
							buffer[size_writing - 1] != '\0', "can't be!");
					}
				}

				//	this->m_p_resource_manager->Write(
				//		this->m_file_exchange_resource_handle_id, buffer,
				//		size_writing);
				this->m_p_file_exchange->write(buffer, size_writing);
				// this->m_p_resource_manager->Write(
				//	this->m_file_exchange_resource_handle_id,
				//	kotek::core::eFileWritingControlCharacterType::kFlush);
				this->m_p_file_exchange->flush();

				current_size += zircon_DEF_STREAM_JSON_STACK_SIZE +
					control_character_amount_of_repetitions;

				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, current_size,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
				this->m_p_file_temp->seekg(current_size, std::ios_base::beg);
			}

			// this->m_p_resource_manager->Seekg(
			//	this->m_file_exchange_resource_handle_id, 0,
			//	kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_exchange->seekg(0, std::ios_base::beg);
		}
	}
}

void zircon_editor_command_history::move_content_from_exchange_to_file(
	kotek::size_t start_offset_in_file, kotek::size_t end_offset_in_file)
{
//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_exchange->is_open())
		{
			this->m_p_file_temp =
				this->reopen_current_file(this->m_p_file_temp);

			this->insert_content(start_offset_in_file, end_offset_in_file, 0);

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		this->m_before_frame_file_offset,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(
				this->m_before_frame_file_offset, std::ios_base::beg);
		}
	}
}

// todo: update before offset, after offset, set correct current_offset to file
// after all modifications
void zircon_editor_command_history::update_dependent_serialized_commands(
	entt::entity id_what_will_be_deleted,
	entt::entity id_that_replaces_what_will_be_deleted)
{
//	if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())
		{
			//	kotek::size_t restored_offset =
			// this->m_p_resource_manager->Tellg(
			//		this->m_file_resource_handle_id);

			kotek::size_t restored_offset = this->m_p_file_temp->tellg();

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0, kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

			this->m_p_file_temp->seekg(0, std::ios_base::end);

			//	kotek::size_t file_size = this->m_p_resource_manager->Tellg(
			//		this->m_file_resource_handle_id);

			kotek::size_t file_size = this->m_p_file_temp->tellg();

			kotek::size_t current_offset{};

			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			char buffer[sizeof(
				zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

			this->m_p_file_exchange =
				this->reopen_exchange_file(this->m_p_file_exchange);

			//	this->m_p_resource_manager->Seekg(
			//		this->m_file_exchange_resource_handle_id, 0,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

			this->m_p_file_exchange->seekg(0, std::ios_base::beg);

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		0,
			// kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

			this->m_p_file_temp->seekg(0, std::ios_base::beg);

			while (current_offset < file_size)
			{
				std::memset(buffer, 0, sizeof(buffer));

				kotek::size_t current_section_offset_begin{current_offset};
				kotek::size_t current_section_offset_end{};

				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id, buffer,
				// sizeof(buffer));

				this->m_p_file_temp->read(buffer, sizeof(buffer));

				if (buffer
						[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
					zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
				{
					auto test_current_offset = current_offset;
					test_current_offset += sizeof(buffer);

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, 0,
					//		kotek::core::eFileSeekDirectionType::kSeekDirectionEnd);

					this->m_p_file_temp->seekg(0, std::ios_base::end);

					//	auto file_size = this->m_p_resource_manager->Tellg(
					//		this->m_file_resource_handle_id);

					auto file_size = this->m_p_file_temp->tellg();

					// we reached end of file, so we can't move further,
					// making leaving from this stack...
					if (test_current_offset == file_size)
					{
						//	auto update_offset =
						// this->m_p_resource_manager->Tellg(
						//		this->m_file_exchange_resource_handle_id);

						auto update_offset = this->m_p_file_exchange->tellg();

						if (file_size == this->m_before_frame_file_offset)
						{
							auto previous = this->m_before_frame_file_offset;

							this->m_before_frame_file_offset = update_offset;

							KOTEK_TRACE(
								"[test_current_offset == file_size] updated "
								"before offset: before={} after={} ",
								previous, this->m_before_frame_file_offset);
						}

						if (file_size == this->m_after_frame_file_offset)
						{
							auto previous = this->m_after_frame_file_offset;

							this->m_after_frame_file_offset = update_offset;

							KOTEK_TRACE(
								"[test_current_offset == file_size] updated "
								"after offset: before={} after={}",
								previous, this->m_after_frame_file_offset);
						}

						if (file_size == this->m_current_file_offset)
						{
							auto previous = this->m_current_file_offset;

							this->m_current_file_offset = update_offset;

							KOTEK_TRACE(
								"[test_current_offset == file_size] updated "
								"current offset: before={} after={}",
								previous, this->m_current_file_offset);
						}

						break;
					}
					else
					{
						// everything is fine we can move further
						current_offset += sizeof(buffer);
						//	this->m_p_resource_manager->Seekg(
						//		this->m_file_resource_handle_id, current_offset,
						//		kotek::core::eFileSeekDirectionType::
						//			kSeekDirectionBegin);

						this->m_p_file_temp->seekg(
							current_offset, std::ios_base::beg);

						//		this->m_p_resource_manager->Read(
						//			this->m_file_resource_handle_id, buffer,
						//			sizeof(buffer));
						this->m_p_file_temp->read(buffer, sizeof(buffer));
					}
				}

				KOTEK_ASSERT(
					buffer[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY,
					"that means we are reading for going to back (to "
					"the "
					"beginning of file) that's not correct and data "
					"might "
					"be corrupted, check everything again!");

				auto offset_for_json_data = std::atoi(buffer);
				//	auto offset_from_exchange =
				// this->m_p_resource_manager->Tellg(
				//		this->m_file_exchange_resource_handle_id);

				auto offset_from_exchange = this->m_p_file_exchange->tellg();

				if (current_offset == this->m_before_frame_file_offset)
				{
					auto previous = this->m_before_frame_file_offset;

					this->m_before_frame_file_offset = offset_from_exchange;

					KOTEK_TRACE(
						"[in while] updated before offset: before={} after={}",
						previous, this->m_before_frame_file_offset);
				}

				if (current_offset == this->m_after_frame_file_offset)
				{
					auto previous = this->m_after_frame_file_offset;

					this->m_after_frame_file_offset = offset_from_exchange;

					KOTEK_TRACE(
						"[in while] updated after offset: before={} after={}",
						previous, this->m_after_frame_file_offset);
				}

				if (current_offset == this->m_current_file_offset)
				{
					auto previous = this->m_current_file_offset;

					this->m_current_file_offset = offset_from_exchange;

					KOTEK_TRACE(
						"[in while] updated current offset: before={} after={}",
						previous, this->m_current_file_offset);
				}

				current_offset += sizeof(buffer) + 2;

				auto real_size_for_json_data = offset_for_json_data - 2;

				//	this->m_p_resource_manager->Seekg(
				//		this->m_file_resource_handle_id, current_offset,
				//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);

				this->m_p_file_temp->seekg(current_offset, std::ios_base::beg);

				kotek::json::value json_data;

				kotek::size_t before_reading_json_entry = current_offset;

				bool is_contain = this->check_json_entry_has_entity_id(
					id_what_will_be_deleted, real_size_for_json_data,
					current_offset, json_data);

				if (is_contain)
				{
					kotek::size_t updated_size_of_entry{};

					kotek::core::eConsoleCommandIndex type =
						static_cast<kotek::core::eConsoleCommandIndex>(
							json_data.at("command")
								.to_number<kotek::enum_base_t>());

					switch (type)
					{
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_CreateEntity:
					{
						auto placement_storage =
							this->m_p_memory_for_stack_command_creation;

						zircon_command_create_entity* p_command =
							new (placement_storage)
								zircon_command_create_entity(
									this->m_p_manager_session_editor);

						p_command->SetEntityID(static_cast<kotek::uint32_t>(
							id_that_replaces_what_will_be_deleted));
						
						KOTEK_ASSERT(
							false, "todo: re-write please"
						);
						/* todo: re-write please
						updated_size_of_entry =
							p_command->Serialize(this->m_p_file_exchange);*/

						p_command->~zircon_command_create_entity();

						std::memset(this->m_p_memory_for_stack_command_creation,
							0,
							sizeof(
								this->m_p_memory_for_stack_command_creation));

						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_DeleteEntity:
					{
						auto placement_storage =
							this->m_p_memory_for_stack_command_creation;

						zircon_command_delete_entity* p_command = new (
							placement_storage)
							zircon_command_delete_entity(this->m_p_manager_session_editor,
								id_that_replaces_what_will_be_deleted);
						
						KOTEK_ASSERT(
							false, "todo: re-write please"
						);
						/* todo: re-write please
						updated_size_of_entry =
							p_command->Serialize(this->m_p_file_exchange);*/

						p_command->~zircon_command_delete_entity();

						std::memset(this->m_p_memory_for_stack_command_creation,
							0,
							sizeof(
								this->m_p_memory_for_stack_command_creation));

						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_CreateComponentForEntity:
					{
						auto placement_storage =
							this->m_p_memory_for_stack_command_creation;

						zircon_command_add_component_to_entity* p_command =
							new (placement_storage)
								zircon_command_add_component_to_entity(
									this->m_p_manager_session_editor);

						p_command->Deserialize(json_data.get_object());
						p_command->SetEntityID(static_cast<kotek::uint32_t>(
							id_that_replaces_what_will_be_deleted));
						
						KOTEK_ASSERT(
							false, "todo: re-write please"
						);
						/* todo: re-write please
						updated_size_of_entry =
							p_command->Serialize(this->m_p_file_exchange);*/

						p_command->~zircon_command_add_component_to_entity();

						std::memset(this->m_p_memory_for_stack_command_creation,
							0,
							sizeof(
								this->m_p_memory_for_stack_command_creation));

						break;
					}
					case kotek::core::eConsoleCommandIndex::
						kConsoleCommand_SDK_DeleteComponentFromEntity:
					{
						auto placement_storage =
							this->m_p_memory_for_stack_command_creation;

						zircon_command_delete_component_from_entity* p_command =
							new (placement_storage)
								zircon_command_delete_component_from_entity(
									this->m_p_manager_session_editor);

						p_command->Deserialize(json_data.get_object());
						p_command->SetEntityID(static_cast<kotek::uint32_t>(
							id_that_replaces_what_will_be_deleted));

						KOTEK_ASSERT(
							false, "todo: re-write please"
						);
						/* todo: re-write please
						updated_size_of_entry =
							p_command->Serialize(this->m_p_file_exchange);*/

						p_command
							->~zircon_command_delete_component_from_entity();

						std::memset(this->m_p_memory_for_stack_command_creation,
							0,
							sizeof(
								this->m_p_memory_for_stack_command_creation));
						break;
					}
					default:
					{
						KOTEK_ASSERT(false,
							"provide implementation in case if you forgot to "
							"add new command index");
						break;
					}
					}

					KOTEK_ASSERT(updated_size_of_entry != 0,
						"can't be something is wrong!");

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, current_offset,
					//		kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);
					this->m_p_file_temp->seekg(
						current_offset, std::ios_base::beg);
				}
				else
				{
					// we can just write to exchange

					kotek::size_t start_offset =
						before_reading_json_entry - (sizeof(buffer) + 2);

					this->insert_content_exchange(start_offset,
						current_offset + (sizeof(buffer)), start_offset);

					//	this->m_p_resource_manager->Seekg(
					//		this->m_file_resource_handle_id, current_offset,
					//		kotek::core::eFileSeekDirectionType::
					//			kSeekDirectionBegin);

					this->m_p_file_temp->seekg(
						current_offset, std::ios_base::beg);

					if (current_offset + sizeof(buffer) == file_size)
					{
						//	auto update_offset =
						// this->m_p_resource_manager->Tellg(
						//		this->m_file_exchange_resource_handle_id);

						auto update_offset = this->m_p_file_exchange->tellg();

						if (file_size == this->m_before_frame_file_offset)
						{
							auto previous = this->m_before_frame_file_offset;

							this->m_before_frame_file_offset = update_offset;

							KOTEK_TRACE("[end of while] updated before offset: "
										"before={} after={} ",
								previous, this->m_before_frame_file_offset);
						}

						if (file_size == this->m_after_frame_file_offset)
						{
							auto previous = this->m_after_frame_file_offset;

							this->m_after_frame_file_offset = update_offset;

							KOTEK_TRACE("[end of while] updated after offset: "
										"before={} after={}",
								previous, this->m_after_frame_file_offset);
						}

						if (file_size == this->m_current_file_offset)
						{
							auto previous = this->m_current_file_offset;

							this->m_current_file_offset = update_offset;

							KOTEK_TRACE("[end of while] updated current "
										"offset: before={} after={}",
								previous, this->m_current_file_offset);
						}

						break;
					}
				}
			}

			this->move_content_from_exchange_to_file(0,
				//	this->m_p_resource_manager->Tellg(
			    //		this->m_file_exchange_resource_handle_id));
				this->m_p_file_exchange->tellg());

			//	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			//		this->m_current_file_offset,
			//		kotek::core::eFileSeekDirectionType::kSeekDirectionBegin);
			this->m_p_file_temp->seekg(
				this->m_current_file_offset, std::ios_base::beg);
		}
	}
}

bool zircon_editor_command_history::check_json_entry_has_entity_id(
	entt::entity id_what_will_be_deleted, int real_size_for_json_data,
	kotek::size_t& current_offset, kotek::json::value& json)
{
	kotek::size_t copy_offset = current_offset;

	bool result{};

	//if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp)
		{
			kotek::json::stream_parser parser;
			kotek::json::static_resource storage_ptr(
				this->m_p_memory_for_stack_parser);
			parser.reset(&storage_ptr);

			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			if (real_size_for_json_data > zircon_DEF_STREAM_JSON_STACK_SIZE)
			{
				int counter{real_size_for_json_data};
				auto prev_size{copy_offset};

				while (counter > 0)
				{
					if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id,
						//		stream_buffer_for_json_data,
						//		sizeof(stream_buffer_for_json_data));

						this->m_p_file_temp->read(stream_buffer_for_json_data,
							sizeof(stream_buffer_for_json_data));
						copy_offset += zircon_DEF_STREAM_JSON_STACK_SIZE;
						// this->m_p_resource_manager->Seekg(
						//	this->m_file_resource_handle_id, copy_offset,
						//	kotek::core::eFileSeekDirectionType::
						//		kSeekDirectionBegin);
						this->m_p_file_temp->seekg(
							copy_offset, std::ios_base::beg);
					}
					else
					{
						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id,
						//		stream_buffer_for_json_data, counter);
						this->m_p_file_temp->read(
							stream_buffer_for_json_data, counter);
						copy_offset += counter;
					}

					if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						parser.write(stream_buffer_for_json_data,
							sizeof(stream_buffer_for_json_data));
					}
					else
					{
						parser.write(stream_buffer_for_json_data, counter);
					}

					counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
					std::memset(stream_buffer_for_json_data, 0,
						sizeof(stream_buffer_for_json_data));
				}

				KOTEK_ASSERT(
					(copy_offset - real_size_for_json_data) == prev_size,
					"wrong calculations! after parsing you have to "
					"get exactly the same size as you minused "
					"offset_for_json_data! curret_offset:[{}] "
					"real_size_for_json_data:[{}] dif:[{}] "
					"prev_size:[{}]",
					copy_offset, real_size_for_json_data,
					(copy_offset - real_size_for_json_data), prev_size);

				if (copy_offset > 0)
					copy_offset += 2;

				auto status = parser.done();

				KOTEK_ASSERT(status, "must be valid json!");
			}
			else
			{
				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id,
				//		stream_buffer_for_json_data, real_size_for_json_data);
				this->m_p_file_temp->read(
					stream_buffer_for_json_data, real_size_for_json_data);
				parser.write(stream_buffer_for_json_data);
				auto status = parser.done();
				KOTEK_ASSERT(status, "must be valid json in stream buffer!");

				copy_offset += real_size_for_json_data;

				if (copy_offset > 0)
					copy_offset += 2;
			}

			json = parser.release();

			KOTEK_ASSERT(json.is_object(), "must be object!");

			auto& map = json.as_object();

			if (map.find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME) !=
				map.end())
			{
				kotek::uint32_t entity_id =
					(map.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME)
							.to_number<kotek::uint32_t>());

				result = static_cast<entt::entity>(entity_id) ==
					id_what_will_be_deleted;
			}
		}
	}

	current_offset = copy_offset;

	return result;
}

bool zircon_editor_command_history::
	check_json_entry_has_entity_id_and_component_type_id(entt::entity id,
		zircon_component_type_t type_id, int real_size_for_json_data,
		kotek::size_t& current_offset, kotek::ktk::json::value& json)
{
	kotek::size_t copy_offset = current_offset;

	bool result{};

	bool found_entity_id{};
	bool found_component_type_id{};

	//if (this->m_p_resource_manager)
	{
		if (this->m_p_file_temp->is_open())
		{
			kotek::json::stream_parser parser;
			kotek::json::static_resource storage_ptr(
				this->m_p_memory_for_stack_parser);
			parser.reset(&storage_ptr);

			char stream_buffer_for_json_data
				[zircon_DEF_STREAM_JSON_STACK_SIZE]{};

			if (real_size_for_json_data > zircon_DEF_STREAM_JSON_STACK_SIZE)
			{
				int counter{real_size_for_json_data};
				auto prev_size{copy_offset};

				while (counter > 0)
				{
					if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id,
						//		stream_buffer_for_json_data,
						//		sizeof(stream_buffer_for_json_data));
						this->m_p_file_temp->read(stream_buffer_for_json_data,
							sizeof(stream_buffer_for_json_data));
						copy_offset += zircon_DEF_STREAM_JSON_STACK_SIZE;
						//	this->m_p_resource_manager->Seekg(
						//		this->m_file_resource_handle_id, copy_offset,
						//		kotek::core::eFileSeekDirectionType::
						//			kSeekDirectionBegin);
						this->m_p_file_temp->seekg(
							copy_offset, std::ios_base::beg);
					}
					else
					{
						//	this->m_p_resource_manager->Read(
						//		this->m_file_resource_handle_id,
						//		stream_buffer_for_json_data, counter);
						this->m_p_file_temp->read(
							stream_buffer_for_json_data, counter);
						copy_offset += counter;
					}

					if (counter > zircon_DEF_STREAM_JSON_STACK_SIZE)
					{
						parser.write(stream_buffer_for_json_data,
							sizeof(stream_buffer_for_json_data));
					}
					else
					{
						parser.write(stream_buffer_for_json_data, counter);
					}

					counter -= zircon_DEF_STREAM_JSON_STACK_SIZE;
					std::memset(stream_buffer_for_json_data, 0,
						sizeof(stream_buffer_for_json_data));
				}

				KOTEK_ASSERT(
					(copy_offset - real_size_for_json_data) == prev_size,
					"wrong calculations! after parsing you have to "
					"get exactly the same size as you minused "
					"offset_for_json_data! curret_offset:[{}] "
					"real_size_for_json_data:[{}] dif:[{}] "
					"prev_size:[{}]",
					copy_offset, real_size_for_json_data,
					(copy_offset - real_size_for_json_data), prev_size);

				if (copy_offset > 0)
					copy_offset += 2;

				auto status = parser.done();

				KOTEK_ASSERT(status, "must be valid json!");
			}
			else
			{
				//	this->m_p_resource_manager->Read(
				//		this->m_file_resource_handle_id,
				//		stream_buffer_for_json_data, real_size_for_json_data);
				this->m_p_file_temp->read(
					stream_buffer_for_json_data, real_size_for_json_data);
				parser.write(stream_buffer_for_json_data);
				auto status = parser.done();
				KOTEK_ASSERT(status, "must be valid json in stream buffer!");

				copy_offset += real_size_for_json_data;

				if (copy_offset > 0)
					copy_offset += 2;
			}

			json = parser.release();

			KOTEK_ASSERT(json.is_object(), "must be object!");

			auto& map = json.as_object();

			if (map.find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME) !=
				map.end())
			{
				kotek::uint32_t entity_id =
					(map.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME)
							.to_number<kotek::uint32_t>());

				found_entity_id = static_cast<entt::entity>(entity_id) == id;
			}

			if (map.find(
					ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME) !=
				map.end())
			{
				zircon_component_type_t restored_type_id = static_cast<
					zircon_component_type_t>(
					map.at(ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME)
						.to_number<kotek::uint32_t>());

				found_component_type_id = restored_type_id == type_id;
			}

			result = found_component_type_id && found_entity_id;
		}
	}

	current_offset = copy_offset;

	return result;
}
