/**
 * @file gbuffers_water.ps.hlsl
 * @brief Water Pixel Shader - Reimagined style with inline SSR
 *
 * CR-compatible architecture: SSR computed and blended directly in gbuffers_water,
 * not deferred to composite pass. This matches ComplementaryReimagined's approach
 * where water reflection is resolved before writing to colortex0.
 *
 * Pipeline context: runs AFTER deferred lighting, so colortex0 contains the lit scene.
 * SSR samples colortex0 at non-water pixels (different screen positions = no hazard).
 *
 * Output Layout:
 * - colortex0 (SV_TARGET0): Final water color with reflection blended in + Alpha
 * - colortex1 (SV_TARGET1): Lightmap (blockLight, skyLight)
 * - colortex2 (SV_TARGET2): World Normal (SNORM, direct [-1,1])
 * - colortex4 (SV_TARGET3): MaterialMask(R) + Smoothness(G) + Reflectivity(B) + fresnelM(A)
 *
 * Reference: ComplementaryReimagined gbuffers_water.fsh + reflections.glsl
 */

#include "../@engine/core/core.hlsl"
#include "../@engine/lib/math.hlsl"
#include "../include/settings.hlsl"
#include "../lib/common.hlsl"
#include "../lib/noise.hlsl"
#include "../lib/water.hlsl"
#include "../lib/ssr.hlsl"

// [RENDERTARGETS] 0,1,2,4
// SV_TARGET0 -> colortex0, SV_TARGET1 -> colortex1,
// SV_TARGET2 -> colortex2, SV_TARGET3 -> colortex4

// Base sky reflection color (modulated by skyLightFactor)
static const float3 SKY_REFLECTION_COLOR = float3(0.35, 0.55, 0.85);

// ============================================================================
// Pixel Shader Structures
// ============================================================================

struct PSInput_Water
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL;
    float2 LightmapCoord : LIGHTMAP;
    float3 WorldPos : TEXCOORD2;
    uint   entityId : TEXCOORD3;
    float2 midTexCoord : TEXCOORD4;
};

struct PSOutput_Water
{
    float4 Color : SV_TARGET0; // colortex0: Final color (reflection blended) + Alpha
    float4 Lightmap : SV_TARGET1; // colortex1: Lightmap
    float4 Normal : SV_TARGET2; // colortex2: World Normal (SNORM)
    float4 Material : SV_TARGET3; // colortex4: MaterialMask(R) + Smoothness(G) + Reflectivity(B) + fresnelM(A)
};

PSOutput_Water main(PSInput_Water input)
{
    PSOutput_Water output;

    // [STEP 1] Sample block atlas
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(sampler1, input.TexCoord);

    if (texColor.a < 0.01)
    {
        discard;
    }

    // [STEP 2] Material ID
    int mat = (int)input.entityId;

    // [STEP 3] Lightmap (shared)
    output.Lightmap = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    if (mat == 32000)
    {
        // ================================================================
        // Water Material (Reimagined style + inline SSR)
        // ================================================================
        float3 viewDir = normalize(cameraPosition - input.WorldPos);

        WaterResult water = ComputeWaterSurface(
            input.WorldPos, viewDir, normalize(input.Normal),
            texColor, input.Color, rainStrength, frameTimeCounter
        );

        // colortex2: Perturbed world normal
        output.Normal = float4(water.normalM, 1.0);

        // colortex4: Material data (still written for potential composite use)
        output.Material = float4(241.0 / 255.0, water.smoothness, WATER_REFLECTIVITY, water.fresnel);

        // ================================================================
        // Inline SSR + Reflection Blending (CR architecture)
        // colortex0 contains deferred-lit scene; safe to sample at other pixels
        // ================================================================
        float3 waterColor = water.color;

#if WATER_REFLECT_QUALITY >= 1
        // View direction camera->fragment for SSR
        float3 viewDirToFrag = normalize(input.WorldPos - cameraPosition);
        float3 reflectDir    = reflect(viewDirToFrag, water.normalM);

        // Sky light factor from lightmap
        float skyLightFactor = input.LightmapCoord.y;

        // Sky fallback reflection
        float3 skyFallback     = GetReflectionFallback(reflectDir, SKY_REFLECTION_COLOR, skyLightFactor);
        float3 reflectionColor = skyFallback;

#if WATER_REFLECT_QUALITY >= 2
        // SSR ray marching
        float depth  = input.Position.z;
        float dither = InterleavedGradientNoiseForClouds(input.Position.xy);

        SSRResult ssr = ComputeSSR(
            input.WorldPos, water.normalM, viewDirToFrag,
            water.smoothness, dither, depth
        );

        // Combine SSR hit with sky fallback (CR: mix(skyReflection, reflection.rgb, reflection.a))
        reflectionColor = lerp(skyFallback, ssr.color, ssr.alpha);
#endif

        // Blend reflection into water color using fresnelM
        // CR: color.rgb = mix(color.rgb, reflection.rgb, fresnelM)
        waterColor = lerp(waterColor, reflectionColor, water.fresnel);
#endif

        output.Color = float4(waterColor, water.alpha);
    }
    else
    {
        // ================================================================
        // Non-water Translucent (default path)
        // ================================================================
        float4 finalColor = texColor * input.Color;

        output.Color    = finalColor;
        output.Normal   = float4(0, 0, 0, 0);
        output.Material = float4(0, 0, 0, 0);
    }

    return output;
}
