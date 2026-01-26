#include "../@engine/core/core.hlsl"

// Final should not have vertex shader
VSOutput main(VSInput input)
{
    VSOutput output;
    output.Bitangent = input.Bitangent;
    output.Color     = input.Color;
    output.Normal    = input.Normal;
    output.Position  = float4(input.Position, 1);
    output.Tangent   = input.Tangent;
    output.TexCoord  = input.TexCoord;
    output.WorldPos  = input.Position;
    return output;
}
