#pragma once

namespace Kotek
{
	namespace Core
	{
		class ktkMainManager;
	}
}

namespace zircon
{
	namespace Render
	{
		bool InitializeModule_Render(Kotek::Core::ktkMainManager* p_main_manager);
		bool ShutdownModule_Render(Kotek::Core::ktkMainManager* p_main_manager);
		void UpdateModule_Render(void);
	}
}