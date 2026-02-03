#pragma once
#include "Game/Framework/RenderPass/SceneRenderPass.hpp"
#include "Engine/Graphic/Camera/ShadowCamera.hpp"
#include "Engine/Math/Vec3.hpp"
#include <memory>

// Forward declarations
namespace enigma::voxel
{
    class ITimeProvider;
}

namespace enigma::graphic
{
    class ShaderProgram;
    class D12Texture;
}

/**
 * @brief [NEW] Setup render pass for shadow map rendering
 *
 * Responsible for:
 * - Loading shadow shaders (shadow.vs.hlsl, shadow.ps.hlsl)
 * - Creating ShadowCamera with light direction vector
 * - Updating ShadowCamera position/direction each frame
 * - Grid alignment to prevent shadow swimming
 * - Rendering terrain to shadow map with proper vertex layout
 *
 * [FIX] Now uses Vec3 light direction instead of EulerAngles
 * to avoid gimbal lock issues at extreme sun angles.
 *
 * Reference: Iris ShadowRenderer.java, TerrainRenderPass.cpp
 */
class ShadowRenderPass : public SceneRenderPass
{
public:
    ShadowRenderPass();
    ~ShadowRenderPass() override = default;

    void Execute() override;

protected:
    void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle) override;
    void OnShaderBundleUnloaded() override;

    void BeginPass() override;
    void EndPass() override;

public:
    Vec3        m_lightDirection;
    EulerAngles m_lightDirectionEulerAngles;

private:
    // ========================================================================
    // Shadow Camera Management
    // ========================================================================

    /**
     * @brief Update shadow camera position and orientation for current frame
     */
    void UpdateShadowCamera();

    /**
     * @brief Render terrain chunks to shadow map
     */
    void RenderShadowMap();

    /**
     * @brief Snap position to grid to prevent shadow swimming
     */
    static Vec3 SnapToGrid(const Vec3& pos, float interval);

    // ========================================================================
    // Shadow Resources (created once in constructor)
    // ========================================================================

    std::unique_ptr<enigma::graphic::ShadowCamera>  m_shadowCamera;
    std::shared_ptr<enigma::graphic::ShaderProgram> m_shadowProgram;
    std::shared_ptr<enigma::graphic::D12Texture>    m_blockAtlasTexture;

    // ========================================================================
    // Shadow Configuration Constants
    // ========================================================================

    static constexpr float SHADOW_INTERVAL_SIZE = 2.0f; // Grid snap interval
    static constexpr float SHADOW_HALF_PLANE    = 160.0f; // Shadow frustum half-size
    static constexpr float SHADOW_NEAR_PLANE    = 0.1f; // Near clip distance
    static constexpr float SHADOW_FAR_PLANE     = 500.0f; // Far clip distance
    static constexpr float SHADOW_DISTANCE      = 128.0f; // Light distance from center
};
