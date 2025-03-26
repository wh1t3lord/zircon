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
	KOTEK_COMPONENT(zircon_component_transform,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

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

	void draw_imgui(
		kotek::Core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

private:
	bool m_is_enabled;
	kotek::uint8_t m_component_type;
	kotek::math::vec3f_t m_position;
	kotek::math::vec3f_t m_scale;
	kotek::math::quatf_t m_rotation;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_transform& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	info[ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_POSITION] =
		kotek::json::value_from(data.get_position());
	info[ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_SCALE] =
		kotek::json::value_from(data.get_scale());
	info[ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_ROTATION] =
		kotek::json::value_from(data.get_rotation());

	#ifdef KOTEK_DEBUG
	info[ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = info;
}

inline zircon_component_transform tag_invoke(
	const kotek::json::value_to_tag<zircon_component_transform>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_transform result;

	result.set_enabled(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_IS_ENABLED)
			.as_bool());

	result.set_position(kotek::json::value_to<kotek::math::vec3f_t>(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_POSITION)));
	result.set_scale(kotek::json::value_to<kotek::math::vec3f_t>(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_SCALE)));
	result.set_rotation(kotek::json::value_to<kotek::math::quatf_t>(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_ROTATION)));

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_TRANSFORM_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?"
	);
	#endif

	return result;
}
#endif
