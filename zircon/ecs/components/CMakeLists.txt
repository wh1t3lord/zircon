cmake_minimum_required(VERSION 3.19.3)

project(zircon.ecs.components VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(zircon_enum_file_name "zircon_ecs_auto_enum_components.h")
set(zircon_components_to_enums_file_name "zircon_factory_components_to_enums.cpp")
set(zircon_create_component_by_enum_file_name "zircon_factory_create_component.cpp")
set(zircon_register_lookuptable_component_enum_and_id_type "zircon_factory_register_lookuptable_component_enum_and_id_type.cpp")
set(zircon_get_component_name_by_enum "zircon_factory_get_component_name_by_enum.cpp")
set(zircon_register_components_game_and_sdk "zircon_factory_register_components_game_and_sdk.cpp")
set(zircon_create_component_by_name "zircon_factory_create_component_by_name.cpp")
set(zircon_validate_get_component_type_of_all_components "zircon_validate_get_component_type_of_all_components.cpp")
set(zircon_generate_header_component_fields "zircon_component_fields.h")
set(to_include_module_files 
	"zircon_ecs_components.h"
	"main_ecs_dll.cpp"
)

set(to_include_game_ecs_files
	"zircon_component_camera.cpp"
	"zircon_component_camera.h"
	"zircon_component_input.cpp"
	"zircon_component_input.h"
	"zircon_component_transform.cpp"
	"zircon_component_transform.h"
	"zircon_component_geometry.h"
	"zircon_component_geometry.cpp"
	"zircon_component_actor.h"
	"zircon_component_actor.cpp"
	"zircon_component_terrain_cbt.h"
	"zircon_component_terrain_cbt.cpp"
	"zircon_component_terrain_static.h"
	"zircon_component_terrain_static.cpp"
	"zircon_component_terrain_gcm.h"
	"zircon_component_terrain_gcm.cpp"
	"zircon_component_frustum.h"
	"zircon_component_frustum.cpp"
	"zircon_component_bounding_sphere.h"
	"zircon_component_bounding_sphere.cpp"
	"zircon_component_ui_surface.h"
	"zircon_component_ui_surface.cpp"
	"zircon_component_ui_camera.h"
	"zircon_component_ui_camera.cpp"
	"zircon_component_animation.h"
	"zircon_component_animation.cpp"
)

set(to_include_game_factory_files 
	"zircon_factory.h"
	"zircon_factory_definitions.h"
	"zircon_factory.cpp"
)

set(to_include_sdk_ecs_files
	"zircon_component_sdk_scene_name.h"
	"zircon_component_sdk_scene_name.cpp"
	"zircon_component_sdk_camera.h"
	"zircon_component_sdk_camera.cpp"
	"zircon_component_sdk_input.h"
	"zircon_component_sdk_input.cpp"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

file(WRITE ${zircon_enum_file_name} "#pragma once")
file(WRITE ${zircon_components_to_enums_file_name} "void zircon_factory_game::register_components_and_their_enums() 
{")
file(WRITE ${zircon_create_component_by_enum_file_name} "bool zircon_factory_game::create_component(entt::entity id,
	zircon_component_type_t component_type_id,
	kotek::ktk::json::value& serialized_component)
{
	bool result{};

	if (this->IsValidEntity(id))
	{
		switch (component_type_id)
		{
")
file(WRITE ${zircon_register_lookuptable_component_enum_and_id_type} "
void zircon_factory_game::register_lookuptable_component_enum_and_id_type()
{
")
file(WRITE ${zircon_get_component_name_by_enum} "
const char* zircon_factory_game::get_component_name_by_enum(zircon_component_type_t component_type) const noexcept {
	switch (component_type) {
")

file(WRITE ${zircon_register_components_game_and_sdk} "
void zircon_factory_game::register_components_game_and_sdk()
{
")

file(WRITE ${zircon_create_component_by_name} "
	auto hash_component_name = kotek::ktk::hash<kotek::ktk::cstring_view>{}(component_name);
	bool was_found=false;
")
file(WRITE ${zircon_validate_get_component_type_of_all_components} "
	void zircon_factory_game::validate_get_component_type_of_all_components() const
	{
	#ifdef KOTEK_DEBUG
")

file(APPEND ${zircon_enum_file_name} "
enum zircon_component_type_t {
")

set(filenames_lists)

foreach (filename IN LISTS to_include_game_ecs_files)
	get_filename_component(clear_filename ${filename} NAME_WE)
	list(APPEND filenames_list ${clear_filename})
endforeach()

foreach (filename IN LISTS to_include_sdk_ecs_files)
	get_filename_component(clear_filename ${filename} NAME_WE)
	list(APPEND filenames_list ${clear_filename})
endforeach()

list(REMOVE_DUPLICATES filenames_list)

foreach (filename IN LISTS filenames_list)
	file(APPEND ${zircon_enum_file_name} "
	kComponentType${filename},
	")
	file(APPEND ${zircon_components_to_enums_file_name} "
	this->m_component_name_to_component_type_id[${filename}::GetComponentName().c_str()] = zircon_component_type_t::kComponentType${filename};")
	file(APPEND ${zircon_components_to_enums_file_name} "
	this->m_component_type_id_to_component_name[zircon_component_type_t::kComponentType${filename}] = ${filename}::GetComponentName().c_str();")
	file(APPEND ${zircon_register_lookuptable_component_enum_and_id_type} "
	this->m_lookuptable_id_types_by_component_enum[zircon_component_type_t::kComponentType${filename}] = entt::type_hash<${filename}>::value();")
	file(APPEND ${zircon_create_component_by_enum_file_name} "
		case zircon_component_type_t::kComponentType${filename}:
		{
			KOTEK_ASSERT(this->HasComponent<${filename}>(id) == false, \"supposed that entity doesn't have a such component ${filename} at all! Are you sure you called a right method?\");

			this->CreateComponent<${filename}>(id);

			${filename}& component = this->GetComponent<${filename}>(id);

			component = kotek::ktk::json::value_to<${filename}>(serialized_component);

			result = true;

			break;
		}
	")
	file(APPEND ${zircon_get_component_name_by_enum} "
		case zircon_component_type_t::kComponentType${filename}:
		{
			return ${filename}::GetComponentName().c_str();
		}
	")

	file(APPEND ${zircon_register_components_game_and_sdk} "
		this->m_component_name_to_id[${filename}::GetComponentName().c_str()] = entt::type_hash<${filename}>::value();
	")

	file(APPEND ${zircon_create_component_by_name} "
		if (hash_component_name == ${filename}::GetComponentNameHash())
		{
			was_found = true;
			this->CreateComponent<${filename}>(id);
		}
	")
	file(APPEND ${zircon_validate_get_component_type_of_all_components} "
		{
			${filename} instance_${filename};
			KOTEK_ASSERT(instance_${filename}.get_component_type() == zircon_component_type_t::kComponentType${filename}, \"you didn't initialize this component type field correctly check your ctor please!\");
		}
	")
endforeach()



file(APPEND ${zircon_enum_file_name} "
	kComponentTypeUnknown
")
file(APPEND ${zircon_enum_file_name} "
};
")

file(APPEND ${zircon_components_to_enums_file_name} "
}
")
file(APPEND ${zircon_create_component_by_enum_file_name} "
		}
	}

	return result;
}
")

file(APPEND ${zircon_register_lookuptable_component_enum_and_id_type} "
}
")
file(APPEND ${zircon_get_component_name_by_enum} "
	default:
	{
		KOTEK_ASSERT(false, \"Unsupported component type, did you register that in your enum aka did you create files? Because enum is autogenerated by CMake\");
		return \"UNSUPPORTED_COMPONENT_ENUM\";
	}

}
}
")

file(APPEND ${zircon_register_components_game_and_sdk} "
}
")

file(APPEND ${zircon_create_component_by_name} "
	KOTEK_ASSERT(was_found,
		\"can't be you forgot to update this if \"
		\"statement (in case where you added a new \"
		\"component)\");
")

file(APPEND ${zircon_validate_get_component_type_of_all_components} "
#endif
}
")

execute_process(
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/component_fields_generator/generate_zircon.py --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/engine/kotek.core.defines_dependent.ecs/include/ --output ${CMAKE_CURRENT_SOURCE_DIR}/${zircon_generate_header_component_fields}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE generate_fields_component_header_result
    ERROR_VARIABLE error_output
)

if(NOT generate_fields_component_header_result EQUAL 0)
    message(FATAL_ERROR "Header generation failed:\n${error_output}\n${CMAKE_CURRENT_SOURCE_DIR}/${zircon_generate_header_component_fields}")
endif()

add_library(${PROJECT_NAME} STATIC
	${to_include_sdk_ecs_files}
	${to_include_game_ecs_files}
	${to_include_module_files}
	${to_include_game_factory_files}
	"zircon_component_interface.h"
	${zircon_enum_file_name}
	${zircon_generate_header_component_fields}
	## we don't include some files because they're used in project with #include directive so they compile with project implicitly
)


# Add custom command to generate header
add_custom_command(
    TARGET ${PROJECT_NAME}
	PRE_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/component_fields_generator/generate_zircon.py --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/engine/kotek.core.defines_dependent.ecs/include/ --output ${zircon_generate_header_component_fields}
#	COMMAND_ECHO STDOUT
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating zircon component fields header"
    VERBATIM
)
# TODO: add if when game.ktk builds as shared

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Game/ECS" FILES 
	${to_include_game_ecs_files}
	"zircon_component_interface.h"
	${zircon_enum_file_name}
)
source_group("Game/Factory" FILES ${to_include_game_factory_files})
source_group("SDK/ECS" FILES ${to_include_sdk_ecs_files})
source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)

