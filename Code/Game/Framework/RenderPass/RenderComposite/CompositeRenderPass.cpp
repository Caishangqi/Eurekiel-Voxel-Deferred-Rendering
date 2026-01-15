#include "CompositeRenderPass.hpp"

#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Target/DepthTextureProvider.hpp"

CompositeRenderPass::CompositeRenderPass()
{
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shaderProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/composite.vs.hlsl",
        ".enigma/assets/engine/shaders/program/composite.ps.hlsl",
        "composite",
        shaderCompileOptions
    );

    /// Fullquads vertex buffer
    std::vector<Vertex_PCU> verts;
    AABB2                   fullQuadsAABB2;
    auto                    renderHeight = g_theRendererSubsystem->GetConfiguration().renderHeight;
    auto                    renderWidth  = g_theRendererSubsystem->GetConfiguration().renderWidth;
    fullQuadsAABB2.SetDimensions(Vec2((float)renderWidth, (float)renderHeight));
    AddVertsForAABB2D(verts, fullQuadsAABB2, Rgba8::WHITE);
    auto vertsTBN           = VertexConversionHelper::ToPCUTBNVector(verts);
    m_fullQuadsVertexBuffer = D3D12RenderSystem::CreateVertexBuffer(vertsTBN.size() * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN), vertsTBN.data());

    /// Ortho Camera
    m_compositeCamera = std::make_unique<OrthographicCamera>(Vec3::ZERO, EulerAngles(), fullQuadsAABB2.m_mins, fullQuadsAABB2.m_maxs, -1.0f, 1.0f);
}

CompositeRenderPass::~CompositeRenderPass()
{
}

void CompositeRenderPass::Execute()
{
    BeginPass();
    // Logic
    MatricesUniforms matricesUniform;
    m_compositeCamera->UpdateMatrixUniforms(matricesUniform);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniform);
    g_theRendererSubsystem->DrawVertexBuffer(m_fullQuadsVertexBuffer);

    EndPass();
}

void CompositeRenderPass::BeginPass()
{
    // [FIX] Transition depthtex0 to PIXEL_SHADER_RESOURCE for sampling
    // D3D12 Rule: Cannot read (SRV) and write (DSV) same resource simultaneously
    // [REFACTOR] Use D12DepthTexture::TransitionToShaderResource() per OCP
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetProvider(RTType::DepthTex));
    if (depthProvider)
    {
        auto depthTex = depthProvider->GetDepthTexture(0);
        if (depthTex)
        {
            depthTex->TransitionToShaderResource();
        }
    }
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
    if (m_shaderProgram)
    {
        g_theRendererSubsystem->UseProgram(m_shaderProgram, {{RTType::ColorTex, 0}});
    }
}

void CompositeRenderPass::EndPass()
{
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // [FIX] Transition depthtex0 back to DEPTH_WRITE for subsequent passes
    // [REFACTOR] Use D12DepthTexture::TransitionToDepthWrite() per OCP
    auto* depthProvider = static_cast<enigma::graphic::DepthTextureProvider*>(g_theRendererSubsystem->GetProvider(RTType::DepthTex));
    if (depthProvider)
    {
        auto depthTex = depthProvider->GetDepthTexture(0);
        if (depthTex)
        {
            depthTex->TransitionToDepthWrite();
        }
    }
}
