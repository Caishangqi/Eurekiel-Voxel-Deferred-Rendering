#pragma once

class DebugRenderPass;

class ImguiSettingRenderDebug
{
public:
    ImguiSettingRenderDebug()                                          = delete;
    ImguiSettingRenderDebug(const ImguiSettingRenderDebug&)            = delete;
    ImguiSettingRenderDebug& operator=(const ImguiSettingRenderDebug&) = delete;

    static void Show();
};
