/// Composite pass - deferred lighting with shadow mapping and fog
#include "../@engine/core/core.hlsl"
#include "../@engine/lib/fog.hlsl"
#include "../@engine/lib/math.hlsl"
#include "../include/rt_formats.hlsl"
#include "../lib/lighting.hlsl"

float CalculateLightIntensity(float blockLight, float skyLight, float skyBrightnessValue)
{
    return max(blockLight, skyLight * skyBrightnessValue);
}

struct PSOutput
{
    float4 color0 : SV_Target0;
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0 */

    // Sample depth: sampler1 = Point (no interpolation for depth)
    // depthtex0 = all geometry, depthtex1 = opaque-only snapshot
    float depthOpaque = depthtex1.Sample(sampler1, input.TexCoord).r;
    float depthAll    = depthtex0.Sample(sampler1, input.TexCoord).r;

    // Albedo + AO (separateAo: RGB=albedo, A=AO for solid/cutout)
    float4 albedoAO = colortex0.Sample(sampler0, input.TexCoord);
    float3 albedo   = albedoAO.rgb;
    float  ao       = albedoAO.a;

    // Sky pixels - no geometry, early out
    if (depthAll >= 1.0)
    {
        if (isEyeInWater == EYE_IN_WATER)
        {
            output.color0 = float4(fogColor, 1.0);
            return output;
        }
        else if (isEyeInWater == EYE_IN_LAVA)
        {
            output.color0 = float4(LAVA_FOG_COLOR, 1.0);
            return output;
        }
        else if (isEyeInWater == EYE_IN_POWDER_SNOW)
        {
            output.color0 = float4(POWDER_SNOW_FOG_COLOR, 1.0);
            return output;
        }

        output.color0 = float4(albedo, 1.0);
        return output;
    }

    // Lightmap
    float blockLight = colortex1.Sample(sampler0, input.TexCoord).r;
    float skyLight   = colortex1.Sample(sampler0, input.TexCoord).g;

    // Cloud/unlit pixels - no lightmap data, skip lighting
    if (blockLight == 0.0 && skyLight == 0.0)
    {
        output.color0 = float4(albedo, 1.0);
        return output;
    }

    // --- Deferred Lighting ---

    float lightIntensity = CalculateLightIntensity(blockLight, skyLight, skyBrightness);
    float finalLight     = max(lightIntensity, 0.03);

    // Reconstruct world position from depth
    float3 viewPos  = ReconstructViewPosition(input.TexCoord, depthAll, gbufferProjectionInverse, gbufferRendererInverse);
    float3 worldPos = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;

    // GBuffer normal (colortex2, SNORM [-1,1], Point sampling)
    float3 worldNormal = normalize(colortex2.Sample(sampler1, input.TexCoord).xyz);

    // Light direction: shadowLightPosition (view space) → world space
    float3 lightDirWorld = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));

    // --- Shadow ---
    // Back-faces always in shadow; front-faces use three-layer bias system
    float NdotL = dot(worldNormal, lightDirWorld);
    float shadowFactor;
    if (NdotL <= 0.0)
    {
        shadowFactor = 0.0;
    }
    else
    {
        shadowFactor = SampleShadowWithBias(worldPos, worldNormal, lightDirWorld,
                                            shadowView, shadowProjection,
                                            shadowtex1, sampler1);
        shadowFactor *= smoothstep(0.0, 0.05, NdotL);
    }

    // Combine: lighting × shadow × AO
    float3 shadowedLight = lerp(SHADOW_COLOR, float3(1.0, 1.0, 1.0), shadowFactor);
    output.color0        = float4(albedo * finalLight * shadowedLight * ao, 1.0);

    // --- Fluid Fog ---
    float viewDistance = length(viewPos);

    if (isEyeInWater == EYE_IN_WATER)
        output.color0.rgb = ApplyUnderwaterFog(output.color0.rgb, viewDistance, fogColor, fogStart, fogEnd);
    else if (isEyeInWater == EYE_IN_LAVA)
        output.color0.rgb = ApplyLavaFog(output.color0.rgb, viewDistance);
    else if (isEyeInWater == EYE_IN_POWDER_SNOW)
        output.color0.rgb = ApplyPowderSnowFog(output.color0.rgb, viewDistance);

    return output;
}
