#pragma once
#include <vector>
#include <memory>
#include <cstdint>

class Image;

// [NEW] 云纹理数据类
// 职责：存储从clouds.png加载的纹理数据和预计算的面可见性掩码
// 参考：Sodium CloudRenderer.java Line 557-665
class CloudTextureData
{
public:
    // [NEW] 嵌套类：纹理切片（支持环绕采样）
    // 参考：Sodium CloudRenderer.java Line 667-693
    class Slice
    {
    public:
        Slice(int width, int height, int radius);

        // [NEW] 获取格子索引（2D坐标转1D索引）
        int GetCellIndex(int x, int z) const;

        // [NEW] 获取格子的可见面掩码（6位）
        int GetCellFaces(int index) const;

        // [NEW] 获取格子颜色（ARGB格式）
        uint32_t GetCellColor(int index) const;

    private:
        friend class CloudTextureData;

        int                   m_width;
        int                   m_height;
        int                   m_radius;
        std::vector<uint8_t>  m_faces; // [NEW] 面掩码数组
        std::vector<uint32_t> m_colors; // [NEW] 颜色数组（ARGB）
    };

    // [NEW] 静态工厂方法：从Image加载256x256纹理
    // 参考：Sodium CloudRenderer.java Line 498-528
    static std::unique_ptr<CloudTextureData> Load(const Image& image);

    // [NEW] 创建切片（环绕采样）
    // 参考：Sodium CloudRenderer.java Line 570-590
    Slice CreateSlice(int originX, int originZ, int radius) const;

    // [NEW] 获取纹理尺寸
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    CloudTextureData(int width, int height);

    // [NEW] 加载纹理数据并预计算面掩码
    bool LoadTextureData(const Image& texture);

    // [NEW] 预计算4邻居面可见性
    // 参考：Sodium CloudRenderer.java Line 620-643
    static int GetOpenFaces(const Image& image, uint32_t color, int x, int z);

    // [NEW] 获取邻居像素颜色（环绕采样）
    static uint32_t GetNeighborTexel(const Image& image, int x, int z);

    // [NEW] 检查像素是否透明（alpha < 10）
    static bool IsTransparent(uint32_t argb);

    // [NEW] 计算1D索引
    static int GetCellIndex(int x, int z, int width);

    int                   m_width;
    int                   m_height;
    std::vector<uint8_t>  m_faces; // [NEW] 每像素可见面掩码（6位）
    std::vector<uint32_t> m_colors; // [NEW] 每像素颜色（ARGB）
};

// [NEW] 面掩码常量定义
// 参考：Sodium CloudRenderer.java Line 47-58（已映射坐标系）
constexpr int FACE_MASK_NEG_Z = 1; // [NEW] 底面（Minecraft NEG_Y）
constexpr int FACE_MASK_POS_Z = 2; // [NEW] 顶面（Minecraft POS_Y）
constexpr int FACE_MASK_NEG_Y = 4; // [NEW] 左面（Minecraft NEG_X）
constexpr int FACE_MASK_POS_Y = 8; // [NEW] 右面（Minecraft POS_X）
constexpr int FACE_MASK_NEG_X = 16; // [NEW] 后面（Minecraft NEG_Z）
constexpr int FACE_MASK_POS_X = 32; // [NEW] 前面（Minecraft POS_Z）
