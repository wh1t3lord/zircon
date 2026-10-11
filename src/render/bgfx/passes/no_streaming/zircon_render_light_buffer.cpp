#include "zircon_render_light_buffer.h"

#include "../../../../ecs/zircon_component_transform.h"
#include "../../../../ecs/zircon_factory.h"

#include <cmath>
#include <cstring>

zircon_render_light_buffer::zircon_render_light_buffer(void) :
	m_is_warned_about_overflow{false}, m_is_warned_about_type{false}
{
}

kotek::uint32_t zircon_render_light_buffer::collect_from_world(
	zircon_factory* p_factory, zircon_ecs_context_t* p_context,
	kotek::uint32_t entity_count_max_limit) noexcept
{
	this->clear();

	if (p_factory == nullptr || p_context == nullptr)
		return 0;

	const kotek::uint32_t entity_count = p_factory->get_all_entities(
		p_context, entity_count_max_limit, this->m_entity_scan,
		ZIRCON_DEF_LIGHT_MAX_ENTITY_SCAN);

	kotek::uint32_t collected = 0;

	for (kotek::uint32_t entity_index = 0; entity_index < entity_count;
		 ++entity_index)
	{
		const kotek::entity_t& entity = this->m_entity_scan[entity_index];

		if (p_factory->has_component(p_context, entity,
				eZirconComponentType::kzircon_component_light) == false)
		{
			continue;
		}

		zircon_component_light* p_light =
			static_cast<zircon_component_light*>(
				p_factory->get_component_by_enum(p_context, entity,
					eZirconComponentType::kzircon_component_light));

		if (p_light == nullptr || p_light->is_enabled() == false)
			continue;

		// the transform contract (the component header's table): position
		// feeds point/spot, the rotated forward feeds directional/spot; a
		// missing/disabled transform is the identity
		float position_xyz[3] = {0.0f, 0.0f, 0.0f};
		float direction_xyz[3] = {0.0f, 0.0f, -1.0f};

		if (p_factory->has_component(p_context, entity,
				eZirconComponentType::kzircon_component_transform))
		{
			zircon_component_transform* p_transform =
				static_cast<zircon_component_transform*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::kzircon_component_transform));

			if (p_transform && p_transform->is_enabled())
			{
				const kotek::math::vec3f_t& position =
					p_transform->get_position();

				position_xyz[0] = position.x();
				position_xyz[1] = position.y();
				position_xyz[2] = position.z();

				// the house forward (0,0,-1) rotated by the entity's
				// rotation — the sdk camera's quaternion-drive convention
				const kotek::math::vec3f_t forward =
					kotek::math::get_math_rotate(p_transform->get_rotation(),
						kotek::math::vec3f_t(0.0f, 0.0f, -1.0f));

				direction_xyz[0] = forward.x();
				direction_xyz[1] = forward.y();
				direction_xyz[2] = forward.z();
			}
		}

		// normalize defensively (a degenerate user quat must not propagate
		// NaN into the GPU table); a zero-length direction falls back to
		// the identity forward
		const float direction_length = std::sqrt(direction_xyz[0] * direction_xyz[0] +
			direction_xyz[1] * direction_xyz[1] + direction_xyz[2] * direction_xyz[2]);

		if (direction_length > 1e-6f)
		{
			direction_xyz[0] /= direction_length;
			direction_xyz[1] /= direction_length;
			direction_xyz[2] /= direction_length;
		}
		else
		{
			direction_xyz[0] = 0.0f;
			direction_xyz[1] = 0.0f;
			direction_xyz[2] = -1.0f;
		}

		const eZirconLightType type_raw = p_light->get_light_type();

		// an out-of-enum type (corrupt content) degrades to a point light
		// — deterministic, and the CPU/GPU sides agree because the clamp
		// happens BEFORE the table write (loud once per the latch, never
		// per-frame spam)
		eZirconLightType type = type_raw;

		if (type != eZirconLightType::kDirectional &&
			type != eZirconLightType::kPoint && type != eZirconLightType::kSpot)
		{
			if (this->m_is_warned_about_type == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[light_buffer] a light with the unknown type {} "
					"degrades to kPoint",
					static_cast<kotek::uint32_t>(type_raw));

				this->m_is_warned_about_type = true;
			}

			type = eZirconLightType::kPoint;
		}

		const float color_rgb[3] = {p_light->get_color(0),
			p_light->get_color(1), p_light->get_color(2)};

		if (this->append_light(type,
				type == eZirconLightType::kDirectional ? nullptr
													   : position_xyz,
				type == eZirconLightType::kPoint ? nullptr : direction_xyz,
				color_rgb, p_light->get_intensity(), p_light->get_range(),
				p_light->get_spot_inner_angle_degrees(),
				p_light->get_spot_outer_angle_degrees()) == false)
		{
			break;
		}

		++collected;
	}

	if (collected < ZIRCON_DEF_LIGHT_MAX_COUNT)
		this->m_is_warned_about_overflow = false;

	return collected;
}

bool zircon_render_light_buffer::append_light(eZirconLightType type,
	const float* p_position_xyz, const float* p_dir_from_xyz,
	const float* p_color_rgb, float intensity, float range,
	float spot_inner_degrees, float spot_outer_degrees) noexcept
{
	KOTEK_ASSERT(p_color_rgb, "must be valid");

	if (p_color_rgb == nullptr)
		return false;

	if (this->m_positions.size() >= ZIRCON_DEF_LIGHT_MAX_COUNT)
	{
		if (this->m_is_warned_about_overflow == false)
		{
			KOTEK_MESSAGE_WARNING(
				"[light_buffer] more than {} enabled lights — the first {} "
				"collect, the rest are dropped this frame",
				ZIRCON_DEF_LIGHT_MAX_COUNT, ZIRCON_DEF_LIGHT_MAX_COUNT);

			this->m_is_warned_about_overflow = true;
		}

		return false;
	}

	const bool is_directional = type == eZirconLightType::kDirectional;
	const bool is_point = type == eZirconLightType::kPoint;

	// the range: clamped to the documented minimum so the shaders'
	// 1/range division is finite (directionals carry 0 — unused)
	if (range < ZIRCON_DEF_LIGHT_MIN_RANGE)
		range = ZIRCON_DEF_LIGHT_MIN_RANGE;

	// the spot cone: inner clamps to <= outer (user content is not a
	// programmer error) and both stay inside [0, 179] degrees so the
	// cosines are finite; the FS evaluates these exact cosines, only the
	// binning's bounding sphere widens for very wide cones
	if (spot_outer_degrees < 0.0f)
		spot_outer_degrees = 0.0f;
	if (spot_outer_degrees > 179.0f)
		spot_outer_degrees = 179.0f;
	if (spot_inner_degrees < 0.0f)
		spot_inner_degrees = 0.0f;
	if (spot_inner_degrees > spot_outer_degrees)
		spot_inner_degrees = spot_outer_degrees;

	const float cos_inner = std::cos(
		spot_inner_degrees * 0.017453292519943295769f /* pi/180 */);
	const float cos_outer = std::cos(
		spot_outer_degrees * 0.017453292519943295769f);

	const float position_xyz[3] = {p_position_xyz ? p_position_xyz[0] : 0.0f,
		p_position_xyz ? p_position_xyz[1] : 0.0f,
		p_position_xyz ? p_position_xyz[2] : 0.0f};
	const float direction_xyz[3] = {p_dir_from_xyz ? p_dir_from_xyz[0] : 0.0f,
		p_dir_from_xyz ? p_dir_from_xyz[1] : 0.0f,
		p_dir_from_xyz ? p_dir_from_xyz[2] : -1.0f};

	zircon_light_stream_record_t position_record = {};
	zircon_light_stream_record_t direction_record = {};
	zircon_light_stream_record_t color_record = {};
	zircon_light_stream_record_t params_record = {};

	// positions: xyz = the world position (point/spot), w = the range
	position_record.m_v[0] = is_directional ? 0.0f : position_xyz[0];
	position_record.m_v[1] = is_directional ? 0.0f : position_xyz[1];
	position_record.m_v[2] = is_directional ? 0.0f : position_xyz[2];
	position_record.m_v[3] = is_directional ? 0.0f : range;

	// directions: xyz = the travel direction (directional/spot —
	// normalized by the caller; the collection normalizes), zero for
	// points
	direction_record.m_v[0] = is_point ? 0.0f : direction_xyz[0];
	direction_record.m_v[1] = is_point ? 0.0f : direction_xyz[1];
	direction_record.m_v[2] = is_point ? 0.0f : direction_xyz[2];
	direction_record.m_v[3] = 0.0f;

	// colors: the premultiplied rgb (the fragment loop never re-applies
	// the intensity)
	color_record.m_v[0] = p_color_rgb[0] * intensity;
	color_record.m_v[1] = p_color_rgb[1] * intensity;
	color_record.m_v[2] = p_color_rgb[2] * intensity;
	color_record.m_v[3] = 0.0f;

	// params: the spot cosines + the type as the z lane's u32 BIT PATTERN
	// (the B1 integer-lane discipline — the shader asuints it)
	params_record.m_v[0] = cos_inner;
	params_record.m_v[1] = cos_outer;

	const kotek::uint32_t type_bits = static_cast<kotek::uint32_t>(type);

	std::memcpy(&params_record.m_v[2], &type_bits, sizeof(kotek::uint32_t));

	this->m_positions.push_back(position_record);
	this->m_directions.push_back(direction_record);
	this->m_colors.push_back(color_record);
	this->m_params.push_back(params_record);
	this->m_types.push_back(static_cast<kotek::uint8_t>(type));
	// flags: bit 0 = the RESERVED shadow-caster pre-wire (B5c), always 0
	// this phase
	this->m_flags.push_back(0);

	return true;
}

void zircon_render_light_buffer::clear(void) noexcept
{
	this->m_positions.clear();
	this->m_directions.clear();
	this->m_colors.clear();
	this->m_params.clear();
	this->m_types.clear();
	this->m_flags.clear();
}

kotek::uint32_t zircon_render_light_buffer::get_count(void) const noexcept
{
	return static_cast<kotek::uint32_t>(this->m_positions.size());
}

const zircon_light_stream_record_t*
zircon_render_light_buffer::get_positions(void) const noexcept
{
	return this->m_positions.data();
}

const zircon_light_stream_record_t*
zircon_render_light_buffer::get_directions(void) const noexcept
{
	return this->m_directions.data();
}

const zircon_light_stream_record_t* zircon_render_light_buffer::get_colors(
	void) const noexcept
{
	return this->m_colors.data();
}

const zircon_light_stream_record_t* zircon_render_light_buffer::get_params(
	void) const noexcept
{
	return this->m_params.data();
}

const kotek::uint8_t* zircon_render_light_buffer::get_types(
	void) const noexcept
{
	return this->m_types.data();
}

const kotek::uint8_t* zircon_render_light_buffer::get_flags(
	void) const noexcept
{
	return this->m_flags.data();
}

bool zircon_render_light_buffer::derive_tile_grid(
	kotek::uint32_t screen_width, kotek::uint32_t screen_height,
	kotek::uint32_t tile_pixels, kotek::uint32_t& out_tiles_x,
	kotek::uint32_t& out_tiles_y) noexcept
{
	if (screen_width == 0 || screen_height == 0 || tile_pixels == 0)
	{
		out_tiles_x = 0;
		out_tiles_y = 0;
		return false;
	}

	out_tiles_x = (screen_width + tile_pixels - 1) / tile_pixels;
	out_tiles_y = (screen_height + tile_pixels - 1) / tile_pixels;

	return out_tiles_x * out_tiles_y <= ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES;
}

bool zircon_render_light_buffer::sphere_vs_frustum(
	const float* p_planes_6x4, const float* p_center_xyz,
	float radius) noexcept
{
	KOTEK_ASSERT(p_planes_6x4, "must be valid");
	KOTEK_ASSERT(p_center_xyz, "must be valid");

	if (p_planes_6x4 == nullptr || p_center_xyz == nullptr)
		return false;

	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		const float* p_plane = p_planes_6x4 + plane_index * 4;

		// the sphere is outside when its nearest point to the plane is
		// still outside: dot(n, c) + d < -r
		if (p_plane[0] * p_center_xyz[0] + p_plane[1] * p_center_xyz[1] +
				p_plane[2] * p_center_xyz[2] + p_plane[3] <
			-radius)
		{
			return false;
		}
	}

	return true;
}

void zircon_render_light_buffer::spot_bounding_sphere(
	const float* p_apex_xyz, const float* p_dir_from_xyz, float range,
	float cos_outer, float* p_out_center_xyz, float& out_radius) noexcept
{
	KOTEK_ASSERT(p_apex_xyz, "must be valid");
	KOTEK_ASSERT(p_dir_from_xyz, "must be valid");
	KOTEK_ASSERT(p_out_center_xyz, "must be valid storage");

	if (p_apex_xyz == nullptr || p_dir_from_xyz == nullptr ||
		p_out_center_xyz == nullptr)
	{
		return;
	}

	// tan from the cosine: sin = sqrt(1 - cos^2), tan = sin/cos
	const float cos_sq = cos_outer * cos_outer;
	const float sin_outer =
		std::sqrt(1.0f - cos_sq > 0.0f ? 1.0f - cos_sq : 0.0f);
	const float tan_outer = sin_outer / cos_outer;

	const float half_range = range * 0.5f;

	// TWO valid bounds, the tighter wins (documented on the header):
	// (a) the mid-cone sphere: center = apex + dir*range/2, radius =
	//     sqrt((range/2)^2 + (range*tan(outer))^2) — tight for narrow
	//     cones, diverges (inf/NaN) as outer -> 90 degrees;
	// (b) the apex sphere: center = apex, radius = range — every cone
	//     point sits within range of the apex BY CONSTRUCTION, so this
	//     bound holds for ANY outer angle (incl. >= 90 degrees where the
	//     tan formula is meaningless) and wins for wide cones.
	// A NaN formula radius fails the < comparison and falls to (b) —
	// never a fault on user angles
	// (a) the mid-cone sphere's radius — valid only for outer < 90 degrees
	//     (the cone-opening picture breaks past a hemisphere: tan goes
	//     negative and the formula shrinks — the cos > 0 guard below
	//     rejects that regime; inf/NaN fail the < comparison)
	const float rim = range * tan_outer;
	const float mid_radius =
		std::sqrt(half_range * half_range + rim * rim);

	if (cos_outer > 0.0f && mid_radius < range)
	{
		p_out_center_xyz[0] = p_apex_xyz[0] + p_dir_from_xyz[0] * half_range;
		p_out_center_xyz[1] = p_apex_xyz[1] + p_dir_from_xyz[1] * half_range;
		p_out_center_xyz[2] = p_apex_xyz[2] + p_dir_from_xyz[2] * half_range;
		out_radius = mid_radius;
	}
	else
	{
		p_out_center_xyz[0] = p_apex_xyz[0];
		p_out_center_xyz[1] = p_apex_xyz[1];
		p_out_center_xyz[2] = p_apex_xyz[2];
		out_radius = range;
	}
}

bool zircon_render_light_buffer::project_sphere_to_tile_range(
	const float* p_view_projection, const float* p_center_xyz, float radius,
	kotek::uint32_t screen_width, kotek::uint32_t screen_height,
	kotek::uint32_t tile_pixels, kotek::uint32_t tiles_x,
	kotek::uint32_t tiles_y, kotek::uint32_t& out_tile_min_x,
	kotek::uint32_t& out_tile_min_y, kotek::uint32_t& out_tile_max_x,
	kotek::uint32_t& out_tile_max_y) noexcept
{
	KOTEK_ASSERT(p_view_projection, "must be valid");
	KOTEK_ASSERT(p_center_xyz, "must be valid");
	KOTEK_ASSERT(screen_width && screen_height, "the screen must be sized");
	KOTEK_ASSERT(tile_pixels, "must be positive");
	KOTEK_ASSERT(tiles_x && tiles_y, "the grid must be sized");

	if (p_view_projection == nullptr || p_center_xyz == nullptr ||
		screen_width == 0 || screen_height == 0 || tile_pixels == 0 ||
		tiles_x == 0 || tiles_y == 0)
	{
		return false;
	}

	float min_px = 0.0f;
	float max_px = 0.0f;
	float min_py = 0.0f;
	float max_py = 0.0f;

	// the sphere's world AABB corners project; the projected convex hull
	// of a convex set fully in front of the camera is bounded by the
	// projected corners (projective maps preserve convexity where w > 0),
	// and the sphere sits inside its AABB — conservative
	for (kotek::uint32_t corner = 0; corner < 8; ++corner)
	{
		const float corner_x = p_center_xyz[0] +
			((corner & 1) ? radius : -radius);
		const float corner_y = p_center_xyz[1] +
			((corner & 2) ? radius : -radius);
		const float corner_z = p_center_xyz[2] +
			((corner & 4) ? radius : -radius);

		// clip[j] = dot((x,y,z,1), (vp[j], vp[4+j], vp[8+j], vp[12+j])) —
		// the transform the shaders' column_major mul(u_viewProj, v)
		// computes over the bx byte layout (translation at [12..14])
		float clip[4];

		for (int component = 0; component < 4; ++component)
		{
			clip[component] = corner_x * p_view_projection[component] +
				corner_y * p_view_projection[4 + component] +
				corner_z * p_view_projection[8 + component] +
				p_view_projection[12 + component];
		}

		// the sphere crosses the camera plane (or sits fully behind it):
		// the projective bound degenerates — the conservative answer is
		// the whole grid
		if (clip[3] <= 0.0f)
		{
			out_tile_min_x = 0;
			out_tile_min_y = 0;
			out_tile_max_x = tiles_x - 1;
			out_tile_max_y = tiles_y - 1;
			return true;
		}

		const float ndc_x = clip[0] / clip[3];
		const float ndc_y = clip[1] / clip[3];

		const float pixel_x = (ndc_x * 0.5f + 0.5f) *
			static_cast<float>(screen_width);
		// ndc +y is up, pixel rows run down — the flip
		const float pixel_y = (0.5f - ndc_y * 0.5f) *
			static_cast<float>(screen_height);

		if (corner == 0)
		{
			min_px = max_px = pixel_x;
			min_py = max_py = pixel_y;
		}
		else
		{
			if (pixel_x < min_px)
				min_px = pixel_x;
			if (pixel_x > max_px)
				max_px = pixel_x;
			if (pixel_y < min_py)
				min_py = pixel_y;
			if (pixel_y > max_py)
				max_py = pixel_y;
		}
	}

	const float inv_tile = 1.0f / static_cast<float>(tile_pixels);

	auto pixel_to_tile = [inv_tile](float pixel) -> int
	{
		return static_cast<int>(std::floor(pixel * inv_tile));
	};

	const int raw_min_x = pixel_to_tile(min_px);
	const int raw_max_x = pixel_to_tile(max_px);
	const int raw_min_y = pixel_to_tile(min_py);
	const int raw_max_y = pixel_to_tile(max_py);

	auto clamp_tile = [](int tile, int tile_count) -> kotek::uint32_t
	{
		if (tile < 0)
			return 0;
		if (tile >= tile_count)
			return static_cast<kotek::uint32_t>(tile_count - 1);
		return static_cast<kotek::uint32_t>(tile);
	};

	out_tile_min_x = clamp_tile(raw_min_x, static_cast<int>(tiles_x));
	out_tile_max_x = clamp_tile(raw_max_x, static_cast<int>(tiles_x));
	out_tile_min_y = clamp_tile(raw_min_y, static_cast<int>(tiles_y));
	out_tile_max_y = clamp_tile(raw_max_y, static_cast<int>(tiles_y));

	return true;
}

void zircon_render_light_buffer::bin_lights(
	const zircon_light_stream_record_t* p_positions,
	const zircon_light_stream_record_t* p_directions,
	const zircon_light_stream_record_t* p_params, kotek::uint32_t light_count,
	const float* p_planes_6x4, const float* p_view_projection,
	kotek::uint32_t screen_width, kotek::uint32_t screen_height,
	kotek::uint32_t tile_pixels, kotek::uint32_t tiles_x,
	kotek::uint32_t tiles_y, kotek::uint32_t max_lights_per_tile,
	kotek::uint32_t* p_out_tile_counts, kotek::uint32_t* p_out_tile_indices,
	zircon_light_bin_stats_t& out_stats) noexcept
{
	KOTEK_ASSERT(p_positions, "must be valid");
	KOTEK_ASSERT(p_directions, "must be valid");
	KOTEK_ASSERT(p_params, "must be valid");
	KOTEK_ASSERT(p_planes_6x4, "must be valid");
	KOTEK_ASSERT(p_view_projection, "must be valid");
	KOTEK_ASSERT(p_out_tile_counts, "must be valid storage");
	KOTEK_ASSERT(p_out_tile_indices, "must be valid storage");

	out_stats.m_assignment_count = 0;
	out_stats.m_dropped_count = 0;
	out_stats.m_culled_count = 0;

	if (p_positions == nullptr || p_directions == nullptr ||
		p_params == nullptr || p_planes_6x4 == nullptr ||
		p_view_projection == nullptr || p_out_tile_counts == nullptr ||
		p_out_tile_indices == nullptr || tiles_x == 0 || tiles_y == 0 ||
		max_lights_per_tile == 0)
	{
		return;
	}

	const kotek::uint32_t tile_count = tiles_x * tiles_y;

	for (kotek::uint32_t light_index = 0; light_index < light_count;
		 ++light_index)
	{
		kotek::uint32_t type_bits = 0;

		std::memcpy(&type_bits, &p_params[light_index].m_v[2],
			sizeof(kotek::uint32_t));

		kotek::uint32_t tile_min_x = 0;
		kotek::uint32_t tile_min_y = 0;
		kotek::uint32_t tile_max_x = 0;
		kotek::uint32_t tile_max_y = 0;

		if (type_bits ==
			static_cast<kotek::uint32_t>(eZirconLightType::kDirectional))
		{
			// the directional touches every tile
			tile_max_x = tiles_x - 1;
			tile_max_y = tiles_y - 1;
		}
		else
		{
			float center_xyz[3] = {p_positions[light_index].m_v[0],
				p_positions[light_index].m_v[1],
				p_positions[light_index].m_v[2]};
			float radius = p_positions[light_index].m_v[3];

			if (type_bits ==
				static_cast<kotek::uint32_t>(eZirconLightType::kSpot))
			{
				// the cone classifies through its bounding sphere (the v1
				// conservative bound — see spot_bounding_sphere)
				float sphere_center[3];
				float sphere_radius = 0.0f;

				spot_bounding_sphere(center_xyz,
					p_directions[light_index].m_v, radius,
					p_params[light_index].m_v[1], sphere_center,
					sphere_radius);

				center_xyz[0] = sphere_center[0];
				center_xyz[1] = sphere_center[1];
				center_xyz[2] = sphere_center[2];
				radius = sphere_radius;
			}

			// a light fully outside the frustum touches nothing
			if (sphere_vs_frustum(p_planes_6x4, center_xyz, radius) == false)
			{
				++out_stats.m_culled_count;
				continue;
			}

			if (project_sphere_to_tile_range(p_view_projection, center_xyz,
					radius, screen_width, screen_height, tile_pixels,
					tiles_x, tiles_y, tile_min_x, tile_min_y, tile_max_x,
					tile_max_y) == false)
			{
				continue;
			}
		}

		for (kotek::uint32_t tile_y = tile_min_y; tile_y <= tile_max_y;
			 ++tile_y)
		{
			for (kotek::uint32_t tile_x = tile_min_x; tile_x <= tile_max_x;
				 ++tile_x)
			{
				const kotek::uint32_t tile = tile_y * tiles_x + tile_x;

				KOTEK_ASSERT(tile < tile_count, "tile index overflow");

				// the count grows past the cap (the shader's unconditional
				// InterlockedAdd is the only race-free form — the mirror
				// matches it); the write happens while the pre-increment
				// slot is in range, else the drop lands in the counter.
				// The fragment side clamps min(count, cap)
				kotek::uint32_t& tile_light_count = p_out_tile_counts[tile];
				const kotek::uint32_t slot = tile_light_count;

				++tile_light_count;

				if (slot < max_lights_per_tile)
				{
					p_out_tile_indices[tile * max_lights_per_tile + slot] =
						light_index;

					++out_stats.m_assignment_count;
				}
				else
				{
					++out_stats.m_dropped_count;
				}
			}
		}
	}
}

void zircon_render_light_buffer::evaluate_directional_diffuse(
	const float* p_normal_xyz, const float* p_light_dir_from_xyz,
	const float* p_color_rgb, float* p_out_rgb) noexcept
{
	KOTEK_ASSERT(p_normal_xyz, "must be valid");
	KOTEK_ASSERT(p_light_dir_from_xyz, "must be valid");
	KOTEK_ASSERT(p_color_rgb, "must be valid");
	KOTEK_ASSERT(p_out_rgb, "must be valid storage");

	if (p_normal_xyz == nullptr || p_light_dir_from_xyz == nullptr ||
		p_color_rgb == nullptr || p_out_rgb == nullptr)
	{
		return;
	}

	const float ndotl_raw = -(p_normal_xyz[0] * p_light_dir_from_xyz[0] +
		p_normal_xyz[1] * p_light_dir_from_xyz[1] +
		p_normal_xyz[2] * p_light_dir_from_xyz[2]);
	const float ndotl =
		ndotl_raw < 0.0f ? 0.0f : (ndotl_raw > 1.0f ? 1.0f : ndotl_raw);

	for (int channel = 0; channel < 3; ++channel)
	{
		p_out_rgb[channel] = p_color_rgb[channel] * ndotl;
	}
}

void zircon_render_light_buffer::evaluate_point_diffuse(
	const float* p_normal_xyz, const float* p_world_pos_xyz,
	const float* p_light_pos_xyz, const float* p_color_rgb, float range,
	float* p_out_rgb) noexcept
{
	KOTEK_ASSERT(p_normal_xyz, "must be valid");
	KOTEK_ASSERT(p_world_pos_xyz, "must be valid");
	KOTEK_ASSERT(p_light_pos_xyz, "must be valid");
	KOTEK_ASSERT(p_color_rgb, "must be valid");
	KOTEK_ASSERT(range > 0.0f, "the collection clamps the range");
	KOTEK_ASSERT(p_out_rgb, "must be valid storage");

	if (p_normal_xyz == nullptr || p_world_pos_xyz == nullptr ||
		p_light_pos_xyz == nullptr || p_color_rgb == nullptr ||
		p_out_rgb == nullptr || range <= 0.0f)
	{
		return;
	}

	const float to_light[3] = {p_light_pos_xyz[0] - p_world_pos_xyz[0],
		p_light_pos_xyz[1] - p_world_pos_xyz[1],
		p_light_pos_xyz[2] - p_world_pos_xyz[2]};

	const float distance = std::sqrt(to_light[0] * to_light[0] +
		to_light[1] * to_light[1] + to_light[2] * to_light[2]);

	// the safe direction (the degenerate overlap contributes 0 — the
	// direction is singular at the light's exact position)
	const float inv_distance =
		1.0f / (distance > 1e-6f ? distance : 1e-6f);

	const float dir_to[3] = {to_light[0] * inv_distance,
		to_light[1] * inv_distance, to_light[2] * inv_distance};

	const float ndotl_raw = p_normal_xyz[0] * dir_to[0] +
		p_normal_xyz[1] * dir_to[1] + p_normal_xyz[2] * dir_to[2];
	const float ndotl =
		ndotl_raw < 0.0f ? 0.0f : (ndotl_raw > 1.0f ? 1.0f : ndotl_raw);

	const float window_raw = 1.0f - distance / range;
	const float attenuation =
		window_raw < 0.0f ? 0.0f : (window_raw > 1.0f ? 1.0f : window_raw);

	for (int channel = 0; channel < 3; ++channel)
	{
		p_out_rgb[channel] = p_color_rgb[channel] * (ndotl * attenuation);
	}
}

void zircon_render_light_buffer::evaluate_spot_diffuse(
	const float* p_normal_xyz, const float* p_world_pos_xyz,
	const float* p_light_pos_xyz, const float* p_light_dir_from_xyz,
	const float* p_color_rgb, float range, float cos_inner,
	float cos_outer, float* p_out_rgb) noexcept
{
	KOTEK_ASSERT(p_normal_xyz, "must be valid");
	KOTEK_ASSERT(p_world_pos_xyz, "must be valid");
	KOTEK_ASSERT(p_light_pos_xyz, "must be valid");
	KOTEK_ASSERT(p_light_dir_from_xyz, "must be valid");
	KOTEK_ASSERT(p_color_rgb, "must be valid");
	KOTEK_ASSERT(p_out_rgb, "must be valid storage");

	if (p_normal_xyz == nullptr || p_world_pos_xyz == nullptr ||
		p_light_pos_xyz == nullptr || p_light_dir_from_xyz == nullptr ||
		p_color_rgb == nullptr || p_out_rgb == nullptr || range <= 0.0f)
	{
		return;
	}

	const float to_light[3] = {p_light_pos_xyz[0] - p_world_pos_xyz[0],
		p_light_pos_xyz[1] - p_world_pos_xyz[1],
		p_light_pos_xyz[2] - p_world_pos_xyz[2]};

	const float distance = std::sqrt(to_light[0] * to_light[0] +
		to_light[1] * to_light[1] + to_light[2] * to_light[2]);

	const float inv_distance =
		1.0f / (distance > 1e-6f ? distance : 1e-6f);

	const float dir_to[3] = {to_light[0] * inv_distance,
		to_light[1] * inv_distance, to_light[2] * inv_distance};

	const float ndotl_raw = p_normal_xyz[0] * dir_to[0] +
		p_normal_xyz[1] * dir_to[1] + p_normal_xyz[2] * dir_to[2];
	const float ndotl =
		ndotl_raw < 0.0f ? 0.0f : (ndotl_raw > 1.0f ? 1.0f : ndotl_raw);

	const float window_raw = 1.0f - distance / range;
	const float attenuation =
		window_raw < 0.0f ? 0.0f : (window_raw > 1.0f ? 1.0f : window_raw);

	// the cone window: the angle between the apex->fragment direction and
	// the cone axis vs the inner/outer cosines
	const float cos_angle = -(dir_to[0] * p_light_dir_from_xyz[0] +
		dir_to[1] * p_light_dir_from_xyz[1] +
		dir_to[2] * p_light_dir_from_xyz[2]);

	const float cone_delta = cos_inner - cos_outer;
	const float cone_raw = (cos_angle - cos_outer) /
		(cone_delta > 1e-5f ? cone_delta : 1e-5f);
	const float cone =
		cone_raw < 0.0f ? 0.0f : (cone_raw > 1.0f ? 1.0f : cone_raw);

	for (int channel = 0; channel < 3; ++channel)
	{
		p_out_rgb[channel] =
			p_color_rgb[channel] * (ndotl * attenuation * cone);
	}
}
