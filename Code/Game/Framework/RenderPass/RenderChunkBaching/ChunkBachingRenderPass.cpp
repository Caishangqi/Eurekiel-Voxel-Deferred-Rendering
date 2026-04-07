#include "ChunkBachingRenderPass.hpp"

#include <cmath>
#include <vector>

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Camera/PerspectiveCamera.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Voxel/Chunk/ChunkRenderRegionStorage.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/Camera/GameCameraDebugState.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp"
#include "Game/Framework/RenderPass/RenderChunkBaching/ChunkBachingDebugViewState.hpp"
#include "Game/Framework/RenderPass/WorldRenderingPhase.hpp"
#include "Game/Gameplay/Game.hpp"

namespace
{
    constexpr float PLAYER_CAMERA_FRUSTUM_LINE_RADIUS  = 0.05f;
    constexpr float PLAYER_CAMERA_FORWARD_ARROW_RADIUS = 0.05f;
    constexpr float PLAYER_CAMERA_FORWARD_ARROW_SIZE   = 0.6f;

    void AppendFrustumEdge(std::vector<Vertex_PCU>& outVerts, const Vec3& start, const Vec3& end, const Rgba8& color)
    {
        AddVertsForCylinder3D(
            outVerts,
            start,
            end,
            PLAYER_CAMERA_FRUSTUM_LINE_RADIUS,
            color,
            AABB2::ZERO_TO_ONE,
            6);
    }

    void BuildCameraFrustumVertices(std::vector<Vertex_PCUTBN>& outVerts, const enigma::graphic::PerspectiveCamera& camera, const Rgba8& color)
    {
        Vec3 forward;
        Vec3 left;
        Vec3 up;
        camera.GetOrientation().GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

        const float halfVerticalRadians = ConvertDegreesToRadians(camera.GetFOV() * 0.5f);
        const float tangentHalfFov      = static_cast<float>(std::tan(halfVerticalRadians));
        const float nearPlane           = camera.GetNearPlane();
        const float farPlane            = camera.GetFarPlane();
        const float aspectRatio         = camera.GetAspectRatio();

        const float nearHalfHeight = tangentHalfFov * nearPlane;
        const float nearHalfWidth  = nearHalfHeight * aspectRatio;
        const float farHalfHeight  = tangentHalfFov * farPlane;
        const float farHalfWidth   = farHalfHeight * aspectRatio;

        const Vec3 cameraPosition = camera.GetPosition();
        const Vec3 nearCenter     = cameraPosition + forward * nearPlane;
        const Vec3 farCenter      = cameraPosition + forward * farPlane;

        const Vec3 nearTopLeft     = nearCenter + left * nearHalfWidth + up * nearHalfHeight;
        const Vec3 nearTopRight    = nearCenter - left * nearHalfWidth + up * nearHalfHeight;
        const Vec3 nearBottomLeft  = nearCenter + left * nearHalfWidth - up * nearHalfHeight;
        const Vec3 nearBottomRight = nearCenter - left * nearHalfWidth - up * nearHalfHeight;

        const Vec3 farTopLeft     = farCenter + left * farHalfWidth + up * farHalfHeight;
        const Vec3 farTopRight    = farCenter - left * farHalfWidth + up * farHalfHeight;
        const Vec3 farBottomLeft  = farCenter + left * farHalfWidth - up * farHalfHeight;
        const Vec3 farBottomRight = farCenter - left * farHalfWidth - up * farHalfHeight;

        std::vector<Vertex_PCU> frustumVerts;
        frustumVerts.reserve(1024);

        AppendFrustumEdge(frustumVerts, nearTopLeft, nearTopRight, color);
        AppendFrustumEdge(frustumVerts, nearTopRight, nearBottomRight, color);
        AppendFrustumEdge(frustumVerts, nearBottomRight, nearBottomLeft, color);
        AppendFrustumEdge(frustumVerts, nearBottomLeft, nearTopLeft, color);

        AppendFrustumEdge(frustumVerts, farTopLeft, farTopRight, color);
        AppendFrustumEdge(frustumVerts, farTopRight, farBottomRight, color);
        AppendFrustumEdge(frustumVerts, farBottomRight, farBottomLeft, color);
        AppendFrustumEdge(frustumVerts, farBottomLeft, farTopLeft, color);

        AppendFrustumEdge(frustumVerts, nearTopLeft, farTopLeft, color);
        AppendFrustumEdge(frustumVerts, nearTopRight, farTopRight, color);
        AppendFrustumEdge(frustumVerts, nearBottomLeft, farBottomLeft, color);
        AppendFrustumEdge(frustumVerts, nearBottomRight, farBottomRight, color);

        const float forwardArrowLength = GetClamped(farPlane * 0.15f, 4.0f, 12.0f);
        AddVertsForArrow3DFixArrowSize(
            frustumVerts,
            cameraPosition,
            cameraPosition + forward * forwardArrowLength,
            PLAYER_CAMERA_FORWARD_ARROW_RADIUS,
            PLAYER_CAMERA_FORWARD_ARROW_SIZE,
            color,
            8);

        std::vector<Vertex_PCUTBN> convertedVerts = VertexConversionHelper::ToPCUTBNVector(frustumVerts);
        outVerts.insert(outVerts.end(), convertedVerts.begin(), convertedVerts.end());
    }
}

ChunkBachingRenderPass::ChunkBachingRenderPass()
{
    m_debugShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_debug.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_debug.ps.hlsl",
        "gbuffers_debug");

    m_regionBoundsGeometry        = std::make_unique<Geometry>(g_theGame);
    m_playerCameraFrustumGeometry = std::make_unique<Geometry>(g_theGame);
}

void ChunkBachingRenderPass::Execute()
{
    const auto* playerCamera = g_theGame ? g_theGame->GetPlayerCamera() : nullptr;
    const auto* renderCamera = g_theGame ? g_theGame->GetRenderCamera() : nullptr;

    const bool shouldDrawRegionWireframe = ChunkBachingDebugViewState::enableRegionWireframe;
    const bool shouldDrawPlayerFrustum = GameCameraDebugState::ShouldDrawPlayerCameraFrustum() &&
        playerCamera &&
        renderCamera &&
        renderCamera != playerCamera;

    if (!shouldDrawRegionWireframe && !shouldDrawPlayerFrustum)
    {
        return;
    }

    BeginPass();
    if (shouldDrawRegionWireframe && RebuildRegionDebugGeometry())
    {
        m_regionBoundsGeometry->Render();
    }
    if (shouldDrawPlayerFrustum && RebuildPlayerCameraFrustumGeometry())
    {
        g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
        m_playerCameraFrustumGeometry->Render();
    }
    EndPass();
}

void ChunkBachingRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::Wireframe());
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->UseProgram(m_debugShader, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}});
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());

    SceneRenderPass::BeginPass();

    if (g_theGame && g_theGame->GetRenderCamera())
    {
        g_theGame->GetRenderCamera()->UpdateMatrixUniforms(MATRICES_UNIFORM);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
    }

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::DEBUG);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);
}

void ChunkBachingRenderPass::EndPass()
{
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());
    SceneRenderPass::EndPass();
}

bool ChunkBachingRenderPass::RebuildRegionDebugGeometry()
{
    std::vector<Vertex_PCUTBN> debugVertices;
    std::vector<Index>         debugIndices;

    if (!g_theGame || !g_theGame->GetWorld())
    {
        m_regionBoundsGeometry->SetVertices(std::vector<Vertex_PCUTBN>{})->SetIndices(std::vector<Index>{});
        return false;
    }

    Frustum mainFrustum;
    const Frustum* mainFrustumPtr = nullptr;
    if (g_theGame && g_theGame->GetChunkBatchCullingCamera() && g_theGame->GetChunkBatchCullingCamera()->GetFrustum(mainFrustum))
    {
        mainFrustumPtr = &mainFrustum;
    }

    const auto& storage = g_theGame->GetWorld()->GetChunkRenderRegionStorage();
    for (const auto& regionEntry : storage.GetRegions())
    {
        const auto& region = regionEntry.second;
        AABB3       regionBounds = region.geometry.worldBounds;
        regionBounds.BuildVertices(debugVertices, debugIndices, GetRegionDebugColor(region, mainFrustumPtr), AABB2::ZERO_TO_ONE);
    }

    m_regionBoundsGeometry->SetVertices(std::move(debugVertices))->SetIndices(std::move(debugIndices));
    return !storage.GetRegions().empty();
}

bool ChunkBachingRenderPass::RebuildPlayerCameraFrustumGeometry()
{
    const auto* playerCamera = g_theGame ? g_theGame->GetPlayerCamera() : nullptr;
    const auto* renderCamera = g_theGame ? g_theGame->GetRenderCamera() : nullptr;
    if (!playerCamera ||
        !renderCamera ||
        renderCamera == playerCamera ||
        !GameCameraDebugState::ShouldDrawPlayerCameraFrustum())
    {
        m_playerCameraFrustumGeometry->SetVertices(std::vector<Vertex_PCUTBN>{})->SetIndices(std::vector<Index>{});
        return false;
    }

    std::vector<Vertex_PCUTBN> frustumVertices;
    frustumVertices.reserve(2048);
    BuildCameraFrustumVertices(frustumVertices, *playerCamera, Rgba8::WHITE);

    m_playerCameraFrustumGeometry->SetVertices(std::move(frustumVertices))->SetIndices(std::vector<Index>{});
    return true;
}

Rgba8 ChunkBachingRenderPass::GetRegionDebugColor(const enigma::voxel::ChunkRenderRegion& region, const Frustum* mainFrustum)
{
    if (region.buildFailed)
    {
        return Rgba8::MAGENTA;
    }

    if (region.dirty)
    {
        return Rgba8::YELLOW;
    }

    if (!region.HasValidBatchGeometry())
    {
        return Rgba8::GRAY;
    }

    if (mainFrustum && !mainFrustum->IsOverlapping(region.geometry.worldBounds))
    {
        return Rgba8::RED;
    }

    return Rgba8::GREEN;
}
