/**
 * @file ImguiSettingComposite.hpp
 * @brief Static ImGui interface for CompositeRenderPass configuration
 *
 * Design Pattern:
 * - Static-only class (no instantiation)
 * - Accesses FogUniforms via FOG_UNIFORM global
 * - Follows ImGui immediate mode paradigm
 */

#pragma once

// Forward declarations
class CompositeRenderPass;

/**
 * @class ImguiSettingComposite
 * @brief Static ImGui interface for composite pass configuration
 */
class ImguiSettingComposite
{
public:
    ImguiSettingComposite()                                        = delete;
    ImguiSettingComposite(const ImguiSettingComposite&)            = delete;
    ImguiSettingComposite& operator=(const ImguiSettingComposite&) = delete;

    /**
     * @brief Show composite pass configuration UI
     * @param compositePass CompositeRenderPass instance (optional, for future use)
     */
    static void Show(CompositeRenderPass* compositePass = nullptr);
};
