
void zircon_factory_game::register_lookuptable_component_enum_and_id_type()
{

	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_camera] = entt::type_hash<zircon_component_camera>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_input] = entt::type_hash<zircon_component_input>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_transform] = entt::type_hash<zircon_component_transform>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_visibility] = entt::type_hash<zircon_component_visibility>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_geometry] = entt::type_hash<zircon_component_geometry>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_model] = entt::type_hash<zircon_component_model>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_actor] = entt::type_hash<zircon_component_actor>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_terrain] = entt::type_hash<zircon_component_terrain>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_terrain_impl_cbt] = entt::type_hash<zircon_component_terrain_impl_cbt>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_frustum] = entt::type_hash<zircon_component_frustum>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_bounding_sphere] = entt::type_hash<zircon_component_bounding_sphere>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_ui_surface] = entt::type_hash<zircon_component_ui_surface>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_ui_camera] = entt::type_hash<zircon_component_ui_camera>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_animation] = entt::type_hash<zircon_component_animation>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_sdk_scene_name] = entt::type_hash<zircon_component_sdk_scene_name>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_sdk_camera] = entt::type_hash<zircon_component_sdk_camera>::value();
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentTypezircon_component_sdk_input] = entt::type_hash<zircon_component_sdk_input>::value();
}
