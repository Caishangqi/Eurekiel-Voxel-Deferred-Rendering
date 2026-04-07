#pragma once

#include <memory>

#include "Engine/Core/Rgba8.hpp"
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"

class Frustum;
class Geometry;

namespace enigma::graphic
{
    class ShaderProgram;
}

namespace enigma::voxel
{
    struct ChunkRenderRegion;
}

class ChunkBachingRenderPass : public SceneRenderPass
{
public:
    ChunkBachingRenderPass();
    ~ChunkBachingRenderPass() override = default;

    void Execute() override;

protected:
    void BeginPass() override;
    void EndPass() override;

private:
    bool  RebuildRegionDebugGeometry();
    bool  RebuildPlayerCameraFrustumGeometry();

private:
    std::unique_ptr<Geometry>                         m_regionBoundsGeometry;
    std::unique_ptr<Geometry>                         m_playerCameraFrustumGeometry;
    std::shared_ptr<enigma::graphic::ShaderProgram>  m_debugShader = nullptr;

public:
    static Rgba8 GetRegionDebugColor(const enigma::voxel::ChunkRenderRegion& region, const Frustum* mainFrustum);
};
