#include "FinalRenderPass.hpp"

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Common/ShaderCompilationHelper.hpp"

FinalRenderPass::FinalRenderPass()
{
    /*enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_shadowProgram = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/final.vs.hlsl",
        ".enigma/assets/engine/shaders/program/final.ps.hlsl",
        "final",
        shaderCompileOptions
    );*/
}

FinalRenderPass::~FinalRenderPass()
{
}

void FinalRenderPass::Execute()
{
    BeginPass();
    /// Logic In Here

    EndPass();
}

void FinalRenderPass::BeginPass()
{
}

void FinalRenderPass::EndPass()
{
}
