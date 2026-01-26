#pragma once

#include <memory>

#include "Engine/Core/Event/MulticastDelegate.hpp"

#include <string>

namespace enigma::graphic
{
    class ShaderBundle;
    class ShaderProgram;
}

class SceneRenderPass
{
public:
    SceneRenderPass();
    virtual ~SceneRenderPass();

    virtual void Execute() = 0;

protected:
    virtual void BeginPass() = 0;
    virtual void EndPass() = 0;

    // ShaderBundle event callbacks - override in subclasses
    virtual void OnShaderBundleLoaded(enigma::graphic::ShaderBundle* newBundle);
    virtual void OnShaderBundleUnloaded();

    // Helper: get program from current active bundle
    std::shared_ptr<enigma::graphic::ShaderProgram> GetProgramFromCurrentBundle(const std::string& programName);

private:
    // Event subscription handles
    enigma::event::DelegateHandle m_loadedHandle   = 0;
    enigma::event::DelegateHandle m_unloadedHandle = 0;

    void SubscribeToShaderBundleEvents();
    void UnsubscribeFromShaderBundleEvents();
};
