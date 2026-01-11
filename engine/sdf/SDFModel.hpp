#pragma once

#include "SDFPrimitive.hpp"
#include "../graphics/Mesh.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Nova {

class Texture;
class Material;

/**
 * @brief Mesh generation settings for SDF to mesh conversion
 */
struct SDFMeshSettings {
    // Marching cubes resolution
    int resolution = 64;
    float boundsPadding = 0.1f;

    // Surface extraction
    float isoLevel = 0.0f;
    bool smoothNormals = true;
    bool generateUVs = true;
    bool generateTangents = true;

    // Optimization
    bool simplifyMesh = false;
    float simplifyRatio = 0.5f;
    float simplifyError = 0.01f;

    // LOD generation
    bool generateLODs = false;
    int lodLevels = 4;
    std::vector<float> lodDistances = {10.0f, 25.0f, 50.0f, 100.0f};
};

/**
 * @brief Texture painting data
 */
struct PaintLayer {
    std::string name;
    std::vector<uint8_t> data;  // RGBA per vertex or texture data
    int width = 0;
    int height = 0;
    float opacity = 1.0f;
    bool visible = true;
};

/**
 * @brief Complete SDF-based model with hierarchy and animation support
 */
class SDFModel {
public:
    SDFModel();
    explicit SDFModel(const std::string& name);
    ~SDFModel();

    // Non-copyable, movable
    SDFModel(const SDFModel&) = delete;
    SDFModel& operator=(const SDFModel&) = delete;
    SDFModel(SDFModel&&) noexcept = default;
    SDFModel& operator=(SDFModel&&) noexcept = default;

    // =========================================================================
    // Properties
    // =========================================================================

    [[nodiscard]] const std::string& GetName() const { return m_name; }
    void SetName(const std::string& name) { m_name = name; }

    [[nodiscard]] uint32_t GetId() const { return m_id; }

    // =========================================================================
    // Hierarchy Management
    // =========================================================================

    /**
     * @brief Get root primitive
     */
    [[nodiscard]] SDFPrimitive* GetRoot() { return m_root.get(); }
    [[nodiscard]] const SDFPrimitive* GetRoot() const { return m_root.get(); }

    /**
     * @brief Set root primitive
     */
    void SetRoot(std::unique_ptr<SDFPrimitive> root);

    /**
     * @brief Create a new primitive and add to parent
     */
    SDFPrimitive* CreatePrimitive(const std::string& name, SDFPrimitiveType type, SDFPrimitive* parent = nullptr);

    /**
     * @brief Find primitive by name
     */
    [[nodiscard]] SDFPrimitive* FindPrimitive(const std::string& name);

    /**
     * @brief Find primitive by ID
     */
    [[nodiscard]] SDFPrimitive* FindPrimitiveById(uint32_t id);

    /**
     * @brief Delete primitive (and children)
     */
    bool DeletePrimitive(SDFPrimitive* primitive);

    /**
     * @brief Get all primitives as flat list
     */
    [[nodiscard]] std::vector<SDFPrimitive*> GetAllPrimitives();
    [[nodiscard]] std::vector<const SDFPrimitive*> GetAllPrimitives() const;

    /**
     * @brief Get primitive count
     */
    [[nodiscard]] size_t GetPrimitiveCount() const;

    // =========================================================================
    // SDF Evaluation
    // =========================================================================

    /**
     * @brief Evaluate combined SDF at world point
     */
    [[nodiscard]] float EvaluateSDF(const glm::vec3& point) const;

    /**
     * @brief Get combined bounding box
     */
    [[nodiscard]] std::pair<glm::vec3, glm::vec3> GetBounds() const;

    /**
     * @brief Calculate normal at point
     */
    [[nodiscard]] glm::vec3 CalculateNormal(const glm::vec3& point, float epsilon = 0.001f) const;

    // =========================================================================
    // Mesh Generation
    // =========================================================================

    /**
     * @brief Generate mesh from SDF using marching cubes
     */
    [[nodiscard]] std::shared_ptr<Mesh> GenerateMesh(const SDFMeshSettings& settings = {}) const;

    /**
     * @brief Generate mesh with LODs
     */
    [[nodiscard]] std::vector<std::shared_ptr<Mesh>> GenerateMeshLODs(const SDFMeshSettings& settings = {}) const;

    /**
     * @brief Get cached mesh (regenerate if dirty)
     */
    [[nodiscard]] std::shared_ptr<Mesh> GetMesh();

    /**
     * @brief Mark mesh as needing regeneration
     */
    void InvalidateMesh();

    /**
     * @brief Get/set mesh settings
     */
    [[nodiscard]] const SDFMeshSettings& GetMeshSettings() const { return m_meshSettings; }
    void SetMeshSettings(const SDFMeshSettings& settings);

    // =========================================================================
    // Texture Painting
    // =========================================================================

    /**
     * @brief Add paint layer
     */
    PaintLayer* AddPaintLayer(const std::string& name, int width = 1024, int height = 1024);

    /**
     * @brief Remove paint layer
     */
    bool RemovePaintLayer(const std::string& name);

    /**
     * @brief Get paint layer
     */
    [[nodiscard]] PaintLayer* GetPaintLayer(const std::string& name);

    /**
     * @brief Get all paint layers
     */
    [[nodiscard]] const std::vector<PaintLayer>& GetPaintLayers() const { return m_paintLayers; }

    /**
     * @brief Paint on surface
     */
    void PaintAt(const glm::vec3& worldPos, const glm::vec4& color, float radius, float hardness, const std::string& layer = "");

    /**
     * @brief Bake paint layers to texture
     */
    [[nodiscard]] std::shared_ptr<Texture> BakePaintTexture() const;

    /**
     * @brief Get/set base texture path
     */
    [[nodiscard]] const std::string& GetBaseTexturePath() const { return m_baseTexturePath; }
    void SetBaseTexturePath(const std::string& path) { m_baseTexturePath = path; }

    // =========================================================================
    // Material
    // =========================================================================

    /**
     * @brief Get combined material
     */
    [[nodiscard]] std::shared_ptr<Material> GetMaterial() const { return m_material; }
    void SetMaterial(std::shared_ptr<Material> material) { m_material = material; }

    // =========================================================================
    // Animation Support
    // =========================================================================

    /**
     * @brief Get primitive names for animation binding
     */
    [[nodiscard]] std::vector<std::string> GetPrimitiveNames() const;

    /**
     * @brief Apply pose to primitives
     */
    void ApplyPose(const std::unordered_map<std::string, SDFTransform>& pose);

    /**
     * @brief Get current pose
     */
    [[nodiscard]] std::unordered_map<std::string, SDFTransform> GetCurrentPose() const;

    /**
     * @brief Reset to bind pose
     */
    void ResetToBindPose();

    /**
     * @brief Store current pose as bind pose
     */
    void SetBindPose();

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] std::string ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    bool FromJson(const std::string& json);

    /**
     * @brief Save to file
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief Load from file
     */
    bool LoadFromFile(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnModified;
    std::function<void(SDFPrimitive*)> OnPrimitiveAdded;
    std::function<void(uint32_t)> OnPrimitiveRemoved;

private:
    float EvaluateHierarchy(const SDFPrimitive* primitive, const glm::vec3& worldPoint) const;
    void CollectPrimitives(SDFPrimitive* primitive, std::vector<SDFPrimitive*>& out);
    void CollectPrimitives(const SDFPrimitive* primitive, std::vector<const SDFPrimitive*>& out) const;

    static uint32_t s_nextId;

    uint32_t m_id;
    std::string m_name;

    std::unique_ptr<SDFPrimitive> m_root;
    std::unordered_map<std::string, SDFTransform> m_bindPose;

    // Mesh caching
    std::shared_ptr<Mesh> m_cachedMesh;
    SDFMeshSettings m_meshSettings;
    bool m_meshDirty = true;

    // Painting
    std::vector<PaintLayer> m_paintLayers;
    std::string m_baseTexturePath;

    // Material
    std::shared_ptr<Material> m_material;
};

/**
 * @brief Marching cubes mesh generation
 */
namespace MarchingCubes {
    /**
     * @brief Generate mesh from SDF function
     */
    [[nodiscard]] std::shared_ptr<Mesh> Generate(
        const std::function<float(const glm::vec3&)>& sdf,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const SDFMeshSettings& settings);

    /**
     * @brief Generate mesh vertices and indices
     */
    void Polygonize(
        const std::function<float(const glm::vec3&)>& sdf,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        int resolution,
        float isoLevel,
        std::vector<glm::vec3>& outPositions,
        std::vector<glm::vec3>& outNormals,
        std::vector<uint32_t>& outIndices);
}

} // namespace Nova
