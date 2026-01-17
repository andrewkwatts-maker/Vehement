#pragma once

#include "CollisionShape.hpp"
#include "CollisionBody.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Nova {

// Forward declarations
class Mesh;

/**
 * @brief Configuration for collision loaded from JSON files
 */
struct CollisionConfiguration {
    std::vector<CollisionShape> shapes;
    BodyType bodyType = BodyType::Static;
    float mass = 1.0f;
    uint32_t layer = CollisionLayer::Default;
    uint32_t mask = CollisionLayer::All;
    float linearDamping = 0.01f;
    float angularDamping = 0.05f;
    float gravityScale = 1.0f;

    /**
     * @brief Check if configuration is valid
     */
    [[nodiscard]] bool IsValid() const { return !shapes.empty(); }

    /**
     * @brief Create a CollisionBody from this configuration
     */
    [[nodiscard]] std::unique_ptr<CollisionBody> CreateBody() const;
};

/**
 * @brief Error types for collision config parsing
 */
enum class CollisionConfigError {
    FileNotFound,
    ParseError,
    InvalidFormat,
    MissingField,
    InvalidShapeType,
    MeshLoadFailed,
    InvalidMeshReference
};

/**
 * @brief Get error description string
 */
[[nodiscard]] const char* CollisionConfigErrorToString(CollisionConfigError error) noexcept;

/**
 * @brief Collision configuration parser
 *
 * Parses collision definitions from JSON configuration files.
 * Supports:
 * - Inline shape definitions
 * - External collision mesh references
 * - Compound shapes
 * - Procedural shape generation from model bounds
 */
class CollisionConfigParser {
public:
    CollisionConfigParser() = default;
    ~CollisionConfigParser() = default;

    /**
     * @brief Set the base path for resolving relative mesh paths
     */
    void SetBasePath(const std::filesystem::path& path) { m_basePath = path; }

    /**
     * @brief Parse collision configuration from JSON
     * @param json JSON object containing collision config
     * @return Parsed configuration or error
     */
    [[nodiscard]] std::optional<CollisionConfiguration> Parse(
        const nlohmann::json& json) const;

    /**
     * @brief Parse collision configuration from file
     * @param filepath Path to JSON file
     * @return Parsed configuration or error
     */
    [[nodiscard]] std::optional<CollisionConfiguration> ParseFile(
        const std::filesystem::path& filepath) const;

    /**
     * @brief Parse collision configuration from JSON string
     */
    [[nodiscard]] std::optional<CollisionConfiguration> ParseString(
        const std::string& jsonString) const;

    /**
     * @brief Generate collision from model bounds
     * @param boundsMin Minimum corner of model AABB
     * @param boundsMax Maximum corner of model AABB
     * @param shapeType Preferred shape type (Box, Sphere, Capsule)
     * @return Generated collision shape
     */
    [[nodiscard]] static CollisionShape GenerateFromBounds(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        ShapeType shapeType = ShapeType::Box);

    /**
     * @brief Generate collision from mesh data
     * @param vertices Mesh vertices
     * @param indices Mesh indices (optional, for triangle mesh)
     * @param convex If true, generate convex hull; otherwise triangle mesh
     * @return Generated collision shape
     */
    [[nodiscard]] static CollisionShape GenerateFromMesh(
        const std::vector<glm::vec3>& vertices,
        const std::vector<uint32_t>& indices = {},
        bool convex = true);

    /**
     * @brief Load collision mesh from file
     * @param filepath Path to mesh file (.obj, .collider, etc.)
     * @return Loaded collision shape or error
     */
    [[nodiscard]] std::optional<CollisionShape> LoadCollisionMesh(
        const std::filesystem::path& filepath) const;

private:
    /**
     * @brief Parse a single shape from JSON
     */
    [[nodiscard]] std::optional<CollisionShape> ParseShape(
        const nlohmann::json& json) const;

    /**
     * @brief Parse compound shape from JSON
     */
    [[nodiscard]] std::optional<CollisionShape> ParseCompoundShape(
        const nlohmann::json& json) const;

    /**
     * @brief Resolve mesh path relative to base path
     */
    [[nodiscard]] std::filesystem::path ResolvePath(const std::string& path) const;

    std::filesystem::path m_basePath;
};

/**
 * @brief Cache for loaded collision configurations
 *
 * Prevents redundant parsing and loading of collision configs.
 */
class CollisionConfigCache {
public:
    static CollisionConfigCache& Instance();

    // Delete copy/move for singleton
    CollisionConfigCache(const CollisionConfigCache&) = delete;
    CollisionConfigCache& operator=(const CollisionConfigCache&) = delete;

    /**
     * @brief Get or load collision configuration
     * @param filepath Path to configuration file
     * @return Cached configuration or nullptr if failed
     */
    [[nodiscard]] const CollisionConfiguration* Get(const std::filesystem::path& filepath);

    /**
     * @brief Get or load collision configuration from JSON
     * @param key Unique key for caching
     * @param json JSON configuration
     * @return Cached configuration or nullptr if failed
     */
    [[nodiscard]] const CollisionConfiguration* GetFromJson(
        const std::string& key,
        const nlohmann::json& json);

    /**
     * @brief Clear all cached configurations
     */
    void Clear();

    /**
     * @brief Remove specific configuration from cache
     */
    void Remove(const std::string& key);

    /**
     * @brief Get number of cached configurations
     */
    [[nodiscard]] size_t Size() const { return m_cache.size(); }

    /**
     * @brief Set the base path for the parser
     */
    void SetBasePath(const std::filesystem::path& path) { m_parser.SetBasePath(path); }

private:
    CollisionConfigCache() = default;
    ~CollisionConfigCache() = default;

    CollisionConfigParser m_parser;
    std::unordered_map<std::string, CollisionConfiguration> m_cache;
};

/**
 * @brief Entity collision configuration schema
 *
 * Defines the expected JSON format for entity collision configurations.
 * Used for documentation and validation.
 */
namespace CollisionSchema {

/**
 * @brief Example JSON schema for collision configuration:
 *
 * {
 *   "collision": {
 *     "shapes": [
 *       {
 *         "type": "box|sphere|capsule|cylinder|convex_hull|triangle_mesh|compound",
 *
 *         // Box parameters
 *         "half_extents": [0.5, 0.5, 0.5],
 *
 *         // Sphere parameters
 *         "radius": 0.5,
 *
 *         // Capsule/Cylinder parameters
 *         "radius": 0.3,
 *         "height": 1.8,
 *
 *         // Convex hull parameters
 *         "vertices": [[x, y, z], ...],
 *         "mesh_file": "path/to/mesh.obj",  // Alternative to inline vertices
 *
 *         // Triangle mesh parameters
 *         "mesh_file": "path/to/mesh.obj",
 *
 *         // Compound shape
 *         "children": [{ shape definitions... }],
 *
 *         // Common optional parameters
 *         "offset": [0.0, 0.0, 0.0],
 *         "rotation": [0.0, 0.0, 0.0],  // Euler angles in degrees
 *
 *         // Material properties
 *         "material": {
 *           "friction": 0.5,
 *           "restitution": 0.3,
 *           "density": 1.0
 *         },
 *
 *         // Trigger support
 *         "is_trigger": false,
 *         "trigger_event": "event_name"
 *       }
 *     ],
 *
 *     // Body configuration
 *     "body_type": "static|kinematic|dynamic",
 *     "mass": 1.0,
 *
 *     // Collision filtering
 *     "layer": "unit",
 *     "mask": ["terrain", "unit", "building"],
 *
 *     // Physics properties
 *     "linear_damping": 0.01,
 *     "angular_damping": 0.05,
 *     "gravity_scale": 1.0,
 *
 *     // Auto-generation options
 *     "auto_generate": {
 *       "from_model": true,
 *       "shape_type": "box|sphere|capsule",
 *       "padding": 0.0
 *     }
 *   }
 * }
 */

/**
 * @brief Validate JSON against collision schema
 */
[[nodiscard]] bool Validate(const nlohmann::json& json, std::string& error);

/**
 * @brief Get default collision configuration JSON
 */
[[nodiscard]] nlohmann::json GetDefault();

} // namespace CollisionSchema

} // namespace Nova
