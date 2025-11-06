#include "GameObject.hpp"

#include "Engine/Core/EngineCommon.hpp"


GameObject::GameObject(Game* parent)
{
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
    // Mat44::MakeNonUniformScale3D(m_scale);
    Mat44 matTranslation = Mat44::MakeTranslation3D(m_position);
    matTranslation.Append(m_orientation.GetAsMatrix_IFwd_JLeft_KUp());
    matTranslation.Append(Mat44::MakeNonUniformScale3D(m_scale));
    return matTranslation;
}
