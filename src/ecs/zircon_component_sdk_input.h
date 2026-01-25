#pragma once

#include "zircon_component_input.h"

class zircon_component_sdk_input : public zircon_component_interface
{
public:
	zircon_component_sdk_input();
	zircon_component_sdk_input(const kotek::core::ktkIInput* p_input);
	~zircon_component_sdk_input();

	const zircon_component_input& get_input(void) const noexcept;
	zircon_component_input& get_input(void) noexcept;

	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

private:
	bool m_is_enabled;
	zircon_component_input m_input;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_sdk_input& data)
{
	kotek::json::object info;

	info[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_SDK_INPUT_FIELD_M_IS_ENABLED] =
		data.is_enabled();

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

	return result;
}
#endif
