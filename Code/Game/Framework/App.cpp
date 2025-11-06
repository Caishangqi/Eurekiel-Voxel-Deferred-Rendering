#include "Game/Framework/App.hpp"

#include "Engine/Window/Window.hpp"
#include "Engine/Audio/AudioSubsystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"

// Resource system integration
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/ResourceCommon.hpp"

// Model system integration  
#include "Engine/Model/ModelSubsystem.hpp"

// Logger system integration
#include "Engine/Core/Logger/Logger.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

// Console system integration
#include "Engine/Core/Console/ConsoleSubsystem.hpp"
#include "Engine/Registry/Core/RegisterSubsystem.hpp"

// Window configuration parser
#include "WindowConfigParser.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"
#include "Engine/Core/ImGui/ImGuiSubsystemConfig.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Integration/RendererSubsystemImGuiContext.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"

// Windows API for testing
#ifdef _WIN32
#include <windows.h>
#endif

Window*                g_theWindow   = nullptr;
IRenderer*             g_theRenderer = nullptr;
App*                   g_theApp      = nullptr;
RandomNumberGenerator* g_rng         = nullptr;
InputSystem*           g_theInput    = nullptr;
AudioSubsystem*        g_theAudio    = nullptr;
Game*                  g_theGame     = nullptr;

App::App()
{
    // Create Engine instance
    enigma::core::Engine::CreateInstance();
}

App::~App()
{
}

void App::Startup(char*)
{
    // ========================================================================
    // Initialize GEngine - Provide a global access point to the Log system
    // ========================================================================

    // If GEngine is not initialized, create a basic engine instance
    if (!GEngine)
    {
        enigma::core::Engine::CreateInstance();
    }


    // ========================================================================
    // Initialize the core engine system (minimum set)
    // ========================================================================

    // Create and register LoggerSubsystem first (highest priority)
    auto logger = std::make_unique<LoggerSubsystem>();
    GEngine->RegisterSubsystem(std::move(logger));

    // Create ResourceSubsystem with configuration
    enigma::resource::ResourceConfig resourceConfig;
    resourceConfig.baseAssetPath    = ".enigma/assets";
    resourceConfig.enableHotReload  = true;
    resourceConfig.logResourceLoads = true;
    resourceConfig.printScanResults = true;
    resourceConfig.AddNamespace("simpleminer", ""); // Add custom namespaces

    auto resourceSubsystem = std::make_unique<enigma::resource::ResourceSubsystem>(resourceConfig);
    GEngine->RegisterSubsystem(std::move(resourceSubsystem));

    // Event system - required, other systems rely on event communication
    EventSystemConfig eventConfig;
    g_theEventSystem = new EventSystem(eventConfig);

    // Subscribe to key events - window closing event
    g_theEventSystem->SubscribeEventCallbackFunction("WindowCloseEvent", WindowCloseEvent);

    // Input system - for testing controls
    InputSystemConfig inputConfig;
    g_theInput = new InputSystem(inputConfig);

    // Window system - render target
    WindowConfig windowConfig;
    windowConfig.m_windowTitle = "Enigma Deferred Rendering Pipeline";
    windowConfig.m_aspectRatio = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    windowConfig.m_windowMode  = WindowMode::Windowed;
    windowConfig.m_resolution  = IntVec2(1920, 1080);
    windowConfig.m_inputSystem = g_theInput;
    g_theWindow                = new Window(windowConfig);

    //Start the event system
    g_theEventSystem->Startup();

    //Start the input system
    g_theInput->Startup();

    // Start the window system - create the actual Win32 window
    g_theWindow->Startup();

    // Render Subsystem
    enigma::graphic::RendererSubsystemConfig renderConfig;
    renderConfig.targetWindow            = g_theWindow;
    renderConfig.renderWidth             = static_cast<uint32_t>(windowConfig.m_resolution.x);
    renderConfig.renderHeight            = static_cast<uint32_t>(windowConfig.m_resolution.y);
    renderConfig.maxFramesInFlight       = 3;
    renderConfig.enableDebugLayer        = ENABLE_DEBUG;
    renderConfig.enableGPUValidation     = ENABLE_GPU_VALIDATION;
    renderConfig.enableBindlessResources = true;
    auto rendererSubsystem               = std::make_unique<enigma::graphic::RendererSubsystem>(renderConfig);
    GEngine->RegisterSubsystem(std::move(rendererSubsystem));

    // Imgui Subsystem
    ImGuiSubsystemConfig imguiConfig;
    auto*                rendererSubsystemPtr = GEngine->GetSubsystem<enigma::graphic::RendererSubsystem>();
    imguiConfig.renderContext                 = std::make_shared<RendererSubsystemImGuiContext>(rendererSubsystemPtr);
    imguiConfig.targetWindow                  = g_theWindow;
    auto imguiSubsystem                       = std::make_unique<ImGuiSubsystem>(imguiConfig);
    GEngine->RegisterSubsystem(std::move(imguiSubsystem));

    GEngine->Startup();

    g_theLogger->SetGlobalLogLevel(enigma::core::LogLevel::DEBUG);

    m_game    = std::make_unique<Game>();
    g_theGame = m_game.get();
}

void App::Shutdown()
{
    /*
     *  All Destroy and ShutDown process should be reverse order of the StartUp
     */

    // Destroy the game
    g_theGame = nullptr;

    // Shutdown Engine subsystems (handles ResourceSubsystem and AudioSubsystem)
    GEngine->Shutdown();

    // Clear global pointers (Engine manages the objects now)
    g_theResource = nullptr;
    g_theAudio    = nullptr;

    DebugRenderSystemShutdown();
    g_theWindow->Shutdown();
    g_theInput->Shutdown();
    g_theEventSystem->Shutdown();
    
    delete g_theWindow;
    g_theWindow = nullptr;

    delete g_theInput;
    g_theInput = nullptr;

    delete g_theEventSystem;
    g_theEventSystem = nullptr;

    // Destroy Engine instance
    Engine::DestroyInstance();
}

void App::RunFrame()
{
    BeginFrame(); //Engine pre-frame stuff
    Update(); // Game updates / moves / spawns / hurts
    Render(); // Game draws current state of things
    EndFrame(); // Engine post-frame
}

bool App::IsQuitting() const
{
    return m_isQuitting;
}

void App::HandleQuitRequested()
{
    m_isQuitting = true;
}

void App::HandleKeyBoardEvent()
{
}

void App::LoadConfigurations()
{
    settings = YamlConfiguration::LoadFromFile(".enigma/settings.yml");
}

bool App::Event_ConsoleStartup(EventArgs& args)
{
    UNUSED(args)
    using namespace enigma::core;
    LogInfo("Game", "This is an example log info test.");

    // Output to DevConsole (will be mirrored to IDE Console via DevConsoleAppender)
    g_theDevConsole->AddLine(Rgba8(95, 95, 95),
                             "Mouse        - Aim\n"
                             "W/A          - Move\n"
                             "S/D          - Strafe\n"
                             "Q/E          - Down | Up\n"
                             "Shift        - Sprint\n"
                             "LMB          - Place select block\n"
                             "RMB          - Break block under player\n"
                             "Wheel Up     - Select Previous block\n"
                             "Wheel Down   - Select Next block\n"
                             "F8           - Reload the Game\n"
                             "F3           - Toggle Chunk Pool Statistic\n"
                             "F3 + G       - Toggle Chunk Boarder\n"
                             "ESC          - Quit\n"
                             "P            - Pause the Game\n"
                             "C            - Switch Camera mode\n"
                             "O            - Step single frame\n"
                             "T            - Toggle time scale between 0.1 and 1.0\n"
                             "~            - Toggle Develop Console");
    return true;
}

void App::AdjustForPauseAndTimeDistortion()
{
}


void App::BeginFrame()
{
    Clock::TickSystemClock();
    GEngine->BeginFrame();
    g_theInput->BeginFrame();
    g_theEventSystem->BeginFrame();
}

void App::Update()
{
    // Update Engine subsystems (including ConsoleSubsystem)
    GEngine->Update(Clock::GetSystemClock().GetDeltaSeconds());

    // Cursor
    g_theInput->SetCursorMode(CursorMode::POINTER);

    HandleKeyBoardEvent();

    if (m_game) m_game->Update();
}

// If this methods is const, the methods inn the method should also promise
// const
void App::Render() const
{
    g_theGame->Render();

    g_theImGui->Render();
}

void App::EndFrame()
{
    if (g_theInput) g_theInput->EndFrame();
    GEngine->EndFrame();
}

bool App::WindowCloseEvent(EventArgs& args)
{
    UNUSED(args)
    g_theApp->HandleQuitRequested();
    return false;
}
