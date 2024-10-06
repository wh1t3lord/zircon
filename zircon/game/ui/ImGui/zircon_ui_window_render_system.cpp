#include "zircon_ui_window_render_system.h"

zircon_ui_window_render_system::zircon_ui_window_render_system() {}

zircon_ui_window_render_system::~zircon_ui_window_render_system() {}

void zircon_ui_window_render_system::initialize(void) {}

void zircon_ui_window_render_system::shutdown(void) {}

void zircon_ui_window_render_system::Draw(
	Kotek::Core::ktkMainManager* p_main_manager)
{
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
								Kotek::Core::eRenderStatistics::
									kStat_Buffer_Index);

						auto* p_stat_vertex_buffer =
							p_render_resource_manager->Get_Statistic(
								Kotek::Core::eRenderStatistics::
									kStat_Buffer_Vertex);

						auto* p_stat_ssbo_matrix_buffer =
							p_render_resource_manager->Get_Statistic(
								Kotek::Core::eRenderStatistics::
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
									sizeof(Kotek::ktk::math::mat4x4f_t));
						}
					}
				}
			}

			p_wrapper_imgui->End();
		}
	}
}
