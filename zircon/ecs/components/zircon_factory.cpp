#include "zircon_factory.h"
#include "../../core/zircon_config.h"

zircon_factory_game::zircon_factory_game(void) : m_p_config{}
{
	this->register_components();
	this->register_components_restrictions();
}

zircon_factory_game::~zircon_factory_game(void) {}

void zircon_factory_game::Initialize(
	zircon_config* p_config, kn_kotek::kn_core::ktkConsole* p_console)
{
	KOTEK_ASSERT(
		p_config, "you can't pass an invalid instance of zircon_config!");
	KOTEK_ASSERT(
		p_console, "you can't pass an invalid instance of ktkConsole!");

	this->m_p_config = p_config;
	this->m_p_console = p_console;
}

void zircon_factory_game::Shutdown(void) {}

bool zircon_factory_game::HasRequiredComponentsForCreation(
	Kotek::ktk::entity_t id, entt::id_type component_hash_id) noexcept
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
							kFeature_AddRequiredComponents_Automatically);

					if (is_enabled)
					{
						this->m_p_console->Execute(
							static_cast<Kotek::ktk::enum_base_t>(
								Kotek::Core::eConsoleCommandIndex::
									kConsoleCommand_SDK_CreateComponentForEntity),
							{{this->m_component_id_to_name.at(hash_id)}, {id}});
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

bool zircon_factory_game::HasRequiredComponentsForCreation(
	Kotek::ktk::entity_t id, const Kotek::ktk::cstring& component_name) noexcept
{
	bool result{};

	if (this->m_component_name_to_id.find(component_name) ==
		this->m_component_name_to_id.end())
		return result;

	result = this->HasRequiredComponentsForCreation(
		id, this->m_component_name_to_id.at(component_name));

	return result;
}

void zircon_factory_game::register_components()
{
	this->register_components_game();
	this->register_components_sdk();

	for (const auto& [component_name, component_id] :
		this->m_component_name_to_id)
	{
		this->m_component_id_to_name[component_id] = component_name;
	}
}

void zircon_factory_game::register_components_restrictions()
{
	this->register_components_restrictions_game();
	this->register_components_restrictions_sdk();
	this->validate_components_restrictions();

	for (const auto& [component_name, list_hashes] :
		this->m_component_creation_restriction_by_component_name)
	{
		this->m_component_creation_restriction_by_hash
			[this->m_component_name_to_id.at(component_name)] = list_hashes;
	}
}

void zircon_factory_game::register_components_game()
{
	this->m_component_name_to_id[zircon_component_actor::GetComponentName()] =
		entt::type_hash<zircon_component_actor>::value();
	this->m_component_name_to_id[zircon_component_camera::GetComponentName()] =
		entt::type_hash<zircon_component_camera>::value();
	this->m_component_name_to_id
		[zircon_component_geometry::GetComponentName()] =
		entt::type_hash<zircon_component_geometry>::value();
	this->m_component_name_to_id[zircon_component_input::GetComponentName()] =
		entt::type_hash<zircon_component_input>::value();
	this->m_component_name_to_id
		[zircon_component_transform::GetComponentName()] =
		entt::type_hash<zircon_component_transform>::value();
	this->m_component_name_to_id
		[zircon_component_visibility::GetComponentName()] =
		entt::type_hash<zircon_component_visibility>::value();
	this->m_component_name_to_id
		[zircon_component_sdk_scene_name::GetComponentName()] =
		entt::type_hash<zircon_component_sdk_scene_name>::value();
	this->m_component_name_to_id
		[zircon_component_ui_camera::GetComponentName()] =
		entt::type_hash<zircon_component_ui_camera>::value();
	this->m_component_name_to_id
		[zircon_component_ui_surface::GetComponentName()] =
		entt::type_hash<zircon_component_ui_surface>::value();
	this->m_component_name_to_id[zircon_component_frustum::GetComponentName()] =
		entt::type_hash<zircon_component_frustum>::value();
	this->m_component_name_to_id
		[zircon_component_bounding_sphere::GetComponentName()] =
		entt::type_hash<zircon_component_bounding_sphere>::value();
}

void zircon_factory_game::register_components_sdk()
{
	this->m_component_name_to_id
		[zircon_component_sdk_camera::GetComponentName()] =
		entt::type_hash<zircon_component_sdk_camera>::value();
	this->m_component_name_to_id
		[zircon_component_sdk_input::GetComponentName()] =
		entt::type_hash<zircon_component_sdk_input>::value();
}

void zircon_factory_game::register_components_restrictions_game()
{
	KOTEK_ASSERT(this->m_component_name_to_id.empty() == false,
		"you must register components first");

	this->m_component_creation_restriction_by_component_name
		[zircon_component_ui_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_camera>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_input>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_frustum>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_transform>::value());
}

void zircon_factory_game::register_components_restrictions_sdk()
{
	KOTEK_ASSERT(this->m_component_name_to_id.empty() == false,
		"you must register components first");

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_sdk_input>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_frustum>::value());

	this->m_component_creation_restriction_by_component_name
		[zircon_component_sdk_camera::GetComponentName()]
			.push_back(entt::type_hash<zircon_component_transform>::value());
}

void zircon_factory_game::validate_components_restrictions()
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
