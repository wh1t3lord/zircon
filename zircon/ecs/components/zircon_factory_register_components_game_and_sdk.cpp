
void zircon_factory_game::register_components_game_and_sdk()
{

		this->m_component_name_to_id[zircon_component_camera::GetComponentName().c_str()] = entt::type_hash<zircon_component_camera>::value();
	
		this->m_component_name_to_id[zircon_component_input::GetComponentName().c_str()] = entt::type_hash<zircon_component_input>::value();
	
		this->m_component_name_to_id[zircon_component_transform::GetComponentName().c_str()] = entt::type_hash<zircon_component_transform>::value();
	
		this->m_component_name_to_id[zircon_component_visibility::GetComponentName().c_str()] = entt::type_hash<zircon_component_visibility>::value();
	
		this->m_component_name_to_id[zircon_component_geometry::GetComponentName().c_str()] = entt::type_hash<zircon_component_geometry>::value();
	
		this->m_component_name_to_id[zircon_component_model::GetComponentName().c_str()] = entt::type_hash<zircon_component_model>::value();
	
		this->m_component_name_to_id[zircon_component_actor::GetComponentName().c_str()] = entt::type_hash<zircon_component_actor>::value();
	
		this->m_component_name_to_id[zircon_component_terrain::GetComponentName().c_str()] = entt::type_hash<zircon_component_terrain>::value();
	
		this->m_component_name_to_id[zircon_component_terrain_impl_cbt::GetComponentName().c_str()] = entt::type_hash<zircon_component_terrain_impl_cbt>::value();
	
		this->m_component_name_to_id[zircon_component_frustum::GetComponentName().c_str()] = entt::type_hash<zircon_component_frustum>::value();
	
		this->m_component_name_to_id[zircon_component_bounding_sphere::GetComponentName().c_str()] = entt::type_hash<zircon_component_bounding_sphere>::value();
	
		this->m_component_name_to_id[zircon_component_ui_surface::GetComponentName().c_str()] = entt::type_hash<zircon_component_ui_surface>::value();
	
		this->m_component_name_to_id[zircon_component_ui_camera::GetComponentName().c_str()] = entt::type_hash<zircon_component_ui_camera>::value();
	
		this->m_component_name_to_id[zircon_component_animation::GetComponentName().c_str()] = entt::type_hash<zircon_component_animation>::value();
	
		this->m_component_name_to_id[zircon_component_sdk_scene_name::GetComponentName().c_str()] = entt::type_hash<zircon_component_sdk_scene_name>::value();
	
		this->m_component_name_to_id[zircon_component_sdk_camera::GetComponentName().c_str()] = entt::type_hash<zircon_component_sdk_camera>::value();
	
		this->m_component_name_to_id[zircon_component_sdk_input::GetComponentName().c_str()] = entt::type_hash<zircon_component_sdk_input>::value();
	
}
