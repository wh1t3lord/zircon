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

class zircon_component_geometry : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_geometry,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_geometry(void);
	~zircon_component_geometry(void);

	void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	kotek::size_t get_vertex_count(void) const noexcept;
	void set_vertex_count(Kotek::size_t count) noexcept;

	kotek::size_t get_index_count(void) const noexcept;
	void set_index_count(Kotek::size_t count) noexcept;

	const char* get_path(void) const noexcept;
	void set_path(const kotek::cstring_t& path) noexcept;

	bool is_visible(void) const noexcept;
	void set_visible(bool status) noexcept;

	kotek::size_t get_render_vertex_buffer_offset(void) const noexcept;
	kotek::size_t get_render_index_buffer_offset(void) const noexcept;

	void set_render_vertex_buffer_offset(kotek::size_t offset) noexcept;
	void set_render_index_buffer_offset(kotek::size_t offset) noexcept;

	// make things better just using universal approach and not using direct
	// handles of any GAPI that's why it is uint8_t (not like uint32_t in GL or
	// VK or DX)
	kotek::uint8_t get_render_vertex_buffer_id(void) const noexcept;
	kotek::uint8_t get_render_index_buffer_id(void) const noexcept;

	kotek::core::eStaticGeometryType get_geometry_type() const noexcept;
	void set_geometry_type(kotek::core::eStaticGeometryType type) noexcept;

private:
	bool m_is_enabled;
	bool m_is_use_model;
	bool m_is_visible;
	kotek::uint8_t m_component_type;
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

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_geometry& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[4096];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif

	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object geometry(&storage);

	geometry[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	geometry["m_is_visible"] = data.is_visible();
	geometry["m_geometry_type"] =
		static_cast<kotek::enum_base_t>(data.get_geometry_type());

	#ifdef KOTEK_USE_SDK_IMGUI
	geometry["m_vertex_count"] = data.get_vertex_count();
	geometry["m_index_count"] = data.get_index_count();
	geometry["m_path"] = data.get_path();
	#endif

	#ifdef KOTEK_DEBUG
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(geometry, data);
	#endif

	write_to = geometry;
}

inline zircon_component_geometry tag_invoke(
	const kotek::json::value_to_tag<zircon_component_geometry>&,
	const kotek::json::value& read_from)
{
	auto geometry = read_from.as_object();

	zircon_component_geometry result;

	result.SetEnabled(
		geometry.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());
	result.set_visible(geometry.at("m_is_visible").as_bool());
	result.set_geometry_type(static_cast<kotek::core::eStaticGeometryType>(
		geometry.at("m_geometry_type").to_number<kotek::enum_base_t>()));

	#ifdef KOTEK_USE_SDK_IMGUI
	result.set_vertex_count(
		geometry.at("m_vertex_count").to_number<kotek::size_t>());
	result.set_index_count(
		geometry.at("m_index_count").to_number<kotek::size_t>());
	result.set_path(geometry.at("m_path").as_string().c_str());
	#endif

	return result;
}
#endif
