#pragma once
#include <memory>
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

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

private:
    // [Component 2] Shaders
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyBasicShader    = nullptr;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyTexturedShader = nullptr;

    // [Component 2] Textures
    std::shared_ptr<enigma::graphic::SpriteAtlas> m_sunAtlas        = nullptr;
    std::shared_ptr<enigma::graphic::SpriteAtlas> m_moonPhasesAtlas = nullptr;

    // [Component 5.1] Sky disc VertexBuffer (cached)
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_skyDiscVB = nullptr;
    std::vector<Vertex>                               m_skyDiscVertices;

    // [Component 6.4] Sky Rendering Parameters
    bool m_enableVoidGradient = true; // Void gradient toggle
    Vec3 m_skyZenithColor     = Vec3(0.47f, 0.65f, 1.0f); // Sky zenith color (blue)
    Vec3 m_skyHorizonColor    = Vec3(0.75f, 0.85f, 1.0f); // Sky horizon color (light blue)

    // [REMOVED] Celestial position calculation moved to TimeOfDayManager (P1 fix)
};
