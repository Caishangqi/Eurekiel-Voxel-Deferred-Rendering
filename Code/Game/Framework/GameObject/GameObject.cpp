#include "GameObject.hpp"

#include "Engine/Core/EngineCommon.hpp"


GameObject::GameObject(Game* parent)
{
    UNUSED(parent);
}

GameObject::~GameObject()
{
}

void GameObject::Update(float deltaSeconds)
{
    UNUSED(deltaSeconds)
}

Mat44 GameObject::GetModelToWorldTransform() const
{
    // [FIXED] Correct transformation order: Scale -> Rotation -> Translation
    Mat44 result = Mat44::MakeNonUniformScale3D(m_scale);
    result.Append(m_orientation.GetAsMatrix_IFwd_JLeft_KUp());
    result.Append(Mat44::MakeTranslation3D(m_position));
    return result;
}
