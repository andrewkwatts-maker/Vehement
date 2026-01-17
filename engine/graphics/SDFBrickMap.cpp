#include "SDFBrickMap.hpp"
#include "../sdf/SDFModel.hpp"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace Nova {

// =============================================================================
// BrickData Implementation
// =============================================================================

float BrickData::Sample(const glm::vec3& localPos) const {
    // Trilinear interpolation
    glm::vec3 p = localPos * static_cast<float>(BRICK_SIZE - 1);
    glm::ivec3 p0 = glm::clamp(glm::ivec3(p), glm::ivec3(0), glm::ivec3(BRICK_SIZE - 2));
    glm::ivec3 p1 = p0 + glm::ivec3(1);
    glm::vec3 t = p - glm::vec3(p0);

    float c000 = GetDistance(p0.x, p0.y, p0.z);
    float c100 = GetDistance(p1.x, p0.y, p0.z);
    float c010 = GetDistance(p0.x, p1.y, p0.z);
    float c110 = GetDistance(p1.x, p1.y, p0.z);
    float c001 = GetDistance(p0.x, p0.y, p1.z);
    float c101 = GetDistance(p1.x, p0.y, p1.z);
    float c011 = GetDistance(p0.x, p1.y, p1.z);
    float c111 = GetDistance(p1.x, p1.y, p1.z);

    float c00 = glm::mix(c000, c100, t.x);
    float c01 = glm::mix(c001, c101, t.x);
    float c10 = glm::mix(c010, c110, t.x);
    float c11 = glm::mix(c011, c111, t.x);

    float c0 = glm::mix(c00, c10, t.y);
    float c1 = glm::mix(c01, c11, t.y);

    return glm::mix(c0, c1, t.z);
}

uint64_t BrickData::ComputeHash() const {
    // Simple hash of distance values (quantized for compression tolerance)
    uint64_t hash = 0;
    constexpr float quantization = 0.01f;

    for (size_t i = 0; i < distanceField.size(); i += 4) {  // Sample every 4th value
        int quantized = static_cast<int>(distanceField[i] / quantization);
        hash = hash * 31 + static_cast<uint64_t>(quantized);
    }

    return hash;
}

// =============================================================================
// SDFBrickMap Implementation
// =============================================================================

SDFBrickMap::SDFBrickMap() = default;

SDFBrickMap::~SDFBrickMap() {
    if (m_gpuTexture) {
        glDeleteTextures(1, &m_gpuTexture);
    }
    if (m_gpuBuffer) {
        glDeleteBuffers(1, &m_gpuBuffer);
    }
    if (m_gpuIndexTexture) {
        glDeleteTextures(1, &m_gpuIndexTexture);
    }
}

void SDFBrickMap::Build(const SDFModel& model, const BrickMapSettings& settings) {
    auto [minBounds, maxBounds] = model.GetBounds();

    auto sdfFunc = [&model](const glm::vec3& pos) {
        return model.EvaluateSDF(pos);
    };

    Build(sdfFunc, minBounds, maxBounds, settings);
}

void SDFBrickMap::Build(const std::function<float(const glm::vec3&)>& sdfFunc,
                         const glm::vec3& boundsMin,
                         const glm::vec3& boundsMax,
                         const BrickMapSettings& settings) {
    auto startTime = std::chrono::high_resolution_clock::now();

    Clear();
    m_boundsMin = boundsMin;
    m_boundsMax = boundsMax;
    m_settings = settings;

    // Allocate bricks
    AllocateBricks(boundsMin, boundsMax);

    // Fill each brick with distance field
    for (auto& [index, brick] : m_bricks) {
        FillBrick(brick, sdfFunc);
    }

    // Compress duplicate bricks
    if (m_settings.enableCompression) {
        CompressBricks();
    }

    ComputeStats();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    InvalidateGPU();
}

void SDFBrickMap::AllocateBricks(const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    glm::vec3 size = boundsMax - boundsMin;
    float brickWorldSize = m_settings.worldVoxelSize * m_settings.brickResolution;

    m_brickGridSize = glm::ivec3(glm::ceil(size / brickWorldSize));

    // Create bricks
    for (int z = 0; z < m_brickGridSize.z; ++z) {
        for (int y = 0; y < m_brickGridSize.y; ++y) {
            for (int x = 0; x < m_brickGridSize.x; ++x) {
                BrickIndex index{x, y, z};
                BrickData brick;

                glm::vec3 brickMin = boundsMin + glm::vec3(x, y, z) * brickWorldSize;
                glm::vec3 brickMax = brickMin + glm::vec3(brickWorldSize);

                brick.worldMin = brickMin;
                brick.worldMax = brickMax;
                brick.brickId = static_cast<uint32_t>(m_bricks.size());

                m_bricks[index] = brick;
            }
        }
    }
}

void SDFBrickMap::FillBrick(BrickData& brick,
                             const std::function<float(const glm::vec3&)>& sdfFunc) {
    glm::vec3 brickSize = brick.worldMax - brick.worldMin;
    glm::vec3 voxelSize = brickSize / static_cast<float>(BrickData::BRICK_SIZE);

    for (int z = 0; z < BrickData::BRICK_SIZE; ++z) {
        for (int y = 0; y < BrickData::BRICK_SIZE; ++y) {
            for (int x = 0; x < BrickData::BRICK_SIZE; ++x) {
                glm::vec3 worldPos = brick.worldMin +
                                     glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f) * voxelSize;
                float distance = sdfFunc(worldPos);
                brick.SetDistance(x, y, z, distance);
            }
        }
    }

    brick.isDirty = false;
}

void SDFBrickMap::CompressBricks() {
    m_compressionMap.clear();
    m_stats.compressedBricks = 0;

    std::vector<std::pair<BrickIndex, uint64_t>> brickHashes;

    // Compute hashes for all bricks
    for (auto& [index, brick] : m_bricks) {
        uint64_t hash = brick.ComputeHash();
        brickHashes.push_back({index, hash});
    }

    // Group by hash
    std::sort(brickHashes.begin(), brickHashes.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    // Find duplicates
    for (size_t i = 0; i < brickHashes.size(); ++i) {
        auto& [index, hash] = brickHashes[i];
        auto& brick = m_bricks[index];

        if (m_compressionMap.find(hash) == m_compressionMap.end()) {
            // First brick with this hash
            m_compressionMap[hash] = brick.brickId;
        } else {
            // Duplicate - reference canonical brick
            brick.compressionId = m_compressionMap[hash];
            brick.isCompressed = true;
            m_stats.compressedBricks++;
        }
    }
}

void SDFBrickMap::UpdateDirtyBricks(const std::function<float(const glm::vec3&)>& sdfFunc) {
    for (auto& [index, brick] : m_bricks) {
        if (brick.isDirty) {
            FillBrick(brick, sdfFunc);
        }
    }

    InvalidateGPU();
}

void SDFBrickMap::MarkRegionDirty(const glm::vec3& min, const glm::vec3& max) {
    BrickIndex minIndex = WorldToBrickIndex(min);
    BrickIndex maxIndex = WorldToBrickIndex(max);

    for (int z = minIndex.z; z <= maxIndex.z; ++z) {
        for (int y = minIndex.y; y <= maxIndex.y; ++y) {
            for (int x = minIndex.x; x <= maxIndex.x; ++x) {
                BrickIndex index{x, y, z};
                auto it = m_bricks.find(index);
                if (it != m_bricks.end()) {
                    it->second.isDirty = true;
                    m_stats.dirtyBricks++;
                }
            }
        }
    }
}

void SDFBrickMap::Clear() {
    m_bricks.clear();
    m_compressionMap.clear();
    m_stats = {};
    InvalidateGPU();
}

float SDFBrickMap::SampleDistance(const glm::vec3& worldPos) const {
    const BrickData* brick = GetBrickAt(worldPos);
    if (!brick) {
        return FLT_MAX;
    }

    glm::vec3 localPos = WorldToLocalBrick(worldPos, *brick);
    return brick->Sample(localPos);
}

const BrickData* SDFBrickMap::GetBrickAt(const glm::vec3& worldPos) const {
    BrickIndex index = WorldToBrickIndex(worldPos);
    return GetBrick(index);
}

const BrickData* SDFBrickMap::GetBrick(const BrickIndex& index) const {
    auto it = m_bricks.find(index);
    if (it != m_bricks.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SDFBrickMap::IsCached(const glm::vec3& worldPos) const {
    return GetBrickAt(worldPos) != nullptr;
}

BrickIndex SDFBrickMap::WorldToBrickIndex(const glm::vec3& worldPos) const {
    float brickWorldSize = m_settings.worldVoxelSize * m_settings.brickResolution;
    glm::vec3 offset = worldPos - m_boundsMin;
    glm::ivec3 index = glm::ivec3(glm::floor(offset / brickWorldSize));

    return BrickIndex{index.x, index.y, index.z};
}

glm::vec3 SDFBrickMap::BrickIndexToWorld(const BrickIndex& index) const {
    float brickWorldSize = m_settings.worldVoxelSize * m_settings.brickResolution;
    return m_boundsMin + glm::vec3(index.x, index.y, index.z) * brickWorldSize;
}

glm::vec3 SDFBrickMap::WorldToLocalBrick(const glm::vec3& worldPos,
                                          const BrickData& brick) const {
    glm::vec3 offset = worldPos - brick.worldMin;
    glm::vec3 brickSize = brick.worldMax - brick.worldMin;
    return offset / brickSize;
}

unsigned int SDFBrickMap::UploadToGPU() {
    if (m_bricks.empty()) {
        return 0;
    }

    // Create 3D texture array - each layer is a brick
    int numBricks = static_cast<int>(m_bricks.size());
    int brickSize = BrickData::BRICK_SIZE;

    std::vector<float> textureData;
    textureData.reserve(numBricks * BrickData::BRICK_VOXELS);

    // Pack all bricks into texture array
    for (const auto& [index, brick] : m_bricks) {
        if (!brick.isCompressed) {
            textureData.insert(textureData.end(),
                               brick.distanceField.begin(),
                               brick.distanceField.end());
        }
    }

    if (!m_gpuTexture) {
        glGenTextures(1, &m_gpuTexture);
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_gpuTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F,
                 brickSize, brickSize, numBricks * brickSize,
                 0, GL_RED, GL_FLOAT, textureData.data());

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    m_gpuValid = true;
    return m_gpuTexture;
}

unsigned int SDFBrickMap::UploadToGPUBuffer() {
    if (m_bricks.empty()) {
        return 0;
    }

    std::vector<float> bufferData;
    for (const auto& [index, brick] : m_bricks) {
        if (!brick.isCompressed) {
            bufferData.insert(bufferData.end(),
                              brick.distanceField.begin(),
                              brick.distanceField.end());
        }
    }

    if (!m_gpuBuffer) {
        glGenBuffers(1, &m_gpuBuffer);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 bufferData.size() * sizeof(float),
                 bufferData.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_gpuValid = true;
    return m_gpuBuffer;
}

void SDFBrickMap::UpdateGPU() {
    // Update only dirty bricks
    if (!m_gpuValid || m_stats.dirtyBricks == 0) {
        return;
    }

    // For simplicity, re-upload all
    UploadToGPU();
}

size_t SDFBrickMap::GetMemoryUsage() const {
    size_t total = 0;
    for (const auto& [index, brick] : m_bricks) {
        if (!brick.isCompressed) {
            total += brick.distanceField.size() * sizeof(float);
        }
    }
    return total;
}

void SDFBrickMap::ComputeStats() {
    m_stats = {};
    m_stats.totalBricks = static_cast<int>(m_bricks.size());

    for (const auto& [index, brick] : m_bricks) {
        if (!brick.isCompressed) {
            m_stats.uniqueBricks++;
        }
        if (brick.isDirty) {
            m_stats.dirtyBricks++;
        }
    }

    m_stats.memoryBytes = GetMemoryUsage();
    m_stats.memoryBytesCompressed = m_stats.memoryBytes;

    if (m_stats.totalBricks > 0) {
        m_stats.compressionRatio = static_cast<float>(m_stats.uniqueBricks) /
                                    static_cast<float>(m_stats.totalBricks);
    }
}

// =============================================================================
// BrickMapUtil Implementation
// =============================================================================

namespace BrickMapUtil {

float TrilinearSample(const BrickData& brick, const glm::vec3& localPos) {
    return brick.Sample(localPos);
}

void ComputeBrickBounds(const BrickIndex& index, float brickWorldSize,
                        const glm::vec3& gridOrigin,
                        glm::vec3& outMin, glm::vec3& outMax) {
    outMin = gridOrigin + glm::vec3(index.x, index.y, index.z) * brickWorldSize;
    outMax = outMin + glm::vec3(brickWorldSize);
}

bool CompareBricks(const BrickData& a, const BrickData& b, float threshold) {
    if (a.distanceField.size() != b.distanceField.size()) {
        return false;
    }

    for (size_t i = 0; i < a.distanceField.size(); ++i) {
        if (std::abs(a.distanceField[i] - b.distanceField[i]) > threshold) {
            return false;
        }
    }

    return true;
}

float EstimateOptimalBrickSize(const SDFModel& model) {
    auto [minBounds, maxBounds] = model.GetBounds();
    glm::vec3 size = maxBounds - minBounds;
    float maxSize = std::max(std::max(size.x, size.y), size.z);

    // Aim for 32-64 bricks along longest axis
    return maxSize / 48.0f;
}

} // namespace BrickMapUtil

} // namespace Nova
