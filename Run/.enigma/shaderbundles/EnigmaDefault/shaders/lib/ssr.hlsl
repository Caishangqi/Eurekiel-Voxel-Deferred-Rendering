/**
 * @file ssr.hlsl
 * @brief Screen-Space Reflections via view-space ray marching
 *
 * Simplified port of ComplementaryReimagined reflections.glsl Method 1.
 * Exponential stepping with binary refinement in view space,
 * projected to screen space for depth comparison.
 *
 * Dependencies:
 *   - @engine/core/core.hlsl  (textures, matrices, samplers)
 *   - @engine/lib/math.hlsl   (ReconstructViewPosition, ScreenToNDC)
 *   - include/settings.hlsl   (SSR_MAX_STEPS, SSR_STEP_SIZE, SSR_BINARY_STEPS)
 *   - lib/common.hlsl         (Luma)
 *
 * Reference: CR reflections.glsl:76-183 (Method 1: Ray Marched Reflection)
 */

#ifndef LIB_SSR_HLSL
#define LIB_SSR_HLSL

// ============================================================================
// SSR Result
// ============================================================================

/// Ray marching result returned by ComputeSSR
struct SSRResult
{
    float3 color; // Reflected scene color at hit point
    float  alpha; // Reflection confidence [0,1] (0=miss, 1=solid hit)
};

// ============================================================================
// Internal Helpers
// ============================================================================

/// Project view-space position to screen UV + NDC depth.
/// Accounts for gbufferRenderer (DX12 API correction) in the projection chain.
/// Chain: View -> Render (gbufferRenderer) -> Clip (gbufferProjection) -> NDC -> Screen
/// @return float3(screenUV.x, screenUV.y, ndcDepth)
float3 SSR_ViewToScreen(float3 viewPos)
{
    float4 renderPos = mul(gbufferRenderer, float4(viewPos, 1.0));
    float4 clipPos   = mul(gbufferProjection, float4(renderPos.xyz, 1.0));
    float3 ndc       = clipPos.xyz / clipPos.w;
    // NDC -> Screen UV (inverse of ScreenToNDC in math.hlsl: flip Y, remap [0,1])
    return float3(ndc.x * 0.5 + 0.5, -ndc.y * 0.5 + 0.5, ndc.z);
}

// ============================================================================
// ComputeSSR
// ============================================================================

/// Screen-space reflection via view-space ray marching.
/// CR-style exponential stepping with binary refinement.
///
/// @param worldPos   Fragment world position
/// @param normal     World-space surface normal (from colortex2)
/// @param viewDir    View direction camera->fragment (world space, normalized)
/// @param smoothness Surface smoothness [0,1] (from colortex4)
/// @param dither     Screen-space dither value [0,1) for temporal jitter
/// @param depth      Fragment depth buffer value [0,1]
/// @return SSRResult with reflected color and confidence alpha
SSRResult ComputeSSR(
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float  smoothness,
    float  dither,
    float  depth)
{
    SSRResult result;
    result.color = float3(0.0, 0.0, 0.0);
    result.alpha = 0.0;

    // Skip sky pixels
    if (depth >= 0.9999)
        return result;

    // --- Transform to view space ---
    float3 viewPos    = mul(gbufferView, float4(worldPos, 1.0)).xyz;
    float3 viewNormal = normalize(mul((float3x3)gbufferView, normal));
    float3 viewDirVS  = normalize(mul((float3x3)gbufferView, viewDir));

    // Reflected view direction in view space
    float3 reflectDirVS = normalize(reflect(viewDirVS, viewNormal));

    // --- Bias start position along normal (avoid self-intersection) ---
    // CR: start = viewPos + normal * (lViewPos * 0.025 * (1-fresnel) + 0.05)
    float  lViewPos = length(viewPos);
    float3 start    = viewPos + viewNormal * (lViewPos * 0.025 + 0.05);

    // --- Initialize ray march (CR exponential stepping) ---
    float3 stepVec   = reflectDirVS * 0.5 * SSR_STEP_SIZE;
    float3 accumStep = stepVec;
    float3 rayPos    = start + stepVec;

    // Screen edge bounds (CR: rEdge = vec2(0.525, 0.525))
    static const float2 SCREEN_EDGE = float2(0.525, 0.525);

    int    refinements  = 0;
    float3 hitScreenPos = float3(0.0, 0.0, 0.0);
    float  hitViewDist  = 0.0;
    bool   hit          = false;

    // --- Main ray march loop (CR reflections.glsl:105-123) ---
    for (int i = 0; i < SSR_MAX_STEPS; i++)
    {
        // Project ray position to screen
        float3 screenPos = SSR_ViewToScreen(rayPos);

        // Boundary check: stop if ray leaves visible screen area
        if (abs(screenPos.x - 0.5) > SCREEN_EDGE.x ||
            abs(screenPos.y - 0.5) > SCREEN_EDGE.y)
            break;

        // Sample scene depth and reconstruct view-space position at that pixel
        float  sceneDepth   = depthtex0.Sample(sampler1, screenPos.xy).r;
        float3 sceneViewPos = ReconstructViewPosition(
            screenPos.xy, sceneDepth,
            gbufferProjectionInverse, gbufferRendererInverse);

        // Error metric: distance between ray and scene surface in view space
        // CR: if (err * 0.33333 < length(vector)) -> hit
        float err = length(rayPos - sceneViewPos);
        if (err * 0.333 < length(stepVec))
        {
            refinements++;
            if (refinements >= SSR_BINARY_STEPS)
            {
                hitScreenPos = screenPos;
                hitViewDist  = length(sceneViewPos);
                hit          = true;
                break;
            }
            // Back up and take smaller steps for refinement
            accumStep -= stepVec;
            stepVec   *= 0.1;
        }

        // Exponential step growth with dither jitter (CR reflections.glsl:120-122)
        stepVec   *= 2.0;
        accumStep += stepVec * (0.95 + 0.1 * dither);
        rayPos    = start + accumStep;
    }

    // --- Process hit result (CR reflections.glsl:128-178) ---
    if (hit && hitScreenPos.z < 0.99997)
    {
        // Border fade: hard cutoff near screen edges
        float2 absPos = abs(hitScreenPos.xy - 0.5);
        float2 cdist  = absPos / SCREEN_EDGE;
        float  border = saturate(1.0 - pow(max(cdist.x, cdist.y), 50.0));

        if (border > 0.001)
        {
            // Sample reflected scene color at hit point
            result.color = colortex0.Sample(sampler0, hitScreenPos.xy).rgb;

            // Edge factor: smooth luminance-adaptive fade (CR reflections.glsl:169)
            // pow8(cdist) for gradual falloff
            float2 cdist4     = cdist * cdist * cdist * cdist;
            float2 edgeFactor = 1.0 - cdist4 * cdist4;
            float  refFactor  = pow(edgeFactor.x * edgeFactor.y,
                                  2.0 + 3.0 * Luma(result.color));

            // Depth proximity: reject hits too close to the source fragment
            // CR: reflection.a *= clamp(posDif + 3.0, 0.0, 1.0)
            float posDif   = hitViewDist - lViewPos;
            float proxFade = saturate(posDif + 3.0);

            // Combine all fade factors, modulate by surface smoothness
            result.alpha = border * refFactor * proxFade * smoothness;
        }
    }

    return result;
}

// ============================================================================
// Reflection Fallback
// ============================================================================

/// Sky-tinted fallback reflection for SSR misses.
/// Provides a basic environment reflection based on the reflection direction.
///
/// @param reflectDir   Reflection direction (world space, normalized)
/// @param skyColor     Current sky/ambient color
/// @param skyLightFactor Sky light influence [0,1] (from lightmap or uniform)
/// @return Fallback reflection color
float3 GetReflectionFallback(float3 reflectDir, float3 skyColor, float skyLightFactor)
{
    // Upward-facing reflections receive more sky color (engine: +Z is up)
    float upFactor = max(reflectDir.z, 0.0);
    float skyBlend = lerp(0.3, 1.0, upFactor);

    return skyColor * skyLightFactor * skyBlend;
}

#endif // LIB_SSR_HLSL
