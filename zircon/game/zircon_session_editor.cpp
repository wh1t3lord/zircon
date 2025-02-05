#include "zircon_session_editor.h"
#include "zircon_scene.h"
#include "zircon_game_manager.h"
#include "../ecs/components/zircon_factory.h"

#include "zircon_command_history.h"

zircon_session_editor::zircon_session_editor(void) :
	m_is_change_title_once_for_editing_status{}, m_p_scene{},
	m_p_game_manager{}, m_p_main_manager{}
{
}

zircon_session_editor::~zircon_session_editor(void) {}

void zircon_session_editor::initialize(
	zircon_scene* p_current_scene, zircon_manager_game* p_game_manager)
{
	KOTEK_ASSERT(p_current_scene, "you can't pass an invalid scene");
	KOTEK_ASSERT(p_game_manager, "you can't pass an invalid game manager");

	this->m_p_scene = p_current_scene;
	this->m_p_game_manager = p_game_manager;
	this->m_p_main_manager = p_game_manager->GetMainManager();
}

void zircon_session_editor::shutdown(void)
{
	if (this->m_p_scene)
	{
		this->m_p_scene->Shutdown();
	}

	if (this->m_p_game_manager->GetCommandHistoryManager())
	{
		this->m_p_game_manager->GetCommandHistoryManager()->shutdown();
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

	auto* p_factory = this->m_p_game_manager->get_factory_game();
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

			this->m_p_game_manager->GetConsole()->Execute_Command(
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
	KOTEK_ASSERT(this->m_p_game_manager->get_factory_game(),
		"you must initialize factory here");

	auto p_factory = this->m_p_game_manager->get_factory_game();

	Kotek::ktk::json::array all_entities;

	const auto& entities = this->m_p_scene->GetEntities();
	for (auto id : entities)
	{
		Kotek::ktk::json::object entity;
		auto all_components = p_factory->GetAllComponentsOfEntity(id);

		for (const auto& [component_name,
				 serialized_component_to_native_json_value] : all_components)
		{
			entity[component_name.data()] =
				serialized_component_to_native_json_value;
		}

		all_entities.push_back(entity);
	}

	output.Write("Entities", all_entities);
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

				auto id = this->m_p_scene->CreateEntity();

				this->m_p_game_manager->get_factory_game()->CreateAllComponents(
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
	if (this->m_p_game_manager)
	{
		auto* p_game_factory = this->m_p_game_manager->get_factory_game();

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

				auto status = component_input.get_input().is_mouse_right_hold();

				if (status)
				{
					if (input.get_input_type() !=
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eInputType::kInputType_DisabledCursor))
					{
						input.set_input_type(
							static_cast<Kotek::ktk::enum_base_t>(Kotek::Core::
									eInputType::kInputType_DisabledCursor));

						auto* p_console = this->m_p_game_manager->GetConsole();

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
						input.set_input_type(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eInputType::kInputType_Cursor));

						auto* p_console = this->m_p_game_manager->GetConsole();

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
	auto* p_command_history =
		this->m_p_game_manager->GetCommandHistoryManager();

	if (p_command_history->is_changed() &&
		!this->m_is_change_title_once_for_editing_status)
	{
		this->m_p_game_manager->GetConsole()->Push_Command(
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
	if (this->m_p_game_manager)
	{
		auto* p_game_factory = this->m_p_game_manager->get_factory_game();

		if (p_game_factory)
		{
			auto& registry = p_game_factory->GetRegistry();

			auto entities = registry.view<zircon_component_sdk_camera>();

			KOTEK_ASSERT(
				entities.size() <= 1, "you must have only one editor camera");

			if (!entities.empty())
			{
				auto id = entities[0];

				auto& component_camera =
					entities.get<zircon_component_sdk_camera>(id);

				const auto& component_input =
					p_game_factory->GetComponent<zircon_component_sdk_input>(
						static_cast<entt::entity>(id));

				const auto& component_transform =
					p_game_factory->GetComponent<zircon_component_transform>(
						static_cast<entt::entity>(id));

				const auto& input = component_input.get_input();

				auto& camera = component_camera.get_camera();

				auto pitch = camera.get_pitch();
				auto yaw = camera.get_yaw();
				auto sens = camera.get_mouse_sensetivity();

				yaw += input.get_offset_mouse_position_x() * sens;
				pitch += input.get_offset_mouse_position_y() * sens;

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
			}
		}
	}
}
