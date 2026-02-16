/// Deferred pass 1 — core deferred shading: lighting + atmosphere + clouds + fluid fog
/// Migrated from composite.ps.hlsl with atmospheric fog, volumetric clouds added.
/// Reference: CR deferred1.glsl, design R4.5
///
/// Pipeline: GBuffer → Lighting → Atmospheric Fog → Volumetric Clouds → Fluid Fog
/// Output: colortex0 (lit scene), colortex5.a (cloudLinearDepth for composite1 VL)
#include "../@engine/core/core.hlsl"
#include "../@engine/lib/fog.hlsl"
#include "../@engine/lib/math.hlsl"
#include "../include/rt_formats.hlsl"
#include "../lib/lighting.hlsl"
#include "../lib/atmosphere.hlsl"
#include "../lib/clouds.hlsl"
#include "../lib/noise.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0
    float4 color1 : SV_Target1; // colortex5 (mapped via RENDERTARGETS: 0,5)
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0,5 */

    // Default colortex5: cloudLinearDepth = 1.0 (no cloud)
    output.color1 = float4(0.0, 0.0, 0.0, 1.0);

    // --- GBuffer Sampling ---
    float depthAll = depthtex0.Sample(sampler1, input.TexCoord).r;

    float4 albedoAO = colortex0.Sample(sampler0, input.TexCoord);
    float3 albedo   = albedoAO.rgb;
    float  ao       = albedoAO.a;

    bool isSkyPixel = (depthAll >= 1.0);

    // Reconstruct view/world position (needed for clouds even on sky pixels)
    float3 viewPos = ReconstructViewPosition(input.TexCoord, depthAll,
                                             gbufferProjectionInverse, gbufferRendererInverse);
    float3 worldPos     = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;
    float  viewDistance = length(viewPos);

    float3 litColor     = albedo;
    bool   isCloudPixel = false; // Vanilla cloud pixel detection (depth < 1.0, zero lightmap)

    if (!isSkyPixel)
    {
        // Lightmap sampling
        float2 lightmapSample = colortex1.Sample(sampler0, input.TexCoord).rg;
        float  blockLight     = lightmapSample.r;
        float  skyLight       = lightmapSample.g;

        // Detect vanilla cloud pixels: has geometry depth but no lightmap (gbuffers_clouds writes colortex0 only)
        isCloudPixel = (blockLight == 0.0 && skyLight == 0.0);

        // Cloud/unlit geometry — no lightmap data, keep albedo as-is (CR approach: volumetric clouds blend over later)
        if (!isCloudPixel)
        {
            // GBuffer normal (colortex2, SNORM [-1,1], Point sampling)
            float3 worldNormal = normalize(colortex2.Sample(sampler1, input.TexCoord).xyz);

            // Light direction: shadowLightPosition (view space) -> world space
            float3 lightDirWorld = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));

            // Atmospheric fog factor (replaces hardcoded fogMix = 0.0)
            float atmFogFactor = GetAtmosphericFogFactor(viewDistance, fogStart, fogEnd);

            // Full lighting pipeline: diffuse + shadow (PCF) + light color + apply
            bool isUnderwater = (isEyeInWater == EYE_IN_WATER);
            litColor          = CalculateLighting(
                albedo, worldPos, worldNormal, lightDirWorld,
                skyLight, blockLight, skyBrightness,
                sunAngle,
                atmFogFactor, // R4.5: was 0.0, now computed
                rainStrength,
                isUnderwater, ao,
                shadowView, shadowProjection,
                shadowtex1, sampler1,
                input.Position.xy);

            litColor = saturate(litColor);

            // [R4.2] Atmospheric distance fog (overworld only, not underwater)
            if (isEyeInWater == EYE_IN_AIR)
            {
                float3 atmFogColor = ComputeAtmosphericFogColor(sunAngle, skyColor);
                litColor           = ApplyAtmosphericFog(litColor, atmFogColor, atmFogFactor);
            }
        }
    }
    else
    {
        // Sky pixel fluid fog override (full-screen color replacement)
        if (isEyeInWater == EYE_IN_WATER)
            litColor = fogColor;
        else if (isEyeInWater == EYE_IN_LAVA)
            litColor = LAVA_FOG_COLOR;
        else if (isEyeInWater == EYE_IN_POWDER_SNOW)
            litColor = POWDER_SNOW_FOG_COLOR;
    }

    // --- [R4.3] Volumetric Clouds ---
    // Compute sunVisibility from SdotU (CR deferred1.glsl:23-26)
    // SdotU = how high the sun is above the horizon
    float SdotU         = dot(normalize(sunPosition), normalize(upPosition));
    float sunVisibility = clamp(SdotU + 0.0625, 0.0, 0.125) / 0.125;

    // [FIX] worldPos is ABSOLUTE in our engine, but Iris/CR expects camera-relative (playerPos)
    // normalize(worldPos) bakes in camera's world offset → clouds tilt with camera movement
    float3 nPlayerPos = normalize(worldPos - cameraPosition);
    // [FIX] shadowLightPosition is view space, nPlayerPos is world space → must match
    float3 sunDirWorld      = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));
    float  VdotS            = dot(nPlayerPos, sunDirWorld);
    float  VdotU            = nPlayerPos.z; // Z-up engine
    float  dither           = InterleavedGradientNoiseForClouds(input.Position.xy);
    float  cloudLinearDepth = 1.0; // 1.0 = no cloud (default)

    // CR-style runtime cloud altitude: compile-time base + runtime offset
    // cloudHeight (Iris uniform, default 192.0) provides runtime adjustment
    int cloudAlt1i = int(CLOUD_ALT1 + (cloudHeight - 192.0));

    // Cloud render distance must reach the cloud layer altitude from camera
    // Player camera far (~128) is too short for clouds at z=192+
    float cloudRenderDist = max(far, 500.0) * 0.8;

    float4 clouds = GetVolumetricClouds(cloudAlt1i, cloudRenderDist, cloudLinearDepth, sunVisibility, cameraPosition, nPlayerPos, viewDistance, VdotS, VdotU, dither);

    if (clouds.a > 0.0)
    {
        // Sky/cloud pixels: always composite volumetric clouds
        // Terrain pixels: only if terrain is behind the cloud
        if (isSkyPixel || isCloudPixel || viewDistance > cloudLinearDepth * far)
        {
            litColor = lerp(litColor, clouds.rgb, clouds.a);
        }
    }

    // --- Fluid Fog (terrain pixels only, migrated from composite) ---
    if (!isSkyPixel && !isCloudPixel)
    {
        if (isEyeInWater == EYE_IN_WATER)
            litColor = ApplyUnderwaterFog(litColor, viewDistance, fogColor, fogStart, fogEnd);
        else if (isEyeInWater == EYE_IN_LAVA)
            litColor = ApplyLavaFog(litColor, viewDistance);
        else if (isEyeInWater == EYE_IN_POWDER_SNOW)
            litColor = ApplyPowderSnowFog(litColor, viewDistance);
    }

    // --- Output ---
    // colortex0: final lit color
    // colortex5.a: cloud linear depth for composite1 VL sampling
    output.color0 = float4(saturate(litColor), 1.0);
    output.color1 = float4(0.0, 0.0, 0.0, cloudLinearDepth);
    return output;
}
