#include "zircon_component_input.h"
#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

zircon_component_input::zircon_component_input(void) :
	m_is_invert_mouse_axis_x{}, m_is_invert_mouse_axis_y{},
	m_sensetivity{ZIRCON_DEF_COMPONENT_INPUT_DEFAULT_SENSETIVITY},
	m_input_type{static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eInputType::kInputType_DisabledCursor)},
	m_input_type_previous{m_input_type}, m_p_input_manager{}
{
}

zircon_component_input::zircon_component_input(
	const kotek::core::ktkIInput* p_manager) :
	m_is_invert_mouse_axis_x{}, m_is_invert_mouse_axis_y{}, m_sensetivity{},
	m_input_type{static_cast<Kotek::ktk::enum_base_t>(
		Kotek::Core::eInputType::kInputType_DisabledCursor)},
	m_input_type_previous{m_input_type}, m_p_input_manager{p_manager}
{
}

zircon_component_input::~zircon_component_input(void) {}

void zircon_component_input::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->BeginTabBar("ZirconComponentInput"))
			{
				if (p_wrapper_imgui->BeginTabItem("info"))
				{
					p_wrapper_imgui->Text("Input type: %s",
						Kotek::Core::helper::Translate_InputType(
							static_cast<Kotek::Core::eInputType>(
								this->m_input_type))
							.c_str());

					// TODO: implement showing all devices that connected to ps
					// for each devices print all necessary information
					if (this->m_p_input_manager)
					{
						p_wrapper_imgui->Text("x: \n\t%.3f (pixels) \n\t%.3f "
											  "(normalized) \n\t%.3f (delta) "
											  "\n\t%.3f (delta*sens)",
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseCoordinateXInPixels),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseCoordinateXNormalized),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseDeltaX),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseDeltaX) *
								this->m_sensetivity);

						p_wrapper_imgui->Text("y: \n\t%.3f (pixels) \n\t%.3f "
											  "(normalized) \n\t%.3f (delta) "
											  "\n\t%.3f (delta*sens)",
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseCoordinateYInPixels),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseCoordinateYNormalized),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseDeltaY),
							this->m_p_input_manager->Get_ControllerData(
								kotek::core::eInputControllerType::
									kControllerMouse,
								kotek::core::eInputControllerMouseData::
									kMouseDeltaY) *
								this->m_sensetivity);
					}

					p_wrapper_imgui->EndTabItem();
				}

				if (p_wrapper_imgui->BeginTabItem("edit"))
				{
					p_wrapper_imgui->DragFloat("sensetivity",
						&this->m_sensetivity, 1.0f, 0.00001f, 1.0f);

					p_wrapper_imgui->EndTabItem();
				}

				p_wrapper_imgui->EndTabBar();
			}
		}
	}
}

void zircon_component_input::set_input_type(
	Kotek::ktk::enum_base_t type) noexcept
{
	if (type != this->m_input_type)
		this->m_input_type_previous = this->m_input_type;

	this->m_input_type = type;
}

Kotek::ktk::enum_base_t zircon_component_input::get_input_type(
	void) const noexcept
{
	return this->m_input_type;
}

Kotek::ktk::enum_base_t zircon_component_input::get_input_type_previous(
	void) const noexcept
{
	return this->m_input_type_previous;
}

bool zircon_component_input::is_key_holding(
	kotek::core::eInputAllKeys key, unsigned char frames) const
{
	KOTEK_ASSERT(this->m_p_input_manager,
		"you forgot to initialize this class, probably wrong constructor was "
		"used?");

	if (this->m_p_input_manager)
	{
		return this->m_p_input_manager->Is_KeyHolding(
			this->m_p_input_manager->Get_ControllerTypeByKey(key), key, frames);
	}

	return false;
}

bool zircon_component_input::is_key_pressed(
	kotek::core::eInputAllKeys key) const
{
	KOTEK_ASSERT(this->m_p_input_manager,
		"you forgot to initialize this class, probably wrong constructor was "
		"used?");

	if (this->m_p_input_manager)
	{
		return this->m_p_input_manager->Is_KeyPressed(
			this->m_p_input_manager->Get_ControllerTypeByKey(key), key);
	}

	return false;
}

bool zircon_component_input::is_key_released(
	kotek::core::eInputAllKeys key) const
{
	KOTEK_ASSERT(this->m_p_input_manager,
		"you forgot to initialize this class, probably wrong constructor was "
		"used?");

	if (this->m_p_input_manager)
	{
		return this->m_p_input_manager->Is_KeyReleased(
			this->m_p_input_manager->Get_ControllerTypeByKey(key), key);
	}

	return false;
}

float zircon_component_input::get_sensetivity(void) const
{
	return m_sensetivity;
}

void zircon_component_input::set_sensetivity(float sensetivity)
{
	m_sensetivity = sensetivity;
}

void zircon_component_input::register_input(
	const kotek::core::ktkIInput* p_input_manager)
{
	KOTEK_ASSERT(p_input_manager, "you can't pass invalid pointer!");

	this->m_p_input_manager = p_input_manager;

	if (this->m_p_input_manager)
	{
	}
}

float zircon_component_input::get_delta_x(
	kotek::core::eInputControllerType type) const
{
	if (this->m_p_input_manager)
	{
		switch (type)
		{
		case kotek::core::eInputControllerType::kControllerMouse:
		{
			float result = this->m_p_input_manager->Get_ControllerData(
				type, kotek::core::eInputControllerMouseData::kMouseDeltaX);

			return result;
		}
		default:
		{
			KOTEK_ASSERT(false, "not implemented!");
			break;
		}
		}
	}

	return 0.0f;
}

float zircon_component_input::get_delta_y(
	kotek::core::eInputControllerType type) const
{
	if (this->m_p_input_manager)
	{
		switch (type)
		{
		case kotek::core::eInputControllerType::kControllerMouse:
		{
			float result = this->m_p_input_manager->Get_ControllerData(
				type, kotek::core::eInputControllerMouseData::kMouseDeltaY);

			return result;
		}
		default:
		{
			KOTEK_ASSERT(false, "not implemented!");
			break;
		}
		}
	}

	return 0.0f;
}

bool zircon_component_input::is_invert_mouse_axis_x(void) const
{
	return this->m_is_invert_mouse_axis_x;
}

bool zircon_component_input::is_invert_mouse_axis_y(void) const
{
	return this->m_is_invert_mouse_axis_y;
}

void zircon_component_input::set_invert_mouse_axis_x(bool status)
{
	this->m_is_invert_mouse_axis_x = status;
}

void zircon_component_input::set_invert_mouse_axis_y(bool status)
{
	this->m_is_invert_mouse_axis_y = status;
}
