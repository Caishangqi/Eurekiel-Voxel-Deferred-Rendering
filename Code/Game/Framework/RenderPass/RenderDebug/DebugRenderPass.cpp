#include "DebugRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Math/Sphere.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"

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
        std::vector<Vertex_PCU> verts;
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(1, 0, 0), 0.02f, 0.1f, Rgba8::RED, 8);
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(0, 1, 0), 0.02f, 0.1f, Rgba8::GREEN, 8);
        AddVertsForArrow3D(verts, Vec3::ZERO, Vec3(0, 0, 1), 0.02f, 0.1f, Rgba8::BLUE, 8);
        center_xyz->SetVertices(VertexConversionHelper::ToPCUTBNVector(verts));
    }
    {
        center             = new Geometry(g_theGame);
        center->m_position = Vec3(0, 0, 0);
        center->m_color    = Rgba8::WHITE;
        Sphere centerSphere(Vec3(0, 0, 0), 0.05f);
        center->SetVertices(centerSphere.GetVertices(Rgba8::WHITE, AABB2::ZERO_TO_ONE, 8))->SetIndices(centerSphere.GetIndices(8));
    }
}

DebugRenderPass::~DebugRenderPass()
{
}

void DebugRenderPass::Execute()
{
    BeginPass();
    center_xyz->Render();
    center->Render();
    EndPass();
}

void DebugRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetDepthMode(DepthMode::Disabled);
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->UseProgram(sp_debugShader, {0}, 0);
}

void DebugRenderPass::EndPass()
{
    //g_theRendererSubsystem->PresentRenderTarget(0, RTType::ColorTex);
}
