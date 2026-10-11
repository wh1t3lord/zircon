#pragma once

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.defines_dependent.ecs/include/kotek_core_defines_dependent_ecs.h>

#include "zircon_component_interface.h"

// zircon_component_terrain_static — the STATIC heightfield terrain (task
// Z24 B5a, the terrain foundation of the approved renderer plan): one
// entity with this component is one heightfield terrain, drawn through
// the B1 GPU-driven chunked path — the heightfield reads through the
// filesystem dispatcher and the mesh builds as CHUNKS into the shared
// zircon_render_chunk_pool (one chunk per world-grid cell; see
// zircon_render_terrain_chunks.h, passes-side, for the build).
//
// THE DESCRIPTOR-ONLY CONTRACT: the component serializes the DESCRIPTOR
// (the heightmap path + the resolution/world/height parameters), never
// the heightfield bytes — content stays files, a scene file references
// them exactly like the geometry component's mesh name does.
//
// THE HEIGHTMAP INTAKE FORMAT (the simplest honest binary, no image
// dependency): a headerless little-endian gray RAW — resolution_x *
// resolution_y samples, row-major, one u8 (kGray8) or one u16
// little-endian (kGray16LE) per sample; the file size must equal
// res_x * res_y * (1|2) exactly (that IS the integrity check — the
// component already carries the dimensions, so no in-file header is
// needed). Per-texel height = value/255 (u8) or value/65535 (u16) *
// m_height_scale meters.
//
// THE TESSELLATION ROADMAP (why three terrain components exist):
//   - zircon_component_terrain_static (THIS one): the static chunk grid
//     — the correctness/proof floor, always available, B5a;
//   - zircon_component_terrain_cbt: the planned B5b+ upgrade — GPU
//     concurrent-binary-tree tessellation (the modern open-world answer,
//     adaptive subdivision on the GPU); stays a stub until then;
//   - zircon_component_terrain_gcm: the geometry-clipmap fallback for
//     the same era; stays a stub.

// capacity of the heightmap path (a content-root-relative file name
// resolved under data_game — e.g. "terrain/heightmaps/island.r16");
// empty means the component is unconfigured (no terrain is built)
#define zircon_DEF_COMPONENT_TERRAIN_STATIC_HEIGHTMAP_PATH_MAX_LENGTH 96

/// the heightmap sample encoding (the component stores the value as the
/// enum's underlying u8)
enum class eZirconTerrainHeightmapFormat : kotek::uint8_t
{
	/// one u8 per sample, height = value/255 * height_scale
	kGray8 = 0,
	/// one little-endian u16 per sample, height = value/65535 *
	/// height_scale
	kGray16LE = 1
};

class zircon_component_terrain_static : public zircon_component_interface
{
public:
	zircon_component_terrain_static();
	~zircon_component_terrain_static();

	kotek::uint8_t get_component_type(void) const noexcept override;

	bool is_enabled(void) const noexcept;
	void set_enabled(bool status) noexcept;

	// the heightmap source (empty = unconfigured, no terrain is built —
	// the loud-graceful path lives in the builder)
	const char* get_heightmap_path(void) const noexcept;
	void set_heightmap_path(const char* p_heightmap_path) noexcept;

	// the heightmap's sample grid in texels per side (both must be >= 2
	// for a build; 0 = unconfigured)
	kotek::uint16_t get_resolution_x(void) const noexcept;
	kotek::uint16_t get_resolution_y(void) const noexcept;
	void set_resolution(
		kotek::uint16_t resolution_x, kotek::uint16_t resolution_y) noexcept;

	// the terrain's world footprint in meters (the full width/depth the
	// heightmap stretches over)
	float get_world_size_x(void) const noexcept;
	float get_world_size_z(void) const noexcept;
	void set_world_size(float world_size_x, float world_size_z) noexcept;

	// the height of a maximum sample in meters (the vertical scale)
	float get_height_scale(void) const noexcept;
	void set_height_scale(float height_scale) noexcept;

	eZirconTerrainHeightmapFormat get_heightmap_format(void) const noexcept;
	void set_heightmap_format(eZirconTerrainHeightmapFormat format) noexcept;

private:
	bool m_is_enabled;
	eZirconTerrainHeightmapFormat m_heightmap_format;
	kotek::uint16_t m_resolution_x;
	kotek::uint16_t m_resolution_y;
	float m_world_size_x;
	float m_world_size_z;
	float m_height_scale;
	kotek::static_cstring_t<
		zircon_DEF_COMPONENT_TERRAIN_STATIC_HEIGHTMAP_PATH_MAX_LENGTH>
		m_heightmap_path;
};

#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
inline void tag_invoke(const kotek::ktk::json::value_from_tag&,
	kotek::ktk::json::value& write_to,
	const zircon_component_terrain_static& data)
{
	unsigned char p_storage_memory[2048];

	kotek::ktk::json::static_resource storage(p_storage_memory);
	kotek::ktk::json::object terrain(&storage);

	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_IS_ENABLED] =
		data.is_enabled();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHTMAP_FORMAT] =
		static_cast<kotek::enum_base_t>(data.get_heightmap_format());
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_RESOLUTION_X] =
		data.get_resolution_x();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_RESOLUTION_Y] =
		data.get_resolution_y();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_WORLD_SIZE_X] =
		data.get_world_size_x();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_WORLD_SIZE_Z] =
		data.get_world_size_z();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHT_SCALE] =
		data.get_height_scale();
	terrain[ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHTMAP_PATH] =
		data.get_heightmap_path();

	write_to = terrain;
}

inline zircon_component_terrain_static tag_invoke(
	const kotek::ktk::json::value_to_tag<zircon_component_terrain_static>&,
	const kotek::ktk::json::value& read_from)
{
	auto terrain = read_from.as_object();

	zircon_component_terrain_static result;

	result.set_enabled(
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_IS_ENABLED)
			.as_bool());
	result.set_heightmap_format(
		static_cast<eZirconTerrainHeightmapFormat>(
			terrain
				.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHTMAP_FORMAT)
				.to_number<kotek::enum_base_t>()));
	result.set_resolution(
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_RESOLUTION_X)
			.to_number<kotek::uint16_t>(),
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_RESOLUTION_Y)
			.to_number<kotek::uint16_t>());
	result.set_world_size(
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_WORLD_SIZE_X)
			.to_number<float>(),
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_WORLD_SIZE_Z)
			.to_number<float>());
	result.set_height_scale(
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHT_SCALE)
			.to_number<float>());
	result.set_heightmap_path(
		terrain
			.at(ZIRCON_DEF_GAME_ZIRCON_COMPONENT_TERRAIN_STATIC_FIELD_M_HEIGHTMAP_PATH)
			.as_string()
			.c_str());

	return result;
}
#endif
