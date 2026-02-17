/// [NEW] Centralized shader configuration for EnigmaDefault ShaderBundle
/// Reference: miniature-shader/shaders/shader.h

#ifndef INCLUDE_SETTINGS_HLSL
#define INCLUDE_SETTINGS_HLSL

//============================================================================//
// Celestial Settings
//============================================================================//

const float sunPathRotation = -30.0; //[-90.0 -80.0 -70.0 -60.0 -50.0 -40.0 -30.0 -20.0 -10.0 0.0 10.0 20.0 30.0 40.0 50.0 60.0 70.0 80.0 90.0]

//============================================================================//
// Shadow Settings
//============================================================================//

#ifndef SHADOW_DARKNESS
#define SHADOW_DARKNESS 0.2         // [0.0 - 1.0] Shadow darkness level
#endif

#ifndef SHADOW_BLUENESS
#define SHADOW_BLUENESS 0.3         // [0.0 - 0.5] Shadow blue tint
#endif

#ifndef SHADOW_PIXEL
#define SHADOW_PIXEL 0             // [0 - 2048] Pixelated shadow resolution (0 = smooth)
#endif

#ifndef SHADOW_MAP_RESOLUTION
#define SHADOW_MAP_RESOLUTION 2048  // [256 - 4096] Shadow map resolution
#endif

#ifndef SHADOW_DISTANCE
#define SHADOW_DISTANCE 128.0       // [8.0 - 1024.0] Shadow render distance
#endif

#ifndef SHADOW_INTERVAL_SIZE
#define SHADOW_INTERVAL_SIZE 7.0    // Shadow cascade interval
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
// Light Settings
//============================================================================//

#ifndef LIGHT_BRIGHTNESS
#define LIGHT_BRIGHTNESS 0.9        // [0.0 - 2.0] Light intensity multiplier
#endif

// Torch colors (inner/middle/outer gradient)
#ifndef TORCH_INNER_COLOR
#define TORCH_INNER_COLOR float3(1.0, 0.85, 0.7)
#endif

#ifndef TORCH_MIDDLE_COLOR
#define TORCH_MIDDLE_COLOR float3(1.0, 0.8, 0.6)
#endif

#ifndef TORCH_OUTER_COLOR
#define TORCH_OUTER_COLOR float3(1.0, 0.55, 0.2)
#endif

#ifndef MOON_COLOR
#define MOON_COLOR float3(0.1, 0.15, 0.3)
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

//============================================================================//
// Volumetric Light Settings
//============================================================================//

#ifndef LIGHTSHAFT_QUALI
#define LIGHTSHAFT_QUALI 4           // [1 2 3 4] Volumetric light quality level
#endif

#ifndef VL_STRENGTH
#define VL_STRENGTH 2.0              // [0.5 0.75 1.0 1.25 1.5 2.0] Volumetric light intensity
#endif

//============================================================================//
// Reflection Settings
//============================================================================//

#define MIN_REFLECTIVITY   0.01
#define WATER_REFLECTIVITY 0.99
#define GLASS_REFLECTIVITY 0.3

#ifndef SSR_MAX_STEPS
#define SSR_MAX_STEPS 10            // [1 - 512] Screen-space reflection steps
#endif

#ifndef SSR_STEP_SIZE
#define SSR_STEP_SIZE 1.6           // [0.1 - 2.0] SSR step size
#endif

#ifndef SSR_BINARY_STEPS
#define SSR_BINARY_STEPS 4          // [1 - 10] Binary refinement steps
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
// Computed Constants (static const for HLSL)
//============================================================================//

// Shadow color with blue tint
static const float3 SHADOW_COLOR = float3(1.0 - SHADOW_DARKNESS, 1.0 - SHADOW_DARKNESS, 1.0 - SHADOW_DARKNESS)
    + float3(0.0, 0.3333, 1.0) * SHADOW_BLUENESS;

// Shadow distance squared (for distance fade calculations)
static const float SHADOW_MAX_DIST_SQUARED     = SHADOW_DISTANCE * SHADOW_DISTANCE;
static const float INV_SHADOW_MAX_DIST_SQUARED = 1.0 / SHADOW_MAX_DIST_SQUARED;

// Fog range
static const float OVERWORLD_FOG_MIN = 1.0 - 0.1 * OVERWORLD_FOG_MAX_SLIDER;
static const float OVERWORLD_FOG_MAX = 1.0 - 0.1 * OVERWORLD_FOG_MIN_SLIDER;

// Lightmap UV constants
static const float2 AMBIENT_UV     = float2(8.0 / 255.0, 247.0 / 255.0);
static const float2 TORCH_UV_SCALE = float2(8.0 / 255.0, 231.0 / 255.0);

#endif // INCLUDE_SETTINGS_HLSL
