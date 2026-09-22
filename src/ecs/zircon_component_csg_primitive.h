#pragma once

// zircon_component_csg_primitive — one CSG PRIMITIVE per entity (task
// Z25 A1): the type enum (kBox evaluated exactly; kWedge the exact
// 5-plane ramp; kCylinder box-approximated this phase — see
// zircon_csg_evaluate.h), the dimensions in the CONFIGURED precision
// (zircon_csg_scalar_t), a material id, the operation flags (bit 0 =
// subtractive; additive/union is the default) and the per-primitive
// transform (position + rotation quat in the scalar; scale stays on
// the dimensions, not on the entity transform — the plan's ECS shape).
// The compound (zircon_component_csg) references these by entity id.
//
// json boundary: the scalars serialize as DOUBLE arrays (the
// mode-independent representation — exact in every precision mode, see
// the roundtrip argument on zircon_csg_u32f_to_double), so a scene
// written by one precision build loads in another.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"
#include "zircon_csg_defs.h"
#include "zircon_csg_evaluate.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_csg_primitive
	: public zircon_component_interface
{
public:
	zircon_component_csg_primitive(void);
	~zircon_component_csg_primitive(void);

	kotek::uint8_t get_component_type(void
	) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	eZirconCsgPrimitiveType get_primitive_type(void) const noexcept;
	void set_primitive_type(eZirconCsgPrimitiveType type) noexcept;

	kotek::uint8_t get_operation_flags(void) const noexcept;
	void set_operation_flags(kotek::uint8_t flags) noexcept;
	bool is_subtractive(void) const noexcept;
	void set_subtractive(bool status) noexcept;

	kotek::uint16_t get_material_id(void) const noexcept;
	void set_material_id(kotek::uint16_t id) noexcept;

	// the scalar accessors (per axis / quat component — the configured
	// precision; the axis/component index is a programmer contract)
	zircon_csg_scalar_t get_dimension(kotek::uint8_t axis
	) const noexcept;
	void set_dimension(
		kotek::uint8_t axis, zircon_csg_scalar_t value) noexcept;
	void set_dimensions(zircon_csg_scalar_t x, zircon_csg_scalar_t y,
		zircon_csg_scalar_t z) noexcept;

	zircon_csg_scalar_t get_position_axis(kotek::uint8_t axis
	) const noexcept;
	void set_position_axis(
		kotek::uint8_t axis, zircon_csg_scalar_t value) noexcept;
	void set_position(zircon_csg_scalar_t x, zircon_csg_scalar_t y,
		zircon_csg_scalar_t z) noexcept;

	zircon_csg_scalar_t get_rotation_component(kotek::uint8_t component
	) const noexcept;
	void set_rotation_component(
		kotek::uint8_t component, zircon_csg_scalar_t value) noexcept;
	void set_rotation(zircon_csg_scalar_t x, zircon_csg_scalar_t y,
		zircon_csg_scalar_t z, zircon_csg_scalar_t w) noexcept;

	// the ECS -> evaluation-core seam (the only conversion point the
	// core sees; zircon_csg_evaluate.h stays ECS-free)
	void fill_desc(zircon_csg_primitive_desc_t<zircon_csg_scalar_t>&
			out_desc) const noexcept;

private:
	bool m_is_enabled;
	eZirconCsgPrimitiveType m_primitive_type;
	kotek::uint8_t m_operation_flags;
	kotek::uint16_t m_material_id;
	zircon_csg_scalar_t m_dimensions[3];
	zircon_csg_scalar_t m_position[3];
	zircon_csg_scalar_t m_rotation[4]; // quat x, y, z, w
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(
	const kotek::ktk::json::value_from_tag&,
	kotek::ktk::json::value& write_to,
	const zircon_component_csg_primitive& data
)
{
	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	unsigned char p_storage_memory[2048];

	kotek::ktk::json::static_resource storage(p_storage_memory);
	kotek::ktk::json::object primitive(&storage);

	primitive
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_IS_ENABLED] =
			data.is_enabled();
	primitive
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_PRIMITIVE_TYPE] =
			static_cast<kotek::enum_base_t>(data.get_primitive_type());
	primitive
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_OPERATION_FLAGS] =
			data.get_operation_flags();
	primitive
		[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_MATERIAL_ID] =
			data.get_material_id();

	{
		kotek::ktk::json::array dimensions(&storage);

		for (kotek::uint8_t axis = 0; axis < 3; ++axis)
		{
			dimensions.push_back(
				traits_t::to_double(data.get_dimension(axis)));
		}

		primitive
			[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_DIMENSIONS] =
				dimensions;
	}

	{
		kotek::ktk::json::array position(&storage);

		for (kotek::uint8_t axis = 0; axis < 3; ++axis)
		{
			position.push_back(
				traits_t::to_double(data.get_position_axis(axis)));
		}

		primitive
			[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_POSITION] =
				position;
	}

	{
		kotek::ktk::json::array rotation(&storage);

		for (kotek::uint8_t component = 0; component < 4; ++component)
		{
			rotation.push_back(traits_t::to_double(
				data.get_rotation_component(component)));
		}

		primitive
			[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_ROTATION] =
				rotation;
	}

	write_to = primitive;
}

inline zircon_component_csg_primitive tag_invoke(
	const kotek::ktk::json::value_to_tag<
		zircon_component_csg_primitive>&,
	const kotek::ktk::json::value& read_from
)
{
	using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

	auto primitive = read_from.as_object();

	zircon_component_csg_primitive result;

	result.set_enabled(
		primitive
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_IS_ENABLED
	        )
			.as_bool()
	);
	result.set_primitive_type(static_cast<eZirconCsgPrimitiveType>(
		primitive
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_PRIMITIVE_TYPE
	        )
			.to_number<kotek::enum_base_t>()
	));
	result.set_operation_flags(
		primitive
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_OPERATION_FLAGS
	        )
			.to_number<kotek::uint8_t>()
	);
	result.set_material_id(
		primitive
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_MATERIAL_ID
	        )
			.to_number<kotek::uint16_t>()
	);

	{
		const auto& dimensions =
			primitive
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_DIMENSIONS
	            )
				.as_array();

		for (kotek::uint8_t axis = 0; axis < 3; ++axis)
		{
			result.set_dimension(axis,
				traits_t::from_double(
					dimensions[axis].to_number<double>()));
		}
	}

	{
		const auto& position =
			primitive
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_POSITION
	            )
				.as_array();

		for (kotek::uint8_t axis = 0; axis < 3; ++axis)
		{
			result.set_position_axis(axis,
				traits_t::from_double(
					position[axis].to_number<double>()));
		}
	}

	{
		const auto& rotation =
			primitive
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_PRIMITIVE_FIELD_M_ROTATION
	            )
				.as_array();

		for (kotek::uint8_t component = 0; component < 4; ++component)
		{
			result.set_rotation_component(component,
				traits_t::from_double(
					rotation[component].to_number<double>()));
		}
	}

	return result;
}
#endif
