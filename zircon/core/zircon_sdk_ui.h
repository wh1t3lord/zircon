#pragma once

class zircon_sdk_ui_interface
{
public:
	virtual ~zircon_sdk_ui_interface() {}

	virtual entt::entity get_selected_entity() const = 0;
	virtual void set_selected_entity(entt::entity id) = 0;

	virtual bool is_need_to_show_component_in_widget(
		const char* p_component_name) = 0;

	virtual void set_imgui_show_modal_save_scene(bool show) = 0;
	virtual bool is_imgui_show_modal_save_scene(void) const = 0;
};

enum class eZirconWindowIDs : int
{
	kWindow_SDK_Log,
	kWindow_SDK_Debug_Input,
	kWindow_SDK_Prefab,
	kWindow_SDK_CommandHistoryLog,
	kWindow_SDK_Sound,
	kWindow_SDK_3DModelAnimation,
	kWindow_SDK_Topbar,
	kWindow_SDK_ObjectList,
	kWindow_SDK_Settings,
	kWindow_SDK_ComponentInspector,
	kWindow_SDK_RenderStats,
	kTotalAmountOfEnum
};


const char* Translate_ZirconWindowIDs(eZirconWindowIDs id);