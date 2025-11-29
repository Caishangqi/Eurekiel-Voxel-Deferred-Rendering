#include "ImguiPlayerDebugInfo.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiPlayerDebugInfo::ShowWindow(bool* pOpen)
{
    if (!pOpen) return;
    ImGui::Separator();
    PlayerCharacter* player = g_theGame->m_player.get();
    if (player)
        ImGui::Text("Player Camera Position: (%.1f,%.1f,%.1f)", player->m_position.x, player->m_position.y, player->m_position.z);
    else
    {
        ImGui::Text("Player Camera Position: <invalid>");
    }
    if (player)
    {
        ImGui::Text("Player Camera Rotation: (%.1f,%.1f,%.1f)", player->m_orientation.m_yawDegrees, player->m_orientation.m_pitchDegrees, player->m_orientation.m_rollDegrees);
    }
    else
    {
        ImGui::Text("Player Camera Rotation: <invalid>");
    }
}
