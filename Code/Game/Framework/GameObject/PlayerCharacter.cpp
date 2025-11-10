#include "PlayerCharacter.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/GameCommon.hpp"
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
    HandleInputAction(deltaSeconds);
    UpdateCamera(deltaSeconds);
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

void PlayerCharacter::HandleInputAction(float deltaSeconds)
{
    g_theInput->SetCursorMode(CursorMode::FPS);
    Vec2 cursorDelta = g_theInput->GetCursorClientDelta();
    m_orientation.m_yawDegrees += -cursorDelta.x * 0.125f;
    m_orientation.m_pitchDegrees += -cursorDelta.y * 0.125f;
    float speed = 2.0f;
    if (g_theInput->IsKeyDown(KEYCODE_LEFT_SHIFT))speed *= 10.f;
    if (g_theInput->IsKeyDown('Q'))m_orientation.m_rollDegrees += 0.125f;

    if (g_theInput->IsKeyDown('E'))m_orientation.m_rollDegrees -= 0.125f;

    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
    m_orientation.m_rollDegrees  = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

    Vec3 forward, left, up;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    if (g_theInput->IsKeyDown('W'))m_position += forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('S'))m_position -= forward * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('A')) m_position += left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('D'))m_position -= left * speed * deltaSeconds;

    if (g_theInput->IsKeyDown('Z'))m_position.z -= deltaSeconds * speed;

    if (g_theInput->IsKeyDown('C'))m_position.z += deltaSeconds * speed;
}

void PlayerCharacter::UpdateCamera(float deltaSeconds)
{
    UNUSED(deltaSeconds)
    m_camera->SetOrientation(m_orientation);
    m_camera->SetPosition(m_position);
}
