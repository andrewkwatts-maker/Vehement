#include "graphics/Batching.hpp"
#include "graphics/Mesh.hpp"
#include "graphics/Material.hpp"
#include "graphics/Shader.hpp"
#include "graphics/Texture.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace Nova {

Batching::Batching() = default;

Batching::~Batching() {
    Shutdown();
}

bool Batching::Initialize(const BatchConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Check for instancing support (OpenGL 3.3+)
    GLint majorVersion, minorVersion;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

    m_instancingSupported = (majorVersion > 3) || (majorVersion == 3 && minorVersion >= 3);
    m_indirectSupported = (majorVersion > 4) || (majorVersion == 4 && minorVersion >= 3);
    m_persistentMappingSupported = (majorVersion > 4) || (majorVersion == 4 && minorVersion >= 4);

    spdlog::info("Batching system initialized:");
    spdlog::info("  - Instancing: {}", m_instancingSupported ? "supported" : "not supported");
    spdlog::info("  - Indirect draw: {}", m_indirectSupported ? "supported" : "not supported");
    spdlog::info("  - Persistent mapping: {}", m_persistentMappingSupported ? "supported" : "not supported");

    // Create frame UBO
    glGenBuffers(1, &m_frameUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUniforms), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_frameUBO);  // Binding point 0
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Create indirect draw buffer if supported
    if (m_indirectSupported && m_config.useIndirectRendering) {
        glGenBuffers(1, &m_indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER,
                     sizeof(IndirectDrawCommand) * m_config.maxBatchSize,
                     nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

    // Create persistent mapped buffer if supported
    if (m_persistentMappingSupported && m_config.usePersistentMapping) {
        m_persistentBufferSize = sizeof(InstanceData) * m_config.maxBatchSize * 10;

        GLuint buffer;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_ARRAY_BUFFER, m_persistentBufferSize, nullptr, flags);

        m_persistentBuffer = glMapBufferRange(GL_ARRAY_BUFFER, 0, m_persistentBufferSize, flags);

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (!m_persistentBuffer) {
            spdlog::warn("Failed to create persistent mapped buffer, falling back to standard buffers");
            m_persistentMappingSupported = false;
            glDeleteBuffers(1, &buffer);
        }
    }

    m_initialized = true;
    return true;
}

void Batching::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Cleanup batch buffers
    for (auto& [mesh, group] : m_meshBatches) {
        for (auto& [key, batch] : group.batches) {
            if (batch.instanceVBO) {
                glDeleteBuffers(1, &batch.instanceVBO);
            }
        }
    }
    m_meshBatches.clear();

    // Cleanup static batches
    m_staticBatches.clear();

    if (m_frameUBO) {
        glDeleteBuffers(1, &m_frameUBO);
        m_frameUBO = 0;
    }

    if (m_indirectBuffer) {
        glDeleteBuffers(1, &m_indirectBuffer);
        m_indirectBuffer = 0;
    }

    m_initialized = false;
}

void Batching::BeginFrame() {
    m_stats.Reset();

    // Clear instance data but keep batch structures
    for (auto& [mesh, group] : m_meshBatches) {
        for (auto& [key, batch] : group.batches) {
            batch.Clear();
        }
    }

    m_persistentBufferOffset = 0;
}

void Batching::EndFrame() {
    // Update dirty instance buffers
    for (auto& [mesh, group] : m_meshBatches) {
        for (auto& [key, batch] : group.batches) {
            if (batch.dirty && !batch.instances.empty()) {
                UpdateInstanceBuffer(batch);
            }
        }
    }

    // Calculate efficiency stats
    if (m_stats.totalObjects > 0) {
        m_stats.batchEfficiency =
            (1.0f - static_cast<float>(m_stats.totalBatches) / m_stats.totalObjects) * 100.0f;
        m_stats.drawCallsSaved = m_stats.totalObjects - m_stats.totalBatches;
    }
}

void Batching::Submit(const std::shared_ptr<Mesh>& mesh,
                      const std::shared_ptr<Material>& material,
                      const glm::mat4& transform,
                      uint32_t objectID,
                      const glm::vec4& color) {
    if (!mesh || !material || !m_config.enabled) {
        return;
    }

    m_stats.totalObjects++;

    // Create batch key from material state
    BatchKey key;
    key.shaderID = material->GetShaderPtr() ? material->GetShaderPtr()->GetID() : 0;
    key.transparent = material->IsTransparent();
    key.twoSided = material->IsTwoSided();
    // Note: In a real implementation, we'd get texture IDs from the material

    // Find or create batch
    auto& group = m_meshBatches[mesh.get()];
    auto& batch = group.batches[key];

    if (!batch.mesh) {
        batch.mesh = mesh;
        batch.material = material;
        batch.key = key;
    }

    // Add instance data
    InstanceData instance;
    instance.modelMatrix = transform;
    instance.normalMatrix = glm::mat4(glm::transpose(glm::inverse(glm::mat3(transform))));
    instance.color = color;
    instance.objectID = objectID;

    batch.instances.push_back(instance);
    batch.dirty = true;
}

void Batching::Flush(const glm::mat4& viewProjection) {
    if (!m_initialized) {
        return;
    }

    // Update frame uniforms
    FrameUniforms frameData;
    frameData.viewProjection = viewProjection;

    glBindBuffer(GL_UNIFORM_BUFFER, m_frameUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUniforms), &frameData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Collect all non-empty batches
    std::vector<RenderBatch*> batchesToRender;
    for (auto& [mesh, group] : m_meshBatches) {
        for (auto& [key, batch] : group.batches) {
            if (!batch.instances.empty()) {
                batchesToRender.push_back(&batch);
            }
        }
    }

    if (batchesToRender.empty()) {
        return;
    }

    // Sort batches to minimize state changes
    SortBatches(batchesToRender);

    // Render batches
    uint32_t lastShaderID = 0;
    uint32_t lastTextureID = 0;

    for (RenderBatch* batch : batchesToRender) {
        m_stats.totalBatches++;

        if (batch->instances.size() >= m_config.minInstancesForBatching &&
            m_instancingSupported && m_config.useInstancedRendering) {
            RenderBatchInstanced(*batch, viewProjection);
            m_stats.instancedDrawCalls++;
        } else {
            RenderBatch(*batch, viewProjection);
        }
    }
}

void Batching::CreateInstanceBuffer(RenderBatch& batch) {
    if (batch.instanceVBO == 0) {
        glGenBuffers(1, &batch.instanceVBO);
    }

    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(InstanceData) * m_config.maxBatchSize,
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Batching::UpdateInstanceBuffer(RenderBatch& batch) {
    if (batch.instanceVBO == 0) {
        CreateInstanceBuffer(batch);
    }

    size_t dataSize = sizeof(InstanceData) * batch.instances.size();

    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);

    // Orphan the buffer for better performance
    glBufferData(GL_ARRAY_BUFFER, dataSize, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, batch.instances.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    batch.instanceCount = static_cast<uint32_t>(batch.instances.size());
    batch.dirty = false;
}

void Batching::RenderBatch(RenderBatch& batch, const glm::mat4& viewProjection) {
    if (!batch.mesh || !batch.material) {
        return;
    }

    batch.material->Bind();
    Shader& shader = batch.material->GetShader();

    // Render each instance individually (fallback path)
    for (const auto& instance : batch.instances) {
        shader.SetMat4("u_ProjectionView", viewProjection);
        shader.SetMat4("u_Model", instance.modelMatrix);
        shader.SetMat3("u_NormalMatrix", glm::mat3(instance.normalMatrix));
        shader.SetVec4("u_InstanceColor", instance.color);

        batch.mesh->Draw();
        m_stats.verticesBatched += batch.mesh->GetVertexCount();
    }
}

void Batching::RenderBatchInstanced(RenderBatch& batch, const glm::mat4& viewProjection) {
    if (!batch.mesh || !batch.material || batch.instanceCount == 0) {
        return;
    }

    batch.material->Bind();
    Shader& shader = batch.material->GetShader();
    shader.SetMat4("u_ProjectionView", viewProjection);

    // Setup instance attributes
    // Assuming mesh VAO is bound
    glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);

    // Instance model matrix (4 vec4s at locations 4-7)
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(4 + i);
        glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE,
                              sizeof(InstanceData),
                              (void*)(offsetof(InstanceData, modelMatrix) + sizeof(glm::vec4) * i));
        glVertexAttribDivisor(4 + i, 1);
    }

    // Instance normal matrix (4 vec4s at locations 8-11)
    for (int i = 0; i < 4; i++) {
        glEnableVertexAttribArray(8 + i);
        glVertexAttribPointer(8 + i, 4, GL_FLOAT, GL_FALSE,
                              sizeof(InstanceData),
                              (void*)(offsetof(InstanceData, normalMatrix) + sizeof(glm::vec4) * i));
        glVertexAttribDivisor(8 + i, 1);
    }

    // Instance color (location 12)
    glEnableVertexAttribArray(12);
    glVertexAttribPointer(12, 4, GL_FLOAT, GL_FALSE,
                          sizeof(InstanceData),
                          (void*)offsetof(InstanceData, color));
    glVertexAttribDivisor(12, 1);

    // Instance object ID (location 13)
    glEnableVertexAttribArray(13);
    glVertexAttribIPointer(13, 1, GL_UNSIGNED_INT,
                           sizeof(InstanceData),
                           (void*)offsetof(InstanceData, objectID));
    glVertexAttribDivisor(13, 1);

    // Draw instanced
    batch.mesh->DrawInstanced(batch.instanceCount);
    m_stats.verticesBatched += batch.mesh->GetVertexCount() * batch.instanceCount;

    // Reset divisors
    for (int i = 4; i <= 13; i++) {
        glVertexAttribDivisor(i, 0);
        glDisableVertexAttribArray(i);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Batching::SortBatches(std::vector<RenderBatch*>& batches) {
    // Sort by: transparent last, then by shader, then by texture
    std::sort(batches.begin(), batches.end(),
        [](const RenderBatch* a, const RenderBatch* b) {
            // Opaque before transparent
            if (a->key.transparent != b->key.transparent) {
                return !a->key.transparent;
            }
            // Sort by shader
            if (a->key.shaderID != b->key.shaderID) {
                return a->key.shaderID < b->key.shaderID;
            }
            // Sort by texture
            return a->key.textureID < b->key.textureID;
        });
}

uint64_t Batching::ComputeSortKey(const BatchKey& key, float depth) {
    // Pack into 64-bit key for fast sorting
    // Layout: [transparent:1][shader:16][texture:16][depth:31]
    uint64_t sortKey = 0;
    sortKey |= (key.transparent ? 1ULL : 0ULL) << 63;
    sortKey |= (static_cast<uint64_t>(key.shaderID) & 0xFFFF) << 47;
    sortKey |= (static_cast<uint64_t>(key.textureID) & 0xFFFF) << 31;

    // Convert depth to fixed-point for sorting
    uint32_t depthBits = static_cast<uint32_t>(depth * 2147483647.0f);
    sortKey |= depthBits;

    return sortKey;
}

int Batching::CreateStaticBatch(const std::vector<std::shared_ptr<Mesh>>& meshes,
                                 const std::vector<std::shared_ptr<Material>>& materials,
                                 const std::vector<glm::mat4>& transforms) {
    if (meshes.empty() || meshes.size() != transforms.size()) {
        return -1;
    }

    // Merge meshes
    auto mergedMesh = MergeMeshes(meshes, transforms);
    if (!mergedMesh) {
        return -1;
    }

    StaticBatch batch;
    batch.mergedMesh = mergedMesh;
    batch.material = materials.empty() ? nullptr : materials[0];
    batch.transform = glm::mat4(1.0f);
    batch.vertexCount = mergedMesh->GetVertexCount();
    batch.indexCount = mergedMesh->GetIndexCount();
    batch.valid = true;

    // Find empty slot or add new
    for (size_t i = 0; i < m_staticBatches.size(); i++) {
        if (!m_staticBatches[i].valid) {
            m_staticBatches[i] = std::move(batch);
            return static_cast<int>(i);
        }
    }

    m_staticBatches.push_back(std::move(batch));
    return static_cast<int>(m_staticBatches.size() - 1);
}

void Batching::RenderStaticBatch(int batchIndex, const glm::mat4& viewProjection) {
    if (batchIndex < 0 || batchIndex >= static_cast<int>(m_staticBatches.size())) {
        return;
    }

    auto& batch = m_staticBatches[batchIndex];
    if (!batch.valid || !batch.mergedMesh || !batch.material) {
        return;
    }

    batch.material->Bind();
    Shader& shader = batch.material->GetShader();
    shader.SetMat4("u_ProjectionView", viewProjection);
    shader.SetMat4("u_Model", batch.transform);

    batch.mergedMesh->Draw();
    m_stats.staticBatches++;
}

void Batching::RemoveStaticBatch(int batchIndex) {
    if (batchIndex >= 0 && batchIndex < static_cast<int>(m_staticBatches.size())) {
        m_staticBatches[batchIndex].valid = false;
        m_staticBatches[batchIndex].mergedMesh.reset();
        m_staticBatches[batchIndex].material.reset();
    }
}

void Batching::SetConfig(const BatchConfig& config) {
    m_config = config;
}

std::shared_ptr<Mesh> Batching::MergeMeshes(
    const std::vector<std::shared_ptr<Mesh>>& meshes,
    const std::vector<glm::mat4>& transforms) {

    // Calculate total vertex/index count
    uint32_t totalVertices = 0;
    uint32_t totalIndices = 0;

    for (const auto& mesh : meshes) {
        if (mesh) {
            totalVertices += mesh->GetVertexCount();
            totalIndices += mesh->GetIndexCount();
        }
    }

    if (totalVertices > m_config.maxVerticesPerStaticBatch) {
        spdlog::warn("Static batch exceeds max vertex count: {} > {}",
                     totalVertices, m_config.maxVerticesPerStaticBatch);
        return nullptr;
    }

    // Merge vertex and index data
    std::vector<Vertex> mergedVertices;
    std::vector<uint32_t> mergedIndices;
    mergedVertices.reserve(totalVertices);
    mergedIndices.reserve(totalIndices);

    uint32_t indexOffset = 0;

    for (size_t i = 0; i < meshes.size(); i++) {
        const auto& mesh = meshes[i];
        if (!mesh) continue;

        const glm::mat4& transform = transforms[i];
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        // Note: In a real implementation, we'd need access to raw vertex data
        // For now, this is a placeholder that would need mesh vertex access
        // The actual implementation would transform vertices by the world matrix

        indexOffset += mesh->GetVertexCount();
    }

    // Create merged mesh
    auto mergedMesh = std::make_unique<Mesh>();
    if (!mergedVertices.empty()) {
        mergedMesh->Create(mergedVertices, mergedIndices);
    }

    return mergedMesh;
}

} // namespace Nova
