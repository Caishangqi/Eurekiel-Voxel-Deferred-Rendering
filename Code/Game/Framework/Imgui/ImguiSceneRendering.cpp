/**
 * @file ImguiSceneRendering.cpp
 * @brief ImGui Scene Rendering Tab Implementation
 * @date 2025-11-26
 */

#include "ImguiSceneRendering.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Framework/RenderPass/RenderCloud/ImguiSettingCloud.hpp"
#include "Game/Framework/RenderPass/RenderCloud/CloudRenderPass.hpp"
#include "Game/Framework/RenderPass/RenderSky/ImguiSettingSky.hpp"
#include "Game/Framework/RenderPass/RenderSky/SkyRenderPass.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/imgui/imgui.h"

/**
 * @brief Show Scene Rendering tab content
 */
void ImguiSceneRendering::Show()
{
    // [MODULE 1] Sky Rendering
    // Note: ImguiSettingSky::Show() requires SkyRenderPass* parameter
    // This will be integrated in Task 18 when connecting to Game::RenderImGui()
    // For now, we create the UI structure

    // [MODULE 2] Cloud Rendering
    CloudRenderPass* cloudPass = dynamic_cast<CloudRenderPass*>(g_theGame->m_cloudRenderPass.get());
    if (cloudPass)
    {
        ImguiSettingCloud::Show(cloudPass);
    }

    ImguiSettingSky::Show(dynamic_cast<SkyRenderPass*>(g_theGame->m_skyRenderPass.get()));

    // [FUTURE] Add more rendering modules here
    // - Terrain Rendering
    // - Water Rendering
    // - Post-Processing Effects
}
