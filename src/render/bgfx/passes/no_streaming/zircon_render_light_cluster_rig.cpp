#include "zircon_render_light_cluster_rig.h"

#include <cstring>

zircon_render_light_cluster_rig::zircon_render_light_cluster_rig(void) :
	m_streams{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE,
		BGFX_INVALID_HANDLE},
	m_tile_counts{BGFX_INVALID_HANDLE},
	m_tile_indices{BGFX_INVALID_HANDLE},
	m_bin_counters{BGFX_INVALID_HANDLE},
	m_stats_texture{BGFX_INVALID_HANDLE},
	m_stats_readback_texture{BGFX_INVALID_HANDLE},
	m_program_bin{BGFX_INVALID_HANDLE},
	m_uniform_cull_planes{BGFX_INVALID_HANDLE},
	m_uniform_view_proj{BGFX_INVALID_HANDLE},
	m_uniform_screen_meta{BGFX_INVALID_HANDLE},
	m_uniform_bin_meta{BGFX_INVALID_HANDLE},
	m_uniform_cluster_info{BGFX_INVALID_HANDLE},
	m_mirror_tile_counts{nullptr},
	m_mirror_tile_indices{nullptr},
	m_cluster_info{0.0f, 0.0f, 0.0f, 0.0f},
	m_stats_readback_values{0, 0},
	m_stats_ready_frame{0},
	m_stats_readback_pending{false},
	m_stats_warmup_done{false},
	m_gpu_assignment_count{0},
	m_gpu_dropped_count{0},
	m_cpu_assignment_count{0},
	m_cpu_dropped_count{0},
	m_tiles_x{0},
	m_tiles_y{0},
	m_clustered_active{false},
	m_first_readback_logged{false},
	m_last_logged_assignments{0xffffffffu},
	m_last_logged_dropped{0xffffffffu},
	m_is_warned_about_tile_cap{false},
	m_p_log_tag{nullptr}
{
}

void zircon_render_light_cluster_rig::create(const char* p_log_tag) noexcept
{
	KOTEK_ASSERT(p_log_tag, "must be valid");

	this->m_p_log_tag = p_log_tag ? p_log_tag : "[light_cluster]";

	// the light streams: 16-byte float4 elements with the explicit compute
	// format so d3d11 types the SRVs R32G32B32A32_FLOAT (the B1 table
	// discipline)
	this->m_layout_stream.begin()
		.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
		.end();

	for (bgfx::DynamicVertexBufferHandle& stream : this->m_streams)
	{
		stream = bgfx::createDynamicVertexBuffer(ZIRCON_DEF_LIGHT_MAX_COUNT,
			this->m_layout_stream,
			BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_32X4 |
				BGFX_BUFFER_COMPUTE_TYPE_FLOAT);
	}

	// the binning output + counters: 32-bit index-buffer-backed so d3d11
	// types the UAV/SRV views R32_UINT (typed UAV atomics need the
	// single-channel 32-bit format — the B1 counter note). COMPUTE_READ |
	// COMPUTE_WRITE so d3d11 creates BOTH views: the compute writes
	// through the UAV, the fragment stage reads through the SRV. bgfx
	// requires an initial memory block (createIndexBuffer asserts on
	// nullptr): the zeroed content is the every-frame-cleared state
	{
		const kotek::uint32_t counts_size = ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
			static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t));
		const bgfx::Memory* mem_counts = bgfx::alloc(counts_size);
		std::memset(mem_counts->data, 0, counts_size);
		this->m_tile_counts =
			bgfx::createIndexBuffer(mem_counts,
				BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ |
					BGFX_BUFFER_COMPUTE_WRITE);
	}

	{
		const kotek::uint32_t indices_size =
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE *
			static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t));
		const bgfx::Memory* mem_indices = bgfx::alloc(indices_size);
		std::memset(mem_indices->data, 0, indices_size);
		this->m_tile_indices =
			bgfx::createIndexBuffer(mem_indices,
				BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ |
					BGFX_BUFFER_COMPUTE_WRITE);
	}

	{
		const bgfx::Memory* mem_counters = bgfx::alloc(
			2 * static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t)));
		std::memset(
			mem_counters->data, 0, 2 * sizeof(kotek::uint32_t));
		this->m_bin_counters =
			bgfx::createIndexBuffer(mem_counters,
				BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ |
					BGFX_BUFFER_COMPUTE_WRITE);
	}

	// the A/B stats hop: the compute-write 2x1 (texel 0 = assignments,
	// texel 1 = per-tile-cap drops) + the readback twin (bgfx forbids
	// COMPUTE_WRITE | READ_BACK on one texture — bgfx.cpp:4663-4670, the
	// B1 note)
	this->m_stats_texture = bgfx::createTexture2D(2, 1, false, 1,
		bgfx::TextureFormat::R32U, BGFX_TEXTURE_COMPUTE_WRITE);
	this->m_stats_readback_texture = bgfx::createTexture2D(2, 1, false, 1,
		bgfx::TextureFormat::R32U,
		BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);

	KOTEK_ASSERT(bgfx::isValid(this->m_streams[0]) &&
			bgfx::isValid(this->m_streams[1]) &&
			bgfx::isValid(this->m_streams[2]) &&
			bgfx::isValid(this->m_streams[3]) &&
			bgfx::isValid(this->m_tile_counts) &&
			bgfx::isValid(this->m_tile_indices) &&
			bgfx::isValid(this->m_bin_counters) &&
			bgfx::isValid(this->m_stats_texture) &&
			bgfx::isValid(this->m_stats_readback_texture),
		"failed to create the light cluster rig's buffers!");

	// the binning cbuffer members (the layout is the shader's
	// LightBinParams) + the FS cluster-info vec4
	this->m_uniform_cull_planes = bgfx::createUniform("u_cullPlanes",
		bgfx::UniformType::Vec4, 6);
	// NOT u_viewProj: that name is a bgfx PREDEFINED uniform — the
	// renderer fills predefined slots from the view state, never from the
	// pass's setUniform (the documented trap)
	this->m_uniform_view_proj =
		bgfx::createUniform("u_lightViewProj", bgfx::UniformType::Vec4, 4);
	this->m_uniform_screen_meta =
		bgfx::createUniform("u_screenMeta", bgfx::UniformType::Vec4);
	this->m_uniform_bin_meta =
		bgfx::createUniform("u_binMeta", bgfx::UniformType::Vec4);
	this->m_uniform_cluster_info =
		bgfx::createUniform("u_lightClusterInfo", bgfx::UniformType::Vec4);

	KOTEK_ASSERT(bgfx::isValid(this->m_uniform_cull_planes) &&
			bgfx::isValid(this->m_uniform_view_proj) &&
			bgfx::isValid(this->m_uniform_screen_meta) &&
			bgfx::isValid(this->m_uniform_bin_meta) &&
			bgfx::isValid(this->m_uniform_cluster_info),
		"failed to create the light cluster rig's uniforms!");

	// the CPU mirror's output scratch (the A/B's cpu side) — heap per the
	// fixture rule, allocated once
	this->m_mirror_tile_counts =
		new kotek::uint32_t[ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES]();
	this->m_mirror_tile_indices =
		new kotek::uint32_t[ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE]();
}

void zircon_render_light_cluster_rig::set_binning_program(
	bgfx::ProgramHandle program_bin) noexcept
{
	this->m_program_bin = program_bin;
}

void zircon_render_light_cluster_rig::destroy(void) noexcept
{
	if (bgfx::isValid(this->m_program_bin))
	{
		bgfx::destroy(this->m_program_bin);
		this->m_program_bin = BGFX_INVALID_HANDLE;
	}

	bgfx::UniformHandle* p_uniforms[] = {&this->m_uniform_cull_planes,
		&this->m_uniform_view_proj, &this->m_uniform_screen_meta,
		&this->m_uniform_bin_meta, &this->m_uniform_cluster_info};

	for (bgfx::UniformHandle* p_uniform : p_uniforms)
	{
		if (bgfx::isValid(*p_uniform))
		{
			bgfx::destroy(*p_uniform);
			*p_uniform = BGFX_INVALID_HANDLE;
		}
	}

	if (bgfx::isValid(this->m_stats_texture))
	{
		bgfx::destroy(this->m_stats_texture);
		this->m_stats_texture = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(this->m_stats_readback_texture))
	{
		bgfx::destroy(this->m_stats_readback_texture);
		this->m_stats_readback_texture = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(this->m_bin_counters))
	{
		bgfx::destroy(this->m_bin_counters);
		this->m_bin_counters = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(this->m_tile_indices))
	{
		bgfx::destroy(this->m_tile_indices);
		this->m_tile_indices = BGFX_INVALID_HANDLE;
	}

	if (bgfx::isValid(this->m_tile_counts))
	{
		bgfx::destroy(this->m_tile_counts);
		this->m_tile_counts = BGFX_INVALID_HANDLE;
	}

	for (bgfx::DynamicVertexBufferHandle& stream : this->m_streams)
	{
		if (bgfx::isValid(stream))
		{
			bgfx::destroy(stream);
			stream = BGFX_INVALID_HANDLE;
		}
	}

	delete[] this->m_mirror_tile_counts;
	this->m_mirror_tile_counts = nullptr;

	delete[] this->m_mirror_tile_indices;
	this->m_mirror_tile_indices = nullptr;

	this->m_clustered_active = false;
	this->m_stats_readback_pending = false;
	this->m_stats_warmup_done = false;
	this->m_first_readback_logged = false;
}

bool zircon_render_light_cluster_rig::is_binning_ready(
	void) const noexcept
{
	return bgfx::isValid(this->m_program_bin);
}

void zircon_render_light_cluster_rig::drive(bgfx::ViewId pass_id,
	const zircon_render_light_buffer& lights, const float* p_planes_6x4,
	const float* p_view_projection_16, kotek::uint32_t screen_width,
	kotek::uint32_t screen_height) noexcept
{
	KOTEK_ASSERT(p_planes_6x4, "must be valid");
	KOTEK_ASSERT(p_view_projection_16, "must be valid");

	this->m_clustered_active = false;
	this->m_cluster_info[0] = 0.0f;
	this->m_cluster_info[1] = 0.0f;
	this->m_cluster_info[2] = 0.0f;
	this->m_cluster_info[3] = 0.0f;

	kotek::uint32_t tiles_x = 0;
	kotek::uint32_t tiles_y = 0;

	const bool grid_fits = zircon_render_light_buffer::derive_tile_grid(
		screen_width, screen_height, ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS,
		tiles_x, tiles_y);

	const kotek::uint32_t light_count = lights.get_count();

	if (light_count > 0 && this->is_binning_ready())
	{
		if (grid_fits == false &&
			this->m_is_warned_about_tile_cap == false)
		{
			KOTEK_MESSAGE_WARNING(
				"{} the backbuffer needs more than {} light tiles — the "
				"clustered path is disabled (the fallback directional "
				"answers)",
				this->m_p_log_tag, ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES);

			this->m_is_warned_about_tile_cap = true;
		}

		if (grid_fits)
		{
			this->m_tiles_x = tiles_x;
			this->m_tiles_y = tiles_y;
			this->m_clustered_active = true;
		}
	}

	if (this->m_clustered_active)
	{
		// the table upload (bounded: <= 64 lights x 16 B per stream)
		const kotek::uint32_t stream_bytes =
			light_count *
			static_cast<kotek::uint32_t>(
				sizeof(zircon_light_stream_record_t));

		bgfx::update(this->m_streams[0], 0,
			bgfx::copy(lights.get_positions(), stream_bytes));
		bgfx::update(this->m_streams[1], 0,
			bgfx::copy(lights.get_directions(), stream_bytes));
		bgfx::update(this->m_streams[2], 0,
			bgfx::copy(lights.get_colors(), stream_bytes));
		bgfx::update(this->m_streams[3], 0,
			bgfx::copy(lights.get_params(), stream_bytes));

		// the CPU mirror (the A/B's cpu side — the classification is the
		// shader's, mirrored one-to-one)
		const kotek::uint32_t tile_total = tiles_x * tiles_y;

		std::memset(this->m_mirror_tile_counts, 0,
			tile_total * sizeof(kotek::uint32_t));

		zircon_light_bin_stats_t cpu_stats;

		zircon_render_light_buffer::bin_lights(lights.get_positions(),
			lights.get_directions(), lights.get_params(), light_count,
			p_planes_6x4, p_view_projection_16, screen_width, screen_height,
			ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y,
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE,
			this->m_mirror_tile_counts, this->m_mirror_tile_indices,
			cpu_stats);

		this->m_cpu_assignment_count = cpu_stats.m_assignment_count;
		this->m_cpu_dropped_count = cpu_stats.m_dropped_count;

		// the frame's cbuffers: the planes + the view-projection (the 4
		// float4 groups — bgfx's setUniform with the element count, the
		// B1 six-plane lesson)
		bgfx::setUniform(this->m_uniform_cull_planes, p_planes_6x4, 6);
		bgfx::setUniform(
			this->m_uniform_view_proj, p_view_projection_16, 4);

		float screen_meta[4] = {static_cast<float>(screen_width),
			static_cast<float>(screen_height), 0.0f, 0.0f};

		std::memcpy(&screen_meta[2], &tiles_x, sizeof(kotek::uint32_t));
		std::memcpy(&screen_meta[3], &tiles_y, sizeof(kotek::uint32_t));

		bgfx::setUniform(this->m_uniform_screen_meta, screen_meta);

		// the three dispatches (clear -> bin -> stats); every dispatch
		// re-binds — BGFX_DISCARD_ALL drops the state after each
		const kotek::uint32_t clear_group_count = (tile_total + 63) / 64;
		const kotek::uint32_t bin_group_count = (light_count + 63) / 64;

		const kotek::uint32_t tile_pixels =
			ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS;
		const kotek::uint32_t max_per_tile =
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE;

		for (kotek::uint32_t mode = 0; mode < 3; ++mode)
		{
			float bin_meta[4] = {0.0f, 0.0f, 0.0f, 0.0f};

			std::memcpy(&bin_meta[0], &light_count, sizeof(kotek::uint32_t));
			std::memcpy(&bin_meta[1], &mode, sizeof(kotek::uint32_t));
			std::memcpy(&bin_meta[2], &tile_pixels, sizeof(kotek::uint32_t));
			std::memcpy(&bin_meta[3], &max_per_tile, sizeof(kotek::uint32_t));

			bgfx::setUniform(this->m_uniform_bin_meta, bin_meta);

			bgfx::setBuffer(0, this->m_streams[0], bgfx::Access::Read);
			bgfx::setBuffer(1, this->m_streams[1], bgfx::Access::Read);
			// the CS's t2 is the PARAMS stream (it never reads the colors
			// — the FS's t2 is colors): the 2026-10-11 A/B mismatch (gpu
			// 2221 vs cpu 4440) was this binding's off-by-one
			bgfx::setBuffer(2, this->m_streams[3], bgfx::Access::Read);
			bgfx::setBuffer(
				3, this->m_tile_counts, bgfx::Access::ReadWrite);
			bgfx::setBuffer(
				4, this->m_tile_indices, bgfx::Access::ReadWrite);
			bgfx::setBuffer(
				5, this->m_bin_counters, bgfx::Access::ReadWrite);
			bgfx::setImage(6, this->m_stats_texture, 0, bgfx::Access::Write,
				bgfx::TextureFormat::R32U);

			bgfx::dispatch(pass_id, this->m_program_bin,
				mode == 0 ? clear_group_count
					: (mode == 1 ? bin_group_count : 1));
		}

		// the stats hop: the counters land in the readback twin (the blit
		// executes in view order after the dispatches)
		bgfx::blit(pass_id, this->m_stats_readback_texture, 0, 0,
			this->m_stats_texture);

		this->pump_stats_readback();

		// the FS contract: tiles_x / active / tile pixels / per-tile cap
		// as u32 bit patterns
		{
			const kotek::uint32_t active = 1;

			std::memcpy(&this->m_cluster_info[0], &tiles_x,
				sizeof(kotek::uint32_t));
			std::memcpy(&this->m_cluster_info[1], &active,
				sizeof(kotek::uint32_t));
			std::memcpy(&this->m_cluster_info[2], &tile_pixels,
				sizeof(kotek::uint32_t));
			std::memcpy(&this->m_cluster_info[3], &max_per_tile,
				sizeof(kotek::uint32_t));
		}
	}

	// the uniform is set EVERY drive (active or not): bgfx uniforms are
	// per-frame state and the fallback branch key must never be stale
	bgfx::setUniform(this->m_uniform_cluster_info, this->m_cluster_info);
}

void zircon_render_light_cluster_rig::bind_for_draw(
	bgfx::ViewId pass_id) noexcept
{
	// the six fragment-stage SRVs (stages 0..5) — bound every frame the
	// pass draws (the always-bound rule: the fallback branch never reads
	// them, but an unbound-but-declared SRV trips the d3d11 binding
	// validation)
	bgfx::setBuffer(0, this->m_streams[0], bgfx::Access::Read);
	bgfx::setBuffer(1, this->m_streams[1], bgfx::Access::Read);
	bgfx::setBuffer(2, this->m_streams[2], bgfx::Access::Read);
	bgfx::setBuffer(3, this->m_streams[3], bgfx::Access::Read);
	bgfx::setBuffer(4, this->m_tile_counts, bgfx::Access::Read);
	bgfx::setBuffer(5, this->m_tile_indices, bgfx::Access::Read);
}

const float* zircon_render_light_cluster_rig::get_cluster_info(
	void) const noexcept
{
	return this->m_cluster_info;
}

kotek::uint32_t
zircon_render_light_cluster_rig::get_gpu_assignment_count(
	void) const noexcept
{
	return this->m_gpu_assignment_count;
}

kotek::uint32_t zircon_render_light_cluster_rig::get_gpu_dropped_count(
	void) const noexcept
{
	return this->m_gpu_dropped_count;
}

kotek::uint32_t
zircon_render_light_cluster_rig::get_cpu_assignment_count(
	void) const noexcept
{
	return this->m_cpu_assignment_count;
}

kotek::uint32_t zircon_render_light_cluster_rig::get_cpu_dropped_count(
	void) const noexcept
{
	return this->m_cpu_dropped_count;
}

kotek::uint32_t zircon_render_light_cluster_rig::get_tiles_x(
	void) const noexcept
{
	return this->m_tiles_x;
}

kotek::uint32_t zircon_render_light_cluster_rig::get_tiles_y(
	void) const noexcept
{
	return this->m_tiles_y;
}

bool zircon_render_light_cluster_rig::is_clustered_active(
	void) const noexcept
{
	return this->m_clustered_active;
}

void zircon_render_light_cluster_rig::pump_stats_readback(void) noexcept
{
	if (this->m_stats_readback_pending == false)
	{
		this->m_stats_ready_frame =
			bgfx::readTexture(this->m_stats_readback_texture,
				&this->m_stats_readback_values);
		this->m_stats_readback_pending = true;
		return;
	}

	if (bgfx::frame() < this->m_stats_ready_frame)
		return;

	this->m_stats_readback_pending = false;

	// the first completed read is pipeline-warmup garbage (it captured the
	// texture from before the first stats store landed) — never adopt it
	if (this->m_stats_warmup_done == false)
	{
		this->m_stats_warmup_done = true;
		return;
	}

	this->m_gpu_assignment_count = this->m_stats_readback_values[0];
	this->m_gpu_dropped_count = this->m_stats_readback_values[1];

	if (this->m_first_readback_logged == false)
	{
		KOTEK_MESSAGE(
			"{} light cluster A/B readback: gpu {} assignments ({} "
			"dropped) vs cpu-mirror {} ({} dropped) — {}",
			this->m_p_log_tag, this->m_gpu_assignment_count,
			this->m_gpu_dropped_count, this->m_cpu_assignment_count,
			this->m_cpu_dropped_count,
			(this->m_gpu_assignment_count == this->m_cpu_assignment_count &&
					this->m_gpu_dropped_count == this->m_cpu_dropped_count)
				? "match"
				: "MISMATCH");

		this->m_first_readback_logged = true;
	}

	// the rate-limited trace (logs on count changes only)
	if (this->m_gpu_assignment_count != this->m_last_logged_assignments ||
		this->m_gpu_dropped_count != this->m_last_logged_dropped)
	{
		KOTEK_MESSAGE_TRACE(
			"{} light cluster: gpu {} assignments ({} dropped) / "
			"cpu-mirror {} ({} dropped)",
			this->m_p_log_tag, this->m_gpu_assignment_count,
			this->m_gpu_dropped_count, this->m_cpu_assignment_count,
			this->m_cpu_dropped_count);

		this->m_last_logged_assignments = this->m_gpu_assignment_count;
		this->m_last_logged_dropped = this->m_gpu_dropped_count;
	}
}
