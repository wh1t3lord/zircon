#include "zircon_render_shadow_cascade.h"

#include <cmath>
#include <cstring>

void zircon_render_shadow_cascade::multiply_4x4(const float* p_a_16,
	const float* p_b_16, float* p_out_16) noexcept
{
	KOTEK_ASSERT(p_a_16, "must be valid");
	KOTEK_ASSERT(p_b_16, "must be valid");
	KOTEK_ASSERT(p_out_16, "must be valid storage");

	if (p_a_16 == nullptr || p_b_16 == nullptr || p_out_16 == nullptr)
		return;

	// out[r*4+c] = sum_j a[r*4+j] * b[j*4+c] — the bx::mtxMul formula
	// (vec4MulMtx per row), re-derived so this module stays bgfx-free
	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			float sum = 0.0f;

			for (int step = 0; step < 4; ++step)
			{
				sum += p_a_16[row * 4 + step] * p_b_16[step * 4 + column];
			}

			p_out_16[row * 4 + column] = sum;
		}
	}
}

void zircon_render_shadow_cascade::transform_point_raw(const float* p_m_16,
	const float* p_point_xyzw, float* p_out_xyzw) noexcept
{
	KOTEK_ASSERT(p_m_16, "must be valid");
	KOTEK_ASSERT(p_point_xyzw, "must be valid");
	KOTEK_ASSERT(p_out_xyzw, "must be valid storage");

	if (p_m_16 == nullptr || p_point_xyzw == nullptr ||
		p_out_xyzw == nullptr)
	{
		return;
	}

	// clip[c] = dot(v, (m[c], m[4+c], m[8+c], m[12+c])) — the shaders'
	// column_major mul() over the same bytes (the light buffer's
	// projection formula)
	for (int component = 0; component < 4; ++component)
	{
		p_out_xyzw[component] = p_point_xyzw[0] * p_m_16[component] +
			p_point_xyzw[1] * p_m_16[4 + component] +
			p_point_xyzw[2] * p_m_16[8 + component] +
			p_point_xyzw[3] * p_m_16[12 + component];
	}
}

bool zircon_render_shadow_cascade::invert_4x4(
	const float* p_m_16, float* p_out_16) noexcept
{
	KOTEK_ASSERT(p_m_16, "must be valid");
	KOTEK_ASSERT(p_out_16, "must be valid storage");

	if (p_m_16 == nullptr || p_out_16 == nullptr)
		return false;

	// the adjugate inverse over the plain row-major matrix A[r][c] =
	// m[r*4+c]: inv = cofactor^T / det. Convention-free algebra — the
	// result inverts the transform under multiply_4x4 regardless of the
	// vector convention
	auto minor3 = [p_m_16](int r0, int r1, int r2, int c0, int c1,
					  int c2) -> float
	{
		const float a = p_m_16[r0 * 4 + c0];
		const float b = p_m_16[r0 * 4 + c1];
		const float c = p_m_16[r0 * 4 + c2];
		const float d = p_m_16[r1 * 4 + c0];
		const float e = p_m_16[r1 * 4 + c1];
		const float f = p_m_16[r1 * 4 + c2];
		const float g = p_m_16[r2 * 4 + c0];
		const float h = p_m_16[r2 * 4 + c1];
		const float i = p_m_16[r2 * 4 + c2];

		return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
	};

	auto cofactor = [&minor3](int row, int column) -> float
	{
		int rows[3];
		int columns[3];
		int ri = 0;
		int ci = 0;

		for (int r = 0; r < 4; ++r)
		{
			if (r != row)
				rows[ri++] = r;
			if (r != column)
				columns[ci++] = r;
		}

		const float minor =
			minor3(rows[0], rows[1], rows[2], columns[0], columns[1],
				columns[2]);

		return ((row + column) & 1) ? -minor : minor;
	};

	float cofactors[16];

	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			cofactors[row * 4 + column] = cofactor(row, column);
		}
	}

	// det by the first-row expansion
	const float determinant = p_m_16[0] * cofactors[0] +
		p_m_16[1] * cofactors[1] + p_m_16[2] * cofactors[2] +
		p_m_16[3] * cofactors[3];

	if (std::fabs(determinant) < 1e-20f)
	{
		std::memset(p_out_16, 0, 16 * sizeof(float));
		return false;
	}

	const float inv_det = 1.0f / determinant;

	for (int row = 0; row < 4; ++row)
	{
		for (int column = 0; column < 4; ++column)
		{
			// the adjugate is the cofactor matrix transposed
			p_out_16[row * 4 + column] =
				cofactors[column * 4 + row] * inv_det;
		}
	}

	return true;
}

void zircon_render_shadow_cascade::compute_split_distances(float near_z,
	float far_z, kotek::uint8_t cascade_count, float lambda,
	float* p_out_splits) noexcept
{
	KOTEK_ASSERT(p_out_splits, "must be valid storage");
	KOTEK_ASSERT(near_z > 0.0f, "the camera near plane must be positive");
	KOTEK_ASSERT(far_z > near_z, "the far plane must be past the near");
	KOTEK_ASSERT(cascade_count > 0, "must be positive");

	if (p_out_splits == nullptr || cascade_count == 0)
		return;

	p_out_splits[0] = near_z;
	p_out_splits[cascade_count] = far_z;

	const float ratio = far_z / near_z;
	const float range = far_z - near_z;

	for (kotek::uint8_t cascade = 1; cascade < cascade_count; ++cascade)
	{
		const float fraction =
			static_cast<float>(cascade) / static_cast<float>(cascade_count);

		const float split_log = near_z * std::pow(ratio, fraction);
		const float split_uniform = near_z + range * fraction;

		p_out_splits[cascade] =
			lambda * split_log + (1.0f - lambda) * split_uniform;
	}
}

bool zircon_render_shadow_cascade::extract_perspective_near_far(
	const float* p_projection_16, float& out_near_z,
	float& out_far_z) noexcept
{
	KOTEK_ASSERT(p_projection_16, "must be valid");

	if (p_projection_16 == nullptr)
		return false;

	// the standard row-vector perspective with [0,1] depth: the w producer
	// m[11] = 1, the perspective marker m[15] = 0, d = m[10] = f/(f-n) > 1,
	// e = m[14] = -n*f/(f-n) < 0. Anything else (ortho, infinite or
	// reverse-z) is not the v1 contract
	constexpr float k_eps = 1e-4f;

	if (std::fabs(p_projection_16[11] - 1.0f) > k_eps ||
		std::fabs(p_projection_16[15]) > k_eps)
	{
		return false;
	}

	const float d = p_projection_16[10];
	const float e = p_projection_16[14];

	if (d <= 1.0f + k_eps || e >= 0.0f)
		return false;

	out_near_z = -e / d;
	out_far_z = e / (1.0f - d);

	if (out_near_z <= 0.0f || out_far_z <= out_near_z)
		return false;

	return true;
}

void zircon_render_shadow_cascade::extract_camera_frame(
	const float* p_view_16, float* p_out_eye_xyz,
	float* p_out_forward_xyz) noexcept
{
	KOTEK_ASSERT(p_view_16, "must be valid");
	KOTEK_ASSERT(p_out_eye_xyz, "must be valid storage");
	KOTEK_ASSERT(p_out_forward_xyz, "must be valid storage");

	if (p_view_16 == nullptr || p_out_eye_xyz == nullptr ||
		p_out_forward_xyz == nullptr)
	{
		return;
	}

	// the basis vectors are the strided columns; the last row holds
	// -dot(eye, basis_c) (the pass's camera_position extraction's
	// derivation — see the header)
	const float right[3] = {p_view_16[0], p_view_16[4], p_view_16[8]};
	const float up[3] = {p_view_16[1], p_view_16[5], p_view_16[9]};
	const float forward[3] = {p_view_16[2], p_view_16[6], p_view_16[10]};

	for (int axis = 0; axis < 3; ++axis)
	{
		p_out_eye_xyz[axis] = -(p_view_16[12] * right[axis] +
			p_view_16[13] * up[axis] + p_view_16[14] * forward[axis]);
		p_out_forward_xyz[axis] = forward[axis];
	}
}

bool zircon_render_shadow_cascade::build_light_view(
	const float* p_light_dir_from_xyz, float* p_out_view_16) noexcept
{
	KOTEK_ASSERT(p_light_dir_from_xyz, "must be valid");
	KOTEK_ASSERT(p_out_view_16, "must be valid storage");

	if (p_light_dir_from_xyz == nullptr || p_out_view_16 == nullptr)
		return false;

	const float length = std::sqrt(p_light_dir_from_xyz[0] * p_light_dir_from_xyz[0] +
		p_light_dir_from_xyz[1] * p_light_dir_from_xyz[1] +
		p_light_dir_from_xyz[2] * p_light_dir_from_xyz[2]);

	if (length < 1e-6f)
		return false;

	const float forward[3] = {p_light_dir_from_xyz[0] / length,
		p_light_dir_from_xyz[1] / length, p_light_dir_from_xyz[2] / length};

	// the up hint: world up, replaced when nearly parallel to the light
	float up_hint[3] = {0.0f, 1.0f, 0.0f};

	const float parallel = std::fabs(forward[0] * up_hint[0] +
		forward[1] * up_hint[1] + forward[2] * up_hint[2]);

	if (parallel > 0.999f)
	{
		up_hint[0] = 0.0f;
		up_hint[1] = 0.0f;
		up_hint[2] = 1.0f;
	}

	// right = normalize(cross(up_hint, forward)); up = cross(forward,
	// right) — a left-handed-looking frame whose only requirement is
	// consistency between the depth pass and the FS sampling (both go
	// through this one matrix)
	float right[3] = {up_hint[1] * forward[2] - up_hint[2] * forward[1],
		up_hint[2] * forward[0] - up_hint[0] * forward[2],
		up_hint[0] * forward[1] - up_hint[1] * forward[0]};

	const float right_length = std::sqrt(right[0] * right[0] +
		right[1] * right[1] + right[2] * right[2]);

	if (right_length < 1e-6f)
		return false;

	right[0] /= right_length;
	right[1] /= right_length;
	right[2] /= right_length;

	const float up[3] = {forward[1] * right[2] - forward[2] * right[1],
		forward[2] * right[0] - forward[0] * right[2],
		forward[0] * right[1] - forward[1] * right[0]};

	// the ROTATION-ONLY view: the basis vectors are the strided columns,
	// the translation row stays zero (the world-anchored texel grid — see
	// the header)
	std::memset(p_out_view_16, 0, 16 * sizeof(float));

	p_out_view_16[0] = right[0];
	p_out_view_16[4] = right[1];
	p_out_view_16[8] = right[2];

	p_out_view_16[1] = up[0];
	p_out_view_16[5] = up[1];
	p_out_view_16[9] = up[2];

	p_out_view_16[2] = forward[0];
	p_out_view_16[6] = forward[1];
	p_out_view_16[10] = forward[2];

	p_out_view_16[15] = 1.0f;

	return true;
}

void zircon_render_shadow_cascade::build_ortho_projection(float min_x,
	float max_x, float min_y, float max_y, float min_z, float max_z,
	float* p_out_projection_16) noexcept
{
	KOTEK_ASSERT(p_out_projection_16, "must be valid storage");
	KOTEK_ASSERT(max_x > min_x && max_y > min_y && max_z > min_z,
		"the ortho window must be non-degenerate");

	if (p_out_projection_16 == nullptr)
		return;

	const float extent_x = max_x - min_x;
	const float extent_y = max_y - min_y;
	const float extent_z = max_z - min_z;

	if (extent_x <= 0.0f || extent_y <= 0.0f || extent_z <= 0.0f)
		return;

	std::memset(p_out_projection_16, 0, 16 * sizeof(float));

	// x/y map [min,max] -> [-1,1], z maps [min,max] -> [0,1] (the
	// row-vector [0,1]-depth ortho: v' = v * M)
	p_out_projection_16[0] = 2.0f / extent_x;
	p_out_projection_16[5] = 2.0f / extent_y;
	p_out_projection_16[10] = 1.0f / extent_z;
	p_out_projection_16[12] = -(max_x + min_x) / extent_x;
	p_out_projection_16[13] = -(max_y + min_y) / extent_y;
	p_out_projection_16[14] = -min_z / extent_z;
	p_out_projection_16[15] = 1.0f;
}

void zircon_render_shadow_cascade::snap_ortho_to_texels(float& io_min_x,
	float& io_max_x, float& io_min_y, float& io_max_y,
	kotek::uint32_t shadow_map_size) noexcept
{
	KOTEK_ASSERT(shadow_map_size > 0, "must be positive");

	if (shadow_map_size == 0)
		return;

	// a degenerate axis (a slice seen edge-on) is widened to one world
	// unit first so the texel size is finite
	if (io_max_x - io_min_x <= 1e-6f)
	{
		io_min_x -= 0.5f;
		io_max_x += 0.5f;
	}

	if (io_max_y - io_min_y <= 1e-6f)
	{
		io_min_y -= 0.5f;
		io_max_y += 0.5f;
	}

	const float inv_size = 1.0f / static_cast<float>(shadow_map_size);

	const float texel_x = (io_max_x - io_min_x) * inv_size;
	const float texel_y = (io_max_y - io_min_y) * inv_size;

	// the window expands OUTWARD to whole texels: the min floors, the max
	// ceils (coverage preserved — never shrink a cascade's fit)
	io_min_x = std::floor(io_min_x / texel_x) * texel_x;
	io_max_x = std::ceil(io_max_x / texel_x) * texel_x;
	io_min_y = std::floor(io_min_y / texel_y) * texel_y;
	io_max_y = std::ceil(io_max_y / texel_y) * texel_y;
}

bool zircon_render_shadow_cascade::compute_frustum_corners(
	const float* p_camera_view_16, const float* p_camera_proj_16,
	float* p_out_corners_8x3) noexcept
{
	KOTEK_ASSERT(p_camera_view_16, "must be valid");
	KOTEK_ASSERT(p_camera_proj_16, "must be valid");
	KOTEK_ASSERT(p_out_corners_8x3, "must be valid storage");

	if (p_camera_view_16 == nullptr || p_camera_proj_16 == nullptr ||
		p_out_corners_8x3 == nullptr)
	{
		return false;
	}

	float view_projection[16];
	multiply_4x4(p_camera_view_16, p_camera_proj_16, view_projection);

	float inverse_vp[16];

	if (invert_4x4(view_projection, inverse_vp) == false)
		return false;

	// the documented corner order: near plane (ndc z = 0) at (x,y) =
	// (-1,-1), (1,-1), (1,1), (-1,1), then the same four at ndc z = 1
	const float corner_x[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
	const float corner_y[4] = {-1.0f, -1.0f, 1.0f, 1.0f};

	for (int corner = 0; corner < 8; ++corner)
	{
		const float ndc[4] = {corner_x[corner & 3], corner_y[corner & 3],
			corner < 4 ? 0.0f : 1.0f, 1.0f};

		float clip[4];

		transform_point_raw(inverse_vp, ndc, clip);

		if (std::fabs(clip[3]) < 1e-20f)
			return false;

		p_out_corners_8x3[corner * 3 + 0] = clip[0] / clip[3];
		p_out_corners_8x3[corner * 3 + 1] = clip[1] / clip[3];
		p_out_corners_8x3[corner * 3 + 2] = clip[2] / clip[3];
	}

	return true;
}

bool zircon_render_shadow_cascade::compute_cascades(
	const float* p_camera_view_16, const float* p_camera_proj_16,
	const float* p_light_dir_from_xyz, kotek::uint32_t shadow_map_size,
	kotek::uint8_t cascade_count, float split_lambda,
	float caster_depth_margin, zircon_shadow_cascade_set_t& out_set) noexcept
{
	KOTEK_ASSERT(p_camera_view_16, "must be valid");
	KOTEK_ASSERT(p_camera_proj_16, "must be valid");
	KOTEK_ASSERT(p_light_dir_from_xyz, "must be valid");

	std::memset(&out_set, 0, sizeof(out_set));

	if (p_camera_view_16 == nullptr || p_camera_proj_16 == nullptr ||
		p_light_dir_from_xyz == nullptr || cascade_count == 0 ||
		cascade_count > ZIRCON_DEF_SHADOW_CASCADE_COUNT ||
		shadow_map_size == 0)
	{
		return false;
	}

	float near_z = 0.0f;
	float far_z = 0.0f;

	if (extract_perspective_near_far(p_camera_proj_16, near_z, far_z) ==
		false)
	{
		return false;
	}

	float splits[ZIRCON_DEF_SHADOW_CASCADE_COUNT + 1];

	compute_split_distances(near_z, far_z, cascade_count, split_lambda,
		splits);

	float corners[8 * 3];

	if (compute_frustum_corners(
			p_camera_view_16, p_camera_proj_16, corners) == false)
	{
		return false;
	}

	float eye[3];
	float camera_forward[3];

	extract_camera_frame(p_camera_view_16, eye, camera_forward);

	float light_view[16];

	if (build_light_view(p_light_dir_from_xyz, light_view) == false)
		return false;

	for (kotek::uint8_t cascade = 0; cascade < cascade_count; ++cascade)
	{
		float min_light[3] = {0.0f, 0.0f, 0.0f};
		float max_light[3] = {0.0f, 0.0f, 0.0f};

		for (int corner = 0; corner < 8; ++corner)
		{
			// the near-corner rays interpolated by view-z (corner(s) = eye
			// + ray * s / near — a corner ray's view-z scales linearly
			// along itself). Corners 4..7 share the rays of corners 0..3
			// (the far corner sits ON its near corner's ray), so the ray
			// always reads the NEAR corner — the far corner's entry would
			// carry an extra far/near factor
			const float split_z = splits[cascade + (corner < 4 ? 0 : 1)];

			float world[3];

			for (int axis = 0; axis < 3; ++axis)
			{
				const float ray =
					corners[(corner & 3) * 3 + axis] - eye[axis];

				world[axis] = eye[axis] + ray * (split_z / near_z);
			}

			const float point4[4] = {world[0], world[1], world[2], 1.0f};
			float light_space[4];

			transform_point_raw(light_view, point4, light_space);

			if (corner == 0)
			{
				for (int axis = 0; axis < 3; ++axis)
				{
					min_light[axis] = light_space[axis];
					max_light[axis] = light_space[axis];
				}
			}
			else
			{
				for (int axis = 0; axis < 3; ++axis)
				{
					if (light_space[axis] < min_light[axis])
						min_light[axis] = light_space[axis];
					if (light_space[axis] > max_light[axis])
						max_light[axis] = light_space[axis];
				}
			}
		}

		// the z range reaches BACK toward the sun by the caster margin
		// (casters sit between the sun and the slice); a degenerate depth
		// widens to one meter so the ortho is finite
		float min_z = min_light[2] - caster_depth_margin;
		float max_z = max_light[2];

		if (max_z - min_z <= 1e-6f)
		{
			min_z -= 0.5f;
			max_z += 0.5f;
		}

		float min_x = min_light[0];
		float max_x = max_light[0];
		float min_y = min_light[1];
		float max_y = max_light[1];

		snap_ortho_to_texels(min_x, max_x, min_y, max_y, shadow_map_size);

		std::memcpy(out_set.m_cascade_view[cascade], light_view,
			16 * sizeof(float));

		build_ortho_projection(min_x, max_x, min_y, max_y, min_z, max_z,
			out_set.m_cascade_proj[cascade]);

		multiply_4x4(out_set.m_cascade_view[cascade],
			out_set.m_cascade_proj[cascade],
			out_set.m_cascade_view_proj[cascade]);

		out_set.m_split_far[cascade] = splits[cascade + 1];
	}

	out_set.m_cascade_count = cascade_count;

	return true;
}

kotek::uint8_t zircon_render_shadow_cascade::select_cascade(float view_z,
	const float* p_split_far_per_cascade,
	kotek::uint8_t cascade_count) noexcept
{
	KOTEK_ASSERT(p_split_far_per_cascade, "must be valid");

	if (p_split_far_per_cascade == nullptr)
		return cascade_count;

	for (kotek::uint8_t cascade = 0; cascade < cascade_count; ++cascade)
	{
		if (view_z <= p_split_far_per_cascade[cascade])
			return cascade;
	}

	return cascade_count;
}

bool zircon_render_shadow_cascade::world_to_light_texel(
	const float* p_cascade_view_proj_16, const float* p_world_xyz,
	kotek::uint32_t shadow_map_size, float* p_out_texel_xy,
	float& out_depth) noexcept
{
	KOTEK_ASSERT(p_cascade_view_proj_16, "must be valid");
	KOTEK_ASSERT(p_world_xyz, "must be valid");
	KOTEK_ASSERT(p_out_texel_xy, "must be valid storage");

	if (p_cascade_view_proj_16 == nullptr || p_world_xyz == nullptr ||
		p_out_texel_xy == nullptr || shadow_map_size == 0)
	{
		return false;
	}

	const float point4[4] = {p_world_xyz[0], p_world_xyz[1], p_world_xyz[2],
		1.0f};
	float clip[4];

	transform_point_raw(p_cascade_view_proj_16, point4, clip);

	if (clip[3] <= 0.0f)
		return false;

	const float ndc_x = clip[0] / clip[3];
	const float ndc_y = clip[1] / clip[3];

	// ndc -> uv (the pixel-rows-down flip, consistent with the RT's row 0
	// at ndc y = +1) -> texel space
	p_out_texel_xy[0] = (ndc_x * 0.5f + 0.5f) *
		static_cast<float>(shadow_map_size);
	p_out_texel_xy[1] = (0.5f - ndc_y * 0.5f) *
		static_cast<float>(shadow_map_size);
	out_depth = clip[2] / clip[3];

	return true;
}

float zircon_render_shadow_cascade::compare_shadow_depth(float stored_depth,
	float receiver_depth, float bias) noexcept
{
	return (receiver_depth - bias) <= stored_depth ? 1.0f : 0.0f;
}

float zircon_render_shadow_cascade::sample_shadow_pcf_3x3(
	const float* p_depth_layer, kotek::uint32_t shadow_map_size,
	float texel_x, float texel_y, float receiver_depth,
	float bias) noexcept
{
	KOTEK_ASSERT(p_depth_layer, "must be valid");
	KOTEK_ASSERT(shadow_map_size > 0, "must be positive");

	if (p_depth_layer == nullptr || shadow_map_size == 0)
		return 1.0f;

	const int base_x = static_cast<int>(std::floor(texel_x));
	const int base_y = static_cast<int>(std::floor(texel_y));

	const int edge_max = static_cast<int>(shadow_map_size) - 1;

	float lit_sum = 0.0f;

	for (int offset_y = -1; offset_y <= 1; ++offset_y)
	{
		for (int offset_x = -1; offset_x <= 1; ++offset_x)
		{
			int tap_x = base_x + offset_x;
			int tap_y = base_y + offset_y;

			if (tap_x < 0)
				tap_x = 0;
			if (tap_x > edge_max)
				tap_x = edge_max;
			if (tap_y < 0)
				tap_y = 0;
			if (tap_y > edge_max)
				tap_y = edge_max;

			const float stored =
				p_depth_layer[tap_y * shadow_map_size + tap_x];

			lit_sum +=
				compare_shadow_depth(stored, receiver_depth, bias);
		}
	}

	return lit_sum * (1.0f / 9.0f);
}
