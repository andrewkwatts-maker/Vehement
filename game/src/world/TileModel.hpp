#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Nova {
    class Mesh;
    class Texture;
    class Shader;
    class Material;
}

namespace Vehement {

/**
 * @brief LOD (Level of Detail) configuration for tile models
 */
struct TileModelLOD {
    float distance = 0.0f;      // Distance at which this LOD becomes active
    int vertexReduction = 0;    // Percentage of vertices removed (0 = full detail)
    bool skipNormalMap = false; // Skip normal mapping at this LOD
};

/**
 * @brief Data structure defining 3D model properties for tiles/objects
 *
 * Contains all configuration needed to load, position, and interact
 * with a 3D model used for tiles or world objects.
 */
struct TileModelData {
    // Model file paths
    std::string modelPath;          // Path to .obj/.gltf file
    std::string texturePath;        // Diffuse texture path
    std::string normalMapPath;      // Optional normal map path
    std::string specularMapPath;    // Optional specular map path
    std::string emissiveMapPath;    // Optional emissive map path

    // Transform defaults
    glm::vec3 defaultScale = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 pivotOffset = glm::vec3(0.0f, 0.0f, 0.0f);  // Offset from tile center
    glm::vec3 rotationOffset = glm::vec3(0.0f, 0.0f, 0.0f); // Default rotation (euler degrees)

    // Shadow properties
    bool castsShadow = true;
    bool receivesShadow = true;

    // Tile footprint (how many tiles the model occupies)
    glm::ivec2 footprintXY = glm::ivec2(1, 1);  // X and Y tile count
    int footprintZ = 1;                          // Z (height) levels

    // Collision properties
    bool isSolid = true;            // Blocks entity movement
    bool blocksLight = true;        // Blocks light rays for shadows/visibility
    bool blocksProjectiles = true;  // Blocks projectile movement

    // Simplified collision shape (convex hull vertices)
    std::vector<glm::vec3> collisionHull;

    // Bounding box (auto-calculated if collision hull is empty)
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(1.0f);

    // LOD levels
    std::vector<TileModelLOD> lodLevels;

    // Animation support
    bool hasAnimation = false;
    std::string defaultAnimation;

    // Rendering hints
    bool isTransparent = false;     // Uses alpha blending
    bool doubleSided = false;       // Render both sides of faces
    int renderQueue = 1000;         // Render order priority

    /**
     * @brief Calculate bounds from collision hull
     */
    void CalculateBoundsFromHull() {
        if (collisionHull.empty()) return;

        boundsMin = collisionHull[0];
        boundsMax = collisionHull[0];

        for (const auto& v : collisionHull) {
            boundsMin = glm::min(boundsMin, v);
            boundsMax = glm::max(boundsMax, v);
        }
    }

    /**
     * @brief Create default collision hull as an AABB
     */
    void CreateDefaultCollisionHull() {
        collisionHull.clear();

        // 8 corners of the bounding box
        collisionHull.push_back(glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z));
        collisionHull.push_back(glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z));
        collisionHull.push_back(glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z));
        collisionHull.push_back(glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z));
        collisionHull.push_back(glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z));
        collisionHull.push_back(glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z));
        collisionHull.push_back(glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z));
        collisionHull.push_back(glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z));
    }
};

/**
 * @brief 3D model representation for tiles and world objects
 *
 * Handles loading, rendering, and LOD management for 3D models
 * used in the tile-based world system. Supports:
 * - Multiple file formats (.obj, .gltf, .fbx via engine ModelLoader)
 * - Texture mapping (diffuse, normal, specular, emissive)
 * - Level of Detail (LOD) switching
 * - Instanced rendering for performance
 */
class TileModel {
public:
    TileModel();
    ~TileModel();

    // Non-copyable, movable
    TileModel(const TileModel&) = delete;
    TileModel& operator=(const TileModel&) = delete;
    TileModel(TileModel&& other) noexcept;
    TileModel& operator=(TileModel&& other) noexcept;

    /**
     * @brief Load model from file
     * @param path Path to the model file (.obj, .gltf, .fbx, etc.)
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Load model with full configuration
     * @param data Model configuration data
     * @return true if loaded successfully
     */
    bool LoadFromData(const TileModelData& data);

    /**
     * @brief Create model from existing mesh data (for procedural models)
     * @param mesh The mesh to use
     * @param texturePath Optional texture path
     * @return true if created successfully
     */
    bool CreateFromMesh(std::unique_ptr<Nova::Mesh> mesh, const std::string& texturePath = "");

    /**
     * @brief Render the model with a transform
     * @param transform Model matrix for positioning
     */
    void Render(const glm::mat4& transform);

    /**
     * @brief Render the model with shader override
     * @param transform Model matrix for positioning
     * @param shader Custom shader to use
     */
    void Render(const glm::mat4& transform, Nova::Shader& shader);

    /**
     * @brief Render for shadow pass (depth only)
     * @param transform Model matrix for positioning
     * @param lightSpaceMatrix Light's view-projection matrix
     */
    void RenderShadow(const glm::mat4& transform, const glm::mat4& lightSpaceMatrix);

    /**
     * @brief Set current LOD level
     * @param level LOD level (0 = full detail, higher = less detail)
     */
    void SetLOD(int level);

    /**
     * @brief Get current LOD level
     */
    int GetLOD() const { return m_currentLOD; }

    /**
     * @brief Automatically select LOD based on distance
     * @param cameraDistance Distance from camera to model
     */
    void UpdateLODFromDistance(float cameraDistance);

    // Texture binding

    /**
     * @brief Set diffuse texture
     * @param texture Texture to use
     */
    void SetTexture(std::shared_ptr<Nova::Texture> texture);

    /**
     * @brief Set normal map
     * @param normalMap Normal map texture
     */
    void SetNormalMap(std::shared_ptr<Nova::Texture> normalMap);

    /**
     * @brief Set specular map
     * @param specularMap Specular map texture
     */
    void SetSpecularMap(std::shared_ptr<Nova::Texture> specularMap);

    /**
     * @brief Set emissive map
     * @param emissiveMap Emissive map texture
     */
    void SetEmissiveMap(std::shared_ptr<Nova::Texture> emissiveMap);

    /**
     * @brief Bind all textures for rendering
     */
    void BindTextures();

    /**
     * @brief Unbind all textures
     */
    void UnbindTextures();

    // Accessors

    /**
     * @brief Get model data configuration
     */
    const TileModelData& GetData() const { return m_data; }

    /**
     * @brief Get model data for modification
     */
    TileModelData& GetData() { return m_data; }

    /**
     * @brief Check if model is valid and ready for rendering
     */
    bool IsValid() const { return m_isValid; }

    /**
     * @brief Get model's unique identifier
     */
    const std::string& GetId() const { return m_id; }

    /**
     * @brief Set model's unique identifier
     */
    void SetId(const std::string& id) { m_id = id; }

    /**
     * @brief Get bounding box minimum (local space)
     */
    const glm::vec3& GetBoundsMin() const { return m_data.boundsMin; }

    /**
     * @brief Get bounding box maximum (local space)
     */
    const glm::vec3& GetBoundsMax() const { return m_data.boundsMax; }

    /**
     * @brief Get bounding box center (local space)
     */
    glm::vec3 GetBoundsCenter() const {
        return (m_data.boundsMin + m_data.boundsMax) * 0.5f;
    }

    /**
     * @brief Get bounding box size
     */
    glm::vec3 GetBoundsSize() const {
        return m_data.boundsMax - m_data.boundsMin;
    }

    /**
     * @brief Get bounding sphere radius
     */
    float GetBoundingRadius() const { return m_boundingRadius; }

    /**
     * @brief Get vertex count (current LOD)
     */
    uint32_t GetVertexCount() const;

    /**
     * @brief Get index count (current LOD)
     */
    uint32_t GetIndexCount() const;

    /**
     * @brief Get underlying mesh (for advanced rendering)
     */
    Nova::Mesh* GetMesh() const { return m_mesh.get(); }

    /**
     * @brief Cleanup all resources
     */
    void Cleanup();

private:
    /**
     * @brief Load textures from data paths
     */
    bool LoadTextures();

    /**
     * @brief Calculate bounding sphere radius
     */
    void CalculateBoundingRadius();

    /**
     * @brief Generate LOD meshes
     */
    void GenerateLODs();

    std::string m_id;
    TileModelData m_data;
    bool m_isValid = false;

    // OpenGL resources
    uint32_t m_vao = 0;
    uint32_t m_vbo = 0;
    uint32_t m_ebo = 0;
    int m_indexCount = 0;

    // Engine mesh (preferred)
    std::unique_ptr<Nova::Mesh> m_mesh;

    // LOD meshes
    std::vector<std::unique_ptr<Nova::Mesh>> m_lodMeshes;
    int m_currentLOD = 0;

    // Textures
    std::shared_ptr<Nova::Texture> m_texture;
    std::shared_ptr<Nova::Texture> m_normalMap;
    std::shared_ptr<Nova::Texture> m_specularMap;
    std::shared_ptr<Nova::Texture> m_emissiveMap;

    // Cached calculations
    float m_boundingRadius = 1.0f;
};

/**
 * @brief Instance data for batch rendering of tile models
 */
struct TileModelInstance {
    glm::mat4 transform;        // World transform matrix
    glm::vec4 color;            // Color tint (RGBA)
    glm::vec4 customData;       // User-defined data (animation, etc.)
};

/**
 * @brief Batch renderer for multiple instances of the same model
 */
class TileModelBatch {
public:
    TileModelBatch();
    ~TileModelBatch();

    /**
     * @brief Initialize batch for a specific model
     * @param model The model to batch render
     * @param maxInstances Maximum number of instances
     */
    bool Initialize(TileModel* model, int maxInstances = 1000);

    /**
     * @brief Add instance to the batch
     * @param transform Instance transform
     * @param color Instance color tint
     * @return Instance index or -1 if batch is full
     */
    int AddInstance(const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief Update instance transform
     * @param index Instance index
     * @param transform New transform
     */
    void UpdateInstance(int index, const glm::mat4& transform);

    /**
     * @brief Update instance color
     * @param index Instance index
     * @param color New color
     */
    void UpdateInstanceColor(int index, const glm::vec4& color);

    /**
     * @brief Remove instance from batch
     * @param index Instance index
     */
    void RemoveInstance(int index);

    /**
     * @brief Clear all instances
     */
    void Clear();

    /**
     * @brief Upload instance data to GPU
     */
    void Upload();

    /**
     * @brief Render all instances
     */
    void Render();

    /**
     * @brief Render all instances with custom shader
     * @param shader Shader to use
     */
    void Render(Nova::Shader& shader);

    /**
     * @brief Get instance count
     */
    int GetInstanceCount() const { return static_cast<int>(m_instances.size()); }

    /**
     * @brief Check if batch is full
     */
    bool IsFull() const { return static_cast<int>(m_instances.size()) >= m_maxInstances; }

    /**
     * @brief Check if batch needs uploading
     */
    bool IsDirty() const { return m_dirty; }

private:
    TileModel* m_model = nullptr;
    int m_maxInstances = 1000;

    std::vector<TileModelInstance> m_instances;
    std::vector<int> m_freeIndices;  // Recycled indices

    uint32_t m_instanceVBO = 0;
    bool m_dirty = true;
};

} // namespace Vehement
