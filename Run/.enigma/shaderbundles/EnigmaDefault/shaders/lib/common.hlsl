/// [NEW] Common utility functions for EnigmaDefault ShaderBundle
/// Ported from miniature-shader/shaders/common/math.glsl

#ifndef LIB_COMMON_HLSL
#define LIB_COMMON_HLSL

// Pseudo-random number generators
float Random(float2 v)
{
    return frac(sin(dot(v, float2(18.9898, 28.633))) * 4378.5453);
}

float Random(float3 v)
{
    return frac(sin(dot(v, float3(12.9898, 78.233, 45.164))) * 43758.5453);
}

float3 Random3(float3 v)
{
    v = frac(v * float3(0.3183099, 0.3678794, 0.7071068)); // 1/π, 1/e, 1/√2
    v += dot(v, v.yzx + 19.19);
    return frac(float3(v.x * v.y * 95.4307,
                       v.y * v.z * 97.5901,
                       v.z * v.x * 93.8365)) - 0.5;
}

// Luminance calculation (Rec. 601)
float Luma(float3 color)
{
    return dot(float3(0.299, 0.587, 0.114), color);
}

// Remap value to [0,1] range
float Rescale(float x, float minVal, float maxVal)
{
    return saturate((x - minVal) / (maxVal - minVal));
}

float3 Rescale3(float3 x, float3 minVal, float3 maxVal)
{
    return saturate((x - minVal) / (maxVal - minVal));
}

// Squared length for distance comparisons (avoids sqrt)
float SquaredLength(float3 v)
{
    return dot(v, v);
}

// Power functions
float Pow2(float x) { return x * x; }
float Pow3(float x) { return x * x * x; }

float Pow4(float x)
{
    float x2 = x * x;
    return x2 * x2;
}

// Smooth interpolation: x²(3-2x)
float Smoothe(float x)
{
    return x * x * (3.0 - 2.0 * x);
}

// Fog falloff function
float Fogify(float x, float w)
{
    return w / (x * x + w);
}

// Contrast adjustment
float3 Contrast(float3 color, float contrastValue)
{
    return contrastValue * (color - 0.5) + 0.5;
}

// Inverse squared (1 - x²)
float InvPow2(float x)
{
    return 1.0 - x * x;
}

// Slope to 1 function
float SlopeTo1(float x, float m)
{
    return max(0.0, x * m - (m - 1.0));
}

// Quantization functions
float Stepify(float x, float stepSize)
{
    return round(x / stepSize) * stepSize;
}

float3 Stepify3(float3 x, float stepSize)
{
    return round(x / stepSize) * stepSize;
}

float3 Bandify(float3 value, float bands)
{
    return (floor(value * bands + 0.01) + 0.5) / bands;
}

#endif // LIB_COMMON_HLSL
