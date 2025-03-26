#pragma once

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

#define ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH 64

class zircon_component_sdk_scene_name : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_sdk_scene_name,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_sdk_scene_name();
	~zircon_component_sdk_scene_name();

	void draw_imgui(
		kotek::core::ktkMainManager* main_manager) noexcept override;
	kotek::json::value serialize(void) noexcept override;
	void deserialize(const kotek::json::value& data) noexcept override;
	kotek::json::value serialize(
		unsigned char* p_raw_memory, kotek::size_t size) override;
	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	const char* get_name(void) const noexcept;
	void set_name(const kotek::static_cstring_view_t& name) noexcept;

private:
	bool m_is_enabled;
	kotek::uint8_t m_component_type;
	kotek::static_cstring_t<ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH>
		m_name;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to,
	const zircon_component_sdk_scene_name& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	info[ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_NAME] =
		data.get_name();

	#ifdef KOTEK_DEBUG
	info[ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = info;
}

inline zircon_component_sdk_scene_name tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_sdk_scene_name>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_scene_name result;

	result.set_enabled(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_IS_ENABLED).as_bool());
	result.set_name(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_NAME)
			.as_string()
			.c_str());

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		data.at(ZIRCON_DEF_ZIRCON_COMPONENT_SDK_SCENE_NAME_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?");
	#endif

	return result;
}
#endif
