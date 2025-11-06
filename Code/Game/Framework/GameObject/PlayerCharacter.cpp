#include "PlayerCharacter.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
using namespace enigma::graphic;

PlayerCharacter::PlayerCharacter(Game* parent) : GameObject(parent)
{
    CameraCreateInfo cameraInfo;
    cameraInfo.mode = CameraMode::Perspective;
    m_camera        = std::make_unique<EnigmaCamera>();
}

PlayerCharacter::~PlayerCharacter()
{
}

void PlayerCharacter::Update(float deltaSeconds)
{
    GameObject::Update(deltaSeconds);
    m_camera->SetOrientation(m_orientation);
    m_camera->SetPosition(m_position);
}

void PlayerCharacter::Render() const
{
    g_theRendererSubsystem->BeginCamera(*m_camera.get());
    g_theRendererSubsystem->EndCamera(*m_camera.get());
}

Mat44 PlayerCharacter::GetModelToWorldTransform() const
{
    return GameObject::GetModelToWorldTransform();
}
