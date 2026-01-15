#pragma once
#include <memory>

#include "Engine/Graphic/Camera/OrthographicCamera.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
    class D12VertexBuffer;
    class ShaderProgram;
}

class CompositeRenderPass : public SceneRenderPass
{
public:
    CompositeRenderPass();
    ~CompositeRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

private:
    std::shared_ptr<enigma::graphic::ShaderProgram>   m_shaderProgram         = nullptr;
    std::shared_ptr<enigma::graphic::D12VertexBuffer> m_fullQuadsVertexBuffer = nullptr;

    std::unique_ptr<enigma::graphic::OrthographicCamera> m_compositeCamera = nullptr;
};
