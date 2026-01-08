#include "DebugRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Math/Sphere.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"
#include "Game/Framework/RenderPass/ShadowRenderPass/ShadowRenderPass.hpp"
#include "Game/Gameplay/Game.hpp"

DebugRenderPass::DebugRenderPass()
{
    /// Prepare Shader
    sp_debugShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_debug.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_debug.ps.hlsl",
        "gbuffers_debug"
    );
    /// Prepare Geometry
    {
        center_xyz             = new Geometry(g_theGame);
        center_xyz->m_position = Vec3(0, 0, 0);
        center_xyz->m_color    = Rgba8::WHITE;
        center_xyz->m_scale    = Vec3(0.25f, 0.25f, 0.25f);
        std::vector<Vertex_PCU> verts;
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(1, 0, 0), 0.02f, 0.1f, Rgba8::RED, 32);
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(0, 1, 0), 0.02f, 0.1f, Rgba8::GREEN, 32);
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(0, 0, 1), 0.02f, 0.1f, Rgba8::BLUE, 32);
        center_xyz->SetVertices(VertexConversionHelper::ToPCUTBNVector(verts));
    }
    {
        center             = new Geometry(g_theGame);
        center->m_position = Vec3(0, 0, 0);
        center->m_color    = Rgba8::WHITE;
        center->m_scale    = Vec3(0.25f, 0.25f, 0.25f);
        Sphere centerSphere(Vec3(0, 0, 0), 0.05f);
        center->SetVertices(centerSphere.GetVertices(Rgba8::WHITE, AABB2::ZERO_TO_ONE, 8))->SetIndices(centerSphere.GetIndices(8));
    }

    {
        gridPlane             = new Geometry(g_theGame);
        gridPlane->m_position = Vec3(0, 0, 0);
        gridPlane->m_color    = Rgba8(255, 255, 255, 80);
        AABB3 gridPlaneBounds(Vec3(-256, -256, 0), Vec3(256, 256, 0));
        gridPlaneBounds.SetCenter(Vec3(0, 0, 0));

        std::vector<Vertex_PCUTBN> outVerts;
        std::vector<Index>         outIndices;
        gridPlaneBounds.BuildVertices(outVerts, outIndices, Rgba8::WHITE, AABB2(Vec2::ZERO, Vec2(128, 128)));
        gridPlane->SetIndices(std::move(outIndices))->SetVertices(std::move(outVerts));

        m_gridTexture = g_theRendererSubsystem->CreateTexture2D(
            ".enigma/assets/engine/textures/test/grid_256.png",
            TextureUsage::ShaderResource,
            "Test UV Texture"
        );
    }

    {
        lightDirection             = new Geometry(g_theGame);
        lightDirection->m_position = Vec3(0, 0, 0);
        lightDirection->m_color    = Rgba8::WHITE;
        lightDirection->m_scale    = Vec3(0.25f, 0.25f, 0.25f);
        std::vector<Vertex_PCU> verts;
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(-1, 0, 0), 0.02f, 0.1f, Rgba8::ORANGE, 32);
        lightDirection->SetVertices(VertexConversionHelper::ToPCUTBNVector(verts));
    }
}

DebugRenderPass::~DebugRenderPass()
{
}

void DebugRenderPass::Execute()
{
    BeginPass();
    //RenderGrid();
    RenderCursor();
    EndPass();
}

void DebugRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetDepthMode(DepthMode::Disabled);
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(sp_debugShader, {
                                           {RTType::ColorTex, 0}, {RTType::DepthTex, 0}
                                       });
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());

    m_player = g_theGame->m_player.get();
}

void DebugRenderPass::EndPass()
{
    //g_theRendererSubsystem->PresentRenderTarget(0, RTType::ColorTex);
}

void DebugRenderPass::RenderCursor()
{
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    if (!m_player) return;

    EulerAngles playerOrientation = m_player->m_orientation;
    Vec3        playerForward, playerLeft, playerUp;
    playerOrientation.GetAsVectors_IFwd_JLeft_KUp(playerForward, playerLeft, playerUp);


    center_xyz->m_position        = m_player->m_position + playerForward * 3.0f;
    lightDirection->m_position    = m_player->m_position + playerForward * 3.0f;
    lightDirection->m_orientation = dynamic_cast<ShadowRenderPass*>(g_theGame->m_shadowRenderPass.get())->m_lightDirectionEulerAngles;
    center->m_position            = m_player->m_position + playerForward * 3.0f;
    center_xyz->Render();
    lightDirection->Render();
    center->Render();
}

void DebugRenderPass::RenderGrid()
{
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
    g_theRendererSubsystem->SetCustomImage(0, m_gridTexture.get());
    gridPlane->Render();
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());
}
