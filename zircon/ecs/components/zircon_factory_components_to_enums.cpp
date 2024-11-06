void zircon_factory_game::register_components_and_their_enums() 
{
	this->m_component_name_to_component_type_id[zircon_component_camera::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_camera;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_camera] = zircon_component_camera::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_input::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_input;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_input] = zircon_component_input::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_transform::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_transform;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_transform] = zircon_component_transform::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_visibility::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_visibility;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_visibility] = zircon_component_visibility::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_geometry::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_geometry;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_geometry] = zircon_component_geometry::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_model::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_model;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_model] = zircon_component_model::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_actor::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_actor;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_actor] = zircon_component_actor::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_terrain::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_terrain;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_terrain] = zircon_component_terrain::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_terrain_impl_cbt::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_terrain_impl_cbt;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_terrain_impl_cbt] = zircon_component_terrain_impl_cbt::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_frustum::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_frustum;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_frustum] = zircon_component_frustum::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_bounding_sphere::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_bounding_sphere;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_bounding_sphere] = zircon_component_bounding_sphere::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_ui_surface::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_ui_surface;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_ui_surface] = zircon_component_ui_surface::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_ui_camera::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_ui_camera;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_ui_camera] = zircon_component_ui_camera::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_sdk_scene_name::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_sdk_scene_name;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_sdk_scene_name] = zircon_component_sdk_scene_name::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_sdk_camera::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_sdk_camera;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_sdk_camera] = zircon_component_sdk_camera::GetComponentName().c_str();
	this->m_component_name_to_component_type_id[zircon_component_sdk_input::GetComponentName().c_str()] = zircon_component_type_t::kComponentTypezircon_component_sdk_input;
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentTypezircon_component_sdk_input] = zircon_component_sdk_input::GetComponentName().c_str();
}
