cmake_minimum_required(VERSION 3.21)

project(zircon.ecs VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

set(zircon_enum_file_name "zircon_ecs_auto_enum_components.h")
set(zircon_create_component_by_enum_file_name "zircon_factory_create_component.cpp")
set(zircon_get_component_name_by_enum "zircon_factory_get_component_name_by_enum.cpp")
set(zircon_create_component_by_name "zircon_factory_create_component_by_name.cpp")
set(zircon_validate_get_component_type_of_all_components "zircon_validate_get_component_type_of_all_components.cpp")
set(zircon_serialize_deserialize_component "zircon_serialize_deserialize_component.cpp")
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

#find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

file(WRITE ${zircon_enum_file_name} "#pragma once")
file(WRITE ${zircon_create_component_by_enum_file_name} "
bool zircon_factory::create_component(
	zircon_ecs_context_t* p_context,
	kotek::entity_t id,
	eZirconComponentType component_type_id,
	kotek::ktk::json::value& serialized_component)
{
	bool result{};

	if (this->is_valid_entity(p_context, id))
	{
		switch (component_type_id)
		{
")
file(WRITE ${zircon_get_component_name_by_enum} "
const char* zircon_factory::get_component_name_by_enum(eZirconComponentType component_type) const noexcept {
	switch (component_type) {
")

file(WRITE ${zircon_create_component_by_name} "
	auto hash_component_name = kotek::ktk::hash<kotek::ktk::cstring_view>{}(component_name);
	bool was_found=false;
")
file(WRITE ${zircon_validate_get_component_type_of_all_components} "
	void zircon_factory::validate_get_component_type_of_all_components() const
	{
	#ifdef KOTEK_DEBUG
")

file(APPEND ${zircon_enum_file_name} "
enum eZirconComponentType {
")

file(WRITE ${zircon_serialize_deserialize_component}
"
void zircon_deserialize_component(
	const kotek::ktk::json::value& serialized_data,
	zircon_component_interface* p_result
) noexcept
{
	KOTEK_ASSERT(p_result, \"must be valid\");

	if (p_result)
	{
		switch (static_cast<eZirconComponentType>(p_result->get_component_type()))
		{
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
	k${filename},
	")

	file(APPEND ${zircon_serialize_deserialize_component}
	"
		case eZirconComponentType::k${filename}:
		{
			${filename}* p_casted = static_cast<${filename}*>(p_result);

			(*p_casted) = kotek::json::value_to<${filename}>(serialized_data);
		}
	")
	file(APPEND ${zircon_create_component_by_enum_file_name} "
		case eZirconComponentType::k${filename}:
		{
			KOTEK_ASSERT(this->has_component(p_context, id, eZirconComponentType::k${filename}) == false, \"supposed that entity doesn't have a such component ${filename} at all! Are you sure you called a right method?\");

			this->create_component(p_context, id, eZirconComponentType::k${filename});

			auto* p_interface = this->get_component_by_enum(p_context, id, eZirconComponentType::k${filename});
			KOTEK_ASSERT(p_interface, \"must return a valid component\");
			KOTEK_ASSERT(p_interface->get_component_type() == static_cast<kotek::uint8_t>(eZirconComponentType::k${filename}), \"impossible something is terribly wrong :O\");

			${filename}& component = static_cast<${filename}&>(*p_interface);

			component = kotek::ktk::json::value_to<${filename}>(serialized_component);

			result = true;

			break;
		}
	")
	file(APPEND ${zircon_get_component_name_by_enum} "
		case eZirconComponentType::k${filename}:
		{
			return \"${filename}\";
		}
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
			KOTEK_ASSERT(instance_${filename}.get_component_type() == eZirconComponentType::k${filename}, \"you didn't initialize this component type field correctly check your ctor please!\");
		}
	")
endforeach()

file(APPEND ${zircon_serialize_deserialize_component}
"
		default:
		{
			KOTEK_ASSERT(false, \"unregistered component type!\");
		}
		}
	}
}

kotek::ktk::json::value zircon_serialize_component(
	zircon_component_interface* p_data
) noexcept
{
	KOTEK_ASSERT(p_data, \"must be valid\");

	if (p_data)
	{
		switch (static_cast<eZirconComponentType>(p_data->get_component_type()))
		{
")

file(APPEND ${zircon_enum_file_name} "
	kunknown
")

file(APPEND ${zircon_enum_file_name} "
};
")

file(APPEND ${zircon_enum_file_name}
"
inline constexpr const char* zircon_translate_component_type_enum_to_string(eZirconComponentType type) 
{
	switch (type)
	{
")

foreach (filename IN LISTS filenames_list)
	file(APPEND ${zircon_enum_file_name}
	"
	case eZirconComponentType::k${filename}:
	{
		return \"${filename}\";
	}
	"
	)

	file(APPEND ${zircon_serialize_deserialize_component}
	"
	case eZirconComponentType::k${filename}:
	{
		${filename}* p_casted = reinterpret_cast<${filename}*>(p_data);
		return kotek::json::value_from(*p_casted);
	}
	")
endforeach()

file(APPEND ${zircon_serialize_deserialize_component}
"
		default:
		{
			KOTEK_ASSERT(false, \"unregistered component type!\");
			return kotek::json::value();
		}
		}
	}
	return kotek::json::value();
}

kotek::ktk::json::value zircon_serialize_component(
	zircon_component_interface* p_data,
	unsigned char* p_raw_memory,
	kotek::size_t memory_size
) noexcept
{
	KOTEK_ASSERT(p_data, \"must be valid\");

	if (p_data)
	{
		switch (static_cast<eZirconComponentType>(p_data->get_component_type()))
		{
")

foreach (filename IN LISTS filenames_list)
	file(APPEND ${zircon_serialize_deserialize_component}
	"
	case eZirconComponentType::k${filename}:
	{
		KOTEK_ASSERT(p_raw_memory, \"you passed an invalid part of memory!\");
		kotek::json::static_resource res(p_raw_memory, memory_size);
		kotek::json::storage_ptr ptr(&res);
		${filename}* p_casted = reinterpret_cast<${filename}*>(p_data);
		return kotek::json::value_from(*p_casted, ptr);
	}
	")
endforeach()

file(APPEND ${zircon_serialize_deserialize_component}
"
		default:
		{
			KOTEK_ASSERT(false, \"unregistered component type\");
			return kotek::json::value();
		}
		}
	}
	return kotek::json::value();
}
")

file(APPEND ${zircon_enum_file_name}
"
	case eZirconComponentType::kunknown:
	{
		return \"zircon_component_unknown\";
	}
	default:
	{
		return \"ZIRCON_COMPONENT_TYPE_UNDEFINED_ENUM\";
	}
	}
}
")

file(APPEND ${zircon_create_component_by_enum_file_name} "
		}
	}

	return result;
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

#execute_process(
#	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/component_fields_generator/generate_zircon.py --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/engine/kotek.core.defines_dependent.ecs/include/ --output ${CMAKE_CURRENT_SOURCE_DIR}/${zircon_generate_header_component_fields}
#    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#    RESULT_VARIABLE generate_fields_component_header_result
#    ERROR_VARIABLE error_output
#)

execute_process(
    COMMAND ${CMAKE_COMMAND} 
        -S ${CMAKE_SOURCE_DIR}/src/tools/cpp_generators/ecs_fields_generator
        -B ${CMAKE_BINARY_DIR}/zircon_tools
        -DKOTEK_DEPS_FOLDER=${KOTEK_DEPS_FOLDER}
        -DKOTEK_DEPS_FOLDER_ENABLE=${KOTEK_DEPS_FOLDER_ENABLE}
        -DKOTEK_ENGINE_ROOT_FOLDER_NAME=${KOTEK_ENGINE_ROOT_FOLDER_NAME}
        -DKOTEK_NUGET_PLATFORM_NAME=${KOTEK_NUGET_PLATFORM_NAME}
        -DKOTEK_NUGET_COMPILER_NAME=${KOTEK_NUGET_COMPILER_NAME}
        -DKOTEK_NUGET_ARCH_BIT_NUMBER=${KOTEK_NUGET_ARCH_BIT_NUMBER}
)



# Determine current build configuration
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (Visual Studio, Xcode, etc.)
    # We can't know the active config at configure time, so we'll use the first available
    list(GET CMAKE_CONFIGURATION_TYPES 0 current_config)
    message(STATUS "Multi-config generator detected, using first configuration: ${current_config}")
else()
    # Single-config generator (Make, Ninja, etc.)
    set(current_config "${CMAKE_BUILD_TYPE}")
    if(NOT current_config)
        set(current_config "Release")  # Default if not specified
    endif()
    message(STATUS "Build configuration: ${current_config}")
endif()

message(STATUS "building zircon_tools for current configuration: ${current_config}")


execute_process(
	COMMAND ${CMAKE_COMMAND}
		--build ${CMAKE_BINARY_DIR}/zircon_tools
		--config ${current_config}		
)

execute_process(
	COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/${current_config}/zircon_generator" --type 0 --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/src/kotek.core.defines_dependent.ecs/include/ --output ${CMAKE_CURRENT_SOURCE_DIR}/${zircon_generate_header_component_fields}
)

execute_process(
	COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/${current_config}/zircon_generator" --type 2 ${CMAKE_SOURCE_DIR}/src/render/bgfx/passes/no_streaming
)

kotek_add_library(${PROJECT_NAME}
	${to_include_sdk_ecs_files}
	${to_include_game_ecs_files}
	${to_include_module_files}
	${to_include_game_factory_files}
	"zircon_component_interface.h"
	"zircon_component_interface.cpp"
	${zircon_enum_file_name}
	${zircon_generate_header_component_fields}
	## we don't include some files because they're used in project with #include directive so they compile with project implicitly
)


# Add custom command to generate header

#add_custom_command(
#    TARGET ${PROJECT_NAME}
#	PRE_BUILD
#    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/component_fields_generator/generate_zircon.py --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/engine/kotek.core.defines_dependent.ecs/include/ --output ${zircon_generate_header_component_fields}
#	COMMAND_ECHO STDOUT
#    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#    COMMENT "Generating zircon component fields header"
#    VERBATIM
#)

#add_custom_command(
#	TARGET ${PROJECT_NAME}
#	PRE_BUILD 
#	COMMAND ${CMAKE_BINARY_DIR}
 #   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  #  COMMENT "Generating zircon component fields header"
  #  VERBATIM
#)

add_custom_command(
	TARGET ${PROJECT_NAME}
	PRE_BUILD 
	COMMAND ${CMAKE_COMMAND} 
		-S ${CMAKE_SOURCE_DIR}/src/tools/cpp_generators/ecs_fields_generator
		-B ${CMAKE_BINARY_DIR}/zircon_tools
		-DKOTEK_DEPS_FOLDER=${KOTEK_DEPS_FOLDER}
		-DKOTEK_DEPS_FOLDER_ENABLE=${KOTEK_DEPS_FOLDER_ENABLE}
		-DKOTEK_ENGINE_ROOT_FOLDER_NAME=${KOTEK_ENGINE_ROOT_FOLDER_NAME}
		-DKOTEK_NUGET_PLATFORM_NAME=${KOTEK_NUGET_PLATFORM_NAME}
		-DKOTEK_NUGET_COMPILER_NAME=${KOTEK_NUGET_COMPILER_NAME}
		-DKOTEK_NUGET_ARCH_BIT_NUMBER=${KOTEK_NUGET_ARCH_BIT_NUMBER}
	COMMAND ${CMAKE_COMMAND}
		--build ${CMAKE_BINARY_DIR}/zircon_tools
		--config $<CONFIG>
	COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/$<CONFIG>/zircon_generator" --type 0 --src ${CMAKE_CURRENT_SOURCE_DIR} --kotek_src ${CMAKE_CURRENT_SOURCE_DIR}/../../kotek/src/kotek.core.defines_dependent.ecs/include/ --output ${CMAKE_CURRENT_SOURCE_DIR}/${zircon_generate_header_component_fields}
    COMMENT "Building zircon_generator"
)

# TODO: add if when game.ktk builds as shared

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/ecs")

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

source_group("Game/Components" FILES 
	${to_include_game_ecs_files}
	"zircon_component_interface.h"
	"zircon_component_interface.cpp"
	${zircon_enum_file_name}
)
source_group("Game/Factory" FILES ${to_include_game_factory_files})
source_group("SDK/Components" FILES ${to_include_sdk_ecs_files})
source_group("Module" FILES ${to_include_module_files})

target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)