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
    constexpr char  CHUNK_BATCH_ACTIVE_BACKEND_LABEL[] = "Direct precise sub-draw DrawIndexed";
    double          s_lastCopyTime = -1000.0;
    const char*     s_lastCopyLabel = nullptr;

    struct ChunkBatchArenaSideSnapshot
    {
        uint32_t growCountLifetime = 0;
        uint32_t relocationCountLifetime = 0;
        uint32_t relocatedElementCountLifetime = 0;
        uint32_t lastRelocationElementCount = 0;
        uint32_t lastRelocationSpanCount = 0;
        uint32_t lastRequestedCapacity = 0;
        uint32_t lastCapacityBeforeGrow = 0;
        uint32_t lastCapacityAfterGrow = 0;
    };

    struct ChunkBatchFallbackSnapshot
    {
        uint32_t totalCountLifetime = 0;
        uint32_t replacementStateInvalidCountLifetime = 0;
        uint32_t replacementVertexOverflowCountLifetime = 0;
        uint32_t replacementIndexOverflowCountLifetime = 0;
        uint32_t replacementVertexUploadFailureCountLifetime = 0;
        uint32_t replacementIndexUploadFailureCountLifetime = 0;
        uint32_t vertexArenaGrowFailureCountLifetime = 0;
        uint32_t indexArenaGrowFailureCountLifetime = 0;
        uint32_t vertexArenaUploadFailureCountLifetime = 0;
        uint32_t indexArenaUploadFailureCountLifetime = 0;
        enigma::voxel::ChunkBatchArenaFallbackReason lastReason = enigma::voxel::ChunkBatchArenaFallbackReason::None;
    };

    struct ChunkBatchingDataSnapshot
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
        uint32_t residentOpaqueSubDraws = 0;
        uint32_t residentCutoutSubDraws = 0;
        uint32_t residentTranslucentSubDraws = 0;
        ChunkBatchArenaSideSnapshot vertexArenaDiagnostics;
        ChunkBatchArenaSideSnapshot indexArenaDiagnostics;
        ChunkBatchFallbackSnapshot fallbackDiagnostics;
    };

    const char* GetChunkBatchFallbackReasonLabel(enigma::voxel::ChunkBatchArenaFallbackReason reason)
    {
        using enigma::voxel::ChunkBatchArenaFallbackReason;

        switch (reason)
        {
        case ChunkBatchArenaFallbackReason::None: return "None";
        case ChunkBatchArenaFallbackReason::ReplacementStateInvalid: return "ReplacementStateInvalid";
        case ChunkBatchArenaFallbackReason::ReplacementVertexOverflow: return "ReplacementVertexOverflow";
        case ChunkBatchArenaFallbackReason::ReplacementIndexOverflow: return "ReplacementIndexOverflow";
        case ChunkBatchArenaFallbackReason::ReplacementVertexUploadFailed: return "ReplacementVertexUploadFailed";
        case ChunkBatchArenaFallbackReason::ReplacementIndexUploadFailed: return "ReplacementIndexUploadFailed";
        case ChunkBatchArenaFallbackReason::VertexArenaGrowFailed: return "VertexArenaGrowFailed";
        case ChunkBatchArenaFallbackReason::IndexArenaGrowFailed: return "IndexArenaGrowFailed";
        case ChunkBatchArenaFallbackReason::VertexArenaUploadFailed: return "VertexArenaUploadFailed";
        case ChunkBatchArenaFallbackReason::IndexArenaUploadFailed: return "IndexArenaUploadFailed";
        default: return "Unknown";
        }
    }

    void CopyArenaSideSnapshot(
        ChunkBatchArenaSideSnapshot&                              destination,
        const enigma::voxel::ChunkBatchArenaSideDiagnostics&      source)
    {
        destination.growCountLifetime = source.growCountLifetime;
        destination.relocationCountLifetime = source.relocationCountLifetime;
        destination.relocatedElementCountLifetime = source.relocatedElementCountLifetime;
        destination.lastRelocationElementCount = source.lastRelocationElementCount;
        destination.lastRelocationSpanCount = source.lastRelocationSpanCount;
        destination.lastRequestedCapacity = source.lastRequestedCapacity;
        destination.lastCapacityBeforeGrow = source.lastCapacityBeforeGrow;
        destination.lastCapacityAfterGrow = source.lastCapacityAfterGrow;
    }

    void CopyFallbackSnapshot(
        ChunkBatchFallbackSnapshot&                               destination,
        const enigma::voxel::ChunkBatchArenaFallbackDiagnostics&  source)
    {
        destination.totalCountLifetime = source.totalCountLifetime;
        destination.replacementStateInvalidCountLifetime = source.replacementStateInvalidCountLifetime;
        destination.replacementVertexOverflowCountLifetime = source.replacementVertexOverflowCountLifetime;
        destination.replacementIndexOverflowCountLifetime = source.replacementIndexOverflowCountLifetime;
        destination.replacementVertexUploadFailureCountLifetime = source.replacementVertexUploadFailureCountLifetime;
        destination.replacementIndexUploadFailureCountLifetime = source.replacementIndexUploadFailureCountLifetime;
        destination.vertexArenaGrowFailureCountLifetime = source.vertexArenaGrowFailureCountLifetime;
        destination.indexArenaGrowFailureCountLifetime = source.indexArenaGrowFailureCountLifetime;
        destination.vertexArenaUploadFailureCountLifetime = source.vertexArenaUploadFailureCountLifetime;
        destination.indexArenaUploadFailureCountLifetime = source.indexArenaUploadFailureCountLifetime;
        destination.lastReason = source.lastReason;
    }

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

    ChunkBatchingDataSnapshot CollectChunkBatchingDataSnapshot(const enigma::voxel::World& world)
    {
        ChunkBatchingDataSnapshot snapshot;
        const auto&               storage = world.GetChunkRenderRegionStorage();
        const auto&               arenaDiagnostics = storage.GetArenaDiagnostics();

        snapshot.loadedChunks = static_cast<uint32_t>(world.GetLoadedChunkCount());
        snapshot.residentRegions = static_cast<uint32_t>(storage.GetRegions().size());
        snapshot.queuedDirtyRegions = storage.GetDirtyRegionCount();
        snapshot.replacementUploadsLifetime = storage.GetReplacementUploadCount();
        snapshot.replacementFallbacksLifetime = storage.GetReplacementFallbackCount();
        snapshot.vertexArenaCapacity = storage.GetVertexArenaCapacity();
        snapshot.vertexArenaRemaining = storage.GetVertexArenaRemainingCapacity();
        snapshot.indexArenaCapacity = storage.GetIndexArenaCapacity();
        snapshot.indexArenaRemaining = storage.GetIndexArenaRemainingCapacity();
        CopyArenaSideSnapshot(snapshot.vertexArenaDiagnostics, arenaDiagnostics.vertex);
        CopyArenaSideSnapshot(snapshot.indexArenaDiagnostics, arenaDiagnostics.index);
        CopyFallbackSnapshot(snapshot.fallbackDiagnostics, arenaDiagnostics.fallback);

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

            snapshot.residentOpaqueSubDraws += static_cast<uint32_t>(region.geometry.opaqueSubDraws.size());
            snapshot.residentCutoutSubDraws += static_cast<uint32_t>(region.geometry.cutoutSubDraws.size());
            snapshot.residentTranslucentSubDraws += static_cast<uint32_t>(region.geometry.translucentSubDraws.size());
        }

        return snapshot;
    }

    std::string BuildChunkBatchDebugInfo(const enigma::voxel::World& world)
    {
        const auto& stats = world.GetChunkBatchStats();
        const ChunkBatchingDataSnapshot batchingSnapshot = CollectChunkBatchingDataSnapshot(world);
        const float mainCullRatio = ComputeCullRatio(stats.visibleRegions, stats.culledRegions);
        const float shadowCullRatio = ComputeCullRatio(stats.shadowVisibleRegions, stats.shadowCulledRegions);
        const uint32_t vertexArenaUsed = batchingSnapshot.vertexArenaCapacity - batchingSnapshot.vertexArenaRemaining;
        const uint32_t indexArenaUsed = batchingSnapshot.indexArenaCapacity - batchingSnapshot.indexArenaRemaining;
        const uint32_t residentPreciseSubDraws = batchingSnapshot.residentOpaqueSubDraws +
            batchingSnapshot.residentCutoutSubDraws +
            batchingSnapshot.residentTranslucentSubDraws;
        const auto& vertexDiagnostics = batchingSnapshot.vertexArenaDiagnostics;
        const auto& indexDiagnostics = batchingSnapshot.indexArenaDiagnostics;
        const auto& fallbackDiagnostics = batchingSnapshot.fallbackDiagnostics;
        const char* lastFallbackReasonLabel = GetChunkBatchFallbackReasonLabel(fallbackDiagnostics.lastReason);

        return Stringf(
            "{\n"
            "  \"chunkBatching\": {\n"
            "    \"runtime\": {\n"
            "      \"backend\": \"%s\",\n"
            "      \"loadedChunks\": %u,\n"
            "      \"queuedDirtyRegions\": %u,\n"
            "      \"dirtyRegionBudget\": %u,\n"
            "      \"notes\": [\n"
            "        \"Chunk batching is always enabled.\",\n"
            "        \"Legacy per-chunk submission has been removed from runtime.\",\n"
            "        \"Direct precise submission consumes exact sub-draw ranges and no longer submits reserved padding spans.\",\n"
            "        \"Dirty regions keep rendering the last committed region geometry until the rebuilt buffers are ready.\"\n"
            "      ]\n"
            "    },\n"
            "    \"batchingData\": {\n"
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
            "      \"submission\": {\n"
            "        \"residentOpaqueSubDraws\": %u,\n"
            "        \"residentCutoutSubDraws\": %u,\n"
            "        \"residentTranslucentSubDraws\": %u,\n"
            "        \"residentPreciseSubDraws\": %u\n"
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
            "      \"arenaRelocation\": {\n"
            "        \"vertex\": {\n"
            "          \"growCountLifetime\": %u,\n"
            "          \"relocationCountLifetime\": %u,\n"
            "          \"relocatedElementsLifetime\": %u,\n"
            "          \"lastRelocation\": {\n"
            "            \"elements\": %u,\n"
            "            \"copySpans\": %u,\n"
            "            \"requestedCapacity\": %u,\n"
            "            \"capacityBeforeGrow\": %u,\n"
            "            \"capacityAfterGrow\": %u\n"
            "          }\n"
            "        },\n"
            "        \"index\": {\n"
            "          \"growCountLifetime\": %u,\n"
            "          \"relocationCountLifetime\": %u,\n"
            "          \"relocatedElementsLifetime\": %u,\n"
            "          \"lastRelocation\": {\n"
            "            \"elements\": %u,\n"
            "            \"copySpans\": %u,\n"
            "            \"requestedCapacity\": %u,\n"
            "            \"capacityBeforeGrow\": %u,\n"
            "            \"capacityAfterGrow\": %u\n"
            "          }\n"
            "        }\n"
            "      },\n"
            "      \"fallbacks\": {\n"
            "        \"scope\": \"lifetime\",\n"
            "        \"totalEvents\": %u,\n"
            "        \"lastReason\": \"%s\",\n"
            "        \"categories\": {\n"
            "          \"replacementStateInvalid\": %u,\n"
            "          \"replacementVertexOverflow\": %u,\n"
            "          \"replacementIndexOverflow\": %u,\n"
            "          \"replacementVertexUploadFailed\": %u,\n"
            "          \"replacementIndexUploadFailed\": %u,\n"
            "          \"vertexArenaGrowFailed\": %u,\n"
            "          \"indexArenaGrowFailed\": %u,\n"
            "          \"vertexArenaUploadFailed\": %u,\n"
            "          \"indexArenaUploadFailed\": %u\n"
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
            CHUNK_BATCH_ACTIVE_BACKEND_LABEL,
            batchingSnapshot.loadedChunks,
            world.GetChunkRenderRegionStorage().GetDirtyRegionCount(),
            world.GetMaxChunkBatchRegionRebuildsPerFrame(),
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_X,
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_Y,
            batchingSnapshot.residentRegions,
            batchingSnapshot.validBatchGeometryRegions,
            batchingSnapshot.dirtyResidentRegions,
            batchingSnapshot.queuedDirtyRegions,
            batchingSnapshot.buildFailedRegions,
            stats.visibleRegions,
            stats.culledRegions,
            mainCullRatio,
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            shadowCullRatio,
            batchingSnapshot.residentOpaqueSubDraws,
            batchingSnapshot.residentCutoutSubDraws,
            batchingSnapshot.residentTranslucentSubDraws,
            residentPreciseSubDraws,
            batchingSnapshot.vertexArenaCapacity,
            vertexArenaUsed,
            batchingSnapshot.vertexArenaRemaining,
            batchingSnapshot.indexArenaCapacity,
            indexArenaUsed,
            batchingSnapshot.indexArenaRemaining,
            vertexDiagnostics.growCountLifetime,
            vertexDiagnostics.relocationCountLifetime,
            vertexDiagnostics.relocatedElementCountLifetime,
            vertexDiagnostics.lastRelocationElementCount,
            vertexDiagnostics.lastRelocationSpanCount,
            vertexDiagnostics.lastRequestedCapacity,
            vertexDiagnostics.lastCapacityBeforeGrow,
            vertexDiagnostics.lastCapacityAfterGrow,
            indexDiagnostics.growCountLifetime,
            indexDiagnostics.relocationCountLifetime,
            indexDiagnostics.relocatedElementCountLifetime,
            indexDiagnostics.lastRelocationElementCount,
            indexDiagnostics.lastRelocationSpanCount,
            indexDiagnostics.lastRequestedCapacity,
            indexDiagnostics.lastCapacityBeforeGrow,
            indexDiagnostics.lastCapacityAfterGrow,
            fallbackDiagnostics.totalCountLifetime,
            lastFallbackReasonLabel,
            fallbackDiagnostics.replacementStateInvalidCountLifetime,
            fallbackDiagnostics.replacementVertexOverflowCountLifetime,
            fallbackDiagnostics.replacementIndexOverflowCountLifetime,
            fallbackDiagnostics.replacementVertexUploadFailureCountLifetime,
            fallbackDiagnostics.replacementIndexUploadFailureCountLifetime,
            fallbackDiagnostics.vertexArenaGrowFailureCountLifetime,
            fallbackDiagnostics.indexArenaGrowFailureCountLifetime,
            fallbackDiagnostics.vertexArenaUploadFailureCountLifetime,
            fallbackDiagnostics.indexArenaUploadFailureCountLifetime,
            batchingSnapshot.replacementUploadsLifetime,
            batchingSnapshot.replacementFallbacksLifetime,
            stats.visibleChunks,
            stats.visibleRegions,
            stats.culledRegions,
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            stats.batchedDraws,
            stats.dirtyRegionRebuilds,
            world.GetMaxChunkBatchRegionRebuildsPerFrame());
    }

    std::string BuildChunkBatchFrameDebugInfo(const enigma::voxel::World& world)
    {
        const auto& stats = world.GetChunkBatchStats();
        const ChunkBatchingDataSnapshot batchingSnapshot = CollectChunkBatchingDataSnapshot(world);
        const float mainCullRatio = ComputeCullRatio(stats.visibleRegions, stats.culledRegions);
        const float shadowCullRatio = ComputeCullRatio(stats.shadowVisibleRegions, stats.shadowCulledRegions);
        const uint32_t residentPreciseSubDraws = batchingSnapshot.residentOpaqueSubDraws +
            batchingSnapshot.residentCutoutSubDraws +
            batchingSnapshot.residentTranslucentSubDraws;

        return Stringf(
            "{\n"
            "  \"chunkBatching\": {\n"
            "    \"scope\": \"frame\",\n"
            "    \"loadedChunks\": %u,\n"
            "    \"residentRegions\": %u,\n"
            "    \"queuedDirtyRegions\": %u,\n"
            "    \"frameStats\": {\n"
            "      \"visibleChunks\": %u,\n"
            "      \"mainVisibleRegions\": %u,\n"
            "      \"mainCulledRegions\": %u,\n"
            "      \"mainCullRatio\": %.4f,\n"
            "      \"shadowVisibleRegions\": %u,\n"
            "      \"shadowCulledRegions\": %u,\n"
            "      \"shadowCullRatio\": %.4f,\n"
            "      \"batchedDraws\": %u,\n"
            "      \"dirtyRegionRebuilds\": %u,\n"
            "      \"dirtyRegionBudget\": %u\n"
            "    },\n"
            "    \"residentSubDraws\": {\n"
            "      \"opaque\": %u,\n"
            "      \"cutout\": %u,\n"
            "      \"translucent\": %u,\n"
            "      \"preciseTotal\": %u\n"
            "    }\n"
            "  }\n"
            "}\n",
            batchingSnapshot.loadedChunks,
            batchingSnapshot.residentRegions,
            batchingSnapshot.queuedDirtyRegions,
            stats.visibleChunks,
            stats.visibleRegions,
            stats.culledRegions,
            mainCullRatio,
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            shadowCullRatio,
            stats.batchedDraws,
            stats.dirtyRegionRebuilds,
            world.GetMaxChunkBatchRegionRebuildsPerFrame(),
            batchingSnapshot.residentOpaqueSubDraws,
            batchingSnapshot.residentCutoutSubDraws,
            batchingSnapshot.residentTranslucentSubDraws,
            residentPreciseSubDraws);
    }

    std::string BuildChunkBatchLifetimeDebugInfo(const enigma::voxel::World& world)
    {
        const ChunkBatchingDataSnapshot batchingSnapshot = CollectChunkBatchingDataSnapshot(world);
        const auto& vertexDiagnostics = batchingSnapshot.vertexArenaDiagnostics;
        const auto& indexDiagnostics = batchingSnapshot.indexArenaDiagnostics;
        const auto& fallbackDiagnostics = batchingSnapshot.fallbackDiagnostics;
        const char* lastFallbackReasonLabel = GetChunkBatchFallbackReasonLabel(fallbackDiagnostics.lastReason);

        return Stringf(
            "{\n"
            "  \"chunkBatching\": {\n"
            "    \"scope\": \"lifetime\",\n"
            "    \"replacementUploads\": %u,\n"
            "    \"replacementFallbacks\": %u,\n"
            "    \"arenaRelocation\": {\n"
            "      \"vertex\": {\n"
            "        \"growCount\": %u,\n"
            "        \"relocationCount\": %u,\n"
            "        \"relocatedElements\": %u\n"
            "      },\n"
            "      \"index\": {\n"
            "        \"growCount\": %u,\n"
            "        \"relocationCount\": %u,\n"
            "        \"relocatedElements\": %u\n"
            "      }\n"
            "    },\n"
            "    \"fallbacks\": {\n"
            "      \"totalEvents\": %u,\n"
            "      \"lastReason\": \"%s\",\n"
            "      \"replacementStateInvalid\": %u,\n"
            "      \"replacementVertexOverflow\": %u,\n"
            "      \"replacementIndexOverflow\": %u,\n"
            "      \"replacementVertexUploadFailed\": %u,\n"
            "      \"replacementIndexUploadFailed\": %u,\n"
            "      \"vertexArenaGrowFailed\": %u,\n"
            "      \"indexArenaGrowFailed\": %u,\n"
            "      \"vertexArenaUploadFailed\": %u,\n"
            "      \"indexArenaUploadFailed\": %u\n"
            "    }\n"
            "  }\n"
            "}\n",
            batchingSnapshot.replacementUploadsLifetime,
            batchingSnapshot.replacementFallbacksLifetime,
            vertexDiagnostics.growCountLifetime,
            vertexDiagnostics.relocationCountLifetime,
            vertexDiagnostics.relocatedElementCountLifetime,
            indexDiagnostics.growCountLifetime,
            indexDiagnostics.relocationCountLifetime,
            indexDiagnostics.relocatedElementCountLifetime,
            fallbackDiagnostics.totalCountLifetime,
            lastFallbackReasonLabel,
            fallbackDiagnostics.replacementStateInvalidCountLifetime,
            fallbackDiagnostics.replacementVertexOverflowCountLifetime,
            fallbackDiagnostics.replacementIndexOverflowCountLifetime,
            fallbackDiagnostics.replacementVertexUploadFailureCountLifetime,
            fallbackDiagnostics.replacementIndexUploadFailureCountLifetime,
            fallbackDiagnostics.vertexArenaGrowFailureCountLifetime,
            fallbackDiagnostics.indexArenaGrowFailureCountLifetime,
            fallbackDiagnostics.vertexArenaUploadFailureCountLifetime,
            fallbackDiagnostics.indexArenaUploadFailureCountLifetime);
    }

    void CopyChunkBatchDebugInfoToClipboard(const enigma::voxel::World& world)
    {
        const std::string debugInfo = BuildChunkBatchDebugInfo(world);
        ImGui::SetClipboardText(debugInfo.c_str());
    }

    void CopyChunkBatchFrameDebugInfoToClipboard(const enigma::voxel::World& world)
    {
        const std::string debugInfo = BuildChunkBatchFrameDebugInfo(world);
        ImGui::SetClipboardText(debugInfo.c_str());
    }

    void CopyChunkBatchLifetimeDebugInfoToClipboard(const enigma::voxel::World& world)
    {
        const std::string debugInfo = BuildChunkBatchLifetimeDebugInfo(world);
        ImGui::SetClipboardText(debugInfo.c_str());
    }

    void RecordChunkBatchCopy(const char* label)
    {
        s_lastCopyTime = ImGui::GetTime();
        s_lastCopyLabel = label;
    }
}

void ImguiSettingChunkBatching::Show(enigma::voxel::World* world)
{
    if (!world)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[ERROR] World is null");
        return;
    }

    const auto& stats = world->GetChunkBatchStats();
    const ChunkBatchingDataSnapshot batchingSnapshot = CollectChunkBatchingDataSnapshot(*world);
    const float mainCullRatio = ComputeCullRatio(stats.visibleRegions, stats.culledRegions);
    const float shadowCullRatio = ComputeCullRatio(stats.shadowVisibleRegions, stats.shadowCulledRegions);
    const uint32_t vertexArenaUsed = batchingSnapshot.vertexArenaCapacity - batchingSnapshot.vertexArenaRemaining;
    const uint32_t indexArenaUsed = batchingSnapshot.indexArenaCapacity - batchingSnapshot.indexArenaRemaining;
    const uint32_t residentPreciseSubDraws = batchingSnapshot.residentOpaqueSubDraws +
        batchingSnapshot.residentCutoutSubDraws +
        batchingSnapshot.residentTranslucentSubDraws;
    const auto& vertexDiagnostics = batchingSnapshot.vertexArenaDiagnostics;
    const auto& indexDiagnostics = batchingSnapshot.indexArenaDiagnostics;
    const auto& fallbackDiagnostics = batchingSnapshot.fallbackDiagnostics;

    ImGui::TextDisabled("Right-click inside the Chunk Batching tab to copy JSON snapshots.");
    if ((ImGui::GetTime() - s_lastCopyTime) < 1.5)
    {
        ImGui::TextDisabled("%s", s_lastCopyLabel ? s_lastCopyLabel : "Copied.");
    }

    if (ImGui::CollapsingHeader("Runtime", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Backend: %s", CHUNK_BATCH_ACTIVE_BACKEND_LABEL);
        ImGui::Text("Loaded Chunks: %u", batchingSnapshot.loadedChunks);
        ImGui::Text("Queued Dirty Regions: %u", world->GetChunkRenderRegionStorage().GetDirtyRegionCount());
        ImGui::Text("Dirty Region Budget: %u", world->GetMaxChunkBatchRegionRebuildsPerFrame());
        ImGui::TextDisabled("Chunk batching is always enabled.");
        ImGui::TextDisabled("Legacy per-chunk submission has been removed from runtime.");
        ImGui::TextWrapped("Direct precise submission consumes exact sub-draw ranges while dirty regions keep the last committed region geometry until rebuilt buffers are ready.");
    }

    if (ImGui::CollapsingHeader("Frame Stats", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Visible Chunks: %u", stats.visibleChunks);
        ImGui::Text("Main Visible Regions: %u", stats.visibleRegions);
        ImGui::Text("Main Culled Regions: %u", stats.culledRegions);
        ImGui::Text("Shadow Visible Regions: %u", stats.shadowVisibleRegions);
        ImGui::Text("Shadow Culled Regions: %u", stats.shadowCulledRegions);
        ImGui::Text("Exact Batched Draws: %u", stats.batchedDraws);
        ImGui::Text("Dirty Region Rebuilds: %u / %u",
            stats.dirtyRegionRebuilds,
            world->GetMaxChunkBatchRegionRebuildsPerFrame());
    }

    if (ImGui::CollapsingHeader("Batching Data", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Region Size: %d x %d chunks",
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_X,
            enigma::voxel::CHUNK_BATCH_REGION_SIZE_Y);
        ImGui::Text("Resident Regions: %u", batchingSnapshot.residentRegions);
        ImGui::Text("Valid Batch Regions: %u", batchingSnapshot.validBatchGeometryRegions);
        ImGui::Text("Dirty Resident Regions: %u", batchingSnapshot.dirtyResidentRegions);
        ImGui::Text("Build Failed Regions: %u", batchingSnapshot.buildFailedRegions);
        ImGui::Text("Resident Precise Sub-Draws: %u", residentPreciseSubDraws);
        ImGui::Text("Resident Opaque Sub-Draws: %u", batchingSnapshot.residentOpaqueSubDraws);
        ImGui::Text("Resident Cutout Sub-Draws: %u", batchingSnapshot.residentCutoutSubDraws);
        ImGui::Text("Resident Translucent Sub-Draws: %u", batchingSnapshot.residentTranslucentSubDraws);
        ImGui::Text("Main Culling: %u visible / %u culled (%.1f%% culled)",
            stats.visibleRegions,
            stats.culledRegions,
            mainCullRatio * 100.0f);
        ImGui::Text("Shadow Culling: %u visible / %u culled (%.1f%% culled)",
            stats.shadowVisibleRegions,
            stats.shadowCulledRegions,
            shadowCullRatio * 100.0f);
        ImGui::Text("Replacement Uploads (lifetime): %u", batchingSnapshot.replacementUploadsLifetime);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Dirty chunk updates that stayed within reserved arena capacity and avoided a full region rebuild");
        }
        ImGui::Text("Replacement Fallbacks (lifetime): %u", batchingSnapshot.replacementFallbacksLifetime);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Dirty chunk updates that could not be replaced in-place and fell back to a full region rebuild");
        }
        ImGui::Text("Storage Fallback Events (lifetime): %u", fallbackDiagnostics.totalCountLifetime);
        ImGui::Text("Last Fallback Reason: %s", GetChunkBatchFallbackReasonLabel(fallbackDiagnostics.lastReason));
        ImGui::Text("Vertex Arena: %u used / %u total (%u free)",
            vertexArenaUsed,
            batchingSnapshot.vertexArenaCapacity,
            batchingSnapshot.vertexArenaRemaining);
        ImGui::Text("Index Arena: %u used / %u total (%u free)",
            indexArenaUsed,
            batchingSnapshot.indexArenaCapacity,
            batchingSnapshot.indexArenaRemaining);
        ImGui::Separator();
        ImGui::Text("Vertex Relocation: %u grows / %u relocations / %u relocated elements (lifetime)",
            vertexDiagnostics.growCountLifetime,
            vertexDiagnostics.relocationCountLifetime,
            vertexDiagnostics.relocatedElementCountLifetime);
        ImGui::Text("Vertex Last Relocation: %u elements across %u spans, request %u, %u -> %u",
            vertexDiagnostics.lastRelocationElementCount,
            vertexDiagnostics.lastRelocationSpanCount,
            vertexDiagnostics.lastRequestedCapacity,
            vertexDiagnostics.lastCapacityBeforeGrow,
            vertexDiagnostics.lastCapacityAfterGrow);
        ImGui::Text("Index Relocation: %u grows / %u relocations / %u relocated elements (lifetime)",
            indexDiagnostics.growCountLifetime,
            indexDiagnostics.relocationCountLifetime,
            indexDiagnostics.relocatedElementCountLifetime);
        ImGui::Text("Index Last Relocation: %u elements across %u spans, request %u, %u -> %u",
            indexDiagnostics.lastRelocationElementCount,
            indexDiagnostics.lastRelocationSpanCount,
            indexDiagnostics.lastRequestedCapacity,
            indexDiagnostics.lastCapacityBeforeGrow,
            indexDiagnostics.lastCapacityAfterGrow);
        ImGui::Separator();
        ImGui::Text("Fallback Categories:");
        ImGui::Text("  Replacement State Invalid: %u", fallbackDiagnostics.replacementStateInvalidCountLifetime);
        ImGui::Text("  Replacement Vertex Overflow: %u", fallbackDiagnostics.replacementVertexOverflowCountLifetime);
        ImGui::Text("  Replacement Index Overflow: %u", fallbackDiagnostics.replacementIndexOverflowCountLifetime);
        ImGui::Text("  Replacement Vertex Upload Failed: %u", fallbackDiagnostics.replacementVertexUploadFailureCountLifetime);
        ImGui::Text("  Replacement Index Upload Failed: %u", fallbackDiagnostics.replacementIndexUploadFailureCountLifetime);
        ImGui::Text("  Vertex Arena Grow Failed: %u", fallbackDiagnostics.vertexArenaGrowFailureCountLifetime);
        ImGui::Text("  Index Arena Grow Failed: %u", fallbackDiagnostics.indexArenaGrowFailureCountLifetime);
        ImGui::Text("  Vertex Arena Upload Failed: %u", fallbackDiagnostics.vertexArenaUploadFailureCountLifetime);
        ImGui::Text("  Index Arena Upload Failed: %u", fallbackDiagnostics.indexArenaUploadFailureCountLifetime);
        ImGui::TextDisabled("Replacement counters are cumulative since startup.");
    }

    if (ImGui::CollapsingHeader("Debug Views", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable Region Wireframe", &ChunkBachingDebugViewState::enableRegionWireframe);
        ImGui::TextDisabled("Colors: green=visible, red=culled, yellow=dirty, magenta=build failed, gray=invalid");

        if (ImGui::CollapsingHeader("Culling Map", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawChunkBatchCullingMap(*world);
        }
    }
}

void ImguiSettingChunkBatching::CopyFullSnapshotToClipboard(const enigma::voxel::World& world)
{
    CopyChunkBatchDebugInfoToClipboard(world);
    RecordChunkBatchCopy("Full snapshot copied.");
}

void ImguiSettingChunkBatching::CopyFrameSnapshotToClipboard(const enigma::voxel::World& world)
{
    CopyChunkBatchFrameDebugInfoToClipboard(world);
    RecordChunkBatchCopy("Frame snapshot copied.");
}

void ImguiSettingChunkBatching::CopyLifetimeSnapshotToClipboard(const enigma::voxel::World& world)
{
    CopyChunkBatchLifetimeDebugInfoToClipboard(world);
    RecordChunkBatchCopy("Lifetime snapshot copied.");
}
