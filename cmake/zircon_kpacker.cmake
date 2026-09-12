# zircon_kpacker.cmake — builds the .kpack host tool
# (src/tools/zircon_kpacker) at BUILD time through a nested standalone
# configure — the exact pattern cmake/zircon_slang_shaders.cmake uses for
# zircon_shaderpack (per-config stamp, nested -S/-B configure, then a normal
# cmake --build of the nested tree).
#
# Unlike the self-contained shaderpack, zircon_kpacker compiles kotek's
# kotek.core.filesystem.pack module sources (THE shared encoder
# kpack_write_file + the runtime reader ktkFileSystem_Pack), so the nested
# configure receives:
#   ZIRCON_KOTEK_SOURCE_DIR    kotek's src/ (module headers/sources)
#   ZIRCON_KOTEK_GENERATED_DIR the engine build tree's kotek/src (the
#                              configure-time generated kotek_std_preprocessors.h
#                              of every defines module lives there)
# plus the vcpkg toolchain/triplet for the zstd/zlib discovery.
# Included from the root CMakeLists.txt.

set(ZIRCON_KPACKER_BUILD_DIR "${CMAKE_BINARY_DIR}/zircon_tools_kpacker")
set(ZIRCON_KPACKER_EXE
	"${ZIRCON_KPACKER_BUILD_DIR}/$<CONFIG>/zircon_kpacker.exe")
# per-config stamp: a Debug-built stamp must not satisfy a Release build
set(ZIRCON_KPACKER_STAMP
	"${ZIRCON_KPACKER_BUILD_DIR}/zircon_kpacker-$<CONFIG>.stamp")

set(ZIRCON_KPACKER_TOOL_SOURCES
	"${CMAKE_SOURCE_DIR}/src/tools/zircon_kpacker/zircon_kpacker.cpp"
	"${CMAKE_SOURCE_DIR}/src/tools/zircon_kpacker/CMakeLists.txt")

set(ZIRCON_KPACKER_PACK_MODULE_DIR
	"${CMAKE_SOURCE_DIR}/kotek/src/kotek.core.filesystem.pack")

add_custom_command(
	OUTPUT "${ZIRCON_KPACKER_STAMP}"
	COMMAND ${CMAKE_COMMAND}
		-S "${CMAKE_SOURCE_DIR}/src/tools/zircon_kpacker"
		-B "${ZIRCON_KPACKER_BUILD_DIR}"
		"-DZIRCON_KOTEK_SOURCE_DIR=${CMAKE_SOURCE_DIR}/kotek/src"
		"-DZIRCON_KOTEK_GENERATED_DIR=${CMAKE_BINARY_DIR}/kotek/src"
		"-DZIRCON_KOTEK_DEVELOPMENT_TYPE=${KOTEK_DEVELOPMENT_TYPE}"
		"-DZIRCON_KOTEK_LOG_LIBRARY=${KOTEK_LOG_LIBRARY}"
		"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/kotek/vcpkg/scripts/buildsystems/vcpkg.cmake"
		"-DVCPKG_TARGET_TRIPLET=x64-windows-static"
	COMMAND ${CMAKE_COMMAND}
		--build "${ZIRCON_KPACKER_BUILD_DIR}"
		--config $<CONFIG>
	COMMAND ${CMAKE_COMMAND} -E touch "${ZIRCON_KPACKER_STAMP}"
	DEPENDS
		${ZIRCON_KPACKER_TOOL_SOURCES}
		# the compiled-in kotek module sources + the format spec: a writer/
		# reader change must rebuild the tool (never drift on the format)
		"${ZIRCON_KPACKER_PACK_MODULE_DIR}/src/kotek_kpack_writer.cpp"
		"${ZIRCON_KPACKER_PACK_MODULE_DIR}/src/kotek_filesystem_pack.cpp"
		"${ZIRCON_KPACKER_PACK_MODULE_DIR}/include/kotek_kpack_format.h"
		"${ZIRCON_KPACKER_PACK_MODULE_DIR}/include/kotek_filesystem_pack.h"
		# the configure-time flag set the tool compiles the module sources
		# under (capacities, backends — content changes on reconfigure)
		"${CMAKE_BINARY_DIR}/kotek/src/kotek.core.defines.static.cpp/include/kotek_std_preprocessors.h"
	COMMENT "building zircon_kpacker (host tool)"
	VERBATIM
)

# ALL: the tool rides the normal `cmake --build` (the same way the shader
# pipeline's zircon_shaders target does) — no engine target depends on it
# (the engine never runs the tool; users and CI do)
add_custom_target(zircon_kpacker_tool ALL DEPENDS "${ZIRCON_KPACKER_STAMP}")
set_target_properties(zircon_kpacker_tool PROPERTIES FOLDER "engine/tools")
