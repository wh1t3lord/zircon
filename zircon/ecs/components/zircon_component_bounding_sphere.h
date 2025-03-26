#pragma once

#include "zircon_component_interface.h"

class zircon_component_bounding_sphere : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_bounding_sphere,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_bounding_sphere(void);
	~zircon_component_bounding_sphere(void);

	void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	kn_kotek::kn_ktk::float_t get_radius(void) const noexcept;
	void set_radius(kn_kotek::kn_ktk::float_t value) noexcept;

	const kn_kotek::kn_ktk::kn_math::vec3f_t& get_center(void) const noexcept;
	void set_center(const kn_kotek::kn_ktk::kn_math::vec3f_t& point) noexcept;

	void include(const kn_kotek::kn_ktk::kn_math::vec3f_t& point) noexcept;

private:
	bool m_is_enabled;
	kotek::uint8_t m_component_type;
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
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif

	kotek::json::static_resource storage(p_storage_memory);
	kotek::ktk::json::object sphere(&storage);

	sphere[ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	sphere[ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_RADIUS] =
		data.get_radius();
	sphere[ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_CENTER] =
		Kotek::ktk::json::value_from(data.get_center());

	#ifdef KOTEK_DEBUG
	sphere[ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = sphere;
}

inline zircon_component_bounding_sphere tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_bounding_sphere>&,
	const Kotek::ktk::json::value& read_from)
{
	auto sphere = read_from.as_object();

	zircon_component_bounding_sphere result;

	result.set_enabled(sphere
			.at(ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_IS_ENABLED)
			.as_bool());
	result.set_center(Kotek::ktk::json::value_to<Kotek::ktk::math::vec3f_t>(
		sphere.at(ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_CENTER)));
	result.set_radius(
		sphere.at(ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_RADIUS)
			.to_number<float>());

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		sphere.at(ZIRCON_DEF_ZIRCON_COMPONENT_BOUNDING_SPHERE_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?");
	#endif

	return result;
}
#endif