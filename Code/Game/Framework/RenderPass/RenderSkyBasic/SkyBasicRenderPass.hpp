#pragma once
#include <memory>
#include <vector>

#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp"

namespace enigma::graphic
{
    class ShaderProgram;
}


class SkyBasicRenderPass : public SceneRenderPass
{
public:
    SkyBasicRenderPass();
    ~SkyBasicRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

    // ShaderBundle event callbacks
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

public:
    // Parameter Access for ImGui
    bool IsVoidGradientEnabled() const { return m_enableVoidGradient; }
    void SetVoidGradientEnabled(bool enabled) { m_enableVoidGradient = enabled; }

    Vec3 GetSkyZenithColor() const { return m_skyZenithColor; }
    void SetSkyZenithColor(const Vec3& color) { m_skyZenithColor = color; }

    Vec3 GetSkyHorizonColor() const { return m_skyHorizonColor; }
    void SetSkyHorizonColor(const Vec3& color) { m_skyHorizonColor = color; }

private:
    // Sky rendering methods
    void WriteSkyColorToRT();
    void RenderSunsetStrip();
    void RenderSkyDome();
    void RenderVoidDome();
    bool ShouldRenderSunsetStrip(float sunAngle) const;
    bool ShouldRenderVoidDome() const;

    // Shader
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyBasicShader = nullptr;

    // Sky geometry (cached, created in constructor)
    std::vector<Vertex> m_skyDomeVertices;
    std::vector<Vertex> m_voidDomeVertices;
    std::vector<Vertex> m_sunsetStripVertices;

    // Sky rendering parameters
    bool m_enableVoidGradient = true;
    Vec3 m_skyZenithColor     = Vec3(0.47f, 0.65f, 1.0f);
    Vec3 m_skyHorizonColor    = Vec3(0.75f, 0.85f, 1.0f);
    bool m_enableSunStrip     = true;

    // Constant buffers (SkyBasic owns CelestialConstantBuffer registration)
    CelestialConstantBuffer celestialData;
};
