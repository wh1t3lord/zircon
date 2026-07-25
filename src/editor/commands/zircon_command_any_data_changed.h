#pragma once

namespace zircon
{
	namespace Game
	{
		class zircon_Scene;

		namespace ecs
		{
			class zircon_GameFactory;
		}
	} // namespace Game
} // namespace zircon

class zircon_command_history;

// for language types it is not for graphics!
class zircon_command_any_data_changed : public kotek::Core::ktkISDKRedoUndo
{
public:
	zircon_command_any_data_changed(zircon_command_history* p_history,
		zircon::Game::zircon_Scene* p_scene,
		zircon::Game::ecs::zircon_GameFactory* p_factory);
	~zircon_command_any_data_changed();

	void Execute(void) override;
	void Undo(void) override;
	const char* GetName() override;
};