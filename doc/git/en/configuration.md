# zircon — configuration reference

How to configure the engine: at build time through kotek's CMake options, at
runtime through `kotek.exe` arguments and the user configuration files. This
file is maintained alongside the code — a change that adds or alters an option,
an argument, or a config key updates this file in the same commit.

## CMake

zircon deliberately adds **no CMake options of its own**. The engine is
configured entirely through kotek's options — see
[kotek's configuration reference](https://github.com/wh1t3lord/kotek/blob/main/doc/git/en/configuration.md)
(`cmake -DKOTEK_HELP=ON` prints the same registry from the build).

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
| `render_passes_editor` | string (comma list) | `present,grid,imgui` | the editor session's render pass set, in execution order; names come from the pass factory registry; imgui must stay last (it draws the UI over everything) |
| `render_passes_game` | string (comma list) | `present,model_static` | the game session's render pass set, in execution order |
| `add_required_components_automatically` | bool | `true` | entity creation auto-attaches the required components |
| `sphere_bounding_box_quality` | int | `8` | tessellation quality of generated bounding spheres |
