#pragma once
#include <unordered_map>

#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class D12Texture;
}

class DeferredRenderPass : public SceneRenderPass
{
public:
    DeferredRenderPass();
    ~DeferredRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

private:
    std::vector<std::shared_ptr<enigma::graphic::ShaderProgram>> m_shaderPrograms;

    // Saved customImage state for stage-scoped texture restore in EndPass
    std::unordered_map<int, enigma::graphic::D12Texture*> m_savedCustomImages;
};
