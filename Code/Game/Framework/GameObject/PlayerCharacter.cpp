#include "PlayerCharacter.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
using namespace enigma::graphic;

PlayerCharacter::PlayerCharacter(Game* parent) : GameObject(parent)
{
    // [FIX] 创建正确的相机配置
    CameraCreateInfo cameraInfo;
    cameraInfo.mode = CameraMode::Perspective;

    // [FIX] 设置初始位置（与旧API Player.cpp一致）
    cameraInfo.position = Vec3(0.0f, 0.0f, 0.0f);

    // [FIX] Set CameraToRenderTransform (game coordinate system → DirectX coordinate system)
    // Old API: ndcMatrix.SetIJK3D(Vec3(0,0,1), Vec3(-1,0,0), Vec3(0,1,0))
    Mat44 cameraToRender;
    cameraToRender.SetIJK3D(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0));
    cameraInfo.cameraToRenderTransform = cameraToRender;

    // [FIX] 使用配置创建相机
    m_camera = std::make_unique<EnigmaCamera>(cameraInfo);
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
