#pragma once
#include <memory>

#include "SceneUnitTest.hpp"

class Geometry;

namespace enigma::graphic
{
    class SpriteAtlas;
    class D12Texture;
    class ShaderProgram;
}

class SceneUnitTest_SpriteAtlas : public SceneUnitTest
{
public:
    SceneUnitTest_SpriteAtlas();
    ~SceneUnitTest_SpriteAtlas() override;

    void Update() override;
    void Render() override;

private:
#pragma region RENDER_RESOURCE
    // Basic GBuffer program that convert model space to screen / clip space and render model color
    // The conversion is in Common.hlsl
    std::shared_ptr<enigma::graphic::ShaderProgram> sp_gBufferBasic    = nullptr; // The non-texture program
    std::shared_ptr<enigma::graphic::ShaderProgram> sp_gBufferTextured = nullptr; // The texture program

    std::shared_ptr<enigma::graphic::D12Texture> tex_testUV     = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture> tex_testCaizii = nullptr;

    std::shared_ptr<enigma::graphic::SpriteAtlas> sa_testMoon = nullptr;
#pragma endregion RENDER_RESOURCE

#pragma region GAME_OBJECT
    std::unique_ptr<Geometry> m_cubeC = nullptr; // sprite Atlas Test cube
    std::unique_ptr<Geometry> m_cubeB = nullptr; // sprite Atlas Test cube
#pragma endregion
};
