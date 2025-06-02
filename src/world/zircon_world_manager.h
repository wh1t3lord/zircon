#pragma once

class zircon_world;

#define ZIRCON_DEF_WORLD_MANAGER_MAX_WORLD_COUNT 1

class zircon_world_manager
{
public:
	zircon_world_manager(void);
	~zircon_world_manager(void);

	void initialize(void);
	void shutdown(void);

	kotek::uint8_t create_world(void) noexcept;
	zircon_world* get_world(kotek::uint8_t id) const noexcept;
	void destroy_world(kotek::uint8_t id);

private:
	kotek::static_vector_t<zircon_world*,
		ZIRCON_DEF_WORLD_MANAGER_MAX_WORLD_COUNT>
		m_worlds;
};