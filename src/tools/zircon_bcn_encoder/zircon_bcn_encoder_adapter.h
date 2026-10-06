#pragma once

// zircon_bcn_encoder_adapter.h — the HOUSE-side adapter between
// zircon.core's texture bake (the injected zircon_texture_bcn_encode_
// level_fn seam — zircon.core never names a third-party library) and
// the vendored single-file BCn encoders living beside this file. TWO
// kinds of callers, both TOOL-side per the owner's rule (the vendored
// encoder is never engine/runtime code):
//   - the zircon_bcn_bake host tool (this directory's CMake project —
//      it compiles the vendored .cpps as regular sources);
//   - the unit tests (src/engine/tests/zircon_unit_tests_texture_bcn.cpp
//      — the test TU #includes the vendored .cpps inside its Debug-only
//      guard, so the encoder exists in game.ktk only where the tests
//      run; the runtime render path never encodes OR decodes).
//
// PROVENANCE (byte-pristine vendored files, never edited — the README
// alongside carries the full notes):
//   rgbcx.{h,cpp} + rgbcx_table4{,_small}.h — BC1/BC3/BC4/BC5 encode,
//     github.com/richgel999/bc7enc_rdo @ b9438627eef73a1157e84201b6fa6eb2
//     ffd6d9f0, dual MIT / public-domain (LICENSE.bc7enc_rdo)
//   bc7enc.{h,cpp} — BC7 encode, same repo + license
//   bc7decomp.{h,cpp} — BC7 decode (the encoder lib's own decoder),
//     same repo + license
//   bcdec.h — BC1/BC3/BC5 (and BC7/BC6H) decode, single header,
//     github.com/iOrange/bcdec @ 80859ed3b7afb1c527a2a99d70c61457bea72d0c,
//     dual MIT / public-domain (LICENSE.bcdec)
//
// THE QUALITY TIERS (fixed parameter sets — determinism is the
// contract; raise quality by raising the tier, never by ad-hoc
// parameters at the call site):
//   kFast:    BC7 uber 0 / 16 partitions / linear weights; rgbcx level 5
//   kDefault: BC7 uber 1 / 64 partitions / perceptual; rgbcx level 10
//             (the bake default — the near-lossless albedo tier)
//   kHigh:    BC7 uber 2 / 64 partitions / perceptual; rgbcx level 18
// BC5 maps no tier parameter (rgbcx's BC5 path is a single fixed-shape
// encoder; the tier is recorded in the .bcn header regardless).
//
// THREADING: rgbcx::init / bc7enc_compress_block_init fill library-global
// tables, so zircon_bcn_encoder_initialize is a process-wide table
// setup — call it single-threaded before the first encode (the bake is
// single-threaded by design; re-initialization with the same tier is
// idempotent).

#include "../../core/zircon_texture_bcn.h"

#include "rgbcx.h"
#include "bc7enc.h"
#include "bc7decomp.h"
#include "bcdec.h"

#include <kotek.core.defines_dependent.message/include/kotek_core_defines_dependent_message.h>

// the quality tier (recorded into every baked .bcn header)
enum class eZirconBcnQuality : kotek::uint8_t
{
	kFast = 0,
	kDefault = 1,
	kHigh = 2,
	kEndOfEnum
};

// the adapter's per-caller context (a member/local of the caller — no
// statics, rule 1a); zircon_bcn_encoder_encode_level receives it as the
// seam's void* user pointer
struct zircon_bcn_encoder_context_t
{
	bool m_initialized;
	eZirconBcnQuality m_quality;
	bc7enc_compress_block_params m_bc7_params;
	kotek::uint32_t m_rgbcx_level;
};

// fills the context for the tier and arms the vendored libraries'
// global tables (single-threaded contract — see the banner)
inline void zircon_bcn_encoder_initialize(
	zircon_bcn_encoder_context_t& context,
	eZirconBcnQuality quality) noexcept
{
	context.m_quality = quality;

	bc7enc_compress_block_params_init(&context.m_bc7_params);

	switch (quality)
	{
	case eZirconBcnQuality::kFast:
		context.m_bc7_params.m_max_partitions = 16;
		context.m_bc7_params.m_uber_level = 0;
		context.m_bc7_params.m_perceptual = false;
		context.m_rgbcx_level = 5;
		break;
	case eZirconBcnQuality::kHigh:
		context.m_bc7_params.m_max_partitions = BC7ENC_MAX_PARTITIONS;
		context.m_bc7_params.m_uber_level = 2;
		context.m_bc7_params.m_perceptual = true;
		context.m_rgbcx_level = 18;
		break;
	case eZirconBcnQuality::kDefault:
	default:
		context.m_bc7_params.m_max_partitions = BC7ENC_MAX_PARTITIONS;
		context.m_bc7_params.m_uber_level = 1;
		context.m_bc7_params.m_perceptual = true;
		context.m_rgbcx_level = 10;
		break;
	}

	// the vendored table setup (idempotent; the BC1 endpoint mode is the
	// cross-vendor ideal — the encode must not favor one GPU vendor's
	// hardware decoder quirks)
	rgbcx::init(rgbcx::bc1_approx_mode::cBC1Ideal);
	bc7enc_compress_block_init();

	context.m_initialized = true;
}

// THE encode implementation matching zircon_texture_bcn_encode_level_fn:
// p_user is the initialized zircon_bcn_encoder_context_t. Encodes the
// level's 4x4 blocks row-major with the tier's fixed parameters.
// kBC6H fails loudly (the documented deferral — no license-clean
// single-file BC6H encoder exists).
inline bool zircon_bcn_encoder_encode_level(void* p_user,
	const kotek::uint8_t* p_rgba_pixels, kotek::uint32_t width,
	kotek::uint32_t height, eZirconTextureBcnFormat format,
	kotek::uint8_t* p_out_blocks, kotek::size_t out_block_capacity) noexcept
{
	const zircon_bcn_encoder_context_t* p_context =
		static_cast<const zircon_bcn_encoder_context_t*>(p_user);

	if (p_context == nullptr || p_context->m_initialized == false ||
		p_rgba_pixels == nullptr || p_out_blocks == nullptr)
	{
		KOTEK_MESSAGE_ERROR(
			"[bcn_encoder] encode: a null/uninitialized context or null "
			"pixels/output");
		return false;
	}

	if (format == eZirconTextureBcnFormat::kBC6H)
	{
		KOTEK_MESSAGE_ERROR(
			"[bcn_encoder] BC6H encoding is DEFERRED this phase (the "
			"plan's HDR-skies slot — no license-clean single-file BC6H "
			"encoder exists to vendor); bake HDR as BC7 or defer the "
			"texture");
		return false;
	}

	const kotek::uint32_t block_bytes =
		zircon_texture_bcn_block_bytes(format);

	const kotek::uint32_t blocks_x = (width + 3) / 4;
	const kotek::uint32_t blocks_y = (height + 3) / 4;

	if (out_block_capacity <
		static_cast<kotek::size_t>(blocks_x) * blocks_y * block_bytes)
	{
		KOTEK_MESSAGE_ERROR(
			"[bcn_encoder] encode: the output capacity is short for "
			"{}x{} ({} blocks of {} bytes)",
			width, height, blocks_x * blocks_y, block_bytes);
		return false;
	}

	for (kotek::uint32_t block_y = 0; block_y < blocks_y; ++block_y)
	{
		for (kotek::uint32_t block_x = 0; block_x < blocks_x; ++block_x)
		{
			// the block's 16 texels, row-major (bake-legal dimensions
			// are multiples of 4, so no edge clamp is needed; the clamp
			// mirrors the box filter's for robustness)
			kotek::uint8_t texels[64];

			for (kotek::uint32_t y = 0; y < 4; ++y)
			{
				kotek::uint32_t src_y = block_y * 4 + y;
				if (src_y >= height)
					src_y = height - 1;

				for (kotek::uint32_t x = 0; x < 4; ++x)
				{
					kotek::uint32_t src_x = block_x * 4 + x;
					if (src_x >= width)
						src_x = width - 1;

					const kotek::uint8_t* p_texel =
						p_rgba_pixels + (src_y * width + src_x) * 4;
					kotek::uint8_t* p_out = texels + (y * 4 + x) * 4;

					p_out[0] = p_texel[0];
					p_out[1] = p_texel[1];
					p_out[2] = p_texel[2];
					p_out[3] = p_texel[3];
				}
			}

			kotek::uint8_t* p_block = p_out_blocks +
				(block_y * blocks_x + block_x) * block_bytes;

			switch (format)
			{
			case eZirconTextureBcnFormat::kBC1:
				// opaque masks — the 3-color (1-bit alpha) path stays off
				rgbcx::encode_bc1(p_context->m_rgbcx_level, p_block, texels,
					false, false);
				break;
			case eZirconTextureBcnFormat::kBC3:
				rgbcx::encode_bc3(p_context->m_rgbcx_level, p_block, texels);
				break;
			case eZirconTextureBcnFormat::kBC5:
				// tangent-space normals: the two channels travel
				// independently (R -> chan0, G -> chan1)
				rgbcx::encode_bc5(p_block, texels, 0, 1, 4);
				break;
			case eZirconTextureBcnFormat::kBC7:
				// the upstream contract is quirky: the return value
				// distinguishes the ALPHA path (true — m_force_alpha or
				// a texel with alpha < 255) from the opaque one (false)
				// — an OPAQUE block returns false ON SUCCESS
				// (handle_opaque_block already wrote the block). It is
				// NOT a success indicator; the table-init assert inside
				// guards the real precondition
				// (bc7enc_compress_block_init ran at initialize)
				bc7enc_compress_block(
					p_block, texels, &p_context->m_bc7_params);
				break;
			default:
				KOTEK_MESSAGE_ERROR(
					"[bcn_encoder] encode: an out-of-enum format value {}",
					static_cast<kotek::uint32_t>(format));
				return false;
			}
		}
	}

	return true;
}

// THE decode seam — the TEST-ONLY proof path (the PSNR quality pins;
// the runtime never decodes — the plan's hard rule). BC7 decodes
// through the encoder lib's own bc7decomp, BC1/BC3/BC5 through bcdec.
// p_out_rgba receives width*height*4 bytes. kBC6H fails (the deferral).
inline bool zircon_bcn_encoder_decode_level(eZirconTextureBcnFormat format,
	const kotek::uint8_t* p_blocks, kotek::uint32_t width,
	kotek::uint32_t height, kotek::uint8_t* p_out_rgba) noexcept
{
	if (p_blocks == nullptr || p_out_rgba == nullptr)
		return false;

	const kotek::uint32_t block_bytes =
		zircon_texture_bcn_block_bytes(format);

	const kotek::uint32_t blocks_x = (width + 3) / 4;
	const kotek::uint32_t blocks_y = (height + 3) / 4;

	const int pitch = static_cast<int>(width * 4);

	for (kotek::uint32_t block_y = 0; block_y < blocks_y; ++block_y)
	{
		for (kotek::uint32_t block_x = 0; block_x < blocks_x; ++block_x)
		{
			const kotek::uint8_t* p_block =
				p_blocks + (block_y * blocks_x + block_x) * block_bytes;

			kotek::uint8_t* p_out_block =
				p_out_rgba + (block_y * 4 * width + block_x * 4) * 4;

			switch (format)
			{
			case eZirconTextureBcnFormat::kBC1:
				bcdec_bc1(p_block, p_out_block, pitch);
				break;
			case eZirconTextureBcnFormat::kBC3:
				bcdec_bc3(p_block, p_out_block, pitch);
				break;
			case eZirconTextureBcnFormat::kBC5:
			{
				// bcdec's BC5 writes a TWO-CHANNEL (RG8) buffer — the
				// pixelSize-2 contract (pinned against the vendored
				// test.c's `bcdec_bc5(src, dst, 4*2)`) — unlike the
				// RGBA4 of the other formats: decode into an RG scratch
				// at the 2-byte pitch, then scatter into the RGBA
				// output (B/A stay untouched — the callers compare the
				// RG channels only)
				kotek::uint8_t rg_scratch[4 * 4 * 2];

				bcdec_bc5(p_block, rg_scratch, 4 * 2);

				for (kotek::uint32_t y = 0; y < 4; ++y)
				{
					for (kotek::uint32_t x = 0; x < 4; ++x)
					{
						kotek::uint8_t* p_out =
							p_out_block + y * pitch + x * 4;

						p_out[0] = rg_scratch[(y * 4 + x) * 2 + 0];
						p_out[1] = rg_scratch[(y * 4 + x) * 2 + 1];
					}
				}
				break;
			}
			case eZirconTextureBcnFormat::kBC7:
			{
				// bc7decomp emits a packed 16-pixel block; scatter it
				// into the image at the pitch
				bc7decomp::color_rgba decoded[16];

				if (bc7decomp::unpack_bc7(p_block, decoded) == false)
					return false;

				for (kotek::uint32_t y = 0; y < 4; ++y)
				{
					std::memcpy(p_out_block + y * pitch,
						decoded + y * 4, 16);
				}
				break;
			}
			default:
				return false;
			}
		}
	}

	return true;
}
