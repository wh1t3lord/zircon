#pragma once

// ZIRCON EMBEDDED CONFIGURATION GUARD (task Z12, owner directive 2026-07-23)
//
// Zircon is an embedded-style engine layer: every ktk_* container alias it
// uses MUST resolve to its static (etl-based, no-reallocation) form. That is
// only true when kotek was configured with the embedded library type
// (KOTEK_LIBRARY_TYPE=EMB, the default) AND static containers are enabled.
//
// If you hit this error, configure kotek with:
//   -DKOTEK_LIBRARY_TYPE=EMB -DKOTEK_STD_LIBRARY_STATIC_CONTAINERS=ON
// Dynamic and hybrid categories may stay enabled for kotek itself — zircon
// simply never uses them by discipline (§2 rule 1). For a strict shipping
// configuration where they are NOT AVAILABLE at all, also pass:
//   -DKOTEK_STD_LIBRARY_DYNAMIC_CONTAINERS=OFF
//   -DKOTEK_STD_LIBRARY_HYBRID_CONTAINERS=OFF
//
// This header is force-included (/FI) into every zircon.* target from the
// root CMakeLists; it is self-contained on purpose because directory-scope
// /FI options reach the compiler before the kotek PCH does.

#include <kotek.core.defines.static.cpp/include/kotek_core_defines_static_cpp.h>

#if !defined(KOTEK_USE_LIBRARY_TYPE_EMB)
	#error "[zircon]: embedded configuration required — KOTEK_LIBRARY_TYPE must be EMB so ktk_* aliases resolve to static (etl-based) containers, see zircon/AGENTS.md task Z12"
#endif

#if !defined(KOTEK_USE_STD_LIBRARY_STATIC_CONTAINERS)
	#error "[zircon]: embedded configuration required — KOTEK_STD_LIBRARY_STATIC_CONTAINERS must be ON, see zircon/AGENTS.md task Z12"
#endif
