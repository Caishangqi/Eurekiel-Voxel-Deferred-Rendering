#pragma once
#include <cstdint>

// =============================================================================
// WorldRenderingPhase - Iris-Compatible Rendering Phase Enum
// =============================================================================
//
// [Purpose]
//   Tracks the current rendering phase for shader program selection and uniform updates.
//   Mirrors Iris WorldRenderingPhase.java for shader pack compatibility.
//
// [Shader Side Declaration]
//   Corresponds to HLSL: int renderStage in CommonUniforms (register b16, space1)
//   File: F:/p4/Personal/SD/Engine/.enigma/assets/engine/shaders/include/common_uniforms.hlsl
//
// [Usage in Shaders]
//   #include "../include/common_uniforms.hlsl"
//
//   if (renderStage == RENDER_STAGE_SKY) { /* sky dome rendering */ }
//   if (renderStage == RENDER_STAGE_SUNSET) { /* sunset strip rendering */ }
//
// [Reference]
//   - Iris WorldRenderingPhase.java
//   - Iris CommonUniforms.java:108 - uniform1i("renderStage", ...)
//
// =============================================================================

enum class WorldRenderingPhase : uint32_t
{
    // ==================== Sky Rendering Phases ====================
    NONE = 0, // No specific phase
    SKY = 1, // Sky dome (gbuffers_skybasic with POSITION format)
    SUNSET = 2, // Sunset/sunrise strip (gbuffers_skybasic with POSITION_COLOR format)
    CUSTOM_SKY = 3, // Custom sky (FabricSkyboxes, etc.)
    SUN = 4, // Sun billboard (gbuffers_skytextured)
    MOON = 5, // Moon billboard (gbuffers_skytextured)
    STARS = 6, // Star field
    SKY_VOID = 7, // Void plane (below world)

    // ==================== Terrain Rendering Phases ====================
    TERRAIN_SOLID = 8, // Solid terrain blocks
    TERRAIN_CUTOUT_MIPPED = 9, // Cutout terrain with mipmapping (leaves, etc.)
    TERRAIN_CUTOUT = 10, // Cutout terrain without mipmapping
    TERRAIN_TRANSLUCENT = 17, // Translucent terrain (water, ice, etc.)

    // ==================== Entity Rendering Phases ====================
    ENTITIES = 11, // Entity rendering
    BLOCK_ENTITIES = 12, // Block entity rendering (chests, signs, etc.)

    // ==================== Special Rendering Phases ====================
    DESTROY = 13, // Block breaking animation
    OUTLINE = 14, // Block selection outline
    DEBUG = 15, // Debug rendering
    HAND_SOLID = 16, // Hand rendering (solid)
    TRIPWIRE = 18, // Tripwire rendering
    PARTICLES = 19, // Particle rendering
    CLOUDS = 20, // Cloud rendering
    RAIN_SNOW = 21, // Weather effects (rain, snow)
    WORLD_BORDER = 22, // World border effect
    HAND_TRANSLUCENT = 23 // Hand rendering (translucent)
};

// =============================================================================
// Helper function to convert enum to int for shader uniform
// =============================================================================
inline int ToRenderStage(WorldRenderingPhase phase)
{
    return static_cast<int>(phase);
}
