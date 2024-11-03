#pragma once

#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON 4096
#define _zircon_FINAL_STR(x) #x
#define _zircon_STR(x) _zircon_FINAL_STR(x)
#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_HOW_MANY_SYMBOLS _zircon_STR(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON)
#define zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON_EXACT_DIGITS \
	sizeof(_zircon_STR(zircon_DEF_COMMAND_SDK_ENTITY_SIZE_JSON)) - 1
#define zircon_DEF_DEFAULT_SYMBOL_DELIMITER_WHEN_WRITE_SIZE_OF_ENTRY '|'
#define zircon_DEF_MAXIMUM_COMMAND_SIZE 65536
#define zircon_DEF_STREAMING_COMMAND_STORAGE_SIZE 10
#define zircon_DEF_MAXIMUM_ENTITY_COMPONENT_SIZE 256

// xxxxxx_y where x count of file on disk like 999999 means literally 999999
// numbers, but y means write line 10 interprets from 0 to 9 and thus only one
// symbol
#define zircon_DEF_MAX_FILENAME_LENGTH_FOR_STREAMING 8

#define zircon_DEF_MAX_COMPONENT_NAME_SIZE 128

#define zircon_DEF_STREAM_JSON_STACK_SIZE 16

#define ZIRCON_DEF_COMMAND_SDK_ENTITY_MAX_SERIALIZED_STRING_SIZE 512

#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_ENTITY_ID_NAME "entity_id"
#define ZIRCON_DEF_COMMAND_HISTORY_SERIALIZE_ATTRIBUTE_COMMAND_NAME "command"