#include "zircon_render_graph_pass_model_static_gpu_driven.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "../../../../ecs/zircon_factory.h"
#include "../../../../game/session/zircon_session_game.h"
#include "../../../../game/session/zircon_session_game_manager.h"
#include "../../../../world/zircon_world.h"

#include <cmath>
#include <cstring>

namespace no_streaming
{
	zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		zircon_render_graph_pass_model_static_gpu_driven_bgfx(void) :
		zircon_render_graph_pass_bgfx(),
		m_vertex_pool{BGFX_INVALID_HANDLE},
		m_index_pool{BGFX_INVALID_HANDLE},
		m_bounds_table{BGFX_INVALID_HANDLE},
		m_ranges_table{BGFX_INVALID_HANDLE},
		m_visible_counter{BGFX_INVALID_HANDLE},
		m_indirect_commands{BGFX_INVALID_HANDLE},
		m_stats_texture{BGFX_INVALID_HANDLE},
		m_stats_readback_texture{BGFX_INVALID_HANDLE},
		m_program_draw{BGFX_INVALID_HANDLE},
		m_program_cull{BGFX_INVALID_HANDLE},
		m_uniform_cull_planes{BGFX_INVALID_HANDLE},
		m_uniform_cull_meta{BGFX_INVALID_HANDLE},
		m_uniform_light_dir{BGFX_INVALID_HANDLE},
		m_uniform_light_color{BGFX_INVALID_HANDLE},
		m_uniform_ambient{BGFX_INVALID_HANDLE},
		m_uniform_camera_pos{BGFX_INVALID_HANDLE},
		m_chunks_collected{false},
		m_is_warned_about_missing_program{false},
		m_stats_readback_value{0},
		m_stats_ready_frame{0},
		m_stats_readback_pending{false},
		m_stats_warmup_done{false},
		m_gpu_visible_count{0},
		m_cpu_visible_count{0},
		m_first_submit_logged{false},
		m_first_readback_logged{false},
		m_last_logged_gpu_count{0xffffffffu},
		m_last_logged_cpu_count{0xffffffffu}
	{
	}

	zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		~zircon_render_graph_pass_model_static_gpu_driven_bgfx(void)
	{
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_manager_main, "must be valid!");

		this->m_p_manager_main = p_manager_main;
		this->m_p_manager_resource = p_manager_resource;

		// the draw pool's vertex format (the model_static interleaved
		// vertex — same layout, same stride)
		this->m_layout_pool.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();

		// the compute tables ride 16-byte-element buffers (one float4 /
		// uint4 per element); the attribute choice only sizes the stride
		this->m_layout_table.begin()
			.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
			.end();

		this->m_vertex_pool = bgfx::createDynamicVertexBuffer(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_VERTICES, this->m_layout_pool);
		this->m_index_pool = bgfx::createDynamicIndexBuffer(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_INDICES);

		// the compute tables: 16-byte elements with explicit compute
		// formats so bgfx's d3d11 renderer types the SRVs exactly the way
		// the shader declares them (R32G32B32A32_FLOAT for the bounds
		// AoSoA stream, R32G32B32A32_UINT for the ranges)
		this->m_bounds_table = bgfx::createDynamicVertexBuffer(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS * 2, this->m_layout_table,
			BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_32X4 |
				BGFX_BUFFER_COMPUTE_TYPE_FLOAT);
		this->m_ranges_table = bgfx::createDynamicVertexBuffer(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS, this->m_layout_table,
			BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_COMPUTE_FORMAT_32X4 |
				BGFX_BUFFER_COMPUTE_TYPE_UINT);

		// the visible-count counter: one R32_UINT (atomics on typed UAVs
		// require the 32-bit single-channel format on d3d11 — this is why
		// the count cannot live in the indirect buffer's header, whose
		// UAV is R32G32B32A32_UINT and not atomic-capable there)
		const kotek::uint32_t counter_zero = 0;

		this->m_visible_counter =
			bgfx::createIndexBuffer(bgfx::copy(&counter_zero,
										static_cast<kotek::uint32_t>(
											sizeof(counter_zero))),
				BGFX_BUFFER_INDEX32 | BGFX_BUFFER_COMPUTE_READ_WRITE);

		this->m_indirect_commands = bgfx::createIndirectBuffer(
			zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

		// the A/B stats hop: bgfx has no buffer readback, so the cull's
		// stats mode writes the count into a compute-write 1x1, the pass
		// blits it into the readback 1x1 and the CPU reads that a few
		// frames later (bgfx forbids COMPUTE_WRITE | READ_BACK on one
		// texture — bgfx.cpp:4663-4670 — hence the two-texture hop)
		this->m_stats_texture = bgfx::createTexture2D(1, 1, false, 1,
			bgfx::TextureFormat::R32U, BGFX_TEXTURE_COMPUTE_WRITE);
		this->m_stats_readback_texture = bgfx::createTexture2D(1, 1, false,
			1, bgfx::TextureFormat::R32U,
			BGFX_TEXTURE_READ_BACK | BGFX_TEXTURE_BLIT_DST);

		KOTEK_ASSERT(bgfx::isValid(this->m_vertex_pool) &&
				bgfx::isValid(this->m_index_pool) &&
				bgfx::isValid(this->m_bounds_table) &&
				bgfx::isValid(this->m_ranges_table) &&
				bgfx::isValid(this->m_visible_counter) &&
				bgfx::isValid(this->m_indirect_commands) &&
				bgfx::isValid(this->m_stats_texture) &&
				bgfx::isValid(this->m_stats_readback_texture),
			"failed to create the gpu-driven pass's buffers!");

		// the cull cbuffer (the six planes as one vec4 array + the meta
		// vec4) and the LightParams contract (filled identically to
		// model_static's — the same defines until light components land)
		this->m_uniform_cull_planes = bgfx::createUniform("u_cullPlanes",
			bgfx::UniformType::Vec4, 6);
		this->m_uniform_cull_meta =
			bgfx::createUniform("u_cullMeta", bgfx::UniformType::Vec4);
		this->m_uniform_light_dir =
			bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
		this->m_uniform_light_color =
			bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
		this->m_uniform_ambient =
			bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
		this->m_uniform_camera_pos =
			bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);

		KOTEK_ASSERT(bgfx::isValid(this->m_uniform_cull_planes) &&
				bgfx::isValid(this->m_uniform_cull_meta) &&
				bgfx::isValid(this->m_uniform_light_dir) &&
				bgfx::isValid(this->m_uniform_light_color) &&
				bgfx::isValid(this->m_uniform_ambient) &&
				bgfx::isValid(this->m_uniform_camera_pos),
			"failed to create the gpu-driven pass's uniforms!");

		// the blob names the Slang pipeline produces from
		// data_game/shaders/slang/model_static_gpu_driven.<stage>.slang +
		// model_static_gpu_driven_cull.cs.slang (task Z24 B1)
		bgfx::ShaderHandle shader_vertex =
			this->load_shader_blob("model_static_gpu_driven.vs.bin");
		bgfx::ShaderHandle shader_fragment =
			this->load_shader_blob("model_static_gpu_driven.fs.bin");
		bgfx::ShaderHandle shader_compute =
			this->load_shader_blob("model_static_gpu_driven_cull.cs.bin");

		if (bgfx::isValid(shader_vertex) && bgfx::isValid(shader_fragment) &&
			bgfx::isValid(shader_compute))
		{
			this->m_program_draw = bgfx::createProgram(
				shader_vertex, shader_fragment, true);
			this->m_program_cull =
				bgfx::createProgram(shader_compute, true);

			KOTEK_ASSERT(bgfx::isValid(this->m_program_draw) &&
					bgfx::isValid(this->m_program_cull),
				"failed to link the gpu-driven pass's programs!");
		}
		else
		{
			if (bgfx::isValid(shader_vertex))
				bgfx::destroy(shader_vertex);
			if (bgfx::isValid(shader_fragment))
				bgfx::destroy(shader_fragment);
			if (bgfx::isValid(shader_compute))
				bgfx::destroy(shader_compute);

			if (this->m_is_warned_about_missing_program == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[model_static_gpu_driven] compiled shader blobs are "
					"absent under data_user/shader_cache/bgfx/ (the Slang "
					"pipeline has not produced them yet) — the pass stays "
					"inert");

				this->m_is_warned_about_missing_program = true;
			}
		}
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		OnDestroyResources()
	{
		if (bgfx::isValid(this->m_program_draw))
		{
			bgfx::destroy(this->m_program_draw);
			this->m_program_draw = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_program_cull))
		{
			bgfx::destroy(this->m_program_cull);
			this->m_program_cull = BGFX_INVALID_HANDLE;
		}

		bgfx::UniformHandle* p_uniforms[] = {&this->m_uniform_cull_planes,
			&this->m_uniform_cull_meta, &this->m_uniform_light_dir,
			&this->m_uniform_light_color, &this->m_uniform_ambient,
			&this->m_uniform_camera_pos};

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

		if (bgfx::isValid(this->m_indirect_commands))
		{
			bgfx::destroy(this->m_indirect_commands);
			this->m_indirect_commands = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_visible_counter))
		{
			bgfx::destroy(this->m_visible_counter);
			this->m_visible_counter = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_ranges_table))
		{
			bgfx::destroy(this->m_ranges_table);
			this->m_ranges_table = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_bounds_table))
		{
			bgfx::destroy(this->m_bounds_table);
			this->m_bounds_table = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_index_pool))
		{
			bgfx::destroy(this->m_index_pool);
			this->m_index_pool = BGFX_INVALID_HANDLE;
		}

		if (bgfx::isValid(this->m_vertex_pool))
		{
			bgfx::destroy(this->m_vertex_pool);
			this->m_vertex_pool = BGFX_INVALID_HANDLE;
		}

		this->m_chunk_pool.clear();
		this->m_chunks_collected = false;
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		if (bgfx::isValid(this->m_program_draw) == false ||
			bgfx::isValid(this->m_program_cull) == false)
		{
			return;
		}

		zircon_session_game* p_session = nullptr;

		if (this->m_p_manager_session_game)
		{
			// the same single-slot session probe model_static does (see
			// its OnRender for why get_current_session_id is not safe)
			for (kotek::uint8_t session_id = 0;
				 session_id < ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT;
				 ++session_id)
			{
				p_session =
					this->m_p_manager_session_game->get_session(session_id);

				if (p_session)
					break;
			}
		}

		zircon_world* p_world = p_session ? p_session->get_world() : nullptr;

		// the one-shot collection (B1 scope note in the header): fill the
		// pools from the world once it is initialized; an empty world
		// fills the synthetic grid fixture instead
		if (this->m_chunks_collected == false && p_world &&
			p_world->is_initialized())
		{
			zircon_factory* p_factory = p_world->get_factory();
			zircon_ecs_context_t* p_context = p_world->get_ecs_context();

			if (p_factory && p_context)
			{
				const kotek::uint32_t world_chunk_count =
					this->collect_chunks_from_world(p_factory, p_context,
						p_world->get_entity_count_max_limit());

				if (world_chunk_count == 0)
				{
					const kotek::uint32_t grid_chunk_count =
						build_synthetic_grid(this->m_chunk_pool,
							zircon_DEF_RENDER_PASS_GPU_DRIVEN_SYNTHETIC_GRID_SIDE);

					KOTEK_MESSAGE(
						"[model_static_gpu_driven] empty world — the "
						"synthetic grid fixture fills the pool ({} chunks)",
						grid_chunk_count);
				}
				else
				{
					KOTEK_MESSAGE(
						"[model_static_gpu_driven] collected {} chunks "
						"from the world",
						world_chunk_count);
				}

				this->m_chunks_collected = true;
			}
		}

		// registration/defrag output -> GPU (rare, bounded)
		this->upload_dirty_spans();

		const kotek::uint32_t slot_count =
			this->m_chunk_pool.get_chunk_slot_count();

		if (slot_count == 0)
			return;

		const bgfx::ViewId pass_id =
			static_cast<bgfx::ViewId>(my_id_in_queue);

		// the present pass (earlier slot) clears the color; this pass
		// owns the depth clear and the 3D draws (the model_static
		// contract)
		bgfx::setViewRect(pass_id, 0, 0, bgfx::BackbufferRatio::Equal);
		bgfx::setViewClear(pass_id, BGFX_CLEAR_DEPTH);
		// the three dispatches + the indirect draw must execute in
		// submission order, never sort-reordered
		bgfx::setViewMode(pass_id, bgfx::ViewMode::Sequential);

		float view[16];
		float projection[16];

		resolve_game_camera(
			p_world && p_world->is_initialized()
				? p_world->get_factory()
				: nullptr,
			p_world && p_world->is_initialized()
				? p_world->get_ecs_context()
				: nullptr,
			p_world ? p_world->get_entity_count_max_limit() : 0,
			this->m_p_manager_main, view, projection);

		bgfx::setViewTransform(pass_id, view, projection);

		// the frustum of the same view-projection bgfx uploads as the
		// predefined u_viewProj (bgfx computes it as view * proj — the
		// bx::mtxMul order below matches, so the cull and the draw
		// classify in one space)
		float view_projection[16];

		bx::mtxMul(view_projection, view, projection);

		zircon_render_chunk_pool::extract_frustum_planes(
			view_projection, this->m_cull_planes);

		// the CPU mirror of the frame's cull (the A/B proof's cpu side)
		this->m_cpu_visible_count =
			this->m_chunk_pool.cull_chunks_against_frustum(
				this->m_cull_planes, this->m_visible_ids,
				zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS);

		// the fixed scene light (the model_static defines) + the camera
		// position — uniforms are bgfx frame state captured at submit,
		// so one fill covers the dispatches and the draw
		float light_dir[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_X,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Y,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Z, 0.0f};

		const float light_dir_length =
			std::sqrt(light_dir[0] * light_dir[0] +
				light_dir[1] * light_dir[1] + light_dir[2] * light_dir[2]);

		KOTEK_ASSERT(light_dir_length > 0.0f,
			"the model_static light direction degenerated");

		if (light_dir_length > 0.0f)
		{
			light_dir[0] /= light_dir_length;
			light_dir[1] /= light_dir_length;
			light_dir[2] /= light_dir_length;
		}

		const float light_color[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_R,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_G,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_B, 1.0f};
		const float ambient[4] = {
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_R,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_G,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_B, 1.0f};

		float camera_position[3] = {
			-(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]),
			-(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]),
			-(view[8] * view[12] + view[9] * view[13] +
				view[10] * view[14])};

		const float camera_pos[4] = {camera_position[0], camera_position[1],
			camera_position[2], 1.0f};

		bgfx::setUniform(this->m_uniform_light_dir, light_dir);
		bgfx::setUniform(this->m_uniform_light_color, light_color);
		bgfx::setUniform(this->m_uniform_ambient, ambient);
		bgfx::setUniform(this->m_uniform_camera_pos, camera_pos);
		// the six-plane ARRAY needs the element count — bgfx's setUniform
		// defaults to 1 element, so without it only the first plane lands
		// (the 2026-09-20 "353 vs 131" bug: a one-plane cull)
		bgfx::setUniform(this->m_uniform_cull_planes, this->m_cull_planes,
			6);

		// the three cull dispatches (clear -> cull -> stats); every
		// dispatch re-binds — BGFX_DISCARD_ALL drops the state after each
		const kotek::uint32_t group_count = (slot_count + 63) / 64;

		for (kotek::uint32_t mode = 0; mode < 3; ++mode)
		{
			float meta[4] = {0.0f, 0.0f, 0.0f, 0.0f};

			std::memcpy(&meta[0], &slot_count, sizeof(kotek::uint32_t));
			std::memcpy(&meta[1], &mode, sizeof(kotek::uint32_t));

			bgfx::setUniform(this->m_uniform_cull_meta, meta);

			bgfx::setBuffer(0, this->m_bounds_table, bgfx::Access::Read);
			bgfx::setBuffer(1, this->m_ranges_table, bgfx::Access::Read);
			bgfx::setBuffer(
				2, this->m_indirect_commands, bgfx::Access::ReadWrite);
			bgfx::setBuffer(
				3, this->m_visible_counter, bgfx::Access::ReadWrite);
			bgfx::setImage(4, this->m_stats_texture, 0, bgfx::Access::Write,
				bgfx::TextureFormat::R32U);

			bgfx::dispatch(pass_id, this->m_program_cull,
				mode == 2 ? 1 : group_count);
		}

		// the stats hop: the count lands in the readback twin (the blit
		// executes in view order after the dispatches)
		bgfx::blit(pass_id, this->m_stats_readback_texture, 0, 0,
			this->m_stats_texture);

		// ONE indirect submit for the whole visible set: the pools stay
		// bound whole, the commands carry the ranges
		bgfx::setVertexBuffer(0, this->m_vertex_pool);
		bgfx::setIndexBuffer(this->m_index_pool);

		constexpr kotek::uint64_t _kState = BGFX_STATE_WRITE_RGB |
			BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
			BGFX_STATE_DEPTH_TEST_LESS;

		bgfx::setState(_kState);

		bgfx::submit(pass_id, this->m_program_draw, this->m_indirect_commands,
			0, slot_count);

		// the A/B readback hop (one readTexture in flight at a time)
		if (this->m_stats_readback_pending == false)
		{
			this->m_stats_ready_frame = bgfx::readTexture(
				this->m_stats_readback_texture,
				&this->m_stats_readback_value);
			this->m_stats_readback_pending = true;
		}
		else if (bgfx::frame() >= this->m_stats_ready_frame)
		{
			this->m_stats_readback_pending = false;

			// the first completed read is pipeline-warmup garbage (it
			// captured the texture from before the first stats store
			// landed) — never adopt it
			if (this->m_stats_warmup_done == false)
			{
				this->m_stats_warmup_done = true;
			}
			else
			{
				this->m_gpu_visible_count = this->m_stats_readback_value;

				if (this->m_first_readback_logged == false)
				{
					KOTEK_MESSAGE(
						"[model_static_gpu_driven] A/B readback: gpu-visible "
						"{} vs cpu-mirror {} — {} ({} chunk slots, 1 indirect "
						"submit x {} command slots)",
						this->m_gpu_visible_count, this->m_cpu_visible_count,
						this->m_gpu_visible_count == this->m_cpu_visible_count
							? "match"
							: "MISMATCH",
						slot_count, slot_count);

					this->m_first_readback_logged = true;
				}
			}
		}

		if (this->m_first_submit_logged == false)
		{
			KOTEK_MESSAGE(
				"[model_static_gpu_driven] first indirect submit: {} "
				"chunk slots ({} live), cpu-mirror visible {}, 1 "
				"indirect submit x {} command slots",
				slot_count, this->m_chunk_pool.get_live_chunk_count(),
				this->m_cpu_visible_count, slot_count);

			this->m_first_submit_logged = true;
		}

		// the rate-limited A/B trace (logs on count changes only, after
		// the readback warmed up)
		if (this->m_first_readback_logged &&
			(this->m_gpu_visible_count != this->m_last_logged_gpu_count ||
				this->m_cpu_visible_count !=
					this->m_last_logged_cpu_count))
		{
			KOTEK_MESSAGE_TRACE(
				"[model_static_gpu_driven] gpu-visible {} / cpu-mirror "
				"{} ({} slots)",
				this->m_gpu_visible_count, this->m_cpu_visible_count,
				slot_count);

			this->m_last_logged_gpu_count = this->m_gpu_visible_count;
			this->m_last_logged_cpu_count = this->m_cpu_visible_count;
		}
	}

	kotek::uint32_t zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		build_synthetic_grid(zircon_render_chunk_pool& pool,
			kotek::uint16_t grid_side) noexcept
	{
		if (grid_side == 0)
			return 0;

		zircon_model_static_vertex_t
			cube_vertices
				[zircon_render_graph_pass_model_static_bgfx::kCubeVertexCount];
		kotek::uint16_t
			cube_indices
				[zircon_render_graph_pass_model_static_bgfx::kCubeIndexCount];

		zircon_render_graph_pass_model_static_bgfx::build_cube_mesh(
			cube_vertices, cube_indices);

		kotek::uint32_t registered_count = 0;

		// centers stride 4 m, the grid centered at the origin — every
		// chunk a distinct AABB (the culling proof)
		const float spacing = 4.0f;
		const float half_extent =
			static_cast<float>(grid_side - 1) * spacing * 0.5f;

		for (kotek::uint16_t grid_x = 0; grid_x < grid_side; ++grid_x)
		{
			for (kotek::uint16_t grid_y = 0; grid_y < grid_side; ++grid_y)
			{
				for (kotek::uint16_t grid_z = 0; grid_z < grid_side;
					 ++grid_z)
				{
					// a pure translation — the phase's identity-fixture
					// path (rotation/scale stay the identity basis)
					float model[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
						0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
						0.0f, 1.0f};

					model[12] =
						static_cast<float>(grid_x) * spacing - half_extent;
					model[13] =
						static_cast<float>(grid_y) * spacing - half_extent;
					model[14] =
						static_cast<float>(grid_z) * spacing - half_extent;

					kotek::uint32_t chunk_id =
						zircon_render_chunk_pool::kInvalidChunkId;

					const bool registered = pool.register_chunk(
						cube_vertices,
						zircon_render_graph_pass_model_static_bgfx::
							kCubeVertexCount,
						cube_indices,
						zircon_render_graph_pass_model_static_bgfx::
							kCubeIndexCount,
						model, 0, chunk_id);

					if (registered == false)
						return registered_count;

					++registered_count;
				}
			}
		}

		return registered_count;
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		compose_model_matrix(const float* p_position_xyz,
			const float* p_rotation_quat_xyzw, const float* p_scale_xyz,
			float* p_out_model_16) noexcept
	{
		KOTEK_ASSERT(p_position_xyz, "must be valid");
		KOTEK_ASSERT(p_rotation_quat_xyzw, "must be valid");
		KOTEK_ASSERT(p_scale_xyz, "must be valid");
		KOTEK_ASSERT(p_out_model_16, "must be valid storage");

		if (p_position_xyz == nullptr || p_rotation_quat_xyzw == nullptr ||
			p_scale_xyz == nullptr || p_out_model_16 == nullptr)
		{
			return;
		}

		// model_static's collect_draw_items math (keep in sync): the bx
		// rotation with the scale folded into the rotation columns,
		// translation at [12..14]
		float rotation_translation[16];

		bx::mtxFromQuaternion(rotation_translation,
			bx::Quaternion(p_rotation_quat_xyzw[0], p_rotation_quat_xyzw[1],
				p_rotation_quat_xyzw[2], p_rotation_quat_xyzw[3]));

		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				float element = rotation_translation[column * 4 + row];

				if (column < 3 && row < 3)
					element *= p_scale_xyz[column];

				p_out_model_16[column * 4 + row] = element;
			}
		}

		p_out_model_16[12] = p_position_xyz[0];
		p_out_model_16[13] = p_position_xyz[1];
		p_out_model_16[14] = p_position_xyz[2];
	}

	zircon_render_chunk_pool&
	zircon_render_graph_pass_model_static_gpu_driven_bgfx::get_chunk_pool(
		void) noexcept
	{
		return this->m_chunk_pool;
	}

	kotek::uint32_t
	zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		get_gpu_visible_count(void) const noexcept
	{
		return this->m_gpu_visible_count;
	}

	kotek::uint32_t
	zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		get_cpu_visible_count(void) const noexcept
	{
		return this->m_cpu_visible_count;
	}

	kotek::uint32_t zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		collect_chunks_from_world(zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit) noexcept
	{
		kotek::entity_t entity_ids
			[zircon_DEF_RENDER_PASS_GPU_DRIVEN_MAX_ENTITY_SCAN];

		const kotek::uint32_t entity_count = p_factory->get_all_entities(
			p_context, entity_count_max_limit, entity_ids,
			zircon_DEF_RENDER_PASS_GPU_DRIVEN_MAX_ENTITY_SCAN);

		kotek::uint32_t registered_count = 0;

		for (kotek::uint32_t entity_index = 0;
			 entity_index < entity_count; ++entity_index)
		{
			const kotek::entity_t& entity = entity_ids[entity_index];

			if (p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_geometry) ==
					false ||
				p_factory->has_component(p_context, entity,
					eZirconComponentType::kzircon_component_transform) ==
					false)
			{
				continue;
			}

			zircon_component_geometry* p_geometry =
				static_cast<zircon_component_geometry*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::kzircon_component_geometry));

			zircon_component_transform* p_transform =
				static_cast<zircon_component_transform*>(
					p_factory->get_component_by_enum(p_context, entity,
						eZirconComponentType::kzircon_component_transform));

			if (p_geometry == nullptr || p_transform == nullptr)
				continue;

			if (p_geometry->is_enabled() == false ||
				p_geometry->is_visible() == false ||
				p_transform->is_enabled() == false)
			{
				continue;
			}

			const bool has_mesh_name =
				p_geometry->get_mesh_name()[0] != '\0';

			if (has_mesh_name == false &&
				p_geometry->get_geometry_type() !=
					kotek::core::eStaticGeometryType::kBox)
			{
				continue;
			}

			const kotek::math::vec3f_t& position =
				p_transform->get_position();
			const kotek::math::quatf_t& rotation =
				p_transform->get_rotation();
			const kotek::math::vec3f_t& scale = p_transform->get_scale();

			const float position_xyz[3] = {
				position.x(), position.y(), position.z()};
			const float rotation_xyzw[4] = {rotation.x(), rotation.y(),
				rotation.z(), rotation.w()};
			const float scale_xyz[3] = {scale.x(), scale.y(), scale.z()};

			float entity_model[16];

			compose_model_matrix(
				position_xyz, rotation_xyzw, scale_xyz, entity_model);

			if (has_mesh_name == false)
			{
				// the fallback cube entity (the model_static fixture
				// geometry) — one chunk
				zircon_model_static_vertex_t
					cube_vertices
						[zircon_render_graph_pass_model_static_bgfx::
								kCubeVertexCount];
				kotek::uint16_t
					cube_indices
						[zircon_render_graph_pass_model_static_bgfx::
								kCubeIndexCount];

				zircon_render_graph_pass_model_static_bgfx::build_cube_mesh(
					cube_vertices, cube_indices);

				kotek::uint32_t chunk_id =
					zircon_render_chunk_pool::kInvalidChunkId;

				if (this->m_chunk_pool.register_chunk(cube_vertices,
						zircon_render_graph_pass_model_static_bgfx::
							kCubeVertexCount,
						cube_indices,
						zircon_render_graph_pass_model_static_bgfx::
							kCubeIndexCount,
						entity_model, 0, chunk_id))
				{
					++registered_count;
				}

				continue;
			}

			// a glTF mesh entity: load the model (one-shot, per entity —
			// the B1 tradeoff documented in the header) and register one
			// chunk per submesh, the node's flattened world transform
			// composed with the entity's model matrix
			kotek::core::ktkIFileSystem* p_filesystem =
				this->m_p_manager_main
					? this->m_p_manager_main->GetFileSystem()
					: nullptr;

			if (p_filesystem == nullptr)
				continue;

			kotek::static_path_t mesh_path;

			kotek::core::path_for(p_filesystem,
				kotek::core::eFolderIndex::kFolderIndex_DataGame_Models,
				p_geometry->get_mesh_name(), mesh_path);

			zircon_gltf_error_t load_error;

			eZirconGltfLoadStatus load_status = zircon_gltf_load_from_file(
				p_filesystem, mesh_path, this->m_mesh_file_buffer,
				sizeof(this->m_mesh_file_buffer), this->m_mesh_scratch,
				load_error);

			if (load_status != eZirconGltfLoadStatus::kSuccess)
			{
				KOTEK_MESSAGE_WARNING(
					"[model_static_gpu_driven] mesh '{}' failed to load "
					"(status {}): {}",
					p_geometry->get_mesh_name(),
					static_cast<int>(load_status), load_error.c_str());
				continue;
			}

			const kotek::uint32_t mesh_vertex_count = static_cast<
				kotek::uint32_t>(this->m_mesh_scratch.m_vertices.size());

			for (const auto& submesh : this->m_mesh_scratch.m_submeshes)
			{
				// the node's flattened world transform applies first,
				// then the entity's model matrix (the model_static
				// composition order)
				float composed_model[16];

				for (int row = 0; row < 4; ++row)
				{
					for (int column = 0; column < 4; ++column)
					{
						float sum = 0.0f;

						for (int step = 0; step < 4; ++step)
						{
							sum += submesh.m_world_matrix[row * 4 + step] *
								entity_model[step * 4 + column];
						}

						composed_model[row * 4 + column] = sum;
					}
				}

				// the chunk's vertex span = the mesh's full vertex set
				// (baked); the submesh's index span addresses it
				// verbatim — vertices shared between submeshes are
				// duplicated per chunk (the B1 tradeoff; dedup is B2)
				for (kotek::uint32_t vertex_index = 0;
					 vertex_index < mesh_vertex_count; ++vertex_index)
				{
					const zircon_gltf_vertex_t& source =
						this->m_mesh_scratch.m_vertices[vertex_index];

					zircon_model_static_vertex_t& target =
						this->m_chunk_vertex_scratch[vertex_index];

					target.m_position[0] = source.m_position[0];
					target.m_position[1] = source.m_position[1];
					target.m_position[2] = source.m_position[2];
					target.m_normal[0] = source.m_normal[0];
					target.m_normal[1] = source.m_normal[1];
					target.m_normal[2] = source.m_normal[2];
					target.m_color_abgr = 0xffffffffu;
				}

				for (kotek::uint32_t index_index = 0;
					 index_index < submesh.m_index_count; ++index_index)
				{
					this->m_chunk_index_scratch[index_index] = static_cast<
						kotek::uint16_t>(
						this->m_mesh_scratch.m_indices
							[submesh.m_index_offset + index_index]);
				}

				kotek::uint32_t chunk_id =
					zircon_render_chunk_pool::kInvalidChunkId;

				if (this->m_chunk_pool.register_chunk(
						this->m_chunk_vertex_scratch, mesh_vertex_count,
						this->m_chunk_index_scratch,
						submesh.m_index_count, composed_model, 0,
						chunk_id))
				{
					++registered_count;
				}
			}
		}

		return registered_count;
	}

	bool zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		resolve_game_camera(zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit,
			kotek::core::ktkMainManager* p_manager_main,
			float* p_out_view_16, float* p_out_projection_16) noexcept
	{
		// the first enabled camera component (the model_static contract)
		if (p_factory && p_context)
		{
			kotek::entity_t entity_ids
				[zircon_DEF_RENDER_PASS_GPU_DRIVEN_MAX_ENTITY_SCAN];

			const kotek::uint32_t entity_count =
				p_factory->get_all_entities(p_context,
					entity_count_max_limit, entity_ids,
					zircon_DEF_RENDER_PASS_GPU_DRIVEN_MAX_ENTITY_SCAN);

			for (kotek::uint32_t entity_index = 0;
				 entity_index < entity_count; ++entity_index)
			{
				if (p_factory->has_component(p_context,
						entity_ids[entity_index],
						eZirconComponentType::kzircon_component_camera) ==
					false)
				{
					continue;
				}

				zircon_component_camera* p_camera =
					static_cast<zircon_component_camera*>(
						p_factory->get_component_by_enum(p_context,
							entity_ids[entity_index],
							eZirconComponentType::
								kzircon_component_camera));

				if (p_camera == nullptr || p_camera->is_enabled() == false)
					continue;

				const float* p_view =
					kotek::math::value_ptr(p_camera->get_view());
				const float* p_projection =
					kotek::math::value_ptr(p_camera->get_projection());

				for (int element_index = 0; element_index < 16;
					 ++element_index)
				{
					p_out_view_16[element_index] = p_view[element_index];
					p_out_projection_16[element_index] =
						p_projection[element_index];
				}

				return true;
			}
		}

		// the default orbit (eye (4,3,4) -> origin, 60-degree fov — the
		// model_static default)
		bx::mtxLookAt(p_out_view_16, bx::Vec3(4.0f, 3.0f, 4.0f),
			bx::Vec3(0.0f, 0.0f, 0.0f), bx::Vec3(0.0f, 1.0f, 0.0f));

		float aspect_ratio = 4.0f / 3.0f;

		if (p_manager_main && p_manager_main->Get_WindowManager())
		{
			int window_width =
				p_manager_main->Get_WindowManager()->ActiveWindow_GetWidth();
			int window_height = p_manager_main->Get_WindowManager()
									->ActiveWindow_GetHeight();

			if (window_width > 0 && window_height > 0)
			{
				aspect_ratio = static_cast<float>(window_width) /
					static_cast<float>(window_height);
			}
		}

		bx::mtxProj(p_out_projection_16, 60.0f, aspect_ratio, 0.1f, 100.0f,
			bgfx::getCaps()->homogeneousDepth);

		return false;
	}

	void zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		upload_dirty_spans(void) noexcept
	{
		if (this->m_chunk_pool.is_pools_dirty())
		{
			const kotek::uint32_t vertex_high_water =
				this->m_chunk_pool.get_vertex_high_water();
			const kotek::uint32_t index_high_water =
				this->m_chunk_pool.get_index_high_water();

			if (vertex_high_water)
			{
				bgfx::update(this->m_vertex_pool, 0,
					bgfx::copy(this->m_chunk_pool.get_vertex_shadow(),
						vertex_high_water *
							static_cast<kotek::uint32_t>(
								sizeof(zircon_model_static_vertex_t))));
			}

			if (index_high_water)
			{
				bgfx::update(this->m_index_pool, 0,
					bgfx::copy(this->m_chunk_pool.get_index_shadow(),
						index_high_water * static_cast<kotek::uint32_t>(
												sizeof(kotek::uint16_t))));
			}
		}

		if (this->m_chunk_pool.is_tables_dirty())
		{
			const kotek::uint32_t slot_count =
				this->m_chunk_pool.get_chunk_slot_count();

			if (slot_count)
			{
				// the bounds AoSoA bundle uploads byte-exact (2 float4
				// elements per chunk)
				bgfx::update(this->m_bounds_table, 0,
					bgfx::copy(this->m_chunk_pool.get_bounds(),
						slot_count * static_cast<kotek::uint32_t>(
										  sizeof(zircon_chunk_bounds_t))));

				// the narrow ranges records expand to one uint4 per chunk
				const zircon_chunk_ranges_t* p_ranges =
					this->m_chunk_pool.get_ranges();

				for (kotek::uint32_t slot = 0; slot < slot_count; ++slot)
				{
					this->m_ranges_upload_staging[slot * 4 + 0] =
						p_ranges[slot].m_vertex_offset;
					this->m_ranges_upload_staging[slot * 4 + 1] =
						p_ranges[slot].m_vertex_count;
					this->m_ranges_upload_staging[slot * 4 + 2] =
						p_ranges[slot].m_index_offset;
					this->m_ranges_upload_staging[slot * 4 + 3] =
						p_ranges[slot].m_index_count;
				}

				bgfx::update(this->m_ranges_table, 0,
					bgfx::copy(this->m_ranges_upload_staging,
						slot_count * 4 * static_cast<kotek::uint32_t>(
											  sizeof(kotek::uint32_t))));
			}
		}

		this->m_chunk_pool.clear_dirty_flags();
	}

	bgfx::ShaderHandle
	zircon_render_graph_pass_model_static_gpu_driven_bgfx::
		load_shader_blob(const char* p_shader_file_name) noexcept
	{
		bgfx::ShaderHandle result = BGFX_INVALID_HANDLE;

		KOTEK_ASSERT(p_shader_file_name, "must be valid");
		KOTEK_ASSERT(this->m_p_manager_main,
			"OnCreateResources must run before loading shaders");

		if (p_shader_file_name == nullptr ||
			this->m_p_manager_main == nullptr)
		{
			return result;
		}

		kotek::core::ktkIFileSystem* p_filesystem =
			this->m_p_manager_main->GetFileSystem();

		KOTEK_ASSERT(p_filesystem, "must be valid");

		if (p_filesystem == nullptr)
			return result;

		// the backend-dialect directory of the active renderer (the
		// model_static resolution, kept identical)
		const char* p_dialect_directory = nullptr;

		switch (bgfx::getRendererType())
		{
		case bgfx::RendererType::Direct3D11:
		{
			p_dialect_directory = "dx11";
			break;
		}
		case bgfx::RendererType::Direct3D12:
		{
			p_dialect_directory = "dx12";
			break;
		}
		case bgfx::RendererType::OpenGLES:
		{
			p_dialect_directory = "essl";
			break;
		}
		case bgfx::RendererType::OpenGL:
		{
			p_dialect_directory = "glsl";
			break;
		}
		case bgfx::RendererType::Vulkan:
		{
			p_dialect_directory = "vulkan";
			break;
		}
		default:
		{
			break;
		}
		}

		if (p_dialect_directory == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static_gpu_driven] no shader-blob dialect "
				"directory for renderer '{}' — shader '{}' not loaded",
				bgfx::getRendererName(bgfx::getRendererType()),
				p_shader_file_name);

			return result;
		}

		kotek::static_path_t shader_path;

		kotek::core::path_for(p_filesystem,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_ShaderCache,
			"bgfx", shader_path);

		shader_path /= p_dialect_directory;
		shader_path /= p_shader_file_name;

		// an absent blob is an expected pre-pipeline state, not an error
		if (p_filesystem->Is_Exists(shader_path) == false)
			return result;

		kotek::uint8_t
			buffer[zircon_DEF_RENDER_PASS_MODEL_STATIC_SHADER_BIN_MAX_SIZE];

		kotek::size_t blob_size = 0;

		bool read_status = kotek::core::read_file(
			p_filesystem, shader_path, buffer, sizeof(buffer), blob_size);

		if (read_status && blob_size)
		{
			// bgfx::copy hands bgfx its own copy, so the filesystem's
			// buffer is not referenced past this call
			result = bgfx::createShader(bgfx::copy(
				buffer, static_cast<uint32_t>(blob_size)));

			KOTEK_ASSERT(bgfx::isValid(result),
				"shader blob '{}' failed to create — corrupt or "
				"wrong-dialect data?",
				p_shader_file_name);
		}
		else
		{
			KOTEK_MESSAGE_WARNING(
				"[model_static_gpu_driven] failed to read shader blob "
				"'{}'",
				p_shader_file_name);
		}

		return result;
	}
} // namespace no_streaming
