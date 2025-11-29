#include "SkyRenderPass.hpp"
#include "SkyGeometryHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Game/Framework/RenderPass/ConstantBuffer/CelestialConstantBuffer.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Engine/Graphic/Camera/EnigmaCamera.hpp"

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
    g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(
        15,
        enigma::graphic::UpdateFrequency::PerFrame // [FIX P1] Celestial data updates per frame, not per object
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

    CelestialConstantBuffer celestialData;
    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();
    celestialData.sunPosition               = g_theGame->m_timeOfDayManager->CalculateSunPosition(gbufferModelView); // [FIX] VIEW SPACE position
    celestialData.moonPosition              = g_theGame->m_timeOfDayManager->CalculateMoonPosition(gbufferModelView); // [FIX] VIEW SPACE position
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [Component 2] Draw Sky Basic (Sky Sphere) ====================
    std::vector<uint32_t> rtOutputs     = {0}; // colortex0
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, rtOutputs, depthTexIndex);

    // [FIX] Draw both hemispheres to form complete sky sphere
    // Upper hemisphere: Sky dome (player looks up to see sky)
    g_theRendererSubsystem->DrawVertexArray(m_skyDomeVertices);

    // Lower hemisphere: Void dome (player looks down to see void gradient)
    g_theRendererSubsystem->DrawVertexArray(m_voidDomeVertices);

    // ==================== [Component 2] Draw Sky Textured (Sun/Moon) ====================
    // [FIX] Enable Alpha Blending for sun/moon soft edges
    g_theRendererSubsystem->SetBlendMode(BlendMode::Additive);
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, rtOutputs, depthTexIndex);

    // [Component 2] Draw Sun Billboard
    {
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
