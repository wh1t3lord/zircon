#pragma once

#include "../../ecs/components/zircon_component_interface.h"

class zircon_factory_game;
class zircon_editor_ui_state_interface;

class zircon_editor_ui_state_component_inspector : public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_state_component_inspector(
		zircon_editor_ui_state_interface* p_sdk_ui, zircon_factory_game* p_factory);
	~zircon_editor_ui_state_component_inspector();

	void initialize(void) override;
	void shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* p_main_manager) override;

	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool HasComponentByName(
		const kotek::static_cstring_t<zircon_DEF_MAX_COMPONENT_NAME_SIZE>&
			component_name_from_preprocessor,
		entt::entity id) noexcept;
 

	void update_modal_windows(kotek::core::ktkIImguiWrapper* p_wrapper_imgui);

private:
	bool m_is_show_window;
	entt::id_type m_combobox_current_item_type;
	zircon_editor_ui_state_interface* m_p_manager_sdk_ui;
	zircon_factory_game* m_p_factory;
	const char* m_p_combobox_current_item;
	const char* m_p_list_selected_item_allocator;
};