#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class CloudRenderPass : public SceneRenderPass
{
public:
    CloudRenderPass();
    ~CloudRenderPass() override;

public:
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
};
