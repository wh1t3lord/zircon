#pragma once

#include "../../zircon_render_graph_pass_editor.h"

namespace no_streaming
{
	class zircon_render_graph_pass_editor_imgui_bgfx
		: public zircon_render_graph_pass_editor_bgfx
	{
	public:
		zircon_render_graph_pass_editor_imgui_bgfx();
		~zircon_render_graph_pass_editor_imgui_bgfx(void);

		void OnCreateResources(kotek::core::ktkMainManager* p_manager_main,
			kotek::core::ktkIRenderResourceManager* p_manager_resource)
			override;

		void OnUpdate(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

		void OnRender(
			const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
				p_previous_pass, kotek::ktk::uint32_t my_id_in_queue) override;

	private:
		kotek::core::ktkIImguiWrapper* m_p_imgui_wrapper;

		bgfx::ProgramHandle m_program;
		bgfx::ProgramHandle m_programImage;
		bgfx::VertexLayout m_layout;
		bgfx::UniformHandle m_imageLodEnabled;
		bgfx::UniformHandle m_tex;
		bgfx::TextureHandle m_texture;
		ImFont* m_font[2];
		int64_t m_last;
		int32_t m_lastScroll;
		// resolved imgui.ini location (data_user/sdk/settings) — persistent
		// storage for io.IniFilename, which imgui reads at load/save time
		kotek::static_cstring_t<KOTEK_DEF_MAXIMUM_OS_PATH_LENGTH>
			m_imgui_ini_path;
	};
}