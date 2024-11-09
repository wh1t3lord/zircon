bool zircon_factory_game::create_component(entt::entity id,
	zircon_component_type_t component_type_id,
	kotek::ktk::json::value& serialized_component)
{
	bool result{};

	if (this->IsValidEntity(id))
	{
		switch (component_type_id)
		{

		case zircon_component_type_t::kComponentTypezircon_component_camera:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_camera>(id) == false, "supposed that entity doesn't have a such component zircon_component_camera at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_camera>(id);

			zircon_component_camera& component = this->GetComponent<zircon_component_camera>(id);

			component = kotek::ktk::json::value_to<zircon_component_camera>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_input:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_input>(id) == false, "supposed that entity doesn't have a such component zircon_component_input at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_input>(id);

			zircon_component_input& component = this->GetComponent<zircon_component_input>(id);

			component = kotek::ktk::json::value_to<zircon_component_input>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_transform:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_transform>(id) == false, "supposed that entity doesn't have a such component zircon_component_transform at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_transform>(id);

			zircon_component_transform& component = this->GetComponent<zircon_component_transform>(id);

			component = kotek::ktk::json::value_to<zircon_component_transform>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_visibility:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_visibility>(id) == false, "supposed that entity doesn't have a such component zircon_component_visibility at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_visibility>(id);

			zircon_component_visibility& component = this->GetComponent<zircon_component_visibility>(id);

			component = kotek::ktk::json::value_to<zircon_component_visibility>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_geometry:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_geometry>(id) == false, "supposed that entity doesn't have a such component zircon_component_geometry at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_geometry>(id);

			zircon_component_geometry& component = this->GetComponent<zircon_component_geometry>(id);

			component = kotek::ktk::json::value_to<zircon_component_geometry>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_model:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_model>(id) == false, "supposed that entity doesn't have a such component zircon_component_model at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_model>(id);

			zircon_component_model& component = this->GetComponent<zircon_component_model>(id);

			component = kotek::ktk::json::value_to<zircon_component_model>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_actor:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_actor>(id) == false, "supposed that entity doesn't have a such component zircon_component_actor at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_actor>(id);

			zircon_component_actor& component = this->GetComponent<zircon_component_actor>(id);

			component = kotek::ktk::json::value_to<zircon_component_actor>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_terrain:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_terrain>(id) == false, "supposed that entity doesn't have a such component zircon_component_terrain at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_terrain>(id);

			zircon_component_terrain& component = this->GetComponent<zircon_component_terrain>(id);

			component = kotek::ktk::json::value_to<zircon_component_terrain>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_terrain_impl_cbt:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_terrain_impl_cbt>(id) == false, "supposed that entity doesn't have a such component zircon_component_terrain_impl_cbt at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_terrain_impl_cbt>(id);

			zircon_component_terrain_impl_cbt& component = this->GetComponent<zircon_component_terrain_impl_cbt>(id);

			component = kotek::ktk::json::value_to<zircon_component_terrain_impl_cbt>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_frustum:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_frustum>(id) == false, "supposed that entity doesn't have a such component zircon_component_frustum at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_frustum>(id);

			zircon_component_frustum& component = this->GetComponent<zircon_component_frustum>(id);

			component = kotek::ktk::json::value_to<zircon_component_frustum>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_bounding_sphere:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_bounding_sphere>(id) == false, "supposed that entity doesn't have a such component zircon_component_bounding_sphere at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_bounding_sphere>(id);

			zircon_component_bounding_sphere& component = this->GetComponent<zircon_component_bounding_sphere>(id);

			component = kotek::ktk::json::value_to<zircon_component_bounding_sphere>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_ui_surface:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_ui_surface>(id) == false, "supposed that entity doesn't have a such component zircon_component_ui_surface at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_ui_surface>(id);

			zircon_component_ui_surface& component = this->GetComponent<zircon_component_ui_surface>(id);

			component = kotek::ktk::json::value_to<zircon_component_ui_surface>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_ui_camera:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_ui_camera>(id) == false, "supposed that entity doesn't have a such component zircon_component_ui_camera at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_ui_camera>(id);

			zircon_component_ui_camera& component = this->GetComponent<zircon_component_ui_camera>(id);

			component = kotek::ktk::json::value_to<zircon_component_ui_camera>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_sdk_scene_name:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_sdk_scene_name>(id) == false, "supposed that entity doesn't have a such component zircon_component_sdk_scene_name at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_sdk_scene_name>(id);

			zircon_component_sdk_scene_name& component = this->GetComponent<zircon_component_sdk_scene_name>(id);

			component = kotek::ktk::json::value_to<zircon_component_sdk_scene_name>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_sdk_camera:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_sdk_camera>(id) == false, "supposed that entity doesn't have a such component zircon_component_sdk_camera at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_sdk_camera>(id);

			zircon_component_sdk_camera& component = this->GetComponent<zircon_component_sdk_camera>(id);

			component = kotek::ktk::json::value_to<zircon_component_sdk_camera>(serialized_component);

			result = true;

			break;
		}
	
		case zircon_component_type_t::kComponentTypezircon_component_sdk_input:
		{
			KOTEK_ASSERT(this->HasComponent<zircon_component_sdk_input>(id) == false, "supposed that entity doesn't have a such component zircon_component_sdk_input at all! Are you sure you called a right method?");

			this->CreateComponent<zircon_component_sdk_input>(id);

			zircon_component_sdk_input& component = this->GetComponent<zircon_component_sdk_input>(id);

			component = kotek::ktk::json::value_to<zircon_component_sdk_input>(serialized_component);

			result = true;

			break;
		}
	
		}
	}

	return result;
}
