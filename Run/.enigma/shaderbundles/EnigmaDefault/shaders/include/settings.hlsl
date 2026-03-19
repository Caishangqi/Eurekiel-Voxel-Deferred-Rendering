/// Centralized shader configuration for EnigmaDefault ShaderBundle
/// Reference: miniature-shader/shaders/shader.h

#ifndef INCLUDE_SETTINGS_HLSL
#define INCLUDE_SETTINGS_HLSL

//============================================================================//
// Visual Style (CR SHADER_STYLE)
// 1 = Reimagined: Stylized Minecraft look, perpendicular sun, vibrant colors
// 4 = Unbound:    Semi-realistic fantasy, angled sun, muted tones
// Reference: ComplementaryReimagined lib/common.glsl:14
//============================================================================//

#ifndef SHADER_STYLE
#define SHADER_STYLE 1              // [1 4] 1=Reimagined, 4=Unbound
#endif

//============================================================================//
// Celestial Settings
//============================================================================//

// Sun path rotation angle (engine directive, parsed at load time)
// IMPORTANT: ConstDirectiveParser reads raw source — no preprocessor evaluation.
//   This value MUST be a plain float literal. Cannot use #if or #define.
//   Change this value to match your SHADER_STYLE:
//     Reimagined (SHADER_STYLE 1): 0.0   (perpendicular, sun overhead, MC style)
//     Unbound    (SHADER_STYLE 4): -40.0 (angled sun, realistic shadows)
// Reference: CR common.glsl:441-445
const float sunPathRotation = -40; //[-90.0 -80.0 -70.0 -60.0 -50.0 -40.0 -30.0 -20.0 -10.0 0.0 10.0 20.0 30.0 40.0 50.0 60.0 70.0 80.0 90.0]

//============================================================================//
// Shadow Settings
//============================================================================//

// Shadow map resolution (engine directive, parsed at load time by ConstDirectiveParser)
// Iris pattern: const int shadowMapResolution = 2048;
const int shadowMapResolution = 2048; //[512 1024 2048 4096 8192]

#ifndef SHADOW_PIXEL
#define SHADOW_PIXEL 0             // [0 - 2048] Pixelated shadow resolution (0 = smooth)
#endif

#ifndef SHADOW_DISTANCE
#define SHADOW_DISTANCE 128.0       // [8.0 - 1024.0] Shadow render distance
#endif

// PCF shadow filtering quality
// -1 = off (hard shadows), 0 = basic 4-tap, 1-5 = circular PCF with increasing samples
#ifndef SHADOW_QUALITY
#define SHADOW_QUALITY 5            // [-1 0 1 2 3 4 5]
#endif

// PCF kernel radius (larger = softer but more blurry)
// 1 = tightest, 4 = widest spread
#ifndef SHADOW_SMOOTHING
#define SHADOW_SMOOTHING 4          // [1 2 3 4]
#endif

// Distance-adaptive PCF: how much to widen the kernel at max shadow distance
// 0.0 = no distance scaling, 3.0 = 4x wider at max distance
#ifndef SHADOW_PCF_DIST_SCALE
#define SHADOW_PCF_DIST_SCALE 5.0   // [0.0 1.0 2.0 3.0 4.0 5.0]
#endif

// Shadow edge fade range (in blocks) — smooth transition at shadow distance boundary
// Ref: ComplementaryReimagined mainLighting.glsl shadowSmooth = 16.0
#ifndef SHADOW_EDGE_FADE_RANGE
#define SHADOW_EDGE_FADE_RANGE 8.0 // [4.0 8.0 16.0 24.0 32.0]
#endif

//============================================================================//
// Scene Lighting Colors (CR lightAndAmbientColors.glsl)
//============================================================================//

// Day: Noon light color
// CR: vec3(0.65, 0.55, 0.375) * 2.05
#ifndef NOON_LIGHT_COLOR
#define NOON_LIGHT_COLOR float3(1.3325, 1.1275, 0.76875)
#endif

// Day: Sunset light color (approximated at typical sunset angle)
// CR: pow(vec3(0.64, 0.45, 0.3), 1.5+) * 5.0
#ifndef SUNSET_LIGHT_COLOR
#define SUNSET_LIGHT_COLOR float3(1.28, 0.675, 0.3375)
#endif

// Night: Moonlight on surfaces
// CR: 0.9 * vec3(0.15, 0.14, 0.20) * (0.4 + vsBrightness * 0.4)
#ifndef NIGHT_LIGHT_COLOR
#define NIGHT_LIGHT_COLOR float3(0.135, 0.126, 0.180)
#endif

// Night: Sky ambient
// CR: 0.9 * vec3(0.09, 0.12, 0.17) * (1.55 + vsBrightness * 0.77)
#ifndef NIGHT_AMBIENT_COLOR
#define NIGHT_AMBIENT_COLOR float3(0.081, 0.108, 0.153)
#endif

// Block light warm color
// CR: blocklightCol
#ifndef BLOCKLIGHT_COLOR
#define BLOCKLIGHT_COLOR float3(1.0, 0.7, 0.46)
#endif

// Cave minimum light color tint
#ifndef MIN_LIGHT_COLOR
#define MIN_LIGHT_COLOR float3(0.45, 0.475, 0.6)
#endif

//============================================================================//
// Time Constants (normalized 0.0 - 1.0, based on MC 24000 tick cycle)
//============================================================================//

#define TIME_NOON     0.25          // 6000 ticks
#define TIME_SUNSET   0.5325        // 12780 ticks
#define TIME_MIDNIGHT 0.75          // 18000 ticks
#define TIME_SUNRISE  0.9675        // 23220 ticks

#define NORMALIZE_TIME (1.0 / 24000.0)

//============================================================================//
// Fog Settings
//============================================================================//

#ifndef OVERWORLD_FOG_MAX_SLIDER
#define OVERWORLD_FOG_MAX_SLIDER 7  // [0 - 10]
#endif

#ifndef OVERWORLD_FOG_MIN_SLIDER
#define OVERWORLD_FOG_MIN_SLIDER 0  // [0 - 10]
#endif

//============================================================================//
// Cloud Settings (Reimagined Style)
//============================================================================//

#ifndef CLOUD_QUALITY
#define CLOUD_QUALITY 3              // [1 2 3] Cloud sampling quality
#endif

#ifndef CLOUD_ALT1
#define CLOUD_ALT1 192               // [-96 0 64 128 192 256 320 400 512 640 800] Primary cloud altitude
#endif

#ifndef CLOUD_STRETCH
#define CLOUD_STRETCH 4.2            // Cloud vertical stretch factor
#endif

#ifndef CLOUD_SPEED_MULT
#define CLOUD_SPEED_MULT 100         // [0 25 50 75 100 150 200 400 600 900] Cloud animation speed
#endif

#ifndef CLOUD_R
#define CLOUD_R 100                  // [25 50 75 100 125 150 200 250 300] Cloud red tint
#endif

#ifndef CLOUD_G
#define CLOUD_G 100                  // [25 50 75 100 125 150 200 250 300] Cloud green tint
#endif

#ifndef CLOUD_B
#define CLOUD_B 100                  // [25 50 75 100 125 150 200 250 300] Cloud blue tint
#endif

#ifndef DOUBLE_REIM_CLOUDS
#define DOUBLE_REIM_CLOUDS 0         // [0 1] Enable dual cloud layers
#endif

#ifndef CLOUD_ALT2
#define CLOUD_ALT2 256               // [-96 0 64 128 192 256 288 320 400 512 640 800] Secondary cloud altitude
#endif

// --- Cloud Shape ---

#ifndef CLOUD_NARROWNESS
#define CLOUD_NARROWNESS 0.07        // Cloud height-density falloff rate
#endif

#ifndef CLOUD_ROUNDNESS_SAMPLE
#define CLOUD_ROUNDNESS_SAMPLE 0.125 // Smoothstep roundness for cloud shape sampling
#endif

#ifndef CLOUD_ROUNDNESS_SHADOW
#define CLOUD_ROUNDNESS_SHADOW 0.35  // Smoothstep roundness for shadow sampling (blurrier)
#endif

// --- Cloud Shading ---

#ifndef CLOUD_SHADING_POWER
#define CLOUD_SHADING_POWER 2.5      // [1.0 1.5 2.0 2.5 3.0 3.5] Height gradient curve exponent (higher = wider dark bottom)
#endif

#ifndef CLOUD_SHADOW_STEP
#define CLOUD_SHADOW_STEP 1.0        // [0.3 0.5 1.0 1.5 2.0] Self-shadow sample step distance (world units)
#endif

#ifndef CLOUD_SHADOW_STRENGTH
#define CLOUD_SHADOW_STRENGTH 0.35   // [0.1 0.2 0.35 0.5 0.7] Self-shadow noise attenuation per sample
#endif

#ifndef CLOUD_SHADOW_MIN
#define CLOUD_SHADOW_MIN 0.3         // [0.1 0.2 0.3 0.4 0.5] Minimum self-shadow light (prevents full black)
#endif

#ifndef CLOUD_LIGHT_MIX_BASE
#define CLOUD_LIGHT_MIX_BASE 0.6     // [0.3 0.4 0.5 0.6 0.7 0.8] Self-shadow influence floor (higher = less shadow contrast)
#endif

#ifndef CLOUD_LIGHT_MIX_RANGE
#define CLOUD_LIGHT_MIX_RANGE 0.4    // [0.2 0.3 0.4 0.5 0.6] Self-shadow influence range (base+range = max multiplier)
#endif

// --- Cloud Colors (Night) ---

#ifndef CLOUD_NIGHT_AMBIENT
#define CLOUD_NIGHT_AMBIENT float3(0.09, 0.12, 0.17)
#endif

#ifndef CLOUD_NIGHT_AMBIENT_MULT
#define CLOUD_NIGHT_AMBIENT_MULT 1.4 // [0.5 1.0 1.4 2.0] Night ambient brightness multiplier
#endif

#ifndef CLOUD_NIGHT_LIGHT
#define CLOUD_NIGHT_LIGHT float3(0.11, 0.14, 0.20)
#endif

#ifndef CLOUD_NIGHT_LIGHT_MULT
#define CLOUD_NIGHT_LIGHT_MULT 0.9   // [0.5 0.7 0.9 1.2] Night moonlight brightness multiplier
#endif

//============================================================================//
// Volumetric Light Settings
//============================================================================//

#ifndef LIGHTSHAFT_QUALI
#define LIGHTSHAFT_QUALI 4           // [1 2 3 4] Volumetric light quality level
#endif

#ifndef VL_STRENGTH
#define VL_STRENGTH 0.5             // [0.25 0.5 0.75 1.0 1.25 1.5] Volumetric light intensity
#endif

#ifndef VL_SUNSET_COLOR_MULT
#define VL_SUNSET_COLOR_MULT 5.5    // [2.0 3.0 4.0 4.5 5.0 5.5 6.0 6.8 8.0] Sunset/sunrise VL color intensity
#endif

// VL sun angle gate: VL fades to zero when |SdotU| < deadzone,
// linearly ramps to full over the fade range above the deadzone.
// Wider deadzone = VL disappears earlier at low sun angles (avoids shadow map artifacts)
// Ref: CR composite1.glsl:39
#ifndef VL_SUN_DEADZONE
#define VL_SUN_DEADZONE 0.05        // [0.05 0.10 0.15 0.20 0.25] Min |SdotU| for VL to activate
#endif

#ifndef VL_SUN_FADE_RANGE
#define VL_SUN_FADE_RANGE 0.20      // [0.10 0.15 0.20 0.25 0.30] |SdotU| range over which VL ramps to full
#endif

//============================================================================//
// SSAO Settings (CR deferred1 style)
// Screen-space ambient occlusion via depth-based sampling.
// Combines multiplicatively with baked vertex AO from colortex0.a.
// Reference: ComplementaryReimagined deferred1.glsl DoAmbientOcclusion
//============================================================================//

#ifndef SSAO_QUALI
#define SSAO_QUALI 3                    // [0 2 3] 0=off, 2=fast(4 samples), 3=quality(12 samples)
#endif

#ifndef SSAO_I
#define SSAO_I 100                      // [0 25 50 75 100 125 150 200 250 300] Intensity (0=off, 100=default, 300=max)
#endif

// Derived: intensity power exponent (CR: SSAO_I * 0.004)
#define SSAO_I_FACTOR 0.004
#define SSAO_IM (SSAO_I * SSAO_I_FACTOR)

//============================================================================//
// Reflection Settings
//============================================================================//

#define MIN_REFLECTIVITY   0.01
#define WATER_REFLECTIVITY 0.99
#define GLASS_REFLECTIVITY 0.3

#ifndef SSR_MAX_STEPS
#define SSR_MAX_STEPS 38            // [1 - 512] Screen-space reflection steps (CR uses 38, increased for longer reach)
#endif

#ifndef SSR_STEP_SIZE
#define SSR_STEP_SIZE 1.0           // [0.1 - 2.0] SSR initial step multiplier (CR uses 1.0)
#endif

#ifndef SSR_BINARY_STEPS
#define SSR_BINARY_STEPS 10          // [1 - 10] Binary refinement steps (CR uses 10)
#endif

//============================================================================//
// Water Settings
//============================================================================//

#ifndef WATER_BRIGHTNESS
#define WATER_BRIGHTNESS 0.4
#endif

#ifndef WATER_WAVE_SIZE
#define WATER_WAVE_SIZE 1
#endif

#ifndef WATER_WAVE_SPEED
#define WATER_WAVE_SPEED 2
#endif

//============================================================================//
// Water Surface Settings (Reimagined Style)
// Reference: ComplementaryReimagined water.glsl + shaders.properties
// customImage3 = cloud-water.png (RG=water normals, B=cloud density, A=height)
// customImage4 = noise.png (CR noisetex equivalent, G=underwater VL noise)
//============================================================================//

#ifndef WATER_STYLE
#define WATER_STYLE 1                // [1 2] 1=Reimagined (stylized), 2=Reimagined+Waves
#endif

#ifndef WATER_BUMPINESS
#define WATER_BUMPINESS 80           // [0 20 40 60 80 100 120 150 200] Normal map intensity (higher = more visible waves)
#endif

#ifndef WATER_FOAM_I
#define WATER_FOAM_I 50              // [0 25 50 75 100 150 200] Shore foam intensity (0=disabled)
#endif

#ifndef WATER_FOG_MULT
#define WATER_FOG_MULT 100           // [25 50 75 100 150 200 300] Underwater fog density multiplier (%)
#endif

#ifndef WATER_ALPHA_MULT
#define WATER_ALPHA_MULT 100         // [50 75 100 125 150] Water transparency multiplier (lower = more transparent)
#endif

#ifndef WATER_REFRACTION_INTENSITY
#define WATER_REFRACTION_INTENSITY 100 // [0 25 50 75 100 150 200] Screen-space refraction distortion strength
#endif

#ifndef WATER_MAT_QUALITY
#define WATER_MAT_QUALITY 3          // [2 3] 2=multi-layer normals, 3=parallax mapping + normals
#endif

#ifndef WATER_REFLECT_QUALITY
#define WATER_REFLECT_QUALITY 2      // [0 1 2] 0=off, 1=sky fallback only, 2=SSR ray march + sky fallback
#endif

#ifndef WATER_CAUSTICS_STRENGTH
#define WATER_CAUSTICS_STRENGTH 1.0  // [0.0 0.25 0.5 0.75 1.0 1.5 2.0] Shadow-map caustic pattern intensity
#endif

#ifndef WATER_VL_STRENGTH
#define WATER_VL_STRENGTH 1.0        // [0.0 0.25 0.5 0.75 1.0 1.5 2.0] Underwater volumetric light intensity
#endif

//============================================================================//
// Water Depth Alpha Settings (CR waterFog formula)
// Controls how water transparency varies with depth-to-seabed distance.
// Shallow water is transparent (see underwater blocks), deep water is opaque.
// Formula: alpha *= WATER_DEPTH_ALPHA_MIN + (1 - WATER_DEPTH_ALPHA_MIN) * waterFog
//   where  waterFog = 1 - exp(-depthDiff * WATER_DEPTH_FOG_DENSITY)
// Reference: ComplementaryReimagined water.glsl lines 179-180
//============================================================================//

#ifndef WATER_DEPTH_FOG_DENSITY
#define WATER_DEPTH_FOG_DENSITY 0.075  // [0.025 0.05 0.075 0.1 0.15 0.2] Depth fog falloff rate (higher = opaque sooner)
#endif

#ifndef WATER_DEPTH_ALPHA_MIN
#define WATER_DEPTH_ALPHA_MIN 0.6     // [0.1 0.15 0.2 0.25 0.3 0.4 0.5] Minimum alpha at zero depth (shore transparency floor)
#endif

//============================================================================//
// Water Sky Reflection Colors (time-dependent)
// Interpolated by sunVisibility^2: night color at midnight, day color at noon.
// Used as fallback when SSR misses or WATER_REFLECT_QUALITY < 2.
//============================================================================//

#ifndef WATER_SKY_REFLECT_DAY
#define WATER_SKY_REFLECT_DAY float3(0.35, 0.55, 0.85)    // Daytime sky reflection (blue sky)
#endif

#ifndef WATER_SKY_REFLECT_NIGHT
#define WATER_SKY_REFLECT_NIGHT float3(0.02, 0.03, 0.06)  // Nighttime sky reflection (dark moonlit)
#endif

//============================================================================//
// Mirrored Image Reflection (Method 2 - CR reflections.glsl:185-233)
// Projects reflected direction to screen space and samples colortex0.
// Bridges the gap between SSR ray march (near) and sky fallback (far).
//============================================================================//

#ifndef MIRROR_VERTICAL_STRETCH
#define MIRROR_VERTICAL_STRETCH 0.013   // Vertical stretch for mirror projection (CR: 0.013)
#endif

#ifndef MIRROR_DISTANCE_FOG_POWER
#define MIRROR_DISTANCE_FOG_POWER 1.5   // [1.0 1.5 2.0 2.5] Distance fog curve exponent
#endif

#ifndef MIRROR_FOG_DENSITY
#define MIRROR_FOG_DENSITY 3.0          // [1.0 2.0 3.0 4.0 5.0] Distance fog strength (exp(-density * fog))
#endif

//============================================================================//
// Procedural Sky Reflection Colors (three-layer architecture)
// Used as Layer 3 fallback when both SSR and mirrored image fade out.
// Gradient from zenith to horizon with sunset warmth overlay.
//============================================================================//

#ifndef SKY_REFLECT_UP_DAY
#define SKY_REFLECT_UP_DAY float3(0.25, 0.45, 0.85)       // Zenith blue (day)
#endif

#ifndef SKY_REFLECT_UP_NIGHT
#define SKY_REFLECT_UP_NIGHT float3(0.01, 0.015, 0.04)    // Dark zenith (night)
#endif

#ifndef SKY_REFLECT_MID_DAY
#define SKY_REFLECT_MID_DAY float3(0.55, 0.65, 0.80)      // Horizon haze (day)
#endif

#ifndef SKY_REFLECT_MID_NIGHT
#define SKY_REFLECT_MID_NIGHT float3(0.03, 0.04, 0.07)    // Dark horizon (night)
#endif

#ifndef SKY_REFLECT_SUNSET
#define SKY_REFLECT_SUNSET float3(0.8, 0.45, 0.25)        // Sunset warm overlay
#endif

//============================================================================//
// Underwater Color Settings (time-dependent, used in composite1)
// Controls underwater tint and fog color, modulated by sunVisibility^2.
// Night multiplier darkens underwater scene; fog color shifts to deep blue.
//============================================================================//

#ifndef UNDERWATER_NIGHT_MULT
#define UNDERWATER_NIGHT_MULT 0.6      // [0.3 0.4 0.5 0.6 0.7 0.8 1.0] Night underwater brightness (fraction of day value)
#endif

#ifndef WATER_FOG_COLOR_DAY
#define WATER_FOG_COLOR_DAY float3(0.25, 0.40, 0.55)      // Daytime underwater fog (gamma space, pow(2.2) in composite1)
#endif

#ifndef WATER_FOG_COLOR_NIGHT
#define WATER_FOG_COLOR_NIGHT float3(0.08, 0.12, 0.20)    // Nighttime underwater fog (gamma space, pow(2.2) in composite1)
#endif

//============================================================================//
// Underwater Distance Fog (CR waterFog.glsl style)
// Applied in deferred1 to terrain pixels when camera is underwater.
// Formula: fog = dist / WATER_UW_FOG_DISTANCE; fog *= fog; 1 - exp(-fog)
// Squared exponential gives clear near-field and smooth distant fade.
// WATER_FOG_MULT (above) scales the input distance as percentage.
// Reference: ComplementaryReimagined lib/atmospherics/fog/waterFog.glsl
//============================================================================//

#ifndef WATER_UW_FOG_DISTANCE
#define WATER_UW_FOG_DISTANCE 32.0     // [16.0 24.0 32.0 48.0 64.0 96.0] Base fog distance (blocks, higher = clearer water)
#endif                                 // CR uses 48.0 with lightshafts active, 32.0 without

//============================================================================//
// Color Grading / Saturation (CR composite5 DoBSLColorSaturation)
// Applied after tonemap in composite5. Reimagined uses default 1.0 values
// which gives a subtle +7% saturation boost (saturationFactor = T_SATURATION + 0.07).
// Reference: ComplementaryReimagined composite5.glsl:89-104, common.glsl:269-270
//============================================================================//

#ifndef T_SATURATION
#define T_SATURATION 1.00           // [0.00 - 2.00] Color saturation (1.0 = CR default, +0.07 internal boost)
#endif

#ifndef T_VIBRANCE
#define T_VIBRANCE 1.00             // [0.00 - 2.00] Selective vibrance (boosts muted colors more)
#endif

//============================================================================//
// Computed Constants (static const for HLSL)
//============================================================================//

// Shadow distance squared (for distance fade calculations)
static const float SHADOW_MAX_DIST_SQUARED     = SHADOW_DISTANCE * SHADOW_DISTANCE;
static const float INV_SHADOW_MAX_DIST_SQUARED = 1.0 / SHADOW_MAX_DIST_SQUARED;

// Fog range
static const float OVERWORLD_FOG_MIN = 1.0 - 0.1 * OVERWORLD_FOG_MAX_SLIDER;
static const float OVERWORLD_FOG_MAX = 1.0 - 0.1 * OVERWORLD_FOG_MIN_SLIDER;

#endif // INCLUDE_SETTINGS_HLSL
