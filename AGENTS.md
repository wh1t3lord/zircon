# AGENTS.md — zircon (engine layer, layer 2)

> Living document. Every agent working in this repo MUST read this first and MUST
> update the Task Registry (status + date + notes) when it advances or finishes a task.
> Also read `kotek/AGENTS.md` — zircon is built on kotek and must not violate its rules.
> Last updated: 2026-07-21 (initial creation after full codebase analysis).

## 1. What zircon is

- **Layer 2** of the stack: `kotek (core framework) → zircon (engine) → layer 3 (game
  content, NOT built and NOT our job now)`.
- Zircon is the **practical proof** of kotek: it consumes kotek's interfaces, registers
  itself as the user game module (`game.ktk`, built from `src/engine`), and must remain
  an **embedded-style** engine foundation: efficient, shrinkable, maintainable, simple.
- kotek is a **git submodule** (`kotek/` → github.com/wh1t3lord/kotek). Commits inside
  `kotek/` go to the kotek repo; then bump the submodule pointer here.

## 2. Philosophy & style rules (owner's intent — enforce strictly)

1. **Static containers only in engine runtime code.** Use kotek aliases:
   `kotek::static_vector_t`, `kotek::array_t`, `kotek::static_cstring_t`,
   `kotek::static_unordered_map_t`, etc. Raw `std::vector/std::string/raw char[]` in
   `src/` (outside `src/tools`) is a defect — migrate to `ktk` aliases (task Z7).
   Host tools (`src/tools/cpp_generators`) are exempt.
2. **Lookup tables on vectors by default** (cache-friendly linear search over small N);
   `unordered_map` only when N or access patterns justify it — and the choice must be
   re-validated, not cargo-culted.
3. **Everything through kotek macros**: namespaces (`KOTEK_BEGIN_NAMESPACE_*`),
   diagnostics (`KOTEK_ASSERT/TRACE/MESSAGE`), feature flags (`KOTEK_USE_*`), enum flag
   operators (`KOTEK_IMPLEMENTATION_ENUM_FLAG_OPERATORS`).
4. **No direct third-party includes in engine code** — third-party reaches zircon only
   through kotek's PCH (`<kotek.pch/pch.h>`, force-included in every `zircon.*` target)
   or explicit kotek virtual-path includes like
   `<kotek.render.bgfx/include/kotek_render_graph_simplified.h>`.
5. **Naming**: files/classes `zircon_snake_case`, constants `zircon_DEF_*` /
   `ZIRCON_DEF_*`; CMake targets `zircon.<module>` (STATIC libs today; `src/engine`
   builds `${KOTEK_DEVELOPMENT_TYPE}` → `game.ktk`).
6. **Every module keeps `main_<module>_dll.cpp`** (kotek module-entry convention) even
   though linking is currently static.
7. Memory: allocate via kotek containers/allocators; placement-new into fixed buffers is
   an accepted pattern (see command history storage). No global `new/delete` games in
   zircon — allocator policy belongs to kotek.

## 3. Current architecture map

```
src/
├── core/     zircon.core        config, session interfaces (zircon_interface_session*), defs, console
├── ecs/      zircon.ecs         components + zircon_factory (entt|pico behind KOTEK_USE_ECS_BACKEND_*)
│                                + PRE_BUILD codegen via zircon_generator (libclang host tool)
├── editor/   zircon.editor      asset manager
│             .commands          undo/redo: zircon_editor_command_history (+ concrete commands)
│             .session           editor session/manager
│             .ui                ~15 ImGui windows (inspector, history log, ...)
├── engine/   zircon (= game.ktk) module entry (main_game_dll.cpp) + zircon_game_manager (god object)
│                                + zircon_resource_manager (MT) + tests
├── game/     game-side asset manager, game session
├── render/   zircon.render      backends: bgfx/ gles3/ vk/ os/ (console pass)
├── tools/    zircon_generator   ecs_fields_generator (libclang; std:: allowed here)
└── world/    zircon.world       world + world manager (owns zircon_ecs_context_t)
```

- **Render (bgfx)**: `zircon_renderer_bgfx : ktkIRenderer` holds up to
  `ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT` (=2) `ktkRenderGraphSimplified`
  instances (one per session: game/editor). Passes derive from
  `zircon_render_graph_pass[_editor]_bgfx` → kotek `ktkRenderGraphSimplifiedRenderPass`
  (`OnCreateResources/OnDestroyResources/OnUpdate/OnRender`). Passes live in
  `render/bgfx/passes/no_streaming/`. `gles3/` is currently a file-for-file copy of
  `bgfx/` (to be deleted, task Z3). `vk/` is excluded from build (to be deleted).
- **Entry**: kotek's `kotek.exe` (`main`) → `Engine::Initialize/Execute/ShutdownEngine`
  → loads `game.ktk` (SHARED dev type) or direct calls (STATIC) → zircon's
  `InitializeModule_Game / InitializeModule_Render / UpdateModule_Game /
  ShutdownModule_Game` (`src/engine/main_game_dll.cpp`).
- **ImGui**: single global context, created in the bgfx editor-imgui pass; all calls on
  the render thread via `ktkIImguiWrapper`. No MT support today (task K13 in kotek).

## 4. Build

```bash
mkdir build && cd build
cmake ..            # VS generator on owner machine; configures kotek too (subdirectory)
cmake --build .
```
Outputs: `build/bin` (`kotek.exe`, `game.ktk`), `build/lib`.
Requires kotek deps (vcpkg minimal set auto-fetched on first configure — slow first run).
Gotcha: root `CMakeLists.txt:66` has `add_dependencies(kotek zircon)` referencing a
non-existent target name in some configs — verify when touching root CMake (task Z1).

## 5. Known issues / debts (verified 2026-07-21)

- **Compilation**: fixed (2026-07-22, full Debug build green: `kotek.exe` +
  `game.ktk` link). If a build fails with `C1076/C3859` (heap/PCH virtual
  memory) it is machine RAM pressure from parallel `cl.exe` + the heavy kotek
  PCH, not code — rerun with `cmake --build . --config Debug --parallel 4`.
  CRT is back to **static (MTd)** (2026-07-22): vcpkg deps rebuilt with the
  `x64-windows-static` triplet (`KOTEK_VCPKG_TRIPLET` in `kotek/CMakeLists.txt`).
  Module linkage is now configurable: `KOTEK_LINKAGE=STATIC|SHARED` plus
  per-module `KOTEK_LINKAGE_FORCE_STATIC/FORCE_SHARED` lists
  (see `kotek/cmake/library.cmake`); all zircon modules go through
  `kotek_add_library`.
- **Undo/redo stores only ≤10 commands in RAM** (ring `m_commands[10]` + placement
  storage) and disk streaming is stubbed out with ~15 `KOTEK_ASSERT(false,
  "todo: re-write please")` in `src/editor/commands/zircon_command_history.cpp`.
  History is *cut* on new-action-after-undo (`clear_content_when_action_issued`,
  `:2450`) and wiped on `shutdown()`. Owner wants FULL history retention (task Z6).
- **Container-rule violations** (task Z7): `kotek::vector_t` in
  `zircon_renderer.h:96`, `zircon_game_manager.h:192`, `zircon_session_editor.h:105`;
  raw `std::thread/std::lock_guard/std::string_view` in resource manager/console;
  raw `char[]` in some commands; `entt::` leftovers; stale duplicate
  `src/editor/ui/zircon_sdk_ui.h`; `Kotek::` vs `kotek::` case inconsistency.
- Render: game pass `zircon_render_graph_pass_model_static.*` are empty stubs;
  `bgfx|gles3/zircon_render_resource_manager.h` marked `todo: delete`;
  `zircon_renderer_bgfx::Get_Name` returns the GLES3 name; `Add_PassesEditor/Game`
  commented out; CMake has stale `gl3_3` lists.
- **Linkage scenarios (2026-07-22)**: kotek implements the three output modes
  (`KOTEK_LINKAGE=STATIC|SHARED|PLUGIN` — see kotek/AGENTS.md §5a). Zircon
  modules participate via `kotek_add_library`; the cyclic editor cluster
  (Z9) blocks zircon from full SHARED/PLUGIN — until Z9 is done, force it
  static: `-DKOTEK_LINKAGE_FORCE_STATIC="zircon.editor.commands;zircon.editor.session;zircon.editor.ui"`.
- ECS: `zircon_factory.cpp` has `#error todo: provide impl` branches; broken comment
  at `zircon_component_geometry.cpp:198`.
- **PICO factory filled-in during Z1 (owner review wanted)**:
  `zircon_factory::register_components` (per-type switch over
  `eZirconComponentType` — must be kept in sync manually, candidate for CMake
  codegen), `register_components_and_their_enums` (intentional no-op),
  `has_component`/`create_component`/`remove_component`/`get_component_by_name`/
  `get_component_enum_by_name` (linear enum scan)/`get_all_entities` (dense-id
  scan via `ecs_is_ready`, capped by caller buffer). `zircon_world` caches a
  non-owning `zircon_factory*` again (`get_factory()`), factory itself is owned
  by `zircon_game_manager`. `zircon_component_interface::draw_imgui` is gone —
  component inspector no longer draws component fields (TODO(zircon) in
  `zircon_ui_component_inspector.cpp`). `IMGUI_IMPL_OPENGL_ES3` define in
  vendored `imgui.h` disabled (TODO(zircon)) because ANGLE import libs are
  absent in the minimal vcpkg config.
- CMake: repeated `# TODO: add if when game.ktk builds as shared` in 10+ module
  CMakeLists; root `add_dependencies(kotek zircon)` suspicious.
- `zircon_config.h` TODO: replace raw char buffers with `static_cstring_t`.

## 6. Task Registry (owner's tasks — update status as work happens)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| Z1 | Fix compilation issues (whole solution, default config) | done (2026-07-21) | full Debug build green; see §5 CRT/imgui notes + PICO factory gaps below |
| Z2 | Formulate style & philosophy | done (2026-07-21) | §2 of this file; refine as owner corrects |
| Z3 | Render restructure: DELETE `render/vk` + `render/gles3`; split `render/bgfx` into TWO projects — (a) passes (dynamic/hot-swappable), (b) render graph + resource manager executor; validate hot-reload of pass library (see §7 verdict) | open | passes lib must be reload-safe: destroy passes BEFORE unload, recreate after |
| Z4 | Document codebase style (preprocessors, memory allocation patterns) | done (2026-07-21) | §2/§3; keep in sync with reality |
| Z5 | Passes for editor AND game, for bgfx AND NRI; NRI gets own folder, same two-project split (passes + executor) | open | depends on kotek K11 (NRI backend) |
| Z6 | Undo/redo: store ALL history (no cutting), reliable restore; assess design & shrinkability | open | see §7 verdict + §5 truncation points |
| Z7 | Enforce static-container/`ktk`-alias rule consistently across `src/` | open | violation list in §5 |
| Z8 | Track codebase TODOs | open | ~130 hits in `src/`; major clusters: command history, game manager, factory `#error`s, render passes |
| Z9 | Make zircon layer SHARED-capable (break editor cycles) | open | found 2026-07-22 during K18 validation: `zircon.editor.session` ↔ `zircon.editor.commands` ↔ `zircon.editor.ui` are cyclic at symbol level (session constructs command_history + ui_state; commands/ui call session getters). Static linking hides it; DLLs forbid it. Fixes (choose): (a) merge the 3 editor targets into one DLL; (b) extract interfaces (`zircon_interface_command_history`, `zircon_interface_editor_ui_state`) into `zircon.core` + register via locator, i.e. apply kotek's own ktkI* discipline to zircon — preferred, matches engine philosophy. Other zircon modules (core/ecs/game/game.session/world) already link fine as DLLs |

## 7. Design verdicts (2026-07-21 analysis — basis for Z3/Z6)

**Hot-reloadable pass library (Z3 idea, 'graphics development' flag):** feasible with
strict discipline: objects created inside a DLL do NOT survive its unload. Flow must be:
GPU idle → destroy all passes → `FreeLibrary` → load new DLL → create passes →
re-register into render graph. So: passes cannot "continue working" across reload;
they are recreated. Editor picks pass sets per session (editor vs game) from config.
Keep pass DLL free of persistent global state; all GPU resources owned by the
executor/resource-manager side, passes reference them by handle.

**Full-retention undo/redo (Z6):** replace "ring of 10 + truncate-on-new-action" with
**append-only journal + periodic snapshots**: every command appends (JSON now, binary
later — see `zircon_command_definitions.h:33`) with its inverse data; every N commands
serialize an ECS snapshot. Undo = move cursor + apply inverse (or replay from nearest
snapshot); new action after undo = **new branch node**, never deletion (tree of
history, like Emacs/persistent undo — restorable always). Persist journal on disk;
remove the `shutdown()` folder wipe. Memory stays bounded via snapshots. The command
interface (`ktkISDKRedoUndo`: Execute/Undo/Serialize) is adequate; the history manager
is what gets rewritten.

## 8. Open questions awaiting owner

1. Priority order for execution (proposed: ~~Z1~~ → ~~K1~~ → Z7/Z6 → K4/K9 → Z3/K11 → tool K2 → CI K5 → docs K7 last).
2. ~~K2 VS2013~~ RESOLVED (2026-07-22): VS13 dropped; offline solutions for VS17/19/22 only.
3. K2 tool scope: parse the kotek/zircon CMake *dialect* (subset) — full CMake language is out of scope for a <1 MB tool. Confirm.
4. K16: NuGet API key delivery (CI secret vs local).
5. K11: does "delete GAPIs except bgfx" also cover `kotek.render.software` and ANGLE modules? And kotek-side gl/vk shared projects?
6. Confirm deletion of `zircon.render.vk` + `gles3` is fine to execute as part of Z3 (destructive).
