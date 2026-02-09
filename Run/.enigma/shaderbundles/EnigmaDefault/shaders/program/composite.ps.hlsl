/// [REFACTOR] Composite pass with RT formats and enhanced lighting
#include "../@engine/core/core.hlsl"
#include "../@engine/lib/fog.hlsl"
#include "../@engine/lib/math.hlsl"

// [NEW] RT format definitions - include ONLY here, engine parses globally
#include "../include/rt_formats.hlsl"

// [NEW] Enhanced lighting system
#include "../lib/lighting.hlsl"

/// Vanilla light intensity (block light vs sky light)
float CalculateLightIntensity(float blockLight, float skyLight, float skyBrightnessValue)
{
    float effectiveSkyLight = skyLight * skyBrightnessValue;
    return max(blockLight, effectiveSkyLight);
}

float NDCDepthToViewDepth(float ndcDepth, float near, float far)
{
    return (near * far) / (far - ndcDepth * (far - near));
}

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0 */

    // [STEP 1] Sample depth textures
    // [IMPORTANT] Use sampler1 (Point filtering) for depth textures!
    // Depth values must NOT be interpolated - use Point sampling only.
    // sampler0 = Linear (for color textures)
    // sampler1 = Point (for depth textures)
    //
    // [FIX] Correct depth texture semantics based on actual pipeline:
    // - depthtex0: Main depth buffer, written by ALL passes (opaque + translucent)
    // - depthtex1: Snapshot copied BEFORE translucent pass (opaque only)
    //
    // Pipeline flow:
    //   TerrainSolid   -> writes depthtex0
    //   TerrainCutout  -> writes depthtex0, then Copy(0,1) in EndPass
    //   TerrainTranslucent -> writes depthtex0 (now includes water)
    //   Composite      -> reads both for depth comparison
    float depthOpaque = depthtex1.Sample(sampler1, input.TexCoord).r; // Opaque only (snapshot before translucent)
    float depthAll    = depthtex0.Sample(sampler1, input.TexCoord).r; // All objects (main depth buffer)

    /*
    float linearDepthOpaque = NDCDepthToViewDepth(depthOpaque, 0.01, 1);
    float linearDepthAll = NDCDepthToViewDepth(depthAll, 0.01, 1);
    output.color0 = float4(linearDepthOpaque, linearDepthAll, 0.0, 1.0);
    return output;
    */

    // [STEP 2] Sample albedo color and AO (use Linear sampler for color)
    // [AO] colortex0.rgb = albedo, colortex0.a = AO (from separateAo mode)
    // For SOLID/CUTOUT blocks: AO stored in alpha channel
    // For TRANSLUCENT blocks: AO already premultiplied to RGB, alpha = blend alpha
    float4 albedoAO = colortex0.Sample(sampler0, input.TexCoord);
    float3 albedo   = albedoAO.rgb;
    float  ao       = albedoAO.a; // AO value (1.0 = no occlusion, 0.0 = full occlusion)

    // [STEP 3] Pure sky detection - both depths are far plane
    // When depthAll >= 1.0: no geometry at all (pure sky)
    // [FIX] When underwater, sky should be completely obscured by water fog
    if (depthAll >= 1.0)
    {
        if (isEyeInWater == EYE_IN_WATER)
        {
            // [NEW] Underwater sky: completely obscured by fog color
            // This handles unloaded chunks and sky visible through water
            output.color0 = float4(fogColor, 1.0);
            return output;
        }
        else if (isEyeInWater == EYE_IN_LAVA)
        {
            // [NEW] In lava: sky obscured by lava fog color
            output.color0 = float4(LAVA_FOG_COLOR, 1.0);
            return output;
        }
        else if (isEyeInWater == EYE_IN_POWDER_SNOW)
        {
            // [NEW] In powder snow: sky obscured by snow fog color
            output.color0 = float4(POWDER_SNOW_FOG_COLOR, 1.0);
            return output;
        }

        // Normal sky - no lighting applied, direct output
        output.color0 = float4(albedo, 1.0);
        return output;
    }


    // [STEP 5] Sample lightmap for terrain (use Linear sampler for color data)
    float blockLight = colortex1.Sample(sampler0, input.TexCoord).r;
    float skyLight   = colortex1.Sample(sampler0, input.TexCoord).g;

    // [STEP 6] Cloud detection - skip lighting if no lightmap data
    // Clouds don't write to colortex1, so lightmap values are 0
    if (blockLight == 0.0 && skyLight == 0.0)
    {
        // Cloud or unlit pixel - no lighting applied
        output.color0 = float4(albedo, 1.0);
        return output;
    }

    // [STEP 7] Apply lighting to terrain with AO
    // Vanilla light intensity + shadow sampling
    float lightIntensity = CalculateLightIntensity(blockLight, skyLight, skyBrightness);
    float finalLight     = max(lightIntensity, 0.03);

    // [NEW] Reconstruct world position for shadow sampling
    float3 viewPos  = ReconstructViewPosition(input.TexCoord, depthAll, gbufferProjectionInverse, gbufferRendererInverse);
    float3 worldPos = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;

    // [NEW] Sample shadow map and apply shadow color tinting
    float3 shadowUV     = WorldToShadowUV(worldPos, shadowView, shadowProjection);
    float  shadowFactor = SampleShadowMap(shadowUV, worldPos, shadowtex1, sampler1);

    // Combine: vanilla lighting * shadow * AO
    float3 shadowedLight = lerp(SHADOW_COLOR, float3(1.0, 1.0, 1.0), shadowFactor);
    output.color0        = float4(albedo * finalLight * shadowedLight * ao, 1.0);

    // [STEP 8] Underwater/Fluid Fog Effects
    float viewDistance = length(viewPos);

    if (isEyeInWater == EYE_IN_WATER)
    {
        output.color0.rgb = ApplyUnderwaterFog(
            output.color0.rgb, viewDistance, fogColor, fogStart, fogEnd
        );
    }
    else if (isEyeInWater == EYE_IN_LAVA)
    {
        output.color0.rgb = ApplyLavaFog(output.color0.rgb, viewDistance);
    }
    else if (isEyeInWater == EYE_IN_POWDER_SNOW)
    {
        output.color0.rgb = ApplyPowderSnowFog(output.color0.rgb, viewDistance);
    }

    return output;
}
