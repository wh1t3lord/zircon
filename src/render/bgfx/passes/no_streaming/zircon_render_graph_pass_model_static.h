#pragma once

#include "../../zircon_render_graph_pass.h"

#include "../../../../core/zircon_gltf_loader.h"
#include "../../../../ecs/zircon_component_geometry.h"

class zircon_factory;
struct zircon_ecs_context_t;

// game pass "model_static" (task Z3 P2b) capacities — named per the
// memory-budget rule; a level's static draws stay far below this
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT 128
// upper bound of one compiled shader blob read through the kotek
// filesystem (the forward-Phong pair is a few KB today; generous
// headroom for the later textured/material techniques)
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_SHADER_BIN_MAX_SIZE 65536
// the glTF mesh cache (P2c): one slot per distinct mesh name drawn in a
// level; the load is one-shot per name (hot reload is task P3)
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_CACHE_COUNT 8
// .glb/.gltf read scratch for cache misses — sized so any model within
// the loader's output caps fits (the raw float payload dominates the
// file size); doubles as the bgfx upload staging (its content is fully
// decoded into the mesh scratch before the upload needs it)
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_FILE_MAX_SIZE 1048576

// the fixed scene light of the forward-Phong shading (P2g): ONE
// directional light + ambient, uploaded to the FS cbuffer (LightParams)
// every frame from these defines until light components land (a later
// task — the pass is the only consumer, so the swap is local). The
// direction points FROM the light (the way the light travels; the pass
// normalizes it on upload), the color is a warm white, the ambient a
// 0.15 neutral floor so backfaces stay readable
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_X 0.5f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Y -0.8f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_DIR_FROM_Z 0.35f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_R 1.0f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_G 0.95f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_LIGHT_COLOR_B 0.85f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_R 0.15f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_G 0.15f
#define zircon_DEF_RENDER_PASS_MODEL_STATIC_AMBIENT_B 0.15f

// interleaved vertex of the pass's meshes (position + normal + packed
// ABGR color, matches the vertex layout created in OnCreateResources and
// the attribute list of the cooked shaders); the normal feeds the
// forward-Phong shading (P2g), the color modulates the lit result
struct zircon_model_static_vertex_t
{
	float m_position[3];
	float m_normal[3];
	kotek::uint32_t m_color_abgr;
};

// one static-model submission: the entity's model matrix plus the mesh
// source — an empty mesh name is the pass's fallback cube, a set name
// routes through the pass's glTF mesh cache
struct zircon_render_pass_model_static_draw_item_t
{
	float m_model_matrix[16];
	kotek::static_cstring_t<
		zircon_DEF_COMPONENT_GEOMETRY_MESH_NAME_MAX_LENGTH>
		m_mesh_name;
};

// one cached glTF mesh: the uploaded GPU buffers plus the submesh
// ranges (each carries its flattened node world transform — the pass
// composes it with the entity's model matrix per draw)
struct zircon_render_pass_model_static_mesh_slot_t
{
	struct submesh_t
	{
		float m_world_matrix[16];
		kotek::uint32_t m_index_offset;
		kotek::uint32_t m_index_count;
	};

	kotek::static_cstring_t<
		zircon_DEF_COMPONENT_GEOMETRY_MESH_NAME_MAX_LENGTH>
		m_name;
	kotek::static_vector_t<submesh_t,
		zircon_DEF_GLTF_MAX_SUBMESH_COUNT>
		m_submeshes;
	bgfx::VertexBufferHandle m_vertex_buffer;
	bgfx::IndexBufferHandle m_index_buffer;
	// a failed load is sticky for the session: the error is logged once
	// and the entity is skipped without per-frame retries
	bool m_is_failed;
};

namespace no_streaming
{
	class zircon_render_graph_pass_model_static_bgfx
		: public zircon_render_graph_pass_bgfx
	{
	public:
		// the fallback cube fixture: 24 vertices (6 faces x 4, per-face
		// normal + ABGR color) and 36 indices (12 triangles); doubles as
		// the unit-test fixture and the editor's future "add primitive"
		// source
		static constexpr kotek::uint8_t kCubeVertexCount = 24;
		static constexpr kotek::uint8_t kCubeIndexCount = 36;

		zircon_render_graph_pass_model_static_bgfx(void);
		~zircon_render_graph_pass_model_static_bgfx(void);

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

		// emits the fallback cube into caller storage (capacities are the
		// kCube* constants above)
		static void build_cube_mesh(
			zircon_model_static_vertex_t* p_vertices,
			kotek::uint16_t* p_indices) noexcept;

		// gathers the frame's static-model draws: every live entity with
		// an enabled+visible zircon_component_geometry and an enabled
		// zircon_component_transform; the geometry's mesh name decides
		// the mesh source (set = a glTF mesh of that name, empty + type
		// kBox = the fallback cube; anything else is skipped). Writes at
		// most out_items_capacity items and returns the amount written
		// (0 = the empty-world no-op)
		static kotek::uint32_t collect_draw_items(
			zircon_factory* p_factory, zircon_ecs_context_t* p_context,
			kotek::uint32_t entity_count_max_limit,
			zircon_render_pass_model_static_draw_item_t* p_out_items,
			kotek::uint32_t out_items_capacity) noexcept;

		// the fragment stage's forward-Phong lighting (P2g), mirrored
		// one-to-one in model_static.fs.slang (the unit tests pin them —
		// keep the two in sync): one directional light + ambient floor +
		// Phong specular, clamped; the shader multiplies the result by the
		// interpolated vertex color afterwards. All directions are
		// normalized on entry (the shader normalizes the interpolated
		// normal and the view vector, the pass uploads a normalized light
		// direction). p_light_dir_from points the way the light travels
		// (FROM the light). p_out_lighting_rgb =
		// saturate(ambient + light_color * (ndotl + specular)) with the
		// specular killed on backfacing normals
		static void evaluate_phong(const float* p_normal,
			const float* p_light_dir_from, const float* p_view_dir,
			const float* p_light_color_rgb, const float* p_ambient_rgb,
			float shininess, float* p_out_lighting_rgb) noexcept;

	private:
		// reads one compiled shader blob from
		// data_user/shader_cache/<backend>/<name> through the kotek
		// filesystem (the blob production pipeline — Slang — is spiked
		// separately; this pass only consumes the results by name). An
		// absent file is not an error: the handle stays invalid and the
		// pass no-ops
		bgfx::ShaderHandle load_shader_blob(
			const char* p_shader_file_name) noexcept;

		// resolves a mesh name to its cache slot, loading the model
		// (data_game/models/<name>) and uploading its buffers on first
		// use; returns nullptr when the slot budget is spent or the mesh
		// failed to load (both logged once)
		const zircon_render_pass_model_static_mesh_slot_t* resolve_mesh(
			const char* p_mesh_name) noexcept;

		// out = a x b in the engine's model-matrix convention (the
		// bx::mtxMul order: a applies first)
		static void multiply_model_matrices(const float* p_a,
			const float* p_b, float* p_out) noexcept;

		// camera world position of a rigid view matrix: the translation
		// of the inverse view, -(R^T * t) in the bx column-major layout
		// (translation lives at [12..14]) — mirrors the editor grid
		// pass's compute_camera_position; feeds u_cameraPos (the
		// specular view vector)
		static void compute_camera_position(const float* p_view,
			float* p_out_position) noexcept;

	private:
		bgfx::VertexLayout m_layout;
		bgfx::VertexBufferHandle m_vertex_buffer;
		bgfx::IndexBufferHandle m_index_buffer;
		bgfx::ProgramHandle m_program;
		// the LightParams cbuffer members of the fragment stage (P2g):
		// the fixed scene light + ambient and the per-frame camera
		// position, created in OnCreateResources and filled every frame
		// the pass draws
		bgfx::UniformHandle m_uniform_light_dir;
		bgfx::UniformHandle m_uniform_light_color;
		bgfx::UniformHandle m_uniform_ambient;
		bgfx::UniformHandle m_uniform_camera_pos;
		// the pass is inert (renders nothing) until its compiled program
		// exists (e.g. the shader pipeline has not produced the blobs
		// yet) — tracks the one-time warning so the log is not spammed
		bool m_is_warned_about_missing_program;
		// the mesh cache is full — same one-time discipline as above
		bool m_is_warned_about_mesh_cache_full;
		// last submitted draw count for the rate-limited trace (the boot
		// log shows submission counts without per-frame spam)
		kotek::uint32_t m_last_submitted_draw_count;
		zircon_render_pass_model_static_draw_item_t m_draw_items
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];
		zircon_render_pass_model_static_mesh_slot_t m_mesh_slots
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_CACHE_COUNT];
		// cache-miss work areas: the decoded mesh and the file/upload
		// scratch (see the define's note); both are reused by every load
		// and never held by a slot
		zircon_gltf_mesh_t m_mesh_scratch;
		kotek::uint8_t m_mesh_file_buffer
			[zircon_DEF_RENDER_PASS_MODEL_STATIC_MESH_FILE_MAX_SIZE];
	};
} // namespace no_streaming
