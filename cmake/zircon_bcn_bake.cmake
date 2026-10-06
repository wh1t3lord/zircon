# zircon_bcn_bake.cmake — builds the BCn texture bake host tool
# (src/tools/zircon_bcn_encoder) at BUILD time through a nested standalone
# configure — the exact pattern cmake/zircon_kpacker.cmake uses (per-config
# stamp, nested -S/-B configure, then a normal cmake --build of the nested
# tree).
#
# The tool compiles the engine's texture format/bake sources
# (src/core/zircon_texture_bcn.cpp — never copied) plus the vendored
# single-file BCn encoders, so the nested configure receives the same two
# kotek paths as the kpacker driver:
#   ZIRCON_KOTEK_SOURCE_DIR    kotek's src/ (module headers)
#   ZIRCON_KOTEK_GENERATED_DIR the engine build tree's kotek/src (the
#                              configure-time generated kotek_std_preprocessors.h
#                              of every defines module lives there)
# plus the vcpkg toolchain/triplet for the SPDLOG log-backend discovery
# (the CUSTOM backend needs nothing). Included from the root CMakeLists.txt.

set(ZIRCON_BCN_BAKE_BUILD_DIR "${CMAKE_BINARY_DIR}/zircon_tools_bcn_bake")
set(ZIRCON_BCN_BAKE_EXE
	"${ZIRCON_BCN_BAKE_BUILD_DIR}/$<CONFIG>/zircon_bcn_bake.exe")
set(ZIRCON_BCN_BAKE_STAMP
	"${ZIRCON_BCN_BAKE_BUILD_DIR}/zircon_bcn_bake-$<CONFIG>.stamp")

set(ZIRCON_BCN_BAKE_TOOL_DIR
	"${CMAKE_SOURCE_DIR}/src/tools/zircon_bcn_encoder")

add_custom_command(
	OUTPUT "${ZIRCON_BCN_BAKE_STAMP}"
	COMMAND ${CMAKE_COMMAND}
		-S "${ZIRCON_BCN_BAKE_TOOL_DIR}"
		-B "${ZIRCON_BCN_BAKE_BUILD_DIR}"
		"-DZIRCON_KOTEK_SOURCE_DIR=${CMAKE_SOURCE_DIR}/kotek/src"
		"-DZIRCON_KOTEK_GENERATED_DIR=${CMAKE_BINARY_DIR}/kotek/src"
		"-DZIRCON_KOTEK_DEVELOPMENT_TYPE=${KOTEK_DEVELOPMENT_TYPE}"
		"-DZIRCON_KOTEK_LOG_LIBRARY=${KOTEK_LOG_LIBRARY}"
		"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/kotek/vcpkg/scripts/buildsystems/vcpkg.cmake"
		"-DVCPKG_TARGET_TRIPLET=x64-windows-static"
	COMMAND ${CMAKE_COMMAND}
		--build "${ZIRCON_BCN_BAKE_BUILD_DIR}"
		--config $<CONFIG>
	COMMAND ${CMAKE_COMMAND} -E touch "${ZIRCON_BCN_BAKE_STAMP}"
	DEPENDS
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/zircon_bcn_bake.cpp"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/zircon_bcn_encoder_adapter.h"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/zircon_tool_alias_prologue.h"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/CMakeLists.txt"
		# the compiled-in engine sources: a format/bake change must
		# rebuild the tool (never drift on the format)
		"${CMAKE_SOURCE_DIR}/src/core/zircon_texture_bcn.h"
		"${CMAKE_SOURCE_DIR}/src/core/zircon_texture_bcn.cpp"
		# the vendored encoders (byte-pristine — an upstream sync bumps
		# the build)
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/rgbcx.cpp"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/rgbcx.h"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/bc7enc.cpp"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/bc7enc.h"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/bc7decomp.cpp"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/bc7decomp.h"
		"${ZIRCON_BCN_BAKE_TOOL_DIR}/bcdec.h"
		# the configure-time flag set the tool compiles the engine
		# sources under (capacities, backends — content changes on
		# reconfigure)
		"${CMAKE_BINARY_DIR}/kotek/src/kotek.core.defines.static.cpp/include/kotek_std_preprocessors.h"
	COMMENT "building zircon_bcn_bake (host tool)"
	VERBATIM
)

# ALL: the tool rides the normal `cmake --build` (the same way
# zircon_kpacker_tool does) — no engine target depends on it (the engine
# never runs the tool; users and CI do)
add_custom_target(zircon_bcn_bake_tool ALL DEPENDS "${ZIRCON_BCN_BAKE_STAMP}")
set_target_properties(zircon_bcn_bake_tool PROPERTIES FOLDER "engine/tools")
