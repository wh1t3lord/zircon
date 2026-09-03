# AGENTS.md — zircon (engine layer, layer 2)

> Living document. Every agent working in this repo MUST read this first and MUST
> update the Task Registry (status + date + notes) when it advances or finishes a task.
> Also read `kotek/AGENTS.md` — zircon is built on kotek and must not violate its rules.
> Last updated: 2026-07-25 (Z5 phase 1: NRI renderer wired over kotek K11 —
> `--render_nri_dx12` boots and presents, exit 0; see the Z5 row).

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
1a. **NO static storage duration (owner directive 2026-08-02, applies to
   zircon AND kotek).** `static` variables of every kind — file-scope
   globals, function-local `static`, static class members — are forbidden:
   they are hidden module-local singletons and the root of this stack's
   worst defect classes (the etl intrusive-list terminator AV (Z11),
   cross-CRT frees, init-order hazards, never-unload lock states, the
   double-run suite collision of 2026-08-01). **Distinction**: etl
   `static_cstring`/`static_vector` TYPES are unaffected — they name
   capacity, not storage duration. Replacements, in preference order:
   (a) members of the owning class; (b) explicit instances owned by a
   manager/context (the ktkMainManager/session pattern); (c) singletons as
   registered services behind interfaces (the ktkI* locator). Existing
   statics are being swept (tasks Z18/K24 — incl. `_pLoggerMain`-style
   module globals and `g_main_manager`); new static storage in a PR is a
   defect on sight.
1b. **Wrappers only (owner directive 2026-09-02).** Zircon code calls ONLY
   kotek-provided wrappers — never standard-library or third-party
   functions directly (no `std::filesystem::*`, C-runtime or OS APIs, no
   third-party calls — host tools under `src/tools` stay exempt). When a
   needed wrapper is missing, it is added KOTEK-SIDE first with the backend
   matrix (STD / BOOST / kotek-own / user re-registration), and zircon then
   consumes the wrapper. That is what keeps every zircon line switchable
   across backends, CRTs, and platforms.
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
5a. **Shaders are Slang ONLY (owner directive 2026-08-01)** — one shading
   language for users across bgfx AND NRI: sources live in
   `data_game/shaders/slang/*.vs.slang|*.fs.slang` (bare-parameter IO,
   `[[vk::binding]]`+`register` pairs, `[shader("...")]` named entries).
   Pipeline: `slangc -target spirv` → `zircon_shaderpack` (own tool, writes
   bgfx's container) → `shader_cache/bgfx/vulkan/*.bin`;
   `slangc -target dxil` → `shader_cache/nri/dx12/*.dxil` directly. shaderc's
   macro dialect was evaluated and rejected on evidence (2026-08-01 spike:
   it only parses `$input/$output`+`void main` dialect, modern explicit IO
   would need a fragile transpiler). The vendored imgui embedded `.bin.h`
   blobs are the temporary exception (editor-internal reference shaders) —
   they migrate to Slang when the editor shaders join the pipeline.
6. **Namespace form**: the real namespace is `Kotek` (capital, renamable via
   `KOTEK_BEGIN_NAMESPACE_KOTEK`); `kotek.core/include/kotek_core.h` defines the
   lowercase aliases (`namespace kotek/core/render = Kotek::Core/...`).
   **Zircon code uses the lowercase aliases (`kotek::...`) — never write
   `Kotek::` directly** (that bypasses the cmake rename feature the alias layer
   exists for). Mixed forms are Z7 debt: measured 2026-07-24 — 258 direct
   `Kotek::` sites in zircon (task Z16 sweep), 43 in kotek (its own code stays
   on the `KUN_*`/`kun_*` macros).
7. **Every module keeps `main_<module>_dll.cpp`** (kotek module-entry convention) even
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
8a. **Test tiers: heavy suites are flag-gated (owner directive 2026-09-02).**
   Heavy stress suites run ONLY under `ZIRCON_USE_TESTS_HEAVY` (CMake
   `-DZIRCON_TESTS_HEAVY=ON`, OFF by default); every heavy suite keeps a
   lightweight default tier of the same functional proof (the command-history
   stress runs 5,000 ops by default, 100,000 under the flag — the reference
   shape). Debug boot time is a budget: no test may make the default boot
   meaningfully slower without the flag. CI runs the default tier; the heavy
   tier is an opt-in local/CI-matrix configuration.
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
├── render/   zircon.render      executor: zircon_renderer_bgfx + zircon_renderer_nri
│                                + pass bases; os/ console pass (gles3/vk deleted Z10)
│             .passes.bgfx       bgfx passes — STATIC by default; hot-swappable DLL in
│                                graphics-development mode (live reload, Z3 P3b)
│             .passes.nri        NRI passes — STATIC; hand-written zircon_nri_passlib
├── tools/    zircon_generator   ecs_fields_generator (libclang; std:: allowed here)
│             zircon_shaderpack  bgfx shader container packer (the Slang pipeline, §2.5a)
└── world/    zircon.world       world + world manager (owns zircon_ecs_context_t)
```

- **Render (bgfx)**: `zircon_renderer_bgfx : ktkIRenderer` holds up to
  `ZIRCON_DEF_RENDERER_BGFX_MAX_RENDER_GRAPH_COUNT` (=2) `ktkRenderGraphSimplified`
  instances (one per session: game/editor). Passes derive from
  `zircon_render_graph_pass[_editor]_bgfx` → kotek `ktkRenderGraphSimplifiedRenderPass`
  (`OnCreateResources/OnDestroyResources/OnUpdate/OnRender`). Passes live in
  `render/bgfx/passes/no_streaming/` and build as their OWN target
  `zircon.render.passes.bgfx` (STATIC by default; **hot-swappable DLL with live
  reload in graphics-development mode** — shadow-copy loading, polling watcher,
  frame-boundary swap, `reload_render_passes()` console override; Z3 P3b, and
  §7's verdict is now implemented there). `zircon.render` is the executor
  (renderer + graph slots + pass bases); pass creation goes through the
  codegen'd `zircon_render_pass_factory` driven by the config keys
  `render_passes_editor`/`render_passes_game` (comma-separated name lists,
  defaults = present+grid+gizmo_own+imgui / present+model_static), with the
  Render Passes editor window (P2a) and level-file pass sets (P2h) on top.
  **Render (NRI)**: `zircon_renderer_nri` drives a pass list through kotek's
  additive `ktkIRenderFramePass`/`ktkIRenderFramePassContext` surface
  (`Present_With_Passes`; no NRI types in zircon — Z5 P4); NRI passes live in
  `render/nri/passes/` (`zircon.render.passes.nri`, STATIC; hot-reload mirror
  is phase 3). **Shaders**: Slang ONLY (§2.5a) —
  `slangc → spirv → zircon_shaderpack → shader_cache/bgfx/vulkan`,
  `slangc → hlsl → fxc → pack → shader_cache/bgfx/dx11`,
  `slangc → dxil → shader_cache/nri/dx12`; runtime picks by active renderer.
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
- **Container-rule violations** (task Z7) — inventory re-audited
  2026-07-25 (batch 3): `kotek::vector_t` in `zircon_game_manager.h`/
  `zircon_session_editor.h` fixed in batches 1+2; `zircon_renderer.h:98`
  fixed in batch 3 (`kotek::vector_t<T,N>` — the N is a silently-ignored
  `NotInUseArgument`, it is a DYNAMIC std::vector — now
  `kotek::static_vector_t<T,N>`); raw `std::thread/std::lock_guard` are
  GONE from `src/` (batches 1+2); the two `std::string_view` remnants are
  kotek-INTERFACE-forced, not fixable zircon-side
  (`console_user_callback_enum_translation_t` =
  `std::function<int(std::string_view)>` in kotek.core.api — the console
  lambda site is annotated exempt; `zircon_component_geometry.cpp` no
  longer spells `std::` at all); the `entt::` "leftovers" resolved on
  audit: every ACTIVE use is either inside `#ifdef
  KOTEK_USE_ECS_BACKEND_ENTT` (legit backend code) or inside the
  commented-out `todo: re-write` console-command block in
  `zircon_game_manager.cpp` (~:2101, kept for Z8), and the migration
  target `kotek::entity_t` already exists in kotek.core.ecs (ui_state
  uses it); stale duplicate `src/editor/ui/zircon_sdk_ui.{h,cpp}`
  DELETED (dead, not in CMake, redefined the live class's methods);
  `Kotek::` vs `kotek::` case = Z16. REMAINING: the two
  `kotek::vector_t<entt::id_type>` inside the INACTIVE ENTT backend
  guard in `zircon_factory.h:874/879` (migrate with the ENTT backend
  task, which also owes "delete outdated methods signatures" at
  `zircon_factory.h:102`), raw `char[]` in some commands (audit).
- Render (P1 of Z3 done 2026-08-01): the dead-weight purge landed — GL-era orphan
  passes (debug/editor_debug/editor_grid/editor_camera/terrain + the never-registered
  game imgui pass) and `zircon_render_resource_manager.*` (`todo: delete`) removed;
  `Add_PassesEditor/Game` commented bodies + dead `create_render_graph` overload
  excised; `Get_Name` returns the real bgfx name (new kotek constant
  `kRenderer_BGFX_Name`); `zircon.render` split into executor +
  `zircon.render.passes.bgfx` (static by default); the pass factory generator had
  THREE latent bugs (abstract/base classes leaking in via includes, nonexistent
  `kun_bgfx` namespace macro, unqualified `enum class` case labels) — fixed in the
  generator (`generate_zircon.cpp`), never edit its output by hand. Pass sets are
  config-driven (`render_passes_editor`/`render_passes_game` in game_config.json).
  REMAINING render debt: game `model_static` is an empty-namespace placeholder
  (P2 fills it); the shared-linkage pass-lib duality is P3.
- **User-settings data layout (owner directive 2026-08-01)**: every editor/game
  user-settings file lives under `data_user` (`game_config.json`, the Render
  Passes window's flags, future prefs); imgui's UI state (`imgui.ini`) is
  redirected to `data_user/sdk/settings/` — the editor imgui pass resolves it
  through `eFolderIndex::kFolderIndex_DataUser_SDK_Settings` (the enum existed
  unused until then) and sets `io.IniFilename` at context creation. Settings
  must never land at the repo root (imgui's default ini is cwd-relative — that
  is exactly the trap).
- **Configuration documentation (owner directive 2026-08-01)**:
  `doc/git/en/configuration.md` carries the tables of CMake options (via
  kotek), runtime arguments, and `game_config.json` keys, referenced from the
  root README — any change that adds or alters an option, an argument, or a
  config key updates this file in the same commit (same drift rule as kotek's
  options registry).
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
  **Hard-crash dialogs (WER) suppressed machine-user-wide (2026-08-01)**:
  `HKCU\Software\Microsoft\Windows\Windows Error Reporting\DontShowUI=1`
  (owner asked agents to handle lingering modals; crashes still log, no
  desktop-camping dialog — revert with the same value set to 0). If a
  modal is suspected, verify before killing: enumerate visible
  `#32770` dialog windows via user32 EnumWindows (powershell Add-Type)
  and check `tasklist //FI "IMAGENAME eq WerFault.exe"`.
  **Exe-run modal discipline (mandatory, 2026-08-01)**: before running ANY
  built .exe (boot, test runner, tool, from ANY build tree incl. agent
  scratch trees) verify the tree's `CMakeCache.txt` has
  `KOTEK_ASSERT_STDERR_ROUTING=ON`; keep captures bounded (`timeout` +
  size cap); after the run sweep for stuck modals (EnumWindows `#32770` +
  WerFault, clear via `taskkill //F //IM <exe>`). A modal an agent cannot
  click blocks all automation — the WER switch above is the last line of
  defense, not a substitute for the routing flag.
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
- **Journal corruption robustness (fixed 2026-07-31)**: a corrupt/truncated
  `history.zjrnl` turned into an unbounded error storm (tens of millions of
  identical line pairs, multi-GB logs) through two compounding defects:
  (1) `std::fstream` keeps a sticky failbit — one failed block read made
  EVERY later read fail at any offset, because no path called `clear()`;
  all four journal failure paths (scan, block write, block header read,
  block payload read) now reset the stream state before returning;
  (2) `zircon_editor_command_history::Undo()` returned early on an
  unobtainable command WITHOUT moving the cursor, so undo-to-origin
  drivers (`while (cursor != root) Undo();`) spun on the same node
  forever — `Undo()` now skips the unreadable node
  (`m_cursor_node_id = node.m_parent_node_id`), i.e. corruption degrades
  to skipped nodes + a loud state-mismatch test failure instead of a
  hang. Process rule that came out of the same incident: **never run
  smoke boots with unbounded output redirects** — cap every capture
  (`timeout` + `head -c`); two multi-GB capture files were written before
  this was enforced.
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
| Z3 | Render restructure: split `render/bgfx` into TWO projects — (a) passes (dynamic/hot-swappable), (b) render graph + resource manager executor; validate hot-reload of pass library (see §7 verdict) | in-progress (2026-08-01 P1+P2a-c done) | vk/gles3 deletion DONE (Z10). P1 DONE (2026-08-01): dead-code purge + `zircon.render` executor / `zircon.render.passes.bgfx` target split + generator-repaired pass factory + config-driven pass sets (`render_passes_editor`/`render_passes_game`); verified 0-error build, both boot variants exit 0, 227+14 suite, game_config.json roundtrip byte-identical. P2a DONE (2026-08-01): **Render Passes window** (`zircon_ui_render_passes`, left-docked, auto-opens until "don't show again" persists `show_pass_manager_on_start=false`) — per-session sections fed by codegen'd registry tables, instant enable/disable via executor skip flags (`draw()` iterates with identical previous-pass/id semantics), add/remove/reorder via frame-boundary rebuild (callback-injected factory keeps the executor pass-type-free, P3 swaps the callback for the DLL export), dirty marker + Save → game_config.json; proven live (imgui.ini gained `[Window][Render Passes]`, config roundtrip intact). P2b DONE (2026-08-01): **game `model_static` pass resurrected** (geometry+transform iteration, composed model matrices, camera-aware view/proj, empty-world no-op) + **Slang-only shader pipeline** (owner directive: user shaders are Slang across bgfx AND NRI; `zircon_shaderpack` writes bgfx's v11 container byte-exact — verified against a shaderc reference; routes: spirv→pack→bgfx/vulkan, hlsl→fxc→pack→bgfx/dx11, dxil→nri/dx12; pinned slang+dxc cmake fetch with SHA256; per-stage incremental rebuilds; shaderc dialect rejected on evidence). P2c DONE (2026-08-01): **own glTF-lite loader** (`zircon_gltf_loader`, zero new deps, backend-portable ktkJson DOM subset, 64-bit bounds-checked accessors, 10-status error enum, static-mesh scope per plan R6) + geometry component `m_mesh_name` (journal-serialization roundtrip proven) + pass mesh cache (8 slots, 16-bit indices, sticky-fail); suite 24+227 (8 new loader/integration tests), KOTEK_OWN compile-checked. P2d DONE (2026-08-01): **editor grid pass** (analytic infinite XZ grid — fullscreen triangle via `bgfx::setVertexCount(3)`+SV_VertexID, 1m/10m lines, axis coloring, distance fade, fwidth AA; Slang shaders through the pipeline, bgfx-predefined `u_invViewProj`; default editor set now present→grid→imgui, proven by boot-log pass order; a latent cmake empty-list packer-arg bug fixed; suite 25+227). P2e DONE (2026-08-01): **own gizmo pass** (`editor_gizmo_own`) — 14-entry static handle table (draw/intersect/apply per handle — new 3D-output functions register without touching the shell), analytic picking only (ray-vs-arrow/quad/ring/sphere, center>plane>axis priority), constant screen size, hover/active highlighting, W/E/R modes + snap toggle (imgui-IO gated), drag with live overlay readout (new click-through `zircon_ui_gizmo_overlay` window fed via ui_state POD — pass-emitted imgui is impossible: the imgui frame opens+closes inside the imgui pass's OnUpdate), drag-end commits ONE journaled `zircon_command_edit_component_state` (undo/redo clean), click-select feeds the EXISTING `zircon_editor_ui_state` selection (no new selection state); suite 32+227 (7 new: pick/drag-translate/rotate/scale math, click-select, drag-end command through the real history). P2f DONE (2026-08-01): **ImGuizmo variant** (vendored v1.92.5 MIT byte-pristine into the passes project) — inert registered pass + `Manipulate` hosted inside the imgui frame via a transparent ui window (frame-ownership rule from P2e); both gizmos share one journaled commit static in zircon.editor.commands; bx and ImGuizmo matrix conventions verified identical (column-major, translation [12..14]) — no transpose bridge; mutual exclusion wired in the Render Passes window (one gizmo at most, none legal); integration lessons: vendored include dir must be ABSOLUTE (KOTEK_ENGINE_ROOT_FOLDER_NAME is a name) and `IMGUI_DEFINE_MATH_OPERATORS` must be target-wide because the PCH transitively includes imgui.h (per-TU defines lose to the force-include); suite 35+227. P2g DONE (2026-08-01): **forward Phong** for model_static — Lambert diffuse + Phong specular + ambient floor, vertex-color modulated, one directional light from named defines (light components later); FS `LightParams` cbuffer with the vk::binding/register discipline, packer-dump-verified uniform table (u_lightDir/u_lightColor/u_ambient/u_cameraPos, cb 64); VS emits world pos + normal (no-inverse-transpose approximation documented); the lighting math is a pure static mirrored 1:1 with the shader and headless-tested (facing/backfacing/perpendicular/specular-peak/clamp cases); suite 36+227. Incidental: the CUSTOM logger's file sink is now bounded (64 MB single-generation rotation, kotek 5a6e273) after all.log hit 1.4 GB. P2h DONE (2026-08-01): **level pass sets** — a scene is the Z6 journal+snapshot PAIR (no scene json existed), so level metadata lives in a new sibling `data_user/sdk/scenes/<name>/scene.json` (`{"render_passes":...}`); resolution chain at game-graph creation: scene file → config → built-in (`zircon_render_pass_set_resolver.h`, per-leg log evidence; unknown names error-log + fall through — user data is not a programmer error, no abort); the active set writes back at module shutdown save (the upstream SaveScene console command is dead Z8 debt — noted); the window's dirty check compares against the RESOLVED baseline; pass sets are NOT journaled (test proves not one byte lands in the journal); suite 39+227. **THE P2 LADDER IS COMPLETE.** P3a DONE (2026-08-01): **the passes-DLL foundation** — kotek `Detach_Passes` (additive, kotek 641d24b); `ZIRCON_GRAPHICS_DEVELOPMENT=ON` builds the passes project as `passes/zircon.render.passes.bgfx.dll` (game.ktk re-exports bgfx via whole-archive + export-all so the DLL shares the ONE bgfx copy — no second global state); C-ABI export surface `zircon_passlib_get_count/get_name/create/destroy` (both session registries); the executor's pass-library manager loads the DLL at init in dev mode and ALL pass create/destroy flow through it (DLL-side lifetime: cross-CRT/unload rules honored); default config byte-identical (main tree: 0-error build, boot exit 0, 39+227). DEV-CONFIG CAVEATS (known, non-blocking): the passes DLL pulls kotek test registrations along its link edges so the suite double-runs (266×2) in the gfxdev boot, and the dev boot aborts at CRT teardown (exit 3) — both are the documented cross-CRT residual class amplified by the third CRT in the process; the real cure is K9. Also fixed en route: KotekPluginOverride test isolation (pid+invocation-unique temp dirs). P3b DONE (2026-08-02): **live hot-reload** — shadow-copy loading (the engine maps `passes/.shadow_<pid>_<n>.dll`, so the real DLL is never write-locked and a bad rebuild can't take the running editor down); polling watcher (500 ms, 2 stable reads) sets only an atomic flag; the swap runs at the top of `draw()`: destroy passes via the OLD library → swap → recreate from the slots' stored name sets → re-init → re-seat; console override `reload_render_passes()`; the Render Passes window refreshes via a registry generation counter (new pass appears without editor restart). PROVEN LIVE: mid-run rebuild links while the engine runs, contract log sequence fires in ~0.5 s, 102 s of frames post-swap, natural exit 0, 12/12 create/destroy balanced, shadows cleaned. The no-static rule (§2.1a) was applied retroactively to P3a: seam is owner-carrying fnptr+void*, all watcher/registry state are members. Two en-route fixes: the imgui pass's destroy-before-create crash (latent, exposed by frame-1 console reload), and KotekPluginOverride test isolation. P4 DONE (2026-08-02, kotek 6ac69d5): **NRI passes exist and drive frames** — the frame-pass surface (additive virtuals, see Z5) + `zircon.render.passes.nri` project + clear pass through the pass path; `--render_nri_dx12` boots with `[nri]: frame passes driven: 1` evidence, 39+227 green. NEXT: P5 docs/CI (incl. the graphics-dev CI leg) → NRI phase 3 (hot-reload split mirror, real geometry/RT passes). APPROVED PLANS (session plans dir): split+reload mechanics (auto watcher + manual console override, C-ABI create/destroy export surface, kotek `Detach_Passes` additive API, game.ktk re-exports bgfx in the dev config only). Passes lib reload-safety rules: destroy passes BEFORE unload (graph Shutdown deletes!), recreate after; passes hold POD/handles only (etl-terminator + cross-CRT rules) |
| Z4 | Document codebase style (preprocessors, memory allocation patterns) | done (2026-07-21) | §2/§3; keep in sync with reality |
| Z10 | gles3/vk backend removal (owner directive) | done (2026-07-22) | `src/render/{gles3,vk}` deleted; `src/render/CMakeLists.txt` rewritten bgfx-only; `zircon_renderer_bgfx` is the only renderer (union member `p_gles3` + all vk/gles3 branches excised from `zircon_game_manager`); `validate_extensions` (dead GL-era) removed; os console pass retargeted to the bgfx pass base (OnUpdate/OnRender signature updated with `my_id_in_queue`) |
| Z11 | Runtime boot chain (kotek.exe runs from repo root) | done (2026-07-23) | FIXED earlier: STD-mode `ktkJson::Get` stub (kotek bug, config never parsed in STD); `dll::shared_library` move semantics; `program_location`; stray `KOTEK_ASSERT(false)` in `Initialize_ResourceManager`; window-console stub assert → warning; world init with factory; `--kotek_frames=N`; splash busy-wait bounded. FIXED 2026-07-23 evening: console `Register_Command` segfault = **etl intrusive-list terminator is a module-local static** — a container built by one module (exe) terminates buckets with an address the other module (game.ktk) never matches, any cross-module insert walks off the end (null+8). Fix: game manager constructs its OWN console and restores the engine's at shutdown (`m_p_console_exe_owned`), ownership stays in the constructing module — THE rule for every object whose inline/template methods touch etl containers. Render shutdown dispatch: stray `if (is_gl)` swallowed the whole else-if chain (bgfx shutdown never ran → `status=false` assert). `zircon_config`: serialize wrote 1024B fixed buffer (garbage tail → json "extra data"), now writes `text_real_length`; deserialize parsed `sizeof(text)` instead of the Read_File-returned size. `matrix2x2_f::e/c`: 6 inverted range asserts (fired on VALID indices). VERIFIED: boot → 14/14 + 163/163 tests → 30 frames → full shutdown → **exit 0**. OPEN: splash thread can hang main-window init (create windows on main thread only); 6 residual CRT-startup heap asserts (pre-flag blocks, tolerated until K9) |
| Z5 | Passes for editor AND game, for bgfx AND NRI; NRI gets own folder, same two-project split (passes + executor) | in-progress (2026-08-02 P4 done) | kotek K11 phase 1 landed: `src/render/nri/zircon_renderer_nri.{h,cpp}` — minimal `ktkIRenderer` over the kotek.render.nri swapchain (clear-color present; **no NRI types in zircon**), DirectX-slot cases in every `zircon_game_manager` renderer switch, sessions skip render-graph init (invalid id), select with `--render_nri_dx12`. Verified: boot exit 0, 30 frames presented, clean shutdown. P4 DONE (2026-08-02, kotek 6ac69d5): **the NRI frame-pass surface** — additive pure-C++ virtuals (`ktkIRenderFramePass` Record(context)+Get_Name; `ktkIRenderFramePassContext` the only mid-frame object, ClearColor today; `Present_With_Passes` default-bodied → Present, bgfx untouched) + swapchain Present split into Begin_Frame/End_Frame with acquire/begin failures skipping instead of assert-into-UB; zircon: `zircon.render.passes.nri` STATIC project + hand-written `zircon_nri_passlib` (C-ABI-shaped for the future DLL split), clear pass `zircon_render_graph_pass_present_nri` reproducing the phase-1 output through the pass path (`[nri]: frame passes driven: 1` evidence), game-session install via the shared resolver (`zircon_resolve_game_render_pass_set_with_default`); two latent NRI+editor holes fixed (editor switch missing kDirectX_Latest case; unguarded bgfx-union read in the scene writer). `.gitignore` corrected: `passes/` → `/passes/` (the unanchored pattern silently hid every SOURCE dir named passes). Verified: build 0 errors both configs, boots default+NRI exit 0, 39+227. REMAINING (phase 3): the two-project hot-reload split mirrored for NRI (bgfx P3b is the template), real NRI passes (geometry, then RT), editor-on-NRI pass set |
| Z6 | Undo/redo: store ALL history (no cutting), reliable restore; assess design & shrinkability | done (2026-07-23) | journal+registry+history rewrite (previous agent) + four fixes landed: (1) **replay crash** `ecs_is_entity_ready` (pico_ecs.h:1102): delete-undo/create-redo reincarnate an entity under a fresh id and the flat recorded→live map went stale for OTHER aliases of the same logical entity (repro chain 12→18→47) — fixed with a reincarnation chain (`m_entity_reincarnation`: incarnation id → next id, values strictly increase so walks always terminate) + history-side before/after `GetEntityID` observation in `Undo()`/`execute_node()` (commands no longer call `update_dependent_commands` themselves — journal-reconstructed ones only know recorded ids and wrote chain-skipping links) + `apply_world_state` rebinds whole chains on snapshot restore. (2) **redo-to-final diverged**: the test mutated components directly (unjournaled edits are unreplayable by design) — added journaled `zircon_command_edit_component_state` (registry type `zircon_DEF_COMMAND_TYPE_EDIT_COMPONENT_STATE`, delta = before+after json) and the stress mix drives edits through it. (3) test scan watermark must resync from `get_entity_watermark()` (pico never recycles, redo mints fresh ids past the model's range → empty serialize). (4) `ResourceManagerLoadTextResourceNoCache` abort (`zircon_resource_manager.cpp:163`): sync `load` never allocated desc/view ids (async path did) and `allocate_desc/view` returned indices into a reserved-but-never-grown vector (OOB on use) — both fixed. VERIFIED full suite green: `CommandHistory_Stress_100k_Commands` (~63k executed ops over 100k iterations: 100% recorded, full undo-to-origin byte-identical, full redo byte-identical, journal ≥2× compression + ≤64 MB disk, journal reopen retains every command) + `CommandHistory_Restore_Node_From_Snapshots` + 12 Zircon_Game + 163 kotek tests, all PASSED, exit 0 |
| Z7 | Enforce static-container/`ktk`-alias rule consistently across `src/` | in-progress (2026-07-25 batch 5) | batches 1-5 done — full re-audit in §5 "Container-rule violations": active `src/` is clean of dynamic `vector_t`, raw `std::thread/lock_guard`, dead duplicates, raw `char[]` (batch 4), and the `zircon_config` dynamic storage (batch 5: `m_features_data_game` deleted — never written; `m_features_data_sdk` unordered_map → `static_vector_t<pair<enum,variant>>` lookup table, capacity `ZIRCON_DEF_CONFIG_MAX_FEATURE_DATA_COUNT`=8; variant's dynamic `cstring` alternative → `static_cstring_t<32>`; `translate_zircon_game_features` (dead stub with `assert(false)`, zero callers) now returns `const char*` literals like the sdk one — resolves the header's `TODO: replace to static_cstring_t` the other way, literals need no container; `get_feature` gained a `holds_alternative` guard so a wrong-Type read returns `Type{}` instead of throwing `bad_variant_access`; game_config.json schema unchanged, roundtrip verified). Interface-forced exemptions (annotated): console `std::string_view` lambda, `zircon_config` serialize raw array (kotek template signature). REMAINING: two `kotek::vector_t<entt::id_type>` inside the INACTIVE ENTT backend guard in `zircon_factory.h` (migrate with the ENTT backend task, which also owes "delete outdated methods signatures" at `zircon_factory.h:102`) |
| Z8 | Track codebase TODOs | open | ~130 hits in `src/`; major clusters: command history, game manager, factory `#error`s, render passes |
| Z9 | Make zircon layer SHARED-capable (break editor cycles) | open | found 2026-07-22 during K18 validation: `zircon.editor.session` ↔ `zircon.editor.commands` ↔ `zircon.editor.ui` are cyclic at symbol level (session constructs command_history + ui_state; commands/ui call session getters). Static linking hides it; DLLs forbid it. Fixes (choose): (a) merge the 3 editor targets into one DLL; (b) extract interfaces (`zircon_interface_command_history`, `zircon_interface_editor_ui_state`) into `zircon.core` + register via locator, i.e. apply kotek's own ktkI* discipline to zircon — preferred, matches engine philosophy. Other zircon modules (core/ecs/game/game.session/world) already link fine as DLLs |
| Z12 | Replace every raw container in `src/` with switchable kotek aliases + hard-require kotek's embedded config | guard done (228cb77), container sweep = Z7 backlog | owner directive 2026-07-23. GUARD HALF **DONE** (landed 228cb77, pre-dating its registry update): `src/core/include/zircon_config_guard.h` is force-included (`/FI`) into every `zircon.*` target from the root CMake and hard-`#error`s unless `KOTEK_USE_LIBRARY_TYPE_EMB` AND `KOTEK_USE_STD_LIBRARY_STATIC_CONTAINERS` are active. KOTEK SIDE VERIFIED (2026-08-01): **EMB is the DEFAULT `KOTEK_LIBRARY_TYPE`** — the daily build tree and all CI legs already compile the full stack (kotek + zircon + game.ktk) under EMB, so the alias coverage is proven in practice; HYB builds correctly REFUSE zircon targets via this guard (independent verification during the HYB container task — the guard fires exactly as designed, kotek modules unaffected). Strict shipping variant documented in the header (`-DKOTEK_STD_LIBRARY_DYNAMIC_CONTAINERS=OFF -DKOTEK_STD_LIBRARY_HYBRID_CONTAINERS=OFF`). REMAINING: only the raw-container sweep itself (Z7 list — the two ENTT-guard `vector_t` sites + `zircon_factory.h:102` outdated signatures) |
| Z13 | CI/CD green on GitHub + three linkage configurations in CI | done (2026-07-31) | **GREEN END-TO-END**: canary run on 3326832 — build (Debug×{default, static}) + tests (engine boot on the runner) both SUCCESS; the `badges` branch is live with all three badges passing (`build Debug-default`, `build Debug-static`, `tests`) and updates outcome-driven on every run. What it took: public flip (owner 2026-07-31 — private-repo Actions was quota/billing-blocked; the account-side payment flag killed every leg in 2s), slim-leg redesign (push = Debug × {default, static}; full Debug/Release × {default, static, dynamic} matrix nightly+dispatch), restore-keys cache fallback, configure retry ×3 with tee'd logs, `contents: write` + clone-based `ci-logs` publishing on failure, outcome-driven shields badges (`merge-multiple` + loud no-json guard), timeouts 180. tests.yml runs the real engine boot (`--no_splash --kotek_frames=30`, `KOTEK_ASSERT_STDERR_ROUTING=ON`) | kotek side fully green (K5: build/tests/modules on 78cbd58); zircon workflows aligned and healthy (no failure publishes on the `ci-logs` branch). build.yml compiles the THREE configurations the owner requires (default / full static / full dynamic as continue-on-error per K18) × Debug/Release; tests.yml runs the real engine boot (`--no_splash --kotek_frames=30`, `KOTEK_ASSERT_STDERR_ROUTING=ON`). Hardening (mirrors kotek K5): `restore-keys` vcpkg cache fallback, configure retry ×3 with tee'd `configure-attempt<N>.log` + `build-output.log`, `contents: write` + clone-based `ci-logs` publishing on failure in both workflows (the public read path — job logs/artifacts are admin-only), timeouts 180. RELEASE COMPILATION FIXED (2026-08-01): the nightly full matrix exposed that zircon never compiled in Release — 8 ECS component headers guarded their serialization scratch buffer behind `#ifdef KOTEK_DEBUG` with a release-branch `KOTEK_ASSERT(false)` and `zircon_command_definitions.h:82` carried an `#error` demanding the binary variant; all guards removed (same buffer + attribute names in every config; the binary-variant optimization stays a tracked TODO in the file — it was the guard's intent, not a compile dependency); verified locally: Release build 0 errors + Release boot exit 0. Dynamic legs stay configure-red by K18 design (cyclic module graph, `continue-on-error`) |
| Z14 | Unit tests for every class and public function (both repos, kotek K22) | open | owner directive 2026-07-23: functional proofs, not per-method formalities — behavior + edge cases + stress where the contract promises it (the 100k-command history stress is the reference). Rule 8 (tests are living code) governs maintenance |
| Z15 | Plugin override system exists at kotek level (kotek K21) — informational | done at kotek level (2026-07-23) | `KOTEK_INVOKE_MODULE` is override-first in EVERY linkage mode: drop `plugins/<module-folder-name>.dll` (or map it in `plugins/plugins.json`, json wins) next to the data dirs and the user's dll replaces that kotek module's `InitializeModule_*/ShutdownModule_*/Serialize/Deserialize` entries; `--kotek_plugins_template` / `--kotek_plugins_modules` codegen the file skeletons. Zircon's own module entries (`InitializeModule_Render` etc. invoked from `src/engine/main_game_dll.cpp`) go through the same macro, so kotek-module overrides apply to zircon's call sites with zero zircon changes; zircon's OWN modules join the override registry once zircon's cmake re-runs `kotek_generate_plugin_manifest` for its targets (not done — zircon modules are the game layer, overriding them is out of K21's scope). See kotek/AGENTS.md §5a "Plugin overrides" |
| Z16 | Namespace case sweep: `Kotek::` → lowercase aliases (`kotek::`) everywhere in zircon | done (2026-07-25) | rule in §2.6: lowercase aliases are canonical (they preserve the cmake namespace-rename feature). All 258 sites across 58 files swept (literal token replace — audited first: zero compound identifiers, zero string literals, zero rule-doc comments containing the token); `src/` now has 0 direct `Kotek::` sites (kotek itself: 43, handled by its K-side `KUN_*`/`kun_*` macro discipline). VERIFIED: full Debug build green on the first pass (the alias header reaches every TU via the force-included kotek PCH), boot `--no_splash --kotek_frames=30` exit 0, 14/14 + 189/189 tests |
| Z17 | UI-press test harness for the imgui editor | design done (2026-07-31), slot pending owner | DESIGN (owner-reviewed): inject events at the `ImGuiIO` seam (`io.AddMousePosEvent/AddMouseButtonEvent/AddKeyEvent`) into the REAL context via the K13/K17 wrapper — no Win32 synthesis, backend-agnostic; assert on ENGINE CONTRACTS (click "Delete Entity" → world count −1, journal +1 node, cursor moved), not pixels; tests are static step tables in `static_vector_t` (capacities as `zircon_DEF_UI_TEST_*`), registry = lookup-table-on-static_vector, zero runtime allocation, Debug/dev-only; deterministic frame stepping via `--kotek_frames=N` + a new `--ui_test=<name|all>` CLI; window layouts pinned per test; two-backend shape: own harness = no-dep floor, `imgui_test_engine` optional later for screenshot/fuzz. Slotting options delivered to owner: after Z12 (done) or parallel with the render split — awaiting his call |
| Z18 | Purge static storage duration from zircon (+ kotek K24) | open | owner directive 2026-08-02, rule §2.1a: NO file-scope globals, function-local `static`, or static class members (etl `static_*` TYPES unaffected — they name capacity). Replacement patterns in preference order: owning-class members → manager/context-owned instances → registered services behind ktkI* interfaces. OWNER DECISIONS (2026-08-02): vendored third-party inside kotek/src (imgui/pico/vma/stb/ImGuizmo, ~1,400 statics) is EXEMPT (byte-pristine, upstream-sync discipline); namespace-scope `constexpr`/`inline constexpr` POD is EXEMPT (no linker singleton, no init hazard — outside the rule's rationale). INVENTORY DONE (2026-08-02, agent-35 audit): zircon is nearly clean (G1: `g_main_manager` + `s_fontRangeMerge`; S1: the `zircon_command_registry` Meyers singleton; zero S2-mutable/S3) — kotek carries the surface (G1 12 mutable/9 sites: the `_pLoggerMain` pair (both backends), dead `isUserCallbackUpdateFunctionContainsLoop`, plugin override+invoke registries (~132 KB never-unload), win32 window-class state, the `kotek_std_memory.cpp` tracker cluster, test globals; S1 12/8 sites incl. `kotek_own_json.h` Meyers set (default_resource documented intentional per-module) and the `saved_placement` latent multi-window bug; S3 3 (imgui TLS pair + `s_inNew`)). PURGE PLAN by hazard class: batch 1 mechanical DONE (2026-08-02, kotek ae5b140 + zircon 2cc1651). BATCH 2 forks DECIDED by owner (2026-08-02): **logger globals → macro-context refactor** (the macros resolve the logger through an explicit caller-passed context instead of the `_pLoggerMain` globals — big per-module sweep, its own task); **own-json → default_resource EXEMPT (documented per-TU by design, not the defect class) + the mutable `fallback_*` scratch converted**; **memory tracker cluster → DEFERRED to K9** (it is the allocator rework's subject matter — no double design). Batch 2a (feasible sites): plugin override+invoke registries (~132 KB) → state struct owned by ktkMainManager (the macros already carry the manager into every call); `zircon_command_registry` Meyers → member of `zircon_editor_command_history`; imgui TLS pair → context-manager member map (thread-id keyed); own-json mutables. Batch 2b: the logger macro-context refactor (separate design+implementation task). First compliance edit DONE (2026-08-02): `s_invocation` static in kotek's plugin-override test replaced with pid+timestamp path uniqueness |

## 7. Design verdicts (2026-07-21 analysis — basis for Z3/Z6)

**Hot-reloadable pass library (Z3, 'graphics development' flag):** ~~feasible~~
**IMPLEMENTED (P3a+P3b, 2026-08-02)** with two refinements over the original
verdict. The protocol holds: passes cannot "continue working" across reload —
they are destroyed (by the library that created them, before that library
unloads) and recreated from the slots' stored name sets. Refinement 1
(**shadow-copy loading**): the engine never maps the real DLL — it loads
`passes/.shadow_<pid>_<n>.dll`, so (a) the real DLL is never write-locked and
mid-run rebuilds always link, (b) a corrupt/mid-write candidate is rejected
before the working library is touched (never pass-less). Refinement 2
(**no-locks threading**): the polling watcher (500 ms, 2 stable reads) only
sets an atomic flag; the swap runs at the top of `draw()` (GPU idle point).
Manual override: `reload_render_passes()` console command. Persistent state in
the pass DLL is forbidden (owner-carrying fnptr+void* seams, §2.1a); GPU
resources stay executor-side, passes reference by handle. The reload-unsafe
originals (create in one module, `delete` in another — the graph's
`Shutdown()`) are gone: creation/destruction both run inside the library
(extern "C" `zircon_passlib_create/destroy`). NRI mirrors the split in
phase 3 (its passlib is already C-ABI-shaped).

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
