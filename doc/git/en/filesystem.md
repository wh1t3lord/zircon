# kotek filesystem — usage guide

How to read, write, stream, pack, and default-proof your game's data through
kotek's filesystem (`ktkIFileSystem`). Audience: engine users writing game or
tool code. Everything here goes through the interface — backends (native,
.kpack archives), the override chain, and the bounds are the engine's, not
yours. Missing or malformed user data is never an assert: the contracts return
`false` with one warning.

The one rule behind every contract below: **user data is not a programmer
error.** A missing/unreadable file is `false` + at most one warning. Caller
errors (null filesystem, null buffer) keep their `KOTEK_ASSERT`.

## 1. The five-minute version

The helpers live in
`kotek/src/kotek.core.filesystem/include/kotek_filesystem_helpers.h` (reachable
through the module umbrella `kotek_core_filesystem.h`). You need a filesystem
pointer — in engine code it comes from the main manager:

```cpp
kotek::core::ktkIFileSystem* p_fs = p_main_manager->GetFileSystem();
```

Read a json file (the probe, the read, and the parse in one call):

```cpp
kotek::core::ktk_filesystem_path path;
kotek::core::path_for(
	p_fs, kotek::core::eFolderIndex::kFolderIndex_DataGame_Configs,
	"my_config.json", path);

kotek::core::ktkResourceText<4096, 4096, false> config;
if (kotek::core::read_json(p_fs, path, config))
{
	int quality = config.Get<int>("quality");
}
```

Edit it and write it back:

```cpp
config.Write("quality", 8);
kotek::core::write_json(p_fs, path, config);
```

Read a plain file into your own buffer:

```cpp
kotek::uint8_t buffer[8192];
kotek::size_t real_size = 0;

if (kotek::core::read_file(p_fs, path, buffer, sizeof(buffer), real_size))
{
	// buffer holds real_size bytes ('\0' terminated when room remains)
}
else if (real_size != 0)
{
	// false with a non-zero size = your buffer was too small; real_size is
	// the REQUIRED size — grow the buffer and retry, or switch to streaming
}
```

Get a path for any well-known folder (`path_for` = `Make_Path` + append in one
call; the folder indices cover `data_game` subtrees, `data_user`, shader
cache, SDK scenes/settings, ...):

```cpp
kotek::core::path_for(
	p_fs, kotek::core::eFolderIndex::kFolderIndex_DataUser,
	"my_save.dat", path);   // -> <root>/data_user/my_save.dat
```

Probe a size without reading:

```cpp
kotek::size_t size = 0;
bool exists = kotek::core::file_size(p_fs, path, size);
```

The helpers' bounds (named defines, never magic numbers): json texts up to
`KOTEK_DEF_FILESYSTEM_HELPERS_JSON_SCRATCH_SIZE` (64 KB) — bigger files are
`false` + one warning and belong on the streaming API or a dedicated loader.
When absence is the *normal* case (a first-boot config, an optional scene
file), keep an `Is_Exists` pre-check so even the single warning line is
suppressed — that is the documented idiom, not a workaround.

## 2. The mental model in one diagram

A read resolves a **root-relative path** (e.g. `data_game/textures/wood.ktx`)
through a chain of backends, in priority order:

```
read("data_game/textures/wood.ktx")
  |
  v
+-------------------------------------------------------------+
| 1. mounted packs (kPack)  -- boot default, packs FIRST      |
|    data_game/packs/*.kpack mounted newest-first at init;    |
|    entry = hash of the root-relative path; a newer pack     |
|    shadows an older one; a miss falls through SILENTLY      |
+-------------------------------------------------------------+
  |  miss
  v
+-------------------------------------------------------------+
| 2. native dirs (kNative)  -- <root>/<path as given>         |
|    data_user/  = user + editor overrides (saves, settings)  |
|    data_game/  = shipped content                            |
|    both are the same native backend: you address the root   |
|    you mean through the path (eFolderIndex); editor saves   |
|    land in data_user so user content overrides shipped      |
|    content by convention; a miss warns ONCE, returns false  |
+-------------------------------------------------------------+
  |  miss
  v
+-------------------------------------------------------------+
| 3. embedded defaults (zircon layer, opt-in per call site)   |
|    resolve_or_default answers the type's compiled-in blob   |
|    (magenta checker / shader token / silence / empty json)  |
|    with is_default=true + ONE loud log per (type, path)     |
+-------------------------------------------------------------+
```

**Pack-first vs native-first.** With packs present and the shipped
`sys_info.json` (`FS_PriorityList: ["Native"]`), the filesystem *prepends*
`kPack` at initialize — "always native-last" — so packed content serves
without per-read miss noise, and a pack miss is free (silent fallthrough).
Two ways to flip it:

- configure the order explicitly in `sys_info.json`:
  `"FS_PriorityList": ["Native", "Pack"]` — a same-named loose file now
  shadows the packed one (the engine honors your order verbatim once `kPack`
  appears in it);
- pass a per-call `priority` argument (`eFileSystemPriorityType::kNative` /
  `kPack`) to `Read_File` / `Begin_Stream` / `Get_FileSize` to pin one call
  to one backend.

Among packs the mount order is the override order: `data_game/packs/` mounts
**newest-first**, so a newer pack wins name collisions — the patch/DLC story.

## 3. Streaming (the streaming API)

One-shot reads materialize the whole file in your buffer. Anything big or
unbounded goes through the streaming API: a **forward-only cursor** with one
outstanding sequential step, bounded scratch, no seeks.

Minimal forward-only read over a native file (the documented loop shape):

```cpp
kotek::core::ktkFileHandleType stream = p_fs->Begin_Stream(path);

if (stream != kotek::core::kInvalidFileHandleType)
{
	// the real step: KOTEK_DEF_FILESYSTEM_STREAM_STEP_SIZE (4 KB) on
	// native, or your Begin_Stream override
	kotek::uint8_t chunk[4096];

	while (p_fs->Get_RemainingStreamsCount(stream) > 0)
	{
		kotek::size_t got = sizeof(chunk);

		if (p_fs->Read_Stream(stream, chunk, got) == false)
			break;   // a data error poisons the stream loudly ONCE

		consume(chunk, got);   // your per-step consumer
	}

	p_fs->End_Stream(stream);   // ALWAYS safe (idempotent)
}
```

The same code, untouched, streams a **pack entry**: the dispatcher resolves
the path to the pack backend and the step becomes the entry's 64 KB
compression block (`KOTEK_DEF_FILESYSTEM_PACK_BLOCK_SIZE`) — the block is
atomic, a `Begin_Stream` step override is ignored there, and
`Get_StreamingBufferLength(stream)` tells you the truth. Decompression is
per-block: a 4 MB packed file is drained in exactly 64 block reads of one
64 KB block each, never a whole-file inflation, and the bytes are
**byte-identical** to the same content on native disk (a hard guarantee,
test-pinned).

When to stream vs one-shot — the bounded-file rule:

| shape | answer |
|---|---|
| bounded, provably small (configs, scene metadata, json <= 64 KB) | one-shot (`read_file`/`read_json`) — one call, no loop |
| big, or sized by user data you don't control | stream — the consumer holds ONE step, never the file |

The HDD discipline (what makes the stream fast on spinning disk): one forward
cursor, one sequential read in flight, steps land on disk in order (a pack
stream's blocks are contiguous inside the entry). Never seek a stream (it is
an error); never interleave two streams over one file region; if you need
random access, that is a different access pattern — say so in the design, do
not emulate it through streams. Sync-sequential is the current floor; the
read-ahead double-buffer is the documented up-scale, deliberately not built
yet.

Streams come from a bounded pool (`KOTEK_DEF_FILESYSTEM_FSTREAM_POOL_SIZE` =
8 concurrent streams); `End_Stream` returns the slot, and `Shutdown` sweeps
leaked streams loudly. The filesystem is single-threaded this phase — keep
all calls for a stream on one thread.

## 4. Packed data end-to-end

`.kpack` v1 in one paragraph: entries keyed by a 64-bit fnv1a **name hash**
of the root-relative path (names are not stored; `'\' -> '/'`, case-folded);
each entry is split into independent 64 KB compression blocks
(`stored`/`zstd`/`zlib`); data spans are 4 KB-aligned and laid out in load
order. The reader mounts the tables once and decompresses **only the blocks a
request spans**. Spec: kotek's `kotek_kpack_format.h`.

Pack a folder (the tool builds with the engine under
`build/zircon_tools_kpacker/Debug/`):

```
zircon_kpacker pack --root data_game/levels --out data_game/packs/levels.kpack
    --compression zstd
```

- `--compression stored|zstd|zlib` (default `zstd`): `stored` for
  already-compressed payloads (images, audio), `zstd` the default middle,
  `zlib` the compat option.
- `--order load-order.json`: a json **array** of root-relative paths
  (`["configs/sys_info.json", "textures/a.png"]`) — listed files first in
  manifest order (they sit at the front of the pack = loaded first), the rest
  appended lexicographically. Without it, plain lexicographic.
- `--extensions .json,.txt`: keep only the listed suffixes.
- `--no-names-manifest`: skip the embedded `_kpack_names.json` sidecar —
  without it the pack is list/verify-only (add/remove need names; a hash is
  not invertible).

Inspect and gate:

```
zircon_kpacker list --pack data_game/packs/levels.kpack
zircon_kpacker verify --pack data_game/packs/levels.kpack   # the CI gate
```

Offline mutation (wholesale rewrite through the shared encoder; the runtime
never mutates a shipped pack — editor saves land in `data_user/`):

```
zircon_kpacker add --pack levels.kpack --file new_boss.json --name data_game/levels/new_boss.json --compression zstd
zircon_kpacker remove --pack levels.kpack --name data_game/levels/old_boss.json
```

Exit codes: `0` ok, `1` operational failure (io/corrupt/conflict), `2` usage.

**Mounting is automatic**: drop the file under `data_game/packs/` and the
filesystem mounts every `*.kpack` there at initialize, newest-first, and
prepends the pack backend to the priority list. Packed and loose content
coexist: a pack hit serves from the archive, a pack miss falls through to the
native dirs silently. Your code never changes — the same `read_file` /
`Begin_Stream` calls resolve either way.

## 5. Fault tolerance (embedded defaults)

When a resource is missing or unreadable **everywhere** on the chain — no
`data_user` override, not in `data_game`, not in any mounted pack — the
zircon layer answers with an **embedded default** instead of a failure
(`src/core/zircon_embedded_defaults.h`):

| type | blob |
|---|---|
| `kTexture_Checker_Magenta` | 2x2 magenta/black checker, RGBA8 (the Source-style "missing" signal) |
| `kShader_Unlit_Magenta` | the semantic token the render side maps to a built-in unlit-magenta material |
| `kAudio_Silence` | a tiny canonical PCM WAV of zeroed samples |
| `kJson_Empty` | the two bytes `{}` |

```cpp
zircon_embedded_defaults defaults;   // one per resolving owner (member)

kotek::uint8_t buffer[4096];
kotek::size_t buffer_size = sizeof(buffer);
zircon_resolved_or_default_t result;

if (zircon_resolve_texture_or_default(
		p_fs, defaults, path, buffer, buffer_size, result))
{
	if (result.m_is_default)
	{
		// the embedded blob answered — result.m_p_bytes points at the
		// compiled-in default (NOT your buffer); the loud-once log fired
		// the first time for this (type, path)
	}
	else
	{
		// real content from the chain — result.m_p_bytes == buffer
	}
}
else
{
	// caller error only: null args, or a REAL file bigger than your
	// buffer (buffer_size now holds the REQUIRED size — retry bigger)
}
```

The contract:

- **Missing/unreadable is never a fault.** The default resolves with
  `is_default=true`; the engine keeps running and the missing content is
  *visible* (magenta), not silent.
- **Loud-once.** One warning per `(type, path)` per owner, deduped in a
  bounded set (16); on overflow one suppression notice fires and defaults
  keep resolving silently.
- **Check `is_default`** to badge/mark substituted content (the resource
  manager's text branch sets `is_default` on its descriptors for exactly
  this).
- A **real file that doesn't fit your buffer is not a default case** — you
  get `false` + the required size; grow the buffer or stream.

To keep default-critical content from ever hitting the chain: ship it. A
resource present in `data_game/` (loose) or in a mounted pack resolves on the
chain and the default never fires — so pack the assets your game cannot
degrade gracefully (boot shaders, core textures) into `data_game/packs/`
with the rest. The defaults are the floor for content that is *allowed* to be
missing, not a substitute for shipping.

Note who deliberately does NOT ride the json default: the localization
manager (a missing language keeps the previous table + key echo — strictly
better than an empty locale) and `zircon_config` (a missing config already
installs the hardcoded defaults — the designed floor).

## 6. Complex scenarios

### 6a. Texture streaming — a big texture, block-by-block, bounded memory

A 256 MB packed texture never enters RAM whole. The consumer holds exactly
one 64 KB block:

```cpp
void stream_texture_from_pack(kotek::core::ktkIFileSystem* p_fs,
	const kotek::core::ktk_filesystem_path& path, TextureUploader& uploader)
{
	// resolves to the pack backend when 'path' is pack-hosted (pack-first
	// default); the step is the entry's 64 KB compression block
	kotek::core::ktkFileHandleType stream = p_fs->Begin_Stream(path);

	if (stream == kotek::core::kInvalidFileHandleType)
		return;   // missing: one warning already logged

	// ONE scratch, sized by the stream's real step (64 KB for packs)
	const kotek::uint32_t step = p_fs->Get_StreamingBufferLength(stream);
	kotek::uint8_t block[KOTEK_DEF_FILESYSTEM_PACK_BLOCK_SIZE];
	KOTEK_ASSERT(step <= sizeof(block), "unexpected step");

	uploader.begin(p_fs, path);   // your staging setup

	while (p_fs->Get_RemainingStreamsCount(stream) > 0)
	{
		kotek::size_t got = sizeof(block);

		if (p_fs->Read_Stream(stream, block, got) == false)
			break;   // corrupt block: loud once, stream poisoned

		uploader.append(block, got);   // stage/upload this slice
	}

	uploader.end();
	p_fs->End_Stream(stream);
}
```

Peak extra memory: `step` bytes, independent of the texture's size. The
blocks arrive contiguous (the format lays an entry's blocks in order), so the
disk sees one sequential read.

### 6b. Geometry/mesh streaming — chunked read feeding a mesh builder

Same shape over a native level file (or a packed one — the loop does not
change): a fixed scratch, forward-only, the builder consumes chunks as they
arrive.

```cpp
bool stream_mesh(kotek::core::ktkIFileSystem* p_fs,
	const kotek::core::ktk_filesystem_path& path, MeshBuilder& builder)
{
	kotek::core::ktkFileHandleType stream =
		p_fs->Begin_Stream(path, 64 * 1024);   // 64 KB steps on native

	if (stream == kotek::core::kInvalidFileHandleType)
		return false;

	kotek::uint8_t scratch[64 * 1024];
	bool ok = true;

	while (p_fs->Get_RemainingStreamsCount(stream) > 0)
	{
		kotek::size_t got = sizeof(scratch);

		if (p_fs->Read_Stream(stream, scratch, got) == false)
		{
			ok = false;
			break;
		}

		// the builder keeps only its OWN bounded parse state across calls
		// (a partial-record carry), never the stream bytes
		builder.feed(scratch, got);
	}

	p_fs->End_Stream(stream);
	return ok && builder.finish();
}
```

The parser is a state machine with a small carry buffer — that is the whole
trick: stream bytes are consumed and forgotten, only *meaning* persists.

### 6c. CACHED scenario — a config/locale file read repeatedly

A 2 KB config read 200 times costs the first disk touch and 199 page-cache
hits. This is the case where keeping bytes in RAM is right:

- the file is **tiny and hot** — a bounded, named capacity covers it
  (`ktkResourceText<4096, 4096, false>`; the locale manager's 192 KB bound);
- the **OS page cache** already is the file-content cache — an engine-owned
  cache of file bytes would duplicate it with worse bookkeeping (the house
  streaming-first doctrine: no user-space caches without a written
  justification of what the OS doesn't do better);
- so: parse once into a resident structure (the DOM, the locale table), or
  re-read freely — both are cheap. `read_json` re-reads through the override
  chain each time, so a `data_user` override or a newer pack is honored on
  the next read for free.

### 6d. NOT-cached scenario — a 2 GB world region

The opposite end: a 2 GB region, cold, touched once per load.

- **Never materialize it.** A one-shot `Read_File` would demand a 2 GB
  caller buffer — an immediate memory-rules violation (allocations are rare
  and bounded; capacity is a named constant sized from measurement). A 2 GB
  file in a pack also streams: 32,768 independent 64 KB blocks, one at a
  time.
- **Stream it with a flow-through shape**: one bounded scratch (the step),
  the region consumed chunk by chunk into the structures that actually
  persist (tiles, octree nodes, GPU buffers), each chunk forgotten as the
  next arrives. Double-buffer (read N+1 while consuming N) is the documented
  up-scale when the sync floor stops being enough — the interface already
  reports the counters (`Get_RemainingStreamsCount`) such a scheduler needs.
- **Cold + big = streaming is strictly better than caching**: caching 2 GB
  in RAM to serve a one-time pass buys nothing over the OS's own streaming
  read-ahead and costs the entire budget. The benchmark suite
  (`test_hdd_cached_vs_streamed_shape`) pins exactly this in code: the
  streamed path's peak extra memory is the step size regardless of the file
  size, while a small hot file is re-read freely.

## Where the evidence lives

Every claim above is pinned by a test. The HDD benchmark suites
(`kotek/src/kotek.core/tests/kotek_core_test_filesystem.cpp`):
`test_hdd_stream_sequential_vs_scattered` (one-shot vs 4 KB vs 64 KB vs
scattered reads of a 64 MB file — measured MB/s logged, byte-equality and the
strictly-sequential invariant asserted), `test_hdd_pack_stream_block_discipline`
(pack-zstd streaming at exactly one 64 KB block per step, byte-identical to
native), `test_hdd_cached_vs_streamed_shape` (the memory shapes of 6c/6d).
