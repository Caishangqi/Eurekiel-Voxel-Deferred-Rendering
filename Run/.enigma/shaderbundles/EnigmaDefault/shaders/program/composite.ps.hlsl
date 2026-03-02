/// Composite pass — reserved for future opaque surface reflections (metals, ice)
/// Water SSR is now computed inline in gbuffers_water.ps.hlsl (CR architecture)
///
/// Pipeline: deferred1 -> gbuffers_water (SSR inline) -> composite -> composite1 (VL) -> composite5 (tonemap)
/// Output: colortex7 (zeroed — no opaque reflections yet)
/// Reference: CR composite.glsl (DRAWBUFFERS:7)
#include "../@engine/core/core.hlsl"

struct PSOutput
{
    float4 color7 : SV_Target0; // colortex7: reflection (reserved for future opaque reflections)
};

PSOutput main(PSInput input)
{
    PSOutput output;

    /* RENDERTARGETS: 7 */

    // No opaque surface reflections implemented yet
    // Future: read colortex4/colortex6 material data for metals/ice and compute SSR here
    output.color7 = float4(0.0, 0.0, 0.0, 0.0);
    return output;
}
