#pragma once
#include <string>
#include <unordered_map>
#include "Engine/Window/Window.hpp"
#include "Engine/Math/IntVec2.hpp"

class WindowConfigParser
{
public:
    static WindowConfig LoadFromYaml(const std::string& yamlPath);
    static WindowMode ParseWindowMode(const std::string& modeString);
    static IntVec2 ParseResolution(const std::string& configPath);
    static float ParseAspectRatio(const std::string& configPath);
    static bool ValidateConfig(const WindowConfig& config);
    
private:
    static const std::unordered_map<std::string, WindowMode> s_windowModeMap;
};