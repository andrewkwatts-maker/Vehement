#include "TileModel.hpp"
#include "../../../engine/graphics/Mesh.hpp"
#include "../../../engine/graphics/Texture.hpp"
#include "../../../engine/graphics/Shader.hpp"
#include "../../../engine/graphics/ModelLoader.hpp"
#include "../../../engine/graphics/TextureManager.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// TileModel Implementation
// ============================================================================

TileModel::TileModel() = default;

TileModel::~TileModel() {
    Cleanup();
}

TileModel::TileModel(TileModel&& other) noexcept
    : m_id(std::move(other.m_id))
    , m_data(std::move(other.m_data))
    , m_isValid(other.m_isValid)
    , m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_indexCount(other.m_indexCount)
    , m_mesh(std::move(other.m_mesh))
    , m_lodMeshes(std::move(other.m_lodMeshes))
    , m_currentLOD(other.m_currentLOD)
    , m_texture(std::move(other.m_texture))
    , m_normalMap(std::move(other.m_normalMap))
    , m_specularMap(std::move(other.m_specularMap))
    , m_emissiveMap(std::move(other.m_emissiveMap))
    , m_boundingRadius(other.m_boundingRadius)
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_isValid = false;
}

TileModel& TileModel::operator=(TileModel&& other) noexcept {
    if (this != &other) {
        Cleanup();

        m_id = std::move(other.m_id);
        m_data = std::move(other.m_data);
        m_isValid = other.m_isValid;
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        m_mesh = std::move(other.m_mesh);
        m_lodMeshes = std::move(other.m_lodMeshes);
        m_currentLOD = other.m_currentLOD;
        m_texture = std::move(other.m_texture);
        m_normalMap = std::move(other.m_normalMap);
        m_specularMap = std::move(other.m_specularMap);
        m_emissiveMap = std::move(other.m_emissiveMap);
        m_boundingRadius = other.m_boundingRadius;

        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_isValid = false;
    }
    return *this;
}

bool TileModel::LoadFromFile(const std::string& path) {
    TileModelData data;
    data.modelPath = path;
    return LoadFromData(data);
}

bool TileModel::LoadFromData(const TileModelData& data) {
    Cleanup();

    m_data = data;

    // Load the model using engine's ModelLoader
    auto model = Nova::ModelLoader::Load(m_data.modelPath, true, false);
    if (!model || model->meshes.empty()) {
        // Model loading failed, mark as invalid
        return false;
    }

    // Take the first mesh from the loaded model
    m_mesh = std::move(model->meshes[0]);

    // Update bounds from mesh
    m_data.boundsMin = model->boundsMin;
    m_data.boundsMax = model->boundsMax;

    // Create default collision hull if none provided
    if (m_data.collisionHull.empty()) {
        m_data.CreateDefaultCollisionHull();
    }

    // Calculate bounding radius
    CalculateBoundingRadius();

    // Load textures
    if (!LoadTextures()) {
        // Continue without textures - not a fatal error
    }

    // Generate LOD meshes if LOD levels are defined
    if (!m_data.lodLevels.empty()) {
        GenerateLODs();
    }

    m_isValid = true;
    return true;
}

bool TileModel::CreateFromMesh(std::unique_ptr<Nova::Mesh> mesh, const std::string& texturePath) {
    if (!mesh || !mesh->IsValid()) {
        return false;
    }

    Cleanup();

    m_mesh = std::move(mesh);

    // Get bounds from mesh
    m_data.boundsMin = m_mesh->GetBoundsMin();
    m_data.boundsMax = m_mesh->GetBoundsMax();
    m_data.CreateDefaultCollisionHull();

    // Calculate bounding radius
    CalculateBoundingRadius();

    // Load texture if provided
    if (!texturePath.empty()) {
        m_data.texturePath = texturePath;
        LoadTextures();
    }

    m_isValid = true;
    return true;
}

void TileModel::Render(const glm::mat4& transform) {
    if (!m_isValid) return;

    BindTextures();

    // Apply default scale and pivot offset
    glm::mat4 finalTransform = transform;
    finalTransform = glm::translate(finalTransform, m_data.pivotOffset);
    finalTransform = glm::scale(finalTransform, m_data.defaultScale);

    // Select appropriate LOD mesh
    Nova::Mesh* meshToRender = m_mesh.get();
    if (m_currentLOD > 0 && m_currentLOD <= static_cast<int>(m_lodMeshes.size())) {
        meshToRender = m_lodMeshes[m_currentLOD - 1].get();
    }

    if (meshToRender) {
        meshToRender->Draw();
    }

    UnbindTextures();
}

void TileModel::Render(const glm::mat4& transform, Nova::Shader& shader) {
    if (!m_isValid) return;

    BindTextures();

    // Select appropriate LOD mesh
    Nova::Mesh* meshToRender = m_mesh.get();
    if (m_currentLOD > 0 && m_currentLOD <= static_cast<int>(m_lodMeshes.size())) {
        meshToRender = m_lodMeshes[m_currentLOD - 1].get();
    }

    if (meshToRender) {
        meshToRender->Draw();
    }

    UnbindTextures();
}

void TileModel::RenderShadow(const glm::mat4& transform, const glm::mat4& lightSpaceMatrix) {
    if (!m_isValid || !m_data.castsShadow) return;

    // For shadow pass, we don't need textures
    Nova::Mesh* meshToRender = m_mesh.get();

    // Use a coarser LOD for shadow pass to improve performance
    int shadowLOD = std::min(m_currentLOD + 1, static_cast<int>(m_lodMeshes.size()));
    if (shadowLOD > 0 && shadowLOD <= static_cast<int>(m_lodMeshes.size())) {
        meshToRender = m_lodMeshes[shadowLOD - 1].get();
    }

    if (meshToRender) {
        meshToRender->Draw();
    }
}

void TileModel::SetLOD(int level) {
    m_currentLOD = std::clamp(level, 0, static_cast<int>(m_lodMeshes.size()));
}

void TileModel::UpdateLODFromDistance(float cameraDistance) {
    if (m_data.lodLevels.empty()) {
        m_currentLOD = 0;
        return;
    }

    // Find appropriate LOD level based on distance
    int newLOD = 0;
    for (size_t i = 0; i < m_data.lodLevels.size(); ++i) {
        if (cameraDistance >= m_data.lodLevels[i].distance) {
            newLOD = static_cast<int>(i) + 1;
        }
    }

    m_currentLOD = std::min(newLOD, static_cast<int>(m_lodMeshes.size()));
}

void TileModel::SetTexture(std::shared_ptr<Nova::Texture> texture) {
    m_texture = texture;
}

void TileModel::SetNormalMap(std::shared_ptr<Nova::Texture> normalMap) {
    m_normalMap = normalMap;
}

void TileModel::SetSpecularMap(std::shared_ptr<Nova::Texture> specularMap) {
    m_specularMap = specularMap;
}

void TileModel::SetEmissiveMap(std::shared_ptr<Nova::Texture> emissiveMap) {
    m_emissiveMap = emissiveMap;
}

void TileModel::BindTextures() {
    if (m_texture) {
        m_texture->Bind(0);  // Diffuse at slot 0
    }
    if (m_normalMap) {
        m_normalMap->Bind(1);  // Normal at slot 1
    }
    if (m_specularMap) {
        m_specularMap->Bind(2);  // Specular at slot 2
    }
    if (m_emissiveMap) {
        m_emissiveMap->Bind(3);  // Emissive at slot 3
    }
}

void TileModel::UnbindTextures() {
    Nova::Texture::Unbind(0);
    Nova::Texture::Unbind(1);
    Nova::Texture::Unbind(2);
    Nova::Texture::Unbind(3);
}

uint32_t TileModel::GetVertexCount() const {
    if (m_mesh) {
        return m_mesh->GetVertexCount();
    }
    return 0;
}

uint32_t TileModel::GetIndexCount() const {
    if (m_mesh) {
        return m_mesh->GetIndexCount();
    }
    return static_cast<uint32_t>(m_indexCount);
}

void TileModel::Cleanup() {
    // Cleanup raw OpenGL resources if used
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }

    // Cleanup engine mesh
    if (m_mesh) {
        m_mesh->Cleanup();
        m_mesh.reset();
    }

    // Cleanup LOD meshes
    for (auto& lod : m_lodMeshes) {
        if (lod) {
            lod->Cleanup();
        }
    }
    m_lodMeshes.clear();

    // Release texture references
    m_texture.reset();
    m_normalMap.reset();
    m_specularMap.reset();
    m_emissiveMap.reset();

    m_isValid = false;
    m_indexCount = 0;
    m_currentLOD = 0;
}

bool TileModel::LoadTextures() {
    bool success = true;

    // Load diffuse texture
    if (!m_data.texturePath.empty()) {
        m_texture = std::make_shared<Nova::Texture>();
        if (!m_texture->Load(m_data.texturePath)) {
            m_texture.reset();
            success = false;
        }
    }

    // Load normal map
    if (!m_data.normalMapPath.empty()) {
        m_normalMap = std::make_shared<Nova::Texture>();
        if (!m_normalMap->Load(m_data.normalMapPath, false)) {  // Normal maps should not be sRGB
            m_normalMap.reset();
        }
    }

    // Load specular map
    if (!m_data.specularMapPath.empty()) {
        m_specularMap = std::make_shared<Nova::Texture>();
        if (!m_specularMap->Load(m_data.specularMapPath, false)) {
            m_specularMap.reset();
        }
    }

    // Load emissive map
    if (!m_data.emissiveMapPath.empty()) {
        m_emissiveMap = std::make_shared<Nova::Texture>();
        if (!m_emissiveMap->Load(m_data.emissiveMapPath)) {
            m_emissiveMap.reset();
        }
    }

    return success;
}

void TileModel::CalculateBoundingRadius() {
    glm::vec3 center = GetBoundsCenter();
    glm::vec3 size = GetBoundsSize();

    // Bounding sphere radius is half the diagonal of the bounding box
    m_boundingRadius = glm::length(size) * 0.5f;
}

void TileModel::GenerateLODs() {
    // LOD generation would typically involve mesh simplification algorithms
    // For now, we just reference the main mesh at all LOD levels
    // In a full implementation, you would use algorithms like:
    // - Quadric Error Metrics (QEM)
    // - Edge collapse decimation
    // - Vertex clustering

    // Placeholder: create references (in real impl, simplified meshes)
    m_lodMeshes.clear();

    // For each LOD level, we would generate a simplified mesh
    // For now, we skip actual simplification
}

// ============================================================================
// TileModelBatch Implementation
// ============================================================================

TileModelBatch::TileModelBatch() = default;

TileModelBatch::~TileModelBatch() {
    if (m_instanceVBO != 0) {
        glDeleteBuffers(1, &m_instanceVBO);
    }
}

bool TileModelBatch::Initialize(TileModel* model, int maxInstances) {
    if (!model || !model->IsValid()) {
        return false;
    }

    m_model = model;
    m_maxInstances = maxInstances;
    m_instances.reserve(maxInstances);

    // Create instance VBO
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(TileModelInstance), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

int TileModelBatch::AddInstance(const glm::mat4& transform, const glm::vec4& color) {
    if (IsFull()) {
        return -1;
    }

    int index;
    if (!m_freeIndices.empty()) {
        // Reuse a freed index
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
        m_instances[index] = {transform, color, glm::vec4(0.0f)};
    } else {
        // Add new instance
        index = static_cast<int>(m_instances.size());
        m_instances.push_back({transform, color, glm::vec4(0.0f)});
    }

    m_dirty = true;
    return index;
}

void TileModelBatch::UpdateInstance(int index, const glm::mat4& transform) {
    if (index >= 0 && index < static_cast<int>(m_instances.size())) {
        m_instances[index].transform = transform;
        m_dirty = true;
    }
}

void TileModelBatch::UpdateInstanceColor(int index, const glm::vec4& color) {
    if (index >= 0 && index < static_cast<int>(m_instances.size())) {
        m_instances[index].color = color;
        m_dirty = true;
    }
}

void TileModelBatch::RemoveInstance(int index) {
    if (index >= 0 && index < static_cast<int>(m_instances.size())) {
        // Mark as removed (zero scale)
        m_instances[index].transform = glm::mat4(0.0f);
        m_freeIndices.push_back(index);
        m_dirty = true;
    }
}

void TileModelBatch::Clear() {
    m_instances.clear();
    m_freeIndices.clear();
    m_dirty = true;
}

void TileModelBatch::Upload() {
    if (!m_dirty || m_instances.empty()) {
        return;
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_instances.size() * sizeof(TileModelInstance), m_instances.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_dirty = false;
}

void TileModelBatch::Render() {
    if (!m_model || m_instances.empty()) {
        return;
    }

    if (m_dirty) {
        Upload();
    }

    m_model->BindTextures();

    // Get the mesh VAO and set up instanced attributes
    Nova::Mesh* mesh = m_model->GetMesh();
    if (mesh) {
        // Bind instance buffer to mesh VAO
        // Note: This assumes the mesh VAO has been set up with instance attributes
        // In a real implementation, you would configure the VAO in Initialize()

        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);

        // Instance matrix (4 vec4s for mat4)
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                                  reinterpret_cast<void*>(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(4 + i, 1);
        }

        // Instance color
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                              reinterpret_cast<void*>(sizeof(glm::mat4)));
        glVertexAttribDivisor(8, 1);

        // Instance custom data
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                              reinterpret_cast<void*>(sizeof(glm::mat4) + sizeof(glm::vec4)));
        glVertexAttribDivisor(9, 1);

        mesh->DrawInstanced(static_cast<int>(m_instances.size()));

        // Reset attribute divisors
        for (int i = 4; i <= 9; ++i) {
            glVertexAttribDivisor(i, 0);
        }
    }

    m_model->UnbindTextures();
}

void TileModelBatch::Render(Nova::Shader& shader) {
    if (!m_model || m_instances.empty()) {
        return;
    }

    if (m_dirty) {
        Upload();
    }

    m_model->BindTextures();

    Nova::Mesh* mesh = m_model->GetMesh();
    if (mesh) {
        glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);

        // Set up instanced attributes (same as above)
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                                  reinterpret_cast<void*>(sizeof(glm::vec4) * i));
            glVertexAttribDivisor(4 + i, 1);
        }

        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                              reinterpret_cast<void*>(sizeof(glm::mat4)));
        glVertexAttribDivisor(8, 1);

        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(TileModelInstance),
                              reinterpret_cast<void*>(sizeof(glm::mat4) + sizeof(glm::vec4)));
        glVertexAttribDivisor(9, 1);

        mesh->DrawInstanced(static_cast<int>(m_instances.size()));

        for (int i = 4; i <= 9; ++i) {
            glVertexAttribDivisor(i, 0);
        }
    }

    m_model->UnbindTextures();
}

} // namespace Vehement
