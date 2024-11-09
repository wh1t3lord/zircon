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


	const Kotek::ktk::cstring& get_current_page(void) const noexcept;
	void set_current_page(const Kotek::ktk::cstring& page_name) noexcept;
	void clear_current_page(void) noexcept;

	const Kotek::ktk::unordered_set<Kotek::ktk::cstring>& get_predefined_pages(void) const noexcept;
	void add_page(const Kotek::ktk::cstring& page_name) noexcept;
	void clear_predefined_pages(void) noexcept;
	
	void clear_all(void) noexcept;

private:
	Kotek::ktk::cstring m_current_page;
	Kotek::ktk::unordered_set<Kotek::ktk::cstring> m_predefined_pages;
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const Kotek::ktk::json::value_from_tag&,
	Kotek::ktk::json::value& write_to, const zircon_component_ui_camera& data)
{
	Kotek::ktk::json::object info;

	info[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.IsEnabled();
	zircon_DEF_TAG_INVOKE_REG_COMPONENT_NAME(info, data);

	write_to = info;
}

inline zircon_component_ui_camera tag_invoke(
	const Kotek::ktk::json::value_to_tag<zircon_component_ui_camera>&,
	const Kotek::ktk::json::value& read_from)
{
	auto data = read_from.as_object();

	zircon_component_ui_camera result;

	result.SetEnabled(data.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}
#endif
