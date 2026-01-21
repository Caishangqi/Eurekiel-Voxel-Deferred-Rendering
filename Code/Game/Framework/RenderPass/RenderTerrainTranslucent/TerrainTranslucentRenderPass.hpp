/**
 * @file TerrainTranslucentRenderPass.hpp
 * @brief Translucent Terrain Rendering Pass (Water, Ice, etc.)
 * @date 2026-01-17
 *
 * Architecture:
 * - Renders translucent terrain blocks (water, ice, stained glass)
 * - Depth copy before rendering (depthtex0 -> depthtex1)
 * - Alpha blending with depth test enabled, depth write disabled
 * - Shader fallback: gbuffers_water -> gbuffers_terrain
 *
 * Render Flow:
 * 1. BeginPass(): Copy depth, save render state, setup blend/depth, bind shader
 * 2. Execute(): Iterate chunks, render translucent meshes
 * 3. EndPass(): Restore render state
 *
 * Reference:
 * - TerrainRenderPass.cpp: Chunk iteration and rendering pattern
 * - CloudRenderPass.cpp: Render state management pattern
 */

#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include <memory>

#include "Engine/Graphic/Core/RenderState/BlendState.hpp"
#include "Engine/Graphic/Core/RenderState/DepthState.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"

// Forward declarations
namespace enigma::graphic
{
    class ShaderProgram;
}

/**
 * @class TerrainTranslucentRenderPass
 * @brief Renders translucent terrain blocks (water, ice, glass)
 *
 * Responsibilities:
 * 1. Copy depth texture before rendering (depthtex0 -> depthtex1)
 * 2. Setup alpha blending and depth states
 * 3. Render translucent terrain chunks with gbuffers_water shader
 * 4. Fallback to gbuffers_terrain if gbuffers_water missing
 * 5. Restore render states after rendering
 */
class TerrainTranslucentRenderPass : public SceneRenderPass
{
public:
    TerrainTranslucentRenderPass();
    ~TerrainTranslucentRenderPass() override;

public:
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

private:
    void SetupBlendState();
    void SetupDepthState();
    void RestoreRenderState();

private:
    // Shader programs (water with terrain fallback)
    std::shared_ptr<enigma::graphic::ShaderProgram> m_waterShader    = nullptr;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_fallbackShader = nullptr;

    // Block atlas texture (shared resource, same as TerrainRenderPass)
    std::shared_ptr<enigma::graphic::D12Texture> m_blockAtlasTexture = nullptr;

    // Saved render states (for restoration in EndPass)
    enigma::graphic::DepthConfig m_savedDepthConfig;
    enigma::graphic::BlendConfig m_savedBlendConfig;
};
