/// Composite pass 5 — HDR Tonemapping (Lottes 2016)
/// Converts HDR scene color (colortex0, R16F) to LDR for final output.
/// Reference: CR composite5.glsl:36-87, Lottes GDC 2016
///
/// Pipeline: composite1 (HDR + VL) → ... → composite5 (tonemap) → final
/// Output: colortex0 (tonemapped LDR)
#include "../@engine/core/core.hlsl"
#include "../lib/common.hlsl"
#include "../include/settings.hlsl"

//============================================================================//
// Tonemap Parameters (CR defaults)
// Ref: CR common.glsl:267-272
//============================================================================//

#ifndef TM_EXPOSURE
#define TM_EXPOSURE 1.0        // [0.40 - 2.80] Overall brightness
#endif

#ifndef TM_CONTRAST
#define TM_CONTRAST 1.05        // [0.50 - 2.00] Lottes curve contrast
#endif

#ifndef TM_WHITE_PATH
#define TM_WHITE_PATH 1.00      // [0.10 - 1.90] Highlight → white compression
#endif

#ifndef TM_DARK_DESATURATION
#define TM_DARK_DESATURATION 0.25 // [0.00 - 1.00] Dark color desaturation
#endif

//============================================================================//
// sRGB Gamma (Linear → sRGB)
// Ref: CR composite5.glsl:31-34
//============================================================================//

float3 LinearToSRGB(float3 color)
{
    // IEC 61966-2-1 sRGB transfer function
    float3 lo = 12.92 * color;
    float3 hi = (1.0 + 0.055) * pow(max(color, 0.0), 1.0 / 2.4) - 0.055;
    return float3(
        color.r < 0.0031308 ? lo.r : hi.r,
        color.g < 0.0031308 ? lo.g : hi.g,
        color.b < 0.0031308 ? lo.b : hi.b);
}

//============================================================================//
// Lottes Tonemap (CR composite5.glsl:36-87)
// "Advanced Techniques and Optimization of HDR Color Pipelines"
// Timothy Lottes, GDC 2016
//============================================================================//

float3 LottesTonemap(float3 color)
{
    // Step 1: Apply exposure
    color = TM_EXPOSURE * color;

    float initialLuminance = Luma(color);

    // Step 2: Lottes curve parameters
    float3 a      = float3(TM_CONTRAST, TM_CONTRAST, TM_CONTRAST);
    float3 d      = float3(1.0, 1.0, 1.0); // Roll-off control
    float3 hdrMax = float3(8.0, 8.0, 8.0); // Max input brightness
    float3 midIn  = float3(0.25, 0.25, 0.25); // Input middle gray
    float3 midOut = float3(0.25, 0.25, 0.25); // Output middle gray

    // Precompute coefficients
    float3 a_d      = a * d;
    float3 hdrMaxA  = pow(hdrMax, a);
    float3 hdrMaxAD = pow(hdrMax, a_d);
    float3 midInA   = pow(midIn, a);
    float3 midInAD  = pow(midIn, a_d);
    float3 HM1      = hdrMaxA * midOut;
    float3 HM2      = hdrMaxAD - midInAD;

    float3 b = (-midInA + HM1) / (HM2 * midOut);
    float3 c = (hdrMaxAD * midInA - HM1 * midInAD) / (HM2 * midOut);

    // Step 3: Apply Lottes curve
    float3 colorOut = pow(max(color, 0.0), a) / (pow(max(color, 0.0), a_d) * b + c);

    // Step 4: Linear → sRGB gamma
    colorOut = LinearToSRGB(colorOut);

    // Step 5: Dark lift — preserve dark color readability
    // Ref: CR composite5.glsl:72-76
    const float darkLiftStart = 0.1;
    const float darkLiftMix   = 0.75;
    float       darkLift      = smoothstep(darkLiftStart, 0.0, initialLuminance);
    float3      smoothColor   = pow(max(color, 0.0), 1.0 / 2.2);
    float       contrastAdj   = max(0.0, 0.55 - abs(1.05 - TM_CONTRAST)) / 0.55;
    colorOut                  = lerp(colorOut, smoothColor, darkLift * darkLiftMix * contrastAdj);

    // Step 6: White path — smooth highlight compression to white
    // Ref: CR composite5.glsl:79-82
    const float wpInputCurveMax   = 16.0;
    float       modifiedLuminance = pow(initialLuminance / wpInputCurveMax, 2.0 - TM_WHITE_PATH) * wpInputCurveMax;
    float       whitePath         = smoothstep(0.0, wpInputCurveMax, modifiedLuminance);
    colorOut                      = lerp(colorOut, float3(1.0, 1.0, 1.0), whitePath);

    // Step 7: Dark desaturation
    // Ref: CR composite5.glsl:85-86
    float desaturatePath = smoothstep(0.1, 0.0, initialLuminance);
    float outLuma        = Luma(colorOut);
    colorOut             = lerp(colorOut, float3(outLuma, outLuma, outLuma), desaturatePath * TM_DARK_DESATURATION);

    return saturate(colorOut);
}

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

    output.color0 = float4(ldrColor, 1.0);
    return output;
}
