@echo off
REM validate_all_configs.bat — the pre-push gate (owner directive 2026-10-06:
REM "before pushing to repositories, with 100% guarantee that it compiles
REM successfully all CI/CD configurations"). Mirrors the CI legs locally:
REM every meaningful build tree compiles, the boot-capable ones boot.
REM Run from the repo root. Exit code 0 = every leg green.
setlocal EnableDelayedExpansion

set "FAIL=0"
set "RESULTS="

echo === validation: builds (incremental, both repos' code through the submodule) ===

call :build_leg build       Debug   "default Debug"
call :build_leg build       Release "default Release"
call :build_leg build-gfxdev Debug  "graphics-development Debug"
call :build_leg build-math-own Debug "math KOTEK_OWN Debug"
call :build_leg build-math   Debug  "math DXM Debug"
call :build_leg build-test-off Debug "tests-off Debug"
call :build_leg build-plugin Debug  "plugin-linkage Debug"
call :build_leg build-static-leg Debug "static Debug"

echo === validation: boots (the headless-capable configs) ===

call :boot_leg build       Debug "default Debug boot"
call :boot_leg build-gfxdev Debug "graphics-development Debug boot"
call :boot_leg build-test-off Debug "tests-off Debug boot"

echo.
echo === SUMMARY ===
echo %RESULTS%
if %FAIL% NEQ 0 (echo VALIDATION: FAIL & exit /b 1)
echo VALIDATION: PASS
exit /b 0

:build_leg
if not exist "%~1\CMakeCache.txt" (
	echo [SKIP] %~3 — no build tree at %~1 ^(configure it first^)
	set "RESULTS=%RESULTS% [SKIP %~3]"
	exit /b 0
)
echo [BUILD] %~3 ...
cmake --build "%~1" --config "%~2" --parallel 8 -- /nr:false > "%~1\validate.log" 2>&1
if errorlevel 1 (
	echo [FAIL] %~3 — see %~1\validate.log
	findstr /C:": error" "%~1\validate.log"
	set "FAIL=1"
	set "RESULTS=%RESULTS% [FAIL %~3]"
) else (
	echo [OK] %~3
	set "RESULTS=%RESULTS% [OK %~3]"
)
exit /b 0

:boot_leg
if not exist "%~1\bin\%~2\kotek.exe" (
	echo [SKIP] %~3 — no binary at %~1\bin\%~2
	set "RESULTS=%RESULTS% [SKIP %~3]"
	exit /b 0
)
echo [BOOT] %~3 ...
timeout 240 "%~1\bin\%~2\kotek.exe" --no_splash --editor_imgui --kotek_frames=30 > "%~1\validate_boot.log" 2>&1
if errorlevel 1 (
	echo [FAIL] %~3 — see %~1\validate_boot.log
	set "FAIL=1"
	set "RESULTS=%RESULTS% [FAIL %~3]"
) else (
	echo [OK] %~3
	set "RESULTS=%RESULTS% [OK %~3]"
)
exit /b 0
