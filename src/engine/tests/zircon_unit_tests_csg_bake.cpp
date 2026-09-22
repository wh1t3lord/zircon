#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>
		#include <filesystem>

		#include "../../core/zircon_csg_bake.h"
		#include "../../ecs/zircon_csg_evaluate.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_chunk_pool.h"

		#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_CSG_BAKE
			#define ZIRCON_DEF_UNIT_TEST_CSG_BAKE 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_CSG_BAKE == 1

// functional proofs for task Z25 A3 (the CSG game bake): the world-grid
// chunking (the centroid floor rule, the exact-boundary case, no
// duplicates at seams), the quantization error bounds (the documented
// half-LSB position bound, the degenerate-axis case, the u8
// octahedral normal bound), the pack roundtrip (bake -> pack ->
// dispatcher -> load_chunks_from_pack -> pool state == the pre-bake
// mesh within the bound), the DETERMINISM contract (two bakes
// byte-identical), the streaming-order emission (the pack's entry
// table in the documented order) and the capacity guards (loud
// graceful). The evaluation fixtures are dyadic-exact so every
// precision mode quantizes them losslessly. Tier: lightweight (rule
// 8a — tiny brushes, small packs; no heavy-flag suites here).

namespace
{
	// the headless environment (the pack_boot fixture's shape): a real
	// filesystem behind a real framework config — the boot dispatcher,
	// no engine session needed
	struct zircon_csg_bake_test_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	// a box primitive descriptor in the configured precision (the A1
	// tests' csg_make_desc, narrowed to the configured scalar)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> bake_make_box_desc(
		double dimension_x, double dimension_y, double dimension_z,
		double position_x, double position_y, double position_z,
		kotek::uint16_t material_id = 0)
	{
		using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		zircon_csg_primitive_desc_t<zircon_csg_scalar_t> desc;

		desc.m_dimensions[0] = traits_t::from_double(dimension_x);
		desc.m_dimensions[1] = traits_t::from_double(dimension_y);
		desc.m_dimensions[2] = traits_t::from_double(dimension_z);
		desc.m_position[0] = traits_t::from_double(position_x);
		desc.m_position[1] = traits_t::from_double(position_y);
		desc.m_position[2] = traits_t::from_double(position_z);
		desc.m_rotation[0] = traits_t::from_double(0.0);
		desc.m_rotation[1] = traits_t::from_double(0.0);
		desc.m_rotation[2] = traits_t::from_double(0.0);
		desc.m_rotation[3] = traits_t::from_double(1.0);
		desc.m_material_id = material_id;
		desc.m_type =
			static_cast<kotek::uint8_t>(eZirconCsgPrimitiveType::kBox);
		desc.m_operation = 0;

		return desc;
	}

	// evaluates a compound through the A1 core (the reference the bake
	// output is compared against). The context is ~6 MB — heap only
	// (the fixture rule)
	eZirconCsgEvaluationStatus bake_evaluate_reference(
		const zircon_csg_primitive_desc_t<zircon_csg_scalar_t>* p_descs,
		kotek::uint32_t count,
		zircon_csg_evaluation_t<zircon_csg_scalar_t>* p_evaluation)
	{
		return zircon_csg_evaluate_compound(
			p_descs, count, *p_evaluation);
	}

	// builds <root>/data_game/packs/<name> (the folder is created by
	// bake_ensure_packs_folder — the caller's pre-clean may have
	// removed a stale one)
	kotek::static_path_t bake_make_pack_path(
		zircon_csg_bake_test_env& env, const char* p_pack_name)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		kotek::static_path_t pack_path = packs_folder;
		pack_path /= p_pack_name;
		return pack_path;
	}

	void bake_ensure_packs_folder(zircon_csg_bake_test_env& env)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		std::error_code ec;
		std::filesystem::create_directories(
			std::filesystem::path(packs_folder.c_str()), ec);
	}

	void bake_remove_pack(const kotek::static_path_t& pack_path)
	{
		std::error_code ec;
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()), ec);

		// the packs folder leaves with the last test pack (the
		// shipped tree has no data_game/packs — the pack_boot /
		// embedded-defaults fixtures pin that invariant)
		ec.clear();
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()).parent_path(), ec);
	}

	// reads a whole file through the dispatcher into a heap buffer
	// (out_size receives the byte count; the caller deletes[])
	kotek::uint8_t* bake_read_through_dispatcher(
		zircon_csg_bake_test_env& env,
		const kotek::static_path_t& absolute_path, kotek::size_t& out_size
		)
	{
		out_size = 0;

		kotek::size_t file_size = 0;

		if (env.filesystem.Get_FileSize(absolute_path, file_size) == false)
			return nullptr;

		kotek::uint8_t* p_buffer = new kotek::uint8_t[file_size + 1];
		kotek::uint8_t* p_cursor = p_buffer;
		kotek::size_t read_size = file_size + 1;

		if (env.filesystem.Read_File(
				absolute_path, p_cursor, read_size) == false ||
			read_size != file_size)
		{
			delete[] p_buffer;
			return nullptr;
		}

		out_size = read_size;
		return p_buffer;
	}

	// one parsed manifest record (the test-side view)
	struct bake_manifest_record_t
	{
		kotek::uint32_t m_compound_index;
		kotek::uint32_t m_chunk_index;
		kotek::int32_t m_cell[3];
		kotek::uint32_t m_welded_vertex_count;
		kotek::uint32_t m_index_count;
		kotek::uint32_t m_triangle_count;
		kotek::uint16_t m_material_id_first;
		double m_aabb_min[3];
		double m_aabb_max[3];
	};

	// parses the manifest bytes (the shared format helpers — the same
	// cursor discipline the loader uses)
	bool bake_parse_manifest(const kotek::uint8_t* p_manifest,
		kotek::size_t manifest_size,
		kotek::static_vector_t<bake_manifest_record_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE>& out_records,
		kotek::uint32_t& out_baked_compound_count)
	{
		out_records.clear();

		if (manifest_size < zircon_csg_bake_manifest_header_size ||
			std::memcmp(p_manifest, zircon_csg_bake_manifest_magic, 8) !=
				0)
		{
			return false;
		}

		out_baked_compound_count =
			zircon_csg_bake_load_u32(p_manifest + 12);

		const kotek::uint32_t record_count =
			zircon_csg_bake_load_u32(p_manifest + 16);

		if (manifest_size !=
			zircon_csg_bake_manifest_header_size +
				record_count * zircon_csg_bake_manifest_record_size)
		{
			return false;
		}

		for (kotek::uint32_t index = 0; index < record_count; ++index)
		{
			const kotek::uint8_t* p_record =
				p_manifest + zircon_csg_bake_manifest_header_size +
				index * zircon_csg_bake_manifest_record_size;

			bake_manifest_record_t record{};
			record.m_compound_index =
				zircon_csg_bake_load_u32(p_record + 0);
			record.m_chunk_index = zircon_csg_bake_load_u32(p_record + 4);
			record.m_cell[0] = zircon_csg_bake_load_i32(p_record + 8);
			record.m_cell[1] = zircon_csg_bake_load_i32(p_record + 12);
			record.m_cell[2] = zircon_csg_bake_load_i32(p_record + 16);
			record.m_welded_vertex_count =
				zircon_csg_bake_load_u32(p_record + 20);
			record.m_index_count = zircon_csg_bake_load_u32(p_record + 24);
			record.m_triangle_count =
				zircon_csg_bake_load_u32(p_record + 28);
			record.m_material_id_first =
				zircon_csg_bake_load_u16(p_record + 32);

			for (int axis = 0; axis < 3; ++axis)
			{
				record.m_aabb_min[axis] =
					zircon_csg_bake_load_f64(p_record + 36 + axis * 8);
				record.m_aabb_max[axis] =
					zircon_csg_bake_load_f64(p_record + 60 + axis * 8);
			}

			out_records.push_back(record);
		}

		return true;
	}

	// the absolute dispatcher path of a baked entry ("csg/<scene>/..."
	// under the filesystem root)
	kotek::static_path_t bake_entry_path(
		zircon_csg_bake_test_env& env, const char* p_relative_name)
	{
		kotek::static_path_t path;
		env.filesystem.Make_Path(
			path, kotek::core::eFolderIndex::kFolderIndex_Root);
		path /= p_relative_name;
		return path;
	}

	// the angular error between two ~unit vectors (the cross/dot
	// atan2 form — stable at small angles, unlike acos)
	double bake_angular_error(const double* p_a, const float* p_b)
	{
		const double cross_x =
			static_cast<double>(p_a[1]) * p_b[2] -
			static_cast<double>(p_a[2]) * p_b[1];
		const double cross_y =
			static_cast<double>(p_a[2]) * p_b[0] -
			static_cast<double>(p_a[0]) * p_b[2];
		const double cross_z =
			static_cast<double>(p_a[0]) * p_b[1] -
			static_cast<double>(p_a[1]) * p_b[0];
		const double dot = static_cast<double>(p_a[0]) * p_b[0] +
			static_cast<double>(p_a[1]) * p_b[1] +
			static_cast<double>(p_a[2]) * p_b[2];

		return std::atan2(std::sqrt(cross_x * cross_x + cross_y * cross_y +
								cross_z * cross_z),
			dot);
	}
} // namespace

// the chunking correctness: a compound spanning 3 grid cells splits
// into exactly the cells its triangles touch (12 triangles per box,
// conserved across the chunks), in the grid order
TEST(Zircon_Game, CsgBakeChunkingGridSplit)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		bake_make_pack_path(env, "csg_bake_grid.kpack");
	bake_remove_pack(pack_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// three disjoint boxes, one per cell (cells (0,0,0), (1,0,0),
	// (2,0,0) on x; the y/z centers at 8 keep every box inside its
	// cell — a box straddling the y=0 plane would span cells)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> descs[3] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 8.0, 8.0, 0),
		bake_make_box_desc(2.0, 2.0, 2.0, 24.0, 8.0, 8.0, 3),
		bake_make_box_desc(2.0, 2.0, 2.0, 40.0, 8.0, 8.0, 7)};

	// the reference: the union evaluates to 36 triangles
	auto* p_reference =
		new zircon_csg_evaluation_t<zircon_csg_scalar_t>();
	ASSERT_EQ(bake_evaluate_reference(descs, 3, p_reference),
		eZirconCsgEvaluationStatus::kSuccess);
	ASSERT_EQ(p_reference->m_mesh.m_indices.size() / 3, 36u);

	const zircon_csg_bake_compound_input_t compounds[] = {{descs, 3}};

	zircon_csg_bake_result_t result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_grid.kpack", compounds, 1, "grid",
		result));

	EXPECT_EQ(result.m_baked_compound_count, 1u);
	EXPECT_EQ(result.m_skipped_compound_count, 0u);
	EXPECT_EQ(result.m_emitted_chunk_count, 3u);
	EXPECT_EQ(result.m_emitted_index_count, 108u);
	EXPECT_EQ(result.m_material_count, 3u);

	// the pack mounted at Initialize of a FRESH filesystem, the
	// manifest read through the dispatcher
	zircon_csg_bake_test_env& reader = *new zircon_csg_bake_test_env();
	reader.initialize();

	kotek::size_t manifest_size = 0;
	kotek::uint8_t* p_manifest = bake_read_through_dispatcher(
		reader, bake_entry_path(reader, "csg/grid/manifest.bin"),
		manifest_size);
	ASSERT_TRUE(p_manifest != nullptr);

	// heap: the record table is ~300 KB at the cap (the fixture rule)
	auto* p_records =
		new kotek::static_vector_t<bake_manifest_record_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE>();
	auto& records = *p_records;
	kotek::uint32_t baked_compounds = 0;
	ASSERT_TRUE(bake_parse_manifest(
		p_manifest, manifest_size, records, baked_compounds));

	ASSERT_EQ(records.size(), 3u);
	EXPECT_EQ(baked_compounds, 1u);

	// the grid order: cells (0,0,0), (1,0,0), (2,0,0); one box (12
	// triangles) per chunk; the triangle sum is conserved (no loss,
	// no duplication at seams)
	const kotek::int32_t expected_cells[3] = {0, 1, 2};

	kotek::uint32_t triangle_sum = 0;

	for (kotek::uint32_t index = 0; index < 3; ++index)
	{
		const bake_manifest_record_t& record = records[index];

		EXPECT_EQ(record.m_compound_index, 0u);
		EXPECT_EQ(record.m_chunk_index, index);
		EXPECT_EQ(record.m_cell[0], expected_cells[index]);
		EXPECT_EQ(record.m_cell[1], 0);
		EXPECT_EQ(record.m_cell[2], 0);
		EXPECT_EQ(record.m_triangle_count, 12u);
		EXPECT_EQ(record.m_index_count, 36u);
		EXPECT_EQ(record.m_welded_vertex_count, 8u);

		// the manifest AABB is the PRE-quantization exact box bound
		const double expected_min_x = 7.0 + 16.0 * index;
		const double expected_max_x = 9.0 + 16.0 * index;

		EXPECT_NEAR(record.m_aabb_min[0], expected_min_x, 1e-9);
		EXPECT_NEAR(record.m_aabb_max[0], expected_max_x, 1e-9);

		for (int axis = 1; axis < 3; ++axis)
		{
			EXPECT_NEAR(record.m_aabb_min[axis], 7.0, 1e-9);
			EXPECT_NEAR(record.m_aabb_max[axis], 9.0, 1e-9);
		}

		triangle_sum += record.m_triangle_count;
	}

	EXPECT_EQ(triangle_sum, 36u);

	// the materials table: the sorted distinct ids
	{
		kotek::size_t materials_size = 0;
		kotek::uint8_t* p_materials = bake_read_through_dispatcher(
			reader, bake_entry_path(reader, "csg/grid/materials.json"),
			materials_size);
		ASSERT_TRUE(p_materials != nullptr);

		const char expected[] = "{\"materials\":[0,3,7]}";
		ASSERT_EQ(materials_size, sizeof(expected) - 1);
		EXPECT_EQ(std::memcmp(
			p_materials, expected, sizeof(expected) - 1), 0);

		delete[] p_materials;
	}

	delete[] p_manifest;
	delete p_records;

	reader.shutdown();
	delete &reader;

	delete p_reference;
	env.shutdown();
	delete &env;

	// the packs folder cleaned (the next test's fresh env mounts
	// nothing stale)
	bake_remove_pack(pack_path);
}

// the boundary rule: a triangle whose centroid sits EXACTLY on a cell
// boundary lands in the HIGHER cell (the documented floor rule); the
// triangle count is conserved (no duplicates at seams)
TEST(Zircon_Game, CsgBakeChunkingBoundaryRule)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		bake_make_pack_path(env, "csg_bake_boundary.kpack");
	bake_remove_pack(pack_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// the box spans x in [8, 16]: its +X face's 2 triangles have their
	// centroid exactly at x = 16 (cell 1), the other 10 land in cell 0
	// (the y/z centers at 8 keep the box inside the y/z cells)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> descs[1] = {
		bake_make_box_desc(8.0, 2.0, 2.0, 12.0, 8.0, 8.0)};

	const zircon_csg_bake_compound_input_t compounds[] = {{descs, 1}};

	zircon_csg_bake_result_t result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_boundary.kpack", compounds, 1,
		"boundary", result));

	EXPECT_EQ(result.m_emitted_chunk_count, 2u);

	zircon_csg_bake_test_env& reader = *new zircon_csg_bake_test_env();
	reader.initialize();

	kotek::size_t manifest_size = 0;
	kotek::uint8_t* p_manifest = bake_read_through_dispatcher(
		reader, bake_entry_path(reader, "csg/boundary/manifest.bin"),
		manifest_size);
	ASSERT_TRUE(p_manifest != nullptr);

	// heap: the record table is ~300 KB at the cap (the fixture rule)
	auto* p_records =
		new kotek::static_vector_t<bake_manifest_record_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE>();
	auto& records = *p_records;
	kotek::uint32_t baked_compounds = 0;
	ASSERT_TRUE(bake_parse_manifest(
		p_manifest, manifest_size, records, baked_compounds));
	delete[] p_manifest;

	ASSERT_EQ(records.size(), 2u);

	// the grid order puts cell 0 first
	EXPECT_EQ(records[0].m_cell[0], 0);
	EXPECT_EQ(records[0].m_triangle_count, 10u);
	EXPECT_EQ(records[1].m_cell[0], 1);
	EXPECT_EQ(records[1].m_triangle_count, 2u);

	// conservation: 10 + 2 == the box's 12 triangles (every triangle
	// in EXACTLY one chunk)
	EXPECT_EQ(records[0].m_triangle_count + records[1].m_triangle_count,
		12u);

	delete p_records;

	reader.shutdown();
	delete &reader;
	env.shutdown();
	delete &env;

	bake_remove_pack(pack_path);
}

// the quantization error bounds: the documented half-LSB position
// bound on a known mesh, the degenerate-axis behavior, the u8
// octahedral normal bound over a direction sweep (incl. the fold
// seams)
TEST(Zircon_Game, CsgBakeQuantizationErrorBounds)
{
	// the documented bound itself: half an LSB of the quantum grid
	EXPECT_NEAR(zircon_csg_bake_position_error_bound(16.0),
		16.0 / (2.0 * 65535.0), 1e-15);

	// a known axis-aligned box chunk: quantize + dequantize every
	// corner against the chunk bounds, the error stays under the bound
	{
		const double aabb_min[3] = {8.0, -1.0, -1.0};
		const double aabb_max[3] = {16.0, 1.0, 1.0};

		for (int corner = 0; corner < 8; ++corner)
		{
			const double point[3] = {
				(corner & 1) ? aabb_max[0] : aabb_min[0],
				(corner & 2) ? aabb_max[1] : aabb_min[1],
				(corner & 4) ? aabb_max[2] : aabb_min[2]};

			for (int axis = 0; axis < 3; ++axis)
			{
				const double extent = aabb_max[axis] - aabb_min[axis];
				const zircon_csg_bake_position_quant_t quantized =
					zircon_csg_bake_quantize_position(
						point[axis], aabb_min[axis], extent);
				const double dequantized =
					zircon_csg_bake_dequantize_position(
						quantized, aabb_min[axis], extent);

				// half an LSB + the f64->f32 slack at the pool
				// boundary (the test compares in double: the slack
				// stays negligible against the u16 LSB)
				EXPECT_LE(std::fabs(dequantized - point[axis]),
					zircon_csg_bake_position_error_bound(extent) +
						extent * 1e-9 + 1e-12);
			}
		}
	}

	// the degenerate single-triangle chunk: a flat axis (extent 0)
	// stores 0 and dequantizes to the bound exactly
	{
		const zircon_csg_bake_position_quant_t quantized =
			zircon_csg_bake_quantize_position(3.0, 3.0, 0.0);
		EXPECT_EQ(static_cast<kotek::uint32_t>(quantized), 0u);
		EXPECT_DOUBLE_EQ(
			zircon_csg_bake_dequantize_position(quantized, 3.0, 0.0),
			3.0);
	}

	// the u8 octahedral normal roundtrip: the axes, the fold-seam
	// diagonals and a Fibonacci sweep — the angular error stays under
	// the documented conservative bound (0.015 rad ~= 0.86 degrees)
	{
		const double kDocumentedBound = 0.015;

		const double axes[6][3] = {{1.0, 0.0, 0.0}, {-1.0, 0.0, 0.0},
			{0.0, 1.0, 0.0}, {0.0, -1.0, 0.0}, {0.0, 0.0, 1.0},
			{0.0, 0.0, -1.0}};

		for (int index = 0; index < 6; ++index)
		{
			kotek::uint8_t oct[2];
			float decoded[3];
			zircon_csg_bake_encode_normal_oct_u8(
				axes[index], oct);
			zircon_csg_bake_decode_normal_oct_u8(oct, decoded);

			EXPECT_LE(bake_angular_error(axes[index], decoded),
				kDocumentedBound);
		}

		double max_error = 0.0;

		for (int index = 0; index < 512; ++index)
		{
			// the Fibonacci sphere (deterministic, seam-covering)
			const double z = 1.0 - 2.0 * (index + 0.5) / 512.0;
			const double radius = std::sqrt(1.0 - z * z);
			const double phi = index * 2.399963229728653;
			const double normal[3] = {radius * std::cos(phi),
				radius * std::sin(phi), z};

			kotek::uint8_t oct[2];
			float decoded[3];
			zircon_csg_bake_encode_normal_oct_u8(normal, oct);
			zircon_csg_bake_decode_normal_oct_u8(oct, decoded);

			const double error = bake_angular_error(normal, decoded);

			if (error > max_error)
				max_error = error;
		}

		EXPECT_LT(max_error, kDocumentedBound);
	}
}

// the pack roundtrip: bake -> pack -> the dispatcher ->
// load_chunks_from_pack -> the pool state == the pre-bake mesh within
// the quantization bound
TEST(Zircon_Game, CsgBakePackRoundtrip)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		bake_make_pack_path(env, "csg_bake_roundtrip.kpack");
	bake_remove_pack(pack_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// one box, one chunk, a non-zero material (centered at (8, 8, 8)
	// so the box sits inside one grid cell)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> descs[1] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 8.0, 8.0, 5)};

	auto* p_reference =
		new zircon_csg_evaluation_t<zircon_csg_scalar_t>();
	ASSERT_EQ(bake_evaluate_reference(descs, 1, p_reference),
		eZirconCsgEvaluationStatus::kSuccess);

	const auto& mesh = p_reference->m_mesh;
	ASSERT_EQ(mesh.m_indices.size() / 3, 12u);

	const zircon_csg_bake_compound_input_t compounds[] = {{descs, 1}};

	zircon_csg_bake_result_t result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_roundtrip.kpack", compounds, 1,
		"roundtrip", result));
	EXPECT_EQ(result.m_emitted_chunk_count, 1u);

	// a FRESH filesystem mounts the pack at Initialize (the boot
	// dispatcher), the load goes through the priority chain
	zircon_csg_bake_test_env& reader = *new zircon_csg_bake_test_env();
	reader.initialize();

	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	kotek::uint32_t loaded_count = 0;
	ASSERT_TRUE(pool.load_chunks_from_pack(
		&reader.filesystem, "csg/roundtrip", loaded_count));
	EXPECT_EQ(loaded_count, 1u);
	EXPECT_EQ(pool.get_live_chunk_count(), 1u);
	EXPECT_EQ(pool.get_free_chunk_slot_count(),
		zircon_DEF_RENDER_CHUNK_POOL_MAX_CHUNKS - 1);

	// the manifest record (the pre-quantization truth)
	kotek::size_t manifest_size = 0;
	kotek::uint8_t* p_manifest = bake_read_through_dispatcher(
		reader, bake_entry_path(reader, "csg/roundtrip/manifest.bin"),
		manifest_size);
	ASSERT_TRUE(p_manifest != nullptr);

	// heap: the record table is ~300 KB at the cap (the fixture rule)
	auto* p_records =
		new kotek::static_vector_t<bake_manifest_record_t,
			ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_SCENE>();
	auto& records = *p_records;
	kotek::uint32_t baked_compounds = 0;
	ASSERT_TRUE(bake_parse_manifest(
		p_manifest, manifest_size, records, baked_compounds));
	delete[] p_manifest;

	ASSERT_EQ(records.size(), 1u);
	const bake_manifest_record_t& record = records[0];

	// the pool's chunk: the soup expansion (3 vertices per triangle)
	ASSERT_TRUE(pool.is_chunk_live(0));
	const zircon_chunk_ranges_t& ranges = pool.get_ranges()[0];
	EXPECT_EQ(ranges.m_vertex_count, 36u);
	EXPECT_EQ(ranges.m_index_count, 36u);
	EXPECT_EQ(pool.get_material_ids()[0], record.m_material_id_first);
	EXPECT_EQ(record.m_material_id_first, 5);

	// the pool AABB == the manifest AABB within the quantization bound
	// (the pool computes it from the dequantized f32 positions)
	const zircon_chunk_bounds_t& bounds = pool.get_bounds()[0];

	for (int axis = 0; axis < 3; ++axis)
	{
		const double extent =
			record.m_aabb_max[axis] - record.m_aabb_min[axis];
		const double bound =
			zircon_csg_bake_position_error_bound(extent) + 1e-5;

		EXPECT_NEAR(bounds.m_aabb_min[axis], record.m_aabb_min[axis],
			bound);
		EXPECT_NEAR(bounds.m_aabb_max[axis], record.m_aabb_max[axis],
			bound);
	}

	// every reference welded vertex has a pool soup vertex within the
	// bound; every soup normal is unit length and axis-aligned
	const zircon_model_static_vertex_t* p_shadow =
		pool.get_vertex_shadow();

	const double bound = zircon_csg_bake_position_error_bound(2.0) +
		2.0 * 1e-9 + 1e-6;

	for (kotek::uint32_t vertex = 0; vertex < mesh.m_positions.size();
		 ++vertex)
	{
		using traits_t = zircon_csg_scalar_traits<zircon_csg_scalar_t>;

		const double reference[3] = {
			traits_t::to_double(mesh.m_positions[vertex].m[0]),
			traits_t::to_double(mesh.m_positions[vertex].m[1]),
			traits_t::to_double(mesh.m_positions[vertex].m[2])};

		bool is_found = false;

		for (kotek::uint32_t soup = 0; soup < ranges.m_vertex_count;
			 ++soup)
		{
			const float* p_position =
				p_shadow[ranges.m_vertex_offset + soup].m_position;

			if (std::fabs(p_position[0] - reference[0]) <= bound &&
				std::fabs(p_position[1] - reference[1]) <= bound &&
				std::fabs(p_position[2] - reference[2]) <= bound)
			{
				is_found = true;
				break;
			}
		}

		EXPECT_TRUE(is_found) << "the reference vertex " << vertex
							  << " has no pool vertex within the bound";
	}

	for (kotek::uint32_t soup = 0; soup < ranges.m_vertex_count; ++soup)
	{
		const float* p_normal =
			p_shadow[ranges.m_vertex_offset + soup].m_normal;
		const float length = std::sqrt(p_normal[0] * p_normal[0] +
			p_normal[1] * p_normal[1] + p_normal[2] * p_normal[2]);

		EXPECT_NEAR(length, 1.0f, 1e-5f);

		// the box's flat normals are the 6 axes: the largest component
		// dominates
		const float max_component = std::fabs(p_normal[0]) >
				std::fabs(p_normal[1])
			? (std::fabs(p_normal[0]) > std::fabs(p_normal[2])
					? std::fabs(p_normal[0])
					: std::fabs(p_normal[2]))
			: (std::fabs(p_normal[1]) > std::fabs(p_normal[2])
					? std::fabs(p_normal[1])
					: std::fabs(p_normal[2]));

		EXPECT_GT(max_component, 0.99f);
	}

	// the soup indices address the chunk's own span, in order
	const kotek::uint16_t* p_index_shadow = pool.get_index_shadow();

	for (kotek::uint32_t index = 0; index < ranges.m_index_count; ++index)
	{
		EXPECT_EQ(p_index_shadow[ranges.m_index_offset + index],
			static_cast<kotek::uint16_t>(index));
	}

	// the pool is dirty for the pass's upload (the registration
	// contract)
	EXPECT_TRUE(pool.is_pools_dirty());
	EXPECT_TRUE(pool.is_tables_dirty());

	// the cull mirror still sees the chunk (the B1 path's headless
	// half): a camera in front of the box's cell (the box sits at
	// (8, 8, 8))
	{
		float view[16];
		bx::mtxLookAt(view, bx::Vec3(8.0f, 8.0f, -2.0f),
			bx::Vec3(8.0f, 8.0f, 8.0f), bx::Vec3(0.0f, 1.0f, 0.0f));

		float projection[16];
		bx::mtxProj(projection, 60.0f, 1.0f, 0.1f, 100.0f,
			bgfx::getCaps() ? bgfx::getCaps()->homogeneousDepth : true);

		float view_projection[16];
		bx::mtxMul(view_projection, view, projection);

		float planes[24];
		zircon_render_chunk_pool::extract_frustum_planes(
			view_projection, planes);

		kotek::uint32_t visible[8];
		EXPECT_EQ(pool.cull_chunks_against_frustum(planes, visible, 8),
			1u);
		EXPECT_EQ(visible[0], 0u);
	}

	delete &pool;
	delete p_records;
	reader.shutdown();
	delete &reader;

	delete p_reference;
	env.shutdown();
	delete &env;

	bake_remove_pack(pack_path);
}

// the DETERMINISM contract: two bakes of the same inputs produce
// BYTE-IDENTICAL packs (the u32f evaluation + the stable quantization
// + the fixed chunk order + the deterministic encoder)
TEST(Zircon_Game, CsgBakeDeterminism)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t first_path =
		bake_make_pack_path(env, "csg_bake_det_a.kpack");
	const kotek::static_path_t second_path =
		bake_make_pack_path(env, "csg_bake_det_b.kpack");
	bake_remove_pack(first_path);
	bake_remove_pack(second_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// two compounds, materials, multi-cell content (centers inside the
	// cells so the chunk counts stay fixture-known)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> first_descs[3] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 8.0, 8.0, 1),
		bake_make_box_desc(4.0, 2.0, 2.0, 24.0, 8.0, 8.0, 2),
		bake_make_box_desc(2.0, 6.0, 2.0, 8.0, 24.0, 8.0, 1)};
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> second_descs[1] = {
		bake_make_box_desc(8.0, 2.0, 2.0, 12.0, 8.0, 8.0, 4)};

	const zircon_csg_bake_compound_input_t compounds[] = {
		{first_descs, 3}, {second_descs, 1}};

	zircon_csg_bake_result_t first_result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_det_a.kpack", compounds, 2, "det",
		first_result));

	zircon_csg_bake_result_t second_result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_det_b.kpack", compounds, 2, "det",
		second_result));

	// the observability matches too
	EXPECT_EQ(first_result.m_baked_compound_count,
		second_result.m_baked_compound_count);
	EXPECT_EQ(first_result.m_emitted_chunk_count,
		second_result.m_emitted_chunk_count);
	EXPECT_EQ(first_result.m_emitted_vertex_count,
		second_result.m_emitted_vertex_count);
	EXPECT_EQ(first_result.m_emitted_index_count,
		second_result.m_emitted_index_count);
	EXPECT_EQ(first_result.m_material_count,
		second_result.m_material_count);

	// the byte-identical packs
	kotek::size_t first_size = 0;
	kotek::uint8_t* p_first = bake_read_through_dispatcher(
		env, first_path, first_size);
	ASSERT_TRUE(p_first != nullptr);

	kotek::size_t second_size = 0;
	kotek::uint8_t* p_second = bake_read_through_dispatcher(
		env, second_path, second_size);
	ASSERT_TRUE(p_second != nullptr);

	ASSERT_EQ(first_size, second_size);
	EXPECT_EQ(std::memcmp(p_first, p_second, first_size), 0);

	delete[] p_first;
	delete[] p_second;

	env.shutdown();
	delete &env;

	bake_remove_pack(first_path);
	bake_remove_pack(second_path);
}

// the streaming-order emission: the pack's entry table carries the
// entries in the documented order — manifest, materials, then the
// chunk bins in grid order (compounds in input order)
TEST(Zircon_Game, CsgBakeStreamingOrder)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		bake_make_pack_path(env, "csg_bake_order.kpack");
	bake_remove_pack(pack_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// compound 0: two boxes (cells (1,0,0) and (0,0,0) — emitted in
	// GRID order, so cell 0's chunk comes first); compound 1: one box
	// in cell (0,1,0) (the y/z centers at 8 keep each box inside its
	// cells)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> first_descs[2] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 24.0, 8.0, 8.0),
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 8.0, 8.0)};
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> second_descs[1] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 24.0, 8.0)};

	const zircon_csg_bake_compound_input_t compounds[] = {
		{first_descs, 2}, {second_descs, 1}};

	zircon_csg_bake_result_t result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_order.kpack", compounds, 2, "order",
		result));
	ASSERT_EQ(result.m_emitted_chunk_count, 3u);

	// the pack's entry table read from the file bytes: header (20
	// bytes: magic 8, entry_count u32, flags u32), then entry_count x
	// 45-byte records in WRITER order
	kotek::size_t pack_size = 0;
	kotek::uint8_t* p_pack = bake_read_through_dispatcher(
		env, pack_path, pack_size);
	ASSERT_TRUE(p_pack != nullptr);

	ASSERT_GE(pack_size, 20u + 5u * 45u);
	EXPECT_EQ(std::memcmp(p_pack, kotek::core::kKpackMagic, 8), 0);

	const kotek::uint32_t entry_count =
		zircon_csg_bake_load_u32(p_pack + 8);
	ASSERT_EQ(entry_count, 5u);

	const char* expected_names[5] = {"csg/order/manifest.bin",
		"csg/order/materials.json", "csg/order/compound_0/chunk_0.bin",
		"csg/order/compound_0/chunk_1.bin",
		"csg/order/compound_1/chunk_0.bin"};

	for (kotek::uint32_t index = 0; index < entry_count; ++index)
	{
		const kotek::uint64_t name_hash =
			zircon_csg_bake_load_u64(p_pack + 20 + index * 45);

		EXPECT_EQ(name_hash,
			kotek::core::kpack_hash_name(expected_names[index],
				std::strlen(expected_names[index])))
			<< "entry " << index << " is out of the streaming order";
	}

	delete[] p_pack;

	env.shutdown();
	delete &env;

	bake_remove_pack(pack_path);
}

// the capacity guards: a compound spanning more cells than the
// per-compound chunk cap is SKIPPED, loudly (the bake continues and
// succeeds for the rest); an invalid scene name is a hard caller
// error
TEST(Zircon_Game, CsgBakeCapacityGuards)
{
	zircon_csg_bake_test_env& env = *new zircon_csg_bake_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		bake_make_pack_path(env, "csg_bake_capacity.kpack");
	bake_remove_pack(pack_path);

	// the pack folder for the bake (kpack_write_file needs it)
	bake_ensure_packs_folder(env);

	// compound 0: one box per cell for 65 cells — over the
	// ZIRCON_DEF_CSG_BAKE_MAX_CHUNKS_PER_COMPOUND (= 64) cap (the
	// y/z centers at 8 keep every box inside its cell — exactly one
	// cell per box, 65 cells total)
	kotek::static_vector_t<zircon_csg_primitive_desc_t<
			zircon_csg_scalar_t>,
		ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND>
		wide_descs;

	for (kotek::uint32_t index = 0; index < 65; ++index)
	{
		wide_descs.push_back(bake_make_box_desc(1.0, 1.0, 1.0,
			16.0 * index + 0.5, 8.0, 8.0));
	}

	// compound 1: a valid single box (one grid cell)
	zircon_csg_primitive_desc_t<zircon_csg_scalar_t> valid_descs[1] = {
		bake_make_box_desc(2.0, 2.0, 2.0, 8.0, 8.0, 8.0)};

	const zircon_csg_bake_compound_input_t compounds[] = {
		{wide_descs.data(), 65}, {valid_descs, 1}};

	zircon_csg_bake_result_t result{};
	ASSERT_TRUE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_capacity.kpack", compounds, 2,
		"capacity", result));

	// the loud graceful skip: the wide compound is dropped, the valid
	// one bakes, the pack writes
	EXPECT_EQ(result.m_baked_compound_count, 1u);
	EXPECT_EQ(result.m_skipped_compound_count, 1u);
	EXPECT_EQ(result.m_emitted_chunk_count, 1u);

	// the pack loads: only the valid compound's chunk
	zircon_csg_bake_test_env& reader = *new zircon_csg_bake_test_env();
	reader.initialize();

	zircon_render_chunk_pool& pool = *new zircon_render_chunk_pool();

	kotek::uint32_t loaded_count = 0;
	ASSERT_TRUE(pool.load_chunks_from_pack(
		&reader.filesystem, "csg/capacity", loaded_count));
	EXPECT_EQ(loaded_count, 1u);
	EXPECT_EQ(pool.get_live_chunk_count(), 1u);

	delete &pool;
	reader.shutdown();
	delete &reader;

	// an invalid scene name (a path walk) is a hard caller error
	zircon_csg_bake_result_t bad_result{};
	EXPECT_FALSE(zircon_csg_bake_pack(&env.filesystem,
		"data_game/packs/csg_bake_capacity.kpack", compounds, 2,
		"../escape", bad_result));

	env.shutdown();
	delete &env;

	bake_remove_pack(pack_path);
}

		#endif
	#endif
#endif
