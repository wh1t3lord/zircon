#include "zircon_ui_window_render_stats.h"
#include "zircon_editor_ui_state.h"

zircon_ui_window_render_stats::zircon_ui_window_render_stats() :
	m_is_show_window(false)
{
}

zircon_ui_window_render_stats::~zircon_ui_window_render_stats() {}

void zircon_ui_window_render_stats::initialize(void) {}

void zircon_ui_window_render_stats::shutdown(void) {}

void zircon_ui_window_render_stats::Draw(
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

int zircon_ui_window_render_stats::Get_ID(void) const
{
	return static_cast<int>(eZirconWindowIDs::kWindow_SDK_RenderStats);
}

void zircon_ui_window_render_stats::Show(void)
{
	this->m_is_show_window = true;
}

void zircon_ui_window_render_stats::Hide(void)
{
	this->m_is_show_window = false;
}

bool zircon_ui_window_render_stats::Is_Shown(void) const
{
	return this->m_is_show_window;
}
