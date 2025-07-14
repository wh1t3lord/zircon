/*
 * Auto-generated enum for Zircon Editor UI Windows
 * Do not edit manually!
*/

#pragma once

enum class eZirconWindowIDs : int {
    kWindow_SDK_ComponentInspector,
    kWindow_SDK_ObjectList,
    kWindow_SDK_TopBar,
    kWindow_SDK_Animation,
    kWindow_SDK_DebugInput,
    kWindow_SDK_HistoryCommandLog,
    kWindow_SDK_Log,
    kWindow_SDK_Prefab,
    kWindow_SDK_PrefabBrowser,
    kWindow_SDK_RenderStats,
    kWindow_SDK_Settings,
    kWindow_SDK_Sound,
    kTotalAmountOfEnum
};

inline const char* Translate_ZirconWindowIDs(eZirconWindowIDs id)
{
    switch (id)
    {
        case eZirconWindowIDs::kWindow_SDK_ComponentInspector: return "kWindow_SDK_ComponentInspector";
        case eZirconWindowIDs::kWindow_SDK_ObjectList: return "kWindow_SDK_ObjectList";
        case eZirconWindowIDs::kWindow_SDK_TopBar: return "kWindow_SDK_TopBar";
        case eZirconWindowIDs::kWindow_SDK_Animation: return "kWindow_SDK_Animation";
        case eZirconWindowIDs::kWindow_SDK_DebugInput: return "kWindow_SDK_DebugInput";
        case eZirconWindowIDs::kWindow_SDK_HistoryCommandLog: return "kWindow_SDK_HistoryCommandLog";
        case eZirconWindowIDs::kWindow_SDK_Log: return "kWindow_SDK_Log";
        case eZirconWindowIDs::kWindow_SDK_Prefab: return "kWindow_SDK_Prefab";
        case eZirconWindowIDs::kWindow_SDK_PrefabBrowser: return "kWindow_SDK_PrefabBrowser";
        case eZirconWindowIDs::kWindow_SDK_RenderStats: return "kWindow_SDK_RenderStats";
        case eZirconWindowIDs::kWindow_SDK_Settings: return "kWindow_SDK_Settings";
        case eZirconWindowIDs::kWindow_SDK_Sound: return "kWindow_SDK_Sound";
        default: return "Unknown";
    }
}
