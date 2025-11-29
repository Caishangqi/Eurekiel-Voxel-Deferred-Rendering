#pragma once

class ImguiLeftDebugOverlay
{
public:
    ImguiLeftDebugOverlay()                                        = delete;
    ImguiLeftDebugOverlay(const ImguiLeftDebugOverlay&)            = delete;
    ImguiLeftDebugOverlay& operator=(const ImguiLeftDebugOverlay&) = delete;

    static void ShowWindow(bool* pOpen = nullptr);
};
