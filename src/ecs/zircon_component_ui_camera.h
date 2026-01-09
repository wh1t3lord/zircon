#pragma once

#include "zircon_component_interface.h"

/// \~russian @brief ������������ ��� �������� ���������� ����
class zircon_component_ui_camera
	: public zircon_component_interface
{
public:
	zircon_component_ui_camera();
	~zircon_component_ui_camera();

	kotek::uint8_t get_component_type(void
	) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	const kotek::cstring_t& get_current_page(void
	) const noexcept;
	void set_current_page(const kotek::cstring_t& page_name
	) noexcept;
	void clear_current_page(void) noexcept;

	const kotek::unordered_set_t<kotek::cstring_t>&
	get_predefined_pages(void) const noexcept;
	void add_page(const kotek::cstring_t& page_name) noexcept;
	void clear_predefined_pages(void) noexcept;

	void clear_all(void) noexcept;

private:
	bool m_is_enabled;
	kotek::cstring_t m_current_page;
	kotek::unordered_set_t<kotek::cstring_t> m_predefined_pages;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(
	const kotek::json::value_from_tag&,
	kotek::json::value& write_to,
	const zircon_component_ui_camera& data
)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif
	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object ui_camera(&storage);

	ui_camera
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_UI_CAMERA_FIELD_M_IS_ENABLED] =
			data.is_enabled();

	#ifdef KOTEK_DEBUG
	ui_camera
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_UI_CAMERA_FIELD_M_COMPONENT_TYPE] =
			data.get_component_type();
	#endif

	write_to = ui_camera;
}

inline zircon_component_ui_camera tag_invoke(
	const kotek::json::value_to_tag<
		zircon_component_ui_camera>&,
	const kotek::json::value& read_from
)
{
	auto data = read_from.as_object();

	zircon_component_ui_camera result;

	result.set_enabled(
		data.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_UI_CAMERA_FIELD_M_IS_ENABLED
	    )
			.as_bool()
	);

	return result;
}
#endif
