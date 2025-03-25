#pragma once

#include "zircon_component_interface.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_frustum : public zircon_component_interface
{
	KOTEK_COMPONENT(zircon_component_frustum,
		kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>)

public:
	zircon_component_frustum(void);
	~zircon_component_frustum(void);

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
};

#ifdef KOTEK_USE_BOOST_LIBRARY
inline void tag_invoke(const kotek::json::value_from_tag&,
	kotek::json::value& write_to, const zircon_component_frustum& data)
{
	#ifdef KOTEK_DEBUG
	unsigned char p_storage_memory[1024];
	#else
	KOTEK_ASSERT(false, "provide optimized buffer for release");
	#endif

	kotek::json::static_resource storage(p_storage_memory);
	kotek::json::object frustum(&storage);

	frustum[ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD] = data.is_enabled();

	#ifdef KOTEK_DEBUG
	ZIRCON_DEF_TAG_INVOKE_REG_COMPONENT_NAME(frustum, data);
	#endif

	write_to = frustum;
}

inline zircon_component_frustum tag_invoke(
	const kotek::json::value_to_tag<zircon_component_frustum>&,
	const kotek::json::value& read_from)
{
	auto frustum = read_from.as_object();

	zircon_component_frustum result;

	result.SetEnabled(
		frustum.at(ZIRCON_DEF_JSON_SERIALIZE_ENABLED_FIELD).as_bool());

	return result;
}
#endif