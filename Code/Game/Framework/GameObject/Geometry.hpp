#pragma once
#include <vector>

#include "GameObject.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp"
#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"

class Geometry : public GameObject
{
public:
    explicit Geometry(Game* parent);
    ~Geometry() override;
    void  Update(float deltaSeconds) override;
    void  Render() const override;
    Mat44 GetModelToWorldTransform() const override;

#pragma region RENDER

private:
    std::vector<Vertex> m_vertices;
    std::vector<Index>  m_indices;

    std::unique_ptr<enigma::graphic::D12VertexBuffer> m_vertexBuffer;
    std::unique_ptr<enigma::graphic::D12IndexBuffer>  m_indexBuffer;

public:
    Geometry* SetVertices(std::vector<Vertex>&& vertices);
    Geometry* SetIndices(std::vector<Index>&& indices);
    
    Rgba8 m_color = Rgba8::WHITE;
#pragma endregion

private:
};
