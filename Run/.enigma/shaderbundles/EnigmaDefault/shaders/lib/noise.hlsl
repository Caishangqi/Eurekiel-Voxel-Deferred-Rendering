/**
 * @file noise.hlsl
 * @brief Minimal noise utilities for cloud self-shadow dithering (Reimagined style)
 *
 * Standalone — no external dependencies.
 * Reimagined clouds use texture-driven shape, NOT Perlin/FBM noise.
 * This file only provides the small helpers needed for dithering.
 *
 * Reference: ComplementaryReimagined reimaginedClouds.glsl:89
 */

#ifndef LIB_NOISE_HLSL
#define LIB_NOISE_HLSL

// 2D hash function for noise generation
// Returns a pseudo-random value in [0, 1) for a given 2D coordinate.
float Hash2D(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3        += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}

// Interleaved Gradient Noise for cloud self-shadow dithering
// Produces a TAA-friendly screen-space dither pattern.
// Reference: reimaginedClouds.glsl:89, Jorge Jimenez (2014)
//
// @param screenPos  Pixel coordinates (e.g. SV_Position.xy)
// @return           Pseudo-random value in [0, 1)
float InterleavedGradientNoiseForClouds(float2 screenPos)
{
    return frac(52.9829189 * frac(dot(screenPos, float2(0.06711056, 0.00583715))));
}

#endif // LIB_NOISE_HLSL
