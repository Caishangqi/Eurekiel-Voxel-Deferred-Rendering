#pragma once
#include <memory>

#include "Engine/Core/Clock.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderProgram.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"

class Geometry;

class Game
{
public:
    explicit Game();
    ~Game();
#pragma region LIFE_HOOK
    void Update();
    void Render();
#pragma endregion LIFE_HOOT

#pragma region GAME_OBJECT
    std::unique_ptr<Geometry>        m_cubeA  = nullptr; // default with outline that do not have depth test
    std::unique_ptr<Geometry>        m_cubeB  = nullptr; // default with texture
    std::unique_ptr<PlayerCharacter> m_player = nullptr; // player
#pragma endregion

#pragma region INPUT_ACTION

private:
    void ProcessInputAction(float deltaSeconds);
    void HandleESC();
#pragma endregion

#pragma region RENDER
    // Basic GBuffer program that convert model space to screen / clip space and render model color
    // The conversion is in Common.hlsl
    std::shared_ptr<enigma::graphic::ShaderProgram> sp_gBufferBasic    = nullptr; // The non-texture program
    std::shared_ptr<enigma::graphic::ShaderProgram> sp_gBufferTextured = nullptr; // The texture program

#pragma endregion RENDER

#pragma region SCENE_TEST


#pragma endregion
#pragma region GAME_CLOCK
    std::unique_ptr<Clock> m_gameClock = nullptr;
#pragma endregion
};
