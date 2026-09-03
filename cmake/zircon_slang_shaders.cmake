# zircon_slang_shaders.cmake — the Slang shader pipeline. Slang is zircon's
# ONLY user shading language (owner directive); one .slang source serves both
# render backends:
#
#   bgfx (vulkan renderer): slangc -target spirv, then zircon_shaderpack wraps
#       the blob into bgfx's .bin container (bin version 11) ->
#       data_user/shader_cache/bgfx/vulkan/<name>.<vs|fs>.bin
#   bgfx (d3d11 renderer — the default boot's active one): slangc -target hlsl
#       (sm_5_0) with -DZIRCON_SLANG_BGFX_FXC (fragment cbuffers land at b0 —
#       bgfx's d3d renderers bind the uniform cbuffer at slot 0 for BOTH
#       stages, unlike the vulkan b0-vs/b1-fs convention), the Windows SDK's
#       fxc compiles DXBC, zircon_shaderpack wraps it into the same v11
#       container with the same io/uniform metadata ->
#       data_user/shader_cache/bgfx/dx11/<name>.<vs|fs>.bin
#   NRI (dx12): slangc -target dxil (raw blob, no container) ->
#       data_user/shader_cache/nri/dx12/<name>.<vs|fs>.dxil
#
# Included from src/render/CMakeLists.txt. Steps:
#   1. fetch the pinned slang + dxc toolchains into ${CMAKE_BINARY_DIR}/_tools
#      (build tree, gitignored — never commit binaries; the pattern follows
#      kotek/cmake/windows/nri.cmake's pinned configure-time fetch)
#   2. build zircon_shaderpack (src/tools/zircon_shaderpack) at BUILD time via
#      a nested standalone configure — the same pattern zircon_generator uses
#      (see the PRE_BUILD step in src/render/CMakeLists.txt)
#   3. per shader/stage/backend custom commands (incremental: rebuilds only
#      when the .slang source, the shared module, or the packer change)
#   4. target zircon_shaders; game.ktk (target `zircon`, src/engine) depends
#      on it so `cmake --build` always produces fresh blobs before boot

# --- pinned toolchains (verified 2026-08-01: hashes match the release zips) -
set(ZIRCON_SLANG_VERSION "2026.14.1")
set(ZIRCON_SLANG_ZIP_URL
	"https://github.com/shader-slang/slang/releases/download/v${ZIRCON_SLANG_VERSION}/slang-${ZIRCON_SLANG_VERSION}-windows-x86_64.zip")
set(ZIRCON_SLANG_ZIP_SHA256
	"5ed0a59d650a0af0aca45d5db4e083b3d8fb5cea05748747dd95dfbe9c580658")
# dxc v1.9.2607 — slangc loads dxcompiler.dll/dxil.dll dynamically for
# -target dxil; they must sit next to slangc.exe
set(ZIRCON_DXC_ZIP_URL
	"https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip")
set(ZIRCON_DXC_ZIP_SHA256
	"a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241")

set(ZIRCON_SHADER_TOOLS_DIR "${CMAKE_BINARY_DIR}/_tools")
set(ZIRCON_SLANG_BIN_DIR "${ZIRCON_SHADER_TOOLS_DIR}/slang/bin")
set(ZIRCON_SLANGC "${ZIRCON_SLANG_BIN_DIR}/slangc.exe")

if (NOT EXISTS "${ZIRCON_SLANGC}")
	message(STATUS "[zircon]: fetching slang ${ZIRCON_SLANG_VERSION} into ${ZIRCON_SHADER_TOOLS_DIR} (one-time)...")
	set(slang_zip "${ZIRCON_SHADER_TOOLS_DIR}/slang-${ZIRCON_SLANG_VERSION}.zip")
	file(DOWNLOAD "${ZIRCON_SLANG_ZIP_URL}" "${slang_zip}"
		EXPECTED_HASH "SHA256=${ZIRCON_SLANG_ZIP_SHA256}"
		SHOW_PROGRESS STATUS slang_dl_status)
	list(GET slang_dl_status 0 slang_dl_code)
	if (NOT slang_dl_code EQUAL 0)
		list(GET slang_dl_status 1 slang_dl_msg)
		message(FATAL_ERROR "[zircon]: slang download failed: ${slang_dl_msg}")
	endif()
	file(ARCHIVE_EXTRACT INPUT "${slang_zip}"
		DESTINATION "${ZIRCON_SHADER_TOOLS_DIR}/slang")
	file(REMOVE "${slang_zip}")
	if (NOT EXISTS "${ZIRCON_SLANGC}")
		message(FATAL_ERROR "[zircon]: slang archive extracted but slangc.exe is missing — upstream layout changed?")
	endif()
endif()

if (NOT EXISTS "${ZIRCON_SLANG_BIN_DIR}/dxcompiler.dll" OR
	NOT EXISTS "${ZIRCON_SLANG_BIN_DIR}/dxil.dll")
	message(STATUS "[zircon]: fetching dxc (dxil backend for slangc) into ${ZIRCON_SHADER_TOOLS_DIR} (one-time)...")
	set(dxc_zip "${ZIRCON_SHADER_TOOLS_DIR}/dxc.zip")
	file(DOWNLOAD "${ZIRCON_DXC_ZIP_URL}" "${dxc_zip}"
		EXPECTED_HASH "SHA256=${ZIRCON_DXC_ZIP_SHA256}"
		SHOW_PROGRESS STATUS dxc_dl_status)
	list(GET dxc_dl_status 0 dxc_dl_code)
	if (NOT dxc_dl_code EQUAL 0)
		list(GET dxc_dl_status 1 dxc_dl_msg)
		message(FATAL_ERROR "[zircon]: dxc download failed: ${dxc_dl_msg}")
	endif()
	file(ARCHIVE_EXTRACT INPUT "${dxc_zip}"
		DESTINATION "${ZIRCON_SHADER_TOOLS_DIR}/dxc-extract")
	file(COPY_FILE
		"${ZIRCON_SHADER_TOOLS_DIR}/dxc-extract/bin/x64/dxcompiler.dll"
		"${ZIRCON_SLANG_BIN_DIR}/dxcompiler.dll" ONLY_IF_DIFFERENT)
	file(COPY_FILE
		"${ZIRCON_SHADER_TOOLS_DIR}/dxc-extract/bin/x64/dxil.dll"
		"${ZIRCON_SLANG_BIN_DIR}/dxil.dll" ONLY_IF_DIFFERENT)
	file(REMOVE_RECURSE "${ZIRCON_SHADER_TOOLS_DIR}/dxc-extract")
	file(REMOVE "${dxc_zip}")
endif()

# --- fxc (Windows SDK) for the bgfx d3d11 route ------------------------------
# the SDK bin folders are versioned (10.0.26100.0, ...); the newest wins
file(GLOB zircon_fxc_candidates
	"C:/Program Files (x86)/Windows Kits/10/bin/*/x64/fxc.exe")

if (NOT zircon_fxc_candidates)
	message(FATAL_ERROR
		"[zircon]: fxc.exe not found — the bgfx d3d11 shader route needs the "
		"Windows 10/11 SDK (expected under C:/Program Files (x86)/Windows "
		"Kits/10/bin/<version>/x64/fxc.exe); install the SDK or build without "
		"the d3d11 backend")
endif()

list(SORT zircon_fxc_candidates COMPARE NATURAL)
list(POP_BACK zircon_fxc_candidates ZIRCON_FXC)
message(STATUS "[zircon]: bgfx d3d11 shader route uses fxc: ${ZIRCON_FXC}")

# --- zircon_shaderpack, built at BUILD time (nested standalone configure) ---
set(ZIRCON_SHADERPACK_BUILD_DIR "${CMAKE_BINARY_DIR}/zircon_tools_shaderpack")
set(ZIRCON_SHADERPACK_EXE
	"${ZIRCON_SHADERPACK_BUILD_DIR}/$<CONFIG>/zircon_shaderpack.exe")
# per-config stamp: a Debug-built stamp must not satisfy a Release build
set(ZIRCON_SHADERPACK_STAMP
	"${ZIRCON_SHADERPACK_BUILD_DIR}/zircon_shaderpack-$<CONFIG>.stamp")

add_custom_command(
	OUTPUT "${ZIRCON_SHADERPACK_STAMP}"
	COMMAND ${CMAKE_COMMAND}
		-S "${CMAKE_SOURCE_DIR}/src/tools/zircon_shaderpack"
		-B "${ZIRCON_SHADERPACK_BUILD_DIR}"
	COMMAND ${CMAKE_COMMAND}
		--build "${ZIRCON_SHADERPACK_BUILD_DIR}"
		--config $<CONFIG>
	COMMAND ${CMAKE_COMMAND} -E touch "${ZIRCON_SHADERPACK_STAMP}"
	DEPENDS
		"${CMAKE_SOURCE_DIR}/src/tools/zircon_shaderpack/zircon_shaderpack.cpp"
		"${CMAKE_SOURCE_DIR}/src/tools/zircon_shaderpack/CMakeLists.txt"
	COMMENT "building zircon_shaderpack (host tool)"
	VERBATIM
)

# --- per-shader registry ----------------------------------------------------
# The io-name sets and the uniform table cannot be recovered from the raw
# blobs (slangc emits no bgfx metadata), so every shader declares them here;
# they must mirror the .slang source (conventions: data_game/shaders/slang/
# zircon_core.slang). Uniform spec: name:type:regIndex:regCount[:num] with
# regIndex = cbuffer byte offset — see zircon_shaderpack --help.
#
# zircon_add_slang_shader(<name>
#     VS_IN <csv attributes, SPIR-V location order>
#     VS_OUT <csv varyings>
#     VS_UNIFORMS <spec>...
#     FS_IN <csv varyings>
#     FS_UNIFORMS <spec>...)
set(ZIRCON_SLANG_SHADER_DIR "${CMAKE_SOURCE_DIR}/data_game/shaders/slang")
set(ZIRCON_SHADER_CACHE_DIR "${CMAKE_SOURCE_DIR}/data_user/shader_cache")
set(ZIRCON_SHADER_SPV_DIR "${CMAKE_BINARY_DIR}/generated/shaders")
file(MAKE_DIRECTORY
	"${ZIRCON_SHADER_CACHE_DIR}/bgfx/vulkan"
	"${ZIRCON_SHADER_CACHE_DIR}/bgfx/dx11"
	"${ZIRCON_SHADER_CACHE_DIR}/nri/dx12"
	"${ZIRCON_SHADER_SPV_DIR}")

set(ZIRCON_SHADER_OUTPUTS "")

function(zircon_add_slang_shader name)
	cmake_parse_arguments(ZS "" "VS_IN;VS_OUT;FS_IN" "VS_UNIFORMS;FS_UNIFORMS" ${ARGN})

	foreach(stage vs fs)
		string(TOUPPER "${stage}" STAGE)
		set(slang_file "${ZIRCON_SLANG_SHADER_DIR}/${name}.${stage}.slang")
		set(spv_file "${ZIRCON_SHADER_SPV_DIR}/${name}.${stage}.spv")
		set(bgfx_bin "${ZIRCON_SHADER_CACHE_DIR}/bgfx/vulkan/${name}.${stage}.bin")
		set(nri_dxil "${ZIRCON_SHADER_CACHE_DIR}/nri/dx12/${name}.${stage}.dxil")

		if ("${stage}" STREQUAL "vs")
			set(pack_type v)
			# empty name lists must not emit the flag at all: a quoted ""
			# list element is dropped on expansion, which would make the
			# packer swallow the NEXT flag as this one's value (the grid
			# vertex stage has no vertex inputs)
			set(pack_names "")
			if (NOT "${ZS_VS_IN}" STREQUAL "")
				list(APPEND pack_names --in-names "${ZS_VS_IN}")
			endif()
			if (NOT "${ZS_VS_OUT}" STREQUAL "")
				list(APPEND pack_names --out-names "${ZS_VS_OUT}")
			endif()
		else()
			set(pack_type f)
			set(pack_names "")
			if (NOT "${ZS_FS_IN}" STREQUAL "")
				list(APPEND pack_names --in-names "${ZS_FS_IN}")
			endif()
		endif()

		set(pack_uniforms "")
		foreach(spec IN LISTS ZS_${STAGE}_UNIFORMS)
			list(APPEND pack_uniforms --uniform "${spec}")
		endforeach()

		add_custom_command(
			OUTPUT "${bgfx_bin}" "${nri_dxil}"
			# bgfx (vulkan): SPIR-V blob -> bgfx .bin container
			COMMAND "${ZIRCON_SLANGC}" "${slang_file}"
				-entry ${stage}_main -target spirv -profile spirv_1_5
				-o "${spv_file}"
			COMMAND "${ZIRCON_SHADERPACK_EXE}"
				--type ${pack_type} --input "${spv_file}" --output "${bgfx_bin}"
				${pack_names} ${pack_uniforms}
			# NRI (dx12): raw DXIL blob, no container
			COMMAND "${ZIRCON_SLANGC}" "${slang_file}"
				-entry ${stage}_main -target dxil -profile sm_6_0
				-o "${nri_dxil}"
			DEPENDS
				"${slang_file}"
				"${ZIRCON_SLANG_SHADER_DIR}/zircon_core.slang"
				"${ZIRCON_SHADERPACK_STAMP}"
			COMMENT "slang: ${name}.${stage} -> bgfx/vulkan .bin + nri/dx12 .dxil"
			VERBATIM
		)

		# --- bgfx d3d11: Slang -> HLSL (sm_5_0) -> FXC DXBC -> the same
		# v11 .bin container + metadata as the vulkan pack above (bgfx's
		# d3d11 reader parses the identical layout); the runtime loader
		# resolves shader_cache/bgfx/<renderer>/<name>.<stage>.bin
		set(hlsl_file "${ZIRCON_SHADER_SPV_DIR}/${name}.${stage}.hlsl")
		set(dxbc_file "${ZIRCON_SHADER_SPV_DIR}/${name}.${stage}.dxbc")
		set(bgfx_bin_d3d11
			"${ZIRCON_SHADER_CACHE_DIR}/bgfx/dx11/${name}.${stage}.bin")

		if ("${stage}" STREQUAL "vs")
			set(fxc_profile vs_5_0)
		else()
			set(fxc_profile ps_5_0)
		endif()

		add_custom_command(
			OUTPUT "${bgfx_bin_d3d11}"
			COMMAND "${ZIRCON_SLANGC}" "${slang_file}"
				-entry ${stage}_main -target hlsl -profile sm_5_0
				-DZIRCON_SLANG_BGFX_FXC
				-o "${hlsl_file}"
			COMMAND "${ZIRCON_FXC}" /nologo /T ${fxc_profile}
				/E ${stage}_main /Fo "${dxbc_file}" "${hlsl_file}"
			COMMAND "${ZIRCON_SHADERPACK_EXE}"
				--type ${pack_type} --input "${dxbc_file}"
				--output "${bgfx_bin_d3d11}"
				${pack_names} ${pack_uniforms}
			DEPENDS
				"${slang_file}"
				"${ZIRCON_SLANG_SHADER_DIR}/zircon_core.slang"
				"${ZIRCON_SHADERPACK_STAMP}"
			COMMENT "slang: ${name}.${stage} -> bgfx/dx11 .bin (fxc)"
			VERBATIM
		)

		list(APPEND ZIRCON_SHADER_OUTPUTS "${bgfx_bin}" "${nri_dxil}"
			"${bgfx_bin_d3d11}")
	endforeach()

	set(ZIRCON_SHADER_OUTPUTS "${ZIRCON_SHADER_OUTPUTS}" PARENT_SCOPE)
endfunction()

# registry: one call per shader (metadata mirrors the .slang sources)
# model_static: forward-Phong lit static geometry (task Z3 P2g) — the FS
# LightParams cbuffer holds the four pass-written light uniforms
zircon_add_slang_shader(model_static
	VS_IN "a_position,a_normal,a_color0"
	VS_OUT "v_worldPos,v_normal,v_color0"
	VS_UNIFORMS
		"u_model:mat4:0:4"
		"u_viewProj:mat4:64:4"
	FS_IN "v_worldPos,v_normal,v_color0"
	FS_UNIFORMS
		"u_lightDir:vec4:0:1"
		"u_lightColor:vec4:16:1"
		"u_ambient:vec4:32:1"
		"u_cameraPos:vec4:48:1"
)

# editor infinite grid (task Z3 P2d): vertex-id fullscreen triangle (no
# vertex inputs), analytic XZ grid in the fragment stage; u_invViewProj is
# a bgfx predefined uniform (auto-filled from the view transform), only
# u_cameraPos is set by the pass
zircon_add_slang_shader(grid
	VS_IN ""
	VS_OUT "v_ndc"
	FS_IN "v_ndc"
	FS_UNIFORMS
		"u_invViewProj:mat4:0:4"
		"u_cameraPos:vec4:64:1"
)

# editor own-gizmo overlay (task Z3 P2e): position-only handle meshes,
# u_modelViewProj is a bgfx predefined uniform (auto-filled from
# setTransform * the view transform), u_color is the pass's per-draw
# solid color
zircon_add_slang_shader(gizmo
	VS_IN "a_position"
	VS_OUT ""
	VS_UNIFORMS
		"u_modelViewProj:mat4:0:4"
	FS_IN ""
	FS_UNIFORMS
		"u_color:vec4:0:1"
)

add_custom_target(zircon_shaders DEPENDS ${ZIRCON_SHADER_OUTPUTS})
set_target_properties(zircon_shaders PROPERTIES FOLDER "engine/render/shaders")

# game.ktk (target `zircon`, src/engine) boots from these blobs — building the
# engine builds the shaders first
add_dependencies(zircon zircon_shaders)
