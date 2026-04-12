/**
 * @file ImguiRenderInspection.hpp
 * @brief Main ImGui window for render diagnostics and inspection tooling
 */

#pragma once

class ImguiRenderInspection
{
public:
    ImguiRenderInspection()                                        = delete;
    ImguiRenderInspection(const ImguiRenderInspection&)            = delete;
    ImguiRenderInspection& operator=(const ImguiRenderInspection&) = delete;

    static void ShowWindow(bool* pOpen = nullptr);
};
