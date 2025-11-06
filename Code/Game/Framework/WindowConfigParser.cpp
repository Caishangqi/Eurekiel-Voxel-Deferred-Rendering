#include "WindowConfigParser.hpp"
#include "Engine/Core/Yaml.hpp"
#include "Engine/Core/EngineCommon.hpp"

using namespace enigma::core;

const std::unordered_map<std::string, WindowMode> WindowConfigParser::s_windowModeMap = {
    {"windowed", WindowMode::Windowed},
    {"fullscreen", WindowMode::Fullscreen},
    {"borderlessFullscreen", WindowMode::BorderlessFullscreen}
};

WindowConfig WindowConfigParser::LoadFromYaml(const std::string& yamlPath)
{
    WindowConfig config;
    
    try
    {
        auto yamlConfig = YamlConfiguration::LoadFromFile(yamlPath);
        
        // 调试输出：确认配置文件加载
        DebuggerPrintf("Loading window config from: %s\n", yamlPath.c_str());
        
        // 解析窗口模式
        std::string windowModeStr = yamlConfig.GetString("video.windowMode", "windowed");
        config.m_windowMode = ParseWindowMode(windowModeStr);
        DebuggerPrintf("Parsed window mode: %s -> %d\n", windowModeStr.c_str(), (int)config.m_windowMode);
        
        // 解析分辨率
        config.m_resolution = ParseResolution(yamlPath);
        DebuggerPrintf("Parsed resolution: %dx%d\n", config.m_resolution.x, config.m_resolution.y);
        
        // 解析宽高比
        config.m_aspectRatio = ParseAspectRatio(yamlPath);
        DebuggerPrintf("Parsed aspect ratio: %f\n", config.m_aspectRatio);
        
        // 解析窗口标题
        std::string appName = yamlConfig.GetString("general.appName", "SimpleMiner");
        config.m_windowTitle = appName;
        DebuggerPrintf("Parsed window title: %s\n", config.m_windowTitle.c_str());
        
        // 验证配置
        if (!ValidateConfig(config))
        {
            DebuggerPrintf("Warning: Invalid window configuration detected, using defaults\n");
        }
    }
    catch (const std::exception& e)
    {
        DebuggerPrintf("Error loading window config from %s: %s\n", yamlPath.c_str(), e.what());
        DebuggerPrintf("Using default window configuration\n");
        // 确保使用合理的默认值
        config.m_windowTitle = "SimpleMiner";
        config.m_windowMode = WindowMode::Windowed;
        config.m_resolution = IntVec2(1600, 900);
        config.m_aspectRatio = 16.0f / 9.0f;
    }
    
    return config;
}

WindowMode WindowConfigParser::ParseWindowMode(const std::string& modeString)
{
    auto it = s_windowModeMap.find(modeString);
    if (it != s_windowModeMap.end())
    {
        return it->second;
    }
    
    DebuggerPrintf("Warning: Unknown window mode '%s', defaulting to windowed\n", modeString.c_str());
    return WindowMode::Windowed;
}

IntVec2 WindowConfigParser::ParseResolution(const std::string& configPath)
{
    IntVec2 resolution(1600, 900); // 默认分辨率
    
    try
    {
        auto yamlConfig = YamlConfiguration::LoadFromFile(configPath);
        
        // 使用修复后的连点语法直接获取分辨率
        if (yamlConfig.Contains("video.resolution.width") && yamlConfig.Contains("video.resolution.height"))
        {
            int width = yamlConfig.GetInt("video.resolution.width", 1600);
            int height = yamlConfig.GetInt("video.resolution.height", 900);
            
            if (width >= 640 && width <= 7680 && height >= 480 && height <= 4320)
            {
                resolution = IntVec2(width, height);
                DebuggerPrintf("Loaded resolution from YAML config: %dx%d\n", width, height);
                return resolution;
            }
        }
        
        // 回退到直接 yaml-cpp（备用方案）
        YAML::Node yamlNode = YAML::LoadFile(configPath);
        
        if (yamlNode["video"] && yamlNode["video"]["resolution"])
        {
            const YAML::Node& resolutionNode = yamlNode["video"]["resolution"];
            
            if (resolutionNode["width"] && resolutionNode["height"])
            {
                int width = resolutionNode["width"].as<int>(1600);
                int height = resolutionNode["height"].as<int>(900);
                
                if (width >= 640 && width <= 7680 && height >= 480 && height <= 4320)
                {
                    resolution = IntVec2(width, height);
                    DebuggerPrintf("Loaded resolution using fallback yaml-cpp: %dx%d\n", width, height);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        DebuggerPrintf("Error parsing resolution: %s\n", e.what());
    }
    
    return resolution;
}

float WindowConfigParser::ParseAspectRatio(const std::string& configPath)
{
    float aspectRatio = 16.0f / 9.0f; // 默认宽高比
    
    try
    {
        // 直接使用 yaml-cpp，避免 YamlConfiguration 的问题
        YAML::Node yamlNode = YAML::LoadFile(configPath);
        
        if (yamlNode["video"] && yamlNode["video"]["aspectRatio"])
        {
            aspectRatio = yamlNode["video"]["aspectRatio"].as<float>(16.0f / 9.0f);
            DebuggerPrintf("直接yaml-cpp加载宽高比: %f\n", aspectRatio);
        }
        else
        {
            // 如果没有显式的宽高比，从分辨率计算
            IntVec2 resolution = ParseResolution(configPath);
            if (resolution.y > 0)
            {
                aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
                DebuggerPrintf("从分辨率计算宽高比: %f\n", aspectRatio);
            }
        }
    }
    catch (const YAML::Exception& e)
    {
        DebuggerPrintf("Error parsing aspect ratio from %s: %s\n", configPath.c_str(), e.what());
        
        // 如果解析失败，尝试从分辨率计算
        IntVec2 resolution = ParseResolution(configPath);
        if (resolution.y > 0)
        {
            aspectRatio = static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
            DebuggerPrintf("Fallback: 从分辨率计算宽高比: %f\n", aspectRatio);
        }
    }
    
    return aspectRatio;
}

bool WindowConfigParser::ValidateConfig(const WindowConfig& config)
{
    // 验证分辨率
    if (config.m_resolution.x < 640 || config.m_resolution.x > 7680 ||
        config.m_resolution.y < 480 || config.m_resolution.y > 4320)
    {
        return false;
    }
    
    // 验证宽高比
    if (config.m_aspectRatio <= 0.0f || config.m_aspectRatio > 10.0f)
    {
        return false;
    }
    
    // 验证窗口标题
    if (config.m_windowTitle.empty())
    {
        return false;
    }
    
    return true;
}