#include "zircon_scene_metadata.h"

bool zircon_scene_metadata::save_render_passes(
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_scene_folder_path,
	const char* p_comma_separated_names) noexcept
{
	KOTEK_ASSERT(
		p_filesystem, "you must pass a valid filesystem instance!");
	KOTEK_ASSERT(p_scene_folder_path && p_scene_folder_path[0] != '\0',
		"you must pass a valid scene folder path!");
	KOTEK_ASSERT(p_comma_separated_names,
		"pass a valid comma-separated pass-name list (never nullptr)");
	KOTEK_ASSERT(p_comma_separated_names &&
			p_comma_separated_names[0] != '\0',
		"an empty pass list must never reach the scene file — the "
		"resolution chain handles fallbacks, an empty key on disk would "
		"read as corruption");

	if (!p_filesystem || !p_scene_folder_path ||
		p_scene_folder_path[0] == '\0' || !p_comma_separated_names ||
		p_comma_separated_names[0] == '\0')
	{
		return false;
	}

	ktk_filesystem_path path_to_file(p_scene_folder_path);
	path_to_file /= kZirconSceneMetadata_FileName;

	kotek::core::ktkResourceText<1024, 2048, false> scene_metadata(
		kZirconSceneMetadata_FileName);

	scene_metadata.Write(
		kZirconSceneMetadata_KeyRenderPasses, p_comma_separated_names);

	// raw array is forced by kotek's template signature
	// (ktkResourceText::Serialize_ToString(char (&)[N], Size&) in
	// kotek.core.filesystem.file_text) — exempt from the no-raw-array
	// rule, same as zircon_config::serialize
	char text[1024];
	kotek::uint16_t text_real_length = 0;

	bool status =
		scene_metadata.Serialize_ToString(text, text_real_length);
	KOTEK_ASSERT(status, "failed to serialize scene metadata!");

	if (!status)
	{
		return false;
	}

	status =
		p_filesystem->Write_File(path_to_file, text, text_real_length);
	KOTEK_ASSERT(
		status, "failed to write scene metadata file: {}", path_to_file);

	return status;
}

bool zircon_scene_metadata::load_render_passes(
	kotek::core::ktkIFileSystem* p_filesystem,
	const char* p_scene_folder_path,
	kotek::static_cstring_t<ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH>&
		out_list) noexcept
{
	KOTEK_ASSERT(
		p_filesystem, "you must pass a valid filesystem instance!");
	KOTEK_ASSERT(p_scene_folder_path && p_scene_folder_path[0] != '\0',
		"you must pass a valid scene folder path!");

	if (!p_filesystem || !p_scene_folder_path ||
		p_scene_folder_path[0] == '\0')
	{
		return false;
	}

	ktk_filesystem_path path_to_file(p_scene_folder_path);
	path_to_file /= kZirconSceneMetadata_FileName;

	// an absent scene.json is the normal "the level carries no pass
	// set" case (older scenes, fresh scenes) — the caller falls back
	// down the resolution chain, silently by design
	if (!p_filesystem->Is_Exists(path_to_file))
	{
		return false;
	}

	kotek::array_t<unsigned char, 1024> text{};

	kotek::ktk::size_t text_size = text.size();

	unsigned char* p_text = text.data();

	bool status = p_filesystem->Read_File(path_to_file, p_text, text_size);
	KOTEK_ASSERT(
		status, "failed to read scene metadata file: {}", path_to_file);

	if (!status)
	{
		return false;
	}

	kotek::core::ktkResourceText<1024, 2048, false> scene_metadata;

	status = scene_metadata.Create_FromMemory(text.data(), text_size);
	KOTEK_ASSERT(status,
		"failed to parse scene metadata file: {} — corrupt json?",
		path_to_file);

	if (!status)
	{
		return false;
	}

	const auto& object = scene_metadata.Get_Object();

	auto it = object.find(kZirconSceneMetadata_KeyRenderPasses);

	if (it == object.end())
	{
		return false;
	}

	// (*it).value(): the own json backend's const iterator has no
	// operator-> (boost's does) — this spelling compiles against both
	// (same as zircon_config::deserialize)
	if (!(*it).value().is_string())
	{
		KOTEK_MESSAGE_ERROR(
			"scene metadata key '{}' must be a string, ignoring "
			"(file: {})",
			kZirconSceneMetadata_KeyRenderPasses, path_to_file);
		return false;
	}

	const auto& value = (*it).value().as_string();

	if (value.empty())
	{
		return false;
	}

	if (value.size() > ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH)
	{
		KOTEK_MESSAGE_ERROR(
			"scene metadata key '{}' is too long ({} > {}) — corrupt "
			"file, ignoring (file: {})",
			kZirconSceneMetadata_KeyRenderPasses, value.size(),
			ZIRCON_DEF_CONFIG_RENDER_PASS_LIST_MAX_LENGTH, path_to_file);
		return false;
	}

	out_list.assign(value.data(), value.size());

	return true;
}
