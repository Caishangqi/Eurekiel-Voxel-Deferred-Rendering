#include "Geometry.hpp"

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"

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
    // [NEW] Upload modelMatrix to UniformManager
    // Calculate modelMatrix (model to world space transformation)
    Mat44 modelMatrix        = GetModelToWorldTransform();
    Mat44 modelMatrixInverse = modelMatrix.GetInverse();

    //Upload to UniformManager
    g_theRendererSubsystem->GetUniformManager()->UniformMat4("modelMatrix", modelMatrix);
    g_theRendererSubsystem->GetUniformManager()->UniformMat4("modelMatrixInverse", modelMatrixInverse);

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
