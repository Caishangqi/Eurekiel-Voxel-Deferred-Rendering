#pragma once
#include <memory>

#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class ShaderProgram;
}

/**
 * @brief Terrain Cutout Render Pass - Alpha-tested blocks (leaves, grass, etc.)
 *
 * Iris Pipeline Reference:
 * - Phase: TERRAIN_CUTOUT (after TERRAIN_SOLID, before TERRAIN_TRANSLUCENT)
 * - Shader: gbuffers_terrain_cutout (fallback to gbuffers_terrain)
 * - Alpha Test: ONE_TENTH_ALPHA (alpha > 0.1)
 *
 * Key Differences from TerrainRenderPass:
 * - Uses GetCutoutD12VertexBuffer() instead of GetOpaqueD12VertexBuffer()
 * - Shader includes alpha test with discard
 * - No depth sorting needed (unlike translucent)
 *
 * Typical Blocks: oak_leaves, birch_leaves, grass, flowers, etc.
 */
class TerrainCutoutRenderPass : public SceneRenderPass
{
public:
    TerrainCutoutRenderPass();
    ~TerrainCutoutRenderPass() override;
    void Execute() override;

protected:
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

    void BeginPass() override;
    void EndPass() override;

private:
    std::shared_ptr<enigma::graphic::ShaderProgram> m_shaderProgram     = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture>    m_blockAtlasTexture = nullptr;
};
