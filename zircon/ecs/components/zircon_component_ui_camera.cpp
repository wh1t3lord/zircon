#include "zircon_component_ui_camera.h"

zircon_component_ui_camera::zircon_component_ui_camera() {}

zircon_component_ui_camera::~zircon_component_ui_camera() {}

void zircon_component_ui_camera::DrawImGui(
	Kotek::Core::ktkMainManager* p_main_manager) noexcept
{
	if (p_main_manager)
	{
		auto* p_wrapper_imgui = p_main_manager->Get_ImguiWrapper();

		if (p_wrapper_imgui)
		{
			if (p_wrapper_imgui->CollapsingHeader("Component UI camera"))
			{
				if (p_wrapper_imgui->Button("set current page"))
				{
				}
				p_wrapper_imgui->SameLine();
				if (p_wrapper_imgui->Button("add page for loading..."))
				{
				}

				if (p_wrapper_imgui->Button("clear current page"))
				{
				}
				p_wrapper_imgui->SameLine();
				if (p_wrapper_imgui->Button("clear pages for loading"))
				{
				}
				p_wrapper_imgui->SameLine();
				if (p_wrapper_imgui->Button("clear all"))
				{
				}

				if (this->m_current_page.empty())
				{
					p_wrapper_imgui->Text("No current page");
				}
				else
				{
					p_wrapper_imgui->Text(Kotek::ktk::format("current page: {}", this->m_current_page).c_str());
				}

				if (this->m_predefined_pages.empty())
				{
					p_wrapper_imgui->Text("No pages for loading");
				}
				else
				{
					p_wrapper_imgui->Text("pages for loading:");
					for (const auto& page_name : this->m_predefined_pages)
					{
						p_wrapper_imgui->Text(page_name.c_str());
					}
				}
			}

		}
	}
}

const Kotek::ktk::cstring& zircon_component_ui_camera::get_current_page(
	void) const noexcept
{
	return this->m_current_page;
}

void zircon_component_ui_camera::set_current_page(
	const Kotek::ktk::cstring& page_name) noexcept
{
	KOTEK_ASSERT(
		page_name.empty() == false, "you must pass a not empty string");

	this->m_current_page = page_name;
}

void zircon_component_ui_camera::clear_current_page(void) noexcept 
{
	this->m_current_page.clear();
}

const Kotek::ktk::unordered_set<Kotek::ktk::cstring>& zircon_component_ui_camera::get_predefined_pages(
	void) const noexcept
{
	return this->m_predefined_pages;
}

void zircon_component_ui_camera::add_page(
	const Kotek::ktk::cstring& page_name) noexcept
{
	KOTEK_ASSERT(page_name.empty() == false, "you can't pass an empty string!");
	this->m_predefined_pages.insert(page_name);
}

void zircon_component_ui_camera::clear_predefined_pages(void) noexcept 
{
	this->m_predefined_pages.clear();
}

void zircon_component_ui_camera::clear_all(void) noexcept 
{
	this->clear_current_page();
	this->clear_predefined_pages();
}
