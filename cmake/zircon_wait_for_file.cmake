# zircon_wait_for_file.cmake — bounded file-existence gate for build
# steps (runs as `cmake -P`). Polls for ZIRCON_WAIT_FILE to appear and
# fails loudly after the timeout. Used by the graphics-development
# passes DLL (task Z3 P3b): its link consumes game.ktk's import library
# BY PATH, and no target edge may order the DLL after `zircon` — that
# edge is exactly what would pull game.ktk into every mid-run
# `--target zircon.render.passes.bgfx` build (the twin sits in
# game.ktk's static closure, so a passes-only edit would force a
# game.ktk relink, LNK1168 while the editor has it loaded). On a fresh
# full build the DLL can reach its link before game.ktk's link produced
# the import library; ALL_BUILD contains `zircon`, so the file always
# appears and the wait succeeds. A bare `--target` build on a tree
# game.ktk was never built in hits the timeout with an actionable
# message instead of a cryptic LNK1104. NOTE: with a strictly serial
# build schedule (-m:1) where the DLL is built before `zircon`, the
# wait cannot succeed inside the same build — use a parallel build.
#
# Inputs (-D):
#   ZIRCON_WAIT_FILE         full path of the awaited file
#   ZIRCON_WAIT_DESCRIPTION  human-readable name for messages
#   ZIRCON_WAIT_TIMEOUT      seconds before failing (default 1800)

if (NOT ZIRCON_WAIT_FILE)
	message(FATAL_ERROR
		"[zircon]: zircon_wait_for_file: ZIRCON_WAIT_FILE is not set")
endif()

if (NOT ZIRCON_WAIT_TIMEOUT)
	set(ZIRCON_WAIT_TIMEOUT 1800)
endif()

set(_waited 0)
while (NOT EXISTS "${ZIRCON_WAIT_FILE}")
	if (_waited GREATER_EQUAL ${ZIRCON_WAIT_TIMEOUT})
		message(FATAL_ERROR
			"[zircon]: timed out after ${ZIRCON_WAIT_TIMEOUT}s waiting for ${ZIRCON_WAIT_DESCRIPTION} at '${ZIRCON_WAIT_FILE}' — build the full solution (ALL_BUILD) once so the producing target creates it, then retry")
	endif()

	math(EXPR _wait_minute_mark "${_waited} % 60")
	if (_waited GREATER 0 AND _wait_minute_mark EQUAL 0)
		message(STATUS
			"[zircon]: waiting for ${ZIRCON_WAIT_DESCRIPTION} (${_waited}s elapsed): ${ZIRCON_WAIT_FILE}")
	endif()

	execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 5)
	math(EXPR _waited "${_waited} + 5")
endwhile()
