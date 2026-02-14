#include "SkyBasicRenderPass.hpp"
#include "SkyGeometryHelper.hpp"
#include "SkyColorHelper.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp"
#include "Game/Framework/RenderPass/WorldRenderingPhase.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Core/RenderState/RasterizeState.hpp"

SkyBasicRenderPass::SkyBasicRenderPass()
{
    // Load shader from current ShaderBundle
    m_skyBasicShader = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_skybasic");

    // Generate sky geometry (two hemispheres)
    m_skyDomeVertices  = SkyGeometryHelper::GenerateSkyDisc(16.0f);
    m_voidDomeVertices = SkyGeometryHelper::GenerateSkyDisc(-16.0f);

    // Register CelestialConstantBuffer (SkyBasic owns registration, executes first)
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(
        9,
        enigma::graphic::UpdateFrequency::PerObject,
        enigma::graphic::BufferSpace::Custom,
        10000
    );
}

SkyBasicRenderPass::~SkyBasicRenderPass()
{
}

void SkyBasicRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (!newBundle) return;
    m_skyBasicShader = newBundle->GetProgram("gbuffers_skybasic");

    // Read sunPathRotation from ShaderBundle const directives
    auto spr = newBundle->GetConstFloat("sunPathRotation");
    g_theGame->m_timeProvider->SetSunPathRotation(spr.value_or(0.0f));
}

void SkyBasicRenderPass::OnShaderBundleUnloaded()
{
    m_skyBasicShader = nullptr;
    g_theGame->m_timeProvider->SetSunPathRotation(0.0f);
}

void SkyBasicRenderPass::Execute()
{
    if (!m_skyBasicShader) return;

    BeginPass();

    // Upload CelestialConstantBuffer
    Mat44 gbufferModelView = g_theGame->m_player->GetCamera()->GetViewMatrix();

    celestialData.celestialAngle            = g_theGame->m_timeProvider->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeProvider->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeProvider->GetCloudTime();
    celestialData.skyBrightness             = g_theGame->m_timeProvider->GetSkyLightMultiplier();
    celestialData.sunPosition               = g_theGame->m_timeProvider->CalculateSunPosition(gbufferModelView);
    celestialData.moonPosition              = g_theGame->m_timeProvider->CalculateMoonPosition(gbufferModelView);
    celestialData.shadowAngle               = g_theGame->m_timeProvider->GetShadowAngle();
    celestialData.shadowLightPosition       = g_theGame->m_timeProvider->CalculateShadowLightPosition(gbufferModelView);
    celestialData.upPosition                = g_theGame->m_timeProvider->CalculateUpPosition(gbufferModelView);
    celestialData.colorModulator            = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // Upload CommonConstantBuffer
    COMMON_UNIFORM.skyColor         = SkyColorHelper::CalculateSkyColor(celestialData.celestialAngle);
    COMMON_UNIFORM.rainStrength     = 0.0f;
    COMMON_UNIFORM.wetness          = 0.0f;
    COMMON_UNIFORM.screenBrightness = 1.0f;
    COMMON_UNIFORM.nightVision      = 0.0f;
    COMMON_UNIFORM.blindness        = 0.0f;
    COMMON_UNIFORM.darknessFactor   = 0.0f;
    COMMON_UNIFORM.renderStage      = ToRenderStage(WorldRenderingPhase::NONE);

    WriteSkyColorToRT();

    g_theRendererSubsystem->UseProgram(m_skyBasicShader, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}});

    {
        MatricesUniforms matUniform;
        g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matUniform);
        matUniform.gbufferView.SetTranslation3D(Vec3(0.0f, 0.0f, 0.0f));
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matUniform);
    }

    RenderSkyDome();
    RenderVoidDome();
    RenderSunsetStrip();

    // Restore renderStage
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::NONE);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    EndPass();
}

void SkyBasicRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
}

void SkyBasicRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());
}

void SkyBasicRenderPass::WriteSkyColorToRT()
{
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float sunAngle       = g_theGame->m_timeProvider->GetSunAngle();
    Vec3  fogColor       = SkyColorHelper::CalculateFogColor(celestialAngle, sunAngle);

    Rgba8 fogColorRgba8(
        static_cast<unsigned char>(fogColor.x * 255.0f),
        static_cast<unsigned char>(fogColor.y * 255.0f),
        static_cast<unsigned char>(fogColor.z * 255.0f),
        255
    );

    g_theRendererSubsystem->ClearRenderTarget(RenderTargetType::ColorTex, 0, fogColorRgba8);
}

void SkyBasicRenderPass::RenderSkyDome()
{
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SKY);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    m_skyDomeVertices    = SkyGeometryHelper::GenerateSkyDiscWithFog(16.0f, celestialAngle);

    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->DrawVertexArray(m_skyDomeVertices);
}

void SkyBasicRenderPass::RenderVoidDome()
{
    if (ShouldRenderVoidDome())
    {
        COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SKY_VOID);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);
        g_theRendererSubsystem->DrawVertexArray(m_voidDomeVertices);
    }
}

void SkyBasicRenderPass::RenderSunsetStrip()
{
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    Vec4  sunriseColor   = SkyColorHelper::CalculateSunriseColor(celestialAngle);
    if (sunriseColor.w <= 0.0f) return;

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUNSET);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    celestialData.colorModulator = sunriseColor;
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    m_sunsetStripVertices = SkyGeometryHelper::GenerateSunriseStrip(sunriseColor, celestialAngle);

    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->DrawVertexArray(m_sunsetStripVertices);
}

bool SkyBasicRenderPass::ShouldRenderSunsetStrip(float sunAngle) const
{
    float timeOfDay;
    if (sunAngle >= 0.25f)
        timeOfDay = sunAngle - 0.25f;
    else
        timeOfDay = sunAngle + 0.75f;

    constexpr float TWO_PI    = 6.28318530718f;
    constexpr float THRESHOLD = 0.4f;
    float           cosValue  = cosf(timeOfDay * TWO_PI);
    return (cosValue >= -THRESHOLD) && (cosValue <= THRESHOLD);
}

bool SkyBasicRenderPass::ShouldRenderVoidDome() const
{
    constexpr float HORIZON_HEIGHT = 63.0f;
    Vec3            cameraPos      = g_theGame->m_player->GetCamera()->GetPosition();
    return cameraPos.z < HORIZON_HEIGHT;
}
