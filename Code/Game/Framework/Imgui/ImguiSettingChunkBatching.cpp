/**
 * @file ImguiSettingChunkBatching.cpp
 * @brief Static ImGui interface for chunk batching runtime stats
 */

#include "ImguiSettingChunkBatching.hpp"

#include "Engine/Voxel/World/World.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiSettingChunkBatching::Show(enigma::voxel::World* world)
{
    if (!world)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] World is null");
        return;
    }

    if (ImGui::CollapsingHeader("Chunk Batching", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        const auto& stats = world->GetChunkBatchStats();

        ImGui::TextDisabled("Chunk batching is always enabled.");
        ImGui::TextDisabled("Legacy per-chunk submission has been removed from runtime.");

        ImGui::SeparatorText("Runtime");
        ImGui::Text("Backend: Direct region DrawIndexed");
        ImGui::Text("Queued Dirty Regions: %u", world->GetChunkRenderRegionStorage().GetDirtyRegionCount());
        ImGui::Text("Dirty Region Budget: %u", world->GetMaxChunkBatchRegionRebuildsPerFrame());
        ImGui::TextWrapped("Dirty regions keep rendering the last committed region geometry until the rebuilt buffers are ready.");

        ImGui::SeparatorText("Frame Stats");
        ImGui::Text("Visible Chunks: %u", stats.visibleChunks);
        ImGui::Text("Visible Regions: %u", stats.visibleRegions);
        ImGui::Text("Batched Draws: %u", stats.batchedDraws);
        ImGui::Text("Dirty Region Rebuilds: %u / %u",
            stats.dirtyRegionRebuilds,
            world->GetMaxChunkBatchRegionRebuildsPerFrame());

        ImGui::Unindent();
    }
}
