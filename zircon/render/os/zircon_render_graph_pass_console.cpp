#include "zircon_render_graph_pass_console.h"

zircon_render_graph_pass_console::zircon_render_graph_pass_console(
	kotek::core::ktkMainManager* p_main_manager) :
	m_p_main_manager{p_main_manager}, m_p_console{}
{
}

zircon_render_graph_pass_console::~zircon_render_graph_pass_console()
{
	if (this->m_p_console)
	{
		delete this->m_p_console;
		this->m_p_console = nullptr;
	}
}

void zircon_render_graph_pass_console::OnCreateResources(
	kotek::core::ktkMainManager* p_manager_main,
	kotek::core::ktkIRenderResourceManager* p_manager_resource)
{
	KOTEK_ASSERT(p_manager_main, "can't be!");
	KOTEK_ASSERT(p_manager_main->GetFileSystem(), "can't be!");
	KOTEK_ASSERT(p_manager_main->Get_WindowManager(), "can't be!");
	KOTEK_ASSERT(
		p_manager_main->Get_WindowManager()->Get_ActiveWindow(), "can't be!");

	if (!p_manager_main || !p_manager_main->GetFileSystem())
		return;

	if (!p_manager_main->GetResourceManager() ||
		!p_manager_main->Get_WindowManager() ||
		!p_manager_main->Get_WindowManager()->Get_ActiveWindow())
		return;

	this->m_p_console = new kotek::core::ktkWindowConsole();

	kotek::static_path_t path_to_log;

	path_to_log = p_manager_main->GetFileSystem()->GetFolderByEnum(
		kotek::core::eFolderIndex::kFolderIndex_UserData);
	path_to_log /= KOTEK_USE_LOG_OUTPUT_FILE_NAME;

#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(p_manager_main->GetFileSystem()->IsValidPath(path_to_log),
		"stange must exist on disk or in archive!");
#endif

	this->m_p_console->Initialize(
		p_manager_main->Get_WindowManager()->Get_ActiveWindow(),
		p_manager_main->GetResourceManager(), path_to_log);
}

void zircon_render_graph_pass_console::OnUpdate(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	if (this->m_p_console)
	{
		this->m_p_console->Update();
	}
}

void zircon_render_graph_pass_console::OnRender(
	const kotek::render::gl::ktkRenderGraphSimplifiedRenderPass*
		p_previous_pass)
{
	if (this->m_p_console)
	{
		//	this->m_p_console->Render();
	}
}
