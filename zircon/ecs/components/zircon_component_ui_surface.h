#pragma once

#include "zircon_component_interface.h"

// TODO: реализовать
/// \~russian @brief если надо использовать на какой-то поверхности вывод
/// интерфейса
class zircon_component_ui_surface : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_ui_surface)

public:
	zircon_component_ui_surface();
	~zircon_component_ui_surface();

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

private:
};

#ifdef KOTEK_USE_BOOST_LIBRARY

inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_ui_surface& data)
{
	Kotek::ktk::json::object info;

	info["m_is_enabled"] = data.IsEnabled();
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(info, data);

	write_to = info;
}

inline zircon_component_ui_surface tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_ui_surface>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_ui_surface result;

	result.SetEnabled(data.at("m_is_enabled").as_bool());

	return result;
}

#endif