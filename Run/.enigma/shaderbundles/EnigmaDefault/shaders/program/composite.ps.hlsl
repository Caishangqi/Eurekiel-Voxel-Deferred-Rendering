/// Composite pass — passthrough (placeholder for Phase 5 reflections/SSR)
/// Lighting, atmospheric fog, clouds, and fluid fog migrated to deferred1.ps.hlsl
/// Reference: CR composite.glsl (PBR reflections), design R4.6.1
#include "../@engine/core/core.hlsl"

struct PSOutput
{
    float4 color0 : SV_Target0;
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 0 */

    // Pass through deferred output unchanged
    output.color0 = colortex0.Sample(sampler0, input.TexCoord);

    return output;
}
