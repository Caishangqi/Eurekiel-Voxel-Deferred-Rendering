#pragma once
#include <memory>

#include "SceneUnitTest.hpp"

class Geometry;

namespace enigma::graphic
{
    class D12Texture;
    class ShaderProgram;
}

class SceneUnitTest_StencilXRay : public SceneUnitTest
{
public:
    SceneUnitTest_StencilXRay();
    ~SceneUnitTest_StencilXRay() override;

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
#pragma endregion RENDER_RESOURCE

#pragma region GAME_OBJECT
    std::unique_ptr<Geometry> m_cubeA = nullptr; // default with outline that do not have depth test
    std::unique_ptr<Geometry> m_cubeB = nullptr; // default with texture
#pragma endregion
};
