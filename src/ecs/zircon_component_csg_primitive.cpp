#include "zircon_component_csg_primitive.h"

zircon_component_csg_primitive::zircon_component_csg_primitive(void) :
	m_is_enabled{true},
	m_primitive_type{eZirconCsgPrimitiveType::kBox},
	m_operation_flags{0}, m_material_id{0},
	m_dimensions{
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::one(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::one(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::one()},
	m_position{
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero()},
	m_rotation{
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero(),
		zircon_csg_scalar_traits<zircon_csg_scalar_t>::one()}
{
}

zircon_component_csg_primitive::~zircon_component_csg_primitive(void)
{
}

kotek::uint8_t
zircon_component_csg_primitive::get_component_type(void
) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_csg_primitive
	);
}

bool zircon_component_csg_primitive::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_csg_primitive::set_enabled(bool status
) noexcept
{
	this->m_is_enabled = status;
}

eZirconCsgPrimitiveType
zircon_component_csg_primitive::get_primitive_type(void
) const noexcept
{
	return this->m_primitive_type;
}

void zircon_component_csg_primitive::set_primitive_type(
	eZirconCsgPrimitiveType type
) noexcept
{
	this->m_primitive_type = type;
}

kotek::uint8_t
zircon_component_csg_primitive::get_operation_flags(void
) const noexcept
{
	return this->m_operation_flags;
}

void zircon_component_csg_primitive::set_operation_flags(
	kotek::uint8_t flags
) noexcept
{
	this->m_operation_flags = flags;
}

bool zircon_component_csg_primitive::is_subtractive(void
) const noexcept
{
	return (this->m_operation_flags &
	           zircon_DEF_CSG_OPERATION_SUBTRACTIVE) != 0;
}

void zircon_component_csg_primitive::set_subtractive(bool status
) noexcept
{
	if (status)
	{
		this->m_operation_flags |=
			zircon_DEF_CSG_OPERATION_SUBTRACTIVE;
	}
	else
	{
		this->m_operation_flags &=
			~zircon_DEF_CSG_OPERATION_SUBTRACTIVE;
	}
}

kotek::uint16_t
zircon_component_csg_primitive::get_material_id(void) const noexcept
{
	return this->m_material_id;
}

void zircon_component_csg_primitive::set_material_id(
	kotek::uint16_t id
) noexcept
{
	this->m_material_id = id;
}

zircon_csg_scalar_t
zircon_component_csg_primitive::get_dimension(kotek::uint8_t axis
) const noexcept
{
	KOTEK_ASSERT(axis < 3, "the axis index is a programmer contract");

	if (axis >= 3)
		return zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero();

	return this->m_dimensions[axis];
}

void zircon_component_csg_primitive::set_dimension(
	kotek::uint8_t axis, zircon_csg_scalar_t value
) noexcept
{
	KOTEK_ASSERT(axis < 3, "the axis index is a programmer contract");

	if (axis >= 3)
		return;

	this->m_dimensions[axis] = value;
}

void zircon_component_csg_primitive::set_dimensions(
	zircon_csg_scalar_t x, zircon_csg_scalar_t y, zircon_csg_scalar_t z
) noexcept
{
	this->m_dimensions[0] = x;
	this->m_dimensions[1] = y;
	this->m_dimensions[2] = z;
}

zircon_csg_scalar_t
zircon_component_csg_primitive::get_position_axis(kotek::uint8_t axis
) const noexcept
{
	KOTEK_ASSERT(axis < 3, "the axis index is a programmer contract");

	if (axis >= 3)
		return zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero();

	return this->m_position[axis];
}

void zircon_component_csg_primitive::set_position_axis(
	kotek::uint8_t axis, zircon_csg_scalar_t value
) noexcept
{
	KOTEK_ASSERT(axis < 3, "the axis index is a programmer contract");

	if (axis >= 3)
		return;

	this->m_position[axis] = value;
}

void zircon_component_csg_primitive::set_position(
	zircon_csg_scalar_t x, zircon_csg_scalar_t y, zircon_csg_scalar_t z
) noexcept
{
	this->m_position[0] = x;
	this->m_position[1] = y;
	this->m_position[2] = z;
}

zircon_csg_scalar_t
zircon_component_csg_primitive::get_rotation_component(
	kotek::uint8_t component
) const noexcept
{
	KOTEK_ASSERT(
		component < 4, "the quat component is a programmer contract"
	);

	if (component >= 4)
		return zircon_csg_scalar_traits<zircon_csg_scalar_t>::zero();

	return this->m_rotation[component];
}

void zircon_component_csg_primitive::set_rotation_component(
	kotek::uint8_t component, zircon_csg_scalar_t value
) noexcept
{
	KOTEK_ASSERT(
		component < 4, "the quat component is a programmer contract"
	);

	if (component >= 4)
		return;

	this->m_rotation[component] = value;
}

void zircon_component_csg_primitive::set_rotation(
	zircon_csg_scalar_t x, zircon_csg_scalar_t y, zircon_csg_scalar_t z,
	zircon_csg_scalar_t w
) noexcept
{
	this->m_rotation[0] = x;
	this->m_rotation[1] = y;
	this->m_rotation[2] = z;
	this->m_rotation[3] = w;
}

void zircon_component_csg_primitive::fill_desc(
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t>& out_desc
) const noexcept
{
	for (kotek::uint8_t axis = 0; axis < 3; ++axis)
	{
		out_desc.m_dimensions[axis] = this->m_dimensions[axis];
		out_desc.m_position[axis] = this->m_position[axis];
	}

	for (kotek::uint8_t component = 0; component < 4; ++component)
	{
		out_desc.m_rotation[component] = this->m_rotation[component];
	}

	out_desc.m_material_id = this->m_material_id;
	out_desc.m_type = static_cast<kotek::uint8_t>(this->m_primitive_type);
	out_desc.m_operation = this->m_operation_flags;
}
