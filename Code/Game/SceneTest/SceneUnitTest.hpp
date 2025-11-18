#pragma once
#include "SceneRenderContextProvider.hpp"

class SceneUnitTest : public SceneRenderContextProvider
{
public:
    virtual void        Render() = 0;
    virtual void        Update() = 0;
    SceneRenderContext& GetSceneRenderContext() override;

private:
    SceneRenderContext m_context;
};
