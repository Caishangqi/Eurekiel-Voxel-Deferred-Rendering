#pragma once
#include <filesystem>

struct GraphicConfig
{
};


class GraphicConfigParser
{
public:
    GraphicConfigParser(std::filesystem::path configFilePath);
};
