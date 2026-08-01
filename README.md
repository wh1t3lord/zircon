# zircon

[![build](https://github.com/wh1t3lord/zircon/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/wh1t3lord/zircon/actions/workflows/build.yml)
[![tests](https://github.com/wh1t3lord/zircon/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/wh1t3lord/zircon/actions/workflows/tests.yml)

zircon is a game engine built on the [kotek](https://github.com/wh1t3lord/kotek)
framework — and its practical proof. It is the second layer of the stack
(**kotek** → **zircon** → game content) and is developed as an embedded-style
foundation: strict memory budgets, static data structures, and streaming over
materialization.

For non-specialists: if kotek is the groundwork, zircon is the building that
proves the groundwork holds. Everything the framework promises — modularity,
replaceability, discipline — this engine consumes, stresses, and demonstrates
in practice.

## What zircon demonstrates

- **Embedded discipline enforced at compile time.** The engine builds only
  against kotek's embedded configuration — static containers, no hidden
  reallocation — and a hard compile-time guard rejects any other configuration.
  This is a rule the codebase enforces, not a convention it hopes for.
- **Full-retention undo/redo.** An append-only journal with periodic snapshots:
  history is never truncated and survives restarts. Verified by a 100,000-command
  randomized stress test — full undo to origin is byte-identical, redo replays
  identically, and the journal stays compressed and bounded on disk.
- **Editor and game sessions.** An ImGui-based editor (command history,
  inspector, and other tool windows, with a render-pass management window in
  active development) runs beside the game session, each with its own render
  pipeline.
- **A two-project render architecture.** An executor (render graph and GPU
  resource management) is separated from a pass library that is being made
  hot-swappable: edit a render pass, rebuild one library, and the editor
  reloads it without restarting. bgfx drives rasterization; NVIDIA NRI
  (DirectX 12, phase one operational) is the Vulkan/ray-tracing path.
- **The engine as a replaceable module.** zircon itself builds as a single
  loadable module (`game.ktk`) of kotek's launcher — the framework's plugin
  philosophy applied to the engine itself.

## Who this repository is for

- **Engine programmers** evaluating architecture: the engine shows the
  framework's contracts under real load, including their failure modes and how
  they were fixed (documented in `AGENTS.md`).
- **Students**: a complete, honest example of an engine layer — sessions, ECS,
  command history, render graphs — written to be read.
- **Managers and reviewers**: this is the second of two substantial, working
  repositories designed and maintained end-to-end by one engineer.

## Building

Requirements: a C++20 compiler and CMake 3.19.3+. kotek is a git submodule.

```
git clone --recursive https://github.com/wh1t3lord/zircon.git
cd zircon
mkdir build && cd build
cmake ..
cmake --build .
```

Run from the repository root (data folders are resolved relative to it):

```
build/bin/Debug/kotek.exe --no_splash --kotek_frames=30     # boot, 30 frames, exit
build/bin/Debug/kotek.exe --editor_imgui                     # editor with ImGui UI
build/bin/Debug/kotek.exe --render_nri_dx12                  # NRI (DirectX 12) renderer
```

## Status and verification

CI builds the engine on every push in the default and full-static
configurations, and nightly across the full matrix (Debug/Release ×
default/static/dynamic — dynamic is a documented, intentionally tolerated
limitation of the current module graph). The test workflow boots the real
engine on the runner: 227 framework tests and 14 engine functional tests,
including the 100,000-command history stress suite.

## About the author

zircon and the kotek framework beneath it are designed and implemented by a
single engineer ([wh1t3lord](https://github.com/wh1t3lord)) — architecture,
engine systems, editor, rendering infrastructure, build and CI pipelines, and
tests. The work is characterized by interface design intended to outlive its
implementations, embedded-grade memory discipline, and an insistence that
every claim in the documentation is verifiable in the repository.
