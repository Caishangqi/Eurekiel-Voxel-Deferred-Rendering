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


class SkyTexturedRenderPass : public SceneRenderPass
{
public:
    SkyTexturedRenderPass();
    ~SkyTexturedRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

    // ShaderBundle event callbacks
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

public:
    // Parameter Access for ImGui - Celestial Body Size
    float GetSunSize() const { return m_sunSize; }
    void  SetSunSize(float size) { m_sunSize = size; }

    float GetMoonSize() const { return m_moonSize; }
    void  SetMoonSize(float size) { m_moonSize = size; }

    // Parameter Access for ImGui - Star Rendering
    bool IsStarRenderingEnabled() const { return m_enableStarRendering; }
    void SetStarRenderingEnabled(bool enabled) { m_enableStarRendering = enabled; }

    unsigned int GetStarSeed() const { return m_starSeed; }
    void         SetStarSeed(unsigned int seed);

    float GetStarBrightnessMultiplier() const { return m_starBrightnessMultiplier; }
    void  SetStarBrightnessMultiplier(float multiplier) { m_starBrightnessMultiplier = multiplier; }

private:
    // Rendering methods
    void RenderSun();
    void RenderMoon();
    void RenderStars();
    bool ShouldRenderStars() const;

    // Celestial matrix pre-computation (once per frame)
    void UpdateCelestialMatrices();

    // Shaders (stars use skybasic, sun/moon use skytextured)
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyBasicShader    = nullptr;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyTexturedShader = nullptr;

    // Textures
    std::shared_ptr<enigma::graphic::D12Texture>  m_sunTexture      = nullptr;
    std::shared_ptr<enigma::graphic::SpriteAtlas> m_moonPhasesAtlas = nullptr;

    // Sun/Moon quad geometry (cached)
    std::vector<Vertex> m_sunQuadVertices;
    std::vector<Vertex> m_moonQuadVertices;

    // Star field geometry (cached)
    std::vector<Vertex>                               m_starVertices;
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_starVB = nullptr;

    // Celestial body size parameters
    float m_sunSize  = 30.0f;
    float m_moonSize = 20.0f;

    // Star rendering parameters
    bool         m_enableStarRendering      = true;
    unsigned int m_starSeed                 = 10842;
    float        m_starBrightnessMultiplier = 1.0f;

    // Constant buffers (uploaded independently each frame)
    CelestialConstantBuffer            celestialData;
    enigma::graphic::PerObjectUniforms perObjectData;

    // Per-frame cached celestial matrices
    Mat44 m_celestialView;
    Mat44 m_celestialViewInverse;
    float m_cachedSkyAngle = 0.0f;
};
