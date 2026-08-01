#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include "../../core/zircon_gltf_loader.h"
		#include "../../core/zircon_config.h"
		#include "../../ecs/zircon_factory.h"
		#include "../../ecs/zircon_component_geometry.h"
		#include "../../ecs/zircon_component_transform.h"
		#include "../../world/zircon_world.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.h"

		#include <cstdio>
		#include <cstring>

		#ifndef ZIRCON_DEF_UNIT_TEST_GLTF_LOADER
			#define ZIRCON_DEF_UNIT_TEST_GLTF_LOADER 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_GLTF_LOADER == 1

// functional proofs for task Z3 P2c (glTF-lite loader + the model_static
// mesh path): the fixtures are generated at test time (nothing binary is
// committed — file-based cases land in data_user/tests/ like the other
// suites) and cover the loader's promises: glb/gltf containers, indexed
// and sequential primitives, uint16/uint32 indices, interleaved and
// planar bufferViews, hierarchy flattening into world transforms, AABB —
// and that every malformed/truncated/unsupported input degrades to an
// error status instead of a crash

namespace
{
	// assembles binary payloads; every segment is 4-byte aligned (glb
	// chunk/accessor alignment)
	struct gltf_test_bin_t
	{
		kotek::array_t<kotek::uint8_t, 8192> data;
		kotek::uint32_t size;

		gltf_test_bin_t(void) : data{}, size{0} {}

		kotek::uint32_t append(
			const void* p_data, kotek::uint32_t length) noexcept
		{
			while (this->size & 3)
				this->data[this->size++] = 0;

			const kotek::uint32_t offset = this->size;

			std::memcpy(
				this->data.data() + this->size, p_data, length);

			this->size += length;

			return offset;
		}
	};

	void gltf_test_write_u32(
		kotek::uint8_t* p_target, kotek::uint32_t value) noexcept
	{
		std::memcpy(p_target, &value, sizeof(value));
	}

	// binary fixture write: the kotek filesystem's byte overload is an
	// unimplemented stub and its char* overload writes in text mode
	// (0x0A bytes in the payload would be CRLF-translated), so the
	// fixture generator goes through the CRT directly
	bool gltf_test_write_binary_file(const char* p_path,
		const kotek::uint8_t* p_data, kotek::size_t size) noexcept
	{
		FILE* p_file = std::fopen(p_path, "wb");

		if (p_file == nullptr)
			return false;

		const kotek::size_t written = std::fwrite(p_data, 1, size, p_file);

		std::fclose(p_file);

		return written == size;
	}

	// assembles a .glb container (header + JSON chunk + optional BIN
	// chunk) into caller storage; returns the container size
	kotek::uint32_t gltf_test_build_glb(const char* p_json,
		kotek::uint32_t json_length, const kotek::uint8_t* p_bin,
		kotek::uint32_t bin_length, kotek::uint8_t* p_out,
		kotek::uint32_t out_capacity) noexcept
	{
		const kotek::uint32_t json_padded = (json_length + 3) & ~3u;
		const kotek::uint32_t bin_padded = (bin_length + 3) & ~3u;

		kotek::uint32_t total = 12 + 8 + json_padded;

		if (p_bin)
			total += 8 + bin_padded;

		if (total > out_capacity)
			return 0;

		gltf_test_write_u32(p_out + 0, 0x46546c67); // "glTF"
		gltf_test_write_u32(p_out + 4, 2);
		gltf_test_write_u32(p_out + 8, total);

		kotek::uint32_t cursor = 12;

		gltf_test_write_u32(p_out + cursor, json_length);
		gltf_test_write_u32(p_out + cursor + 4, 0x4e4f534a); // "JSON"
		cursor += 8;

		std::memcpy(p_out + cursor, p_json, json_length);
		cursor += json_length;

		while (cursor & 3)
			p_out[cursor++] = ' ';

		if (p_bin)
		{
			gltf_test_write_u32(p_out + cursor, bin_length);
			gltf_test_write_u32(
				p_out + cursor + 4, 0x004e4942); // "BIN\0"
			cursor += 8;

			std::memcpy(p_out + cursor, p_bin, bin_length);
			cursor += bin_length;

			while (cursor & 3)
				p_out[cursor++] = 0;
		}

		return total;
	}

	// the cube fixture: 24 vertices (6 faces x 4; per-face normal,
	// per-quad uv) + 36 uint16 indices, positions/normals/uvs packed
	// into ONE interleaved bufferView (stride 32) so the interleaved
	// decode path is the one under test; returns the bin blob's size
	kotek::uint32_t gltf_test_build_cube_bin(
		gltf_test_bin_t& out_bin, kotek::uint32_t& out_indices_offset,
		kotek::uint32_t& out_indices_length) noexcept
	{
		// per-vertex: position[3] normal[3] uv[2]
		const float _kVertices[24][8] = {
			// +X
			{1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
			{1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
			{1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
			// -X
			{-1.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
			{-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
			{-1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
			{-1.0f, -1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
			// +Y
			{-1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
			{-1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
			{1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
			// -Y
			{-1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f},
			{-1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f},
			{1.0f, -1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f},
			// +Z
			{-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
			{1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
			{1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
			{-1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
			// -Z
			{1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f},
			{-1.0f, -1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f},
			{-1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f},
		};

		kotek::uint16_t indices[36];

		for (kotek::uint8_t face_index = 0; face_index < 6; ++face_index)
		{
			const kotek::uint16_t base =
				static_cast<kotek::uint16_t>(face_index * 4);
			const kotek::uint8_t index_offset =
				static_cast<kotek::uint8_t>(face_index * 6);

			indices[index_offset + 0] = base;
			indices[index_offset + 1] = base + 1;
			indices[index_offset + 2] = base + 2;
			indices[index_offset + 3] = base;
			indices[index_offset + 4] = base + 2;
			indices[index_offset + 5] = base + 3;
		}

		const kotek::uint32_t vertices_offset =
			out_bin.append(_kVertices, sizeof(_kVertices));

		// the cube builder always emits vertices first at offset 0
		KOTEK_ASSERT(vertices_offset == 0, "vertices must come first");
		(void)vertices_offset;

		out_indices_offset = out_bin.append(indices, sizeof(indices));
		out_indices_length = sizeof(indices);

		return out_bin.size;
	}

	// the cube's json text; position_count_override doubles as the
	// malformed-fixture knob (a count past the bufferView's range must
	// be caught by the accessor bounds checks)
	void gltf_test_build_cube_json(
		kotek::static_cstring_t<4096>& out_json,
		kotek::uint32_t vertex_count, kotek::uint32_t indices_offset,
		kotek::uint32_t indices_length,
		kotek::uint32_t total_length) noexcept
	{
		kotek::array_t<char, 2048> text{};

		kotek::ktk::sprintf(text.data(), text.size(),
			"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
			"\"scenes\":[{\"nodes\":[0]}],"
			"\"nodes\":[{\"mesh\":0}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":"
			"0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3}]}],"
			"\"accessors\":["
			"{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,"
			"\"count\":%u,\"type\":\"VEC3\"},"
			"{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,"
			"\"count\":%u,\"type\":\"VEC3\"},"
			"{\"bufferView\":0,\"byteOffset\":20,\"componentType\":5126,"
			"\"count\":%u,\"type\":\"VEC2\"},"
			"{\"bufferView\":1,\"componentType\":5123,\"count\":36,"
			"\"type\":\"SCALAR\"}],"
			"\"bufferViews\":["
			"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":768,"
			"\"byteStride\":32},"
			"{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
			"\"buffers\":[{\"byteLength\":%u}]}",
			vertex_count, vertex_count, vertex_count, indices_offset,
			indices_length, total_length);

		out_json.assign(text.data());
	}

	// the cube fixture as one .glb container
	kotek::uint32_t gltf_test_make_cube_glb(kotek::uint8_t* p_out,
		kotek::uint32_t out_capacity,
		kotek::uint32_t position_count_override = 24,
		bool include_bin = true) noexcept
	{
		gltf_test_bin_t bin;

		kotek::uint32_t indices_offset = 0;
		kotek::uint32_t indices_length = 0;

		const kotek::uint32_t bin_size = gltf_test_build_cube_bin(
			bin, indices_offset, indices_length);

		kotek::static_cstring_t<4096> json;

		gltf_test_build_cube_json(json, position_count_override,
			indices_offset, indices_length, bin_size);

		return gltf_test_build_glb(json.c_str(),
			static_cast<kotek::uint32_t>(json.size()),
			include_bin ? bin.data.data() : nullptr, bin_size, p_out,
			out_capacity);
	}

	// two translated nodes sharing one triangle mesh (parent at
	// +5,0,0 with the mesh, its child at +0,3,0 with the same mesh) —
	// uint32 indices, planar bufferViews, no normals/uvs
	kotek::uint32_t gltf_test_make_two_node_glb(
		kotek::uint8_t* p_out, kotek::uint32_t out_capacity) noexcept
	{
		const float _kPositions[3][3] = {
			{0.0f, 0.0f, 0.0f},
			{1.0f, 0.0f, 0.0f},
			{0.0f, 1.0f, 0.0f},
		};

		const kotek::uint32_t _kIndices[3] = {0, 1, 2};

		gltf_test_bin_t bin;

		const kotek::uint32_t positions_offset =
			bin.append(_kPositions, sizeof(_kPositions));
		const kotek::uint32_t indices_offset =
			bin.append(_kIndices, sizeof(_kIndices));

		kotek::array_t<char, 2048> text{};

		kotek::ktk::sprintf(text.data(), text.size(),
			"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
			"\"scenes\":[{\"nodes\":[0]}],"
			"\"nodes\":["
			"{\"translation\":[5.0,0.0,0.0],\"children\":[1],"
			"\"mesh\":0},"
			"{\"translation\":[0.0,3.0,0.0],\"mesh\":0}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":"
			"0},\"indices\":1}]}],"
			"\"accessors\":["
			"{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
			"\"type\":\"VEC3\"},"
			"{\"bufferView\":1,\"componentType\":5125,\"count\":3,"
			"\"type\":\"SCALAR\"}],"
			"\"bufferViews\":["
			"{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"
			"{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":12}],"
			"\"buffers\":[{\"byteLength\":%u}]}",
			positions_offset, indices_offset, bin.size);

		kotek::static_cstring_t<2048> json;

		json.assign(text.data());

		return gltf_test_build_glb(json.c_str(),
			static_cast<kotek::uint32_t>(json.size()), bin.data.data(),
			bin.size, p_out, out_capacity);
	}

	// one triangle without an indices accessor (the sequential path)
	kotek::uint32_t gltf_test_make_sequential_glb(
		kotek::uint8_t* p_out, kotek::uint32_t out_capacity) noexcept
	{
		const float _kPositions[3][3] = {
			{0.0f, 0.0f, 0.0f},
			{2.0f, 0.0f, 0.0f},
			{0.0f, 2.0f, 0.0f},
		};

		gltf_test_bin_t bin;

		const kotek::uint32_t positions_offset =
			bin.append(_kPositions, sizeof(_kPositions));

		kotek::array_t<char, 2048> text{};

		kotek::ktk::sprintf(text.data(), text.size(),
			"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
			"\"scenes\":[{\"nodes\":[0]}],"
			"\"nodes\":[{\"mesh\":0}],"
			"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":"
			"0}}]}],"
			"\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
			"\"count\":3,\"type\":\"VEC3\"}],"
			"\"bufferViews\":[{\"buffer\":0,\"byteOffset\":%u,"
			"\"byteLength\":36}],"
			"\"buffers\":[{\"byteLength\":%u}]}",
			positions_offset, bin.size);

		kotek::static_cstring_t<2048> json;

		json.assign(text.data());

		return gltf_test_build_glb(json.c_str(),
			static_cast<kotek::uint32_t>(json.size()), bin.data.data(),
			bin.size, p_out, out_capacity);
	}

	// heap-allocated mesh storage — the mesh is ~700 KB of static
	// containers (the loader's caps), far past the test thread's stack
	// budget (same pattern as the other suites' console fixtures)
	struct gltf_test_mesh_holder_t
	{
		zircon_gltf_mesh_t mesh;
	};
} // namespace

TEST(Zircon_Game, GltfLoaderCubeGlbRoundtrip)
{
	kotek::array_t<kotek::uint8_t, 8192> glb{};

	const kotek::uint32_t glb_size =
		gltf_test_make_cube_glb(glb.data(),
			static_cast<kotek::uint32_t>(glb.size()));

	ASSERT_NE(glb_size, 0u);

	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	zircon_gltf_error_t error;

	eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
		glb.data(), glb_size, holder.mesh, error);

	ASSERT_EQ(status, eZirconGltfLoadStatus::kSuccess)
		<< "loader error: " << error.c_str();

	EXPECT_EQ(holder.mesh.m_vertices.size(), 24u);
	EXPECT_EQ(holder.mesh.m_indices.size(), 36u);
	ASSERT_EQ(holder.mesh.m_submeshes.size(), 1u);

	EXPECT_TRUE(holder.mesh.m_has_normals);
	EXPECT_TRUE(holder.mesh.m_has_texcoords);

	// AABB [-1,1]^3
	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_FLOAT_EQ(holder.mesh.m_aabb_min[axis], -1.0f);
		EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[axis], 1.0f);
	}

	// vertex 0 of the +X face: position (1,-1,-1), normal +X, uv (0,0)
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_position[0], 1.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_position[1], -1.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_position[2], -1.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_normal[0], 1.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_normal[1], 0.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_normal[2], 0.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_texcoord[0], 0.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[0].m_texcoord[1], 0.0f);

	// the -Z face's normal must decode through the interleaved stride
	EXPECT_FLOAT_EQ(holder.mesh.m_vertices[20].m_normal[2], -1.0f);

	// uint16 indices decode into canonical 32-bit values
	for (kotek::uint32_t index_index = 0; index_index < 36;
		 ++index_index)
	{
		EXPECT_LT(holder.mesh.m_indices[index_index], 24u);
	}

	EXPECT_EQ(holder.mesh.m_indices[0], 0u);
	EXPECT_EQ(holder.mesh.m_indices[1], 1u);
	EXPECT_EQ(holder.mesh.m_indices[2], 2u);

	// the node carried no transform: the submesh world is identity
	const zircon_gltf_submesh_t& submesh = holder.mesh.m_submeshes[0];

	EXPECT_EQ(submesh.m_index_offset, 0u);
	EXPECT_EQ(submesh.m_index_count, 36u);
	EXPECT_FLOAT_EQ(submesh.m_base_color_factor[0], 1.0f);
	EXPECT_FLOAT_EQ(submesh.m_base_color_factor[3], 1.0f);
	EXPECT_FALSE(submesh.m_has_base_color_texture);

	for (int element = 0; element < 16; ++element)
	{
		const float expected =
			(element == 0 || element == 5 || element == 10 ||
				element == 15)
			? 1.0f
			: 0.0f;

		EXPECT_FLOAT_EQ(submesh.m_world_matrix[element], expected)
			<< "element " << element;
	}

	delete &holder;
}

TEST(Zircon_Game, GltfLoaderTwoNodeHierarchyFlattened)
{
	kotek::array_t<kotek::uint8_t, 8192> glb{};

	const kotek::uint32_t glb_size = gltf_test_make_two_node_glb(
		glb.data(), static_cast<kotek::uint32_t>(glb.size()));

	ASSERT_NE(glb_size, 0u);

	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	zircon_gltf_error_t error;

	eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
		glb.data(), glb_size, holder.mesh, error);

	ASSERT_EQ(status, eZirconGltfLoadStatus::kSuccess)
		<< "loader error: " << error.c_str();

	// the mesh is shared by both nodes: the triangle's 3 vertices are
	// appended once per referencing node
	EXPECT_EQ(holder.mesh.m_vertices.size(), 6u);
	EXPECT_EQ(holder.mesh.m_indices.size(), 6u);
	ASSERT_EQ(holder.mesh.m_submeshes.size(), 2u);

	// uint32 index source, no NORMAL/TEXCOORD_0 in the file
	EXPECT_FALSE(holder.mesh.m_has_normals);
	EXPECT_FALSE(holder.mesh.m_has_texcoords);

	// parent node: translation (5,0,0)
	const float* p_parent_world =
		holder.mesh.m_submeshes[0].m_world_matrix;

	EXPECT_FLOAT_EQ(p_parent_world[12], 5.0f);
	EXPECT_FLOAT_EQ(p_parent_world[13], 0.0f);
	EXPECT_FLOAT_EQ(p_parent_world[14], 0.0f);
	EXPECT_FLOAT_EQ(p_parent_world[0], 1.0f);
	EXPECT_FLOAT_EQ(p_parent_world[5], 1.0f);
	EXPECT_FLOAT_EQ(p_parent_world[10], 1.0f);
	EXPECT_FLOAT_EQ(p_parent_world[15], 1.0f);

	// child node: its (0,3,0) flattened under the parent's (5,0,0)
	const float* p_child_world =
		holder.mesh.m_submeshes[1].m_world_matrix;

	EXPECT_FLOAT_EQ(p_child_world[12], 5.0f);
	EXPECT_FLOAT_EQ(p_child_world[13], 3.0f);
	EXPECT_FLOAT_EQ(p_child_world[14], 0.0f);
	EXPECT_FLOAT_EQ(p_child_world[0], 1.0f);
	EXPECT_FLOAT_EQ(p_child_world[5], 1.0f);
	EXPECT_FLOAT_EQ(p_child_world[10], 1.0f);
	EXPECT_FLOAT_EQ(p_child_world[15], 1.0f);

	// the AABB spans both transformed triangles: x 5..6, y 0..4, z 0
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_min[0], 5.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_min[1], 0.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_min[2], 0.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[0], 6.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[1], 4.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[2], 0.0f);

	// the second node's indices address its own appended vertices
	EXPECT_EQ(holder.mesh.m_submeshes[1].m_index_offset, 3u);
	EXPECT_EQ(holder.mesh.m_indices[3], 3u);
	EXPECT_EQ(holder.mesh.m_indices[4], 4u);
	EXPECT_EQ(holder.mesh.m_indices[5], 5u);

	delete &holder;
}

TEST(Zircon_Game, GltfLoaderSequentialPrimitive)
{
	kotek::array_t<kotek::uint8_t, 8192> glb{};

	const kotek::uint32_t glb_size = gltf_test_make_sequential_glb(
		glb.data(), static_cast<kotek::uint32_t>(glb.size()));

	ASSERT_NE(glb_size, 0u);

	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	zircon_gltf_error_t error;

	eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
		glb.data(), glb_size, holder.mesh, error);

	ASSERT_EQ(status, eZirconGltfLoadStatus::kSuccess)
		<< "loader error: " << error.c_str();

	EXPECT_EQ(holder.mesh.m_vertices.size(), 3u);
	ASSERT_EQ(holder.mesh.m_indices.size(), 3u);
	ASSERT_EQ(holder.mesh.m_submeshes.size(), 1u);

	// no indices accessor — the generated sequence is 0,1,2
	EXPECT_EQ(holder.mesh.m_indices[0], 0u);
	EXPECT_EQ(holder.mesh.m_indices[1], 1u);
	EXPECT_EQ(holder.mesh.m_indices[2], 2u);

	EXPECT_EQ(holder.mesh.m_submeshes[0].m_index_count, 3u);

	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[0], 2.0f);
	EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[1], 2.0f);

	delete &holder;
}

TEST(Zircon_Game, GltfLoaderGltfExternalBuffer)
{
	kotek::core::ktkFrameworkConfig config;
	kotek::core::ktkFileSystem filesystem;

	filesystem.Initialize(&config);

	kotek::static_path_t gltf_path;
	filesystem.Make_Path(gltf_path,
		kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests);

	gltf_path /= "p2c_cube.gltf";

	kotek::static_path_t bin_path = gltf_path.parent_path();
	bin_path /= "p2c_cube.bin";

	// the same cube, split into a .gltf + its external buffer file
	gltf_test_bin_t bin;

	kotek::uint32_t indices_offset = 0;
	kotek::uint32_t indices_length = 0;

	const kotek::uint32_t bin_size = gltf_test_build_cube_bin(
		bin, indices_offset, indices_length);

	kotek::array_t<char, 2048> text{};

	kotek::ktk::sprintf(text.data(), text.size(),
		"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
		"\"scenes\":[{\"nodes\":[0]}],"
		"\"nodes\":[{\"mesh\":0}],"
		"\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,"
		"\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3}]}],"
		"\"accessors\":["
		"{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,"
		"\"count\":24,\"type\":\"VEC3\"},"
		"{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,"
		"\"count\":24,\"type\":\"VEC3\"},"
		"{\"bufferView\":0,\"byteOffset\":20,\"componentType\":5126,"
		"\"count\":24,\"type\":\"VEC2\"},"
		"{\"bufferView\":1,\"componentType\":5123,\"count\":36,"
		"\"type\":\"SCALAR\"}],"
		"\"bufferViews\":["
		"{\"buffer\":0,\"byteOffset\":0,\"byteLength\":768,"
		"\"byteStride\":32},"
		"{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
		"\"buffers\":[{\"byteLength\":%u,\"uri\":\"p2c_cube.bin\"}]}",
		indices_offset, indices_length, bin_size);

	bool write_status = filesystem.Write_File(
		gltf_path, text.data(), std::strlen(text.data()));

	ASSERT_TRUE(write_status);

	write_status = gltf_test_write_binary_file(
		bin_path.c_str(), bin.data.data(), bin.size);

	ASSERT_TRUE(write_status);

	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	kotek::array_t<kotek::uint8_t, 16384> file_buffer{};
	zircon_gltf_error_t error;

	eZirconGltfLoadStatus status = zircon_gltf_load_from_file(
		&filesystem, gltf_path, file_buffer.data(),
		file_buffer.size(), holder.mesh, error);

	ASSERT_EQ(status, eZirconGltfLoadStatus::kSuccess)
		<< "loader error: " << error.c_str();

	EXPECT_EQ(holder.mesh.m_vertices.size(), 24u);
	EXPECT_EQ(holder.mesh.m_indices.size(), 36u);
	ASSERT_EQ(holder.mesh.m_submeshes.size(), 1u);

	EXPECT_TRUE(holder.mesh.m_has_normals);
	EXPECT_TRUE(holder.mesh.m_has_texcoords);

	for (int axis = 0; axis < 3; ++axis)
	{
		EXPECT_FLOAT_EQ(holder.mesh.m_aabb_min[axis], -1.0f);
		EXPECT_FLOAT_EQ(holder.mesh.m_aabb_max[axis], 1.0f);
	}

	delete &holder;

	filesystem.Shutdown();
}

TEST(Zircon_Game, GltfLoaderMalformedGlb)
{
	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	zircon_gltf_error_t error;

	// bad magic
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const kotek::uint32_t glb_size = gltf_test_make_cube_glb(
			glb.data(), static_cast<kotek::uint32_t>(glb.size()));

		ASSERT_NE(glb_size, 0u);

		glb[0] = 'X';

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size, holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_BadContainer);
		EXPECT_STRNE(error.c_str(), "");
	}

	// wrong container version
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const kotek::uint32_t glb_size = gltf_test_make_cube_glb(
			glb.data(), static_cast<kotek::uint32_t>(glb.size()));

		ASSERT_NE(glb_size, 0u);

		gltf_test_write_u32(glb.data() + 4, 3);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size, holder.mesh, error);

		EXPECT_EQ(
			status, eZirconGltfLoadStatus::kError_UnsupportedVersion);
	}

	// truncated container (the declared length outlives the data)
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const kotek::uint32_t glb_size = gltf_test_make_cube_glb(
			glb.data(), static_cast<kotek::uint32_t>(glb.size()));

		ASSERT_NE(glb_size, 0u);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size - 10, holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_BadContainer);
	}

	// json that isn't
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const char* _kBadJson = "{not json at all";

		const kotek::uint32_t glb_size = gltf_test_build_glb(_kBadJson,
			static_cast<kotek::uint32_t>(std::strlen(_kBadJson)),
			nullptr, 0, glb.data(),
			static_cast<kotek::uint32_t>(glb.size()));

		ASSERT_NE(glb_size, 0u);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size, holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_JsonMalformed);
	}

	// the BIN chunk is missing while the json references it
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const kotek::uint32_t glb_size = gltf_test_make_cube_glb(
			glb.data(), static_cast<kotek::uint32_t>(glb.size()), 24,
			false);

		ASSERT_NE(glb_size, 0u);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size, holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_BufferMissing);
	}

	// an accessor count that runs past its bufferView
	{
		kotek::array_t<kotek::uint8_t, 8192> glb{};

		const kotek::uint32_t glb_size = gltf_test_make_cube_glb(
			glb.data(), static_cast<kotek::uint32_t>(glb.size()), 48);

		ASSERT_NE(glb_size, 0u);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			glb.data(), glb_size, holder.mesh, error);

		EXPECT_EQ(
			status, eZirconGltfLoadStatus::kError_AccessorOutOfRange);
	}

	// empty inputs are argument errors, not crashes
	{
		eZirconGltfLoadStatus status = zircon_gltf_load_from_memory(
			nullptr, 0, holder.mesh, error);

		EXPECT_EQ(
			status, eZirconGltfLoadStatus::kError_InvalidArguments);
	}

	delete &holder;
}

TEST(Zircon_Game, GltfLoaderDataUriAndMissingBuffer)
{
	kotek::core::ktkFrameworkConfig config;
	kotek::core::ktkFileSystem filesystem;

	filesystem.Initialize(&config);

	kotek::static_path_t tests_path;
	filesystem.Make_Path(tests_path,
		kotek::core::eFolderIndex::kFolderIndex_DataUser_Tests);

	gltf_test_mesh_holder_t& holder = *new gltf_test_mesh_holder_t();
	kotek::array_t<kotek::uint8_t, 16384> file_buffer{};
	zircon_gltf_error_t error;

	// a base64 data-URI buffer: out of the lite scope, logged and
	// reported (never a crash)
	{
		kotek::static_path_t gltf_path = tests_path;
		gltf_path /= "p2c_data_uri.gltf";

		const char* _kJson =
			"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{"
			"\"byteLength\":4,\"uri\":\"data:application/"
			"octet-stream;base64,AAAAAA==\"}]}";

		bool write_status = filesystem.Write_File(gltf_path, _kJson,
			std::strlen(_kJson));

		ASSERT_TRUE(write_status);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_file(
			&filesystem, gltf_path, file_buffer.data(),
			file_buffer.size(), holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_Unsupported);
	}

	// an external buffer that isn't on disk
	{
		kotek::static_path_t gltf_path = tests_path;
		gltf_path /= "p2c_missing_bin.gltf";

		const char* _kJson =
			"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{"
			"\"byteLength\":4,\"uri\":\"p2c_no_such_file.bin\"}]}";

		bool write_status = filesystem.Write_File(gltf_path, _kJson,
			std::strlen(_kJson));

		ASSERT_TRUE(write_status);

		eZirconGltfLoadStatus status = zircon_gltf_load_from_file(
			&filesystem, gltf_path, file_buffer.data(),
			file_buffer.size(), holder.mesh, error);

		EXPECT_EQ(status, eZirconGltfLoadStatus::kError_FileRead);
	}

	delete &holder;

	filesystem.Shutdown();
}

TEST(Zircon_Game, GltfGeometryComponentMeshNameSerialization)
{
	// the journaled field (Z6 replay reads these json states) must
	// roundtrip through the component's tag_invoke pair
	#ifdef KOTEK_USE_NOT_CUSTOM_LIBRARY
	zircon_component_geometry source;

	source.set_geometry_type(kotek::core::eStaticGeometryType::kBox);
	source.set_mesh_name("p2c_cube.glb");
	source.set_visible(false);

	kotek::json::value state = kotek::json::value_from(source);

	zircon_component_geometry restored =
		kotek::json::value_to<zircon_component_geometry>(state);

	EXPECT_STREQ(restored.get_mesh_name(), "p2c_cube.glb");
	EXPECT_EQ(restored.get_geometry_type(),
		kotek::core::eStaticGeometryType::kBox);
	EXPECT_FALSE(restored.is_visible());

	// the default keeps the primitive path
	zircon_component_geometry empty_source;

	kotek::json::value empty_state =
		kotek::json::value_from(empty_source);

	zircon_component_geometry empty_restored =
		kotek::json::value_to<zircon_component_geometry>(empty_state);

	EXPECT_EQ(empty_restored.get_mesh_name()[0], '\0');
	#endif
}

namespace
{
	// headless world environment for the collect test (heap-allocated:
	// the console alone is about a megabyte — see the note in
	// zircon_unit_tests_game.cpp)
	struct zircon_test_gltf_pass_env
	{
		kotek::core::ktkConsole console;
		kotek::core::ktkInput input;
		zircon_config engine_config;
		zircon_factory factory;
		zircon_world world;
	};
} // namespace

TEST(Zircon_Game, RenderPassModelStaticCollectDrawItemsMeshName)
{
	zircon_test_gltf_pass_env& env = *new zircon_test_gltf_pass_env();

	env.factory.Initialize(&env.engine_config, &env.console, &env.input);

	env.world.initialize("zircon_p2c_test_world", &env.engine_config,
		&env.console, &env.input, &env.factory,
		ZIRCON_DEF_WORLD_DEFAULT_ENTITY_COUNT);

	zircon_ecs_context_t* p_context = env.world.get_ecs_context();

	// one box-primitive entity (the fallback cube path) and one
	// mesh-named entity (the glTF path — collection copies the name,
	// no file is touched here)
	kotek::entity_t entity_box = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_box,
		eZirconComponentType::kzircon_component_transform);
	env.factory.create_component(p_context, entity_box,
		eZirconComponentType::kzircon_component_geometry);

	zircon_component_geometry* p_box_geometry =
		static_cast<zircon_component_geometry*>(
			env.factory.get_component_by_enum(p_context, entity_box,
				eZirconComponentType::kzircon_component_geometry));

	ASSERT_NE(p_box_geometry, nullptr);

	p_box_geometry->set_geometry_type(
		kotek::core::eStaticGeometryType::kBox);

	kotek::entity_t entity_mesh = env.factory.create_entity(p_context);

	env.factory.create_component(p_context, entity_mesh,
		eZirconComponentType::kzircon_component_transform);
	env.factory.create_component(p_context, entity_mesh,
		eZirconComponentType::kzircon_component_geometry);

	zircon_component_geometry* p_mesh_geometry =
		static_cast<zircon_component_geometry*>(
			env.factory.get_component_by_enum(p_context, entity_mesh,
				eZirconComponentType::kzircon_component_geometry));

	ASSERT_NE(p_mesh_geometry, nullptr);

	// a mesh name overrides the (unknown) primitive type
	p_mesh_geometry->set_mesh_name("p2c_cube.glb");

	zircon_render_pass_model_static_draw_item_t
		draw_items[zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT];

	kotek::uint32_t draw_count = no_streaming::
		zircon_render_graph_pass_model_static_bgfx::collect_draw_items(
			&env.factory, p_context,
			env.world.get_entity_count_max_limit(), draw_items,
			zircon_DEF_RENDER_PASS_MODEL_STATIC_MAX_DRAW_COUNT);

	ASSERT_EQ(draw_count, 2u);

	bool found_box = false;
	bool found_mesh = false;

	for (kotek::uint32_t item_index = 0; item_index < draw_count;
		 ++item_index)
	{
		const zircon_render_pass_model_static_draw_item_t& item =
			draw_items[item_index];

		if (item.m_mesh_name.empty())
		{
			found_box = true;
		}
		else
		{
			EXPECT_STREQ(item.m_mesh_name.c_str(), "p2c_cube.glb");
			found_mesh = true;
		}
	}

	EXPECT_TRUE(found_box);
	EXPECT_TRUE(found_mesh);

	env.world.shutdown(&env.factory);
	env.factory.Shutdown();

	delete &env;
}

		#endif
	#endif
#endif
