#include "zircon_render.h"

namespace zircon
{
	namespace Render
	{
 

		bool InitializeModule_Render(kotek::Core::ktkMainManager* p_main_manager)
		{
			return true;
		}

		bool ShutdownModule_Render(kotek::Core::ktkMainManager* p_main_manager)
		{
			return true;
		}

		void UpdateModule_Render(void) {}
	}
}