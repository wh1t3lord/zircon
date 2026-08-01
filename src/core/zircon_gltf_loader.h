#pragma once

#include <kotek.core.containers.string/include/kotek_core_containers_string.h>
#include <kotek.core.containers.vector/include/kotek_core_containers_vector.h>
#include <kotek.core.containers.filesystem.path/include/kotek_core_containers_filesystem_path.h>

KOTEK_BEGIN_NAMESPACE_KOTEK
KOTEK_BEGIN_NAMESPACE_CORE
class ktkIFileSystem;
KOTEK_END_NAMESPACE_CORE
KOTEK_END_NAMESPACE_KOTEK

// glTF 2.0 lite loader (task Z3 P2c, owner-decided zero-dependency model
// format — the plan's "glTF-lite loader" section): static meshes only.
// .glb (JSON + BIN chunks) and .gltf with external buffer files are
// supported; base64 data-URIs, skins, animations, cameras, sparse
// accessors, quantized (non-float32) vertex attributes, non-triangle
// primitive modes and every extension are OUT of scope — their presence is
// logged once per file ("unsupported: <feature>") and loading continues
// with the base geometry, or fails gracefully when the feature is
// required to make sense of the payload. Every accessor/bufferView range
// is bounds-checked against the real buffer sizes: malformed or truncated
// input yields an error status, never a crash.
//
// The JSON chunk is parsed through the backend-agnostic kotek::json DOM
// API (compiles against BOTH the boost backend and KOTEK_JSON_LIBRARY=
// KOTEK_OWN); the binary payload is decoded directly. A streaming-JSON
// upgrade is a documented later optimization — glTF JSON chunks are
// small, the bulk lives in the BIN chunk.

// capacities — sized for cooked editor/game-bootstrap models; anything
// bigger is a streaming problem and out of the lite loader's scope
// (kError_CapacityExceeded is returned, never an overflow)
#define zircon_DEF_GLTF_MAX_VERTEX_COUNT 16384
#define zircon_DEF_GLTF_MAX_INDEX_COUNT 49152
#define zircon_DEF_GLTF_MAX_SUBMESH_COUNT 16
#define zircon_DEF_GLTF_MAX_NODE_COUNT 64
#define zircon_DEF_GLTF_MAX_BUFFER_COUNT 4
// the JSON chunk of a .glb / the whole .gltf text must fit this (the
// binary bulk is not json); 64 KB of gltf json describes far more
// primitives than the vertex/index caps above admit
#define zircon_DEF_GLTF_JSON_CHUNK_MAX_SIZE 65536
// inline scratch of the monotonic DOM resource (grows by bounded
// allocation for json texts that outgrow it; small models never
// allocate)
#define zircon_DEF_GLTF_JSON_DOM_SCRATCH_SIZE 32768
#define zircon_DEF_GLTF_ERROR_MESSAGE_MAX_LENGTH 128

enum class eZirconGltfLoadStatus : kotek::uint8_t
{
	kSuccess = 0,
	// null pointers / zero sizes passed by the caller
	kError_InvalidArguments,
	// file missing, unreadable, or larger than the caller's file buffer
	kError_FileRead,
	// glb magic / chunk layout violated or the container is truncated
	kError_BadContainer,
	// glb container version != 2 or asset "2.x" requirement violated
	kError_UnsupportedVersion,
	// json parse error or a wrong value kind where the schema is fixed
	kError_JsonMalformed,
	// buffer without data: no BIN chunk, unreadable external buffer or
	// a data-URI (base64 is out of the lite scope)
	kError_BufferMissing,
	// a feature outside the lite scope is required to decode the
	// payload (e.g. an extension in extensionsRequired)
	kError_Unsupported,
	// an accessor/bufferView range check against the real buffer sizes
	// failed (corrupt or hostile input)
	kError_AccessorOutOfRange,
	// the model exceeds one of the zircon_DEF_GLTF_* caps
	kError_CapacityExceeded
};

// one interleaved vertex of the decoded mesh; missing NORMAL /
// TEXCOORD_0 attributes are zero-filled and reported through the mesh's
// flags
struct zircon_gltf_vertex_t
{
	float m_position[3];
	float m_normal[3];
	float m_texcoord[2];
};

// one drawable range of the mesh: a glTF primitive of a node, with the
// node's hierarchy transform already flattened into a world matrix. The
// matrix follows the engine's model-matrix convention (the same layout
// bx::mtxFromQuaternion produces: translation at [12..14], rotation as
// bx stores it) so passes compose it with an entity model matrix
// directly.
struct zircon_gltf_submesh_t
{
	float m_world_matrix[16];
	kotek::uint32_t m_index_offset;
	kotek::uint32_t m_index_count;
	float m_base_color_factor[4];
	bool m_has_base_color_texture;
};

struct zircon_gltf_mesh_t
{
	kotek::static_vector_t<zircon_gltf_vertex_t,
		zircon_DEF_GLTF_MAX_VERTEX_COUNT>
		m_vertices;
	// canonical 32-bit indices regardless of the source encoding
	// (uint16/uint32 accessors both decode into this)
	kotek::static_vector_t<kotek::uint32_t,
		zircon_DEF_GLTF_MAX_INDEX_COUNT>
		m_indices;
	kotek::static_vector_t<zircon_gltf_submesh_t,
		zircon_DEF_GLTF_MAX_SUBMESH_COUNT>
		m_submeshes;
	// over world-transformed positions of every decoded primitive
	float m_aabb_min[3];
	float m_aabb_max[3];
	// false when at least one decoded primitive lacked the attribute
	// (its vertices are zero-filled there)
	bool m_has_normals;
	bool m_has_texcoords;
};

using zircon_gltf_error_t =
	kotek::static_cstring_t<zircon_DEF_GLTF_ERROR_MESSAGE_MAX_LENGTH>;

// decodes a .glb container held in memory (JSON + BIN chunks; external
// buffers cannot resolve without a file context and fail with
// kError_Unsupported / kError_BufferMissing)
eZirconGltfLoadStatus zircon_gltf_load_from_memory(
	const void* p_glb_data, kotek::size_t data_size,
	zircon_gltf_mesh_t& out_mesh,
	zircon_gltf_error_t& out_error) noexcept;

// loads a .glb or a .gltf (+ external buffer files resolved relative to
// the .gltf's directory) through the kotek filesystem. The caller owns
// the file scratch: the model file AND every external buffer must fit
// p_file_buffer together (they are packed sequentially, the parsed json
// text is not kept once the DOM exists). Existence and size are checked
// before every read — the native read path asserts on missing files and
// on caller-buffer overflows, so this function never forwards such
// inputs to it.
eZirconGltfLoadStatus zircon_gltf_load_from_file(
	kotek::core::ktkIFileSystem* p_filesystem,
	const kotek::static_path_t& path_to_file,
	kotek::uint8_t* p_file_buffer, kotek::size_t file_buffer_capacity,
	zircon_gltf_mesh_t& out_mesh,
	zircon_gltf_error_t& out_error) noexcept;
