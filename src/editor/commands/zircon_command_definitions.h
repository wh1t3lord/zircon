#pragma once

#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON 4096
#define _zircon_FINAL_STR(x) #x
#define _zircon_STR(x) _zircon_FINAL_STR(x)
#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS \
	_zircon_STR(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON)
#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS \
	sizeof(_zircon_STR(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON)) - 1
#define zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY '|'
#define zircon_DEF_MAXIMUM_COMMAND_SIZE 65536

// amount of live command objects that the history keeps in RAM for
// instant undo/redo of the most recent actions; older commands are
// reconstructed from the journal on demand
#define zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE 64

// xxxxxx_y where x count of file on disk like 999999 means literally 999999
// numbers, but y means write line 10 interprets from 0 to 9 and thus only one
// symbol
#define zircon_DEF_MAX_FILENAME_LENGTH_FOR_STREAMING 8

#define zircon_DEF_STREAM_JSON_STACK_SIZE 16

#define ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE 512

// fixed byte size of one placement-new slot for a command instance,
// must be >= sizeof of the largest registered command class
// (zircon_command_registry validates it at registration time)
#define zircon_DEF_COMMAND_INSTANCE_STORAGE_SIZE 16384

// byte size of the json memory buffer inside zircon_command_delete_entity
// that stores component states of the deleted entity (delta payload),
// 64 components with small states fit into it; overflow is asserted
#define zircon_DEF_COMMAND_DELETE_ENTITY_STATE_BUFFER_SIZE 8192

// journal file: entries are appended into an in-memory block and the
// whole block is zstd-compressed to disk when it reaches this amount
// of entries (tunable, part of the journal file header)
#define zircon_DEF_COMMAND_HISTORY_JOURNAL_ENTRIES_PER_BLOCK 128

// a zstd-compressed snapshot of the world state is taken every N
// applied commands measured as depth of a node in the history tree
#define zircon_DEF_COMMAND_HISTORY_SNAPSHOT_INTERVAL 256

// inline capacities for hybrid containers of the history manager
#define zircon_DEF_COMMAND_HISTORY_NODES_INLINE_COUNT 1024
#define zircon_DEF_COMMAND_HISTORY_ID_MAP_INLINE_COUNT 4096
#define zircon_DEF_COMMAND_HISTORY_JOURNAL_BLOCK_INLINE_SIZE 32768
#define zircon_DEF_COMMAND_HISTORY_PATH_INLINE_COUNT 128

// journal and snapshot binary file identification (little endian)
#define zircon_DEF_COMMAND_JOURNAL_MAGIC 0x31524A5A // 'ZJR1'
#define zircon_DEF_COMMAND_SNAPSHOT_MAGIC 0x3152535A // 'ZSR1'
#define zircon_DEF_COMMAND_JOURNAL_VERSION 1
#define zircon_DEF_COMMAND_SNAPSHOT_VERSION 1

// root node id of the history tree (sentinel, holds no command)
#define zircon_DEF_COMMAND_HISTORY_ROOT_NODE_ID 0
#define zircon_DEF_COMMAND_HISTORY_INVALID_NODE_ID 0xFFFFFFFF
#define zircon_DEF_COMMAND_HISTORY_INVALID_POOL_SLOT 0xFFFFFFFF
#define zircon_DEF_COMMAND_HISTORY_INVALID_ENTITY_ID 0xFFFFFFFF

// zircon-local command type ids for journaled commands that have no
// eConsoleCommandIndex entry (they are not console commands, the
// registry only needs a unique uint32 key, the high bit keeps them
// clear of kotek's enum range)
#define zircon_DEF_COMMAND_TYPE_EDIT_COMPONENT_STATE 0x80000001

// serialization attribute names (the JSON representation). These compile
// in every config — JSON is the debug representation per the Z6 design but
// it is fully functional in release.
// TODO(zircon): provide the binary implementation of history command
// streaming — the release-optimized variant the old #error was guarding
#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME \
	"entity_id"
#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME \
	"command"
#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_ID_NAME \
	"component_type_id"
#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMPONENT_IDS_NAME \
	"component_type_ids"
