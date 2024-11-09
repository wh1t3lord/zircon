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
	KOTEK_COMPONENT(zircon_component_geometry)

public:
	zircon_component_geometry(void);
	~zircon_component_geometry(void);

	Kotek::ktk::size_t GetVertexCount(void) const noexcept;
	void SetVertexCount(Kotek::ktk::size_t count) noexcept;

	Kotek::ktk::size_t GetIndexCount(void) const noexcept;
	void SetIndexCount(Kotek::ktk::size_t count) noexcept;

	const Kotek::ktk::cstring& GetPath(void) const noexcept;
	void SetPath(const Kotek::ktk::cstring& path) noexcept;

	bool is_visible(void) const noexcept;
	void set_visible(bool status) noexcept;

	void Clear(void) noexcept;
	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
	bool m_is_use_model;
	bool m_is_visible;
	Kotek::Core::eStaticGeometryType m_geometry_type;
	Kotek::ktk::size_t m_vertex_count;
	Kotek::ktk::size_t m_index_count;
	Kotek::ktk::cstring m_path;
	Kotek::ktk::cstring m_geometry_name;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_geometry& data)
{
	Kotek::ktk::json::object geometry;

	geometry[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	geometry["m_is_visible"] = data.is_visible();
	geometry["m_vertex_count"] = data.GetVertexCount();
	geometry["m_index_count"] = data.GetIndexCount();
	geometry["m_path"] = data.GetPath();
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(geometry, data);

	write_to = geometry;
}

inline zircon_component_geometry tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_geometry>&,
	const Kotek::ktk::json::value& read_from)
{
	auto geometry = read_from.as_object();

	zircon_component_geometry result;

	result.SetEnabled(geometry.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());
	result.set_visible(geometry.at("m_is_visible").as_bool());
	result.SetVertexCount(
		geometry.at("m_vertex_count").to_number<Kotek::ktk::size_t>());
	result.SetIndexCount(
		geometry.at("m_index_count").to_number<Kotek::ktk::size_t>());
	result.SetPath(geometry.at("m_path").as_string().c_str());

	return result;
}
#endif
