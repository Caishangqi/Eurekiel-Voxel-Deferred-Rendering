#include "SkyRenderPass.hpp"
#include "SkyGeometryHelper.hpp"
#include "SkyColorHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CommonConstantBuffer.hpp"
#include "Game/Framework/RenderPass/WorldRenderingPhase.hpp" // [NEW] For renderStage
#include "Game/Gameplay/Game.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Engine/Graphic/Camera/EnigmaCamera.hpp"
#include <cmath> // [NEW] For std::abs in ShouldRenderSunsetStrip

#include "Engine/Core/VertexUtils.hpp"

SkyRenderPass::SkyRenderPass()
{
    // [Component 2] Load Shaders (gbuffers_skybasic, gbuffers_skytextured)
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;

    m_skyBasicShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_skybasic.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_skybasic.ps.hlsl",
        "gbuffers_skybasic",
        shaderCompileOptions
    );

    m_skyTexturedShader = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_skytextured.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_skytextured.ps.hlsl",
        "gbuffers_skytextured",
        shaderCompileOptions
    );


    // [Component 2] Load sun.png 
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

    // [Component 2] Load moon_phases.png as SpriteAtlas (4x2 grid)
    m_moonPhasesAtlas = std::make_shared<SpriteAtlas>("MoonPhases");
    m_moonPhasesAtlas->BuildFromGrid(".enigma/assets/engine/textures/environment/moon_phases.png", IntVec2(4, 2));

    // ==========================================================================
    // [Component 5.1] Generate Sky Sphere (Two Hemispheres)
    // ==========================================================================
    // Minecraft sky structure:
    //     ● (0,0,+16) Sky Zenith
    //    /|\
    //   / | \  Upper Hemisphere (Sky Dome)
    //  /  |  \
    // ●───●───● Z=0 Horizon (Player Level)
    //  \  |  /
    //   \ | /  Lower Hemisphere (Void Dome)
    //    \|/
    //     ● (0,0,-16) Void Nadir
    // ==========================================================================
    m_skyDomeVertices  = SkyGeometryHelper::GenerateSkyDisc(16.0f); // Upper hemisphere (sky)
    m_voidDomeVertices = SkyGeometryHelper::GenerateSkyDisc(-16.0f); // Lower hemisphere (void)

    // [Component 2] Register CelestialConstantBuffer to slot 15 (PerFrame update)
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(15, enigma::graphic::UpdateFrequency::PerObject // [FIX P1] Celestial data updates per frame, not per object
    );

    // [NEW] Register CommonConstantBuffer to slot 16 (PerFrame update)
    // Contains skyColor, fogColor, weather parameters - mirrors Iris CommonUniforms
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CommonConstantBuffer>(8, enigma::graphic::UpdateFrequency::PerObject, 10000
    );
}

SkyRenderPass::~SkyRenderPass()
{
}

void SkyRenderPass::Execute()
{
    BeginPass();

    // ==================== [Component 2] Upload CelestialConstantBuffer ====================
    // [FIX] Get gbufferModelView (World->Camera transform) from player camera
    // This is needed to calculate VIEW SPACE sun/moon positions (following Iris CelestialUniforms.java)
    Mat44 gbufferModelView = g_theGame->m_player->GetCamera()->GetWorldToCameraTransform();

    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();

    // [FIX] Minecraft daylight factor formula (ClientLevel.java:681)
    // Formula: h = cos(celestialAngle * 2π) * 2 + 0.5, clamped to [0, 1]
    // - celestialAngle=0.0 (noon, tick=6000) -> h = 2.5 -> 1.0 (brightest)
    // - celestialAngle=0.5 (midnight, tick=18000) -> h = -1.5 -> 0.0 (darkest)
    float h                     = cosf(celestialData.celestialAngle * 6.28318530718f) * 2.0f + 0.5f;
    celestialData.skyBrightness = (h < 0.0f) ? 0.0f : ((h > 1.0f) ? 1.0f : h);

    celestialData.sunPosition  = g_theGame->m_timeOfDayManager->CalculateSunPosition(gbufferModelView); // [FIX] VIEW SPACE position
    celestialData.moonPosition = g_theGame->m_timeOfDayManager->CalculateMoonPosition(gbufferModelView); // [FIX] VIEW SPACE position
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [NEW] Upload CommonConstantBuffer ====================
    // [IMPORTANT] skyColor is CPU-calculated, mirrors Iris CommonUniforms.getSkyColor()
    // This replaces hardcoded sky colors in gbuffers_skybasic.ps.hlsl
    commonData.skyColor         = SkyColorHelper::CalculateSkyColor(celestialData.celestialAngle);
    commonData.fogColor         = SkyColorHelper::CalculateFogColor(celestialData.celestialAngle, celestialData.compensatedCelestialAngle);
    commonData.rainStrength     = 0.0f; // TODO: Get from weather system
    commonData.wetness          = 0.0f; // TODO: Smoothed rain strength
    commonData.thunderStrength  = 0.0f; // TODO: Get from weather system
    commonData.screenBrightness = 1.0f; // TODO: Get from player settings
    commonData.nightVision      = 0.0f; // TODO: Get from player effects
    commonData.blindness        = 0.0f; // TODO: Get from player effects
    commonData.darknessFactor   = 0.0f; // TODO: Get from biome effects
    commonData.renderStage      = ToRenderStage(WorldRenderingPhase::NONE); // [NEW] Initialize to NONE

    // ==================== [NEW] Step 1: Write sky background color to colortex0 ====================
    WriteSkyColorToRT();

    // ==================== [Component 2] Draw Sky Basic (Sky Sphere) ====================
    std::vector<uint32_t> rtOutputs     = {0}; // colortex0
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, rtOutputs, depthTexIndex);

    RenderSkyDome();
    RenderVoidDome();

    // ==================== [NEW] Step 4: Render sunset strip (conditional) ====================
    // [NEW] Set renderStage to SUNSET for sunset strip rendering
    RenderSunsetStrip();

    // Reset the Camera Matrices
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(g_theGame->m_player->GetCamera()->GetMatricesUniforms());

    // ==================== [Component 2] Draw Sky Textured (Sun/Moon) ====================
    // [FIX] Enable Alpha Blending for sun/moon soft edges
    g_theRendererSubsystem->SetBlendMode(BlendMode::Additive);
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, rtOutputs, depthTexIndex);

    // [Component 2] Draw Sun Billboard
    {
        // [NEW] Set renderStage to SUN
        commonData.renderStage    = ToRenderStage(WorldRenderingPhase::SUN);
        commonData.darknessFactor = 12.f;
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

        // Generate sun billboard vertices with calculated UV
        std::vector<Vertex> sunVertices = SkyGeometryHelper::GenerateCelestialBillboard(
            0.0f, // celestialType = 0 (Sun)
            AABB2::ZERO_TO_ONE
        );

        // Bind sun texture to customImage0
        g_theRendererSubsystem->SetCustomImage(0, m_sunTexture.get());

        // Draw sun billboard (2 triangles = 6 vertices)
        g_theRendererSubsystem->DrawVertexArray(sunVertices);
    }

    // [Component 2] Draw Moon Billboard
    {
        // [NEW] Set renderStage to MOON
        commonData.renderStage = ToRenderStage(WorldRenderingPhase::MOON);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

        // [FIX] Calculate moon phase using dayCount (changes daily, not per-tick)
        // Minecraft: moonPhase = dayCount % 8 (0=full moon, cycles through 8 phases)
        int moonPhase = g_theGame->m_timeOfDayManager->GetDayCount() % 8;

        // Calculate moon UV from atlas (CPU-side calculation)
        AABB2 moonUV = m_moonPhasesAtlas->GetSprite(moonPhase).GetUVBounds();

        // Generate moon billboard vertices with calculated UV
        std::vector<Vertex> moonVertices = SkyGeometryHelper::GenerateCelestialBillboard(
            1.0f, // celestialType = 1 (Moon)
            moonUV
        );

        // Bind moon texture to customImage0
        g_theRendererSubsystem->SetCustomImage(0, m_moonPhasesAtlas->GetSprite(moonPhase).GetTexture().get());

        // Draw moon billboard (2 triangles = 6 vertices)
        g_theRendererSubsystem->DrawVertexArray(moonVertices);
    }

    // [NEW] Reset renderStage to NONE at end of pass
    commonData.renderStage = ToRenderStage(WorldRenderingPhase::NONE);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    EndPass();
}

void SkyRenderPass::BeginPass()
{
    // ==================== [Component 2] Set Depth State to ALWAYS (depth value 1.0) ====================
    // WHY: Sky is rendered at maximum depth (1.0) to ensure it's always behind all geometry
    // Depth test ALWAYS ensures all sky pixels pass depth test regardless of depth buffer content
    g_theRendererSubsystem->SetDepthMode(DepthMode::Always);
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
}

void SkyRenderPass::EndPass()
{
    // ==================== [Component 2] Cleanup Render States ====================
    // Reference: SceneUnitTest_StencilXRay.cpp:94 EndPass cleanup logic

    // [CLEANUP] Reset depth state to default (ReadWrite + LessEqual)
    g_theRendererSubsystem->SetDepthMode(DepthMode::Enabled);

    // [CLEANUP] Reset stencil state to disabled
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());

    // [CLEANUP] Reset blend mode to opaque
    g_theRendererSubsystem->SetBlendMode(BlendMode::Opaque);
}

void SkyRenderPass::WriteSkyColorToRT()
{
    // [FIX] Use fog color for Clear RT (matching Minecraft FogRenderer algorithm)
    // Reference: Minecraft FogRenderer.java:45-150 setupColor()
    float celestialAngle = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    float sunAngle       = g_theGame->m_timeOfDayManager->GetSunAngle();

    // [FIX] Calculate fog color using SkyColorHelper (not sky color!)
    // Minecraft uses fogColor for clearing, not skyColor
    Vec3 fogColor = SkyColorHelper::CalculateFogColor(celestialAngle, sunAngle);

    // [NEW] Convert Vec3 to Rgba8 for ClearRenderTarget API
    Rgba8 fogColorRgba8(
        static_cast<unsigned char>(fogColor.x * 255.0f),
        static_cast<unsigned char>(fogColor.y * 255.0f),
        static_cast<unsigned char>(fogColor.z * 255.0f),
        255
    );

    // [FIX] Clear colortex0 with fog color (RT index 0)
    g_theRendererSubsystem->ClearRenderTarget(0, fogColorRgba8);
}

void SkyRenderPass::RenderSunsetStrip()
{
    // [FIX] Use sunAngle instead of celestialAngle for sunrise/sunset calculations
    float sunAngle = g_theGame->m_timeOfDayManager->GetSunAngle();

    // [NEW] Skip if not during sunrise/sunset
    if (!ShouldRenderSunsetStrip(sunAngle))
    {
        return;
    }

    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SUNSET);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    // [NEW] Calculate strip color (includes alpha for intensity)
    Vec4 sunriseColor = SkyColorHelper::CalculateSunriseColor(sunAngle);

    // ==========================================================================
    // [NEW] Set colorModulator for GPU-side coloring (Minecraft/Iris style)
    // ==========================================================================
    // Reference: Minecraft DynamicTransforms.ColorModulator
    //            Iris iris_ColorModulator (VanillaTransformer.java:76-79)
    //
    // Flow:
    //   1. Vertex color = pure white (255,255,255), alpha gradient
    //   2. colorModulator = sunriseColor (set here)
    //   3. Shader: finalColor = white * colorModulator = actual sunset color
    // ==========================================================================
    celestialData.colorModulator = sunriseColor; // [NEW] Set colorModulator = sunriseColor
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // [NEW] Generate strip geometry with CPU transform (matches Minecraft LevelRenderer.java)
    // Pass sunAngle for flip calculation: XP(90) * ZP(flip) * ZP(90)
    m_sunsetStripVertices = SkyGeometryHelper::GenerateSunriseStrip(sunriseColor, sunAngle);

    // [NEW] Enable additive blending for glow effect
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);

    // [NEW] Draw strip using same shader as sky dome
    g_theRendererSubsystem->DrawVertexArray(m_sunsetStripVertices);
}

void SkyRenderPass::RenderSkyDome()
{
    commonData.renderStage = ToRenderStage(WorldRenderingPhase::SKY);
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);

    MatricesUniforms matUniform = g_theGame->m_player->GetCamera()->GetMatricesUniforms();
    matUniform.gbufferModelView.SetTranslation3D(Vec3(0.0f, 0.0f, 0.0f));

    // ==========================================================================
    // [NEW] Generate sky dome with CPU-side fog blending (Iris-style)
    // ==========================================================================
    // Instead of using pre-generated vertices with uniform color, we regenerate
    // the sky dome each frame with per-vertex colors blended based on elevation:
    // - Zenith (center): pure skyColor
    // - Horizon (edge): pure fogColor
    // GPU interpolation creates smooth gradient matching Minecraft appearance
    // ==========================================================================
    float celestialAngle = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    m_skyDomeVertices    = SkyGeometryHelper::GenerateSkyDiscWithFog(16.0f, celestialAngle);

    // [FIX] Draw both hemispheres to form complete sky sphere
    // Upper hemisphere: Sky dome (player looks up to see sky)
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(matUniform);
    g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
    g_theRendererSubsystem->DrawVertexArray(m_skyDomeVertices);
}

void SkyRenderPass::RenderVoidDome()
{
    // [NEW] Lower hemisphere: Void dome (only render when camera Z < horizon height)
    // Reference: Minecraft LevelRenderer.java:1599-1607
    // Condition: player.getEyePosition().y < level.getHorizonHeight()
    // In our coordinate system: camera Z < 63.0 (sea level)
    if (ShouldRenderVoidDome())
    {
        // [NEW] Set renderStage to VOID for void dome rendering
        commonData.renderStage = ToRenderStage(WorldRenderingPhase::SKY_VOID);
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);
        g_theRendererSubsystem->DrawVertexArray(m_voidDomeVertices);
    }
}

bool SkyRenderPass::ShouldRenderSunsetStrip(float sunAngle) const
{
    // [FIX] Minecraft time reference using sunAngle (not celestialAngle):
    // Reference: Iris CelestialUniforms.java:24-32 getSunAngle()
    // 
    // sunAngle = celestialAngle + 0.25 (with wrap-around)
    // This ensures:
    // - tick 18000 (celestialAngle=0.75) -> sunAngle=0.0 -> sunrise
    // - tick 0 (celestialAngle=0.0) -> sunAngle=0.25 -> after midnight (no strip)
    // - tick 6000 (celestialAngle=0.25) -> sunAngle=0.5 -> sunset
    // - tick 12000 (celestialAngle=0.5) -> sunAngle=0.75 -> noon (no strip)
    constexpr float SUNRISE_CENTER = 0.0f; // [FIX] Sunrise at sunAngle 0.0
    constexpr float SUNSET_CENTER  = 0.5f; // [FIX] Sunset at sunAngle 0.5
    constexpr float THRESHOLD      = 0.1f;

    // [FIX] Handle sunrise wrap-around (sunAngle near 0.0 or 1.0)
    float distToSunrise = std::abs(sunAngle - SUNRISE_CENTER);
    if (distToSunrise > 0.5f)
    {
        distToSunrise = 1.0f - distToSunrise; // Wrap around (e.g., 0.95 -> 0.05)
    }

    float distToSunset = std::abs(sunAngle - SUNSET_CENTER);

    return (distToSunrise < THRESHOLD) || (distToSunset < THRESHOLD);
}

bool SkyRenderPass::ShouldRenderVoidDome() const
{
    // [NEW] Minecraft void dome rendering condition
    // Reference: LevelRenderer.java:1599-1607
    // 
    // Minecraft code:
    // double d = this.minecraft.player.getEyePosition(f).y - this.level.getLevelData().getHorizonHeight(this.level);
    // if (d < 0.0D) { /* render darkBuffer (void dome) */ }
    //
    // getHorizonHeight() returns:
    // - 63.0 for normal worlds (sea level)
    // - minBuildHeight for flat worlds
    //
    // Coordinate mapping:
    // - Minecraft Y axis = Our Z axis
    // - Camera position in our system uses Z for vertical

    constexpr float HORIZON_HEIGHT = 63.0f; // Minecraft sea level

    // Get camera position (eye position)
    Vec3 cameraPos = g_theGame->m_player->GetCamera()->GetPosition();

    // Check if camera Z (Minecraft Y) is below horizon height
    // Only render void dome when player is underground (Z < 63)
    return cameraPos.z < HORIZON_HEIGHT;
}
