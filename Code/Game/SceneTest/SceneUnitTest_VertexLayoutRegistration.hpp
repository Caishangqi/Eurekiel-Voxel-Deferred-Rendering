#pragma once
#include "SceneUnitTest.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Math/Vec4.hpp"

class Geometry;

namespace enigma::graphic
{
    class D12IndexBuffer;
    class D12VertexBuffer;
    class D12Texture;
    class ShaderProgram;
}

// ============================================================================
// SceneUnitTest_VertexLayoutRegistration
//
// Multi-Layout Unit Test for VertexLayout System Validation:
// - Phase 1: Ring Buffer data independence (3 RGB cubes using Vertex_PCUTBN)
// - Phase 2: Multi-layout rendering with PSO switching (PCU vs PCUTBN)
//
// Expected Results:
// - Phase 1: 3 cubes (Red/Green/Blue) at positions (-4,0,0), (0,0,0), (4,0,0)
// - Phase 2: PSO cache shows distinct PSOs for different layouts
// ============================================================================
class SceneUnitTest_VertexLayoutRegistration : public SceneUnitTest
{
public:
    SceneUnitTest_VertexLayoutRegistration();
    ~SceneUnitTest_VertexLayoutRegistration() override;

    void Render() override;
    void Update() override;

private:
#pragma region RENDER_RESOURCE

    std::shared_ptr<enigma::graphic::D12Texture> m_cubeTexture;

    /// Cube 1
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_cube1VertexBuffer;
    std::shared_ptr<enigma::graphic::D12IndexBuffer>  m_cube1IndexBuffer;
    std::vector<Vertex>                               m_cube1Vertices;
    std::vector<uint32_t>                             m_cube1Indices;
    std::shared_ptr<Geometry>                         m_cube1Geometry;
    enigma::graphic::PerObjectUniforms                m_cube1Uniforms;
    std::shared_ptr<enigma::graphic::ShaderProgram>   m_cube1ShaderProgram;

    /// Cube 2
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_cube2VertexBuffer;
    std::shared_ptr<enigma::graphic::D12IndexBuffer>  m_cube2IndexBuffer;
    std::vector<Vertex>                               m_cube2Vertices;
    std::vector<uint32_t>                             m_cube2Indices;
    std::shared_ptr<Geometry>                         m_cube2Geometry;
    enigma::graphic::PerObjectUniforms                m_cube2Uniforms;
    std::shared_ptr<enigma::graphic::ShaderProgram>   m_cube2ShaderProgram;

    /// Cube 3
    std::shared_ptr<Geometry> m_cube3Geometry;

#pragma endregion RENDER_RESOURCE

#pragma region GAME_OBJECT
#pragma endregion GAME_OBJECT
};
