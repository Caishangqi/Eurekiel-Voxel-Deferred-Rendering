#pragma once
#include <memory>
#include <unordered_map>

#include "Engine/Graphic/Camera/OrthographicCamera.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class D12VertexBuffer;
    class D12Texture;
    class ShaderProgram;
}

class CompositeRenderPass : public SceneRenderPass
{
public:
    CompositeRenderPass();
    ~CompositeRenderPass() override;
    void Execute() override;

protected:
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

    void BeginPass() override;
    void EndPass() override;

private:
    std::vector<std::shared_ptr<enigma::graphic::ShaderProgram>> m_shaderPrograms;

    // Saved customImage state for stage-scoped texture restore in EndPass
    // Key: customImage slot index, Value: previous D12Texture* at that slot
    std::unordered_map<int, enigma::graphic::D12Texture*> m_savedCustomImages;
};
