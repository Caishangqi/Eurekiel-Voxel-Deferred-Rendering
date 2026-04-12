/**
 * @file ImguiQueueDiagnosticsPanel.cpp
 * @brief ImGui sub-panel for multi-queue execution diagnostics
 */

#include "ImguiQueueDiagnosticsPanel.hpp"

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/CommandQueueTypes.hpp"
#include "Engine/Voxel/Chunk/MeshBuild/AsyncChunkMeshDiagnostics.hpp"
#include "Engine/Voxel/World/World.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/implot/implot.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    using enigma::core::Json;
    using enigma::graphic::CommandQueueType;
    using enigma::graphic::FrameLifecyclePhase;
    using enigma::graphic::QueueExecutionDiagnostics;
    using enigma::graphic::QueueExecutionMode;
    using enigma::graphic::QueueFallbackReason;
    using enigma::graphic::QueueWorkloadClass;
    using enigma::voxel::AsyncChunkMeshCounters;
    using enigma::voxel::AsyncChunkMeshDiagnostics;
    using enigma::voxel::AsyncChunkMeshLiveState;
    using enigma::voxel::AsyncChunkMeshMode;

    constexpr size_t kQueueTypeCount = 3;
    constexpr size_t kQueueWorkloadCount = enigma::graphic::kQueueWorkloadClassCount;
    constexpr size_t kHistoryLength = 240;
    constexpr size_t kVisibleHistoryLength = 120;
    constexpr float  kFallbackPulseValue = 7.0f;
    constexpr float  kAsyncFallbackPulseValue = 6.0f;

    using QueueCountArray = std::array<uint64_t, kQueueTypeCount>;
    using WorkloadSubmissionMatrix = std::array<QueueCountArray, kQueueWorkloadCount>;
    using QueueWaitMatrix = std::array<QueueCountArray, kQueueTypeCount>;
    using HistorySeries = std::array<float, kHistoryLength>;

    struct QueueExecutionFrameSnapshot
    {
        int                 imguiFrame = -1;
        QueueExecutionMode  requestedMode = QueueExecutionMode::GraphicsOnly;
        QueueExecutionMode  activeMode = QueueExecutionMode::GraphicsOnly;
        FrameLifecyclePhase lifecyclePhase = FrameLifecyclePhase::Idle;
        QueueFallbackReason lastFallbackReason = QueueFallbackReason::None;
        uint32_t            requestedFramesInFlightDepth = 0;
        uint32_t            activeFramesInFlightDepth = 0;
        uint64_t            graphicsSubmissions = 0;
        uint64_t            computeSubmissions = 0;
        uint64_t            copySubmissions = 0;
        uint64_t            queueWaitInsertions = 0;
        WorkloadSubmissionMatrix workloadSubmissionsByQueue = {};
        QueueWaitMatrix          queueWaitsByProducerConsumer = {};
    };

    struct QueueExecutionHistory
    {
        int           sampleCount = 0;
        float         nextSampleIndex = 0.0f;
        HistorySeries sampleIndices = {};
        HistorySeries graphicsSubmissions = {};
        HistorySeries computeSubmissions = {};
        HistorySeries copySubmissions = {};
        HistorySeries graphicsSharePercent = {};
        HistorySeries computeSharePercent = {};
        HistorySeries copySharePercent = {};
        HistorySeries graphicsQueueWaits = {};
        HistorySeries computeQueueWaits = {};
        HistorySeries copyQueueWaits = {};
        HistorySeries totalQueueWaitInsertions = {};
        HistorySeries copyReadyUploads = {};
        HistorySeries mipmapGeneration = {};
        HistorySeries graphicsStatefulUploads = {};
        HistorySeries lifecyclePhaseValues = {};
        HistorySeries requestedFramesInFlight = {};
        HistorySeries activeFramesInFlight = {};
        HistorySeries fallbackEventPulses = {};
    };

    struct AsyncChunkMeshFrameSnapshot
    {
        int                    imguiFrame = -1;
        AsyncChunkMeshMode     mode = AsyncChunkMeshMode::AsyncEnabled;
        bool                   asyncEnabled = true;
        bool                   fallbackActive = false;
        std::string            lastFallbackReason = "None";
        std::string            lastWorkerMaterializationStatus = "None";
        std::string            lastWorkerMaterializationFailureReason = "None";
        AsyncChunkMeshLiveState live = {};
        AsyncChunkMeshCounters  counters = {};
    };

    struct AsyncChunkMeshHistory
    {
        int           sampleCount = 0;
        float         nextSampleIndex = 0.0f;
        HistorySeries sampleIndices = {};
        HistorySeries queuedBacklog = {};
        HistorySeries pendingDispatchCount = {};
        HistorySeries activeHandleCount = {};
        HistorySeries schedulerQueuedCount = {};
        HistorySeries schedulerExecutingCount = {};
        HistorySeries workerMaterializationQueuedCount = {};
        HistorySeries workerMaterializationExecutingCount = {};
        HistorySeries workerMaterializationInFlight = {};
        HistorySeries importantPendingCount = {};
        HistorySeries importantActiveCount = {};
        HistorySeries importantQueuedCount = {};
        HistorySeries importantExecutingCount = {};
        HistorySeries queued = {};
        HistorySeries submitted = {};
        HistorySeries executing = {};
        HistorySeries completed = {};
        HistorySeries published = {};
        HistorySeries mainThreadSnapshotBuildCount = {};
        HistorySeries workerMaterializationAttempts = {};
        HistorySeries workerMaterializationSucceeded = {};
        HistorySeries workerMaterializationRetryLater = {};
        HistorySeries workerMaterializationResolveFailed = {};
        HistorySeries workerMaterializationMissingNeighbors = {};
        HistorySeries workerMaterializationValidationFailed = {};
        HistorySeries discardedStale = {};
        HistorySeries discardedCancelled = {};
        HistorySeries retryLater = {};
        HistorySeries syncFallbackCount = {};
        HistorySeries boundedWaitAttempts = {};
        HistorySeries boundedWaitSatisfied = {};
        HistorySeries boundedWaitTimedOut = {};
        HistorySeries boundedWaitYieldCount = {};
        HistorySeries boundedWaitMicroseconds = {};
        HistorySeries fallbackModePulses = {};
    };

    struct SeriesStats
    {
        float latest = 0.0f;
        float average = 0.0f;
        float peak = 0.0f;
        float peakSampleX = 0.0f;
    };

    QueueExecutionFrameSnapshot s_frameSnapshot = {};
    QueueExecutionHistory       s_history = {};
    QueueExecutionDiagnostics   s_previousDiagnostics = {};
    bool                        s_hasPreviousDiagnostics = false;
    AsyncChunkMeshFrameSnapshot s_asyncChunkMeshFrameSnapshot = {};
    AsyncChunkMeshHistory       s_asyncChunkMeshHistory = {};
    double                      s_lastQueueCopyTime = -1000.0;
    const char*                 s_lastQueueCopyLabel = nullptr;

    uint64_t ComputeDelta(uint64_t current, uint64_t previous)
    {
        return current >= previous ? current - previous : current;
    }

    const char* GetQueueTypeLabel(CommandQueueType queueType)
    {
        switch (queueType)
        {
        case CommandQueueType::Graphics:
            return "Graphics";
        case CommandQueueType::Compute:
            return "Compute";
        case CommandQueueType::Copy:
            return "Copy";
        }

        return "Unknown";
    }

    const char* GetQueueTypeJsonKey(CommandQueueType queueType)
    {
        switch (queueType)
        {
        case CommandQueueType::Graphics:
            return "graphics";
        case CommandQueueType::Compute:
            return "compute";
        case CommandQueueType::Copy:
            return "copy";
        }

        return "unknown";
    }

    const char* GetQueueWorkloadLabel(QueueWorkloadClass workload)
    {
        switch (workload)
        {
        case QueueWorkloadClass::Unknown:
            return "Unknown";
        case QueueWorkloadClass::FrameGraphics:
            return "FrameGraphics";
        case QueueWorkloadClass::ImmediateGraphics:
            return "ImmediateGraphics";
        case QueueWorkloadClass::Present:
            return "Present";
        case QueueWorkloadClass::ImGui:
            return "ImGui";
        case QueueWorkloadClass::GraphicsStatefulUpload:
            return "GraphicsStatefulUpload";
        case QueueWorkloadClass::CopyReadyUpload:
            return "CopyReadyUpload";
        case QueueWorkloadClass::MipmapGeneration:
            return "MipmapGeneration";
        case QueueWorkloadClass::ChunkGeometryUpload:
            return "ChunkGeometryUpload";
        case QueueWorkloadClass::ChunkArenaRelocation:
            return "ChunkArenaRelocation";
        }

        return "Unknown";
    }

    const char* GetQueueExecutionModeLabel(QueueExecutionMode mode)
    {
        switch (mode)
        {
        case QueueExecutionMode::GraphicsOnly:
            return "GraphicsOnly";
        case QueueExecutionMode::MixedQueueExperimental:
            return "MixedQueueExperimental";
        }

        return "Unknown";
    }

    const char* GetQueueFallbackReasonLabel(QueueFallbackReason reason)
    {
        switch (reason)
        {
        case QueueFallbackReason::None:
            return "None";
        case QueueFallbackReason::GraphicsOnlyMode:
            return "GraphicsOnlyMode";
        case QueueFallbackReason::RouteNotValidated:
            return "RouteNotValidated";
        case QueueFallbackReason::UnsupportedWorkload:
            return "UnsupportedWorkload";
        case QueueFallbackReason::RequiresGraphicsStateTransition:
            return "RequiresGraphicsStateTransition";
        case QueueFallbackReason::DedicatedQueueUnavailable:
            return "DedicatedQueueUnavailable";
        case QueueFallbackReason::QueueTypeUnavailable:
            return "QueueTypeUnavailable";
        case QueueFallbackReason::ResourceStateNotSupported:
            return "ResourceStateNotSupported";
        }

        return "Unknown";
    }

    const char* GetFrameLifecyclePhaseLabel(FrameLifecyclePhase phase)
    {
        switch (phase)
        {
        case FrameLifecyclePhase::Idle:
            return "Idle";
        case FrameLifecyclePhase::PreFrameBegin:
            return "PreFrameBegin";
        case FrameLifecyclePhase::RetiringFrameSlot:
            return "RetiringFrameSlot";
        case FrameLifecyclePhase::FrameSlotAcquired:
            return "FrameSlotAcquired";
        case FrameLifecyclePhase::RecordingFrame:
            return "RecordingFrame";
        case FrameLifecyclePhase::SubmittingFrame:
            return "SubmittingFrame";
        }

        return "Unknown";
    }

    float GetFrameLifecyclePhaseValue(FrameLifecyclePhase phase)
    {
        switch (phase)
        {
        case FrameLifecyclePhase::Idle:
            return 0.0f;
        case FrameLifecyclePhase::PreFrameBegin:
            return 1.0f;
        case FrameLifecyclePhase::RetiringFrameSlot:
            return 2.0f;
        case FrameLifecyclePhase::FrameSlotAcquired:
            return 3.0f;
        case FrameLifecyclePhase::RecordingFrame:
            return 4.0f;
        case FrameLifecyclePhase::SubmittingFrame:
            return 5.0f;
        }

        return 0.0f;
    }

    float ComputeSharePercent(uint64_t numerator, uint64_t total)
    {
        if (total == 0)
        {
            return 0.0f;
        }

        return static_cast<float>(static_cast<double>(numerator) * 100.0 / static_cast<double>(total));
    }

    uint64_t GetWorkloadTotal(const WorkloadSubmissionMatrix& matrix, size_t workloadIndex)
    {
        uint64_t total = 0;
        for (uint64_t count : matrix[workloadIndex])
        {
            total += count;
        }
        return total;
    }

    uint64_t GetWorkloadSubmissionTotal(const WorkloadSubmissionMatrix& matrix, QueueWorkloadClass workload)
    {
        return GetWorkloadTotal(matrix, static_cast<size_t>(workload));
    }

    uint64_t GetQueueWaitTotal(const QueueWaitMatrix& matrix)
    {
        uint64_t total = 0;
        for (const QueueCountArray& producerCounts : matrix)
        {
            for (uint64_t count : producerCounts)
            {
                total += count;
            }
        }
        return total;
    }

    uint64_t GetQueueWaitTotalForConsumer(const QueueWaitMatrix& matrix, CommandQueueType consumerQueue)
    {
        uint64_t total = 0;
        const size_t consumerIndex = static_cast<size_t>(consumerQueue);
        for (const QueueCountArray& producerCounts : matrix)
        {
            total += producerCounts[consumerIndex];
        }

        return total;
    }

    template <size_t N>
    void AppendHistoryValue(std::array<float, N>& series, int sampleCount, float value)
    {
        if (sampleCount < static_cast<int>(N))
        {
            series[sampleCount] = value;
            return;
        }

        std::move(series.begin() + 1, series.end(), series.begin());
        series.back() = value;
    }

    int GetVisibleHistoryStartIndex(int sampleCount)
    {
        return std::max(0, sampleCount - static_cast<int>(kVisibleHistoryLength));
    }

    int GetVisibleHistoryCount(int sampleCount)
    {
        return std::max(0, sampleCount - GetVisibleHistoryStartIndex(sampleCount));
    }

    float GetVisibleHistoryMinX(const HistorySeries& sampleIndices, int sampleCount)
    {
        if (sampleCount <= 0)
        {
            return 0.0f;
        }

        return sampleIndices[GetVisibleHistoryStartIndex(sampleCount)];
    }

    float GetVisibleHistoryMaxX(const HistorySeries& sampleIndices, int sampleCount)
    {
        if (sampleCount <= 0)
        {
            return 1.0f;
        }

        const float xMax = sampleIndices[sampleCount - 1];
        const float xMin = GetVisibleHistoryMinX(sampleIndices, sampleCount);
        return xMax > xMin ? xMax : (xMin + 1.0f);
    }

    const float* GetVisibleHistoryXs(const HistorySeries& sampleIndices, int sampleCount)
    {
        return sampleIndices.data() + GetVisibleHistoryStartIndex(sampleCount);
    }

    const float* GetVisibleHistorySeries(const HistorySeries& series, int sampleCount)
    {
        return series.data() + GetVisibleHistoryStartIndex(sampleCount);
    }

    float GetVisibleSeriesMax(const HistorySeries& series,
                              const HistorySeries& sampleIndices,
                              int                  sampleCount)
    {
        UNUSED(sampleIndices);

        const int startIndex = GetVisibleHistoryStartIndex(sampleCount);
        const int visibleCount = GetVisibleHistoryCount(sampleCount);
        float maxValue = 0.0f;
        for (int i = 0; i < visibleCount; ++i)
        {
            maxValue = std::max(maxValue, series[startIndex + i]);
        }

        return maxValue;
    }

    float GetVisibleSeriesMax(const HistorySeries&                  sampleIndices,
                              int                                   sampleCount,
                              std::initializer_list<const HistorySeries*> seriesSet)
    {
        float maxValue = 0.0f;
        for (const HistorySeries* series : seriesSet)
        {
            if (!series)
            {
                continue;
            }

            maxValue = std::max(maxValue, GetVisibleSeriesMax(*series, sampleIndices, sampleCount));
        }

        return maxValue;
    }

    SeriesStats ComputeVisibleSeriesStats(const HistorySeries& series,
                                          const HistorySeries& sampleIndices,
                                          int                  sampleCount)
    {
        SeriesStats stats = {};
        const int startIndex = GetVisibleHistoryStartIndex(sampleCount);
        const int visibleCount = GetVisibleHistoryCount(sampleCount);
        if (visibleCount <= 0)
        {
            return stats;
        }

        float sum = 0.0f;
        for (int i = 0; i < visibleCount; ++i)
        {
            const int seriesIndex = startIndex + i;
            const float value = series[seriesIndex];
            const float sampleX = sampleIndices[seriesIndex];
            sum += value;

            if (value >= stats.peak)
            {
                stats.peak = value;
                stats.peakSampleX = sampleX;
            }
        }

        stats.latest = series[sampleCount - 1];
        stats.average = sum / static_cast<float>(visibleCount);
        return stats;
    }

    int CountVisibleNonZeroSamples(const HistorySeries& series,
                                   const HistorySeries& sampleIndices,
                                   int                  sampleCount)
    {
        UNUSED(sampleIndices);

        const int startIndex = GetVisibleHistoryStartIndex(sampleCount);
        const int visibleCount = GetVisibleHistoryCount(sampleCount);
        int       count = 0;
        for (int i = 0; i < visibleCount; ++i)
        {
            if (series[startIndex + i] > 0.0f)
            {
                ++count;
            }
        }

        return count;
    }

    int GetVisibleHistoryStartIndex()
    {
        return GetVisibleHistoryStartIndex(s_history.sampleCount);
    }

    int GetVisibleHistoryCount()
    {
        return GetVisibleHistoryCount(s_history.sampleCount);
    }

    float GetVisibleHistoryMinX()
    {
        return GetVisibleHistoryMinX(s_history.sampleIndices, s_history.sampleCount);
    }

    float GetVisibleHistoryMaxX()
    {
        return GetVisibleHistoryMaxX(s_history.sampleIndices, s_history.sampleCount);
    }

    const float* GetVisibleHistoryXs()
    {
        return GetVisibleHistoryXs(s_history.sampleIndices, s_history.sampleCount);
    }

    const float* GetVisibleHistorySeries(const HistorySeries& series)
    {
        return GetVisibleHistorySeries(series, s_history.sampleCount);
    }

    float GetVisibleSeriesMax(const HistorySeries& series)
    {
        return GetVisibleSeriesMax(series, s_history.sampleIndices, s_history.sampleCount);
    }

    float GetVisibleSeriesMax(std::initializer_list<const HistorySeries*> seriesSet)
    {
        return GetVisibleSeriesMax(s_history.sampleIndices, s_history.sampleCount, seriesSet);
    }

    SeriesStats ComputeVisibleSeriesStats(const HistorySeries& series)
    {
        return ComputeVisibleSeriesStats(series, s_history.sampleIndices, s_history.sampleCount);
    }

    int CountVisibleNonZeroSamples(const HistorySeries& series)
    {
        return CountVisibleNonZeroSamples(series, s_history.sampleIndices, s_history.sampleCount);
    }

    void PushHistorySample(const QueueExecutionFrameSnapshot& frameSnapshot)
    {
        const int sampleWriteIndex = s_history.sampleCount;
        const uint64_t totalSubmissions = frameSnapshot.graphicsSubmissions +
                                          frameSnapshot.computeSubmissions +
                                          frameSnapshot.copySubmissions;
        const float fallbackPulse = frameSnapshot.lastFallbackReason == QueueFallbackReason::None
            ? 0.0f
            : kFallbackPulseValue;

        AppendHistoryValue(s_history.sampleIndices, sampleWriteIndex, s_history.nextSampleIndex);
        AppendHistoryValue(s_history.graphicsSubmissions, sampleWriteIndex, static_cast<float>(frameSnapshot.graphicsSubmissions));
        AppendHistoryValue(s_history.computeSubmissions, sampleWriteIndex, static_cast<float>(frameSnapshot.computeSubmissions));
        AppendHistoryValue(s_history.copySubmissions, sampleWriteIndex, static_cast<float>(frameSnapshot.copySubmissions));
        AppendHistoryValue(s_history.graphicsSharePercent, sampleWriteIndex, ComputeSharePercent(frameSnapshot.graphicsSubmissions, totalSubmissions));
        AppendHistoryValue(s_history.computeSharePercent, sampleWriteIndex, ComputeSharePercent(frameSnapshot.computeSubmissions, totalSubmissions));
        AppendHistoryValue(s_history.copySharePercent, sampleWriteIndex, ComputeSharePercent(frameSnapshot.copySubmissions, totalSubmissions));
        AppendHistoryValue(s_history.graphicsQueueWaits,
                           sampleWriteIndex,
                           static_cast<float>(GetQueueWaitTotalForConsumer(frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Graphics)));
        AppendHistoryValue(s_history.computeQueueWaits,
                           sampleWriteIndex,
                           static_cast<float>(GetQueueWaitTotalForConsumer(frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Compute)));
        AppendHistoryValue(s_history.copyQueueWaits,
                           sampleWriteIndex,
                           static_cast<float>(GetQueueWaitTotalForConsumer(frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Copy)));
        AppendHistoryValue(s_history.totalQueueWaitInsertions, sampleWriteIndex, static_cast<float>(frameSnapshot.queueWaitInsertions));
        AppendHistoryValue(s_history.copyReadyUploads,
                           sampleWriteIndex,
                           static_cast<float>(GetWorkloadSubmissionTotal(frameSnapshot.workloadSubmissionsByQueue, QueueWorkloadClass::CopyReadyUpload)));
        AppendHistoryValue(s_history.mipmapGeneration,
                           sampleWriteIndex,
                           static_cast<float>(GetWorkloadSubmissionTotal(frameSnapshot.workloadSubmissionsByQueue, QueueWorkloadClass::MipmapGeneration)));
        AppendHistoryValue(s_history.graphicsStatefulUploads,
                           sampleWriteIndex,
                           static_cast<float>(GetWorkloadSubmissionTotal(frameSnapshot.workloadSubmissionsByQueue, QueueWorkloadClass::GraphicsStatefulUpload)));
        AppendHistoryValue(s_history.lifecyclePhaseValues, sampleWriteIndex, GetFrameLifecyclePhaseValue(frameSnapshot.lifecyclePhase));
        AppendHistoryValue(s_history.requestedFramesInFlight, sampleWriteIndex, static_cast<float>(frameSnapshot.requestedFramesInFlightDepth));
        AppendHistoryValue(s_history.activeFramesInFlight, sampleWriteIndex, static_cast<float>(frameSnapshot.activeFramesInFlightDepth));
        AppendHistoryValue(s_history.fallbackEventPulses, sampleWriteIndex, fallbackPulse);

        if (s_history.sampleCount < static_cast<int>(kHistoryLength))
        {
            ++s_history.sampleCount;
        }

        s_history.nextSampleIndex += 1.0f;
    }

    void PushAsyncChunkMeshHistorySample(const AsyncChunkMeshFrameSnapshot& frameSnapshot)
    {
        const int sampleWriteIndex = s_asyncChunkMeshHistory.sampleCount;
        const float fallbackPulse = frameSnapshot.fallbackActive ? kAsyncFallbackPulseValue : 0.0f;

        AppendHistoryValue(s_asyncChunkMeshHistory.sampleIndices, sampleWriteIndex, s_asyncChunkMeshHistory.nextSampleIndex);
        AppendHistoryValue(s_asyncChunkMeshHistory.queuedBacklog, sampleWriteIndex, static_cast<float>(frameSnapshot.live.queuedBacklog));
        AppendHistoryValue(s_asyncChunkMeshHistory.pendingDispatchCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.pendingDispatchCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.activeHandleCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.activeHandleCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.schedulerQueuedCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.schedulerQueuedCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.schedulerExecutingCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.schedulerExecutingCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationQueuedCount,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.live.workerMaterializationQueuedCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationExecutingCount,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.live.workerMaterializationExecutingCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationInFlight,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.live.workerMaterializationInFlight));
        AppendHistoryValue(s_asyncChunkMeshHistory.importantPendingCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.importantPendingCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.importantActiveCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.importantActiveCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.importantQueuedCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.importantQueuedCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.importantExecutingCount, sampleWriteIndex, static_cast<float>(frameSnapshot.live.importantExecutingCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.queued, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.queued));
        AppendHistoryValue(s_asyncChunkMeshHistory.submitted, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.submitted));
        AppendHistoryValue(s_asyncChunkMeshHistory.executing, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.executing));
        AppendHistoryValue(s_asyncChunkMeshHistory.completed, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.completed));
        AppendHistoryValue(s_asyncChunkMeshHistory.published, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.published));
        AppendHistoryValue(s_asyncChunkMeshHistory.mainThreadSnapshotBuildCount,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.mainThreadSnapshotBuildCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationAttempts,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationAttempts));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationSucceeded,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationSucceeded));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationRetryLater,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationRetryLater));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationResolveFailed,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationResolveFailed));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationMissingNeighbors,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationMissingNeighbors));
        AppendHistoryValue(s_asyncChunkMeshHistory.workerMaterializationValidationFailed,
                           sampleWriteIndex,
                           static_cast<float>(frameSnapshot.counters.workerMaterializationValidationFailed));
        AppendHistoryValue(s_asyncChunkMeshHistory.discardedStale, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.discardedStale));
        AppendHistoryValue(s_asyncChunkMeshHistory.discardedCancelled, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.discardedCancelled));
        AppendHistoryValue(s_asyncChunkMeshHistory.retryLater, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.retryLater));
        AppendHistoryValue(s_asyncChunkMeshHistory.syncFallbackCount, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.syncFallbackCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.boundedWaitAttempts, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.boundedWaitAttempts));
        AppendHistoryValue(s_asyncChunkMeshHistory.boundedWaitSatisfied, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.boundedWaitSatisfied));
        AppendHistoryValue(s_asyncChunkMeshHistory.boundedWaitTimedOut, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.boundedWaitTimedOut));
        AppendHistoryValue(s_asyncChunkMeshHistory.boundedWaitYieldCount, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.boundedWaitYieldCount));
        AppendHistoryValue(s_asyncChunkMeshHistory.boundedWaitMicroseconds, sampleWriteIndex, static_cast<float>(frameSnapshot.counters.boundedWaitMicroseconds));
        AppendHistoryValue(s_asyncChunkMeshHistory.fallbackModePulses, sampleWriteIndex, fallbackPulse);

        if (s_asyncChunkMeshHistory.sampleCount < static_cast<int>(kHistoryLength))
        {
            ++s_asyncChunkMeshHistory.sampleCount;
        }

        s_asyncChunkMeshHistory.nextSampleIndex += 1.0f;
    }

    void UpdateAsyncChunkMeshFrameSnapshot(const AsyncChunkMeshDiagnostics& diagnostics)
    {
        const int currentImGuiFrame = ImGui::GetFrameCount();
        if (s_asyncChunkMeshFrameSnapshot.imguiFrame == currentImGuiFrame)
        {
            return;
        }

        s_asyncChunkMeshFrameSnapshot.imguiFrame = currentImGuiFrame;
        s_asyncChunkMeshFrameSnapshot.mode = diagnostics.mode;
        s_asyncChunkMeshFrameSnapshot.asyncEnabled = diagnostics.asyncEnabled;
        s_asyncChunkMeshFrameSnapshot.fallbackActive = diagnostics.fallbackActive;
        s_asyncChunkMeshFrameSnapshot.lastFallbackReason = diagnostics.lastFallbackReason;
        s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationStatus = diagnostics.lastWorkerMaterializationStatus;
        s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationFailureReason = diagnostics.lastWorkerMaterializationFailureReason;
        s_asyncChunkMeshFrameSnapshot.live = diagnostics.live;
        s_asyncChunkMeshFrameSnapshot.counters = diagnostics.frame;

        PushAsyncChunkMeshHistorySample(s_asyncChunkMeshFrameSnapshot);
    }

    void UpdateFrameSnapshot(const QueueExecutionDiagnostics& diagnostics)
    {
        const int currentImGuiFrame = ImGui::GetFrameCount();
        if (s_frameSnapshot.imguiFrame == currentImGuiFrame)
        {
            return;
        }

        s_frameSnapshot.imguiFrame = currentImGuiFrame;
        s_frameSnapshot.requestedMode = diagnostics.requestedMode;
        s_frameSnapshot.activeMode = diagnostics.activeMode;
        s_frameSnapshot.lifecyclePhase = diagnostics.lifecyclePhase;
        s_frameSnapshot.lastFallbackReason = diagnostics.lastFallbackReason;
        s_frameSnapshot.requestedFramesInFlightDepth = diagnostics.requestedFramesInFlightDepth;
        s_frameSnapshot.activeFramesInFlightDepth = diagnostics.activeFramesInFlightDepth;

        const QueueExecutionDiagnostics* previousDiagnostics = s_hasPreviousDiagnostics
            ? &s_previousDiagnostics
            : nullptr;

        if (!previousDiagnostics)
        {
            s_frameSnapshot.graphicsSubmissions = diagnostics.graphicsSubmissions;
            s_frameSnapshot.computeSubmissions = diagnostics.computeSubmissions;
            s_frameSnapshot.copySubmissions = diagnostics.copySubmissions;
            s_frameSnapshot.queueWaitInsertions = diagnostics.queueWaitInsertions;
            s_hasPreviousDiagnostics = true;
        }
        else
        {
            s_frameSnapshot.graphicsSubmissions = ComputeDelta(diagnostics.graphicsSubmissions, previousDiagnostics->graphicsSubmissions);
            s_frameSnapshot.computeSubmissions = ComputeDelta(diagnostics.computeSubmissions, previousDiagnostics->computeSubmissions);
            s_frameSnapshot.copySubmissions = ComputeDelta(diagnostics.copySubmissions, previousDiagnostics->copySubmissions);
            s_frameSnapshot.queueWaitInsertions = ComputeDelta(diagnostics.queueWaitInsertions, previousDiagnostics->queueWaitInsertions);
        }

        for (size_t workloadIndex = 0; workloadIndex < kQueueWorkloadCount; ++workloadIndex)
        {
            for (size_t queueIndex = 0; queueIndex < kQueueTypeCount; ++queueIndex)
            {
                const uint64_t currentValue = diagnostics.workloadSubmissionsByQueue[workloadIndex][queueIndex];
                const uint64_t previousValue = previousDiagnostics
                    ? previousDiagnostics->workloadSubmissionsByQueue[workloadIndex][queueIndex]
                    : 0;

                s_frameSnapshot.workloadSubmissionsByQueue[workloadIndex][queueIndex] =
                    ComputeDelta(currentValue, previousValue);
            }
        }

        for (size_t producerIndex = 0; producerIndex < kQueueTypeCount; ++producerIndex)
        {
            for (size_t consumerIndex = 0; consumerIndex < kQueueTypeCount; ++consumerIndex)
            {
                const uint64_t currentValue = diagnostics.queueWaitsByProducerConsumer[producerIndex][consumerIndex];
                const uint64_t previousValue = previousDiagnostics
                    ? previousDiagnostics->queueWaitsByProducerConsumer[producerIndex][consumerIndex]
                    : 0;

                s_frameSnapshot.queueWaitsByProducerConsumer[producerIndex][consumerIndex] =
                    ComputeDelta(currentValue, previousValue);
            }
        }

        PushHistorySample(s_frameSnapshot);
        s_previousDiagnostics = diagnostics;
    }

    const enigma::voxel::World* GetActiveWorld()
    {
        return g_theGame ? g_theGame->GetWorld() : nullptr;
    }

    const AsyncChunkMeshDiagnostics* GetAsyncChunkMeshDiagnosticsOrNull()
    {
        const enigma::voxel::World* world = GetActiveWorld();
        return world ? &world->GetAsyncChunkMeshDiagnostics() : nullptr;
    }

    const char* GetAsyncChunkMeshModeLabel(AsyncChunkMeshMode mode)
    {
        return enigma::voxel::GetAsyncChunkMeshModeName(mode);
    }

    void RecordQueueCopy(const char* label)
    {
        s_lastQueueCopyTime = ImGui::GetTime();
        s_lastQueueCopyLabel = label;
    }

    Json BuildUnavailableJson()
    {
        Json root = Json::object();
        root["queueDiagnostics"] = Json::object();
        root["queueDiagnostics"]["error"] = "RendererSubsystem unavailable";
        return root;
    }

    Json BuildAsyncChunkMeshUnavailableJson(const char* error)
    {
        Json root = Json::object();
        root["asyncChunkMeshDiagnostics"] = Json::object();
        root["asyncChunkMeshDiagnostics"]["error"] = error;
        return root;
    }

    void DrawSubmissionCounters(uint64_t graphicsSubmissions,
                                uint64_t computeSubmissions,
                                uint64_t copySubmissions,
                                uint64_t queueWaitInsertions)
    {
        ImGui::Text("Graphics Submissions: %llu", static_cast<unsigned long long>(graphicsSubmissions));
        ImGui::Text("Compute Submissions: %llu", static_cast<unsigned long long>(computeSubmissions));
        ImGui::Text("Copy Submissions: %llu", static_cast<unsigned long long>(copySubmissions));
        ImGui::Text("Queue Wait Insertions: %llu", static_cast<unsigned long long>(queueWaitInsertions));
    }

    Json BuildSubmissionJson(uint64_t graphicsSubmissions,
                             uint64_t computeSubmissions,
                             uint64_t copySubmissions)
    {
        Json submissions = Json::object();
        submissions["graphics"] = graphicsSubmissions;
        submissions["compute"] = computeSubmissions;
        submissions["copy"] = copySubmissions;
        return submissions;
    }

    Json BuildAsyncChunkMeshLiveJson(const AsyncChunkMeshLiveState& live)
    {
        Json liveJson = Json::object();
        liveJson["queuedBacklog"] = live.queuedBacklog;
        liveJson["pendingDispatchCount"] = live.pendingDispatchCount;
        liveJson["activeHandleCount"] = live.activeHandleCount;
        liveJson["schedulerQueuedCount"] = live.schedulerQueuedCount;
        liveJson["schedulerExecutingCount"] = live.schedulerExecutingCount;
        liveJson["workerMaterializationQueuedCount"] = live.workerMaterializationQueuedCount;
        liveJson["workerMaterializationExecutingCount"] = live.workerMaterializationExecutingCount;
        liveJson["workerMaterializationInFlight"] = live.workerMaterializationInFlight;
        liveJson["importantPendingCount"] = live.importantPendingCount;
        liveJson["importantActiveCount"] = live.importantActiveCount;
        liveJson["importantQueuedCount"] = live.importantQueuedCount;
        liveJson["importantExecutingCount"] = live.importantExecutingCount;
        return liveJson;
    }

    Json BuildAsyncChunkMeshCountersJson(const AsyncChunkMeshCounters& counters)
    {
        Json countersJson = Json::object();
        countersJson["queued"] = counters.queued;
        countersJson["submitted"] = counters.submitted;
        countersJson["executing"] = counters.executing;
        countersJson["completed"] = counters.completed;
        countersJson["published"] = counters.published;
        countersJson["mainThreadSnapshotBuildCount"] = counters.mainThreadSnapshotBuildCount;
        countersJson["workerMaterializationAttempts"] = counters.workerMaterializationAttempts;
        countersJson["workerMaterializationSucceeded"] = counters.workerMaterializationSucceeded;
        countersJson["workerMaterializationRetryLater"] = counters.workerMaterializationRetryLater;
        countersJson["workerMaterializationResolveFailed"] = counters.workerMaterializationResolveFailed;
        countersJson["workerMaterializationMissingNeighbors"] = counters.workerMaterializationMissingNeighbors;
        countersJson["workerMaterializationValidationFailed"] = counters.workerMaterializationValidationFailed;
        countersJson["discardedStale"] = counters.discardedStale;
        countersJson["discardedCancelled"] = counters.discardedCancelled;
        countersJson["retryLater"] = counters.retryLater;
        countersJson["syncFallbackCount"] = counters.syncFallbackCount;
        countersJson["boundedWaitAttempts"] = counters.boundedWaitAttempts;
        countersJson["boundedWaitSatisfied"] = counters.boundedWaitSatisfied;
        countersJson["boundedWaitTimedOut"] = counters.boundedWaitTimedOut;
        countersJson["boundedWaitYieldCount"] = counters.boundedWaitYieldCount;
        countersJson["boundedWaitMicroseconds"] = counters.boundedWaitMicroseconds;
        return countersJson;
    }

    Json BuildWorkloadSubmissionJson(const WorkloadSubmissionMatrix& matrix)
    {
        Json workloadJson = Json::object();
        for (size_t workloadIndex = 0; workloadIndex < kQueueWorkloadCount; ++workloadIndex)
        {
            Json workloadEntry = Json::object();
            workloadEntry["graphics"] = matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Graphics)];
            workloadEntry["compute"] = matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Compute)];
            workloadEntry["copy"] = matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Copy)];
            workloadEntry["total"] = GetWorkloadTotal(matrix, workloadIndex);
            workloadJson[GetQueueWorkloadLabel(static_cast<QueueWorkloadClass>(workloadIndex))] = workloadEntry;
        }
        return workloadJson;
    }

    Json BuildQueueWaitJson(const QueueWaitMatrix& matrix)
    {
        Json waitsJson = Json::object();
        for (size_t producerIndex = 0; producerIndex < kQueueTypeCount; ++producerIndex)
        {
            Json producerEntry = Json::object();
            for (size_t consumerIndex = 0; consumerIndex < kQueueTypeCount; ++consumerIndex)
            {
                producerEntry[GetQueueTypeJsonKey(static_cast<CommandQueueType>(consumerIndex))] =
                    matrix[producerIndex][consumerIndex];
            }

            waitsJson[GetQueueTypeJsonKey(static_cast<CommandQueueType>(producerIndex))] = producerEntry;
        }
        return waitsJson;
    }

    Json BuildQueueParticipationJson(const std::array<bool, kQueueTypeCount>& values)
    {
        Json participationJson = Json::object();
        for (size_t queueIndex = 0; queueIndex < kQueueTypeCount; ++queueIndex)
        {
            participationJson[GetQueueTypeJsonKey(static_cast<CommandQueueType>(queueIndex))] = values[queueIndex];
        }

        return participationJson;
    }

    bool HasHistorySamples()
    {
        return s_history.sampleCount > 1;
    }

    void DrawHistoryUnavailableText()
    {
        ImGui::TextDisabled("History will appear after at least two sampled ImGui frames.");
    }

    void PrepareSlidingWindowPlot(std::initializer_list<const HistorySeries*> seriesSet)
    {
        const float xMin = GetVisibleHistoryMinX();
        const float xMax = GetVisibleHistoryMaxX();
        const float seriesMax = GetVisibleSeriesMax(seriesSet);
        const float yMax = std::max(1.0f, std::ceil(seriesMax * 1.15f));

        ImPlot::SetNextAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, yMax, ImPlotCond_Always);
    }

    void DrawSeriesSummary(const char* label, const HistorySeries& series)
    {
        const SeriesStats stats = ComputeVisibleSeriesStats(series);
        ImGui::BulletText("%s  latest=%.0f  avg=%.2f  peak=%.0f @ sample %.0f",
                          label,
                          stats.latest,
                          stats.average,
                          stats.peak,
                          stats.peakSampleX);
    }

    void DrawPercentSeriesSummary(const char* label, const HistorySeries& series)
    {
        const SeriesStats stats = ComputeVisibleSeriesStats(series);
        ImGui::BulletText("%s  latest=%.1f%%  avg=%.1f%%  peak=%.1f%% @ sample %.0f",
                          label,
                          stats.latest,
                          stats.average,
                          stats.peak,
                          stats.peakSampleX);
    }

    void DrawQueueSubmissionHistoryPlot()
    {
        if (!HasHistorySamples())
        {
            DrawHistoryUnavailableText();
            return;
        }

        PrepareSlidingWindowPlot({&s_history.graphicsSubmissions, &s_history.computeSubmissions, &s_history.copySubmissions});

        if (ImPlot::BeginPlot("Recent Queue Submission History##QueueSubmissionHistory", ImVec2(-1.0f, 240.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Submissions / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Graphics", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.graphicsSubmissions), GetVisibleHistoryCount());
            ImPlot::PlotLine("Compute", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.computeSubmissions), GetVisibleHistoryCount());
            ImPlot::PlotLine("Copy", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.copySubmissions), GetVisibleHistoryCount());
            ImPlot::TagX(GetVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawSeriesSummary("Graphics", s_history.graphicsSubmissions);
        DrawSeriesSummary("Compute", s_history.computeSubmissions);
        DrawSeriesSummary("Copy", s_history.copySubmissions);
        ImGui::TextDisabled("Sliding window: last %d of %d stored samples.", GetVisibleHistoryCount(), s_history.sampleCount);
    }

    void DrawQueueWaitHistoryPlot()
    {
        if (!HasHistorySamples())
        {
            DrawHistoryUnavailableText();
            return;
        }

        PrepareSlidingWindowPlot({&s_history.graphicsQueueWaits,
                                  &s_history.computeQueueWaits,
                                  &s_history.copyQueueWaits,
                                  &s_history.totalQueueWaitInsertions});

        if (ImPlot::BeginPlot("Recent Cross-Queue Wait History##QueueWaitHistory", ImVec2(-1.0f, 220.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Wait Insertions / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Graphics Queue Waits", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.graphicsQueueWaits), GetVisibleHistoryCount());
            ImPlot::PlotLine("Compute Queue Waits", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.computeQueueWaits), GetVisibleHistoryCount());
            ImPlot::PlotLine("Copy Queue Waits", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.copyQueueWaits), GetVisibleHistoryCount());
            ImPlot::PlotLine("Total Wait Insertions", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.totalQueueWaitInsertions), GetVisibleHistoryCount());
            ImPlot::TagX(GetVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawSeriesSummary("Graphics Waits", s_history.graphicsQueueWaits);
        DrawSeriesSummary("Compute Waits", s_history.computeQueueWaits);
        DrawSeriesSummary("Copy Waits", s_history.copyQueueWaits);
        DrawSeriesSummary("Total Wait Insertions", s_history.totalQueueWaitInsertions);
    }

    void DrawDedicatedWorkloadHistoryPlot()
    {
        if (!HasHistorySamples())
        {
            DrawHistoryUnavailableText();
            return;
        }

        PrepareSlidingWindowPlot({&s_history.copyReadyUploads, &s_history.mipmapGeneration, &s_history.graphicsStatefulUploads});

        if (ImPlot::BeginPlot("Recent Dedicated Workload History##QueueDedicatedWorkloads", ImVec2(-1.0f, 220.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Submissions / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("CopyReadyUpload", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.copyReadyUploads), GetVisibleHistoryCount());
            ImPlot::PlotLine("MipmapGeneration", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.mipmapGeneration), GetVisibleHistoryCount());
            ImPlot::PlotLine("GraphicsStatefulUpload", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.graphicsStatefulUploads), GetVisibleHistoryCount());
            ImPlot::TagX(GetVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawSeriesSummary("CopyReadyUpload", s_history.copyReadyUploads);
        DrawSeriesSummary("MipmapGeneration", s_history.mipmapGeneration);
        DrawSeriesSummary("GraphicsStatefulUpload", s_history.graphicsStatefulUploads);
    }

    void DrawQueueShareHistoryPlot()
    {
        if (!HasHistorySamples())
        {
            DrawHistoryUnavailableText();
            return;
        }

        const float xMin = GetVisibleHistoryMinX();
        const float xMax = GetVisibleHistoryMaxX();
        ImPlot::SetNextAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 100.0f, ImPlotCond_Always);

        if (ImPlot::BeginPlot("Recent Queue Share History##QueueShareHistory", ImVec2(-1.0f, 220.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Submission Share (%)", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Graphics %", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.graphicsSharePercent), GetVisibleHistoryCount());
            ImPlot::PlotLine("Compute %", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.computeSharePercent), GetVisibleHistoryCount());
            ImPlot::PlotLine("Copy %", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.copySharePercent), GetVisibleHistoryCount());
            ImPlot::TagX(GetVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawPercentSeriesSummary("Graphics", s_history.graphicsSharePercent);
        DrawPercentSeriesSummary("Compute", s_history.computeSharePercent);
        DrawPercentSeriesSummary("Copy", s_history.copySharePercent);
    }

    void DrawLifecycleAndFramesInFlightPlot()
    {
        if (!HasHistorySamples())
        {
            DrawHistoryUnavailableText();
            return;
        }

        const float xMin = GetVisibleHistoryMinX();
        const float xMax = GetVisibleHistoryMaxX();
        ImPlot::SetNextAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 8.0f, ImPlotCond_Always);

        if (ImPlot::BeginPlot("Recent Lifecycle & Frames-In-Flight##QueueLifecycleAndFif", ImVec2(-1.0f, 240.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "State / Depth", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Lifecycle Phase", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.lifecyclePhaseValues), GetVisibleHistoryCount());
            ImPlot::PlotLine("Requested FIF", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.requestedFramesInFlight), GetVisibleHistoryCount());
            ImPlot::PlotLine("Active FIF", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.activeFramesInFlight), GetVisibleHistoryCount());
            ImPlot::PlotStems("Fallback Event", GetVisibleHistoryXs(), GetVisibleHistorySeries(s_history.fallbackEventPulses), GetVisibleHistoryCount());
            ImPlot::TagX(GetVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        ImGui::BulletText("Lifecycle  latest=%s", GetFrameLifecyclePhaseLabel(s_frameSnapshot.lifecyclePhase));
        DrawSeriesSummary("Requested FIF", s_history.requestedFramesInFlight);
        DrawSeriesSummary("Active FIF", s_history.activeFramesInFlight);
        ImGui::BulletText("Fallback events in visible window: %d", CountVisibleNonZeroSamples(s_history.fallbackEventPulses));
    }

    void DrawAccumulatedWorkloadHighlights(const WorkloadSubmissionMatrix& matrix)
    {
        bool hasAnyData = false;
        for (size_t workloadIndex = 0; workloadIndex < kQueueWorkloadCount; ++workloadIndex)
        {
            const uint64_t total = GetWorkloadTotal(matrix, workloadIndex);
            if (total == 0)
            {
                continue;
            }

            hasAnyData = true;
            ImGui::BulletText("%s: Graphics=%llu Compute=%llu Copy=%llu Total=%llu",
                              GetQueueWorkloadLabel(static_cast<QueueWorkloadClass>(workloadIndex)),
                              static_cast<unsigned long long>(matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Graphics)]),
                              static_cast<unsigned long long>(matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Compute)]),
                              static_cast<unsigned long long>(matrix[workloadIndex][static_cast<size_t>(CommandQueueType::Copy)]),
                              static_cast<unsigned long long>(total));
        }

        if (!hasAnyData)
        {
            ImGui::TextDisabled("No accumulated workload submissions recorded.");
        }
    }

    void DrawAccumulatedQueueWaitRoutes(const QueueWaitMatrix& matrix)
    {
        bool hasAnyData = false;
        for (size_t producerIndex = 0; producerIndex < kQueueTypeCount; ++producerIndex)
        {
            for (size_t consumerIndex = 0; consumerIndex < kQueueTypeCount; ++consumerIndex)
            {
                const uint64_t count = matrix[producerIndex][consumerIndex];
                if (count == 0)
                {
                    continue;
                }

                hasAnyData = true;
                ImGui::BulletText("%s -> %s: %llu",
                                  GetQueueTypeLabel(static_cast<CommandQueueType>(producerIndex)),
                                  GetQueueTypeLabel(static_cast<CommandQueueType>(consumerIndex)),
                                  static_cast<unsigned long long>(count));
            }
        }

        if (!hasAnyData)
        {
            ImGui::TextDisabled("No accumulated cross-queue waits recorded.");
        }
    }

    bool HasAsyncChunkMeshHistorySamples()
    {
        return s_asyncChunkMeshHistory.sampleCount > 1;
    }

    void DrawAsyncChunkMeshHistoryUnavailableText()
    {
        ImGui::TextDisabled("Async mesh history will appear after at least two sampled ImGui frames.");
    }

    int GetAsyncVisibleHistoryCount()
    {
        return GetVisibleHistoryCount(s_asyncChunkMeshHistory.sampleCount);
    }

    float GetAsyncVisibleHistoryMinX()
    {
        return GetVisibleHistoryMinX(s_asyncChunkMeshHistory.sampleIndices, s_asyncChunkMeshHistory.sampleCount);
    }

    float GetAsyncVisibleHistoryMaxX()
    {
        return GetVisibleHistoryMaxX(s_asyncChunkMeshHistory.sampleIndices, s_asyncChunkMeshHistory.sampleCount);
    }

    const float* GetAsyncVisibleHistoryXs()
    {
        return GetVisibleHistoryXs(s_asyncChunkMeshHistory.sampleIndices, s_asyncChunkMeshHistory.sampleCount);
    }

    const float* GetAsyncVisibleHistorySeries(const HistorySeries& series)
    {
        return GetVisibleHistorySeries(series, s_asyncChunkMeshHistory.sampleCount);
    }

    void PrepareAsyncChunkMeshSlidingWindowPlot(std::initializer_list<const HistorySeries*> seriesSet)
    {
        const float xMin = GetAsyncVisibleHistoryMinX();
        const float xMax = GetAsyncVisibleHistoryMaxX();
        const float seriesMax = GetVisibleSeriesMax(
            s_asyncChunkMeshHistory.sampleIndices,
            s_asyncChunkMeshHistory.sampleCount,
            seriesSet);
        const float yMax = std::max(1.0f, std::ceil(seriesMax * 1.15f));

        ImPlot::SetNextAxisLimits(ImAxis_X1, xMin, xMax, ImPlotCond_Always);
        ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, yMax, ImPlotCond_Always);
    }

    void DrawAsyncSeriesSummary(const char* label, const HistorySeries& series)
    {
        const SeriesStats stats = ComputeVisibleSeriesStats(
            series,
            s_asyncChunkMeshHistory.sampleIndices,
            s_asyncChunkMeshHistory.sampleCount);
        ImGui::BulletText("%s  latest=%.0f  avg=%.2f  peak=%.0f @ sample %.0f",
                          label,
                          stats.latest,
                          stats.average,
                          stats.peak,
                          stats.peakSampleX);
    }

    void DrawAsyncChunkMeshLivePipelinePlot()
    {
        if (!HasAsyncChunkMeshHistorySamples())
        {
            DrawAsyncChunkMeshHistoryUnavailableText();
            return;
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.queuedBacklog,
                                                &s_asyncChunkMeshHistory.pendingDispatchCount,
                                                &s_asyncChunkMeshHistory.activeHandleCount,
                                                &s_asyncChunkMeshHistory.schedulerQueuedCount,
                                                &s_asyncChunkMeshHistory.schedulerExecutingCount});

        if (ImPlot::BeginPlot("Recent Async Mesh Live Pipeline##AsyncChunkMeshLivePipeline", ImVec2(-1.0f, 240.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Chunk Count", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Queued Backlog", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.queuedBacklog), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Pending Dispatch", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.pendingDispatchCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Active Handles", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.activeHandleCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Scheduler Queued", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.schedulerQueuedCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Scheduler Executing", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.schedulerExecutingCount), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawAsyncSeriesSummary("Queued Backlog", s_asyncChunkMeshHistory.queuedBacklog);
        DrawAsyncSeriesSummary("Pending Dispatch", s_asyncChunkMeshHistory.pendingDispatchCount);
        DrawAsyncSeriesSummary("Active Handles", s_asyncChunkMeshHistory.activeHandleCount);
        DrawAsyncSeriesSummary("Scheduler Queued", s_asyncChunkMeshHistory.schedulerQueuedCount);
        DrawAsyncSeriesSummary("Scheduler Executing", s_asyncChunkMeshHistory.schedulerExecutingCount);
    }

    void DrawAsyncChunkMeshImportantPipelinePlot()
    {
        if (!HasAsyncChunkMeshHistorySamples())
        {
            DrawAsyncChunkMeshHistoryUnavailableText();
            return;
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.importantPendingCount,
                                                &s_asyncChunkMeshHistory.importantActiveCount,
                                                &s_asyncChunkMeshHistory.importantQueuedCount,
                                                &s_asyncChunkMeshHistory.importantExecutingCount});

        if (ImPlot::BeginPlot("Recent Important Chunk Pipeline##AsyncChunkMeshImportantPipeline", ImVec2(-1.0f, 220.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Important Chunk Count", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Important Pending", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.importantPendingCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Important Active", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.importantActiveCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Important Queued", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.importantQueuedCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Important Executing", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.importantExecutingCount), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawAsyncSeriesSummary("Important Pending", s_asyncChunkMeshHistory.importantPendingCount);
        DrawAsyncSeriesSummary("Important Active", s_asyncChunkMeshHistory.importantActiveCount);
        DrawAsyncSeriesSummary("Important Queued", s_asyncChunkMeshHistory.importantQueuedCount);
        DrawAsyncSeriesSummary("Important Executing", s_asyncChunkMeshHistory.importantExecutingCount);
    }

    void DrawAsyncChunkMeshFrameActivityPlot()
    {
        if (!HasAsyncChunkMeshHistorySamples())
        {
            DrawAsyncChunkMeshHistoryUnavailableText();
            return;
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.queued,
                                                &s_asyncChunkMeshHistory.submitted,
                                                &s_asyncChunkMeshHistory.executing,
                                                &s_asyncChunkMeshHistory.completed,
                                                &s_asyncChunkMeshHistory.published,
                                                &s_asyncChunkMeshHistory.retryLater,
                                                &s_asyncChunkMeshHistory.syncFallbackCount});

        if (ImPlot::BeginPlot("Recent Async Mesh Frame Activity##AsyncChunkMeshFrameActivity", ImVec2(-1.0f, 240.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Events / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Queued", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.queued), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Submitted", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.submitted), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Executing", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.executing), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Completed", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.completed), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Published", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.published), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Retry Later", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.retryLater), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Sync Fallback", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.syncFallbackCount), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawAsyncSeriesSummary("Queued", s_asyncChunkMeshHistory.queued);
        DrawAsyncSeriesSummary("Submitted", s_asyncChunkMeshHistory.submitted);
        DrawAsyncSeriesSummary("Executing", s_asyncChunkMeshHistory.executing);
        DrawAsyncSeriesSummary("Completed", s_asyncChunkMeshHistory.completed);
        DrawAsyncSeriesSummary("Published", s_asyncChunkMeshHistory.published);
        DrawAsyncSeriesSummary("Retry Later", s_asyncChunkMeshHistory.retryLater);
        DrawAsyncSeriesSummary("Sync Fallback", s_asyncChunkMeshHistory.syncFallbackCount);
    }

    void DrawAsyncChunkMeshMaterializationPlot()
    {
        if (!HasAsyncChunkMeshHistorySamples())
        {
            DrawAsyncChunkMeshHistoryUnavailableText();
            return;
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.workerMaterializationQueuedCount,
                                                &s_asyncChunkMeshHistory.workerMaterializationExecutingCount,
                                                &s_asyncChunkMeshHistory.workerMaterializationInFlight});

        if (ImPlot::BeginPlot("Recent Worker Materialization Live##AsyncChunkMeshMaterializationLive", ImVec2(-1.0f, 210.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Chunk Count", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Materialization Queued", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationQueuedCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Materialization Executing", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationExecutingCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Materialization In Flight", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationInFlight), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.mainThreadSnapshotBuildCount,
                                                &s_asyncChunkMeshHistory.workerMaterializationAttempts,
                                                &s_asyncChunkMeshHistory.workerMaterializationSucceeded,
                                                &s_asyncChunkMeshHistory.workerMaterializationRetryLater,
                                                &s_asyncChunkMeshHistory.workerMaterializationResolveFailed,
                                                &s_asyncChunkMeshHistory.workerMaterializationMissingNeighbors,
                                                &s_asyncChunkMeshHistory.workerMaterializationValidationFailed});

        if (ImPlot::BeginPlot("Recent Worker Materialization Outcomes##AsyncChunkMeshMaterializationOutcomes", ImVec2(-1.0f, 230.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Events / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Main-Thread Snapshot", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.mainThreadSnapshotBuildCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Worker Attempts", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationAttempts), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Worker Succeeded", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationSucceeded), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Worker Retry Later", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationRetryLater), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Resolve Failed", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationResolveFailed), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Missing Neighbors", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationMissingNeighbors), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Validation Failed", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.workerMaterializationValidationFailed), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawAsyncSeriesSummary("Main-Thread Snapshot", s_asyncChunkMeshHistory.mainThreadSnapshotBuildCount);
        DrawAsyncSeriesSummary("Worker Attempts", s_asyncChunkMeshHistory.workerMaterializationAttempts);
        DrawAsyncSeriesSummary("Worker Succeeded", s_asyncChunkMeshHistory.workerMaterializationSucceeded);
        DrawAsyncSeriesSummary("Worker Retry Later", s_asyncChunkMeshHistory.workerMaterializationRetryLater);
        DrawAsyncSeriesSummary("Resolve Failed", s_asyncChunkMeshHistory.workerMaterializationResolveFailed);
        DrawAsyncSeriesSummary("Missing Neighbors", s_asyncChunkMeshHistory.workerMaterializationMissingNeighbors);
        DrawAsyncSeriesSummary("Validation Failed", s_asyncChunkMeshHistory.workerMaterializationValidationFailed);
    }

    void DrawAsyncChunkMeshBoundedWaitPlot()
    {
        if (!HasAsyncChunkMeshHistorySamples())
        {
            DrawAsyncChunkMeshHistoryUnavailableText();
            return;
        }

        PrepareAsyncChunkMeshSlidingWindowPlot({&s_asyncChunkMeshHistory.boundedWaitAttempts,
                                                &s_asyncChunkMeshHistory.boundedWaitSatisfied,
                                                &s_asyncChunkMeshHistory.boundedWaitTimedOut,
                                                &s_asyncChunkMeshHistory.boundedWaitYieldCount,
                                                &s_asyncChunkMeshHistory.fallbackModePulses});

        if (ImPlot::BeginPlot("Recent Async Mesh Wait Assistance##AsyncChunkMeshBoundedWait", ImVec2(-1.0f, 220.0f)))
        {
            ImPlot::SetupAxes("Recent ImGui Samples", "Events / Sample", ImPlotAxisFlags_NoInitialFit, ImPlotAxisFlags_NoInitialFit);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
            ImPlot::PlotLine("Wait Attempts", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.boundedWaitAttempts), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Wait Satisfied", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.boundedWaitSatisfied), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Wait Timed Out", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.boundedWaitTimedOut), GetAsyncVisibleHistoryCount());
            ImPlot::PlotLine("Yield Count", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.boundedWaitYieldCount), GetAsyncVisibleHistoryCount());
            ImPlot::PlotStems("Fallback Active", GetAsyncVisibleHistoryXs(), GetAsyncVisibleHistorySeries(s_asyncChunkMeshHistory.fallbackModePulses), GetAsyncVisibleHistoryCount());
            ImPlot::TagX(GetAsyncVisibleHistoryMaxX(), ImVec4(1.0f, 1.0f, 1.0f, 0.85f), "Now");
            ImPlot::EndPlot();
        }

        DrawAsyncSeriesSummary("Wait Attempts", s_asyncChunkMeshHistory.boundedWaitAttempts);
        DrawAsyncSeriesSummary("Wait Satisfied", s_asyncChunkMeshHistory.boundedWaitSatisfied);
        DrawAsyncSeriesSummary("Wait Timed Out", s_asyncChunkMeshHistory.boundedWaitTimedOut);
        DrawAsyncSeriesSummary("Yield Count", s_asyncChunkMeshHistory.boundedWaitYieldCount);
        DrawAsyncSeriesSummary("Wait Microseconds", s_asyncChunkMeshHistory.boundedWaitMicroseconds);
        ImGui::BulletText("Fallback-active samples in visible window: %d",
                          CountVisibleNonZeroSamples(
                              s_asyncChunkMeshHistory.fallbackModePulses,
                              s_asyncChunkMeshHistory.sampleIndices,
                              s_asyncChunkMeshHistory.sampleCount));
    }
}

enigma::core::Json ImguiQueueDiagnosticsPanel::BuildCurrentFrameJson()
{
    if (!g_theRendererSubsystem)
    {
        return BuildUnavailableJson();
    }

    const QueueExecutionDiagnostics& diagnostics = g_theRendererSubsystem->GetQueueExecutionDiagnostics();
    UpdateFrameSnapshot(diagnostics);

    Json root = Json::object();
    Json queueDiagnostics = Json::object();

    queueDiagnostics["scope"] = "frame";
    queueDiagnostics["imguiFrame"] = s_frameSnapshot.imguiFrame;
    queueDiagnostics["requestedMode"] = GetQueueExecutionModeLabel(s_frameSnapshot.requestedMode);
    queueDiagnostics["activeMode"] = GetQueueExecutionModeLabel(s_frameSnapshot.activeMode);
    queueDiagnostics["lifecyclePhase"] = GetFrameLifecyclePhaseLabel(s_frameSnapshot.lifecyclePhase);
    queueDiagnostics["lastFallbackReason"] = GetQueueFallbackReasonLabel(s_frameSnapshot.lastFallbackReason);
    queueDiagnostics["requestedFramesInFlightDepth"] = s_frameSnapshot.requestedFramesInFlightDepth;
    queueDiagnostics["activeFramesInFlightDepth"] = s_frameSnapshot.activeFramesInFlightDepth;
    queueDiagnostics["submissions"] = BuildSubmissionJson(
        s_frameSnapshot.graphicsSubmissions,
        s_frameSnapshot.computeSubmissions,
        s_frameSnapshot.copySubmissions);
    queueDiagnostics["queueWaitInsertions"] = s_frameSnapshot.queueWaitInsertions;
    queueDiagnostics["workloadSubmissions"] = BuildWorkloadSubmissionJson(s_frameSnapshot.workloadSubmissionsByQueue);
    queueDiagnostics["queueWaits"] = BuildQueueWaitJson(s_frameSnapshot.queueWaitsByProducerConsumer);

    root["queueDiagnostics"] = queueDiagnostics;
    return root;
}

enigma::core::Json ImguiQueueDiagnosticsPanel::BuildAccumulatedJson()
{
    if (!g_theRendererSubsystem)
    {
        return BuildUnavailableJson();
    }

    const QueueExecutionDiagnostics& diagnostics = g_theRendererSubsystem->GetQueueExecutionDiagnostics();

    Json root = Json::object();
    Json queueDiagnostics = Json::object();

    queueDiagnostics["scope"] = "cumulative";
    queueDiagnostics["requestedMode"] = GetQueueExecutionModeLabel(diagnostics.requestedMode);
    queueDiagnostics["activeMode"] = GetQueueExecutionModeLabel(diagnostics.activeMode);
    queueDiagnostics["lifecyclePhase"] = GetFrameLifecyclePhaseLabel(diagnostics.lifecyclePhase);
    queueDiagnostics["lastFallbackReason"] = GetQueueFallbackReasonLabel(diagnostics.lastFallbackReason);
    queueDiagnostics["requestedFramesInFlightDepth"] = diagnostics.requestedFramesInFlightDepth;
    queueDiagnostics["activeFramesInFlightDepth"] = diagnostics.activeFramesInFlightDepth;
    queueDiagnostics["submissions"] = BuildSubmissionJson(
        diagnostics.graphicsSubmissions,
        diagnostics.computeSubmissions,
        diagnostics.copySubmissions);
    queueDiagnostics["queueWaitInsertions"] = diagnostics.queueWaitInsertions;
    queueDiagnostics["workloadSubmissions"] = BuildWorkloadSubmissionJson(diagnostics.workloadSubmissionsByQueue);
    queueDiagnostics["queueWaits"] = BuildQueueWaitJson(diagnostics.queueWaitsByProducerConsumer);
    queueDiagnostics["lastBeginFrameHadTrackedRetirement"] = diagnostics.lastBeginFrameHadTrackedRetirement;
    queueDiagnostics["lastBeginFrameWaitedOnRetirement"] = diagnostics.lastBeginFrameWaitedOnRetirement;
    queueDiagnostics["lastWaitedFrameSlot"] = diagnostics.lastWaitedFrameSlot;
    queueDiagnostics["lastRetirementParticipatingQueues"] = BuildQueueParticipationJson(diagnostics.lastRetirementParticipatingQueues);
    queueDiagnostics["lastRetirementExpectedQueues"] = BuildQueueParticipationJson(diagnostics.lastRetirementExpectedQueues);
    queueDiagnostics["lastRetirementMissingQueues"] = BuildQueueParticipationJson(diagnostics.lastRetirementMissingQueues);
    queueDiagnostics["lastRetirementWaitedQueues"] = BuildQueueParticipationJson(diagnostics.lastRetirementWaitedQueues);

    root["queueDiagnostics"] = queueDiagnostics;
    return root;
}

enigma::core::Json ImguiQueueDiagnosticsPanel::BuildAsyncChunkMeshCurrentFrameJson()
{
    const AsyncChunkMeshDiagnostics* diagnostics = GetAsyncChunkMeshDiagnosticsOrNull();
    if (!diagnostics)
    {
        return BuildAsyncChunkMeshUnavailableJson("World unavailable");
    }

    UpdateAsyncChunkMeshFrameSnapshot(*diagnostics);

    Json root = Json::object();
    Json asyncChunkMeshDiagnostics = Json::object();

    asyncChunkMeshDiagnostics["scope"] = "frame";
    asyncChunkMeshDiagnostics["imguiFrame"] = s_asyncChunkMeshFrameSnapshot.imguiFrame;
    asyncChunkMeshDiagnostics["mode"] = GetAsyncChunkMeshModeLabel(s_asyncChunkMeshFrameSnapshot.mode);
    asyncChunkMeshDiagnostics["asyncEnabled"] = s_asyncChunkMeshFrameSnapshot.asyncEnabled;
    asyncChunkMeshDiagnostics["fallbackActive"] = s_asyncChunkMeshFrameSnapshot.fallbackActive;
    asyncChunkMeshDiagnostics["lastFallbackReason"] = s_asyncChunkMeshFrameSnapshot.lastFallbackReason;
    asyncChunkMeshDiagnostics["lastWorkerMaterializationStatus"] = s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationStatus;
    asyncChunkMeshDiagnostics["lastWorkerMaterializationFailureReason"] = s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationFailureReason;
    asyncChunkMeshDiagnostics["live"] = BuildAsyncChunkMeshLiveJson(s_asyncChunkMeshFrameSnapshot.live);
    asyncChunkMeshDiagnostics["counters"] = BuildAsyncChunkMeshCountersJson(s_asyncChunkMeshFrameSnapshot.counters);

    root["asyncChunkMeshDiagnostics"] = asyncChunkMeshDiagnostics;
    return root;
}

enigma::core::Json ImguiQueueDiagnosticsPanel::BuildAsyncChunkMeshAccumulatedJson()
{
    const AsyncChunkMeshDiagnostics* diagnostics = GetAsyncChunkMeshDiagnosticsOrNull();
    if (!diagnostics)
    {
        return BuildAsyncChunkMeshUnavailableJson("World unavailable");
    }

    Json root = Json::object();
    Json asyncChunkMeshDiagnostics = Json::object();

    asyncChunkMeshDiagnostics["scope"] = "cumulative";
    asyncChunkMeshDiagnostics["mode"] = GetAsyncChunkMeshModeLabel(diagnostics->mode);
    asyncChunkMeshDiagnostics["asyncEnabled"] = diagnostics->asyncEnabled;
    asyncChunkMeshDiagnostics["fallbackActive"] = diagnostics->fallbackActive;
    asyncChunkMeshDiagnostics["lastFallbackReason"] = diagnostics->lastFallbackReason;
    asyncChunkMeshDiagnostics["lastWorkerMaterializationStatus"] = diagnostics->lastWorkerMaterializationStatus;
    asyncChunkMeshDiagnostics["lastWorkerMaterializationFailureReason"] = diagnostics->lastWorkerMaterializationFailureReason;
    asyncChunkMeshDiagnostics["live"] = BuildAsyncChunkMeshLiveJson(diagnostics->live);
    asyncChunkMeshDiagnostics["counters"] = BuildAsyncChunkMeshCountersJson(diagnostics->cumulative);

    root["asyncChunkMeshDiagnostics"] = asyncChunkMeshDiagnostics;
    return root;
}

void ImguiQueueDiagnosticsPanel::Show()
{
    if (!g_theRendererSubsystem)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "RendererSubsystem is unavailable.");
        return;
    }

    const QueueExecutionDiagnostics& diagnostics = g_theRendererSubsystem->GetQueueExecutionDiagnostics();
    const AsyncChunkMeshDiagnostics* asyncChunkMeshDiagnostics = GetAsyncChunkMeshDiagnosticsOrNull();
    UpdateFrameSnapshot(diagnostics);
    if (asyncChunkMeshDiagnostics)
    {
        UpdateAsyncChunkMeshFrameSnapshot(*asyncChunkMeshDiagnostics);
    }

    if ((ImGui::GetTime() - s_lastQueueCopyTime) < 1.5)
    {
        ImGui::TextDisabled("%s", s_lastQueueCopyLabel ? s_lastQueueCopyLabel : "Copied.");
    }

    if (ImGui::CollapsingHeader("Overview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Requested Mode: %s", GetQueueExecutionModeLabel(diagnostics.requestedMode));
        ImGui::Text("Active Mode: %s", GetQueueExecutionModeLabel(diagnostics.activeMode));
        ImGui::Text("Lifecycle Phase: %s", GetFrameLifecyclePhaseLabel(diagnostics.lifecyclePhase));
        ImGui::Text("Last Fallback Reason: %s", GetQueueFallbackReasonLabel(diagnostics.lastFallbackReason));
        ImGui::Text("Frames In Flight: requested=%u active=%u",
                    diagnostics.requestedFramesInFlightDepth,
                    diagnostics.activeFramesInFlightDepth);
        ImGui::Text("Fallback Events In Visible Window: %d", CountVisibleNonZeroSamples(s_history.fallbackEventPulses));
        if (asyncChunkMeshDiagnostics)
        {
            ImGui::Text("Async Mesh Mode: %s", GetAsyncChunkMeshModeLabel(asyncChunkMeshDiagnostics->mode));
            ImGui::Text("Async Mesh Fallback: %s",
                        asyncChunkMeshDiagnostics->fallbackActive ? "Active" : "Inactive");
            ImGui::Text("Async Mesh Backlog: queued=%llu pendingDispatch=%llu activeHandles=%llu",
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.queuedBacklog),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.pendingDispatchCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.activeHandleCount));
            ImGui::Text("Worker Materialization: queued=%llu executing=%llu inFlight=%llu",
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationQueuedCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationExecutingCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationInFlight));
            ImGui::Text("Last Worker Materialization: status=%s failure=%s",
                        asyncChunkMeshDiagnostics->lastWorkerMaterializationStatus.c_str(),
                        asyncChunkMeshDiagnostics->lastWorkerMaterializationFailureReason.c_str());
        }
        else
        {
            ImGui::TextDisabled("Async chunk mesh diagnostics are unavailable because the world is not active.");
        }

        ImGui::TextDisabled("Right-click inside the Queue tab to copy queue-only or async mesh JSON snapshots.");
    }

    if (ImGui::CollapsingHeader("Current Frame", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawSubmissionCounters(
            s_frameSnapshot.graphicsSubmissions,
            s_frameSnapshot.computeSubmissions,
            s_frameSnapshot.copySubmissions,
            s_frameSnapshot.queueWaitInsertions);
        ImGui::Text("Graphics Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(s_frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Graphics)));
        ImGui::Text("Compute Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(s_frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Compute)));
        ImGui::Text("Copy Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(s_frameSnapshot.queueWaitsByProducerConsumer, CommandQueueType::Copy)));
        ImGui::Text("CopyReadyUpload: %llu",
                    static_cast<unsigned long long>(
                        GetWorkloadSubmissionTotal(s_frameSnapshot.workloadSubmissionsByQueue, QueueWorkloadClass::CopyReadyUpload)));
        ImGui::Text("MipmapGeneration: %llu",
                    static_cast<unsigned long long>(
                        GetWorkloadSubmissionTotal(s_frameSnapshot.workloadSubmissionsByQueue, QueueWorkloadClass::MipmapGeneration)));
        ImGui::TextDisabled("Frame values are derived from the latest ImGui sample.");
    }

    if (ImGui::CollapsingHeader("Recent Queue Activity", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawQueueSubmissionHistoryPlot();
    }

    if (ImGui::CollapsingHeader("Recent Cross-Queue Waits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawQueueWaitHistoryPlot();
    }

    if (ImGui::CollapsingHeader("Recent Queue Share", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawQueueShareHistoryPlot();
    }

    if (ImGui::CollapsingHeader("Recent Lifecycle & Frames In Flight", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawLifecycleAndFramesInFlightPlot();
    }

    if (ImGui::CollapsingHeader("Recent Dedicated Workloads", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawDedicatedWorkloadHistoryPlot();
    }

    if (ImGui::CollapsingHeader("Accumulated", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawSubmissionCounters(
            diagnostics.graphicsSubmissions,
            diagnostics.computeSubmissions,
            diagnostics.copySubmissions,
            diagnostics.queueWaitInsertions);
        ImGui::Text("Graphics Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(diagnostics.queueWaitsByProducerConsumer, CommandQueueType::Graphics)));
        ImGui::Text("Compute Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(diagnostics.queueWaitsByProducerConsumer, CommandQueueType::Compute)));
        ImGui::Text("Copy Queue Waits: %llu",
                    static_cast<unsigned long long>(
                        GetQueueWaitTotalForConsumer(diagnostics.queueWaitsByProducerConsumer, CommandQueueType::Copy)));
        ImGui::TextDisabled("Counters are cumulative since startup.");
    }

    if (ImGui::CollapsingHeader("Accumulated Workload Highlights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawAccumulatedWorkloadHighlights(diagnostics.workloadSubmissionsByQueue);
    }

    if (ImGui::CollapsingHeader("Accumulated Cross-Queue Wait Routes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawAccumulatedQueueWaitRoutes(diagnostics.queueWaitsByProducerConsumer);
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Overview", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Async chunk mesh diagnostics cannot be sampled.");
        }
        else
        {
            ImGui::Text("Mode: %s", GetAsyncChunkMeshModeLabel(asyncChunkMeshDiagnostics->mode));
            ImGui::Text("Async Enabled: %s", asyncChunkMeshDiagnostics->asyncEnabled ? "true" : "false");
            ImGui::Text("Fallback Active: %s", asyncChunkMeshDiagnostics->fallbackActive ? "true" : "false");
            ImGui::Text("Last Fallback Reason: %s", asyncChunkMeshDiagnostics->lastFallbackReason.c_str());
            ImGui::Text("Last Worker Materialization Status: %s", s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationStatus.c_str());
            ImGui::Text("Last Worker Materialization Failure: %s", s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationFailureReason.c_str());
            ImGui::Text("Current Live: backlog=%llu pendingDispatch=%llu activeHandles=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.queuedBacklog),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.pendingDispatchCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.activeHandleCount));
            ImGui::Text("Worker Live: queued=%llu executing=%llu inFlight=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationQueuedCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationExecutingCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationInFlight));
            ImGui::Text("Current Frame: queued=%llu submitted=%llu executing=%llu completed=%llu published=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.queued),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.submitted),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.executing),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.completed),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.published));
            ImGui::Text("Worker Frame: mainThreadSnapshot=%llu attempts=%llu succeeded=%llu retryLater=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.mainThreadSnapshotBuildCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationAttempts),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationSucceeded),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationRetryLater));
            ImGui::Text("Current Frame Discards: stale=%llu cancelled=%llu retryLater=%llu syncFallback=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.discardedStale),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.discardedCancelled),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.retryLater),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.syncFallbackCount));
            ImGui::TextDisabled("Use the Queue tab context menu to copy frame or cumulative async mesh JSON.");
        }
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Live Pipeline", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Live async mesh pipeline state cannot be shown.");
        }
        else
        {
            ImGui::Text("Scheduler State: queued=%llu executing=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.schedulerQueuedCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.schedulerExecutingCount));
            ImGui::Text("Worker Materialization Live: queued=%llu executing=%llu inFlight=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationQueuedCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationExecutingCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationInFlight));
            ImGui::Text("Important Chunks: pending=%llu active=%llu queued=%llu executing=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.importantPendingCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.importantActiveCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.importantQueuedCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.importantExecutingCount));
            DrawAsyncChunkMeshLivePipelinePlot();
            DrawAsyncChunkMeshImportantPipelinePlot();
            ImGui::TextDisabled("Sliding window: last %d of %d stored samples.",
                                GetAsyncVisibleHistoryCount(),
                                s_asyncChunkMeshHistory.sampleCount);
        }
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Materialization", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Worker materialization state cannot be shown.");
        }
        else
        {
            ImGui::Text("Last Worker Status: %s", s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationStatus.c_str());
            ImGui::Text("Last Worker Failure: %s", s_asyncChunkMeshFrameSnapshot.lastWorkerMaterializationFailureReason.c_str());
            ImGui::Text("Live Worker State: queued=%llu executing=%llu inFlight=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationQueuedCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationExecutingCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.live.workerMaterializationInFlight));
            ImGui::Text("Frame Worker Outcomes: mainThreadSnapshot=%llu attempts=%llu succeeded=%llu retryLater=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.mainThreadSnapshotBuildCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationAttempts),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationSucceeded),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationRetryLater));
            ImGui::Text("Frame Worker Failures: resolveFailed=%llu missingNeighbors=%llu validationFailed=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationResolveFailed),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationMissingNeighbors),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationValidationFailed));
            DrawAsyncChunkMeshMaterializationPlot();
        }
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Frame Activity", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Frame activity cannot be shown.");
        }
        else
        {
            ImGui::Text("Frame Counters: queued=%llu submitted=%llu executing=%llu completed=%llu published=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.queued),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.submitted),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.executing),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.completed),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.published));
            ImGui::Text("Frame Rejections: stale=%llu cancelled=%llu retryLater=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.discardedStale),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.discardedCancelled),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.retryLater));
            ImGui::Text("Frame Worker Materialization: attempts=%llu succeeded=%llu mainThreadSnapshot=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationAttempts),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.workerMaterializationSucceeded),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.mainThreadSnapshotBuildCount));
            DrawAsyncChunkMeshFrameActivityPlot();
        }
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Important & Bounded Wait", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Important chunk wait assistance cannot be shown.");
        }
        else
        {
            ImGui::Text("Frame Wait Assistance: attempts=%llu satisfied=%llu timedOut=%llu yields=%llu waitedMicros=%llu",
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.boundedWaitAttempts),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.boundedWaitSatisfied),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.boundedWaitTimedOut),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.boundedWaitYieldCount),
                        static_cast<unsigned long long>(s_asyncChunkMeshFrameSnapshot.counters.boundedWaitMicroseconds));
            DrawAsyncChunkMeshBoundedWaitPlot();
        }
    }

    if (ImGui::CollapsingHeader("Async Chunk Mesh Accumulated", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextDisabled("World unavailable. Cumulative async chunk mesh counters cannot be shown.");
        }
        else
        {
            const AsyncChunkMeshCounters& counters = asyncChunkMeshDiagnostics->cumulative;
            ImGui::Text("Lifecycle: queued=%llu submitted=%llu executing=%llu completed=%llu published=%llu",
                        static_cast<unsigned long long>(counters.queued),
                        static_cast<unsigned long long>(counters.submitted),
                        static_cast<unsigned long long>(counters.executing),
                        static_cast<unsigned long long>(counters.completed),
                        static_cast<unsigned long long>(counters.published));
            ImGui::Text("Reject/Retry: stale=%llu cancelled=%llu retryLater=%llu syncFallback=%llu",
                        static_cast<unsigned long long>(counters.discardedStale),
                        static_cast<unsigned long long>(counters.discardedCancelled),
                        static_cast<unsigned long long>(counters.retryLater),
                        static_cast<unsigned long long>(counters.syncFallbackCount));
            ImGui::Text("Worker Materialization: mainThreadSnapshot=%llu attempts=%llu succeeded=%llu retryLater=%llu",
                        static_cast<unsigned long long>(counters.mainThreadSnapshotBuildCount),
                        static_cast<unsigned long long>(counters.workerMaterializationAttempts),
                        static_cast<unsigned long long>(counters.workerMaterializationSucceeded),
                        static_cast<unsigned long long>(counters.workerMaterializationRetryLater));
            ImGui::Text("Worker Failures: resolveFailed=%llu missingNeighbors=%llu validationFailed=%llu",
                        static_cast<unsigned long long>(counters.workerMaterializationResolveFailed),
                        static_cast<unsigned long long>(counters.workerMaterializationMissingNeighbors),
                        static_cast<unsigned long long>(counters.workerMaterializationValidationFailed));
            ImGui::Text("Bounded Wait: attempts=%llu satisfied=%llu timedOut=%llu yields=%llu waitedMicros=%llu",
                        static_cast<unsigned long long>(counters.boundedWaitAttempts),
                        static_cast<unsigned long long>(counters.boundedWaitSatisfied),
                        static_cast<unsigned long long>(counters.boundedWaitTimedOut),
                        static_cast<unsigned long long>(counters.boundedWaitYieldCount),
                        static_cast<unsigned long long>(counters.boundedWaitMicroseconds));
            ImGui::Text("Live Snapshot: backlog=%llu pendingDispatch=%llu activeHandles=%llu",
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.queuedBacklog),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.pendingDispatchCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.activeHandleCount));
            ImGui::Text("Worker Live Snapshot: queued=%llu executing=%llu inFlight=%llu",
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationQueuedCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationExecutingCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.workerMaterializationInFlight));
            ImGui::Text("Important Snapshot: pending=%llu active=%llu queued=%llu executing=%llu",
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.importantPendingCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.importantActiveCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.importantQueuedCount),
                        static_cast<unsigned long long>(asyncChunkMeshDiagnostics->live.importantExecutingCount));
            ImGui::TextDisabled("Async mesh counters are cumulative since startup. Live snapshot reflects the latest frame sample.");
        }
    }

    if (ImGui::CollapsingHeader("Interpretation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const uint64_t frameDedicatedSubmissions = s_frameSnapshot.computeSubmissions + s_frameSnapshot.copySubmissions;
        const uint64_t cumulativeDedicatedSubmissions = diagnostics.computeSubmissions + diagnostics.copySubmissions;
        const uint64_t cumulativeAsyncUploadSubmissions =
            diagnostics.GetWorkloadSubmissionTotal(QueueWorkloadClass::CopyReadyUpload);
        const uint64_t cumulativeMipmapSubmissions =
            diagnostics.GetWorkloadSubmissionTotal(QueueWorkloadClass::MipmapGeneration);
        const uint64_t cumulativeQueueWaits = GetQueueWaitTotal(diagnostics.queueWaitsByProducerConsumer);

        if (cumulativeDedicatedSubmissions == 0)
        {
            ImGui::TextWrapped("The current run has not shown dedicated compute or copy submissions yet. This build is still behaving like a graphics-only workload.");
        }
        else if (frameDedicatedSubmissions == 0)
        {
            ImGui::TextWrapped("Dedicated queues have been observed cumulatively, but the current sampled frame is graphics-only. That usually means async work is bursty and tied to upload or mip-generation events.");
        }
        else if (diagnostics.computeSubmissions == 0)
        {
            ImGui::TextWrapped("Copy queue work has been observed, but compute submissions have not occurred in this run.");
        }
        else if (diagnostics.copySubmissions == 0)
        {
            ImGui::TextWrapped("Compute queue work has been observed, but copy submissions have not occurred in this run.");
        }
        else
        {
            ImGui::TextWrapped("Dedicated compute and copy queue submissions have both been observed in this run.");
        }

        ImGui::Separator();
        ImGui::TextWrapped("Cumulative async upload submissions: %llu", static_cast<unsigned long long>(cumulativeAsyncUploadSubmissions));
        ImGui::TextWrapped("Cumulative mip-generation submissions: %llu", static_cast<unsigned long long>(cumulativeMipmapSubmissions));
        ImGui::TextWrapped("Cumulative cross-queue waits: %llu", static_cast<unsigned long long>(cumulativeQueueWaits));

        ImGui::Separator();
        if (!asyncChunkMeshDiagnostics)
        {
            ImGui::TextWrapped("Async chunk mesh diagnostics are unavailable because the world is not active.");
        }
        else if (asyncChunkMeshDiagnostics->cumulative.mainThreadSnapshotBuildCount > 0)
        {
            ImGui::TextWrapped("Main-thread snapshot construction has been observed in this run. That means the old pre-dispatch hotspot path has regressed and should be treated as a correctness/performance warning.");
        }
        else if (asyncChunkMeshDiagnostics->fallbackActive ||
                 asyncChunkMeshDiagnostics->mode == AsyncChunkMeshMode::SyncFallback ||
                 !asyncChunkMeshDiagnostics->asyncEnabled)
        {
            ImGui::TextWrapped("Async chunk meshing is currently in synchronous fallback mode. Main-thread hitch analysis should treat meshing as no longer isolated from gameplay/update work.");
        }
        else if (asyncChunkMeshDiagnostics->live.workerMaterializationInFlight > 0 ||
                 asyncChunkMeshDiagnostics->cumulative.workerMaterializationAttempts > 0)
        {
            ImGui::TextWrapped("Worker-side materialization has been observed in this run. If hitchs remain while these counters are healthy, focus next on meshing cost, publish cost, or region rebuild follow-up instead of the old dispatch-time snapshot path.");
        }
        else if (s_asyncChunkMeshFrameSnapshot.live.schedulerExecutingCount > 0 ||
                 s_asyncChunkMeshFrameSnapshot.live.importantExecutingCount > 0)
        {
            ImGui::TextWrapped("Async chunk meshing is actively executing worker jobs in the latest sampled frame. Hitchs that remain are more likely to come from publish, region rebuild, or downstream CPU work.");
        }
        else if (s_asyncChunkMeshFrameSnapshot.live.queuedBacklog > 0 ||
                 s_asyncChunkMeshFrameSnapshot.live.pendingDispatchCount > 0 ||
                 s_asyncChunkMeshFrameSnapshot.live.schedulerQueuedCount > 0)
        {
            ImGui::TextWrapped("Async chunk meshing currently has backlog ahead of execution. If visible hitchs align with this state, the bottleneck is likely in queueing, scheduler throughput, or publish catch-up rather than GPU submission.");
        }
        else if (asyncChunkMeshDiagnostics->cumulative.published > 0)
        {
            ImGui::TextWrapped("Async chunk meshing has already published results in this run and is currently idle. Remaining frame spikes should be investigated in region rebuild, upload, or gameplay-side invalidation bursts.");
        }
        else
        {
            ImGui::TextWrapped("No async chunk mesh publishes have been observed in this run yet.");
        }

        if (asyncChunkMeshDiagnostics)
        {
            ImGui::TextWrapped("Async mesh cumulative published=%llu, workerAttempts=%llu, mainThreadSnapshot=%llu, retryLater=%llu, syncFallback=%llu, boundedWaitTimedOut=%llu",
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.published),
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.workerMaterializationAttempts),
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.mainThreadSnapshotBuildCount),
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.retryLater),
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.syncFallbackCount),
                               static_cast<unsigned long long>(asyncChunkMeshDiagnostics->cumulative.boundedWaitTimedOut));
        }
    }
}

void ImguiQueueDiagnosticsPanel::CopyCurrentFrameJsonToClipboard()
{
    const std::string json = BuildCurrentFrameJson().dump(4);
    ImGui::SetClipboardText(json.c_str());
    RecordQueueCopy("Queue frame JSON copied.");
}

void ImguiQueueDiagnosticsPanel::CopyAccumulatedJsonToClipboard()
{
    const std::string json = BuildAccumulatedJson().dump(4);
    ImGui::SetClipboardText(json.c_str());
    RecordQueueCopy("Queue accumulated JSON copied.");
}

void ImguiQueueDiagnosticsPanel::CopyAsyncChunkMeshCurrentFrameJsonToClipboard()
{
    const std::string json = BuildAsyncChunkMeshCurrentFrameJson().dump(4);
    ImGui::SetClipboardText(json.c_str());
    RecordQueueCopy("Async mesh frame JSON copied.");
}

void ImguiQueueDiagnosticsPanel::CopyAsyncChunkMeshAccumulatedJsonToClipboard()
{
    const std::string json = BuildAsyncChunkMeshAccumulatedJson().dump(4);
    ImGui::SetClipboardText(json.c_str());
    RecordQueueCopy("Async mesh accumulated JSON copied.");
}
