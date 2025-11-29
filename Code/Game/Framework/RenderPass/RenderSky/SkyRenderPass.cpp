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

    // [Component 2] Load sun.png as SpriteAtlas (1x1 grid - single sprite)
    m_sunAtlas = std::make_shared<SpriteAtlas>("Sun");
    m_sunAtlas->BuildFromGrid(".enigma/assets/engine/textures/environment/sun.png", IntVec2(1, 1));

    // [Component 2] Load moon_phases.png as SpriteAtlas (4x2 grid)
    m_moonPhasesAtlas = std::make_shared<SpriteAtlas>("MoonPhases");
    m_moonPhasesAtlas->BuildFromGrid(".enigma/assets/engine/textures/environment/moon_phases.png", IntVec2(4, 2));

    // [Component 5.1] Generate and cache sky disc VertexBuffer
    m_skyDiscVertices = SkyGeometryHelper::GenerateSkyDisc(16.0f);
    //size_t vertexDataSize = m_skyDiscVertices.size() * sizeof(Vertex);
    //m_skyDiscVB.reset(g_theRendererSubsystem->CreateVertexBuffer(vertexDataSize, sizeof(Vertex)));

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
    CelestialConstantBuffer celestialData;
    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();
    celestialData.sunPosition               = g_theGame->m_timeOfDayManager->CalculateSunPosition(); // [FIX P1] Moved to TimeOfDayManager
    celestialData.moonPosition              = g_theGame->m_timeOfDayManager->CalculateMoonPosition(); // [FIX P1] Moved to TimeOfDayManager
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [Component 2] Draw Sky Basic (Void Gradient) ====================
    std::vector<uint32_t> rtOutputs     = {0}; // colortex0
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, rtOutputs, depthTexIndex);

    // [Component 5.1] Draw sky disc (16 segments)
    size_t vertexCount = 10; // [FIX P0] TRIANGLE_FAN: 1 center + 9 perimeter vertices
    UNUSED(vertexCount);
    g_theRendererSubsystem->DrawVertexArray(m_skyDiscVertices); // [FIX P0] Match GenerateSkyDisc() output

    // ==================== [Component 2] Draw Sky Textured (Sun/Moon) ====================
    g_theRendererSubsystem->UseProgram(m_skyTexturedShader, rtOutputs, depthTexIndex);

    // [Component 2] Draw Sun Billboard
    {
        // Calculate sun UV from atlas (CPU-side calculation)
        AABB2 sunUV = m_sunAtlas->GetSprite(0).GetUVBounds();

        // Generate sun billboard vertices with calculated UV
        std::vector<Vertex> sunVertices = SkyGeometryHelper::GenerateCelestialBillboard(
            0.0f, // celestialType = 0 (Sun)
            sunUV
        );

        // Bind sun texture to customImage0
        g_theRendererSubsystem->SetCustomImage(0, m_sunAtlas->GetSprite(0).GetTexture().get());

        // Draw sun billboard (2 triangles = 6 vertices)
        g_theRendererSubsystem->DrawVertexArray(sunVertices);
    }

    // [Component 2] Draw Moon Billboard
    {
        // Calculate moon phase (CPU-side calculation)
        float celestialAngle = g_theGame->m_timeOfDayManager->GetCelestialAngle();
        int   moonPhase      = static_cast<int>(celestialAngle * 8.0f) % 8;

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
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(PerObjectUniforms());
    // ==================== [Component 2] Set Depth State to ALWAYS (depth value 1.0) ====================
    // WHY: Sky is rendered at maximum depth (1.0) to ensure it's always behind all geometry
    // Depth test ALWAYS ensures all sky pixels pass depth test regardless of depth buffer content
    g_theRendererSubsystem->SetDepthMode(DepthMode::Always);
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
