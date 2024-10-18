#pragma once

namespace zircon
{
	namespace sdk
	{
		namespace ui
		{
			class zircon_frame;
		}
	} // namespace sdk
} // namespace zircon

class zircon_factory_game;

class zircon_manager_sdk_ui
{
public:
	zircon_manager_sdk_ui(zircon_factory_game* p_factory, bool is_use_sdk);
	~zircon_manager_sdk_ui(void);

	void AddObjectToSceneList(entt::entity id) noexcept;
	void DeleteObjectFromSceneList(entt::entity id) noexcept;

	void imgui_SetSelectedEntity(entt::entity* id) noexcept;
	entt::entity* imgui_GetSelectedEntity(void) noexcept;

	void imgui_SetShowModalSceneSaveAndCloseOrClose(bool status) noexcept;
	bool imgui_GetShowModalSceneSaveAndCloseOrClose(void) noexcept;

	bool is_need_to_show_component_in_widget(
		const kn_kotek::kn_ktk::cstring& component_name) const noexcept;

private:
	bool m_is_show_modal_scene_save;
	bool m_is_use_sdk_imgui;
	kn_kotek::kn_ktk::unordered_map<kn_kotek::kn_ktk::cstring, bool>
		m_components_to_show;
	entt::entity* m_p_view_selected_entity;
};