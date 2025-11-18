#include "Geometry.hpp"

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Shader/Uniform/UniformManager.hpp"
#include "Engine/Graphic/Shader/Uniform/PerObjectUniforms.hpp" // [NEW] PerObjectUniforms结构体

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
    // ==================== [REFACTORED] 使用新的Uniform架构 ====================
    //
    // 架构说明:
    // - BeginCamera已经上传了Camera相关矩阵到MatricesUniforms (slot 7)
    // - Geometry::Render负责上传Per-Object数据到PerObjectUniforms (slot 1)
    // - 两个Uniform Buffer互不干扰，避免相互覆盖
    // - DrawVertexArray内部会自动调用SyncToGPU，无需手动调用
    //
    // ==================== Step 1: 计算物体变换矩阵 ====================

    Mat44 modelMatrix        = GetModelToWorldTransform(); // Model → World
    Mat44 modelMatrixInverse = modelMatrix.GetInverse(); // World → Model

    // ==================== Step 2: 填充PerObjectUniforms ====================

    enigma::graphic::PerObjectUniforms perObjData;

    // 物体变换矩阵（每个物体独立）
    perObjData.modelMatrix        = modelMatrix;
    perObjData.modelMatrixInverse = modelMatrixInverse;

    // 物体颜色（从Geometry成员变量获取）
    m_color.GetAsFloats(perObjData.modelColor); // Geometry.hpp中的m_color成员

    // ==================== Step 3: 上传PerObjectUniforms到GPU ====================
    //
    // 注意:
    // - PerObjectUniforms使用slot 1（b1寄存器）
    // - MatricesUniforms使用slot 7（b7寄存器），由BeginCamera管理
    // - 两者互不干扰，不会相互覆盖
    //
    g_theRendererSubsystem->GetUniformManager()->UploadBuffer<enigma::graphic::PerObjectUniforms>(perObjData);

    // ==================== Step 4: 绘制 ====================
    //
    // DrawVertexArray内部会自动:
    // 1. 调用SyncToGPU()同步所有Uniform数据
    // 2. 绑定Root Constants
    // 3. 执行DrawIndexedInstanced
    //
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
