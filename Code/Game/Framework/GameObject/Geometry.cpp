#include "Geometry.hpp"

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"

Geometry::Geometry(Game* parent) : GameObject(parent)
{
}

Geometry::~Geometry()
{
}

void Geometry::Update(float deltaSeconds)
{
    GameObject::Update(deltaSeconds);
}

void Geometry::Render() const
{
    // TODO: update model to world buffer data through uniform manager
    // g_theRenderer->SetModelConstants(GetModelToWorldTransform(), m_color);

    // We Draw the vertex array
    g_theRendererSubsystem->DrawVertexArray(m_vertices);
}

Mat44 Geometry::GetModelToWorldTransform() const
{
    return GameObject::GetModelToWorldTransform();
}

Geometry* Geometry::SetVertices(std::vector<Vertex>&& vertices)
{
    m_vertices = std::move(vertices);
    return this;
}

Geometry* Geometry::SetIndices(std::vector<Index>&& indices)
{
    m_indices = std::move(indices);
    return this;
}
