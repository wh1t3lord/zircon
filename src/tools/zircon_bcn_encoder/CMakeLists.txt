cmake_minimum_required(VERSION 3.21)
project(zircon_bcn_bake LANGUAGES CXX)

# Host tool (the zircon_kpacker structural pattern): configured STANDALONE
# via a nested cmake invocation at engine BUILD time
# (cmake/zircon_bcn_bake.cmake), so it sets its own standard/CRT and
# receives every kotek path through -D cache vars — it does not inherit
# the engine root's compiler globals. std is allowed in tools (AGENTS.md
# §2.1 exemption); the embedded discipline is kept anyway (bounded
# reads, no exceptions).
#
# The tool compiles the ENGINE's texture format/bake sources
# (../../core/zircon_texture_bcn.cpp — the kpacker precedent of
# compiling the shared module sources straight from the tree: bake,
# loader and tool can never drift on the format) plus the vendored
# single-file BCn encoders (byte-pristine — see README.md). No kotek
# link edges: the bake driver is pure content (no filesystem, no
# kpack); packing the output is zircon_kpacker's job.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# static CRT: the vcpkg deps are the x64-windows-static triplet and the
# engine itself is /MTd in Debug — one CRT flavour everywhere. The compile
# options mirror kotek/cmake/windows/compiler_globals.cmake
if (MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options(/utf-8 /Zc:__cplusplus /FS)
endif()

# per-CONFIG parity with the engine root (zircon CMakeLists.txt): the
# generated kotek preprocessors header carries only configure-time flags;
# KOTEK_DEBUG is a compile definition there and must match here
add_compile_definitions($<$<CONFIG:Debug>:KOTEK_DEBUG>)

# the UNICODE trio (kotek.core.defines.static.string's KOTEK_OUTPUT_FLAGS)
add_compile_definitions(UNICODE _UNICODE KOTEK_UNICODE)

# the namespace selectors the generated preprocessors header's KUN_*/kun_*
# blocks gate on (the kpacker replicates the same set — see its
# CMakeLists for the rename-safety argument)
add_compile_definitions(
	KOTEK_NAMESPACE_KOTEK
	KOTEK_NAMESPACE_CORE
	KOTEK_NAMESPACE_KTK
	KOTEK_NAMESPACE_FILESYSTEM
)

# the module dev type (KOTEK_MODULE_EXPORT resolution) arrives from the
# driver; the console window constant is unconditional in the engine
if ("${ZIRCON_KOTEK_DEVELOPMENT_TYPE}" STREQUAL "")
	message(FATAL_ERROR
		"[zircon_bcn_bake]: ZIRCON_KOTEK_DEVELOPMENT_TYPE is empty — the "
		"driver (cmake/zircon_bcn_bake.cmake) passes the engine tree's "
		"KOTEK_DEVELOPMENT_TYPE")
endif()
add_compile_definitions(
	KOTEK_DEVELOPMENT_TYPE_${ZIRCON_KOTEK_DEVELOPMENT_TYPE}
	KOTEK_WINDOW_CONSOLE_STRING_VIEW_SIZE
)

# the backend/library selectors the generated headers translate into
# KOTEK_USE_* form (the kpacker's set — the engine's source of truth is
# kotek.core.defines.static.cpp's KOTEK_OUTPUT_FLAGS): the std casting
# branch and the filesystem backends the containers' include chain tests
add_compile_definitions(
	KOTEK_STD_LIBRARY
	KOTEK_FILESYSTEM_TYPE_PACK
	KOTEK_FILESYSTEM_TYPE_NATIVE
)

# the platform selector (kotek.core.os's "unknown platform" #error gate)
add_compile_definitions(KOTEK_PLATFORM_WINDOWS)

# the log backend (KOTEK_MESSAGE_* resolves Get_LoggerMain() through it):
# mirrors the engine tree's KOTEK_LOG_LIBRARY (empty = SPDLOG, kotek's
# default; the main tree runs CUSTOM, gfxdev SPDLOG — the tool's logger
# shim compiles the matching branch)
if ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "")
	set(ZIRCON_KOTEK_LOG_LIBRARY "SPDLOG")
endif()

if ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "SPDLOG")
	add_compile_definitions(KOTEK_LOG_LIBRARY_SPDLOG)
	find_package(spdlog CONFIG REQUIRED)
	set(ZIRCON_BCN_BAKE_LOG_BACKEND_LIBS spdlog::spdlog_header_only)
elseif ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "CUSTOM")
	add_compile_definitions(KOTEK_LOG_LIBRARY_CUSTOM)
	set(ZIRCON_BCN_BAKE_LOG_BACKEND_LIBS "")
else()
	message(FATAL_ERROR
		"[zircon_bcn_bake]: the tool's logger shim covers the SPDLOG and "
		"CUSTOM log backends; '${ZIRCON_KOTEK_LOG_LIBRARY}' needs the boost "
		"log plumbing added")
endif()

foreach(required_var ZIRCON_KOTEK_SOURCE_DIR ZIRCON_KOTEK_GENERATED_DIR)
	if ("${${required_var}}" STREQUAL "")
		message(FATAL_ERROR
			"[zircon_bcn_bake]: ${required_var} is empty — this project is "
			"configured by the engine build (cmake/zircon_bcn_bake.cmake), "
			"not by hand")
	endif()
endforeach()

if (NOT EXISTS "${ZIRCON_KOTEK_GENERATED_DIR}/kotek.core.defines.static.cpp/include/kotek_std_preprocessors.h")
	message(FATAL_ERROR
		"[zircon_bcn_bake]: the generated kotek preprocessors header is "
		"missing under '${ZIRCON_KOTEK_GENERATED_DIR}' — configure the "
		"engine tree first (the header is written at configure time)")
endif()

# the bake driver includes kotek's static-container aliases
# (static_cstring for the entry names) — the etl headers come from the
# same vcpkg tree the engine uses (the driver passes the toolchain file +
# static triplet)
find_package(etl CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
	"zircon_bcn_bake.cpp"
	# the engine's format + bake driver, compiled FROM SOURCE (the
	# kpacker pattern — never copied)
	"../../core/zircon_texture_bcn.cpp"
	# the vendored single-file encoders (byte-pristine; README.md pins
	# the upstream commits + licenses)
	"rgbcx.cpp"
	"bc7enc.cpp"
	"bc7decomp.cpp"
)

# the vendored TUs are upstream code at the engine's /W4+: the house
# keeps them byte-pristine (never patched for warnings) and silences the
# warning surface at the compile flag instead (the ImGuizmo precedent's
# spirit — upstream stays upstream)
set_source_files_properties("rgbcx.cpp" "bc7enc.cpp" "bc7decomp.cpp"
	PROPERTIES COMPILE_OPTIONS "/W0")

# the compiled-in engine source speaks the lowercase aliases
# (kotek::...) — the engine resolves them through the kotek PCH; the
# standalone tool force-includes the minimal alias prologue instead
# (see the prologue's banner — the whole-core umbrella is NOT
# compilable here, the zircon_kpacker CMake comment documents why)
set_source_files_properties("../../core/zircon_texture_bcn.cpp"
	PROPERTIES COMPILE_OPTIONS
		"/FI${CMAKE_CURRENT_SOURCE_DIR}/zircon_tool_alias_prologue.h")

target_include_directories(${PROJECT_NAME} PRIVATE
	# module headers: <kotek.core.api/include/kotek_api.h>
	"${ZIRCON_KOTEK_SOURCE_DIR}"
	# the configure-time generated headers (kotek_std_preprocessors.h)
	"${ZIRCON_KOTEK_GENERATED_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	etl::etl
	${ZIRCON_BCN_BAKE_LOG_BACKEND_LIBS}
)
