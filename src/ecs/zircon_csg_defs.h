#pragma once

// zircon_csg_defs.h — the precision configuration, scalar types and
// capacity constants of the brush-based CSG (task Z25, phase A1).
//
// One scalar type drives BOTH storage (the components) and evaluation
// (zircon_csg_evaluate.h), selected by the preprocessor (the plan's
// "precision configuration", owner requirement):
//
//   ZIRCON_DEF_CSG_PRECISION = ZIRCON_CSG_PRECISION_F32  (default)
//     the editor/PC path — plane math in float with the epsilon policy
//     below.
//   ZIRCON_DEF_CSG_PRECISION = ZIRCON_CSG_PRECISION_U32F
//     the deterministic embedded/console path — 16.16 fixed-point in a
//     u32 lane (two's-complement signed interpretation, so negative
//     coordinates exist; range ±32768, quantum 1/65536). Every
//     arithmetic op is INTEGER math: identical bits on every platform,
//     CRT and optimization level (no FPU rounding modes, no FMA
//     contraction, no x87 excess precision) — deterministic input =
//     deterministic output, which is what makes game-side BAKING
//     reproducible (bake on PC, run anywhere, identical hashes).
//   ZIRCON_DEF_CSG_PRECISION = ZIRCON_CSG_PRECISION_F64
//     the robustness-debug configuration (development only; slow,
//     precise — validates that the f32/u32f paths do not diverge).
//
// The CMake cache variable ZIRCON_CSG_PRECISION (=F32|U32F|F64, root
// CMakeLists) materializes the define; the #error below guards unknown
// values. The evaluation core (zircon_csg_evaluate.h) is templated on
// the scalar and instantiates all three modes in every build — the
// precision parity and u32f determinism proofs run everywhere; the
// define selects what the COMPONENTS store and what the integration
// evaluates with. Conversions happen at the ECS/json boundaries only
// (scalars serialize as json doubles — exact in every mode, see the
// roundtrip argument on zircon_csg_u32f_to_double).
//
// RANGE CONTRACT (u32f): world coordinates should stay within ±16384
// (brush sizes in meters) so plane-distance dot products (three
// products + two adds) keep their final scalar result inside ±32768.
// Overflow beyond the 16.16 range is NEVER silent and NEVER
// saturating: the op emits a loud warning and the value WRAPS
// (two's-complement, exactly like a hardware integer) — a content
// error must stay visible, not be smoothed over.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>

// std::sqrt / std::fabs for the float/double scalar traits (the
// chunk-pool precedent: plain C math inside the pure-POD kernels)
#include <cmath>

// ------------------------------------------------------------------
// precision configuration
// ------------------------------------------------------------------
#define ZIRCON_CSG_PRECISION_F32 1
#define ZIRCON_CSG_PRECISION_U32F 2
#define ZIRCON_CSG_PRECISION_F64 3

#ifndef ZIRCON_DEF_CSG_PRECISION
	#define ZIRCON_DEF_CSG_PRECISION ZIRCON_CSG_PRECISION_F32
#endif

#if ZIRCON_DEF_CSG_PRECISION != ZIRCON_CSG_PRECISION_F32 &&               \
	ZIRCON_DEF_CSG_PRECISION != ZIRCON_CSG_PRECISION_U32F &&              \
	ZIRCON_DEF_CSG_PRECISION != ZIRCON_CSG_PRECISION_F64
	#error \
		"unknown ZIRCON_DEF_CSG_PRECISION (expected ZIRCON_CSG_PRECISION_F32/U32F/F64; set -DZIRCON_CSG_PRECISION=F32|U32F|F64)"
#endif

// ------------------------------------------------------------------
// capacities (rule 9: named, sized by comment, raised by measurement)
// ------------------------------------------------------------------
// a brush is a convex plane set (the Quake/Valve model): box = 6,
// wedge = 5; 64 leaves headroom for a future N-gon prism (the cylinder
// is box-approximated this phase — see zircon_csg_evaluate.h)
#define ZIRCON_DEF_CSG_MAX_PLANES_PER_BRUSH 64
// convex brush: one face per plane at most
#define ZIRCON_DEF_CSG_MAX_FACES_PER_BRUSH ZIRCON_DEF_CSG_MAX_PLANES_PER_BRUSH
// pristine primitive soup: box 24 verts / wedge 18; 256 leaves headroom
// for a 64-gon prism pair of caps + sides
#define ZIRCON_DEF_CSG_MAX_VERTICES_PER_BRUSH 256
// a convex polygon clipped by a half-plane gains at most one vertex per
// clip: primitive faces (<= 8 this phase) + one full brush clip (64),
// rounded up
#define ZIRCON_DEF_CSG_MAX_POLYGON_VERTICES 80
// the compound's member list (the plan's 128); doubles as the
// evaluate_compound input cap
#define ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND 128
// fragments in flight for one compound: 128 primitives x 64 planes is
// the pathological bound; 8192 covers sane content with headroom — an
// overflow is a loud graceful drop (kError_CapacityPolygons), never a
// silent truncation... the pieces ARE dropped, loudly counted
#define ZIRCON_DEF_CSG_MAX_POLYGONS_PER_EVALUATION 8192
// pre-weld soup vertices / post-weld mesh positions (the plan's 65536):
// 8192 polygons x 8 average vertices; keeps the welded vertex count
// u16-addressable for the A3 quantization bake
#define ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION 65536
// fan triangulation emits (verts - 2) per polygon; triangles <= soup
// vertices, indices = 3 x triangles (the chunk pool's 65536/196608
// proportions)
#define ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION \
	ZIRCON_DEF_CSG_MAX_VERTICES_PER_EVALUATION
#define ZIRCON_DEF_CSG_MAX_INDICES_PER_EVALUATION \
	(3 * ZIRCON_DEF_CSG_MAX_TRIANGLES_PER_EVALUATION)
// transient piece lists while one face is clipped against the other
// brushes: a face splits into at most (planes + 1) convex fragments per
// brush, and fragments continue against later brushes — 256 pieces x
// 4096 scratch vertices covers the pathological unit-test compounds
// with headroom; overflow is the same loud drop class as the soup caps
#define ZIRCON_DEF_CSG_MAX_FRAGMENTS_IN_FLIGHT 256
#define ZIRCON_DEF_CSG_MAX_FRAGMENT_SCRATCH_VERTICES 4096

// ------------------------------------------------------------------
// epsilon policy (named, per mode, SCALED to the brush size): the
// evaluation computes the brush-set bounding-box diagonal D and uses
//   plane_eps = max(relative * D, mode floor)   — the in-front/behind/
//                                               on-plane band
//   weld_eps  = max(relative * D, mode floor)   — the vertex weld
//                                                 cluster radius
//   sliver_area_eps = weld_eps^2                — fragments thinner
//                                                 than a weld cell are
//                                                 dropped, counted
// The relatives are mode-independent; the floors keep tiny brushes
// sane (u32f's floor is a few quanta — the quantum IS its fundamental
// resolution).
// ------------------------------------------------------------------
#define ZIRCON_DEF_CSG_EPS_PLANE_RELATIVE 0.00001 // 1e-5 x diagonal
#define ZIRCON_DEF_CSG_EPS_WELD_RELATIVE 0.0001   // 1e-4 x diagonal

#define ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_F32 0.000001f  // 1e-6
#define ZIRCON_DEF_CSG_EPS_WELD_FLOOR_F32 0.00001f    // 1e-5
#define ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_U32F 4         // quanta (4/65536)
#define ZIRCON_DEF_CSG_EPS_WELD_FLOOR_U32F 8          // quanta (8/65536)
#define ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_F64 0.00000000001  // 1e-11
#define ZIRCON_DEF_CSG_EPS_WELD_FLOOR_F64 0.0000000001    // 1e-10

// the operation flags (zircon_component_csg_primitive::m_operation_flags)
// — bit 0 set = the primitive SUBTRACTS from the compound; clear =
// additive (union, the default)
#define zircon_DEF_CSG_OPERATION_SUBTRACTIVE 0x1

// the primitive kinds (zircon_component_csg_primitive::m_primitive_type);
// kWedge is evaluated exactly (5-plane ramp brush), kCylinder is
// box-approximated this phase (its AABB) — see zircon_csg_evaluate.h
enum class eZirconCsgPrimitiveType : kotek::uint8_t
{
	kBox = 0,
	kWedge,
	kCylinder,
	kCount
};

// the evaluation result (the gltf loader's status-enum pattern: graceful,
// never an assert, user content is not a programmer error)
enum class eZirconCsgEvaluationStatus : kotek::uint8_t
{
	kSuccess = 0,
	// null pointers / zero counts / non-positive dimensions
	kError_InvalidArguments,
	// zero primitives in the compound
	kError_EmptyCompound,
	// more primitives than ZIRCON_DEF_CSG_MAX_PRIMITIVES_PER_COMPOUND
	kError_TooManyPrimitives,
	// a primitive type outside eZirconCsgPrimitiveType
	kError_UnknownPrimitiveType,
	// the fragment soup / weld exceeded the vertex caps (the overflowing
	// fragments were dropped and counted; the mesh holds what fit)
	kError_CapacityVertices,
	// the polygon caps (same loud-degradation contract)
	kError_CapacityPolygons
};

// the evaluated-mesh state of zircon_component_csg::m_mesh_handle
enum class eZirconCsgMeshState : kotek::uint8_t
{
	kUnevaluated = 0,
	kValid,
	kFailed
};

// pool-agnostic evaluated-mesh handle (A2 owns the dynamic vertex/index
// pools; A1 stores the last evaluation's counts so the journal roundtrip
// and the tests observe the evaluation without any render dependency)
struct zircon_csg_mesh_handle_t
{
	kotek::uint32_t m_vertex_count;
	kotek::uint32_t m_triangle_count;
	// bumps per successful evaluation (a stale handle is detectable)
	kotek::uint32_t m_generation;
	// eZirconCsgMeshState
	kotek::uint8_t m_state;
};

// ------------------------------------------------------------------
// the u32f scalar (16.16 fixed-point in a u32 lane)
// ------------------------------------------------------------------
//
// The exactness argument (the bake-reproducibility proof): the value is
// the raw bit pattern; add/sub/neg are exact two's-complement wraps mod
// 2^32; mul accumulates the EXACT 62-bit product in i64 and shifts once
// (arithmetic >>, i.e. floor — C++20 guarantees it); div widens the
// dividend by 2^16 in i64 and truncates toward zero (C++ integer
// division); comparisons are signed integer compares over the same
// bits. No FPU register is touched by add/sub/mul/div/compare, so no
// rounding mode, contraction or excess-precision difference can exist
// between platforms. sqrt goes through IEEE-754 double (correctly
// rounded by the standard — identical on every IEEE platform) and the
// clipper's edge/plane crossings go through the same double hop
// (endpoints and distances convert exactly; the seam vertex is a pure
// function of the edge and plane, independent of clip history — see
// zircon_csg_split_ring) and quantize back through the fixed
// round-half-away rule of from_double. Overflow beyond the 16.16 range
// warns loudly and WRAPS (never saturates — the range contract at the
// top of this file).
struct zircon_csg_scalar_u32f_t
{
	kotek::uint32_t m_raw_bits;
};

// raw-lane views (C++20 two's-complement casts are exact and portable)
inline kotek::int32_t
zircon_csg_u32f_as_signed(kotek::uint32_t raw_bits) noexcept
{
	return static_cast<kotek::int32_t>(raw_bits);
}

inline kotek::uint32_t
zircon_csg_u32f_as_raw(kotek::int32_t value) noexcept
{
	return static_cast<kotek::uint32_t>(value);
}

inline bool zircon_csg_u32f_is_zero(zircon_csg_scalar_u32f_t value
) noexcept
{
	return value.m_raw_bits == 0u;
}

// boundary conversion: the quantum grid is k/65536 with |k| < 2^31 —
// exact in double (k x 2^-16 needs only the mantissa), so
// to_double(from_double(x)) is the identity on the grid and the json
// roundtrip is lossless in every precision mode
inline zircon_csg_scalar_u32f_t
zircon_csg_u32f_from_double(double value) noexcept
{
	// the power-of-two scale is exact; the +-0.5 shift + truncation is
	// round-half-away-from-zero, identical on every IEEE-754 platform
	const double scaled = value * 65536.0;

	// |value| >= 2^47 is outside every sane brush world; guard the
	// double->i64 cast (UB out of range) before the range check
	if (scaled >= 140737488355328.0 || scaled <= -140737488355328.0)
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] from_double far outside the 16.16 range: {}",
			value
		);
		return zircon_csg_scalar_u32f_t{0u};
	}

	const double shifted = scaled + (scaled >= 0.0 ? 0.5 : -0.5);
	const kotek::int64_t quantized =
		static_cast<kotek::int64_t>(shifted);

	if (quantized !=
	    static_cast<kotek::int64_t>(
			static_cast<kotek::int32_t>(quantized)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] from_double overflow (wraps, not clamps): {}",
			value
		);
	}

	return zircon_csg_scalar_u32f_t{zircon_csg_u32f_as_raw(
		static_cast<kotek::int32_t>(quantized))};
}

inline double zircon_csg_u32f_to_double(zircon_csg_scalar_u32f_t value
) noexcept
{
	// int32 -> double is exact, the power-of-two divide is exact
	return static_cast<double>(
			   zircon_csg_u32f_as_signed(value.m_raw_bits)) *
		0.0000152587890625; // 1/65536
}

inline zircon_csg_scalar_u32f_t
zircon_csg_u32f_from_int(kotek::int32_t value) noexcept
{
	const kotek::int64_t shifted =
		static_cast<kotek::int64_t>(value) << 16;

	if (shifted !=
	    static_cast<kotek::int64_t>(
			static_cast<kotek::int32_t>(shifted)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] from_int overflow (wraps, not clamps): {}",
			value
		);
	}

	return zircon_csg_scalar_u32f_t{zircon_csg_u32f_as_raw(
		static_cast<kotek::int32_t>(shifted))};
}

inline zircon_csg_scalar_u32f_t
operator+(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	// exact wrap; the i64 intermediate exists only for the loud check
	const kotek::int64_t sum =
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(left.m_raw_bits)) +
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(right.m_raw_bits));

	if (sum !=
	    static_cast<kotek::int64_t>(static_cast<kotek::int32_t>(sum)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] add overflow (wraps, not clamps)"
		);
	}

	return zircon_csg_scalar_u32f_t{
		left.m_raw_bits + right.m_raw_bits};
}

inline zircon_csg_scalar_u32f_t
operator-(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	const kotek::int64_t difference =
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(left.m_raw_bits)) -
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(right.m_raw_bits));

	if (difference !=
	    static_cast<kotek::int64_t>(
			static_cast<kotek::int32_t>(difference)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] sub overflow (wraps, not clamps)"
		);
	}

	return zircon_csg_scalar_u32f_t{
		left.m_raw_bits - right.m_raw_bits};
}

inline zircon_csg_scalar_u32f_t
operator-(zircon_csg_scalar_u32f_t value) noexcept
{
	if (value.m_raw_bits == 0x80000000u)
	{
		// -(-32768.0) is unrepresentable
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] negate overflow (wraps, not clamps)"
		);
	}

	return zircon_csg_scalar_u32f_t{0u - value.m_raw_bits};
}

inline zircon_csg_scalar_u32f_t
operator*(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	// the exact 62-bit product; one arithmetic shift (floor) — the only
	// rounding a fixed-point mul can have, identical everywhere
	const kotek::int64_t product =
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(left.m_raw_bits)) *
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(right.m_raw_bits));

	const kotek::int64_t result = product >> 16;

	if (result !=
	    static_cast<kotek::int64_t>(
			static_cast<kotek::int32_t>(result)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] mul overflow (wraps, not clamps)"
		);
	}

	return zircon_csg_scalar_u32f_t{zircon_csg_u32f_as_raw(
		static_cast<kotek::int32_t>(result))};
}

inline zircon_csg_scalar_u32f_t
operator/(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	if (right.m_raw_bits == 0u)
	{
		// a zero divisor is a degenerate-brush content error; the
		// deterministic answer is 0 (a loud one — never a crash, user
		// content is not a programmer error)
		KOTEK_MESSAGE_WARNING("[csg][u32f] division by zero -> 0");
		return zircon_csg_scalar_u32f_t{0u};
	}

	const kotek::int64_t dividend =
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(left.m_raw_bits))
		<< 16;

	const kotek::int64_t quotient =
		dividend /
		static_cast<kotek::int64_t>(
			zircon_csg_u32f_as_signed(right.m_raw_bits));

	if (quotient !=
	    static_cast<kotek::int64_t>(
			static_cast<kotek::int32_t>(quotient)))
	{
		KOTEK_MESSAGE_WARNING(
			"[csg][u32f] div overflow (wraps, not clamps)"
		);
	}

	return zircon_csg_scalar_u32f_t{zircon_csg_u32f_as_raw(
		static_cast<kotek::int32_t>(quotient))};
}

// comparisons are signed integer compares over the raw bits — exact
inline bool operator==(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return left.m_raw_bits == right.m_raw_bits;
}

inline bool operator!=(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return left.m_raw_bits != right.m_raw_bits;
}

inline bool operator<(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return zircon_csg_u32f_as_signed(left.m_raw_bits) <
		zircon_csg_u32f_as_signed(right.m_raw_bits);
}

inline bool operator<=(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return zircon_csg_u32f_as_signed(left.m_raw_bits) <=
		zircon_csg_u32f_as_signed(right.m_raw_bits);
}

inline bool operator>(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return zircon_csg_u32f_as_signed(left.m_raw_bits) >
		zircon_csg_u32f_as_signed(right.m_raw_bits);
}

inline bool operator>=(
	zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right
) noexcept
{
	return zircon_csg_u32f_as_signed(left.m_raw_bits) >=
		zircon_csg_u32f_as_signed(right.m_raw_bits);
}

// ------------------------------------------------------------------
// per-scalar traits: the seam the templated evaluation core
// (zircon_csg_evaluate.h) is written against. Only conversions, sqrt,
// abs, the widened dot and the epsilon policy live here — arithmetic
// and comparisons are plain operators on every mode.
// ------------------------------------------------------------------
template <typename Scalar>
struct zircon_csg_scalar_traits; // undefined: the three modes specialize

template <>
struct zircon_csg_scalar_traits<float>
{
	static float zero(void) noexcept { return 0.0f; }
	static float one(void) noexcept { return 1.0f; }
	static float from_double(double value) noexcept
	{
		return static_cast<float>(value);
	}
	static double to_double(float value) noexcept { return value; }
	static float from_int(kotek::int32_t value) noexcept
	{
		return static_cast<float>(value);
	}
	// std math in the pure-POD kernel follows the chunk-pool precedent
	// (zircon_render_chunk_pool.cpp's std::sqrt); kotek's math wrappers
	// are float-VECTOR types and cannot serve a scalar-templated core
	static float sqrt(float value) noexcept { return std::sqrt(value); }
	static float abs(float value) noexcept { return std::fabs(value); }
	static float dot(const float left[3], const float right[3]) noexcept
	{
		return left[0] * right[0] + left[1] * right[1] +
			left[2] * right[2];
	}

	// |a - b| <= eps with the difference computed wide enough that the
	// comparison itself cannot overflow (plain float math here)
	static bool
	difference_within(float left, float right, float eps) noexcept
	{
		return (left >= right ? left - right : right - left) <= eps;
	}

	static float plane_eps_floor(void) noexcept
	{
		return ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_F32;
	}
	static float weld_eps_floor(void) noexcept
	{
		return ZIRCON_DEF_CSG_EPS_WELD_FLOOR_F32;
	}
};

template <>
struct zircon_csg_scalar_traits<double>
{
	static double zero(void) noexcept { return 0.0; }
	static double one(void) noexcept { return 1.0; }
	static double from_double(double value) noexcept { return value; }
	static double to_double(double value) noexcept { return value; }
	static double from_int(kotek::int32_t value) noexcept
	{
		return value;
	}
	static double sqrt(double value) noexcept
	{
		return std::sqrt(value);
	}
	static double abs(double value) noexcept
	{
		return std::fabs(value);
	}
	static double dot(const double left[3], const double right[3]) noexcept
	{
		return left[0] * right[0] + left[1] * right[1] +
			left[2] * right[2];
	}

	static bool
	difference_within(double left, double right, double eps) noexcept
	{
		return (left >= right ? left - right : right - left) <= eps;
	}

	static double plane_eps_floor(void) noexcept
	{
		return ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_F64;
	}
	static double weld_eps_floor(void) noexcept
	{
		return ZIRCON_DEF_CSG_EPS_WELD_FLOOR_F64;
	}
};

template <>
struct zircon_csg_scalar_traits<zircon_csg_scalar_u32f_t>
{
	static zircon_csg_scalar_u32f_t zero(void) noexcept
	{
		return zircon_csg_scalar_u32f_t{0u};
	}
	static zircon_csg_scalar_u32f_t one(void) noexcept
	{
		return zircon_csg_u32f_from_int(1);
	}
	static zircon_csg_scalar_u32f_t from_double(double value) noexcept
	{
		return zircon_csg_u32f_from_double(value);
	}
	static double to_double(zircon_csg_scalar_u32f_t value) noexcept
	{
		return zircon_csg_u32f_to_double(value);
	}
	static zircon_csg_scalar_u32f_t from_int(kotek::int32_t value
	) noexcept
	{
		return zircon_csg_u32f_from_int(value);
	}
	static zircon_csg_scalar_u32f_t
	sqrt(zircon_csg_scalar_u32f_t value) noexcept
	{
		// IEEE-754 double sqrt is correctly rounded by the standard —
		// identical on every IEEE platform — and the quantum round-trip
		// through from_double is the fixed round-half-away rule, so the
		// u32f sqrt stays deterministic (to_double itself is exact)
		if (zircon_csg_u32f_as_signed(value.m_raw_bits) <= 0)
			return zero();

		return zircon_csg_u32f_from_double(
			std::sqrt(zircon_csg_u32f_to_double(value)));
	}
	static zircon_csg_scalar_u32f_t
	abs(zircon_csg_scalar_u32f_t value) noexcept
	{
		// -32768.0 has no positive twin — loud wrap like every op
		if (value.m_raw_bits == 0x80000000u)
		{
			KOTEK_MESSAGE_WARNING(
				"[csg][u32f] abs of the minimum value (wraps)"
			);
			return value;
		}

		return zircon_csg_u32f_as_signed(value.m_raw_bits) < 0
			? -value
			: value;
	}

	// the WIDENED dot: three exact 60-bit products accumulate in i64
	// (the range contract ±16384 keeps them there), one floor shift at
	// the end — integer-exact like mul, with the same loud range guard
	static zircon_csg_scalar_u32f_t
	dot(const zircon_csg_scalar_u32f_t left[3],
		const zircon_csg_scalar_u32f_t right[3]) noexcept
	{
		kotek::int64_t sum = 0;

		for (int axis = 0; axis < 3; ++axis)
		{
			sum += static_cast<kotek::int64_t>(
					   zircon_csg_u32f_as_signed(left[axis].m_raw_bits)) *
				static_cast<kotek::int64_t>(
					zircon_csg_u32f_as_signed(right[axis].m_raw_bits));
		}

		const kotek::int64_t result = sum >> 16;

		if (result !=
		    static_cast<kotek::int64_t>(
				static_cast<kotek::int32_t>(result)))
		{
			KOTEK_MESSAGE_WARNING(
				"[csg][u32f] dot overflow beyond the 16.16 range "
				"(wraps, not clamps — the world exceeds the range "
				"contract)");
		}

		return zircon_csg_scalar_u32f_t{zircon_csg_u32f_as_raw(
			static_cast<kotek::int32_t>(result))};
	}

	// the weld/classification comparison widened to i64: two in-range
	// coordinates can be 65536.0 apart — beyond the 16.16 range a plain
	// subtract would wrap — while the DIFFERENCE is never stored as
	// geometry (comparisons only), so widening is exact and silent
	static bool difference_within(
		zircon_csg_scalar_u32f_t left, zircon_csg_scalar_u32f_t right,
		zircon_csg_scalar_u32f_t eps) noexcept
	{
		const kotek::int64_t difference =
			static_cast<kotek::int64_t>(
				zircon_csg_u32f_as_signed(left.m_raw_bits)) -
			static_cast<kotek::int64_t>(
				zircon_csg_u32f_as_signed(right.m_raw_bits));

		const kotek::uint64_t magnitude =
			difference < 0 ? static_cast<kotek::uint64_t>(-difference)
						   : static_cast<kotek::uint64_t>(difference);

		return magnitude <=
			static_cast<kotek::uint64_t>(static_cast<kotek::uint32_t>(
				zircon_csg_u32f_as_signed(eps.m_raw_bits)));
	}

	static zircon_csg_scalar_u32f_t plane_eps_floor(void) noexcept
	{
		return zircon_csg_scalar_u32f_t{
			ZIRCON_DEF_CSG_EPS_PLANE_FLOOR_U32F};
	}
	static zircon_csg_scalar_u32f_t weld_eps_floor(void) noexcept
	{
		return zircon_csg_scalar_u32f_t{
			ZIRCON_DEF_CSG_EPS_WELD_FLOOR_U32F};
	}
};

// ------------------------------------------------------------------
// the configured scalar (what the components store and the integration
// evaluates with — the tests instantiate all three modes explicitly)
// ------------------------------------------------------------------
#if ZIRCON_DEF_CSG_PRECISION == ZIRCON_CSG_PRECISION_U32F
using zircon_csg_scalar_t = zircon_csg_scalar_u32f_t;
#elif ZIRCON_DEF_CSG_PRECISION == ZIRCON_CSG_PRECISION_F64
using zircon_csg_scalar_t = double;
#else
using zircon_csg_scalar_t = float;
#endif
