/// [NEW] RenderTarget Format Definitions for EnigmaDefault ShaderBundle
/// Reference: flexible-deferred-rendering-feature-rendertarget-format-refactor
///
/// Include this file in ONE shader only (e.g., composite.ps.hlsl).
/// The engine parses all shaders globally to collect directives.

#ifndef INCLUDE_RT_FORMATS_HLSL
#define INCLUDE_RT_FORMATS_HLSL

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
// Clear Control Directives
//============================================================================//
const bool colortex0Clear = true;
const bool colortex1Clear = true;
const bool colortex2Clear = true;
const bool colortex3Clear = true;
const bool colortex4Clear = true;
const bool colortex5Clear = true;
const bool colortex6Clear = false;
const bool colortex7Clear = false;

//============================================================================//
// Clear Color Directives
//============================================================================//
const float4 colortex0ClearColor = float4(0.0, 0.0, 0.0, 1.0);
const float4 colortex1ClearColor = float4(0.0, 0.0, 1.0, 1.0);
const float4 colortex2ClearColor = float4(0.0, 0.0, 1.0, 1.0);
const float4 colortex3ClearColor = float4(0.0, 0.0, 0.0, 0.0);
const float4 colortex4ClearColor = float4(0.0, 0.0, 0.0, 0.0);
const float4 colortex5ClearColor = float4(0.0, 0.0, 0.0, 0.0);

#endif // INCLUDE_RT_FORMATS_HLSL
