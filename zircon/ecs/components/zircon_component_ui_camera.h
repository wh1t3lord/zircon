#pragma once

#include "zircon_component_interface.h"

/// \~russian @brief используется для создания интерфейса худа
class zircon_component_ui_camera : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_ui_camera)

public:
	zircon_component_ui_camera();
	~zircon_component_ui_camera();

	void DrawImGui(Kotek::Core::ktkMainManager* main_manager) noexcept override;

	const kotek::cstring_t& get_current_page(void) const noexcept;
	void set_current_page(const kotek::cstring_t& page_name) noexcept;
	void clear_current_page(void) noexcept;

	const kotek::unordered_set_t<kotek::cstring_t>& get_predefined_pages(
		void) const noexcept;
	void add_page(const kotek::cstring_t& page_name) noexcept;
	void clear_predefined_pages(void) noexcept;

	void clear_all(void) noexcept;

private:
	kotek::cstring_t m_current_page;
	kotek::unordered_set_t<kotek::cstring_t> m_predefined_pages;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_ui_camera& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif
	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object ui_camera(&storage);

	ui_camera[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();

	#ifdef KOTEK_DEBUG
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(ui_camera, data);
	#endif

	write_to = ui_camera;
}

inline zircon_component_ui_camera tag_invoke(
	const kotek::json::value_to_tag<zircon_component_ui_camera>&,
	const kotek::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_ui_camera result;

	result.SetEnabled(
		data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}
#endif
