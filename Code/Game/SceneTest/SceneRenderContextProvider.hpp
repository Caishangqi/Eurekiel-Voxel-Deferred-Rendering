#pragma once
#include <map>
#include <memory>
#include <string>
#include "Engine/Graphic/Shader/Program/ShaderProgram.hpp"

class SceneRenderContextProvider
{
public:
    struct SceneRenderContext
    {
        std::map<std::string, std::shared_ptr<enigma::graphic::ShaderProgram>> includeShaderPrograms;
    };

public:
    virtual SceneRenderContext& GetSceneRenderContext() = 0;
    virtual                     ~SceneRenderContextProvider() = default;
};
