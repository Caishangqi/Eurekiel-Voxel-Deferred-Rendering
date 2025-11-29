#pragma once
#include <memory>

#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

namespace enigma::graphic
{
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
    Geometry* center = nullptr;
    Geometry* center_xyz = nullptr;

    std::shared_ptr<enigma::graphic::ShaderProgram> sp_debugShader = nullptr;
};
