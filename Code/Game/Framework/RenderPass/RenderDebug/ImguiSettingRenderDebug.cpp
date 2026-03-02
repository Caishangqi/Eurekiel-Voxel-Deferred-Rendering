#include "ImguiSettingRenderDebug.hpp"

#include "DebugRenderPass.hpp"
#include "ThirdParty/imgui/imgui.h"

void ImguiSettingRenderDebug::Show()
{
    if (ImGui::CollapsingHeader("Debug Rendering"))
    {
        ImGui::Indent();
        ImGui::Checkbox("Enable Coordinate Gizmos", &DebugRenderPass::ENABLE_COORDINATE_GIZMOS);
        ImGui::Checkbox("Enable Light Direction Gizmos", &DebugRenderPass::ENABLE_LIGHT_DIRECTION_DEBUG_DRAW);
        ImGui::Unindent();
    }
}
