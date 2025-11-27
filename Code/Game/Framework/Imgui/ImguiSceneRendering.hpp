/**
 * @file ImguiSceneRendering.hpp
 * @brief ImGui Scene Rendering Tab (Sky, Cloud, Sun/Moon Parameters)
 * @date 2025-11-26
 *
 * Features:
 * - Sky rendering parameters (ImguiSettingSky)
 * - Cloud rendering parameters (ImguiSettingCloud)
 * - Sun & Moon parameters (size, brightness)
 * - Static-only class (no instantiation)
 *
 * Architecture:
 * ImguiSceneRendering (Tab Content)
 *   ├── ImguiSettingSky::Show()
 *   ├── ImguiSettingCloud::Show()
 *   └── Sun & Moon CollapsingHeader
 *
 * Usage:
 * @code
 * // Called by ImguiGameSettings in "Scene Rendering" tab
 * ImguiSceneRendering::Show();
 * @endcode
 */

#pragma once

// Forward declarations
class SkyRenderPass;
class CloudRenderPass;

/**
 * @class ImguiSceneRendering
 * @brief Static-only class for Scene Rendering tab content
 *
 * Responsibilities:
 * - Organize scene rendering UI modules
 * - Call sky rendering UI (ImguiSettingSky)
 * - Call cloud rendering UI (ImguiSettingCloud)
 * - Display sun/moon parameters
 */
class ImguiSceneRendering
{
public:
    // [REQUIRED] Static-only class - Delete all constructors
    ImguiSceneRendering()                                      = delete;
    ImguiSceneRendering(const ImguiSceneRendering&)            = delete;
    ImguiSceneRendering& operator=(const ImguiSceneRendering&) = delete;

    /**
     * @brief Show Scene Rendering tab content
     *
     * Current Modules:
     * - Sky Rendering (ImguiSettingSky::Show())
     * - Cloud Rendering (ImguiSettingCloud::Show())
     * - Sun & Moon Parameters
     *
     * Future Modules:
     * - Terrain Rendering
     * - Water Rendering
     * - Post-Processing Effects
     */
    static void Show();

private:
    /**
     * @brief Show Sun & Moon parameters section
     *
     * Parameters:
     * - Sun Size (slider)
     * - Moon Size (slider)
     * - Celestial Brightness (slider)
     */
    static void ShowSunMoonParameters();
};
