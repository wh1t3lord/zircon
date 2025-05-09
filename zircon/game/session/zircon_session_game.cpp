#include "zircon_session_game.h"
#include "../../world/zircon_world.h"
#include "../../core/zircon_console.h"

// todo: move to config please
// todo#2: create constexpr for render graph id invalid constant
constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_game::zircon_session_game(kotek::uint8_t id) :
#ifdef KOTEK_DEBUG
	m_was_allocated_by_manager{}, m_was_destroyed_by_manager{},
#endif
	m_was_initialized{}, m_was_render_graph_initialized{}, m_id{id},
	m_render_graph_id{static_cast<kotek::uint8_t>(-1)}, m_p_main_manager{},
	m_p_console{}, m_p_world{}, m_name{"not_inited"}
{
}

zircon_session_game::zircon_session_game(void) :
#ifdef KOTEK_DEBUG
	m_was_allocated_by_manager{}, m_was_destroyed_by_manager{},
#endif
	m_was_initialized{}, m_was_render_graph_initialized{},
	m_id{_kInvalidSessionID},
	m_render_graph_id{static_cast<kotek::uint8_t>(-1)}, m_p_main_manager{},
	m_p_console{}, m_p_world{}, m_name{"not_inited"}
{
}

zircon_session_game::~zircon_session_game(void)
{
	KOTEK_ASSERT(
		!this->m_was_initialized, "you forgot to call shutdown method");

#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(this->m_was_allocated_by_manager
			? this->m_was_destroyed_by_manager
			: !this->m_was_destroyed_by_manager,
		"if inited_by_manager was true it means this instance must be "
		"destroyed through calling from manager but you can't by yourself call "
		"a delete operator for destroying this instance.");
#endif
}

kotek::uint8_t zircon_session_game::get_id(void) const noexcept
{
	return this->m_id;
}

const char* zircon_session_game::get_session_name(void) const noexcept
{
	return this->m_name.c_str();
}

eZirconSessionType zircon_session_game::get_type(void) const noexcept
{
	return eZirconSessionType::kGame;
}

void zircon_session_game::shutdown(void)
{
	KOTEK_ASSERT(
		this->m_was_initialized, "call it only for initialized instance!");

	if (this->m_was_initialized)
	{
		this->m_was_initialized = false;
	}
}

void zircon_session_game::update(void)
{
#ifdef KOTEK_USE_SDK_IMGUI
	this->try_to_initialize_render_graph();
#endif
}

void zircon_session_game::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name,
	kotek::uint8_t id, kotek::core::ktkMainManager* p_main_manager,
	kotek::core::ktkConsole* p_console, zircon_world* p_current_world,
	bool is_need_to_initialize_render_graph)
{
	KOTEK_ASSERT(session_name.empty() == false,
		"you must pass a reasonable name please!");
	KOTEK_ASSERT(p_current_world, "must be a valid world!");
	KOTEK_ASSERT(this->m_id != _kInvalidSessionID ? this->m_id == id : true,
		"you must pass a same id as you passed in ctor. Otherwise it means "
		"that you won't obtain a right session by id. Default ctor was created "
		"for handling rare case where a user needs pure deferred "
		"initialization, but when it comes is unknown... So generally for "
		"validation of your ::initialize calling you have to construct this "
		"instance with ctor that accepts id in order to prevent some "
		"misclicking behaviour");
	KOTEK_ASSERT(p_current_world->get_factory(),
		"you must initialize factory inside of world!");
	KOTEK_ASSERT(this->m_was_initialized == false,
		"you should call shutdown and then initialize or reinitialize method "
		"for calling twice. "
		"Otherwise why do you need to call this method again?");
	KOTEK_ASSERT(p_main_manager, "you must pass a valid main manager!");
	KOTEK_ASSERT(p_console, "you must pass a valid console!");

	if (!this->m_was_initialized)
	{
		this->m_name = session_name;
		this->m_id = id;

		this->m_p_world = p_current_world;
		this->m_p_main_manager = p_main_manager;
		this->m_p_console = p_console;

		if (is_need_to_initialize_render_graph)
		{
			if (this->m_p_console)
			{
				this->m_p_console->Push_Command(
					static_cast<kotek::enum_base_t>(
						eZirconConsoleCommands::initialize_render_graph),
					{this->m_render_graph_id});

				// supposed that game session is 'optimized' session so it is
				// expected that you pass render graph for shipping aka release
				// so there's no point for implementing features like changing
				// render graphs and etc it is useful only for SDK development
				// cycle/build
#ifdef KOTEK_USE_SDK_IMGUI
				this->m_was_render_graph_initialized = true;
#endif
			}
		}

		this->m_was_initialized = true;
#ifdef KOTEK_DEBUG
		KOTEK_MESSAGE("initialized game session {} {} with world {}",
			session_name, id, p_current_world->get_id());
#endif
	}
}

kotek::uint8_t zircon_session_game::get_render_graph_id(void) const noexcept
{
	return this->m_render_graph_id;
}

void zircon_session_game::set_render_graph_id(kotek::uint8_t id) noexcept
{
	if (this->m_render_graph_id != id)
	{
		this->m_render_graph_id = id;
		this->m_was_render_graph_initialized = false;
	}
}

zircon_world* zircon_session_game::get_world(void) const noexcept
{
	return this->m_p_world;
}

void zircon_session_game::set_imgui_ui_elements(
	const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
	KOTEK_ASSERT(this->m_imgui_ui_elements.empty(), "already initialized!");
	this->m_imgui_ui_elements = imgui_elements;
}

const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
zircon_session_game::get_imgui_ui_elements(void) const noexcept
{
	return this->m_imgui_ui_elements;
}

void zircon_session_game::try_to_initialize_render_graph(void) noexcept
{
#ifdef KOTEK_USE_SDK_IMGUI
	if (!this->m_was_render_graph_initialized)
	{
		KOTEK_ASSERT(this->m_p_console,
			"supposed that you should initialize m_p_console field so early "
			"calling");

		if (this->m_p_console)
		{
			this->m_p_console->Push_Command(
				static_cast<kotek::enum_base_t>(
					eZirconConsoleCommands::initialize_render_graph),
				{this->m_render_graph_id});
		}

		this->m_was_render_graph_initialized = true;
	}
#endif
}
