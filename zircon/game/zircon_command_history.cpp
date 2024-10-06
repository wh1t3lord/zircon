#include "zircon_command_history.h"
#include "zircon_scene_manager.h"

#include "zircon_command_create_entity.h"

constexpr const char* _kExchangeFileNameWithExtension = "exchange.json";
constexpr const char* _kTempFileNameWithExtension = "temp.json";
constexpr const char* _kTempFileName = "temp";
constexpr const char* _kExchangeFileName = "exchange";

zircon_command_history::zircon_command_history(void) :
	m_is_changed{}, m_is_first_serialize_happened{},
	m_file_resource_handle_id{}, m_file_exchange_resource_handle_id{},
	m_p_filesystem{}, m_p_scene_manager{}, m_p_resource_manager{}, m_index{},
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

	static_assert(
		zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY != ' ' &&
		"you can't use whitespace as delimiter because with whitespace we "
		"define right side (e.g. next entry) for moving cursor otherwise it is "
		"impossible to determine which direction we want to use in file for "
		"moving cursor that means we can't define operations like undo and "
		"redo, undo is moving to the beggining of file redo means moving to "
		"the end of file");
}
zircon_command_history::~zircon_command_history(void) {}

void zircon_command_history::initialize(
	Kotek::Core::ktkIFileSystem* p_filesystem,
	zircon_scene_manager* p_scene_manager,
	Kotek::Core::ktkIResourceManager* p_resource_manager)
{
	KOTEK_ASSERT(p_filesystem, "you must pass a valid pointer!");
	KOTEK_ASSERT(p_scene_manager, "you must pass a valid pointer!");
	KOTEK_ASSERT(p_resource_manager, "you must pass a valid pointer!");

	this->m_p_filesystem = p_filesystem;
	this->m_p_scene_manager = p_scene_manager;
	this->m_p_resource_manager = p_resource_manager;

	ktk_filesystem_path path_to_file = this->m_p_filesystem->GetFolderByEnum(
		Kotek::Core::eFolderIndex::kFolderIndex_UserData_SDK_Scenes);
	path_to_file /= "current";

	this->m_path_to_streaming_folder.append(
		reinterpret_cast<char*>(path_to_file.u8string().data()),
		path_to_file.u8string().size());

	bool is_valid_path = this->m_p_filesystem->IsValidPath(
		this->m_path_to_streaming_folder.c_str());

	if (is_valid_path == false)
	{
		this->m_p_filesystem->Create_Directory(
			this->m_path_to_streaming_folder.c_str(),
			Kotek::Core::eFolderVisibilityType::kVisible);
	}

	if (this->m_p_resource_manager)
	{
		this->m_file_resource_handle_id =
			this->m_p_resource_manager->GenerateFileID();

		Kotek::Core::ktkResourceWritingRequest request;
		request.Set_Path(path_to_file / _kTempFileNameWithExtension);
		request.Set_ID(this->m_file_resource_handle_id);
		request.Set_ResourceType(Kotek::Core::eResourceWritingType::kText);

		this->m_p_resource_manager->Open(request);

		this->m_file_exchange_resource_handle_id =
			this->m_p_resource_manager->GenerateFileID();

		request.Set_Path(path_to_file / _kExchangeFileNameWithExtension);
		request.Set_ID(this->m_file_exchange_resource_handle_id);

		this->m_p_resource_manager->Open(request);
	}
}

void zircon_command_history::shutdown(void)
{
	this->m_p_resource_manager->Close(this->m_file_resource_handle_id);
	this->m_p_resource_manager->Close(this->m_file_exchange_resource_handle_id);
	for (const auto& path_file : Kotek::ktk::filesystem::directory_iterator(
			 this->m_path_to_streaming_folder.c_str()))
	{
		Kotek::ktk::filesystem::remove(path_file);
	}
}

void zircon_command_history::ExecuteCommand(
	Kotek::Core::ktkISDKRedoUndo* p_command)
{
	KOTEK_ASSERT(p_command, "you can't send an invalid command here");

	//	this->m_index = this->m_cursor_index %
	//	static_cast<size_t>(zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE);

	this->m_commands[this->m_index] = p_command;
	p_command->Execute();

	set_changed(true);
}

void zircon_command_history::Undo()
{
	if (this->m_cursor_index > -1)
	{
		KOTEK_ASSERT(this->m_commands[this->m_index], "something is wrong!");
		this->m_commands[this->m_index]->Undo();

		if (this->m_index > 0)
			--this->m_index;

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

					this->reopen_current_file(this->m_file_resource_handle_id);

					this->insert_content(
						0, this->m_exchange_file_offset_after, 0);

					this->m_p_resource_manager->Seekg(
						this->m_file_resource_handle_id,
						this->m_before_frame_file_offset,
						Kotek::Core::eFileSeekDirectionType::
							kSeekDirectionBegin);

					auto file_size = this->m_p_resource_manager->Tellg(
						this->m_file_resource_handle_id);

					char buffer[sizeof(
						zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS)]{};

					this->m_index =
						zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE - 1;

					for (auto* p_command : this->m_commands)
					{
						if (p_command)
						{
							p_command->Serialize(
								this->m_file_resource_handle_id,
								this->m_p_resource_manager);
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

					this->m_p_resource_manager->Seekg(
						this->m_file_resource_handle_id, 0,
						Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

					auto file_size_after_serialize =
						this->m_p_resource_manager->Tellg(
							this->m_file_resource_handle_id);

					this->m_p_resource_manager->Seekg(
						this->m_file_exchange_resource_handle_id, 0,
						Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

					auto file_size_of_exchange =
						this->m_p_resource_manager->Tellg(
							this->m_file_exchange_resource_handle_id);

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
						this->m_p_resource_manager->Seekg(
							this->m_file_resource_handle_id,
							this->m_current_file_offset,
							Kotek::Core::eFileSeekDirectionType::
								kSeekDirectionBegin);

						this->m_p_resource_manager->Read(
							this->m_file_resource_handle_id, buffer,
							sizeof(buffer));

						if (buffer
								[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] !=
							zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
						{
							// making a suggestion that we need to read next
							// because we're on right side

							this->m_current_file_offset -= size_for_number;
							this->m_p_resource_manager->Seekg(
								this->m_file_resource_handle_id,
								this->m_current_file_offset,
								Kotek::Core::eFileSeekDirectionType::
									kSeekDirectionBegin);

							this->m_p_resource_manager->Read(
								this->m_file_resource_handle_id, buffer,
								sizeof(buffer));

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

						this->m_p_resource_manager->Seekg(
							this->m_file_resource_handle_id,
							this->m_current_file_offset,
							Kotek::Core::eFileSeekDirectionType::
								kSeekDirectionBegin);

						Kotek::ktk::json::stream_parser parser;
						Kotek::ktk::json::static_resource storage_ptr(
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
									this->m_p_resource_manager->Read(
										this->m_file_resource_handle_id,
										stream_buffer_for_json_data,
										sizeof(stream_buffer_for_json_data));
									this->m_current_file_offset +=
										zircon_DEF_STREAM_JSON_STACK_SIZE;
									this->m_p_resource_manager->Seekg(
										this->m_file_resource_handle_id,
										this->m_current_file_offset,
										Kotek::Core::eFileSeekDirectionType::
											kSeekDirectionBegin);
								}
								else
								{
									this->m_p_resource_manager->Read(
										this->m_file_resource_handle_id,
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
							this->m_p_resource_manager->Read(
								this->m_file_resource_handle_id,
								stream_buffer_for_json_data,
								real_size_for_json_data);
							parser.write(stream_buffer_for_json_data);
							auto status = parser.done();
							KOTEK_ASSERT(
								status, "must be valid json in stream buffer!");
						}

						Kotek::ktk::json::value json_data = parser.release();

						KOTEK_ASSERT(json_data.is_object(), "must be object!");

						auto& json = json_data.as_object();

						KOTEK_ASSERT(json.find("command") != json.end(),
							"must exist a such key, because we can't "
							"understand which command we need to restore!");

						Kotek::Core::eConsoleCommandIndex type =
							static_cast<Kotek::Core::eConsoleCommandIndex>(
								json.at("command").as_int64());

						switch (type)
						{
						case Kotek::Core::eConsoleCommandIndex::
							kConsoleCommand_SDK_CreateEntity:
						{
							auto placement_storage =
								this->m_storage[copy_index];
							zircon_command_create_entity* p_command = new (
								placement_storage)
								zircon_command_create_entity(this,
									this->m_p_scene_manager->GetCurrentScene());
							p_command->Deserialize(json);

							this->m_commands[copy_index] = p_command;

							--copy_index;
							break;
						}
						default:
						{
							KOTEK_ASSERT(
								false, "can't be!!! data corruption?!");
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

void zircon_command_history::Redo()
{
	/*
	if (this->m_storage.empty() == false &&
	    this->m_index < this->m_storage.size())
	{
	    this->m_storage[this->m_index]->Execute();
	    ++this->m_index;
	}
	*/

	if (this->m_cursor_index < this->m_max_index - 1 ||
		(this->m_cursor_index == -1 && this->m_max_index > 0))
	{
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

				this->reopen_current_file(this->m_file_resource_handle_id);

				this->insert_content(0, this->m_exchange_file_offset_after, 0);

				this->m_p_resource_manager->Seekg(
					this->m_file_resource_handle_id,
					this->m_before_frame_file_offset,
					Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);

				auto file_size = this->m_p_resource_manager->Tellg(
					this->m_file_resource_handle_id);

				for (auto* p_command : this->m_commands)
				{
					if (p_command)
					{
						p_command->Serialize(this->m_file_resource_handle_id,
							this->m_p_resource_manager);
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

				this->m_p_resource_manager->Seekg(
					this->m_file_resource_handle_id, 0,
					Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

				auto file_size_after_serialize =
					this->m_p_resource_manager->Tellg(
						this->m_file_resource_handle_id);

				this->m_p_resource_manager->Seekg(
					this->m_file_exchange_resource_handle_id, 0,
					Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

				auto file_size_of_exchange = this->m_p_resource_manager->Tellg(
					this->m_file_exchange_resource_handle_id);

				this->insert_content(this->m_exchange_file_offset_after,
					file_size_of_exchange, file_size_after_serialize);

				this->m_before_frame_file_offset = file_size_after_serialize;

				this->m_current_file_offset = this->m_before_frame_file_offset;
				int command_count{};
				for (int i = 0; i < zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
					 ++i)
				{
					std::memset(buffer, 0, sizeof(buffer));
					this->m_p_resource_manager->Seekg(
						this->m_file_resource_handle_id,
						this->m_current_file_offset,
						Kotek::Core::eFileSeekDirectionType::
							kSeekDirectionBegin);
					this->m_p_resource_manager->Read(
						this->m_file_resource_handle_id, buffer,
						sizeof(buffer));

					if (buffer
							[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS] ==
						zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY)
					{
						auto current_offset = this->m_current_file_offset;
						current_offset += sizeof(buffer);

						this->m_p_resource_manager->Seekg(
							this->m_file_resource_handle_id, 0,
							Kotek::Core::eFileSeekDirectionType::
								kSeekDirectionEnd);

						auto file_size = this->m_p_resource_manager->Tellg(
							this->m_file_resource_handle_id);

						// we reached end of file, so we can't move further,
						// making leaving from this stack...
						if (current_offset == file_size)
						{
							this->m_current_file_offset =
								this->m_before_frame_file_offset;
							this->m_p_resource_manager->Seekg(
								this->m_file_resource_handle_id,
								this->m_current_file_offset,
								Kotek::Core::eFileSeekDirectionType::
									kSeekDirectionBegin);
							this->m_current_file_offset -= sizeof(buffer);

							break;
						}
						else
						{
							// everything is fine we can move further
							this->m_current_file_offset += sizeof(buffer);
							this->m_p_resource_manager->Seekg(
								this->m_file_resource_handle_id,
								this->m_current_file_offset,
								Kotek::Core::eFileSeekDirectionType::
									kSeekDirectionBegin);

							this->m_p_resource_manager->Read(
								this->m_file_resource_handle_id, buffer,
								sizeof(buffer));
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

					this->m_p_resource_manager->Seekg(
						this->m_file_resource_handle_id,
						this->m_current_file_offset,
						Kotek::Core::eFileSeekDirectionType::
							kSeekDirectionBegin);

					Kotek::ktk::json::stream_parser parser;
					Kotek::ktk::json::static_resource storage_ptr(
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
								this->m_p_resource_manager->Read(
									this->m_file_resource_handle_id,
									stream_buffer_for_json_data,
									sizeof(stream_buffer_for_json_data));
								this->m_current_file_offset +=
									zircon_DEF_STREAM_JSON_STACK_SIZE;
								this->m_p_resource_manager->Seekg(
									this->m_file_resource_handle_id,
									this->m_current_file_offset,
									Kotek::Core::eFileSeekDirectionType::
										kSeekDirectionBegin);
							}
							else
							{
								this->m_p_resource_manager->Read(
									this->m_file_resource_handle_id,
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
						this->m_p_resource_manager->Read(
							this->m_file_resource_handle_id,
							stream_buffer_for_json_data,
							real_size_for_json_data);
						parser.write(stream_buffer_for_json_data);
						auto status = parser.done();
						KOTEK_ASSERT(
							status, "must be valid json in stream buffer!");
					}

					Kotek::ktk::json::value json_data = parser.release();

					KOTEK_ASSERT(json_data.is_object(), "must be object!");

					auto& json = json_data.as_object();

					KOTEK_ASSERT(json.find("command") != json.end(),
						"must exist a such key, because we can't "
						"understand which command we need to restore!");

					Kotek::Core::eConsoleCommandIndex type =
						static_cast<Kotek::Core::eConsoleCommandIndex>(
							json.at("command").as_int64());

					switch (type)
					{
					case Kotek::Core::eConsoleCommandIndex::
						kConsoleCommand_SDK_CreateEntity:
					{
						auto placement_storage = this->m_storage[i];
						zircon_command_create_entity* p_command =
							new (placement_storage)
								zircon_command_create_entity(this,
									this->m_p_scene_manager->GetCurrentScene());
						p_command->Deserialize(json);

						this->m_commands[i] = p_command;
						break;
					}
					default:
					{
						KOTEK_ASSERT(false, "can't be!!! data corruption?!");
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
		if (this->m_max_index > 1)
		{
			++this->m_cursor_index;
			this->m_index = this->m_cursor_index %
				zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;
		}

		KOTEK_ASSERT(this->m_commands[this->m_index], "something is wrong");
		this->m_commands[this->m_index]->Execute();

		set_changed(true);
	}
}

void zircon_command_history::set_changed(bool status) noexcept
{
	this->m_is_changed = status;
}

bool zircon_command_history::is_changed() const noexcept
{
	return this->m_is_changed;
}

const Kotek::ktk::array<Kotek::Core::ktkISDKRedoUndo*,
	zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
zircon_command_history::GetCommands(void) const noexcept
{
	return this->m_commands;
}

void zircon_command_history::update_dependent_commands(
	Kotek::ktk::entity_t id_what_will_be_deleted,
	Kotek::ktk::entity_t id_that_replaces_what_will_be_deleted) noexcept
{
	for (auto* p_command : this->m_commands)
	{
		if (p_command)
		{
			if (p_command->GetEntityID() == id_what_will_be_deleted)
			{
				p_command->SetEntityID(id_that_replaces_what_will_be_deleted);
			}
		}
	}
}

unsigned char* zircon_command_history::allocate_memory_for_command(
	Kotek::ktk::size_t size_of_class) noexcept
{
	KOTEK_ASSERT(size_of_class != 0 && size_of_class != Kotek::ktk::size_t(-1),
		"you can't pass a invalid size!");
	KOTEK_ASSERT(size_of_class <= zircon_DEF_MAXIMUM_COMMAND_SIZE,
		"you passed size larger than standard we can't allocate!");
	KOTEK_ASSERT(this->m_p_scene_manager->GetCurrentScene(),
		"must be a valid scene! early calling somehow!");

	unsigned char* pResultPlacementNewBuffer{};

	constexpr size_t _kLimitSize =
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE - 1;

	++this->m_cursor_index;
	++this->m_max_index;
	this->m_index = this->m_cursor_index %
		static_cast<size_t>(zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE);
	// update new data
	if (this->m_cursor_index > 0 &&
		this->m_cursor_index % zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE == 0)
	{
		for (auto* p_command : this->m_commands)
		{
			KOTEK_ASSERT(p_command, "must be valid!");
			p_command->Serialize(
				this->m_file_resource_handle_id, this->m_p_resource_manager);
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

		this->m_current_file_offset =
			this->m_p_resource_manager->Tellg(this->m_file_resource_handle_id);

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

void zircon_command_history::unload_content()
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

void zircon_command_history::unload_content_before()
{
	this->m_p_resource_manager->Close(this->m_file_exchange_resource_handle_id);

	Kotek::Core::ktkResourceWritingRequest request;
	request.Set_ResourceType(Kotek::Core::eResourceWritingType::kText);

	const auto& path_to_exchange =
		this->get_full_path_of_file(_kExchangeFileNameWithExtension);

	request.Set_Path(path_to_exchange);
	request.Set_ID(this->m_file_exchange_resource_handle_id);

	this->m_p_resource_manager->Open(request);

	auto file_size = this->m_before_frame_file_offset;

	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id, 0,
		Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);

	Kotek::ktk::size_t current_size{};
	Kotek::ktk::size_t size_reading{};
	Kotek::ktk::size_t size_writing{};

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

		this->m_p_resource_manager->Read(
			this->m_file_resource_handle_id, buffer, size_reading);

		// control character is '\n' because FILE interprets is as a complex
		// char so it is two values not one as we write in code
		Kotek::ktk::size_t control_character_amount_of_repetitions{};
		bool is_contain_control_character = this->is_contain_control_character(
			buffer, control_character_amount_of_repetitions, size_reading);

		if (buffer[size_writing] == '\0')
		{
			if (is_contain_control_character)
				size_writing -= control_character_amount_of_repetitions;

			KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
		}

		this->m_p_resource_manager->Write(
			this->m_file_exchange_resource_handle_id, buffer, size_writing);
		this->m_p_resource_manager->Write(
			this->m_file_exchange_resource_handle_id,
			Kotek::Core::eFileWritingControlCharacterType::kFlush);

		current_size += zircon_DEF_STREAM_JSON_STACK_SIZE +
			control_character_amount_of_repetitions;

		this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			current_size,
			Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);
	}

	this->m_p_resource_manager->Seekg(this->m_file_exchange_resource_handle_id,
		0, Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

	this->m_exchange_file_offset_after = this->m_p_resource_manager->Tellg(
		this->m_file_exchange_resource_handle_id);
}

void zircon_command_history::unload_content_after(bool is_need_to_reopen)
{
	KOTEK_ASSERT(this->m_p_resource_manager->Is_Open(
					 this->m_file_exchange_resource_handle_id),
		"you must write some data that goes BEFORE this method");
	KOTEK_ASSERT(
		this->m_p_resource_manager->Is_Open(this->m_file_resource_handle_id),
		"something is wrong, that file must be opened before calling this "
		"method!");

	if (is_need_to_reopen)
	{
		this->m_p_resource_manager->Close(
			this->m_file_exchange_resource_handle_id);

		Kotek::Core::ktkResourceWritingRequest request;
		request.Set_ResourceType(Kotek::Core::eResourceWritingType::kText);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kExchangeFileNameWithExtension);

		request.Set_Path(path_to_exchange);
		request.Set_ID(this->m_file_exchange_resource_handle_id);

		this->m_p_resource_manager->Open(request);
	}

	if (this->m_p_resource_manager->Is_Open(
			this->m_file_exchange_resource_handle_id))
	{
		this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id, 0,
			Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

		Kotek::ktk::size_t file_size =
			this->m_p_resource_manager->Tellg(this->m_file_resource_handle_id);

		this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
			this->m_after_frame_file_offset,
			Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);

		Kotek::ktk::size_t current_size{this->m_after_frame_file_offset};
		Kotek::ktk::size_t size_reading{};
		Kotek::ktk::size_t size_writing{};

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

			this->m_p_resource_manager->Read(
				this->m_file_resource_handle_id, buffer, size_reading);

			// control character is '\n' because FILE interprets is as a complex
			// char so it is two values not one as we write in code
			Kotek::ktk::size_t control_character_amount_of_repetitions{};
			bool is_contain_control_character =
				this->is_contain_control_character(buffer,
					control_character_amount_of_repetitions, size_reading);

			if (buffer[size_writing] == '\0')
			{
				if (is_contain_control_character)
					size_writing -= control_character_amount_of_repetitions;

				KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
			}

			this->m_p_resource_manager->Write(
				this->m_file_exchange_resource_handle_id, buffer, size_writing);
			this->m_p_resource_manager->Write(
				this->m_file_exchange_resource_handle_id,
				Kotek::Core::eFileWritingControlCharacterType::kFlush);

			current_size += zircon_DEF_STREAM_JSON_STACK_SIZE +
				control_character_amount_of_repetitions;

			this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
				current_size,
				Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);
		}
	}
}

void zircon_command_history::insert_content(Kotek::ktk::size_t from_offset,
	Kotek::ktk::size_t to_offset, Kotek::ktk::size_t cursor_offset_current)
{
	if (to_offset == size_t(-1))
		return;

	if (from_offset == to_offset)
		return;

	this->m_p_resource_manager->Seekg(this->m_file_exchange_resource_handle_id,
		0, Kotek::Core::eFileSeekDirectionType::kSeekDirectionEnd);

	Kotek::ktk::size_t current_file_exchange_size{from_offset};
	Kotek::ktk::size_t current_file_current_size{cursor_offset_current};

	// потому что мы очистили наш файл путем его "переоткрытия"
	this->m_p_resource_manager->Seekg(this->m_file_resource_handle_id,
		cursor_offset_current,
		Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);

	Kotek::ktk::size_t size_writing{};
	Kotek::ktk::size_t size_reading{};

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

		this->m_p_resource_manager->Seekg(
			this->m_file_exchange_resource_handle_id,
			current_file_exchange_size,
			Kotek::Core::eFileSeekDirectionType::kSeekDirectionBegin);

		this->m_p_resource_manager->Read(
			this->m_file_exchange_resource_handle_id, buffer, size_reading);

		Kotek::ktk::size_t amount_of_repeats{};
		bool is_contain_control_character = this->is_contain_control_character(
			buffer, amount_of_repeats, size_reading);

		if (buffer[size_writing] == '\0')
		{
			if (is_contain_control_character)
				size_writing -= amount_of_repeats;

			KOTEK_ASSERT(buffer[size_writing - 1] != '\0', "can't be!");
		}
		// if (is_contain_control_character)
		// size_writing -= amount_of_repeats;

		this->m_p_resource_manager->Write(
			this->m_file_resource_handle_id, buffer, size_writing);
		this->m_p_resource_manager->Write(this->m_file_resource_handle_id,
			Kotek::Core::eFileWritingControlCharacterType::kFlush);

		current_file_exchange_size +=
			zircon_DEF_STREAM_JSON_STACK_SIZE + amount_of_repeats;
	}
}

bool zircon_command_history::is_contain_control_character(const char* p_buffer,
	Kotek::ktk::size_t& how_much_time_control_character_repeats,
	Kotek::ktk::size_t size_of_buffer, char control_character)
{
	KOTEK_ASSERT(p_buffer, "you passed an invalid buffer!");
	KOTEK_ASSERT(size_of_buffer,
		"you must have a valid length that determines the buffer!");

	bool result{};
	how_much_time_control_character_repeats = 0;

	for (Kotek::ktk::size_t i = 0; i < size_of_buffer; ++i)
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

ktk_filesystem_path zircon_command_history::get_full_path_of_file(
	const char* filename_with_extension)
{
	KOTEK_ASSERT(this->m_path_to_streaming_folder.empty() == false,
		"early calling you should intialize the path of streaming folder!");

	return ktk_filesystem_path(this->m_path_to_streaming_folder) /
		filename_with_extension;
}

void zircon_command_history::reopen_current_file(Kotek::ktk::uint32_t file_id)
{
	KOTEK_ASSERT(
		this->m_p_resource_manager, "resource manager must be initialized");

	if (this->m_p_resource_manager)
	{
		this->m_p_resource_manager->Close(file_id);

		Kotek::Core::ktkResourceWritingRequest request;
		request.Set_ResourceType(Kotek::Core::eResourceWritingType::kText);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kTempFileNameWithExtension);

		request.Set_Path(path_to_exchange);
		request.Set_ID(file_id);

		this->m_p_resource_manager->Open(request);
	}
}

void zircon_command_history::reopen_exchange_file(Kotek::ktk::uint32_t file_id)
{
	KOTEK_ASSERT(
		this->m_p_resource_manager, "resource manager must be initialized!");

	if (this->m_p_resource_manager)
	{
		this->m_p_resource_manager->Close(file_id);

		Kotek::Core::ktkResourceWritingRequest request;
		request.Set_ResourceType(Kotek::Core::eResourceWritingType::kText);

		const auto& path_to_exchange =
			this->get_full_path_of_file(_kExchangeFileNameWithExtension);

		request.Set_Path(path_to_exchange);
		request.Set_ID(file_id);

		this->m_p_resource_manager->Open(request);
	}
}
