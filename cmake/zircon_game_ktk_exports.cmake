# zircon_game_ktk_exports.cmake — game.ktk's export surface for the
# graphics-development configuration (task Z3 P3a). The hot-swappable
# passes DLL (passes/zircon.render.passes.bgfx.dll) links ONLY game.ktk's
# import library, so game.ktk must re-export every symbol of its static
# link closure (bgfx/bx/bimg, imgui, the kotek/zircon modules): the
# single bgfx/imgui/engine copy lives inside game.ktk and the DLL
# resolves from it (a statically-linked second copy would duplicate
# global state — bgfx's context, imgui's GImGui, etl's module-local
# terminators).
#
# Two cooperating mechanisms:
#   1. /WHOLEARCHIVE on the curated library list below forces every
#      object of those libraries into game.ktk (an export entry for an
#      unpulled archive member would not resolve — LNK2001).
#   2. a PRE_LINK step (cmake/zircon_generate_exports.cmake) dumps
#      dumpbin /SYMBOLS over every static library of the build's lib
#      directory (the whole kotek/zircon module set lands there) plus
#      game.ktk's own objects and writes the .def the link consumes.
#      WINDOWS_EXPORT_ALL_SYMBOLS cannot see static-library symbols
#      (cmake -E __create_def scans only the target's own objects) —
#      that is why the .def is generated.
#
# Iteration knobs (task Z3 risk R1): a LNK2001 at game.ktk's link names a
# symbol whose library has unpulled objects — find it in the generated
# .def (the per-source comment headers say which library it came from)
# and either move the library into the whole-archive list or add its
# file name to the exclusion list. A LNK2019 at the passes DLL's link
# names a symbol missing from the export surface — make sure its library
# is dumped (not excluded) and pulled into game.ktk (whole-archived).

# wires the export surface onto game.ktk: whole-archive link options, the
# PRE_LINK .def generation and the /DEF link option. MUST be called from
# the directory that creates the target (add_custom_command(TARGET)
# refuses cross-directory targets).
function(zircon_setup_game_ktk_exports game_target)
	# bgfx's imported targets were created by find_package inside
	# kotek.render.bgfx's directory scope — re-import them here so the
	# whole-archive/dump lists can reference them. VCPKG_TARGET_TRIPLET
	# is set in kotek's own directory scope only — mirror it so the
	# re-import resolves the same (CRT-matching) triplet instead of the
	# toolchain default
	set(VCPKG_TARGET_TRIPLET "${KOTEK_VCPKG_TRIPLET}")
	find_package(bgfx CONFIG REQUIRED)

	# the curated whole-archive list (task Z3 R1 knob): every object of
	# these libraries is forced into game.ktk, so their .def entries
	# always resolve. bgfx's API objects are only partially referenced by
	# the engine itself (the passes use the rest); kotek.ui.imgui carries
	# the renderer backend objects (dx9/10/11/12, gl, vulkan) that
	# nothing references but whose symbols the dump lists.
	set(_wholearchive_libs
		bgfx::bgfx
		bgfx::bx
		bgfx::bimg
		bgfx::bimg_decode
		kotek.ui.imgui
	)

	set(_wholearchive_files "")
	foreach (_lib IN LISTS _wholearchive_libs)
		if (TARGET ${_lib})
			target_link_options(${game_target} PRIVATE
				"/WHOLEARCHIVE:$<TARGET_FILE:${_lib}>")
			list(APPEND _wholearchive_files "$<TARGET_FILE:${_lib}>")
		else()
			message(WARNING
				"[zircon]: graphics-development export surface: '${_lib}' is not a target, skipped")
		endif()
	endforeach()

	# the system libraries the whole-archived imgui backend objects
	# reference (kotek.ui.imgui already brings opengl32)
	target_link_libraries(${game_target} PRIVATE
		d3d9 d3d10 d3d11 d3d12 dxgi)

	# ... and the vulkan loader for the vulkan backend: the house route
	# (kotek.core.os.win32 does the same), falling back to the vcpkg
	# triplet's lib dir by path when no Vulkan package is discoverable
	find_package(Vulkan QUIET)
	if (TARGET Vulkan::Vulkan)
		target_link_libraries(${game_target} PRIVATE Vulkan::Vulkan)
	else()
		target_link_libraries(${game_target} PRIVATE
			"$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/vulkan-1.lib,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/vulkan-1.lib>")
	endif()

	# libraries excluded from the dump (task Z3 R1 knob), matched by
	# exact file name against the lib-directory glob: import libraries of
	# dlls (their "symbols" are import stubs, not definitions) and static
	# libraries game.ktk never links or only partially pulls (dead
	# backends, unused subsystems — the passes can never reference their
	# symbols, and the unpulled objects would fail the link)
	set(_dump_exclude_files
		# game.ktk's own import library (from a previous build)
		"game.lib"
		# import libraries of the test plugin dlls
		"kotek.core.tests.plugin.lib"
		"kotek.core.main_manager.tests.plugin.lib"
		# import library of the glad dll
		"kotek.render.gl.glad.lib"
		# the passes DLL's own import library
		"zircon.render.passes.bgfx.lib"
		# dead/alternative backends and unused subsystems
		"kotek.render.gl.lib"
		"kotek.render.vk.lib"
		"kotek.render.software.lib"
		"kotek.render.shared.gl.lib"
		"kotek.render.shared.vk.lib"
		"kotek.render.shared.dx.lib"
		"kotek.render.nri.lib"
		"NRI_D3D12.lib"
		"NRI_Shared.lib"
		"NRI_Validation.lib"
		"kotek.core.window.glfw.lib"
		"kotek.core.os.linux.lib"
		"kotek.core.defines.static.os.linux.lib"
		"kotek.core.memory.gpu.vulkan.lib"
		"kotek.ui.rmlui.lib"
		"kotek.ui.videoplayer.lib"
		"kotek.ui.videoplayer.avif.lib"
		# the exe-side game framework host — game.ktk never links it
		# (its 174 std::function/entry-point objects would be unresolved
		# export entries)
		"kotek.game.lib"
		# the test-bearing libraries below are routed to per-object
		# dumps instead (see _dump_as_objects_libs)
		"kotek.core.lib"
		"kotek.core.containers.string.lib"
		"kotek.core.enum.core.lib"
		"kotek.core.input.lib"
		"kotek.core.math.lib"
	)

	# test-bearing libraries, dumped per-OBJECT with the test objects
	# skipped. A /DEF export entry acts like /INCLUDE: it pulls the
	# archive member into game.ktk — and a pulled gtest object
	# self-registers, so the kotek suite would run inside game.ktk's
	# registry too and then again in kotek.exe's; the plugin-override
	# test's never-unloaded double dll then fails its own second run
	# (file lock). The convention these paths rely on: a kotek module's
	# objects land in kotek/src/<module>/<module>.dir/<config>/ and its
	# test objects are named *_test_*.obj
	set(_dump_as_objects_libs
		kotek.core
		kotek.core.containers.string
		kotek.core.enum.core
		kotek.core.input
		kotek.core.math
	)

	set(_object_dir_inputs "")
	foreach (_lib IN LISTS _dump_as_objects_libs)
		list(APPEND _object_dir_inputs
			"@OBJECTS_SKIP_TESTS:${CMAKE_BINARY_DIR}/kotek/src/${_lib}/${_lib}.dir/$<CONFIG>")
	endforeach()

	list(JOIN _dump_exclude_files "|" _dump_exclude_joined)
	list(JOIN _wholearchive_files "|" _wholearchive_joined)
	list(JOIN _object_dir_inputs "|" _object_dir_inputs_joined)

	# locate dumpbin (CMake records it as CMAKE_DUMPBIN only for some
	# generators — the VS generator leaves it empty; fall back to the
	# linker's directory, where dumpbin.exe always sits in an MSVC
	# toolchain). A dedicated variable name: a find_program on a variable
	# that already holds an empty string skips the search
	set(_dumpbin "${CMAKE_DUMPBIN}")
	if (NOT EXISTS "${_dumpbin}")
		get_filename_component(_linker_dir "${CMAKE_LINKER}" DIRECTORY)
		find_program(ZIRCON_DUMPBIN_EXECUTABLE
			NAMES dumpbin
			HINTS "${_linker_dir}")
		set(_dumpbin "${ZIRCON_DUMPBIN_EXECUTABLE}")
	endif()
	if (NOT EXISTS "${_dumpbin}")
		message(FATAL_ERROR
			"[zircon]: dumpbin not found (CMAKE_DUMPBIN is empty and no dumpbin.exe sits next to ${CMAKE_LINKER})")
	endif()

	set(_def_file
		"${CMAKE_BINARY_DIR}/generated/zircon_game_exports_$<CONFIG>.def")

	add_custom_command(
		TARGET ${game_target}
		PRE_LINK
		COMMAND ${CMAKE_COMMAND}
			"-DZIRCON_GENERATE_EXPORTS_DUMPBIN=${_dumpbin}"
			"-DZIRCON_GENERATE_EXPORTS_OUT=${_def_file}"
			"-DZIRCON_GENERATE_EXPORTS_LIBDIR=${CMAKE_BINARY_DIR}/lib/$<CONFIG>"
			"-DZIRCON_GENERATE_EXPORTS_EXCLUDE=${_dump_exclude_joined}"
			"-DZIRCON_GENERATE_EXPORTS_EXTRALIBS=${_wholearchive_joined}"
			"-DZIRCON_GENERATE_EXPORTS_OBJDIR=$<TARGET_PROPERTY:${game_target},BINARY_DIR>/${game_target}.dir/$<CONFIG>"
			"-DZIRCON_GENERATE_EXPORTS_TESTLIB_OBJDIRS=${_object_dir_inputs_joined}"
			-P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/zircon_generate_exports.cmake"
		COMMENT "generating game.ktk's export surface for the passes DLL"
		VERBATIM
	)

	target_link_options(${game_target} PRIVATE
		"/DEF:${_def_file}"
		# the dump lists every archive member's symbols, so COMDAT
		# duplicates across libraries are expected and harmless (the
		# linker keeps the first definition)
		/IGNORE:4197)

	message(STATUS
		"[zircon]: graphics-development export surface: PRE_LINK dump of ${CMAKE_BINARY_DIR}/lib + whole-archived bgfx/bx/bimg/bimg_decode/kotek.ui.imgui")
endfunction()
