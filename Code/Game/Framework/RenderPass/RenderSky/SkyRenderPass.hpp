#pragma once
#include <memory>
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Engine/Math/Mat44.hpp"
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

    // ShaderBundle event callbacks
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

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

    // [NEW] Star Rendering Parameters Access for ImGui
    bool IsStarRenderingEnabled() const { return m_enableStarRendering; }
    void SetStarRenderingEnabled(bool enabled) { m_enableStarRendering = enabled; }

    unsigned int GetStarSeed() const { return m_starSeed; }
    void         SetStarSeed(unsigned int seed);

    float GetStarBrightnessMultiplier() const { return m_starBrightnessMultiplier; }
    void  SetStarBrightnessMultiplier(float multiplier) { m_starBrightnessMultiplier = multiplier; }

private:
    // [NEW] Sky color rendering methods
    void WriteSkyColorToRT();
    void RenderSunsetStrip();
    void RenderSkyDome();
    void RenderVoidDome();
    void RenderSun();
    void RenderMoon();
    void RenderStars();
    bool ShouldRenderSunsetStrip(float sunAngle) const;
    bool ShouldRenderStars() const;

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

    // [NEW] Star field geometry (cached, created in constructor)
    // Reference: Minecraft LevelRenderer.java:571-620 createStars()
    std::vector<Vertex>                               m_starVertices; // 1500 stars × 6 vertices = 9000 vertices
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_starVB = nullptr;

    // [Component 6.4] Sky Rendering Parameters
    bool m_enableVoidGradient = true; // Void gradient toggle
    Vec3 m_skyZenithColor     = Vec3(0.47f, 0.65f, 1.0f); // Sky zenith color (blue)
    Vec3 m_skyHorizonColor    = Vec3(0.75f, 0.85f, 1.0f); // Sky horizon color (light blue)

    // Sun Stripe
    bool m_enableSunStrip = true;

    // [NEW] Celestial Body Size Parameters (ImGui configurable)
    float m_sunSize  = 30.0f; // Sun billboard size (Minecraft default: 30)
    float m_moonSize = 20.0f; // Moon billboard size (Minecraft default: 20)

    // [NEW] Star Rendering Parameters (ImGui configurable)
    // Reference: Minecraft LevelRenderer.java:571-620, 1556-1560
    bool         m_enableStarRendering      = true; // Star rendering toggle
    unsigned int m_starSeed                 = 10842; // Random seed (Minecraft default: 10842)
    float        m_starBrightnessMultiplier = 1.0f; // Brightness multiplier for artistic control

    CelestialConstantBuffer            celestialData;
    enigma::graphic::PerObjectUniforms perObjectData;

    // ==========================================================================
    // [REFACTOR] Per-Frame Cached Data (computed once in Execute(), reused by render methods)
    // ==========================================================================
    // Problem: celestialView matrix was computed identically in RenderSun(), RenderMoon(), RenderStars()
    // Solution: Pre-compute once per frame, store as member, reuse in all celestial body rendering
    // ==========================================================================

    // Celestial view matrix: camera rotation + time rotation, no translation
    // Used by: RenderSun(), RenderMoon(), RenderStars()
    Mat44 m_celestialView; // View matrix for celestial bodies (no translation)
    Mat44 m_celestialViewInverse; // Inverse of celestial view matrix

    // Sky angle for current frame (cached to avoid multiple GetSunAngle() calls)
    float m_cachedSkyAngle = 0.0f;

    // Helper method to update cached celestial matrices
    void UpdateCelestialMatrices();
};
