#pragma once
#include "Engine/Math/Vec3.hpp"

// =============================================================================
// CommonConstantBuffer - Iris CommonUniforms Compatible Constant Buffer
// =============================================================================
//
// [Purpose]
//   Provides common rendering data to shaders, strictly matching Iris CommonUniforms.java
//   Reference: Iris CommonUniforms.java generalCommonUniforms() (lines 130-171)
//
// [Shader Side Declaration]
//   Corresponds to HLSL: cbuffer CommonUniforms : register(b8, space1)
//   File location: F:/p4/Personal/SD/Engine/.enigma/assets/engine/shaders/include/common_uniforms.hlsl
//
// [Iris CommonUniforms.java Variable Mapping]
//   Line 136: hideGUI          -> hideGUI
//   Line 137: isRightHanded    -> isRightHanded
//   Line 138: isEyeInWater     -> isEyeInWater (0=air, 1=water, 2=lava, 3=powder_snow)
//   Line 139: blindness        -> blindness
//   Line 140: darknessFactor   -> darknessFactor
//   Line 141: darknessLightFactor -> darknessLightFactor
//   Line 142: nightVision      -> nightVision
//   Line 143: is_sneaking      -> is_sneaking
//   Line 144: is_sprinting     -> is_sprinting
//   Line 145: is_hurt          -> is_hurt
//   Line 146: is_invisible     -> is_invisible
//   Line 147: is_burning       -> is_burning
//   Line 148: is_on_ground     -> is_on_ground
//   Line 151: screenBrightness -> screenBrightness
//   Line 158: playerMood       -> playerMood
//   Line 159: constantMood     -> constantMood
//   Line 160: eyeBrightness    -> eyeBrightnessX, eyeBrightnessY
//   Line 161: eyeBrightnessSmooth -> eyeBrightnessSmoothX, eyeBrightnessSmoothY
//   Line 165: rainStrength     -> rainStrength
//   Line 166: wetness          -> wetness
//   Line 167: skyColor         -> skyColor
//   Line 108: renderStage      -> renderStage (from addDynamicUniforms)
//
// [Alignment Requirements]
//   - HLSL cbuffer requires 16-byte alignment for each row
//   - Vec3 = 12 bytes, needs padding to 16 bytes
//   - int/float fields grouped in 16-byte blocks (4 fields per row)
//   - Total: 160 bytes = 10 rows * 16 bytes
//
// =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324) // Structure padded due to alignas - expected behavior

struct CommonConstantBuffer
{
    // ==================== Row 0: Sky Color (16 bytes) ====================
    // Iris CommonUniforms.java:167 - skyColor
    // CPU-calculated sky color based on time, weather, dimension
    alignas(16) Vec3 skyColor;
    float            _pad0;

    // ==================== Row 1: Player State Flags (16 bytes) ====================
    // Iris CommonUniforms.java:136-137, 138
    alignas(16) int hideGUI; // Line 136: GUI hidden (F1 toggle)
    int             isRightHanded; // Line 137: Main hand is RIGHT
    int             isEyeInWater; // Line 138: 0=air, 1=water, 2=lava, 3=powder_snow
    int             _pad1;

    // ==================== Row 2: Player Visual Effects (16 bytes) ====================
    // Iris CommonUniforms.java:139-142
    alignas(16) float blindness; // Line 139: Blindness effect (0.0-1.0)
    float             darknessFactor; // Line 140: Darkness effect (0.0-1.0)
    float             darknessLightFactor; // Line 141: Darkness light factor (0.0-1.0)
    float             nightVision; // Line 142: Night vision effect (0.0-1.0)

    // ==================== Row 3: Player Action Flags (16 bytes) ====================
    // Iris CommonUniforms.java:143-146
    alignas(16) int is_sneaking; // Line 143: Player is crouching
    int             is_sprinting; // Line 144: Player is sprinting
    int             is_hurt; // Line 145: Player hurtTime > 0
    int             is_invisible; // Line 146: Player is invisible

    // ==================== Row 4: Player Action Flags 2 (16 bytes) ====================
    // Iris CommonUniforms.java:147-148, 151
    alignas(16) int is_burning; // Line 147: Player is on fire
    int             is_on_ground; // Line 148: Player is on ground
    float           screenBrightness; // Line 151: Gamma setting (0.0+)
    float           _pad2;

    // ==================== Row 5: Player Mood (16 bytes) ====================
    // Iris CommonUniforms.java:158-159
    alignas(16) float playerMood; // Line 158: Player mood (0.0-1.0)
    float             constantMood; // Line 159: Constant mood baseline (0.0-1.0)
    float             _pad3;
    float             _pad4;

    // ==================== Row 6: Eye Brightness (16 bytes) ====================
    // Iris CommonUniforms.java:160-161
    // eyeBrightness: x = block light * 16, y = sky light * 16 (range 0-240)
    alignas(16) int eyeBrightnessX; // Line 160: Block light
    int             eyeBrightnessY; // Line 160: Sky light
    int             eyeBrightnessSmoothX; // Line 161: Smoothed block light
    int             eyeBrightnessSmoothY; // Line 161: Smoothed sky light

    // ==================== Row 7: Weather (16 bytes) ====================
    // Iris CommonUniforms.java:165-166
    alignas(16) float rainStrength; // Line 165: Rain intensity (0.0-1.0)
    float             wetness; // Line 166: Wetness factor (smoothed rainStrength)
    float             _pad5;
    float             _pad6;

    // ==================== Row 8: Render Stage (16 bytes) ====================
    // Iris CommonUniforms.java:108 (addDynamicUniforms)
    // Current rendering phase for shader program differentiation
    // IMPORTANT: Do NOT use array here - HLSL cbuffer arrays pad each element to 16 bytes!
    alignas(16) int renderStage; // WorldRenderingPhase ordinal
    int             _pad7_0;
    int             _pad7_1;
    int             _pad7_2;

    // ==================== Row 9: Time Counters (16 bytes) ====================
    // Iris CommonUniforms.java:118 (frameCounter), :119 (frameTime)
    // Iris SystemTimeUniforms.java:63 (frameTimeCounter)
    // Used by cloud wind animation and other time-based shader effects
    alignas(16) int frameCounter; // Frame count since application start
    float           frameTime; // Delta time of last frame in seconds
    float           frameTimeCounter; // Accumulated frame time, wraps at 3600.0
    float           _pad9;

    // ==================== Default Constructor ====================
    CommonConstantBuffer()
        : skyColor(0.47f, 0.65f, 1.0f) // Default blue sky
          , _pad0(0.0f)
          // Row 1: Player State Flags
          , hideGUI(0)
          , isRightHanded(1) // Default right-handed
          , isEyeInWater(0) // Default in air
          , _pad1(0)
          // Row 2: Visual Effects
          , blindness(0.0f)
          , darknessFactor(0.0f)
          , darknessLightFactor(0.0f)
          , nightVision(0.0f)
          // Row 3: Action Flags
          , is_sneaking(0)
          , is_sprinting(0)
          , is_hurt(0)
          , is_invisible(0)
          // Row 4: Action Flags 2
          , is_burning(0)
          , is_on_ground(1) // Default on ground
          , screenBrightness(1.0f)
          , _pad2(0.0f)
          // Row 5: Mood
          , playerMood(0.5f)
          , constantMood(0.5f)
          , _pad3(0.0f)
          , _pad4(0.0f)
          // Row 6: Eye Brightness
          , eyeBrightnessX(0)
          , eyeBrightnessY(240) // Default full sky light
          , eyeBrightnessSmoothX(0)
          , eyeBrightnessSmoothY(240)
          // Row 7: Weather
          , rainStrength(0.0f)
          , wetness(0.0f)
          , _pad5(0.0f)
          , _pad6(0.0f)
          // Row 8: Render Stage
          , renderStage(0)
          , _pad7_0(0)
          , _pad7_1(0)
          , _pad7_2(0)
          // Row 9: Time Counters
          , frameCounter(0)
          , frameTime(0.0f)
          , frameTimeCounter(0.0f)
          , _pad9(0.0f)
    {
    }
};

#pragma warning(pop)

// Compile-time validation: 10 rows * 16 bytes = 160 bytes
static_assert(sizeof(CommonConstantBuffer) == 160,
              "CommonConstantBuffer size mismatch - expected 160 bytes (10 rows * 16 bytes)");

// =============================================================================
// [HLSL Mapping Reference]
// =============================================================================
//
// cbuffer CommonUniforms : register(b8, space1)
// {
//     // Row 0: Sky Color
//     float3 skyColor;
//
//     // Row 1: Player State Flags
//     int hideGUI;
//     int isRightHanded;
//     int isEyeInWater;
//
//     // Row 2: Visual Effects
//     float blindness;
//     float darknessFactor;
//     float darknessLightFactor;
//     float nightVision;
//
//     // Row 3: Action Flags
//     int is_sneaking;
//     int is_sprinting;
//     int is_hurt;
//     int is_invisible;
//
//     // Row 4: Action Flags 2
//     int is_burning;
//     int is_on_ground;
//     float screenBrightness;
//
//     // Row 5: Mood
//     float playerMood;
//     float constantMood;
//
//     // Row 6: Eye Brightness
//     int eyeBrightnessX;
//     int eyeBrightnessY;
//     int eyeBrightnessSmoothX;
//     int eyeBrightnessSmoothY;
//
//     // Row 7: Weather
//     float rainStrength;
//     float wetness;
//
//     // Row 8: Render Stage
//     int renderStage;
//
//     // Row 9: Time Counters
//     int   frameCounter;
//     float frameTime;
//     float frameTimeCounter;
// };
//
// =============================================================================
