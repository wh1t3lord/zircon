
	auto hash_component_name = kotek::ktk::hash<kotek::ktk::cstring_view>{}(component_name);
	bool was_found=false;

		if (hash_component_name == zircon_component_camera::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_camera>(id);
		}
	
		if (hash_component_name == zircon_component_input::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_input>(id);
		}
	
		if (hash_component_name == zircon_component_transform::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_transform>(id);
		}
	
		if (hash_component_name == zircon_component_visibility::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_visibility>(id);
		}
	
		if (hash_component_name == zircon_component_geometry::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_geometry>(id);
		}
	
		if (hash_component_name == zircon_component_model::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_model>(id);
		}
	
		if (hash_component_name == zircon_component_actor::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_actor>(id);
		}
	
		if (hash_component_name == zircon_component_terrain::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_terrain>(id);
		}
	
		if (hash_component_name == zircon_component_terrain_impl_cbt::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_terrain_impl_cbt>(id);
		}
	
		if (hash_component_name == zircon_component_frustum::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_frustum>(id);
		}
	
		if (hash_component_name == zircon_component_bounding_sphere::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_bounding_sphere>(id);
		}
	
		if (hash_component_name == zircon_component_ui_surface::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_ui_surface>(id);
		}
	
		if (hash_component_name == zircon_component_ui_camera::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_ui_camera>(id);
		}
	
		if (hash_component_name == zircon_component_animation::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_animation>(id);
		}
	
		if (hash_component_name == zircon_component_sdk_scene_name::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_sdk_scene_name>(id);
		}
	
		if (hash_component_name == zircon_component_sdk_camera::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_sdk_camera>(id);
		}
	
		if (hash_component_name == zircon_component_sdk_input::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<zircon_component_sdk_input>(id);
		}
	
	KOTEK_ASSERT(was_found,
		"can't be you forgot to update this if "
		"statement (in case where you added a new "
		"component)");
