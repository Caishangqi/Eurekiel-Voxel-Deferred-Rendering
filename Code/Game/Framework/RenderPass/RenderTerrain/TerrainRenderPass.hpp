#pragma once
#include <memory>

#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class ShaderProgram;
}

// Terrain render pass - executes gbuffers_terrain shaders
// Uses Buffer Holder pattern: World owns VBO/IBO, this pass consumes them
class TerrainRenderPass : public SceneRenderPass
{
public:
    TerrainRenderPass();
    ~TerrainRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

private:
    std::shared_ptr<enigma::graphic::ShaderProgram> m_shaderProgram     = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture>    m_blockAtlasTexture = nullptr;
    bool                                            m_shadersLoaded     = false;
};
