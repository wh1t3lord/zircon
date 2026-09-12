cmake_minimum_required(VERSION 3.21)
project(zircon_kpacker LANGUAGES CXX)

# Host tool (same structural pattern as src/tools/zircon_shaderpack):
# configured STANDALONE via a nested cmake invocation at engine BUILD time
# (cmake/zircon_kpacker.cmake), so it sets its own standard/CRT and receives
# every kotek path through -D cache vars — it does not inherit the engine
# root's compiler globals. std is allowed in tools (AGENTS.md §2.1
# exemption); the embedded discipline is kept anyway (bounded scratch,
# streaming IO, no exceptions).
#
# Unlike the shaderpack (self-contained), this tool REUSES the kotek .kpack
# module: the shared encoder kpack_write_file + the reader ktkFileSystem_Pack
# are compiled straight from their module sources (the include dirs below
# mirror what the engine build gives that module), so writer, reader and
# tool can never drift on the format.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# static CRT: the vcpkg deps are the x64-windows-static triplet and the
# engine itself is /MTd in Debug — one CRT flavour everywhere. The compile
# options mirror kotek/cmake/windows/compiler_globals.cmake (the engine adds
# them to every target; fmt's unicode path hard-requires /utf-8)
if (MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_compile_options(/utf-8 /Zc:__cplusplus /FS)
endif()

# per-CONFIG parity with the engine root (zircon CMakeLists.txt): the
# generated kotek preprocessors header carries only configure-time flags;
# KOTEK_DEBUG is a compile definition there and must match here (the reader's
# debug discipline asserts on it)
add_compile_definitions($<$<CONFIG:Debug>:KOTEK_DEBUG>)

# the UNICODE trio (kotek.core.defines.static.string's KOTEK_OUTPUT_FLAGS —
# ON in every engine tree today); KOTEK_USE_UNICODE in the generated string
# header is gated on it at TU compile time
add_compile_definitions(UNICODE _UNICODE KOTEK_UNICODE)

# the engine passes these as compile definitions from
# kotek.core.defines.static.cpp's KOTEK_OUTPUT_FLAGS: they gate the
# KUN_*/kun_* namespace-macro blocks inside the generated preprocessors
# header. The standalone tool replicates the set the pack module's include
# chain tests (api/types/containers/filesystem.path/log/format). The names
# themselves always come from the generated header, so this stays correct
# under a cmake namespace rename.
add_compile_definitions(
	KOTEK_NAMESPACE_KOTEK
	KOTEK_NAMESPACE_CORE
	KOTEK_NAMESPACE_KTK
	KOTEK_NAMESPACE_FILESYSTEM
)

# more of the engine's compile-flag surface the generated headers branch on:
# the module dev type (STATIC/SHARED — KOTEK_MODULE_EXPORT resolution in
# kotek_core_defines_static_os.h) arrives from the driver because it mirrors
# the engine tree's KOTEK_DEVELOPMENT_TYPE; the console window constant is
# unconditional in the engine (its #ifndef static_asserts otherwise)
if ("${ZIRCON_KOTEK_DEVELOPMENT_TYPE}" STREQUAL "")
	message(FATAL_ERROR
		"[zircon_kpacker]: ZIRCON_KOTEK_DEVELOPMENT_TYPE is empty — the "
		"driver (cmake/zircon_kpacker.cmake) passes the engine tree's "
		"KOTEK_DEVELOPMENT_TYPE")
endif()
add_compile_definitions(
	KOTEK_DEVELOPMENT_TYPE_${ZIRCON_KOTEK_DEVELOPMENT_TYPE}
	KOTEK_WINDOW_CONSOLE_STRING_VIEW_SIZE
)

# the backend/library selectors the generated preprocessors header's #ifdef
# branches translate into KOTEK_USE_* form (kotek.core.defines.static.cpp's
# KOTEK_OUTPUT_FLAGS is the engine's source of truth — replicated for the
# standalone tool; the generated headers #error/static_assert LOUDLY on a
# missing one, never silently miscompile):
#   KOTEK_STD_LIBRARY            -> KOTEK_USE_STD_LIBRARY (the casting header's
#                                   implementation branch)
#   KOTEK_FILESYSTEM_TYPE_PACK   -> KOTEK_USE_FILESYSTEM_TYPE_PACK (the pack
#                                   backend is compiled in)
#   KOTEK_FILESYSTEM_TYPE_NATIVE -> KOTEK_USE_FILESYSTEM_TYPE_NATIVE
add_compile_definitions(
	KOTEK_STD_LIBRARY
	KOTEK_FILESYSTEM_TYPE_PACK
	KOTEK_FILESYSTEM_TYPE_NATIVE
)

# the platform selector (kotek.core.os's "unknown platform" #error gate;
# -> KOTEK_USE_PLATFORM_WINDOWS in the generated os header) — the tool is
# Windows-first like the whole stack (the pack module itself calls
# _fseeki64/_ftelli64)
add_compile_definitions(KOTEK_PLATFORM_WINDOWS)

# the log backend (KOTEK_MESSAGE_* resolves Get_LoggerMain() through it):
# mirrors the engine tree's KOTEK_LOG_LIBRARY (empty = SPDLOG, kotek's
# default; the gfxdev tree runs SPDLOG, the main tree CUSTOM — the tool's
# logger shim compiles the matching branch). BOOST is rejected: its
# BOOST_LOG_TRIVIAL route needs the boost log linkage nobody has needed.
if ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "")
	set(ZIRCON_KOTEK_LOG_LIBRARY "SPDLOG")
endif()

if ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "SPDLOG")
	add_compile_definitions(KOTEK_LOG_LIBRARY_SPDLOG)
	find_package(spdlog CONFIG REQUIRED)
	set(ZIRCON_KPACKER_LOG_BACKEND_LIBS spdlog::spdlog_header_only)
elseif ("${ZIRCON_KOTEK_LOG_LIBRARY}" STREQUAL "CUSTOM")
	add_compile_definitions(KOTEK_LOG_LIBRARY_CUSTOM)
	set(ZIRCON_KPACKER_LOG_BACKEND_LIBS "")
else()
	message(FATAL_ERROR
		"[zircon_kpacker]: the tool's logger shim covers the SPDLOG and "
		"CUSTOM log backends; '${ZIRCON_KOTEK_LOG_LIBRARY}' needs the boost "
		"log plumbing added")
endif()

foreach(required_var ZIRCON_KOTEK_SOURCE_DIR ZIRCON_KOTEK_GENERATED_DIR)
	if ("${${required_var}}" STREQUAL "")
		message(FATAL_ERROR
			"[zircon_kpacker]: ${required_var} is empty — this project is "
			"configured by the engine build (cmake/zircon_kpacker.cmake), "
			"not by hand")
	endif()
endforeach()

if (NOT EXISTS "${ZIRCON_KOTEK_GENERATED_DIR}/kotek.core.defines.static.cpp/include/kotek_std_preprocessors.h")
	message(FATAL_ERROR
		"[zircon_kpacker]: the generated kotek preprocessors header is "
		"missing under '${ZIRCON_KOTEK_GENERATED_DIR}' — configure the "
		"engine tree first (the header is written at configure time)")
endif()

# zstd + zlib: the .kpack codecs — discovered from the same vcpkg tree the
# engine uses (the driver passes the toolchain file + static triplet)
find_package(zstd CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

set(ZIRCON_KOTEK_PACK_MODULE_DIR
	"${ZIRCON_KOTEK_SOURCE_DIR}/kotek.core.filesystem.pack")

add_executable(${PROJECT_NAME}
	"zircon_kpacker.cpp"
	# THE shared encoder (B2a's kpack_write_file) and the real reader (the
	# verify command mounts through it) — compiled from the module sources,
	# never copied
	"${ZIRCON_KOTEK_PACK_MODULE_DIR}/src/kotek_kpack_writer.cpp"
	"${ZIRCON_KOTEK_PACK_MODULE_DIR}/src/kotek_filesystem_pack.cpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	# module headers: <kotek.core.filesystem.pack/include/kotek_kpack_format.h>
	"${ZIRCON_KOTEK_SOURCE_DIR}"
	# the configure-time generated headers (kotek_std_preprocessors.h per
	# defines module): <build>/kotek/src in the engine tree
	"${ZIRCON_KOTEK_GENERATED_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	zstd::libzstd_static
	ZLIB::ZLIB
	${ZIRCON_KPACKER_LOG_BACKEND_LIBS}
)
