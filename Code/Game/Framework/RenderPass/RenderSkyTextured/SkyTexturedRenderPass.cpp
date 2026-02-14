#include "SkyTexturedRenderPass.hpp"
#include "StarGeometryHelper.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
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
#include "Game/Framework/RenderPass/RenderSkyBasic/SkyGeometryHelper.hpp"

SkyTexturedRenderPass::SkyTexturedRenderPass()
{
    // Load shaders from current ShaderBundle
    m_skyBasicShader    = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_skybasic");
    m_skyTexturedShader = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_skytextured");

    // Load textures
    m_sunTexture = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/environment/sun.png",
        TextureUsage::ShaderResource,
        "Sun Texture"
    );

    m_moonPhasesAtlas = std::make_shared<SpriteAtlas>("MoonPhases");
    m_moonPhasesAtlas->BuildFromGrid(".enigma/assets/engine/textures/environment/moon_phases.png", IntVec2(4, 2));

    // Generate celestial geometry
    m_sunQuadVertices = SkyGeometryHelper::GenerateCelestialQuad(AABB2::ZERO_TO_ONE);

    // Generate star field geometry (1500 stars)
    m_starVertices = StarGeometryHelper::GenerateStarVertices(m_starSeed);
}

SkyTexturedRenderPass::~SkyTexturedRenderPass()
{
}

void SkyTexturedRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (!newBundle) return;
    m_skyBasicShader    = newBundle->GetProgram("gbuffers_skybasic");
    m_skyTexturedShader = newBundle->GetProgram("gbuffers_skytextured");
}

void SkyTexturedRenderPass::OnShaderBundleUnloaded()
{
    m_skyBasicShader    = nullptr;
    m_skyTexturedShader = nullptr;
}

void SkyTexturedRenderPass::Execute()
{
    if (!m_skyBasicShader || !m_skyTexturedShader) return;

    BeginPass();

    // Pre-compute celestial matrices once per frame
    UpdateCelestialMatrices();

    // Upload CelestialConstantBuffer (independent from SkyBasic)
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

    // Upload view matrix with zero translation for celestial rendering
    {
        MatricesUniforms matUniform;
        g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matUniform);
        matUniform.gbufferView.SetTranslation3D(Vec3(0.0f, 0.0f, 0.0f));
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matUniform);
    }

    // Stars use skybasic shader (rendered before sun/moon, behind celestial bodies)
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}});
    RenderStars();

    // Sun/Moon use skytextured shader with additive blending
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Additive());
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}});
    RenderSun();
    RenderMoon();

    // Restore normal camera matrices for subsequent passes
    {
        g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(MATRICES_UNIFORM);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(MATRICES_UNIFORM);
    }

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::NONE);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    EndPass();
}

void SkyTexturedRenderPass::BeginPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
}

void SkyTexturedRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());
}

void SkyTexturedRenderPass::RenderSun()
{
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    constexpr float SUN_DISTANCE = 100.0f;

    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(SUN_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(-90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(m_sunSize));

    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

    perObjectData.modelMatrix        = modelMatrix;
    perObjectData.modelMatrixInverse = modelMatrix.GetInverse();

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    celestialData.colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    g_theRendererSubsystem->SetCustomImage(0, m_sunTexture.get());
    g_theRendererSubsystem->DrawVertexArray(m_sunQuadVertices);
}

void SkyTexturedRenderPass::RenderMoon()
{
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    constexpr float MOON_DISTANCE = -100.0f;

    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(MOON_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(m_moonSize));

    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

    perObjectData.modelMatrix        = modelMatrix;
    perObjectData.modelMatrixInverse = modelMatrix.GetInverse();

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    int moonPhase = g_theGame->m_timeProvider->GetDayCount() % 8;

    g_theRendererSubsystem->SetCustomImage(0, m_moonPhasesAtlas->GetSprite(moonPhase).GetTexture().get());
    m_moonQuadVertices = SkyGeometryHelper::GenerateCelestialQuad(m_moonPhasesAtlas->GetSprite(moonPhase).GetUVBounds());
    g_theRendererSubsystem->DrawVertexArray(m_moonQuadVertices);
}

void SkyTexturedRenderPass::RenderStars()
{
    if (!m_enableStarRendering) return;

    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float rainStrength   = COMMON_UNIFORM.rainStrength;
    float starBrightness = StarGeometryHelper::CalculateStarBrightness(celestialAngle, rainStrength);
    starBrightness       *= m_starBrightnessMultiplier;

    if (starBrightness <= 0.0f) return;

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::STARS);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    celestialData.colorModulator = Vec4(starBrightness, starBrightness, starBrightness, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

    perObjectData.modelMatrix        = Mat44::IDENTITY;
    perObjectData.modelMatrixInverse = Mat44::IDENTITY;

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Additive());
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());
    g_theRendererSubsystem->DrawVertexArray(m_starVertices);
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    celestialData.colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);
}

bool SkyTexturedRenderPass::ShouldRenderStars() const
{
    if (!m_enableStarRendering) return false;

    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float rainStrength   = COMMON_UNIFORM.rainStrength;
    float starBrightness = StarGeometryHelper::CalculateStarBrightness(celestialAngle, rainStrength);
    return starBrightness > 0.0f;
}

void SkyTexturedRenderPass::SetStarSeed(unsigned int seed)
{
    if (m_starSeed != seed)
    {
        m_starSeed     = seed;
        m_starVertices = StarGeometryHelper::GenerateStarVertices(m_starSeed);
    }
}

void SkyTexturedRenderPass::UpdateCelestialMatrices()
{
    m_cachedSkyAngle = g_theGame->m_timeProvider->GetSunAngle();

    MatricesUniforms tempMatrices;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(tempMatrices);

    m_celestialView       = tempMatrices.gbufferView;
    float sunPathRotation = g_theGame->m_timeProvider->GetSunPathRotation();
    m_celestialView.AppendXRotation(-sunPathRotation);
    m_celestialView.AppendYRotation(-360.0f * m_cachedSkyAngle);
    m_celestialView.SetTranslation3D(Vec3::ZERO);

    m_celestialViewInverse = m_celestialView.GetInverse();
}
