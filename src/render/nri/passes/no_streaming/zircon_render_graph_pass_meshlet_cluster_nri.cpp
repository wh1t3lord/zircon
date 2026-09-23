#include "zircon_render_graph_pass_meshlet_cluster_nri.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include <cmath>
#include <cstring>

namespace no_streaming
{
	zircon_render_graph_pass_meshlet_cluster_nri::
		zircon_render_graph_pass_meshlet_cluster_nri(void)
	{
	}

	zircon_render_graph_pass_meshlet_cluster_nri::
		~zircon_render_graph_pass_meshlet_cluster_nri(void)
	{
		this->release_resources();
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::release_resources(
		void) noexcept
	{
		if (this->m_p_geometry_manager == nullptr)
			return;

		if (this->m_vertex_buffer !=
			kotek::core::kInvalidRenderGeometryBufferHandle)
		{
			this->m_p_geometry_manager->Destroy_Buffer(
				this->m_vertex_buffer);
			this->m_vertex_buffer =
				kotek::core::kInvalidRenderGeometryBufferHandle;
		}

		if (this->m_index_buffer !=
			kotek::core::kInvalidRenderGeometryBufferHandle)
		{
			this->m_p_geometry_manager->Destroy_Buffer(
				this->m_index_buffer);
			this->m_index_buffer =
				kotek::core::kInvalidRenderGeometryBufferHandle;
		}

		if (this->m_pipeline !=
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(
				this->m_pipeline);
			this->m_pipeline =
				kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		this->m_index_count = 0;
		this->m_is_ready = false;
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::Initialize_Nri(
		kotek::core::ktkMainManager* p_main_manager)
	{
		this->release_resources();

		if (p_main_manager == nullptr)
			return;

		this->m_p_filesystem = p_main_manager->GetFileSystem();
		this->m_p_geometry_manager =
			p_main_manager->GetRenderGeometryManager();

		if (this->m_p_filesystem == nullptr ||
			this->m_p_geometry_manager == nullptr)
		{
			KOTEK_MESSAGE_WARNING(
				"[nri meshlet] pass '{}' stays inert: the filesystem or "
				"the geometry manager is missing (a backend without "
				"geometry support?)",
				kZircon_RenderGraphPassMeshletClusterNri_Name);

			return;
		}

		// ---- the cluster read through the filesystem dispatcher (the
		// chunk-pool load's shape: root-relative paths, packs-first,
		// cwd-independent)
		kotek::static_path_t root_path;
		this->m_p_filesystem->Make_Path(
			root_path, kotek::core::eFolderIndex::kFolderIndex_Root);

		kotek::static_cstring_t<64> manifest_relative;
		manifest_relative = "meshlets/";
		manifest_relative +=
			ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE;
		manifest_relative += "/manifest.bin";

		kotek::static_path_t manifest_path = root_path;
		manifest_path /= manifest_relative.c_str();

		kotek::size_t manifest_size = 0;

		if (this->m_p_filesystem->Get_FileSize(
				manifest_path, manifest_size) == false)
		{
			// the B0 probe's warning IS the message — the draw stays
			// inert (bake content per zircon_meshlet_clusterize.h)
			KOTEK_MESSAGE_WARNING(
				"[nri meshlet] no baked cluster manifest at '{}' — "
				"the pass records nothing (a missing content file is "
				"not an error)",
				manifest_path.c_str());

			return;
		}

		if (manifest_size == 0 ||
			manifest_size >
				(zircon_meshlet_manifest_header_size +
					ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS *
						zircon_meshlet_manifest_level_size +
					ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE *
						zircon_meshlet_manifest_record_size))
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the manifest size {} is outside the "
				"format's bounds — corrupt content, the pass records "
				"nothing",
				static_cast<kotek::uint32_t>(manifest_size));

			return;
		}

		kotek::uint8_t* p_manifest =
			new kotek::uint8_t[manifest_size + 1];
		kotek::size_t manifest_read_size = manifest_size + 1;

		const bool is_manifest_read = this->m_p_filesystem->Read_File(
			manifest_path, p_manifest, manifest_read_size);

		if (is_manifest_read == false ||
			manifest_read_size != manifest_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the manifest read failed ({} of {} "
				"bytes) — the pass records nothing",
				static_cast<kotek::uint32_t>(manifest_read_size),
				static_cast<kotek::uint32_t>(manifest_size));

			delete[] p_manifest;
			return;
		}

		zircon_meshlet_read_manifest_header_t manifest_header{};

		if (zircon_meshlet_read_manifest_header(p_manifest,
				manifest_size, manifest_header) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the manifest at '{}' is not a valid "
				"meshlet manifest — the pass records nothing",
				manifest_path.c_str());

			delete[] p_manifest;
			return;
		}

		kotek::uint32_t first_lod0_cluster = 0;
		kotek::uint32_t lod0_cluster_count = 0;

		if (zircon_meshlet_read_level_range(p_manifest, manifest_size, 0,
				first_lod0_cluster, lod0_cluster_count) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the manifest declares no LOD0 level — "
				"the pass records nothing");

			delete[] p_manifest;
			return;
		}

		zircon_meshlet_cluster_t cluster_record{};

		if (zircon_meshlet_read_cluster_record(p_manifest, manifest_size,
				first_lod0_cluster, cluster_record) == false ||
			cluster_record.m_level != 0u)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the first LOD0 cluster record is "
				"invalid — the pass records nothing");

			delete[] p_manifest;
			return;
		}

		char entry_name[ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH]{};

		if (zircon_meshlet_read_entry_name(entry_name, sizeof(entry_name),
				ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE, 0u,
				0u) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the cluster entry name did not build "
				"(internal) — the pass records nothing");

			delete[] p_manifest;
			return;
		}

		kotek::static_path_t cluster_path = root_path;
		cluster_path /= entry_name;

		kotek::size_t cluster_size = 0;

		if (this->m_p_filesystem->Get_FileSize(
				cluster_path, cluster_size) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the cluster bin '{}' is missing — the "
				"pass records nothing",
				cluster_path.c_str());

			delete[] p_manifest;
			return;
		}

		kotek::uint8_t* p_cluster_bin =
			new kotek::uint8_t[cluster_size + 1];
		kotek::size_t cluster_read_size = cluster_size + 1;

		const bool is_cluster_read = this->m_p_filesystem->Read_File(
			cluster_path, p_cluster_bin, cluster_read_size);

		if (is_cluster_read == false ||
			cluster_read_size != cluster_size)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the cluster read failed ({} of {} "
				"bytes) — the pass records nothing",
				static_cast<kotek::uint32_t>(cluster_read_size),
				static_cast<kotek::uint32_t>(cluster_size));

			delete[] p_cluster_bin;
			delete[] p_manifest;
			return;
		}

		// ---- the soup expansion (the LOD0 budget bounds the stack
		// scratch: <= 124 triangles -> <= 372 soup vertices)
		constexpr kotek::uint32_t k_lod0_max_triangles =
			zircon_meshlet_max_tris_for_level(0u);

		const kotek::uint32_t soup_vertex_capacity =
			k_lod0_max_triangles * 3u;

		float vb[zircon_meshlet_read_vb_capacity_floats(
			k_lod0_max_triangles)]{};
		kotek::uint32_t
			ib[zircon_meshlet_read_ib_capacity(k_lod0_max_triangles)]{};

		zircon_meshlet_cluster_content_t content{};

		if (zircon_meshlet_read_cluster(p_cluster_bin, cluster_size,
				cluster_record.m_aabb_min, cluster_record.m_aabb_max,
				vb,
				zircon_meshlet_read_vb_capacity_floats(
					k_lod0_max_triangles),
				ib, soup_vertex_capacity, content) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the cluster bin '{}' is invalid — the "
				"pass records nothing",
				cluster_path.c_str());

			delete[] p_cluster_bin;
			delete[] p_manifest;
			return;
		}

		delete[] p_cluster_bin;
		delete[] p_manifest;

		if (content.m_soup_vertex_count > soup_vertex_capacity)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the cluster exceeds the LOD0 soup "
				"capacity — the pass records nothing");

			return;
		}

		// ---- the buffers through the geometry seam (uploads are
		// immediate — readable from the first submitted frame)
		const kotek::uint32_t vb_size_bytes =
			content.m_soup_vertex_count *
			ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES;
		const kotek::uint32_t ib_size_bytes =
			content.m_index_count * sizeof(kotek::uint32_t);

		this->m_vertex_buffer = this->m_p_geometry_manager->Create_Buffer(
			vb_size_bytes, kotek::core::eRenderGeometryBufferUsage::
				kVertex);

		this->m_index_buffer = this->m_p_geometry_manager->Create_Buffer(
			ib_size_bytes,
			kotek::core::eRenderGeometryBufferUsage::kIndex);

		if (this->m_vertex_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_index_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the vertex/index buffer creation "
				"failed — the pass records nothing");

			this->release_resources();
			return;
		}

		if (this->m_p_geometry_manager->Upload_Buffer(
				this->m_vertex_buffer, 0, vb, vb_size_bytes) == false ||
			this->m_p_geometry_manager->Upload_Buffer(
				this->m_index_buffer, 0, ib, ib_size_bytes) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the vertex/index upload failed — the "
				"pass records nothing");

			this->release_resources();
			return;
		}

		// ---- the pipeline (the Slang dxil route's raw blobs)
		kotek::uint8_t* p_vertex_blob = nullptr;
		kotek::uint8_t* p_pixel_blob = nullptr;
		kotek::uint32_t vertex_blob_size = 0;
		kotek::uint32_t pixel_blob_size = 0;

		if (this->load_shader_blob("meshlet_cluster.vs.dxil",
				p_vertex_blob, vertex_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster.fs.dxil",
				p_pixel_blob, pixel_blob_size) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the shader blobs (shader_cache/nri/dx12/"
				"meshlet_cluster.*.dxil) are missing or oversized — "
				"the pass records nothing (build zircon_shaders "
				"first)");

			delete[] p_vertex_blob;
			delete[] p_pixel_blob;

			this->release_resources();
			return;
		}

		const kotek::core::ktkRenderGeometryVertexAttributeDesc
			attributes[2] = {
				{"POSITION", 0u,
					kotek::core::eRenderGeometryVertexFormat::kFloat3, 0u},
				{"NORMAL", 0u,
					kotek::core::eRenderGeometryVertexFormat::kFloat3, 12u},
			};

		kotek::core::ktkRenderGeometryPipelineDesc pipeline_desc{};
		pipeline_desc.m_vertex_shader.m_p_bytecode = p_vertex_blob;
		pipeline_desc.m_vertex_shader.m_size_bytes = vertex_blob_size;
		pipeline_desc.m_vertex_shader.m_p_entry_point = "vs_main";
		pipeline_desc.m_pixel_shader.m_p_bytecode = p_pixel_blob;
		pipeline_desc.m_pixel_shader.m_size_bytes = pixel_blob_size;
		pipeline_desc.m_pixel_shader.m_p_entry_point = "fs_main";
		pipeline_desc.m_p_attributes = attributes;
		pipeline_desc.m_attribute_count = 2;
		pipeline_desc.m_vertex_stride_bytes =
			ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES;
		pipeline_desc.m_color_format =
			kotek::core::eRenderGeometryColorFormat::kRGBA8Unorm;
		pipeline_desc.m_push_constant_bytes = 64u; // u_viewProj

		this->m_pipeline = this->m_p_geometry_manager->Create_Pipeline(
			pipeline_desc);

		// the backend copied what it needed during creation
		delete[] p_vertex_blob;
		delete[] p_pixel_blob;

		if (this->m_pipeline ==
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the pipeline creation failed — the "
				"pass records nothing");

			this->release_resources();
			return;
		}

		this->m_index_count = content.m_index_count;

		for (int axis = 0; axis < 3; ++axis)
		{
			this->m_aabb_min[axis] =
				static_cast<float>(cluster_record.m_aabb_min[axis]);
			this->m_aabb_max[axis] =
				static_cast<float>(cluster_record.m_aabb_max[axis]);
		}

		this->m_is_ready = true;

		KOTEK_MESSAGE(
			"[nri meshlet] pass created: cluster '{}' LOD0 cluster 0 = "
			"{} triangles ({} welded vertices -> {} soup vertices), VB "
			"{} B + IB {} B uploaded, pipeline created (vs+ps DXIL, 64 B "
			"push constants)",
			ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE,
			content.m_triangle_count, content.m_welded_vertex_count,
			content.m_soup_vertex_count, vb_size_bytes, ib_size_bytes);
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::Record(
		kotek::core::ktkIRenderFramePassContext* p_context)
	{
		KOTEK_ASSERT(p_context,
			"the NRI meshlet pass needs a valid frame pass context");

		if (p_context == nullptr || this->m_is_ready == false)
			return;

		// the fixed debug orbit frames the cluster AABB (see the header:
		// the draw proves the geometry path, not the camera chain)
		const float center[3] = {
			0.5f * (this->m_aabb_min[0] + this->m_aabb_max[0]),
			0.5f * (this->m_aabb_min[1] + this->m_aabb_max[1]),
			0.5f * (this->m_aabb_min[2] + this->m_aabb_max[2])};

		const float extent[3] = {
			this->m_aabb_max[0] - this->m_aabb_min[0],
			this->m_aabb_max[1] - this->m_aabb_min[1],
			this->m_aabb_max[2] - this->m_aabb_min[2]};

		const float radius = 0.5f * std::sqrt(extent[0] * extent[0] +
			extent[1] * extent[1] + extent[2] * extent[2]);

		const float orbit_direction[3] = {0.7071f, 0.55f, 0.7071f};

		const float eye[3] = {
			center[0] + orbit_direction[0] * (radius * 2.5f + 0.5f),
			center[1] + orbit_direction[1] * (radius * 2.5f + 0.5f),
			center[2] + orbit_direction[2] * (radius * 2.5f + 0.5f)};

		kotek::ktk::math::matrix4x4f view =
			kotek::ktk::math::look_at(kotek::ktk::math::vector3f(
										  eye[0], eye[1], eye[2]),
				kotek::ktk::math::vector3f(
					center[0], center[1], center[2]),
				kotek::ktk::math::vector3f(0.0f, 1.0f, 0.0f));

		kotek::uint32_t width = p_context->Get_Back_Buffer_Width();
		kotek::uint32_t height = p_context->Get_Back_Buffer_Height();

		if (height == 0)
			height = 1;

		const float aspect =
			static_cast<float>(width) / static_cast<float>(height);

		kotek::ktk::math::matrix4x4f projection =
			kotek::ktk::math::perspective(
				kotek::ktk::math::convert_to_radians(60.0f), aspect,
				(radius * 0.05f > 0.01f) ? radius * 0.05f : 0.01f,
				radius * 20.0f + 100.0f);

		const kotek::ktk::math::matrix4x4f view_projection =
			projection * view;

		p_context->Begin_Render_Pass();
		p_context->Set_Pipeline(this->m_pipeline);
		p_context->Set_Push_Constants(
			kotek::ktk::math::value_ptr(view_projection),
			64u);
		p_context->Set_Vertex_Buffer(this->m_vertex_buffer, 0u,
			ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES, 0u);
		p_context->Set_Index_Buffer(this->m_index_buffer, 0u,
			kotek::core::eRenderGeometryIndexFormat::kUint32);
		p_context->Draw_Indexed(this->m_index_count, 1u, 0u, 0, 0u);
		p_context->End_Render_Pass();
	}

	const char* zircon_render_graph_pass_meshlet_cluster_nri::Get_Name(
		void) const noexcept
	{
		return kZircon_RenderGraphPassMeshletClusterNri_Name;
	}

	bool zircon_render_graph_pass_meshlet_cluster_nri::load_shader_blob(
		const char* p_file_name, kotek::uint8_t*& p_out_blob,
		kotek::uint32_t& out_size) noexcept
	{
		p_out_blob = nullptr;
		out_size = 0;

		if (p_file_name == nullptr)
			return false;

		kotek::static_path_t shader_path;

		kotek::core::path_for(this->m_p_filesystem,
			kotek::core::eFolderIndex::kFolderIndex_DataUser_ShaderCache,
			"nri", shader_path);

		shader_path /= "dx12";
		shader_path /= p_file_name;

		// an absent blob is the expected pre-pipeline state — the
		// existence check keeps the (graceful since kotek B0) missing-file
		// read from logging its warning
		if (this->m_p_filesystem->Is_Exists(shader_path) == false)
			return false;

		kotek::size_t file_size = 0;

		if (this->m_p_filesystem->Get_FileSize(shader_path, file_size) ==
				false ||
			file_size == 0 ||
			file_size >
				ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SHADER_MAX_SIZE)
		{
			return false;
		}

		kotek::uint8_t* p_blob = new (std::nothrow)
			kotek::uint8_t[file_size];

		if (p_blob == nullptr)
			return false;

		kotek::size_t read_size = file_size;

		if (kotek::core::read_file(this->m_p_filesystem, shader_path,
				p_blob, file_size, read_size) == false ||
			read_size != file_size)
		{
			delete[] p_blob;
			return false;
		}

		p_out_blob = p_blob;
		out_size = static_cast<kotek::uint32_t>(read_size);

		return true;
	}

	bool zircon_render_graph_pass_meshlet_cluster_nri::is_ready(
		void) const noexcept
	{
		return this->m_is_ready;
	}

	kotek::uint32_t
		zircon_render_graph_pass_meshlet_cluster_nri::get_index_count(
			void) const noexcept
	{
		return this->m_index_count;
	}

	kotek::core::ktkRenderGeometryBufferHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_vertex_buffer(
			void) const noexcept
	{
		return this->m_vertex_buffer;
	}

	kotek::core::ktkRenderGeometryBufferHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_index_buffer(
			void) const noexcept
	{
		return this->m_index_buffer;
	}

	kotek::core::ktkRenderGeometryPipelineHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_pipeline(
			void) const noexcept
	{
		return this->m_pipeline;
	}
} // namespace no_streaming
