#pragma once

// zircon_render_graph_pass_model_static_gpu_driven — the GPU-driven
// classic baseline for chunked static geometry (task Z24 B1, the "pre-
// nanite" path of the approved renderer plan): the world mirror of the
// model_static game pass, re-shaped the GPU-driven way. ONE shared
// vertex pool + ONE shared index pool hold every registered chunk (see
// zircon_render_chunk_pool.h for the AoSoA table layout); per frame a
// compute kernel frustum-culls the chunk AABBs and compacts the visible
// draw commands into an indirect buffer; ONE indirect submit draws the
// whole visible set. The CPU never iterates draw items — the per-frame
// world iteration of model_static becomes registration-time chunk
// collection here.
//
// The A/B toggle against the per-item CPU path (model_static) is the
// pass-set system's job: the two passes are alternate members of the
// game pass set, swapped live by the render_passes_game_toggle_ab
// console command (the renderer's frame-boundary rebuild) or pinned by
// a scene.json render_passes key — this pass registers in the generated
// pass factory like every other pass.
//
// Contracts inherited from model_static (kept identical on purpose):
//   - the same interleaved vertex (zircon_model_static_vertex_t);
//   - the same forward-Phong lighting — the FS is the shared
//     zircon_evaluate_phong, fed by the same four pass-written uniforms
//     (u_lightDir/u_lightColor/u_ambient/u_cameraPos, the b0/b1
//     discipline), filled from the same zircon_DEF_RENDER_PASS_MODEL_STATIC_*
//     defines;
//   - the same camera resolution (first enabled camera component, else
//     the default orbit) so the A/B comparison draws the same frame.
//
// B1 scope documents (the deliberate simplifications):
//   - transforms are BAKED at registration (the phase's fixtures are
//     identity — a real matrix takes the same path); a runtime transform
//     edit needs re-registration (B2);
//   - start_instance = chunk index is written into every command but
//     NOT read by the draw (d3d11 does not expose StartInstanceLocation
//     to the shader; baked transforms + the single shared material make
//     per-chunk fetch unnecessary this phase);
//   - chunk collection is one-shot at the first rendered frame (world
//     edits after that need a pass rebuild — the A/B toggle or the dev
//     hot-reload does it);
//   - an empty world (the boot's game session) fills the pool with the
//     synthetic grid fixture (below) so the path visibly runs — the
//     same fallback-cube role model_static's cube plays;
//   - the GPU-vs-CPU visible-count equivalence is the A/B proof: the
//     pass mirrors the cull on the CPU (the pool's
//     cull_chunks_against_frustum) every frame and reads the GPU count
//     back through the 1x1 R32U stats texture (bgfx has no buffer
//     readback); the first valid readback logs both counts. Vulkan
//     note: bgfx's vk renderer inserts no barriers between dispatches —
//     the clear->cull->stats ordering is API-guaranteed on d3d11 (the
//     boot's active backend); on vulkan it inherits bgfx's behavior.

#include "../../zircon_render_graph_pass.h"

#include "../../../../core/zircon_gltf_loader.h"
#include "zircon_render_chunk_pool.h"

class zircon_factory;
struct zircon_ecs_context_t;

// the entity scan cap of the one-shot world collect (each entity yields
// one chunk per drawn submesh; the pool's MAX_CHUNKS is the real bound —
// an overflow is a loud register failure)
#define zircon_DEF_RENDER_PASS_GPU_DRIVEN_MAX_ENTITY_SCAN 1024
// the synthetic grid fixture's side in cubes (N x N x N chunks on a 4 m
// grid of unit cubes — the phase's culling proof; 8^3 = 512 chunks, the
// pool's headroom absorbs them + the world chunks). Named per rule 9;
// the tests use their own smaller sides
#define zircon_DEF_RENDER_PASS_GPU_DRIVEN_SYNTHETIC_GRID_SIDE 8

namespace no_streaming
{
	class zircon_render_graph_pass_model_static_gpu_driven_bgfx
		: public zircon_render_graph_pass_bgfx
	{
	public:
		zircon_render_graph_pass_model_static_gpu_driven_bgfx(void);
		~zircon_render_graph_pass_model_static_gpu_driven_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass,
			kotek::ktk::uint32_t my_id_in_queue) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass,
			kotek::ktk::uint32_t my_id_in_queue) override;

		// the synthetic N-chunk fixture (the culling proof): a
		// side x side x side grid of unit cubes (the model_static
		// fallback cube geometry) with 4 m spacing, centered at the
		// origin, every chunk a distinct AABB. Pure translations — the
		// phase's identity-fixture path. Returns the registered chunk
		// count (0 on the first overflow — the failure is already loud)
		static kotek::uint32_t build_synthetic_grid(
			zircon_render_chunk_pool& pool,
			kotek::uint16_t grid_side) noexcept;

		// the model-matrix composition of a transform component,
		// mirrored from model_static's collect_draw_items (rotation with
		// the scale folded into the columns, translation at [12..14]) —
		// kept as a static so the tests pin the bake against it
		static void compose_model_matrix(const float* p_position_xyz,
			const float* p_rotation_quat_xyzw, const float* p_scale_xyz,
			float* p_out_model_16) noexcept;

		// ---- test/A-B seams (headless: no bgfx state touched) --------
		zircon_render_chunk_pool& get_chunk_pool(void) noexcept;

		// the A/B stats: the GPU-visible count from the stats-texture
		// readback (valid a few frames after the first submit; 0 until
		// then) and the CPU mirror's count for the same frame/chunks
		kotek::uint32_t get_gpu_visible_count(void) const noexcept;
		kotek::uint32_t get_cpu_visible_count(void) const noexcept;

	private:
		// the one-shot world collect: every live entity with an
		// enabled+visible geometry and an enabled transform registers its
		// chunks (kBox without a mesh name = the fallback cube; a mesh
		// name routes through the gltf loader — one chunk per submesh,
		// the node's flattened world transform composed with the
		// entity's model matrix and baked). Returns the registered count
		kotek::uint32_t collect_chunks_from_world(zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit) noexcept;

		// the camera resolution, mirrored from model_static's OnRender:
		// the first enabled camera component's view/projection, else the
		// default orbit (eye (4,3,4) -> origin, 60-degree fov). Returns
		// false for the default path
		static bool resolve_game_camera(zircon_factory* p_factory,
			zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit,
			kotek::core::ktkMainManager* p_manager_main,
			float* p_out_view_16, float* p_out_projection_16) noexcept;

		// reads one compiled shader blob from
		// data_user/shader_cache/bgfx/<dialect>/<name> (the same
		// dialect resolution as model_static); an absent file keeps the
		// handle invalid and the pass inert (one-time warning)
		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

		// uploads the pool's dirty spans into the GPU buffers (called at
		// the top of OnRender; registration is rare, so the used-range
		// upload is bounded and amortized — never per-frame)
		void upload_dirty_spans(void) noexcept;

	private:
		zircon_render_chunk_pool m_chunk_pool;

		bgfx::VertexLayout m_layout_pool;
		// the 16-byte-element layout of the compute tables' buffers
		// (never drawn — only the stride matters)
		bgfx::VertexLayout m_layout_table;
		bgfx::DynamicVertexBufferHandle m_vertex_pool;
		bgfx::DynamicIndexBufferHandle m_index_pool;
		bgfx::DynamicVertexBufferHandle m_bounds_table;
		bgfx::DynamicVertexBufferHandle m_ranges_table;
		bgfx::IndexBufferHandle m_visible_counter;
		bgfx::IndirectBufferHandle m_indirect_commands;
		bgfx::TextureHandle m_stats_texture;
		// the readback twin of the stats texture (the CPU side of the
		// A/B count hop — see OnCreateResources)
		bgfx::TextureHandle m_stats_readback_texture;
		bgfx::ProgramHandle m_program_draw;
		bgfx::ProgramHandle m_program_cull;
		// the cull cbuffer: the six planes (vec4 x6) + the meta vec4
		bgfx::UniformHandle m_uniform_cull_planes;
		bgfx::UniformHandle m_uniform_cull_meta;
		// the LightParams contract (the model_static forward-Phong
		// uniforms, filled identically)
		bgfx::UniformHandle m_uniform_light_dir;
		bgfx::UniformHandle m_uniform_light_color;
		bgfx::UniformHandle m_uniform_ambient;
		bgfx::UniformHandle m_uniform_camera_pos;

		// the one-shot collection latch (B1 scope note above)
		bool m_chunks_collected;
		bool m_is_warned_about_missing_program;

		// the A/B stats: the readback hop state (one readTexture in
		// flight at a time) and the two counts; the first completed read
		// is pipeline-warmup garbage and is dropped (m_stats_warmup_done)
		kotek::uint32_t m_stats_readback_value;
		kotek::uint32_t m_stats_ready_frame;
		bool m_stats_readback_pending;
		bool m_stats_warmup_done;
		kotek::uint32_t m_gpu_visible_count;
		kotek::uint32_t m_cpu_visible_count;
		bool m_first_submit_logged;
		bool m_first_readback_logged;
		// the last logged (gpu, cpu) pair for the rate-limited trace
		kotek::uint32_t m_last_logged_gpu_count;
		kotek::uint32_t m_last_logged_cpu_count;

		// the per-frame cull work areas: the six planes, the visible-id
		// list of the CPU mirror, and the ranges-table upload staging
		// (the narrow 12-byte records expanded to uint4 per chunk)
		float m_cull_planes[24];
		kotek::uint32_t m_visible_ids
			[zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS];
		kotek::uint32_t m_ranges_upload_staging
			[zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS * 4];

		// the collect-time gltf scratch (the model_static pattern: the
		// decoded mesh + the file/upload staging, reused by every load)
		zircon_gltf_mesh_t m_mesh_scratch;
		kotek::uint8_t m_mesh_file_buffer
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_FILE_MAX_SIZE];
		// the per-chunk registration scratch (one submesh's worth — the
		// loader's caps bound it)
		zircon_model_static_vertex_t m_chunk_vertex_scratch
			[zircon_DEF_GLTF_MAX_VERTEX_COUNT];
		kotek::uint16_t m_chunk_index_scratch
			[zircon_DEF_GLTF_MAX_INDEX_COUNT];
	};
} // namespace no_streaming
