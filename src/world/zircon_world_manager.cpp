#include "zircon_world_manager.h"
#include "zircon_world.h"

constexpr kotek::uint8_t _kInvalidWorldID =
	std::numeric_limits<kotek::uint8_t>::max();

static_assert(std::numeric_limits<kotek::uint8_t>::max() >
		ZIRCON_DEF_WORLD_MANAGER_MAX_WORLD_COUNT,
	"overflow are you sure that you need a such amount of worlds? report to "
	"developers https://github.com/wh1t3lord/zircon/issues");

zircon_world_manager::zircon_world_manager(void) {}

zircon_world_manager::~zircon_world_manager(void)
{
	KOTEK_ASSERT(
		this->m_worlds.empty(), "you must deallocate and call shutdown!");
}

void zircon_world_manager::initialize()
{
#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("initialized!");
#endif
}

void zircon_world_manager::shutdown(void)
{
	for (zircon_world* p_world : this->m_worlds)
	{
		KOTEK_ASSERT(p_world,
			"expected always valid otherwise why nullptr place wasn're "
			"replaced by allocated?");

		if (p_world)
		{
			p_world->shutdown();
			delete p_world;
		}
	}

	this->m_worlds.clear();

#ifdef KOTEK_DEBUG
	KOTEK_MESSAGE("destroyed!");
#endif
}

kotek::uint8_t zircon_world_manager::create_world(void) noexcept
{
	kotek::uint8_t generated_world_id{};

	// todo: probably you have to make complex heuristic for generation but for
	// keep architecture simplier let's define as like this
	generated_world_id = this->m_worlds.size();

	zircon_world* p_world = new zircon_world(generated_world_id);

	KOTEK_ASSERT(
		p_world, "failed to allocate memory for world: {}", generated_world_id);

	if (p_world)
	{
#ifdef KOTEK_DEBUG
		KOTEK_MESSAGE("created world: {}", generated_world_id);
#endif
		this->m_worlds.push_back(p_world);
	}
	else
	{
		KOTEK_MESSAGE_ERROR("failed to allocate world: {}", generated_world_id);
		generated_world_id = _kInvalidWorldID;
	}

	return generated_world_id;
}

zircon_world* zircon_world_manager::get_world(kotek::uint8_t id) const noexcept
{
#ifdef KOTEK_DEBUG
	bool was_found{};
	kotek::uint8_t duplicate{};
#endif

	zircon_world* p_result{};

	for (zircon_world* p_world : this->m_worlds)
	{
		if (p_world)
		{
			if (p_world->get_id() == id)
			{
#ifdef KOTEK_DEBUG
				if (!p_result)
#endif
					p_result = p_world;

#ifdef KOTEK_DEBUG
				was_found = true;
				++duplicate;
				KOTEK_ASSERT(
					duplicate == 1, "found a duplicate with same id={}!", id);
#endif

#ifndef KOTEK_DEBUG
				break;
#endif
			}
		}
	}

#ifdef KOTEK_DEBUG
	KOTEK_ASSERT(was_found, "failed to obtain world#{}!", id);
#endif

	return p_result;
}

void zircon_world_manager::destroy_world(kotek::uint8_t id)
{
	zircon_world* p_world = this->get_world(id);

	if (p_world)
	{
		p_world->shutdown();

		auto it = std::find_if(this->m_worlds.begin(), this->m_worlds.end(),
			[id](const zircon_world* p_world) -> bool
			{
				KOTEK_ASSERT(p_world,
					"expected always valid otherwise why nullptr place wasn're "
					"replaced by allocated?");

				if (p_world)
				{
					if (p_world->get_id() == id)
					{
						return true;
					}
				}

				return false;
			});

		KOTEK_ASSERT(it != this->m_worlds.end(),
			"failed to obtain world by id, did you change it in shutdown? It "
			"is wrong in such case...");

		this->m_worlds.erase(it);
		delete p_world;
	}
#ifdef KOTEK_DEBUG
	else
	{
		KOTEK_MESSAGE_WARNING(
			"failed to obtain world#{} in order to destroy it!", id);
	}
#endif
}
