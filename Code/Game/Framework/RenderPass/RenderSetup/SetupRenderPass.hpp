#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class SetupRenderPass : public SceneRenderPass
{
public:
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
};
