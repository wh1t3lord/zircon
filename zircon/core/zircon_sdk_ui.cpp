#include "zircon_sdk_ui.h"

const char* Translate_ZirconWindowIDs(eZirconWindowIDs id)
{
	switch (id)
	{
	case eZirconWindowIDs::kWindow_SDK_Log:
		return "Log";
	case eZirconWindowIDs::kWindow_SDK_Debug_Input:
		return "Debug - Input";
	case eZirconWindowIDs::kWindow_SDK_Prefab:
		return "Prefab";
	case eZirconWindowIDs::kWindow_SDK_CommandHistoryLog:
		return "Command History Log";
	case eZirconWindowIDs::kWindow_SDK_Sound:
		return "Sound";
	case eZirconWindowIDs::kWindow_SDK_3DModelAnimation:
		return "Model - Animation";
	case eZirconWindowIDs::kWindow_SDK_Topbar:
		return "Topbar";
	case eZirconWindowIDs::kWindow_SDK_ObjectList:
		return "Object list";
	case eZirconWindowIDs::kWindow_SDK_Settings:
		return "Settings";
	case eZirconWindowIDs::kWindow_SDK_ComponentInspector:
		return "Component Inspector";
	case eZirconWindowIDs::kWindow_SDK_RenderStats:
		return "Render - Statistics";
	default:
		return "UNDEFINED_ENUM_OF_ZIRCON_WINDOW_IDS";
	}
}
