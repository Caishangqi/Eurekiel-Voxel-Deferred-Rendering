#pragma once

class SkyBasicRenderPass;

/**
 * ImguiSettingSkyBasic - Static ImGui interface for SkyBasicRenderPass debugging
 *
 * Displays: Void gradient toggle, sky dome phase colors, fog phase colors,
 *           sunrise/sunset strip colors, phase transition easing curves.
 */
class ImguiSettingSkyBasic
{
public:
    ImguiSettingSkyBasic()                                       = delete;
    ImguiSettingSkyBasic(const ImguiSettingSkyBasic&)            = delete;
    ImguiSettingSkyBasic& operator=(const ImguiSettingSkyBasic&) = delete;

    static void Show(SkyBasicRenderPass* skyPass);
};
