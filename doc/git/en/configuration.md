# zircon — configuration reference

How to configure the engine: at build time through kotek's CMake options, at
runtime through `kotek.exe` arguments and the user configuration files. This
file is maintained alongside the code — a change that adds or alters an option,
an argument, or a config key updates this file in the same commit.

## CMake

zircon adds one option of its own; everything else is configured through
kotek's options — see
[kotek's configuration reference](https://github.com/wh1t3lord/kotek/blob/main/doc/git/en/configuration.md)
(`cmake -DKOTEK_HELP=ON` prints the same registry from the build).

| Option | Default | Meaning |
|---|---|---|
| `ZIRCON_GRAPHICS_DEVELOPMENT` | `OFF` | graphics-development mode (task Z3 P3a): the bgfx render passes build as the hot-swappable `passes/zircon.render.passes.bgfx.dll` (next to `data_game`/`data_user`) with a static fallback twin linked into `game.ktk`; `game.ktk` re-exports its whole static closure (`/WHOLEARCHIVE` + a PRE_LINK-generated `.def`) so the DLL resolves every engine/bgfx/imgui symbol from the single loaded copy. `OFF` keeps the passes statically linked — zero behavior change. Whether passes actually come from the DLL at runtime is the `graphics_development` config key / `--graphics_development` CLI override (default ON in these builds) |

Two hard requirements are enforced by zircon itself
(`src/core/include/zircon_config_guard.h`, force-included into every zircon
target):

| Requirement | Why |
|---|---|
| `KOTEK_LIBRARY_TYPE=EMB` (the default) | the `ktk_*` aliases must resolve to static (etl-based) containers — zircon never reallocates behind its back |
| `KOTEK_STD_LIBRARY_STATIC_CONTAINERS=ON` (the default) | static containers must exist to resolve to |

For a strict shipping configuration where dynamic and hybrid containers do not
exist at all, additionally pass
`-DKOTEK_STD_LIBRARY_DYNAMIC_CONTAINERS=OFF -DKOTEK_STD_LIBRARY_HYBRID_CONTAINERS=OFF`.

When `KOTEK_DEVELOPMENT_TYPE=STATIC`, the layer root provides the three
`KOTEK_GAME_MODULE_*` / `KOTEK_USER_GAME_*` variables (see kotek's table).

## Runtime arguments

zircon is the game module of `kotek.exe`, so every argument in
[kotek's runtime table](https://github.com/wh1t3lord/kotek/blob/main/doc/git/en/configuration.md#runtime-arguments-kotekexe)
applies. The arguments zircon itself consumes:

| Argument | Meaning |
|---|---|
| `--editor_imgui` | run the ImGui editor session (editor + game render graphs, tool windows, command history) |
| `--editor` | run the wxWidgets SDK path (only in `KOTEK_USE_SDK` builds) |
| `--graphics_development` | force the `graphics_development` feature on for this run (session-scoped): render passes are created through `passes/zircon.render.passes.bgfx.dll` instead of the statically-linked passlib — only in `ZIRCON_GRAPHICS_DEVELOPMENT=ON` builds; other builds warn and keep the static passes |

Useful combinations:

```
kotek.exe --no_splash --kotek_frames=30    # boot, run 30 frames, exit — the smoke test
kotek.exe --editor_imgui                    # the editor
kotek.exe --render_nri_dx12                 # boot on the NRI (DirectX 12) renderer
```

## Configuration files (all under `data_user/` — never the repo root)

| File | Contents |
|---|---|
| `data_user/game_config.json` | engine configuration (serialized by `zircon_config`) — keys below |
| `data_user/sdk/settings/imgui.ini` | editor UI layout (window positions, docking) |
| `data_user/sdk/scenes/` | saved scenes/levels, incl. their command-history journals |
| `data_user/shader_cache/` | compiled shader binaries |

`game_config.json` keys:

| Key | Type | Default | Meaning |
|---|---|---|---|
| `render_passes_editor` | string (comma list) | `present,grid,gizmo_own,imgui` | the editor session's render pass set, in execution order; names come from the pass factory registry; imgui must stay last (it draws the UI over everything) |
| `render_passes_game` | string (comma list) | `present,model_static` | the game session's render pass set, in execution order. The GPU-driven A/B (task Z24 B1): swap `model_static` for `model_static_gpu_driven` (the chunked GPU-driven baseline — compute frustum cull + one indirect draw) by editing this key, by a scene's `scene.json` `render_passes`, or live at the console with `render_passes_game_toggle_ab()` |
| `add_required_components_automatically` | bool | `true` | entity creation auto-attaches the required components |
| `sphere_bounding_box_quality` | int | `8` | tessellation quality of generated bounding spheres |
| `graphics_development` | bool | `true` in `ZIRCON_GRAPHICS_DEVELOPMENT=ON` builds, `false` otherwise | render passes come from the hot-swappable `passes/zircon.render.passes.bgfx.dll` instead of the statically-linked passlib (task Z3 P3a); inert in builds without the option (a warning and the static passes) |
| `sdk_camera_rotation_quaternion` | bool | `false` | editor camera rotation representation (task Z20): `true` accumulates mouse deltas into the camera component's quaternion (yaw about world Y, pitch about the local right axis, same ±89° window), `false` keeps the euler yaw/pitch driver; toggleable live in the Settings window |
| `sdk_camera_input_bootstrap` | bool | `true` | editor camera bootstrap entity (task Z20, opt-out per owner): when an editor session's world starts without a scene load, the engine auto-creates the one sdk_camera+sdk_input+transform entity the camera driver needs; `false` leaves the world untouched (for scenes that always bring their own camera); toggleable in the Settings window, applies to the next session start |
| `localization_editor_language` | string (language tag) | `en` | active language of the EDITOR localization instance (task Z22): names the `data_game/configs/locale/editor/<tag>.json` table; SINGLE-LANGUAGE residency (one table per instance — a missing key echoes the key itself with one deduped warning); runtime-switchable through `zircon_localization_manager::set_language` (persisted at the next save; a rejected switch keeps the previous working table and tag); tags are 1–16 chars of `[a-zA-Z0-9_-]` (the tag becomes a file name) |
| `localization_game_language` | string (language tag) | `en` | active language of the GAME localization instance (`data_game/configs/locale/game/<tag>.json`) — layer-3 in-game text, same single-residency contract; switches at load |

## Tools

Host tools build at engine BUILD time through nested standalone configures
(the `zircon_shaderpack` pattern) — a plain `cmake --build` produces them
under `<build>/zircon_tools_<name>/<config>/`.

### zircon_kpacker (the .kpack packer, filesystem plan B2b)

Packs a folder tree into a `.kpack` archive and maintains existing archives
offline. The format spec (v1) is kotek's `kotek_kpack_format.h`: entries
keyed by a 64-bit fnv1a **name hash** (names are not stored), 64 KB
independent compression blocks (`stored`/`zstd`/`zlib`), 4 KB-aligned data
spans in load order. The tool never reimplements the codec: it links
kotek's `kotek.core.filesystem.pack` module sources and drives the shared
`kpack_write_file` encoder; `verify` mounts the archive through the real
runtime reader (`ktkFileSystem_Pack`) — the engine's own code path.

```
zircon_kpacker pack --root <folder> --out <file.kpack>
    [--compression stored|zstd|zlib]  (default: zstd)
    [--order <load-order.json>] [--extensions .json,.txt,...]
    [--no-names-manifest]
zircon_kpacker add --pack <file.kpack> --file <disk path>
    --name <entry name> [--compression stored|zstd|zlib]
zircon_kpacker remove --pack <file.kpack> --name <entry name>
zircon_kpacker list --pack <file.kpack>
zircon_kpacker verify --pack <file.kpack>
```

- **pack** walks `<folder>` recursively (bounded by the reader's 4096-entry
  cap) and writes one `.kpack`. Entry order = physical load order on disk:
  with `--order` (a json **array** of root-relative paths, forward slashes —
  `["configs/sys_info.json", "textures/a.png"]`) the listed files come first
  in manifest order and unlisted files follow lexicographically (a warning
  names the count); without it, plain lexicographic after the fold rule
  (`\`→`/`, case-folded). `--extensions` keeps only the listed suffixes
  (case-insensitive). Source files are memory-mapped read-only — the tool
  never copies a whole file into its own buffers; the encoder consumes one
  64 KB block at a time.
- **names sidecar**: since the format stores only hashes, the tool embeds a
  reserved last entry `_kpack_names.json` (zstd) holding `{"names":[...]}`
  in entry order. `add`/`remove` REQUIRE it (a rewrite re-emits the kept
  entries by name through the shared encoder — a hash is not invertible);
  `list`/`verify` use it when present. Packs packed with
  `--no-names-manifest` are list/verify-only (list then shows
  hash+offset+sizes+codec per entry).
- **mutation model (offline)**: `add`/`remove` read the whole entry set and
  REWRITE the pack wholesale through the shared encoder (temp sibling +
  replace; no in-place surgery). Intended for small packs, tool-side; the
  runtime never mutates a shipped pack — editor saves land in the
  `data_user/` override dirs instead.
- **verify** (the CI integrity gate): full structural validation
  (magic/flags/tables/offsets/alignment/duplicate hashes), every block
  decompressed cleanly (v1 stores no checksums — clean decompression + size
  consistency is the gate; a `stored`-block byte flip is undetectable by
  design), the runtime reader mounts the pack, and every named entry is
  re-read through it byte-for-byte.
- Exit codes: `0` success, `1` operational failure (io/corrupt/conflict),
  `2` usage error.

Typical flow for shipping packed content: pack `data_game/` subtrees to
`data_game/packs/*.kpack` — at boot the filesystem mounts that folder
newest-first and prepends the pack backend to the priority list (packs
override native dirs; a miss falls through to native silently), so packed
and unpacked content coexist and `data_user/` overrides still win when the
user orders them first.

The consumer-side guide to the whole filesystem (helpers, streaming, the
override chain, embedded defaults) is
[filesystem.md](filesystem.md).
