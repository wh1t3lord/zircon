# zircon_bcn_encoder — the BCn texture bake host tool + the vendored encoders (task Z24 B4)

This directory is the TOOL-side home of the texture pipeline's encode
leg (the owner's rule: the vendored encoder is host-tool-side only,
NEVER engine/runtime code — the runtime uploads the baked blocks as-is
and the hardware decodes in the sampler).

## Vendored files (BYTE-PRISTINE — never edit; upstream sync replaces the file + this note)

| Files | Upstream | Version pinned | License |
|---|---|---|---|
| `rgbcx.h`, `rgbcx.cpp`, `rgbcx_table4.h`, `rgbcx_table4_small.h` | github.com/richgel999/bc7enc_rdo | commit `b9438627eef73a1157e84201b6fa6eb2ffd6d9f0` | dual MIT / public domain (`LICENSE.bc7enc_rdo`) |
| `bc7enc.h`, `bc7enc.cpp` | github.com/richgel999/bc7enc_rdo | same commit | dual MIT / public domain (`LICENSE.bc7enc_rdo`) |
| `bc7decomp.h`, `bc7decomp.cpp` | github.com/richgel999/bc7enc_rdo | same commit | dual MIT / public domain (`LICENSE.bc7enc_rdo`) |
| `bcdec.h` | github.com/iOrange/bcdec | commit `80859ed3b7afb1c527a2a99d70c61457bea72d0c` | dual MIT / public domain (`LICENSE.bcdec`) |

Why this pick (the task asked for the smallest license-clean single-file
set covering the format enum with a decoder):

- **bc7enc** is the plan's named reference ("bc7enc-style single-file
  C++") — a one-TU BC7 encoder with a quality knob (uber level +
  partitions + perceptual weights), deterministic single-threaded.
- **rgbcx** is the same author/repo and the same license — the smallest
  serious BC1/BC3/BC4/BC5 encoder (levels 0-18; the `cBC1Ideal` endpoint
  mode keeps BC1 output cross-vendor).
- **bc7decomp** ships in the SAME repo — the BC7 decoder the tests'
  PSNR pins use ("the lib carries one").
- **bcdec.h** is a single self-contained header decoding BC1/BC3/BC5
  (and BC7/BC6H for later) — the decode seam for the BC1/BC3/BC5 quality
  pins. Decoding is a TEST-ONLY proof; no runtime code path decodes
  (the plan's hard rule).
- No single-file open encoder covers BC6H (ispc-texcomp needs the ISPC
  toolchain, DirectXTex is library-scale): **BC6H encode is DEFERRED**
  (the plan's HDR use is skies — a later intake task). The .bcn format
  + the bgfx upload mapping already cover it.

## House-side files

- `zircon_bcn_encoder_adapter.h` — the adapter implementing zircon.core's
  injected encode seam (`zircon_texture_bcn_encode_level_fn`) over the
  vendored libs, the quality tiers (fast/default/high — fixed parameter
  sets, the determinism contract) and the test-only decode seam.
- `zircon_bcn_bake.cpp` — the CLI (`--help`). Reads `.zraw` (the house
  intake container), writes `<root>/textures/<scene>/<name>.bcn`;
  `--synthesize_boot_checker` regenerates the shipped boot probe
  `textures/boot/boot_checker.bcn` at the engine root (the meshlets/boot
  precedent — the pack entry namespace is root-relative). Packing into a
  `.kpack` is zircon_kpacker's job — the tools compose.
- `CMakeLists.txt` — the standalone host project (the zircon_kpacker
  structural pattern): compiles the vendored sources + the engine's
  format/bake sources (`../../core/zircon_texture_bcn.cpp`) from source,
  configured by the engine build through `cmake/zircon_bcn_bake.cmake`.
