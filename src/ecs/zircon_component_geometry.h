#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_geometry
	: public zircon_component_interface
{
public:
	zircon_component_geometry(void);
	~zircon_component_geometry(void);

	kotek::uint8_t get_component_type(void
	) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	kotek::size_t get_vertex_count(void) const noexcept;
	void set_vertex_count(kotek::size_t count) noexcept;

	kotek::size_t get_index_count(void) const noexcept;
	void set_index_count(kotek::size_t count) noexcept;

	const char* get_path(void) const noexcept;
	void set_path(const kotek::cstring_t& path) noexcept;

	bool is_visible(void) const noexcept;
	void set_visible(bool status) noexcept;

	kotek::size_t get_render_vertex_buffer_offset(void
	) const noexcept;
	kotek::size_t get_render_index_buffer_offset(void
	) const noexcept;

	void set_render_vertex_buffer_offset(kotek::size_t offset
	) noexcept;
	void set_render_index_buffer_offset(kotek::size_t offset
	) noexcept;

	// make things better just using universal approach and not
	// using direct handles of any GAPI that's why it is uint8_t
	// (not like uint32_t in GL or VK or DX)
	kotek::uint8_t get_render_vertex_buffer_id(void
	) const noexcept;
	kotek::uint8_t get_render_index_buffer_id(void
	) const noexcept;

	kotek::core::eStaticGeometryType
	get_geometry_type() const noexcept;
	void set_geometry_type(kotek::core::eStaticGeometryType type
	) noexcept;

private:
	bool m_is_enabled;
	bool m_is_use_model;
	bool m_is_visible;
	kotek::uint8_t m_render_internal_vertex_buffer_id;
	kotek::uint8_t m_render_internal_index_buffer_id;

	kotek::core::eStaticGeometryType m_geometry_type;
	const char* m_p_geometry_name;
	kotek::size_t m_render_internal_vertex_buffer_offset;
	kotek::size_t m_render_internal_index_buffer_offset;

	// making for release much much tight for memory
	// release means without editor
#ifdef KOTEK_USE_SDK_IMGUI
	kotek::size_t m_vertex_count;
	kotek::size_t m_index_count;
	kotek::cstring_t m_path;
#endif
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(
	const kotek::json::value_from_tag&,
	kotek::json::value& write_to,
	const zircon_component_geometry& data
)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[4096];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif

	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object geometry(&storage);

	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_IS_ENABLED] =
			data.is_enabled();
	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_IS_VISIBLE] =
			data.is_visible();
	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_GEOMETRY_TYPE] =
			static_cast<kotek::enum_base_t>(
				data.get_geometry_type()
			);

	#ifdef KOTEK_USE_SDK_IMGUI
	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_VERTEX_COUNT] =
			data.get_vertex_count();
	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_INDEX_COUNT] =
			data.get_index_count();
	geometry
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_PATH] =
			data.get_path();
	#endif

	write_to = geometry;
}

inline zircon_component_geometry tag_invoke(
	const kotek::json::value_to_tag<zircon_component_geometry>&,
	const kotek::json::value& read_from
)
{
	auto geometry = read_from.as_object();

	zircon_component_geometry result;

	result.set_enabled(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_IS_ENABLED
	        )
			.as_bool()
	);
	result.set_visible(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_IS_VISIBLE
	        )
			.as_bool()
	);
	result.set_geometry_type(static_cast<
							 kotek::core::eStaticGeometryType>(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_GEOMETRY_TYPE
	        )
			.to_number<kotek::enum_base_t>()
	));

	#ifdef KOTEK_USE_SDK_IMGUI
	result.set_vertex_count(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_VERTEX_COUNT
	        )
			.to_number<kotek::size_t>()
	);
	result.set_index_count(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_INDEX_COUNT
	        )
			.to_number<kotek::size_t>()
	);
	result.set_path(
		geometry
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_GEOMETRY_FIELD_M_PATH
	        )
			.as_string()
			.c_str()
	);
	#endif

	return result;
}
#endif
