#pragma once

#include "../../ecs/zircon_component_interface.h"

class zircon_factory;
class zircon_editor_ui_state_interface;
class zircon_session_editor_manager;

class zircon_editor_ui_window_component_inspector
	: public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_component_inspector(
		zircon_session_editor_manager* p_manager_session_editor,
		zircon_editor_ui_state_interface* p_sdk_ui
	);
	~zircon_editor_ui_window_component_inspector();

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(Kotek::Core::ktkMainManager* p_main_manager
	) override;

	int Get_ID(void) const override;

	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	bool HasComponent(
		eZirconComponentType component_type, kotek::entity_t id
	) noexcept;

	void update_modal_windows(
		kotek::core::ktkIImguiWrapper* p_wrapper_imgui
	);

private:
	bool m_is_show_window;
	kotek::uint32_t m_current_item_name;
	zircon_editor_ui_state_interface* m_p_manager_sdk_ui;
	zircon_factory* m_p_factory;
	zircon_session_editor_manager* m_p_manager_session_editor;
	const char* m_p_list_selected_item_allocator;
};