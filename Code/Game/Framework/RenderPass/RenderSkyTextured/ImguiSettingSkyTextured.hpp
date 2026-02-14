#pragma once

class SkyTexturedRenderPass;

/**
 * ImguiSettingSkyTextured - Static ImGui interface for SkyTexturedRenderPass debugging
 *
 * Displays: Celestial body size (sun/moon), star rendering parameters.
 */
class ImguiSettingSkyTextured
{
public:
    ImguiSettingSkyTextured()                                          = delete;
    ImguiSettingSkyTextured(const ImguiSettingSkyTextured&)            = delete;
    ImguiSettingSkyTextured& operator=(const ImguiSettingSkyTextured&) = delete;

    static void Show(SkyTexturedRenderPass* skyPass);
};
