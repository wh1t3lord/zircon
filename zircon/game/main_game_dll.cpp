#include "zircon_game.h"
#include "zircon_game_manager.h"
#include <kotek.render/include/kotek_render.h>
#include "../core/zircon_sdk_ui.h"

zircon_manager_game g_main_manager;

constexpr const char* kUserInfoField_RendererForLoading =
	("RendererForLoading");
constexpr const char* kUserInfoField_RendererFallback = ("RendererFallback");
constexpr const char* kUserInfoField_RendererForLoadingVersion =
	("RendererForLoading_Version");
constexpr const char* kUserInfoField_RendererFallbackVersion =
	("RendererFallback_Version");

bool SerializeModule_Game(Kotek::Core::ktkMainManager* p_main_manager)
{
	auto* p_casted =
		dynamic_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

	KOTEK_ASSERT(p_casted, "you must get valid instance here after casting");

	if (p_casted)
	{
		p_casted->Serialize();
	}

	return true;
}

// TODO: rethink, maybe you need to delete it, because we load information from
// user settings for multiple renderers the same thing we have startup renderer.
// So if we failed to load user settings we use what engine has
void DeserializeRendererConfig(Kotek::Core::ktkMainManager* p_main_manager)
{
	if (p_main_manager->Get_Splash())
	{
		p_main_manager->Get_Splash()->Set_Text(
			"[user_game_module]: deserialize [renderer][config]");
		p_main_manager->Get_Splash()->Set_Progress();
	}
}

bool DeserializeModule_Game(Kotek::Core::ktkMainManager* p_main_manager)
{
	if (p_main_manager->Get_Splash())
	{
		p_main_manager->Get_Splash()->Set_Text(
			"[user_game_module]: deserialize [game]");
		p_main_manager->Get_Splash()->Set_Progress();
	}

	KOTEK_ASSERT(p_main_manager,
		"engine must pass a valid pointer to ktkMainManager instance!");

	auto* p_casted =
		dynamic_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

	KOTEK_ASSERT(p_casted, "you must get valid instance here after casting");

	if (p_casted)
	{
		p_casted->Deserialize();
	}

	return true;
}

bool InitializeModule_Game(Kotek::Core::ktkMainManager* p_main_manager)
{
	if (p_main_manager->Get_Splash())
	{
		p_main_manager->Get_Splash()->Set_Text("[user_game_module]: init");
		p_main_manager->Get_Splash()->Set_Progress();
	}

#ifdef KOTEK_USE_CPU_PROFILER
	KOTEK_ASSERT(false,
		"TODO: implement calling from "
		"p_main_manager->GetProfiler()->Initialize()");
	tracy::StartupProfiler();
#endif

	p_main_manager->SetGameManager(&g_main_manager);

#ifdef KOTEK_USE_DEVELOPMENT_TYPE_SHARED
	kotek::Set_LoggerMain(
		p_main_manager->Get_Logger()->Get(kotek::kLoggerMainName));
	kotek::Set_LoggerMsvcOutput(
		p_main_manager->Get_Logger()->Get(kotek::kLoggerMsvcOutputWindowName));
#endif

	if (p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				Kotek::kConsoleCommandArg_Editor))
	{
#ifdef KOTEK_USE_SDK
		KOTEK_MESSAGE("SDK is running...");
		auto argc = p_main_manager->Get_EngineConfig()->GetARGC();

		wxApp::SetInstance(new zircon::sdk::ui::zircon_App(p_main_manager));
		KOTEK_ASSERT(
			wxEntryStart(argc, p_main_manager->Get_EngineConfig()->GetARGV()),
			"can't initialize wxWidgets. Abort...");

		wxTheApp->CallOnInit();

		KOTEK_MESSAGE("SDK is initialized!");
#endif
	}

	if (p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				Kotek::kConsoleCommandArg_Editor_ImGui))
	{
		p_main_manager->Get_EngineConfig()->SetFeatureStatus(
			Kotek::Core::eEngineFeatureSDK::kEngine_Feature_SDK_ImGui, true);
	}

	return true;
}

bool ShutdownModule_Game(Kotek::Core::ktkMainManager* p_main_manager)
{
	SerializeModule_Game(p_main_manager);

#ifdef KOTEK_USE_CPU_PROFILER
	KOTEK_ASSERT(false,
		"TODO: implement calling from "
		"p_main_manager->GetProfiler()->Shutdown()");
	tracy::ShutdownProfiler();
#endif

	if (p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				Kotek::kConsoleCommandArg_Editor))
	{
#ifdef KOTEK_USE_SDK
		wxTheApp->OnExit();
		wxEntryCleanup();
#endif
	}

	Kotek::Render::ShutdownModule_Render(p_main_manager);

	g_main_manager.Shutdown(p_main_manager);

	g_main_manager.GetWindow()->Shutdown();

	return true;
}

void UpdateModule_Game(Kotek::Core::ktkMainManager* p_main_manager)
{
	if (p_main_manager->Get_EngineConfig()
			->IsContainsConsoleCommandLineArgument(
				Kotek::kConsoleCommandArg_Editor))
	{
		// TODO: rethink about architecture duplicating code is not good....
#ifdef KOTEK_USE_SDK
		zircon::sdk::ui::zircon_App* p_app =
			static_cast<zircon::sdk::ui::zircon_App*>(wxTheApp);
		p_app->ShowWindow();
		wxTheApp->OnRun();
#endif
	}
	else
	{
		Kotek::Core::ktkConsole* p_console =
			p_main_manager->GetGameManager()->GetConsole();

		auto* p_game_manager =
			static_cast<zircon_manager_game*>(p_main_manager->GetGameManager());

		Kotek::Core::ktkIWindowManager* p_window_manager =
			p_main_manager->Get_WindowManager();

		auto* p_renderer = p_game_manager->GetRenderer();

		while (p_main_manager->Get_EngineConfig()->IsApplicationWorking())
		{
			bool is_should_close =
				p_window_manager->ActiveWindow_ShouldToClose();
			if (is_should_close)
			{
				if (p_main_manager->Get_EngineConfig()
						->IsContainsConsoleCommandLineArgument(
							Kotek::kConsoleCommandArg_Editor_ImGui))
				{
					is_should_close = false;

					if (p_game_manager->GetSDKUI()
							->imgui_GetShowModalSceneSaveAndCloseOrClose() ==
						false)
					{
						p_game_manager->GetConsole()->PushCommand(static_cast<
							Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_SDK_ShowModalWindow_SaveAndCloseOrCloseScene));
					}
				}
				else
				{
					g_main_manager.GetWindow()->HideWindow();
					p_console->PushCommand(
						static_cast<Kotek::ktk::enum_base_t>(
							Kotek::Core::eConsoleCommandIndex::
								kConsoleCommand_App_Close),
						{{"false"}});
				}
			}

			// TODO: make one function for sdk and this part
			p_console->Flush();

			if (!is_should_close)
			{
				p_game_manager->Update();
				p_renderer->draw();

				p_window_manager->ActiveWindow_PollEvents();
			}
#ifdef KOTEK_USE_CPU_PROFILER
			p_main_manager->GetProfiler()->FrameMark();
#endif
		}
	}
}

bool InitializeModule_Render(kotek::core::ktkMainManager* p_main_manager)
{
	if (p_main_manager->Get_Splash())
	{
		p_main_manager->Get_Splash()->Set_Text(
			"[user_game_module]: init [render]");
		p_main_manager->Get_Splash()->Set_Progress();
	}

	DeserializeRendererConfig(p_main_manager);
	kotek::render::InitializeModule_Render(p_main_manager);

	g_main_manager.Initialize(p_main_manager);
	DeserializeModule_Game(p_main_manager);

	return true;
}
