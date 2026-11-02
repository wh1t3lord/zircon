#include "zircon_render_shadow_rig.h"

#include "zircon_render_light_buffer.h"

#include <cfloat>
#include <cstring>

zircon_render_shadow_rig::zircon_render_shadow_rig(void) :
	m_depth_array{BGFX_INVALID_HANDLE},
	m_cascade_frame_buffers{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
		BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE},
	m_program_depth{BGFX_INVALID_HANDLE},
	m_uniform_cascade_view_proj{BGFX_INVALID_HANDLE},
	m_uniform_split_far{BGFX_INVALID_HANDLE},
	m_uniform_meta{BGFX_INVALID_HANDLE},
	m_uniform_shadow_map{BGFX_INVALID_HANDLE},
	m_shadow_meta{0.0f, 0.0f, 0.0f, 0.0f},
	m_caster_light_index{zircon_render_light_buffer::kNoShadowCaster},
	m_active{false},
	m_view_order_installed{false},
	m_first_render_logged{false},
	m_p_log_tag{nullptr}
{
}

void zircon_render_shadow_rig::create(const char* p_log_tag) noexcept
{
	KOTEK_ASSERT(p_log_tag, "must be valid");

	this->m_p_log_tag = p_log_tag ? p_log_tag : "[shadow]";

	// the cascade depth array: ONE D32F texture of
	// ZIRCON_DEF_SHADOW_CASCADE_COUNT layers, render-target flagged (the
	// header documents the array-vs-separate-textures choice); d3d11
	// creates it R32_TYPELESS with a per-layer DSV and an R32_FLOAT
	// Texture2DArray SRV
	this->m_depth_array = bgfx::createTexture2D(ZIRCON_DEF_SHADOW_MAP_SIZE,
		ZIRCON_DEF_SHADOW_MAP_SIZE, false, ZIRCON_DEF_SHADOW_CASCADE_COUNT,
		bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT);

	KOTEK_ASSERT(bgfx::isValid(this->m_depth_array),
		"failed to create the shadow cascade depth array!");

	for (kotek::uint8_t cascade = 0;
		 cascade < ZIRCON_DEF_SHADOW_CASCADE_COUNT; ++cascade)
	{
		bgfx::Attachment attachment;

		attachment.init(this->m_depth_array, bgfx::Access::Write, cascade, 1,
			0, BGFX_RESOLVE_NONE);

		this->m_cascade_frame_buffers[cascade] =
			bgfx::createFrameBuffer(1, &attachment, false);

		KOTEK_ASSERT(
			bgfx::isValid(this->m_cascade_frame_buffers[cascade]),
			"failed to create the shadow cascade frame buffer!");
	}

	// the FS cbuffer members (the banner's layout; u_view is a bgfx
	// predefined and is NEVER created here — the documented trap)
	this->m_uniform_cascade_view_proj = bgfx::createUniform(
		"u_shadowCascadeViewProj", bgfx::UniformType::Mat4,
		ZIRCON_DEF_SHADOW_CASCADE_COUNT);
	this->m_uniform_split_far =
		bgfx::createUniform("u_shadowSplitFar", bgfx::UniformType::Vec4);
	this->m_uniform_meta =
		bgfx::createUniform("u_shadowMeta", bgfx::UniformType::Vec4);
	this->m_uniform_shadow_map =
		bgfx::createUniform("u_shadowMap", bgfx::UniformType::Sampler);

	KOTEK_ASSERT(bgfx::isValid(this->m_uniform_cascade_view_proj) &&
			bgfx::isValid(this->m_uniform_split_far) &&
			bgfx::isValid(this->m_uniform_meta) &&
			bgfx::isValid(this->m_uniform_shadow_map),
		"failed to create the shadow rig's uniforms!");

	KOTEK_MESSAGE(
		"{} shadow rig created: {} cascade layers of {}x{} D32F (the CSM sun "
		"path — spot/point atlases deferred)",
		this->m_p_log_tag,
		static_cast<kotek::uint32_t>(ZIRCON_DEF_SHADOW_CASCADE_COUNT),
		static_cast<kotek::uint32_t>(ZIRCON_DEF_SHADOW_MAP_SIZE),
		static_cast<kotek::uint32_t>(ZIRCON_DEF_SHADOW_MAP_SIZE));
}

void zircon_render_shadow_rig::set_depth_program(
	bgfx::ProgramHandle program_depth) noexcept
{
	this->m_program_depth = program_depth;
}

void zircon_render_shadow_rig::destroy(void) noexcept
{
	// the view order is persistent Context state — restore the identity
	// before the textures go away so a rebuilt pass never inherits it
	this->restore_view_order();

	if (bgfx::isValid(this->m_program_depth))
	{
		bgfx::destroy(this->m_program_depth);
		this->m_program_depth = BGFX_INVALID_HANDLE;
	}

	bgfx::UniformHandle* p_uniforms[] = {&this->m_uniform_cascade_view_proj,
		&this->m_uniform_split_far, &this->m_uniform_meta,
		&this->m_uniform_shadow_map};

	for (bgfx::UniformHandle* p_uniform : p_uniforms)
	{
		if (bgfx::isValid(*p_uniform))
		{
			bgfx::destroy(*p_uniform);
			*p_uniform = BGFX_INVALID_HANDLE;
		}
	}

	for (bgfx::FrameBufferHandle& frame_buffer :
		this->m_cascade_frame_buffers)
	{
		if (bgfx::isValid(frame_buffer))
		{
			bgfx::destroy(frame_buffer);
			frame_buffer = BGFX_INVALID_HANDLE;
		}
	}

	if (bgfx::isValid(this->m_depth_array))
	{
		bgfx::destroy(this->m_depth_array);
		this->m_depth_array = BGFX_INVALID_HANDLE;
	}

	this->m_active = false;
	this->m_first_render_logged = false;
}

bool zircon_render_shadow_rig::is_shadow_ready(void) const noexcept
{
	return bgfx::isValid(this->m_program_depth) &&
		bgfx::isValid(this->m_depth_array);
}

void zircon_render_shadow_rig::render_cascades(
	const zircon_shadow_cascade_set_t& cascades,
	const zircon_render_chunk_pool& pool,
	bgfx::DynamicVertexBufferHandle vertex_pool,
	bgfx::DynamicIndexBufferHandle index_pool,
	kotek::uint32_t caster_light_index) noexcept
{
	if (this->is_shadow_ready() == false ||
		cascades.m_cascade_count == 0)
	{
		this->set_inactive();
		return;
	}

	this->m_active = true;
	this->m_caster_light_index = caster_light_index;

	// per cascade: cull the pool against the cascade VP's frustum (the
	// SAME B1 AABB statics — the scope's contract) and draw the visible
	// chunks depth-only into the cascade's layer
	for (kotek::uint8_t cascade = 0; cascade < cascades.m_cascade_count;
		 ++cascade)
	{
		const bgfx::ViewId view_id = static_cast<bgfx::ViewId>(
			ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + cascade);

		bgfx::setViewFrameBuffer(
			view_id, this->m_cascade_frame_buffers[cascade]);
		bgfx::setViewRect(view_id, 0, 0, ZIRCON_DEF_SHADOW_MAP_SIZE,
			ZIRCON_DEF_SHADOW_MAP_SIZE);
		bgfx::setViewClear(view_id, BGFX_CLEAR_DEPTH);
		bgfx::setViewTransform(view_id,
			cascades.m_cascade_view[cascade],
			cascades.m_cascade_proj[cascade]);

		zircon_render_chunk_pool::extract_frustum_planes(
			cascades.m_cascade_view_proj[cascade], this->m_cascade_planes);

		const kotek::uint32_t caster_count =
			pool.cull_chunks_against_frustum(this->m_cascade_planes,
				this->m_visible_ids,
				zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

		// the clear must execute even with zero casters in the slice
		bgfx::touch(view_id);

		const zircon_chunk_ranges_t* p_ranges = pool.get_ranges();

		for (kotek::uint32_t caster = 0; caster < caster_count; ++caster)
		{
			const zircon_chunk_ranges_t& ranges =
				p_ranges[this->m_visible_ids[caster]];

			// the chunk's vertices are pool-local and its indices
			// chunk-local: the vertex buffer's start offset shifts the
			// stream origin (d3d11: the VB byte offset —
			// renderer_d3d11.cpp's m_startVertex * stride), so the
			// chunk-local indices address the chunk's span
			bgfx::setVertexBuffer(0, vertex_pool, ranges.m_vertex_offset,
				ranges.m_vertex_count);
			bgfx::setIndexBuffer(
				index_pool, ranges.m_index_offset, ranges.m_index_count);

			// depth only: no color writes, no culling (double-sided
			// casters; the bias handles acne)
			constexpr kotek::uint64_t _kState =
				BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

			bgfx::setState(_kState);

			bgfx::submit(view_id, this->m_program_depth);
		}
	}

	this->install_view_order();

	// the FS uniforms (the banner's contract; the meta is set here because
	// the active drive always pairs with it)
	bgfx::setUniform(this->m_uniform_cascade_view_proj,
		cascades.m_cascade_view_proj[0], cascades.m_cascade_count);

	{
		float split_far[4] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};

		for (kotek::uint8_t cascade = 0;
			 cascade < cascades.m_cascade_count; ++cascade)
		{
			split_far[cascade] = cascades.m_split_far[cascade];
		}

		bgfx::setUniform(this->m_uniform_split_far, split_far);
	}

	{
		const float inv_map_size =
			1.0f / static_cast<float>(ZIRCON_DEF_SHADOW_MAP_SIZE);

		// the integer lanes ride as u32 BIT PATTERNS (the B1/B5b meta
		// discipline — the shader asuints them)
		const kotek::uint32_t cascade_count_bits = cascades.m_cascade_count;

		std::memcpy(&this->m_shadow_meta[0], &cascade_count_bits,
			sizeof(kotek::uint32_t));
		this->m_shadow_meta[1] = inv_map_size;
		this->m_shadow_meta[2] = ZIRCON_DEF_SHADOW_DEPTH_BIAS;
		std::memcpy(&this->m_shadow_meta[3], &caster_light_index,
			sizeof(kotek::uint32_t));

		bgfx::setUniform(this->m_uniform_meta, this->m_shadow_meta);
	}

	if (this->m_first_render_logged == false)
	{
		KOTEK_MESSAGE(
			"{} shadow cascades active: {} x {}x{} D32F layers, caster = "
			"light table index {} (split lambda {}, splits far {}/{}/{}/{} m)",
			this->m_p_log_tag,
			static_cast<kotek::uint32_t>(cascades.m_cascade_count),
			static_cast<kotek::uint32_t>(ZIRCON_DEF_SHADOW_MAP_SIZE),
			static_cast<kotek::uint32_t>(ZIRCON_DEF_SHADOW_MAP_SIZE),
			caster_light_index,
			static_cast<double>(ZIRCON_DEF_SHADOW_CASCADE_SPLIT_LAMBDA),
			static_cast<double>(cascades.m_split_far[0]),
			static_cast<double>(
				cascades.m_split_far[cascades.m_cascade_count > 1 ? 1 : 0]),
			static_cast<double>(
				cascades.m_split_far[cascades.m_cascade_count > 2 ? 2 : 0]),
			static_cast<double>(cascades.m_split_far
					[cascades.m_cascade_count > 3 ? 3 : 0]));

		this->m_first_render_logged = true;
	}
}

void zircon_render_shadow_rig::set_inactive(void) noexcept
{
	this->m_active = false;
	this->m_caster_light_index =
		zircon_render_light_buffer::kNoShadowCaster;

	this->m_shadow_meta[0] = 0.0f;
	this->m_shadow_meta[1] = 0.0f;
	this->m_shadow_meta[2] = 0.0f;
	this->m_shadow_meta[3] = 0.0f;

	// the meta is set EVERY drive (active or not): bgfx uniforms are
	// per-frame state and the FS branch key must never be stale
	if (bgfx::isValid(this->m_uniform_meta))
	{
		bgfx::setUniform(this->m_uniform_meta, this->m_shadow_meta);
	}

	this->restore_view_order();
}

void zircon_render_shadow_rig::bind_for_draw(bgfx::ViewId pass_id) noexcept
{
	// the depth array at the fragment stage — bound every frame the pass
	// draws (the always-bound rule; the factor-1.0 branch never samples).
	// Point sampling: the FS's 3x3 PCF does its OWN per-texel compares —
	// linear-filtered depth before the compare would be the wrong kernel
	if (bgfx::isValid(this->m_uniform_shadow_map) &&
		bgfx::isValid(this->m_depth_array))
	{
		bgfx::setTexture(ZIRCON_DEF_SHADOW_FS_TEXTURE_STAGE,
			this->m_uniform_shadow_map, this->m_depth_array,
			BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP |
				BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
	}
}

bool zircon_render_shadow_rig::is_active(void) const noexcept
{
	return this->m_active;
}

const float* zircon_render_shadow_rig::get_shadow_meta(
	void) const noexcept
{
	return this->m_shadow_meta;
}

kotek::uint32_t zircon_render_shadow_rig::get_caster_light_index(
	void) const noexcept
{
	return this->m_caster_light_index;
}

void zircon_render_shadow_rig::install_view_order(void) noexcept
{
	// the full permutation over [0, 244): the shadow views first (they
	// must execute before the graph's views — the map is sampled by the
	// main draw), then the natural order. bgfx's inverse remap assumes a
	// bijection, so the array covers EVERY position of the touched range
	// (see the header)
	kotek::uint32_t position = 0;

	for (kotek::uint8_t cascade = 0;
		 cascade < ZIRCON_DEF_SHADOW_CASCADE_COUNT; ++cascade)
	{
		this->m_view_order[position++] = static_cast<bgfx::ViewId>(
			ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + cascade);
	}

	for (kotek::uint32_t view = 0; view < ZIRCON_DEF_SHADOW_FIRST_VIEW_ID;
		 ++view)
	{
		this->m_view_order[position++] = static_cast<bgfx::ViewId>(view);
	}

	bgfx::setViewOrder(0,
		ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + ZIRCON_DEF_SHADOW_CASCADE_COUNT,
		this->m_view_order);

	this->m_view_order_installed = true;
}

void zircon_render_shadow_rig::restore_view_order(void) noexcept
{
	if (this->m_view_order_installed == false)
		return;

	// identity over the touched range (nullptr = identity); positions
	// beyond were never written
	bgfx::setViewOrder(0,
		ZIRCON_DEF_SHADOW_FIRST_VIEW_ID + ZIRCON_DEF_SHADOW_CASCADE_COUNT,
		nullptr);

	this->m_view_order_installed = false;
}
