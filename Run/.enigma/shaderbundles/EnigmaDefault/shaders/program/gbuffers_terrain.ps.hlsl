/**
 * @file gbuffers_terrain.ps.hlsl
 * @brief Terrain Pixel Shader - G-Buffer Output
 *
 * [REFACTOR] Uses R8G8B8A8_SNORM for normals - no encoding needed
 *
 * G-Buffer Output Layout:
 * - colortex0 (SV_TARGET0): Albedo RGB + AO in alpha
 * - colortex1 (SV_TARGET1): Lightmap (R=blocklight, G=skylight)
 * - colortex2 (SV_TARGET2): Normal (SNORM, [-1,1] stored directly)
 */

#include "../@engine/core/core.hlsl"

// [RENDERTARGETS] 0,1,2
// Output: colortex0 (Albedo), colortex1 (Lightmap), colortex2 (Normal)

// ============================================================================
// Terrain-Specific Pixel Shader Structures
// ============================================================================

/**
 * @brief Pixel shader input - matches VSOutput_Terrain
 */
struct PSInput_Terrain
{
    float4 Position : SV_POSITION; // Clip position (system use)
    float4 Color : COLOR0; // Vertex color
    float2 TexCoord : TEXCOORD0; // UV coordinates
    float3 Normal : NORMAL; // World normal
    float2 LightmapCoord: LIGHTMAP; // Lightmap (blocklight, skylight)
    float3 WorldPos : TEXCOORD2; // World position
    uint   entityId : TEXCOORD5; // Block entity ID (unused)
    float2 midTexCoord : TEXCOORD6; // Texture center (unused)
};

/**
 * @brief Pixel shader output - G-Buffer targets
 */
struct PSOutput_Terrain
{
    float4 Color0 : SV_TARGET0; // colortex0: Albedo (RGB) + AO (A)
    float4 Color1 : SV_TARGET1; // colortex1: Lightmap (R=block, G=sky)
    float4 Color2 : SV_TARGET2; // colortex2: Normal (SNORM, direct [-1,1])
};

/**
 * @brief Terrain pixel shader main entry
 */
PSOutput_Terrain main(PSInput_Terrain input)
{
    PSOutput_Terrain output;

    // [STEP 1] Sample terrain atlas (customImage0 = gtexture)
    Texture2D gtexture = GetCustomImage(0);
    float4    texColor = gtexture.Sample(sampler1, input.TexCoord);

    // [STEP 2] Alpha test - discard transparent pixels
    if (texColor.a < 0.1)
    {
        discard;
    }

    // [STEP 3] Calculate albedo (texture * vertex color RGB)
    // [AO] In separateAo mode, Color.a contains AO value, not alpha
    // RGB = directional shade, A = AO
    float3 albedoRGB = texColor.rgb * input.Color.rgb;
    float  ao        = input.Color.a; // AO from vertex color alpha

    // TODO: Should happen in final or composite pass
    // See https://shaders.properties/current/guides/your-first-shaderpack/3_deferred_lighting/, the deferred light equation should be
    // color.rgb *= blocklight + skylight + ambient + sunlight;

    // [STEP 6] Write G-Buffer outputs
    // colortex0: Albedo RGB + AO in alpha channel (for composite pass)
    output.Color0 = float4(albedoRGB, ao);

    // colortex1: Lightmap data (R=blocklight, G=skylight, B=0, A=1)
    output.Color1 = float4(input.LightmapCoord.x, input.LightmapCoord.y, 0.0, 1.0);

    // colortex2: World normal (SNORM format handles [-1,1] natively)
    output.Color2 = float4(normalize(input.Normal), 1.0);

    return output;
}
