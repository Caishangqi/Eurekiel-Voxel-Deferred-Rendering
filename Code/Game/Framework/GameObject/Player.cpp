#include "Player.hpp"

#include "../../GameCommon.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Window/Window.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/GUISubsystem.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/gui/GUICrosser.hpp"

bool Player::Event_Player_Join_World(EventArgs& args)
{
    UNUSED(args)
    std::shared_ptr<GUI> gui = g_theGUI->GetGUI(std::type_index(typeid(GUICrosser)));
    if (!gui)
    {
        g_theGUI->AddToViewPort(std::make_shared<GUICrosser>(g_theGame->m_player));
    }
    return false;
}

bool Player::Event_Player_Quit_World(EventArgs& args)
{
    UNUSED(args)
    std::shared_ptr<GUI> gui = g_theGUI->GetGUI(std::type_index(typeid(GUICrosser)));
    if (gui)
        g_theGUI->RemoveFromViewPort(gui);
    return false;
}

Player::Player(Game* owner) : Entity(owner)
{
    m_camera         = new Camera();
    m_camera->m_mode = eMode_Perspective;
    m_camera->SetOrthographicView(Vec2(-1, -1), Vec2(1, 1));

    // Event Subscribe
    g_theEventSystem->SubscribeEventCallbackFunction("Event.PlayerJoinWorld", Event_Player_Join_World);
    g_theEventSystem->SubscribeEventCallbackFunction("Event.PlayerQuitWorld", Event_Player_Quit_World);
}

Player::~Player()
{
    POINTER_SAFE_DELETE(m_camera)
}

void Player::Update(float deltaSeconds)
{
    Vec2 cursorDelta = g_theInput->GetCursorClientDelta();
    //printf(Stringf("%f %f \n", cursorDelta.x, cursorDelta.y).c_str());


    m_orientation.m_yawDegrees += -cursorDelta.x * 0.125f;
    m_orientation.m_pitchDegrees += -cursorDelta.y * 0.125f;

    const XboxController& controller = g_theInput->GetController(0);
    float                 speed      = 4.0f;

    Vec2  leftStickPos  = controller.GetLeftStick().GetPosition();
    Vec2  rightStickPos = controller.GetRightStick().GetPosition();
    float leftStickMag  = controller.GetLeftStick().GetMagnitude();
    float rightStickMag = controller.GetRightStick().GetMagnitude();
    float leftTrigger   = controller.GetLeftTrigger();
    float rightTrigger  = controller.GetRightTrigger();

    if (rightStickMag > 0.f)
    {
        m_orientation.m_yawDegrees += -(rightStickPos * speed * rightStickMag * 0.125f).x;
        m_orientation.m_pitchDegrees += -(rightStickPos * speed * rightStickMag * 0.125f).y;
    }

    if (g_theInput->IsKeyDown(KEYCODE_LEFT_SHIFT) || controller.IsButtonDown(XBOX_BUTTON_A))
    {
        speed *= 20.f;
    }

    m_orientation.m_rollDegrees += leftTrigger * 0.125f * deltaSeconds * speed;
    m_orientation.m_rollDegrees -= rightTrigger * 0.125f * deltaSeconds * speed;

    //m_orientation.m_yawDegrees   = GetClamped(m_orientation.m_yawDegrees, -85.f, 85.f);
    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);
    m_orientation.m_rollDegrees  = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);


    Vec3 forward, left, up;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    //printf("x: %f, y: %f\n", leftStickPos.x, leftStickPos.y);

    m_position += (leftStickPos * speed * leftStickMag * deltaSeconds).y * forward;
    m_position += -(leftStickPos * speed * leftStickMag * deltaSeconds).x * left;

    if (g_theInput->IsKeyDown('W'))
    {
        m_position += forward * speed * deltaSeconds;
    }

    if (g_theInput->IsKeyDown('S'))
    {
        m_position -= forward * speed * deltaSeconds;
    }

    if (g_theInput->IsKeyDown('A'))
    {
        m_position += left * speed * deltaSeconds;
    }

    if (g_theInput->IsKeyDown('D'))
    {
        m_position -= left * speed * deltaSeconds;
    }

    if (g_theInput->IsKeyDown('Q') || controller.IsButtonDown(XBOX_BUTTON_RS))
    {
        m_position.z -= deltaSeconds * speed;
    }

    if (g_theInput->IsKeyDown('E') || controller.IsButtonDown(XBOX_BUTTON_LS))
    {
        m_position.z += deltaSeconds * speed;
    }
    m_camera->SetPerspectiveView(g_theWindow->GetClientAspectRatio(), 60.f, 0.1f, 400.f);
    Mat44 ndcMatrix;
    ndcMatrix.SetIJK3D(Vec3(0, 0, 1), Vec3(-1, 0, 0), Vec3(0, 1, 0));

    m_camera->SetPosition(m_position);
    m_camera->SetOrientation(m_orientation);


    m_camera->SetCameraToRenderTransform(ndcMatrix);

    Entity::Update(deltaSeconds);
}

void Player::Render() const
{
    g_theRenderer->BeginCamera(*m_camera);
    g_theRenderer->EndCamera(*m_camera);
}
