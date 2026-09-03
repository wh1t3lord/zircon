# zircon_generate_exports.cmake — PRE_LINK export-surface generator for
# the graphics-development configuration (task Z3 P3a, runs as
# `cmake -P`). Dumps dumpbin /SYMBOLS over the static libraries of the
# build's lib directory (minus the exclusion list), the curated extra
# libraries (the whole-archived vcpkg set lives outside the lib
# directory) and game.ktk's own objects — and writes the .def that makes
# game.ktk re-export every engine/bgfx/imgui symbol the hot-swappable
# passes DLL can reference.
#
# Why this exists: WINDOWS_EXPORT_ALL_SYMBOLS scans only the target's OWN
# object files (cmake -E __create_def over the objects list) — symbols of
# static libraries linked into the dll are invisible to it, even with
# /WHOLEARCHIVE. The passes DLL needs those symbols (bgfx, imgui incl.
# the GImGui data symbol, the zircon/kotek modules), so the export list
# is generated here instead. The .def itself is written write-if-
# different: a content-identical dump keeps the old file's mtime (task Z3
# P3b — see the _def_content accumulation below).
#
# Inputs (-D):
#   ZIRCON_GENERATE_EXPORTS_DUMPBIN   full path to dumpbin.exe
#   ZIRCON_GENERATE_EXPORTS_OUT       the .def file to write
#   ZIRCON_GENERATE_EXPORTS_LIBDIR    directory globbed for *.lib
#   ZIRCON_GENERATE_EXPORTS_EXCLUDE   |-separated exact file names to
#                                     skip inside LIBDIR
#   ZIRCON_GENERATE_EXPORTS_EXTRALIBS |-separated extra library paths
#                                     (already genex-expanded)
#   ZIRCON_GENERATE_EXPORTS_OBJDIR    directory globbed for *.obj (the
#                                     target's own objects)
#   ZIRCON_GENERATE_EXPORTS_TESTLIB_OBJDIRS
#                                     |-separated @OBJECTS_SKIP_TESTS:<dir>
#                                     entries: object directories of the
#                                     test-bearing libraries, globbed for
#                                     *.obj with *_test_* objects skipped
#                                     (a /DEF entry acts like /INCLUDE —
#                                     dumping a test object would pull it
#                                     into the image and double-register
#                                     the kotek suite inside game.ktk)

if (NOT ZIRCON_GENERATE_EXPORTS_DUMPBIN OR
	NOT EXISTS "${ZIRCON_GENERATE_EXPORTS_DUMPBIN}")
	message(FATAL_ERROR
		"[zircon]: dumpbin not found: '${ZIRCON_GENERATE_EXPORTS_DUMPBIN}'")
endif()

string(REPLACE "|" ";" _exclude "${ZIRCON_GENERATE_EXPORTS_EXCLUDE}")
string(REPLACE "|" ";" _extra_libs "${ZIRCON_GENERATE_EXPORTS_EXTRALIBS}")
string(REPLACE "|" ";" _testlib_objdirs
	"${ZIRCON_GENERATE_EXPORTS_TESTLIB_OBJDIRS}")

file(GLOB _libs "${ZIRCON_GENERATE_EXPORTS_LIBDIR}/*.lib")
file(GLOB _objects "${ZIRCON_GENERATE_EXPORTS_OBJDIR}/*.obj")

set(_inputs "")

foreach (_lib IN LISTS _libs)
	get_filename_component(_lib_name "${_lib}" NAME)
	if ("${_lib_name}" IN_LIST _exclude)
		continue()
	endif()
	list(APPEND _inputs "${_lib}")
endforeach()

foreach (_extra IN LISTS _extra_libs)
	if (_extra AND EXISTS "${_extra}")
		list(APPEND _inputs "${_extra}")
	endif()
endforeach()

foreach (_object IN LISTS _objects)
	list(APPEND _inputs "${_object}")
endforeach()

# the test-bearing libraries contribute their NON-test objects only
foreach (_entry IN LISTS _testlib_objdirs)
	if (NOT "${_entry}" MATCHES "^@OBJECTS_SKIP_TESTS:(.+)$")
		continue()
	endif()

	set(_testlib_dir "${CMAKE_MATCH_1}")
	file(GLOB _testlib_objects "${_testlib_dir}/*.obj")

	if (NOT _testlib_objects)
		message(WARNING
			"[zircon]: no objects found for a test-bearing library at '${_testlib_dir}' — its symbols will be missing from the export surface")
		continue()
	endif()

	foreach (_object IN LISTS _testlib_objects)
		get_filename_component(_object_name "${_object}" NAME)
		if ("${_object_name}" MATCHES "_test_" OR
			"${_object_name}" MATCHES "^main_.*_dll\\.obj$")
			# test objects (gtest self-registration must not leak into
			# game.ktk) and module entry objects (the passes DLL never
			# imports InitializeModule_*/ShutdownModule_*; their
			# registrar references would drag the test objects in)
			continue()
		endif()
		list(APPEND _inputs "${_object}")
	endforeach()
endforeach()

# the def is accumulated in memory and written once at the end,
# write-if-different: a content-identical dump keeps the previous file's
# mtime, so a no-change PRE_LINK regeneration does not by itself re-arm
# the link inputs (task Z3 P3b — a mid-run passes rebuild in the
# graphics-development configuration must leave game.ktk untouched)
set(_def_content "EXPORTS\n")

set(_total_functions 0)
set(_total_data 0)

foreach (_file IN LISTS _inputs)
	execute_process(
		COMMAND "${ZIRCON_GENERATE_EXPORTS_DUMPBIN}" /SYMBOLS /NOLOGO
			"${_file}"
		OUTPUT_VARIABLE _symbols
		ERROR_QUIET
	)

	# dumpbin line shape:
	#   00A 00000000 SECT3  notype ()    External     | ?name@@...
	# defined symbols carry a SECT<n> section (UNDEF ones must never be
	# exported — the def entry would not resolve); functions are marked
	# by the () type suffix, everything else is data (the consumer
	# declares it __declspec(dllimport) — see the passes DLL's IMGUI_API
	# define). NOTE the CMake regex dialect: \(\) and \| are NOT literal
	# escapes (they are group/alternation syntax) — literal parens and
	# pipes are written [()] and [|]
	string(REGEX MATCHALL
		"SECT[0-9A-F]+[ \t]+[A-Za-z]+[ \t]+[(][)][ \t]+External[ \t]+[|][ \t]+[^ \t\r\n]+"
		_functions "${_symbols}")
	string(REGEX MATCHALL
		"SECT[0-9A-F]+[ \t]+[A-Za-z]+[ \t]+External[ \t]+[|][ \t]+[^ \t\r\n]+"
		_data "${_symbols}")

	# strip everything up to and including the '|' (per match-all
	# element; [^|;] never crosses an element boundary)
	string(REGEX REPLACE "[^|;]*[|][ \t]*" "" _functions "${_functions}")
	string(REGEX REPLACE "[^|;]*[|][ \t]*" "" _data "${_data}")

	# PCH sentinels truncate to '__' and break the link with
	# LNK2001/LNK4022 (kotek/cmake/library.cmake documents the same
	# hazard for WINDOWS_EXPORT_ALL_SYMBOLS); import-descriptor records
	# are import-lib artifacts, never real exports. Deleting destructors
	# (??_G/??_E) are skipped too: exporting them draws LNK4102 ("image
	# may not run correctly") and the passes DLL never deletes through an
	# exported destructor — every pass is destroyed by the library's own
	# zircon_passlib_destroy (the reload-safety rule).
	#
	# Module entries (?InitializeModule_*/?ShutdownModule_* etc.) are
	# never imported by the passes DLL, and listing them would /INCLUDE-
	# pull the module entry objects whose references to the test
	# registrars drag every test object into game.ktk (the kotek suite
	# would then run in game.ktk's registry too — see the run_unit_tests
	# filter in zircon_game_manager.cpp). std::/stdext:: symbols are
	# excluded as well: the DLL is a self-contained /MT module and must
	# never bind std runtime into game.ktk (cross-CRT), and their shared
	# COMDATs are the lottery vector that once pulled
	# kotek_core_test_plugin_override.obj for std::string::find.
	list(FILTER _functions EXCLUDE REGEX
		"PchSym|__IMPORT_DESCRIPTOR|__NULL_IMPORT_DESCRIPTOR|^__imp_|^\\?\\?_G|^\\?\\?_E|^\\?(Initialize|Shutdown|Serialize|Deserialize)Module_|std@@|stdext@@")
	list(FILTER _data EXCLUDE REGEX
		"PchSym|__IMPORT_DESCRIPTOR|__NULL_IMPORT_DESCRIPTOR|^__imp_|std@@|stdext@@")

	list(LENGTH _functions _fn_count)
	list(LENGTH _data _data_count)
	math(EXPR _total_functions "${_total_functions} + ${_fn_count}")
	math(EXPR _total_data "${_total_data} + ${_data_count}")

	if (_fn_count OR _data_count)
		# per-source comment headers keep the def navigable: when a
		# LNK2001 names a symbol, finding it here says which library it
		# came from
		get_filename_component(_file_name "${_file}" NAME)
		string(APPEND _def_content
			"; source: ${_file_name} (${_fn_count} functions, ${_data_count} data)\n")
		string(REPLACE ";" "\n" _fn_blob "${_functions}")
		string(APPEND _def_content "${_fn_blob}\n")
		if (_data_count)
			list(TRANSFORM _data APPEND " DATA")
			string(REPLACE ";" "\n" _data_blob "${_data}")
			string(APPEND _def_content "${_data_blob}\n")
		endif()
	endif()
endforeach()

# write-if-different (see _def_content above): identical content keeps
# the old mtime
set(_def_write_needed TRUE)
if (EXISTS "${ZIRCON_GENERATE_EXPORTS_OUT}")
	file(READ "${ZIRCON_GENERATE_EXPORTS_OUT}" _def_existing)
	if ("${_def_existing}" STREQUAL "${_def_content}")
		set(_def_write_needed FALSE)
	endif()
endif()

if (_def_write_needed)
	file(WRITE "${ZIRCON_GENERATE_EXPORTS_OUT}" "${_def_content}")
else()
	message(STATUS
		"[zircon]: ${ZIRCON_GENERATE_EXPORTS_OUT} unchanged — write skipped (mtime preserved)")
endif()

math(EXPR _total_exports "${_total_functions} + ${_total_data}")
message(STATUS
	"[zircon]: ${ZIRCON_GENERATE_EXPORTS_OUT}: ${_total_functions} functions + ${_total_data} data exports")

if (_total_exports GREATER 60000)
	message(WARNING
		"[zircon]: export count ${_total_exports} approaches the 65535 dll export limit — curate the generator inputs")
endif()
