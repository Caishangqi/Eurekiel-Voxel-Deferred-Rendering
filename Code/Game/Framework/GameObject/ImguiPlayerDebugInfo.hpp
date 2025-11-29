#pragma once

class ImguiPlayerDebugInfo
{
public:
    ImguiPlayerDebugInfo()                                       = delete;
    ImguiPlayerDebugInfo(const ImguiPlayerDebugInfo&)            = delete;
    ImguiPlayerDebugInfo& operator=(const ImguiPlayerDebugInfo&) = delete;

    static void ShowWindow(bool* pOpen = nullptr);
};
