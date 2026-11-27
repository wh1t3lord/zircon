#include "zircon_ui_window_render_stats.h"
#include "../../core/zircon_editor_ui_state_interface.h"
#include "../../core/zircon_editor_renderer_interface.h"

zircon_editor_ui_window_render_stats::zircon_editor_ui_window_render_stats() :
	m_is_show_window(false)
{
}

zircon_editor_ui_window_render_stats::~zircon_editor_ui_window_render_stats() {}

void zircon_editor_ui_window_render_stats::Initialize(void) {}

void zircon_editor_ui_window_render_stats::Shutdown(void) {}

void zircon_editor_ui_window_render_stats::Draw(
	kotek::core::ktkMainManager* p_main_manager)
{
	if (!this->m_is_show_window)
		return;

	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->Begin("Render System"))
			{
				if (p_wrapper_imgui->CollapsingHeader("Stats"))
				{
					if (p_wrapper_imgui->CollapsingHeader("GPU (bgfx)"))
					{
						zircon_editor_renderer_interface* p_renderer = nullptr;
						if (p_main_manager->GetGameManager())
						{
							p_renderer = dynamic_cast<zircon_editor_renderer_interface*>(
								p_main_manager->GetGameManager()->GetRenderer());
						}

						zircon_editor_gpu_stats_t stats{};
						if (p_renderer && p_renderer->get_editor_gpu_stats(stats))
						{
							constexpr float bytes_to_mib = 1.0f / (1024.0f * 1024.0f);

							p_wrapper_imgui->Text("Texture memory: %.2f MiB",
								static_cast<float>(stats.m_texture_memory_used) * bytes_to_mib);
							p_wrapper_imgui->Text("Render-target memory: %.2f MiB",
								static_cast<float>(stats.m_render_target_memory_used) * bytes_to_mib);
							p_wrapper_imgui->Text("Transient vertex: %.2f MiB",
								static_cast<float>(stats.m_transient_vertex_memory_used) * bytes_to_mib);
							p_wrapper_imgui->Text("Transient index: %.2f MiB",
								static_cast<float>(stats.m_transient_index_memory_used) * bytes_to_mib);
							p_wrapper_imgui->Text("Dynamic VB/IB: %u / %u",
								stats.m_dynamic_vertex_buffer_count,
								stats.m_dynamic_index_buffer_count);
							p_wrapper_imgui->Text("Vertex/index buffers: %u / %u",
								stats.m_vertex_buffer_count, stats.m_index_buffer_count);
							p_wrapper_imgui->Text("Textures: %u | frame buffers: %u",
								stats.m_texture_count, stats.m_frame_buffer_count);
							p_wrapper_imgui->Text("Programs: %u | shaders: %u | uniforms: %u",
								stats.m_program_count, stats.m_shader_count,
								stats.m_uniform_count);
						}
						else
						{
							p_wrapper_imgui->TextDisabled("bgfx stats are unavailable");
						}
					}

					if (p_wrapper_imgui->CollapsingHeader("Memory Management"))
					{
						auto* p_render_resource_manager =
							p_main_manager->GetRenderResourceManager();

						auto* p_stat_index_buffer =
							p_render_resource_manager->Get_Statistic(
								kotek::core::eRenderStatistics::
									kStat_Buffer_Index);

						auto* p_stat_vertex_buffer =
							p_render_resource_manager->Get_Statistic(
								kotek::core::eRenderStatistics::
									kStat_Buffer_Vertex);

						auto* p_stat_ssbo_matrix_buffer =
							p_render_resource_manager->Get_Statistic(
								kotek::core::eRenderStatistics::
									kStat_Buffer_SSBO_Matrix);

						if (p_stat_vertex_buffer)
						{
							float mem_allocated =
								p_stat_vertex_buffer->Get_AllocatedMemory();
							float mem_free =
								p_stat_vertex_buffer->Get_FreeMemory();
							float mem_used =
								p_stat_vertex_buffer->Get_UsedMemory();

							mem_allocated /= (1024.0f * 1024.0f);
							mem_free /= (1024.0f * 1024.0f);
							mem_used /= (1024.0f * 1024.0f);

							p_wrapper_imgui->Text("--- Vertex Buffer ---");
							p_wrapper_imgui->Text(
								"Allocated memory: %.2f Mb", mem_allocated);
							p_wrapper_imgui->Text(
								"Used memory: %.2f Mb", mem_used);
							p_wrapper_imgui->Text(
								"Available memory: %.2f Mb", mem_free);
						}

						if (p_stat_index_buffer)
						{
							float mem_allocated =
								p_stat_index_buffer->Get_AllocatedMemory();
							float mem_free =
								p_stat_index_buffer->Get_FreeMemory();
							float mem_used =
								p_stat_index_buffer->Get_UsedMemory();

							mem_allocated /= (1024.0f * 1024.0f);
							mem_free /= (1024.0f * 1024.0f);
							mem_used /= (1024.0f * 1024.0f);

							p_wrapper_imgui->Text("--- Index Buffer ---");
							p_wrapper_imgui->Text(
								"Allocated memory: %.2f Mb", mem_allocated);
							p_wrapper_imgui->Text(
								"Used memory: %.2f Mb", mem_used);
							p_wrapper_imgui->Text(
								"Available memory: %.2f Mb", mem_free);
						}

						if (p_stat_ssbo_matrix_buffer)
						{
							float mem_allocated = p_stat_ssbo_matrix_buffer
													  ->Get_AllocatedMemory();
							float mem_free =
								p_stat_ssbo_matrix_buffer->Get_FreeMemory();
							float mem_used =
								p_stat_ssbo_matrix_buffer->Get_UsedMemory();

							mem_allocated /= (1024.0f * 1024.0f);
							mem_free /= (1024.0f * 1024.0f);
							mem_used /= (1024.0f * 1024.0f);

							p_wrapper_imgui->Text(
								"--- SSBO Instancing (matrix) ---");
							p_wrapper_imgui->Text(
								"Allocated memory: %.2f Mb", mem_allocated);
							p_wrapper_imgui->Text(
								"Used memory: %.2f Mb", mem_used);
							p_wrapper_imgui->Text(
								"Available memory: %.2f Mb", mem_free);
							p_wrapper_imgui->Text("Instance count: %d",
								p_stat_ssbo_matrix_buffer
										->Get_AllocatedMemory() /
									sizeof(kotek::ktk::math::mat4x4f_t));
						}
					}
				}
			}

			p_wrapper_imgui->End();
		}
	}
}

int zircon_editor_ui_window_render_stats::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_RenderStats);
}

void zircon_editor_ui_window_render_stats::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_editor_ui_window_render_stats::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_editor_ui_window_render_stats::Is_Shown(void) const
{
	return this->m_is_show_window;
}
