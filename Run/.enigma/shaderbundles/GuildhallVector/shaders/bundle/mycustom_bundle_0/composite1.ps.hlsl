#include "../../@engine/core/core.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0; // colortex4
};

float NDCDepthToViewDepth(float ndcDepth, float near, float far)
{
    return (near * far) / (far - ndcDepth * (far - near));
}

PSOutput main(PSInput input)
{
    /* RENDERTARGETS: 0 */
    PSOutput output;

    // [STEP 1] Sample depth textures
    // [IMPORTANT] Use sampler1 (Point filtering) for depth textures!
    // Depth values must NOT be interpolated - use Point sampling only.
    // sampler0 = Linear (for color textures)
    // sampler1 = Point (for depth textures)
    //
    // [FIX] Correct depth texture semantics based on actual pipeline:
    // - depthtex0: Main depth buffer, written by ALL passes (opaque + translucent)
    // - depthtex1: Snapshot copied BEFORE translucent pass (opaque only)
    //
    // Pipeline flow:
    //   TerrainSolid   -> writes depthtex0
    //   TerrainCutout  -> writes depthtex0, then Copy(0,1) in EndPass
    //   TerrainTranslucent -> writes depthtex0 (now includes water)
    //   Composite      -> reads both for depth comparison
    float depthOpaque = depthtex1.Sample(sampler1, input.TexCoord).r; // Opaque only (snapshot before translucent)
    float depthAll    = depthtex0.Sample(sampler1, input.TexCoord).r; // All objects (main depth buffer)

    float linearDepthOpaque = NDCDepthToViewDepth(depthOpaque, 0.001, 1);
    float linearDepthAll    = NDCDepthToViewDepth(depthAll, 0.001, 1);
    output.color0           = float4(linearDepthOpaque, linearDepthAll, 0.0, 1.0);
    return output;
}
