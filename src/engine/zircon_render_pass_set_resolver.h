#pragma once

#include <cstring>

#include "../core/zircon_config.h"
#include "../editor/session/zircon_scene_metadata.h"

/// @brief \~english task Z3 P2h — the game render pass set resolution
/// chain. When a scene is loaded (and at game-session creation when no
/// scene is loaded) the game pass set resolves as:
///   1. the scene file's render_passes key
///      (data_user/sdk/scenes/<name>/scene.json — present AND every
///      name registered in the generated pass factory's game registry)
///   2. the config's render_passes_game (non-empty AND valid)
///   3. the built-in kZirconConfig_DefaultRenderPassesGame
/// A source with an unknown/unregistered name is dropped LOUDLY
/// (KOTEK_MESSAGE_ERROR, no assert: a stale or hand-edited user file
/// is runtime data, not a programmer error — the boot must survive it
/// and the test suite exercises these paths) and the chain falls
/// through to the next source. Pass-set changes stay config/editor
/// state — they never enter the undo/redo journal (scene.json is a
/// sibling of the journal, not a journal entry).
///
/// Header-only on purpose: the game manager (boot) and the unit tests
/// share exactly this instance, and the file stays free of concrete
/// pass types — the registry arrives as a plain name table from the
/// caller.

/// @brief \~english which leg of the resolution chain produced the set
/// (logs, tests)
enum class eZirconRenderPassSetSource : kotek::uint8_t
{
	kScene,
	kConfig,
	kBuiltin
};

/// @brief \~english validates a comma-separated pass-name list against
/// the pass library's registry table: whitespace-trimmed tokens, empty
/// tokens skipped, at least one real token required, every token must
/// match a registered name exactly. Unknown names are reported one by
/// one (loud drop). A nullptr registry (count 0) disables validation —
/// the factory's own creation path then stays the last line of defense
inline bool zircon_validate_render_pass_list(
	const char* p_comma_separated_names,
	const char* const* p_registry_pass_names,
	kotek::uint8_t registry_pass_count,
	const char* p_list_origin_for_logs) noexcept
{
	if (!p_comma_separated_names || (*p_comma_separated_names == '\0'))
	{
		return false;
	}

	const char* p_cursor = p_comma_separated_names;

	kotek::uint8_t token_count = 0;
	bool is_valid = true;

	while (*p_cursor != '\0')
	{
		while ((*p_cursor == ' ') || (*p_cursor == '\t'))
		{
			++p_cursor;
		}

		const char* p_token_begin = p_cursor;

		while ((*p_cursor != '\0') && (*p_cursor != ','))
		{
			++p_cursor;
		}

		const char* p_token_end = p_cursor;

		while ((p_token_end > p_token_begin) &&
			((p_token_end[-1] == ' ') || (p_token_end[-1] == '\t')))
		{
			--p_token_end;
		}

		if (p_token_end > p_token_begin)
		{
			++token_count;

			if (p_registry_pass_names && registry_pass_count > 0)
			{
				bool is_registered = false;

				// lookup-table-on-vector (rule 2): a handful of names,
				// linear scan
				for (kotek::uint8_t registry_index = 0;
					 registry_index < registry_pass_count; ++registry_index)
				{
					const char* p_registered_name =
						p_registry_pass_names[registry_index];

					const kotek::ktk::size_t registered_length =
						std::strlen(p_registered_name);

					if ((registered_length ==
							static_cast<kotek::ktk::size_t>(
								p_token_end - p_token_begin)) &&
						(std::strncmp(p_token_begin, p_registered_name,
							 registered_length) == 0))
					{
						is_registered = true;
						break;
					}
				}

				if (!is_registered)
				{
					// a single name is strictly shorter than the whole
					// list, so the list capacity can never truncate a
					// valid token (same argument as
					// zircon_create_render_passes_from_config)
					kotek::static_cstring_t<
						ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
						name;
					name.assign(p_token_begin,
						static_cast<kotek::ktk::size_t>(
							p_token_end - p_token_begin));

					KOTEK_MESSAGE_ERROR(
						"unknown render pass '{}' in {} — check the "
						"name against the generated "
						"zircon_render_pass_factory registry",
						name.c_str(), p_list_origin_for_logs);

					is_valid = false;
				}
			}
		}

		if (*p_cursor == ',')
		{
			++p_cursor;
		}
	}

	return is_valid && (token_count > 0);
}

/// @brief \~english resolves the game render pass set for a scene
/// load / game-session creation: scene.json's render_passes (present
/// + valid) -> config render_passes_game (non-empty + valid) ->
/// the given built-in default. out_resolved_list always receives the
/// winning list (never empty); the return value says which leg won.
/// This is the backend-neutral form (task Z5 phase 2 / P4): the bgfx
/// game set resolves through zircon_resolve_game_render_pass_set below
/// with kZirconConfig_DefaultRenderPassesGame; the NRI frame passes
/// resolve through this one with the NRI registry + the NRI built-in
/// default (the config/scene keys are shared — a name the backend's
/// registry does not know is dropped loudly and the chain falls
/// through, per source)
inline eZirconRenderPassSetSource
	zircon_resolve_game_render_pass_set_with_default(
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_scene_folder_path,
	const char* p_config_render_passes_game,
	const char* const* p_registry_game_pass_names,
	kotek::uint8_t registry_game_pass_count,
	const char* p_builtin_default,
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>&
		out_resolved_list) noexcept
{
	if (p_filesystem && p_scene_folder_path &&
		p_scene_folder_path[0] != '\0')
	{
		kotek::static_cstring_t<
			ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>
			scene_list;

		if (zircon_scene_metadata::load_render_passes(
				p_filesystem, p_scene_folder_path, scene_list))
		{
			if (zircon_validate_render_pass_list(scene_list.c_str(),
					p_registry_game_pass_names, registry_game_pass_count,
					"the scene file's render_passes"))
			{
				out_resolved_list = scene_list;

				KOTEK_MESSAGE(
					"[render]: game render pass set resolved from the "
					"scene file ({}/{}): {}",
					p_scene_folder_path, kZirconSceneMetadata_FileName,
					out_resolved_list.c_str());

				return eZirconRenderPassSetSource::kScene;
			}

			KOTEK_MESSAGE_ERROR(
				"the scene file's render_passes ({}/{}) is invalid — "
				"falling back to the config default",
				p_scene_folder_path, kZirconSceneMetadata_FileName);
		}
	}

	if (p_config_render_passes_game &&
		p_config_render_passes_game[0] != '\0')
	{
		if (zircon_validate_render_pass_list(p_config_render_passes_game,
				p_registry_game_pass_names, registry_game_pass_count,
				"the config's render_passes_game"))
		{
			out_resolved_list.assign(p_config_render_passes_game);

			KOTEK_MESSAGE(
				"[render]: game render pass set resolved from the "
				"config (render_passes_game): {}",
				out_resolved_list.c_str());

			return eZirconRenderPassSetSource::kConfig;
		}

		KOTEK_MESSAGE_ERROR(
			"the config's render_passes_game is invalid — falling back "
			"to the built-in default");
	}

	out_resolved_list.assign(p_builtin_default);

	KOTEK_MESSAGE(
		"[render]: game render pass set resolved from the built-in "
		"default: {}",
		out_resolved_list.c_str());

	return eZirconRenderPassSetSource::kBuiltin;
}

/// @brief \~english the bgfx game pass set chain (task Z3 P2h): the
/// backend-neutral resolution above with the bgfx registry + the bgfx
/// built-in default (kZirconConfig_DefaultRenderPassesGame). Kept as
/// the named entry of the bgfx path so callers and tests read the same
/// as before the NRI generalization (task Z5 phase 2 / P4)
inline eZirconRenderPassSetSource zircon_resolve_game_render_pass_set(
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_scene_folder_path,
	const char* p_config_render_passes_game,
	const char* const* p_registry_game_pass_names,
	kotek::uint8_t registry_game_pass_count,
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>&
		out_resolved_list) noexcept
{
	return zircon_resolve_game_render_pass_set_with_default(p_filesystem,
		p_scene_folder_path, p_config_render_passes_game,
		p_registry_game_pass_names, registry_game_pass_count,
		kZirconConfig_DefaultRenderPassesGame, out_resolved_list);
}
