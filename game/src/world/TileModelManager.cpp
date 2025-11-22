#include "TileModelManager.hpp"
#include "ProceduralModels.hpp"
#include "../../../engine/graphics/Mesh.hpp"
#include "../../../engine/graphics/Texture.hpp"
#include "../../../engine/graphics/Shader.hpp"
#include "../../../engine/graphics/TextureManager.hpp"
#include "../../../engine/graphics/ModelLoader.hpp"
#include "../../../engine/core/Logger.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <algorithm>

namespace Vehement {

TileModelManager::TileModelManager() = default;

TileModelManager::~TileModelManager() {
    Shutdown();
}

bool TileModelManager::Initialize(const TileModelManagerConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_initialized = true;

    // Preload common models if enabled
    if (m_config.preloadCommonModels) {
        PreloadCommonModels();
    }

    return true;
}

void TileModelManager::Shutdown() {
    // Clear all batches
    m_instanceBatches.clear();

    // Clear all models
    m_models.clear();

    // Reset state
    m_batchState = BatchState{};
    m_stats = ManagerStats{};
    m_initialized = false;
}

void TileModelManager::SetTextureManager(Nova::TextureManager* textureManager) {
    m_textureManager = textureManager;
}

// ============================================================================
// Model Loading
// ============================================================================

std::shared_ptr<TileModel> TileModelManager::LoadModel(const std::string& path) {
    // Check if already loaded
    auto it = m_models.find(path);
    if (it != m_models.end()) {
        it->second.useCount++;
        return it->second.model;
    }

    // Construct full path
    std::string fullPath = m_config.modelBasePath + path;

    // Create and load model
    auto model = std::make_shared<TileModel>();
    if (!model->LoadFromFile(fullPath)) {
        Nova::Logger::Error("TileModelManager: Failed to load model: {}", fullPath);
        return nullptr;
    }

    model->SetId(path);

    // Create entry
    TileModelEntry entry;
    entry.model = model;
    entry.displayName = path;
    entry.isProcedural = false;
    entry.useCount = 1;

    m_models[path] = entry;

    // Update stats
    m_stats.totalModels++;
    m_stats.loadedFromFile++;
    m_stats.totalVertices += model->GetVertexCount();
    m_stats.totalIndices += model->GetIndexCount();

    return model;
}

std::shared_ptr<TileModel> TileModelManager::LoadModel(const std::string& id, const TileModelData& data) {
    // Check if already loaded
    auto it = m_models.find(id);
    if (it != m_models.end()) {
        it->second.useCount++;
        return it->second.model;
    }

    // Create and load model
    auto model = std::make_shared<TileModel>();
    if (!model->LoadFromData(data)) {
        Nova::Logger::Error("TileModelManager: Failed to load model with data: {}", id);
        return nullptr;
    }

    model->SetId(id);

    // Create entry
    TileModelEntry entry;
    entry.model = model;
    entry.displayName = id;
    entry.isProcedural = false;
    entry.useCount = 1;

    m_models[id] = entry;

    // Update stats
    m_stats.totalModels++;
    m_stats.loadedFromFile++;
    m_stats.totalVertices += model->GetVertexCount();
    m_stats.totalIndices += model->GetIndexCount();

    return model;
}

std::shared_ptr<TileModel> TileModelManager::GetModel(const std::string& id) {
    auto it = m_models.find(id);
    if (it != m_models.end()) {
        return it->second.model;
    }
    return nullptr;
}

bool TileModelManager::HasModel(const std::string& id) const {
    return m_models.find(id) != m_models.end();
}

void TileModelManager::UnloadModel(const std::string& id) {
    auto it = m_models.find(id);
    if (it != m_models.end()) {
        it->second.useCount--;
        if (it->second.useCount <= 0) {
            // Update stats
            m_stats.totalModels--;
            if (it->second.isProcedural) {
                m_stats.proceduralModels--;
            } else {
                m_stats.loadedFromFile--;
            }

            // Remove instance batch if exists
            m_instanceBatches.erase(id);

            m_models.erase(it);
        }
    }
}

bool TileModelManager::ReloadModel(const std::string& id) {
    auto it = m_models.find(id);
    if (it == m_models.end() || it->second.isProcedural) {
        return false;
    }

    // Store old use count
    int useCount = it->second.useCount;

    // Reload from file
    std::string fullPath = m_config.modelBasePath + id;
    if (!it->second.model->LoadFromFile(fullPath)) {
        return false;
    }

    it->second.useCount = useCount;
    return true;
}

// ============================================================================
// Procedural Primitives
// ============================================================================

std::shared_ptr<TileModel> TileModelManager::CreateCube(const glm::vec3& size, const std::string& texture) {
    std::string id = GenerateProceduralId("cube");

    MeshData data = ProceduralModels::CreateCube(size);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateHexPrism(float radius, float height, const std::string& texture) {
    std::string id = GenerateProceduralId("hexprism");

    MeshData data = ProceduralModels::CreateHexPrism(radius, height);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateCylinder(float radius, float height, int segments,
                                                            const std::string& texture) {
    std::string id = GenerateProceduralId("cylinder");

    MeshData data = ProceduralModels::CreateCylinder(radius, height, segments);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateSphere(float radius, int segments, const std::string& texture) {
    std::string id = GenerateProceduralId("sphere");

    MeshData data = ProceduralModels::CreateSphere(radius, segments);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateCone(float radius, float height, int segments,
                                                        const std::string& texture) {
    std::string id = GenerateProceduralId("cone");

    MeshData data = ProceduralModels::CreateCone(radius, height, segments);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateWedge(const glm::vec3& size, const std::string& texture) {
    std::string id = GenerateProceduralId("wedge");

    MeshData data = ProceduralModels::CreateWedge(size);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateStairs(float width, float height, int steps,
                                                          const std::string& texture) {
    std::string id = GenerateProceduralId("stairs");

    MeshData data = ProceduralModels::CreateStairs(width, height, steps);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreatePlane(float width, float depth, const std::string& texture) {
    std::string id = GenerateProceduralId("plane");

    MeshData data = ProceduralModels::CreatePlane(width, depth);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateTorus(float innerRadius, float outerRadius, int segments,
                                                         const std::string& texture) {
    std::string id = GenerateProceduralId("torus");

    MeshData data = ProceduralModels::CreateTorus(innerRadius, outerRadius, segments, segments);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

// ============================================================================
// Hex Grid Specific
// ============================================================================

std::shared_ptr<TileModel> TileModelManager::CreateHexTile(float radius, float height, const std::string& texture) {
    std::string id = GenerateProceduralId("hextile");

    MeshData data = ProceduralModels::CreateHexTile(radius, height);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateHexWall(float radius, float height, int side,
                                                           const std::string& texture) {
    std::string id = GenerateProceduralId("hexwall");

    MeshData data = ProceduralModels::CreateHexWall(radius, height, side);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

std::shared_ptr<TileModel> TileModelManager::CreateHexCorner(float radius, float height, int corner,
                                                             const std::string& texture) {
    std::string id = GenerateProceduralId("hexcorner");

    MeshData data = ProceduralModels::CreateHexCorner(radius, height, corner);
    auto mesh = ProceduralModels::CreateMeshFromData(data);

    return CreateProceduralModel(id, std::move(mesh), texture);
}

// ============================================================================
// Batch Rendering
// ============================================================================

void TileModelManager::RenderInstanced(const std::string& modelId, const std::vector<glm::mat4>& transforms) {
    auto model = GetModel(modelId);
    if (!model || transforms.empty()) {
        return;
    }

    // Get or create instance batch
    auto& batch = m_instanceBatches[modelId];
    if (!batch) {
        batch = std::make_unique<TileModelBatch>();
        batch->Initialize(model.get(), static_cast<int>(transforms.size()) + 100);
    }

    // Clear and add instances
    batch->Clear();
    for (const auto& transform : transforms) {
        batch->AddInstance(transform);
    }

    // Render
    batch->Render();

    // Update stats
    m_stats.batchedDrawCalls++;
    m_stats.instancesRendered += static_cast<int>(transforms.size());
}

void TileModelManager::RenderInstanced(const std::string& modelId, const std::vector<glm::mat4>& transforms,
                                       const std::vector<glm::vec4>& colors) {
    auto model = GetModel(modelId);
    if (!model || transforms.empty()) {
        return;
    }

    // Get or create instance batch
    auto& batch = m_instanceBatches[modelId];
    if (!batch) {
        batch = std::make_unique<TileModelBatch>();
        batch->Initialize(model.get(), static_cast<int>(transforms.size()) + 100);
    }

    // Clear and add instances
    batch->Clear();
    for (size_t i = 0; i < transforms.size(); ++i) {
        glm::vec4 color = (i < colors.size()) ? colors[i] : glm::vec4(1.0f);
        batch->AddInstance(transforms[i], color);
    }

    // Render
    batch->Render();

    // Update stats
    m_stats.batchedDrawCalls++;
    m_stats.instancesRendered += static_cast<int>(transforms.size());
}

void TileModelManager::BeginBatch(Nova::Shader* shader, const glm::mat4& viewProjection) {
    if (m_batchState.active) {
        FlushBatch();
    }

    m_batchState.shader = shader;
    m_batchState.viewProjection = viewProjection;
    m_batchState.instancesByModel.clear();
    m_batchState.active = true;
}

void TileModelManager::AddToBatch(const std::string& modelId, const glm::mat4& transform, const glm::vec4& color) {
    if (!m_batchState.active) {
        return;
    }

    TileModelInstance instance;
    instance.transform = transform;
    instance.color = color;
    instance.customData = glm::vec4(0.0f);

    m_batchState.instancesByModel[modelId].push_back(instance);
}

void TileModelManager::EndBatch() {
    if (!m_batchState.active) {
        return;
    }

    FlushBatch();
    m_batchState.active = false;
}

void TileModelManager::FlushBatch() {
    if (!m_batchState.active || !m_batchState.shader) {
        return;
    }

    // Render each model's instances
    for (auto& [modelId, instances] : m_batchState.instancesByModel) {
        if (instances.empty()) {
            continue;
        }

        auto model = GetModel(modelId);
        if (!model) {
            continue;
        }

        // Get or create batch
        auto& batch = m_instanceBatches[modelId];
        if (!batch) {
            batch = std::make_unique<TileModelBatch>();
            batch->Initialize(model.get(), std::max(1000, static_cast<int>(instances.size())));
        }

        // Add instances
        batch->Clear();
        for (const auto& inst : instances) {
            batch->AddInstance(inst.transform, inst.color);
        }

        // Render with shader
        batch->Render(*m_batchState.shader);

        // Update stats
        m_stats.batchedDrawCalls++;
        m_stats.instancesRendered += static_cast<int>(instances.size());
    }

    m_batchState.instancesByModel.clear();
}

// ============================================================================
// Model Management
// ============================================================================

void TileModelManager::RegisterModel(const std::string& id, std::shared_ptr<TileModel> model,
                                     TileModelCategory category) {
    if (!model) {
        return;
    }

    TileModelEntry entry;
    entry.model = model;
    entry.category = category;
    entry.displayName = id;
    entry.isProcedural = false;
    entry.useCount = 1;

    m_models[id] = entry;
    model->SetId(id);

    m_stats.totalModels++;
}

std::vector<std::string> TileModelManager::GetModelsByCategory(TileModelCategory category) const {
    std::vector<std::string> result;

    for (const auto& [id, entry] : m_models) {
        if (entry.category == category) {
            result.push_back(id);
        }
    }

    return result;
}

std::vector<std::string> TileModelManager::GetAllModelIds() const {
    std::vector<std::string> result;
    result.reserve(m_models.size());

    for (const auto& [id, entry] : m_models) {
        result.push_back(id);
    }

    return result;
}

const TileModelEntry* TileModelManager::GetModelEntry(const std::string& id) const {
    auto it = m_models.find(id);
    if (it != m_models.end()) {
        return &it->second;
    }
    return nullptr;
}

void TileModelManager::ClearCache() {
    m_models.clear();
    m_instanceBatches.clear();
    m_stats = ManagerStats{};
}

void TileModelManager::PreloadCommonModels() {
    // Create common placeholder models for development

    // Basic shapes
    CreateCube(glm::vec3(1.0f), "");
    CreateSphere(0.5f, 16, "");
    CreateCylinder(0.5f, 1.0f, 16, "");
    CreateCone(0.5f, 1.0f, 16, "");

    // Hex grid models
    CreateHexTile(1.0f, 0.1f, "");
    CreateHexPrism(1.0f, 1.0f, "");

    // Building components
    CreateWedge(glm::vec3(1.0f, 0.5f, 1.0f), "");
    CreateStairs(1.0f, 1.0f, 4, "");
    CreatePlane(1.0f, 1.0f, "");
}

void TileModelManager::ResetFrameStats() {
    m_stats.batchedDrawCalls = 0;
    m_stats.instancesRendered = 0;
}

// ============================================================================
// Private Methods
// ============================================================================

std::string TileModelManager::GenerateProceduralId(const std::string& prefix) {
    std::stringstream ss;
    ss << "_procedural_" << prefix << "_" << m_proceduralCounter++;
    return ss.str();
}

void TileModelManager::LoadModelTexture(TileModel& model, const std::string& texturePath) {
    if (texturePath.empty()) {
        return;
    }

    std::string fullPath = m_config.textureBasePath + texturePath;

    if (m_textureManager) {
        auto texture = m_textureManager->Load(fullPath);
        if (texture) {
            model.SetTexture(texture);
        }
    } else {
        auto texture = std::make_shared<Nova::Texture>();
        if (texture->Load(fullPath)) {
            model.SetTexture(texture);
        }
    }
}

std::shared_ptr<TileModel> TileModelManager::CreateProceduralModel(const std::string& id,
                                                                    std::unique_ptr<Nova::Mesh> mesh,
                                                                    const std::string& texture) {
    auto model = std::make_shared<TileModel>();

    if (!model->CreateFromMesh(std::move(mesh), texture.empty() ? "" : (m_config.textureBasePath + texture))) {
        return nullptr;
    }

    model->SetId(id);

    // Create entry
    TileModelEntry entry;
    entry.model = model;
    entry.displayName = id;
    entry.isProcedural = true;
    entry.useCount = 1;

    m_models[id] = entry;

    // Update stats
    m_stats.totalModels++;
    m_stats.proceduralModels++;
    m_stats.totalVertices += model->GetVertexCount();
    m_stats.totalIndices += model->GetIndexCount();

    return model;
}

} // namespace Vehement
