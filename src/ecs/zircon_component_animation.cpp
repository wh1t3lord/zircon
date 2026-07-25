#include "zircon_component_animation.h"

zircon_component_animation::zircon_component_animation(void) :
	m_is_enabled{true}
{
}

zircon_component_animation::~zircon_component_animation(void) {}

/*
void zircon_component_animation::draw_imgui(
	kotek::Core::ktkMainManager* main_manager
) noexcept
{
	if (main_manager)
	{
		kotek::core::ktkIImguiWrapper* p_imgui_wrapper =
			main_manager->Get_ImguiWrapper();

		if (p_imgui_wrapper)
		{
			if (p_imgui_wrapper->BeginTabBar(
					"ZirconComponentAnimationTabBar"
				))
			{
				if (p_imgui_wrapper->BeginTabItem("info"))
				{
					p_imgui_wrapper->EndTabItem();
				}

				if (p_imgui_wrapper->BeginTabItem("edit"))
				{
					p_imgui_wrapper->EndTabItem();
				}

				p_imgui_wrapper->EndTabBar();
			}
		}
	}
}*/

/*
kotek::json::value zircon_component_animation::serialize(void
) noexcept
{
	return kotek::json::value_from(*this);
}

void zircon_component_animation::deserialize(
	const kotek::json::value& data
) noexcept
{
	*this =
		kotek::json::value_to<zircon_component_animation>(data);
}

kotek::json::value zircon_component_animation::serialize(
	unsigned char* p_raw_memory, kotek::size_t size
)
{
	KOTEK_ASSERT(
		p_raw_memory, "you passed an invalid part of memory!"
	);
	kotek::json::static_resource res(p_raw_memory, size);
	kotek::json::storage_ptr ptr(&res);
	return kotek::json::value_from(*this, ptr);
}*/

kotek::uint8_t
zircon_component_animation::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_animation
	);
}

bool zircon_component_animation::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_animation::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}
