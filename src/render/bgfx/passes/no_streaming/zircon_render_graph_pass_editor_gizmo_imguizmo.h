#pragma once

#include "../../zircon_render_graph_pass_editor.h"

namespace no_streaming
{
	// editor pass "editor_gizmo_imguizmo" (task Z3 P2f): the ImGuizmo
	// variant of the editor gizmo, mutually exclusive with the own gizmo
	// (kZirconConfig_RenderPassEditorGizmo*Name pair — the Render Passes
	// window enforces the exclusion).
	//
	// The pass hooks are INERT BY DESIGN: the imgui frame opens and closes
	// inside the imgui pass's OnUpdate, so a separately-ordered pass can
	// never emit ImGuizmo calls (ImGuizmo draws into the current ImDrawList
	// and reads the live IO — both only exist inside the frame). The actual
	// work is hosted by zircon_editor_ui_window_gizmo_imguizmo, a
	// ui_element whose Draw runs inside that frame: it reads the editor
	// slot's pass-name list through zircon_renderer_bgfx::get_render_graph_
	// info and only Manipulates while THIS pass's class name is in the set
	// and enabled. The pass therefore exists as the registered, config-
	// carryable token that the pass-set mechanism (P1) and the window
	// switch (P2a) operate on — its presence in the editor set IS the
	// activation state. It owns no GPU resources and keeps no per-frame
	// state, which also makes it free under the P3 hot-reload rules.
	class zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx(void);
		~zircon_render_graph_pass_editor_gizmo_imguizmo_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;
		void OnDestroyResources() override;
		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;
		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;
	};
} // namespace no_streaming
