#include "../zircon_game_manager.h"

#ifdef KOTEK_USE_TESTS_RUNTIME
	#ifdef KOTEK_DEBUG

		#include <gtest/gtest.h>

		#include <cmath>
		#include <cstring>
		#include <filesystem>

		#include "../../core/zircon_texture_bcn.h"
		#include "../../render/bgfx/passes/no_streaming/zircon_render_texture_bcn.h"

		#include <kotek.core.filesystem.pack/include/kotek_kpack_format.h>

		#ifndef ZIRCON_DEF_UNIT_TEST_TEXTURE_BCN
			#define ZIRCON_DEF_UNIT_TEST_TEXTURE_BCN 1
		#endif

		#if ZIRCON_DEF_UNIT_TEST_TEXTURE_BCN == 1

// functional proofs for task Z24 phase B4 (the BCn texture pipeline):
// the format header/layout pins, the BC1 solid-block exact bytes, the
// per-class quality bounds (encode through the vendored encoders,
// decode through the vendored decoders, pin the PSNR floor), the
// box-filter mip chain pinned on a known 8x8, the pack roundtrip
// (encode -> pack -> the render-side prepare through the dispatcher ->
// the upload layout == the baked blocks byte-identical), the
// dimension/capacity guards (the v1 multiple-of-4 rule, the caps, the
// parse corruptions, the entry-name rule, the .zraw intake, the BC6H
// deferral) and the shipped boot probe fixture pinned against a fresh
// encode (the determinism contract). Tier: lightweight (rule 8a — the
// fixtures are 4x4..64x64; the encodes are milliseconds).
//
// THE VENDORED SEAM: the encoder/decoder are TOOL-side code — the
// runtime path never encodes or decodes, it uploads blocks as-is. This
// TU includes their HEADERS only; the implementations compile as
// separate Debug-only target sources (see src/engine/CMakeLists.txt —
// bc7enc.h has no include guard upstream, so the byte-pristine sources
// cannot share one TU with their own headers). bcdec is header-only:
// its implementation macro lands here (this TU is its only consumer).

			#pragma warning(push, 0)
			#include "../../tools/zircon_bcn_encoder/zircon_bcn_encoder_adapter.h"

			#define BCDEC_IMPLEMENTATION
			#include "../../tools/zircon_bcn_encoder/bcdec.h"
			#pragma warning(pop)

namespace
{
	// the headless filesystem environment (the csg-bake fixture's
	// shape): a real filesystem behind a real framework config — the
	// boot dispatcher, no engine session needed. Heap per the fixture
	// rule
	struct zircon_texture_bcn_test_env
	{
		kotek::core::ktkFrameworkConfig framework_config;
		kotek::core::ktkFileSystem filesystem;

		void initialize(void)
		{
			this->filesystem.Initialize(&this->framework_config);
		}

		void shutdown(void) { this->filesystem.Shutdown(); }
	};

	kotek::static_path_t texture_make_pack_path(
		zircon_texture_bcn_test_env& env, const char* p_pack_name)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		kotek::static_path_t pack_path = packs_folder;
		pack_path /= p_pack_name;
		return pack_path;
	}

	void texture_ensure_packs_folder(zircon_texture_bcn_test_env& env)
	{
		kotek::static_path_t packs_folder;
		env.filesystem.Make_Path(packs_folder,
			kotek::core::eFolderIndex::kFolderIndex_DataGame);
		packs_folder /= kotek::core::kKpackPacksFolderName;

		std::error_code ec;
		std::filesystem::create_directories(
			std::filesystem::path(packs_folder.c_str()), ec);
	}

	void texture_remove_pack(const kotek::static_path_t& pack_path)
	{
		std::error_code ec;
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()), ec);

		// the packs folder leaves with the last test pack (the shipped
		// tree has no data_game/packs — the pack_boot / embedded-defaults
		// fixtures pin that invariant)
		ec.clear();
		std::filesystem::remove(
			std::filesystem::path(pack_path.c_str()).parent_path(), ec);
	}

	// the deterministic mild-noise generator (an integer LCG — no libm
	// in the fixtures, so the fixture bytes are platform-exact)
	struct texture_lcg_t
	{
		kotek::uint32_t m_state;

		kotek::uint32_t next(void) noexcept
		{
			this->m_state =
				this->m_state * 1664525u + 1013904223u;
			return this->m_state >> 16;
		}
	};

	// the albedo fixture: a smooth diagonal gradient + mild
	// deterministic noise (+-8)
	void texture_make_gradient_noise_rgba(kotek::uint8_t* p_out,
		kotek::uint32_t width, kotek::uint32_t height) noexcept
	{
		texture_lcg_t lcg{0x1234abcd};

		for (kotek::uint32_t y = 0; y < height; ++y)
		{
			for (kotek::uint32_t x = 0; x < width; ++x)
			{
				const int noise =
					static_cast<int>(lcg.next() % 17u) - 8;

				const int r = static_cast<int>(x * 255 / (width - 1));
				const int g = static_cast<int>(y * 255 / (height - 1));
				const int b = static_cast<int>(
					(x + y) * 255 / (width + height - 2));

				auto clamp8 = [](int v) -> kotek::uint8_t
				{
					return static_cast<kotek::uint8_t>(
						v < 0 ? 0 : (v > 255 ? 255 : v));
				};

				kotek::uint8_t* p = p_out + (y * width + x) * 4;
				p[0] = clamp8(r + noise);
				p[1] = clamp8(g - noise / 2);
				p[2] = clamp8(b + noise / 3);
				p[3] = 255;
			}
		}
	}

	// the normal-map fixture: smooth tangent-space normals (RG carries
	// the map; B/A are filler the BC5 path ignores)
	void texture_make_normal_map_rgba(kotek::uint8_t* p_out,
		kotek::uint32_t width, kotek::uint32_t height) noexcept
	{
		for (kotek::uint32_t y = 0; y < height; ++y)
		{
			for (kotek::uint32_t x = 0; x < width; ++x)
			{
				// smooth waves — integer math only (the determinism
				// posture): R = 128 + 96 * wave(x), G = 128 +
				// 96 * wave(y), wave = a triangle wave of period 16
				auto wave = [](kotek::uint32_t v) -> int
				{
					const kotek::uint32_t phase = v % 16u;
					const int t = phase < 8u
						? static_cast<int>(phase)
						: 15 - static_cast<int>(phase);
					return (t * 2 - 7) * 96 / 7;
				};

				kotek::uint8_t* p = p_out + (y * width + x) * 4;
				p[0] = static_cast<kotek::uint8_t>(128 + wave(x));
				p[1] = static_cast<kotek::uint8_t>(128 + wave(y));
				p[2] = 255;
				p[3] = 255;
			}
		}
	}

	// the mask fixture: a two-color 4-texel checker (both colors off
	// the 565 grid, so the BC1 error is finite but small)
	void texture_make_mask_rgba(kotek::uint8_t* p_out,
		kotek::uint32_t width, kotek::uint32_t height) noexcept
	{
		constexpr kotek::uint8_t k_color_a[4] = {200, 30, 90, 255};
		constexpr kotek::uint8_t k_color_b[4] = {40, 220, 60, 255};

		for (kotek::uint32_t y = 0; y < height; ++y)
		{
			for (kotek::uint32_t x = 0; x < width; ++x)
			{
				const kotek::uint8_t* p_color =
					((x / 2 + y / 2) & 1u) ? k_color_a : k_color_b;

				kotek::uint8_t* p = p_out + (y * width + x) * 4;
				p[0] = p_color[0];
				p[1] = p_color[1];
				p[2] = p_color[2];
				p[3] = p_color[3];
			}
		}
	}

	// the PSNR of two RGBA8 images over the channel set (channel_count
	// 3 = RGB, 2 = RG). +inf when identical (the caller treats that as
	// a pass)
	double texture_compute_psnr_db(const kotek::uint8_t* p_a,
		const kotek::uint8_t* p_b, kotek::uint32_t width,
		kotek::uint32_t height, kotek::uint32_t channel_count) noexcept
	{
		double squared_error = 0.0;

		for (kotek::uint32_t i = 0; i < width * height; ++i)
		{
			for (kotek::uint32_t c = 0; c < channel_count; ++c)
			{
				const double delta = static_cast<double>(p_a[i * 4 + c]) -
					static_cast<double>(p_b[i * 4 + c]);
				squared_error += delta * delta;
			}
		}

		if (squared_error == 0.0)
			return 1e9;

		const double mse = squared_error /
			(static_cast<double>(width) * height * channel_count);

		return 10.0 * std::log10(255.0 * 255.0 / mse);
	}

	// one bake through the real seam: the adapter context at the
	// default tier + zircon_texture_bcn_build (the fixture sizes are
	// tiny — the output buffer is a bounded heap block)
	bool texture_bake_to_heap(const kotek::uint8_t* p_rgba,
		kotek::uint32_t width, kotek::uint32_t height,
		eZirconTextureBcnFormat format,
		eZirconTextureBcnClass content_class, kotek::uint32_t mip_count,
		kotek::uint8_t* p_out, kotek::size_t out_capacity,
		kotek::size_t& out_size)
	{
		zircon_bcn_encoder_context_t encoder;
		zircon_bcn_encoder_initialize(
			encoder, eZirconBcnQuality::kDefault);

		return zircon_texture_bcn_build(p_rgba, width, height, format,
				   content_class, mip_count,
				   static_cast<kotek::uint8_t>(eZirconBcnQuality::kDefault),
				   &zircon_bcn_encoder_encode_level, &encoder, p_out,
				   out_capacity, out_size) ==
			eZirconTextureBcnStatus::kSuccess;
	}
} // namespace

TEST(Zircon_TextureBcn, FormatHeaderLayoutPinned)
{
	// the fixed record sizes
	EXPECT_EQ(zircon_texture_bcn_header_size, 32u);
	EXPECT_EQ(zircon_texture_bcn_mip_record_size, 8u);
	EXPECT_EQ(zircon_texture_bcn_zraw_header_size, 16u);

	// the block strides per format
	EXPECT_EQ(zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kBC1),
		8u);
	EXPECT_EQ(zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kBC3),
		16u);
	EXPECT_EQ(zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kBC5),
		16u);
	EXPECT_EQ(zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kBC7),
		16u);
	EXPECT_EQ(zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kBC6H),
		16u);
	EXPECT_EQ(
		zircon_texture_bcn_block_bytes(eZirconTextureBcnFormat::kEndOfEnum),
		0u);

	// the class -> default-format map (the plan's mapping)
	EXPECT_EQ(zircon_texture_bcn_default_format_for_class(
				  eZirconTextureBcnClass::kAlbedo),
		eZirconTextureBcnFormat::kBC7);
	EXPECT_EQ(zircon_texture_bcn_default_format_for_class(
				  eZirconTextureBcnClass::kNormal),
		eZirconTextureBcnFormat::kBC5);
	EXPECT_EQ(
		zircon_texture_bcn_default_format_for_class(eZirconTextureBcnClass::kAO),
		eZirconTextureBcnFormat::kBC7);
	EXPECT_EQ(zircon_texture_bcn_default_format_for_class(
				  eZirconTextureBcnClass::kMask),
		eZirconTextureBcnFormat::kBC1);
	EXPECT_EQ(zircon_texture_bcn_default_format_for_class(
				  eZirconTextureBcnClass::kHDR),
		eZirconTextureBcnFormat::kBC6H);

	// the mip math: 64x64 -> 7 levels (64,32,16,8,4,2,1), 8x4 -> 4,
	// 4x4 -> 3, 4096 -> 13
	EXPECT_EQ(zircon_texture_bcn_full_mip_count(64, 64), 7u);
	EXPECT_EQ(zircon_texture_bcn_full_mip_count(8, 4), 4u);
	EXPECT_EQ(zircon_texture_bcn_full_mip_count(4, 4), 3u);
	EXPECT_EQ(zircon_texture_bcn_full_mip_count(4096, 4096), 13u);
	EXPECT_EQ(zircon_texture_bcn_mip_dimension(64, 0), 64u);
	EXPECT_EQ(zircon_texture_bcn_mip_dimension(64, 6), 1u);
	EXPECT_EQ(zircon_texture_bcn_mip_dimension(64, 12), 1u);

	// the payload math: 64x64 BC7 = 256 blocks x 16 at mip 0, the full
	// chain sums (256+64+16+4+1+1+1) x 16 = 5488; the file is 32 + 56 +
	// 5488 = 5576
	EXPECT_EQ(
		zircon_texture_bcn_level_block_bytes(
			64, 64, eZirconTextureBcnFormat::kBC7),
		4096u);
	EXPECT_EQ(
		zircon_texture_bcn_level_block_bytes(
			8, 8, eZirconTextureBcnFormat::kBC1),
		32u);
	EXPECT_EQ(
		zircon_texture_bcn_level_block_bytes(
			4, 4, eZirconTextureBcnFormat::kBC1),
		8u);
	EXPECT_EQ(
		zircon_texture_bcn_total_payload_bytes(
			64, 64, eZirconTextureBcnFormat::kBC7, 7),
		5488u);
	EXPECT_EQ(
		zircon_texture_bcn_total_file_bytes(
			64, 64, eZirconTextureBcnFormat::kBC7, 7),
		5576u);

	// the bgfx format map (the no-decode upload's enum bridge)
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kBC1),
		bgfx::TextureFormat::BC1);
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kBC3),
		bgfx::TextureFormat::BC3);
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kBC5),
		bgfx::TextureFormat::BC5);
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kBC7),
		bgfx::TextureFormat::BC7);
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kBC6H),
		bgfx::TextureFormat::BC6H);
	EXPECT_EQ(no_streaming::zircon_render_texture_bcn_map_format(
				  eZirconTextureBcnFormat::kEndOfEnum),
		bgfx::TextureFormat::Unknown);

	// the entry-name builder
	kotek::static_cstring_t<ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH>
		entry_name;

	ASSERT_EQ(zircon_texture_bcn_make_entry_name(
				  "boot", "boot_checker", entry_name),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_STREQ(entry_name.c_str(), "textures/boot/boot_checker.bcn");
}

TEST(Zircon_TextureBcn, BC1SolidColorBlockExact)
{
	// a 4x4 solid pure red: the BC1 block is format-determined — every
	// texel takes selector 0, endpoint c0 is the RGB565 of the color
	// (0xF800). rgbcx's cBC1Ideal solid-block path pins c0 = 0xF800 and
	// NUDGES the unused second endpoint (0xF7FF — the precomputed
	// single-color table's bracket value; with all-zero selectors it
	// never reaches a texel). The pinned bytes are the encoder's
	// deterministic output, verified reasonable by the decode leg below
	// (bcdec expands selector-0 -> c0 -> (255,0,0,255) exactly)
	kotek::uint8_t pixels[4 * 4 * 4];

	for (kotek::uint32_t i = 0; i < 16u; ++i)
	{
		pixels[i * 4 + 0] = 255;
		pixels[i * 4 + 1] = 0;
		pixels[i * 4 + 2] = 0;
		pixels[i * 4 + 3] = 255;
	}

	kotek::uint8_t file_bytes[256];
	kotek::size_t file_size = 0;

	ASSERT_TRUE(texture_bake_to_heap(pixels, 4, 4,
		eZirconTextureBcnFormat::kBC1, eZirconTextureBcnClass::kMask, 1,
		file_bytes, sizeof(file_bytes), file_size));

	// one level, one block: 32 + 8 + 8 = 48 file bytes
	ASSERT_EQ(file_size, 48u);

	const kotek::uint8_t* p_block = file_bytes + 40;

	const kotek::uint8_t expected_block[8] = {0x00, 0xF8, 0xFF, 0xF7,
		0x00, 0x00, 0x00, 0x00};

	for (kotek::uint32_t i = 0; i < 8u; ++i)
	{
		EXPECT_EQ(p_block[i], expected_block[i])
			<< "the BC1 solid block's byte " << i << " diverged";
	}

	// the decode leg (the vendored decoder): the block expands back to
	// the solid red exactly (0xF800 -> (255,0,0) through the (v<<3)|
	// (v>>2) expansion)
	kotek::uint8_t decoded[4 * 4 * 4];

	ASSERT_TRUE(zircon_bcn_encoder_decode_level(
		eZirconTextureBcnFormat::kBC1, p_block, 4, 4, decoded));

	for (kotek::uint32_t i = 0; i < 16u; ++i)
	{
		EXPECT_EQ(decoded[i * 4 + 0], 255);
		EXPECT_EQ(decoded[i * 4 + 1], 0);
		EXPECT_EQ(decoded[i * 4 + 2], 0);
	}

	// every block of a solid 8x8 encodes to the SAME bytes (the
	// determinism posture at the block level)
	kotek::uint8_t pixels8[8 * 8 * 4];

	for (kotek::uint32_t i = 0; i < 64u; ++i)
	{
		pixels8[i * 4 + 0] = 255;
		pixels8[i * 4 + 1] = 0;
		pixels8[i * 4 + 2] = 0;
		pixels8[i * 4 + 3] = 255;
	}

	kotek::uint8_t file_bytes8[512];
	kotek::size_t file_size8 = 0;

	ASSERT_TRUE(texture_bake_to_heap(pixels8, 8, 8,
		eZirconTextureBcnFormat::kBC1, eZirconTextureBcnClass::kMask, 1,
		file_bytes8, sizeof(file_bytes8), file_size8));

	for (kotek::uint32_t block = 0; block < 4u; ++block)
	{
		EXPECT_EQ(std::memcmp(file_bytes8 + 40 + block * 8, expected_block,
					  8),
			0)
			<< "solid block " << block << " diverged";
	}
}

TEST(Zircon_TextureBcn, QualityPsnrBoundsPerClass)
{
	// the per-class quality proof: encode at the class's default format
	// through the vendored encoders, decode through the vendored
	// decoders and pin the PSNR floor. The floors are MEASURED on these
	// fixtures and pinned with ~2.5 dB of margin (the exact measured
	// values ride the quality-pin log line):
	//   BC7 (albedo, gradient + +-8 noise): measured 32.48 dB -> floor 30
	//     (the mild noise sits at BC7's 4 bpp limit by design — the
	//     gradient itself is tracked near-losslessly)
	//   BC5 (normals, smooth waves):       measured 38.02 dB -> floor 35
	//   BC1 (mask, two off-grid colors):   measured 43.12 dB -> floor 40
	constexpr kotek::uint32_t k_size = 16;

	kotek::uint8_t source[k_size * k_size * 4];
	kotek::uint8_t decoded[k_size * k_size * 4];
	kotek::uint8_t file_bytes[4096];

	struct class_case_t
	{
		eZirconTextureBcnFormat m_format;
		eZirconTextureBcnClass m_class;
		kotek::uint32_t m_channels;
		double m_psnr_floor;
		void (*m_fill)(kotek::uint8_t*, kotek::uint32_t, kotek::uint32_t);
	};

	const class_case_t cases[] = {
		{eZirconTextureBcnFormat::kBC7, eZirconTextureBcnClass::kAlbedo, 3,
			30.0, &texture_make_gradient_noise_rgba},
		{eZirconTextureBcnFormat::kBC5, eZirconTextureBcnClass::kNormal, 2,
			35.0, &texture_make_normal_map_rgba},
		{eZirconTextureBcnFormat::kBC1, eZirconTextureBcnClass::kMask, 3,
			40.0, &texture_make_mask_rgba},
	};

	for (const class_case_t& test_case : cases)
	{
		test_case.m_fill(source, k_size, k_size);

		kotek::size_t file_size = 0;

		ASSERT_TRUE(texture_bake_to_heap(source, k_size, k_size,
			test_case.m_format, test_case.m_class, 1, file_bytes,
			sizeof(file_bytes), file_size));

		const kotek::uint8_t* p_blocks = file_bytes + 40;

		ASSERT_TRUE(zircon_bcn_encoder_decode_level(
			test_case.m_format, p_blocks, k_size, k_size, decoded));

		const double psnr = texture_compute_psnr_db(source, decoded,
			k_size, k_size, test_case.m_channels);

		KOTEK_MESSAGE(
			"[texture_bcn] quality pin: format {} measured {} dB (floor "
			"{} dB)",
			static_cast<kotek::uint32_t>(test_case.m_format), psnr,
			test_case.m_psnr_floor);

		EXPECT_GE(psnr, test_case.m_psnr_floor)
			<< "format " << static_cast<kotek::uint32_t>(test_case.m_format)
			<< " fell below its quality floor";
	}
}

TEST(Zircon_TextureBcn, MipChainBoxFilterPinned)
{
	// the box filter pinned on a known 8x8: R = x + 8y, G = 7,
	// B = 200, A = 255. Derived expectations (integer round-nearest):
	//   level1 4x4: R = 2x + 16y + 5
	//   level2 2x2: R = 4x + 32y + 14
	//   level3 1x1: R = 32
	kotek::uint8_t source[8 * 8 * 4];

	for (kotek::uint32_t y = 0; y < 8u; ++y)
	{
		for (kotek::uint32_t x = 0; x < 8u; ++x)
		{
			kotek::uint8_t* p = source + (y * 8 + x) * 4;
			p[0] = static_cast<kotek::uint8_t>(x + 8 * y);
			p[1] = 7;
			p[2] = 200;
			p[3] = 255;
		}
	}

	kotek::uint8_t level1[4 * 4 * 4];
	zircon_texture_bcn_downsample_box(source, 8, 8, level1);

	for (kotek::uint32_t y = 0; y < 4u; ++y)
	{
		for (kotek::uint32_t x = 0; x < 4u; ++x)
		{
			const kotek::uint8_t* p = level1 + (y * 4 + x) * 4;
			EXPECT_EQ(p[0],
				static_cast<kotek::uint8_t>(2 * x + 16 * y + 5))
				<< "level1 (" << x << ", " << y << ")";
			EXPECT_EQ(p[1], 7);
			EXPECT_EQ(p[2], 200);
			EXPECT_EQ(p[3], 255);
		}
	}

	kotek::uint8_t level2[2 * 2 * 4];
	zircon_texture_bcn_downsample_box(level1, 4, 4, level2);

	for (kotek::uint32_t y = 0; y < 2u; ++y)
	{
		for (kotek::uint32_t x = 0; x < 2u; ++x)
		{
			const kotek::uint8_t* p = level2 + (y * 2 + x) * 4;
			EXPECT_EQ(p[0],
				static_cast<kotek::uint8_t>(4 * x + 32 * y + 14))
				<< "level2 (" << x << ", " << y << ")";
		}
	}

	kotek::uint8_t level3[4];
	zircon_texture_bcn_downsample_box(level2, 2, 2, level3);

	EXPECT_EQ(level3[0], 32);
	EXPECT_EQ(level3[1], 7);
	EXPECT_EQ(level3[2], 200);
	EXPECT_EQ(level3[3], 255);

	// the full chain through the build: 8x8 BC7 has 4 levels, sizes
	// 64 + 16 + 16 + 16 = 112 block bytes (2x2 blocks at mip 0; 4x4,
	// 2x2 and 1x1 are one block each)
	EXPECT_EQ(
		zircon_texture_bcn_total_payload_bytes(
			8, 8, eZirconTextureBcnFormat::kBC7, 4),
		112u);
}

TEST(Zircon_TextureBcn, PackRoundtripUploadLayoutByteIdentical)
{
	zircon_texture_bcn_test_env& env = *new zircon_texture_bcn_test_env();
	env.initialize();

	const kotek::static_path_t pack_path =
		texture_make_pack_path(env, "texture_bcn_roundtrip.kpack");
	texture_remove_pack(pack_path);
	texture_ensure_packs_folder(env);

	// the bake: 16x16 gradient BC7, the FULL chain (5 levels)
	constexpr kotek::uint32_t k_size = 16;

	kotek::uint8_t source[k_size * k_size * 4];
	texture_make_gradient_noise_rgba(source, k_size, k_size);

	kotek::uint8_t file_bytes[4096];
	kotek::size_t file_size = 0;

	ASSERT_TRUE(texture_bake_to_heap(source, k_size, k_size,
		eZirconTextureBcnFormat::kBC7, eZirconTextureBcnClass::kAlbedo, 0,
		file_bytes, sizeof(file_bytes), file_size));

	// the pack (zstd — the plan's "the pack's zstd blocks squeeze BCn
	// another ~10-30%"): one entry. THE PACK-ENTRY NAMESPACE IS
	// REPO-ROOT-RELATIVE (the pack backend relativizes absolute read
	// paths against the filesystem root — the Z23 marker precedent), so
	// the data_game-resident texture's entry name carries the
	// "data_game/" prefix while its LOGICAL name stays
	// content-root-relative
	kotek::static_cstring_t<ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH>
		entry_name;

	ASSERT_EQ(zircon_texture_bcn_make_entry_name(
				  "roundtrip", "gradient", entry_name),
		eZirconTextureBcnStatus::kSuccess);

	char pack_entry_name[ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH];
	snprintf(pack_entry_name, sizeof(pack_entry_name), "data_game/%s",
		entry_name.c_str());

	kotek::core::kpack_writer_entry_t entries[1];
	entries[0].p_name = pack_entry_name;
	entries[0].p_data = file_bytes;
	entries[0].data_size = file_size;
	entries[0].compression = kotek::core::eKpackCompression::kZstd;

	ASSERT_TRUE(kotek::core::kpack_write_file(
		pack_path.c_str(), entries, 1));

	// a FRESH filesystem mounts the pack at Initialize (the boot
	// dispatcher) and the render-side prepare reads through the
	// priority chain
	zircon_texture_bcn_test_env& reader =
		*new zircon_texture_bcn_test_env();
	reader.initialize();

	kotek::uint8_t scratch[4096];

	no_streaming::zircon_render_texture_bcn_upload_t upload;

	ASSERT_TRUE(no_streaming::zircon_render_texture_bcn_prepare(
		&reader.filesystem, kotek::static_path_t(entry_name.c_str()),
		scratch, sizeof(scratch), upload));

	// the upload layout == the baked bytes, byte-identical (the
	// no-decode contract: prepare moved nothing but the container)
	EXPECT_EQ(upload.m_width, k_size);
	EXPECT_EQ(upload.m_height, k_size);
	EXPECT_EQ(upload.m_mip_count, 5u);
	EXPECT_EQ(upload.m_bgfx_format, bgfx::TextureFormat::BC7);
	EXPECT_EQ(upload.m_payload_size,
		zircon_texture_bcn_total_payload_bytes(
			k_size, k_size, eZirconTextureBcnFormat::kBC7, 5));

	// the whole file (header + table + payload) survived the pack
	// byte-identical
	EXPECT_EQ(std::memcmp(scratch, file_bytes, file_size), 0);

	// the payload region == the bake's blocks
	EXPECT_EQ(std::memcmp(upload.m_p_payload,
				  file_bytes + upload.m_desc.m_payload_file_offset,
				  upload.m_payload_size),
		0);

	// the per-mip records tile the payload exactly
	kotek::uint32_t expected_offset = 0;

	for (kotek::uint32_t level = 0; level < upload.m_mip_count; ++level)
	{
		kotek::uint32_t offset = 0, size = 0;

		ASSERT_TRUE(zircon_texture_bcn_read_mip_record(
			scratch, upload.m_desc, level, offset, size));

		EXPECT_EQ(offset, expected_offset);
		EXPECT_EQ(size,
			zircon_texture_bcn_level_block_bytes(k_size >> level,
				k_size >> level, eZirconTextureBcnFormat::kBC7));

		expected_offset += size;
	}

	reader.shutdown();
	delete &reader;

	texture_remove_pack(pack_path);

	env.shutdown();
	delete &env;
}

TEST(Zircon_TextureBcn, DimensionCapacityAndCorruptionGuards)
{
	// the source-validation matrix
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  6, 8, eZirconTextureBcnFormat::kBC7, 0),
		eZirconTextureBcnStatus::kDimensionNotMultipleOf4);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  2, 4, eZirconTextureBcnFormat::kBC7, 0),
		eZirconTextureBcnStatus::kDimensionOutOfRange);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  8192, 4, eZirconTextureBcnFormat::kBC7, 0),
		eZirconTextureBcnStatus::kDimensionOutOfRange);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  4, 4, eZirconTextureBcnFormat::kBC7, 4),
		eZirconTextureBcnStatus::kMipCountInvalid);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  4, 4, eZirconTextureBcnFormat::kEndOfEnum, 0),
		eZirconTextureBcnStatus::kInvalidArgument);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  4096, 4096, eZirconTextureBcnFormat::kBC7, 0),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_EQ(zircon_texture_bcn_validate_source(
				  4096, 4096, eZirconTextureBcnFormat::kBC1, 13),
		eZirconTextureBcnStatus::kSuccess);

	// the build's caller-error + capacity paths
	kotek::uint8_t pixels[4 * 4 * 4] = {};
	kotek::uint8_t file_bytes[512];
	kotek::size_t file_size = 0;

	zircon_bcn_encoder_context_t encoder;
	zircon_bcn_encoder_initialize(encoder, eZirconBcnQuality::kDefault);

	EXPECT_EQ(zircon_texture_bcn_build(nullptr, 4, 4,
				  eZirconTextureBcnFormat::kBC1,
				  eZirconTextureBcnClass::kMask, 1, 1,
				  &zircon_bcn_encoder_encode_level, &encoder, file_bytes,
				  sizeof(file_bytes), file_size),
		eZirconTextureBcnStatus::kInvalidArgument);

	// the BC6H deferral: the adapter fails the level loudly
	EXPECT_EQ(zircon_texture_bcn_build(pixels, 4, 4,
				  eZirconTextureBcnFormat::kBC6H,
				  eZirconTextureBcnClass::kHDR, 1, 1,
				  &zircon_bcn_encoder_encode_level, &encoder, file_bytes,
				  sizeof(file_bytes), file_size),
		eZirconTextureBcnStatus::kEncodeFailed);

	// the too-small output reports the REQUIRED size (the B0 contract's
	// shape): a 4x4 BC1 full chain is 32 header + 24 table (3 levels) +
	// (8+8+8) payload = 80
	EXPECT_EQ(zircon_texture_bcn_build(pixels, 4, 4,
				  eZirconTextureBcnFormat::kBC1,
				  eZirconTextureBcnClass::kMask, 0, 1,
				  &zircon_bcn_encoder_encode_level, &encoder, file_bytes,
				  16, file_size),
		eZirconTextureBcnStatus::kBufferTooSmall);
	EXPECT_EQ(file_size, 80u);

	// the parse's corruption rejections
	kotek::uint8_t good_file[128];
	kotek::size_t good_size = 0;

	ASSERT_TRUE(texture_bake_to_heap(pixels, 4, 4,
		eZirconTextureBcnFormat::kBC1, eZirconTextureBcnClass::kMask, 0,
		good_file, sizeof(good_file), good_size));
	ASSERT_EQ(good_size, 80u);

	zircon_texture_bcn_desc_t desc;

	ASSERT_EQ(zircon_texture_bcn_parse(good_file, good_size, desc),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_EQ(desc.m_width, 4u);
	EXPECT_EQ(desc.m_mip_count, 3u);
	EXPECT_EQ(desc.m_payload_file_offset, 56u);

	kotek::uint8_t corrupt[80];
	std::memcpy(corrupt, good_file, sizeof(corrupt));

	corrupt[0] = 'X';
	EXPECT_EQ(zircon_texture_bcn_parse(corrupt, sizeof(corrupt), desc),
		eZirconTextureBcnStatus::kCorruptData);

	// a truncated file
	EXPECT_EQ(zircon_texture_bcn_parse(good_file, 40, desc),
		eZirconTextureBcnStatus::kCorruptData);

	// a mip-table skew (the second record's offset bumped)
	std::memcpy(corrupt, good_file, sizeof(corrupt));
	corrupt[32 + 8] = 7;
	EXPECT_EQ(zircon_texture_bcn_parse(corrupt, sizeof(corrupt), desc),
		eZirconTextureBcnStatus::kCorruptData);

	// the entry-name rule
	kotek::static_cstring_t<ZIRCON_DEF_TEXTURE_BCN_ENTRY_NAME_MAX_LENGTH>
		entry_name;

	EXPECT_EQ(zircon_texture_bcn_make_entry_name(
				  "..", "x", entry_name),
		eZirconTextureBcnStatus::kNameInvalid);
	EXPECT_EQ(zircon_texture_bcn_make_entry_name(
				  "boot", "a/b", entry_name),
		eZirconTextureBcnStatus::kNameInvalid);
	EXPECT_EQ(zircon_texture_bcn_make_entry_name(
				  "boot", "", entry_name),
		eZirconTextureBcnStatus::kNameInvalid);

	// the .zraw roundtrip + rejections
	kotek::uint8_t zraw[16 + 16 * 4];
	kotek::size_t zraw_size = 0;

	ASSERT_EQ(zircon_texture_bcn_zraw_write(pixels, 4, 4, zraw,
				  sizeof(zraw), zraw_size),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_EQ(zraw_size, 16u + 64u);

	kotek::uint32_t zraw_w = 0, zraw_h = 0, zraw_offset = 0;

	ASSERT_EQ(zircon_texture_bcn_zraw_parse(
				  zraw, zraw_size, zraw_w, zraw_h, zraw_offset),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_EQ(zraw_w, 4u);
	EXPECT_EQ(zraw_h, 4u);
	EXPECT_EQ(zraw_offset, 16u);
	EXPECT_EQ(std::memcmp(zraw + zraw_offset, pixels, sizeof(pixels)), 0);

	zraw[0] = 'X';
	EXPECT_EQ(zircon_texture_bcn_zraw_parse(
				  zraw, zraw_size, zraw_w, zraw_h, zraw_offset),
		eZirconTextureBcnStatus::kCorruptData);

	EXPECT_EQ(zircon_texture_bcn_zraw_parse(
				  zraw, 20, zraw_w, zraw_h, zraw_offset),
		eZirconTextureBcnStatus::kCorruptData);
}

TEST(Zircon_TextureBcn, BootFixtureMatchesEncoder)
{
	// the shipped boot probe (data_game/textures/boot/boot_checker.bcn —
	// the B1 pass uploads it at create): pinned against a FRESH encode
	// of the recipe (zircon_texture_bcn_make_boot_checker_rgba). This
	// pins the fixture's freshness, the encoder's determinism across
	// two binaries (the tool baked the shipped file; this binary
	// re-encodes it) and the recipe's single home
	zircon_texture_bcn_test_env& env = *new zircon_texture_bcn_test_env();
	env.initialize();

	kotek::static_path_t fixture_path;
	env.filesystem.Make_Path(fixture_path,
		kotek::core::eFolderIndex::kFolderIndex_DataGame);
	fixture_path /= "textures/boot/boot_checker.bcn";

	kotek::size_t fixture_size = 0;

	ASSERT_TRUE(env.filesystem.Get_FileSize(fixture_path, fixture_size))
		<< "the shipped boot probe is missing — regenerate it with: "
		   "zircon_bcn_bake --synthesize_boot_checker";

	kotek::uint8_t* p_fixture = new kotek::uint8_t[fixture_size + 1];
	kotek::size_t read_size = fixture_size + 1;

	ASSERT_TRUE(env.filesystem.Read_File(fixture_path, p_fixture, read_size));
	ASSERT_EQ(read_size, fixture_size);

	// the format view: 64x64 BC7, the full 7-level chain
	zircon_texture_bcn_desc_t desc;

	ASSERT_EQ(
		zircon_texture_bcn_parse(p_fixture, fixture_size, desc),
		eZirconTextureBcnStatus::kSuccess);
	EXPECT_EQ(desc.m_width, 64u);
	EXPECT_EQ(desc.m_height, 64u);
	EXPECT_EQ(desc.m_format, eZirconTextureBcnFormat::kBC7);
	EXPECT_EQ(desc.m_mip_count, 7u);
	EXPECT_EQ(desc.m_payload_size, 5488u);
	EXPECT_EQ(fixture_size, 5576u);

	// the fresh encode from the recipe (the default tier, the full
	// chain — the tool's --synthesize_boot_checker defaults)
	kotek::uint8_t pixels[64 * 64 * 4];
	zircon_texture_bcn_make_boot_checker_rgba(pixels);

	kotek::uint8_t rebuilt[6000];
	kotek::size_t rebuilt_size = 0;

	ASSERT_TRUE(texture_bake_to_heap(pixels, 64, 64,
		eZirconTextureBcnFormat::kBC7, eZirconTextureBcnClass::kAlbedo, 0,
		rebuilt, sizeof(rebuilt), rebuilt_size));

	ASSERT_EQ(rebuilt_size, fixture_size);
	EXPECT_EQ(std::memcmp(rebuilt, p_fixture, fixture_size), 0)
		<< "the shipped boot probe diverged from a fresh encode — "
		   "regenerate it with: zircon_bcn_bake --synthesize_boot_checker";

	delete[] p_fixture;

	env.shutdown();
	delete &env;
}

		#endif
	#endif
#endif
