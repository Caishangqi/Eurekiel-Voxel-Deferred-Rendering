#pragma once
#include <memory>

#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class ShaderProgram;
}

class FinalRenderPass : public SceneRenderPass
{
public:
    FinalRenderPass();
    ~FinalRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

private:
    std::shared_ptr<enigma::graphic::ShaderProgram> m_shadowProgram = nullptr;
};
