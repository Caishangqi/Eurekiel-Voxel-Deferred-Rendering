/**
 * @file CloudConfigParser.cpp
 * @brief Cloud rendering configuration parser implementation
 * @date 2025-12-04
 *
 * Reference: WindowConfigParser.cpp (similar pattern for enum mapping)
 */

#include "CloudConfigParser.hpp"
#include "CloudRenderPass.hpp"  // For CloudStatus enum
#include "Engine/Core/Yaml.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

using namespace enigma::core;

// ========================================
// Static Members
// ========================================

const std::unordered_map<std::string, CloudStatus> CloudConfigParser::s_cloudModeMap = {
    {"fast", CloudStatus::FAST},
    {"fancy", CloudStatus::FANCY}
};

// ========================================
// Constructor
// ========================================

CloudConfigParser::CloudConfigParser(const std::filesystem::path& configFilePath)
{
    LoadFromYaml(configFilePath.string());
}

// ========================================
// Load/Save
// ========================================

bool CloudConfigParser::LoadFromYaml(const std::string& yamlPath)
{
    try
    {
        m_config = YamlConfiguration::LoadFromFile(yamlPath);

        DebuggerPrintf("Loading cloud config from: %s\n", yamlPath.c_str());

        // [Basic Settings]
        m_parsedConfig.enabled = m_config.GetBoolean("video.cloud.enabled", true);

        std::string renderModeStr = m_config.GetString("video.cloud.renderMode", "fancy");
        m_parsedConfig.renderMode = ParseRenderMode(renderModeStr);
        DebuggerPrintf("Parsed cloud render mode: %s -> %d\n", renderModeStr.c_str(), static_cast<int>(m_parsedConfig.renderMode));

        // [Geometry Parameters]
        m_parsedConfig.height         = m_config.GetFloat("video.cloud.height", 20.0f);
        m_parsedConfig.thickness      = m_config.GetFloat("video.cloud.thickness", 4.0f);
        m_parsedConfig.renderDistance = m_config.GetInt("video.cloud.renderDistance", 16);
        DebuggerPrintf("Parsed cloud geometry: height=%.1f, thickness=%.1f, renderDistance=%d\n",
                       m_parsedConfig.height, m_parsedConfig.thickness, m_parsedConfig.renderDistance);

        // [Animation Parameters]
        m_parsedConfig.speed = m_config.GetFloat("video.cloud.speed", 1.0f);

        // [Visual Parameters]
        m_parsedConfig.opacity = m_config.GetFloat("video.cloud.opacity", 0.8f);
        DebuggerPrintf("Parsed cloud visual: speed=%.2f, opacity=%.2f\n",
                       m_parsedConfig.speed, m_parsedConfig.opacity);

        // [Validation]
        if (!ValidateConfig(m_parsedConfig))
        {
            DebuggerPrintf("Warning: Invalid cloud configuration detected, using defaults\n");
        }

        return true;
    }
    catch (const std::exception& e)
    {
        ERROR_RECOVERABLE(Stringf("Error loading cloud config from %s: %s", yamlPath.c_str(), e.what()));
        return false;
    }
}

bool CloudConfigParser::SaveToYaml(const std::string& yamlPath) const
{
    UNUSED(yamlPath)
    // [TODO] Implement save functionality if needed for runtime config persistence
    // For now, settings are read-only from file
    DebuggerPrintf("CloudConfigParser::SaveToYaml not implemented yet\n");
    return false;
}

// ========================================
// Validation
// ========================================

bool CloudConfigParser::ValidateConfig(const CloudConfig& config)
{
    // Validate height (reasonable range for cloud layer)
    if (config.height < 0.0f || config.height > 500.0f)
    {
        return false;
    }

    // Validate thickness (must be positive)
    if (config.thickness <= 0.0f || config.thickness > 100.0f)
    {
        return false;
    }

    // Validate render distance (1-64 cells)
    if (config.renderDistance < 1 || config.renderDistance > 64)
    {
        return false;
    }

    // Validate speed (reasonable range)
    if (config.speed < 0.0f || config.speed > 10.0f)
    {
        return false;
    }

    // Validate opacity (0.0-1.0)
    if (config.opacity < 0.0f || config.opacity > 1.0f)
    {
        return false;
    }

    return true;
}

// ========================================
// Parse Helpers
// ========================================

CloudStatus CloudConfigParser::ParseRenderMode(const std::string& modeString)
{
    auto it = s_cloudModeMap.find(modeString);
    if (it != s_cloudModeMap.end())
    {
        return it->second;
    }

    DebuggerPrintf("Warning: Unknown cloud render mode '%s', defaulting to fancy\n", modeString.c_str());
    return CloudStatus::FANCY;
}

std::string CloudConfigParser::RenderModeToString(CloudStatus mode)
{
    switch (mode)
    {
    case CloudStatus::FAST: return "fast";
    case CloudStatus::FANCY: return "fancy";
    default: return "fancy";
    }
}
