#include "SceneUnitTest_StencilXRay.hpp"

#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Graphic/Target/RTTypes.hpp"
#include "Engine/Graphic/Sprite/Sprite.hpp"
#include "Engine/Graphic/Sprite/SpriteAtlas.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/Geometry.hpp"

SceneUnitTest_StencilXRay::SceneUnitTest_StencilXRay()
{
    /// Prepare scene
    m_cubeA             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeA      = AABB3(Vec3(0, 0, 0), Vec3(2, 2, 2));
    m_cubeA->m_position = Vec3(0, 0, 0);
    m_cubeA->SetVertices(geoCubeA.GetVertices())->SetIndices(geoCubeA.GetIndices());

    m_cubeB             = std::make_unique<Geometry>(g_theGame);
    AABB3 geoCubeB      = AABB3(Vec3(0, 0, 0), Vec3(3, 3, 3));
    m_cubeB->m_position = Vec3(10, 0, 0);
    m_cubeB->m_color    = Rgba8::DEBUG_BLUE;
    m_cubeB->m_scale    = Vec3(1.5f, 1.5f, 1.5f); // Make CubeB larger to create occlusion for X-Ray test
    m_cubeB->SetVertices(geoCubeB.GetVertices())->SetIndices(geoCubeB.GetIndices());

    /// Prepare render resources
    enigma::graphic::ShaderCompileOptions shaderCompileOptions;
    shaderCompileOptions.enableDebugInfo = true;
    sp_gBufferBasic                      = g_theRendererSubsystem->CreateShaderProgramFromFiles(
        ".enigma/assets/engine/shaders/program/gbuffers_basic.vs.hlsl",
        ".enigma/assets/engine/shaders/program/gbuffers_basic.ps.hlsl",
        "gbuffers_basic",
        shaderCompileOptions
    );

    /// Ideally the shader code hlsl will include "Common.hlsl"
    // [FIX] 保存CreateTexture2D返回的纹理指针到tex_testUV
    // CreateTexture2D返回裸指针，需要包装为shared_ptr
    tex_testUV = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/TestUV.png",
        TextureUsage::ShaderResource,
        "TestUV"
    );

    tex_testCaizii = g_theRendererSubsystem->CreateTexture2D(
        ".enigma/assets/engine/textures/test/Caizii.png",
        TextureUsage::ShaderResource,
        "TestCaizii"
    );
}

SceneUnitTest_StencilXRay::~SceneUnitTest_StencilXRay()
{
}

void SceneUnitTest_StencilXRay::Update()
{
}

//----------------------------------------------------------------------------------------------------------------------
// X-Ray Outline Rendering Test (4-Pass Stencil Buffer Technique)
//
// This demonstrates advanced stencil testing for creating an X-Ray outline effect.
// The technique highlights parts of an object (CubeA) that are occluded by another object (CubeB).
//
// Stencil Buffer Encoding (3-value system):
//   0 = Background pixels (cleared state)
//   1 = Occluded pixels (CubeA projection area that fails depth test)
//   2 = Visible pixels (CubeA pixels that pass depth test)
//
// Technical Flow:
//   Pass 1: Mark entire CubeA projection area (stencil = 1, depth test DISABLED)
//           WHY: Depth disabled ensures we mark ALL pixels in projection, including occluded ones
//   Pass 2: Overwrite visible CubeA pixels (stencil = 2, depth test ENABLED)
//           WHY: Depth enabled ensures only front-facing pixels get marked as visible
//           RESULT: stencil=1 (occluded), stencil=2 (visible), stencil=0 (background)
//   Pass 3: Draw occluder CubeB (normal rendering, stencil disabled)
//           WHY: Update depth buffer with occluder geometry for correct depth ordering
//   Pass 4: X-Ray outline (stencil != 2, depth test DISABLED)
//           WHY: TestNotEqual(2) filters out visible areas, leaving only occluded pixels
//           WHY: Depth disabled allows rendering through CubeB (X-Ray effect)
//           RESULT: Orange outline ONLY visible where CubeA is behind CubeB
//
// Critical Fix History:
//   [FIX 1] Depth format: D32_FLOAT -> D24_UNORM_S8_UINT (enables 8-bit stencil buffer)
//   [FIX 2] Stencil ref: SetStencilRefValue(1) -> SetStencilRefValue(2) in Pass 4
//
// Educational Value:
//   - Demonstrates multi-pass rendering with stencil buffer state machine
//   - Shows depth test control for selective pixel marking
//   - Illustrates stencil test for occlusion detection (TestNotEqual filtering)
//   - Common technique in games for object selection/highlighting through walls
//   - Key insight: Stencil encodes "visibility state", depth controls "what to mark/draw"
//----------------------------------------------------------------------------------------------------------------------
void SceneUnitTest_StencilXRay::Render()
{
    // ========================================
    // [PASS 1] Mark entire CubeA projection area (stencil=1)
    // ========================================
    // Purpose: Mark ALL pixels where CubeA projects (visible + occluded)
    // WHY Depth DISABLED: Ensures we mark entire projection regardless of depth buffer state
    //                     This creates the "full projection mask" that includes occluded pixels
    // Stencil Operation: MarkAlways() writes ref value (1) to all rasterized pixels
    // Result: Entire CubeA projection area has stencil=1 (both visible and occluded regions)
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::MarkAlways());
    g_theRendererSubsystem->SetStencilRefValue(1);
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled()); // [CRITICAL] Disable depth to mark entire projection
    // [REFACTOR] Pair-based RT binding
    g_theRendererSubsystem->UseProgram(sp_gBufferBasic, {
                                           {RTType::ColorTex, 4}, {RTType::ColorTex, 5},
                                           {RTType::ColorTex, 6}, {RTType::ColorTex, 7},
                                           {RTType::DepthTex, 0}
                                       });
    m_cubeA->Render();

    // ========================================
    // [PASS 2] Mark visible CubeA pixels (stencil=2)
    // ========================================
    // Purpose: Overwrite ONLY visible pixels with stencil=2, leaving occluded pixels as stencil=1
    // WHY Depth ENABLED: Only pixels that pass depth test (front-facing, unoccluded) get marked
    //                    Occluded pixels fail depth test and retain their stencil=1 value from Pass 1
    // Stencil Operation: MarkAlways() writes ref value (2) to pixels that pass depth test
    // Result: stencil=2 (visible CubeA), stencil=1 (occluded CubeA), stencil=0 (background)
    // This creates the "visibility mask" that separates visible from occluded regions

    g_theRendererSubsystem->SetCustomImage(0, tex_testCaizii.get());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::MarkAlways());
    g_theRendererSubsystem->SetStencilRefValue(2);
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled()); // [CRITICAL] Enable depth to mark only visible pixels
    m_cubeA->Render();

    // ========================================
    // [PASS 3] Draw occluder (CubeB)
    // ========================================
    // Purpose: Render CubeB normally to update depth buffer and color buffer
    // WHY Stencil DISABLED: We don't need to modify stencil buffer for the occluder
    //                       Stencil buffer already contains the visibility information we need
    // WHY Depth ENABLED: Standard depth testing ensures correct depth ordering in final image
    // Result: CubeB written to depth buffer and color buffer, stencil buffer unchanged

    g_theRendererSubsystem->SetCustomImage(0, tex_testUV.get());
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::Disabled());
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Enabled());
    m_cubeB->Render();

    // ========================================
    // [PASS 4] X-Ray outline (only where stencil != 2)
    // ========================================
    // Purpose: Draw outline ONLY in occluded regions (where stencil=1 or stencil=0)
    // WHY TestNotEqual(2): Filters out visible pixels (stencil=2), allowing only occluded (stencil=1)
    //                      and background (stencil=0) pixels to pass the stencil test
    //                      Since we render scaled CubeA, only stencil=1 regions will actually draw
    // WHY Depth DISABLED: Allows rendering through CubeB (X-Ray effect), ignoring depth buffer
    //                     This is the key to "seeing through walls" - depth test would reject pixels
    // Stencil Logic: if (stencil != 2) pass; // Passes for stencil=0 and stencil=1
    // Result: Orange outline visible ONLY where CubeA is occluded by CubeB (stencil=1 regions)
    //
    // [CRITICAL FIX] SetStencilRefValue(2) is correct:
    //   - TestNotEqual(2) means "pass if stencil != 2"
    //   - This filters out visible pixels (stencil=2), leaving occluded pixels (stencil=1)
    //   - Common mistake: Using ref=1 would filter occluded pixels instead of visible ones
    g_theRendererSubsystem->SetCustomImage(0, nullptr);
    g_theRendererSubsystem->SetStencilTest(StencilTestDetail::TestNotEqual());
    g_theRendererSubsystem->SetStencilRefValue(2); // [CORRECT] Test for stencil != 2 (filter visible, keep occluded)
    g_theRendererSubsystem->SetDepthConfig(DepthConfig::Disabled()); // [CRITICAL] Disable depth for X-Ray effect

    // [OUTLINE EFFECT] Save original transform and color
    Vec3  originalScale    = m_cubeA->m_scale;
    Vec3  originalPosition = m_cubeA->m_position;
    Rgba8 originalColor    = m_cubeA->m_color;

    // Make cube slightly larger and use bright outline color (orange for X-Ray effect)
    m_cubeA->m_scale = originalScale * 1.2f; // 20% larger for outline visibility
    m_cubeA->m_color = Rgba8::ORANGE;

    m_cubeA->Render();

    // Restore original transform and color
    m_cubeA->m_scale    = originalScale;
    m_cubeA->m_color    = originalColor;
    m_cubeA->m_position = originalPosition;

    // ========================================
    // [PRESENT] Display result
    // ========================================
    g_theRendererSubsystem->PresentRenderTarget(4, RTType::ColorTex);
}
