/// Composite pass 1 — volumetric light (god rays) post-processing
/// Reads colortex5.a (cloudLinearDepth/vlFactor) written by deferred1
/// Reference: CR composite1.glsl, design R4.6.2
///
/// Pipeline: deferred1 output -> composite (passthrough) -> composite1 (VL) -> final
/// Output: colortex0 (scene + VL), colortex5.a (vlFactor preserved)
#include "../@engine/core/core.hlsl"
#include "../@engine/lib/math.hlsl"
#include "../include/settings.hlsl"
#include "../lib/common.hlsl"
#include "../lib/shadow.hlsl"
#include "../lib/noise.hlsl"
#include "../lib/volumetricLight.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0
    float4 color1 : SV_Target1; // colortex5 (mapped via RENDERTARGETS: 0,5)
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0,5 */

    // Read scene color from deferred/composite output
    float3 sceneColor = colortex0.Sample(sampler0, input.TexCoord).rgb;

    // Gamma → Linear: deferred1 outputs gamma-space color (sRGB textures, gamma lighting).
    // Must linearize before VL addition and tonemap (composite5 applies LinearToSRGB).
    // Without this, LinearToSRGB in composite5 double-gammas → washed out, low saturation.
    // Ref: CR composite1.glsl — color = pow(color, vec3(2.2))
    sceneColor  = pow(max(sceneColor, 0.0), 2.2);
    float depth = depthtex0.Sample(sampler1, input.TexCoord).r;

    // Read cloud linear depth from colortex5.a (written by deferred1)
    // vlFactor: 1.0 = no cloud (full VL), <1.0 = cloud present (reduced VL)
    float vlFactor = colortex5.Sample(sampler1, input.TexCoord).a;

    // Compute sunVisibility from SdotU (CR composite1.glsl:24)
    float SdotU         = dot(normalize(sunPosition), normalize(upPosition));
    float sunVisibility = clamp(SdotU + 0.0625, 0.0, 0.125) / 0.125;

    // Reconstruct world position
    float3 viewPos = ReconstructViewPosition(input.TexCoord, depth,
                                             gbufferProjectionInverse, gbufferRendererInverse);
    float3 worldPos = mul(gbufferViewInverse, float4(viewPos, 1.0)).xyz;

    // View direction (camera-relative, world space)
    // [FIX] worldPos is ABSOLUTE in our engine — subtract cameraPosition
    float3 nViewDir = normalize(worldPos - cameraPosition);

    // Light direction in world space
    float3 lightDirWorld = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));
    // Up direction in world space
    float3 upDirWorld = normalize(mul((float3x3)gbufferViewInverse, upPosition));

    // Directional dot products (CR composite1.glsl:196-198)
    float VdotL = dot(nViewDir, lightDirWorld);
    float VdotU = dot(nViewDir, upDirWorld);

    // Screen-space dither for ray march offset
    float dither = InterleavedGradientNoiseForClouds(input.Position.xy);

    // Volumetric light via shadow map ray marching
    // Returns float3 (additive color), no longer float4
    float3 vl = GetVolumetricLight(
        worldPos, cameraPosition, VdotL, VdotU, SdotU, dither, vlFactor,
        sunVisibility,
        shadowView, shadowProjection,
        shadowtex1, sampler1);

    // Additive blend: VL adds light to scene (HDR, no clamping)
    // Tonemap in composite5 will handle HDR → LDR conversion
    sceneColor += vl;

    output.color0 = float4(sceneColor, 1.0);
    output.color1 = float4(0.0, 0.0, 0.0, vlFactor);
    return output;
}
