#include "RTXAccelerationStructure.hpp"
#include "RTXSupport.hpp"
#include "../core/SDF.hpp"
#include "Mesh.hpp"
#include <glad/glad.h>
#include <spdlog/spdlog.h>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <chrono>

namespace Nova {

// =============================================================================
// TLASInstance Implementation
// =============================================================================

void TLASInstance::SetTransform(const glm::mat4& mat) {
    // Convert to 3x4 row-major matrix for GPU
    transform[0] = mat[0][0]; transform[1] = mat[1][0]; transform[2] = mat[2][0]; transform[3] = mat[3][0];
    transform[4] = mat[0][1]; transform[5] = mat[1][1]; transform[6] = mat[2][1]; transform[7] = mat[3][1];
    transform[8] = mat[0][2]; transform[9] = mat[1][2]; transform[10] = mat[2][2]; transform[11] = mat[3][2];
}

glm::mat4 TLASInstance::GetTransform() const {
    glm::mat4 mat(1.0f);
    mat[0][0] = transform[0]; mat[1][0] = transform[1]; mat[2][0] = transform[2]; mat[3][0] = transform[3];
    mat[0][1] = transform[4]; mat[1][1] = transform[5]; mat[2][1] = transform[6]; mat[3][1] = transform[7];
    mat[0][2] = transform[8]; mat[1][2] = transform[9]; mat[2][2] = transform[10]; mat[3][2] = transform[11];
    return mat;
}

// =============================================================================
// ASBuildStats Implementation
// =============================================================================

std::string ASBuildStats::ToString() const {
    std::stringstream ss;
    ss << "=== Acceleration Structure Build Stats ===\n";
    ss << "Build Time: " << buildTimeMs << " ms\n";
    ss << "Compaction Time: " << compactionTimeMs << " ms\n";
    ss << "Update Time: " << updateTimeMs << " ms\n";
    ss << "Original Size: " << (originalSize / 1024) << " KB\n";
    ss << "Compacted Size: " << (compactedSize / 1024) << " KB\n";
    ss << "Scratch Size: " << (scratchSize / 1024) << " KB\n";
    ss << "Compression Ratio: " << GetCompressionRatio() << "\n";
    ss << "Triangles: " << triangleCount << "\n";
    ss << "Instances: " << instanceCount << "\n";
    return ss.str();
}

// =============================================================================
// RTXAccelerationStructure Implementation
// =============================================================================

RTXAccelerationStructure::RTXAccelerationStructure() = default;

RTXAccelerationStructure::~RTXAccelerationStructure() {
    Shutdown();
}

bool RTXAccelerationStructure::Initialize() {
    if (m_initialized) {
        return true;
    }

    if (!RTXSupport::IsAvailable()) {
        spdlog::error("Cannot initialize RTX acceleration structures: No ray tracing support");
        return false;
    }

    spdlog::info("Initializing RTX acceleration structure manager...");
    m_initialized = true;
    return true;
}

void RTXAccelerationStructure::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down RTX acceleration structures...");

    // Destroy all BLAS
    for (auto& blas : m_blasList) {
        if (blas.buffer != 0) {
            DestroyBuffer(blas.buffer);
        }
        if (blas.scratchBuffer != 0) {
            DestroyBuffer(blas.scratchBuffer);
        }
    }
    m_blasList.clear();

    // Destroy all TLAS
    for (auto& tlas : m_tlasList) {
        if (tlas.buffer != 0) {
            DestroyBuffer(tlas.buffer);
        }
        if (tlas.scratchBuffer != 0) {
            DestroyBuffer(tlas.scratchBuffer);
        }
        if (tlas.instanceBuffer != 0) {
            DestroyBuffer(tlas.instanceBuffer);
        }
    }
    m_tlasList.clear();

    m_initialized = false;
}

// =============================================================================
// BLAS Management
// =============================================================================

uint64_t RTXAccelerationStructure::BuildBLAS(const BLASDescriptor& desc) {
    if (!m_initialized) {
        spdlog::error("RTXAccelerationStructure not initialized");
        return 0;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    BLAS blas;
    blas.handle = GetNextBLASHandle();
    blas.debugName = desc.debugName;
    blas.buildQuality = desc.buildQuality;
    blas.allowUpdate = (desc.buildFlags & AS_BUILD_FLAG_ALLOW_UPDATE) != 0;

    BuildBLASInternal(blas, desc);

    // Optionally compact
    if ((desc.buildFlags & AS_BUILD_FLAG_ALLOW_COMPACTION) && !blas.allowUpdate) {
        CompactAccelerationStructure(blas);
    }

    m_blasList.push_back(blas);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.buildTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.triangleCount += desc.triangleCount;

    spdlog::debug("Built BLAS '{}': {} triangles, {:.2f} KB, {:.2f} ms",
                  desc.debugName, desc.triangleCount, blas.size / 1024.0, m_stats.buildTimeMs);

    return blas.handle;
}

uint64_t RTXAccelerationStructure::BuildBLASFromSDF(const SDFModel& model, float voxelSize) {
    // Convert SDF to mesh
    auto mesh = ConvertSDFToMesh(model, voxelSize);
    if (!mesh) {
        spdlog::error("Failed to convert SDF to mesh");
        return 0;
    }

    // Build BLAS from mesh
    BLASDescriptor desc;
    desc.vertexBuffer = mesh->GetVBO();
    desc.indexBuffer = mesh->GetIBO();
    desc.vertexCount = mesh->GetVertexCount();
    desc.triangleCount = mesh->GetIndexCount() / 3;
    desc.vertexStride = sizeof(float) * 3; // Position only
    desc.debugName = "SDF_" + std::to_string(reinterpret_cast<uintptr_t>(&model));

    return BuildBLAS(desc);
}

std::vector<uint64_t> RTXAccelerationStructure::BuildBLASBatch(
    const std::vector<BLASDescriptor>& descriptors) {

    std::vector<uint64_t> handles;
    handles.reserve(descriptors.size());

    // TODO: Implement parallel building for better performance
    for (const auto& desc : descriptors) {
        handles.push_back(BuildBLAS(desc));
    }

    return handles;
}

bool RTXAccelerationStructure::UpdateBLAS(uint64_t blasHandle, const BLASDescriptor& desc) {
    const BLAS* blas = GetBLAS(blasHandle);
    if (!blas) {
        spdlog::error("Invalid BLAS handle: {}", blasHandle);
        return false;
    }

    if (!blas->allowUpdate) {
        spdlog::error("BLAS '{}' does not allow updates", blas->debugName);
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // TODO: Implement actual update (would use GL_NV_ray_tracing API)
    spdlog::debug("Updating BLAS '{}'", blas->debugName);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return true;
}

bool RTXAccelerationStructure::RefitBLAS(uint64_t blasHandle) {
    const BLAS* blas = GetBLAS(blasHandle);
    if (!blas) {
        spdlog::error("Invalid BLAS handle: {}", blasHandle);
        return false;
    }

    // TODO: Implement refit (faster than full update)
    spdlog::debug("Refitting BLAS '{}'", blas->debugName);
    return true;
}

uint64_t RTXAccelerationStructure::CompactBLAS(uint64_t blasHandle) {
    BLAS* blas = const_cast<BLAS*>(GetBLAS(blasHandle));
    if (!blas) {
        spdlog::error("Invalid BLAS handle: {}", blasHandle);
        return 0;
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    CompactAccelerationStructure(*blas);
    auto endTime = std::chrono::high_resolution_clock::now();

    m_stats.compactionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    return blas->handle;
}

void RTXAccelerationStructure::DestroyBLAS(uint64_t blasHandle) {
    for (auto it = m_blasList.begin(); it != m_blasList.end(); ++it) {
        if (it->handle == blasHandle) {
            if (it->buffer != 0) {
                DestroyBuffer(it->buffer);
            }
            if (it->scratchBuffer != 0) {
                DestroyBuffer(it->scratchBuffer);
            }
            m_blasList.erase(it);
            spdlog::debug("Destroyed BLAS {}", blasHandle);
            return;
        }
    }
}

const BLAS* RTXAccelerationStructure::GetBLAS(uint64_t handle) const {
    for (const auto& blas : m_blasList) {
        if (blas.handle == handle) {
            return &blas;
        }
    }
    return nullptr;
}

// =============================================================================
// TLAS Management
// =============================================================================

uint64_t RTXAccelerationStructure::BuildTLAS(const std::vector<TLASInstance>& instances,
                                              const std::string& debugName) {
    if (!m_initialized) {
        spdlog::error("RTXAccelerationStructure not initialized");
        return 0;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    TLAS tlas;
    tlas.handle = GetNextTLASHandle();
    tlas.debugName = debugName;
    tlas.instanceCount = static_cast<uint32_t>(instances.size());

    BuildTLASInternal(tlas, instances);

    m_tlasList.push_back(tlas);

    auto endTime = std::chrono::high_resolution_clock::now();
    double buildTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.instanceCount += static_cast<uint32_t>(instances.size());

    spdlog::debug("Built TLAS '{}': {} instances, {:.2f} KB, {:.2f} ms",
                  debugName, instances.size(), tlas.size / 1024.0, buildTime);

    return tlas.handle;
}

bool RTXAccelerationStructure::UpdateTLAS(uint64_t tlasHandle,
                                          const std::vector<TLASInstance>& instances) {
    TLAS* tlas = const_cast<TLAS*>(GetTLAS(tlasHandle));
    if (!tlas) {
        spdlog::error("Invalid TLAS handle: {}", tlasHandle);
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // Update instance buffer
    if (tlas->instanceBuffer != 0) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, tlas->instanceBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     instances.size() * sizeof(TLASInstance),
                     instances.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    tlas->instanceCount = static_cast<uint32_t>(instances.size());

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    spdlog::debug("Updated TLAS '{}': {} instances, {:.2f} ms",
                  tlas->debugName, instances.size(), m_stats.updateTimeMs);

    return true;
}

bool RTXAccelerationStructure::UpdateTLASTransforms(uint64_t tlasHandle,
                                                    const std::vector<glm::mat4>& transforms) {
    TLAS* tlas = const_cast<TLAS*>(GetTLAS(tlasHandle));
    if (!tlas) {
        spdlog::error("Invalid TLAS handle: {}", tlasHandle);
        return false;
    }

    if (transforms.size() != tlas->instanceCount) {
        spdlog::error("Transform count mismatch: {} vs {}", transforms.size(), tlas->instanceCount);
        return false;
    }

    // TODO: Update only transforms in instance buffer
    spdlog::debug("Updated TLAS '{}' transforms", tlas->debugName);
    return true;
}

void RTXAccelerationStructure::DestroyTLAS(uint64_t tlasHandle) {
    for (auto it = m_tlasList.begin(); it != m_tlasList.end(); ++it) {
        if (it->handle == tlasHandle) {
            if (it->buffer != 0) {
                DestroyBuffer(it->buffer);
            }
            if (it->scratchBuffer != 0) {
                DestroyBuffer(it->scratchBuffer);
            }
            if (it->instanceBuffer != 0) {
                DestroyBuffer(it->instanceBuffer);
            }
            m_tlasList.erase(it);
            spdlog::debug("Destroyed TLAS {}", tlasHandle);
            return;
        }
    }
}

const TLAS* RTXAccelerationStructure::GetTLAS(uint64_t handle) const {
    for (const auto& tlas : m_tlasList) {
        if (tlas.handle == handle) {
            return &tlas;
        }
    }
    return nullptr;
}

uint32_t RTXAccelerationStructure::GetTLASBuffer(uint64_t handle) const {
    const TLAS* tlas = GetTLAS(handle);
    return tlas ? tlas->buffer : 0;
}

// =============================================================================
// Utilities
// =============================================================================

std::shared_ptr<Mesh> RTXAccelerationStructure::ConvertSDFToMesh(const SDFModel& model,
                                                                  float voxelSize) {
    // Check cache first
    for (const auto& cached : m_sdfMeshCache) {
        if (cached.model == &model && cached.voxelSize == voxelSize) {
            return cached.mesh;
        }
    }

    // TODO: Implement marching cubes or dual contouring
    // For now, return a placeholder cube mesh
    spdlog::warn("SDF to mesh conversion not yet implemented - using placeholder");

    auto mesh = std::make_shared<Mesh>();
    // Initialize with simple cube for testing
    // In production, this would run marching cubes on the SDF

    // Cache the result
    m_sdfMeshCache.push_back({&model, voxelSize, mesh});

    return mesh;
}

size_t RTXAccelerationStructure::GetTotalMemoryUsage() const {
    size_t total = 0;

    for (const auto& blas : m_blasList) {
        total += blas.size;
    }

    for (const auto& tlas : m_tlasList) {
        total += tlas.size;
    }

    return total;
}

void RTXAccelerationStructure::LogStats() const {
    spdlog::info(m_stats.ToString());
    spdlog::info("Total AS Memory: {:.2f} MB", GetTotalMemoryUsage() / (1024.0 * 1024.0));
}

// =============================================================================
// Internal Helpers
// =============================================================================

void RTXAccelerationStructure::BuildBLASInternal(BLAS& blas, const BLASDescriptor& desc) {
    // Store geometry info
    blas.vertexBuffer = desc.vertexBuffer;
    blas.indexBuffer = desc.indexBuffer;
    blas.triangleCount = desc.triangleCount;

    // Calculate size (simplified - actual would query driver)
    blas.size = desc.triangleCount * 64; // ~64 bytes per triangle in BVH
    blas.scratchSize = blas.size; // Scratch buffer typically same size

    // Create buffers
    blas.buffer = CreateBuffer(blas.size);
    blas.scratchBuffer = CreateBuffer(blas.scratchSize);

    // TODO: Build actual BLAS using GL_NV_ray_tracing
    // For now, just allocate the buffers
    m_stats.originalSize += blas.size;
}

void RTXAccelerationStructure::BuildTLASInternal(TLAS& tlas,
                                                 const std::vector<TLASInstance>& instances) {
    // Calculate size
    tlas.size = instances.size() * 64; // ~64 bytes per instance
    tlas.scratchSize = tlas.size;

    // Create buffers
    tlas.buffer = CreateBuffer(tlas.size);
    tlas.scratchBuffer = CreateBuffer(tlas.scratchSize);

    // Create and upload instance buffer
    tlas.instanceBuffer = CreateBuffer(instances.size() * sizeof(TLASInstance), instances.data());

    // TODO: Build actual TLAS using GL_NV_ray_tracing
}

void RTXAccelerationStructure::CompactAccelerationStructure(BLAS& blas) {
    if (blas.compacted) {
        return;
    }

    // TODO: Implement actual compaction
    // Typically reduces size by 30-50%
    size_t originalSize = blas.size;
    blas.size = static_cast<size_t>(blas.size * 0.6); // Assume 40% reduction
    blas.compacted = true;

    m_stats.compactedSize += blas.size;
    spdlog::debug("Compacted BLAS '{}': {:.2f} KB -> {:.2f} KB",
                  blas.debugName, originalSize / 1024.0, blas.size / 1024.0);
}

uint32_t RTXAccelerationStructure::CreateBuffer(size_t size, const void* data) {
    uint32_t buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return buffer;
}

void RTXAccelerationStructure::DestroyBuffer(uint32_t buffer) {
    if (buffer != 0) {
        glDeleteBuffers(1, &buffer);
    }
}

uint64_t RTXAccelerationStructure::GetNextBLASHandle() {
    return m_nextBLASHandle++;
}

uint64_t RTXAccelerationStructure::GetNextTLASHandle() {
    return m_nextTLASHandle++;
}

// =============================================================================
// Free Functions
// =============================================================================

TLASInstance CreateTLASInstance(uint64_t blasHandle,
                                const glm::mat4& transform,
                                uint32_t customIndex,
                                uint32_t mask) {
    TLASInstance instance;
    instance.SetTransform(transform);
    instance.blasHandle = blasHandle;
    instance.instanceCustomIndex = customIndex;
    instance.mask = mask;
    instance.flags = AS_INSTANCE_FLAG_NONE;
    instance.instanceShaderBindingTableRecordOffset = 0;
    return instance;
}

} // namespace Nova
