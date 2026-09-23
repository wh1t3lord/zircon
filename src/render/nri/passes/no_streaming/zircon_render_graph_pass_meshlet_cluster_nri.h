#pragma once

#include "../../../../core/zircon_meshlet_cluster_read.h"
#include "../zircon_render_graph_pass_nri.h"

/// \file zircon_render_graph_pass_meshlet_cluster_nri.h
/// \~english the FIRST REAL NRI DRAW (task Z24 B3b): one baked meshlet
/// cluster (LOD0, no culling — B3c owns cluster culling/LOD selection and
/// the classic/nanite toggle) drawn through the kotek geometry seam. The
/// pass reads ONE cluster of the B3a pack format through the filesystem
/// dispatcher (packs-first, native fallback), expands it to the
/// interleaved soup vertex buffer + the identity u32 index buffer
/// (zircon_meshlet_cluster_read.h), creates the buffers + the pipeline
/// through ktkIRenderGeometryManager and records one indexed draw per
/// frame. Everything GPU-side is an OPAQUE HANDLE (the Z5 boundary rule:
/// no NRI types in zircon); the pass holds non-owning service pointers +
/// handles only (the reload-safety laws).
///
/// THE PIPELINE CHOICE (the documented B3b decision): the vendored NRI
/// v180 implements mesh shaders internally (PipelineD3D12 consumes
/// StageBits::MESH_SHADER; CommandBufferD3D12::DrawMeshTasks exists) but
/// its PUBLIC CoreInterface table exposes no mesh-dispatch entry point —
/// and the vendored third-party stays byte-pristine. So the honest v1 is
/// the CLASSIC indexed draw of the cluster's triangles; the seam shape is
/// identical for the future mesh path (same binds, a Draw_Meshlets
/// instead of Draw_Indexed), which lands with an NRI version bump (B3c).
///
/// THE CAMERA: B3b has no scene camera contract on the NRI path, so the
/// pass frames the cluster with a FIXED debug orbit built from the
/// manifest AABB (logged once) — the draw proves the geometry path, not
/// the camera chain. THE LIGHT: the fragment stage hardcodes a
/// directional lambert (the shared Phong contract needs bound cbuffers —
/// the descriptor seam lands with the material/lighting phase).

/// the scene the pass loads ("meshlets/<scene>/...") — the shipped boot
/// probe fixture at the ENGINE ROOT (the pack entry namespace is
/// root-relative, the B3a writer's contract — a pack carrying the same
/// entries resolves through the dispatcher identically)
#define ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE "boot"

/// one compiled shader stage's blob bound (the Slang dxil route writes
/// small blobs — 64 KB of headroom)
#define ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SHADER_MAX_SIZE \
	(64u * 1024u)

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

		/// \~english loads the cluster + creates the buffers/pipeline
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
		kotek::core::ktkRenderGeometryBufferHandle get_vertex_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryBufferHandle get_index_buffer(
			void) const noexcept;
		kotek::core::ktkRenderGeometryPipelineHandle get_pipeline(
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
		kotek::core::ktkRenderGeometryPipelineHandle m_pipeline{
			kotek::core::kInvalidRenderGeometryPipelineHandle};
		kotek::uint32_t m_index_count{};
		/// the cluster AABB (floats — the debug-orbit camera's frame)
		float m_aabb_min[3]{};
		float m_aabb_max[3]{};
		bool m_is_ready{};
	};
} // namespace no_streaming
