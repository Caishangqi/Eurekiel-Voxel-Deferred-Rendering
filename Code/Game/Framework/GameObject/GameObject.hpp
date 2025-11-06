#pragma once

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/EulerAngles.hpp"

class Game;

class GameObject
{
public:
    GameObject(Game* parent);
    virtual ~GameObject();

    virtual void Update(float deltaSeconds);
    virtual void Render() const = 0;

    virtual Mat44 GetModelToWorldTransform() const;

    std::shared_ptr<Game> m_parent = nullptr;
    Vec3                  m_position;
    Vec3                  m_velocity;
    EulerAngles           m_orientation;
    Vec3                  m_scale = Vec3(1, 1, 1);
    EulerAngles           m_angularVelocity;

    Rgba8 m_color = Rgba8::WHITE;
};
