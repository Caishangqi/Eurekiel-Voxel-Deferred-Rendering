#pragma once
#pragma warning(push)
#pragma warning(disable: 4324) // Structure padded due to alignas - expected behavior
#include "Engine/Math/Vec3.hpp"

// =============================================================================
// FogUniforms - Iris FogUniforms.java Compatible
// =============================================================================
//
// [Purpose]
//   Provides fog rendering parameters to shaders
//   Strictly mirrors Iris FogUniforms.java for shader pack compatibility
//
// [Iris Reference]
//   Source: FogUniforms.java (lines 19-53)
//   Path: Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/FogUniforms.java
//
// [Iris Fields]
//   Line 21/24: fogMode     - Fog mode (0=off, 9729=linear, 2048=exp, 2049=exp2)
//   Line 22/36: fogShape    - Fog shape (0=sphere, 1=cylinder, -1=disabled)
//   Line 39:    fogDensity  - Fog density for exp/exp2 modes
//   Line 45:    fogStart    - Linear fog start distance
//   Line 47:    fogEnd      - Linear fog end distance
//   Line 51:    fogColor    - Fog color (RGB)
//
// [Memory Layout] 32 bytes (2 rows x 16 bytes)
//   Row 0 [0-15]:  fogColor (float3), fogDensity
//   Row 1 [16-31]: fogStart, fogEnd, fogMode, fogShape
//
// [Shader Side Declaration]
//   Corresponds to HLSL: cbuffer FogUniforms : register(b10, space1)
//   File location: F:/p4/Personal/SD/Engine/.enigma/assets/engine/shaders/include/fog_uniforms.hlsl
//
// =============================================================================

struct FogUniforms
{
    // ==================== Row 0: Fog Color + Density (16 bytes) ====================
    // @iris fogColor (FogUniforms.java:51-53)
    // RGB fog color, set by RenderSystem.getShaderFogColor()
    // When underwater, Minecraft sets this to blue tint
    alignas(16) Vec3 fogColor;

    // @iris fogDensity (FogUniforms.java:39-43)
    // Fog density for exponential fog modes
    // Range: 0.0+ (typically 0.0-1.0)
    float fogDensity;

    // ==================== Row 1: Fog Parameters (16 bytes) ====================
    // @iris fogStart (FogUniforms.java:45)
    // Linear fog start distance (where fog begins)
    alignas(16) float fogStart;

    // @iris fogEnd (FogUniforms.java:47)
    // Linear fog end distance (where fog is fully opaque)
    float fogEnd;

    // @iris fogMode (FogUniforms.java:21/24-34)
    // Fog equation mode:
    //   0     = GL_FOG_MODE off
    //   9729  = GL_LINEAR
    //   2048  = GL_EXP
    //   2049  = GL_EXP2
    int fogMode;

    // @iris fogShape (FogUniforms.java:22/36)
    // Fog shape:
    //   -1 = disabled
    //   0  = sphere (default)
    //   1  = cylinder
    int fogShape;

    // ==================== Default Constructor ====================
    FogUniforms()
        : fogColor(0.5f, 0.6f, 0.7f) // Default gray-blue fog
          , fogDensity(0.0f)
          , fogStart(0.0f)
          , fogEnd(192.0f) // Default render distance
          , fogMode(9729) // GL_LINEAR
          , fogShape(0) // Sphere
    {
    }
};

#pragma warning(pop)

// =============================================================================
// [IMPORTANT] C++ struct to HLSL cbuffer field mapping (32 bytes total)
// =============================================================================
//
// C++ Side (FogUniforms):
//   - Vec3 fogColor      -> HLSL: float3 fogColor
//   - float fogDensity   -> HLSL: float fogDensity
//   - float fogStart     -> HLSL: float fogStart
//   - float fogEnd       -> HLSL: float fogEnd
//   - int fogMode        -> HLSL: int fogMode
//   - int fogShape       -> HLSL: int fogShape
//
// HLSL Side (fog_uniforms.hlsl):
//   cbuffer FogUniforms : register(b10, space1)
//   {
//       float3 fogColor;
//       float  fogDensity;
//       float  fogStart;
//       float  fogEnd;
//       int    fogMode;
//       int    fogShape;
//   };
//
// =============================================================================
