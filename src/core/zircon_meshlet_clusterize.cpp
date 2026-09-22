#include "zircon_meshlet_clusterize.h"

#include <kotek.core.api/include/kotek_api.h>
#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

#include <cstring>

// ---------------------------------------------------------------------------
// zircon_meshlet_clusterize.cpp — the nanite-style meshlet clusterizer
// (task Z24 phase B3a, the content side). The implementation of the
// contract in zircon_meshlet_clusterize.h: seeded region growth over the
// shared-vertex adjacency graph into <=124-triangle / <=62-vertex
// clusters, the decimation-free LOD hierarchy (greedy adjacency pairing
// — the simplifier's placeholder), the normal cones, and the pack
// emission through the shared kpack encoder. The whole state is ONE
// heap-allocated context per call (the adjacency + growth tables, ~19 MB
// at the caps — the fixture rule); no statics (rule 1a).
// ---------------------------------------------------------------------------

namespace
{
	// the per-triangle clustering state (the state machine of the
	// banner's algorithm: free -> queued -> clustered | rejected; a
	// rejected candidate is held by the open cluster and re-offered to
	// the next one at close — never lost, so every triangle ends
	// clustered exactly once)
	enum meshlet_tri_state_t : kotek::uint8_t
	{
		kTriStateFree = 0,
		kTriStateQueued,
		kTriStateClustered,
		kTriStateRejected
	};

	constexpr kotek::uint32_t k_no_seed = 0xFFFFFFFFu;

	// the clusterizer state, heap-allocated per call (no statics)
	struct meshlet_context_t
	{
		// the CSR vertex->triangles adjacency (built once per call;
		// m_vert_cursor is the RESUMABLE per-vertex scan cursor: a
		// vertex's triangle list is walked once per level in total —
		// the amortized linear bound)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES + 1>
			m_vert_offsets;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES>
			m_vert_cursor;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_INDICES>
			m_vert_tris;

		// the generation-stamped weld tables (stamp != generation means
		// "not in the current cluster"; the slot is the local index)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES>
			m_vert_stamp;
		kotek::static_vector_t<kotek::uint16_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES>
			m_vert_slot;
		kotek::uint32_t m_generation;

		// the per-triangle state
		kotek::static_vector_t<kotek::uint8_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES>
			m_tri_state;
		// the triangle's cluster in the CURRENT level (level-local id)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES>
			m_tri_cluster;
		// the growth frontier ring — LEVEL-GLOBAL (the meshlette
		// discipline: the frontier survives the cluster close, so a
		// new cluster grows from the previous boundary; each triangle
		// holds at most one live entry, so the window never exceeds
		// the triangle count)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES>
			m_queue;
		kotek::uint32_t m_queue_head;
		kotek::uint32_t m_queue_tail;
		// the open cluster's rejected candidates (the vertex-budget
		// misfits — re-offered to the next cluster at close; a
		// triangle enters at most once per bordering cluster, so the
		// total offers stay bounded by the cluster count per triangle)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES>
			m_holding;
		kotek::uint32_t m_seed_cursor;

		// the current cluster's growth state: the welded slot -> mesh
		// vertex map (the level-5 budget bounds the slot count)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_VERTICES
				<< (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1)>
			m_cluster_vert_mesh;
		kotek::uint32_t m_cluster_vert_count;
		// the current cluster's triangle list (committed to the set at
		// close)
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES>
			m_cluster_tri_scratch;
		// the pairing pass's per-cluster flags
		kotek::static_vector_t<kotek::uint8_t,
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE>
			m_paired;

		meshlet_context_t(void) :
			m_generation{0},
			m_queue_head{0},
			m_queue_tail{0},
			m_seed_cursor{0},
			m_cluster_vert_count{0}
		{
		}

		meshlet_context_t(const meshlet_context_t&) = delete;
		meshlet_context_t& operator=(const meshlet_context_t&) = delete;
	};

	// the emission state (the weld recompute for the bins + the
	// manifest/bin scratch) — a separate small heap context so the
	// clusterizer context is released before the pack write
	struct meshlet_emit_context_t
	{
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES>
			m_vert_stamp;
		kotek::static_vector_t<kotek::uint16_t,
			ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES>
			m_vert_slot;
		kotek::uint32_t m_generation;
		kotek::uint32_t m_weld_vert_count;
		kotek::static_vector_t<kotek::uint32_t,
			ZIRCON_DEF_MESHLET_MAX_VERTICES
				<< (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1)>
			m_weld_vert_mesh;
		// the entry table's stable name storage (the encoder keeps the
		// name pointers until the write returns)
		kotek::static_vector_t<kotek::static_cstring_t<
				ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>,
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE + 1>
			m_entry_names;

		// the cluster bin scratch (the worst-case bin at the level cap)
		static constexpr kotek::uint32_t k_cluster_scratch_size =
			zircon_meshlet_cluster_header_size +
			(ZIRCON_DEF_MESHLET_MAX_VERTICES
					<< (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1)) *
				3 * sizeof(zircon_meshlet_position_quant_t) +
			(ZIRCON_DEF_MESHLET_MAX_TRIANGLES
					<< (ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS - 1)) *
				(2 + 3 * sizeof(kotek::uint16_t) +
					sizeof(kotek::uint16_t));
		kotek::static_vector_t<kotek::uint8_t, k_cluster_scratch_size>
			m_bin_scratch;

		// the manifest accumulator
		kotek::static_vector_t<kotek::uint8_t,
			zircon_meshlet_manifest_header_size +
				ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS *
					zircon_meshlet_manifest_level_size +
				ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE *
					zircon_meshlet_manifest_record_size +
				ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE *
					sizeof(kotek::uint32_t)>
			m_manifest;

		meshlet_emit_context_t(void) : m_generation{0},
			m_weld_vert_count{0} {}

		meshlet_emit_context_t(const meshlet_emit_context_t&) = delete;
		meshlet_emit_context_t& operator=(
			const meshlet_emit_context_t&) = delete;
	};

	void ctx_bump_generation(meshlet_context_t& context) noexcept
	{
		++context.m_generation;

		if (context.m_generation == 0u)
		{
			// the 32-bit generation wrapped: clear the stamp table
			// (unreachable at real scales — the wrap-safety discipline)
			for (kotek::uint32_t& stamp : context.m_vert_stamp)
			{
				stamp = 0u;
			}

			context.m_generation = 1u;
		}
	}

	void emit_bump_generation(meshlet_emit_context_t& context) noexcept
	{
		++context.m_generation;

		if (context.m_generation == 0u)
		{
			for (kotek::uint32_t& stamp : context.m_vert_stamp)
			{
				stamp = 0u;
			}

			context.m_generation = 1u;
		}
	}

	bool mesh_validate(const zircon_meshlet_mesh_input_t& mesh
	) noexcept
	{
		if (mesh.p_positions == nullptr || mesh.p_indices == nullptr ||
			mesh.p_normals == nullptr || mesh.p_materials == nullptr)
		{
			KOTEK_MESSAGE_ERROR(
				"[meshlet] the mesh input has a null table");
			return false;
		}

		if (mesh.m_vertex_count == 0 ||
			mesh.m_vertex_count > ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES ||
			mesh.m_triangle_count == 0 ||
			mesh.m_triangle_count >
				ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES)
		{
			KOTEK_MESSAGE_ERROR(
				"[meshlet] the mesh is outside the supported bounds "
				"({} vertices / {} triangles caps)",
				ZIRCON_DEF_MESHLET_MAX_MESH_VERTICES,
				ZIRCON_DEF_MESHLET_MAX_MESH_TRIANGLES);
			return false;
		}

		for (kotek::uint32_t index = 0;
			 index < mesh.m_triangle_count * 3; ++index)
		{
			if (mesh.p_indices[index] >= mesh.m_vertex_count)
			{
				KOTEK_MESSAGE_ERROR(
					"[meshlet] index {} references vertex {} of {} — "
					"the input is corrupt",
					index, mesh.p_indices[index],
					mesh.m_vertex_count);
				return false;
			}
		}

		return true;
	}

	// the CSR vertex->triangles adjacency build (two passes + prefix
	// sums — deterministic, index-ordered)
	bool ctx_build_adjacency(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh) noexcept
	{
		const kotek::uint32_t vertex_count = mesh.m_vertex_count;

		context.m_vert_offsets.resize(vertex_count + 1);
		context.m_vert_cursor.resize(vertex_count);
		context.m_vert_stamp.resize(vertex_count);
		context.m_vert_slot.resize(vertex_count);
		context.m_tri_state.resize(mesh.m_triangle_count);
		context.m_tri_cluster.resize(mesh.m_triangle_count);
		context.m_queue.resize(mesh.m_triangle_count);
		context.m_holding.resize(mesh.m_triangle_count);
		context.m_cluster_tri_scratch.resize(mesh.m_triangle_count);

		for (kotek::uint32_t vertex = 0; vertex <= vertex_count; ++vertex)
		{
			context.m_vert_offsets[vertex] = 0u;
		}

		for (kotek::uint32_t index = 0;
			 index < mesh.m_triangle_count * 3; ++index)
		{
			++context.m_vert_offsets[mesh.p_indices[index] + 1];
		}

		for (kotek::uint32_t vertex = 0; vertex < vertex_count; ++vertex)
		{
			context.m_vert_offsets[vertex + 1] +=
				context.m_vert_offsets[vertex];
		}

		const kotek::uint32_t total_refs =
			context.m_vert_offsets[vertex_count];
		context.m_vert_tris.resize(total_refs);

		for (kotek::uint32_t vertex = 0; vertex < vertex_count; ++vertex)
		{
			context.m_vert_cursor[vertex] =
				context.m_vert_offsets[vertex];
			context.m_vert_stamp[vertex] = 0u;
			context.m_vert_slot[vertex] = 0u;
		}

		for (kotek::uint32_t index = 0;
			 index < mesh.m_triangle_count * 3; ++index)
		{
			const kotek::uint32_t vertex = mesh.p_indices[index];
			context.m_vert_tris[context.m_vert_cursor[vertex]++] =
				index / 3u;
		}

		return true;
	}

	// the per-level reset: every triangle back to free, the level-global
	// frontier emptied, the resumable cursors back to each vertex's list
	// start
	void ctx_reset_level(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh) noexcept
	{
		for (kotek::uint32_t triangle = 0;
			 triangle < mesh.m_triangle_count; ++triangle)
		{
			context.m_tri_state[triangle] =
				static_cast<kotek::uint8_t>(kTriStateFree);
			context.m_tri_cluster[triangle] = 0u;
		}

		for (kotek::uint32_t vertex = 0; vertex < mesh.m_vertex_count;
			 ++vertex)
		{
			context.m_vert_cursor[vertex] =
				context.m_vert_offsets[vertex];
		}

		context.m_queue_head = 0u;
		context.m_queue_tail = 0u;
		context.m_holding.clear();
		context.m_seed_cursor = 0u;
	}

	// the growth frontier push (a triangle enters exactly once from
	// free, so the ring never holds more than the triangle count)
	void ctx_queue_push(meshlet_context_t& context,
		kotek::uint32_t triangle) noexcept
	{
		context
			.m_queue[context.m_queue_tail %
			static_cast<kotek::uint32_t>(context.m_queue.size())] =
			triangle;
		++context.m_queue_tail;
	}

	// the resumable per-vertex scan: pushes every still-free triangle of
	// the vertex's remaining list (each vertex's list is consumed once
	// per level in total — the amortized linear bound)
	void ctx_scan_vertex(meshlet_context_t& context,
		kotek::uint32_t vertex) noexcept
	{
		const kotek::uint32_t end =
			context.m_vert_offsets[vertex + 1];

		while (context.m_vert_cursor[vertex] < end)
		{
			const kotek::uint32_t triangle =
				context.m_vert_tris[context.m_vert_cursor[vertex]++];

			if (context.m_tri_state[triangle] ==
				static_cast<kotek::uint8_t>(kTriStateFree))
			{
				context.m_tri_state[triangle] =
					static_cast<kotek::uint8_t>(kTriStateQueued);
				ctx_queue_push(context, triangle);
			}
		}
	}

	// adds a triangle to the open cluster: records it, stamps its new
	// vertices, then enqueues the vertex-neighborhood frontier
	void ctx_add_triangle(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		kotek::uint32_t triangle) noexcept
	{
		context.m_tri_state[triangle] =
			static_cast<kotek::uint8_t>(kTriStateClustered);
		context.m_cluster_tri_scratch.push_back(triangle);

		for (kotek::uint8_t corner = 0; corner < 3; ++corner)
		{
			const kotek::uint32_t vertex =
				mesh.p_indices[triangle * 3 + corner];

			if (context.m_vert_stamp[vertex] != context.m_generation)
			{
				context.m_vert_stamp[vertex] = context.m_generation;
				context.m_vert_slot[vertex] =
					static_cast<kotek::uint16_t>(
						context.m_cluster_vert_count);
				context.m_cluster_vert_mesh[context
						.m_cluster_vert_count] = vertex;
				++context.m_cluster_vert_count;
			}

			ctx_scan_vertex(context, vertex);
		}
	}

	// the cluster metadata shared by growth (LOD0) and pairing (parents)
	void compute_cluster_metadata(
		const zircon_meshlet_mesh_input_t& mesh,
		const kotek::uint32_t* p_triangles, kotek::uint32_t tri_count,
		const kotek::uint32_t* p_cluster_verts,
		kotek::uint32_t vert_count, zircon_meshlet_cluster_t& out_info,
		kotek::uint8_t level) noexcept
	{
		out_info = zircon_meshlet_cluster_t{};
		out_info.m_triangle_count = tri_count;
		out_info.m_vertex_count = vert_count;
		out_info.m_level = level;

		for (int axis = 0; axis < 3; ++axis)
		{
			out_info.m_aabb_min[axis] =
				mesh.p_positions[p_cluster_verts[0] * 3 + axis];
			out_info.m_aabb_max[axis] = out_info.m_aabb_min[axis];
		}

		for (kotek::uint32_t slot = 1; slot < vert_count; ++slot)
		{
			const kotek::uint32_t vertex = p_cluster_verts[slot];

			for (int axis = 0; axis < 3; ++axis)
			{
				const double value =
					mesh.p_positions[vertex * 3 + axis];

				if (value < out_info.m_aabb_min[axis])
					out_info.m_aabb_min[axis] = value;
				if (value > out_info.m_aabb_max[axis])
					out_info.m_aabb_max[axis] = value;
			}
		}

		// the normal cone (the banner's derivation): the axis is the
		// normalized mean; the cutoff sin(theta) is the largest
		// deviation; a span >= 90 degrees (or a degenerate mean) is the
		// never-cull sentinel (cutoff 2.0f, axis 0)
		double mean[3] = {0.0, 0.0, 0.0};

		for (kotek::uint32_t order = 0; order < tri_count; ++order)
		{
			const kotek::uint32_t triangle = p_triangles[order];

			for (int axis = 0; axis < 3; ++axis)
			{
				mean[axis] += mesh.p_normals[triangle * 3 + axis];
			}
		}

		const double mean_length =
			std::sqrt(mean[0] * mean[0] + mean[1] * mean[1] +
				mean[2] * mean[2]);

		if (!(mean_length > 1e-12))
		{
			out_info.m_cone_axis[0] = 0.0f;
			out_info.m_cone_axis[1] = 0.0f;
			out_info.m_cone_axis[2] = 0.0f;
			out_info.m_cone_cutoff = 2.0f;
		}
		else
		{
			const double axis[3] = {mean[0] / mean_length,
				mean[1] / mean_length, mean[2] / mean_length};

			double min_dot = 2.0;

			for (kotek::uint32_t order = 0; order < tri_count;
				 ++order)
			{
				const kotek::uint32_t triangle = p_triangles[order];
				const double* p_normal =
					mesh.p_normals + triangle * 3;

				const double dot = axis[0] * p_normal[0] +
					axis[1] * p_normal[1] + axis[2] * p_normal[2];

				if (dot < min_dot)
					min_dot = dot;
			}

			if (min_dot <= 0.0)
			{
				out_info.m_cone_axis[0] = 0.0f;
				out_info.m_cone_axis[1] = 0.0f;
				out_info.m_cone_axis[2] = 0.0f;
				out_info.m_cone_cutoff = 2.0f;
			}
			else
			{
				out_info.m_cone_axis[0] =
					static_cast<float>(axis[0]);
				out_info.m_cone_axis[1] =
					static_cast<float>(axis[1]);
				out_info.m_cone_axis[2] =
					static_cast<float>(axis[2]);

				const double sin_squared =
					1.0 - min_dot * min_dot;
				out_info.m_cone_cutoff = static_cast<float>(
					std::sqrt(sin_squared > 0.0 ? sin_squared : 0.0));
			}
		}

		kotek::uint16_t material_min = mesh.p_materials[p_triangles[0]];
		kotek::uint16_t material_max = material_min;

		for (kotek::uint32_t order = 1; order < tri_count; ++order)
		{
			const kotek::uint16_t material =
				mesh.p_materials[p_triangles[order]];

			if (material < material_min)
				material_min = material;
			if (material > material_max)
				material_max = material;
		}

		out_info.m_material_min = material_min;
		out_info.m_material_max = material_max;
	}

	// commits the open cluster (the growth left its triangles in the
	// scratch + its vertices in m_cluster_vert_mesh) to the set.
	// false = a hard capacity failure (the loud error already emitted)
	bool ctx_commit_cluster(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		kotek::uint8_t level, kotek::uint32_t level_local_id,
		zircon_meshlet_lod_set_t& set) noexcept
	{
		if (set.m_clusters.full() ||
			set.m_cluster_triangles.size() +
					context.m_cluster_tri_scratch.size() >
				set.m_cluster_triangles.capacity())
		{
			KOTEK_MESSAGE_ERROR(
				"[meshlet] the cluster tables exceed the scene budget "
				"({} clusters / {} triangle refs) — the bake stops",
				ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE,
				set.m_cluster_triangles.capacity());
			return false;
		}

		zircon_meshlet_cluster_t info{};
		compute_cluster_metadata(mesh,
			context.m_cluster_tri_scratch.data(),
			static_cast<kotek::uint32_t>(
				context.m_cluster_tri_scratch.size()),
			context.m_cluster_vert_mesh.data(),
			context.m_cluster_vert_count, info, level);

		info.m_first_triangle =
			static_cast<kotek::uint32_t>(set.m_cluster_triangles.size());

		for (const kotek::uint32_t triangle :
			context.m_cluster_tri_scratch)
		{
			set.m_cluster_triangles.push_back(triangle);
			context.m_tri_cluster[triangle] = level_local_id;
		}

		set.m_clusters.push_back(info);
		return true;
	}

	// the seeded region growth for ONE cluster (the banner's
	// algorithm): pops the LEVEL-GLOBAL frontier while the budgets
	// allow; a vertex-budget misfit is held and re-offered to the next
	// cluster at close (never lost). When the frontier runs dry the
	// cluster's own vertices are rescanned through the resumable
	// cursors; a dry refill closes the cluster — the holding re-push
	// feeds the next cluster from this boundary. false = a hard
	// capacity failure only
	bool ctx_grow_cluster(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		kotek::uint32_t budget_tris, kotek::uint32_t budget_verts,
		kotek::uint8_t level, kotek::uint32_t level_local_id,
		zircon_meshlet_lod_set_t& set) noexcept
	{
		context.m_cluster_tri_scratch.clear();
		context.m_cluster_vert_count = 0u;
		context.m_holding.clear();
		ctx_bump_generation(context);

		while (true)
		{
			while (context.m_queue_head < context.m_queue_tail &&
				context.m_cluster_tri_scratch.size() < budget_tris)
			{
				const kotek::uint32_t triangle =
					context.m_queue[context.m_queue_head %
						mesh.m_triangle_count];

				++context.m_queue_head;

				if (context.m_tri_state[triangle] !=
					static_cast<kotek::uint8_t>(kTriStateQueued))
				{
					continue; // a stale duplicate entry — skip it
				}

				// the vertex-budget fit check: count the distinct new
				// vertices; a misfit is held for the next cluster
				kotek::uint32_t new_verts = 0u;

				for (kotek::uint8_t corner = 0; corner < 3; ++corner)
				{
					const kotek::uint32_t vertex =
						mesh.p_indices[triangle * 3 + corner];

					if (context.m_vert_stamp[vertex] !=
						context.m_generation)
					{
						++new_verts;
					}
				}

				if (context.m_cluster_vert_count + new_verts >
					budget_verts)
				{
					context.m_tri_state[triangle] =
						static_cast<kotek::uint8_t>(kTriStateRejected);
					context.m_holding.push_back(triangle);
					continue;
				}

				ctx_add_triangle(context, mesh, triangle);
			}

			if (context.m_cluster_tri_scratch.size() == budget_tris)
				break;

			// the frontier ran dry: rescan the cluster's own vertices
			// through the resumable cursors for new candidates
			const kotek::uint32_t queued_before =
				context.m_queue_tail - context.m_queue_head;

			for (kotek::uint32_t slot = 0;
				 slot < context.m_cluster_vert_count; ++slot)
			{
				ctx_scan_vertex(context,
					context.m_cluster_vert_mesh[slot]);
			}

			if (context.m_queue_tail - context.m_queue_head ==
				queued_before)
			{
				break; // no new candidates — the cluster is closed
			}
		}

		// the close: the held candidates re-enter the level-global
		// frontier (behind the current entries — the FIFO order keeps
		// the boundary the next cluster's first contact)
		for (const kotek::uint32_t triangle : context.m_holding)
		{
			context.m_tri_state[triangle] =
				static_cast<kotek::uint8_t>(kTriStateQueued);
			ctx_queue_push(context, triangle);
		}

		if (context.m_cluster_tri_scratch.empty())
		{
			// nothing was offerable at all — not a cluster
			return true;
		}

		return ctx_commit_cluster(context, mesh, level, level_local_id,
			set);
	}

	// one level of the region-growth clustering. false = hard failure
	bool clusterize_level(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh, kotek::uint32_t level,
		kotek::uint32_t& out_level_cluster_count,
		zircon_meshlet_lod_set_t& set) noexcept
	{
		ctx_reset_level(context, mesh);

		const kotek::uint32_t level_first =
			static_cast<kotek::uint32_t>(set.m_clusters.size());
		const kotek::uint32_t budget_tris =
			zircon_meshlet_max_tris_for_level(level);
		const kotek::uint32_t budget_verts =
			zircon_meshlet_max_verts_for_level(level);

		while (true)
		{
			// the seed: the level-global frontier first (the previous
			// cluster's boundary), else the primary index cursor over
			// the untouched region
			if (context.m_queue_head == context.m_queue_tail)
			{
				kotek::uint32_t seed = k_no_seed;

				while (context.m_seed_cursor < mesh.m_triangle_count)
				{
					const kotek::uint32_t triangle =
						context.m_seed_cursor++;

					if (context.m_tri_state[triangle] ==
						static_cast<kotek::uint8_t>(kTriStateFree))
					{
						seed = triangle;
						break;
					}
				}

				if (seed == k_no_seed)
					break; // every triangle is clustered

				context.m_tri_state[seed] =
					static_cast<kotek::uint8_t>(kTriStateQueued);
				ctx_queue_push(context, seed);
			}

			if (ctx_grow_cluster(context, mesh, budget_tris,
					budget_verts,
					static_cast<kotek::uint8_t>(level),
					static_cast<kotek::uint32_t>(
						set.m_clusters.size()) -
						level_first,
					set) == false)
			{
				return false;
			}
		}

		out_level_cluster_count =
			static_cast<kotek::uint32_t>(set.m_clusters.size()) -
			level_first;
		return true;
	}

	// the pairing pass: the first unpaired vertex-adjacent cluster
	// (emission-order scan), falling back to the next unpaired cluster
	// in emission order
	kotek::uint32_t pairing_find_partner(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		const zircon_meshlet_lod_set_t& set, kotek::uint32_t child,
		kotek::uint32_t prev_first, kotek::uint32_t prev_count
		) noexcept
	{
		const zircon_meshlet_cluster_t& info =
			set.m_clusters[prev_first + child];

		for (kotek::uint32_t order = 0; order < info.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle =
				set.m_cluster_triangles[info.m_first_triangle + order];

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t vertex =
					mesh.p_indices[triangle * 3 + corner];
				const kotek::uint32_t begin =
					context.m_vert_offsets[vertex];
				const kotek::uint32_t end =
					context.m_vert_offsets[vertex + 1];

				for (kotek::uint32_t cursor = begin; cursor < end;
					 ++cursor)
				{
					const kotek::uint32_t neighbor =
						context.m_tri_cluster[context
							.m_vert_tris[cursor]];

					if (neighbor != child && neighbor < prev_count &&
						context.m_paired[neighbor] == 0u)
					{
						return neighbor;
					}
				}
			}
		}

		for (kotek::uint32_t candidate = child + 1;
			 candidate < prev_count; ++candidate)
		{
			if (context.m_paired[candidate] == 0u)
			{
				return candidate;
			}
		}

		return k_no_seed;
	}

	// builds ONE parent level by the greedy adjacency pairing (the
	// placeholder simplification — the banner's contract). false = hard
	// failure
	bool build_parent_level(meshlet_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		kotek::uint32_t prev_first, kotek::uint32_t prev_count,
		kotek::uint32_t level, zircon_meshlet_lod_set_t& set
		) noexcept
	{
		context.m_paired.clear();
		context.m_paired.resize(prev_count);

		for (kotek::uint32_t index = 0; index < prev_count; ++index)
		{
			context.m_paired[index] = 0u;
		}

		const kotek::uint32_t level_first =
			static_cast<kotek::uint32_t>(set.m_clusters.size());
		const kotek::uint32_t budget_verts =
			zircon_meshlet_max_verts_for_level(level);

		for (kotek::uint32_t child = 0; child < prev_count; ++child)
		{
			if (context.m_paired[child] != 0u)
				continue;

			const kotek::uint32_t partner =
				pairing_find_partner(context, mesh, set, child,
					prev_first, prev_count);

			const kotek::uint32_t first_child = child;
			kotek::uint32_t second_child = k_no_seed;

			if (partner != k_no_seed)
			{
				second_child = partner;
				context.m_paired[partner] = 1u;
			}

			context.m_paired[child] = 1u;

			// the parent owns the ordered children (min, max) — the
			// weld order is independent of the pairing creation order
			kotek::uint32_t ordered[2] = {first_child, second_child};

			if (second_child != k_no_seed && second_child < first_child)
			{
				ordered[0] = second_child;
				ordered[1] = first_child;
			}

			const kotek::uint32_t child_count =
				second_child == k_no_seed ? 1u : 2u;

			if (set.m_clusters.full() ||
				set.m_cluster_triangles.size() +
						mesh.m_triangle_count >
					set.m_cluster_triangles.capacity() ||
				set.m_child_links.size() + child_count >
					set.m_child_links.capacity())
			{
				KOTEK_MESSAGE_ERROR(
					"[meshlet] the cluster tables exceed the scene "
					"budget while pairing level {} — the bake stops",
					level);
				return false;
			}

			// the merged triangle list (children in min, max order)
			context.m_cluster_tri_scratch.clear();
			context.m_cluster_vert_count = 0u;
			ctx_bump_generation(context);

			for (kotek::uint32_t child_index = 0;
				 child_index < child_count; ++child_index)
			{
				const zircon_meshlet_cluster_t& child_info =
					set.m_clusters[prev_first + ordered[child_index]];

				for (kotek::uint32_t order = 0;
					 order < child_info.m_triangle_count; ++order)
				{
					const kotek::uint32_t triangle =
						set.m_cluster_triangles[child_info
								.m_first_triangle +
							order];

					context.m_cluster_tri_scratch.push_back(triangle);

					for (kotek::uint8_t corner = 0; corner < 3;
						 ++corner)
					{
						const kotek::uint32_t vertex =
							mesh.p_indices[triangle * 3 + corner];

						if (context.m_vert_stamp[vertex] !=
							context.m_generation)
						{
							context.m_vert_stamp[vertex] =
								context.m_generation;
							context.m_vert_slot[vertex] =
								static_cast<kotek::uint16_t>(
									context.m_cluster_vert_count);
							context.m_cluster_vert_mesh[context
									.m_cluster_vert_count] =
								vertex;
							++context.m_cluster_vert_count;
						}
					}
				}
			}

			if (context.m_cluster_vert_count > budget_verts)
			{
				// by construction two children at the halved budget
				// never exceed the doubled budget — an invariant
				// breach is a programmer error, loud
				KOTEK_MESSAGE_ERROR(
					"[meshlet] the level {} pairing produced a "
					"cluster of {} vertices (budget {}) — internal "
					"invariant breached",
					level, context.m_cluster_vert_count,
					budget_verts);
				return false;
			}

			zircon_meshlet_cluster_t info{};
			compute_cluster_metadata(mesh,
				context.m_cluster_tri_scratch.data(),
				static_cast<kotek::uint32_t>(
					context.m_cluster_tri_scratch.size()),
				context.m_cluster_vert_mesh.data(),
				context.m_cluster_vert_count, info,
				static_cast<kotek::uint8_t>(level));

			info.m_first_triangle =
				static_cast<kotek::uint32_t>(
					set.m_cluster_triangles.size());
			info.m_first_child_link =
				static_cast<kotek::uint32_t>(set.m_child_links.size());
			info.m_child_count = child_count;

			for (const kotek::uint32_t triangle :
				context.m_cluster_tri_scratch)
			{
				set.m_cluster_triangles.push_back(triangle);
			}

			for (kotek::uint32_t child_index = 0;
				 child_index < child_count; ++child_index)
			{
				set.m_child_links.push_back(
					prev_first + ordered[child_index]);
			}

			set.m_clusters.push_back(info);
		}

		// ONLY after the whole pass: reassign the triangle->cluster
		// map to this level (mid-pass reassignment would corrupt the
		// remaining children's adjacency scans — the ids overlap
		// numerically across levels)
		for (kotek::uint32_t parent = level_first;
			 parent < set.m_clusters.size(); ++parent)
		{
			const zircon_meshlet_cluster_t& info =
				set.m_clusters[parent];
			const kotek::uint32_t level_local_id =
				parent - level_first;

			for (kotek::uint32_t order = 0;
				 order < info.m_triangle_count; ++order)
			{
				context.m_tri_cluster[set.m_cluster_triangles
						[info.m_first_triangle + order]] =
					level_local_id;
			}
		}

		return true;
	}

	// u32 -> decimal append (hand-rolled: deterministic, locale-free —
	// the CSG-bake discipline)
	void meshlet_append_u32(kotek::static_cstring_t<
			ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>& out_string,
		kotek::uint32_t value) noexcept
	{
		char digits[10];
		kotek::uint32_t count = 0;

		do
		{
			digits[count++] = static_cast<char>('0' + (value % 10u));
			value /= 10u;
		} while (value != 0u);

		while (count > 0)
		{
			char symbol[2] = {digits[--count], '\0'};
			out_string += symbol;
		}
	}

	// "meshlets/<scene>/lod_<level>/cluster_<index>.bin"
	void meshlet_build_entry_name(kotek::static_cstring_t<
			ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>& out_name,
		const char* p_scene_name, kotek::uint32_t level,
		kotek::uint32_t cluster_index) noexcept
	{
		out_name.assign("meshlets/");
		out_name += p_scene_name;
		out_name += "/lod_";
		meshlet_append_u32(out_name, level);
		out_name += "/cluster_";
		meshlet_append_u32(out_name, cluster_index);
		out_name += ".bin";
	}

	bool meshlet_is_scene_name_valid(const char* p_scene_name
	) noexcept
	{
		if (p_scene_name == nullptr || p_scene_name[0] == '\0')
			return false;

		kotek::uint32_t length = 0;

		while (p_scene_name[length] != '\0')
		{
			const char symbol = p_scene_name[length];

			const bool is_valid =
				(symbol >= 'a' && symbol <= 'z') ||
				(symbol >= 'A' && symbol <= 'Z') ||
				(symbol >= '0' && symbol <= '9') || symbol == '_' ||
				symbol == '-';

			if (is_valid == false)
				return false;

			++length;

			if (length > ZIRCON_DEF_MESHLET_MAX_SCENE_NAME_LENGTH)
				return false;
		}

		return true;
	}

	// the weld recompute for one cluster's bin (first-use order — the
	// deterministic emission order; identical to the growth/pairing
	// weld)
	kotek::uint32_t emit_weld_cluster(meshlet_emit_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		const zircon_meshlet_cluster_t& info,
		const kotek::uint32_t* p_triangles) noexcept
	{
		context.m_weld_vert_count = 0u;
		emit_bump_generation(context);

		for (kotek::uint32_t order = 0; order < info.m_triangle_count;
			 ++order)
		{
			const kotek::uint32_t triangle = p_triangles[order];

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t vertex =
					mesh.p_indices[triangle * 3 + corner];

				if (context.m_vert_stamp[vertex] !=
					context.m_generation)
				{
					context.m_vert_stamp[vertex] =
						context.m_generation;
					context.m_vert_slot[vertex] =
						static_cast<kotek::uint16_t>(
							context.m_weld_vert_count);
					context.m_weld_vert_mesh[context
							.m_weld_vert_count] = vertex;
					++context.m_weld_vert_count;
				}
			}
		}

		return context.m_weld_vert_count;
	}

	// emits one cluster bin into the scratch (the banner's layout);
	// returns the bin size (0 = a hard capacity failure)
	kotek::uint32_t emit_cluster_bin(meshlet_emit_context_t& context,
		const zircon_meshlet_mesh_input_t& mesh,
		const zircon_meshlet_cluster_t& info,
		const kotek::uint32_t* p_triangles) noexcept
	{
		const kotek::uint32_t vert_count =
			emit_weld_cluster(context, mesh, info, p_triangles);

		const kotek::uint32_t tri_count = info.m_triangle_count;
		const kotek::uint32_t index_width =
			info.m_level == 0u ? 1u : 2u;

		const kotek::uint32_t bin_size =
			zircon_meshlet_cluster_header_size +
			vert_count * 3 *
				sizeof(zircon_meshlet_position_quant_t) +
			tri_count * 2 + tri_count * 3 * index_width +
			tri_count * sizeof(kotek::uint16_t);

		context.m_bin_scratch.resize(bin_size);
		kotek::uint8_t* p_bin = context.m_bin_scratch.data();

		std::memcpy(p_bin, zircon_meshlet_cluster_magic, 4);
		zircon_csg_bake_store_u32(p_bin + 4, vert_count);
		zircon_csg_bake_store_u32(p_bin + 8, tri_count);
		zircon_csg_bake_store_u32(p_bin + 12, index_width);
		zircon_csg_bake_store_u32(p_bin + 16, 0u);

		kotek::uint32_t cursor = zircon_meshlet_cluster_header_size;

		// the positions, quantized against the cluster's own AABB
		for (kotek::uint32_t slot = 0; slot < vert_count; ++slot)
		{
			const kotek::uint32_t vertex =
				context.m_weld_vert_mesh[slot];

			for (int axis = 0; axis < 3; ++axis)
			{
				const double value =
					mesh.p_positions[vertex * 3 + axis];
				const double extent =
					info.m_aabb_max[axis] - info.m_aabb_min[axis];

				const zircon_meshlet_position_quant_t quantized =
					zircon_meshlet_quantize_position(
						value, info.m_aabb_min[axis], extent);

				if constexpr (sizeof(
								  zircon_meshlet_position_quant_t) ==
					2)
				{
					zircon_csg_bake_store_u16(p_bin + cursor,
						static_cast<kotek::uint16_t>(quantized));
					cursor += 2;
				}
				else
				{
					p_bin[cursor] =
						static_cast<kotek::uint8_t>(quantized);
					cursor += 1;
				}
			}
		}

		// the per-triangle octahedral normals (the shared codec)
		for (kotek::uint32_t order = 0; order < tri_count; ++order)
		{
			const kotek::uint32_t triangle = p_triangles[order];

			zircon_csg_bake_encode_normal_oct_u8(
				mesh.p_normals + triangle * 3, p_bin + cursor);
			cursor += 2;
		}

		// the cluster-local indices (u8 at LOD0 — the 62-vert budget
		// guarantees the fit; u16 above)
		for (kotek::uint32_t order = 0; order < tri_count; ++order)
		{
			const kotek::uint32_t triangle = p_triangles[order];

			for (kotek::uint8_t corner = 0; corner < 3; ++corner)
			{
				const kotek::uint32_t vertex =
					mesh.p_indices[triangle * 3 + corner];
				const kotek::uint32_t slot =
					context.m_vert_slot[vertex];

				if (index_width == 1u)
				{
					p_bin[cursor] = static_cast<kotek::uint8_t>(slot);
					cursor += 1;
				}
				else
				{
					zircon_csg_bake_store_u16(p_bin + cursor,
						static_cast<kotek::uint16_t>(slot));
					cursor += 2;
				}
			}
		}

		// the per-triangle materials
		for (kotek::uint32_t order = 0; order < tri_count; ++order)
		{
			const kotek::uint32_t triangle = p_triangles[order];

			zircon_csg_bake_store_u16(p_bin + cursor,
				mesh.p_materials[triangle]);
			cursor += 2;
		}

		return bin_size;
	}
} // namespace

bool zircon_meshlet_clusterize_lod_hierarchy(
	const zircon_meshlet_mesh_input_t& mesh,
	zircon_meshlet_lod_set_t& out_set) noexcept
{
	out_set.m_clusters.clear();
	out_set.m_level_first_cluster.clear();
	out_set.m_child_links.clear();
	out_set.m_cluster_triangles.clear();

	if (mesh_validate(mesh) == false)
		return false;

	meshlet_context_t* p_context = new meshlet_context_t();

	ctx_build_adjacency(*p_context, mesh);

	bool is_ok = true;

	// ---- LOD0: the seeded region growth ----
	kotek::uint32_t level_count = 0u;

	if (clusterize_level(*p_context, mesh, 0u, level_count, out_set) ==
		false)
	{
		is_ok = false;
	}

	if (is_ok &&
		level_count > ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE)
	{
		// a LOD0 overflow can never be written as a loadable pack
		// (the pack reader's entry budget) — a hard failure
		KOTEK_MESSAGE_ERROR(
			"[meshlet] the mesh produced {} LOD0 clusters (the scene "
			"budget is {} — coarser content or a split mesh is "
			"required)",
			level_count, ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE);
		is_ok = false;
	}

	// the level table opens with LOD0's first index; each following
	// entry is the next level's first, and the sentinel closes the
	// table (the per-level counts are the adjacent differences)
	out_set.m_level_first_cluster.push_back(0u);

	// ---- the placeholder LOD levels: the greedy adjacency pairing
	// (halves the count exactly per level) ----
	kotek::uint32_t level = 1u;

	while (is_ok && level < ZIRCON_DEF_MESHLET_MAX_LOD_LEVELS &&
		level_count > 1u)
	{
		const kotek::uint32_t parent_count = (level_count + 1u) / 2u;

		if (out_set.m_clusters.size() + parent_count >
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE)
		{
			// the graceful half: the hierarchy truncates at the last
			// level that fits (the pack stays loadable, just shallower)
			KOTEK_MESSAGE_WARNING(
				"[meshlet] the scene cluster budget is exhausted at "
				"level {} — the LOD hierarchy truncates ({} levels "
				"emitted)",
				level, level);
			break;
		}

		const kotek::uint32_t prev_first =
			out_set.m_level_first_cluster[
				out_set.m_level_first_cluster.size() - 1u];
		const kotek::uint32_t new_level_first =
			static_cast<kotek::uint32_t>(out_set.m_clusters.size());

		if (build_parent_level(*p_context, mesh, prev_first,
				level_count, level, out_set) == false)
		{
			is_ok = false;
			break;
		}

		out_set.m_level_first_cluster.push_back(new_level_first);
		level_count = parent_count;
		++level;
	}

	// the sentinel entry == the total (the level table's end marker)
	out_set.m_level_first_cluster.push_back(
		static_cast<kotek::uint32_t>(out_set.m_clusters.size()));

	delete p_context;
	return is_ok;
}

bool zircon_meshlet_clusterize_pack(kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& pack_path_relative_to_root,
	const zircon_meshlet_mesh_input_t& mesh, const char* p_scene_name,
	zircon_meshlet_bake_result_t& out_result) noexcept
{
	out_result = zircon_meshlet_bake_result_t{};

	KOTEK_ASSERT(p_filesystem,
		"the clusterizer resolves the pack path through the filesystem");
	KOTEK_ASSERT(p_scene_name, "the clusterizer needs the scene name");

	if (p_filesystem == nullptr || p_scene_name == nullptr)
	{
		return false;
	}

	if (meshlet_is_scene_name_valid(p_scene_name) == false)
	{
		KOTEK_MESSAGE_ERROR(
			"[meshlet] scene name '{}' is invalid (1..{} of "
			"[a-zA-Z0-9_-])",
			p_scene_name, ZIRCON_DEF_MESHLET_MAX_SCENE_NAME_LENGTH);
		return false;
	}

	// the hierarchy (heap: the set's triangle-reference table is ~6 MB
	// at the caps — the fixture rule)
	auto* p_set = new zircon_meshlet_lod_set_t();

	if (zircon_meshlet_clusterize_lod_hierarchy(mesh, *p_set) == false)
	{
		delete p_set;
		return false;
	}

	// the pack path resolved against the filesystem root (cwd-
	// independent); the caller created the parent directory
	kotek::static_path_t pack_path;
	p_filesystem->Make_Path(
		pack_path, kotek::core::eFolderIndex::kFolderIndex_Root);
	pack_path /= pack_path_relative_to_root;

	meshlet_emit_context_t* p_emit = new meshlet_emit_context_t();

	p_emit->m_vert_stamp.resize(mesh.m_vertex_count);
	p_emit->m_vert_slot.resize(mesh.m_vertex_count);

	for (kotek::uint32_t vertex = 0; vertex < mesh.m_vertex_count;
		 ++vertex)
	{
		p_emit->m_vert_stamp[vertex] = 0u;
		p_emit->m_vert_slot[vertex] = 0u;
	}

	// the payload arena (the bins are accumulated entry-resident — the
	// CSG-bake discipline)
	kotek::uint8_t* p_payload =
		new kotek::uint8_t[ZIRCON_DEF_MESHLET_MAX_TOTAL_PAYLOAD];
	kotek::uint32_t payload_used = 0u;

	// the entry table (heap: ~180 KB at the cap — the fixture rule)
	auto* p_entries =
		new kotek::static_vector_t<kotek::core::kpack_writer_entry_t,
			ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE + 1>();

	bool is_ok = true;

	// ---- the cluster bins, emitted level-DESCENDING (the coarsest
	// level first — the documented streaming order) ----
	const kotek::uint32_t lod_level_count =
		static_cast<kotek::uint32_t>(
			p_set->m_level_first_cluster.size() - 1u);

	for (kotek::uint32_t emit_level = lod_level_count; emit_level-- > 0u;)
	{
		const kotek::uint32_t level_first =
			p_set->m_level_first_cluster[emit_level];
		const kotek::uint32_t level_clusters =
			p_set->m_level_first_cluster[emit_level + 1u] -
			level_first;

		for (kotek::uint32_t index = 0; index < level_clusters;
			 ++index)
		{
			const zircon_meshlet_cluster_t& info =
				p_set->m_clusters[level_first + index];
			const kotek::uint32_t* p_triangles =
				p_set->m_cluster_triangles.data() +
				info.m_first_triangle;

			const kotek::uint32_t bin_size = emit_cluster_bin(
				*p_emit, mesh, info, p_triangles);

			if (payload_used + bin_size >
				ZIRCON_DEF_MESHLET_MAX_TOTAL_PAYLOAD)
			{
				KOTEK_MESSAGE_ERROR(
					"[meshlet] the cluster payload exceeds {} bytes — "
					"the bake stops (raise "
					"ZIRCON_DEF_MESHLET_MAX_TOTAL_PAYLOAD)",
					ZIRCON_DEF_MESHLET_MAX_TOTAL_PAYLOAD);
				is_ok = false;
				break;
			}

			std::memcpy(p_payload + payload_used,
				p_emit->m_bin_scratch.data(), bin_size);

			const kotek::uint32_t payload_offset = payload_used;
			payload_used += bin_size;

			// the entry table rows
			p_emit->m_entry_names.push_back(
				kotek::static_cstring_t<
					ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>{});
			meshlet_build_entry_name(p_emit->m_entry_names.back(),
				p_scene_name, emit_level, index);

			const char* p_entry_name =
				p_emit->m_entry_names.back().c_str();

			kotek::core::kpack_writer_entry_t entry{};
			entry.p_name = p_entry_name;
			entry.p_data = p_payload + payload_offset;
			entry.data_size = bin_size;
			entry.compression = kotek::core::eKpackCompression::kZstd;
			p_entries->push_back(entry);
		}

		if (is_ok == false)
			break;
	}

	// ---- the manifest (header + level table + records + links),
	// stored — the reader's first contact ----
	if (is_ok)
	{
		const kotek::uint32_t cluster_count_total =
			static_cast<kotek::uint32_t>(p_set->m_clusters.size());
		const kotek::uint32_t link_count_total =
			static_cast<kotek::uint32_t>(p_set->m_child_links.size());

		const kotek::uint32_t manifest_size =
			zircon_meshlet_manifest_header_size +
			lod_level_count * zircon_meshlet_manifest_level_size +
			cluster_count_total *
				zircon_meshlet_manifest_record_size +
			link_count_total * sizeof(kotek::uint32_t);

		p_emit->m_manifest.resize(manifest_size);
		kotek::uint8_t* p_manifest = p_emit->m_manifest.data();

		std::memcpy(p_manifest, zircon_meshlet_manifest_magic, 8);
		zircon_csg_bake_store_u32(p_manifest + 8, 0u);
		zircon_csg_bake_store_u32(p_manifest + 12, lod_level_count);
		zircon_csg_bake_store_u32(p_manifest + 16, cluster_count_total);
		zircon_csg_bake_store_u32(
			p_manifest + 20, mesh.m_triangle_count);
		zircon_csg_bake_store_u32(p_manifest + 24, link_count_total);
		zircon_csg_bake_store_u32(p_manifest + 28, 0u);

		kotek::uint32_t cursor =
			zircon_meshlet_manifest_header_size;

		// the level table, level-ASCENDING
		for (kotek::uint32_t index = 0; index < lod_level_count;
			 ++index)
		{
			zircon_csg_bake_store_u32(p_manifest + cursor,
				p_set->m_level_first_cluster[index]);
			zircon_csg_bake_store_u32(p_manifest + cursor + 4,
				p_set->m_level_first_cluster[index + 1u] -
					p_set->m_level_first_cluster[index]);
			cursor += zircon_meshlet_manifest_level_size;
		}

		// the cluster records, level-major ascending
		for (kotek::uint32_t index = 0; index < cluster_count_total;
			 ++index)
		{
			const zircon_meshlet_cluster_t& info =
				p_set->m_clusters[index];
			kotek::uint8_t* p_record = p_manifest + cursor;

			zircon_csg_bake_store_u32(
				p_record + 0, info.m_triangle_count);
			zircon_csg_bake_store_u32(
				p_record + 4, info.m_vertex_count);
			zircon_csg_bake_store_u32(
				p_record + 8, info.m_first_child_link);
			zircon_csg_bake_store_u32(
				p_record + 12, info.m_child_count);

			kotek::uint32_t error_bits = 0u;
			std::memcpy(&error_bits, &info.m_error_metric,
				sizeof(error_bits));
			zircon_csg_bake_store_u32(p_record + 16, error_bits);

			kotek::uint32_t cutoff_bits = 0u;
			std::memcpy(&cutoff_bits, &info.m_cone_cutoff,
				sizeof(cutoff_bits));
			zircon_csg_bake_store_u32(p_record + 20, cutoff_bits);

			for (int axis = 0; axis < 3; ++axis)
			{
				kotek::uint32_t axis_bits = 0u;
				std::memcpy(&axis_bits, &info.m_cone_axis[axis],
					sizeof(axis_bits));
				zircon_csg_bake_store_u32(
					p_record + 24 + axis * 4, axis_bits);

				zircon_csg_bake_store_f64(p_record + 36 + axis * 8,
					info.m_aabb_min[axis]);
				zircon_csg_bake_store_f64(p_record + 60 + axis * 8,
					info.m_aabb_max[axis]);
			}

			zircon_csg_bake_store_u16(
				p_record + 84, info.m_material_min);
			zircon_csg_bake_store_u16(
				p_record + 86, info.m_material_max);

			cursor += zircon_meshlet_manifest_record_size;
		}

		for (kotek::uint32_t index = 0; index < link_count_total;
			 ++index)
		{
			zircon_csg_bake_store_u32(p_manifest + cursor,
				p_set->m_child_links[index]);
			cursor += sizeof(kotek::uint32_t);
		}

		// the entry order = the load/streaming order: the manifest
		// first, then the bins (they were appended level-descending
		// already)
		kotek::static_cstring_t<ZIRCON_DEF_MESHLET_ENTRY_NAME_MAX_LENGTH>
			manifest_name;
		manifest_name.assign("meshlets/");
		manifest_name += p_scene_name;
		manifest_name += "/manifest.bin";

		auto* p_final_entries =
			new kotek::static_vector_t<kotek::core::kpack_writer_entry_t,
				ZIRCON_DEF_MESHLET_MAX_CLUSTERS_PER_SCENE + 1>();

		kotek::core::kpack_writer_entry_t manifest_entry{};
		manifest_entry.p_name = manifest_name.c_str();
		manifest_entry.p_data = p_manifest;
		manifest_entry.data_size = manifest_size;
		manifest_entry.compression =
			kotek::core::eKpackCompression::kStored;
		p_final_entries->push_back(manifest_entry);

		for (const auto& entry : *p_entries)
		{
			p_final_entries->push_back(entry);
		}

		is_ok = kotek::core::kpack_write_file(pack_path.c_str(),
			p_final_entries->data(), p_final_entries->size());

		delete p_final_entries;

		if (is_ok == false)
		{
			KOTEK_MESSAGE_ERROR(
				"[meshlet] the pack write failed for '{}'",
				pack_path.c_str());
		}
	}

	out_result.m_emitted_cluster_count =
		static_cast<kotek::uint32_t>(p_set->m_clusters.size());
	out_result.m_emitted_lod_level_count = lod_level_count;
	out_result.m_emitted_child_link_count =
		static_cast<kotek::uint32_t>(p_set->m_child_links.size());
	out_result.m_mesh_triangle_count = mesh.m_triangle_count;

	delete[] p_payload;
	delete p_entries;
	delete p_emit;
	delete p_set;
	return is_ok;
}
