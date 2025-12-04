#pragma once
#include <memory>

#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class PlayerCharacter;

namespace enigma::graphic
{
    class D12Texture;
    class ShaderProgram;
}

class Geometry;

class DebugRenderPass : public SceneRenderPass
{
public:
    DebugRenderPass();
    ~DebugRenderPass() override;
    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

private:
    Geometry* center     = nullptr;
    Geometry* center_xyz = nullptr;
    Geometry* gridPlane  = nullptr;

    std::shared_ptr<enigma::graphic::ShaderProgram> sp_debugShader = nullptr;

    std::shared_ptr<enigma::graphic::D12Texture> m_gridTexture = nullptr;

private:
    PlayerCharacter* m_player = nullptr;
    void             RenderCursor();
    void             RenderGrid();
};
