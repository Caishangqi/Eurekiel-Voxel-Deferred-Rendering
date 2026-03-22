/// Composite pass 4 — Bloom Tile Atlas Generation
/// Generates 7-level downsampled + Gaussian-blurred bloom tiles into colortex3.
/// Reads colortex0 (HDR scene with hardware mipmaps enabled).
/// Reference: CR composite4.glsl
///
/// Pipeline: composite1 (VL + underwater) -> composite4 (bloom gen) -> composite5 (bloom apply + tonemap)
/// Output: colortex3 (bloom tile atlas, RGBA8, gamma-encoded)
///
/// Tile Atlas Layout:
///   LOD 2 (1/4):   top-left
///   LOD 3 (1/8):   below LOD2
///   LOD 4-5:       right of LOD3
///   LOD 6-8:       below LOD4
#include "../@engine/core/core.hlsl"
#include "../include/settings.hlsl"
#include "../lib/common.hlsl"
#include "../lib/bloom.hlsl"

//============================================================================//
// Main
//============================================================================//

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex3 (bloom tile atlas)
};

/* RENDERTARGETS: 3 */

PSOutput main(PSInput input)
{
    PSOutput output;

    float3 blur = float3(0.0, 0.0, 0.0);

#if BLOOM_ENABLED == 1
    // Scale UV for sub-1080p resolution handling
    float2 scaledCoord = input.TexCoord * GetBloomRescale();

    // Generate 7 bloom tiles at LOD 2-8
    // LOD weights: higher LODs get reduced weight to prevent over-blur
    // Reference: CR composite4.glsl:65-84 (Overworld path)
    blur += BloomTile(2.0, float2(0.0, 0.0), scaledCoord);
    blur += BloomTile(3.0, float2(0.0, 0.26), scaledCoord);
    blur += BloomTile(4.0, float2(0.135, 0.26), scaledCoord);
    blur += BloomTile(5.0, float2(0.2075, 0.26), scaledCoord) * 0.8;
    blur += BloomTile(6.0, float2(0.135, 0.3325), scaledCoord) * 0.8;
    blur += BloomTile(7.0, float2(0.160625, 0.3325), scaledCoord) * 0.6;
    blur += BloomTile(8.0, float2(0.1784375, 0.3325), scaledCoord) * 0.4;
#endif

    output.color0 = float4(blur, 1.0);
    return output;
}
