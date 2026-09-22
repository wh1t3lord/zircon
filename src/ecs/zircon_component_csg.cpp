#include "zircon_component_csg.h"

zircon_component_csg::zircon_component_csg(void) :
	m_is_enabled{true}, m_is_dirty{0},
	m_mesh_handle{
		0, 0, 0,
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kUnevaluated)}
{
}

zircon_component_csg::~zircon_component_csg(void) {}

kotek::uint8_t
zircon_component_csg::get_component_type(void) const noexcept
{
	return static_cast<kotek::uint8_t>(
		eZirconComponentType::kzircon_component_csg
	);
}

bool zircon_component_csg::is_enabled(void) const noexcept
{
	return this->m_is_enabled;
}

void zircon_component_csg::set_enabled(bool status) noexcept
{
	this->m_is_enabled = status;
}

bool zircon_component_csg::add_primitive_entity(kotek::entity_t id
) noexcept
{
	if (this->has_primitive_entity(id))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] the compound already lists primitive entity {} — "
			"a duplicate is a caller error",
			id.id);
		return false;
	}

	if (this->m_primitive_entities.size() >=
	    ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg] the compound is full ({} primitives) — raise "
			"ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND by "
			"measurement",
			static_cast<kotek::uint32_t>(
				ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND));
		return false;
	}

	this->m_primitive_entities.push_back(id);
	return true;
}

bool zircon_component_csg::remove_primitive_entity(kotek::entity_t id
) noexcept
{
	for (kotek::size_t index = 0;
	     index < this->m_primitive_entities.size(); ++index)
	{
		if (this->m_primitive_entities[index] == id)
		{
			this->m_primitive_entities.erase(
				this->m_primitive_entities.begin() + index);
			return true;
		}
	}

	return false;
}

bool zircon_component_csg::has_primitive_entity(kotek::entity_t id
) const noexcept
{
	for (kotek::entity_t entity : this->m_primitive_entities)
	{
		if (entity == id)
			return true;
	}

	return false;
}

const kotek::static_vector_t<kotek::entity_t,
	ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND>&
zircon_component_csg::get_primitive_entities(void) const noexcept
{
	return this->m_primitive_entities;
}

kotek::uint32_t
zircon_component_csg::get_primitive_count(void) const noexcept
{
	return static_cast<kotek::uint32_t>(
		this->m_primitive_entities.size());
}

kotek::uint8_t zircon_component_csg::is_dirty(void) const noexcept
{
	return this->m_is_dirty;
}

void zircon_component_csg::mark_dirty(void) noexcept
{
	// a GENERATION bump, not a level (task Z25 A2): the editor
	// rebuild scheduler debounces on edges — a re-edit while the
	// flag is still set must be observable as a NEW edit, which a
	// plain 0/1 level cannot express. Nonzero = dirty is the whole
	// contract (is_dirty(), the json roundtrip); the u8 wraps
	// naturally and the scheduler compares raw bytes (inequality is
	// wrap-safe — the generation only has to differ from the last
	// observation)
	this->m_is_dirty = static_cast<kotek::uint8_t>(
		this->m_is_dirty + 1u);
}

void zircon_component_csg::clear_dirty(void) noexcept
{
	this->m_is_dirty = 0;
}

const zircon_csg_mesh_handle_t&
zircon_component_csg::get_mesh_handle(void) const noexcept
{
	return this->m_mesh_handle;
}

void zircon_component_csg::set_mesh_handle(
	const zircon_csg_mesh_handle_t& handle
) noexcept
{
	this->m_mesh_handle = handle;
}

void zircon_component_csg::record_evaluation(
	kotek::uint32_t vertex_count, kotek::uint32_t triangle_count
) noexcept
{
	this->m_mesh_handle.m_vertex_count = vertex_count;
	this->m_mesh_handle.m_triangle_count = triangle_count;
	++this->m_mesh_handle.m_generation;
	this->m_mesh_handle.m_state =
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kValid);
}

void zircon_component_csg::mark_evaluation_failed(void) noexcept
{
	this->m_mesh_handle.m_state =
		static_cast<kotek::uint8_t>(eZirconCsgMeshState::kFailed);
}
