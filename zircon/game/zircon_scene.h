#pragma once

class zircon_manager_game;
class zircon_factory_game;

// TODO: probably I need to be sure that this class I can use like thread safe
// otherwise I need to implement that because thread safe is only factory...
class zircon_scene
{
public:
	zircon_scene(void);
	~zircon_scene(void);

	void Initialize(zircon_factory_game* p_factory,
		zircon_manager_game* p_game_manager) noexcept;

	void Shutdown(void) noexcept;

	entt::entity CreateEntity(void);

	bool RemoveEntity(entt::entity id);

	const kotek::ktk::ustring& GetSceneName(void) const noexcept;

	void SetSceneName(const kotek::ktk::ustring& scene_name) noexcept;

	const kotek::view_entities_t& GetEntities(
		void) const noexcept;

	entt::entity GetActor(void) const noexcept;
	void SetActor(entt::entity actor_id) noexcept;

private:
	entt::entity m_actor_entity_id;
	zircon_factory_game* m_p_game_factory;
	zircon_manager_game* m_p_game_manager;
	// TODO: replace with static container pls
	kotek::ktk::ustring m_scene_name;
};