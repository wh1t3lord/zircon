#pragma once

#include "../../../../core/zircon_meshlet_cluster_read.h"
#include "../../../../core/zircon_meshlet_cull.h"
#include "../zircon_render_graph_pass_nri.h"

/// \file zircon_render_graph_pass_meshlet_cluster_nri.h
/// \~english the NRI meshlet cluster pass (task Z24 B3b: the first real
/// NRI draw — one baked cluster through the geometry seam; B3c: the
/// GPU-side finish of the nanite path — the WHOLE baked hierarchy, the
/// compute cull + the LOD cut, the compacted indirect draw and the A/B
/// readback). The pass reads the B3a pack format through the filesystem
/// dispatcher (packs-first, native fallback), expands EVERY cluster of
/// EVERY level into ONE pooled interleaved soup VB + ONE pooled identity
/// IB (the reader's per-cluster identity slices, concatenated — a
/// visible cluster's command draws its [start_index, start_index +
/// index_count) element range with base_vertex = its soup offset), builds the
/// 64-byte GPU cluster table (AABB + cone + the error slots + the draw
/// offsets, the parent->child links inverted into the per-record
/// parent error the LOD cut needs) and uploads it as a storage buffer.
/// Per frame the CULL COMPUTE (meshlet_cluster_cull.cs.slang, the C++
/// mirror zircon_meshlet_cull.h) classifies every cluster — the LOD cut
/// by the projected error vs ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD,
/// the frustum, the cone — and compacts 20-byte indirect commands
/// (the NRI DrawIndexedDesc layout) into the argument buffer; ONE
/// Draw_Indexed_Indirect consumes them. Everything GPU-side is an OPAQUE
/// HANDLE (the Z5 boundary rule: no NRI types in zircon); the pass holds
/// non-owning service pointers + handles only (the reload-safety laws).
///
/// THE PIPELINE CHOICE (the documented B3b decision): the vendored NRI
/// v180 implements mesh shaders internally but its PUBLIC CoreInterface
/// table exposes no mesh-dispatch entry point — and the vendored
/// third-party stays byte-pristine. So the draw is the CLASSIC indexed
/// draw of the culled clusters; the seam shape is identical for the
/// future mesh path (same binds, a Draw_Meshlets instead).
///
/// THE CAMERA: no scene camera contract on the NRI path yet, so the pass
/// frames the hierarchy with a FIXED debug orbit built from the whole
/// scene AABB (logged once) — the draw proves the cull/LOD path, not the
/// camera chain. THE LIGHT: the fragment stage hardcodes a directional
/// lambert (the shared Phong contract needs the descriptor seam — the
/// material/lighting phase).
///
/// THE A/B EVIDENCE (the B1 readback discipline, NRI form): NRI/D3D12
/// HAS buffer readback — no texture hop. Frame 0 copies the 4-byte
/// counter into a HOST_READBACK buffer right after the draw (the barrier
/// dance documented in the .cpp); at frame
/// ZIRCON_DEF_NRI_MESHLET_PASS_READBACK_FRAME the CPU maps it (the
/// swapchain's frame pacing makes that copy long complete) and logs
/// "gpu-visible N vs cpu-mirror M — match" — the GPU/CPU cull
/// equivalence of the fixed camera.

/// the scene the pass loads ("meshlets/<scene>/...") — the shipped boot
/// probe fixture at the ENGINE ROOT (the pack entry namespace is
/// root-relative, the B3a writer's contract — a pack carrying the same
/// entries resolves through the dispatcher identically)
#define ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE "boot"

/// one compiled shader stage's blob bound (the Slang dxil route writes
/// small blobs — 64 KB of headroom)
#define ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SHADER_MAX_SIZE \
	(64u * 1024u)

/// the v1 whole-hierarchy residency per draw (task Z24 B3c): the pass
/// loads EVERY cluster of EVERY level into the pools up front — the boot
/// probe carries 1; a 6-level placeholder hierarchy of a 100k-triangle
/// mesh lands ~1.9k clusters, so 256 covers the v1 scenes with margin;
/// the per-scene streaming up-scale is the documented B-later work.
/// Over the cap the pass stays loud-inert (user content is not a
/// programmer error)
#define ZIRCON_DEF_NRI_MESHLET_PASS_MAX_CLUSTERS 256u

/// the pooled soup-vertex residency cap (the identity IB carries the same
/// count of u32 elements): 1M soup vertices = 24 MB of pooled VB — the
/// triangle-soup expansion of ~139k LOD0-budget triangles across all
/// levels at the 256-cluster cap; sized for the v1, raised by measurement
#define ZIRCON_DEF_NRI_MESHLET_PASS_MAX_SOUP_VERTICES (1u << 20)

/// the push-constant block (the kernel's CullParams cbuffer): 6 planes +
/// the camera vec4 + the meta vec4 = 128 bytes
#define ZIRCON_DEF_NRI_MESHLET_PASS_CULL_PARAMS_BYTES 128u

/// the one-shot counter readback: frame 0 records the copy, this frame
/// maps it — the swapchain's frame pacing (1 queued frame: at frame N
/// everything through N-1 is complete) makes a copy from frame 0 long
/// done by frame 3, with margin
#define ZIRCON_DEF_NRI_MESHLET_PASS_READBACK_FRAME 3u

namespace no_streaming
{
	/// the registered pass name (single source: the NRI passlib registry
	/// and Get_Name both spell it through this constant)
	constexpr const char* kZircon_RenderGraphPassMeshletClusterNri_Name =
		"no_streaming::zircon_render_graph_pass_meshlet_cluster_nri";

	class zircon_render_graph_pass_meshlet_cluster_nri
		: public zircon_render_graph_pass_nri
	{
	public:
		zircon_render_graph_pass_meshlet_cluster_nri(void);
		~zircon_render_graph_pass_meshlet_cluster_nri(
			void) override;

		/// \~english loads the hierarchy + creates the buffers/pipelines
		/// BEFORE the first frame (the passlib calls this once right after
		/// creation); loud-but-graceful when the content or the seam is
		/// missing — the pass then records nothing (user data is not a
		/// programmer error)
		void Initialize_Nri(
			kotek::core::ktkMainManager* p_main_manager) override;

		void Record(
			kotek::core::ktkIRenderFramePassContext* p_context) override;

		const char* Get_Name(void) const noexcept override;

		/// \~english the test accessors (read-only views of the load)
		bool is_ready(void) const noexcept;
		kotek::uint32_t get_index_count(void) const noexcept;
		kotek::uint32_t get_cluster_count(void) const noexcept;
		kotek::uint32_t get_cpu_visible_count(void) const noexcept;
		kotek::uint32_t get_gpu_visible_count(void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_vertex_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_index_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_cluster_table(
			void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_indirect_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_counter_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryPipelineHandle get_pipeline(
			void) const noexcept;
		kotek::core::ktkRenderGeometryPipelineHandle get_cull_pipeline(
			void) const noexcept;

	private:
		/// \~english releases the created handles (dtor + re-init guard);
		/// safe when the manager never arrived (the seam-less construction)
		void release_resources(void) noexcept;

		/// \~english reads one compiled shader blob from
		/// shader_cache/nri/dx12/ into a FRESH heap buffer (the caller
		/// delete[]s it); false when the blob is missing or over the cap —
		/// the expected pre-pipeline state, not an error
		bool load_shader_blob(const char* p_file_name,
			kotek::uint8_t*& p_out_blob,
			kotek::uint32_t& out_size) noexcept;

	private:
		kotek::core::ktkIFileSystem* m_p_filesystem{};
		kotek::core::ktkIRenderGeometryManager* m_p_geometry_manager{};
		kotek::core::ktkRenderGeometryBufferHandle m_vertex_buffer{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryBufferHandle m_index_buffer{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryBufferHandle m_cluster_table{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryBufferHandle m_indirect_buffer{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryBufferHandle m_counter_buffer{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryBufferHandle m_readback_buffer{
			kotek::core::kInvalidRenderGeometryBufferHandle};
		kotek::core::ktkRenderGeometryPipelineHandle m_pipeline{
			kotek::core::kInvalidRenderGeometryPipelineHandle};
		kotek::core::ktkRenderGeometryPipelineHandle m_cull_pipeline{
			kotek::core::kInvalidRenderGeometryPipelineHandle};
		/// the pooled identity IB element count (= the soup vertex count)
		kotek::uint32_t m_index_count{};
		/// the loaded cluster count (the GPU table + the cull dispatch)
		kotek::uint32_t m_cluster_count{};
		kotek::uint32_t m_cpu_visible_count{};
		kotek::uint32_t m_gpu_visible_count{};
		kotek::uint32_t m_frame_counter{};
		/// the hierarchy AABB (floats — the debug-orbit camera's frame)
		float m_aabb_min[3]{};
		float m_aabb_max[3]{};
		/// the CPU mirror's reference: the uploaded GPU cluster table,
		/// owned here (heap — sized to the loaded count), freed in
		/// release_resources
		zircon_meshlet_cull_layout::gpu_record_t* m_p_records{};
		bool m_is_ready{};
		bool m_readback_copied{};
		bool m_ab_logged{};
	};
} // namespace no_streaming
