/**
 * @file clouds.hlsl
 * @brief Reimagined-style volumetric cloud rendering via screen-space ray marching
 *
 * Cloud shape is driven by texture sampling (.b channel of cloud-water.png
 * via GetCustomImage(3) / customImage3 Bindless macro), NOT Perlin noise.
 *
 * Dependencies:
 *   - core.hlsl              (PI, TWO_PI)
 *   - common.hlsl            (Pow2, Luma)
 *   - noise.hlsl             (InterleavedGradientNoiseForClouds)
 *   - settings.hlsl          (CLOUD_* parameters)
 *   - custom_image_uniforms  (GetCustomImage / customImage3)
 *   - sampler_uniforms       (linearSampler)
 *   - common_uniforms        (skyColor, rainStrength, frameTimeCounter)
 *   - celestial_uniforms     (sunAngle, sunPosition, shadowLightPosition)
 *   - camera_uniforms        (cameraPosition, far)
 *
 * Reference:
 *   - ComplementaryReimagined reimaginedClouds.glsl
 *   - ComplementaryReimagined cloudCoord.glsl
 *   - ComplementaryReimagined cloudColors.glsl
 *
 * Requirements: R4.3.2, R4.3.3, R4.3.4, R4.3.5
 */

#ifndef LIB_CLOUDS_HLSL
#define LIB_CLOUDS_HLSL

// Cloud shape constants — defined in settings.hlsl
// CLOUD_NARROWNESS, CLOUD_ROUNDNESS_SAMPLE, CLOUD_ROUNDNESS_SHADOW

// ---------------------------------------------------------------------------
// Cloud Coordinate System
// Reference: CR cloudCoord.glsl
// ---------------------------------------------------------------------------

// Convert world XY position to 256x256 cloud texture coordinate with
// smoothstep rounding for soft cloud edges.
// Faithful port of CR cloudCoord.glsl — GetRoundedCloudCoord (by SixthSurge)
//
// @param pos        Narrowness-scaled horizontal position (after ModifyTracePos)
// @param roundness  Smoothstep roundness (0.125 for shape, 0.35 for shadow)
// @return           Normalized texture coordinate [0,1]
float2 GetRoundedCloudCoord(float2 pos, float roundness)
{
    float2 coord     = pos.yx + 0.5;
    float2 signCoord = sign(coord);
    coord            = abs(coord) + 1.0;

    float2 i;
    float2 f = modf(coord, i);

    f     = smoothstep(0.5 - roundness, 0.5 + roundness, f);
    coord = i + f;

    return (coord - 0.5) * signCoord / 256.0;
}

// ---------------------------------------------------------------------------
// Wind Animation
// Reference: CR reimaginedClouds.glsl — ModifyTracePos
// ---------------------------------------------------------------------------

// Apply wind animation and narrowness scaling to cloud trace position.
// Faithful port of CR cloudCoord.glsl — ModifyTracePos
// Coordinate mapping: MC(X,Y,Z) → Engine(X,Z,Y), our engine is Z-up
//   MC X → our X,  MC Z → our Y,  MC Y(up) → our Z(up)
//
// NOTE: Layer offset uses compile-time CLOUD_ALT1, NOT runtime-adjusted altitude.
//       The * 64.0 offset is for multi-layer texture differentiation only (CR design).
//       Runtime altitude adjustment (cloudHeight) only affects ray intersection & height mask.
//
// Wind uses worldTime (world-synced time, affected by time acceleration) instead of
// frameTimeCounter (real time). This matches CR's default use of syncedTime and ensures
// clouds accelerate with the world when using time scale controls.
// CLOUD_SPEED_MULT is preserved as an artistic speed multiplier on top of world time.
//
// @param tracePos  Current ray march position (world space, Z-up)
// @return          Wind-animated, narrowness-scaled position
float3 ModifyTracePos(float3 tracePos)
{
    // Convert world ticks to seconds: MC standard = 20 ticks/sec
    // worldDay * 24000 + worldTime = total ticks since world start
    float worldSeconds = float(worldDay * 24000 + worldTime) * 0.05; // 1/20 = 0.05

    float windSpeed  = float(CLOUD_SPEED_MULT) * 0.01;
    float windOffset = worldSeconds * windSpeed;

    tracePos.y  -= windOffset; // Wind on Y (= MC Z)
    tracePos.x  += float(CLOUD_ALT1) * 64.0; // Layer offset: compile-time base only
    tracePos.xy *= CLOUD_NARROWNESS; // Scale horizontal plane

    return tracePos;
}

// ---------------------------------------------------------------------------
// Cloud Noise Sampling
// Reference: CR reimaginedClouds.glsl — GetCloudNoise
// ---------------------------------------------------------------------------

// Sample cloud density from cloud-water.png .b channel.
// Uses 8th-power threshold for sharp Reimagined-style cloud edges.
//
// @param tracePos       Ray march position (world space)
// @param tracePosM      [inout] Modified trace position (wind-animated)
// @param cloudAltitude  Cloud layer altitude
// @return               true if cloud density > threshold (cloud hit)
bool GetCloudNoise(float3 tracePos, inout float3 tracePosM, int cloudAltitude)
{
    tracePosM = ModifyTracePos(tracePos);

    float2    coord         = GetRoundedCloudCoord(tracePosM.xy, CLOUD_ROUNDNESS_SAMPLE);
    Texture2D cloudWaterTex = customImage3;
    float     noise         = cloudWaterTex.Sample(sampler0, coord).b;

    // Height-based density modulation — use tracePos.z (actual vertical position, Z-up)
    float heightFactor = abs(tracePos.z - float(cloudAltitude));
    float heightMask   = saturate(1.0 - heightFactor * CLOUD_NARROWNESS);

    noise *= heightMask;

    // 8th-power threshold for sharp cloud edges (Reimagined signature)
    // pow2(pow2(pow2(x))) = x^8
    float threshold = Pow2(Pow2(Pow2(noise)));

    return threshold > 0.001;
}

// ---------------------------------------------------------------------------
// Cloud Colors
// Reference: CR cloudColors.glsl
// ---------------------------------------------------------------------------

// Compute cloud ambient color based on sky conditions.
//
// @param ambientColor   Base ambient light color
// @param inSkyColor     Sky color from CommonUniforms
// @param rain           Rain strength (0-1)
// @param sunVis         Sun visibility factor (0-1)
// @param noonFactor     Noon proximity factor (0-1)
// @return               Cloud ambient illumination color
float3 GetCloudAmbientColor(float3 ambientColor, float3 inSkyColor,
                            float  rain, float          sunVis, float noonFactor)
{
    float3 cloudRainColor = lerp(inSkyColor * 0.3, inSkyColor * 0.7, sunVis);
    return lerp(ambientColor * (sunVis * (0.55 + 0.17 * noonFactor) + 0.35),
                cloudRainColor * 0.5, rain);
}

// Compute cloud direct light color.
//
// @param lightColor   Direct sunlight/moonlight color
// @param inSkyColor   Sky color from CommonUniforms
// @param rain         Rain strength (0-1)
// @param noonFactor   Noon proximity factor (0-1)
// @return             Cloud direct illumination color
float3 GetCloudLightColor(float3 lightColor, float3 inSkyColor,
                          float  rain, float        noonFactor)
{
    float3 cloudRainColor = lerp(inSkyColor * 0.3, inSkyColor * 0.7, 0.5);
    return lerp(lightColor * 1.3, cloudRainColor * 0.45, noonFactor * rain);
}

// ---------------------------------------------------------------------------
// Main Volumetric Cloud Ray March
// Reference: CR reimaginedClouds.glsl — GetVolumetricClouds
// ---------------------------------------------------------------------------

// Ray march through cloud layer to produce volumetric cloud color and opacity.
// Cloud shape is texture-driven (Reimagined style), NOT Perlin noise.
// Self-shadow is computed at CLOUD_QUALITY >= 2 via 2 additional texture samples.
//
// @param cloudAltitude    Cloud layer altitude (CLOUD_ALT1 or CLOUD_ALT2)
// @param renderDistance   Max render distance for clouds (typically far * 0.8)
// @param cloudLinearDepth [inout] Cloud linear depth output (1.0 = no cloud)
// @param sunVisibility    Sun visibility factor (0-1)
// @param moonVisibility   Moon visibility factor (0-1)
// @param camPos           Camera world position
// @param nPlayerPos       Normalized view direction (camera to pixel)
// @param maxViewDist      Max cloud sample distance (terrain: viewDist-1, sky: 1e9)
// @param VdotS            dot(viewDir, sunDir) for light scattering
// @param VdotU            viewDir.z (vertical component, Z-up)
// @param dither           Screen-space dither value from IGN
// @param lightDirWorld    Normalized shadow light direction in WORLD space
// @return                 float4(cloudColor.rgb, cloudOpacity)
float4 GetVolumetricClouds(
    int         cloudAltitude,
    float       renderDistance,
    inout float cloudLinearDepth,
    float       sunVisibility,
    float3      camPos,
    float3      nPlayerPos,
    float       maxViewDist,
    float       VdotS,
    float       VdotU,
    float       dither,
    float3      lightDirWorld)
{
    float4 result = float4(0.0, 0.0, 0.0, 0.0);

    // Cloud layer vertical bounds
    float cloudStretch = CLOUD_STRETCH;
    float cloudUpper   = float(cloudAltitude) + cloudStretch;
    float cloudLower   = float(cloudAltitude) - cloudStretch;

    // [FIX] Near-horizontal rays: HLSL handles non-zero/0 = ±Inf correctly,
    // and min/max clamp Inf to renderDistance. Only guard against exact 0/0 = NaN
    // when camera is exactly at cloud boundary AND ray is perfectly horizontal.
    float safeZ = (abs(nPlayerPos.z) < 1e-7) ? 1e-7 : nPlayerPos.z;

    // Calculate ray intersection distances with cloud layer planes
    float distToUpper = (cloudUpper - camPos.z) / safeZ;
    float distToLower = (cloudLower - camPos.z) / safeZ;

    // Order distances so nearDist < farDist
    float nearDist = min(distToUpper, distToLower);
    float farDist  = max(distToUpper, distToLower);

    // Clamp to valid range
    nearDist = max(nearDist, 0.0);
    farDist  = min(farDist, renderDistance);

    if (nearDist >= farDist)
        return result;

    // Sample count based on quality level
    int sampleCount;
#if CLOUD_QUALITY == 1
    sampleCount = 16;
#elif CLOUD_QUALITY == 3
    sampleCount = 48;
#else // CLOUD_QUALITY == 2 (default)
    sampleCount = 32;
#endif

    // Dynamic step size: near = fine detail, far = coarse
    float planeDistanceDif = farDist - nearDist;
    float stepMult         = planeDistanceDif / float(sampleCount);

    // Light direction for self-shadow sampling (world space, passed from caller)
    float3 lightDir = lightDirWorld;

    // Cloud color setup — simplified approach using skyColor uniform
    // skyColor is computed by SkyColorHelper's 5-phase system (C++ side)
    // Cloud base = bright white tinted slightly by sky color (day) / dark blue (night)
    float noonFactor = sqrt(max(sin(sunAngle * TWO_PI), 0.0));
    float sunVis2    = sunVisibility * sunVisibility;

    // Cloud ambient: sky-tinted white (lerp toward white for brighter clouds)
    float3 dayAmbient   = lerp(skyColor, float3(1.0, 1.0, 1.0), 0.6) * 0.85;
    float3 nightAmbient = CLOUD_NIGHT_AMBIENT * CLOUD_NIGHT_AMBIENT_MULT;
    float3 ambientColor = lerp(nightAmbient, dayAmbient, sunVis2);

    // Cloud light: warm sunlight (day) / cool moonlight (night)
    float3 dayLight   = lerp(float3(1.2, 0.9, 0.6), float3(1.0, 1.0, 0.95), noonFactor);
    float3 nightLight = CLOUD_NIGHT_LIGHT * CLOUD_NIGHT_LIGHT_MULT;
    float3 lightColor = lerp(nightLight, dayLight, sunVis2);

    // Apply user color tint
    float3 colorTint = float3(float(CLOUD_R), float(CLOUD_G), float(CLOUD_B)) * 0.01;

    float3 cloudAmbient = GetCloudAmbientColor(ambientColor, skyColor, rainStrength,
                                               sunVis2, noonFactor) * colorTint;
    float3 cloudLight = GetCloudLightColor(lightColor, skyColor, rainStrength,
                                           noonFactor) * colorTint;

    // Ray march through cloud layer
    for (int i = 0; i < sampleCount; i++)
    {
        float lTracePos = nearDist + (float(i) + dither) * stepMult;

        // Skip samples beyond render distance
        if (lTracePos > renderDistance)
            break;

        // [CR] Per-sample terrain occlusion: skip cloud samples behind terrain geometry.
        // maxViewDist = viewDistance - 1.0 for terrain pixels, 1e9 for sky pixels.
        // Ref: CR reimaginedClouds.glsl — if (lTracePos > lViewPosM) continue;
        if (lTracePos > maxViewDist)
            continue;

        float3 tracePos = camPos + nPlayerPos * lTracePos;
        float3 tracePosM;

        if (GetCloudNoise(tracePos, tracePosM, cloudAltitude))
        {
            // Cloud hit — compute shading

            // Height-based shading gradient (Z-up: tracePos.z is altitude)
            // 0.0 at cloud bottom, 0.5 at center, 1.0 at cloud top
            float heightGrad = saturate((tracePos.z - float(cloudAltitude) + cloudStretch)
                / (2.0 * cloudStretch));

            // pow(2.5): continuous curve, no hard cutoff.
            // Bottom half stays dark (0.5 → 0.18), gradual transition to bright top.
            // Overhead clouds (first-hit at ~0.4-0.5) get heightGrad ~0.10-0.18 → dark.
            heightGrad = pow(max(heightGrad, 0.0), CLOUD_SHADING_POWER);

            // Self-shadow: sample along light direction (Q >= 2)
            // CR: shadow weight is height-dependent — stronger at bottom, zero at top
            float cloudShadingM = 1.0 - Pow2(heightGrad);
            float light         = 1.0;
#if CLOUD_QUALITY >= 2
            {
                Texture2D cloudWaterTex = customImage3;
                // Shadow step: larger than CR's 0.3 to reach deeper into cloud body,
                // ensuring overhead clouds get self-shadow even with low-angle sun
                float shadowStep = CLOUD_SHADOW_STEP;

                [unroll]
                for (int s = 1; s <= 2; s++)
                {
                    // Offset in WORLD space, then apply ModifyTracePos for correct coordinates
                    float3 shadowWorldPos = tracePos + lightDir * (shadowStep * float(s));
                    float3 shadowPosM     = ModifyTracePos(shadowWorldPos);
                    float2 shadowCoord    = GetRoundedCloudCoord(shadowPosM.xy, CLOUD_ROUNDNESS_SHADOW);
                    float  shadowNoise    = cloudWaterTex.Sample(sampler0, shadowCoord).b;
                    light                 -= shadowNoise * cloudShadingM * CLOUD_SHADOW_STRENGTH;
                }
                light = max(light, CLOUD_SHADOW_MIN);
            }
#endif

            // CR shading formula: compressed range with view-angle scattering
            // VdotSM1: half-lambert of view-sun angle for forward scattering
            float VdotSM1      = max(VdotS * 0.5 + 0.5, 0.0);
            float VdotSM2      = VdotSM1 * sunVisibility * 0.25 + 0.5 * heightGrad + 0.08;
            float cloudShading = VdotSM2 * (CLOUD_LIGHT_MIX_BASE + CLOUD_LIGHT_MIX_RANGE * light);

            // CR color: ambient * 0.95 * (1 - 0.35*shading) + light * (0.1 + shading)
            // Compressed shading range (~0.08-0.58) ensures smooth bottom-to-top gradient
            float3 cloudColor = cloudAmbient * 0.95 * (1.0 - 0.35 * cloudShading)
                + cloudLight * (0.1 + cloudShading);

            // Distance fog applied to cloud itself
            float cloudDist = lTracePos / renderDistance;
            float cloudFog  = saturate(cloudDist * cloudDist);
            cloudColor      = lerp(cloudColor, skyColor, cloudFog * 0.5);

            // Cloud linear depth for VL modulation (CR convention)
            // sqrt for non-linear distribution: near clouds get more precision
            cloudLinearDepth = sqrt(lTracePos / renderDistance);

            // Output: first-hit, single layer
            result = float4(cloudColor, saturate(1.0 - cloudFog));
            break;
        }
    }

    return result;
}

#endif // LIB_CLOUDS_HLSL
