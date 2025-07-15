#pragma once

#define ZIRCON_DEF_CONSOLE_CUSTOM_COMMANDS_ENUM(OP)                  \
	OP(app_close)                                                    \
	OP(app_hide)                                                     \
	OP(app_show)                                                     \
	OP(app_addtexttoexistedwindowtitle)                              \
	OP(app_resize)                                                   \
	OP(app_pause)                                                    \
	OP(input_type)                                                   \
	OP(render_uploadallresourcetogpu)                                \
	OP(render_calculateboudingprimitive)                             \
	OP(render_resize)                                                \
	OP(resourcemanager_load)                                         \
	OP(resourcemanager_openforwriting)                               \
	OP(resourcemanager_write)                                        \
	OP(resourcemanager_finishwriting)                                \
	OP(sdk_loadscene)                                                \
	OP(sdk_savescene)                                                \
	OP(sdk_redo)                                                     \
	OP(sdk_undo)                                                     \
	OP(sdk_closecurrentscene)                                        \
	OP(sdk_startsimulate)                                            \
	OP(sdk_stopsimulate)                                             \
	OP(sdk_selectentity)                                             \
	OP(sdk_deletecomponentfromentity)                                \
	OP(sdk_createcomponentfromentity)                                \
	OP(sdk_createentity)                                             \
	OP(sdk_deletenentity)                                            \
	OP(sdk_sendmessagetologwindow)                                   \
	OP(sdk_sendmessagewarningtologwindow)                            \
	OP(sdk_sendmessageerrortologwindow)                              \
	OP(sdk_sendmessageinfotologwindow)                               \
	OP(sdk_sendmessagegreytologwindow)                               \
	OP(sdk_showmodalwindow_saveandcloseorclosescene)                 \
	OP(sdk_setfeature_addcomponentstoentity_thatrequiredforcreation) \
	OP(sdk_showwindow)                                               \
	OP(sdk_hidewindow)                                               \
	OP(sdk_printregisteredwindows)                                   \
	OP(end_of_enum)                                                  \
	OP(set_current_game_session_for_engine)                          \
	OP(set_current_editor_session_for_engine)                        \
	OP(create_session)                                               \
	OP(destroy_session)                                              \
	OP(connect_to_session)                                           \
	OP(disconnect_from_session)                                      \
	OP(load_world)                                                   \
	OP(unload_world)                                                 \
	OP(create_world)                                                 \
	OP(destroy_world)                                                \
	OP(set_world_to_session)                                         \
	OP(set_current_render_graph_for_renderer)                        \
	OP(initialize_render_graph)                                      \
	OP(initialize_world)

enum class eZirconConsoleCommands : kotek::enum_base_t
{
#define _EZCC_GENERATE_ENUM(ENUM) ENUM,
	ZIRCON_DEF_CONSOLE_CUSTOM_COMMANDS_ENUM(_EZCC_GENERATE_ENUM)
#undef _EZCC_GENERATE_ENUM
};

inline constexpr auto zircon_user_console_translation_callback =
	[](std::string_view str) -> int
{
	const kotek::uint32_t hash = kotek::ktk::fnv1a_32(str);
	switch (hash)
	{
#define GENERATE_CASE(ENUM)                                        \
	case kotek::ktk::fnv1a_32(#ENUM):                              \
		if (str == #ENUM)                                          \
			return static_cast<int>(eZirconConsoleCommands::ENUM); \
		break;
		ZIRCON_DEF_CONSOLE_CUSTOM_COMMANDS_ENUM(GENERATE_CASE)
#undef GENERATE_CASE
	default:
		break;
	}

	KOTEK_ASSERT(false,
		"failed to match the inpput string with enum, did you break something "
		"in code? Input string: [{}]",
		str);

	return -1;
};
