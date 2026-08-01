cmake_minimum_required(VERSION 3.21)

project(zircon.render VERSION 1 LANGUAGES CXX)


add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# render backends: bgfx (raster) is the only built-in one; NRI (dx12/vk +
# ray tracing) is planned and gets its own folder with the same two-project
# split (passes + executor). gles3/vk were removed (2026-07-22, owner
# decision).

### BGFX

# the executor: renderer + render-graph slots + pass base classes. Concrete
# passes live in the zircon.render.passes.bgfx project below (task Z3 split)
# and reach the graphs through zircon_game_manager + the generated pass
# factory.
set(to_include_bgfx_implementation_files
	"bgfx/zircon_renderer.h"
	"bgfx/zircon_renderer.cpp"
	"bgfx/zircon_render_pass_library_manager.h"
	"bgfx/zircon_render_pass_library_manager.cpp"
)

set(to_include_bgfx_editor_pass_interface
	"bgfx/zircon_render_graph_pass_editor.h"
	"bgfx/zircon_render_graph_pass_editor.cpp"
)

set(to_include_bgfx_game_pass_interface
	"bgfx/zircon_render_graph_pass.h"
	"bgfx/zircon_render_graph_pass.cpp"
)

### BGFX

### shaders
file(GLOB to_include_shaders_bgfx
	"../../data_game/shaders/bgfx/*.frag"
	"../../data_game/shaders/bgfx/*.vert"
)

### shaders

### os
set(to_include_shared_os
	"os/zircon_render_graph_pass_console.h"
	"os/zircon_render_graph_pass_console.cpp"
)
###

### NRI (task K11/Z5 phase 1: minimal renderer over the kotek.render.nri
### backend; the passes split is the deferred Z5 work)
set(to_include_nri
	"nri/zircon_renderer_nri.h"
	"nri/zircon_renderer_nri.cpp"
)
###

set(to_include_module_files
	"zircon_render.h"
	"main_render_dll.cpp"
)

set(to_include_bgfx
	${to_include_bgfx_implementation_files}
	${to_include_bgfx_game_pass_interface}
	${to_include_bgfx_editor_pass_interface}
	${to_include_shaders_bgfx}
)

kotek_add_library(${PROJECT_NAME}
	${to_include_bgfx}
	${to_include_module_files}
	${to_include_shared_os}
	${to_include_nri}
)

# TODO: add if when game.ktk builds as shared

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "engine/render")

if (UNIX)
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()


source_group("BGFX/Implementation" FILES ${to_include_bgfx_implementation_files})
source_group("BGFX/Passes/Game/Interfaces" FILES ${to_include_bgfx_game_pass_interface})
source_group("BGFX/Passes/Editor/Interfaces" FILES ${to_include_bgfx_editor_pass_interface})
source_group("BGFX/Shaders" FILES ${to_include_shaders_bgfx})
source_group("BGFX/Passes/OS" FILES ${to_include_shared_os})
source_group("NRI" FILES ${to_include_nri})

source_group("Module" FILES ${to_include_module_files})

kotek_target_precompile_headers(${PROJECT_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")

kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.core)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.ecs)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.world)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.editor)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC zircon.game)
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render)
# render glue shared with the bgfx backend (pass bases, simplified graph) —
# static leaf, linked directly even in PLUGIN mode
kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.render.shared.bgfx)

if (ZIRCON_GRAPHICS_DEVELOPMENT)
	# the pass-library manager (task Z3 P3a) owns a kotek
	# dll::shared_library handle for the hot-swappable passes DLL; the
	# default configuration never references the dll-container module
	kotek_target_link_libraries(${PROJECT_NAME} PUBLIC kotek.core.containers.dll)
endif()

### passes (bgfx) — the concrete render passes. STATIC in the default
### config (zero behavior change vs. the pre-split monolith); under
### -DZIRCON_GRAPHICS_DEVELOPMENT=ON (task Z3 P3a) the same sources build
### as the hot-swappable passes/zircon.render.passes.bgfx.dll (loaded by
### the executor's zircon_render_pass_library_manager) PLUS a static
### fallback twin (zircon.render.passes.bgfx.static) linked into
### game.ktk. The generated enum/factory headers
### (zircon_render_*_passes_enum.h, zircon_render_pass_factory.h) are
### regenerated in-place by the PRE_BUILD step below and are intentionally
### NOT listed as sources: they do not exist at configure time on a fresh
### clone.
set(to_include_bgfx_game_passes_no_streaming_files
	"bgfx/passes/no_streaming/zircon_render_graph_pass_present.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_present.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_model_static.h"
)

set(to_include_bgfx_editor_passes_no_streaming_files
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_imgui.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_imgui.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_present.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_present.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_model_static.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_model_static.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_grid.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_own.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_own.cpp"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_imguizmo.h"
	"bgfx/passes/no_streaming/zircon_render_graph_pass_editor_gizmo_imguizmo.cpp"
)

# vendored third-party (task Z3 P2f, owner-sanctioned exception to the
# no-third-party-in-engine rule — passes-project-local, never engine code):
# ImGuizmo v1.92.5 WIP, upstream master pinned at commit 5ab7676402ace03cdf
# 930b2d972f59c7d03c6fa8 (2026-07-29), MIT license (header intact in
# ImGuizmo.h + LICENSE alongside). vcpkg DOES carry an imguizmo port (1.10,
# stale vs. upstream) — vendoring chosen per the approved plan. Kept
# byte-pristine; compiles against kotek's vendored imgui (1.91.4) through
# the PRIVATE include dir below (its quoted "imgui.h"/"imgui_internal.h"
# includes predate kotek's virtual-path layout)
set(to_include_bgfx_passes_imguizmo_files
	"bgfx/passes/imguizmo/ImGuizmo.h"
	"bgfx/passes/imguizmo/ImGuizmo.cpp"
	"bgfx/passes/imguizmo/LICENSE"
)

# ImGuizmo.cpp includes imgui_internal.h, which demands the math operators
# macro before imgui.h. The macro must be TARGET-WIDE: the target PCH
# force-includes kotek.ui (which already pulls imgui.h), so a per-TU define
# would arrive after the PCH processed imgui.h without it (the vendored file
# stays byte-pristine)

# embedded shader/font blobs consumed by the editor imgui pass
set(to_include_bgfx_passes_embedded_files
	"bgfx/passes/fs_imgui_image.bin.h"
	"bgfx/passes/fs_ocornut_imgui.bin.h"
	"bgfx/passes/vs_imgui_image.bin.h"
	"bgfx/passes/vs_ocornut_imgui.bin.h"
	"bgfx/passes/droidsans.ttf.h"
	"bgfx/passes/icons_font_awesome.ttf.h"
	"bgfx/passes/icons_kenney.ttf.h"
	"bgfx/passes/robotomono_regular.ttf.h"
	"bgfx/passes/roboto_regular.ttf.h"
)

# the pass-library C ABI (task Z3 P3a): the stable create/destroy/
# enumerate surface the executor's zircon_render_pass_library_manager
# talks to. Compiled into every variant of the passes target (static
# default, dev-mode static twin, dev-mode DLL) so the seam is
# mode-agnostic.
set(to_include_bgfx_passes_library_files
	"bgfx/passes/zircon_passlib.h"
	"bgfx/passes/zircon_passlib.cpp"
)

set(to_include_bgfx_passes_all_files
	${to_include_bgfx_game_passes_no_streaming_files}
	${to_include_bgfx_editor_passes_no_streaming_files}
	${to_include_bgfx_passes_embedded_files}
	${to_include_bgfx_passes_imguizmo_files}
	${to_include_bgfx_passes_library_files}
)

set(ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME zircon.render.passes.bgfx)

# the setup every passes variant shares (the graphics-development
# configuration builds the same sources twice: the hot-swappable DLL +
# a static in-build fallback twin)
function(zircon_configure_passes_target target_name)
	# the vendored ImGuizmo.cpp includes "imgui.h"/"imgui_internal.h"
	# quoted (upstream layout); kotek's imgui lives behind the
	# virtual-path root (kotek/src) — add the module's raw include dir
	# for this target only (absolute: KOTEK_ENGINE_ROOT_FOLDER_NAME is a
	# NAME, not a path)
	target_include_directories(${target_name} PRIVATE
		"${CMAKE_SOURCE_DIR}/${KOTEK_ENGINE_ROOT_FOLDER_NAME}/src/kotek.ui.imgui/include")

	# ImGuizmo.cpp includes imgui_internal.h, which demands the math
	# operators macro before imgui.h. The macro must be TARGET-WIDE: the
	# target PCH force-includes kotek.ui (which already pulls imgui.h),
	# so a per-TU define would arrive after the PCH processed imgui.h
	# without it (the vendored file stays byte-pristine)
	target_compile_definitions(${target_name} PRIVATE
		IMGUI_DEFINE_MATH_OPERATORS)

	kotek_target_precompile_headers(${target_name} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:<kotek.pch/pch.h>>")
endfunction()

# the PRE_BUILD codegen of the pass enum/factory headers
# (zircon_render_*_passes_enum.h, zircon_render_pass_factory.h) —
# regenerated in-place, intentionally NOT listed as sources (they do not
# exist at configure time on a fresh clone)
function(zircon_add_passes_codegen target_name)
	add_custom_command(
		TARGET ${target_name}
		PRE_BUILD
		COMMAND ${CMAKE_COMMAND}
			-S ${CMAKE_SOURCE_DIR}/src/tools/cpp_generators/ecs_fields_generator
			-B ${CMAKE_BINARY_DIR}/zircon_tools
			-DKOTEK_DEPS_FOLDER=${KOTEK_DEPS_FOLDER}
			-DKOTEK_DEPS_FOLDER_ENABLE=${KOTEK_DEPS_FOLDER_ENABLE}
			-DKOTEK_ENGINE_ROOT_FOLDER_NAME=${KOTEK_ENGINE_ROOT_FOLDER_NAME}
			-DKOTEK_NUGET_PLATFORM_NAME=${KOTEK_NUGET_PLATFORM_NAME}
			-DKOTEK_NUGET_COMPILER_NAME=${KOTEK_NUGET_COMPILER_NAME}
			-DKOTEK_NUGET_ARCH_BIT_NUMBER=${KOTEK_NUGET_ARCH_BIT_NUMBER}
		COMMAND ${CMAKE_COMMAND}
			--build ${CMAKE_BINARY_DIR}/zircon_tools
			--config $<CONFIG>
		COMMAND "${CMAKE_BINARY_DIR}/zircon_tools/$<CONFIG>/zircon_generator" --type 2 ${CMAKE_SOURCE_DIR}/src/render/bgfx/passes/no_streaming
	    COMMENT "generating render = no_streaming"
	)
endfunction()

if (ZIRCON_GRAPHICS_DEVELOPMENT)
	### graphics-development configuration (task Z3 P3a): the passes ###
	### build as a hot-swappable DLL next to the data dirs, plus a    ###
	### static twin inside game.ktk as the fallback (and for the      ###
	### engine test sources that exercise the generated factory)      ###

	# the static twin: identical to the default-configuration passes
	# library, only renamed. Linked into game.ktk; used when the
	# graphics_development feature is off or the DLL cannot be loaded.
	add_library(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static STATIC
		${to_include_bgfx_passes_all_files}
	)

	set_target_properties(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static
		PROPERTIES FOLDER "engine/render/passes")

	if (UNIX)
	    set_property(TARGET ${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static
			PROPERTY POSITION_INDEPENDENT_CODE ON)
	endif()

	zircon_add_passes_codegen(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static)
	zircon_configure_passes_target(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static)

	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC zircon.core)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC zircon.ecs)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC zircon.world)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC zircon.editor)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC zircon.game)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC kotek.render)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static PUBLIC kotek.render.shared.bgfx)

	# the hot-swappable DLL: output to <repo root>/passes/ (the boot
	# discipline runs kotek.exe from the repo root, so the loader finds
	# it at the fixed relative path in
	# ZIRCON_DEF_RENDER_PASS_LIBRARY_SHARED_FILE_PATH). It links ONLY
	# game.ktk's import library — every engine/bgfx/imgui symbol
	# resolves from the single copy inside game.ktk (statically linking
	# any of them would duplicate global state across the boundary:
	# bgfx's context, imgui's GImGui, etl's module-local terminators).
	# game.ktk loads the DLL via LoadLibrary at runtime and never links
	# it — there is no link cycle.
	add_library(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} SHARED
		${to_include_bgfx_passes_all_files}
	)

	set_target_properties(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PROPERTIES
		FOLDER "engine/render/passes"
		OUTPUT_NAME "zircon.render.passes.bgfx")

	# exact per-config output dirs (a multi-config generator would
	# append a $<CONFIG> subdir to the plain properties): the DLL and
	# its PDB land flat in passes/
	foreach (zircon_gfxdev_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
		set_target_properties(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY_${zircon_gfxdev_config} "${CMAKE_SOURCE_DIR}/passes"
			PDB_OUTPUT_DIRECTORY_${zircon_gfxdev_config} "${CMAKE_SOURCE_DIR}/passes")
	endforeach()

	zircon_configure_passes_target(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME})

	target_compile_definitions(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PRIVATE
		# dllexport on the 4 C-ABI entries (zircon_passlib.h)
		ZIRCON_PASSLIB_SHARED)

	# the imgui symbols (incl. the GImGui data symbol the vendored
	# ImGuizmo reads directly) live in game.ktk — data imports must be
	# declared. As a compile OPTION with embedded quotes: CMake's VS
	# generator splits a parenthesized target_compile_definitions value
	# into garbage ('IMGUI_API=__declspec;(;dllimport;)'), while
	# AdditionalOptions passes through verbatim and cl parses the quoted
	# /D value correctly
	target_compile_options(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PRIVATE
		"/DIMGUI_API=__declspec(dllimport)")

	# the dependency edge: the DLL builds after game.ktk (whose import
	# library it links) and after the twin (whose PRE_BUILD runs the
	# pass-factory codegen both variants compile against)
	target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PRIVATE zircon)
	add_dependencies(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}
		${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}.static)

	# unresolved-symbol hygiene: game.ktk re-exports its closure, so
	# anything still missing must fail loudly here (a silent pull from a
	# statically-linked copy would duplicate global state) — nothing but
	# zircon (game.ktk's import lib) is on the link line above
	target_link_options(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PRIVATE
		/IGNORE:4197)
else()
	### default configuration: one static passes library (zero behavior ###
	### change vs. the pre-split monolith)                              ###
	kotek_add_library(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME}
		${to_include_bgfx_passes_all_files}
	)

	set_target_properties(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PROPERTIES FOLDER "engine/render/passes")

	if (UNIX)
	    set_property(TARGET ${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
	endif()

	zircon_add_passes_codegen(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME})
	zircon_configure_passes_target(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME})

	# the pass sources include editor session/ecs/world headers and the
	# kotek render modules; all zircon modules are static today so this
	# propagates include paths only (the final link happens in game.ktk)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC zircon.core)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC zircon.ecs)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC zircon.world)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC zircon.editor)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC zircon.game)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC kotek.render)
	kotek_target_link_libraries(${ZIRCON_RENDER_PASSES_BGFX_TARGET_NAME} PUBLIC kotek.render.shared.bgfx)
endif()

source_group("BGFX/Passes/Game/No Streaming" FILES ${to_include_bgfx_game_passes_no_streaming_files})
source_group("BGFX/Passes/Editor/No Streaming" FILES ${to_include_bgfx_editor_passes_no_streaming_files})
source_group("BGFX/Passes/Embedded" FILES ${to_include_bgfx_passes_embedded_files})
source_group("BGFX/Passes/ImGuizmo (vendored)" FILES ${to_include_bgfx_passes_imguizmo_files})
source_group("BGFX/Passes/Library" FILES ${to_include_bgfx_passes_library_files})

### Slang shaders (the only user shading language; serves bgfx-vulkan via
### SPIR-V + zircon_shaderpack, and NRI-dx12 via DXIL). Defines the
### zircon_shaders target and makes game.ktk (target `zircon`) depend on it.
include("${CMAKE_SOURCE_DIR}/cmake/zircon_slang_shaders.cmake")

# IDE visibility for the Slang sources (compilation is the zircon_shaders
# target's job, driven by the registry in cmake/zircon_slang_shaders.cmake)
file(GLOB to_include_shaders_slang
	"../../data_game/shaders/slang/*.slang"
)
source_group("Slang/Shaders" FILES ${to_include_shaders_slang})
target_sources(${PROJECT_NAME} PRIVATE ${to_include_shaders_slang})
