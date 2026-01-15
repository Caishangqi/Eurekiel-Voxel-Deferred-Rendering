#include "SkyRenderPass.hpp"
#include "SkyGeometryHelper.hpp"
#include "SkyColorHelper.hpp"
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
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"

SkyRenderPass::SkyRenderPass()
{
    // Load shaders
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_skyBasicShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_skybasic.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_skybasic.ps.hlsl",
        "gbuffers_skybasic",
        shaderCompileOptions
    );
    auto testAlpha        = m_skyBasicShader->GetDirectives().GetAlphaTest();
    auto testRenderTarget = m_skyBasicShader->GetDirectives().GetDrawBuffers();


    m_skyTexturedShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_skytextured.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_skytextured.ps.hlsl",
        "gbuffers_skytextured",
        shaderCompileOptions
    );

    // Load textures
    m_sunTexture = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/environment/sun.png",
        TextureUsage::ShaderResource,
        "Sun Texture"
    );

    m_testUV = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/TestUV.png",
        TextureUsage::ShaderResource,
        "Test UV Texture"
    );

    m_moonPhasesAtlas = std::make_shared<SpriteAtlas>("MoonPhases");
    m_moonPhasesAtlas->BuildFromGrid(".enigma/assets/engine/textures/environment/moon_phases.png", IntVec2(4, 2));

    // Generate sky geometry (two hemispheres)
    // Reference: Minecraft LevelRenderer.java sky structure
    m_skyDomeVertices  = SkyGeometryHelper::GenerateSkyDisc(16.0f);
    m_voidDomeVertices = SkyGeometryHelper::GenerateSkyDisc(-16.0f);
    m_sunQuadVertices  = SkyGeometryHelper::GenerateCelestialQuad(AABB2::ZERO_TO_ONE);

    // Register constant buffers
    // [FIX] Use BufferSpace::Custom for Descriptor Table path (space=1)
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(
        9, // slot
        enigma::graphic::UpdateFrequency::PerObject,
        enigma::graphic::BufferSpace::Custom,
        10000 // maxDraws
    );
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CommonConstantBuffer>(
        8, // slot
        enigma::graphic::UpdateFrequency::PerObject,
        enigma::graphic::BufferSpace::Custom,
        10000 // maxDraws
    );
}

SkyRenderPass::~SkyRenderPass()
{
}

void SkyRenderPass::Execute()
{
    BeginPass();

    // Upload CelestialConstantBuffer
    // Reference: Iris CelestialUniforms.java
    // [REFACTOR] Use GetViewMatrix() instead of deprecated GetWorldToCameraTransform()
    Mat44 gbufferModelView = g_theGame->m_player->GetCamera()->GetViewMatrix();

    celestialData.celestialAngle            = g_theGame->m_timeProvider->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeProvider->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeProvider->GetCloudTime();

    celestialData.skyBrightness = g_theGame->m_timeProvider->GetSkyLightMultiplier();

    celestialData.sunPosition  = g_theGame->m_timeProvider->CalculateSunPosition(gbufferModelView);
    celestialData.moonPosition = g_theGame->m_timeProvider->CalculateMoonPosition(gbufferModelView);

    // [NEW] Shadow-related uniforms - Reference: Iris CelestialUniforms.java:34-42, 93-95
    celestialData.shadowAngle         = g_theGame->m_timeProvider->GetShadowAngle();
    celestialData.shadowLightPosition = g_theGame->m_timeProvider->CalculateShadowLightPosition(gbufferModelView);

    // [NEW] Up direction vector - Reference: Iris CelestialUniforms.java:44-58
    celestialData.upPosition = g_theGame->m_timeProvider->CalculateUpPosition(gbufferModelView);

    celestialData.colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // Upload CommonConstantBuffer
    // Reference: Iris CommonUniforms.java
    commonData.skyColor         = SkyColorHelper::CalculateSkyColor(celestialData.celestialAngle);
    commonData.fogColor         = SkyColorHelper::CalculateFogColor(celestialData.celestialAngle, celestialData.compensatedCelestialAngle);
    commonData.rainStrength     = 0.0f;
    commonData.wetness          = 0.0f;
    commonData.thunderStrength  = 0.0f;
    commonData.screenBrightness = 1.0f;
    commonData.nightVision      = 0.0f;
    commonData.blindness        = 0.0f;
    commonData.darknessFactor   = 0.0f;
    commonData.renderStage      = ToRenderStage(WorldRenderingPhase::NONE);

    WriteSkyColorToRT();

    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, {{RTType::ColorTex, 0}, {RTType::DepthTex, 0}});

    {
        // [REFACTOR] Use UpdateMatrixUniforms() instead of deprecated GetMatricesUniforms()
        MatricesUniforms matUniform;
        g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matUniform);
        matUniform.gbufferView.SetTranslation3D(Vec3(0.0f, 0.0f, 0.0f));
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matUniform);
    }

    RenderSkyDome();
    RenderVoidDome();
    RenderSunsetStrip();

    // Draw sky textured (sun/moon)
    g_theRendererSubsystem->SetBlendMode(BlendMode::Additive);
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, {
                                           {RTType::ColorTex, 0}, {RTType::DepthTex, 0}
                                       });

    RenderSun();
    RenderMoon();

    // Restore normal camera matrices after celestial rendering
    // [REFACTOR] Use UpdateMatrixUniforms() instead of deprecated GetMatricesUniforms()
    {
        MatricesUniforms restoredUniforms;
        g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(restoredUniforms);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(restoredUniforms);
    }

    commonData.renderStage = ToRenderStage(WorldRenderingPhase::NONE);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    EndPass();
}

void SkyRenderPass::BeginPass()
{
    // Sky rendered at maximum depth (1.0) - always behind all geometry
    g_theRendererSubsystem->SetDepthMode(DepthMode::Disabled);
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
}

void SkyRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendMode(BlendMode::Opaque);
}

void SkyRenderPass::WriteSkyColorToRT()
{
    // Reference: Minecraft FogRenderer.java:45-150 setupColor()
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float sunAngle       = g_theGame->m_timeProvider->GetSunAngle();

    Vec3 fogColor = SkyColorHelper::CalculateFogColor(celestialAngle, sunAngle);

    Rgba8 fogColorRgba8(
        static_cast<unsigned char>(fogColor.x * 255.0f),
        static_cast<unsigned char>(fogColor.y * 255.0f),
        static_cast<unsigned char>(fogColor.z * 255.0f),
        255
    );

    g_theRendererSubsystem->ClearRenderTarget(RTType::ColorTex, 0, fogColorRgba8);
}

void SkyRenderPass::RenderSunsetStrip()
{
    // Reference: Minecraft LevelRenderer.java:1520-1550, DimensionSpecialEffects.java:44-60
    // Minecraft uses timeOfDay (celestialAngle) for all strip calculations
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();

    // Get sunrise color (returns null equivalent if not in sunrise/sunset window)
    Vec4 sunriseColor = SkyColorHelper::CalculateSunriseColor(celestialAngle);
    if (sunriseColor.w <= 0.0f)
    {
        return; // No strip to render (alpha = 0 means outside sunrise/sunset window)
    }

    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SUNSET);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    // Reference: Iris iris_ColorModulator (VanillaTransformer.java:76-79)
    celestialData.colorModulator = sunriseColor;
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    m_sunsetStripVertices = SkyGeometryHelper::GenerateSunriseStrip(sunriseColor, celestialAngle);

    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->DrawVertexArray(m_sunsetStripVertices);
}

void SkyRenderPass::RenderSkyDome()
{
    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SKY);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    // Generate sky dome with per-vertex fog blending (zenith=skyColor, horizon=fogColor)
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    m_skyDomeVertices    = SkyGeometryHelper::GenerateSkyDiscWithFog(16.0f, celestialAngle);

    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->DrawVertexArray(m_skyDomeVertices);
}

void SkyRenderPass::RenderVoidDome()
{
    // Reference: Minecraft LevelRenderer.java:1599-1607
    if (ShouldRenderVoidDome())
    {
        commonData.renderStage = ToRenderStage(WorldRenderingPhase::SKY_VOID);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);
        g_theRendererSubsystem->DrawVertexArray(m_voidDomeVertices);
    }
}

void SkyRenderPass::RenderSun()
{
    // Reference: Minecraft LevelRenderer.java:1548-1558
    // Reference: Iris CelestialUniforms.java:119-133
    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    float           skyAngle     = g_theGame->m_timeProvider->GetSunAngle();
    float           sunSize      = m_sunSize;
    constexpr float SUN_DISTANCE = 100.0f;

    // Model matrix: Translate -> Orient -> Scale
    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(SUN_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(-90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(sunSize));

    // Celestial view: camera rotation + time rotation, no translation
    // [REFACTOR] Use UpdateMatrixUniforms() instead of deprecated GetMatricesUniforms()
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    Mat44 celestialView = matricesUniforms.gbufferView;
    celestialView.AppendYRotation(-360 * skyAngle);
    celestialView.SetTranslation3D(Vec3::ZERO);
    matricesUniforms.gbufferView        = celestialView;
    matricesUniforms.gbufferViewInverse = celestialView.GetInverse();

    perObjectData.modelMatrix        = modelMatrix;
    perObjectData.modelMatrixInverse = modelMatrix.GetInverse();

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    // Reset colorModulator to white (RenderSunsetStrip may have changed it)
    celestialData.colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    g_theRendererSubsystem->SetCustomImage(0, m_sunTexture.get());
    g_theRendererSubsystem->DrawVertexArray(m_sunQuadVertices);
}

void SkyRenderPass::RenderMoon()
{
    // Reference: Minecraft LevelRenderer.java:1559-1580
    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    float           skyAngle      = g_theGame->m_timeProvider->GetSunAngle();
    float           moonSize      = m_moonSize;
    constexpr float MOON_DISTANCE = -100.0f;

    // Model matrix: Translate -> Orient -> Scale
    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(MOON_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(moonSize));

    // Celestial view: camera rotation + time rotation, no translation
    // [REFACTOR] Use UpdateMatrixUniforms() instead of deprecated GetMatricesUniforms()
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    Mat44 celestialView = matricesUniforms.gbufferView;
    celestialView.AppendYRotation(-360 * skyAngle);
    celestialView.SetTranslation3D(Vec3::ZERO);
    matricesUniforms.gbufferView        = celestialView;
    matricesUniforms.gbufferViewInverse = celestialView.GetInverse();

    perObjectData.modelMatrix        = modelMatrix;
    perObjectData.modelMatrixInverse = modelMatrix.GetInverse();

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    // Reference: Minecraft LevelRenderer.java:1562 - moon phase calculation
    int moonPhase = g_theGame->m_timeProvider->GetDayCount() % 8;

    g_theRendererSubsystem->SetCustomImage(0, m_moonPhasesAtlas->GetSprite(moonPhase).GetTexture().get());
    m_moonQuadVertices = SkyGeometryHelper::GenerateCelestialQuad(m_moonPhasesAtlas->GetSprite(moonPhase).GetUVBounds());
    g_theRendererSubsystem->DrawVertexArray(m_moonQuadVertices);
}

bool SkyRenderPass::ShouldRenderSunsetStrip(float sunAngle) const
{
    // Reference: Minecraft DimensionSpecialEffects.java:44-60 getSunriseColor()
    // Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
    //
    // Minecraft uses: cos(timeOfDay * 2π) in [-0.4, 0.4] to decide rendering
    // We use sunAngle which relates to timeOfDay (skyAngle) as:
    //   sunAngle = timeOfDay + 0.25  (when timeOfDay < 0.75)
    //   sunAngle = timeOfDay - 0.75  (when timeOfDay >= 0.75)
    //
    // Convert sunAngle back to timeOfDay for the cos check:
    float timeOfDay;
    if (sunAngle >= 0.25f)
    {
        timeOfDay = sunAngle - 0.25f; // sunAngle in [0.25, 1.0) -> timeOfDay in [0.0, 0.75)
    }
    else
    {
        timeOfDay = sunAngle + 0.75f; // sunAngle in [0.0, 0.25) -> timeOfDay in [0.75, 1.0)
    }

    // Minecraft's condition: cos(timeOfDay * 2π) in [-0.4, 0.4]
    constexpr float TWO_PI    = 6.28318530718f;
    constexpr float THRESHOLD = 0.4f;

    float cosValue = cosf(timeOfDay * TWO_PI);
    return (cosValue >= -THRESHOLD) && (cosValue <= THRESHOLD);
}

bool SkyRenderPass::ShouldRenderVoidDome() const
{
    // Reference: Minecraft LevelRenderer.java:1599-1607
    // Render void dome when camera Z < horizon height (63.0 = sea level)
    constexpr float HORIZON_HEIGHT = 63.0f;
    Vec3            cameraPos      = g_theGame->m_player->GetCamera()->GetPosition();
    return cameraPos.z < HORIZON_HEIGHT;
}
