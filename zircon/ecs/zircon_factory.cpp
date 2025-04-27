#include "zircon_factory.h"
#include "../core/zircon_config.h"
#include "../editor/session/zircon_session_editor_manager.h"
#include "../game/session/zircon_session_game_manager.h"

zircon_factory::zircon_factory(void) : m_p_config{}, m_p_input{}
{
	this->register_components();
	this->register_components_restrictions();
	this->register_components_and_their_enums();
	this->register_lookuptable_component_enum_and_id_type();
	this->validate_get_component_type_of_all_components();
}

zircon_factory::~zircon_factory(void) {}

void zircon_factory::Initialize(zircon_config* p_config,
	zircon_session_game_manager* p_manager_session_game,
	zircon_session_editor_manager* p_manager_session_editor,
	kotek::core::ktkConsole* p_console, kotek::core::ktkIInput* p_input)
{
	KOTEK_ASSERT(
		p_config, "you can't pass an invalid instance of zircon_config!");
	KOTEK_ASSERT(
		p_console, "you can't pass an invalid instance of ktkConsole!");
	KOTEK_ASSERT(p_input, "you can't pass an invalid instance of ktkIInput!");
	KOTEK_ASSERT(p_manager_session_game,
		"you can't pass a invalid pointer of session game manager");
	KOTEK_ASSERT(p_manager_session_editor,
		"you can't pass an invalid pointer of session editor manager");

	this->m_p_config = p_config;
	this->m_p_console = p_console;
	this->m_p_input = p_input;
	this->m_p_manager_session_editor = p_manager_session_editor;
	this->m_p_manager_session_game = p_manager_session_game;
}

void zircon_factory::Shutdown(void) {}

bool zircon_factory::HasRequiredComponentsForCreation(
	entt::entity id, entt::id_type component_hash_id) noexcept
{
	bool result{true};

	if (this->m_component_creation_restriction_by_hash.find(
			component_hash_id) !=
		this->m_component_creation_restriction_by_hash.end())
	{
		const auto& required_components =
			this->m_component_creation_restriction_by_hash.at(
				component_hash_id);

		for (auto hash_id : required_components)
		{
			if (!this->HasComponent(id, hash_id))
			{
				result = false;

				if (this->m_p_config)
				{
					bool is_enabled = this->m_p_config->is_feature_enabled(
						eZirconSDKFeatures::
							kSDK_Feature_AddRequiredComponents_Automatically);

					if (is_enabled)
					{
						this->m_p_console->Execute_Command(
							static_cast<kotek::enum_base_t>(
								kotek::core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CreateComponentForEntity),
							{{this->m_component_id_to_name.at(hash_id).data()},
								{static_cast<kotek::uint32_t>(id)}});
						result = is_enabled;

						continue;
					}
				}

				break;
			}
		}
	}

	return result;
}

bool zircon_factory::HasRequiredComponentsForCreation(entt::entity id,
	const kotek::static_cstring_view_t& component_name) noexcept
{
	bool result{};

	if (this->m_component_name_to_id.find(component_name) ==
		this->m_component_name_to_id.end())
		return result;

	result = this->HasRequiredComponentsForCreation(
		id, this->m_component_name_to_id.at(component_name));

	return result;
}

void zircon_factory::register_components()
{
	this->register_components_game_and_sdk();

	for (const auto& [component_name, component_id] :
		this->m_component_name_to_id)
	{
		this->m_component_id_to_name[component_id] = component_name;
	}
}

void zircon_factory::register_components_restrictions()
{
	this->register_components_restrictions_game();
	this->register_components_restrictions_sdk();
	this->validate_components_restrictions();

	for (const auto& [component_name, list_hashes] :
		this->m_component_creation_restriction_by_component_name)
	{
		this->m_component_creation_restriction_by_hash[this
				->m_component_name_to_id.at(component_name)] = list_hashes;
	}
}

void zircon_factory::register_components_restrictions_game()
{
	KOTEK_ASSERT(this->m_component_name_to_id.empty() == false,
		"you must register components first");

	this->m_component_creation_restriction_by_component_name
		[zircon_component_ui_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_camera>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_input>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_frustum>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_transform>::value());
}

void zircon_factory::register_components_restrictions_sdk()
{
	KOTEK_ASSERT(this->m_component_name_to_id.empty() == false,
		"you must register components first");

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_sdk_input>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_frustum>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName().c_str()]
			.push_back(entt::type_hash<zircon_component_transform>::value());
}

void zircon_factory::validate_components_restrictions()
{
#ifdef KOTEK_DEBUG
	for (const auto& [component_name, vector_ids] :
		this->m_component_creation_restriction_by_component_name)
	{
		auto myself_id = this->m_component_name_to_id.at(component_name);

		// check if I didn't register myself as restriction because
		// it could cause a recursive...
		for (const auto id : vector_ids)
		{
			KOTEK_ASSERT(id != myself_id,
				"you MUST NOT register component as restriction. "
				"You registered yourself as restriction and it is "
				"pointless...");
		}
	}
#endif
}

#include "zircon_factory_components_to_enums.cpp"
#include "zircon_factory_create_component.cpp"
#include "zircon_factory_register_lookuptable_component_enum_and_id_type.cpp"
#include "zircon_factory_get_component_name_by_enum.cpp"
#include "zircon_factory_register_components_game_and_sdk.cpp"
#include "zircon_validate_get_component_type_of_all_components.cpp"