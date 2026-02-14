#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"

// =============================================================================
// CelestialConstantBuffer - Celestial Uniforms (Iris Compatible)
// =============================================================================
//
// [Iris Reference]
//   Source: CelestialUniforms.java:74-82
//   Path: ReferenceProject/Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/CelestialUniforms.java
//
// [Iris Fields Mapping]
//   Iris                    -> Engine
//   sunAngle                -> sunAngle (celestialAngle的别名)
//   sunPosition             -> sunPosition
//   moonPosition            -> moonPosition
//   shadowAngle             -> shadowAngle [NEW]
//   shadowLightPosition     -> shadowLightPosition [NEW]
//   upPosition              -> upPosition [NEW]
//
// [Shader Side Declaration]
// Corresponding to HLSL: cbuffer CelestialUniforms: register(b9, space1)
// File location: Engine/.enigma/assets/engine/shaders/include/celestial_uniforms.hlsl
//
// [shadowLightPosition Logic] (CelestialUniforms.java:93-95)
//   public Vector4f getShadowLightPosition() {
//       return isDay() ? getSunPosition() : getMoonPosition();
//   }
// day = sunPosition, night = moonPosition
//
// =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324)

struct CelestialConstantBuffer
{
    // ==================== Row 0: Angle data (16 bytes) ====================
    alignas(16) float celestialAngle; // Celestial angle (0.0-1.0), Iris: sunAngle
    float             sunAngle; // @iris sunAngle = celestialAngle + 0.25 (0.25=noon, 0.75=midnight)
    float             cloudTime; // cloud time (tick * 0.03)
    float             skyBrightness; // sky brightness (0.0-1.0)

    // ==================== Row 1: Sun position (16 bytes) ====================
    // @iris sunPosition
    alignas(16) Vec3 sunPosition; // Sun direction vector (view space), length 100
    float            shadowAngle; // [NEW] @iris shadowAngle (0.0-0.5)

    // ==================== Row 2: Moon position (16 bytes) ====================
    // @iris moonPosition
    alignas(16) Vec3 moonPosition; // Moon direction vector (view space), length 100
    float            _padding2; // Padding (sunPathRotation is NOT a uniform in Iris, use const float in shader source)
    // ==================== Row 3: Shadow light position (16 bytes) ====================
    // @iris shadowLightPosition
    // Logic: isDay() ? sunPosition : moonPosition (CelestialUniforms.java:93-95)
    alignas(16) Vec3 shadowLightPosition; // [NEW] Shadow light source position (day=sun, night=moon)
    float            _padding3; //Padding to 16 bytes

    // ==================== Row 4: Upward vector (16 bytes) ====================
    // @iris upPosition
    alignas(16) Vec3 upPosition; // [NEW] Up direction vector (view space), length 100
    float            _padding4; // Padding to 16 bytes

    // ==================== Row 5: Color modulator (16 bytes) ====================
    // @iris colorModulator (ExternallyManagedUniforms.java:49)
    alignas(16) Vec4 colorModulator = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    CelestialConstantBuffer()
        : celestialAngle(0.25f) //Default noon
          , sunAngle(0.5f) // 0.25 + 0.25
          , cloudTime(0.0f)
          , skyBrightness(1.0f) // Brightest at noon
          , sunPosition(Vec3(0.0f, 100.0f, 0.0f)) // Default is directly above
          , shadowAngle(0.25f) //Default noon shadow angle
          , moonPosition(Vec3(0.0f, -100.0f, 0.0f)) // 默认正下方
          , _padding2(0.0f)
          , shadowLightPosition(Vec3(0.0f, 100.0f, 0.0f)) // 默认太阳位置（白天）
          , _padding3(0.0f)
          , upPosition(Vec3(0.0f, 100.0f, 0.0f)) // 默认上方向
          , _padding4(0.0f)
          , colorModulator(Vec4(1.0f, 1.0f, 1.0f, 1.0f))
    {
    }
};

#pragma warning(pop)

// =============================================================================
// [Memory Layout] Total size: 96 bytes (6 rows × 16 bytes)
// =============================================================================
//
// Row 0 [0-15]:   celestialAngle, sunAngle, cloudTime, skyBrightness
// Row 1 [16-31]:  sunPosition.xyz, shadowAngle
// Row 2 [32-47]:  moonPosition.xyz, _padding2
// Row 3 [48-63]:  shadowLightPosition.xyz, _padding3
// Row 4 [64-79]:  upPosition.xyz, _padding4
// Row 5 [80-95]:  colorModulator.xyzw
//
