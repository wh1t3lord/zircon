#ifndef ZIRCON_RENDER_PASS_FACTORY_H
#define ZIRCON_RENDER_PASS_FACTORY_H

#include "zircon_render_game_passes_enum.h"
#include "zircon_render_editor_passes_enum.h"
#include <cstring>

    inline constexpr const char* convert_render_pass_to_string(eZirconRenderGamePasses pass) {
        switch (pass) {
            case eZirconRenderGamePasses::kno_streaming_zircon_render_graph_pass_imgui_bgfx: return "no_streaming::zircon_render_graph_pass_imgui_bgfx";
            case eZirconRenderGamePasses::kno_streaming_zircon_render_graph_pass_present_bgfx: return "no_streaming::zircon_render_graph_pass_present_bgfx";
            case eZirconRenderGamePasses::kzircon_render_graph_pass_bgfx: return "zircon_render_graph_pass_bgfx";
            case eZirconRenderGamePasses::kzircon_render_graph_pass_debug_bgfx: return "zircon_render_graph_pass_debug_bgfx";
            default: return "";
        }
    }

    inline constexpr const char* convert_render_pass_to_string(eZirconRenderEditorPasses pass) {
        switch (pass) {
            case eZirconRenderEditorPasses::kno_streaming_zircon_render_graph_pass_editor_imgui_bgfx: return "no_streaming::zircon_render_graph_pass_editor_imgui_bgfx";
            case eZirconRenderEditorPasses::kno_streaming_zircon_render_graph_pass_editor_model_static_bgfx: return "no_streaming::zircon_render_graph_pass_editor_model_static_bgfx";
            case eZirconRenderEditorPasses::kno_streaming_zircon_render_graph_pass_editor_present_bgfx: return "no_streaming::zircon_render_graph_pass_editor_present_bgfx";
            case eZirconRenderEditorPasses::kzircon_render_graph_pass_editor_bgfx: return "zircon_render_graph_pass_editor_bgfx";
            case eZirconRenderEditorPasses::kzircon_render_graph_pass_editor_debug_bgfx: return "zircon_render_graph_pass_editor_debug_bgfx";
            default: return "";
        }
    }
class zircon_render_pass_factory {
public:
    static kun_kotek kun_render kun_bgfx ktkRenderGraphSimplifiedRenderPass* create(eZirconRenderGamePasses pass) {
        switch (pass) {
            case kno_streaming_zircon_render_graph_pass_imgui_bgfx: return new no_streaming::zircon_render_graph_pass_imgui_bgfx();
            case kno_streaming_zircon_render_graph_pass_present_bgfx: return new no_streaming::zircon_render_graph_pass_present_bgfx();
            case kzircon_render_graph_pass_bgfx: return new zircon_render_graph_pass_bgfx();
            case kzircon_render_graph_pass_debug_bgfx: return new zircon_render_graph_pass_debug_bgfx();
            default: return nullptr;
        }
    }

    static kun_kotek kun_render kun_bgfx ktkRenderGraphSimplifiedRenderPass* create(eZirconRenderEditorPasses pass) {
        switch (pass) {
            case kno_streaming_zircon_render_graph_pass_editor_imgui_bgfx: return new no_streaming::zircon_render_graph_pass_editor_imgui_bgfx();
            case kno_streaming_zircon_render_graph_pass_editor_model_static_bgfx: return new no_streaming::zircon_render_graph_pass_editor_model_static_bgfx();
            case kno_streaming_zircon_render_graph_pass_editor_present_bgfx: return new no_streaming::zircon_render_graph_pass_editor_present_bgfx();
            case kzircon_render_graph_pass_editor_bgfx: return new zircon_render_graph_pass_editor_bgfx();
            case kzircon_render_graph_pass_editor_debug_bgfx: return new zircon_render_graph_pass_editor_debug_bgfx();
            default: return nullptr;
        }
    }

    static kun_kotek kun_render kun_bgfx ktkRenderGraphSimplifiedRenderPass* create(const char* class_name) {
        // Game passes
        if (std::strcmp(class_name, "no_streaming::zircon_render_graph_pass_imgui_bgfx") == 0) {
            return new no_streaming::zircon_render_graph_pass_imgui_bgfx();
        }
        if (std::strcmp(class_name, "no_streaming::zircon_render_graph_pass_present_bgfx") == 0) {
            return new no_streaming::zircon_render_graph_pass_present_bgfx();
        }
        if (std::strcmp(class_name, "zircon_render_graph_pass_bgfx") == 0) {
            return new zircon_render_graph_pass_bgfx();
        }
        if (std::strcmp(class_name, "zircon_render_graph_pass_debug_bgfx") == 0) {
            return new zircon_render_graph_pass_debug_bgfx();
        }
        
        // Editor passes
        if (std::strcmp(class_name, "no_streaming::zircon_render_graph_pass_editor_imgui_bgfx") == 0) {
            return new no_streaming::zircon_render_graph_pass_editor_imgui_bgfx();
        }
        if (std::strcmp(class_name, "no_streaming::zircon_render_graph_pass_editor_model_static_bgfx") == 0) {
            return new no_streaming::zircon_render_graph_pass_editor_model_static_bgfx();
        }
        if (std::strcmp(class_name, "no_streaming::zircon_render_graph_pass_editor_present_bgfx") == 0) {
            return new no_streaming::zircon_render_graph_pass_editor_present_bgfx();
        }
        if (std::strcmp(class_name, "zircon_render_graph_pass_editor_bgfx") == 0) {
            return new zircon_render_graph_pass_editor_bgfx();
        }
        if (std::strcmp(class_name, "zircon_render_graph_pass_editor_debug_bgfx") == 0) {
            return new zircon_render_graph_pass_editor_debug_bgfx();
        }
        
        return nullptr;
    }

};

#endif // ZIRCON_RENDER_PASS_FACTORY_H
