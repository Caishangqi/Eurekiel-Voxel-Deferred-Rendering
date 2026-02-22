/**
 * @file gbuffers_skytextured.ps.hlsl
 * @brief Sun/Moon Texture Rendering - Pixel Shader
 * @date 2025-12-08
 *
 * Renders sun/moon celestial textures with additive blending.
 *
 * [FIX] Alpha=0 output: colortex0 is R16G16B16A16_FLOAT, additive blending
 * would accumulate alpha (sky 1.0 + sun 1.0 = 2.0) creating a visible quad
 * rectangle when viewed through water (deferred1 reads alpha as AO).
 * Output alpha=0 so additive blend preserves the existing sky alpha.
 *
 * [FIX] Underwater discard: when isEyeInWater==1, celestial bodies should
 * not be visible (CR gbuffers_skytextured.glsl:98, composite1.glsl:241).
 *
 * Reference:
 * - Minecraft position_tex.fsh
 * - CR gbuffers_skytextured.glsl:98 (isEyeInWater dimming)
 */

#include "../@engine/core/core.hlsl"
// [RENDERTARGETS] 0
// Output to colortex0 (sky color with sun/moon)

/**
 * @brief Pixel Shader Output Structure
 */
struct PSOutput
{
    float4 Color0 : SV_Target0; // colortex0 (sun/moon texture)
};

/**
 * @brief Pixel Shader Main Entry
 * @param input Interpolated vertex data from VS
 * @return PSOutput Sun/moon color (alpha=0 for safe additive blending)
 */
PSOutput main(PSInput input)
{
    PSOutput output;

    // [FIX] Underwater: discard all celestial pixels
    // When submerged, sun/moon are replaced by fogColor in deferred1 sky branch.
    // But water surface geometry (depth < 1.0) bypasses that branch, so we must
    // prevent celestial color from entering colortex0 entirely.
    // Ref: CR gbuffers_skytextured.glsl:98
    if (isEyeInWater == EYE_IN_WATER)
    {
        discard;
    }

    // [STEP 1] Sample sun/moon texture from customImage0
    Texture2D celestialTexture = GetCustomImage(0);
    float4    texColor         = celestialTexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Discard fully transparent pixels (Minecraft style)
    if (texColor.a < 0.01)
    {
        discard;
    }

    // [STEP 3] Apply ColorModulator (brightness control)
    float4 finalColor = texColor * colorModulator;

    // [STEP 4] Output RGB with alpha=0
    // Additive blend: dest = src + dest
    // With src.a=0: dest.a = 0 + sky_a = sky_a (unchanged)
    // Prevents alpha accumulation in R16G16B16A16_FLOAT colortex0,
    // which deferred1 reads as AO — alpha mismatch would reveal the quad.
    output.Color0 = float4(finalColor.rgb, 0.0);

    return output;
}
