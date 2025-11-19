#include "Game.hpp"


#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/SceneTest/SceneUnitTest_StencilXRay.hpp"

Game::Game()
{
    /// Set the game state

    // Set CursorMode
    g_theInput->SetCursorMode(CursorMode::FPS);

    /// Prepare clock;
    m_gameClock = std::make_unique<Clock>(Clock::GetSystemClock());
    m_gameClock->Unpause();

    /// Prepare player
    m_player             = std::make_unique<PlayerCharacter>(this);
    m_player->m_position = Vec3(0, 0, 0);

    /// Scene
    m_scene = std::make_unique<SceneUnitTest_StencilXRay>();
}

Game::~Game()
{
}

void Game::Update()
{
    /// Update InputActions
    ProcessInputAction(m_gameClock->GetDeltaSeconds());
    /// Update Player locomotion
    m_player->Update(m_gameClock->GetDeltaSeconds());
    if (m_scene) m_scene->Update();
}

void Game::Render()
{
    // ========================================
    // [SETUP] Camera and Render Targets
    // ========================================
    m_player->Render();
    if (m_scene) m_scene->Render();
}

void Game::ProcessInputAction(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    if (g_theInput->WasKeyJustPressed(KEYCODE_ESC)) g_theApp->m_isQuitting = true;
    if (g_theInput->WasKeyJustPressed(KEYCODE_TILDE)) g_theInput->GetCursorMode() == CursorMode::POINTER ? g_theInput->SetCursorMode(CursorMode::FPS) : g_theInput->SetCursorMode(CursorMode::POINTER);
}

void Game::HandleESC()
{
}
