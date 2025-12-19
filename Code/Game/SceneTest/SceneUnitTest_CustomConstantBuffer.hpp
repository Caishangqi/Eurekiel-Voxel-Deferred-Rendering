#pragma once
#include "SceneUnitTest.hpp"
#include "Engine/Math/Vec4.hpp"

class Geometry;

namespace enigma::graphic
{
    class SpriteAtlas;
    class D12Texture;
}
#pragma warning(push)
#pragma warning(disable: 4324)

// [NEW] Test Custom Uniform for multi-draw data independence verification
// Uses Vec4 color to visually verify Ring Buffer isolation
struct TestUserCustomUniform
{
    Vec4  color; // RGBA color for visual verification
    float padding[12]; // Padding to 64 bytes (256-byte aligned for CBV)
};

class SceneUnitTest_CustomConstantBuffer : public SceneUnitTest
{
public:
    SceneUnitTest_CustomConstantBuffer();
    ~SceneUnitTest_CustomConstantBuffer() override;

    void Update() override;
    void Render() override;

private:
#pragma region RENDER_RESOURCE
    std::shared_ptr<enigma::graphic::ShaderProgram> sp_gBufferTestCustomBuffer = nullptr; // Test Shader program

    std::shared_ptr<enigma::graphic::D12Texture> tex_testUV     = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture> tex_testCaizii = nullptr;

    std::shared_ptr<enigma::graphic::SpriteAtlas> sa_testMoon = nullptr;
#pragma endregion RENDER_RESOURCE

#pragma region GAME_OBJECT
    // [NEW] Three cubes for RGB multi-draw test (left to right: Red, Green, Blue)
    std::unique_ptr<Geometry> m_cubeA = nullptr; // Red cube at (-4, 0, 0)
    std::unique_ptr<Geometry> m_cubeB = nullptr; // Green cube at (0, 0, 0)
    std::unique_ptr<Geometry> m_cubeC = nullptr; // Blue cube at (4, 0, 0)
#pragma endregion

#pragma region CUSTOM_CONSTANT_BUFFER
    TestUserCustomUniform m_userCustomUniform;
#pragma endregion
};
