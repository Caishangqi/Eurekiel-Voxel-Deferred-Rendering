/// Final pass — screen output with underwater distortion
/// Applies screen-space sinusoidal UV warp when camera is submerged.
/// Reference: CR final.glsl lines 89-92
///
/// Pipeline: composite5 (bloom + tonemap) -> final (underwater distortion + output)
/// Output: colortex0 (final screen color)
#include "../@engine/core/core.hlsl"
#include "../include/settings.hlsl"
#include "../lib/underwaterDistortion.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // Apply underwater screen distortion (sinusoidal UV warp)
    float2 texCoordM = ApplyUnderwaterDistortion(input.TexCoord, frameTimeCounter);

    // Sample final composited scene (use linearClampSampler to prevent edge wrapping)
    output.color0 = colortex0.SampleLevel(linearClampSampler, texCoordM, 0);
    return output;
}
