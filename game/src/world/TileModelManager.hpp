#pragma once

#include "TileModel.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Shader;
    class TextureManager;
    class Camera;
}

namespace Vehement {

// Forward declarations
class ProceduralModels;

/**
 * @brief Model category for organization
 */
enum class TileModelCategory {
    Terrain,        // Ground, floors, terrain features
    Structure,      // Walls, buildings, roofs
    Prop,           // Decorative objects, furniture
    Interactive,    // Doors, switches, levers
    Nature,         // Trees, rocks, plants
    Industrial,     // Machines, pipes, containers
    Custom          // User-defined models
};

/**
 * @brief Model entry with metadata
 */
struct TileModelEntry {
    std::shared_ptr<TileModel> model;
    TileModelCategory category = TileModelCategory::Custom;
    std::string displayName;
    std::string description;
    std::vector<std::string> tags;
    bool isProcedural = false;
    int useCount = 0;  // Reference counting for caching
};

/**
 * @brief Configuration for model manager
 */
struct TileModelManagerConfig {
    std::string modelBasePath = "assets/models/";
    std::string textureBasePath = "assets/textures/";
    int maxCachedModels = 100;
    bool enableAutoLOD = true;
    bool preloadCommonModels = true;
    bool enableHotReload = false;
};

/**
 * @brief Manages all tile models with caching and procedural generation
 *
 * Features:
 * - Model loading with caching
 * - Procedural primitive generation
 * - Batch rendering for instanced models
 * - LOD management
 * - Hot reloading for development
 */
class TileModelManager {
public:
    TileModelManager();
    ~TileModelManager();

    // Non-copyable
    TileModelManager(const TileModelManager&) = delete;
    TileModelManager& operator=(const TileModelManager&) = delete;

    /**
     * @brief Initialize the manager
     * @param config Configuration settings
     * @return true if initialized successfully
     */
    bool Initialize(const TileModelManagerConfig& config = {});

    /**
     * @brief Shutdown and cleanup all resources
     */
    void Shutdown();

    /**
     * @brief Set texture manager for texture loading
     * @param textureManager Pointer to texture manager
     */
    void SetTextureManager(Nova::TextureManager* textureManager);

    // ========== Model Loading ==========

    /**
     * @brief Load a model from file (cached)
     * @param path Path to the model file (relative to base path)
     * @return Shared pointer to loaded model, or nullptr on failure
     */
    std::shared_ptr<TileModel> LoadModel(const std::string& path);

    /**
     * @brief Load a model with full configuration
     * @param id Unique identifier for the model
     * @param data Model configuration data
     * @return Shared pointer to loaded model, or nullptr on failure
     */
    std::shared_ptr<TileModel> LoadModel(const std::string& id, const TileModelData& data);

    /**
     * @brief Get a previously loaded model by ID
     * @param id Model identifier
     * @return Shared pointer to model, or nullptr if not found
     */
    std::shared_ptr<TileModel> GetModel(const std::string& id);

    /**
     * @brief Check if a model is loaded
     * @param id Model identifier
     * @return true if model is loaded
     */
    bool HasModel(const std::string& id) const;

    /**
     * @brief Unload a model and free its resources
     * @param id Model identifier
     */
    void UnloadModel(const std::string& id);

    /**
     * @brief Reload a model from disk (for hot reloading)
     * @param id Model identifier
     * @return true if reloaded successfully
     */
    bool ReloadModel(const std::string& id);

    // ========== Procedural Primitives ==========

    /**
     * @brief Create a textured cube
     * @param size Cube dimensions (width, height, depth)
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateCube(const glm::vec3& size, const std::string& texture = "");

    /**
     * @brief Create a hexagonal prism (for hex grids)
     * @param radius Hex radius (center to vertex)
     * @param height Prism height
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateHexPrism(float radius, float height, const std::string& texture = "");

    /**
     * @brief Create a cylinder
     * @param radius Cylinder radius
     * @param height Cylinder height
     * @param segments Number of segments around the cylinder
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateCylinder(float radius, float height, int segments = 16,
                                              const std::string& texture = "");

    /**
     * @brief Create a sphere
     * @param radius Sphere radius
     * @param segments Number of segments (latitude and longitude)
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateSphere(float radius, int segments = 16,
                                            const std::string& texture = "");

    /**
     * @brief Create a cone
     * @param radius Base radius
     * @param height Cone height
     * @param segments Number of segments
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateCone(float radius, float height, int segments = 16,
                                          const std::string& texture = "");

    /**
     * @brief Create a wedge (ramp shape)
     * @param size Wedge dimensions
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateWedge(const glm::vec3& size, const std::string& texture = "");

    /**
     * @brief Create a staircase
     * @param width Staircase width
     * @param height Total height
     * @param steps Number of steps
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateStairs(float width, float height, int steps,
                                            const std::string& texture = "");

    /**
     * @brief Create a plane/quad
     * @param width Plane width
     * @param depth Plane depth
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreatePlane(float width, float depth, const std::string& texture = "");

    /**
     * @brief Create a torus
     * @param innerRadius Inner ring radius
     * @param outerRadius Outer ring radius
     * @param segments Ring and tube segments
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateTorus(float innerRadius, float outerRadius, int segments = 16,
                                           const std::string& texture = "");

    // ========== Hex Grid Specific ==========

    /**
     * @brief Create a hex tile (flat-topped hexagon)
     * @param radius Hex radius
     * @param height Tile height/thickness
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateHexTile(float radius, float height, const std::string& texture = "");

    /**
     * @brief Create a hex wall segment
     * @param radius Hex radius this wall belongs to
     * @param height Wall height
     * @param side Which side (0-5)
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateHexWall(float radius, float height, int side,
                                             const std::string& texture = "");

    /**
     * @brief Create a hex corner piece
     * @param radius Hex radius
     * @param height Corner height
     * @param corner Which corner (0-5)
     * @param texture Texture path (optional)
     * @return Shared pointer to created model
     */
    std::shared_ptr<TileModel> CreateHexCorner(float radius, float height, int corner,
                                               const std::string& texture = "");

    // ========== Batch Rendering ==========

    /**
     * @brief Render multiple instances of a model
     * @param modelId Model identifier
     * @param transforms Transform matrices for each instance
     */
    void RenderInstanced(const std::string& modelId, const std::vector<glm::mat4>& transforms);

    /**
     * @brief Render multiple instances with colors
     * @param modelId Model identifier
     * @param transforms Transform matrices for each instance
     * @param colors Color tints for each instance
     */
    void RenderInstanced(const std::string& modelId, const std::vector<glm::mat4>& transforms,
                         const std::vector<glm::vec4>& colors);

    /**
     * @brief Begin batch rendering session
     * @param shader Shader to use for rendering
     * @param viewProjection View-projection matrix
     */
    void BeginBatch(Nova::Shader* shader, const glm::mat4& viewProjection);

    /**
     * @brief Add instance to current batch
     * @param modelId Model identifier
     * @param transform Instance transform
     * @param color Instance color tint
     */
    void AddToBatch(const std::string& modelId, const glm::mat4& transform,
                    const glm::vec4& color = glm::vec4(1.0f));

    /**
     * @brief End batch and render all instances
     */
    void EndBatch();

    /**
     * @brief Flush current batch (render immediately)
     */
    void FlushBatch();

    // ========== Model Management ==========

    /**
     * @brief Register a model with the manager
     * @param id Unique identifier
     * @param model Model to register
     * @param category Model category
     */
    void RegisterModel(const std::string& id, std::shared_ptr<TileModel> model,
                       TileModelCategory category = TileModelCategory::Custom);

    /**
     * @brief Get all models in a category
     * @param category Category to query
     * @return Vector of model IDs in the category
     */
    std::vector<std::string> GetModelsByCategory(TileModelCategory category) const;

    /**
     * @brief Get all loaded model IDs
     * @return Vector of all model IDs
     */
    std::vector<std::string> GetAllModelIds() const;

    /**
     * @brief Get model entry with metadata
     * @param id Model identifier
     * @return Pointer to entry or nullptr if not found
     */
    const TileModelEntry* GetModelEntry(const std::string& id) const;

    /**
     * @brief Clear all cached models
     */
    void ClearCache();

    /**
     * @brief Preload common placeholder models
     */
    void PreloadCommonModels();

    // ========== Statistics ==========

    struct ManagerStats {
        int totalModels = 0;
        int proceduralModels = 0;
        int loadedFromFile = 0;
        size_t totalVertices = 0;
        size_t totalIndices = 0;
        int batchedDrawCalls = 0;
        int instancesRendered = 0;
    };

    /**
     * @brief Get manager statistics
     */
    const ManagerStats& GetStats() const { return m_stats; }

    /**
     * @brief Reset frame statistics
     */
    void ResetFrameStats();

private:
    /**
     * @brief Generate unique ID for procedural model
     */
    std::string GenerateProceduralId(const std::string& prefix);

    /**
     * @brief Load texture for a model
     */
    void LoadModelTexture(TileModel& model, const std::string& texturePath);

    /**
     * @brief Create and register a procedural model
     */
    std::shared_ptr<TileModel> CreateProceduralModel(const std::string& id,
                                                      std::unique_ptr<Nova::Mesh> mesh,
                                                      const std::string& texture);

    TileModelManagerConfig m_config;
    bool m_initialized = false;

    // Model storage
    std::unordered_map<std::string, TileModelEntry> m_models;

    // Batch rendering state
    struct BatchState {
        Nova::Shader* shader = nullptr;
        glm::mat4 viewProjection;
        std::unordered_map<std::string, std::vector<TileModelInstance>> instancesByModel;
        bool active = false;
    };
    BatchState m_batchState;

    // Instance buffers for batch rendering
    std::unordered_map<std::string, std::unique_ptr<TileModelBatch>> m_instanceBatches;

    // External dependencies
    Nova::TextureManager* m_textureManager = nullptr;

    // Statistics
    ManagerStats m_stats;

    // Procedural model counter
    int m_proceduralCounter = 0;
};

} // namespace Vehement
