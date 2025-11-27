/**
 * @file CloudRenderPass.hpp
 * @brief Cloud Rendering Pass (Minecraft Vanilla Style)
 * @date 2025-11-26
 *
 * Features:
 * - Renders 32x32 cell cloud mesh grid
 * - Fast Mode: 6144 vertices (2 triangles per cell)
 * - Fancy Mode: 24576 vertices (8 triangles per cell)
 * - Cloud height: Z=192-196 (Engine coordinate system)
 * - Alpha blending for semi-transparent clouds
 * - Cloud animation driven by cloudTime from CelestialConstantBuffer
 *
 * Render Flow:
 * 1. BeginPass: Bind colortex0/6/3, set depth test (LESS_EQUAL, no write), enable alpha blending
 * 2. Execute: Upload CelestialConstantBuffer, draw cloud mesh with gbuffers_clouds shader
 * 3. EndPass: Clean up depth/stencil/blend states
 */

#pragma once
#include <memory>
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class D12VertexBuffer;
    class ShaderProgram;
}

class CloudRenderPass : public SceneRenderPass
{
public:
    CloudRenderPass();
    ~CloudRenderPass() override;

public:
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

public:
    // [Component 6.5] Parameter Access for ImGui
    float GetCloudSpeed() const { return m_cloudSpeed; }
    void  SetCloudSpeed(float speed) { m_cloudSpeed = speed; }

    float GetCloudOpacity() const { return m_cloudOpacity; }
    void  SetCloudOpacity(float opacity) { m_cloudOpacity = opacity; }

    bool IsFancyMode() const { return m_fancyMode; }
    void SetFancyMode(bool fancy);

private:
    // [Component 3] Shader
    std::shared_ptr<enigma::graphic::ShaderProgram> m_cloudsShader = nullptr;

    // [Component 5.2] Cloud mesh VertexBuffer (cached)
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_cloudMeshVB      = nullptr;
    size_t                                            m_cloudVertexCount = 0; // 6144 (Fast) or 24576 (Fancy)

    // [Component 6.5] Cloud Rendering Parameters
    float m_cloudSpeed   = 1.0f; // Cloud animation speed multiplier
    float m_cloudOpacity = 0.8f; // Cloud transparency (0.0 = transparent, 1.0 = opaque)
    bool  m_fancyMode    = false; // false = Fast (6144 verts), true = Fancy (24576 verts)
};
