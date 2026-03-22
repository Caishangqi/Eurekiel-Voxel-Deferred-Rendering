/// Bloom tile atlas generation and reading utilities
/// Reference: ComplementaryReimagined composite4.glsl, composite5.glsl
///
/// Architecture:
///   composite2 (generation): BloomTile() reads colortex0 mipmaps, writes 7-LOD tile atlas to colortex3
///   composite5 (application): GetBloomTile() reads atlas from colortex3, DoBloom() composites onto scene
///
/// Tile Atlas Layout (normalized UV space):
///   LOD 2 (1/4):   offset (0.0,      0.0)
///   LOD 3 (1/8):   offset (0.0,      0.26)
///   LOD 4 (1/16):  offset (0.135,    0.26)
///   LOD 5 (1/32):  offset (0.2075,   0.26)
///   LOD 6 (1/64):  offset (0.135,    0.3325)
///   LOD 7 (1/128): offset (0.160625, 0.3325)
///   LOD 8 (1/256): offset (0.1784375,0.3325)
///
/// Gamma Encoding: composite2 stores pow(x/128, 0.25), composite5 decodes x^4 * 128
/// This preserves HDR range in RGBA8 colortex3.

#ifndef LIB_BLOOM_HLSL
#define LIB_BLOOM_HLSL

//============================================================================//
// Gaussian Kernel (Pascal's triangle row 6, sum = 64)
// 7x7 2D kernel: weight[i] * weight[j], total sum = 64 * 64 = 4096
//============================================================================//

static const float bloomWeight[7] = {1.0, 6.0, 15.0, 20.0, 15.0, 6.0, 1.0};

//============================================================================//
// Resolution Rescale
// CR normalizes tile layout to 1920x1080. For lower resolutions, scale UV
// so tiles don't overlap. For higher resolutions, no scaling needed.
//============================================================================//

float2 GetBloomRescale()
{
    return max(float2(viewWidth, viewHeight) / float2(1920.0, 1080.0), float2(1.0, 1.0));
}

//============================================================================//
// Bloom Tile Generation (composite2)
// Reads colortex0 with 7x7 Gaussian blur at the given LOD level.
// Hardware mipmaps provide implicit downsampling via UV scaling.
// Reference: CR composite4.glsl:31-50
//============================================================================//

float3 BloomTile(float lod, float2 offset, float2 scaledCoord)
{
    float3 bloom   = float3(0.0, 0.0, 0.0);
    float  scale   = exp2(lod);
    float2 coord   = (scaledCoord - offset) * scale;
    float  padding = 0.5 + 0.005 * scale;

    // Only process pixels within this tile's region
    if (abs(coord.x - 0.5) < padding && abs(coord.y - 0.5) < padding)
    {
        float2 pixelSize = 1.0 / float2(viewWidth, viewHeight);

        [unroll]
        for (int i = -3; i <= 3; i++)
        {
            [unroll]
            for (int j = -3; j <= 3; j++)
            {
                float  wg          = bloomWeight[i + 3] * bloomWeight[j + 3];
                float2 pixelOffset = float2(i, j) * pixelSize;
                float2 bloomCoord  = (scaledCoord - offset + pixelOffset) * scale;
                // SampleLevel mip 0: the 7x7 Gaussian kernel provides sufficient
                // low-pass filtering. Hardware mip selection (Sample) would give
                // slightly better quality at high LODs but requires mipmap generation
                // to run between composite1 and composite4.
                bloom += colortex0.SampleLevel(sampler0, bloomCoord, 0).rgb * wg;
            }
        }
        bloom /= 4096.0; // Normalize: 64 * 64
    }

    // Gamma encode for RGBA8 storage: pow(x/128, 0.25)
    return pow(max(bloom / 128.0, 0.0), 0.25);
}

//============================================================================//
// Bloom Tile Reading (composite5)
// Reads a single tile from the bloom atlas in colortex3.
// Decodes gamma encoding: x^4 * 128 (inverse of pow(x/128, 0.25))
// Reference: CR composite5.glsl:108-117
//============================================================================//

float3 GetBloomTile(float lod, float2 coord, float2 offset, float2 rescale)
{
    float  scale      = exp2(lod);
    float2 bloomCoord = coord / scale + offset;
    bloomCoord        = clamp(bloomCoord, offset, 1.0 / scale + offset);

    float3 bloom = colortex3.Sample(sampler0, bloomCoord / rescale).rgb;
    bloom        *= bloom; // x^2
    bloom        *= bloom; // x^4
    return bloom * 128.0; // Undo the /128 from generation
}

//============================================================================//
// Bloom Application (composite5)
// Reads all 7 LOD tiles, averages them, and blends onto scene color.
// Reference: CR composite5.glsl:119-144
//============================================================================//

void DoBloom(inout float3 color, float2 coord)
{
    float2 rescale = GetBloomRescale();

    float3 blur1 = GetBloomTile(2.0, coord, float2(0.0, 0.0), rescale);
    float3 blur2 = GetBloomTile(3.0, coord, float2(0.0, 0.26), rescale);
    float3 blur3 = GetBloomTile(4.0, coord, float2(0.135, 0.26), rescale);
    float3 blur4 = GetBloomTile(5.0, coord, float2(0.2075, 0.26), rescale);
    float3 blur5 = GetBloomTile(6.0, coord, float2(0.135, 0.3325), rescale);
    float3 blur6 = GetBloomTile(7.0, coord, float2(0.160625, 0.3325), rescale);
    float3 blur7 = GetBloomTile(8.0, coord, float2(0.1784375, 0.3325), rescale);

    // Average all 7 LOD levels (1/7 ~ 0.14)
    float3 blur = (blur1 + blur2 + blur3 + blur4 + blur5 + blur6 + blur7) * 0.14;

    // Bloom strength with darkness boost (CR: + 0.2 * darknessFactor)
    float bloomStrength = BLOOM_STRENGTH + 0.2 * darknessFactor;

    color = lerp(color, blur, bloomStrength);
}

#endif // LIB_BLOOM_HLSL
