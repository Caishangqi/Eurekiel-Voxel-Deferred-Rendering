/**
 * @file ImguiSettingChunkBatching.cpp
 * @brief Static ImGui interface for chunk batching runtime stats
 */

#include "ImguiSettingChunkBatching.hpp"
#include "ChunkBachingRenderPass.hpp"

#include <cmath>
#include <functional>

#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/Camera/GameCameraDebugState.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Framework/RenderPass/RenderChunkBaching/ChunkBachingDebugViewState.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace
{
    constexpr float CHUNK_BATCH_CULLING_MAP_HEIGHT = 360.0f;

    struct ChunkBatchPhase2Snapshot
    {
        uint32_t loadedChunks = 0;
        uint32_t residentRegions = 0;
        uint32_t validBatchGeometryRegions = 0;
        uint32_t dirtyResidentRegions = 0;
        uint32_t buildFailedRegions = 0;
        uint32_t queuedDirtyRegions = 0;
        uint32_t replacementUploadsLifetime = 0;
        uint32_t replacementFallbacksLifetime = 0;
        uint32_t vertexArenaCapacity = 0;
        uint32_t vertexArenaRemaining = 0;
        uint32_t indexArenaCapacity = 0;
        uint32_t indexArenaRemaining = 0;
    };

    ImU32 ToImGuiColor(const Rgba8& color)
    {
        return IM_COL32(color.r, color.g, color.b, color.a);
    }

    Vec2 GetActiveCullingMapCenter(const Vec2& playerPosition2D)
    {
        if (!GameCameraDebugState::IsDetachedDebugCameraEnabled() || !g_theGame || !g_theGame->GetRenderCamera())
        {
            return playerPosition2D;
        }

        const Vec3 debugCameraPosition = g_theGame->GetRenderCamera()->GetPosition();
        return Vec2(debugCameraPosition.x, debugCameraPosition.y);
    }

    bool TryGetMainCameraFrustum(Frustum& outFrustum)
    {
        return g_theGame &&
            g_theGame->GetPlayerCamera() &&
            g_theGame->GetPlayerCamera()->GetFrustum(outFrustum);
    }

    void DrawCameraFrustumOnMap(
        ImDrawList* drawList,
        const Vec2& cameraPosition,
        const EulerAngles& cameraOrientation,
        float verticalFovDegrees,
        float aspectRatio,
        float farPlane,
        const ImU32 color,
        const std::function<ImVec2(const Vec2&)>& worldToScreen)
    {
        Vec3 forward3D;
        Vec3 left3D;
        Vec3 up3D;
        cameraOrientation.GetAsVectors_IFwd_JLeft_KUp(forward3D, left3D, up3D);

        Vec2 forward2D = Vec2(forward3D.x, forward3D.y).GetNormalized();
        Vec2 left2D    = Vec2(left3D.x, left3D.y).GetNormalized();
        if (forward2D == Vec2::ZERO || left2D == Vec2::ZERO)
        {
            return;
        }

        const float halfVerticalRadians   = ConvertDegreesToRadians(verticalFovDegrees * 0.5f);
        const float halfHorizontalRadians = static_cast<float>(std::atan(std::tan(halfVerticalRadians) * aspectRatio));
        const float halfHorizontalDegrees = ConvertRadiansToDegrees(halfHorizontalRadians);

        const Vec2 leftRayDirection  = forward2D.GetRotatedDegrees(halfHorizontalDegrees);
        const Vec2 rightRayDirection = forward2D.GetRotatedDegrees(-halfHorizontalDegrees);

        const Vec2 farLeftPoint  = cameraPosition + leftRayDirection * farPlane;
        const Vec2 farRightPoint = cameraPosition + rightRayDirection * farPlane;

        const ImVec2 screenOrigin    = worldToScreen(cameraPosition);
        const ImVec2 screenFarLeft   = worldToScreen(farLeftPoint);
        const ImVec2 screenFarRight  = worldToScreen(farRightPoint);
        const ImVec2 screenForward   = worldToScreen(cameraPosition + forward2D * 12.0f);

        drawList->AddLine(screenOrigin, screenFarLeft, color, 1.5f);
        drawList->AddLine(screenOrigin, screenFarRight, color, 1.5f);
        drawList->AddLine(screenFarLeft, screenFarRight, color, 1.0f);
        drawList->AddLine(screenOrigin, screenForward, color, 2.0f);
        drawList->AddCircleFilled(screenOrigin, 4.0f, color, 12);
    }

    void DrawChunkBatchCullingMap(const enigma::voxel::World& world)
    {
        if (!g_theGame || !g_theGame->m_player)
        {
            ImGui::TextDisabled("Culling map requires an active player camera.");
            return;
        }

        Vec2 playerPosition2D = Vec2(
            g_theGame->m_player->m_position.x,
            g_theGame->m_player->m_position.y);
        Vec2 mapCenter = GetActiveCullingMapCenter(playerPosition2D);

        Frustum mainFrustum;
        const Frustum* mainFrustumPtr = TryGetMainCameraFrustum(mainFrustum) ? &mainFrustum : nullptr;

        bool useDetachedDebugCamera = GameCameraDebugState::IsDetachedDebugCameraEnabled();
        if (ImGui::Checkbox("Use Detached Debug Camera", &useDetachedDebugCamera))
        {
            GameCameraDebugState::SetDetachedDebugCameraEnabled(useDetachedDebugCamera);
        }

        if (GameCameraDebugState::IsDetachedDebugCameraEnabled())
        {
            if (ImGui::Button("Reset Debug Camera To Player"))
            {
                GameCameraDebugState::RequestDebugCameraSync();
            }
        }

        bool drawPlayerCameraFrustum = GameCameraDebugState::ShouldDrawPlayerCameraFrustum();
        if (ImGui::Checkbox("Draw Player Camera Frustum", &drawPlayerCameraFrustum))
        {
            GameCameraDebugState::SetDrawPlayerCameraFrustum(drawPlayerCameraFrustum);
        }
        ImGui::SliderFloat("Culling Map Half Extent", &ChunkBachingDebugViewState::cullingMapHalfExtent, 32.0f, 512.0f, "%.0f");
        mapCenter = GetActiveCullingMapCenter(playerPosition2D);

        ImGui::TextDisabled("Detached mode routes input to the debug camera. Chunk loading and chunk-batch culling still use the player camera.");

        const ImVec2 availableSize = ImGui::GetContentRegionAvail();
        const float  mapWidth = availableSize.x > 0.0f ? availableSize.x : 320.0f;
        const ImVec2 mapSize = ImVec2(mapWidth, CHUNK_BATCH_CULLING_MAP_HEIGHT);
        const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("ChunkBatchCullingMapCanvas", mapSize);
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 mapMin = cursorScreenPos;
        const ImVec2 mapMax = ImVec2(cursorScreenPos.x + mapSize.x, cursorScreenPos.y + mapSize.y);
        drawList->AddRectFilled(mapMin, mapMax, IM_COL32(18, 22, 28, 220), 4.0f);
        drawList->AddRect(mapMin, mapMax, IM_COL32(90, 110, 130, 255), 4.0f, 0, 1.0f);

        const float halfExtent = ChunkBachingDebugViewState::cullingMapHalfExtent;
        const float usableHalfExtent = halfExtent > 0.0f ? halfExtent : 1.0f;
        const float pixelsPerWorldX = mapSize.x / (usableHalfExtent * 2.0f);
        const float pixelsPerWorldY = mapSize.y / (usableHalfExtent * 2.0f);

        auto worldToScreen = [&](const Vec2& worldPos)
        {
            const float normalizedX = (worldPos.x - mapCenter.x) * pixelsPerWorldX;
            const float normalizedY = (worldPos.y - mapCenter.y) * pixelsPerWorldY;
            return ImVec2(
                mapMin.x + mapSize.x * 0.5f + normalizedX,
                mapMin.y + mapSize.y * 0.5f - normalizedY);
        };

        drawList->AddLine(
            ImVec2(mapMin.x, mapMin.y + mapSize.y * 0.5f),
            ImVec2(mapMax.x, mapMin.y + mapSize.y * 0.5f),
            IM_COL32(70, 80, 90, 255),
            1.0f);
        drawList->AddLine(
            ImVec2(mapMin.x + mapSize.x * 0.5f, mapMin.y),
            ImVec2(mapMin.x + mapSize.x * 0.5f, mapMax.y),
            IM_COL32(70, 80, 90, 255),
            1.0f);

        const auto& storage = world.GetChunkRenderRegionStorage();
        for (const auto& regionEntry : storage.GetRegions())
        {
            const auto& region = regionEntry.second;
            const AABB3& bounds = region.geometry.worldBounds;

            const ImVec2 rectMin = worldToScreen(Vec2(bounds.m_mins.x, bounds.m_maxs.y));
            const ImVec2 rectMax = worldToScreen(Vec2(bounds.m_maxs.x, bounds.m_mins.y));
            const ImU32  color = ToImGuiColor(ChunkBachingRenderPass::GetRegionDebugColor(region, mainFrustumPtr));

            drawList->AddRect(rectMin, rectMax, color, 0.0f, 0, 1.5f);
        }

        if (GameCameraDebugState::ShouldDrawPlayerCameraFrustum() && g_theGame->GetPlayerCamera())
        {
            auto* playerCamera = g_theGame->GetPlayerCamera();
            DrawCameraFrustumOnMap(
                drawList,
                Vec2(playerCamera->GetPosition().x, playerCamera->GetPosition().y),
                playerCamera->GetOrientation(),
                playerCamera->GetFOV(),
                playerCamera->GetAspectRatio(),
                playerCamera->GetFarPlane(),
                IM_COL32(255, 255, 255, 255),
                worldToScreen);
        }

        if (GameCameraDebugState::IsDetachedDebugCameraEnabled() && g_theGame->GetRenderCamera())
        {
            auto* debugCamera = g_theGame->GetRenderCamera();
            DrawCameraFrustumOnMap(
                drawList,
                Vec2(debugCamera->GetPosition().x, debugCamera->GetPosition().y),
                debugCamera->GetOrientation(),
                debugCamera->GetFOV(),
                debugCamera->GetAspectRatio(),
                debugCamera->GetFarPlane(),
                IM_COL32(80, 200, 255, 255),
                worldToScreen);
        }

        ImGui::TextDisabled("White = player camera frustum used for culling. Cyan = detached debug camera used for observation.");
    }

    float ComputeCullRatio(uint32_t visibleCount, uint32_t culledCount)
    {
        const uint32_t totalCount = visibleCount + culledCount;
        if (totalCount == 0u)
        {
            return 0.0f;
        }

        return static_cast<float>(culledCount) / static_cast<float>(totalCount);
    }

    ChunkBatchPhase2Snapshot CollectChunkBatchPhase2Snapshot(const enigma::voxel::World& world)
    {
        ChunkBatchPhase2Snapshot snapshot;
        const auto&              storage = world.GetChunkRenderRegionStorage();

        snapshot.loadedChunks = static_cast<uint32_t>(world.GetLoadedChunkCount());
        snapshot.residentRegions = static_cast<uint32_t>(storage.GetRegions().size());
        snapshot.queuedDirtyRegions = storage.GetDirtyRegionCount();
        snapshot.replacementUploadsLifetime = storage.GetReplacementUploadCount();
        snapshot.replacementFallbacksLifetime = storage.GetReplacementFallbackCount();
        snapshot.vertexArenaCapacity = storage.GetVertexArenaCapacity();
        snapshot.vertexArenaRemaining = storage.GetVertexArenaRemainingCapacity();
        snapshot.indexArenaCapacity = storage.GetIndexArenaCapacity();
        snapshot.indexArenaRemaining = storage.GetIndexArenaRemainingCapacity();

        for (const auto& regionEntry : storage.GetRegions())
        {
            const auto& region = regionEntry.second;
            if (region.HasValidBatchGeometry())
            {
                snapshot.validBatchGeometryRegions++;
            }
            if (region.dirty)
            {
                snapshot.dirtyResidentRegions++;
            }
            if (region.buildFailed)
            {
                snapshot.buildFailedRegions++;
            }
        }

        return snapshot;
    }

    std::string BuildChunkBatchDebugInfo(const enigma::voxel::World& world)
    {
        const auto& stats = world.GetChunkBatchStats();
        const ChunkBatchPhase2Snapshot phase2Snapshot = CollectChunkBatchPhase2Snapshot(world);
        const float mainCullRatio = ComputeCullRatio(stats.visibleRegions, stats.culledRegions);
        const float shadowCullRatio = ComputeCullRatio(stats.shadowVisibleRegions, stats.shadowCulledRegions);
        const uint32_t vertexArenaUsed = phase2Snapshot.vertexArenaCapacity - phase2Snapshot.vertexArenaRemaining;
        const uint32_t indexArenaUsed = phase2Snapshot.indexArenaCapacity - phase2Snapshot.indexArenaRemaining;

        return Stringf(
            "{\n"
            "  \"chunkBatching\": {\n"
            "    \"runtime\": {\n"
            "      \"backend\": \"Direct region DrawIndexed\",\n"
            "      \"loadedChunks\": %u,\n"
            "      \"queuedDirtyRegions\": %u,\n"
            "      \"dirtyRegionBudget\": %u,\n"
            "      \"notes\": [\n"
            "        \"Chunk batching is always enabled.\",\n"
            "        \"Legacy per-chunk submission has been removed from runtime.\",\n"
            "        \"Dirty regions keep rendering the last committed region geometry until the rebuilt buffers are ready.\"\n"
            "      ]\n"
            "    },\n"
            "    \"phase2\": {\n"
            "      \"regionLayout\": {\n"
            "        \"sizeInChunks\": {\n"
            "          \"x\": %d,\n"
            "          \"y\": %d\n"
            "        },\n"
            "        \"residentRegions\": %u,\n"
            "        \"validBatchGeometryRegions\": %u,\n"
            "        \"dirtyResidentRegions\": %u,\n"
            "        \"queuedDirtyRegions\": %u,\n"
            "        \"buildFailedRegions\": %u\n"
            "      },\n"
            "      \"culling\": {\n"
            "        \"mainCamera\": {\n"
            "          \"visibleRegions\": %u,\n"
            "          \"culledRegions\": %u,\n"
            "          \"culledRatio\": %.4f\n"
            "        },\n"
            "        \"shadowCamera\": {\n"
            "          \"visibleRegions\": %u,\n"
            "          \"culledRegions\": %u,\n"
            "          \"culledRatio\": %.4f\n"
            "        }\n"
            "      },\n"
            "      \"sharedArena\": {\n"
            "        \"vertex\": {\n"
            "          \"capacity\": %u,\n"
            "          \"used\": %u,\n"
            "          \"remaining\": %u\n"
            "        },\n"
            "        \"index\": {\n"
            "          \"capacity\": %u,\n"
            "          \"used\": %u,\n"
            "          \"remaining\": %u\n"
            "        }\n"
            "      },\n"
            "      \"replacementUpload\": {\n"
            "        \"scope\": \"lifetime\",\n"
            "        \"appliedChunkUpdates\": %u,\n"
            "        \"fallbackToFullRegionRebuilds\": %u\n"
            "      }\n"
            "    },\n"
            "    \"frameStats\": {\n"
            "      \"visibleChunks\": %u,\n"
            "      \"mainVisibleRegions\": %u,\n"
            "      \"mainCulledRegions\": %u,\n"
            "      \"shadowVisibleRegions\": %u,\n"
            "      \"shadowCulledRegions\": %u,\n"
            "      \"batchedDraws\": %u,\n"
            "      \"dirtyRegionRebuilds\": {\n"
            "        \"current\": %u,\n"
            "        \"budget\": %u\n"
            "      }\n"
            "    }\n"
            "  }\n"
            "}\n",
            phase2Snapshot.loadedChunks,
            world.GetChunkRenderRegionStorage().GetDirtyRegionCount(),
            world.GetMaxChunkBatchRegionRebuildsPerFrame(),
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_X,
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_Y,
            phase2Snapshot.residentRegions,
            phase2Snapshot.validBatchGeometryRegions,
            phase2Snapshot.dirtyResidentRegions,
            phase2Snapshot.queuedDirtyRegions,
            phase2Snapshot.buildFailedRegions,
            stats.visibleRegions,
            stats.culledRegions,
            mainCullRatio,
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            shadowCullRatio,
            phase2Snapshot.vertexArenaCapacity,
            vertexArenaUsed,
            phase2Snapshot.vertexArenaRemaining,
            phase2Snapshot.indexArenaCapacity,
            indexArenaUsed,
            phase2Snapshot.indexArenaRemaining,
            phase2Snapshot.replacementUploadsLifetime,
            phase2Snapshot.replacementFallbacksLifetime,
            stats.visibleChunks,
            stats.visibleRegions,
            stats.culledRegions,
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            stats.batchedDraws,
            stats.dirtyRegionRebuilds,
            world.GetMaxChunkBatchRegionRebuildsPerFrame());
    }

    void CopyChunkBatchDebugInfoToClipboard(const enigma::voxel::World& world)
    {
        const std::string debugInfo = BuildChunkBatchDebugInfo(world);
        ImGui::SetClipboardText(debugInfo.c_str());
    }
}

void ImguiSettingChunkBatching::Show(enigma::voxel::World* world)
{
    static double s_lastCopyTime = -1000.0;

    if (!world)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] World is null");
        return;
    }

    const bool isOpen = ImGui::CollapsingHeader("Chunk Batching", ImGuiTreeNodeFlags_DefaultOpen);
    if (ImGui::BeginPopupContextItem("ChunkBatchingContextMenu"))
    {
        if (ImGui::MenuItem("Copy Debug Info"))
        {
            CopyChunkBatchDebugInfoToClipboard(*world);
            s_lastCopyTime = ImGui::GetTime();
        }
        ImGui::EndPopup();
    }

    if (isOpen)
    {
        ImGui::Indent();

        const auto& stats = world->GetChunkBatchStats();
        const ChunkBatchPhase2Snapshot phase2Snapshot = CollectChunkBatchPhase2Snapshot(*world);
        const float mainCullRatio = ComputeCullRatio(stats.visibleRegions, stats.culledRegions);
        const float shadowCullRatio = ComputeCullRatio(stats.shadowVisibleRegions, stats.shadowCulledRegions);
        const uint32_t vertexArenaUsed = phase2Snapshot.vertexArenaCapacity - phase2Snapshot.vertexArenaRemaining;
        const uint32_t indexArenaUsed = phase2Snapshot.indexArenaCapacity - phase2Snapshot.indexArenaRemaining;

        if (ImGui::Button("Copy Debug Info"))
        {
            CopyChunkBatchDebugInfoToClipboard(*world);
            s_lastCopyTime = ImGui::GetTime();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Copy the current chunk batching debug snapshot to the clipboard");
        }
        if ((ImGui::GetTime() - s_lastCopyTime) < 1.5)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Copied.");
        }

        ImGui::SeparatorText("Debug Draw");
        ImGui::Checkbox("Enable Region Wireframe", &ChunkBachingDebugViewState::enableRegionWireframe);
        ImGui::TextDisabled("Colors: green=visible, red=culled, yellow=dirty, magenta=build failed, gray=invalid");
        ImGui::SeparatorText("Culling Map");
        DrawChunkBatchCullingMap(*world);

        ImGui::TextDisabled("Chunk batching is always enabled.");
        ImGui::TextDisabled("Legacy per-chunk submission has been removed from runtime.");

        ImGui::SeparatorText("Runtime");
        ImGui::Text("Backend: Direct region DrawIndexed");
        ImGui::Text("Loaded Chunks: %u", phase2Snapshot.loadedChunks);
        ImGui::Text("Queued Dirty Regions: %u", world->GetChunkRenderRegionStorage().GetDirtyRegionCount());
        ImGui::Text("Dirty Region Budget: %u", world->GetMaxChunkBatchRegionRebuildsPerFrame());
        ImGui::TextWrapped("Dirty regions keep rendering the last committed region geometry until the rebuilt buffers are ready.");

        ImGui::SeparatorText("Frame Stats");
        ImGui::Text("Visible Chunks: %u", stats.visibleChunks);
        ImGui::Text("Main Visible Regions: %u", stats.visibleRegions);
        ImGui::Text("Main Culled Regions: %u", stats.culledRegions);
        ImGui::Text("Shadow Visible Regions: %u", stats.shadowVisibleRegions);
        ImGui::Text("Shadow Culled Regions: %u", stats.shadowCulledRegions);
        ImGui::Text("Batched Draws: %u", stats.batchedDraws);
        ImGui::Text("Dirty Region Rebuilds: %u / %u",
            stats.dirtyRegionRebuilds,
            world->GetMaxChunkBatchRegionRebuildsPerFrame());

        ImGui::SeparatorText("Phase 2");
        ImGui::Text("Region Size: %d x %d chunks",
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_X,
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_Y);
        ImGui::Text("Resident Regions: %u", phase2Snapshot.residentRegions);
        ImGui::Text("Valid Batch Regions: %u", phase2Snapshot.validBatchGeometryRegions);
        ImGui::Text("Dirty Resident Regions: %u", phase2Snapshot.dirtyResidentRegions);
        ImGui::Text("Build Failed Regions: %u", phase2Snapshot.buildFailedRegions);
        ImGui::Text("Main Culling: %u visible / %u culled (%.1f%% culled)",
            stats.visibleRegions,
            stats.culledRegions,
            mainCullRatio * 100.0f);
        ImGui::Text("Shadow Culling: %u visible / %u culled (%.1f%% culled)",
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            shadowCullRatio * 100.0f);
        ImGui::Text("Replacement Uploads (lifetime): %u", phase2Snapshot.replacementUploadsLifetime);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Dirty chunk updates that stayed within reserved arena capacity and avoided a full region rebuild");
        }
        ImGui::Text("Replacement Fallbacks (lifetime): %u", phase2Snapshot.replacementFallbacksLifetime);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Dirty chunk updates that could not be replaced in-place and fell back to a full region rebuild");
        }
        ImGui::Text("Vertex Arena: %u used / %u total (%u free)",
            vertexArenaUsed,
            phase2Snapshot.vertexArenaCapacity,
            phase2Snapshot.vertexArenaRemaining);
        ImGui::Text("Index Arena: %u used / %u total (%u free)",
            indexArenaUsed,
            phase2Snapshot.indexArenaCapacity,
            phase2Snapshot.indexArenaRemaining);
        ImGui::TextDisabled("Phase 2 replacement counters are cumulative since startup.");

        ImGui::Unindent();
    }
}
