#pragma once
#include <memory>
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp"

namespace enigma::graphic
{
    class D12Texture;
    class ShaderProgram;
    class D12VertexBuffer;
    class SpriteAtlas;
}


class SkyRenderPass : public SceneRenderPass
{
public:
    SkyRenderPass();
    ~SkyRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

public:
    // [Component 6.4] Parameter Access for ImGui
    bool IsVoidGradientEnabled() const { return m_enableVoidGradient; }
    void SetVoidGradientEnabled(bool enabled) { m_enableVoidGradient = enabled; }

    Vec3 GetSkyZenithColor() const { return m_skyZenithColor; }
    void SetSkyZenithColor(const Vec3& color) { m_skyZenithColor = color; }

    Vec3 GetSkyHorizonColor() const { return m_skyHorizonColor; }
    void SetSkyHorizonColor(const Vec3& color) { m_skyHorizonColor = color; }

    // [NEW] Sun/Moon Size Access for ImGui
    float GetSunSize() const { return m_sunSize; }
    void  SetSunSize(float size) { m_sunSize = size; }

    float GetMoonSize() const { return m_moonSize; }
    void  SetMoonSize(float size) { m_moonSize = size; }

private:
    // [NEW] Sky color rendering methods
    void WriteSkyColorToRT();
    void RenderSunsetStrip();
    void RenderSkyDome();
    void RenderVoidDome();
    void RenderSun();
    void RenderMoon();
    bool ShouldRenderSunsetStrip(float sunAngle) const;

    // [NEW] Void dome conditional rendering
    bool ShouldRenderVoidDome() const;

    // [Component 2] Shaders
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyBasicShader    = nullptr;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyTexturedShader = nullptr;

    // [Component 2] Textures
    std::shared_ptr<enigma::graphic::D12Texture>  m_sunTexture      = nullptr;
    std::shared_ptr<enigma::graphic::SpriteAtlas> m_moonPhasesAtlas = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture>  m_testUV          = nullptr;

    // [Component 5.1] Sky Sphere VertexBuffers (cached)
    // Upper hemisphere (sky dome) + Lower hemisphere (void dome)
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_skyDiscVB = nullptr;
    std::vector<Vertex>                               m_skyDomeVertices; // Upper: centerZ = +16
    std::vector<Vertex>                               m_voidDomeVertices; // Lower: centerZ = -16
    // [NEW] Sunset strip geometry
    std::vector<Vertex> m_sunsetStripVertices;

    // [REFACTOR] Fixed Sun/Moon billboard quad geometry (cached, created in constructor)
    // CPU-side quad vertices in XY plane (Z=0), rotated by modelMatrix on CPU
    // Reference: Minecraft vanilla uses fixed quad with 6 vertices (2 triangles)
    std::vector<Vertex> m_sunQuadVertices; // Sun: 30×30 units, XY plane, Z=0
    std::vector<Vertex> m_moonQuadVertices; // Moon: 20×20 units, XY plane, Z=0

    // [Component 6.4] Sky Rendering Parameters
    bool m_enableVoidGradient = true; // Void gradient toggle
    Vec3 m_skyZenithColor     = Vec3(0.47f, 0.65f, 1.0f); // Sky zenith color (blue)
    Vec3 m_skyHorizonColor    = Vec3(0.75f, 0.85f, 1.0f); // Sky horizon color (light blue)

    // Sun Stripe
    bool m_enableSunStrip = true;

    // [NEW] Celestial Body Size Parameters (ImGui configurable)
    float m_sunSize  = 30.0f; // Sun billboard size (Minecraft default: 30)
    float m_moonSize = 20.0f; // Moon billboard size (Minecraft default: 20)

    CelestialConstantBuffer celestialData;
    enigma::graphic::PerObjectUniforms perObjectData;
};
