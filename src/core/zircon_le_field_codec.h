#pragma once

// zircon_le_field_codec.h — the shared little-endian field codec: every
// multi-byte field of every zircon content format (the CSG bake, the
// meshlet bake, the BCn texture bake) is assembled byte-by-byte through
// these helpers, so a layout never depends on the host's struct packing
// or endianness. THE ONE CODEC HOME — never forked (the codec lived in
// zircon_csg_bake.h until task Z24 B4 split it out so pure-content
// modules like zircon_texture_bcn.h can reuse it WITHOUT dragging the
// CSG evaluation chain in — host tools compile those modules
// standalone, where the lowercase-alias umbrella is unavailable).

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>

// std::memcpy in the f64 leg (the pure-POD kernel posture)
#include <cstring>

// ---------------------------------------------------------------------
// the writer legs
// ---------------------------------------------------------------------
inline void zircon_csg_bake_store_u16(
	kotek::uint8_t* p, kotek::uint16_t value) noexcept
{
	p[0] = static_cast<kotek::uint8_t>(value & 0xFFu);
	p[1] = static_cast<kotek::uint8_t>((value >> 8) & 0xFFu);
}

inline void zircon_csg_bake_store_u32(
	kotek::uint8_t* p, kotek::uint32_t value) noexcept
{
	p[0] = static_cast<kotek::uint8_t>(value & 0xFFu);
	p[1] = static_cast<kotek::uint8_t>((value >> 8) & 0xFFu);
	p[2] = static_cast<kotek::uint8_t>((value >> 16) & 0xFFu);
	p[3] = static_cast<kotek::uint8_t>((value >> 24) & 0xFFu);
}

inline void zircon_csg_bake_store_i32(
	kotek::uint8_t* p, kotek::int32_t value) noexcept
{
	zircon_csg_bake_store_u32(p, static_cast<kotek::uint32_t>(value));
}

inline void zircon_csg_bake_store_u64(
	kotek::uint8_t* p, kotek::uint64_t value) noexcept
{
	zircon_csg_bake_store_u32(p, static_cast<kotek::uint32_t>(
								 value & 0xFFFFFFFFull));
	zircon_csg_bake_store_u32(p + 4, static_cast<kotek::uint32_t>(
									 value >> 32));
}

inline void zircon_csg_bake_store_f64(
	kotek::uint8_t* p, double value) noexcept
{
	static_assert(sizeof(double) == sizeof(kotek::uint64_t));
	kotek::uint64_t bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	zircon_csg_bake_store_u64(p, bits);
}

// ---------------------------------------------------------------------
// the reader legs
// ---------------------------------------------------------------------
inline kotek::uint16_t zircon_csg_bake_load_u16(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint16_t>(
		static_cast<kotek::uint16_t>(p[0]) |
		(static_cast<kotek::uint16_t>(p[1]) << 8));
}

inline kotek::uint32_t zircon_csg_bake_load_u32(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint32_t>(p[0]) |
		(static_cast<kotek::uint32_t>(p[1]) << 8) |
		(static_cast<kotek::uint32_t>(p[2]) << 16) |
		(static_cast<kotek::uint32_t>(p[3]) << 24);
}

inline kotek::int32_t zircon_csg_bake_load_i32(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::int32_t>(zircon_csg_bake_load_u32(p));
}

inline kotek::uint64_t zircon_csg_bake_load_u64(
	const kotek::uint8_t* p) noexcept
{
	return static_cast<kotek::uint64_t>(zircon_csg_bake_load_u32(p)) |
		(static_cast<kotek::uint64_t>(zircon_csg_bake_load_u32(p + 4))
			<< 32);
}

inline double zircon_csg_bake_load_f64(const kotek::uint8_t* p) noexcept
{
	const kotek::uint64_t bits = zircon_csg_bake_load_u64(p);
	double value = 0.0;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}
