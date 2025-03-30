#pragma once

#include "zircon_component_input.h"

class zircon_component_sdk_input : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_sdk_input,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_sdk_input();
	zircon_component_sdk_input(const kotek::core::ktkIInput* p_input);
	~zircon_component_sdk_input();

	const zircon_component_input& get_input(void) const noexcept;
	zircon_component_input& get_input(void) noexcept;

	void draw_imgui(
		Kotek::Core::ktkMainManager* main_manager) noexcept override;
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
	zircon_component_input m_input;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_sdk_input& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_INPUT_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	#ifdef KOTEK_DEBUG
	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_INPUT_FIELD_M_COMPONENT_TYPE] =
		data.get_component_type();
	#endif

	write_to = info;
}

inline zircon_component_sdk_input tag_invoke(
	const kotek::json::value_to_tag<zircon_component_sdk_input>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_sdk_input result;

	result.set_enabled(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_INPUT_FIELD_M_IS_ENABLED)
			.as_bool());

	#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_INPUT_FIELD_M_COMPONENT_TYPE)
				.to_number<kotek::uint8_t>() == result.get_component_type(),
		"component type is not equal, data corruption?");
	#endif

	return result;
}
#endif
