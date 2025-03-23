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

inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_ui_surface& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "");
	#endif

	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object ui_surface(&storage);

	ui_surface[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();

	#ifdef KOTEK_DEBUG
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(ui_surface, data);
	#endif

	write_to = ui_surface;
}

inline zircon_component_ui_surface tag_invoke(
	const kotek::json::value_to_tag<zircon_component_ui_surface>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_ui_surface result;

	result.SetEnabled(
		data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}

#endif