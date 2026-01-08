#pragma once

#include "../core/zircon_defs.h"

class zircon_world;
class zircon_factory;

class zircon_world_manager
{
public:
	zircon_world_manager(void);
	~zircon_world_manager(void);

	void initialize(void);
	void shutdown(zircon_factory* p_factory);

	kotek::uint8_t create_world(void) noexcept;
	zircon_world* get_world(kotek::uint8_t id) const noexcept;
	void destroy_world(kotek::uint8_t id, zircon_factory* p_factory);

private:
	kotek::static_vector_t<zircon_world*, ZIRCON_DEF_MAX_WORLD_COUNT>
		m_worlds;
};