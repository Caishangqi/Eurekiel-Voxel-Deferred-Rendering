/**
 * @file CloudConfigParser.hpp
 * @brief Cloud rendering configuration parser
 * @date 2025-12-04
 *
 * Responsibilities:
 * - Parse cloud settings from settings.yml (video.cloud section)
 * - Provide CloudConfig struct with all cloud parameters
 * - Support runtime modification via ImguiSettingCloud
 *
 * Configuration Path: Run/.enigma/settings.yml -> video.cloud
 *
 * Reference: WindowConfigParser.hpp (similar pattern)
 */

#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>

#include "Engine/Core/Yaml.hpp"

// Forward declaration
enum class CloudStatus;

/**
 * @struct CloudConfig
 * @brief All cloud rendering parameters
 *
 * Loaded from settings.yml video.cloud section.
 * Can be modified at runtime via ImguiSettingCloud.
 */
struct CloudConfig
{
    // [Basic Settings]
    bool        enabled = true; // Cloud rendering enabled
    CloudStatus renderMode; // FAST or FANCY mode

    // [Geometry Parameters]
    float height         = 20.0f; // Cloud layer base height (Z-axis)
    float thickness      = 4.0f; // Cloud layer thickness (4 blocks)
    int   renderDistance = 16; // Render distance in cells (16 cells = 192 blocks)

    // [Animation Parameters]
    float speed = 1.0f; // Cloud scroll speed multiplier

    // [Visual Parameters]
    float opacity = 0.8f; // Cloud opacity (0.0-1.0)

    // [Computed Values]
    float GetMinZ() const { return height; }
    float GetMaxZ() const { return height + thickness; }
};

/**
 * @class CloudConfigParser
 * @brief Parse and manage cloud configuration from YAML
 *
 * Usage:
 * @code
 * CloudConfigParser parser(".enigma/settings.yml");
 * CloudConfig& config = parser.GetParsedConfig();
 * cloudRenderPass->ApplyConfig(config);
 * @endcode
 */
class CloudConfigParser
{
public:
    // [Static] CloudStatus string mapping (similar to WindowConfigParser)
    static const std::unordered_map<std::string, CloudStatus> s_cloudModeMap;

public:
    CloudConfigParser() = default;
    explicit CloudConfigParser(const std::filesystem::path& configFilePath);
    ~CloudConfigParser() = default;

    // [Load/Save]
    bool LoadFromYaml(const std::string& yamlPath);
    bool SaveToYaml(const std::string& yamlPath) const;

    // [Accessors]
    const enigma::core::YamlConfiguration& GetConfig() const { return m_config; }
    CloudConfig&                           GetParsedConfig() { return m_parsedConfig; }
    const CloudConfig&                     GetParsedConfig() const { return m_parsedConfig; }

    // [Validation]
    static bool ValidateConfig(const CloudConfig& config);

private:
    // [Parse Helpers]
    static CloudStatus ParseRenderMode(const std::string& modeString);
    static std::string RenderModeToString(CloudStatus mode);

private:
    enigma::core::YamlConfiguration m_config;
    CloudConfig                     m_parsedConfig;
};
