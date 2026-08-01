#pragma once

#include "zircon_command_definitions.h"
#include "zircon_command_journal.h"
#include "zircon_command_registry.h"

#include "../../ecs/zircon_ecs_auto_enum_components.h"

class zircon_world;
class zircon_factory;
class zircon_session_editor_manager;

/// @brief \~english one node of the history tree: a journaled
/// command with branch links. Node 0 is the root sentinel (the
/// initial world state, holds no command)
struct zircon_history_tree_node
{
	kotek::uint32_t m_parent_node_id;
	/// @brief \~english the child that redo descends into; a new
	/// action after undo creates a NEW child (branch) and replaces
	/// this pointer, the old child stays in the tree forever
	kotek::uint32_t m_preferred_child_node_id;
	kotek::uint32_t m_command_type;
	/// @brief \~english entity id as it was recorded when the
	/// command was first executed; translated through the history's
	/// id map on (re)application
	kotek::uint32_t m_entity_id;
	kotek::uint32_t m_depth;
	/// @brief \~english live pool slot or
	/// zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT when the command
	/// object is only present in the journal
	kotek::uint32_t m_pool_slot;
	zircon_command_journal_locator m_locator;
	/// @brief \~english snapshot identification, all zero when the
	/// node has no snapshot
	kotek::uint64_t m_snapshot_offset;
	kotek::uint32_t m_snapshot_compressed_size;
	kotek::uint32_t m_snapshot_raw_size;
};

/// @brief \~english full-retention undo/redo history (task Z6):
/// append-only zstd-compressed journal on disk + history tree with
/// branching in RAM + periodic world snapshots. Nothing is ever
/// deleted: a new action after undo creates a branch node at the
/// cursor, the old redo path stays reachable.
///
/// Undo/redo walk the tree along the preferred path. Commands of the
/// last zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE nodes live in a
/// fixed pool for instant access; older commands are reconstructed
/// from the journal (type registry + delta payload).
///
/// Entity ids: pico_ecs never recycles ids (destroyed entities are
/// queued and never flushed by zircon), so a re-executed
/// create/delete obtains a NEW id. The history keeps two flat
/// tables: "recorded id -> last observed live id" (a chain-start
/// hint updated on every (re)application) and a reincarnation chain
/// "incarnation id -> id it was recreated as" (updated by
/// update_dependent_commands whenever an entity is reincarnated).
/// translate_entity_id resolves a recorded id by following the
/// chain to the latest incarnation, so every recorded alias of a
/// logical entity stays valid no matter which alias was
/// reincarnated; journal entries stay immutable.
///
/// Snapshots (binary world states, zstd-compressed) are taken every
/// zircon_DEF_COMMAND_HISTORY_SNAPSHOT_INTERVAL applied commands and
/// accelerate restore_node; plain undo/redo never needs them because
/// command inverses are always available.
class zircon_editor_command_history
	: public kotek::core::ktkISDKCommandHistoryManager
{
public:
	zircon_editor_command_history(void);
	~zircon_editor_command_history(void);

	void initialize(
		zircon_session_editor_manager* p_manager_session_editor,
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_streaming_folder_name = "current"
	);
	void shutdown(void);

	void ExecuteCommand(kotek::core::ktkISDKRedoUndo* p_command
	) override;

	void Undo() override;
	void Redo() override;

	void set_changed(bool status) noexcept;
	bool is_changed() const noexcept;

	/// @brief \~english the live command pool (last executed
	/// commands around the cursor), kept for compatibility with the
	/// history log window
	const kotek::array_t<
		kotek::core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>&
	GetCommands(void) const noexcept;

	/// @brief \~english called by commands when the ecs backend
	/// hands them a different entity id than the recorded one
	/// (entity id recycling / re-execution); updates the id
	/// translation table and the live pool
	void update_dependent_commands(
		kotek::entity_t id_what_will_be_deleted,
		kotek::entity_t id_that_replaces_what_will_be_deleted
	) noexcept;

	/// @brief \~english returns a pool slot for placement-new of the
	/// next command; the oldest live command is evicted (its delta
	/// stays in the journal and can be reconstructed at any time)
	unsigned char* allocate_memory_for_command(
		kotek::size_t size_of_class,
		const char* p_debug_type_name
	) noexcept;

	/// @brief \~english the node id the cursor points at
	kotek::size_t get_current_index(void) const;
	kotek::ptrdiff_t get_cursor_index(void) const;

	/// @brief \~english total amount of recorded commands (tree
	/// nodes minus the root sentinel)
	kotek::uint64_t get_total_recorded_commands(void
	) const noexcept;

	kotek::uint32_t get_cursor_node_id(void) const noexcept;

	/// @brief \~english the currently live entity id for a recorded
	/// one (resolves the reincarnation chain); returns the
	/// recorded id itself when the entity was never rebound
	kotek::entity_t get_live_entity_id(
		kotek::entity_t recorded_id
	) const noexcept;

	/// @brief \~english the highest entity id the history has ever
	/// observed (used to bound world scans by tests and snapshots)
	kotek::uint32_t get_entity_watermark(void) const noexcept;

	/// @brief \~english restores the world to an arbitrary history
	/// node: nearest snapshot on the node's path + journal replay
	/// forward (or plain replay from the root when the path has no
	/// snapshot). Nothing is deleted from the history itself.
	bool restore_node(kotek::uint32_t node_id) noexcept;

	/// @brief \~english returns if component was serialized for id
	/// otherwise value will be empty; scans the current path for
	/// the nearest delete-entity entry that contains the component
	bool get_serialized_component_by_entity_and_component_type_id(
		kotek::ktk::json::value&
			constructed_value_on_stack_based_on_placement_new_memory,
		kotek::entity_t id,
		eZirconComponentType type_id
	);

	/// @brief \~english tunables and disk statistics (tests, budget
	/// checks)
	void set_snapshot_interval(
		kotek::uint32_t interval
	) noexcept;
	kotek::uint32_t get_snapshot_interval(void) const noexcept;
	kotek::uint64_t get_journal_file_size(void) noexcept;
	kotek::uint64_t get_snapshot_file_size(void) noexcept;
	kotek::uint64_t get_journal_raw_entry_bytes(void) const noexcept;
	kotek::uint64_t get_total_snapshot_count(void) const noexcept;

	/// @brief \~english the absolute path of this scene's streaming
	/// folder (data_user/sdk/scenes/<name>/): the journal pair and the
	/// scene.json level-metadata sibling (task Z3 P2h) live there.
	/// Empty before initialize; the game boot's pass-set resolution
	/// reads scene.json through this path
	const char* get_streaming_folder_path(void) const noexcept;

private:
	/// @brief \~english the command object for a node: live pool
	/// hit or reconstruction from the journal into the scratch slot
	kotek::core::ktkISDKRedoUndo* get_command_for_node(
		kotek::uint32_t node_id
	) noexcept;

	kotek::core::ktkISDKRedoUndo* reconstruct_command(
		kotek::uint32_t node_id
	) noexcept;

	/// @brief \~english destructs the scratch command when it was
	/// used for the given pointer
	void release_scratch_command(
		kotek::core::ktkISDKRedoUndo* p_command
	) noexcept;

	/// @brief \~english shared execute step of Redo/restore_node:
	/// translates the entity id, executes, observes id changes
	void execute_node(kotek::uint32_t node_id) noexcept;

	kotek::uint32_t translate_entity_id(
		kotek::uint32_t recorded_id
	) const noexcept;

	void set_entity_translation(
		kotek::uint32_t recorded_id, kotek::uint32_t live_id
	) noexcept;

	/// @brief \~english records that the entity that lived as
	/// live_id was reincarnated as next_id (undo of delete-entity,
	/// redo of create-entity, snapshot restore)
	void set_entity_reincarnation(
		kotek::uint32_t live_id, kotek::uint32_t next_id
	) noexcept;

	/// @brief \~english records the live id of a command after
	/// (re)application into the translation table
	void observe_entity_id(
		kotek::core::ktkISDKRedoUndo* p_command,
		kotek::uint32_t recorded_id
	) noexcept;

	void evict_pool_slot(kotek::uint32_t slot_index) noexcept;

	void take_snapshot_if_needed(kotek::uint32_t node_id) noexcept;

	zircon_world* get_current_world(void) noexcept;

	/// @brief \~english binary world state: [u32 entity_count] then
	/// per entity [u32 live_id][u32 component_count][u32 type][u32
	/// json_size][json]..., entities ordered by live id
	void capture_world_state(
		zircon_world* p_world,
		kotek::hybrid_vector_t<unsigned char, 4096>& output
	) noexcept;

	bool apply_world_state(
		zircon_world* p_world,
		const unsigned char* p_data,
		kotek::uint32_t data_size
	) noexcept;

	void destroy_all_entities(zircon_world* p_world) noexcept;

private:
	bool m_is_changed;
	bool m_is_scratch_in_use;
	zircon_session_editor_manager* m_p_manager_session_editor;
	kotek::Core::ktkIFileSystem* m_p_filesystem;

	kotek::uint32_t m_cursor_node_id;
	kotek::uint32_t m_snapshot_interval;
	kotek::uint32_t m_entity_watermark;
	kotek::uint64_t m_total_snapshot_count;

	/// @brief \~english node storage, index == node id, node 0 is
	/// the root sentinel
	kotek::hybrid_vector_t<
		zircon_history_tree_node,
		zircon_DEF_COMMAND_HISTORY_NODES_INLINE_COUNT>
		m_nodes;

	/// @brief \~english flat id translation table indexed by the
	/// recorded entity id: value is the last observed live id (a
	/// chain-start hint) or
	/// zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID
	kotek::hybrid_vector_t<
		kotek::uint32_t,
		zircon_DEF_COMMAND_HISTORY_ID_MAP_INLINE_COUNT>
		m_entity_id_translation;

	/// @brief \~english reincarnation chain indexed by an
	/// incarnation id: value is the id the entity was recreated as
	/// (or zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID when the
	/// entity was never reincarnated). Values strictly increase,
	/// so chain walks always terminate at the latest incarnation
	kotek::hybrid_vector_t<
		kotek::uint32_t,
		zircon_DEF_COMMAND_HISTORY_ID_MAP_INLINE_COUNT>
		m_entity_reincarnation;

	zircon_command_journal m_journal;

	kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
		m_path_to_streaming_folder;

	/// @brief \~english live command pool: ring of the most recent
	/// commands with fixed slots for placement-new
	kotek::array_t<
		kotek::core::ktkISDKRedoUndo*,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_pool_commands;
	kotek::array_t<
		kotek::uint32_t,
		zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE>
		m_pool_node_ids;
	kotek::uint32_t m_pool_next_victim_slot;
	kotek::uint32_t m_pending_slot;
	unsigned char m_pool_storage
		[zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE]
		[zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE];

	/// @brief \~english one scratch slot for journal reconstruction
	unsigned char
		m_scratch_storage[zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE];

	/// @brief \~english reused buffer for delta (de)serialization
	unsigned char
		m_payload_scratch[zircon_DEF_MAXIMUM_COMMAND_SIZE];
};
