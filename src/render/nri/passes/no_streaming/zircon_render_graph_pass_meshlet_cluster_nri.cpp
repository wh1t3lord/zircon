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

		const kotek::core::ktkRenderGeometryBufferHandle buffers[] = {
			this->m_vertex_buffer, this->m_index_buffer,
			this->m_cluster_table, this->m_indirect_buffer,
			this->m_counter_buffer, this->m_readback_buffer};

		for (kotek::uint32_t index = 0; index < 6u; ++index)
		{
			if (buffers[index] !=
				kotek::core::kInvalidRenderGeometryBufferHandle)
			{
				this->m_p_geometry_manager->Destroy_Buffer(buffers[index]);
			}
		}

		this->m_vertex_buffer = kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_index_buffer = kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_cluster_table = kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_indirect_buffer =
			kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_counter_buffer =
			kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_readback_buffer =
			kotek::core::kInvalidRenderGeometryBufferHandle;

		if (this->m_pipeline != kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(this->m_pipeline);
			this->m_pipeline = kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		if (this->m_cull_pipeline !=
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(
				this->m_cull_pipeline);
			this->m_cull_pipeline =
				kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		this->m_index_count = 0;
		this->m_cluster_count = 0;
		this->m_cpu_visible_count = 0;
		this->m_gpu_visible_count = 0;
		this->m_frame_counter = 0;
		this->m_is_ready = false;
		this->m_readback_copied = false;
		this->m_ab_logged = false;

		delete[] this->m_p_records;
		this->m_p_records = nullptr;
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

		// ---- the manifest read through the filesystem dispatcher (the
		// chunk-pool load's shape: content lives under data_game (the
		// house folder doctrine), packs-first, cwd-independent)
		kotek::static_path_t root_path;
		this->m_p_filesystem->Make_Path(
			root_path, kotek::core::eFolderIndex::kFolderIndex_DataGame);

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
						zircon_meshlet_manifest_record_size +
					ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE * 4u))
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

		if (manifest_header.m_cluster_count_total >
			ZIRCON_DEF_NRI_MESHLET_PASS_MAX_CLUSTERS)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the hierarchy declares {} clusters, the "
				"v1 pass residency cap is {} — bake smaller scenes "
				"(the streaming up-scale is the documented later "
				"phase); the pass records nothing",
				manifest_header.m_cluster_count_total,
				ZIRCON_DEF_NRI_MESHLET_PASS_MAX_CLUSTERS);

			delete[] p_manifest;
			return;
		}

		// the whole-hierarchy soup budget: every level re-groups the same
		// geometry (the B3a placeholder), the soup expansion costs 3
		// vertices per triangle per level
		kotek::uint64_t total_soup_vertices = 0;

		for (kotek::uint32_t level = 0;
			 level < manifest_header.m_lod_level_count; ++level)
		{
			kotek::uint32_t first = 0;
			kotek::uint32_t count = 0;

			if (zircon_meshlet_read_level_range(p_manifest, manifest_size,
					level, first, count) == false)
			{
				KOTEK_MESSAGE_ERROR(
					"[nri meshlet] the manifest's level {} range is "
					"invalid — the pass records nothing", level);

				delete[] p_manifest;
				return;
			}

			for (kotek::uint32_t member = 0; member < count; ++member)
			{
				zircon_meshlet_cluster_t record{};

				if (zircon_meshlet_read_cluster_record(p_manifest,
						manifest_size, first + member, record) == false)
				{
					KOTEK_MESSAGE_ERROR(
						"[nri meshlet] the manifest's cluster record {} "
						"is invalid — the pass records nothing",
						first + member);

					delete[] p_manifest;
					return;
				}

				total_soup_vertices += record.m_triangle_count * 3u;
			}
		}

		if (total_soup_vertices == 0u ||
			total_soup_vertices > ZIRCON_DEF_NRI_MESHLET_PASS_MAX_SOUP_VERTICES)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the hierarchy needs {} soup vertices, the "
				"v1 pool cap is {} — the pass records nothing",
				static_cast<kotek::uint32_t>(total_soup_vertices),
				ZIRCON_DEF_NRI_MESHLET_PASS_MAX_SOUP_VERTICES);

			delete[] p_manifest;
			return;
		}

		// ---- the heap pools + the GPU table scratch (the fixture rule:
		// ~megabytes never live on the stack; the uploads are
		// synchronous, so the scratch dies at the end of this call)
		const kotek::uint32_t cluster_count =
			manifest_header.m_cluster_count_total;
		const kotek::uint32_t soup_vertex_count =
			static_cast<kotek::uint32_t>(total_soup_vertices);

		float* p_pool_vb = new (std::nothrow)
			float[static_cast<kotek::size_t>(total_soup_vertices) *
				ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS]{};
		kotek::uint32_t* p_pool_ib = new (std::nothrow)
			kotek::uint32_t[static_cast<kotek::size_t>(
				total_soup_vertices)]{};
		zircon_meshlet_cull_layout::gpu_record_t* p_records =
			new (std::nothrow) zircon_meshlet_cull_layout::gpu_record_t[
				cluster_count]{};
		float* p_parent_errors = new (std::nothrow)
			float[cluster_count];

		if (p_pool_vb == nullptr || p_pool_ib == nullptr ||
			p_records == nullptr || p_parent_errors == nullptr)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the heap pool allocation failed — the "
				"pass records nothing");

			delete[] p_pool_vb;
			delete[] p_pool_ib;
			delete[] p_records;
			delete[] p_parent_errors;
			delete[] p_manifest;
			return;
		}

		// every cluster starts parent-less; the link walk below installs
		// the real parent errors
		for (kotek::uint32_t index = 0; index < cluster_count; ++index)
		{
			p_parent_errors[index] =
				ZIRCON_DEF_MESHLET_CULL_NO_PARENT_ERROR;
		}

		// ---- the cluster load: every level, every cluster, level-major
		// order — the bins dequantize straight into the pooled slices
		kotek::uint32_t soup_cursor = 0;
		bool is_load_failed = false;

		for (kotek::uint32_t level = 0;
			 level < manifest_header.m_lod_level_count && !is_load_failed;
			 ++level)
		{
			kotek::uint32_t first = 0;
			kotek::uint32_t count = 0;

			zircon_meshlet_read_level_range(p_manifest, manifest_size,
				level, first, count);

			for (kotek::uint32_t member = 0; member < count; ++member)
			{
				const kotek::uint32_t cluster_index = first + member;

				zircon_meshlet_cluster_t record{};

				if (zircon_meshlet_read_cluster_record(p_manifest,
						manifest_size, cluster_index, record) == false)
				{
					is_load_failed = true;
					break;
				}

				char entry_name[ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH]{};

				if (zircon_meshlet_read_entry_name(entry_name,
						sizeof(entry_name),
						ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE,
						level, member) == false)
				{
					is_load_failed = true;
					break;
				}

				kotek::static_path_t cluster_path = root_path;
				cluster_path /= entry_name;

				kotek::size_t cluster_size = 0;

				if (this->m_p_filesystem->Get_FileSize(
						cluster_path, cluster_size) == false)
				{
					KOTEK_MESSAGE_ERROR(
						"[nri meshlet] the cluster bin '{}' is missing "
						"— the pass records nothing",
						cluster_path.c_str());

					is_load_failed = true;
					break;
				}

				kotek::uint8_t* p_cluster_bin =
					new (std::nothrow) kotek::uint8_t[cluster_size + 1];

				if (p_cluster_bin == nullptr)
				{
					is_load_failed = true;
					break;
				}

				kotek::size_t cluster_read_size = cluster_size + 1;

				const bool is_cluster_read = this->m_p_filesystem->Read_File(
					cluster_path, p_cluster_bin, cluster_read_size);

				if (is_cluster_read == false ||
					cluster_read_size != cluster_size)
				{
					KOTEK_MESSAGE_ERROR(
						"[nri meshlet] the cluster read failed ({} of "
						"{} bytes) — the pass records nothing",
						static_cast<kotek::uint32_t>(cluster_read_size),
						static_cast<kotek::uint32_t>(cluster_size));

					delete[] p_cluster_bin;
					is_load_failed = true;
					break;
				}

				float* p_slice_vb =
					p_pool_vb +
					static_cast<kotek::size_t>(soup_cursor) *
						ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;
				kotek::uint32_t* p_slice_ib = p_pool_ib + soup_cursor;

				zircon_meshlet_cluster_content_t content{};

				if (zircon_meshlet_read_cluster(p_cluster_bin, cluster_size,
						record.m_aabb_min, record.m_aabb_max, p_slice_vb,
						zircon_meshlet_read_vb_capacity_floats(
							record.m_triangle_count),
						p_slice_ib, zircon_meshlet_read_ib_capacity(
							record.m_triangle_count),
						content) == false)
				{
					KOTEK_MESSAGE_ERROR(
						"[nri meshlet] the cluster bin '{}' is invalid "
						"— the pass records nothing",
						cluster_path.c_str());

					delete[] p_cluster_bin;
					is_load_failed = true;
					break;
				}

				delete[] p_cluster_bin;

				// the GPU table record (the 64-byte layout — the w lanes
				// carry the cull operands)
				zircon_meshlet_cull_layout::gpu_record_t& gpu_record =
					p_records[cluster_index];

				for (int axis = 0; axis < 3; ++axis)
				{
					gpu_record.m_aabb_min[axis] =
						static_cast<float>(record.m_aabb_min[axis]);
					gpu_record.m_aabb_max[axis] =
						static_cast<float>(record.m_aabb_max[axis]);
					gpu_record.m_cone_axis[axis] = record.m_cone_axis[axis];
				}

				gpu_record.m_aabb_min[3] = record.m_cone_cutoff;
				gpu_record.m_aabb_max[3] = record.m_error_metric;
				gpu_record.m_index_count = content.m_index_count;
				gpu_record.m_base_vertex = soup_cursor;
				gpu_record.m_start_index = soup_cursor;
				gpu_record.m_reserved = 0u;

				soup_cursor += content.m_soup_vertex_count;
			}
		}

		if (is_load_failed || soup_cursor != soup_vertex_count)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the hierarchy load did not produce the "
				"expected {} soup vertices (got {}) — the pass records "
				"nothing",
				soup_vertex_count, soup_cursor);

			delete[] p_pool_vb;
			delete[] p_pool_ib;
			delete[] p_records;
			delete[] p_parent_errors;
			delete[] p_manifest;
			return;
		}

		// ---- the parent errors: the B3a links run parent->child (the
		// global link table rides right after the cluster records) — the
		// cut predicate needs the child's PARENT error, inverted once
		// here
		const kotek::uint64_t link_table_offset =
			static_cast<kotek::uint64_t>(
				zircon_meshlet_manifest_header_size) +
			static_cast<kotek::uint64_t>(
				manifest_header.m_lod_level_count) *
				zircon_meshlet_manifest_level_size +
			static_cast<kotek::uint64_t>(cluster_count) *
				zircon_meshlet_manifest_record_size;

		for (kotek::uint32_t cluster_index = 0; cluster_index < cluster_count;
			 ++cluster_index)
		{
			zircon_meshlet_cluster_t record{};

			if (zircon_meshlet_read_cluster_record(p_manifest,
					manifest_size, cluster_index, record) == false ||
				record.m_child_count == 0u)
			{
				continue;
			}

			for (kotek::uint32_t child = 0; child < record.m_child_count;
				 ++child)
			{
				const kotek::uint64_t link_offset =
					link_table_offset +
					static_cast<kotek::uint64_t>(
						record.m_first_child_link + child) * 4u;

				if (link_offset + 4u > manifest_size)
					continue;

				const kotek::uint32_t child_index =
					zircon_csg_bake_load_u32(p_manifest + link_offset);

				if (child_index < cluster_count)
				{
					p_parent_errors[child_index] = record.m_error_metric;
				}
			}
		}

		for (kotek::uint32_t cluster_index = 0; cluster_index < cluster_count;
			 ++cluster_index)
		{
			p_records[cluster_index].m_cone_axis[3] =
				p_parent_errors[cluster_index];
		}

		// the hierarchy AABB (the debug-orbit camera's frame)
		for (int axis = 0; axis < 3; ++axis)
		{
			this->m_aabb_min[axis] = p_records[0].m_aabb_min[axis];
			this->m_aabb_max[axis] = p_records[0].m_aabb_max[axis];
		}

		for (kotek::uint32_t cluster_index = 1; cluster_index < cluster_count;
			 ++cluster_index)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				if (p_records[cluster_index].m_aabb_min[axis] <
					this->m_aabb_min[axis])
				{
					this->m_aabb_min[axis] =
						p_records[cluster_index].m_aabb_min[axis];
				}

				if (p_records[cluster_index].m_aabb_max[axis] >
					this->m_aabb_max[axis])
				{
					this->m_aabb_max[axis] =
						p_records[cluster_index].m_aabb_max[axis];
				}
			}
		}

		delete[] p_parent_errors;
		delete[] p_manifest;

		// ---- the buffers through the geometry seam (uploads are
		// immediate — readable from the first submitted frame)
		const kotek::uint32_t vb_size_bytes =
			soup_vertex_count * ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES;
		const kotek::uint32_t ib_size_bytes =
			soup_vertex_count * sizeof(kotek::uint32_t);
		const kotek::uint32_t table_size_bytes =
			cluster_count *
			static_cast<kotek::uint32_t>(
				sizeof(zircon_meshlet_cull_layout::gpu_record_t));
		const kotek::uint32_t indirect_size_bytes =
			cluster_count *
			static_cast<kotek::uint32_t>(
				sizeof(zircon_meshlet_cull_layout::indirect_command_t));

		this->m_vertex_buffer = this->m_p_geometry_manager->Create_Buffer(
			vb_size_bytes,
			kotek::core::eRenderGeometryBufferUsage::kVertex);

		this->m_index_buffer = this->m_p_geometry_manager->Create_Buffer(
			ib_size_bytes, kotek::core::eRenderGeometryBufferUsage::kIndex);

		this->m_cluster_table = this->m_p_geometry_manager->Create_Buffer(
			table_size_bytes,
			kotek::core::eRenderGeometryBufferUsage::kShaderRead);

		this->m_indirect_buffer = this->m_p_geometry_manager->Create_Buffer(
			indirect_size_bytes,
			kotek::core::eRenderGeometryBufferUsage::kStorage |
				kotek::core::eRenderGeometryBufferUsage::kIndirect |
				kotek::core::eRenderGeometryBufferUsage::kDevice);

		this->m_counter_buffer = this->m_p_geometry_manager->Create_Buffer(
			4u,
			kotek::core::eRenderGeometryBufferUsage::kStorage |
				kotek::core::eRenderGeometryBufferUsage::kIndirect |
				kotek::core::eRenderGeometryBufferUsage::kDevice);

		this->m_readback_buffer = this->m_p_geometry_manager->Create_Buffer(
			4u, kotek::core::eRenderGeometryBufferUsage::kReadback);

		if (this->m_vertex_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_index_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_cluster_table ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_indirect_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_counter_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle ||
			this->m_readback_buffer ==
				kotek::core::kInvalidRenderGeometryBufferHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] a buffer creation failed — the pass "
				"records nothing");

			delete[] p_pool_vb;
			delete[] p_pool_ib;
			delete[] p_records;
			this->release_resources();
			return;
		}

		// the uploads: ONLY the host-visible pools (the VB/IB/table) —
		// the indirect + counter are GPU-local (device memory is not
		// CPU-mappable) and fully written by the clear/cull dispatches
		// before every draw; the readback buffer receives the one-shot
		// copy
		if (this->m_p_geometry_manager->Upload_Buffer(
				this->m_vertex_buffer, 0, p_pool_vb, vb_size_bytes) ==
				false ||
			this->m_p_geometry_manager->Upload_Buffer(
				this->m_index_buffer, 0, p_pool_ib, ib_size_bytes) ==
				false ||
			this->m_p_geometry_manager->Upload_Buffer(
				this->m_cluster_table, 0, p_records, table_size_bytes) ==
				false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] a buffer upload failed — the pass "
				"records nothing");

			delete[] p_pool_vb;
			delete[] p_pool_ib;
			delete[] p_records;
			this->release_resources();
			return;
		}

		delete[] p_pool_vb;
		delete[] p_pool_ib;
		// p_records ownership moves to the member — it is the CPU
		// mirror's reference for the frame classification, freed in
		// release_resources
		this->m_p_records = p_records;

		// ---- the pipelines (the Slang dxil route's raw blobs)
		kotek::uint8_t* p_vertex_blob = nullptr;
		kotek::uint8_t* p_pixel_blob = nullptr;
		kotek::uint8_t* p_cull_blob = nullptr;
		kotek::uint32_t vertex_blob_size = 0;
		kotek::uint32_t pixel_blob_size = 0;
		kotek::uint32_t cull_blob_size = 0;

		if (this->load_shader_blob("meshlet_cluster.vs.dxil",
				p_vertex_blob, vertex_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster.fs.dxil",
				p_pixel_blob, pixel_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster_cull.cs.dxil",
				p_cull_blob, cull_blob_size) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the shader blobs (shader_cache/nri/dx12/"
				"meshlet_cluster*.dxil) are missing or oversized — "
				"the pass records nothing (build zircon_shaders "
				"first)");

			delete[] p_vertex_blob;
			delete[] p_pixel_blob;
			delete[] p_cull_blob;

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

		kotek::core::ktkRenderGeometryComputePipelineDesc cull_desc{};
		cull_desc.m_compute_shader.m_p_bytecode = p_cull_blob;
		cull_desc.m_compute_shader.m_size_bytes = cull_blob_size;
		cull_desc.m_compute_shader.m_p_entry_point = "cs_main";
		cull_desc.m_storage_buffer_count = 3u; // table + commands + counter
		cull_desc.m_read_only_storage_mask = 1u << 0u; // the table is SRV
		cull_desc.m_push_constant_bytes =
			ZIRCON_DEF_NRI_MESHLET_PASS_CULL_PARAMS_BYTES;

		this->m_cull_pipeline =
			this->m_p_geometry_manager->Create_Compute_Pipeline(cull_desc);

		// the backend copied what it needed during creation
		delete[] p_vertex_blob;
		delete[] p_pixel_blob;
		delete[] p_cull_blob;

		if (this->m_pipeline ==
				kotek::core::kInvalidRenderGeometryPipelineHandle ||
			this->m_cull_pipeline ==
				kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] a pipeline creation failed — the pass "
				"records nothing");

			this->release_resources();
			return;
		}

		this->m_index_count = soup_vertex_count;
		this->m_cluster_count = cluster_count;
		this->m_is_ready = true;

		KOTEK_MESSAGE(
			"[nri meshlet] pass created: hierarchy '{}' = {} clusters "
			"across {} LOD levels ({} soup vertices), VB {} B + IB {} B "
			"+ table {} B uploaded, pipelines created (vs+ps DXIL 64 B "
			"push, cull cs DXIL {} B push + 3 storage bindings)",
			ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE,
			cluster_count, manifest_header.m_lod_level_count,
			soup_vertex_count, vb_size_bytes, ib_size_bytes,
			table_size_bytes,
			ZIRCON_DEF_NRI_MESHLET_PASS_CULL_PARAMS_BYTES);
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::Record(
		kotek::core::ktkIRenderFramePassContext* p_context)
	{
		KOTEK_ASSERT(p_context,
			"the NRI meshlet pass needs a valid frame pass context");

		if (p_context == nullptr || this->m_is_ready == false)
			return;

		// the fixed debug orbit frames the hierarchy AABB (see the
		// header: the draw proves the cull/LOD path, not the camera
		// chain)
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

		const float fov_y =
			kotek::ktk::math::convert_to_radians(60.0f);

		kotek::ktk::math::matrix4x4f projection =
			kotek::ktk::math::perspective(
				fov_y, aspect,
				(radius * 0.05f > 0.01f) ? radius * 0.05f : 0.01f,
				radius * 20.0f + 100.0f);

		const kotek::ktk::math::matrix4x4f view_projection =
			projection * view;

		// ---- the cull view (the push-constant block, the kernel's exact
		// layout) + the CPU mirror of this frame's classification
		zircon_meshlet_cull_view_t cull_view{};

		zircon_meshlet_cull_extract_frustum_planes(
			kotek::ktk::math::value_ptr(view_projection),
			cull_view.m_planes);

		cull_view.m_camera_position[0] = eye[0];
		cull_view.m_camera_position[1] = eye[1];
		cull_view.m_camera_position[2] = eye[2];
		cull_view.m_proj_scale =
			0.5f * static_cast<float>(height) / std::tan(0.5f * fov_y);
		cull_view.m_error_threshold =
			ZIRCON_DEF_NRI_MESHLET_LOD_ERROR_THRESHOLD;

		zircon_meshlet_cull_layout::indirect_command_t
			mirror_commands[ZIRCON_DEF_NRI_MESHLET_PASS_MAX_CLUSTERS]{};

		this->m_cpu_visible_count = zircon_meshlet_cull_compact(
			this->m_p_records, this->m_cluster_count, cull_view,
			mirror_commands,
			ZIRCON_DEF_NRI_MESHLET_PASS_MAX_CLUSTERS);

		// ---- the cull dispatch pair (clear, then classify+compact)
		p_context->Set_Compute_Pipeline(this->m_cull_pipeline);
		p_context->Set_Compute_Storage_Buffer(0u, this->m_cluster_table, 0u,
			this->m_cluster_count *
				static_cast<kotek::uint32_t>(
					sizeof(zircon_meshlet_cull_layout::gpu_record_t)));
		p_context->Set_Compute_Storage_Buffer(1u, this->m_indirect_buffer,
			0u,
			this->m_cluster_count *
				static_cast<kotek::uint32_t>(
					sizeof(zircon_meshlet_cull_layout::indirect_command_t)));
		p_context->Set_Compute_Storage_Buffer(2u, this->m_counter_buffer,
			0u, 4u);

		struct cull_params_t
		{
			float m_planes[24];
			float m_camera[4];
			float m_meta[4]; // x = count, y = mode, z = scale, w = threshold
		};

		static_assert(sizeof(cull_params_t) ==
				ZIRCON_DEF_NRI_MESHLET_PASS_CULL_PARAMS_BYTES,
			"the cull params block must stay 128 bytes (the kernel's "
			"CullParams cbuffer)");

		cull_params_t params{};

		for (int index = 0; index < 24; ++index)
		{
			params.m_planes[index] = cull_view.m_planes[index];
		}

		params.m_camera[0] = cull_view.m_camera_position[0];
		params.m_camera[1] = cull_view.m_camera_position[1];
		params.m_camera[2] = cull_view.m_camera_position[2];

		// the meta lanes: x/y carry INTEGER BITS (the kernel reads them
		// through asuint — the memcpy discipline, the B1 lesson), z/w
		// are real floats
		std::memcpy(&params.m_meta[0], &this->m_cluster_count,
			sizeof(kotek::uint32_t));
		params.m_meta[2] = cull_view.m_proj_scale;
		params.m_meta[3] = cull_view.m_error_threshold;

		kotek::uint32_t kernel_mode = 0u; // clear
		std::memcpy(&params.m_meta[1], &kernel_mode,
			sizeof(kotek::uint32_t));

		p_context->Set_Compute_Push_Constants(&params, sizeof(params));
		p_context->Dispatch(1u, 1u, 1u);

		kernel_mode = 1u; // cull
		std::memcpy(&params.m_meta[1], &kernel_mode,
			sizeof(kotek::uint32_t));

		p_context->Set_Compute_Push_Constants(&params, sizeof(params));

		const kotek::uint32_t group_count =
			(this->m_cluster_count + 63u) / 64u;

		p_context->Dispatch(group_count, 1u, 1u);

		// ---- the access-kind change: the GPU-written storage becomes
		// the draw's indirect arguments (both buffers)
		p_context->Barrier_Buffer(this->m_indirect_buffer,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierAccess::kIndirectArgument,
			kotek::core::eRenderGeometryBarrierStage::kCompute,
			kotek::core::eRenderGeometryBarrierStage::kIndirect);
		p_context->Barrier_Buffer(this->m_counter_buffer,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierAccess::kIndirectArgument,
			kotek::core::eRenderGeometryBarrierStage::kCompute,
			kotek::core::eRenderGeometryBarrierStage::kIndirect);

		p_context->Begin_Render_Pass();
		p_context->Set_Pipeline(this->m_pipeline);
		p_context->Set_Push_Constants(
			kotek::ktk::math::value_ptr(view_projection), 64u);
		p_context->Set_Vertex_Buffer(this->m_vertex_buffer, 0u,
			ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES, 0u);
		p_context->Set_Index_Buffer(this->m_index_buffer, 0u,
			kotek::core::eRenderGeometryIndexFormat::kUint32);
		p_context->Draw_Indexed_Indirect(this->m_indirect_buffer, 0u,
			this->m_cluster_count,
			static_cast<kotek::uint32_t>(
				sizeof(zircon_meshlet_cull_layout::indirect_command_t)),
			this->m_counter_buffer, 0u);
		p_context->End_Render_Pass();

		// ---- restore the storage state for the next frame's dispatches
		p_context->Barrier_Buffer(this->m_indirect_buffer,
			kotek::core::eRenderGeometryBarrierAccess::kIndirectArgument,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierStage::kIndirect,
			kotek::core::eRenderGeometryBarrierStage::kCompute);
		p_context->Barrier_Buffer(this->m_counter_buffer,
			kotek::core::eRenderGeometryBarrierAccess::kIndirectArgument,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierStage::kIndirect,
			kotek::core::eRenderGeometryBarrierStage::kCompute);

		// ---- the one-shot A/B readback (the B1 discipline, NRI form):
		// frame 0 copies the counter to the readback heap right after
		// the draw; the frame pacing makes it long complete at
		// ZIRCON_DEF_NRI_MESHLET_PASS_READBACK_FRAME, where the CPU maps
		// it and logs the equivalence
		if (this->m_readback_copied == false)
		{
			this->m_readback_copied = true;

			p_context->Barrier_Buffer(this->m_counter_buffer,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierAccess::kCopySource,
				kotek::core::eRenderGeometryBarrierStage::kCompute,
				kotek::core::eRenderGeometryBarrierStage::kCopy);
			p_context->Copy_Buffer(this->m_readback_buffer, 0u,
				this->m_counter_buffer, 0u, 4u);
			p_context->Barrier_Buffer(this->m_counter_buffer,
				kotek::core::eRenderGeometryBarrierAccess::kCopySource,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierStage::kCopy,
				kotek::core::eRenderGeometryBarrierStage::kCompute);
		}
		else if (this->m_ab_logged == false &&
			this->m_frame_counter >=
				ZIRCON_DEF_NRI_MESHLET_PASS_READBACK_FRAME)
		{
			kotek::uint32_t gpu_visible_count = 0u;

			if (this->m_p_geometry_manager->Read_Buffer(
					this->m_readback_buffer, 0u, &gpu_visible_count,
					4u))
			{
				this->m_gpu_visible_count = gpu_visible_count;
				this->m_ab_logged = true;

				KOTEK_MESSAGE(
					"[nri meshlet] first indirect submit: {} cluster "
					"slots, cpu-mirror visible {}, 1 indirect submit x "
					"{} command slots",
					this->m_cluster_count, this->m_cpu_visible_count,
					this->m_cluster_count);

				KOTEK_MESSAGE(
					"[nri meshlet] A/B readback: gpu-visible {} vs "
					"cpu-mirror {} — {} ({} cluster slots)",
					this->m_gpu_visible_count, this->m_cpu_visible_count,
					this->m_gpu_visible_count == this->m_cpu_visible_count
						? "match"
						: "MISMATCH",
					this->m_cluster_count);
			}
		}

		++this->m_frame_counter;
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

	kotek::uint32_t
		zircon_render_graph_pass_meshlet_cluster_nri::get_cluster_count(
			void) const noexcept
	{
		return this->m_cluster_count;
	}

	kotek::uint32_t
		zircon_render_graph_pass_meshlet_cluster_nri::get_cpu_visible_count(
			void) const noexcept
	{
		return this->m_cpu_visible_count;
	}

	kotek::uint32_t
		zircon_render_graph_pass_meshlet_cluster_nri::get_gpu_visible_count(
			void) const noexcept
	{
		return this->m_gpu_visible_count;
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

	kotek::core::ktkRenderGeometryBufferHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_cluster_table(
			void) const noexcept
	{
		return this->m_cluster_table;
	}

	kotek::core::ktkRenderGeometryBufferHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_indirect_buffer(
			void) const noexcept
	{
		return this->m_indirect_buffer;
	}

	kotek::core::ktkRenderGeometryBufferHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_counter_buffer(
			void) const noexcept
	{
		return this->m_counter_buffer;
	}

	kotek::core::ktkRenderGeometryPipelineHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_pipeline(
			void) const noexcept
	{
		return this->m_pipeline;
	}

	kotek::core::ktkRenderGeometryPipelineHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_cull_pipeline(
			void) const noexcept
	{
		return this->m_cull_pipeline;
	}
} // namespace no_streaming
