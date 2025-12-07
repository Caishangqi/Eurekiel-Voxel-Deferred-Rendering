#pragma once
#include "Engine/Math/Vec3.hpp"

// =============================================================================
// CommonConstantBuffer - Common Uniforms Constant Buffer
// =============================================================================
//
// [Purpose]
//   Provides common rendering data to shaders (sky color, fog color, render stage, etc.)
//   Mirrors Iris CommonUniforms for Minecraft compatibility
//
// [Shader Side Declaration]
//   Corresponds to HLSL: cbuffer CommonUniforms : register(b16, space1)
//   File location: F:/p4/Personal/SD/Engine/.enigma/assets/engine/shaders/include/common_uniforms.hlsl
//
// [Registration and Usage]
//   // Register in SkyRenderPass constructor
//   g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CommonConstantBuffer>(
//       16,  // Slot >= 15 (user-defined area)
//       UpdateFrequency::PerFrame  // Update once per frame
//   );
//
//   // Upload data in SkyRenderPass::Execute()
//   CommonConstantBuffer commonData;
//   commonData.skyColor = CalculateSkyColor();
//   commonData.fogColor = CalculateFogColor();
//   commonData.renderStage = ToRenderStage(WorldRenderingPhase::SKY);
//   g_theRendererSubsystem->GetUniformManager()->UploadBuffer(commonData);
//
// [Alignment Requirements]
//   - Each field must be 16-byte aligned (HLSL cbuffer specification)
//   - Vec3 requires padding (Vec3 = 12 bytes, needs padding to 16 bytes)
//   - int/float fields grouped in 16-byte blocks
//
// [Reference]
//   - Iris CommonUniforms.java - skyColor, fogColor, rainStrength, wetness, renderStage
//   - Iris WorldRenderingPhase.java - renderStage enum values
//   - CelestialConstantBuffer.hpp - Alignment example
//
// =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324) // Structure padded due to alignas - expected behavior

struct CommonConstantBuffer
{
    // ==================== Sky Color (16 bytes) ====================
    // CPU-calculated sky color based on time, weather, dimension
    // Source: Minecraft ClientLevel.getSkyColor(position, tickDelta)
    alignas(16) Vec3 skyColor;
    float            _padding1; // Pad to 16 bytes

    // ==================== Fog Color (16 bytes) ====================
    // CPU-calculated fog color, used for horizon blending
    // Source: Minecraft FogRenderer.setupColor()
    alignas(16) Vec3 fogColor;
    float            _padding2; // Pad to 16 bytes

    // ==================== Weather Parameters (16 bytes) ====================
    alignas(16) float rainStrength; // Rain intensity (0.0-1.0)
    float             wetness; // Wetness factor (smoothed rainStrength)
    float             thunderStrength; // Thunder intensity (0.0-1.0)
    float             _padding3; // Pad to 16 bytes

    // ==================== Player State (16 bytes) ====================
    alignas(16) float screenBrightness; // Gamma setting (0.0-1.0)
    float             nightVision; // Night vision effect (0.0-1.0)
    float             blindness; // Blindness effect (0.0-1.0)
    float             darknessFactor; // Darkness effect (0.0-1.0)

    // ==================== Render Stage (16 bytes) ====================
    // [NEW] Current rendering phase for shader program differentiation
    // Source: Iris CommonUniforms.java:108 - uniform1i("renderStage", ...)
    // Values: WorldRenderingPhase enum ordinal (0=NONE, 1=SKY, 2=SUNSET, etc.)
    alignas(16) int renderStage; // WorldRenderingPhase ordinal
    int             _padding4[3]; // Pad to 16 bytes
};

#pragma warning(pop)

// =============================================================================
// [IMPORTANT] C++ struct to HLSL cbuffer field mapping (80 bytes total)
// =============================================================================
//
// C++ Side (CommonConstantBuffer):
//   - Vec3 skyColor                  -> HLSL: float3 skyColor
//   - float _padding1                -> HLSL: (auto-padded, no declaration needed)
//   - Vec3 fogColor                  -> HLSL: float3 fogColor
//   - float _padding2                -> HLSL: (auto-padded, no declaration needed)
//   - float rainStrength             -> HLSL: float rainStrength
//   - float wetness                  -> HLSL: float wetness
//   - float thunderStrength          -> HLSL: float thunderStrength
//   - float _padding3                -> HLSL: (auto-padded, no declaration needed)
//   - float screenBrightness         -> HLSL: float screenBrightness
//   - float nightVision              -> HLSL: float nightVision
//   - float blindness                -> HLSL: float blindness
//   - float darknessFactor           -> HLSL: float darknessFactor
//   - int renderStage                -> HLSL: int renderStage [NEW]
//   - int _padding4[3]               -> HLSL: (auto-padded, no declaration needed)
//
// HLSL Side (common_uniforms.hlsl):
//   cbuffer CommonUniforms : register(b16, space1)
//   {
//       float3 skyColor;
//       float3 fogColor;
//       float  rainStrength;
//       float  wetness;
//       float  thunderStrength;
//       float  screenBrightness;
//       float  nightVision;
//       float  blindness;
//       float  darknessFactor;
//       int    renderStage;  // [NEW] Use RENDER_STAGE_* macros
//   };
//
// HLSL Macros Available:
//   RENDER_STAGE_SKY (1), RENDER_STAGE_SUNSET (2), RENDER_STAGE_SUN (4), etc.
//   See common_uniforms.hlsl for full list
//
// =============================================================================
