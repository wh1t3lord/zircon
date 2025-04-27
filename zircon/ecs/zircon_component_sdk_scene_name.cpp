#include "zircon_component_sdk_scene_name.h"

zircon_component_sdk_scene_name::zircon_component_sdk_scene_name() :
	m_is_enabled{true},
	m_component_type{kComponentTypezircon_component_sdk_scene_name}
{
}

zircon_component_sdk_scene_name::~zircon_component_sdk_scene_name() {}

void zircon_component_sdk_scene_name::draw_imgui(
	kotek::core::ktkMainManager* main_manager) noexcept
{
	auto* p_wrapper_imgui = main_manager->Get_ImguiWrapper();

	if (p_wrapper_imgui)
	{
		if (p_wrapper_imgui->CollapsingHeader("Scene name"))
		{
			char buffer[ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH]{};

			kotek::ktk::memory::memcpy(buffer, this->m_name.data(),
				ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH);

			if (p_wrapper_imgui->InputText("Name", buffer,
					ZIRCON_DEF_COMPONENT_SDK_SCENE_NAME_MAX_LENGTH))
			{
				this->m_name = buffer;
			}
		}
	}
}

kotek::json::value zircon_component_sdk_scene_name::serialize(void) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_sdk_scene_name::deserialize(
	const kotek::json::value& data) noexcept
{
	*this = kotek::json::value_to<zircon_component_sdk_scene_name>(data);
}

kotek::json::value zircon_component_sdk_scene_name::serialize(
	unsigned char* p_raw_memory, kotek::size_t size)
{
	KOTEK_ASSERT(p_raw_memory, "you passed an invalid part of memory!");
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}

kotek::uint8_t zircon_component_sdk_scene_name::get_component_type(
	void) const noexcept
{
	return this->m_component_type;
}

bool zircon_component_sdk_scene_name::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_sdk_scene_name::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

const char* zircon_component_sdk_scene_name::get_name(void) const noexcept
{
	return this->m_name.c_str();
}

void zircon_component_sdk_scene_name::set_name(
	const kotek::static_cstring_view_t& name) noexcept
{
	this->m_name = name.data();
}
