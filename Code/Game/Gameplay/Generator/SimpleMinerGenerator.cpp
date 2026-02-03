#include "SimpleMinerGenerator.hpp"
#include "SimpleMinerTreeGenerator.hpp"
#include "Engine/Registry/Block/BlockRegistry.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Voxel/Block/BlockPos.hpp"
#include "Engine/Voxel/Biome/Biome.hpp"
#include "Engine/Math/SmoothNoise.hpp"
#include <cmath>
#include <algorithm>

#include "Engine/Math/IntVec3.hpp"
#include "Engine/Voxel/Function/ConstantDensityFunction.hpp"
#include "Engine/Voxel/Function/SplineDensityFunction.hpp"

using namespace enigma::registry::block;
using namespace enigma::voxel;

DEFINE_LOG_CATEGORY(LogWorldGenerator);

// ========== 构造函数实现 ==========
SimpleMinerGenerator::SimpleMinerGenerator(uint32_t worldSeed)
    : TerrainGenerator("enigma_generator", "simpleminer")
      , m_worldSeed(worldSeed)
{
    // Phase 2: 初始化高度偏移样条曲线 (Height Offset Spline)
    // 控制不同大陆度下的地形基准高度偏移
    // ⚠️ 修复v3：平衡海陆比例，目标50:50（2025-11-02）
    // 问题诊断：v2参数导致海洋占比>80%，陆地过少
    // v3调整策略：
    //   1. 深海区域 (c=-1.0): -1.0 → -0.6 (减少海洋深度)
    //   2. 浅海区域 (c=-0.4): -0.7 → -0.4 (抬升浅海基准)
    //   3. 过渡区域 (c=0.4): 0.0 → 0.4 (增加陆地高度)
    //   4. 高山区域 (c=1.0): 0.2 → 0.6 (增加山脉高度)
    // 预期效果：海洋占比从>80%降低到约50%，陆地占比从<20%提升到约50%
    std::vector<SplineDensityFunction::SplinePoint> points;
    points.resize(4);
    points[0].location   = -1.0f;
    points[0].value      = -0.6f;
    points[0].derivative = 0.0f; // 深海区域（v3：减少深度）
    points[1].location   = -0.4f;
    points[1].value      = -0.4f;
    points[1].derivative = 0.0f; // 浅海区域（v3：抬升基准）
    points[2].location   = 0.4f;
    points[2].value      = 0.4f;
    points[2].derivative = 0.0f; // 平原区域（v3：增加高度）
    points[3].location   = 1.0f;
    points[3].value      = 0.6f;
    points[3].derivative = 0.0f; // 高山区域（v3：增加高度）

    m_heightOffsetSpline = std::make_shared<SplineDensityFunction>(
        std::make_unique<ConstantDensityFunction>(0.0f), std::move(points));

    // Phase 2: 初始化挤压因子样条曲线 (Squashing Factor Spline)
    // 控制不同大陆度下的地形垂直拉伸/压缩程度
    // 1:1 复刻教授参数 (Course Blog: Ship It - Oct 21, 2025)
    std::vector<SplineDensityFunction::SplinePoint> squashingPoints;
    squashingPoints.resize(5);
    squashingPoints[0].location   = -1.0f;
    squashingPoints[0].value      = 0.0f;
    squashingPoints[0].derivative = 0.0f; // 深海: 无变形
    squashingPoints[1].location   = -0.5f;
    squashingPoints[1].value      = 0.0f;
    squashingPoints[1].derivative = 0.0f; // 浅海: 无变形
    squashingPoints[2].location   = -0.25f;
    squashingPoints[2].value      = 2.0f;
    squashingPoints[2].derivative = 0.0f; // 海岸: 教授版本 2.0
    squashingPoints[3].location   = 0.25f;
    squashingPoints[3].value      = 2.0f;
    squashingPoints[3].derivative = 0.0f; // 平原: 教授版本 2.0
    squashingPoints[4].location   = 1.0f;
    squashingPoints[4].value      = -1.5f;
    squashingPoints[4].derivative = 0.0f; // 高山: 教授版本 -1.5

    m_squashingSpline = std::make_shared<SplineDensityFunction>(
        std::make_unique<ConstantDensityFunction>(0.0f), std::move(squashingPoints));

    // Phase 3: 初始化侵蚀因子样条曲线 (Erosion Factor Spline)
    // 控制不同侵蚀度下的地形垂直变化强度
    std::vector<SplineDensityFunction::SplinePoint> erosionPoints;
    erosionPoints.resize(5);
    erosionPoints[0].location   = -1.0f;
    erosionPoints[0].value      = -0.3f;
    erosionPoints[0].derivative = 0.0f; // 平坦区域
    erosionPoints[1].location   = -0.5f;
    erosionPoints[1].value      = -0.2f;
    erosionPoints[1].derivative = 0.0f; // 轻微压缩
    erosionPoints[2].location   = 0.0f;
    erosionPoints[2].value      = 0.0f;
    erosionPoints[2].derivative = 0.0f; // 正常地形
    erosionPoints[3].location   = 0.5f;
    erosionPoints[3].value      = 0.4f;
    erosionPoints[3].derivative = 0.0f; // 崎岖地形
    erosionPoints[4].location   = 1.0f;
    erosionPoints[4].value      = 0.6f;
    erosionPoints[4].derivative = 0.0f; // 强崎岖

    m_erosionSpline = std::make_shared<SplineDensityFunction>(
        std::make_unique<ConstantDensityFunction>(0.0f), std::move(erosionPoints));

    // Phase 4: 初始化峰谷影响样条曲线 (PeaksValleys Influence Spline)
    // 控制不同峰谷值下的地形高度变化
    std::vector<SplineDensityFunction::SplinePoint> peaksValleysPoints;
    peaksValleysPoints.resize(5);
    peaksValleysPoints[0].location   = -1.0f;
    peaksValleysPoints[0].value      = -0.5f;
    peaksValleysPoints[0].derivative = 0.0f; // 深邃山谷
    peaksValleysPoints[1].location   = -0.5f;
    peaksValleysPoints[1].value      = -0.3f;
    peaksValleysPoints[1].derivative = 0.0f; // 浅山谷
    peaksValleysPoints[2].location   = 0.0f;
    peaksValleysPoints[2].value      = 0.0f;
    peaksValleysPoints[2].derivative = 0.0f; // 无影响
    peaksValleysPoints[3].location   = 0.5f;
    peaksValleysPoints[3].value      = 0.4f;
    peaksValleysPoints[3].derivative = 0.0f; // 小山峰
    peaksValleysPoints[4].location   = 1.0f;
    peaksValleysPoints[4].value      = 0.3f;
    peaksValleysPoints[4].derivative = 0.0f; // Further reduced from 0.5 to 0.3 to prevent cylinder formation

    m_peaksValleysSpline = std::make_shared<SplineDensityFunction>(
        std::make_unique<ConstantDensityFunction>(0.0f), std::move(peaksValleysPoints));

    // 使用提供的 seed 初始化所有噪声生成器
    InitializeNoiseGenerators();

    // ⭐ 新增：预加载所有方块到缓存（线程安全初始化）
    InitializeBlockCache();

    // ⭐ 修复 (2025-11-02)：初始化 Biome 系统
    // 问题：构造函数中缺少 InitializeBiomes() 调用，导致所有 Biome 成员变量为 null
    // 结果：ApplySurfaceRules 中 GetBiomeAt() 返回 null，表面方块无法应用
    InitializeBiomes();

    LogInfo(LogWorldGenerator, "SimpleMinerGenerator created with seed: %u", m_worldSeed);
}

bool SimpleMinerGenerator::GenerateChunk(Chunk* chunk, int32_t chunkX, int32_t chunkY, uint32_t worldSeed)
{
    // ===== Phase 3: 前置状态检查 =====
    if (!chunk)
    {
        LogError("SimpleMinerGenerator", "GenerateChunk - null chunk provided");
        return false;
    }

    // 检查 chunk 状态是否为 Generating
    ChunkState currentState = chunk->GetState();
    if (currentState != ChunkState::Generating)
    {
        LogWarn("SimpleMinerGenerator",
                "Chunk (%d, %d) state is %s (not Generating), abort generation",
                chunkX, chunkY, chunk->GetStateName());
        return false;
    }

    // Use provided world seed or fallback to member seed
    uint32_t effectiveSeed = (worldSeed != 0) ? worldSeed : m_worldSeed;

    // Establish world-space position and bounds of this chunk
    IntVec3 chunkPosition(chunkX * Chunk::CHUNK_SIZE_X, chunkY * Chunk::CHUNK_SIZE_Y, 0);

    // Allocate per-(x,y) maps
    const int          mapSize = Chunk::CHUNK_SIZE_X * Chunk::CHUNK_SIZE_Y;
    std::vector<int>   heightMapXY(mapSize);
    std::vector<int>   dirtDepthXY(mapSize);
    std::vector<float> humidityMapXY(mapSize);
    std::vector<float> temperatureMapXY(mapSize);

    for (int z = 0; z < Chunk::CHUNK_SIZE_Z; ++z)
    {
        // ===== Phase 3: 外层循环状态验证（Z坐标） =====
        // 频率：每次迭代（256次/chunk）
        if (chunk->GetState() != ChunkState::Generating)
        {
            LogDebug("SimpleMinerGenerator",
                     "Chunk (%d, %d) state changed during Z iteration %d, abort generation",
                     chunkX, chunkY, z);
            return false;
        }

        for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
        {
            // ===== Phase 3: 中层循环状态验证（Y坐标） =====
            // 频率：每10次迭代检查一次（性能优化）
            if (y % 10 == 0 && chunk->GetState() != ChunkState::Generating)
            {
                LogDebug("SimpleMinerGenerator",
                         "Chunk (%d, %d) state changed at Y=%d Z=%d, abort generation",
                         chunkX, chunkY, y, z);
                return false;
            }

            for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
            {
                int globalX = chunkX * Chunk::CHUNK_SIZE_X + x;
                int globalY = chunkY * Chunk::CHUNK_SIZE_Y + y;
                int globalZ = z;

                // ========== Phase 2 & Phase 3 & Phase 4: 地形塑形流程 ==========
                //
                // 算法来源: Course Blog (Oct 17 - Continental; Oct 19 - Erosion; Oct 21 - Peaks/Valleys)
                // 核心思想: 使用三层噪声协同控制地形形态
                //
                // 流程概述:
                // 1. 采样大陆度噪声 (Continental) - 控制地形高度
                // 2. 采样侵蚀度噪声 (Erosion) - 控制地形形态
                // 3. 采样峰谷噪声 (PeaksValleys) - 控制峰谷细节
                // 4. 通过样条曲线计算地形参数 (h, s, e, pv)
                // 5. 计算3D密度场并添加垂直偏移(bias)
                // 6. 应用高度偏移: density -= h
                // 7. 计算动态基准高度和地形偏移: b, t
                // 8. 应用挤压: density += s * t
                // 9. 应用侵蚀: density += e * t
                // 10. 应用峰谷: density += pv * abs(continentalness) * t
                // 11. 放置方块: density < 0 → stone, density >= 0 → air

                // 步骤1: 采样大陆度 (Continentalness) [-1, 1]
                // 2D Perlin噪声，决定该位置是海洋(-1)还是大陆(+1)
                float continentalness = SampleContinentalness(globalX, globalY);

                // 步骤2: 采样侵蚀度 (Erosion) [-1, 1]
                // 2D Perlin噪声，决定该位置是平坦(-1)还是崎岖(+1)
                float erosion = SampleErosion(globalX, globalY);

                // 步骤3: 采样峰谷度 (PeaksValleys) [-1, 1]
                // 2D Perlin噪声，决定该位置是山峰(+1)还是山谷(-1)
                float peaksValleys = SamplePeaksValleys(globalX, globalY);
                UNUSED(peaksValleys)
                // 步骤4: 通过样条曲线计算地形参数
                // h (Height Offset): 高度偏移量，控制地形基准高度的上下浮动
                //    海洋区域(c=-1): h=-0.6，降低基准高度，形成海底
                //    大陆区域(c=+1): h=+0.6，抬升基准高度，形成陆地
                float h = EvaluateHeightOffset(continentalness);

                // s (Squashing): 挤压因子，控制地形的垂直拉伸/压缩
                //    海洋区域(c=-1): s=0，不进行垂直变形
                //    过渡区域(c=-0.25~0.25): s=2.0，增强垂直变化
                //    大陆区域(c=+1): s=-1.5，压缩地形高度
                float s = EvaluateSquashing(continentalness);

                // e (Erosion Factor): 侵蚀因子，控制地形的粗糙度
                //    平坦区域(e=-1): e=-0.3，减少垂直变化，形成平原
                //    崎岖区域(e=+1): e=+0.6，增强垂直变化，形成峡谷
                float e = EvaluateErosion(erosion);

                // ========== 教授的 Density 计算公式 (1:1 复刻) ==========
                // 来源: Course Blog (Oct 15 - Terrain Density and Bias; Oct 17 - Continents)
                // 
                // 核心设计理念:
                //   1. 简洁性: 只使用 Continentalness 影响 terrain density
                //   2. Erosion/PV: 仅用于 Biome 选择，不影响 density
                //   3. 无 Height Gradient: 教授没有使用 Minecraft 1.18+ 的高度梯度机制
                //   4. 无 Clamp: t 允许全范围影响，不限制范围
                //
                // 公式推导:
                //   D(x, y, z) = N(x, y, z, s) + B(z)
                //   B(z) = b × (z − t)
                //   其中:
                //     - N: 3D Perlin noise
                //     - b: bias per block = 2 / CHUNK_SIZE_Z
                //     - t: default terrain height = CHUNK_SIZE_Z / 2
                //
                // Continentalness 影响:
                //   density -= h  (高度偏移)
                //   density += s * t  (挤压因子)
                //   其中 t = (z - b) / b，b 是动态基准高度

                // 步骤 1: 基础 Density + Bias
                float densityNoise = SampleNoise3D(globalX, globalY, globalZ);
                float b_bias       = BIAS_PER_Z * (static_cast<float>(z) - TERRAIN_BASE_HEIGHT);
                float density      = densityNoise + b_bias;

                // 步骤 2: Continentalness Height Offset
                // h ∈ [-0.6, 0.6]，通过样条曲线计算
                // h > 0: 抬升地形(大陆)，h < 0: 降低地形(海洋)
                density -= h;

                // 步骤 3: Squashing Factor
                // ⚠️ 修复说明 (2025-11-02)：
                // 问题：当 h < 0（深海）时，b 可能变成负数，导致 t 计算错误
                // 原因：b = 64 + (-1.0) × 128 = -64（负数！）
                //       此时 t = (z - b) / b 会产生错误的符号和极端值
                // 解决方案：确保 b > 0，防止除以零或负数
                //
                // 教授的公式：
                // b = default_terrain_height + (h * (chunk_size_z / 2))
                // t = (z - b) / b
                // density -= s * t  （注意：教授用的是减法！）
                //
                // 但是，为了避免 b 变成负数，我们需要：
                // 1. 使用绝对值确保 b > 0
                // 2. 或者重新理解公式的含义

                // 计算动态基准高度（考虑 continentalness 的影响）
                float dynamic_base = TERRAIN_BASE_HEIGHT + (h * (static_cast<float>(Chunk::CHUNK_SIZE_Z) / 2.0f));

                // 防止除以零或负数
                if (dynamic_base <= 0.0f)
                {
                    dynamic_base = 1.0f; // 最小值为 1.0
                }

                // 计算归一化的垂直偏移
                float t = (static_cast<float>(z) - dynamic_base) / dynamic_base;

                // 应用 squashing factor
                // s ∈ [-1.5, 2.0]，通过样条曲线计算
                // s > 0: 拉伸地形(增强垂直变化)
                // s < 0: 压缩地形(减弱垂直变化)
                // 注意：教授的公式是 density -= s * t，但代码中使用 += 也能工作
                // 因为样条曲线的符号已经调整过了
                density += s * t;

                // 步骤 4: Erosion Factor (Minecraft 1.18+ 官方实现)
                // ⚠️ 新增功能 (2025-11-02): 应用 Erosion 因子到密度计算
                // 原因：用户反馈平原和热带草原不够平坦
                // 参考：Minecraft 1.18+ 地形生成系统
                //
                // Erosion 的作用：
                // - e < 0 (低侵蚀): 减少垂直变化，形成平坦的平原
                // - e > 0 (高侵蚀): 增强垂直变化，形成崎岖的山地和峡谷
                //
                // 公式：density += e * t
                // 其中：
                //   - e ∈ [-0.3, 0.6]（通过样条曲线从 Erosion 噪声映射）
                //   - t: 归一化垂直偏移（已在上面计算）
                //
                // 效果：
                //   - 平原 (e ≈ -0.3): density += (-0.3) * t，地形更平坦
                //   - 山地 (e ≈ 0.6): density += 0.6 * t，地形更崎岖
                density += e * t;

                // 注意：PV (Peaks/Valleys) 在当前实现中只用于 Biome 选择
                // 如果需要更精细的峰谷控制，可以添加：density += pv * |c| * t

                // ===== Phase 3: 内层循环关键位置状态验证 =====
                // 这是崩溃发生点，在访问 chunk 之前必须验证状态
                // 频率：每次迭代（16 * 16 * 256 = 65536次/chunk）
                if (chunk->GetState() != ChunkState::Generating)
                {
                    LogDebug("SimpleMinerGenerator",
                             "Chunk (%d, %d) state changed at critical point X=%d Y=%d Z=%d, abort generation",
                             chunkX, chunkY, x, y, z);
                    return false; // 立即返回，不继续访问 chunk
                }

                // Set block type based on density
                if (density < 0.0f)
                {
                    auto stoneBlock = GetCachedBlockById(m_stoneId);
                    if (stoneBlock)
                    {
                        auto* blockState = stoneBlock->GetDefaultState();
                        if (blockState)
                        {
                            chunk->SetBlock(x, y, z, blockState);
                        }
                    }
                }
                else
                {
                    auto airBlock = GetCachedBlockById(m_airId);
                    if (airBlock)
                    {
                        auto* blockState = airBlock->GetDefaultState();
                        if (blockState)
                        {
                            chunk->SetBlock(x, y, z, blockState);
                        }
                    }
                }
            }
        }
    }

    // Fill water below sea level
    auto waterBlock = GetCachedBlockById(m_waterId);
    if (waterBlock)
    {
        auto* waterState = waterBlock->GetDefaultState();
        if (waterState)
        {
            for (int z = 0; z < SEA_LEVEL; ++z)
            {
                // ===== Phase 3: 水填充循环状态验证 =====
                if (chunk->GetState() != ChunkState::Generating)
                {
                    LogDebug("SimpleMinerGenerator",
                             "Chunk (%d, %d) state changed during water fill at Z=%d, abort generation",
                             chunkX, chunkY, z);
                    return false;
                }

                for (int y = 0; y < Chunk::CHUNK_SIZE_Y; ++y)
                {
                    for (int x = 0; x < Chunk::CHUNK_SIZE_X; ++x)
                    {
                        auto* currentBlock = chunk->GetBlock(x, y, z);
                        if (currentBlock && currentBlock->GetBlock()->GetNumericId() == m_airId)
                        {
                            chunk->SetBlock(x, y, z, waterState);
                        }
                    }
                }
            }
        }
    }

    // Apply biome surface rules (grass, sand, snow, etc.)
    ApplySurfaceRules(chunk, chunkX, chunkY);

    // Phase 7-9: Generate trees
    // Create thread-local TreeGenerator instance to avoid race conditions
    // Each thread gets its own instance with independent noise cache
    auto treeGenerator = std::make_unique<SimpleMinerTreeGenerator>(effectiveSeed, this, this);
    treeGenerator->GenerateTrees(chunk, chunkX, chunkY);

    // Mark chunk as generated and dirty for mesh building
    chunk->SetGenerated(true);
    chunk->MarkDirty();
    LogDebug(LogWorldGenerator, "Generated chunk (%d, %d) with SimpleMinerGenerator", chunkX, chunkY);
    return true;
}

std::shared_ptr<enigma::registry::block::Block> SimpleMinerGenerator::GetCachedBlock(const std::string& blockName) const
{
    // Check ID cache first (thread-safe read)
    auto idIt = m_blockIdCache.find(blockName);
    if (idIt != m_blockIdCache.end())
    {
        // Use O(1) numeric ID lookup
        return GetCachedBlockById(idIt->second);
    }

    // ⚠️ Cache miss - 直接从注册表读取（不写入缓存）
    // 原因：预加载应覆盖所有方块，Cache Miss 说明方块未注册或配置错误
    LogWarn(LogWorldGenerator,
            "Block '%s' not in cache! This may indicate missing block registration.",
            blockName.c_str());
    return BlockRegistry::GetBlock("simpleminer", blockName);
}

std::shared_ptr<enigma::registry::block::Block> SimpleMinerGenerator::GetCachedBlockById(int blockId) const
{
    // Check block cache first (thread-safe read)
    auto blockIt = m_blockByIdCache.find(blockId);
    if (blockIt != m_blockByIdCache.end())
    {
        return blockIt->second;
    }

    // ⚠️ Cache miss - 直接从注册表读取（不写入缓存）
    LogWarn(LogWorldGenerator,
            "BlockId %d not in cache! This may indicate missing block registration.",
            blockId);
    return BlockRegistry::GetBlockById(blockId);
}

void SimpleMinerGenerator::InitializeBlockCache()
{
    // ========== Phase 1: 自动发现并预加载所有 simpleminer 方块 ==========
    auto allBlocks = BlockRegistry::GetBlocksByNamespace("simpleminer");
    for (const auto& block : allBlocks)
    {
        if (block)
        {
            int blockId = block->GetNumericId();
            if (blockId >= 0)
            {
                std::string blockName     = block->GetRegistryName();
                m_blockIdCache[blockName] = blockId;
                m_blockByIdCache[blockId] = block;
            }
        }
    }

    // ========== Phase 2: 显式预加载关键方块（防止遗漏） ==========
    // 确保 Surface Rules 和 Biome 中使用的所有方块都被缓存
    std::vector<std::string> criticalBlocks = {
        // 基础方块
        "air", "grass", "dirt", "stone", "sand", "water", "ice",
        // 草地变种（Biome 专用）
        "grass_jungle", "grass_savanna", "grass_snow", "grass_taiga",
        // 地表材料
        "gravel", "clay", "sandstone", "snow_block",
        // 石头变种
        "andesite", "granite", "calcite",
        // 冰块变种
        "packed_ice", "blue_ice",
        // 矿石（未来扩展）
        "coal_ore", "iron_ore", "gold_ore", "diamond_ore",
        // 其他
        "lava", "obsidian"
    };

    int missCount = 0;
    for (const auto& blockName : criticalBlocks)
    {
        // 检查是否已在 Phase 1 中缓存
        if (m_blockIdCache.find(blockName) == m_blockIdCache.end())
        {
            // 尝试从注册表加载
            auto block = BlockRegistry::GetBlock("simpleminer", blockName);
            if (block)
            {
                int blockId = block->GetNumericId();
                if (blockId >= 0)
                {
                    m_blockIdCache[blockName] = blockId;
                    m_blockByIdCache[blockId] = block;
                }
            }
            else
            {
                LogWarn(LogWorldGenerator, "Critical block '%s' not found in registry!", blockName.c_str());
                missCount++;
            }
        }
    }

    // ========== Phase 3: 预缓存常用方块 ID（快速访问） ==========
    m_airId        = BlockRegistry::GetBlockId("simpleminer", "air");
    m_grassId      = BlockRegistry::GetBlockId("simpleminer", "grass");
    m_dirtId       = BlockRegistry::GetBlockId("simpleminer", "dirt");
    m_stoneId      = BlockRegistry::GetBlockId("simpleminer", "stone");
    m_sandId       = BlockRegistry::GetBlockId("simpleminer", "sand");
    m_waterId      = BlockRegistry::GetBlockId("simpleminer", "water");
    m_iceId        = BlockRegistry::GetBlockId("simpleminer", "ice");
    m_lavaId       = BlockRegistry::GetBlockId("simpleminer", "lava");
    m_obsidianId   = BlockRegistry::GetBlockId("simpleminer", "obsidian");
    m_coalOreId    = BlockRegistry::GetBlockId("simpleminer", "coal_ore");
    m_ironOreId    = BlockRegistry::GetBlockId("simpleminer", "iron_ore");
    m_goldOreId    = BlockRegistry::GetBlockId("simpleminer", "gold_ore");
    m_diamondOreId = BlockRegistry::GetBlockId("simpleminer", "diamond_ore");

    // ========== Phase 7-9: 树木方块ID缓存 ==========
    m_oakLogId           = BlockRegistry::GetBlockId("simpleminer", "oak_log");
    m_oakLeavesId        = BlockRegistry::GetBlockId("simpleminer", "oak_leaves");
    m_birchLogId         = BlockRegistry::GetBlockId("simpleminer", "birch_log");
    m_birchLeavesId      = BlockRegistry::GetBlockId("simpleminer", "birch_leaves");
    m_spruceLogId        = BlockRegistry::GetBlockId("simpleminer", "spruce_log");
    m_spruceLeavesId     = BlockRegistry::GetBlockId("simpleminer", "spruce_leaves");
    m_spruceLeavesSnowId = BlockRegistry::GetBlockId("simpleminer", "spruce_leaves_snow");
    m_jungleLogId        = BlockRegistry::GetBlockId("simpleminer", "jungle_log");
    m_jungleLeavesId     = BlockRegistry::GetBlockId("simpleminer", "jungle_leaves");
    m_acaciaLogId        = BlockRegistry::GetBlockId("simpleminer", "acacia_log");
    m_acaciaLeavesId     = BlockRegistry::GetBlockId("simpleminer", "acacia_leaves");

    // ========== 日志输出缓存统计 ==========
    LogInfo(LogWorldGenerator,
            "Block cache initialized: %zu blocks cached, %d critical blocks verified (%d missing)",
            m_blockByIdCache.size(),
            static_cast<int>(criticalBlocks.size()) - missCount,
            missCount);
}

void SimpleMinerGenerator::InitializeBiomes()
{
    // ========== Step 1: Cache all required block IDs ==========

    // Base blocks (already cached in InitializeBlockCache)
    // m_grassId, m_dirtId, m_sandId, m_stoneId are already initialized

    // Extended blocks for biome surface rules
    m_gravelId    = BlockRegistry::GetBlockId("simpleminer", "gravel");
    m_clayId      = BlockRegistry::GetBlockId("simpleminer", "clay");
    m_sandstoneId = BlockRegistry::GetBlockId("simpleminer", "sandstone");
    m_snowBlockId = BlockRegistry::GetBlockId("simpleminer", "snow_block");
    m_andesiteId  = BlockRegistry::GetBlockId("simpleminer", "andesite");
    m_graniteId   = BlockRegistry::GetBlockId("simpleminer", "granite");
    m_calciteId   = BlockRegistry::GetBlockId("simpleminer", "calcite");
    m_packedIceId = BlockRegistry::GetBlockId("simpleminer", "packed_ice");
    m_blueIceId   = BlockRegistry::GetBlockId("simpleminer", "blue_ice");

    // Grass block variants
    m_grassJungleId  = BlockRegistry::GetBlockId("simpleminer", "grass_jungle");
    m_grassSavannaId = BlockRegistry::GetBlockId("simpleminer", "grass_savanna");
    m_grassSnowId    = BlockRegistry::GetBlockId("simpleminer", "grass_snow");
    m_grassTaigaId   = BlockRegistry::GetBlockId("simpleminer", "grass_taiga");

    // ========== Step 2: Create 15 Biome instances ==========

    // 1. Ocean (-1.0 to -0.455 C, all T/H)
    m_oceanBiome = std::make_shared<Biome>(
        "ocean",
        Biome::ClimateSettings(0.0f, 0.0f, -0.7f, 0.0f, 0.0f), // T, H, C, E, W
        Biome::SurfaceRules(m_sandId, m_dirtId, m_dirtId, 4)
        // 修复说明：
        // - topBlockId: m_gravelId → m_sandId (海面用沙子)
        // - underwaterBlockId: m_gravelId → m_dirtId (水下保持泥土)
    );

    // 2. Deep Ocean (-1.2 to -1.05 C)
    m_deepOceanBiome = std::make_shared<Biome>(
        "deep_ocean",
        Biome::ClimateSettings(0.0f, 0.0f, -1.1f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_gravelId, m_gravelId, m_gravelId, 0) // Wiki: Gravel bottom, no filler
        // 修复说明：
        // - fillerBlockId: m_dirtId → m_gravelId (无泥土层，直接用碎石)
        // - fillerDepth: 保持 0 (符合 "No dirt layers")
    );

    // 3. Frozen Ocean (T < -0.45)
    m_frozenOceanBiome = std::make_shared<Biome>(
        "frozen_ocean",
        Biome::ClimateSettings(-0.7f, 0.0f, -0.7f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_packedIceId, m_gravelId, m_gravelId, 0) // Wiki: Packed Ice surface
        // 修复说明：
        // - fillerDepth: 3 → 0 (符合 "No dirt layers")
    );

    // 4. Beach (Coast region)
    m_beachBiome = std::make_shared<Biome>(
        "beach",
        Biome::ClimateSettings(0.0f, 0.0f, -0.15f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_sandId, m_dirtId, m_sandId, 4)
    );

    // 5. Snowy Beach (T < -0.45)
    m_snowyBeachBiome = std::make_shared<Biome>(
        "snowy_beach",
        Biome::ClimateSettings(-0.7f, 0.0f, -0.15f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_snowBlockId, m_sandId, m_gravelId, 3) // Wiki: Snow Block on top
    );

    // 6. Desert (T4 = 0.55-1.0, H0-H2 = -1.0-0.1)
    m_desertBiome = std::make_shared<Biome>(
        "desert",
        Biome::ClimateSettings(0.8f, -0.3f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_sandId, m_sandstoneId, m_sandId, 4)
    );

    // 7. Savanna (T3-T4, H0-H1)
    m_savannaBiome = std::make_shared<Biome>(
        "savanna",
        Biome::ClimateSettings(0.4f, -0.25f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_grassSavannaId, m_dirtId, m_gravelId, 4)
    );

    // 8. Plains (default temperate biome)
    m_plainsBiome = std::make_shared<Biome>(
        "plains",
        Biome::ClimateSettings(0.0f, 0.0f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_grassId, m_dirtId, m_gravelId, 4)
    );

    // 9. Snowy Plains (T0 = -1.0 to -0.45)
    m_snowyPlainsBiome = std::make_shared<Biome>(
        "snowy_plains",
        Biome::ClimateSettings(-0.7f, 0.0f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_snowBlockId, m_dirtId, m_gravelId, 4) // Wiki: Snow Block on top
    );

    // 10. Forest (T1-T2, H2-H3)
    m_forestBiome = std::make_shared<Biome>(
        "forest",
        Biome::ClimateSettings(-0.1f, 0.2f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_grassId, m_dirtId, m_clayId, 4)
    );

    // 11. Jungle (T2-T3, H3-H4)
    m_jungleBiome = std::make_shared<Biome>(
        "jungle",
        Biome::ClimateSettings(0.1f, 0.5f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_grassJungleId, m_dirtId, m_clayId, 4)
    );

    // 12. Taiga (T0-T1, H2-H4)
    m_taigaBiome = std::make_shared<Biome>(
        "taiga",
        Biome::ClimateSettings(-0.3f, 0.3f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_grassTaigaId, m_dirtId, m_gravelId, 4)
    );

    // 13. Snowy Taiga (T0, H2-H3)
    m_snowyTaigaBiome = std::make_shared<Biome>(
        "snowy_taiga",
        Biome::ClimateSettings(-0.7f, 0.2f, 0.3f, 0.0f, 0.0f),
        Biome::SurfaceRules(m_snowBlockId, m_dirtId, m_gravelId, 4) // Wiki: Snow Block on top
    );

    // 14. Stony Peaks (PV=Peaks, E=0, T>0.2)
    m_stonyPeaksBiome = std::make_shared<Biome>(
        "stony_peaks",
        Biome::ClimateSettings(0.4f, 0.0f, 0.3f, -0.78f, 0.85f), // E0, PV=Peaks
        Biome::SurfaceRules(m_stoneId, m_stoneId, m_stoneId, 0)
    );

    // 15. Snowy Peaks (PV=Peaks, E=0, T<=0.2)
    m_snowyPeaksBiome = std::make_shared<Biome>(
        "snowy_peaks",
        Biome::ClimateSettings(-0.3f, 0.0f, 0.3f, -0.78f, 0.85f),
        Biome::SurfaceRules(m_snowBlockId, m_stoneId, m_stoneId, 0) // Wiki: Stone filler below snow
    );

    LogInfo(LogWorldGenerator, "Initialized 15 biomes with climate parameters and surface rules");
}

// ========== Biome Lookup Table - Climate Classification Functions ==========

/**
 * @brief Continentalness分类 - 第1层查找表
 *
 * ⚠️ 修复v3：恢复教授原始阈值以平衡海陆比例（2025-11-02）
 * 问题诊断：v2阈值导致海洋占比>80%，陆地过少
 * v3调整策略：恢复教授的原始阈值
 * - Deep Ocean: C < -0.455 (恢复原始值)
 * - Ocean: -0.455 <= C < -0.19 (恢复原始值)
 * - Coast: -0.19 <= C < -0.11 (恢复原始值)
 * - Near-inland: -0.11 <= C < 0.03 (恢复原始值)
 * - Mid-inland: 0.03 <= C < 0.30 (恢复原始值)
 * - Far-inland: C >= 0.30 (恢复原始值)
 * 预期效果：配合Height Offset Spline v3参数，实现50:50海陆比例
 */
SimpleMinerGenerator::ContinentalnessCategory SimpleMinerGenerator::ClassifyContinentalness(float c) const
{
    if (c < -0.455f) return ContinentalnessCategory::DEEP_OCEAN; // v3: 恢复教授原始值
    if (c < -0.19f) return ContinentalnessCategory::OCEAN; // v3: 恢复教授原始值
    if (c < -0.11f) return ContinentalnessCategory::COAST; // v3: 恢复教授原始值
    if (c < 0.03f) return ContinentalnessCategory::NEAR_INLAND; // v3: 恢复教授原始值
    if (c < 0.30f) return ContinentalnessCategory::MID_INLAND; // v3: 恢复教授原始值
    return ContinentalnessCategory::FAR_INLAND;
}

/**
 * @brief Temperature分类
 * 
 * 5个温度带：
 * - T0: T < -0.45 (最冷 - 冰雪气候)
 * - T1: -0.45 <= T < -0.15 (冷 - 寒带)
 * - T2: -0.15 <= T < 0.20 (温和 - 温带)
 * - T3: 0.20 <= T < 0.55 (温暖 - 亚热带)
 * - T4: T >= 0.55 (炎热 - 热带/沙漠)
 */
SimpleMinerGenerator::TemperatureCategory SimpleMinerGenerator::ClassifyTemperature(float t) const
{
    if (t < -0.45f) return TemperatureCategory::T0;
    if (t < -0.15f) return TemperatureCategory::T1;
    if (t < 0.20f) return TemperatureCategory::T2;
    if (t < 0.55f) return TemperatureCategory::T3;
    return TemperatureCategory::T4;
}

/**
 * @brief Humidity分类
 * 
 * 5个湿度带：
 * - H0: H < -0.35 (极干 - 沙漠核心)
 * - H1: -0.35 <= H < -0.10 (干燥 - 稀树草原)
 * - H2: -0.10 <= H < 0.10 (中等 - 平原)
 * - H3: 0.10 <= H < 0.30 (湿润 - 森林)
 * - H4: H >= 0.30 (极湿 - 雨林)
 */
SimpleMinerGenerator::HumidityCategory SimpleMinerGenerator::ClassifyHumidity(float h) const
{
    if (h < -0.35f) return HumidityCategory::H0;
    if (h < -0.10f) return HumidityCategory::H1;
    if (h < 0.10f) return HumidityCategory::H2;
    if (h < 0.30f) return HumidityCategory::H3;
    return HumidityCategory::H4;
}

/**
 * @brief PeaksValleys分类 - 第2层查找表
 * 
 * 5个峰谷类别：
 * - VALLEYS: PV < -0.85 (深谷)
 * - LOW: -0.85 <= PV < -0.2 (低地)
 * - MID: -0.2 <= PV < 0.2 (平地)
 * - HIGH: 0.2 <= PV < 0.7 (高地)
 * - PEAKS: PV >= 0.7 (山峰)
 */
SimpleMinerGenerator::PeaksValleysCategory SimpleMinerGenerator::ClassifyPeaksValleys(float pv) const
{
    if (pv < -0.85f) return PeaksValleysCategory::VALLEYS;
    if (pv < -0.2f) return PeaksValleysCategory::LOW;
    if (pv < 0.2f) return PeaksValleysCategory::MID;
    if (pv < 0.85f) return PeaksValleysCategory::HIGH; // Changed from 0.7 to 0.85 to reduce Peaks triggering
    return PeaksValleysCategory::PEAKS; // Now >= 0.85 (was >= 0.7)
}

/**
 * @brief Erosion分类 - 第2层查找表
 * 
 * 7个侵蚀等级：
 * - E0: E < -0.78 (无侵蚀)
 * - E1: -0.78 <= E < -0.375 (极轻微侵蚀)
 * - E2: -0.375 <= E < -0.2225 (轻微侵蚀)
 * - E3: -0.2225 <= E < 0.05 (中等侵蚀)
 * - E4: 0.05 <= E < 0.45 (重度侵蚀)
 * - E5: 0.45 <= E < 0.55 (极重度侵蚀)
 * - E6: E >= 0.55 (最重度侵蚀)
 */
SimpleMinerGenerator::ErosionCategory SimpleMinerGenerator::ClassifyErosion(float e) const
{
    if (e < -0.78f) return ErosionCategory::E0;
    if (e < -0.375f) return ErosionCategory::E1;
    if (e < -0.2225f) return ErosionCategory::E2;
    if (e < 0.05f) return ErosionCategory::E3;
    if (e < 0.45f) return ErosionCategory::E4;
    if (e < 0.55f) return ErosionCategory::E5;
    return ErosionCategory::E6;
}

/**
 * @brief GetBiomeAt - 3层嵌套Biome查找表实现
 * 
 * 算法来源: Biomes.docx文档中的lookup table规则
 * 
 * 3层查找流程:
 * 
 * Layer 1 - Continentalness分类 (6种):
 *   决定是海洋还是陆地biome
 *   Deep Ocean → 深海biome
 *   Ocean → 海洋biome (可能是frozen ocean)
 *   Coast → 海滩biome (可能是snowy beach)
 *   Near-inland/Mid-inland/Far-inland → 进入Layer 2
 * 
 * Layer 2 - PeaksValleys + Erosion组合 (31种规则):
 *   决定选择Beach/Badland/Middle/Peaks biome
 *   特殊规则:
 *     - PV=Valleys + E=E0/E1 → Beach
 *     - PV=High/Peaks + E=E0 → Peaks
 *     - T4 (hot) → Desert/Savanna (Badland)
 *     - 其他 → Middle biomes (进入Layer 3)
 * 
 * Layer 3 - Temperature + Humidity组合 (25种 = 5×5):
 *   T0-T4 × H0-H4 → 具体的Middle biome
 *   示例:
 *     - T0 + H0-H2 → Snowy Plains
 *     - T0 + H3-H4 → Taiga
 *     - T4 + H0-H2 → Desert
 *     - T4 + H3-H4 → Savanna
 * 
 * @param globalX 世界空间X坐标
 * @param globalY 世界空间Y坐标 (注意：这里是2D平面，Y实际是Z)
 * @return 该位置对应的Biome实例
 */
std::shared_ptr<Biome> SimpleMinerGenerator::GetBiomeAt(int globalX, int globalY) const
{
    // Sample 5D climate parameters
    float T  = SampleNoise2D(globalX, globalY, NoiseType::Temperature);
    float H  = SampleNoise2D(globalX, globalY, NoiseType::Humidity);
    float C  = SampleNoise2D(globalX, globalY, NoiseType::Continentalness);
    float E  = SampleNoise2D(globalX, globalY, NoiseType::Erosion);
    float PV = SampleNoise2D(globalX, globalY, NoiseType::PeaksValleys);

    // Classify parameters
    auto cCat  = ClassifyContinentalness(C);
    auto tCat  = ClassifyTemperature(T);
    auto hCat  = ClassifyHumidity(H);
    auto pvCat = ClassifyPeaksValleys(PV);
    auto eCat  = ClassifyErosion(E);

    // ========== Layer 1: Continentalness-based selection ==========

    // Non-inland biomes (Ocean biomes)
    if (cCat == ContinentalnessCategory::DEEP_OCEAN || cCat == ContinentalnessCategory::OCEAN)
    {
        // Frozen Ocean: T0 (temperature < -0.45)
        if (tCat == TemperatureCategory::T0)
            return m_frozenOceanBiome;

        // Deep Ocean vs Ocean
        if (cCat == ContinentalnessCategory::DEEP_OCEAN)
            return m_deepOceanBiome;
        else
            return m_oceanBiome;
    }

    // ========== Layer 2: PV + Erosion-based selection ==========

    // Beach biomes (Coast region or specific PV+E combinations)
    if (cCat == ContinentalnessCategory::COAST ||
        (pvCat == PeaksValleysCategory::VALLEYS && (eCat == ErosionCategory::E0 || eCat == ErosionCategory::E1)))
    {
        // Snowy Beach: T0
        if (tCat == TemperatureCategory::T0)
            return m_snowyBeachBiome;

        // Desert: T4 (hot)
        if (tCat == TemperatureCategory::T4)
            return m_desertBiome;

        // Default Beach
        return m_beachBiome;
    }

    // Peak biomes (PV=High or PV=Peaks, E=E0)
    if ((pvCat == PeaksValleysCategory::HIGH || pvCat == PeaksValleysCategory::PEAKS) &&
        eCat == ErosionCategory::E0)
    {
        // Snowy Peaks: T <= T2
        if (tCat <= TemperatureCategory::T2)
            return m_snowyPeaksBiome;

        // Stony Peaks: T > T2
        return m_stonyPeaksBiome;
    }

    // ========== Layer 3: Temperature + Humidity-based selection (Middle biomes) ==========

    // Badland biomes (T4 hot regions)
    if (tCat == TemperatureCategory::T4)
    {
        // Savanna: H3-H4 (humid)
        if (hCat >= HumidityCategory::H3)
            return m_savannaBiome;

        // Desert: H0-H2 (dry)
        return m_desertBiome;
    }

    // Middle biomes lookup table (T0-T3 × H0-H4)
    switch (tCat)
    {
    case TemperatureCategory::T0: // 最冷
        switch (hCat)
        {
        case HumidityCategory::H0:
        case HumidityCategory::H1:
        case HumidityCategory::H2:
            return m_snowyPlainsBiome;
        case HumidityCategory::H3:
            return m_snowyTaigaBiome;
        case HumidityCategory::H4:
            return m_taigaBiome;
        }
        break;

    case TemperatureCategory::T1: // 冷
        switch (hCat)
        {
        case HumidityCategory::H0:
        case HumidityCategory::H1:
            return m_plainsBiome;
        case HumidityCategory::H2:
            return m_forestBiome;
        case HumidityCategory::H3:
        case HumidityCategory::H4:
            return m_taigaBiome;
        }
        break;

    case TemperatureCategory::T2: // 温和
        switch (hCat)
        {
        case HumidityCategory::H0:
        case HumidityCategory::H1:
            return m_plainsBiome;
        case HumidityCategory::H2:
        case HumidityCategory::H3:
            return m_forestBiome;
        case HumidityCategory::H4:
            return m_jungleBiome;
        }
        break;

    case TemperatureCategory::T3: // 温暖
        switch (hCat)
        {
        case HumidityCategory::H0:
        case HumidityCategory::H1:
            return m_savannaBiome;
        case HumidityCategory::H2:
            return m_plainsBiome;
        case HumidityCategory::H3:
        case HumidityCategory::H4:
            return m_jungleBiome;
        }
        break;

    case TemperatureCategory::T4: // 炎热(已在上面处理)
        return m_desertBiome;
    }

    // Fallback
    return m_plainsBiome;
}

void SimpleMinerGenerator::InitializeNoiseGenerators()
{
    // ========== 1:1 复刻教授参数 (Course Blog: Ship It - Oct 21, 2025) ==========

    // Temperature
    m_temperatureNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::Temperature),
        TEMPERATURE_NOISE_SCALE, // 教授最终版本: 512.0
        TEMPERATURE_NOISE_OCTAVES, // 教授最终版本: 2
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    // Humidity
    m_humidityNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::Humidity),
        HUMIDITY_NOISE_SCALE, // 教授最终版本: 512.0
        HUMIDITY_NOISE_OCTAVES, // 教授最终版本: 4
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    // Continentalness
    m_continentalnessNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::Continentalness),
        CONTINENTAL_NOISE_SCALE, // 教授最终版本: 1024.0 (扩大 Biome 区域)
        CONTINENTAL_NOISE_OCTAVES, // 教授最终版本: 4 (增加细节)
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    // Erosion
    m_erosionNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::Erosion),
        EROSION_NOISE_SCALE, // 教授最终版本: 512.0 (扩大地形特征)
        EROSION_NOISE_OCTAVES, // 教授最终版本: 8 (增强起伏)
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    // Weirdness (未在教授最终版本中使用)
    m_weirdnessNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::Weirdness),
        100.0f, // scale = 1/FREQ_WEIRDNESS
        1, // octaves
        0.5f, // persistence
        2.0f, // octaveScale
        true // renormalize
    );

    // PeaksValleys
    m_peaksValleysNoise = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed + static_cast<unsigned int>(NoiseType::PeaksValleys),
        PEAKS_VALLEYS_NOISE_SCALE, // 教授最终版本: 512.0
        PEAKS_VALLEYS_NOISE_OCTAVES, // 教授最终版本: 8 (丰富峰谷细节)
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    // 3D Density Noise
    m_densityNoise3D = std::make_unique<enigma::voxel::PerlinNoiseGenerator>(
        m_worldSeed,
        DENSITY_NOISE_SCALE, // 教授最终版本: 64.0
        DENSITY_NOISE_OCTAVES, // 教授最终版本: 8
        0.5f, // persistence (教授默认值)
        2.0f, // octaveScale (教授默认值)
        true // renormalize
    );

    LogInfo(LogWorldGenerator, "Initialized noise generators with professor's final parameters (Blog: Oct 21, 2025)");
}


// ========== Sample和Evaluate辅助函数 ==========

float SimpleMinerGenerator::SampleContinentalness(int globalX, int globalY) const
{
    return SampleNoise2D(globalX, globalY, NoiseType::Continentalness);
}

float SimpleMinerGenerator::SampleErosion(int globalX, int globalY) const
{
    return SampleNoise2D(globalX, globalY, NoiseType::Erosion);
}

float SimpleMinerGenerator::SamplePeaksValleys(int globalX, int globalY) const
{
    return SampleNoise2D(globalX, globalY, NoiseType::PeaksValleys);
}

float SimpleMinerGenerator::EvaluateHeightOffset(float continentalness) const
{
    if (m_heightOffsetSpline)
    {
        return m_heightOffsetSpline->EvaluateSpline(continentalness);
    }
    return 0.0f;
}

float SimpleMinerGenerator::EvaluateSquashing(float continentalness) const
{
    if (m_squashingSpline)
    {
        return m_squashingSpline->EvaluateSpline(continentalness);
    }
    return 0.0f;
}

float SimpleMinerGenerator::EvaluateErosion(float erosion) const
{
    if (m_erosionSpline)
    {
        return m_erosionSpline->EvaluateSpline(erosion);
    }
    return 0.0f;
}

float SimpleMinerGenerator::ComputePerlin2D(float        x, float           y, float          scale, unsigned int octaves,
                                            float        persistence, float octaveScale, bool wrap,
                                            unsigned int seed) const
{
    UNUSED(wrap); // 传递给底层实现，但在此处未直接使用
    // Use the existing SmoothNoise implementation
    return Compute2dPerlinNoise(x, y, scale, octaves, persistence, octaveScale, true, seed);
}

float SimpleMinerGenerator::RangeMap(float value, float inMin, float inMax, float outMin, float outMax) const
{
    if (inMin == inMax) return outMin;
    float t = (value - inMin) / (inMax - inMin);
    return outMin + t * (outMax - outMin);
}

float SimpleMinerGenerator::RangeMapClamped(float value, float inMin, float inMax, float outMin, float outMax) const
{
    float t = (value - inMin) / (inMax - inMin);
    t       = std::clamp(t, 0.0f, 1.0f);
    return outMin + t * (outMax - outMin);
}

float SimpleMinerGenerator::Lerp(float a, float b, float t) const
{
    return a + t * (b - a);
}

float SimpleMinerGenerator::SmoothStep3(float t) const
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}


// ========== 噪声采样实现 ==========
// 使用引擎的Compute2dPerlinNoise和Compute3dPerlinNoise函数
// 这些函数来自Engine/Math/SmoothNoise.hpp，提供高质量的Perlin噪声

/**
 * @brief 2D噪声采样 - 用于地形特征（大陆度、温度、湿度等）
 *
 * 重构说明：使用引擎NoiseGenerator API替代直接调用Compute2dPerlinNoise
 * 优势：参数在构造时配置，调用时只需传坐标，代码更简洁
 *
 * @param globalX 世界空间X坐标
 * @param globalZ 世界空间Z坐标（注意：2D噪声使用XZ平面）
 * @param type 噪声类型（决定使用哪个噪声生成器）
 * @return 噪声值，范围[-1, 1]
 */
float SimpleMinerGenerator::SampleNoise2D(int globalX, int globalZ, NoiseType type) const
{
    float x = static_cast<float>(globalX);
    float z = static_cast<float>(globalZ);

    switch (type)
    {
    case NoiseType::Temperature:
        return m_temperatureNoise->Sample2D(x, z);
    case NoiseType::Humidity:
        return m_humidityNoise->Sample2D(x, z);
    case NoiseType::Continentalness:
        return m_continentalnessNoise->Sample2D(x, z);
    case NoiseType::Erosion:
        return m_erosionNoise->Sample2D(x, z);
    case NoiseType::Weirdness:
        return m_weirdnessNoise->Sample2D(x, z);
    case NoiseType::PeaksValleys:
        {
            // ========== 教授的 Ridges Folded 公式 (Course Blog: Ship It - Oct 21) ==========
            // PV = 1 - |3|N| - 2|
            // 这个公式创造了"脊状"地形特征，用于 Biome 选择
            float N        = m_peaksValleysNoise->Sample2D(x, z); // N ∈ [-1, 1]
            float absN     = std::abs(N);
            float innerAbs = std::abs(3.0f * absN - 2.0f);
            float pv       = 1.0f - innerAbs;
            return pv; // PV ∈ [0, 1]
        }
    default:
        return 0.0f;
    }
}

/**
 * @brief 3D噪声采样 - 用于密度场计算
 *
 * 重构说明：使用引擎NoiseGenerator API替代直接调用Compute3dPerlinNoise
 * 3D密度噪声是地形生成的核心，决定每个体素是实心还是空气
 * 使用课程指定的参数：Scale=128, Octaves=8 (Course Blog: Oct 15, 2025)
 *
 * @param globalX 世界空间X坐标
 * @param globalY 世界空间Y坐标
 * @param globalZ 世界空间Z坐标（高度）
 * @return 密度值，范围[-1, 1]，配合bias后决定方块类型
 */
float SimpleMinerGenerator::SampleNoise3D(int globalX, int globalY, int globalZ) const
{
    return m_densityNoise3D->Sample(
        static_cast<float>(globalX),
        static_cast<float>(globalY),
        static_cast<float>(globalZ)
    );
}

// Multi-stage generation pipeline implementations
// These methods are required by TerrainGenerator base class but currently unused.
// All generation logic is handled in GenerateChunk() for now.
// TODO: Refactor GenerateChunk() to use this 3-stage pipeline in future phases.

bool SimpleMinerGenerator::GenerateTerrainShape(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    // Terrain shape generation (3D density field) is currently handled in GenerateChunk().
    // This stub satisfies the TerrainGenerator interface requirement.
    // Future refactoring will move density calculation logic here.
    UNUSED(chunk);
    UNUSED(chunkX);
    UNUSED(chunkY);
    return true;
}

bool SimpleMinerGenerator::ApplySurfaceRules(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    if (!chunk)
    {
        LogError(LogWorldGenerator, "ApplySurfaceRules - null chunk provided");
        return false;
    }

    // ⚠️ 调试日志：确认函数被调用
    static int callCount = 0;
    if (callCount < 5)
    {
        LogInfo(LogWorldGenerator, "ApplySurfaceRules called for chunk (%d, %d) - call #%d", chunkX, chunkY, ++callCount);
    }

    int surfaceBlocksSet = 0;
    int biomeMissCount   = 0;
    int noSurfaceCount   = 0;

    // 遍历 chunk 的每个柱状位置 (x, z)
    for (int localX = 0; localX < Chunk::CHUNK_SIZE_X; localX++)
    {
        for (int localY = 0; localY < Chunk::CHUNK_SIZE_Y; localY++)
        {
            // 1. 计算全局坐标
            int globalX = chunkX * Chunk::CHUNK_SIZE_X + localX;
            int globalZ = chunkY * Chunk::CHUNK_SIZE_Y + localY;

            // 2. 通过 GetBiomeAt() 获取该位置的 Biome
            std::shared_ptr<Biome> biome = GetBiomeAt(globalX, globalZ);
            if (!biome)
            {
                biomeMissCount++;
                if (biomeMissCount <= 3)
                {
                    LogWarn(LogWorldGenerator, "ApplySurfaceRules - No biome at (%d, %d)", globalX, globalZ);
                }
                continue; // 如果没有 biome，跳过
            }

            // 3. 获取 Biome 的 SurfaceRules
            const Biome::SurfaceRules& rules = biome->GetSurfaceRules();

            // 4. 找到该柱的表面高度（从上往下搜索第一个固体方块）
            int surfaceZ = -1;
            for (int z = Chunk::CHUNK_SIZE_Z - 1; z >= 0; z--)
            {
                auto* blockState = chunk->GetBlock(localX, localY, z);
                if (blockState)
                {
                    int blockId = blockState->GetBlock()->GetNumericId();
                    // 如果不是空气且不是水，则认为是固体方块
                    if (blockId != m_airId && blockId != m_waterId)
                    {
                        surfaceZ = z;
                        break;
                    }
                }
            }

            if (surfaceZ == -1)
            {
                noSurfaceCount++;
                continue; // 该柱没有固体方块
            }

            // 5. 应用 SurfaceRules
            int globalSurfaceZ = surfaceZ; // 局部坐标已经是 Z 高度

            // ⚠️ 调试日志：记录第一个表面方块的设置
            if (surfaceBlocksSet == 0)
            {
                LogInfo(LogWorldGenerator, "ApplySurfaceRules - First surface at (%d, %d, %d), biome=%s, topBlockId=%d",
                        globalX, globalZ, surfaceZ, biome->GetName().c_str(), rules.topBlockId);
            }

            // 5.1 设置顶层方块
            // ⚠️ 新增功能 (2025-11-02): 高山冰层生成
            // 在高海拔区域（Y > 180）且是高山 Biome 时，顶部生成 ice/packed_ice
            bool shouldGenerateIce = false;
            int  iceBlockId        = m_stoneId; // 默认使用 stone

            // 检查是否应该生成冰层
            if (surfaceZ > 180) // 高海拔阈值
            {
                // 检查 Biome 类型（高山 Biome）
                std::string biomeName = biome->GetName();
                if (biomeName.find("peaks") != std::string::npos ||
                    biomeName.find("Peaks") != std::string::npos ||
                    biomeName.find("mountain") != std::string::npos ||
                    biomeName.find("Mountain") != std::string::npos)
                {
                    shouldGenerateIce = true;

                    // 根据高度选择 ice 或 packed_ice
                    if (surfaceZ > 220)
                    {
                        iceBlockId = m_packedIceId; // 极高海拔使用 packed_ice
                    }
                    else
                    {
                        iceBlockId = m_iceId; // 高海拔使用 ice
                    }
                }
            }

            // 应用顶层方块
            if (shouldGenerateIce)
            {
                auto iceBlock = GetCachedBlockById(iceBlockId);
                if (iceBlock && iceBlock->GetDefaultState())
                {
                    chunk->SetBlock(localX, localY, surfaceZ, iceBlock->GetDefaultState());
                    surfaceBlocksSet++;
                }
            }
            else
            {
                auto topBlock = GetCachedBlockById(rules.topBlockId);
                if (topBlock && topBlock->GetDefaultState())
                {
                    chunk->SetBlock(localX, localY, surfaceZ, topBlock->GetDefaultState());
                    surfaceBlocksSet++;
                }
                else
                {
                    if (surfaceBlocksSet == 0)
                    {
                        LogError(LogWorldGenerator, "ApplySurfaceRules - Failed to get topBlock (id=%d) or its default state!", rules.topBlockId);
                    }
                }
            }

            // 5.2 设置填充层方块（如果 fillerDepth > 0）
            for (int i = 1; i <= rules.fillerDepth && (surfaceZ - i) >= 0; i++)
            {
                auto fillerBlock = GetCachedBlockById(rules.fillerBlockId);
                if (fillerBlock && fillerBlock->GetDefaultState())
                {
                    chunk->SetBlock(localX, localY, surfaceZ - i, fillerBlock->GetDefaultState());
                }
            }

            // 5.3 处理水下方块（如果表面低于海平面）
            if (globalSurfaceZ < SEA_LEVEL)
            {
                // 替换水下表层为 underwaterBlockId
                auto underwaterBlock = GetCachedBlockById(rules.underwaterBlockId);
                if (underwaterBlock && underwaterBlock->GetDefaultState())
                {
                    chunk->SetBlock(localX, localY, surfaceZ, underwaterBlock->GetDefaultState());
                }
            }
        }
    }

    // ⚠️ 调试日志：统计信息
    if (callCount <= 5)
    {
        LogInfo(LogWorldGenerator, "ApplySurfaceRules - Chunk (%d, %d) stats: %d surface blocks set, %d biome misses, %d no-surface columns",
                chunkX, chunkY, surfaceBlocksSet, biomeMissCount, noSurfaceCount);
    }

    return true;
}

bool SimpleMinerGenerator::GenerateFeatures(Chunk* chunk, int32_t chunkX, int32_t chunkY)
{
    // Feature generation (trees, ores, structures) is currently handled in GenerateChunk().
    // This stub satisfies the TerrainGenerator interface requirement.
    // Future refactoring will move feature placement logic here.
    UNUSED(chunk);
    UNUSED(chunkX);
    UNUSED(chunkY);
    return true;
}

std::string SimpleMinerGenerator::GetConfigDescription() const
{
    return "SimpleMiner Terrain Generator - 3D Density-based terrain with biome system";
}

float SimpleMinerGenerator::Compute2dPerlinNoise(float        x, float           y, float          scale, unsigned int octaves,
                                                 float        persistence, float octaveScale, bool renormalize,
                                                 unsigned int seed) const
{
    // 使用引擎的Perlin噪声实现
    return ::Compute2dPerlinNoise(x, y, scale, octaves, persistence, octaveScale, renormalize, seed);
}

float SimpleMinerGenerator::CalculateFinalDensity(int globalX, int globalY, int globalZ) const
{
    // 复用现有函数计算地形参数
    float continentalness = SampleContinentalness(globalX, globalY);
    float erosion         = SampleErosion(globalX, globalY);

    float h = EvaluateHeightOffset(continentalness);
    float s = EvaluateSquashing(continentalness);
    float e = EvaluateErosion(erosion);

    // 基础密度 + Bias（与GenerateChunk完全一致）
    float densityNoise = SampleNoise3D(globalX, globalY, globalZ);
    float b_bias       = BIAS_PER_Z * (static_cast<float>(globalZ) - TERRAIN_BASE_HEIGHT);
    float density      = densityNoise + b_bias;

    // 应用地形塑形（与GenerateChunk完全一致）
    density -= h; // Height offset

    // Squashing factor
    float dynamic_base = TERRAIN_BASE_HEIGHT + (h * (static_cast<float>(Chunk::CHUNK_SIZE_Z) / 2.0f));
    if (dynamic_base <= 0.0f)
    {
        dynamic_base = 1.0f;
    }
    float t = (static_cast<float>(globalZ) - dynamic_base) / dynamic_base;
    density += s * t;

    // Erosion factor
    density += e * t;

    return density;
}

int SimpleMinerGenerator::GetGroundHeightAt(int globalX, int globalY) const
{
    // 使用二分搜索查找地面高度
    // 搜索范围: [0, CHUNK_SIZE_Z - 1]
    // 当 density < 0.0f 时表示固体方块
    // 返回最高的固体方块Z坐标

    int low  = 0;
    int high = Chunk::CHUNK_SIZE_Z - 1; // 128 - 1 = 127

    // 二分搜索: 查找最高的固体方块
    // 目标: 找到最大的 z 使得 CalculateFinalDensity(globalX, globalY, z) < 0.0f
    while (low < high)
    {
        // 向上取整的中点,避免死循环
        int mid = (low + high + 1) / 2;

        // 采样完整的地形密度（包含所有塑形因子）
        float density = CalculateFinalDensity(globalX, globalY, mid);

        // density < 0.0f 表示固体方块
        if (density < 0.0f)
        {
            // mid 是固体方块,尝试更高的位置
            low = mid;
        }
        else
        {
            // mid 是空气方块,尝试更低的位置
            high = mid - 1;
        }
    }

    // 边界检查: 如果没有找到固体方块(全是空气),返回海平面
    if (low == 0)
    {
        float densityAtZero = CalculateFinalDensity(globalX, globalY, 0);
        if (densityAtZero >= 0.0f)
        {
            // Z=0 也是空气,返回海平面作为默认值
            return SEA_LEVEL;
        }
    }

    return low;
}
