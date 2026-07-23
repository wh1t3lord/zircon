#include "zircon_session_editor.h"
#include "../../world/zircon_world.h"
#include "../../ecs/zircon_factory.h"
#include "../../core/zircon_console.h"
#include "../commands/zircon_command_history.h"

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_editor::zircon_session_editor(kotek::uint8_t id) :
#ifdef KOTEK_DEBUG
	m_was_allocated_by_manager{}, m_was_destroyed_by_manager{},
#endif
	m_was_render_graph_initialized{}, m_was_initialized{},
	m_is_change_title_once_for_editing_status{}, m_id{id},
	m_render_graph_id{static_cast<kotek::uint8_t>(-1)}, m_p_world{},
	m_p_main_manager{}, m_name{"not_inited"}
{
}

zircon_session_editor::zircon_session_editor(void) :
#ifdef KOTEK_DEBUG
	m_was_allocated_by_manager{}, m_was_destroyed_by_manager{},
#endif
	m_was_render_graph_initialized{}, m_was_initialized{},
	m_is_change_title_once_for_editing_status{}, m_id{_kInvalidSessionID},
	m_render_graph_id{static_cast<kotek::uint8_t>(-1)}, m_p_world{},
	m_p_main_manager{}, m_name{"not_inited"}
{
}

zircon_session_editor::~zircon_session_editor(void)
{
	KOTEK_ASSERT(
		!this->m_was_initialized, "you forgot to call ::shutdown method!");
	KOTEK_ASSERT(
		this->m_imgui_ui_elements.empty(), "you forgot to destroy elements!");

#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(this->m_was_allocated_by_manager
			? this->m_was_destroyed_by_manager
			: !this->m_was_destroyed_by_manager,
		"if inited_by_manager was true it means this instance must be "
		"destroyed through calling from manager but you can't by yourself call "
		"a delete operator for destroying this instance.");
#endif
}

void zircon_session_editor::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name,
	kotek::uint8_t id, zircon_world* p_current_world,
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::core::ktkMainManager* p_main_manager,
	kotek::core::ktkConsole* p_console,
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_history_streaming_folder_name)
{
	KOTEK_ASSERT(
		session_name.empty() == false, "pass a reasonable name please!");
	KOTEK_ASSERT(p_current_world, "you can't pass an invalid scene");
	KOTEK_ASSERT(p_current_world->get_factory(),
		"you must initialize factory inside of world!");
	KOTEK_ASSERT(p_main_manager, "must be valid!");
	KOTEK_ASSERT(p_console, "must be valid!");
	KOTEK_ASSERT(this->m_id != _kInvalidSessionID ? this->m_id == id : true,
		"you must pass a same id as you passed in ctor. Otherwise it means "
		"that you won't obtain a right session by id. Default ctor was created "
		"for handling rare case where a user needs pure deferred "
		"initialization, but when it comes is unknown... So generally for "
		"validation of your ::initialize calling you have to construct this "
		"instance with ctor that accepts id in order to prevent some "
		"misclicking behaviour");
	KOTEK_ASSERT(this->m_was_initialized == false,
		"you should call shutdown and then initialize or reinitialize method "
		"for calling twice. "
		"Otherwise why do you need to call this method again?");
	KOTEK_ASSERT(p_manager_session_editor,
		"you must pass a valid pointer of session editor manager!");

	if (!this->m_was_initialized)
	{
		this->m_name = session_name;
		this->m_id = id;

		this->m_p_world = p_current_world;
		this->m_p_main_manager = p_main_manager;
		this->m_p_console = p_console;
		this->m_command_history_manager.initialize(
			p_manager_session_editor, p_filesystem,
			p_history_streaming_folder_name);
		this->m_state.initialize(p_current_world->get_factory());

		this->m_was_initialized = true;
	}
}

void zircon_session_editor::shutdown(void)
{
	if (this->m_was_initialized)
	{
		this->m_command_history_manager.shutdown();
		for (kotek::core::ktkISDKUIElement* p_element :
			this->m_imgui_ui_elements)
		{
			delete p_element;
		}

		this->m_imgui_ui_elements.clear();
		this->m_p_world = nullptr;
		this->m_was_initialized = false;
	}
}

void zircon_session_editor::update(void)
{
	this->try_to_initialize_render_graph();

	if (this->m_p_main_manager)
	{
		auto* p_input = this->m_p_main_manager->Get_Input();

		if (p_input)
		{
			p_input->Update();
		}
	}

	this->update_editing_status();

	this->update_component_input_sdk();
	this->update_component_camera_sdk();
}

kotek::uint8_t zircon_session_editor::get_id(void) const noexcept
{
	return this->m_id;
}

eZirconSessionType zircon_session_editor::get_type(void) const noexcept
{
	return eZirconSessionType::kEditor;
}

const char* zircon_session_editor::get_session_name(void) const noexcept
{
	return this->m_name.c_str();
}

void zircon_session_editor::Serialize(
	const ktk_filesystem_path& full_path_to_file) noexcept
{
	KOTEK_ASSERT(full_path_to_file.empty() == false,
		"you can't send an empty path here");

	auto copied = full_path_to_file;

	if (copied.has_extension() == false)
	{
		copied.replace_extension("json");
	}
	KOTEK_ASSERT(false, "todo: re-write");
	/*
	Kotek::Core::ktkFileText output(reinterpret_cast<const char*>(
		full_path_to_file.filename().u8string().c_str()));*/

/*
	this->Serialize_Settings(output,
		reinterpret_cast<const char*>(
			full_path_to_file.stem().u8string().c_str()));
	this->Serialize_Entities(output);*/

/*
	this->m_p_main_manager->GetResourceManager()->Get_ResourceSaver()->Save(
		copied, kotek::core::ktkResourceHandle(&output, true));*/
}

void zircon_session_editor::Serialize_Settings(
	text_t& output,
	const Kotek::ktk::cstring& scenename) noexcept
{
	KOTEK_ASSERT(
		scenename.empty() == false, "you can't pass an empty scene name!");

	Kotek::ktk::json::object settings;

	settings["scene_name"] = scenename.c_str();

	output.Write("Settings", settings);
}

void zircon_session_editor::Deserialize_Settings(text_t& input
) noexcept
{
	if (input.Is_KeyExist("Settings"))
	{
		auto settings = input.Get<Kotek::ktk::json::object>("Settings");

		if (settings.find("scene_name") != settings.end())
		{
			kotek::static_cstring_t<1024> formatted = "[";
			formatted += settings.at("scene_name").as_string().c_str();
			formatted += "]";

			this->m_p_console->Execute_Command(
				static_cast<Kotek::ktk::enum_base_t>(
					Kotek::Core::eConsoleCommandIndex::
						kConsoleCommand_App_AddTextToExistedWindowTitle),
				{static_cast<Kotek::ktk::enum_base_t>(
					 Kotek::Core::eWindowTitleType::kTitle_CurrentSceneName),
					formatted});
		}
	}
}

void zircon_session_editor::Serialize_Entities(text_t& output
) noexcept
{
	KOTEK_ASSERT(this->m_p_world->get_factory(), "early calling?");

	auto p_factory = this->m_p_world->get_factory();
	if (p_factory)
	{
		Kotek::ktk::json::array all_entities;

		zircon_ecs_context_t* p_context =
			this->m_p_world->get_ecs_context();

		kotek::entity_t entities
			[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
		kotek::uint32_t entities_count =
			p_factory->get_all_entities(
				p_context,
				this->m_p_world->get_entity_count_max_limit(),
				entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
			);

		for (kotek::uint32_t entity_index = 0;
		     entity_index < entities_count; ++entity_index)
		{
			kotek::entity_t id = entities[entity_index];

			Kotek::ktk::json::object entity;

			kotek::static_vector_t<
				eZirconComponentType,
				zircon_DEF_MAXIMUM_ENTITY_COMPONENTS_COUNT>
				all_components;

			p_factory->get_all_components_of_entity(
				p_context, id, all_components
			);

			for (eZirconComponentType component_type :
			     all_components)
			{
				const char* component_name =
					p_factory->get_component_name_by_enum(
						component_type
					);

				zircon_component_interface* p_component =
					p_factory->get_component_by_enum(
						p_context, id, component_type
					);

				entity[component_name] =
					zircon_serialize_component(p_component);
			}

			all_entities.push_back(entity);
		}

		output.Write("Entities", all_entities);
	}
}

void zircon_session_editor::Deserialize_Entities(text_t& input
) noexcept
{
	if (input.Is_KeyExist("Entities"))
	{
		auto entities = input.Get<Kotek::ktk::json::array>("Entities");

		if (entities.empty() == false)
		{
			for (auto raw_data : entities)
			{
				auto entity = raw_data.as_object();

				Kotek::ktk::vector<Kotek::ktk::pair<Kotek::ktk::cstring,
					Kotek::ktk::json::value>>
					serialized_data;

				for (auto pair : entity)
				{
					serialized_data.push_back({pair.key_c_str(), pair.value()});
				}

				zircon_factory* p_factory =
					this->m_p_world->get_factory();

				KOTEK_ASSERT(
					p_factory, "world must contain factory!"
				);

				zircon_ecs_context_t* p_context =
					this->m_p_world->get_ecs_context();

				auto id = p_factory->create_entity(p_context);

				for (auto& [component_name, serialized_component] :
				     serialized_data)
				{
					p_factory->create_component(
						p_context,
						id,
						p_factory->get_component_enum_by_name(
							component_name.c_str()
						),
						serialized_component
					);
				}
			}
		}
	}
}

kotek::uint8_t zircon_session_editor::get_render_graph_id(void) const noexcept
{
	return this->m_render_graph_id;
}

void zircon_session_editor::set_render_graph_id(kotek::uint8_t id) noexcept
{
	if (this->m_render_graph_id != id)
	{
		this->m_render_graph_id = id;
		this->m_was_render_graph_initialized = false;
	}
}

zircon_editor_ui_state* zircon_session_editor::get_ui_state(void) noexcept
{
	return &this->m_state;
}

zircon_editor_command_history* zircon_session_editor::get_command_history(
	void) noexcept
{
	return &this->m_command_history_manager;
}

const zircon_editor_ui_state* zircon_session_editor::get_ui_state(
	void) const noexcept
{
	return &this->m_state;
}

const zircon_editor_command_history* zircon_session_editor::get_command_history(
	void) const noexcept
{
	return &this->m_command_history_manager;
}

zircon_world* zircon_session_editor::get_world(void) const noexcept
{
	return this->m_p_world;
}

void zircon_session_editor::set_imgui_ui_elements(
	const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
		imgui_elements) noexcept
{
	KOTEK_ASSERT(this->m_imgui_ui_elements.empty(), "already initialized!");
	this->m_imgui_ui_elements = imgui_elements;
}

const kotek::vector_t<kotek::core::ktkISDKUIElement*>&
zircon_session_editor::get_imgui_ui_elements(void) const noexcept
{
	return this->m_imgui_ui_elements;
}

void zircon_session_editor::Deserialize(
	const kotek::static_path_t& full_path_to_file) noexcept
{
	KOTEK_ASSERT(full_path_to_file.empty() == false,
		"you can't send an empty path here");

	text_t input;
	KOTEK_ASSERT(false, "todo: re-write please");

	/*
	this->m_p_main_manager->GetResourceManager()->Get_ResourceLoader()->Load(
		full_path_to_file, kotek::core::ktkResourceHandle(&input, true));*/

	this->Deserialize_Entities(input);
	this->Deserialize_Settings(input);
}

void zircon_session_editor::update_component_input_sdk(void) noexcept
{
	auto* p_game_factory = this->m_p_world->get_factory();

	if (p_game_factory)
	{
		zircon_ecs_context_t* p_context =
			this->m_p_world->get_ecs_context();

		// pico has no view<>, so scan entities for the (single)
		// sdk_input component owner
		kotek::entity_t entities
			[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
		kotek::uint32_t entities_count =
			p_game_factory->get_all_entities(
				p_context,
				this->m_p_world->get_entity_count_max_limit(),
				entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
			);

		kotek::entity_t id = kotek::ktk::kInvalidECSEntity;

		for (kotek::uint32_t entity_index = 0;
		     entity_index < entities_count; ++entity_index)
		{
			if (p_game_factory->has_component(
					p_context, entities[entity_index],
					eZirconComponentType::kzircon_component_sdk_input
				))
			{
				id = entities[entity_index];
				break;
			}
		}

		if (ecs_is_invalid_entity(id) == false)
		{
			auto* p_component_input = static_cast<
				zircon_component_sdk_input*>(
				p_game_factory->get_component_by_enum(
					p_context, id,
					eZirconComponentType::kzircon_component_sdk_input
				)
			);

			if (p_component_input)
			{
				auto& input = p_component_input->get_input();

				auto status = input.is_key_holding(
					kotek::core::eInputAllKeys::kCM_KEY_RIGHT);

				if (status)
				{
					if (input.get_input_type() !=
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eInputType::kInputType_DisabledCursor))
					{
						input.set_input_type(static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eInputType::kInputType_DisabledCursor));

						auto* p_console = this->m_p_console;

						if (p_console)
						{
							p_console->Push_Command(
								static_cast<Kotek::ktk::enum_base_t>(
									Kotek::Core::eConsoleCommandIndex::
										kConsoleCommand_Input_Type),
								{input.get_input_type()});
						}
					}
				}
				else
				{
					if (input.get_input_type() !=
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eInputType::kInputType_Cursor))
					{
						input.set_input_type(static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eInputType::kInputType_Cursor));

						auto* p_console = this->m_p_console;

						if (p_console)
						{
							p_console->Push_Command(
								static_cast<Kotek::ktk::enum_base_t>(
									Kotek::Core::eConsoleCommandIndex::
										kConsoleCommand_Input_Type),
								{input.get_input_type()});
						}
					}
				}
			}
		}
	}
}

void zircon_session_editor::update_editing_status(void) noexcept
{
	auto* p_command_history = &this->m_command_history_manager;

	if (p_command_history->is_changed() &&
		!this->m_is_change_title_once_for_editing_status)
	{
		this->m_p_console->Push_Command(
			static_cast<Kotek::ktk::enum_base_t>(
				Kotek::Core::eConsoleCommandIndex::
					kConsoleCommand_App_AddTextToExistedWindowTitle),
			{{static_cast<Kotek::ktk::enum_base_t>(
				 Kotek::Core::eWindowTitleType::kTitle_CurrentSceneEditStatus)},
				kotek::static_cstring_t<16>{"-- editing"}});

		this->m_is_change_title_once_for_editing_status = true;
	}

	if (p_command_history->is_changed() == false &&
		this->m_is_change_title_once_for_editing_status)
	{
		this->m_is_change_title_once_for_editing_status = false;
	}
}

void zircon_session_editor::update_component_camera(void) noexcept {}

void zircon_session_editor::update_component_camera_sdk(void) noexcept
{
	auto* p_game_factory = this->m_p_world->get_factory();

	if (p_game_factory)
	{
		zircon_ecs_context_t* p_context =
			this->m_p_world->get_ecs_context();

		// pico has no view<>, so scan entities for sdk_camera
		// owners (must be at most one editor camera)
		kotek::entity_t entities
			[ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT]{};
		kotek::uint32_t entities_count =
			p_game_factory->get_all_entities(
				p_context,
				this->m_p_world->get_entity_count_max_limit(),
				entities, ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT
			);

		kotek::entity_t id = kotek::ktk::kInvalidECSEntity;
		kotek::uint32_t sdk_camera_count{};

		for (kotek::uint32_t entity_index = 0;
		     entity_index < entities_count; ++entity_index)
		{
			if (p_game_factory->has_component(
					p_context, entities[entity_index],
					eZirconComponentType::kzircon_component_sdk_camera
				))
			{
				id = entities[entity_index];
				++sdk_camera_count;
			}
		}

		KOTEK_ASSERT(
			sdk_camera_count <= 1,
			"you must have only one editor camera"
		);

		if (ecs_is_invalid_entity(id) == false)
		{
			if (p_game_factory->has_component(
					p_context, id,
					eZirconComponentType::kzircon_component_sdk_input
				) &&
				p_game_factory->has_component(
					p_context, id,
					eZirconComponentType::kzircon_component_transform
				))
			{
				auto* p_component_input = static_cast<
					zircon_component_sdk_input*>(
					p_game_factory->get_component_by_enum(
						p_context, id,
						eZirconComponentType::
							kzircon_component_sdk_input
					)
				);

				KOTEK_ASSERT(
					p_component_input, "must be valid component"
				);

				const auto& input = p_component_input->get_input();

				auto* p_component_transform = static_cast<
					zircon_component_transform*>(
					p_game_factory->get_component_by_enum(
						p_context, id,
						eZirconComponentType::
							kzircon_component_transform
					)
				);

				KOTEK_ASSERT(
					p_component_transform, "must be valid component"
				);

				auto& component_transform = *p_component_transform;

				auto* p_component_camera = static_cast<
					zircon_component_sdk_camera*>(
					p_game_factory->get_component_by_enum(
						p_context, id,
						eZirconComponentType::
							kzircon_component_sdk_camera
					)
				);

				KOTEK_ASSERT(
					p_component_camera, "must be valid component"
				);

				auto& camera = p_component_camera->get_camera();

				auto pitch = camera.get_pitch();
				auto yaw = camera.get_yaw();

				if (input.is_key_holding(
						kotek::core::eInputAllKeys::kCM_KEY_RIGHT))
				{
					yaw += input.get_delta_x(kotek::core::eInputControllerType::
								   kControllerMouse) *
						input.get_sensetivity();
					pitch += input.get_delta_y(kotek::core::
									 eInputControllerType::kControllerMouse) *
						input.get_sensetivity();
				}

				if (pitch > 89.0f)
					pitch = 89.0f;

				if (pitch < -89.0f)
					pitch = -89.0f;

				camera.set_pitch(pitch);
				camera.set_yaw(yaw);

				Kotek::ktk::math::vec3f_t front;

				front.x() = cos(Kotek::ktk::math::convert_to_radians(yaw)) *
					cos(Kotek::ktk::math::convert_to_radians(pitch));
				front.y() = sin(Kotek::ktk::math::convert_to_radians(pitch));
				front.z() = sin(Kotek::ktk::math::convert_to_radians(yaw)) *
					cos(Kotek::ktk::math::convert_to_radians(pitch));
				auto height = this->m_p_main_manager->Get_WindowManager()
								  ->ActiveWindow_GetHeight();
				auto width = this->m_p_main_manager->Get_WindowManager()
								 ->ActiveWindow_GetWidth();

				// TODO: projection update only when we zoom
				camera.set_projection(Kotek::ktk::math::perspective(
					Kotek::ktk::math::convert_to_radians(
						camera.get_field_of_view()),
					width / height, camera.get_plane_near(),
					camera.get_plane_far()));
				camera.set_view(Kotek::ktk::math::look_at(
					component_transform.get_position(),
					component_transform.get_position() + front,
					{0.0f, 1.0f, 0.0f}));

				if (input.is_key_holding(
						kotek::core::eInputAllKeys::kCM_KEY_RIGHT))
				{
					float movement_speed = 0.1f;
					const kotek::ktk::math::vector3f& right =
						kotek::ktk::math::get_math_cross(front,
							kotek::ktk::math::vector3f(0.0f, 1.0f, 0.0f));

					if (input.is_key_holding(
							kotek::core::eInputAllKeys::kCK_KEY_A,
							ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES))
					{
						component_transform.get_position() -=
							right * movement_speed;
					}

					if (input.is_key_holding(
							kotek::core::eInputAllKeys::kCK_KEY_D,
							ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES))
					{
						component_transform.get_position() +=
							right * movement_speed;
					}

					if (input.is_key_holding(
							kotek::core::eInputAllKeys::kCK_KEY_W,
							ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES))
					{
						component_transform.get_position() +=
							front * movement_speed;
					}

					if (input.is_key_holding(
							kotek::core::eInputAllKeys::kCK_KEY_S,
							ZIRCON_DEF_INPUT_KEYBOARD_HOLDING_FRAMES))
					{
						component_transform.get_position() -=
							front * movement_speed;
					}
				}
			}
			else
			{
				auto* p_component_camera = static_cast<
					zircon_component_sdk_camera*>(
					p_game_factory->get_component_by_enum(
						p_context, id,
						eZirconComponentType::
							kzircon_component_sdk_camera
					)
				);

				KOTEK_ASSERT(
					p_component_camera, "must be valid component"
				);

				if (!p_component_camera->is_initialized())
				{
					zircon_component_camera& camera =
						p_component_camera->get_camera();

					camera.set_pitch(0.0f);
					camera.set_yaw(-90.0f);

					p_component_camera->set_initialized(true);
				}

				zircon_component_camera& camera =
					p_component_camera->get_camera();

				auto width = this->m_p_main_manager->Get_WindowManager()
								 ->ActiveWindow_GetWidth();
				auto height = this->m_p_main_manager->Get_WindowManager()
								  ->ActiveWindow_GetHeight();

				camera.set_projection(kotek::math::perspective(
					kotek::math::convert_to_radians(camera.get_field_of_view()),
					width / height, camera.get_plane_near(),
					camera.get_plane_far()));

				camera.set_view(kotek::ktk::math::look_at(
					kotek::math::vector3f(0.0f, 0.0f, 3.0f),
					kotek::math::vector3f(0.0f, 0.0f, -3.0f),
					kotek::math::vector3f(0.0f, 1.0f, 0.0f)));
			}
		}
	}
}

void zircon_session_editor::try_to_initialize_render_graph(void) noexcept
{
	if (!this->m_was_render_graph_initialized)
	{
		KOTEK_ASSERT(this->m_p_console,
			"console must be initialized for initializing render graph from "
			"session!");

		if (this->m_p_console)
		{
			this->m_p_console->Push_Command(
				static_cast<kotek::enum_base_t>(
					eZirconConsoleCommands::initialize_render_graph),
				{this->m_render_graph_id});
		}

		this->m_was_render_graph_initialized = true;
	}
}
