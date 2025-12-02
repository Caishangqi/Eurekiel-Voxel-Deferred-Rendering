#include "CloudTextureData.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <algorithm>
#include <cmath>

// [NEW] 辅助函数：将Rgba8转换为ARGB uint32_t
static uint32_t PackARGB(const Rgba8& color)
{
    return (static_cast<uint32_t>(color.a) << 24) |
        (static_cast<uint32_t>(color.r) << 16) |
        (static_cast<uint32_t>(color.g) << 8) |
        static_cast<uint32_t>(color.b);
}

// [NEW] 辅助函数：环绕取模（支持负数）
static int FloorMod(int a, int b)
{
    int r = a % b;
    return (r < 0) ? (r + b) : r;
}

//--------------------------------------------------------------------------------------------------
// CloudTextureData 实现
//--------------------------------------------------------------------------------------------------

CloudTextureData::CloudTextureData(int width, int height)
    : m_width(width)
      , m_height(height)
{
    // [NEW] 预分配存储空间
    int totalPixels = width * height;
    m_faces.resize(totalPixels, 0);
    m_colors.resize(totalPixels, 0);
}

std::unique_ptr<CloudTextureData> CloudTextureData::Load(const Image& image)
{
    // [NEW] 验证纹理尺寸（必须是256x256）
    IntVec2 dimensions = image.GetDimensions();
    if (dimensions.x != 256 || dimensions.y != 256)
    {
        return nullptr;
    }

    // [NEW] 创建CloudTextureData实例
    auto data = std::unique_ptr<CloudTextureData>(new CloudTextureData(dimensions.x, dimensions.y));

    // [NEW] 加载纹理数据
    if (!data->LoadTextureData(image))
    {
        return nullptr;
    }

    return data;
}

bool CloudTextureData::LoadTextureData(const Image& texture)
{
    // 遍历每个像素，参考：Sodium CloudRenderer.java Line 498-528
    int opaqueCount = 0;

    for (int z = 0; z < m_height; ++z)
    {
        for (int x = 0; x < m_width; ++x)
        {
            Rgba8 color = texture.GetTexelColor(IntVec2(x, z));

            // 支持两种PNG格式的透明度检测：
            // 1. RGBA PNG: alpha < 10 表示透明
            // 2. 灰度/索引色 PNG: 黑色(R<10)表示透明，白色表示云
            bool isTransparent = (color.a < 10) ||
                (color.a == 255 && color.r < 10 && color.g < 10 && color.b < 10);

            if (isTransparent)
            {
                continue;
            }

            opaqueCount++;

            // 强制白色云（忽略PNG边缘可能的垃圾RGB数据）
            uint32_t argb = PackARGB(Rgba8(255, 255, 255, 255));

            int index       = GetCellIndex(x, z, m_width);
            m_colors[index] = argb;
            m_faces[index]  = static_cast<uint8_t>(GetOpenFaces(texture, argb, x, z));
        }
    }

    return opaqueCount > 0;
}

CloudTextureData::Slice CloudTextureData::CreateSlice(int originX, int originZ, int radius) const
{
    // [NEW] 创建切片（环绕采样）
    // 参考：Sodium CloudRenderer.java Line 570-590
    int sliceWidth  = 2 * radius + 1;
    int sliceHeight = 2 * radius + 1;

    Slice slice(sliceWidth, sliceHeight, radius);

    // [NEW] 逐行复制（处理环绕）
    for (int dstZ = 0; dstZ < sliceHeight; ++dstZ)
    {
        // [NEW] 源纹理Z坐标（环绕）
        int srcZ = FloorMod(originZ - radius + dstZ, m_height);

        // [NEW] 逐列复制（处理X轴环绕）
        int srcX = FloorMod(originX - radius, m_width);
        for (int dstX = 0; dstX < sliceWidth;)
        {
            // [NEW] 计算本次复制长度（避免跨边界）
            int length = std::min(m_width - srcX, sliceWidth - dstX);

            // [NEW] 计算源和目标索引
            int srcPos = GetCellIndex(srcX, srcZ, m_width);
            int dstPos = GetCellIndex(dstX, dstZ, sliceWidth);

            // [NEW] 复制faces[]和colors[]
            std::copy_n(m_faces.begin() + srcPos, length, slice.m_faces.begin() + dstPos);
            std::copy_n(m_colors.begin() + srcPos, length, slice.m_colors.begin() + dstPos);

            // [NEW] 更新位置
            dstX += length;
            srcX = 0; // [NEW] 换行后从头开始
        }
    }

    return slice;
}

int CloudTextureData::GetOpenFaces(const Image& image, uint32_t color, int x, int z)
{
    // [NEW] 预计算4邻居面可见性
    // 参考：Sodium CloudRenderer.java Line 620-643（已映射坐标系）
    // [FIX] 恢复Sodium原始逻辑：比较颜色而非检查透明度
    // 当邻居颜色与自己不同时（包括邻居透明），渲染该面

    // [NEW] 顶面和底面始终可见
    int faces = FACE_MASK_NEG_Z | FACE_MASK_POS_Z;

    // [FIX] 检查4个水平邻居颜色是否与自己不同（已映射坐标系）
    uint32_t neighbor;

    // [NEW] 左邻居（Minecraft West -> Our -Y）
    neighbor = GetNeighborTexel(image, x - 1, z);
    if (color != neighbor)
    {
        faces |= FACE_MASK_NEG_Y;
    }

    // [NEW] 右邻居（Minecraft East -> Our +Y）
    neighbor = GetNeighborTexel(image, x + 1, z);
    if (color != neighbor)
    {
        faces |= FACE_MASK_POS_Y;
    }

    // [NEW] 后邻居（Minecraft North -> Our -X）
    neighbor = GetNeighborTexel(image, x, z - 1);
    if (color != neighbor)
    {
        faces |= FACE_MASK_NEG_X;
    }

    // [NEW] 前邻居（Minecraft South -> Our +X）
    neighbor = GetNeighborTexel(image, x, z + 1);
    if (color != neighbor)
    {
        faces |= FACE_MASK_POS_X;
    }

    return faces;
}

uint32_t CloudTextureData::GetNeighborTexel(const Image& image, int x, int z)
{
    // [NEW] 获取邻居像素颜色（环绕采样）
    IntVec2 dimensions = image.GetDimensions();

    // [NEW] 环绕坐标
    int wrappedX = FloorMod(x, dimensions.x);
    int wrappedZ = FloorMod(z, dimensions.y);

    // [NEW] 获取颜色
    Rgba8 color = image.GetTexelColor(IntVec2(wrappedX, wrappedZ));

    // [FIX] 使用与 LoadTextureData 相同的透明度检测逻辑
    // 支持两种PNG格式：
    // 1. RGBA PNG: alpha < 10 表示透明
    // 2. 灰度/索引色 PNG: 黑色(R<10)表示透明
    bool isTransparent = false;
    if (color.a < 10)
    {
        isTransparent = true;
    }
    else if (color.a == 255 && color.r < 10 && color.g < 10 && color.b < 10)
    {
        isTransparent = true;
    }

    // [FIX] 透明像素返回0（与m_colors[]默认值一致）
    // 不透明像素返回0xFFFFFFFF（与LoadTextureData存储的颜色一致）
    // 这样 color != neighbor 比较才能正确工作
    if (isTransparent)
    {
        return 0; // 透明：返回0
    }
    else
    {
        return PackARGB(Rgba8(255, 255, 255, 255)); // 不透明：白色
    }
}

bool CloudTextureData::IsTransparent(uint32_t argb)
{
    // [NEW] 检查像素是否透明（alpha < 10）
    // 参考：Sodium CloudRenderer.java Line 505
    uint8_t alpha = (argb >> 24) & 0xFF;
    return alpha < 10;
}

int CloudTextureData::GetCellIndex(int x, int z, int width)
{
    // [NEW] 计算1D索引
    return z * width + x;
}

//--------------------------------------------------------------------------------------------------
// CloudTextureData::Slice 实现
//--------------------------------------------------------------------------------------------------

CloudTextureData::Slice::Slice(int width, int height, int radius)
    : m_width(width)
      , m_height(height)
      , m_radius(radius)
{
    // [NEW] 预分配存储空间
    int totalPixels = width * height;
    m_faces.resize(totalPixels, 0);
    m_colors.resize(totalPixels, 0);
}

int CloudTextureData::Slice::GetCellIndex(int x, int z) const
{
    // [NEW] 计算偏移后的索引
    // 将相对坐标（-radius到+radius）转换为数组索引（0到2*radius）
    int arrayX = x + m_radius;
    int arrayZ = z + m_radius;
    return CloudTextureData::GetCellIndex(arrayX, arrayZ, m_width);
}

int CloudTextureData::Slice::GetCellFaces(int index) const
{
    // [NEW] 返回面掩码
    return m_faces[index];
}

uint32_t CloudTextureData::Slice::GetCellColor(int index) const
{
    // [NEW] 返回颜色
    return m_colors[index];
}
