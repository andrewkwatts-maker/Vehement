#include "SDFBrickCache.hpp"
#include "SDFGPUEvaluator.hpp"
#include <glad/glad.h>
#include <chrono>
#include <cstring>

namespace Nova {

SDFBrickCache::SDFBrickCache() {}

SDFBrickCache::~SDFBrickCache() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool SDFBrickCache::Initialize(const glm::ivec3& atlasSize) {
    if (m_initialized) {
        return true;
    }

    m_atlas.atlasSize = atlasSize;
    m_atlas.totalBricks = atlasSize.x * atlasSize.y * atlasSize.z;
    m_atlas.allocatedBricks = 0;

    // Initialize free slots
    m_atlas.freeSlots.resize(m_atlas.totalBricks, true);

    // Calculate texture size in voxels
    glm::ivec3 texSize = atlasSize * SDFBrick::SIZE;

    // Create 3D texture for distance values
    glGenTextures(1, &m_atlas.texture3D);
    glBindTexture(GL_TEXTURE_3D, m_atlas.texture3D);

    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_R32F,
        texSize.x, texSize.y, texSize.z,
        0,
        GL_RED,
        GL_FLOAT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Create 3D texture for material IDs
    glGenTextures(1, &m_atlas.materialTexture3D);
    glBindTexture(GL_TEXTURE_3D, m_atlas.materialTexture3D);

    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_R16UI,
        texSize.x, texSize.y, texSize.z,
        0,
        GL_RED_INTEGER,
        GL_UNSIGNED_SHORT,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR || m_atlas.texture3D == 0 || m_atlas.materialTexture3D == 0) {
        Shutdown();
        return false;
    }

    m_stats.atlasCapacity = m_atlas.totalBricks;
    UpdateStats();

    m_initialized = true;
    return true;
}

void SDFBrickCache::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_atlas.texture3D != 0) {
        glDeleteTextures(1, &m_atlas.texture3D);
        m_atlas.texture3D = 0;
    }

    if (m_atlas.materialTexture3D != 0) {
        glDeleteTextures(1, &m_atlas.materialTexture3D);
        m_atlas.materialTexture3D = 0;
    }

    m_bricks.clear();
    m_hashToBrick.clear();
    m_freeBrickIDs.clear();
    m_atlas.freeSlots.clear();

    m_initialized = false;
}

// =============================================================================
// Brick Management
// =============================================================================

uint32_t SDFBrickCache::BuildBrick(
    SDFGPUEvaluator* evaluator,
    const glm::vec3& worldMin,
    const glm::vec3& worldMax)
{
    if (!m_initialized || !evaluator) {
        return 0xFFFFFFFF;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Create brick
    SDFBrick brick;
    brick.worldMin = worldMin;
    brick.worldMax = worldMax;

    glm::vec3 voxelSize = (worldMax - worldMin) / float(SDFBrick::SIZE);

    // Evaluate SDF at each voxel
    // NOTE: In a real implementation, this would use GPU compute shader
    // For now, we'll initialize with placeholder values
    for (int z = 0; z < SDFBrick::SIZE; ++z) {
        for (int y = 0; y < SDFBrick::SIZE; ++y) {
            for (int x = 0; x < SDFBrick::SIZE; ++x) {
                int index = x + y * SDFBrick::SIZE + z * SDFBrick::SIZE * SDFBrick::SIZE;

                glm::vec3 voxelPos = worldMin + glm::vec3(x, y, z) * voxelSize + voxelSize * 0.5f;

                // Placeholder: Simple sphere SDF
                float dist = glm::length(voxelPos) - 5.0f;
                brick.distances[index] = dist;
                brick.materials[index] = 0;
            }
        }
    }

    // Calculate hash for deduplication
    brick.hash = CalculateBrickHash(brick);

    // Check if identical brick already exists
    int32_t existingBrickID = FindBrickByHash(brick.hash);
    if (existingBrickID >= 0) {
        // Reuse existing brick
        m_bricks[existingBrickID].refCount++;
        m_stats.dedupedBricks++;
        return existingBrickID;
    }

    // Allocate new brick
    uint32_t brickID;
    if (!m_freeBrickIDs.empty()) {
        brickID = m_freeBrickIDs.back();
        m_freeBrickIDs.pop_back();
        m_bricks[brickID] = brick;
    } else {
        brickID = m_nextBrickID++;
        m_bricks.push_back(brick);
    }

    m_bricks[brickID].refCount = 1;

    // Allocate atlas slot
    uint32_t slotIndex = AllocateBrickSlot();
    if (slotIndex == 0xFFFFFFFF) {
        // Atlas full
        m_freeBrickIDs.push_back(brickID);
        return 0xFFFFFFFF;
    }

    // Upload to GPU
    if (!UploadBrick(slotIndex, brick)) {
        FreeBrickSlot(slotIndex);
        m_freeBrickIDs.push_back(brickID);
        return 0xFFFFFFFF;
    }

    // Store hash mapping
    m_hashToBrick[brick.hash] = brickID;

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    UpdateStats();

    return brickID;
}

std::vector<uint32_t> SDFBrickCache::BuildVolume(
    SDFGPUEvaluator* evaluator,
    const glm::vec3& worldMin,
    const glm::vec3& worldMax,
    uint32_t maxBricks)
{
    std::vector<uint32_t> brickIDs;

    glm::vec3 volumeSize = worldMax - worldMin;
    float brickWorldSize = 2.0f;  // World space size of one brick

    // Calculate grid dimensions
    glm::ivec3 gridSize = glm::ivec3(glm::ceil(volumeSize / brickWorldSize));

    uint32_t totalBricks = gridSize.x * gridSize.y * gridSize.z;
    if (totalBricks > maxBricks) {
        // Too many bricks, increase brick size
        float scaleFactor = std::cbrt(float(totalBricks) / float(maxBricks));
        brickWorldSize *= scaleFactor;
        gridSize = glm::ivec3(glm::ceil(volumeSize / brickWorldSize));
    }

    // Build bricks
    for (int z = 0; z < gridSize.z; ++z) {
        for (int y = 0; y < gridSize.y; ++y) {
            for (int x = 0; x < gridSize.x; ++x) {
                glm::vec3 brickMin = worldMin + glm::vec3(x, y, z) * brickWorldSize;
                glm::vec3 brickMax = glm::min(brickMin + brickWorldSize, worldMax);

                uint32_t brickID = BuildBrick(evaluator, brickMin, brickMax);
                if (brickID != 0xFFFFFFFF) {
                    brickIDs.push_back(brickID);
                }
            }
        }
    }

    return brickIDs;
}

void SDFBrickCache::ReleaseBrick(uint32_t brickID) {
    if (brickID >= m_bricks.size()) {
        return;
    }

    SDFBrick& brick = m_bricks[brickID];
    if (brick.refCount > 0) {
        brick.refCount--;
    }

    if (brick.refCount == 0) {
        // Free brick
        m_hashToBrick.erase(brick.hash);
        m_freeBrickIDs.push_back(brickID);
        UpdateStats();
    }
}

BrickLocation SDFBrickCache::GetBrickLocation(uint32_t brickID) const {
    BrickLocation loc;
    loc.brickIndex = brickID;
    loc.atlasCoord = IndexToAtlasCoord(brickID);
    return loc;
}

const SDFBrick* SDFBrickCache::GetBrick(uint32_t brickID) const {
    if (brickID >= m_bricks.size()) {
        return nullptr;
    }
    return &m_bricks[brickID];
}

void SDFBrickCache::ClearBricks() {
    m_bricks.clear();
    m_hashToBrick.clear();
    m_freeBrickIDs.clear();
    m_nextBrickID = 1;
    m_atlas.allocatedBricks = 0;
    m_atlas.freeSlots.assign(m_atlas.freeSlots.size(), true);
    UpdateStats();
}

// =============================================================================
// Atlas Access
// =============================================================================

void SDFBrickCache::BindAtlas(uint32_t distanceUnit, uint32_t materialUnit) {
    if (!m_initialized) {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + distanceUnit);
    glBindTexture(GL_TEXTURE_3D, m_atlas.texture3D);

    glActiveTexture(GL_TEXTURE0 + materialUnit);
    glBindTexture(GL_TEXTURE_3D, m_atlas.materialTexture3D);

    glActiveTexture(GL_TEXTURE0);
}

// =============================================================================
// Statistics
// =============================================================================

void SDFBrickCache::UpdateStats() {
    m_stats.totalBricks = static_cast<uint32_t>(m_bricks.size());
    m_stats.activeBricks = m_atlas.allocatedBricks;
    m_stats.atlasCapacity = m_atlas.totalBricks;

    if (m_atlas.totalBricks > 0) {
        m_stats.utilizationPercent = (float(m_atlas.allocatedBricks) / float(m_atlas.totalBricks)) * 100.0f;
    } else {
        m_stats.utilizationPercent = 0.0f;
    }

    // Calculate memory usage
    size_t brickMemory = m_bricks.size() * sizeof(SDFBrick);
    size_t atlasMemory = m_atlas.atlasSize.x * m_atlas.atlasSize.y * m_atlas.atlasSize.z;
    atlasMemory *= SDFBrick::TOTAL_VOXELS;
    atlasMemory *= (sizeof(float) + sizeof(uint16_t));  // Distance + material

    m_stats.memoryUsageMB = (brickMemory + atlasMemory) / (1024 * 1024);
}

// =============================================================================
// Private Helper Functions
// =============================================================================

uint32_t SDFBrickCache::AllocateBrickSlot() {
    for (uint32_t i = 0; i < m_atlas.freeSlots.size(); ++i) {
        if (m_atlas.freeSlots[i]) {
            m_atlas.freeSlots[i] = false;
            m_atlas.allocatedBricks++;
            return i;
        }
    }
    return 0xFFFFFFFF;  // No free slots
}

void SDFBrickCache::FreeBrickSlot(uint32_t brickIndex) {
    if (brickIndex < m_atlas.freeSlots.size() && !m_atlas.freeSlots[brickIndex]) {
        m_atlas.freeSlots[brickIndex] = true;
        m_atlas.allocatedBricks--;
    }
}

bool SDFBrickCache::UploadBrick(uint32_t brickIndex, const SDFBrick& brick) {
    glm::ivec3 atlasCoord = IndexToAtlasCoord(brickIndex);
    glm::ivec3 texOffset = atlasCoord * SDFBrick::SIZE;

    // Upload distance values
    glBindTexture(GL_TEXTURE_3D, m_atlas.texture3D);
    glTexSubImage3D(
        GL_TEXTURE_3D,
        0,
        texOffset.x, texOffset.y, texOffset.z,
        SDFBrick::SIZE, SDFBrick::SIZE, SDFBrick::SIZE,
        GL_RED,
        GL_FLOAT,
        brick.distances
    );

    // Upload material values
    glBindTexture(GL_TEXTURE_3D, m_atlas.materialTexture3D);
    glTexSubImage3D(
        GL_TEXTURE_3D,
        0,
        texOffset.x, texOffset.y, texOffset.z,
        SDFBrick::SIZE, SDFBrick::SIZE, SDFBrick::SIZE,
        GL_RED_INTEGER,
        GL_UNSIGNED_SHORT,
        brick.materials
    );

    glBindTexture(GL_TEXTURE_3D, 0);

    return glGetError() == GL_NO_ERROR;
}

uint32_t SDFBrickCache::CalculateBrickHash(const SDFBrick& brick) const {
    // FNV-1a hash
    uint32_t hash = 2166136261u;

    for (int i = 0; i < SDFBrick::TOTAL_VOXELS; ++i) {
        // Hash distance value (quantized to 0.01 units)
        int32_t quantized = static_cast<int32_t>(brick.distances[i] * 100.0f);
        hash ^= quantized;
        hash *= 16777619u;

        // Hash material ID
        hash ^= brick.materials[i];
        hash *= 16777619u;
    }

    return hash;
}

int32_t SDFBrickCache::FindBrickByHash(uint32_t hash) const {
    auto it = m_hashToBrick.find(hash);
    if (it != m_hashToBrick.end()) {
        return it->second;
    }
    return -1;
}

glm::ivec3 SDFBrickCache::IndexToAtlasCoord(uint32_t index) const {
    glm::ivec3 coord;
    coord.x = index % m_atlas.atlasSize.x;
    coord.y = (index / m_atlas.atlasSize.x) % m_atlas.atlasSize.y;
    coord.z = index / (m_atlas.atlasSize.x * m_atlas.atlasSize.y);
    return coord;
}

uint32_t SDFBrickCache::AtlasCoordToIndex(const glm::ivec3& coord) const {
    return coord.x + coord.y * m_atlas.atlasSize.x + coord.z * m_atlas.atlasSize.x * m_atlas.atlasSize.y;
}

} // namespace Nova
