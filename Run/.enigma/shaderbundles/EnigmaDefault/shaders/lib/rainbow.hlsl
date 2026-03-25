/// Rainbow arc library (CR rainbow.glsl port)
/// Procedural spectral rainbow via modulo math — no texture lookups.
/// Appears at ~42 degrees from anti-solar point when sun is at low angle.
///
/// Architecture:
///   composite1 calls GetRainbow() after VL, adds result to scene color.
///   Pure math: spectral color from modulo arithmetic, intensity from angular falloff.
///
/// Dependencies:
///   - core.hlsl (sunPosition, upPosition, isEyeInWater, far, colortex5)
///   - include/settings.hlsl (RAINBOWS, RAINBOW_DIAMETER)
///   - lib/common.hlsl (Pow2, Pow4)
///
/// Reference: ComplementaryReimagined lib/atmospherics/rainbow.glsl

#ifndef LIB_RAINBOW_HLSL
#define LIB_RAINBOW_HLSL

/// Compute procedural rainbow color contribution for a given pixel.
///
/// @param translucentMult  Water/translucent color filter from colortex3
/// @param nViewDir         Normalized view direction (world space, camera-relative)
/// @param z0               depthtex0 value (all geometry)
/// @param z1               depthtex1 value (opaque only)
/// @param lViewPos         Linear view distance (all geometry)
/// @param VdotL            dot(viewDir, lightDir) — angle from sun
/// @param VdotU            dot(viewDir, upDir) — vertical angle
/// @param SdotU            dot(sunDir, upDir) — sun elevation
/// @param sunAngle         Sun angle [0,1] for noon factor
/// @param vlFactor         Cloud linear depth from colortex5.a
/// @param dither           Dither value for banding reduction
/// @param wetness          Wetness factor [0,1] (placeholder, unused for now)
/// @param rainStrength     Rain intensity [0,1] (placeholder, unused for now)
/// @return                 Additive rainbow color (HDR, linear space)
float3 GetRainbow(float3 translucentMult, float3 nViewDir,
                  float  z0, float               z1, float    lViewPos,
                  float  VdotL, float            VdotU, float SdotU, float sunAngle,
                  float  vlFactor, float         dither,
                  float  wetness, float          inRainStrength)
{
    float3 rainbow = float3(0.0, 0.0, 0.0);

    // --- Phase 1: Visibility timing ---
    // Rainbow appears when sun is at low angle (SdotU between 0.1 and 0.25)
    float rainbowTime = saturate(max(SdotU - 0.1, 0.0) / 0.15);

    // Suppress at high noon (noonFactor^8 ramp)
    float noonFactor   = sqrt(max(sin(sunAngle * TWO_PI), 0.0));
    float noonSuppress = Pow2(Pow2(Pow2(noonFactor)));
    rainbowTime        = clamp(rainbowTime - noonSuppress * 8.0, 0.0, 0.85);

    // --- Phase 2: Weather modulation (placeholder) ---
    // TODO: When weather system is ready, uncomment:
    // rainbowTime *= sqrt(max(wetness - 0.333, 0.0) * 1.5) * (1.0 - inRainStrength);
    // For now: always visible when sun angle is right (RAINBOWS == 3 mode)

    if (rainbowTime < 0.001)
        return rainbow;

    // --- Phase 3: Distance and cloud attenuation ---
    float rainbowLength = far * 0.9;
    float lViewPos1     = lViewPos;
    if (z1 >= 1.0)
        lViewPos1 = rainbowLength; // Sky pixels: use full rainbow distance

    // Cloud depth modulation: clouds in front reduce rainbow
    float cloudDisMult = Pow2(vlFactor + (1.0 / 255.0) * dither);
    lViewPos1          *= cloudDisMult;

    // Depth gate: rainbow only visible on distant/sky pixels.
    // Nearby geometry (trees, terrain) should NOT show rainbow.
    // Fade in starting at 70% of rainbowLength, full at 100%.
    float depthGate = smoothstep(rainbowLength * 0.7, rainbowLength, lViewPos1);
    if (depthGate < 0.001)
        return rainbow;

    // --- Phase 4: Anti-solar point geometry ---
    // Rainbow at ~42 degrees from anti-solar point (VdotL ~ -0.75)
    // RAINBOW_DIAMETER controls arc width
    float rainbowCoord = saturate(1.0 - (VdotL + 0.75) / (0.0625 * RAINBOW_DIAMETER));

    // Bell curve intensity: peaks at center of arc, fades at edges
    float rainbowFactor = rainbowCoord * (1.0 - rainbowCoord);
    rainbowFactor       = Pow2(Pow2(rainbowFactor * 3.7)); // Sharpen edges

    // Depth gate from Phase 3 (replaces CR's pow2 distance fade)
    rainbowFactor *= depthGate;

    // Horizon limit: only show rainbow above the horizon (VdotU > 0)
    // Physically, ground blocks the lower arc. Smooth fade near horizon.
    rainbowFactor *= smoothstep(-0.05, 0.05, VdotU);

    // Time and visibility
    rainbowFactor *= rainbowTime;

    if (rainbowFactor < 0.001)
        return rainbow;

    // --- Phase 5: Spectral color generation ---
    // Non-linear coordinate mapping for natural color distribution
    float rainbowCoordM = pow(rainbowCoord, 1.4 + max(rainbowCoord - 0.5, 0.0) * 1.6);
    rainbowCoordM       = smoothstep(0.0, 1.0, rainbowCoordM) * 0.85;
    rainbowCoordM       += (dither - 0.5) * 0.1; // Dither to break banding

    // RGB spectrum via modulo arithmetic (3 samples averaged for smoothness)
    // Phase offsets (-0.55, 4.3, 2.2) separate R, G, B channels
    float3 phaseOffset = float3(-0.55, 4.3, 2.2);

    float3 s1   = saturate(abs(fmod(rainbowCoordM * 6.0 + phaseOffset, 6.0) - 3.0) - 1.0);
    float  rcm2 = rainbowCoordM + 0.1;
    float3 s2   = saturate(abs(fmod(rcm2 * 6.0 + phaseOffset, 6.0) - 3.0) - 1.0);
    float  rcm3 = rainbowCoordM - 0.1;
    float3 s3   = saturate(abs(fmod(rcm3 * 6.0 + phaseOffset, 6.0) - 3.0) - 1.0);
    rainbow     = (s1 + s2 + s3) / 3.0;

    // --- Phase 6: Red enhancement (outer arc, natural appearance) ---
    rainbow.r += Pow2(max(rainbowCoord - 0.5, 0.0)) * max(1.0 - rainbowCoord, 0.0) * 26.0;

    // --- Phase 7: Tone and gamma ---
    rainbow = pow(max(rainbow, 0.0), 2.2) * float3(0.25, 0.075, 0.25) * 3.0;

    // --- Phase 8: Translucent surface interaction ---
    if (z1 > z0 && lViewPos < rainbowLength)
        rainbow *= lerp(translucentMult, float3(1.0, 1.0, 1.0), lViewPos / rainbowLength);

    // Underwater attenuation: rainbow seen through water is significantly dimmed.
    // sqrt(VdotU) fades when looking down; base 0.15 factor for overall water absorption.
    if (isEyeInWater != EYE_IN_AIR)
        rainbow *= 0.15 * saturate(sqrt(max(VdotU, 0.0)));

    rainbow *= rainbowFactor;

    return rainbow;
}

#endif // LIB_RAINBOW_HLSL
