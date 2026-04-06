/**
 * @file ImguiGameSettings.cpp
 * @brief ImGui Game Settings Main Window Implementation
 * @date 2025-11-26
 */

#include "ImguiGameSettings.hpp"
#include "ImguiGameLogic.hpp"
#include "ImguiSceneRendering.hpp"
#include "ImguiSettingChunkBatching.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * @brief Show main game settings window with TabBar
 */
void ImguiGameSettings::ShowWindow(bool* pOpen)
{
    // [STEP 1] Create Main Window
    if (!ImGui::Begin("Game Settings", pOpen, ImGuiWindowFlags_None))
    {
        // Window is collapsed, early out
        ImGui::End();
        return;
    }

    // [STEP 2] Create TabBar Structure
    if (ImGui::BeginTabBar("GameSettingsTabs", ImGuiTabBarFlags_None))
    {
        // [TAB 1] Game Logic Tab
        if (ImGui::BeginTabItem("Game Logic"))
        {
            // Call Game Logic UI
            ImguiGameLogic::Show();

            ImGui::EndTabItem();
        }

        // [TAB 2] Scene Rendering Tab
        if (ImGui::BeginTabItem("Scene Rendering"))
        {
            // Call Scene Rendering UI
            ImguiSceneRendering::Show();

            ImGui::EndTabItem();
        }

        // [TAB 3] Chunk Batching Tab
        if (ImGui::BeginTabItem("Chunk Batching"))
        {
            ImguiSettingChunkBatching::Show(g_theGame ? g_theGame->GetWorld() : nullptr);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // [STEP 3] End Main Window
    ImGui::End();
}
