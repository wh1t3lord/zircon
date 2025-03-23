#pragma once

#include <kotek.core.math/include/kotek_core_math.h>
#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_transform : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_transform)

public:
	zircon_component_transform(void);
	~zircon_component_transform(void);

	const kotek::math::vec3f_t& get_position(void) const noexcept;
	kotek::math::vec3f_t& get_position(void) noexcept;
	void set_position(const kotek::math::vec3f_t& pos) noexcept;

	const kotek::math::vec3f_t& get_scale(void) const noexcept;
	void set_scale(const kotek::math::vec3f_t& scale) noexcept;

	const kotek::math::quatf_t& get_rotation(void) const noexcept;
	void set_rotation(const kotek::math::quatf_t& rot) noexcept;

	void DrawImGui(kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
	kotek::math::vec3f_t m_position;
	kotek::math::vec3f_t m_scale;
	kotek::math::quatf_t m_rotation;
};

constexpr const char* kComponentTransformSerializationField_Position =
	"m_position";
constexpr const char* kComponentTransformSerializationField_Scale = "m_scale";
constexpr const char* kComponentTransformSerializationField_Rotation =
	"m_rotation";

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_transform& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	info[kComponentTransformSerializationField_Position] =
		kotek::json::value_from(data.get_position());
	info[kComponentTransformSerializationField_Scale] =
		kotek::json::value_from(data.get_scale());
	info[kComponentTransformSerializationField_Rotation] =
		kotek::json::value_from(data.get_rotation());

	#ifdef KOTEK_DEBUG
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(info, data);
	#endif

	write_to = info;
}

inline zircon_component_transform tag_invoke(
	const kotek::json::value_to_tag<zircon_component_transform>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_transform result;

	result.SetEnabled(
		data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	result.set_position(kotek::json::value_to<kotek::math::vec3f_t>(
		data.at(kComponentTransformSerializationField_Position)));
	result.set_scale(kotek::json::value_to<kotek::math::vec3f_t>(
		data.at(kComponentTransformSerializationField_Scale)));
	result.set_rotation(kotek::json::value_to<kotek::math::quatf_t>(
		data.at(kComponentTransformSerializationField_Rotation)));

	return result;
}
#endif
