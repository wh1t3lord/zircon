#pragma once

// zircon_component_csg — the CSG COMPOUND (task Z25 A1): the entity
// that owns a bounded member list of primitive entity ids (the
// zircon_component_csg_primitive carriers), the dirty flag (u8 — the
// A2 debounced-rebuild trigger, marked by the journaled edit commands)
// and the evaluated-mesh handle (a small pool-agnostic POD — A2 owns
// the dynamic pools; A1 stores the last evaluation's counts for the
// journal roundtrip and the tests). The compound is what the renderer
// draws. Evaluation order = the member list order (additive primitives
// union first, subtractive ones carve in list order — the Valve-carve
// semantic documented in zircon_csg_evaluate.h).
//
// json boundary: the member list serializes as an array of u64 entity
// ids, the mesh handle as a 4-number array [vertex_count,
// triangle_count, generation, state] under its single field key (the
// handle POD lives in zircon_csg_defs.h — the field generator only
// names this component's own members).

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"
#include "zircon_csg_defs.h"

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkMainManager;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

class zircon_component_csg : public zircon_component_interface
{
public:
	zircon_component_csg(void);
	~zircon_component_csg(void);

	kotek::uint8_t get_component_type(void
	) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	// the member list (lookup-table-on-vector, house rule 2 — the
	// compound cap is 128 and the scans are per edit, not per frame).
	// add: false + a loud warning on the capacity guard or a duplicate
	// (a duplicate id is a caller error, never stored twice).
	bool add_primitive_entity(kotek::entity_t id) noexcept;
	bool remove_primitive_entity(kotek::entity_t id) noexcept;
	bool has_primitive_entity(kotek::entity_t id) const noexcept;
	const kotek::static_vector_t<kotek::entity_t,
		ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND>&
	get_primitive_entities(void) const noexcept;
	kotek::uint32_t get_primitive_count(void) const noexcept;

	// the dirty flag (u8 — a GENERATION counter since task Z25 A2,
	// not a level: every mark_dirty() bumps it so a re-edit while
	// the flag is still set stays observable as a new edit; nonzero
	// = dirty is the whole contract)
	kotek::uint8_t is_dirty(void) const noexcept;
	void mark_dirty(void) noexcept;
	void clear_dirty(void) noexcept;

	// the evaluated-mesh handle (pool-agnostic)
	const zircon_csg_mesh_handle_t& get_mesh_handle(void
	) const noexcept;
	void set_mesh_handle(const zircon_csg_mesh_handle_t& handle
	) noexcept;
	// the evaluation seam: records the counts, bumps the generation,
	// marks the handle valid
	void record_evaluation(kotek::uint32_t vertex_count,
		kotek::uint32_t triangle_count) noexcept;
	void mark_evaluation_failed(void) noexcept;

private:
	bool m_is_enabled;
	kotek::static_vector_t<kotek::entity_t,
		ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND>
		m_primitive_entities;
	kotek::uint8_t m_is_dirty;
	zircon_csg_mesh_handle_t m_mesh_handle;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(
	const kotek::ktk::json::value_from_tag&,
	kotek::ktk::json::value& write_to,
	const zircon_component_csg& data
)
{
	unsigned char p_storage_memory[4096];

	kotek::ktk::json::static_resource storage(p_storage_memory);
	kotek::ktk::json::object compound(&storage);

	compound[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_IS_ENABLED] =
		data.is_enabled();

	{
		kotek::ktk::json::array entities(&storage);

		for (kotek::entity_t entity : data.get_primitive_entities())
		{
			entities.push_back(entity.id);
		}

		compound
			[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_PRIMITIVE_ENTITIES] =
				entities;
	}

	compound[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_IS_DIRTY] =
		data.is_dirty();

	{
		const zircon_csg_mesh_handle_t& handle = data.get_mesh_handle();

		kotek::ktk::json::array mesh_handle(&storage);

		mesh_handle.push_back(handle.m_vertex_count);
		mesh_handle.push_back(handle.m_triangle_count);
		mesh_handle.push_back(handle.m_generation);
		mesh_handle.push_back(handle.m_state);

		compound[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_MESH_HANDLE] =
			mesh_handle;
	}

	write_to = compound;
}

inline zircon_component_csg tag_invoke(
	const kotek::ktk::json::value_to_tag<zircon_component_csg>&,
	const kotek::ktk::json::value& read_from
)
{
	auto compound = read_from.as_object();

	zircon_component_csg result;

	result.set_enabled(
		compound.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_IS_ENABLED)
			.as_bool());

	{
		const auto& entities =
			compound
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_PRIMITIVE_ENTITIES
	            )
				.as_array();

		for (const auto& entity_value : entities)
		{
			kotek::entity_t entity{
				entity_value.to_number<kotek::uint64_t>()};

			result.add_primitive_entity(entity);
		}
	}

	{
		const kotek::uint8_t is_dirty =
			compound.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_IS_DIRTY)
				.to_number<kotek::uint8_t>();

		if (is_dirty)
		{
			result.mark_dirty();
		}
		else
		{
			result.clear_dirty();
		}
	}

	{
		const auto& mesh_handle =
			compound.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_CSG_FIELD_M_MESH_HANDLE)
				.as_array();

		zircon_csg_mesh_handle_t handle;

		handle.m_vertex_count =
			mesh_handle[0].to_number<kotek::uint32_t>();
		handle.m_triangle_count =
			mesh_handle[1].to_number<kotek::uint32_t>();
		handle.m_generation =
			mesh_handle[2].to_number<kotek::uint32_t>();
		handle.m_state = mesh_handle[3].to_number<kotek::uint8_t>();

		result.set_mesh_handle(handle);
	}

	return result;
}
#endif
