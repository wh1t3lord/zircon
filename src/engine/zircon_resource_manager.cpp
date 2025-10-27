#include "zircon_resource_manager.h"

zircon_resource_manager::zircon_resource_manager() :
#ifdef KOTEK_DEBUG
	m_was_shutdown_called{-1}
#endif

{
}

zircon_resource_manager::~zircon_resource_manager(void)
{
#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(
		m_was_shutdown_called == 1 ||
			m_was_shutdown_called == -1,
		"you forgot to call shutdown"
	);
#endif
}

void zircon_resource_manager::initialize(
	Kotek::core::ktkMainManager* p_main_manager
)
{
#ifdef KOTEK_DEBUG
	if (m_was_shutdown_called == -1)
	{
		m_was_shutdown_called = 0;
	}
#endif

	KOTEK_ASSERT(p_main_manager, "must be valid");

	if (p_main_manager)
	{
	}
}

void zircon_resource_manager::shutdown(void)
{
#ifdef KOTEK_DEBUG
	m_was_shutdown_called = 1;
#endif
}

zircon_resource_t::zircon_resource_t() : m_p_desc{}, m_p_view{}
{
}

zircon_resource_t::~zircon_resource_t()
{
	if (this->m_p_desc)
	{
		if (this->m_p_desc->is_temp)
		{
			/*
			kotek::core::ktkIResource* p_interface =
				static_cast<kotek::core::ktkIResource*>(m_p_data
			    );

			delete p_interface;

#ifdef KOTEK_DEBUG
			KOTEK_MESSAGE(
				"destroyed & unloaded resource: {} (temp)",
				this->m_p_desc->filename
			);
#endif
*/
		}
	}
}

const zircon_resource_desc_t*
zircon_resource_t::get_desc() const noexcept
{
	return this->m_p_desc;
}


void* zircon_resource_t::get_view_resource(void) const noexcept
{
	return m_p_view;
}

void zircon_resource_t::set_desc(zircon_resource_desc_t* p_desc
) noexcept
{
	KOTEK_ASSERT(
		this->m_p_desc == nullptr, "you can't override it !"
	);

	if (p_desc)
	{
		this->m_p_desc = p_desc;
	}
}
