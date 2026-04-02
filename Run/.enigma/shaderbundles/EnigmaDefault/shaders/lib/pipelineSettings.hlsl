/// Pipeline Settings for EnigmaDefault ShaderBundle
/// RT format, clear, mipmap directives (Iris-compatible const directive pattern)
/// Parsed by PackRenderTargetDirectives::AcceptDirectives()
///
/// Reference: Complementary Reimagined shaders/lib/pipelineSettings.glsl
/// Migrated from: include/rt_formats.hlsl

#ifndef LIB_PIPELINE_SETTINGS_HLSL
#define LIB_PIPELINE_SETTINGS_HLSL

//============================================================================//
// RT Format Directives (parsed by PackRenderTargetDirectives)
// [IMPORTANT] Format directives MUST be inside /* */ comment blocks
//============================================================================//

/*
const int colortex0Format = R16G16B16A16_FLOAT;
const int colortex1Format = R8G8B8A8_UNORM;
const int colortex2Format = R8G8B8A8_SNORM;
const int colortex3Format = R8G8B8A8_UNORM;
const int colortex4Format = R8G8B8A8_UNORM;
const int colortex5Format = R8G8B8A8_UNORM;
const int colortex6Format = R16G16B16A16_FLOAT;
const int colortex7Format = R16G16B16A16_FLOAT;
*/

//============================================================================//
// Mipmap Directives (parsed by PackRenderTargetDirectives)
// Enable hardware mipmap generation for specific colortex.
// Required for Bloom: composite2 reads colortex0 mipmaps for downsampling.
//============================================================================//
const bool colortex0MipmapEnabled = true;

//============================================================================//
// colortex Clear Control Directives
// true  = clear each frame (LoadAction::Clear)
// false = preserve previous content (LoadAction::Load)
//============================================================================//
const bool colortex0Clear  = true; // Main color - clear each frame
const bool colortex1Clear  = false; // GBuffer normal - overwritten by terrain
const bool colortex2Clear  = false; // GBuffer material - overwritten by terrain
const bool colortex3Clear  = true; // Cloud/bloom data - clear each frame
const bool colortex4Clear  = false; // Water data - written per-pass
const bool colortex5Clear  = false; // VL factor - written by deferred1
const bool colortex6Clear  = false; // Unused
const bool colortex7Clear  = false; // Sky/SSR temporal - preserve across frames
const bool colortex8Clear  = false; // Unused
const bool colortex9Clear  = false; // Unused
const bool colortex10Clear = false; // Unused
const bool colortex11Clear = false; // Unused
const bool colortex12Clear = false; // Unused
const bool colortex13Clear = false; // Unused
const bool colortex14Clear = false; // Unused
const bool colortex15Clear = false; // Unused

//============================================================================//
// colortex Clear Color Directives
//============================================================================//
const float4 colortex0ClearColor = float4(0.0, 0.0, 0.0, 1.0);
const float4 colortex1ClearColor = float4(0.0, 0.0, 1.0, 1.0);
const float4 colortex2ClearColor = float4(0.0, 0.0, 1.0, 1.0);
const float4 colortex3ClearColor = float4(0.0, 0.0, 0.0, 0.0);
const float4 colortex4ClearColor = float4(0.0, 0.0, 0.0, 0.0);
const float4 colortex5ClearColor = float4(0.0, 0.0, 0.0, 0.0);

//============================================================================//
// ShadowColor RT Configuration (Phase 5: Water Caustics + Underwater VL)
//============================================================================//

// Format Directives (parsed by engine)
/*
const int shadowcolor0Format = R8G8B8A8_UNORM;
const int shadowcolor1Format = R8G8B8A8_UNORM;
*/

// shadowcolor Clear Control
const bool shadowcolor0Clear = true; // Caustic pattern - clear each frame
const bool shadowcolor1Clear = true; // Underwater VL - clear each frame
const bool shadowcolor2Clear = false; // Unused
const bool shadowcolor3Clear = false; // Unused
const bool shadowcolor4Clear = false; // Unused
const bool shadowcolor5Clear = false; // Unused
const bool shadowcolor6Clear = false; // Unused
const bool shadowcolor7Clear = false; // Unused

// shadowcolor Clear Color
// shadowcolor0 = white: caustic pattern default no-effect (multiplicative blend, white = no change)
// shadowcolor1 = black: underwater VL default no-light (additive blend, black = no contribution)
const float4 shadowcolor0ClearColor = float4(1.0, 1.0, 1.0, 1.0);
const float4 shadowcolor1ClearColor = float4(0.0, 0.0, 0.0, 1.0);

#endif // LIB_PIPELINE_SETTINGS_HLSL
