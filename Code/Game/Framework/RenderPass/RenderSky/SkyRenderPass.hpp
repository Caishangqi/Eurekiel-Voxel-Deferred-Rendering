#pragma once
#include <memory>

#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class D12Texture;
    class ShaderProgram;
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

private:
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyBasicShader    = nullptr;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_skyTexturedShader = nullptr;

    std::shared_ptr<enigma::graphic::D12Texture> m_sunTexture        = nullptr;
    std::shared_ptr<enigma::graphic::D12Texture> m_moonPhasesTexture = nullptr;
};
