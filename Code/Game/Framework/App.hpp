#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Yaml.hpp"

class Window;
class Game;

static enigma::core::YamlConfiguration settings; // Minecraft Style global configuration

// Forward declaration for resource system
namespace enigma::resource
{
    class ResourceSubsystem;
}


extern Window* g_theWindow;

class App
{
public:
    App();
    ~App();

    void Startup(char* commandLineString = nullptr);
    void Shutdown();
    void RunFrame();

    bool IsQuitting() const;
    void HandleQuitRequested();

    void         AdjustForPauseAndTimeDistortion();
    void HandleKeyBoardEvent();

    void LoadConfigurations();

    /// Event Handle
    static bool Event_ConsoleStartup(EventArgs& args);
    /// 
private:
    void BeginFrame();
    void Update();

    void Render() const;
    void EndFrame();

public:
#pragma region GAME

    std::unique_ptr<Game> m_game;

#pragma endregion GAME

    bool  m_isQuitting       = false;
    bool  m_isPaused         = false;
    bool  m_isSlowMo         = false;
    bool  m_isDebug          = false;
    bool  m_isPendingRestart = false;
    Rgba8 m_backgroundColor  = Rgba8(63, 63, 63, 255);

    AABB2 m_consoleSpace;

    STATIC bool WindowCloseEvent(EventArgs& args);
};
