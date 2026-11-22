#include "zircon_render_graph_pass_meshlet_cluster_nri.h"

#include <kotek.core.main_manager/include/kotek_core_main_manager.h>

#include "../../../../engine/zircon_game_manager.h"
#include "../../../../game/session/zircon_session_game.h"
#include "../../../../game/session/zircon_session_game_manager.h"
#include "../../../../world/zircon_world.h"

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

	void zircon_render_graph_pass_meshlet_cluster_nri::release_render_targets(
		void) noexcept
	{
		if (this->m_p_geometry_manager)
		{
			const kotek::core::ktkRenderGeometryTextureViewHandle views[] = {
				this->m_visibility_shader_view, this->m_visibility_rt_view,
				this->m_depth_view};
			for (kotek::uint32_t index = 0; index < 3u; ++index)
			{
				if (views[index] !=
					kotek::core::kInvalidRenderGeometryTextureViewHandle)
				{
					this->m_p_geometry_manager->Destroy_Texture_View(views[index]);
				}
			}

			if (this->m_visibility_texture !=
				kotek::core::kInvalidRenderGeometryTextureHandle)
			{
				this->m_p_geometry_manager->Destroy_Texture(
					this->m_visibility_texture);
			}
			if (this->m_depth_texture !=
				kotek::core::kInvalidRenderGeometryTextureHandle)
			{
				this->m_p_geometry_manager->Destroy_Texture(this->m_depth_texture);
			}
		}

		this->m_visibility_texture =
			kotek::core::kInvalidRenderGeometryTextureHandle;
		this->m_depth_texture = kotek::core::kInvalidRenderGeometryTextureHandle;
		this->m_visibility_rt_view =
			kotek::core::kInvalidRenderGeometryTextureViewHandle;
		this->m_visibility_shader_view =
			kotek::core::kInvalidRenderGeometryTextureViewHandle;
		this->m_depth_view = kotek::core::kInvalidRenderGeometryTextureViewHandle;
		this->m_target_width = 0;
		this->m_target_height = 0;
		this->m_targets_need_initial_transition = false;
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::release_resources(
		void) noexcept
	{
		if (this->m_p_geometry_manager == nullptr)
			return;

		const kotek::core::ktkRenderGeometryBufferHandle buffers[] = {
			this->m_vertex_buffer, this->m_index_buffer,
			this->m_cluster_table, this->m_indirect_buffer,
			this->m_counter_buffer, this->m_readback_buffer,
			this->m_light_streams[0], this->m_light_streams[1],
			this->m_light_streams[2], this->m_light_streams[3],
			this->m_light_tile_counts, this->m_light_tile_indices,
			this->m_light_bin_counters, this->m_light_stats_readback};

		for (kotek::uint32_t index = 0; index < 13u; ++index)
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
		for (kotek::uint32_t index = 0; index < 4u; ++index)
		{
			this->m_light_streams[index] =
				kotek::core::kInvalidRenderGeometryBufferHandle;
		}
		this->m_light_tile_counts =
			kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_light_tile_indices =
			kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_light_bin_counters =
			kotek::core::kInvalidRenderGeometryBufferHandle;
		this->m_light_stats_readback =
			kotek::core::kInvalidRenderGeometryBufferHandle;

		if (this->m_pipeline != kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(this->m_pipeline);
			this->m_pipeline = kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		if (this->m_visibility_pipeline !=
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(
				this->m_visibility_pipeline);
			this->m_visibility_pipeline =
				kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		if (this->m_cull_pipeline !=
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(
				this->m_cull_pipeline);
			this->m_cull_pipeline =
				kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		if (this->m_light_bin_pipeline !=
			kotek::core::kInvalidRenderGeometryPipelineHandle)
		{
			this->m_p_geometry_manager->Destroy_Pipeline(
				this->m_light_bin_pipeline);
			this->m_light_bin_pipeline =
				kotek::core::kInvalidRenderGeometryPipelineHandle;
		}

		this->release_render_targets();
		this->m_index_count = 0;
		this->m_cluster_count = 0;
		this->m_cpu_visible_count = 0;
		this->m_gpu_visible_count = 0;
		this->m_frame_counter = 0;
		this->m_is_ready = false;
		this->m_readback_copied = false;
		this->m_ab_logged = false;
		this->m_gpu_light_assignments = 0;
		this->m_gpu_light_dropped = 0;
		this->m_cpu_light_assignments = 0;
		this->m_cpu_light_dropped = 0;
		this->m_light_readback_copied = false;
		this->m_light_ab_logged = false;
		this->m_light_warned_tile_cap = false;
		this->m_light_lists_are_srv = false;
		this->m_light_buffer.clear();

		delete[] this->m_p_records;
		this->m_p_records = nullptr;
		delete[] this->m_p_mirror_tile_counts;
		this->m_p_mirror_tile_counts = nullptr;
		delete[] this->m_p_mirror_tile_indices;
		this->m_p_mirror_tile_indices = nullptr;
	}

	bool zircon_render_graph_pass_meshlet_cluster_nri::ensure_render_targets(
		kotek::uint32_t width, kotek::uint32_t height) noexcept
	{
		if (width == 0 || height == 0)
			return false;

		if (this->m_target_width == width &&
			this->m_target_height == height &&
			this->m_visibility_shader_view !=
				kotek::core::kInvalidRenderGeometryTextureViewHandle)
		{
			return true;
		}

		this->release_render_targets();

		kotek::core::ktkRenderGeometryTextureDesc visibility_desc{};
		visibility_desc.m_width = width;
		visibility_desc.m_height = height;
		visibility_desc.m_color_format =
			kotek::core::eRenderGeometryColorFormat::kR32Uint;
		this->m_visibility_texture =
			this->m_p_geometry_manager->Create_Texture(visibility_desc);

		kotek::core::ktkRenderGeometryTextureDesc depth_desc{};
		depth_desc.m_width = width;
		depth_desc.m_height = height;
		depth_desc.m_depth_format =
			kotek::core::eRenderGeometryDepthFormat::kD32Float;
		this->m_depth_texture =
			this->m_p_geometry_manager->Create_Texture(depth_desc);

		if (this->m_visibility_texture ==
				kotek::core::kInvalidRenderGeometryTextureHandle ||
			this->m_depth_texture ==
				kotek::core::kInvalidRenderGeometryTextureHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] visibility/depth texture creation failed");
			this->release_render_targets();
			return false;
		}

		kotek::core::ktkRenderGeometryTextureViewDesc visibility_view{};
		visibility_view.m_texture = this->m_visibility_texture;
		visibility_view.m_type =
			kotek::core::eRenderGeometryTextureViewType::kRenderTarget;
		visibility_view.m_color_format =
			kotek::core::eRenderGeometryColorFormat::kR32Uint;
		this->m_visibility_rt_view =
			this->m_p_geometry_manager->Create_Texture_View(visibility_view);

		visibility_view.m_type =
			kotek::core::eRenderGeometryTextureViewType::kShaderResource;
		this->m_visibility_shader_view =
			this->m_p_geometry_manager->Create_Texture_View(visibility_view);

		kotek::core::ktkRenderGeometryTextureViewDesc depth_view{};
		depth_view.m_texture = this->m_depth_texture;
		depth_view.m_type =
			kotek::core::eRenderGeometryTextureViewType::kDepthStencil;
		depth_view.m_depth_format =
			kotek::core::eRenderGeometryDepthFormat::kD32Float;
		this->m_depth_view =
			this->m_p_geometry_manager->Create_Texture_View(depth_view);

		if (this->m_visibility_rt_view ==
				kotek::core::kInvalidRenderGeometryTextureViewHandle ||
			this->m_visibility_shader_view ==
				kotek::core::kInvalidRenderGeometryTextureViewHandle ||
			this->m_depth_view ==
				kotek::core::kInvalidRenderGeometryTextureViewHandle)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] visibility/depth view creation failed");
			this->release_render_targets();
			return false;
		}

		this->m_target_width = width;
		this->m_target_height = height;
		this->m_targets_need_initial_transition = true;

		KOTEK_MESSAGE(
			"[nri meshlet] visibility targets: {}x{} R32Uint + D32Float",
			width, height);
		return true;
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::Initialize_Nri(
		kotek::core::ktkMainManager* p_main_manager)
	{
		this->release_resources();

		if (p_main_manager == nullptr)
			return;

		this->m_p_main_manager = p_main_manager;
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
		manifest_relative = "models/meshlets/";
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
				ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_FLOATS]{};
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
						ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_FLOATS;
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

				// Expand the reader's six-float vertices IN PLACE, back to
				// front, and append the nonzero cluster id. The allocation
				// already reserves the seventh float for every soup vertex.
				const kotek::uint32_t visibility_id = cluster_index + 1u;
				for (kotek::uint32_t cursor = content.m_soup_vertex_count;
					 cursor > 0; --cursor)
				{
					const kotek::uint32_t vertex = cursor - 1u;
					float* p_destination = p_slice_vb +
						static_cast<kotek::size_t>(vertex) *
							ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_FLOATS;
					const float* p_source = p_slice_vb +
						static_cast<kotek::size_t>(vertex) *
							ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS;
					// destination > source: copying components backwards is
					// the overlap-safe memmove discipline without a std call
					for (int component =
							 ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS - 1;
						 component >= 0; --component)
					{
						p_destination[component] = p_source[component];
					}
					kotek::ktk::memory::memcpy(p_destination +
							ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_FLOATS,
						&visibility_id, sizeof(visibility_id));
				}

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
			soup_vertex_count * ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_BYTES;
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

		for (kotek::uint32_t stream = 0; stream < 4u; ++stream)
		{
			this->m_light_streams[stream] =
				this->m_p_geometry_manager->Create_Buffer(
					ZIRCON_DEF_LIGHT_MAX_COUNT *
						static_cast<kotek::uint32_t>(
							sizeof(zircon_light_stream_record_t)),
					kotek::core::eRenderGeometryBufferUsage::kShaderRead);
		}
		this->m_light_tile_counts = this->m_p_geometry_manager->Create_Buffer(
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
				static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t)),
			kotek::core::eRenderGeometryBufferUsage::kStorage |
				kotek::core::eRenderGeometryBufferUsage::kShaderRead |
				kotek::core::eRenderGeometryBufferUsage::kDevice);
		this->m_light_tile_indices = this->m_p_geometry_manager->Create_Buffer(
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
				ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE *
				static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t)),
			kotek::core::eRenderGeometryBufferUsage::kStorage |
				kotek::core::eRenderGeometryBufferUsage::kShaderRead |
				kotek::core::eRenderGeometryBufferUsage::kDevice);
		this->m_light_bin_counters = this->m_p_geometry_manager->Create_Buffer(
			2u * static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t)),
			kotek::core::eRenderGeometryBufferUsage::kStorage |
				kotek::core::eRenderGeometryBufferUsage::kDevice);
		this->m_light_stats_readback = this->m_p_geometry_manager->Create_Buffer(
			2u * static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t)),
			kotek::core::eRenderGeometryBufferUsage::kReadback);

		this->m_p_mirror_tile_counts = new (std::nothrow)
			kotek::uint32_t[ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES]{};
		this->m_p_mirror_tile_indices = new (std::nothrow)
			kotek::uint32_t[ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
				ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE]{};

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

		bool are_light_buffers_valid =
			this->m_p_mirror_tile_counts && this->m_p_mirror_tile_indices &&
			this->m_light_tile_counts !=
				kotek::core::kInvalidRenderGeometryBufferHandle &&
			this->m_light_tile_indices !=
				kotek::core::kInvalidRenderGeometryBufferHandle &&
			this->m_light_bin_counters !=
				kotek::core::kInvalidRenderGeometryBufferHandle &&
			this->m_light_stats_readback !=
				kotek::core::kInvalidRenderGeometryBufferHandle;
		for (kotek::uint32_t stream = 0; stream < 4u; ++stream)
		{
			are_light_buffers_valid = are_light_buffers_valid &&
				this->m_light_streams[stream] !=
					kotek::core::kInvalidRenderGeometryBufferHandle;
		}
		if (are_light_buffers_valid == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] a light buffer/allocation failed — inert");
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

		// ---- four pipelines over THREE compute/fragment shader stages:
		// shared VS + visibility FS + clustered forward FS, plus cull and
		// light-binning compute. Both graphics pipelines share the VS blob.
		kotek::uint8_t* p_vertex_blob = nullptr;
		kotek::uint8_t* p_pixel_blob = nullptr;
		kotek::uint8_t* p_visibility_blob = nullptr;
		kotek::uint8_t* p_cull_blob = nullptr;
		kotek::uint8_t* p_light_bin_blob = nullptr;
		kotek::uint32_t vertex_blob_size = 0;
		kotek::uint32_t pixel_blob_size = 0;
		kotek::uint32_t visibility_blob_size = 0;
		kotek::uint32_t cull_blob_size = 0;
		kotek::uint32_t light_bin_blob_size = 0;

		if (this->load_shader_blob("meshlet_cluster.vs.dxil",
				p_vertex_blob, vertex_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster.fs.dxil",
				p_pixel_blob, pixel_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster_visibility.fs.dxil",
				p_visibility_blob, visibility_blob_size) == false ||
			this->load_shader_blob("meshlet_cluster_cull.cs.dxil",
				p_cull_blob, cull_blob_size) == false ||
			this->load_shader_blob("light_cluster_bin.cs.dxil",
				p_light_bin_blob, light_bin_blob_size) == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[nri meshlet] the shader blobs (shader_cache/nri/dx12/"
				"meshlet_cluster*.dxil + light_cluster_bin.cs.dxil) are "
				"missing or oversized — build zircon_shaders first");

			delete[] p_vertex_blob;
			delete[] p_pixel_blob;
			delete[] p_visibility_blob;
			delete[] p_cull_blob;
			delete[] p_light_bin_blob;

			this->release_resources();
			return;
		}

		const kotek::core::ktkRenderGeometryVertexAttributeDesc
			attributes[3] = {
				{"POSITION", 0u,
					kotek::core::eRenderGeometryVertexFormat::kFloat3, 0u},
				{"NORMAL", 0u,
					kotek::core::eRenderGeometryVertexFormat::kFloat3, 12u},
				{"TEXCOORD", 0u,
					kotek::core::eRenderGeometryVertexFormat::kUint1,
					ZIRCON_DEF_MESHLET_READ_VERTEX_STRIDE_BYTES},
			};

		kotek::core::ktkRenderGeometryPipelineDesc pipeline_desc{};
		pipeline_desc.m_vertex_shader.m_p_bytecode = p_vertex_blob;
		pipeline_desc.m_vertex_shader.m_size_bytes = vertex_blob_size;
		pipeline_desc.m_vertex_shader.m_p_entry_point = "vs_main";
		pipeline_desc.m_pixel_shader.m_p_bytecode = p_pixel_blob;
		pipeline_desc.m_pixel_shader.m_size_bytes = pixel_blob_size;
		pipeline_desc.m_pixel_shader.m_p_entry_point = "fs_main";
		pipeline_desc.m_p_attributes = attributes;
		pipeline_desc.m_attribute_count = 3;
		pipeline_desc.m_vertex_stride_bytes =
			ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_BYTES;
		pipeline_desc.m_color_format =
			kotek::core::eRenderGeometryColorFormat::kRGBA8Unorm;
		pipeline_desc.m_depth_format =
			kotek::core::eRenderGeometryDepthFormat::kD32Float;
		pipeline_desc.m_depth_compare =
			kotek::core::eRenderGeometryDepthCompareOp::kEqual;
		pipeline_desc.m_depth_write = false;
		pipeline_desc.m_shader_resource_count = 1u; // visibility at t0
		const kotek::uint32_t light_strides[6] = {16u, 16u, 16u, 16u, 4u, 4u};
		pipeline_desc.m_structured_buffer_count = 6u; // light streams at t1..t6
		pipeline_desc.m_p_structured_buffer_strides = light_strides;
		pipeline_desc.m_push_constant_bytes =
			ZIRCON_DEF_NRI_MESHLET_PASS_GRAPHICS_PARAMS_BYTES;

		this->m_pipeline = this->m_p_geometry_manager->Create_Pipeline(
			pipeline_desc);

		kotek::core::ktkRenderGeometryPipelineDesc visibility_desc =
			pipeline_desc;
		visibility_desc.m_pixel_shader.m_p_bytecode = p_visibility_blob;
		visibility_desc.m_pixel_shader.m_size_bytes = visibility_blob_size;
		visibility_desc.m_color_format =
			kotek::core::eRenderGeometryColorFormat::kR32Uint;
		visibility_desc.m_depth_format =
			kotek::core::eRenderGeometryDepthFormat::kD32Float;
		visibility_desc.m_depth_compare =
			kotek::core::eRenderGeometryDepthCompareOp::kLess;
		visibility_desc.m_depth_write = true;
		visibility_desc.m_shader_resource_count = 0u;
		visibility_desc.m_structured_buffer_count = 0u;
		visibility_desc.m_p_structured_buffer_strides = nullptr;
		this->m_visibility_pipeline =
			this->m_p_geometry_manager->Create_Pipeline(visibility_desc);

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

		kotek::core::ktkRenderGeometryComputePipelineDesc light_desc{};
		light_desc.m_compute_shader.m_p_bytecode = p_light_bin_blob;
		light_desc.m_compute_shader.m_size_bytes = light_bin_blob_size;
		light_desc.m_compute_shader.m_p_entry_point = "cs_main";
		light_desc.m_storage_buffer_count = 6u;
		light_desc.m_read_only_storage_mask =
			(1u << 0u) | (1u << 1u) | (1u << 2u);
		light_desc.m_push_constant_bytes =
			ZIRCON_DEF_NRI_MESHLET_PASS_LIGHT_BIN_PARAMS_BYTES;
		this->m_light_bin_pipeline =
			this->m_p_geometry_manager->Create_Compute_Pipeline(light_desc);

		// the backend copied what it needed during creation
		delete[] p_vertex_blob;
		delete[] p_pixel_blob;
		delete[] p_visibility_blob;
		delete[] p_cull_blob;
		delete[] p_light_bin_blob;

		if (this->m_pipeline ==
				kotek::core::kInvalidRenderGeometryPipelineHandle ||
			this->m_visibility_pipeline ==
				kotek::core::kInvalidRenderGeometryPipelineHandle ||
			this->m_cull_pipeline ==
				kotek::core::kInvalidRenderGeometryPipelineHandle ||
			this->m_light_bin_pipeline ==
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
			"+ table {} B uploaded; shared VS + visibility/forward FS + "
			"cull CS ({} B push)",
			ZIRCON_DEF_RENDER_PASS_MESHLET_CLUSTER_NRI_SCENE,
			cluster_count, manifest_header.m_lod_level_count,
			soup_vertex_count, vb_size_bytes, ib_size_bytes,
			table_size_bytes,
			ZIRCON_DEF_NRI_MESHLET_PASS_CULL_PARAMS_BYTES);
	}

	void zircon_render_graph_pass_meshlet_cluster_nri::drive_light_binning(
		kotek::core::ktkIRenderFramePassContext* p_context,
		const zircon_meshlet_cull_view_t& cull_view,
		const kotek::ktk::math::matrix4x4f& view_projection,
		kotek::uint32_t width, kotek::uint32_t height,
		float out_cluster_info[4]) noexcept
	{
		for (kotek::uint32_t lane = 0; lane < 4u; ++lane)
			out_cluster_info[lane] = 0.0f;

		// The previous frame's fragment SRVs return to UAV before any early
		// exit, so a zero-light frame cannot strand the access state.
		if (this->m_light_lists_are_srv)
		{
			p_context->Barrier_Buffer(this->m_light_tile_counts,
				kotek::core::eRenderGeometryBarrierAccess::kShaderRead,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierStage::kAll,
				kotek::core::eRenderGeometryBarrierStage::kCompute);
			p_context->Barrier_Buffer(this->m_light_tile_indices,
				kotek::core::eRenderGeometryBarrierAccess::kShaderRead,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierStage::kAll,
				kotek::core::eRenderGeometryBarrierStage::kCompute);
			this->m_light_lists_are_srv = false;
		}

		zircon_world* p_world = nullptr;
		zircon_game_manager* p_game_manager = dynamic_cast<zircon_game_manager*>(
			this->m_p_main_manager->GetGameManager());
		if (p_game_manager)
		{
			zircon_session_game_manager* p_sessions =
				p_game_manager->get_session_game_manager();
			if (p_sessions)
			{
				for (kotek::uint8_t id = 0;
					 id < ZIRCON_DEF_SESSION_GAME_MANAGER_MAX_SESSION_COUNT; ++id)
				{
					zircon_session_game* p_session = p_sessions->get_session(id);
					if (p_session)
					{
						p_world = p_session->get_world();
						break;
					}
				}
			}
		}

		kotek::uint32_t light_count = this->m_light_buffer.collect_from_world(
			p_world && p_world->is_initialized() ? p_world->get_factory()
				: nullptr,
			p_world && p_world->is_initialized() ? p_world->get_ecs_context()
				: nullptr,
			p_world ? p_world->get_entity_count_max_limit() : 0u);

#if ZIRCON_DEF_NRI_MESHLET_PASS_ENABLE_LIGHT_FIXTURE
		if (light_count == 0)
		{
			const float sun_dir[3] = {-0.4f, -1.0f, -0.3f};
			const float sun_color[3] = {1.0f, 0.98f, 0.95f};
			const float point_position[3] = {0.0f, 4.0f, 0.0f};
			const float point_color[3] = {0.35f, 0.55f, 1.0f};
			this->m_light_buffer.append_light(eZirconLightType::kDirectional,
				nullptr, sun_dir, sun_color, 1.0f, 0.0f, 0.0f, 0.0f,
				zircon_render_light_buffer::kLightFlagCastsShadow);
			this->m_light_buffer.append_light(eZirconLightType::kPoint,
				point_position, nullptr, point_color, 3.0f, 12.0f, 0.0f, 0.0f);
			light_count = this->m_light_buffer.get_count();
		}
#endif

		kotek::uint32_t tiles_x = 0;
		kotek::uint32_t tiles_y = 0;
		const bool grid_fits = zircon_render_light_buffer::derive_tile_grid(
			width, height, ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS, tiles_x, tiles_y);
		if (grid_fits == false)
		{
			if (this->m_light_warned_tile_cap == false)
			{
				KOTEK_MESSAGE_WARNING(
					"[nri meshlet] the backbuffer exceeds {} light tiles — "
					"fallback directional",
					ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES);
				this->m_light_warned_tile_cap = true;
			}
			return;
		}
		if (light_count == 0)
			return;

		const kotek::uint32_t tile_total = tiles_x * tiles_y;
		const kotek::uint32_t tile_pixels =
			ZIRCON_DEF_LIGHT_CLUSTER_TILE_PIXELS;
		const kotek::uint32_t max_per_tile =
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE;
		const kotek::uint32_t active = 1u;
		kotek::ktk::memory::memcpy(out_cluster_info + 0, &tiles_x,
			sizeof(tiles_x));
		kotek::ktk::memory::memcpy(out_cluster_info + 1, &active,
			sizeof(active));
		kotek::ktk::memory::memcpy(out_cluster_info + 2, &tile_pixels,
			sizeof(tile_pixels));
		kotek::ktk::memory::memcpy(out_cluster_info + 3, &max_per_tile,
			sizeof(max_per_tile));

		// Upload the four read-only streams (positions/directions/colors/params).
		const kotek::uint32_t stream_bytes = light_count *
			static_cast<kotek::uint32_t>(sizeof(zircon_light_stream_record_t));
		const zircon_light_stream_record_t* stream_data[4] = {
			this->m_light_buffer.get_positions(),
			this->m_light_buffer.get_directions(),
			this->m_light_buffer.get_colors(), this->m_light_buffer.get_params()};
		for (kotek::uint32_t stream = 0; stream < 4u; ++stream)
		{
			if (this->m_p_geometry_manager->Upload_Buffer(
					this->m_light_streams[stream], 0, stream_data[stream],
					stream_bytes) == false)
			{
				KOTEK_MESSAGE_ERROR("[nri meshlet] light stream upload failed");
				out_cluster_info[1] = 0.0f;
				return;
			}
		}

		// The serial mirror proves the GPU's assignments and drops.
		kotek::ktk::memory::memset(this->m_p_mirror_tile_counts, 0,
			tile_total * sizeof(kotek::uint32_t));
		zircon_light_bin_stats_t cpu_stats{};
		zircon_render_light_buffer::bin_lights(
			this->m_light_buffer.get_positions(),
			this->m_light_buffer.get_directions(), this->m_light_buffer.get_params(),
			light_count, cull_view.m_planes,
			kotek::ktk::math::value_ptr(view_projection), width, height,
			tile_pixels, tiles_x, tiles_y, max_per_tile,
			this->m_p_mirror_tile_counts, this->m_p_mirror_tile_indices, cpu_stats);

		struct light_bin_params_t
		{
			float m_planes[24];
			float m_view_projection[16];
			float m_screen_meta[4];
			float m_bin_meta[4];
		};
		static_assert(sizeof(light_bin_params_t) ==
				ZIRCON_DEF_NRI_MESHLET_PASS_LIGHT_BIN_PARAMS_BYTES,
			"the light-binning push block must stay 192 bytes");
		light_bin_params_t params{};
		kotek::ktk::memory::memcpy(params.m_planes, cull_view.m_planes,
			sizeof(params.m_planes));
		kotek::ktk::memory::memcpy(params.m_view_projection,
			kotek::ktk::math::value_ptr(view_projection),
			sizeof(params.m_view_projection));
		params.m_screen_meta[0] = static_cast<float>(width);
		params.m_screen_meta[1] = static_cast<float>(height);
		kotek::ktk::memory::memcpy(&params.m_screen_meta[2], &tiles_x,
			sizeof(tiles_x));
		kotek::ktk::memory::memcpy(&params.m_screen_meta[3], &tiles_y,
			sizeof(tiles_y));

		p_context->Set_Compute_Pipeline(this->m_light_bin_pipeline);
		p_context->Set_Compute_Structured_Buffer(0u, this->m_light_streams[0], 0u,
			stream_bytes, 16u);
		p_context->Set_Compute_Structured_Buffer(1u, this->m_light_streams[1], 0u,
			stream_bytes, 16u);
		p_context->Set_Compute_Structured_Buffer(2u, this->m_light_streams[3], 0u,
			stream_bytes, 16u);
		p_context->Set_Compute_Structured_Buffer(3u, this->m_light_tile_counts, 0u,
			tile_total * sizeof(kotek::uint32_t), 4u);
		p_context->Set_Compute_Structured_Buffer(4u, this->m_light_tile_indices, 0u,
			tile_total * max_per_tile * sizeof(kotek::uint32_t), 4u);
		p_context->Set_Compute_Structured_Buffer(5u, this->m_light_bin_counters, 0u,
			2u * sizeof(kotek::uint32_t), 4u);

		for (kotek::uint32_t mode = 0; mode < 2u; ++mode)
		{
			kotek::ktk::memory::memcpy(&params.m_bin_meta[0], &light_count,
				sizeof(light_count));
			kotek::ktk::memory::memcpy(&params.m_bin_meta[1], &mode,
				sizeof(mode));
			kotek::ktk::memory::memcpy(&params.m_bin_meta[2], &tile_pixels,
				sizeof(tile_pixels));
			kotek::ktk::memory::memcpy(&params.m_bin_meta[3], &max_per_tile,
				sizeof(max_per_tile));
			p_context->Set_Compute_Push_Constants(&params, sizeof(params));
			p_context->Dispatch(mode == 0u ? (tile_total + 63u) / 64u
				: (light_count + 63u) / 64u, 1u, 1u);
		}

		// The forward fragment stage consumes the lists as SRVs.
		p_context->Barrier_Buffer(this->m_light_tile_counts,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierAccess::kShaderRead,
			kotek::core::eRenderGeometryBarrierStage::kCompute,
			kotek::core::eRenderGeometryBarrierStage::kAll);
		p_context->Barrier_Buffer(this->m_light_tile_indices,
			kotek::core::eRenderGeometryBarrierAccess::kStorage,
			kotek::core::eRenderGeometryBarrierAccess::kShaderRead,
			kotek::core::eRenderGeometryBarrierStage::kCompute,
			kotek::core::eRenderGeometryBarrierStage::kAll);
		this->m_light_lists_are_srv = true;

		// Direct buffer readback — no stats image and no mode-2 permutation.
		if (this->m_light_readback_copied == false)
		{
			this->m_cpu_light_assignments = cpu_stats.m_assignment_count;
			this->m_cpu_light_dropped = cpu_stats.m_dropped_count;
			p_context->Barrier_Buffer(this->m_light_bin_counters,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierAccess::kCopySource,
				kotek::core::eRenderGeometryBarrierStage::kCompute,
				kotek::core::eRenderGeometryBarrierStage::kCopy);
			p_context->Copy_Buffer(this->m_light_stats_readback, 0u,
				this->m_light_bin_counters, 0u, 2u * sizeof(kotek::uint32_t));
			p_context->Barrier_Buffer(this->m_light_bin_counters,
				kotek::core::eRenderGeometryBarrierAccess::kCopySource,
				kotek::core::eRenderGeometryBarrierAccess::kStorage,
				kotek::core::eRenderGeometryBarrierStage::kCopy,
				kotek::core::eRenderGeometryBarrierStage::kCompute);
			this->m_light_readback_copied = true;
		}
		else if (this->m_light_ab_logged == false &&
			this->m_frame_counter >= ZIRCON_DEF_NRI_MESHLET_PASS_READBACK_FRAME)
		{
			kotek::uint32_t values[2]{};
			if (this->m_p_geometry_manager->Read_Buffer(
					this->m_light_stats_readback, 0u, values, sizeof(values)))
			{
				this->m_gpu_light_assignments = values[0];
				this->m_gpu_light_dropped = values[1];
				this->m_light_ab_logged = true;
				KOTEK_MESSAGE(
					"[nri meshlet] light cluster A/B: gpu {} assignments "
					"({} dropped) vs cpu-mirror {} ({} dropped) — {}",
					this->m_gpu_light_assignments, this->m_gpu_light_dropped,
					this->m_cpu_light_assignments, this->m_cpu_light_dropped,
					(this->m_gpu_light_assignments == this->m_cpu_light_assignments &&
						this->m_gpu_light_dropped == this->m_cpu_light_dropped)
						? "match"
						: "MISMATCH");
			}
		}
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

		if (width == 0 || height == 0)
			return;
		if (this->ensure_render_targets(width, height) == false)
		{
			// creation already logged the precise failure; do not retry and
			// flood every subsequent frame
			this->m_is_ready = false;
			return;
		}

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

		struct graphics_params_t
		{
			float m_view_projection[16];
			float m_cluster_info[4];
		};
		static_assert(sizeof(graphics_params_t) ==
				ZIRCON_DEF_NRI_MESHLET_PASS_GRAPHICS_PARAMS_BYTES,
			"the draw root block must stay 80 bytes");
		graphics_params_t graphics_params{};
		kotek::ktk::memory::memcpy(graphics_params.m_view_projection,
			kotek::ktk::math::value_ptr(view_projection),
			sizeof(graphics_params.m_view_projection));
		this->drive_light_binning(p_context, cull_view, view_projection, width,
			height, graphics_params.m_cluster_info);

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

		// The visibility texture returns from SRV to color attachment every
		// frame; depth stays in its attachment layout after initial creation.
		const bool is_initial_target_transition =
			this->m_targets_need_initial_transition;
		p_context->Barrier_Texture(this->m_visibility_texture,
			is_initial_target_transition
				? kotek::core::eRenderGeometryTextureBarrierLayout::kUndefined
				: kotek::core::eRenderGeometryTextureBarrierLayout::kShaderResource,
			kotek::core::eRenderGeometryTextureBarrierLayout::kColorAttachment);
		if (is_initial_target_transition)
		{
			p_context->Barrier_Texture(this->m_depth_texture,
				kotek::core::eRenderGeometryTextureBarrierLayout::kUndefined,
				kotek::core::eRenderGeometryTextureBarrierLayout::kDepthAttachment);
			this->m_targets_need_initial_transition = false;
		}

		auto record_indirect_draw = [&](void)
		{
			p_context->Set_Push_Constants(
				&graphics_params, sizeof(graphics_params));
			p_context->Set_Vertex_Buffer(this->m_vertex_buffer, 0u,
				ZIRCON_DEF_NRI_MESHLET_PASS_VERTEX_STRIDE_BYTES, 0u);
			p_context->Set_Index_Buffer(this->m_index_buffer, 0u,
				kotek::core::eRenderGeometryIndexFormat::kUint32);
			p_context->Draw_Indexed_Indirect(this->m_indirect_buffer, 0u,
				this->m_cluster_count,
				static_cast<kotek::uint32_t>(
					sizeof(zircon_meshlet_cull_layout::indirect_command_t)),
				this->m_counter_buffer, 0u);
		};

		// 1. R32Uint cluster IDs + D32 depth. The clear value's bits are all
		// zero, reserving zero for the background on every integer target.
		const float clear_visibility[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		p_context->Begin_Render_Pass_With_Attachments(
			this->m_visibility_rt_view, this->m_depth_view, clear_visibility);
		p_context->Set_Pipeline(this->m_visibility_pipeline);
		record_indirect_draw();
		p_context->End_Render_Pass();

		// 2. Resolve visibility to a shader resource, then shade the same
		// compacted draw list. The ID test rejects other clusters; the SAME
		// D32 surface is also bound read-only with an EQUAL test, which
		// rejects overlapping fragments from the same cluster that an id-only
		// visibility buffer cannot distinguish.
		p_context->Barrier_Texture(this->m_visibility_texture,
			kotek::core::eRenderGeometryTextureBarrierLayout::kColorAttachment,
			kotek::core::eRenderGeometryTextureBarrierLayout::kShaderResource);
		p_context->Begin_Render_Pass_With_Back_Buffer_Depth(this->m_depth_view);
		p_context->Set_Pipeline(this->m_pipeline);
		p_context->Set_Graphics_Shader_Resource(
			0u, this->m_visibility_shader_view);

		// Every declared light SRV is initialized every frame, including the
		// zero-light fallback (the STATIC-descriptor rule).
		constexpr kotek::uint32_t light_stream_bytes =
			ZIRCON_DEF_LIGHT_MAX_COUNT *
			static_cast<kotek::uint32_t>(sizeof(zircon_light_stream_record_t));
		constexpr kotek::uint32_t tile_count_bytes =
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_TILES *
			static_cast<kotek::uint32_t>(sizeof(kotek::uint32_t));
		constexpr kotek::uint32_t tile_index_bytes = tile_count_bytes *
			ZIRCON_DEF_LIGHT_CLUSTER_MAX_LIGHTS_PER_TILE;
		p_context->Set_Graphics_Structured_Buffer(
			0u, this->m_light_streams[0], 0u, light_stream_bytes, 16u);
		p_context->Set_Graphics_Structured_Buffer(
			1u, this->m_light_streams[1], 0u, light_stream_bytes, 16u);
		p_context->Set_Graphics_Structured_Buffer(
			2u, this->m_light_streams[2], 0u, light_stream_bytes, 16u);
		p_context->Set_Graphics_Structured_Buffer(
			3u, this->m_light_streams[3], 0u, light_stream_bytes, 16u);
		p_context->Set_Graphics_Structured_Buffer(
			4u, this->m_light_tile_counts, 0u, tile_count_bytes, 4u);
		p_context->Set_Graphics_Structured_Buffer(
			5u, this->m_light_tile_indices, 0u, tile_index_bytes, 4u);
		record_indirect_draw();
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
		zircon_render_graph_pass_meshlet_cluster_nri::get_visibility_pipeline(
			void) const noexcept
	{
		return this->m_visibility_pipeline;
	}

	kotek::core::ktkRenderGeometryPipelineHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_light_bin_pipeline(
			void) const noexcept
	{
		return this->m_light_bin_pipeline;
	}

	kotek::core::ktkRenderGeometryPipelineHandle
		zircon_render_graph_pass_meshlet_cluster_nri::get_cull_pipeline(
			void) const noexcept
	{
		return this->m_cull_pipeline;
	}
} // namespace no_streaming
