#pragma once
#include "GameObject.hpp"
#include "Engine/Graphic/Camera/EnigmaCamera.hpp"

class PlayerCharacter : public GameObject
{
public:
    explicit PlayerCharacter(Game* parent);
    ~PlayerCharacter() override;
    void                           Update(float deltaSeconds) override;
    void                           Render() const override;
    Mat44                          GetModelToWorldTransform() const override;
    enigma::graphic::EnigmaCamera* GetCamera() const;

private:
    void HandleInputAction(float deltaSeconds);
    void UpdateCamera(float deltaSeconds);

private:
    std::unique_ptr<enigma::graphic::EnigmaCamera> m_camera = nullptr;
};
