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
 * Water surface receives forward lighting (CalculateLighting) since it bypasses deferred1.
 * Depth-based alpha: shallow water transparent, deep water opaque (CR waterFog formula).
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
#include "../lib/reflection.hlsl"
#include "../lib/lighting.hlsl"
#include "../lib/atmosphere.hlsl"

// [RENDERTARGETS] 0,1,2,3,4
// SV_TARGET0 -> colortex0, SV_TARGET1 -> colortex1,
// SV_TARGET2 -> colortex2, SV_TARGET3 -> colortex3, SV_TARGET4 -> colortex4

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
    float4 TranslucentMult : SV_TARGET3; // colortex3: 1.0 - translucentMult (CR TM5723 trick)
    float4 Material : SV_TARGET4; // colortex4: MaterialMask(R) + Smoothness(G) + Reflectivity(B) + fresnelM(A)
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

        // ================================================================
        // Forward Lighting (CR: water lit before reflection blending)
        // gbuffers_water runs AFTER deferred1, so water bypasses deferred
        // lighting. Apply the same CR lighting formula here so water color
        // changes with time of day (noon warm, sunset orange, night cold).
        // ================================================================
        float3 lightDirWorld = normalize(mul((float3x3)gbufferViewInverse, shadowLightPosition));
        float3 litWaterColor = CalculateLighting(
            water.color, input.WorldPos, water.normalM, lightDirWorld,
            input.LightmapCoord.y, input.LightmapCoord.x,
            skyBrightness, sunAngle,
            0.0, // fogMix: no atmospheric fog attenuation on water diffuse
            rainStrength, false, 1.0, // not underwater, ao=1.0
            shadowView, shadowProjection, shadowtex1, sampler1,
            input.Position.xy);

        // ================================================================
        // Depth-based Water Alpha (CR: shallow=transparent, deep=opaque)
        // Reads depthtex1 (opaque-only depth) to compute water-to-seabed
        // distance. Shallow water lets you see underwater blocks clearly.
        // CR: waterFog = max(0, 1 - exp(depthDiff * 0.075))
        //     alpha *= 0.25 + 0.75 * waterFog
        // ================================================================
        float2 screenUV    = input.Position.xy / float2(viewWidth, viewHeight);
        float  opaqueDepth = depthtex1.Sample(sampler1, screenUV).r;

        float3 waterViewPos  = mul(gbufferView, float4(input.WorldPos, 1.0)).xyz;
        float  waterViewDist = length(waterViewPos);

        float3 opaqueViewPos = ReconstructViewPosition(
            screenUV, opaqueDepth,
            gbufferProjectionInverse, gbufferRendererInverse);
        float opaqueViewDist = length(opaqueViewPos);

        float depthDiff     = max(opaqueViewDist - waterViewDist, 0.0);
        float waterFogAlpha = max(0.0, 1.0 - exp(-depthDiff * WATER_DEPTH_FOG_DENSITY));
        water.alpha         *= WATER_DEPTH_ALPHA_MIN + (1.0 - WATER_DEPTH_ALPHA_MIN) * waterFogAlpha;

        // Re-apply fresnel alpha AFTER depth fog (CR order: base -> depthFog -> fresnel)
        // ComputeWaterSurface already applied fresnel4, but depth fog crushed it.
        // At grazing angles fresnel4 -> 1.0, pulling alpha back to opaque.
        float rawFresnel = saturate(1.0 + dot(water.normalM, -viewDir));
        float fresnel4   = Pow2(Pow2(rawFresnel));
        water.alpha      = lerp(water.alpha, 1.0, fresnel4);

        // colortex2: Perturbed world normal
        output.Normal = float4(water.normalM, 1.0);

        // colortex4: Material data
        output.Material = float4(241.0 / 255.0, water.smoothness, WATER_REFLECTIVITY, water.fresnel);

        // ================================================================
        // Three-Layer Reflection Architecture (CR Reimagined style)
        // Layer 1: SSR Ray March     (near, high quality)
        // Layer 2: Mirrored Image    (mid, colortex0 reprojection)
        // Layer 3: Procedural Sky    (far/fallback, atmospheric gradient)
        // Underwater: reduce reflection strength to 25% (CR: reflectMult)
        // ================================================================
        float sunVis  = GetSunVisibility(sunPosition, upPosition);
        float sunVis2 = sunVis * sunVis;
        float noonFac = GetNoonFactor(sunAngle);

        float  reflectMult = (isEyeInWater == EYE_IN_WATER) ? 0.25 : 1.0;
        float3 waterColor  = litWaterColor;

#if WATER_REFLECT_QUALITY >= 1
        // Shared view-space transforms (used by both SSR and Mirror)
        float3 viewDirToFrag = normalize(input.WorldPos - cameraPosition);
        float3 reflectDir    = reflect(viewDirToFrag, water.normalM);
        float3 viewPos       = mul(gbufferView, float4(input.WorldPos, 1.0)).xyz;
        float3 nViewPos      = normalize(viewPos);
        float3 viewNormal    = normalize(mul((float3x3)gbufferView, water.normalM));
        float3 reflectDirVS  = normalize(reflect(nViewPos, viewNormal));
        float  lViewPos      = length(viewPos);

        float skyLightFactor = input.LightmapCoord.y;
        float dither         = InterleavedGradientNoiseForClouds(input.Position.xy);

        // Layer 3: Procedural sky reflection (always computed)
        float3 skyReflection = ComputeSkyReflection(
            reflectDir, skyLightFactor, sunVis2, noonFac);

        // Layer 2: Mirrored image reflection
        SSRResult mirror = ComputeMirroredReflection(
            viewPos, nViewPos, reflectDirVS, lViewPos, dither);

        // Layer 1: SSR ray march (quality >= 2 only)
        SSRResult ssr;
        ssr.color = float3(0.0, 0.0, 0.0);
        ssr.alpha = 0.0;

#if WATER_REFLECT_QUALITY >= 2
        float depth = input.Position.z;
        ssr         = ComputeSSR(
            input.WorldPos, water.normalM, viewDirToFrag,
            water.smoothness, dither, depth);
#endif

        // Blend three layers: SSR -> Mirror -> Sky
        ReflectionResult reflection = BlendReflectionLayers(ssr, mirror, skyReflection);
        waterColor                  = lerp(waterColor, reflection.color, water.fresnel * reflectMult);
#endif

        // ================================================================
        // Underwater Distance Fog (CR: DoFog in gbuffers_water)
        // When camera is underwater, water surface must fade into fog
        // at distance, matching terrain fog from deferred1.
        // ================================================================
        if (isEyeInWater == EYE_IN_WATER)
        {
            waterColor = ApplyCRUnderwaterFog(waterColor, waterViewDist, sunVis2);
        }

        // ================================================================
        // TranslucentMult for VL colored light shafts (CR water.glsl:11-13, 243-245)
        // Encodes how light is filtered when passing through the water surface.
        // Used by composite1 VL raymarching to tint underwater god rays.
        // Written as 1.0 - value (TM5723 trick: clear=0 inverts to 1.0 = no effect).
        // ================================================================
        float3 translucentMult = normalize(max(abs(input.Color.rgb), 0.001));
        // sqrt2 approximation: CR uses sqrt2(glColor) = pow(glColor, 0.25)
        translucentMult   = sqrt(translucentMult);
        translucentMult.g *= 0.88; // CR water.glsl:13

        // Fresnel attenuation: less light passes through at grazing angles
        // CR water.glsl:245 (WATER_STYLE != 1 path)
        translucentMult *= 1.0 - 0.9 * max(0.5 * sqrt(fresnel4), fresnel4);

        // Distance fade: at far distances, translucentMult -> 1.0 (no effect)
        // CR gbuffers_water.glsl:250
        float distFade  = saturate(Pow2(Pow2(waterViewDist / far)));
        translucentMult = lerp(translucentMult, float3(1.0, 1.0, 1.0), distFade);

        // Store inverted (TM5723): colortex3 clears to 0, so 1-tm gives 1.0 for non-water pixels
        output.TranslucentMult = float4(1.0 - translucentMult, 1.0);

        output.Color = float4(waterColor, water.alpha);
    }
    else
    {
        // ================================================================
        // Non-water Translucent (default path)
        // ================================================================
        float4 finalColor = texColor * input.Color;

        output.Color           = finalColor;
        output.Normal          = float4(0, 0, 0, 0);
        output.TranslucentMult = float4(0, 0, 0, 0); // Inverts to 1.0 = no translucent effect
        output.Material        = float4(0, 0, 0, 0);
    }

    return output;
}
