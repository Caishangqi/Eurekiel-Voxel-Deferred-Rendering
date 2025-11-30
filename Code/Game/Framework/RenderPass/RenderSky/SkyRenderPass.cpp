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
#include "Engine/Math/MathUtils.hpp"
#include "Game/Framework/GameObject/PlayerCharacter.hpp"
#include "Engine/Graphic/Camera/EnigmaCamera.hpp"
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

    // ==================== [CPU-Side Vertex Transform] Pure Rotation View Matrix ====================
    // Following Minecraft/Iris approach: vertices are transformed on CPU with pure rotation matrix
    // 
    // Minecraft data flow (from GameRenderer.java:1156):
    //   Quaternionf quaternionf = camera.rotation().conjugate(new Quaternionf());
    //   Matrix4f matrix4f2 = (new Matrix4f()).rotation(quaternionf);  // Pure rotation, NO translation!
    //
    // Our approach:
    //   1. Get camera orientation (EulerAngles: yaw, pitch, roll)
    //   2. Build rotation-only matrix using GetAsMatrix_IFwd_JLeft_KUp()
    //   3. This matrix has NO translation component (translation = [0,0,0])
    //
    // Engine coordinate system: +X forward, +Y left, +Z up
    // =========================================================================================
    EulerAngles cameraOrientation = g_theGame->m_player->GetCamera()->GetOrientation();
    Mat44       skyViewMatrix     = cameraOrientation.GetAsMatrix_IFwd_JLeft_KUp();
    // GetAsMatrix_IFwd_JLeft_KUp() returns a pure rotation matrix (translation is already [0,0,0,1])
    // No need to explicitly clear translation - it's already zero by construction

    // ==================== [Component 2] Upload CelestialConstantBuffer ====================
    // [FIX] Get gbufferModelView (World->Camera transform) from player camera
    // This is needed to calculate VIEW SPACE sun/moon positions (following Iris CelestialUniforms.java)
    Mat44 gbufferModelView = g_theGame->m_player->GetCamera()->GetWorldToCameraTransform();

    CelestialConstantBuffer celestialData;
    celestialData.celestialAngle            = g_theGame->m_timeOfDayManager->GetCelestialAngle();
    celestialData.compensatedCelestialAngle = g_theGame->m_timeOfDayManager->GetCompensatedCelestialAngle();
    celestialData.cloudTime                 = g_theGame->m_timeOfDayManager->GetCloudTime();

    celestialData.skyBrightness = cosf((celestialData.celestialAngle - 0.25f) * 6.28318530718f) * 0.5f + 0.5f;

    celestialData.sunPosition  = g_theGame->m_timeOfDayManager->CalculateSunPosition(gbufferModelView); // [FIX] VIEW SPACE position
    celestialData.moonPosition = g_theGame->m_timeOfDayManager->CalculateMoonPosition(gbufferModelView); // [FIX] VIEW SPACE position
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);

    // ==================== [Component 2] Draw Sky Basic (Sky Sphere) ====================
    std::vector<uint32_t> rtOutputs     = {0}; // colortex0
    int                   depthTexIndex = 0; // depthtex0
    g_theRendererSubsystem->UseProgram(m_skyBasicShader, rtOutputs, depthTexIndex);

    // [STEP 0] Upload IDENTITY modelMatrix (transformation done on CPU side)
    enigma::graphic::PerObjectUniforms skyDomePerObj;
    skyDomePerObj.modelMatrix        = Mat44::IDENTITY;
    skyDomePerObj.modelMatrixInverse = Mat44::IDENTITY;
    skyDomePerObj.modelColor[0]      = 1.0f;
    skyDomePerObj.modelColor[1]      = 1.0f;
    skyDomePerObj.modelColor[2]      = 1.0f;
    skyDomePerObj.modelColor[3]      = 1.0f;
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer<enigma::graphic::PerObjectUniforms>(skyDomePerObj);

    // [CPU-SIDE VERTEX TRANSFORM] Transform Sky Dome vertices with skyViewMatrix
    // Following Minecraft/Iris approach: vertices transformed on CPU, GPU only does Projection
    // Sky Dome has no celestial rotation, only camera rotation (skyViewMatrix)

    // Upper hemisphere: Sky dome (player looks up to see sky)
    std::vector<Vertex> transformedSkyDome = m_skyDomeVertices;
    TransformVertexArray3D(transformedSkyDome, skyViewMatrix);
    g_theRendererSubsystem->DrawVertexArray(transformedSkyDome);

    // Lower hemisphere: Void dome (player looks down to see void gradient)
    std::vector<Vertex> transformedVoidDome = m_voidDomeVertices;
    TransformVertexArray3D(transformedVoidDome, skyViewMatrix);
    g_theRendererSubsystem->DrawVertexArray(transformedVoidDome);

    // ==================== [NEW] Draw Sunrise/Sunset Strip ====================
    // Only render during sunrise (celestialAngle near 0.0) or sunset (near 0.5)
    Vec4 sunriseColor = CalculateSunriseColor(celestialData.celestialAngle);
    if (sunriseColor.w > 0.0f) // alpha > 0 means we should render
    {
        // [STEP 1] Generate strip geometry in Y-Z plane (facing +X direction)
        std::vector<Vertex> stripVertices = SkyGeometryHelper::GenerateSunriseStrip(sunriseColor);

        // [STEP 2] Build celestial rotation matrix (strip follows sun position)
        // Engine coordinate system: +X forward, +Y left, +Z up
        // Rotation 1: Y-axis rotation - follow sun direction (0-360 degrees)
        // Rotation 2: X-axis 90 degrees - tilt to horizon position
        float rotationAngleY    = celestialData.celestialAngle * 360.0f;
        Mat44 celestialRotation = Mat44::IDENTITY;
        celestialRotation.AppendYRotation(rotationAngleY);
        celestialRotation.AppendXRotation(90.0f);

        // [STEP 3] CPU-SIDE VERTEX TRANSFORM (following Minecraft/Iris approach)
        // Combined transform: celestialRotation * skyViewMatrix
        // Order: First apply celestial rotation (position strip in world), then apply camera rotation
        Mat44 combinedTransform = celestialRotation;
        combinedTransform.Append(skyViewMatrix);

        // Transform all strip vertices on CPU side
        TransformVertexArray3D(stripVertices, combinedTransform);

        // [STEP 4] Upload IDENTITY modelMatrix (transformation already done on CPU)
        enigma::graphic::PerObjectUniforms perObjData;
        perObjData.modelMatrix        = Mat44::IDENTITY;
        perObjData.modelMatrixInverse = Mat44::IDENTITY;
        perObjData.modelColor[0]      = 1.0f;
        perObjData.modelColor[1]      = 1.0f;
        perObjData.modelColor[2]      = 1.0f;
        perObjData.modelColor[3]      = 1.0f;
        g_theRendererSubsystem->GetUniformManager()->UploadBuffer<enigma::graphic::PerObjectUniforms>(perObjData);

        // [STEP 5] Render strip (vertices already transformed, GPU only does Projection)
        g_theRendererSubsystem->SetBlendMode(BlendMode::Alpha);
        g_theRendererSubsystem->DrawVertexArray(stripVertices);
    }

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
    g_theRendererSubsystem->SetDepthMode(DepthMode::LessEqual);
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

//-----------------------------------------------------------------------------------------------
Vec4 SkyRenderPass::CalculateSunriseColor(float celestialAngle) const
{
    // ==========================================================================
    // [Sunrise/Sunset Color Calculation] - FIXED VERSION
    // ==========================================================================
    // Render when celestialAngle is near 0.0 (sunrise) or 0.5 (sunset)
    // 
    // celestialAngle mapping:
    //   0.0 (or 1.0) = Sunrise (tick 0 or 24000)
    //   0.25         = Noon (tick 6000)
    //   0.5          = Sunset (tick 12000)
    //   0.75         = Midnight (tick 18000)
    //
    // [OLD BUG] Previous formula used cos(celestialAngle * 2PI) which:
    //   - At sunrise (0.0): heightFactor = 1.0 (NOT in [-0.4, 0.4], no render)
    //   - At sunset (0.5):  heightFactor = -1.0 (NOT in [-0.4, 0.4], no render)
    //   - At noon (0.25):   heightFactor = 0.0 (IN range, wrong render)
    //
    // [NEW FIX] Calculate distance to nearest horizon event (sunrise/sunset)
    // ==========================================================================

    // Calculate distance to sunrise (celestialAngle = 0.0 or 1.0)
    float distanceToSunrise = (celestialAngle < 0.5f) ? celestialAngle : (1.0f - celestialAngle);

    // Calculate distance to sunset (celestialAngle = 0.5)
    float distanceToSunset = fabsf(celestialAngle - 0.5f);

    // Get distance to nearest horizon event
    float distanceToHorizon = (distanceToSunrise < distanceToSunset) ? distanceToSunrise : distanceToSunset;

    // Render window: within 0.1 of horizon event (about 2400 ticks = 2 minutes game time)
    constexpr float HORIZON_WINDOW = 0.1f;

    if (distanceToHorizon < HORIZON_WINDOW)
    {
        // intensity: 1.0 at horizon event center, 0.0 at window edge
        float intensity = 1.0f - (distanceToHorizon / HORIZON_WINDOW);

        // Apply smoothstep for smoother transition (Hermite interpolation)
        intensity = intensity * intensity * (3.0f - 2.0f * intensity);

        // Alpha with smooth falloff (max 0.8 for subtle effect)
        float alpha = intensity * 0.8f;

        // Orange-to-red gradient color
        float red   = 1.0f; // Full red
        float green = 0.4f + intensity * 0.3f; // 0.4 to 0.7 (orange tint)
        float blue  = 0.1f * intensity; // 0.0 to 0.1 (slight warmth)

        return Vec4(red, green, blue, alpha);
    }

    // Return zero vector when not in sunrise/sunset period
    return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
}
