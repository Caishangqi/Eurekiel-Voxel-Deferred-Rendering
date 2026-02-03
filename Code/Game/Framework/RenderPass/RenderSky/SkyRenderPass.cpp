#include "SkyRenderPass.hpp"
#include "SkyGeometryHelper.hpp"
#include "SkyColorHelper.hpp"
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
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Resource/VertexLayout/Layouts/Vertex_PCUTBNLayout.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Graphic/Bundle/ShaderBundle.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/Integration/ShaderBundleSubsystem.hpp"
#include "Engine/Graphic/Core/RenderState/RasterizeState.hpp"

SkyRenderPass::SkyRenderPass()
{
    // Shaders loaded via OnShaderBundleLoaded event callback
    m_skyBasicShader    = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_skybasic");
    m_skyTexturedShader = g_theShaderBundleSubsystem->GetCurrentShaderBundle()->GetProgram("gbuffers_skytextured");

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

    // Generate star field geometry (1500 stars)
    // Reference: Minecraft LevelRenderer.java:571-620 createStars()
    m_starVertices = StarGeometryHelper::GenerateStarVertices(m_starSeed);

    // Register constant buffers
    // [FIX] Use BufferSpace::Custom for Descriptor Table path (space=1)
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(
        9, // slot
        enigma::graphic::UpdateFrequency::PerObject,
        enigma::graphic::BufferSpace::Custom,
        10000 // maxDraws
    );
}

SkyRenderPass::~SkyRenderPass()
{
}

void SkyRenderPass::OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle)
{
    if (!newBundle)
    {
        return;
    }
    m_skyBasicShader    = newBundle->GetProgram("gbuffers_skybasic");
    m_skyTexturedShader = newBundle->GetProgram("gbuffers_skytextured");
}

void SkyRenderPass::OnShaderBundleUnloaded()
{
    m_skyBasicShader    = nullptr;
    m_skyTexturedShader = nullptr;
}

void SkyRenderPass::Execute()
{
    // Skip rendering if shaders not loaded
    if (!m_skyBasicShader || !m_skyTexturedShader)
    {
        return;
    }

    BeginPass();

    // ==========================================================================
    // [REFACTOR] Pre-compute celestial matrices once per frame
    // ==========================================================================
    // This replaces duplicate calculations in RenderSun(), RenderMoon(), RenderStars()
    // All celestial bodies share the same view matrix (camera rotation + time rotation)
    // ==========================================================================
    UpdateCelestialMatrices();

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
    COMMON_UNIFORM.skyColor         = SkyColorHelper::CalculateSkyColor(celestialData.celestialAngle);
    COMMON_UNIFORM.rainStrength     = 0.0f;
    COMMON_UNIFORM.wetness          = 0.0f;
    COMMON_UNIFORM.screenBrightness = 1.0f;
    COMMON_UNIFORM.nightVision      = 0.0f;
    COMMON_UNIFORM.blindness        = 0.0f;
    COMMON_UNIFORM.darknessFactor   = 0.0f;
    COMMON_UNIFORM.renderStage      = ToRenderStage(WorldRenderingPhase::NONE);

    WriteSkyColorToRT();

    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, {{RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}});

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

    // [NEW] Render stars before sun/moon (stars are behind celestial bodies)
    // Reference: Minecraft LevelRenderer.java:1556-1560
    RenderStars();

    // Draw sky textured (sun/moon)
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Additive());
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, {
                                           {RenderTargetType::ColorTex, 0}, {RenderTargetType::DepthTex, 0}
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

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::NONE);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    EndPass();
}

void SkyRenderPass::BeginPass()
{
    // Sky rendered at maximum depth (1.0) - always behind all geometry
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled());
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->SetVertexLayout(Vertex_PCUTBNLayout::Get());
}

void SkyRenderPass::EndPass()
{
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Opaque());
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

    g_theRendererSubsystem->ClearRenderTarget(RenderTargetType::ColorTex, 0, fogColorRgba8);
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

    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUNSET);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    // Reference: Iris iris_ColorModulator (VanillaTransformer.java:76-79)
    celestialData.colorModulator = sunriseColor;
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    m_sunsetStripVertices = SkyGeometryHelper::GenerateSunriseStrip(sunriseColor, celestialAngle);

    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->DrawVertexArray(m_sunsetStripVertices);
}

void SkyRenderPass::RenderSkyDome()
{
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SKY);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    // Generate sky dome with per-vertex fog blending (zenith=skyColor, horizon=fogColor)
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    m_skyDomeVertices    = SkyGeometryHelper::GenerateSkyDiscWithFog(16.0f, celestialAngle);

    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Alpha());
    g_theRendererSubsystem->DrawVertexArray(m_skyDomeVertices);
}

void SkyRenderPass::RenderVoidDome()
{
    // Reference: Minecraft LevelRenderer.java:1599-1607
    if (ShouldRenderVoidDome())
    {
        COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SKY_VOID);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);
        g_theRendererSubsystem->DrawVertexArray(m_voidDomeVertices);
    }
}

void SkyRenderPass::RenderSun()
{
    // Reference: Minecraft LevelRenderer.java:1548-1558
    // Reference: Iris CelestialUniforms.java:119-133
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    constexpr float SUN_DISTANCE = 100.0f;

    // Model matrix: Translate -> Orient -> Scale
    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(SUN_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(-90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(m_sunSize));

    // [REFACTOR] Use pre-computed celestial view matrix (computed once in Execute())
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

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
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::SUN);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    constexpr float MOON_DISTANCE = -100.0f;

    // Model matrix: Translate -> Orient -> Scale
    Mat44 modelMatrix;
    modelMatrix.Append(Mat44::MakeTranslation3D(Vec3(MOON_DISTANCE, 0.0f, 0.0f)));
    modelMatrix.AppendYRotation(90.f);
    modelMatrix.Append(Mat44::MakeUniformScale3D(m_moonSize));

    // [REFACTOR] Use pre-computed celestial view matrix (computed once in Execute())
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

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

void SkyRenderPass::RenderStars()
{
    // Reference: Minecraft LevelRenderer.java:1556-1560
    //
    // Minecraft code:
    //   float f10 = level.getStarBrightness(partialTick) * f9;
    //   if (f10 > 0.0F) {
    //       FogRenderer.setupNoFog();
    //       this.starBuffer.bind();
    //       this.starBuffer.drawWithShader(poseStack.last().pose(), projectionMatrix, GameRenderer.getPositionColorShader());
    //   }
    //
    // f9 = (1.0 - rainLevel), so stars fade out in rain

    if (!m_enableStarRendering)
    {
        return;
    }

    // Calculate star brightness based on time of day and weather
    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float rainStrength   = COMMON_UNIFORM.rainStrength;
    float starBrightness = StarGeometryHelper::CalculateStarBrightness(celestialAngle, rainStrength);

    // Apply user-configurable brightness multiplier
    starBrightness *= m_starBrightnessMultiplier;

    // Skip rendering if stars are not visible
    if (starBrightness <= 0.0f)
    {
        return;
    }

    // Set render stage for shader
    COMMON_UNIFORM.renderStage = ToRenderStage(WorldRenderingPhase::STARS);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(COMMON_UNIFORM);

    // Set star color with calculated brightness
    // Reference: Minecraft uses white color with alpha = brightness
    celestialData.colorModulator = Vec4(starBrightness, starBrightness, starBrightness, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // [REFACTOR] Use pre-computed celestial view matrix (computed once in Execute())
    MatricesUniforms matricesUniforms;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(matricesUniforms);
    matricesUniforms.gbufferView        = m_celestialView;
    matricesUniforms.gbufferViewInverse = m_celestialViewInverse;

    // Identity model matrix (stars are already positioned in world space)
    perObjectData.modelMatrix        = Mat44::IDENTITY;
    perObjectData.modelMatrixInverse = Mat44::IDENTITY;

    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(perObjectData);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matricesUniforms);

    // Use additive blending for stars (they glow against the sky)
    g_theRendererSubsystem->SetBlendConfig(BlendConfig::Additive());

    // Disable back-face culling for star quads (visible from all angles)
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::NoCull());

    // Draw star vertices
    g_theRendererSubsystem->DrawVertexArray(m_starVertices);

    // Reset rasterization to default (back-face culling)
    g_theRendererSubsystem->SetRasterizationConfig(RasterizationConfig::CullBack());

    // Reset colorModulator to white
    celestialData.colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);
}

bool SkyRenderPass::ShouldRenderStars() const
{
    // Reference: Minecraft LevelRenderer.java:1556-1560
    // Stars are rendered when brightness > 0 (nighttime, no rain)
    if (!m_enableStarRendering)
    {
        return false;
    }

    float celestialAngle = g_theGame->m_timeProvider->GetCelestialAngle();
    float rainStrength   = COMMON_UNIFORM.rainStrength;
    float starBrightness = StarGeometryHelper::CalculateStarBrightness(celestialAngle, rainStrength);

    return starBrightness > 0.0f;
}

void SkyRenderPass::SetStarSeed(unsigned int seed)
{
    if (m_starSeed != seed)
    {
        m_starSeed = seed;
        // Regenerate star vertices with new seed
        m_starVertices = StarGeometryHelper::GenerateStarVertices(m_starSeed);
    }
}

// ==========================================================================
// [REFACTOR] Celestial Matrix Pre-computation
// ==========================================================================
// This method computes the celestial view matrix once per frame.
// Previously, this identical calculation was duplicated in:
//   - RenderSun()
//   - RenderMoon()
//   - RenderStars()
//
// The celestial view matrix combines:
//   1. Camera rotation (from gbufferView)
//   2. Sky rotation based on time (skyAngle)
//   3. Zero translation (celestial bodies are at "infinity")
// ==========================================================================
void SkyRenderPass::UpdateCelestialMatrices()
{
    // Cache sky angle for this frame
    m_cachedSkyAngle = g_theGame->m_timeProvider->GetSunAngle();

    // Get base view matrix from camera
    MatricesUniforms tempMatrices;
    g_theGame->m_player->GetCamera()->UpdateMatrixUniforms(tempMatrices);

    // Build celestial view: camera rotation + time-based sky rotation, no translation
    m_celestialView = tempMatrices.gbufferView;
    m_celestialView.AppendYRotation(-360.0f * m_cachedSkyAngle);
    m_celestialView.SetTranslation3D(Vec3::ZERO);

    // Pre-compute inverse for shader use
    m_celestialViewInverse = m_celestialView.GetInverse();
}
