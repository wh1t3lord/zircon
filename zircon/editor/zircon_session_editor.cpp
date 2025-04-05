#include "zircon_session_editor.h"
#include "../world/zircon_world.h"
#include "../ecs/components/zircon_factory.h"

#include "commands/zircon_command_history.h"

constexpr kotek::uint8_t _kInvalidSessionID =
	std::numeric_limits<kotek::uint8_t>::max();

zircon_session_editor::zircon_session_editor(kotek::uint8_t id) :
	m_was_initialized{}, m_is_change_title_once_for_editing_status{}, m_id{id},
	m_p_world{}, m_p_main_manager{}, m_name{"not_inited"}
{
}

zircon_session_editor::zircon_session_editor(void) :
	m_was_initialized{}, m_is_change_title_once_for_editing_status{},
	m_id{_kInvalidSessionID}, m_p_world{}, m_p_main_manager{},
	m_name{"not_inited"}
{
}

zircon_session_editor::~zircon_session_editor(void)
{
	KOTEK_ASSERT(
		!this->m_was_initialized, "you forgot to call ::shutdown method!");
}

void zircon_session_editor::initialize(
	const kotek::static_cstring_t<ZIRCON_DEF_MAX_SESSION_NAME_LENGTH>&
		session_name,
	kotek::uint8_t id, zircon_world* p_current_world,
	kotek::core::ktkMainManager* p_main_manager,
	kotek::core::ktkConsole* p_console,
	kotek::core::ktkIFileSystem* p_filesystem,
	kotek::core::ktkIResourceManager* p_resource_manager)
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

	if (!this->m_was_initialized)
	{
		this->m_name = session_name;
		this->m_id = id;

		this->m_p_world = p_current_world;
		this->m_p_main_manager = p_main_manager;
		this->m_p_console = p_console;
		this->m_command_history_manager.initialize(p_filesystem,
			p_current_world, p_current_world->get_factory(),
			p_resource_manager);
		this->m_state.initialize(p_current_world->get_factory());

		this->m_was_initialized = true;
	}
}

void zircon_session_editor::shutdown(void)
{
	KOTEK_ASSERT(this->m_was_initialized,
		"don't call this method when you didn't initialize the instance of "
		"this class!");

	if (this->m_was_initialized)
	{
		if (this->m_p_world)
		{
			this->m_p_world->shutdown();
		}

		this->m_was_initialized = false;
	}
}

void zircon_session_editor::update(void)
{
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

	Kotek::Core::ktkFileText output(reinterpret_cast<const char*>(
		full_path_to_file.filename().u8string().c_str()));

	this->Serialize_Settings(output,
		reinterpret_cast<const char*>(
			full_path_to_file.stem().u8string().c_str()));
	this->Serialize_Entities(output);

	this->m_p_main_manager->GetResourceManager()->Get_ResourceSaver()->Save(
		copied, kotek::core::ktkResourceHandle(&output, true));
}

void zircon_session_editor::Serialize_Settings(Kotek::Core::ktkFileText& output,
	const Kotek::ktk::cstring& scenename) noexcept
{
	KOTEK_ASSERT(
		scenename.empty() == false, "you can't pass an empty scene name!");

	Kotek::ktk::json::object settings;

	settings["scene_name"] = scenename.c_str();

	output.Write("Settings", settings);
}

void zircon_session_editor::Deserialize_Settings(
	Kotek::Core::ktkFileText& input) noexcept
{
	if (input.IsKeyExist("Settings"))
	{
		auto settings = input.Get<Kotek::ktk::json::object>("Settings");

		if (settings.find("scene_name") != settings.end())
		{
			Kotek::ktk::cstring formatted = "[";
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

void zircon_session_editor::Serialize_Entities(
	Kotek::Core::ktkFileText& output) noexcept
{
	KOTEK_ASSERT(this->m_p_world->get_factory(), "early calling?");

	auto p_factory = this->m_p_world->get_factory();
	if (p_factory)
	{
		Kotek::ktk::json::array all_entities;

		const auto& entities = this->m_p_world->get_entities();
		for (auto id : entities)
		{
			Kotek::ktk::json::object entity;
			auto all_components = p_factory->GetAllComponentsOfEntity(id);

			for (const auto& [component_name,
					 serialized_component_to_native_json_value] :
				all_components)
			{
				entity[component_name.data()] =
					serialized_component_to_native_json_value;
			}

			all_entities.push_back(entity);
		}

		output.Write("Entities", all_entities);
	}
}

void zircon_session_editor::Deserialize_Entities(
	Kotek::Core::ktkFileText& input) noexcept
{
	if (input.IsKeyExist("Entities"))
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

				auto id = this->m_p_world->create_entity();

				this->m_p_world->get_factory()->CreateAllComponents(
					id, serialized_data);
			}
		}
	}
}

void zircon_session_editor::Deserialize(
	const kotek::static_path_t& full_path_to_file) noexcept
{
	KOTEK_ASSERT(full_path_to_file.empty() == false,
		"you can't send an empty path here");

	kotek::core::ktkFileText input;

	this->m_p_main_manager->GetResourceManager()->Get_ResourceLoader()->Load(
		full_path_to_file, kotek::core::ktkResourceHandle(&input, true));

	this->Deserialize_Entities(input);
	this->Deserialize_Settings(input);
}

void zircon_session_editor::update_component_input_sdk(void) noexcept
{
	auto* p_game_factory = this->m_p_world->get_factory();

	if (p_game_factory)
	{
		auto& registry = p_game_factory->GetRegistry();

		auto entities = registry.view<zircon_component_sdk_input>();

		if (!entities.empty())
		{
			auto id = entities[0];

			auto& component_input =
				entities.get<zircon_component_sdk_input>(id);

			auto& input = component_input.get_input();

			auto status = component_input.get_input().is_key_holding(
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
		auto& registry = p_game_factory->GetRegistry();

		auto entities = registry.view<zircon_component_sdk_camera>();

		KOTEK_ASSERT(
			entities.size() <= 1, "you must have only one editor camera");

		if (!entities.empty())
		{
			auto id = entities[0];

			if (p_game_factory->HasComponent<zircon_component_sdk_input>(id) &&
				p_game_factory->HasComponent<zircon_component_transform>(id))
			{
				const auto& component_input =
					p_game_factory->GetComponent<zircon_component_sdk_input>(
						static_cast<entt::entity>(id));

				const auto& input = component_input.get_input();

				auto& component_transform =
					p_game_factory->GetComponent<zircon_component_transform>(
						static_cast<entt::entity>(id));

				auto& component_camera =
					entities.get<zircon_component_sdk_camera>(id);
				auto& camera = component_camera.get_camera();

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
						kotek::ktk::math::cross(front,
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
				auto& component_camera =
					entities.get<zircon_component_sdk_camera>(id);

				if (!component_camera.is_initialized())
				{
					zircon_component_camera& camera =
						component_camera.get_camera();

					camera.set_pitch(0.0f);
					camera.set_yaw(-90.0f);

					component_camera.set_initialized(true);
				}

				zircon_component_camera& camera = component_camera.get_camera();

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
