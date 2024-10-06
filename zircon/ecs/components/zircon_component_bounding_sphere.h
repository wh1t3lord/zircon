#pragma once

#include "zircon_component_interface.h"

class zircon_component_bounding_sphere : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_bounding_sphere)

public:
	zircon_component_bounding_sphere(void);
	~zircon_component_bounding_sphere(void);

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

	kn_kotek::kn_ktk::float_t get_radius(void) const noexcept;
	void set_radius(kn_kotek::kn_ktk::float_t value) noexcept;

	const kn_kotek::kn_ktk::kn_math::vec3f_t& get_center(void) const noexcept;
	void set_center(const kn_kotek::kn_ktk::kn_math::vec3f_t& point) noexcept;

	void include(const kn_kotek::kn_ktk::kn_math::vec3f_t& point) noexcept;

private:
#ifdef KOTEK_DEBUG
	kn_kotek::kn_ktk::int32_t m_quality;
#endif

	kn_kotek::kn_ktk::float_t m_radius;
	kn_kotek::kn_ktk::kn_math::vec3f_t m_center;
};

zircon_component_bounding_sphere zircon_calculate_bounding_sphere(
	const kn_kotek::kn_ktk::vector<kn_kotek::kn_ktk::kn_math::vec3f_t>&
		geometry,
	int precision);

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_bounding_sphere& data)
{
	Kotek::ktk::json::object sphere;

	sphere["m_is_enabled"] = data.IsEnabled();
	sphere["m_radius"] = data.get_radius();
	sphere["m_center"] = Kotek::ktk::json::value_from(data.get_center());
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(sphere, data);

	write_to = sphere;
}

inline zircon_component_bounding_sphere tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_bounding_sphere>&,
	const Kotek::ktk::json::value& read_from)
{
	auto sphere = read_from.as_object();

	zircon_component_bounding_sphere result;

	result.SetEnabled(sphere.at("m_is_enabled").as_bool());
	result.set_center(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		sphere.at("m_center")));
	result.set_radius(sphere.at("m_radius").to_number<float>());

	return result;
}
#endif