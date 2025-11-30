#pragma once
#include "Engine/Math/Vec3.hpp"

// =============================================================================
// CelestialConstantBuffer - 用户自定义 Constant Buffer
// =============================================================================
//
// [Purpose]
//   为 Shader 提供天体数据（天体角度、日月位置等），用于天空和云渲染
//
// [Shader Side Declaration]
//   对应 HLSL: cbuffer CelestialUniforms : register(b15)
//   文件位置: F:/p4/Personal/SD/Engine/.enigma/assets/engine/shaders/include/celestial_uniforms.hlsl
//
// [Registration and Usage]
//   // 在 SkyRenderPass 构造函数中注册
//   g_theRendererSubsystem->GetUniformManager()->RegisterBuffer<CelestialConstantBuffer>(
//       15,  // 槽位 >= 15 (用户自定义)
//       UpdateFrequency::PerFrame  // 每帧更新一次
//   );
//
//   // 在 SkyRenderPass::Execute() 中上传数据
//   CelestialConstantBuffer celestialData;
//   celestialData.celestialAngle = m_timeOfDayManager->GetCelestialAngle();
//   celestialData.compensatedCelestialAngle = m_timeOfDayManager->GetCompensatedCelestialAngle();
//   celestialData.cloudTime = m_timeOfDayManager->GetCloudTime();
//   celestialData.sunPosition = CalculateSunPosition();
//   celestialData.moonPosition = CalculateMoonPosition();
//   g_theRendererSubsystem->GetUniformManager()->UploadBuffer(celestialData);
//
// [Alignment Requirements]
//   - 每个字段必须 16 字节对齐（HLSL cbuffer 规范）
//   - Vec3 后需要 padding（Vec3 = 12 字节，需要补齐到 16 字节）
//   - float 字段也需要单独占用 16 字节（通过 alignas 或 padding）
//
// [Reference]
//   - Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp (对齐示例)
//   - Code/Game/Framework/GameObject/Geometry.cpp:55 (UploadBuffer 示例)
//
// =============================================================================

#pragma warning(push)
#pragma warning(disable: 4324) // 结构体因 alignas 而填充 - 预期行为

struct CelestialConstantBuffer
{
    // ==================== 天体角度（16 字节） ====================
    alignas(16) float celestialAngle; // 天体角度 (0.0-1.0)，对应 Minecraft worldTime % 24000
    float             compensatedCelestialAngle; // 补偿角度 (+0.25)，用于日出/日落平滑过渡
    float             cloudTime; // 云时间 (tick * 0.03)，用于云动画
    float             skyBrightness; // 天空亮度 (0.0-1.0)，正午1.0，午夜0.0

    // ==================== 太阳方向向量（16 字节） ====================
    alignas(16) Vec3 sunPosition; // 太阳方向向量（视图空间，w=0），长度约100单位
    float            _padding2; // 填充到 16 字节

    // ==================== 月亮方向向量（16 字节） ====================
    alignas(16) Vec3 moonPosition; // 月亮方向向量（视图空间，w=0），长度约100单位
    float            _padding3; // 填充到 16 字节
};

#pragma warning(pop)

// =============================================================================
// [IMPORTANT] C++ 结构体与 HLSL cbuffer 字段对应关系
// =============================================================================
//
// C++ Side (CelestialConstantBuffer):
//   - float celestialAngle               → HLSL: float celestialAngle
//   - float compensatedCelestialAngle    → HLSL: float compensatedCelestialAngle
//   - float cloudTime                    → HLSL: float cloudTime
//   - float skyBrightness                → HLSL: float skyBrightness
//   - Vec3 sunPosition                   → HLSL: float3 sunPosition
//   - float _padding2                    → HLSL: (自动填充，无需声明)
//   - Vec3 moonPosition                  → HLSL: float3 moonPosition
//   - float _padding3                    → HLSL: (自动填充，无需声明)
//
// HLSL Side (celestial_uniforms.hlsl):
//   cbuffer CelestialUniforms : register(b15)
//   {
//       float  celestialAngle;
//       float  compensatedCelestialAngle;
//       float  cloudTime;
//       float  skyBrightness;
//       float3 sunPosition;
//       // padding 由 HLSL 编译器自动处理
//       float3 moonPosition;
//       // padding 由 HLSL 编译器自动处理
//   };
//
// =============================================================================
