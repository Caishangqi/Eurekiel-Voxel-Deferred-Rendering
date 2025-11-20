#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class CompositeRenderPass : public SceneRenderPass
{
public:
    CompositeRenderPass();
    ~CompositeRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
};
