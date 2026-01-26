#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class ShadowCompositeRenderPass : public SceneRenderPass
{
public:
    ~ShadowCompositeRenderPass() override;
    ShadowCompositeRenderPass();
    void Execute() override;

protected:
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

    void BeginPass() override;
    void EndPass() override;

private:
    std::vector<std::shared_ptr<enigma::graphic::ShaderProgram>> m_shaderPrograms;
};
