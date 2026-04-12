/**
 * @file ImguiRenderInspection.cpp
 * @brief Main ImGui window for render diagnostics and inspection tooling
 */

#include "ImguiRenderInspection.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderInspection/ImguiQueueDiagnosticsPanel.hpp"
#include "Game/Framework/RenderPass/RenderChunkBaching/ImguiSettingChunkBatching.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace
{
    void ShowQueueTab()
    {
        if (ImGui::BeginPopupContextWindow("QueueDiagnosticsContextMenu"))
        {
            if (ImGui::MenuItem("Copy Queue Frame JSON"))
            {
                ImguiQueueDiagnosticsPanel::CopyCurrentFrameJsonToClipboard();
            }

            if (ImGui::MenuItem("Copy Queue Accumulated JSON"))
            {
                ImguiQueueDiagnosticsPanel::CopyAccumulatedJsonToClipboard();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Copy Async Mesh Frame JSON"))
            {
                ImguiQueueDiagnosticsPanel::CopyAsyncChunkMeshCurrentFrameJsonToClipboard();
            }

            if (ImGui::MenuItem("Copy Async Mesh Accumulated JSON"))
            {
                ImguiQueueDiagnosticsPanel::CopyAsyncChunkMeshAccumulatedJsonToClipboard();
            }

            ImGui::EndPopup();
        }

        ImGui::TextDisabled("Right-click inside this tab to copy queue or async mesh diagnostics JSON.");
        ImguiQueueDiagnosticsPanel::Show();
    }

    void ShowChunkBatchingTab()
    {
        enigma::voxel::World* world = g_theGame ? g_theGame->GetWorld() : nullptr;

        if (ImGui::BeginPopupContextWindow("ChunkBatchingContextMenu"))
        {
            if (ImGui::MenuItem("Copy Full Snapshot JSON", nullptr, false, world != nullptr))
            {
                ImguiSettingChunkBatching::CopyFullSnapshotToClipboard(*world);
            }

            if (ImGui::MenuItem("Copy Frame Snapshot JSON", nullptr, false, world != nullptr))
            {
                ImguiSettingChunkBatching::CopyFrameSnapshotToClipboard(*world);
            }

            if (ImGui::MenuItem("Copy Lifetime Snapshot JSON", nullptr, false, world != nullptr))
            {
                ImguiSettingChunkBatching::CopyLifetimeSnapshotToClipboard(*world);
            }

            ImGui::EndPopup();
        }

        ImGui::TextDisabled("Right-click inside this tab to copy chunk batching JSON snapshots.");
        ImguiSettingChunkBatching::Show(world);
    }
}

void ImguiRenderInspection::ShowWindow(bool* pOpen)
{
    if (!ImGui::Begin("Render Inspection", pOpen, ImGuiWindowFlags_None))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("RenderInspectionTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Queue"))
        {
            ShowQueueTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Chunk Batching"))
        {
            ShowChunkBatchingTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
