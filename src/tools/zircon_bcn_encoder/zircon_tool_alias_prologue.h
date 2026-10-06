#pragma once

// zircon_tool_alias_prologue.h — the lowercase-alias surface for
// STANDALONE host-tool compiles of zircon engine sources (task Z24 B4).
// House rule 6: zircon code speaks the lowercase aliases (kotek::...),
// which kotek.core/include/kotek_core.h defines — but that header is the
// whole core umbrella (console/filesystem/os/window/input/ecs...), a
// surface a one-file host tool neither needs nor can compile (the
// zircon_kpacker's CMake comment documents exactly this). The engine
// build never needs this file (the kotek PCH carries the umbrella into
// every TU); the zircon_bcn_bake tool force-includes it (/FI) ONLY into
// the engine source it compiles (zircon_texture_bcn.cpp) and includes
// it first in its own main. It must provide exactly what the compiled
// zircon headers name: the integer/size twins (types.numerics), the
// static string alias (containers.string) and the namespace alias
// itself (the kotek_core.h form, rename-safe).

#include <kotek.core.types.numerics/include/kotek_core_types_numerics.h>
#include <kotek.core.containers.string/include/kotek_core_containers_string.h>

#ifdef KOTEK_NAMESPACE_KOTEK
namespace KOTEK_NAMESPACE_NAME_LOWERED = KN_KOTEK;
#endif
