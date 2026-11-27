#pragma once

#include "../../ecs/zircon_csg_defs.h"

class zircon_component_csg;
class zircon_component_csg_primitive;
class zircon_factory;
class zircon_session_editor_interface;
class zircon_session_editor_manager_interface;
class zircon_ui_test_widget_registry;
class zircon_world;

/// Source/Quake-style brush editor: compound/member list, all current
/// primitive types, subtractive/additive operation, dimensions, transform,
/// material and delete. Every world mutation goes through the Z6 journal.
class zircon_editor_ui_window_csg : public kotek::core::ktkISDKUIElement
{
public:
	zircon_editor_ui_window_csg(
		zircon_session_editor_manager_interface* p_manager_session_editor,
		kotek::core::ktkConsole* p_console,
		zircon_factory* p_factory,
		zircon_ui_test_widget_registry* p_widget_registry = nullptr);
	~zircon_editor_ui_window_csg(void);

	void Initialize(void) override;
	void Shutdown(void) override;
	void Draw(kotek::core::ktkMainManager* p_main_manager) override;

	int Get_ID(void) const override;
	void Show(void) override;
	void Hide(void) override;
	bool Is_Shown(void) const override;

private:
	zircon_session_editor_interface* get_session(void) const noexcept;
	zircon_world* get_world(void) const noexcept;
	zircon_component_csg* get_selected_compound(void) const noexcept;
	zircon_component_csg_primitive* get_selected_primitive(void) const noexcept;

	void create_compound(void) noexcept;
	void create_primitive(eZirconCsgPrimitiveType type) noexcept;
	void delete_selected_primitive(void) noexcept;
	void commit_subtractive(bool is_subtractive) noexcept;
	void commit_edit(void) noexcept;
	void track(const char* p_label) noexcept;

private:
	bool m_is_show_window;
	kotek::entity_t m_selected_primitive_id;
	zircon_session_editor_manager_interface* m_p_manager_session_editor;
	kotek::core::ktkConsole* m_p_console;
	zircon_factory* m_p_factory;
	kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;
	zircon_ui_test_widget_registry* m_p_widget_registry;

	float m_dimensions[3];
	float m_position[3];
	float m_rotation[4];
	int m_material_id;
	bool m_is_subtractive;
};
