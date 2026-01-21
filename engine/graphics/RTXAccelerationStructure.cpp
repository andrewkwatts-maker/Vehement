#include "RTXAccelerationStructure.hpp"
#include "RTXSupport.hpp"
#include "../core/SDF.hpp"
#include "Mesh.hpp"
#include <glad/gl.h>
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

    // Parallel BLAS building would require:
    // 1. GL_NV_ray_tracing parallel build support or worker threads with shared context
    // 2. Multiple scratch buffers (one per concurrent build)
    // 3. Synchronization primitives for completion tracking
    // For now, build sequentially - this is still fast for most use cases
    spdlog::debug("Building {} BLAS sequentially (parallel build not yet implemented)",
                  descriptors.size());

    auto batchStartTime = std::chrono::high_resolution_clock::now();

    for (const auto& desc : descriptors) {
        handles.push_back(BuildBLAS(desc));
    }

    auto batchEndTime = std::chrono::high_resolution_clock::now();
    double batchTimeMs = std::chrono::duration<double, std::milli>(batchEndTime - batchStartTime).count();

    spdlog::debug("Batch BLAS build complete: {} structures in {:.2f} ms ({:.2f} ms/structure)",
                  descriptors.size(), batchTimeMs,
                  descriptors.empty() ? 0.0 : batchTimeMs / descriptors.size());

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

    // BLAS update stub implementation
    // Full implementation with GL_NV_ray_tracing would:
    // 1. Bind the existing acceleration structure
    // 2. Call glBuildAccelerationStructureNV with VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
    // 3. Use the same scratch buffer (must be preserved from build)
    // 4. Wait for completion with memory barrier

    spdlog::debug("UpdateBLAS '{}': Stub implementation - geometry update simulated", blas->debugName);
    spdlog::debug("  Vertex buffer: {}, Index buffer: {}, Triangles: {}",
                  desc.vertexBuffer, desc.indexBuffer, desc.triangleCount);

    // In a real implementation, we would re-upload vertex data and rebuild
    // For now, just update the stored descriptor info
    // Note: The actual BLAS data would need to be rebuilt with new geometry

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    spdlog::debug("  Update completed in {:.3f} ms", m_stats.updateTimeMs);
    return true;
}

bool RTXAccelerationStructure::RefitBLAS(uint64_t blasHandle) {
    const BLAS* blas = GetBLAS(blasHandle);
    if (!blas) {
        spdlog::error("Invalid BLAS handle: {}", blasHandle);
        return false;
    }

    if (!blas->allowUpdate) {
        spdlog::error("BLAS '{}' does not allow refit (was not built with ALLOW_UPDATE flag)",
                      blas->debugName);
        return false;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // BLAS refit stub implementation
    // Refit is faster than full update because it only adjusts bounding boxes
    // without rebuilding the BVH hierarchy. Use for:
    // - Skinned/animated meshes where topology doesn't change
    // - Slight vertex position changes (cloth, soft body)
    //
    // Full implementation with GL_NV_ray_tracing would:
    // 1. Bind the existing BLAS
    // 2. Call glBuildAccelerationStructureNV with refit flag
    // 3. This updates AABBs in-place without restructuring the tree

    spdlog::debug("RefitBLAS '{}': Stub implementation - bounds refit simulated", blas->debugName);
    spdlog::debug("  Triangle count: {}, Current size: {} KB",
                  blas->triangleCount, blas->size / 1024);

    // In production, the driver would update internal AABB nodes
    // Refit is typically 5-10x faster than full rebuild

    auto endTime = std::chrono::high_resolution_clock::now();
    double refitTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    spdlog::debug("  Refit completed in {:.3f} ms", refitTimeMs);
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

    auto startTime = std::chrono::high_resolution_clock::now();

    // Update only the transform portion of each instance in the instance buffer
    // This is the fastest TLAS update path - use when only object positions/rotations change
    //
    // Full implementation would:
    // 1. Map the instance buffer
    // 2. Update only the 3x4 transform matrix for each instance
    // 3. Unmap and rebuild TLAS with the updated instances

    if (tlas->instanceBuffer != 0) {
        // Map buffer and update transforms
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, tlas->instanceBuffer);

        // For each instance, update only the transform (first 12 floats)
        for (size_t i = 0; i < transforms.size(); ++i) {
            // Convert glm::mat4 to 3x4 row-major format
            float transformData[12];
            const glm::mat4& mat = transforms[i];
            transformData[0] = mat[0][0]; transformData[1] = mat[1][0]; transformData[2] = mat[2][0]; transformData[3] = mat[3][0];
            transformData[4] = mat[0][1]; transformData[5] = mat[1][1]; transformData[6] = mat[2][1]; transformData[7] = mat[3][1];
            transformData[8] = mat[0][2]; transformData[9] = mat[1][2]; transformData[10] = mat[2][2]; transformData[11] = mat[3][2];

            // Update transform portion of instance i
            glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                           i * sizeof(TLASInstance),  // Offset to instance i
                           sizeof(transformData),     // Size of transform data (12 floats)
                           transformData);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double updateTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    spdlog::debug("UpdateTLASTransforms '{}': Updated {} instance transforms in {:.3f} ms",
                  tlas->debugName, transforms.size(), updateTimeMs);

    // Note: After updating transforms, the TLAS itself may need to be rebuilt
    // for ray tracing to use the new positions. This stub updates the buffer
    // but actual TLAS rebuild would require GL_NV_ray_tracing API calls.

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
            spdlog::debug("ConvertSDFToMesh: Using cached mesh for SDF model");
            return cached.mesh;
        }
    }

    // SDF to mesh conversion stub implementation
    // Full implementation would use one of these algorithms:
    // 1. Marching Cubes - fast, produces watertight meshes, but can have sharp feature loss
    // 2. Dual Contouring - preserves sharp features, requires hermite data
    // 3. Surface Nets - good balance of quality and speed
    //
    // Algorithm outline (Marching Cubes):
    // 1. Sample SDF on a 3D grid with spacing = voxelSize
    // 2. For each cube cell, determine which of 256 configurations applies
    // 3. Generate triangles based on lookup table
    // 4. Optionally: run mesh simplification to reduce triangle count

    spdlog::warn("SDF to mesh conversion not yet implemented - returning empty placeholder mesh");
    spdlog::info("  To implement: Add marching cubes algorithm from engine/core/SDF.hpp");
    spdlog::info("  Voxel size: {:.4f}, estimated grid resolution: ~{}^3",
                 voxelSize, static_cast<int>(1.0f / voxelSize));

    auto mesh = std::make_shared<Mesh>();

    // In production, this would:
    // 1. Call model.Sample(position) to get SDF values at grid points
    // 2. Run marching cubes to generate vertices and indices
    // 3. Upload to GPU buffers via mesh->SetVertexData() and mesh->SetIndexData()

    // For now, create an empty mesh that won't crash when used
    // This allows the rest of the ray tracing pipeline to function

    // Cache the result (even the placeholder, to avoid repeated warnings)
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

    // Calculate size (simplified - actual would query driver via glGetAccelerationStructureMemoryRequirementsNV)
    // BVH typically uses ~64 bytes per triangle, but this varies by build quality:
    // - Fast build: ~48-56 bytes/triangle (shallower tree)
    // - High quality: ~72-96 bytes/triangle (deeper tree, better ray tracing perf)
    size_t bytesPerTriangle = 64;
    switch (desc.buildQuality) {
        case ASBuildQuality::Fast:
            bytesPerTriangle = 48;
            break;
        case ASBuildQuality::Balanced:
            bytesPerTriangle = 64;
            break;
        case ASBuildQuality::HighQuality:
            bytesPerTriangle = 80;
            break;
    }

    blas.size = desc.triangleCount * bytesPerTriangle;
    blas.scratchSize = blas.size; // Scratch buffer typically same size as output

    // Create buffers for BLAS storage
    blas.buffer = CreateBuffer(blas.size);
    blas.scratchBuffer = CreateBuffer(blas.scratchSize);

    // BLAS build stub implementation
    // Full implementation with GL_NV_ray_tracing would:
    // 1. Create VkAccelerationStructureGeometryKHR describing the geometry
    // 2. Call vkGetAccelerationStructureBuildSizesKHR for exact size
    // 3. Allocate buffers based on returned sizes
    // 4. Call vkCmdBuildAccelerationStructuresKHR to build the BLAS
    // 5. Insert memory barrier before use

    spdlog::debug("BuildBLASInternal: Created stub BLAS (awaiting GL_NV_ray_tracing implementation)");
    spdlog::debug("  Triangles: {}, Estimated size: {:.2f} KB, Scratch: {:.2f} KB",
                  desc.triangleCount, blas.size / 1024.0, blas.scratchSize / 1024.0);

    m_stats.originalSize += blas.size;
}

void RTXAccelerationStructure::BuildTLASInternal(TLAS& tlas,
                                                 const std::vector<TLASInstance>& instances) {
    // Calculate size
    // TLAS is typically smaller per-instance than BLAS per-triangle
    // ~64 bytes per instance for the acceleration structure itself
    tlas.size = instances.size() * 64;
    tlas.scratchSize = tlas.size;

    // Create buffers for TLAS storage
    tlas.buffer = CreateBuffer(tlas.size);
    tlas.scratchBuffer = CreateBuffer(tlas.scratchSize);

    // Create and upload instance buffer
    // This contains the TLASInstance structures that reference BLASes with transforms
    tlas.instanceBuffer = CreateBuffer(instances.size() * sizeof(TLASInstance), instances.data());

    // TLAS build stub implementation
    // Full implementation with GL_NV_ray_tracing would:
    // 1. Create VkAccelerationStructureInstanceKHR array from TLASInstance data
    // 2. Upload instance data to GPU buffer
    // 3. Create VkAccelerationStructureGeometryKHR with instances geometry type
    // 4. Call vkGetAccelerationStructureBuildSizesKHR for exact size
    // 5. Call vkCmdBuildAccelerationStructuresKHR to build the TLAS
    // 6. Insert memory barrier before ray tracing

    spdlog::debug("BuildTLASInternal: Created stub TLAS (awaiting GL_NV_ray_tracing implementation)");
    spdlog::debug("  Instances: {}, Estimated size: {:.2f} KB, Instance buffer: {:.2f} KB",
                  instances.size(), tlas.size / 1024.0,
                  (instances.size() * sizeof(TLASInstance)) / 1024.0);
}

void RTXAccelerationStructure::CompactAccelerationStructure(BLAS& blas) {
    if (blas.compacted) {
        spdlog::debug("BLAS '{}' already compacted, skipping", blas.debugName);
        return;
    }

    // Compaction stub implementation
    // Full implementation with GL_NV_ray_tracing would:
    // 1. Query compacted size via vkGetQueryPoolResults (with VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR)
    // 2. Allocate new buffer with compacted size
    // 3. Call vkCmdCopyAccelerationStructureKHR with VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
    // 4. Delete old buffer
    // 5. Update handle to point to compacted structure

    // Compaction typically achieves 30-50% size reduction by:
    // - Removing unused space allocated for worst-case scenarios
    // - Packing nodes more tightly
    // - Removing build-time scratch data embedded in structure

    size_t originalSize = blas.size;

    // Simulate compaction with typical reduction ratios
    // Actual ratio depends on geometry characteristics
    float compactionRatio = 0.6f; // 40% reduction is typical
    blas.size = static_cast<size_t>(blas.size * compactionRatio);
    blas.compacted = true;

    // In production, we would also:
    // - Delete the original buffer and scratch buffer
    // - Create a new smaller buffer
    // - Copy compacted data to new buffer

    m_stats.compactedSize += blas.size;

    double reductionPercent = (1.0 - static_cast<double>(blas.size) / originalSize) * 100.0;
    spdlog::debug("CompactAccelerationStructure '{}': {:.2f} KB -> {:.2f} KB ({:.1f}% reduction)",
                  blas.debugName, originalSize / 1024.0, blas.size / 1024.0, reductionPercent);
    spdlog::debug("  Note: Stub implementation - actual compaction requires GL_NV_ray_tracing");
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
