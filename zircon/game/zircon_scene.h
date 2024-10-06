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

	Kotek::ktk::entity_t CreateEntity(void);

	bool RemoveEntity(Kotek::ktk::entity_t id);

	const Kotek::ktk::ustring& GetSceneName(void) const noexcept;

	void SetSceneName(const Kotek::ktk::ustring& scene_name) noexcept;

	const Kotek::ktk::unordered_set<Kotek::ktk::entity_t>& GetEntities(
		void) const noexcept;

	Kotek::ktk::entity_t GetActor(void) const noexcept;
	void SetActor(Kotek::ktk::entity_t actor_id) noexcept;

private:
	Kotek::ktk::entity_t m_actor_entity_id;
	zircon_factory_game* m_p_game_factory;
	zircon_manager_game* m_p_game_manager;
	Kotek::ktk::unordered_set<Kotek::ktk::entity_t> m_entities;
	Kotek::ktk::ustring m_scene_name;
};