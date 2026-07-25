#include "zircon_render_graph_pass_editor_imgui.h"
#include "../../../../editor/session/zircon_session_editor.h"
#include "../../../../editor/session/zircon_session_editor_manager.h"

#include "../fs_imgui_image.bin.h"
#include "../fs_ocornut_imgui.bin.h"
#include "../vs_imgui_image.bin.h"
#include "../vs_ocornut_imgui.bin.h"
#include "../droidsans.ttf.h"
#include "../icons_font_awesome.ttf.h"
#include "../icons_kenney.ttf.h"
#include "../robotomono_regular.ttf.h"
#include "../roboto_regular.ttf.h"

const bgfx::EmbeddedShader _kEmbeddedShaders[] = {
	BGFX_EMBEDDED_SHADER(vs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
	BGFX_EMBEDDED_SHADER(vs_imgui_image), BGFX_EMBEDDED_SHADER(fs_imgui_image),
	BGFX_EMBEDDED_SHADER_END()};

struct FontRangeMerge
{
	const void* data;
	size_t size;
	ImWchar ranges[3];
};

#define ICON_MIN_KI 0xe900
#define ICON_MAX_KI 0xe9e3
#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf2e0

static FontRangeMerge s_fontRangeMerge[] = {
	{s_iconsKenneyTtf, sizeof(s_iconsKenneyTtf), {ICON_MIN_KI, ICON_MAX_KI, 0}},
	{s_iconsFontAwesomeTtf, sizeof(s_iconsFontAwesomeTtf),
		{ICON_MIN_FA, ICON_MAX_FA, 0}},
};

inline bool checkAvailTransientBuffers(uint32_t _numVertices,
	const bgfx::VertexLayout& _layout, uint32_t _numIndices)
{
	return _numVertices ==
		bgfx::getAvailTransientVertexBuffer(_numVertices, _layout) &&
		(0 == _numIndices ||
			_numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices));
}

namespace no_streaming
{
	zircon_render_graph_pass_editor_imgui_bgfx::
		zircon_render_graph_pass_editor_imgui_bgfx() :
		zircon_render_graph_pass_editor_bgfx(), m_p_imgui_wrapper{}
	{
	}

	zircon_render_graph_pass_editor_imgui_bgfx::
		~zircon_render_graph_pass_editor_imgui_bgfx(void)
	{
		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
			is_sdk_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				if (is_sdk_enabled)
				{
				}
				else
				{
					bgfx::destroy(m_tex);
					bgfx::destroy(m_texture);

					bgfx::destroy(m_imageLodEnabled);
					bgfx::destroy(m_programImage);
					bgfx::destroy(m_program);

#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					this->m_p_imgui_wrapper->ImGui_ImplGlfw_Shutdown();
#else
	#error not implemented
#endif
				}

				this->m_p_imgui_wrapper->DestroyContext();
			}
		}

		p_config->SetFeatureStatus(kotek::Core::eEngineFeatureSDK::
									   kEngine_Feature_SDK_ImGui_Initialized,
			false);
	}

	void zircon_render_graph_pass_editor_imgui_bgfx::OnCreateResources(
		kotek::core::ktkMainManager* p_main_manager,
		kotek::core::ktkIRenderResourceManager* p_manager_resource)
	{
		KOTEK_ASSERT(p_main_manager, "must be valid!");
		KOTEK_ASSERT(p_main_manager->Get_ImguiWrapper(), "must be valid!");

		this->m_p_manager_main = p_main_manager;
		this->m_p_imgui_wrapper = p_main_manager->Get_ImguiWrapper();

		bool is_imgui_enabled = false;
		bool is_sdk_enabled = false;

		kotek::Core::ktkIFrameworkConfig* p_config =
			p_main_manager->Get_EngineConfig();

		if (p_config)
		{
			is_imgui_enabled =
				p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);

			is_sdk_enabled =
				p_main_manager->Get_EngineConfig()->IsFeatureEnabled(
					kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->DebugCheckVersionAndDataLayout(
					IMGUI_VERSION,
					sizeof(ImGuiIO),
					sizeof(ImGuiStyle),
					sizeof(ImVec2),
					sizeof(ImVec4),
					sizeof(ImDrawVert),
					sizeof(ImDrawIdx));

				this->m_p_imgui_wrapper->CreateContext();

				// the editor context is the default one for the imgui
				// multithreading context manager (model 1: single UI thread)
				this->m_p_imgui_wrapper->Get_ContextManager()
					->AdoptDefaultContext(
						this->m_p_imgui_wrapper->GetCurrentContext());

				this->m_p_imgui_wrapper->GetIO().ConfigFlags |=
					ImGuiConfigFlags_NavEnableKeyboard;

#ifdef KOTEK_DEBUG
				this->m_p_imgui_wrapper->GetIO()
					.ConfigDebugHighlightIdConflicts = true;
#endif

				bgfx::RendererType::Enum renderer_type =
					bgfx::getRendererType();

				this->m_program = bgfx::createProgram(
					bgfx::createEmbeddedShader(
						_kEmbeddedShaders, renderer_type, "vs_ocornut_imgui"),
					bgfx::createEmbeddedShader(
						_kEmbeddedShaders, renderer_type, "fs_ocornut_imgui"),
					true);

				this->m_imageLodEnabled = bgfx::createUniform(
					"u_imageLodEnabled", bgfx::UniformType::Vec4);

				this->m_programImage = bgfx::createProgram(
					bgfx::createEmbeddedShader(
						_kEmbeddedShaders, renderer_type, "vs_imgui_image"),
					bgfx::createEmbeddedShader(
						_kEmbeddedShaders, renderer_type, "fs_imgui_image"),
					true);

				this->m_layout.begin()
					.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
					.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
					.end();

				this->m_tex =
					bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

				uint8_t* data;
				int32_t width;
				int32_t height;
				{
					ImFontConfig config =
						this->m_p_imgui_wrapper->ImFontConfig_Create();
					config.FontDataOwnedByAtlas = false;
					config.MergeMode = false;
					//			config.MergeGlyphCenterV = true;

					const ImWchar* ranges =
						this->m_p_imgui_wrapper->FontAtlas_GetGlyphRangesCyrillic(
							this->m_p_imgui_wrapper->GetIO().Fonts);

					m_font[0] =
						this->m_p_imgui_wrapper->FontAtlas_AddFontFromMemoryTTF(
							this->m_p_imgui_wrapper->GetIO().Fonts,
							(void*)s_robotoRegularTtf,
							sizeof(s_robotoRegularTtf), 18.0f,
							&config, ranges);
					m_font[1] =
						this->m_p_imgui_wrapper->FontAtlas_AddFontFromMemoryTTF(
							this->m_p_imgui_wrapper->GetIO().Fonts,
							(void*)s_robotoMonoRegularTtf,
							sizeof(s_robotoMonoRegularTtf),
							18.0f - 3.0f, &config, ranges);

					config.MergeMode = true;
					config.DstFont = m_font[0];

					for (uint32_t ii = 0; ii < BX_COUNTOF(s_fontRangeMerge);
						++ii)
					{
						const FontRangeMerge& frm = s_fontRangeMerge[ii];

						this->m_p_imgui_wrapper->FontAtlas_AddFontFromMemoryTTF(
							this->m_p_imgui_wrapper->GetIO().Fonts,
							(void*)frm.data,
							(int)frm.size, 18.0f - 3.0f, &config,
							frm.ranges);
					}
				}

				this->m_p_imgui_wrapper->FontAtlas_GetTexDataAsRGBA32(
					this->m_p_imgui_wrapper->GetIO().Fonts,
					&data, &width, &height);

				this->m_texture = bgfx::createTexture2D((uint16_t)width,
					(uint16_t)height, false, 1, bgfx::TextureFormat::BGRA8, 0,
					bgfx::copy(data, width * height * 4));

#ifdef KOTEK_USE_IMGUI_DOCKING
				this->m_p_imgui_wrapper->GetIO().ConfigFlags |=
					ImGuiConfigFlags_DockingEnable;
#endif

				this->m_p_imgui_wrapper->StyleColorsDark();

#ifdef KOTEK_USE_IMGUI_DOCKING
				if (this->m_p_imgui_wrapper->GetIO().ConfigFlags &
					ImGuiConfigFlags_ViewportsEnable)
				{
					this->m_p_imgui_wrapper->GetStyle().WindowRounding = 0.0f;
					this->m_p_imgui_wrapper->GetStyle()
						.Colors[ImGuiCol_WindowBg]
						.w = 1.0f;
				}
#endif

				if (is_sdk_enabled)
				{
				}
				else
				{
#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					GLFWwindow* p_handle = static_cast<GLFWwindow*>(
						p_main_manager->GetGameManager()->GetWindowHandle());

					KOTEK_ASSERT(
						this->m_p_imgui_wrapper->ImGui_ImplGlfw_InitForOther(
							p_handle, false),
						"failed to ImGui_ImplGlfw_InitForOther");
#endif
				}
			}

			p_main_manager->Get_EngineConfig()->SetFeatureStatus(
				kotek::Core::eEngineFeatureSDK::
					kEngine_Feature_SDK_ImGui_Initialized,
				true);
		}
	}

	void zircon_render_graph_pass_editor_imgui_bgfx::OnUpdate(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				// this->m_p_imgui_wrapper->ImGui_ImplOpenGL3_NewFrame();

				this->m_p_imgui_wrapper->ImGui_ImplGlfw_NewFrame();

				if (this->m_p_imgui_wrapper->GetIO().DeltaTime <= 0.0f)
					this->m_p_imgui_wrapper->GetIO().DeltaTime = 0.00001f;

				this->m_p_imgui_wrapper->NewFrame();
			}

			KOTEK_ASSERT(this->m_p_manager_session_editor,
				"Did you call OnRegisterManagers because session manager "
				"editor is "
				"not initialized!");

			zircon_session_editor* p_session =
				this->m_p_manager_session_editor->get_session(
					this->m_p_manager_session_editor->get_current_session_id());

			KOTEK_ASSERT(p_session, "failed to obtain session editor by id: {}",
				this->m_p_manager_session_editor->get_current_session_id());

			if (!p_session)
			{
				KOTEK_MESSAGE_WARNING(
					"failed to obtain session editor by id: {}",
					this->m_p_manager_session_editor->get_current_session_id());
				return;
			}

			auto& imgui_ui_elements = p_session->get_imgui_ui_elements();

			for (auto* p_element : imgui_ui_elements)
			{
				if (p_element)
				{
					p_element->Draw(this->m_p_manager_main);
				}
			}

			if (this->m_p_imgui_wrapper)
			{
				this->m_p_imgui_wrapper->Render();
			}
		}
	}

	void zircon_render_graph_pass_editor_imgui_bgfx::OnRender(
		const kotek::render::bgfx::ktkRenderGraphSimplifiedRenderPass*
			p_previous_pass,
		kotek::ktk::uint32_t my_id_in_queue)
	{
		kotek::Core::ktkIFrameworkConfig* p_config =
			this->m_p_manager_main->Get_EngineConfig();

		bool is_imgui_enabled = false;

		if (p_config)
		{
			is_imgui_enabled = p_config->IsFeatureEnabled(
				kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui);
		}

		if (is_imgui_enabled)
		{
			if (this->m_p_imgui_wrapper)
			{
				ImDrawData* _drawData = this->m_p_imgui_wrapper->GetDrawData();

				// Avoid rendering when minimized, scale coordinates for retina
				// displays (screen coordinates != framebuffer coordinates)
				int32_t dispWidth = int32_t(
					_drawData->DisplaySize.x * _drawData->FramebufferScale.x);
				int32_t dispHeight = int32_t(
					_drawData->DisplaySize.y * _drawData->FramebufferScale.y);
				if (dispWidth <= 0 || dispHeight <= 0)
				{
					return;
				}

				bgfx::setViewName(
					static_cast<bgfx::ViewId>(my_id_in_queue), "ImGui");
				bgfx::setViewMode(static_cast<bgfx::ViewId>(my_id_in_queue),
					bgfx::ViewMode::Sequential);

				const bgfx::Caps* caps = bgfx::getCaps();
				{
					float ortho[16];
					float x = _drawData->DisplayPos.x;
					float y = _drawData->DisplayPos.y;
					float width = _drawData->DisplaySize.x;
					float height = _drawData->DisplaySize.y;

					bx::mtxOrtho(ortho, x, x + width, y + height, y, 0.0f,
						1000.0f, 0.0f, caps->homogeneousDepth);
					bgfx::setViewTransform(
						static_cast<bgfx::ViewId>(my_id_in_queue), NULL, ortho);
					bgfx::setViewRect(static_cast<bgfx::ViewId>(my_id_in_queue), 0, 0,
						uint16_t(width), uint16_t(height));
				}

				const ImVec2 clipPos =
					_drawData->DisplayPos; // (0,0) unless using multi-viewports
				const ImVec2 clipScale =
					_drawData
						->FramebufferScale; // (1,1) unless using retina display
				                            // which are often (2,2)

				// Render command lists
				for (int32_t ii = 0, num = _drawData->CmdListsCount; ii < num;
					++ii)
				{
					bgfx::TransientVertexBuffer tvb;
					bgfx::TransientIndexBuffer tib;

					const ImDrawList* drawList = _drawData->CmdLists[ii];
					uint32_t numVertices = (uint32_t)drawList->VtxBuffer.size();
					uint32_t numIndices = (uint32_t)drawList->IdxBuffer.size();

					if (!checkAvailTransientBuffers(
							numVertices, m_layout, numIndices))
					{
						// not enough space in transient buffer just quit
						// drawing the rest...
						break;
					}

					bgfx::allocTransientVertexBuffer(
						&tvb, numVertices, m_layout);
					bgfx::allocTransientIndexBuffer(
						&tib, numIndices, sizeof(ImDrawIdx) == 4);

					ImDrawVert* verts = (ImDrawVert*)tvb.data;
					bx::memCopy(verts, drawList->VtxBuffer.begin(),
						numVertices * sizeof(ImDrawVert));

					ImDrawIdx* indices = (ImDrawIdx*)tib.data;
					bx::memCopy(indices, drawList->IdxBuffer.begin(),
						numIndices * sizeof(ImDrawIdx));

					bgfx::Encoder* encoder = bgfx::begin();

					for (const ImDrawCmd *cmd = drawList->CmdBuffer.begin(),
										 *cmdEnd = drawList->CmdBuffer.end();
						cmd != cmdEnd; ++cmd)
					{
						if (cmd->UserCallback)
						{
							cmd->UserCallback(drawList, cmd);
						}
						else if (0 != cmd->ElemCount)
						{
							uint64_t state = 0 | BGFX_STATE_WRITE_RGB |
								BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

							bgfx::TextureHandle th = m_texture;
							bgfx::ProgramHandle program = m_program;

							if (ImU64(0) != cmd->TextureId)
							{
								union
								{
									ImTextureID ptr;
									struct
									{
										bgfx::TextureHandle handle;
										uint8_t flags;
										uint8_t mip;
									} s;
								} texture = {cmd->TextureId};

								state |= 0 != (UINT8_C(0x01) & texture.s.flags)
									? BGFX_STATE_BLEND_FUNC(
										  BGFX_STATE_BLEND_SRC_ALPHA,
										  BGFX_STATE_BLEND_INV_SRC_ALPHA)
									: BGFX_STATE_NONE;
								th = texture.s.handle;

								if (0 != texture.s.mip)
								{
									const float lodEnabled[4] = {
										float(texture.s.mip), 1.0f, 0.0f, 0.0f};
									bgfx::setUniform(
										m_imageLodEnabled, lodEnabled);
									program = m_programImage;
								}
							}
							else
							{
								state |= BGFX_STATE_BLEND_FUNC(
									BGFX_STATE_BLEND_SRC_ALPHA,
									BGFX_STATE_BLEND_INV_SRC_ALPHA);
							}

							// Project scissor/clipping rectangles into
							// framebuffer space
							ImVec4 clipRect;
							clipRect.x =
								(cmd->ClipRect.x - clipPos.x) * clipScale.x;
							clipRect.y =
								(cmd->ClipRect.y - clipPos.y) * clipScale.y;
							clipRect.z =
								(cmd->ClipRect.z - clipPos.x) * clipScale.x;
							clipRect.w =
								(cmd->ClipRect.w - clipPos.y) * clipScale.y;

							if (clipRect.x < dispWidth &&
								clipRect.y < dispHeight && clipRect.z >= 0.0f &&
								clipRect.w >= 0.0f)
							{
								const uint16_t xx =
									uint16_t(bx::max(clipRect.x, 0.0f));
								const uint16_t yy =
									uint16_t(bx::max(clipRect.y, 0.0f));
								encoder->setScissor(xx, yy,
									uint16_t(
										bx::min(clipRect.z, 65535.0f) - xx),
									uint16_t(
										bx::min(clipRect.w, 65535.0f) - yy));

								encoder->setState(state);
								encoder->setTexture(0, m_tex, th);
								encoder->setVertexBuffer(
									0, &tvb, cmd->VtxOffset, numVertices);
								encoder->setIndexBuffer(
									&tib, cmd->IdxOffset, cmd->ElemCount);
								encoder->submit(
									static_cast<bgfx::ViewId>(my_id_in_queue), program);
							}
						}
					}

					bgfx::end(encoder);
				}

#ifdef KOTEK_USE_IMGUI_DOCKING
				if (this->m_p_imgui_wrapper->GetIO().ConfigFlags &
					ImGuiConfigFlags_ViewportsEnable)
				{
	#ifdef KOTEK_USE_WINDOW_LIBRARY_GLFW
					GLFWwindow* p_current_context = glfwGetCurrentContext();

					this->m_p_imgui_wrapper->UpdatePlatformWindows();
					this->m_p_imgui_wrapper->RenderPlatformWindowsDefault();
					glfwMakeContextCurrent(p_current_context);
	#else
		#error not implemented
	#endif
				}
#endif
			}
		}
	}
} // namespace no_streaming