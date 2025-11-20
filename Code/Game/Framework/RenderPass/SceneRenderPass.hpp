#pragma once

class SceneRenderPass
{
public:
    virtual      ~SceneRenderPass() = default;
    virtual void Execute() = 0;

protected:
    virtual void BeginPass() = 0;
    virtual void EndPass() = 0;
};
