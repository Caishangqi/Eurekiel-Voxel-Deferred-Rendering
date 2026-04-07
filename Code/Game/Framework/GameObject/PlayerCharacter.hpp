#pragma once
#include "GameObject.hpp"
#include "Game/Framework/Camera/PlayerCameraRig.hpp"

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
    enigma::graphic::PerspectiveCamera* GetRenderCamera() const;
    enigma::graphic::PerspectiveCamera* GetDebugCamera() const;
    void                                SyncDebugCameraToGameplayCamera();

private:
    void HandleInputAction(float deltaSeconds);
    void UpdateCamera(float deltaSeconds);
    void UpdatePlayerStatus(float deltaSeconds);
    void ApplyFreeCameraInput(Vec3& position, EulerAngles& orientation, float deltaSeconds) const;

private:
    std::unique_ptr<PlayerCameraRig> m_cameraRig = nullptr;
};
