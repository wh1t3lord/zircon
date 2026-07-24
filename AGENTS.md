# AGENTS.md — zircon (engine layer, layer 2)

> Living document. Every agent working in this repo MUST read this first and MUST
> update the Task Registry (status + date + notes) when it advances or finishes a task.
> Also read `kotek/AGENTS.md` — zircon is built on kotek and must not violate its rules.
> Last updated: 2026-07-23 (Z6 done: history fixes verified by stress/restore suites;
> console etl-terminator crash fixed; CRT leak-dump flood contained — see §5).

## 1. What zircon is

- **Layer 2** of the stack: `kotek (core framework) → zircon (engine) → layer 3 (game
  content, NOT built and NOT our job now)`.
- Zircon is the **practical proof** of kotek: it consumes kotek's interfaces, registers
  itself as the user game module (`game.ktk`, built from `src/engine`), and must remain
  an **embedded-style** engine foundation: efficient, shrinkable, maintainable, simple.
- kotek is a **git submodule** (`kotek/` → github.com/wh1t3lord/kotek). Commits inside
  `kotek/` go to the kotek repo; then bump the submodule pointer here.

## 2. Philosophy & style rules (owner's intent — enforce strictly)

1. **NO dynamic containers in zircon — embedded discipline, enforced.**
   Only etl-based / static containers, reached through kotek's switchable
   aliases (`ktk_vector`, `kotek::static_vector_t`, `array_t`,
   `static_cstring_t`, `static_unordered_map_t`, ...), and the aliases must
   resolve to their static form under kotek's embedded configuration
   (task Z12 adds a hard `#error` guard so zircon only compiles against
   that config). Raw `std::vector/std::string/raw char[]` or anything that
   reallocates behind your back in `src/` (outside `src/tools`) is a defect.
   Memory budget rules: allocations are rare and bounded; prefer smaller
   arithmetic types (`short`/`char` over `int`) wherever the value range
   provably allows it; prefer **streaming** (append-only journal, chunked
   IO) over materializing whole blobs in RAM. The undo/redo journal is the
   reference design: per-command *deltas* (never full state), fixed-size
   buffers, disk spill, zstd-compressed blocks, snapshots every N.
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
8. **Unit tests are living code — always actualized.** Every test must match the
   current codebase's contracts: when behavior changes, the test changes in the
   same commit. A test that no longer matches reality is refactored or deleted
   (context decides) — never left rotting. Tests stay functional proofs of what
   a class/module PROMISES (§7 test philosophy), not per-method formalities.
   Coverage bar (task Z14): every class and every public function in zircon
   gets one; cover behavior and edge cases, not happy paths only.
9. **Memory, streaming, cache (the standing engineering bar).** Memory is a
   budgeted resource: every capacity is a named preprocessor constant
   (`zircon_DEF_*` / `KOTEK_DEF_*`), sized from measured data — never a magic
   number. Container choice: `array_t` when the count is fixed at compile
   time (zero overhead, fully constexpr); `static_vector_t` when bounded but
   variable; a hash map only with written justification (N, load factor,
   access pattern) — the default is lookup-table-on-vector (rule 2).
   Arithmetic types: smallest that provably fits for *stored* data
   (`char`/`short` over `int`); native word for loop counters and hot math.
   Hot iteration runs over contiguous, pointer-free storage (SoA where a
   system touches one field of many entities — the ECS dense arrays are the
   reference); no per-element heap nodes in hot paths. Big or unbounded data
   is **streamed**: fixed-size chunks, bounded queues, double buffers — never
   materialize a whole file/scene/journal in RAM when a forward cursor
   suffices (the undo/redo journal is the reference implementation).
   Scalability rule: code must scale DOWN (embedded/consoles) and UP (PC)
   purely by changing preprocessor capacities, not by changing code.

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
  the render thread via `ktkIImguiWrapper`. MT support now exists in kotek
  (K13, 2026-07-22): `ktkIImguiContextManager` via
  `Get_ImguiWrapper()->Get_ContextManager()` — model 1 single-UI-thread
  (current default; the pass adopts its context), model 2 context-per-thread
  + `ktkImguiLockGuard` serialization. Pass authors must use the wrapper for
  ALL ImGui calls (raw `ImGui::` breaks PLUGIN-mode linking — see K20).

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
- **Console assert-dialog handling**: `_CrtSetReportMode/_CrtSetReportFile/
  _set_error_mode(_OUT_TO_STDERR)` now set at `kotek.exe` entry in Debug —
  CRT asserts print to stderr and abort (code 3) instead of hanging on a
  modal dialog. For stuck dialogs from any other tool, `taskkill //F //IM
  kotek.exe` clears them; UI-automation clicking is possible but avoided.
- **Cross-CRT heap frees (root-caused 2026-07-23, mitigated)**: `/MT` gives
  kotek.exe and game.ktk separate debug-heap block lists; objects with
  header-inline code (std::string with `_ITERATOR_DEBUG_LEVEL=2` proxies,
  etl/std containers) crossing module boundaries get freed by the wrong CRT →
  `__acrt_first_block == header` (debug_heap.cpp:996) plus list poisoning that
  can HANG a later free (an engine-config `delete` stormed 5.68M asserts).
  Mitigation: `_CRTDBG_ALLOC_MEM_DF` off in BOTH modules via
  `#pragma init_seg(compiler)` statics (must run before global ctors like
  `g_main_manager` — plain calls at main()/module-init are too late), so
  blocks free straight through to the shared process heap. Residual: a few
  asserts from blocks allocated by CRT startup (pre-flag) — tolerated.
  **Real fix is K9 (shared allocator / new+delete override) — then no
  cross-CRT free exists at all.** Until then: `ShutdownModule_Core_
  Engine_Config` intentionally LEAKS the config (deleting it walks
  cross-module string members; OS reclaims at exit). Same hazard class as
  the etl-terminator rule in Z11: do not share heap-owning objects across
  module boundaries; construct+destroy in the consuming module.
- **Test-found issues (2026-07-23)**: `Zircon_Game.
  ResourceManagerLoadTextResourceNoCache` aborts at
  `zircon_resource_manager.cpp:163` (resource desc_id invalid after the
  test's own file write+load of `rsltrnc.json` — load path bug, not data
  env). Z6 replay hits `ecs_is_entity_ready` (pico id lifecycle on undo —
  see Z6).
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
- **RESOLVED (2026-07-23)**: the `RegisterConsole_Commands` segfault (see
  Z11 notes for the full fix). The earlier "etl pool exhaustion"
  hypothesis below was a red herring from a minimal repro: the real
  cause is that etl's intrusive-list `terminator` is a **module-local
  static** (`intrusive_forward_list_base<TLink>::terminator`, one per
  module per link type), so an etl container constructed in one module
  and driven by another module's header-inline code never matches the
  using module's terminator and walks off the bucket list into
  `equal_to` on garbage (AV). Rule: every object whose inline/template
  methods touch etl containers must be constructed in the module that
  uses those methods — the console is therefore built in game.ktk now
  (engine's instance is saved/restored around it). Same family as the
  CRT issue below: per-module statics + cross-module inline code.
- **Cross-CRT heap diagnostics (2026-07-23; residual markers tolerated
  until K9)**: with static MTd, kotek.exe and game.ktk each keep a CRT
  debug block list but share one process heap. Heap-owning objects
  crossing the module boundary (`std::string` incl. its
  `_ITERATOR_DEBUG_LEVEL=2` `_Container_proxy`, by-value `ktk::cstring`
  interface returns like `GetRenderName()`) are freed by the other CRT;
  `_free_dbg`'s unlink path asserts `__acrt_first_block == header`
  (debug_heap.cpp:996) whenever the freed block is the head of its home
  list — a non-fatal report that also poisons both debug lists.
  kotek.core.memory.cpu force-enables
  `_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF`
  (`main_core_memory_cpu_dll.cpp:23`), and the **exit leak dump** then
  walked the poisoned lists into a multi-minute, multi-million-line
  assert flood (exit 1/139). Containment landed: the dump is disabled
  again at each module's shutdown (`kotek.exe` main after
  ShutdownEngine, zircon `ShutdownModule_Game`) — allocation tracking
  itself stays on. Verified 2026-07-23: full boot + tests + frame loop
  + shutdown reach **exit 0** with only ~4-6 discrete residual markers
  of the same cross-CRT class. The proper fix is kotek-side: stop
  passing heap-owning objects across module boundaries or share one
  CRT (K9); do not "fix" it by hiding reports.

## 6. Task Registry (owner's tasks — update status as work happens)

| ID | Task | Status | Notes |
|----|------|--------|-------|
| Z1 | Fix compilation issues (whole solution, default config) | done (2026-07-21) | full Debug build green; see §5 CRT/imgui notes + PICO factory gaps below |
| Z2 | Formulate style & philosophy | done (2026-07-21) | §2 of this file; refine as owner corrects |
| Z3 | Render restructure: split `render/bgfx` into TWO projects — (a) passes (dynamic/hot-swappable), (b) render graph + resource manager executor; validate hot-reload of pass library (see §7 verdict) | open | vk/gles3 deletion part DONE (see Z10); split + hot-reload pending; passes lib must be reload-safe: destroy passes BEFORE unload, recreate after |
| Z4 | Document codebase style (preprocessors, memory allocation patterns) | done (2026-07-21) | §2/§3; keep in sync with reality |
| Z10 | gles3/vk backend removal (owner directive) | done (2026-07-22) | `src/render/{gles3,vk}` deleted; `src/render/CMakeLists.txt` rewritten bgfx-only; `zircon_renderer_bgfx` is the only renderer (union member `p_gles3` + all vk/gles3 branches excised from `zircon_game_manager`); `validate_extensions` (dead GL-era) removed; os console pass retargeted to the bgfx pass base (OnUpdate/OnRender signature updated with `my_id_in_queue`) |
| Z11 | Runtime boot chain (kotek.exe runs from repo root) | done (2026-07-23) | FIXED earlier: STD-mode `ktkJson::Get` stub (kotek bug, config never parsed in STD); `dll::shared_library` move semantics; `program_location`; stray `KOTEK_ASSERT(false)` in `Initialize_ResourceManager`; window-console stub assert → warning; world init with factory; `--kotek_frames=N`; splash busy-wait bounded. FIXED 2026-07-23 evening: console `Register_Command` segfault = **etl intrusive-list terminator is a module-local static** — a container built by one module (exe) terminates buckets with an address the other module (game.ktk) never matches, any cross-module insert walks off the end (null+8). Fix: game manager constructs its OWN console and restores the engine's at shutdown (`m_p_console_exe_owned`), ownership stays in the constructing module — THE rule for every object whose inline/template methods touch etl containers. Render shutdown dispatch: stray `if (is_gl)` swallowed the whole else-if chain (bgfx shutdown never ran → `status=false` assert). `zircon_config`: serialize wrote 1024B fixed buffer (garbage tail → json "extra data"), now writes `text_real_length`; deserialize parsed `sizeof(text)` instead of the Read_File-returned size. `matrix2x2_f::e/c`: 6 inverted range asserts (fired on VALID indices). VERIFIED: boot → 14/14 + 163/163 tests → 30 frames → full shutdown → **exit 0**. OPEN: splash thread can hang main-window init (create windows on main thread only); 6 residual CRT-startup heap asserts (pre-flag blocks, tolerated until K9) |
| Z5 | Passes for editor AND game, for bgfx AND NRI; NRI gets own folder, same two-project split (passes + executor) | open | depends on kotek K11 (NRI backend) |
| Z6 | Undo/redo: store ALL history (no cutting), reliable restore; assess design & shrinkability | done (2026-07-23) | journal+registry+history rewrite (previous agent) + four fixes landed: (1) **replay crash** `ecs_is_entity_ready` (pico_ecs.h:1102): delete-undo/create-redo reincarnate an entity under a fresh id and the flat recorded→live map went stale for OTHER aliases of the same logical entity (repro chain 12→18→47) — fixed with a reincarnation chain (`m_entity_reincarnation`: incarnation id → next id, values strictly increase so walks always terminate) + history-side before/after `GetEntityID` observation in `Undo()`/`execute_node()` (commands no longer call `update_dependent_commands` themselves — journal-reconstructed ones only know recorded ids and wrote chain-skipping links) + `apply_world_state` rebinds whole chains on snapshot restore. (2) **redo-to-final diverged**: the test mutated components directly (unjournaled edits are unreplayable by design) — added journaled `zircon_command_edit_component_state` (registry type `zircon_DEF_COMMAND_TYPE_EDIT_COMPONENT_STATE`, delta = before+after json) and the stress mix drives edits through it. (3) test scan watermark must resync from `get_entity_watermark()` (pico never recycles, redo mints fresh ids past the model's range → empty serialize). (4) `ResourceManagerLoadTextResourceNoCache` abort (`zircon_resource_manager.cpp:163`): sync `load` never allocated desc/view ids (async path did) and `allocate_desc/view` returned indices into a reserved-but-never-grown vector (OOB on use) — both fixed. VERIFIED full suite green: `CommandHistory_Stress_100k_Commands` (~63k executed ops over 100k iterations: 100% recorded, full undo-to-origin byte-identical, full redo byte-identical, journal ≥2× compression + ≤64 MB disk, journal reopen retains every command) + `CommandHistory_Restore_Node_From_Snapshots` + 12 Zircon_Game + 163 kotek tests, all PASSED, exit 0 |
| Z7 | Enforce static-container/`ktk`-alias rule consistently across `src/` | open | violation list in §5 |
| Z8 | Track codebase TODOs | open | ~130 hits in `src/`; major clusters: command history, game manager, factory `#error`s, render passes |
| Z9 | Make zircon layer SHARED-capable (break editor cycles) | open | found 2026-07-22 during K18 validation: `zircon.editor.session` ↔ `zircon.editor.commands` ↔ `zircon.editor.ui` are cyclic at symbol level (session constructs command_history + ui_state; commands/ui call session getters). Static linking hides it; DLLs forbid it. Fixes (choose): (a) merge the 3 editor targets into one DLL; (b) extract interfaces (`zircon_interface_command_history`, `zircon_interface_editor_ui_state`) into `zircon.core` + register via locator, i.e. apply kotek's own ktkI* discipline to zircon — preferred, matches engine philosophy. Other zircon modules (core/ecs/game/game.session/world) already link fine as DLLs |
| Z12 | Replace every raw container in `src/` with switchable kotek aliases + hard-require kotek's embedded config | open | owner directive 2026-07-23: zircon must not contain raw `std::vector/std::string/raw char[]` at all (only `ktk_*` aliases resolving to etl/static). Add a compile-time `#error` guard (one central zircon header included everywhere, e.g. via the PCH) that fails the build unless kotek's embedded container configuration is active — dynamic (regular STL) and hybrid containers disabled at preprocessor level. Kotek side: verify/implement that config (`KOTEK_USE_LIBRARY_TYPE_EMB` + friends exist in alias headers; check it covers ALL containers and actually compiles — HYB is broken today, EMB state unverified). Until the guard lands, the Z7 violation list is the migration backlog |
| Z13 | CI/CD green on GitHub + three linkage configurations in CI | in-progress (2026-07-23) | workflows build/tests exist and run on every push; fixed so far: vcpkg cache 6.5GB→1.2GB (quota), tests workflow runs the real engine boot with `KOTEK_ASSERT_STDERR_ROUTING=ON`, vcpkg pinned to the validated snapshot (29fb1f4, was floating master). STILL RED at configure on the CI image (vcpkg port build; job logs are admin-only — need owner access or the K16 nuget binary cache). Owner requirement: CI compiles THREE configurations — full static (all .lib + kotek.exe), full dynamic (all .dll + kotek.exe; **blocked today by kotek's cyclic module graph, K18 — CI leg exists as continue-on-error**), and the default (kotek static + game.ktk) |
| Z14 | Unit tests for every class and public function (both repos, kotek K22) | open | owner directive 2026-07-23: functional proofs, not per-method formalities — behavior + edge cases + stress where the contract promises it (the 100k-command history stress is the reference). Rule 8 (tests are living code) governs maintenance |
| Z15 | Plugin override system exists at kotek level (kotek K21) — informational | done at kotek level (2026-07-23) | `KOTEK_INVOKE_MODULE` is override-first in EVERY linkage mode: drop `plugins/<module-folder-name>.dll` (or map it in `plugins/plugins.json`, json wins) next to the data dirs and the user's dll replaces that kotek module's `InitializeModule_*/ShutdownModule_*/Serialize/Deserialize` entries; `--kotek_plugins_template` / `--kotek_plugins_modules` codegen the file skeletons. Zircon's own module entries (`InitializeModule_Render` etc. invoked from `src/engine/main_game_dll.cpp`) go through the same macro, so kotek-module overrides apply to zircon's call sites with zero zircon changes; zircon's OWN modules join the override registry once zircon's cmake re-runs `kotek_generate_plugin_manifest` for its targets (not done — zircon modules are the game layer, overriding them is out of K21's scope). See kotek/AGENTS.md §5a "Plugin overrides" |

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

### Z6 data minimality & compression (owner directive 2026-07-23)

Even with disk streaming, what we write must be minimal. Rules for the implementation:
1. **Journal entries store deltas, not states.** A command serializes only: its type
   id, the target entity, the changed component fields (before/after), and branch
   links. Never a full component blob when one field changed.
2. **Binary format, not JSON, for the journal** (JSON only as the debug
   representation). `zstd` (already a kotek dependency) compresses journal blocks
   and snapshots — commands are highly repetitive and compress ~10-20x.
3. **Snapshots every N commands** (tunable, default ~256): the full ECS state at
   that point, zstd-compressed. Undo across a snapshot boundary = nearest snapshot
   + replay journal forward; undo within = apply inverses backward.
4. **Branching**: nodes form a tree (child pointers + parent id); the linear
   "current path" cursor moves, nothing is ever truncated. New action after undo =
   new child at the cursor's node.
5. **Registration**: command types register into a static type table (id →
   create/deserialize fn) so future editor commands (terrain ops, prefab ops,
   component-array edits, multi-entity batch ops — see "Future editor commands"
   below) join the journal with zero history-manager changes.

### Future editor commands to keep in mind (design the journal for them)

- batch/multi-entity commands (delete N entities — the existing 30k+ JSON concern
  in `zircon_command_delete_entity.h:13` must be a delta list, not N full serializations)
- component field-array edits (terrain brushes, tile ops — high frequency; journal
  must not thrash; coalescing window: same entity+field within T ms merges entries)
- prefab instantiate/remove
- scene load/save as journaled operations (or explicitly excluded — decide)

### Unit-test philosophy (owner directive 2026-07-23)

Tests are functional proofs, not per-method formalities: each test targets what a
class/module PROMISES. For the command history specifically: a stress suite —
~100k randomized commands across all command types, verifying (a) every entry was
recorded, (b) full undo to origin restores byte-identical state, (c) redo replays
to the same final state, (d) journal+snapshot sizes stay within compression
budgets. Same pattern for other modules: containers (capacity/overflow/UB edges),
factory (create/destroy/serialize roundtrips at scale), sessions, resource manager.
Editor+game integration test: boot the engine (--no_splash --kotek_frames=N),
create a scene, issue real commands, verify state.

## 8. Open questions awaiting owner

1. Priority order for execution (proposed: ~~Z1~~ → ~~K1~~ → Z7/Z6 → K4/K9 → Z3/K11 → tool K2 → CI K5 → docs K7 last).
2. ~~K2 VS2013~~ RESOLVED (2026-07-22): VS13 dropped; offline solutions for VS17/19/22 only.
3. K2 tool scope: parse the kotek/zircon CMake *dialect* (subset) — full CMake language is out of scope for a <1 MB tool. Confirm.
4. K16: NuGet API key delivery (CI secret vs local).
5. K11: does "delete GAPIs except bgfx" also cover `kotek.render.software` and ANGLE modules? And kotek-side gl/vk shared projects?
6. Confirm deletion of `zircon.render.vk` + `gles3` is fine to execute as part of Z3 (destructive).
