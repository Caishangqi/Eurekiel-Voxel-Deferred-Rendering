#include "Geometry.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
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
    // 1. Calculate the Model matrix
    Mat44 modelMatrix        = GetModelToWorldTransform();
    Mat44 modelMatrixInverse = modelMatrix.GetInverse();

    // 2. [NEW] Calculate normalMatrix = transpose(inverse(modelMatrix))
    Mat44 normalMatrix = modelMatrixInverse.GetTranspose();

    // 3. Upload to UniformManager
    g_theRendererSubsystem->GetUniformManager()->UniformMat4("modelMatrix", modelMatrix);
    g_theRendererSubsystem->GetUniformManager()->UniformMat4("modelMatrixInverse", modelMatrixInverse);
    g_theRendererSubsystem->GetUniformManager()->UniformMat4("normalMatrix", normalMatrix); // [NEW]

    // [DIAGNOSTIC] 打印模型矩阵和变换信息来诊断顶点输出为零的问题
    LogInfo(LogRenderer, "[DIAG] Geometry Transform - Position: (%.2f, %.2f, %.2f), Orientation: (Y:%.1f, P:%.1f, R:%.1f), Scale: (%.2f, %.2f, %.2f)\n",
            m_position.x, m_position.y, m_position.z,
            m_orientation.m_yawDegrees, m_orientation.m_pitchDegrees, m_orientation.m_rollDegrees,
            m_scale.x, m_scale.y, m_scale.z);
    LogInfo(LogRenderer, "[DIAG] modelMatrix: [%.2f, %.2f, %.2f, %.2f] / [%.2f, %.2f, %.2f, %.2f] / [%.2f, %.2f, %.2f, %.2f] / [%.2f, %.2f, %.2f, %.2f]\n",
            modelMatrix.Ix, modelMatrix.Iy, modelMatrix.Iz, modelMatrix.Iw,
            modelMatrix.Jx, modelMatrix.Jy, modelMatrix.Jz, modelMatrix.Jw,
            modelMatrix.Kx, modelMatrix.Ky, modelMatrix.Kz, modelMatrix.Kw,
            modelMatrix.Tx, modelMatrix.Ty, modelMatrix.Tz, modelMatrix.Tw);

    g_theRendererSubsystem->GetUniformManager()->SyncToGPU();

    // 4. Draw
    g_theRendererSubsystem->DrawVertexArray(m_vertices, m_indices);
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
