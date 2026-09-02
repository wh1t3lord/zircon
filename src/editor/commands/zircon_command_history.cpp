#include "zircon_command_history.h"
#include "../../world/zircon_world.h"
#include "../../ecs/zircon_factory.h"
#include "../session/zircon_session_editor.h"
#include "../session/zircon_session_editor_manager.h"

constexpr const char* _kJournalFileNameWithExtension =
	"history.zjrnl";
constexpr const char* _kSnapshotFileNameWithExtension =
	"history.zsnap";

zircon_editor_command_history::zircon_editor_command_history(
	void
) :
	m_is_changed{}, m_is_scratch_in_use{},
	m_p_manager_session_editor{}, m_p_filesystem{},
	m_cursor_node_id{zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID},
	m_snapshot_interval{
		zircon_DEF_COMMAND_HISTORY_SNAPSHOT_INTERVAL
	},
	m_entity_watermark{}, m_total_snapshot_count{},
	m_pool_next_victim_slot{},
	m_pending_slot{zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT}
{
	for (kotek::size_t i = 0;
	     i < this->m_pool_commands.size();
	     ++i)
	{
		this->m_pool_commands[i] = nullptr;
		this->m_pool_node_ids[i] =
			zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
	}
}

zircon_editor_command_history::~zircon_editor_command_history(
	void
)
{
	if (this->m_journal.is_open())
	{
		this->shutdown();
	}
}

void zircon_editor_command_history::initialize(
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_streaming_folder_name
)
{
	KOTEK_ASSERT(
		p_filesystem,
		"you must pass a valid pointer of file system "
		"interface!"
	);
	KOTEK_ASSERT(
		p_manager_session_editor,
		"you must pass a valid session editor manager!"
	);
	KOTEK_ASSERT(
		p_streaming_folder_name,
		"you must pass a valid folder name!"
	);

	this->m_p_filesystem = p_filesystem;
	this->m_p_manager_session_editor = p_manager_session_editor;

	zircon_register_builtin_command_types(
		this->m_command_registry
	);

	KOTEK_ASSERT(
		this->m_command_registry
				.get_max_instance_size() <=
			zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE,
		"registered commands do not fit into the pool slots, "
		"increase zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE"
	);

	ktk_filesystem_path path_to_file;

	this->m_p_filesystem->Make_Path(
		path_to_file,
		kotek::core::eFolderIndex::
			kFolderIndex_DataUser_SDK_Scenes
	);

	path_to_file /= p_streaming_folder_name;

	this->m_path_to_streaming_folder.clear();

	{
		auto u8_string = path_to_file.u8string();

		this->m_path_to_streaming_folder.append(
			reinterpret_cast<const char*>(u8_string.data()),
			u8_string.size()
		);
	}

	bool is_valid_path = this->m_p_filesystem->Is_Exists(
		this->m_path_to_streaming_folder.c_str()
	);

	if (is_valid_path == false)
	{
		this->m_p_filesystem->Create_Directory(
			this->m_path_to_streaming_folder.c_str(),
			kotek::core::eFolderVisibilityType::kVisible
		);
	}

	// reset the tree to the root sentinel
	this->m_nodes.clear();

	zircon_history_tree_node root_node{};
	root_node.m_parent_node_id =
		zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
	root_node.m_preferred_child_node_id =
		zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
	root_node.m_command_type = 0;
	root_node.m_entity_id = 0;
	root_node.m_depth = 0;
	root_node.m_pool_slot =
		zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
	root_node.m_locator = {};
	root_node.m_snapshot_offset = 0;
	root_node.m_snapshot_compressed_size = 0;
	root_node.m_snapshot_raw_size = 0;

	this->m_nodes.push_back(root_node);

	this->m_cursor_node_id =
		zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID;
	this->m_entity_id_translation.clear();
	this->m_entity_reincarnation.clear();
	this->m_entity_watermark = 0;
	this->m_total_snapshot_count = 0;

	ktk_filesystem_path journal_path = path_to_file;
	journal_path /= _kJournalFileNameWithExtension;

	ktk_filesystem_path snapshot_path = path_to_file;
	snapshot_path /= _kSnapshotFileNameWithExtension;

	if (this->m_journal.open(journal_path, snapshot_path) ==
	    false)
	{
		KOTEK_MESSAGE_ERROR(
			"failed to open the command journal in folder: {}",
			this->m_path_to_streaming_folder.c_str()
		);
		return;
	}

	// rebuild the tree from an existing journal (full retention
	// across sessions); the cursor lands on the last recorded node
	if (this->m_journal.get_total_entry_count() > 0)
	{
		this->m_journal.read_all_entries(
			[this](
				const zircon_command_journal_entry_header& header,
				const unsigned char* p_payload,
				const zircon_command_journal_locator& locator
			)
			{
				(void)p_payload;

				zircon_history_tree_node node{};
				node.m_parent_node_id = header.m_parent_node_id;
				node.m_preferred_child_node_id =
					zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
				node.m_command_type = header.m_command_type;
				node.m_entity_id = header.m_entity_id;
				node.m_pool_slot =
					zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
				node.m_locator = locator;
				node.m_snapshot_offset = 0;
				node.m_snapshot_compressed_size = 0;
				node.m_snapshot_raw_size = 0;

				const kotek::uint32_t new_node_id =
					static_cast<kotek::uint32_t>(
						this->m_nodes.size()
					);

				if (node.m_parent_node_id <
				    this->m_nodes.size())
				{
					node.m_depth =
						this->m_nodes[node.m_parent_node_id]
							.m_depth +
						1;

					// the last appended child is the preferred
					// one
					this->m_nodes[node.m_parent_node_id]
						.m_preferred_child_node_id = new_node_id;
				}
				else
				{
					node.m_depth = 1;
				}

				if (node.m_entity_id >
				    this->m_entity_watermark)
				{
					this->m_entity_watermark =
						node.m_entity_id;
				}

				this->m_nodes.push_back(node);
				this->m_cursor_node_id = new_node_id;
			}
		);

		// re-attach snapshot records to their nodes
		this->m_journal.read_all_snapshot_headers(
			[this](
				kotek::uint32_t node_id,
				kotek::uint64_t offset,
				kotek::uint32_t compressed_size,
				kotek::uint32_t raw_size
			)
			{
				if (node_id < this->m_nodes.size())
				{
					this->m_nodes[node_id]
						.m_snapshot_offset = offset;
					this->m_nodes[node_id]
						.m_snapshot_compressed_size =
						compressed_size;
					this->m_nodes[node_id]
						.m_snapshot_raw_size = raw_size;

					++this->m_total_snapshot_count;
				}
			}
		);

		KOTEK_MESSAGE(
			"[history]: loaded {} commands from the journal",
			this->m_nodes.size() - 1
		);
	}
}

void zircon_editor_command_history::shutdown(void)
{
	for (kotek::size_t i = 0;
	     i < this->m_pool_commands.size();
	     ++i)
	{
		if (this->m_pool_commands[i])
		{
			this->m_pool_commands[i]->~ktkISDKRedoUndo();
			this->m_pool_commands[i] = nullptr;
			this->m_pool_node_ids[i] =
				zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
		}
	}

	if (this->m_is_scratch_in_use)
	{
		kotek::core::ktkISDKRedoUndo* p_command =
			reinterpret_cast<kotek::core::ktkISDKRedoUndo*>(
				this->m_scratch_storage
			);
		p_command->~ktkISDKRedoUndo();
		this->m_is_scratch_in_use = false;
	}

	// flushes and closes the files; the content stays on disk
	// forever (no wipe, full retention)
	this->m_journal.close();

	this->m_nodes.clear();
	this->m_entity_id_translation.clear();
	this->m_entity_reincarnation.clear();
	this->m_cursor_node_id =
		zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID;
	this->m_pending_slot =
		zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
}

void zircon_editor_command_history::ExecuteCommand(
	kotek::core::ktkISDKRedoUndo* p_command
)
{
	KOTEK_ASSERT(
		p_command, "you can't send an invalid command here"
	);

	if (p_command == nullptr)
		return;

	const kotek::uint32_t command_type =
		static_cast<kotek::uint32_t>(
			p_command->GetCommandType()
		);

	KOTEK_ASSERT(
		this->m_command_registry.find_by_type(
			command_type
		),
		"command [{}] with type {} is not registered in "
		"zircon_command_registry!",
		p_command->GetName(),
		command_type
	);

	p_command->Execute();

	// serialize the delta payload; every registered command
	// implements the delta interface (registry contract)
	zircon_interface_command_delta* p_delta =
		dynamic_cast<zircon_interface_command_delta*>(p_command);

	KOTEK_ASSERT(
		p_delta,
		"command [{}] does not implement "
		"zircon_interface_command_delta",
		p_command->GetName()
	);

	if (p_delta == nullptr)
		return;

	zircon_command_delta_writer writer(
		this->m_payload_scratch, sizeof(this->m_payload_scratch)
	);

	const bool serialized = p_delta->Serialize_Delta(writer);

	KOTEK_ASSERT(
		serialized && writer.is_valid(),
		"command [{}] failed to serialize its delta, the "
		"payload is too big for zircon_DEF_MAXIMUM_COMMAND_SIZE "
		"({})",
		p_command->GetName(),
		zircon_DEF_MAXIMUM_COMMAND_SIZE
	);

	if (serialized == false || writer.is_valid() == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: command [{}] was executed but NOT "
			"journaled (delta serialization failed), history "
			"and world diverge!",
			p_command->GetName()
		);
		return;
	}

	const kotek::uint32_t recorded_entity_id =
		static_cast<kotek::uint32_t>(
			p_command->GetEntityID().id
		);

	zircon_command_journal_entry_header entry_header{};
	entry_header.m_node_id = static_cast<kotek::uint32_t>(
		this->m_nodes.size()
	);
	entry_header.m_parent_node_id = this->m_cursor_node_id;
	entry_header.m_command_type = command_type;
	entry_header.m_entity_id = recorded_entity_id;
	entry_header.m_payload_size =
		static_cast<kotek::uint32_t>(writer.get_offset());

	zircon_command_journal_locator locator{};

	if (this->m_journal.append_entry(
			entry_header,
			this->m_payload_scratch,
			entry_header.m_payload_size,
			locator
		) == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: failed to append a journal entry for "
			"[{}]",
			p_command->GetName()
		);
		return;
	}

	// new child at the cursor: a new action after undo creates a
	// BRANCH, nothing is ever truncated
	zircon_history_tree_node node{};
	node.m_parent_node_id = this->m_cursor_node_id;
	node.m_preferred_child_node_id =
		zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
	node.m_command_type = command_type;
	node.m_entity_id = recorded_entity_id;
	node.m_depth =
		this->m_nodes[this->m_cursor_node_id].m_depth + 1;
	node.m_pool_slot =
		zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
	node.m_locator = locator;
	node.m_snapshot_offset = 0;
	node.m_snapshot_compressed_size = 0;
	node.m_snapshot_raw_size = 0;

	const kotek::uint32_t new_node_id =
		static_cast<kotek::uint32_t>(this->m_nodes.size());

	this->m_nodes[this->m_cursor_node_id]
		.m_preferred_child_node_id = new_node_id;

	this->m_nodes.push_back(node);
	this->m_cursor_node_id = new_node_id;

	// bind the pool slot that allocate_memory_for_command handed
	// out for this command
	if (this->m_pending_slot !=
	    zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT)
	{
		this->m_pool_commands[this->m_pending_slot] = p_command;
		this->m_pool_node_ids[this->m_pending_slot] =
			new_node_id;
		this->m_nodes[new_node_id].m_pool_slot =
			this->m_pending_slot;
		this->m_pending_slot =
			zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
	}

	this->observe_entity_id(p_command, recorded_entity_id);

	this->take_snapshot_if_needed(new_node_id);

	this->set_changed(true);
}

void zircon_editor_command_history::Undo()
{
	if (this->m_cursor_node_id ==
	    zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID)
	{
		KOTEK_TRACE("[history]: nothing to undo");
		return;
	}

	zircon_history_tree_node& node =
		this->m_nodes[this->m_cursor_node_id];

	kotek::core::ktkISDKRedoUndo* p_command =
		this->get_command_for_node(this->m_cursor_node_id);

	if (p_command == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: failed to obtain a command for node {}",
			this->m_cursor_node_id
		);

		// the node exists in the tree but its journal payload is
		// unreadable — skipping it keeps the cursor moving so
		// undo-to-origin drivers terminate on a corrupt journal
		// instead of spinning on the same node forever
		this->m_cursor_node_id = node.m_parent_node_id;

		this->set_changed(true);
		return;
	}

	const kotek::uint32_t recorded_id = node.m_entity_id;
	const kotek::uint32_t live_id =
		this->translate_entity_id(recorded_id);

	if (live_id != recorded_id)
	{
		p_command->SetEntityID(kotek::entity_t{live_id});
	}

	p_command->Undo();

	// an undone delete-entity reincarnates its entity: record the
	// chain link so every recorded alias of it resolves to the new
	// incarnation (the command itself may report a stale alias when
	// it was reconstructed from the journal)
	const kotek::uint32_t live_id_after =
		static_cast<kotek::uint32_t>(p_command->GetEntityID().id);

	if (live_id_after != live_id &&
	    live_id_after != 0 &&
	    live_id_after !=
	        zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
	{
		this->update_dependent_commands(
			kotek::entity_t{live_id},
			kotek::entity_t{live_id_after}
		);
	}

	this->observe_entity_id(p_command, recorded_id);

	this->release_scratch_command(p_command);

	this->m_cursor_node_id = node.m_parent_node_id;

	this->set_changed(true);
}

void zircon_editor_command_history::Redo()
{
	const kotek::uint32_t child_node_id =
		this->m_nodes[this->m_cursor_node_id]
			.m_preferred_child_node_id;

	if (child_node_id ==
	    zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID)
	{
		KOTEK_TRACE("[history]: nothing to redo");
		return;
	}

	this->execute_node(child_node_id);

	this->m_cursor_node_id = child_node_id;

	this->set_changed(true);
}

void zircon_editor_command_history::set_changed(bool status
) noexcept
{
	this->m_is_changed = status;
}

bool zircon_editor_command_history::is_changed() const noexcept
{
	return this->m_is_changed;
}

const kotek::array_t<
	kotek::core::ktkISDKRedoUndo*,
	zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
zircon_editor_command_history::GetCommands(void) const noexcept
{
	return this->m_pool_commands;
}

void zircon_editor_command_history::update_dependent_commands(
	kotek::entity_t id_what_will_be_deleted,
	kotek::entity_t id_that_replaces_what_will_be_deleted
) noexcept
{
	if (id_what_will_be_deleted.id ==
	    id_that_replaces_what_will_be_deleted.id)
	{
		return;
	}

	this->set_entity_reincarnation(
		static_cast<kotek::uint32_t>(id_what_will_be_deleted.id),
		static_cast<kotek::uint32_t>(
			id_that_replaces_what_will_be_deleted.id
		)
	);

	// fix the live pool so that pending commands reference the
	// new live id
	for (kotek::size_t i = 0;
	     i < this->m_pool_commands.size();
	     ++i)
	{
		kotek::core::ktkISDKRedoUndo* p_command =
			this->m_pool_commands[i];

		if (p_command)
		{
			if (p_command->GetEntityID().id ==
			    id_what_will_be_deleted.id)
			{
				p_command->SetEntityID(
					id_that_replaces_what_will_be_deleted
				);
			}
		}
	}
}

unsigned char*
zircon_editor_command_history::allocate_memory_for_command(
	kotek::size_t size_of_class, const char* p_debug_type_name
) noexcept
{
	KOTEK_ASSERT(
		size_of_class != 0 &&
			size_of_class != kotek::size_t(-1),
		"you can't pass a invalid size!"
	);
	KOTEK_ASSERT(
		size_of_class <=
			zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE,
		"you passed size larger than the command slot ({} > "
		"{})!",
		size_of_class,
		zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE
	);

	const kotek::uint32_t slot_index =
		this->m_pool_next_victim_slot;

	this->m_pool_next_victim_slot =
		(this->m_pool_next_victim_slot + 1) %
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE;

	this->evict_pool_slot(slot_index);

	std::memset(
		this->m_pool_storage[slot_index],
		0,
		zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE
	);

	this->m_pending_slot = slot_index;

	return this->m_pool_storage[slot_index];
}

kotek::size_t
zircon_editor_command_history::get_current_index(void) const
{
	return this->m_cursor_node_id;
}

kotek::ptrdiff_t
zircon_editor_command_history::get_cursor_index(void) const
{
	return static_cast<kotek::ptrdiff_t>(
		this->m_cursor_node_id
	);
}

kotek::uint64_t
zircon_editor_command_history::get_total_recorded_commands(void
) const noexcept
{
	return this->m_nodes.empty()
		? 0
		: static_cast<kotek::uint64_t>(this->m_nodes.size() - 1);
}

kotek::uint32_t
zircon_editor_command_history::get_cursor_node_id(void
) const noexcept
{
	return this->m_cursor_node_id;
}

kotek::entity_t
zircon_editor_command_history::get_live_entity_id(
	kotek::entity_t recorded_id
) const noexcept
{
	kotek::entity_t result;
	result.id = this->translate_entity_id(
		static_cast<kotek::uint32_t>(recorded_id.id)
	);

	return result;
}

kotek::uint32_t
zircon_editor_command_history::get_entity_watermark(void
) const noexcept
{
	return this->m_entity_watermark;
}

bool zircon_editor_command_history::restore_node(
	kotek::uint32_t node_id
) noexcept
{
	KOTEK_ASSERT(
		node_id < this->m_nodes.size(),
		"node {} does not exist (nodes: {})",
		node_id,
		this->m_nodes.size()
	);

	if (node_id >= this->m_nodes.size())
		return false;

	zircon_world* p_world = this->get_current_world();

	if (p_world == nullptr ||
	    p_world->get_ecs_context() == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: restore_node requires an initialized "
			"world"
		);
		return false;
	}

	// build the path root..target (stored target-first)
	kotek::hybrid_vector_t<
		kotek::uint32_t,
		zircon_DEF_COMMAND_HISTORY_PATH_INLINE_COUNT>
		path;

	for (kotek::uint32_t current = node_id; current !=
	     zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID;
	     current = this->m_nodes[current].m_parent_node_id)
	{
		path.push_back(current);
	}

	// find the deepest node with a snapshot (path[0] is the
	// target itself)
	int snapshot_path_index = -1;

	for (kotek::size_t i = 0; i < path.size(); ++i)
	{
		if (this->m_nodes[path[i]].m_snapshot_offset != 0)
		{
			snapshot_path_index = static_cast<int>(i);
			break;
		}
	}

	this->destroy_all_entities(p_world);

	this->m_cursor_node_id =
		zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID;

	int replay_start_index = static_cast<int>(path.size()) - 1;

	if (snapshot_path_index >= 0)
	{
		const zircon_history_tree_node& snapshot_node =
			this->m_nodes[path[snapshot_path_index]];

		KOTEK_TRACE(
			"[history]: restore_node {} from snapshot of node {} "
			"(raw {} bytes)",
			node_id,
			path[snapshot_path_index],
			snapshot_node.m_snapshot_raw_size
		);

		kotek::hybrid_vector_t<unsigned char, 4096>
			snapshot_data;
		snapshot_data.resize(
			snapshot_node.m_snapshot_raw_size
		);

		if (this->m_journal.read_snapshot(
				snapshot_node.m_snapshot_offset,
				snapshot_node.m_snapshot_compressed_size,
				snapshot_node.m_snapshot_raw_size,
				snapshot_data.data(),
				snapshot_node.m_snapshot_raw_size
			) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[history]: failed to read the snapshot of "
				"node {}",
				path[snapshot_path_index]
			);
			return false;
		}

		if (this->apply_world_state(
				p_world,
				snapshot_data.data(),
				snapshot_node.m_snapshot_raw_size
			) == false)
		{
			return false;
		}

		KOTEK_TRACE(
			"[history]: restore_node {} snapshot applied",
			node_id
		);

		this->m_cursor_node_id = path[snapshot_path_index];
		replay_start_index = snapshot_path_index - 1;
	}

	// replay the journal forward to the target
	for (int i = replay_start_index; i >= 0; --i)
	{
		this->execute_node(path[i]);
		this->m_cursor_node_id = path[i];
	}

	KOTEK_TRACE(
		"[history]: restore_node {} done",
		node_id
	);

	this->set_changed(true);

	return true;
}

bool zircon_editor_command_history::
	get_serialized_component_by_entity_and_component_type_id(
		kotek::ktk::json::value&
			constructed_value_on_stack_based_on_placement_new_memory,
		kotek::entity_t id,
		eZirconComponentType type_id
	)
{
	constexpr kotek::uint32_t delete_entity_command_type =
		static_cast<kotek::uint32_t>(
			kotek::core::eConsoleCommandIndex::
				kConsoleCommand_SDK_DeleteEntity
		);

	kotek::uint32_t current = this->m_cursor_node_id;

	while (current != zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID)
	{
		const zircon_history_tree_node& node =
			this->m_nodes[current];

		if (node.m_command_type == delete_entity_command_type &&
		    node.m_entity_id ==
		        static_cast<kotek::uint32_t>(id.id))
		{
			zircon_command_journal_entry_header header{};

			if (this->m_journal.read_entry(
					node.m_locator,
					header,
					this->m_payload_scratch,
					sizeof(this->m_payload_scratch)
				) == false)
			{
				return false;
			}

			zircon_command_delta_reader reader(
				this->m_payload_scratch,
				header.m_payload_size
			);

			bool status{};

			reader.read_u32(&status);

			if (status == false)
				return false;

			const kotek::uint32_t component_count =
				reader.read_u32(&status);

			if (status == false)
				return false;

			for (kotek::uint32_t i = 0;
			     i < component_count;
			     ++i)
			{
				const kotek::uint32_t component_type =
					reader.read_u32(&status);

				if (status == false)
					return false;

				char state_buffer
					[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

				status = reader.read_string(
					state_buffer, sizeof(state_buffer)
				);

				if (status == false)
					return false;

				if (component_type ==
				    static_cast<kotek::uint32_t>(type_id))
				{
					kotek::ktk::json::error_code
						parse_error;

					kotek::ktk::json::value parsed =
						kotek::ktk::json::parse(
							kotek::cstring_view_t(
								state_buffer,
								strlen(state_buffer)
							),
							parse_error
						);

					if (parse_error)
					{
						KOTEK_MESSAGE_ERROR(
							"[history]: failed to parse a "
							"serialized component: {}",
							parse_error.message()
						);
						return false;
					}

					constructed_value_on_stack_based_on_placement_new_memory =
						parsed;

					return true;
				}
			}
		}

		current = node.m_parent_node_id;
	}

	return false;
}

void zircon_editor_command_history::set_snapshot_interval(
	kotek::uint32_t interval
) noexcept
{
	this->m_snapshot_interval = interval;
}

kotek::uint32_t
zircon_editor_command_history::get_snapshot_interval(void
) const noexcept
{
	return this->m_snapshot_interval;
}

kotek::uint64_t
zircon_editor_command_history::get_journal_file_size(
	void
) noexcept
{
	return this->m_journal.get_journal_file_size();
}

kotek::uint64_t
zircon_editor_command_history::get_snapshot_file_size(
	void
) noexcept
{
	return this->m_journal.get_snapshot_file_size();
}

const char*
zircon_editor_command_history::get_streaming_folder_path(
	void
) const noexcept
{
	return this->m_path_to_streaming_folder.c_str();
}

zircon_command_registry&
zircon_editor_command_history::get_command_registry(
	void
) noexcept
{
	return this->m_command_registry;
}

kotek::uint64_t
zircon_editor_command_history::get_journal_raw_entry_bytes(
	void
) const noexcept
{
	return this->m_journal.get_total_raw_entry_bytes();
}

kotek::uint64_t
zircon_editor_command_history::get_total_snapshot_count(void
) const noexcept
{
	return this->m_total_snapshot_count;
}

kotek::core::ktkISDKRedoUndo*
zircon_editor_command_history::get_command_for_node(
	kotek::uint32_t node_id
) noexcept
{
	const zircon_history_tree_node& node =
		this->m_nodes[node_id];

	if (node.m_pool_slot !=
	        zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT &&
	    node.m_pool_slot <
	        zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE)
	{
		if (this->m_pool_node_ids[node.m_pool_slot] == node_id &&
		    this->m_pool_commands[node.m_pool_slot] !=
		        nullptr)
		{
			return this->m_pool_commands[node.m_pool_slot];
		}
	}

	return this->reconstruct_command(node_id);
}

kotek::core::ktkISDKRedoUndo*
zircon_editor_command_history::reconstruct_command(
	kotek::uint32_t node_id
) noexcept
{
	KOTEK_ASSERT(
		this->m_is_scratch_in_use == false,
		"scratch command slot is already in use, undo/redo "
		"must not nest"
	);

	if (this->m_is_scratch_in_use)
		return nullptr;

	const zircon_history_tree_node& node =
		this->m_nodes[node_id];

	const zircon_command_type_info* p_type_info =
		this->m_command_registry.find_by_type(
			node.m_command_type
		);

	if (p_type_info == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: unknown command type {} in node {}",
			node.m_command_type,
			node_id
		);
		return nullptr;
	}

	zircon_command_journal_entry_header header{};

	if (this->m_journal.read_entry(
			node.m_locator,
			header,
			this->m_payload_scratch,
			sizeof(this->m_payload_scratch)
		) == false)
	{
		return nullptr;
	}

	zircon_factory* p_factory = nullptr;

	if (zircon_world* p_world = this->get_current_world())
	{
		p_factory = p_world->get_factory();
	}

	kotek::core::ktkISDKRedoUndo* p_command =
		p_type_info->m_p_create(
			this->m_scratch_storage,
			this->m_p_manager_session_editor,
			p_factory
		);

	if (p_command == nullptr)
		return nullptr;

	zircon_interface_command_delta* p_delta =
		dynamic_cast<zircon_interface_command_delta*>(
			p_command
		);

	KOTEK_ASSERT(
		p_delta,
		"reconstructed command of type {} does not implement "
		"zircon_interface_command_delta",
		node.m_command_type
	);

	if (p_delta == nullptr)
	{
		p_command->~ktkISDKRedoUndo();
		return nullptr;
	}

	zircon_command_delta_reader reader(
		this->m_payload_scratch, header.m_payload_size
	);

	if (p_delta->Deserialize_Delta(reader) == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: failed to deserialize the delta of "
			"node {}",
			node_id
		);

		p_command->~ktkISDKRedoUndo();
		return nullptr;
	}

	this->m_is_scratch_in_use = true;

	return p_command;
}

void zircon_editor_command_history::release_scratch_command(
	kotek::core::ktkISDKRedoUndo* p_command
) noexcept
{
	if (p_command == nullptr)
		return;

	if (this->m_is_scratch_in_use == false)
		return;

	if (reinterpret_cast<unsigned char*>(p_command) >=
	        this->m_scratch_storage &&
	    reinterpret_cast<unsigned char*>(p_command) <
	        this->m_scratch_storage +
	            zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE)
	{
		p_command->~ktkISDKRedoUndo();
		this->m_is_scratch_in_use = false;
	}
}

void zircon_editor_command_history::execute_node(
	kotek::uint32_t node_id
) noexcept
{
	kotek::core::ktkISDKRedoUndo* p_command =
		this->get_command_for_node(node_id);

	if (p_command == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[history]: failed to obtain a command for node {}",
			node_id
		);
		return;
	}

	const kotek::uint32_t recorded_id =
		this->m_nodes[node_id].m_entity_id;
	const kotek::uint32_t live_id =
		this->translate_entity_id(recorded_id);

	KOTEK_TRACE(
		"[history]: execute_node {} type {} entity {} -> {}",
		node_id,
		this->m_nodes[node_id].m_command_type,
		recorded_id,
		live_id
	);

	if (live_id != recorded_id)
	{
		p_command->SetEntityID(kotek::entity_t{live_id});
	}

	p_command->Execute();

	// a redone create-entity reincarnates its entity: record the
	// chain link so every recorded alias of it resolves to the new
	// incarnation (the command itself may report a stale alias when
	// it was reconstructed from the journal)
	const kotek::uint32_t live_id_after =
		static_cast<kotek::uint32_t>(p_command->GetEntityID().id);

	if (live_id_after != live_id &&
	    live_id_after != 0 &&
	    live_id_after !=
	        zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
	{
		this->update_dependent_commands(
			kotek::entity_t{live_id},
			kotek::entity_t{live_id_after}
		);
	}

	this->observe_entity_id(p_command, recorded_id);

	this->release_scratch_command(p_command);

	this->take_snapshot_if_needed(node_id);
}

kotek::uint32_t
zircon_editor_command_history::translate_entity_id(
	kotek::uint32_t recorded_id
) const noexcept
{
	kotek::uint32_t result = recorded_id;

	if (recorded_id < this->m_entity_id_translation.size())
	{
		const kotek::uint32_t translated =
			this->m_entity_id_translation[recorded_id];

		if (translated !=
		    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
		{
			result = translated;
		}
	}

	// follow the reincarnation chain to the latest incarnation;
	// chain values strictly increase (pico_ecs hands out fresh ids
	// and never recycles while zircon does not flush the destroy
	// queue), so the walk always terminates
	kotek::uint32_t guard = 0;
	const kotek::uint32_t guard_limit =
		static_cast<kotek::uint32_t>(
			this->m_entity_reincarnation.size()
		) + 16;

	while (result < this->m_entity_reincarnation.size())
	{
		const kotek::uint32_t next =
			this->m_entity_reincarnation[result];

		if (next ==
		    zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID)
		{
			break;
		}

		result = next;

		if (++guard > guard_limit)
		{
			KOTEK_ASSERT(
				false,
				"reincarnation chain has a cycle at entity {}",
				result
			);
			break;
		}
	}

	return result;
}

void zircon_editor_command_history::set_entity_reincarnation(
	kotek::uint32_t live_id, kotek::uint32_t next_id
) noexcept
{
	if (live_id == 0)
		return;

	if (live_id >= this->m_entity_reincarnation.size())
	{
		this->m_entity_reincarnation.resize(
			live_id + 1,
			zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID
		);
	}

	this->m_entity_reincarnation[live_id] = next_id;

	if (live_id > this->m_entity_watermark)
	{
		this->m_entity_watermark = live_id;
	}

	if (next_id != zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID &&
	    next_id > this->m_entity_watermark)
	{
		this->m_entity_watermark = next_id;
	}
}

void zircon_editor_command_history::set_entity_translation(
	kotek::uint32_t recorded_id, kotek::uint32_t live_id
) noexcept
{
	if (recorded_id == 0)
		return;

	if (recorded_id >= this->m_entity_id_translation.size())
	{
		this->m_entity_id_translation.resize(
			recorded_id + 1,
			zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID
		);
	}

	this->m_entity_id_translation[recorded_id] = live_id;

	if (recorded_id > this->m_entity_watermark)
	{
		this->m_entity_watermark = recorded_id;
	}

	if (live_id != zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID &&
	    live_id > this->m_entity_watermark)
	{
		this->m_entity_watermark = live_id;
	}
}

void zircon_editor_command_history::observe_entity_id(
	kotek::core::ktkISDKRedoUndo* p_command,
	kotek::uint32_t recorded_id
) noexcept
{
	KOTEK_ASSERT(p_command, "must be a valid command");

	if (p_command == nullptr)
		return;

	const kotek::uint32_t live_id =
		static_cast<kotek::uint32_t>(
			p_command->GetEntityID().id
		);

	if (live_id == 0)
		return;

	this->set_entity_translation(recorded_id, live_id);
}

void zircon_editor_command_history::evict_pool_slot(
	kotek::uint32_t slot_index
) noexcept
{
	kotek::core::ktkISDKRedoUndo* p_command =
		this->m_pool_commands[slot_index];

	if (p_command)
	{
		p_command->~ktkISDKRedoUndo();
		this->m_pool_commands[slot_index] = nullptr;
	}

	const kotek::uint32_t evicted_node_id =
		this->m_pool_node_ids[slot_index];

	if (evicted_node_id !=
	        zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID &&
	    evicted_node_id < this->m_nodes.size())
	{
		this->m_nodes[evicted_node_id].m_pool_slot =
			zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT;
	}

	this->m_pool_node_ids[slot_index] =
		zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID;
}

void zircon_editor_command_history::take_snapshot_if_needed(
	kotek::uint32_t node_id
) noexcept
{
	if (this->m_snapshot_interval == 0)
		return;

	zircon_history_tree_node& node = this->m_nodes[node_id];

	if (node.m_depth % this->m_snapshot_interval != 0)
		return;

	if (node.m_snapshot_offset != 0)
		return;

	zircon_world* p_world = this->get_current_world();

	if (p_world == nullptr ||
	    p_world->get_ecs_context() == nullptr)
		return;

	kotek::hybrid_vector_t<unsigned char, 4096> world_state;

	this->capture_world_state(p_world, world_state);

	kotek::uint64_t offset{};
	kotek::uint32_t compressed_size{};

	if (this->m_journal.append_snapshot(
			node_id,
			world_state.data(),
			static_cast<kotek::uint32_t>(world_state.size()),
			offset,
			compressed_size
		))
	{
		node.m_snapshot_offset = offset;
		node.m_snapshot_compressed_size = compressed_size;
		node.m_snapshot_raw_size =
			static_cast<kotek::uint32_t>(world_state.size());

		++this->m_total_snapshot_count;
	}
}

zircon_world*
zircon_editor_command_history::get_current_world(void) noexcept
{
	zircon_world* p_result = nullptr;

	if (this->m_p_manager_session_editor)
	{
		zircon_session_editor* p_session =
			this->m_p_manager_session_editor->get_session(
				this->m_p_manager_session_editor
					->get_current_session_id()
			);

		if (p_session)
		{
			p_result = p_session->get_world();
		}
	}

	return p_result;
}

void zircon_editor_command_history::capture_world_state(
	zircon_world* p_world,
	kotek::hybrid_vector_t<unsigned char, 4096>& output
) noexcept
{
	output.clear();

	KOTEK_ASSERT(p_world, "must be a valid world");

	if (p_world == nullptr)
		return;

	zircon_factory* p_factory = p_world->get_factory();
	zircon_ecs_context_t* p_context =
		p_world->get_ecs_context();

	KOTEK_ASSERT(p_factory, "world must have a factory");
	KOTEK_ASSERT(p_context, "world must have an ecs context");

	if (p_factory == nullptr || p_context == nullptr)
		return;

	auto append_u32 =
		[&output](kotek::uint32_t value)
	{
		const unsigned char* p_bytes =
			reinterpret_cast<const unsigned char*>(&value);
		output.insert(
			output.end(), p_bytes, p_bytes + sizeof(value)
		);
	};

	// live entities come out ordered by their (dense) id, so the
	// serialization is canonical
	kotek::hybrid_vector_t<kotek::entity_t, 1024> entities;
	entities.resize(this->m_entity_watermark + 16);

	const kotek::uint32_t entity_count =
		p_factory->get_all_entities(
			p_context,
			this->m_entity_watermark + 16,
			entities.data(),
			static_cast<kotek::uint32_t>(entities.size())
		);

	entities.resize(entity_count);

	append_u32(entity_count);

	for (const kotek::entity_t& entity : entities)
	{
		kotek::uint32_t component_count = 0;

		for (int i = 0;
		     i < static_cast<int>(eZirconComponentType::kunknown);
		     ++i)
		{
			if (p_factory->has_component(
					p_context,
					entity,
					static_cast<eZirconComponentType>(i)
				))
			{
				++component_count;
			}
		}

		append_u32(static_cast<kotek::uint32_t>(entity.id));
		append_u32(component_count);

		for (int i = 0;
		     i < static_cast<int>(eZirconComponentType::kunknown);
		     ++i)
		{
			const eZirconComponentType component_type =
				static_cast<eZirconComponentType>(i);

			if (p_factory->has_component(
					p_context, entity, component_type
				) == false)
			{
				continue;
			}

			zircon_component_interface* p_component =
				p_factory->get_component_by_enum(
					p_context, entity, component_type
				);

			if (p_component == nullptr)
				continue;

			kotek::ktk::json::value serialized_state =
				zircon_serialize_component(p_component);

			auto state_string =
				kotek::ktk::json::serialize(serialized_state);

			append_u32(static_cast<kotek::uint32_t>(
				component_type
			));
			append_u32(static_cast<kotek::uint32_t>(
				state_string.size()
			));

			output.insert(
				output.end(),
				reinterpret_cast<const unsigned char*>(
					state_string.data()
				),
				reinterpret_cast<const unsigned char*>(
					state_string.data()
				) +
					state_string.size()
			);
		}
	}
}

bool zircon_editor_command_history::apply_world_state(
	zircon_world* p_world,
	const unsigned char* p_data,
	kotek::uint32_t data_size
) noexcept
{
	KOTEK_ASSERT(p_world, "must be a valid world");
	KOTEK_ASSERT(p_data, "must be a valid buffer");

	if (p_world == nullptr || p_data == nullptr)
		return false;

	zircon_factory* p_factory = p_world->get_factory();
	zircon_ecs_context_t* p_context =
		p_world->get_ecs_context();

	if (p_factory == nullptr || p_context == nullptr)
		return false;

	// NOTE: the translation hint table and the reincarnation chain
	// are deliberately NOT cleared here. The world is recreated with
	// fresh ids and every historical incarnation id of a restored
	// entity is chained forward to its fresh incarnation below; ids
	// that belong to entities not present in the snapshot stay dead
	// and are rebound by the replayed commands (restore_node
	// re-executes every command after the snapshot point).

	zircon_command_delta_reader reader(p_data, data_size);

	bool status{};

	const kotek::uint32_t entity_count =
		reader.read_u32(&status);

	if (status == false)
		return false;

	for (kotek::uint32_t i = 0; i < entity_count; ++i)
	{
		const kotek::uint32_t snapshot_live_id =
			reader.read_u32(&status);
		const kotek::uint32_t component_count =
			reader.read_u32(&status);

		if (status == false)
			return false;

		kotek::entity_t new_entity =
			p_factory->create_entity(p_context);

		if (static_cast<kotek::uint32_t>(new_entity.id) >
		    this->m_entity_watermark)
		{
			this->m_entity_watermark =
				static_cast<kotek::uint32_t>(new_entity.id);
		}

		// chain rebinding: journal entries may reference ANY
		// historical incarnation id of this logical entity (the
		// snapshot stores the incarnation that was live when the
		// snapshot was taken, undo/redo may have reincarnated it
		// several times since). Walk the reincarnation chain from
		// the snapshot's incarnation forward and rebind every link
		// to the fresh entity
		{
			const kotek::uint32_t fresh_id =
				static_cast<kotek::uint32_t>(new_entity.id);

			kotek::uint32_t chain_walker = snapshot_live_id;
			kotek::uint32_t chain_guard = 0;
			const kotek::uint32_t chain_guard_limit =
				static_cast<kotek::uint32_t>(
					this->m_entity_reincarnation.size()
				) + 16;

			while (chain_walker !=
			           zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID &&
			       chain_walker != fresh_id)
			{
				kotek::uint32_t next =
					zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID;

				if (chain_walker <
				    this->m_entity_reincarnation.size())
				{
					next =
						this->m_entity_reincarnation
							[chain_walker];
				}

				this->set_entity_reincarnation(
					chain_walker, fresh_id
				);

				if (++chain_guard > chain_guard_limit)
				{
					KOTEK_ASSERT(
						false,
						"reincarnation chain has a cycle at "
						"entity {}",
						chain_walker
					);
					break;
				}

				chain_walker = next;
			}
		}

		for (kotek::uint32_t j = 0; j < component_count; ++j)
		{
			const kotek::uint32_t component_type =
				reader.read_u32(&status);
			const kotek::uint32_t state_size =
				reader.read_u32(&status);

			if (status == false)
				return false;

			if (state_size >=
			    zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON)
			{
				KOTEK_MESSAGE_ERROR(
					"[history]: snapshot component state is "
					"too big ({} bytes)",
					state_size
				);
				return false;
			}

			char state_buffer
				[zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON];

			status = reader.read_bytes(
				state_buffer, state_size
			);

			if (status == false)
				return false;

			state_buffer[state_size] = 0;

			p_factory->create_component(
				p_context,
				new_entity,
				static_cast<eZirconComponentType>(
					component_type
				)
			);

			zircon_component_interface* p_component =
				p_factory->get_component_by_enum(
					p_context,
					new_entity,
					static_cast<eZirconComponentType>(
						component_type
					)
				);

			if (p_component == nullptr)
				continue;

			kotek::ktk::json::error_code parse_error;

			kotek::ktk::json::value component_state =
				kotek::ktk::json::parse(
					kotek::cstring_view_t(
						state_buffer, state_size
					),
					parse_error
				);

			if (parse_error)
			{
				KOTEK_MESSAGE_ERROR(
					"[history]: failed to parse a snapshot "
					"component: {}",
					parse_error.message()
				);
				return false;
			}

			KOTEK_TRACE(
				"[history]: snapshot restore component {} for "
				"entity {}: [{}]",
				component_type,
				static_cast<kotek::uint32_t>(new_entity.id),
				state_buffer
			);

			p_factory->DeserializeComponent(
				p_component, component_state
			);
		}
	}

	return reader.is_valid();
}

void zircon_editor_command_history::destroy_all_entities(
	zircon_world* p_world
) noexcept
{
	KOTEK_ASSERT(p_world, "must be a valid world");

	if (p_world == nullptr)
		return;

	zircon_factory* p_factory = p_world->get_factory();
	zircon_ecs_context_t* p_context =
		p_world->get_ecs_context();

	if (p_factory == nullptr || p_context == nullptr)
		return;

	kotek::hybrid_vector_t<kotek::entity_t, 1024> entities;
	entities.resize(this->m_entity_watermark + 16);

	const kotek::uint32_t entity_count =
		p_factory->get_all_entities(
			p_context,
			this->m_entity_watermark + 16,
			entities.data(),
			static_cast<kotek::uint32_t>(entities.size())
		);

	for (kotek::uint32_t i = 0; i < entity_count; ++i)
	{
		p_factory->destroy_entity(p_context, entities[i]);
	}
}
