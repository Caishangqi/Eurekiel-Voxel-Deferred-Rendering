#pragma once

// =============================================================================
// WorldInfoUniforms - Iris WorldInfoUniforms Compatible Constant Buffer
// =============================================================================
//
// [Purpose]
//   Provides world/dimension information to shaders, matching Iris WorldInfoUniforms.java
//   Reference: Iris WorldInfoUniforms.java
//
// [Shader Side Declaration]
//   Corresponds to HLSL: cbuffer WorldInfoUniforms : register(b3, space1)
//   File: Engine/.enigma/assets/engine/shaders/include/worldinfo_uniforms.hlsl
//
// [Memory Layout] 32 bytes (2 rows x 16 bytes)
//   Row 0 [0-15]:  cloudHeight, heightLimit, bedrockLevel, logicalHeightLimit
//   Row 1 [16-31]: ambientLight, hasCeiling, hasSkylight, _pad1
//
// =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324) // Structure padded due to alignas - expected behavior

struct WorldInfoUniforms
{
    // ==================== Row 0: Cloud & Height (16 bytes) ====================
    // @iris cloudHeight (WorldInfoUniforms.java) - runtime cloud height offset
    alignas(16) float cloudHeight; // Default 192.0 (overworld cloud altitude)
    int               heightLimit; // Dimension total height (default 384)
    int               bedrockLevel; // Dimension min Y (default 0)
    int               logicalHeightLimit; // Logical height limit (default 384)

    // ==================== Row 1: Dimension Properties (16 bytes) ====================
    // @iris ambientLight, hasCeiling, hasSkylight
    alignas(16) float ambientLight; // Ambient light level (default 0.0, nether=0.1)
    int               hasCeiling; // Has ceiling (nether=1, default 0)
    int               hasSkylight; // Has skylight (default 1)
    int               _pad1;

    // ==================== Default Constructor (Overworld defaults) ====================
    WorldInfoUniforms()
        : cloudHeight(192.0f)
          , heightLimit(384)
          , bedrockLevel(0)
          , logicalHeightLimit(384)
          , ambientLight(0.0f)
          , hasCeiling(0)
          , hasSkylight(1)
          , _pad1(0)
    {
    }
};

#pragma warning(pop)

// Compile-time validation: 2 rows * 16 bytes = 32 bytes
static_assert(sizeof(WorldInfoUniforms) == 32,
              "WorldInfoUniforms size mismatch - expected 32 bytes (2 rows * 16 bytes)");
