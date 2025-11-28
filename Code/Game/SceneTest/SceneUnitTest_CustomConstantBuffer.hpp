#pragma once
#include "SceneUnitTest.hpp"

class Geometry;

namespace enigma::graphic
{
    class SpriteAtlas;
    class D12Texture;
}
#pragma warning(push)
#pragma warning(disable: 4324)

struct TestUserCustomUniform
{
    alignas(16) float dummySineValue;
    float             dummyValue1;
    float             dummyValue2;
    float             _padding1;
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
    std::unique_ptr<Geometry> m_cubeC = nullptr; // sprite Atlas Test cube
    std::unique_ptr<Geometry> m_cubeB = nullptr; // sprite Atlas Test cube
#pragma endregion

#pragma region CUSTOM_CONSTANT_BUFFER
    TestUserCustomUniform m_userCustomUniform;
#pragma endregion
};
