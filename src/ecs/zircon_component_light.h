#pragma once

// zircon_component_light — one light source per entity (task Z24 B5b, the
// clustered forward+ lighting phase of the approved renderer plan). The
// component is a pure DESCRIPTOR: the per-frame world collection
// (zircon_render_light_buffer, passes-side) turns the enabled instances
// into the GPU light table, the binning compute assigns them to
// screen-space tiles and the shared fragment path iterates the tile's
// list. A scene with ZERO enabled light components keeps the pre-B5b
// behavior exactly: the named-define directional of
// zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_* (the fallback is the
// shaders' contract, not this component's).
//
// WHICH TRANSFORM FEEDS WHAT (the only scene-state the collection reads
// besides this component — the entity's zircon_component_transform):
//   - kPoint:  m_position is the light's world position; rotation/scale
//     are ignored;
//   - kSpot:   m_position is the apex; the cone axis is the transform's
//     rotated forward — rotation * (0,0,-1), the house forward of the sdk
//     camera's quaternion drive (zircon_editor_sdk_camera.cpp);
//   - kDirectional: m_position is ignored; the direction the light
//     TRAVELS is the same rotated forward (rotation * (0,0,-1)).
// A missing/disabled transform is the identity (origin, forward -Z) —
// graceful, never an assert (user content is not a programmer error).
//
// Angles serialize in DEGREES (the house convention: the camera's fov and
// the sdk camera's pitch/yaw are degrees); the cosine conversion happens
// once per frame at collection, never in the fragment loop.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

/// the light's shape (stored as the enum's underlying u8; the value
/// travels to the GPU table as the params.z lane's bits — the shaders'
/// kZirconLightType_* constants in zircon_core.slang mirror these values,
/// keep them in sync)
enum class eZirconLightType : kotek::uint8_t
{
	/// the sun-like infinite light: direction only, no attenuation; the
	/// binning assigns it to EVERY tile
	kDirectional = 0,
	/// the omni point light: position + range (the sphere volume)
	kPoint = 1,
	/// the cone light: position (apex) + direction (axis) + range +
	/// inner/outer angles
	kSpot = 2
};

class zircon_component_light : public zircon_component_interface
{
public:
	zircon_component_light();
	~zircon_component_light();

	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	eZirconLightType get_light_type(void) const noexcept;
	void set_light_type(eZirconLightType type) noexcept;

	// the light's linear rgb (the intensity multiplies it at collection —
	// the GPU stream carries the premultiplied color)
	float get_color(kotek::uint8_t channel) const noexcept;
	void set_color(float r, float g, float b) noexcept;

	// the brightness multiplier (>= 0; 0 = a dark but present light)
	float get_intensity(void) const noexcept;
	void set_intensity(float intensity) noexcept;

	// the influence radius in meters (kPoint/kSpot; ignored by
	// kDirectional). The collection clamps it to a small positive minimum
	// so the shaders' 1/range division never degenerates
	float get_range(void) const noexcept;
	void set_range(float range) noexcept;

	// the spot cone in degrees (kSpot only): the full-intensity inner
	// cone and the falloff's outer edge; the collection clamps inner <=
	// outer (user content is not a programmer error)
	float get_spot_inner_angle_degrees(void) const noexcept;
	float get_spot_outer_angle_degrees(void) const noexcept;
	void set_spot_angles_degrees(float inner_degrees, float outer_degrees) noexcept;

private:
	bool m_is_enabled;
	eZirconLightType m_light_type;
	float m_color[3];
	float m_intensity;
	float m_range;
	float m_spot_inner_angle_degrees;
	float m_spot_outer_angle_degrees;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::ktk::json::value_from_tag&,
	kotek::ktk::json::value& write_to, const zircon_component_light& data)
{
	unsigned char p_storage_memory[2048];

	kotek::ktk::json::static_resource storage(p_storage_memory);
	kotek::ktk::json::object light(&storage);

	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_LIGHT_TYPE] =
		static_cast<kotek::enum_base_t>(data.get_light_type());

	{
		kotek::ktk::json::array color(&storage);

		for (kotek::uint8_t channel = 0; channel < 3; ++channel)
		{
			color.push_back(
				static_cast<double>(data.get_color(channel)));
		}

		light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_COLOR] = color;
	}

	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_INTENSITY] =
		data.get_intensity();
	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_RANGE] =
		data.get_range();
	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_SPOT_INNER_ANGLE_DEGREES] =
		data.get_spot_inner_angle_degrees();
	light[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_SPOT_OUTER_ANGLE_DEGREES] =
		data.get_spot_outer_angle_degrees();

	write_to = light;
}

inline zircon_component_light tag_invoke(
	const kotek::ktk::json::value_to_tag<zircon_component_light>&,
	const kotek::ktk::json::value& read_from)
{
	auto light = read_from.as_object();

	zircon_component_light result;

	result.set_enabled(
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_IS_ENABLED)
			.as_bool());
	result.set_light_type(static_cast<eZirconLightType>(
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_LIGHT_TYPE)
			.to_number<kotek::enum_base_t>()));

	{
		const auto& color =
			light
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_COLOR)
				.as_array();

		result.set_color(
			static_cast<float>(color[0].to_number<double>()),
			static_cast<float>(color[1].to_number<double>()),
			static_cast<float>(color[2].to_number<double>()));
	}

	result.set_intensity(
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_INTENSITY)
			.to_number<float>());
	result.set_range(
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_RANGE)
			.to_number<float>());
	result.set_spot_angles_degrees(
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_SPOT_INNER_ANGLE_DEGREES)
			.to_number<float>(),
		light
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_LIGHT_FIELD_M_SPOT_OUTER_ANGLE_DEGREES)
			.to_number<float>());

	return result;
}
#endif
