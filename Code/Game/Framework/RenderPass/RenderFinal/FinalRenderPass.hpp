#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class FinalRenderPass : public SceneRenderPass
{
public:
    FinalRenderPass();
    ~FinalRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
};
