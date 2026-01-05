#pragma once
#include "GameObject.hpp"
#include "Engine/Graphic/Camera/PerspectiveCamera.hpp"

class PlayerCharacter : public GameObject
{
public:
    explicit PlayerCharacter(Game* parent);
    ~PlayerCharacter() override;
    void  Update(float deltaSeconds) override;
    void  Render() const override;
    Mat44 GetModelToWorldTransform() const override;

    // [REFACTOR] Return PerspectiveCamera* for full access to camera methods
    // SkyRenderPass needs GetPosition(), GetViewMatrix(), UpdateMatrixUniforms()
    enigma::graphic::PerspectiveCamera* GetCamera() const;

private:
    void HandleInputAction(float deltaSeconds);
    void UpdateCamera(float deltaSeconds);

private:
    // [REFACTOR] Use PerspectiveCamera instead of deprecated EnigmaCamera
    std::unique_ptr<enigma::graphic::PerspectiveCamera> m_camera = nullptr;
};
