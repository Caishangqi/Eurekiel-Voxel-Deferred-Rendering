#include "ImguiLeftDebugOverlay.hpp"

#include "Engine/Core/Clock.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Framework/GameObject/ImguiPlayerDebugInfo.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

// ============================================================================
// FPS History Tracker - ring buffer for average FPS over 1s/5s/10s windows
// ============================================================================
static constexpr int FPS_HISTORY_MAX_SECONDS = 10;
static constexpr int FPS_HISTORY_CAPACITY    = 2400; // 10s * 240fps max sampling (covers 60/120/144/240Hz)
static float         s_frameTimeHistory[FPS_HISTORY_CAPACITY];
static float         s_frameTimeStamps[FPS_HISTORY_CAPACITY];
static int           s_historyHead     = 0;
static int           s_historyCount    = 0;
static float         s_maxFrameReached = 0.0f;

// Smoothed display values (updated every UPDATE_INTERVAL to reduce flicker)
static constexpr float DISPLAY_UPDATE_INTERVAL = 0.25f; // update display 4x per second
static float           s_displayFPS            = 0.0f;
static float           s_displayMs             = 0.0f;
static float           s_displayAvg1s          = 0.0f;
static float           s_displayAvg5s          = 0.0f;
static float           s_displayAvg10s         = 0.0f;
static float           s_lastDisplayUpdate     = 0.0f;

static void PushFrameSample(float deltaSeconds, float totalSeconds)
{
    s_frameTimeHistory[s_historyHead] = deltaSeconds;
    s_frameTimeStamps[s_historyHead]  = totalSeconds;
    s_historyHead                     = (s_historyHead + 1) % FPS_HISTORY_CAPACITY;
    if (s_historyCount < FPS_HISTORY_CAPACITY)
        s_historyCount++;
}

static float CalcAverageFPS(float totalSeconds, float windowSeconds)
{
    if (s_historyCount == 0)
        return 0.0f;

    float cutoff   = totalSeconds - windowSeconds;
    float sumDelta = 0.0f;
    int   count    = 0;

    for (int i = 0; i < s_historyCount; ++i)
    {
        int idx = (s_historyHead - 1 - i + FPS_HISTORY_CAPACITY) % FPS_HISTORY_CAPACITY;
        if (s_frameTimeStamps[idx] < cutoff)
            break;
        sumDelta += s_frameTimeHistory[idx];
        count++;
    }

    return (count > 0 && sumDelta > 0.0f) ? (float)count / sumDelta : 0.0f;
}

void ImguiLeftDebugOverlay::ShowWindow(bool* pOpen)
{
    static int       location     = 0;
    ImGuiIO&         io           = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (location >= 0)
    {
        const float          PAD       = 10.0f;
        const ImGuiViewport* viewport  = ImGui::GetMainViewport();
        ImVec2               work_pos  = viewport->WorkPos;
        ImVec2               work_size = viewport->WorkSize;
        ImVec2               window_pos, window_pos_pivot;
        window_pos.x       = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y       = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    else if (location == -2)
    {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Debugger Overlay", pOpen, window_flags))
    {
        // Record frame sample
        const Clock* gameClock    = g_theGame->GetGameClock();
        float        deltaSeconds = gameClock->GetDeltaSeconds();
        float        totalSeconds = gameClock->GetTotalSeconds();
        float        currentFPS   = gameClock->GetFrameRate();
        PushFrameSample(deltaSeconds, totalSeconds);

        if (currentFPS > s_maxFrameReached) s_maxFrameReached = currentFPS;

        // Update smoothed display values at fixed interval (anti-flicker)
        if (totalSeconds - s_lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
        {
            s_displayFPS        = currentFPS;
            s_displayMs         = deltaSeconds * 1000.0f;
            s_displayAvg1s      = CalcAverageFPS(totalSeconds, 1.0f);
            s_displayAvg5s      = CalcAverageFPS(totalSeconds, 5.0f);
            s_displayAvg10s     = CalcAverageFPS(totalSeconds, 10.0f);
            s_lastDisplayUpdate = totalSeconds;
        }

        // FPS display
        ImGui::Text("FPS: %.1f (%.2f ms) | Max: %.1f", s_displayFPS, s_displayMs, s_maxFrameReached);
        ImGui::Text("Avg 1/5/10s: %.1f / %.1f / %.1f", s_displayAvg1s, s_displayAvg5s, s_displayAvg10s);

        ImGui::Separator();
        ImGui::Text("Debugger Overlay");
        ImGui::Separator();
        if (ImGui::IsMousePosValid())
            ImGui::Text("Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
        else
            ImGui::Text("Mouse Position: <invalid>");
        ImguiPlayerDebugInfo::ShowWindow(pOpen);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom", NULL, location == -1)) location = -1;
            if (ImGui::MenuItem("Center", NULL, location == -2)) location = -2;
            if (ImGui::MenuItem("Top-left", NULL, location == 0)) location = 0;
            if (ImGui::MenuItem("Top-right", NULL, location == 1)) location = 1;
            if (ImGui::MenuItem("Bottom-left", NULL, location == 2)) location = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, location == 3)) location = 3;
            if (pOpen && ImGui::MenuItem("Close")) *pOpen = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}
