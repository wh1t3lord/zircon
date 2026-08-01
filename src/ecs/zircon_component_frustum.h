#pragma once

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_frustum
	: public zircon_component_interface
{
public:
	zircon_component_frustum(void);
	~zircon_component_frustum(void);

	kotek::uint8_t get_component_type(void
	) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

private:
	bool m_is_enabled;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(
	const kotek::json::value_from_tag&,
	kotek::json::value& write_to,
	const zircon_component_frustum& data
)
{
	unsigned char p_storage_memory[1024];

	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object frustum(&storage);

	frustum
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_FRUSTUM_FIELD_M_IS_ENABLED] =
			data.is_enabled();

	write_to = frustum;
}

inline zircon_component_frustum tag_invoke(
	const kotek::json::value_to_tag<zircon_component_frustum>&,
	const kotek::json::value& read_from
)
{
	auto frustum = read_from.as_object();

	zircon_component_frustum result;

	result.set_enabled(
		frustum
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_FRUSTUM_FIELD_M_IS_ENABLED
	        )
			.as_bool()
	);

	return result;
}
#endif