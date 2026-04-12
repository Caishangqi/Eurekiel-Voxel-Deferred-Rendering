/**
 * @file ImguiQueueDiagnosticsPanel.hpp
 * @brief ImGui sub-panel for multi-queue execution diagnostics
 */

#pragma once

#include "Engine/Core/Json.hpp"

class ImguiQueueDiagnosticsPanel
{
public:
    ImguiQueueDiagnosticsPanel()                                            = delete;
    ImguiQueueDiagnosticsPanel(const ImguiQueueDiagnosticsPanel&)            = delete;
    ImguiQueueDiagnosticsPanel& operator=(const ImguiQueueDiagnosticsPanel&) = delete;

    static void Show();
    static void CopyCurrentFrameJsonToClipboard();
    static void CopyAccumulatedJsonToClipboard();
    static void CopyAsyncChunkMeshCurrentFrameJsonToClipboard();
    static void CopyAsyncChunkMeshAccumulatedJsonToClipboard();

    static enigma::core::Json BuildCurrentFrameJson();
    static enigma::core::Json BuildAccumulatedJson();
    static enigma::core::Json BuildAsyncChunkMeshCurrentFrameJson();
    static enigma::core::Json BuildAsyncChunkMeshAccumulatedJson();
};
