/// Composite pass 1 vertex shader — full-screen quad passthrough
/// Reuses the same pattern as composite.vs.hlsl / deferred1.vs.hlsl
#include "../@engine/core/core.hlsl"

VSOutput main(VSInput input)
{
    VSOutput output;

    float4 localPos = float4(input.Position, 1.0);

    output.Position = localPos;
    output.WorldPos = localPos.xyz;
    output.Color    = input.Color;
    output.TexCoord = input.TexCoord;

    output.Normal    = normalize(mul(normalMatrix, float4(input.Normal, 0.0)).xyz);
    output.Tangent   = normalize(mul(gbufferView, float4(input.Tangent, 0.0)).xyz);
    output.Bitangent = normalize(mul(gbufferView, float4(input.Bitangent, 0.0)).xyz);

    return output;
}
