#pragma once

#include "../../core/zircon_config.h"

/// @brief \~english file name of the level-metadata sibling inside a
/// scene folder (data_user/sdk/scenes/<name>/scene.json)
constexpr const char* kZirconSceneMetadata_FileName = "scene.json";

/// @brief \~english json key of the scene's game render pass set (a
/// comma-separated pass class-name list, the same spelling as the
/// config keys render_passes_editor/render_passes_game)
constexpr const char* kZirconSceneMetadata_KeyRenderPasses =
	"render_passes";

/// @brief \~english scene.json — the level-metadata sibling of the Z6
/// journal pair (task Z3 P2h). A scene folder under
/// data_user/sdk/scenes/<name>/ holds history.zjrnl (append-only
/// zstd-compressed command deltas) and history.zsnap (binary world
/// snapshots); neither can carry level metadata — the journal is
/// world-state deltas only (pass sets are config/editor state, NOT
/// world state, and are deliberately excluded from the undo/redo
/// journal), and the snapshot format is a binary entity dump. So the
/// ACTIVE game render pass set of the session the scene was saved
/// with lives in this small json sibling, read back at game boot with
/// the resolution order: scene's render_passes -> config
/// render_passes_game -> built-in default
/// (kZirconConfig_DefaultRenderPassesGame). Future level metadata
/// (scene name, environment, ...) joins this file instead of
/// inventing new siblings.
///
/// The class is pure file IO: it knows nothing about render passes,
/// renderers or the pass registry — validation and the resolution
/// chain live in the engine layer
/// (src/engine/zircon_render_pass_set_resolver.h), the call sites are
/// composed by zircon_game_manager.
class zircon_scene_metadata
{
public:
	/// @brief \~english writes {"render_passes": "<list>"} into
	/// <p_scene_folder_path>/scene.json (the folder must exist — the
	/// command history creates it at scene open); an empty list
	/// asserts and writes nothing (an empty key would be
	/// indistinguishable from a corrupt file on load)
	static bool save_render_passes(
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_scene_folder_path,
		const char* p_comma_separated_names) noexcept;

	/// @brief \~english reads the render_passes key into out_list;
	/// returns false (out_list untouched) when the file or the key is
	/// absent, the value is not a string or is empty — the caller
	/// then falls back down the resolution chain. An overlong value
	/// is a corrupt file: loud error + false
	static bool load_render_passes(
		kotek::core::ktkIFileSystem* p_filesystem,
		const char* p_scene_folder_path,
		kotek::static_cstring_t<
			ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>&
			out_list) noexcept;
};
