/// Composite pass 5 — HDR Tonemapping (Lottes 2016)
/// Converts HDR scene color (colortex0, R16F) to LDR for final output.
/// Reference: CR composite5.glsl:36-87, Lottes GDC 2016
///
/// Pipeline: composite1 (HDR + VL) → ... → composite5 (tonemap) → final
/// Output: colortex0 (tonemapped LDR)
#include "../@engine/core/core.hlsl"
#include "../lib/common.hlsl"
#include "../include/settings.hlsl"
#include "../lib/tonemap.hlsl"

//============================================================================//
// Main
//============================================================================//

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex0 (tonemapped LDR)
};

/* RENDERTARGETS: 0 */

PSOutput main(PSInput input)
{
    PSOutput output;

    // Read HDR scene color
    float3 hdrColor = colortex0.Sample(sampler0, input.TexCoord).rgb;

    // Apply Lottes tonemap: HDR → LDR
    float3 ldrColor = LottesTonemap(hdrColor);

    // Apply color saturation/vibrance (CR DoBSLColorSaturation)
    ldrColor = ApplyColorSaturation(ldrColor);

    output.color0 = float4(ldrColor, 1.0);
    return output;
}
