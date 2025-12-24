#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class TerrainRenderPass : public SceneRenderPass
{
public:
    TerrainRenderPass();
    ~TerrainRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;
};
