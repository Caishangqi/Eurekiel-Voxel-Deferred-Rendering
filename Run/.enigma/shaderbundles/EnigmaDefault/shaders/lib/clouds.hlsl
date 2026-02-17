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

// Cloud shape constants (Reimagined style)
static const float CLOUD_NARROWNESS       = 0.07;
static const float CLOUD_ROUNDNESS_SAMPLE = 0.125;
static const float CLOUD_ROUNDNESS_SHADOW = 0.35;

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
// @param tracePos       Current ray march position (world space, Z-up)
// @param cloudAltitude  Cloud layer altitude (CLOUD_ALT1 or CLOUD_ALT2)
// @return               Wind-animated, narrowness-scaled position
float3 ModifyTracePos(float3 tracePos, int cloudAltitude)
{
    float windSpeed  = float(CLOUD_SPEED_MULT) * 0.01;
    float windOffset = frameTimeCounter * windSpeed;

    tracePos.y  -= windOffset; // Wind on Y (= MC Z)
    tracePos.x  += float(cloudAltitude) * 64.0; // Layer offset on X (= MC X)
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
    tracePosM = ModifyTracePos(tracePos, cloudAltitude);

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
// @param viewDistance     Linear distance from camera to scene pixel
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
    float       viewDistance,
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

    // Skip if view direction is nearly horizontal (no intersection with cloud plane)
    if (abs(nPlayerPos.z) < 0.001)
        return result;

    // Calculate ray intersection distances with cloud layer planes
    float distToUpper = (cloudUpper - camPos.z) / nPlayerPos.z;
    float distToLower = (cloudLower - camPos.z) / nPlayerPos.z;

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

    // Cloud color setup
    float  sunHeight    = sin(sunAngle * TWO_PI);
    float  noonFactor   = saturate(sunHeight);
    float3 ambientColor = skyColor * 0.8;
    float3 lightColor   = lerp(float3(0.1, 0.15, 0.3), float3(1.0, 0.95, 0.85), sunVisibility);

    // Apply user color tint
    float3 colorTint = float3(float(CLOUD_R), float(CLOUD_G), float(CLOUD_B)) * 0.01;

    float3 cloudAmbient = GetCloudAmbientColor(ambientColor, skyColor, rainStrength,
                                               sunVisibility, noonFactor) * colorTint;
    float3 cloudLight = GetCloudLightColor(lightColor, skyColor, rainStrength,
                                           noonFactor) * colorTint;

    // Ray march through cloud layer
    for (int i = 0; i < sampleCount; i++)
    {
        float lTracePos = nearDist + (float(i) + dither) * stepMult;

        // Skip samples beyond render distance
        if (lTracePos > renderDistance)
            break;

        float3 tracePos = camPos + nPlayerPos * lTracePos;
        float3 tracePosM;

        if (GetCloudNoise(tracePos, tracePosM, cloudAltitude))
        {
            // Cloud hit — compute shading

            // Height-based shading gradient (Z-up: tracePos.z is altitude)
            float heightGrad = saturate((tracePos.z - float(cloudAltitude) + cloudStretch)
                / (2.0 * cloudStretch));

            // Self-shadow: 2 additional texture samples along light direction (Q >= 2)
            float shadow = 1.0;
#if CLOUD_QUALITY >= 2
            {
                Texture2D cloudWaterTex = customImage3;
                float     shadowStep    = cloudStretch * 0.5;

                [unroll]
                for (int s = 1; s <= 2; s++)
                {
                    float3 shadowPos   = tracePosM + lightDir * (shadowStep * float(s));
                    float2 shadowCoord = GetRoundedCloudCoord(shadowPos.xy, CLOUD_ROUNDNESS_SHADOW);
                    float  shadowNoise = cloudWaterTex.Sample(sampler0, shadowCoord).b;
                    shadow             -= shadowNoise * 0.35;
                }
                shadow = max(shadow, 0.2);
            }
#endif

            // Blend ambient + direct light with shadow
            float3 cloudColor = cloudAmbient + cloudLight * shadow * heightGrad;

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
