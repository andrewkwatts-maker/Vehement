#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <optional>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

namespace Nova {

/**
 * @brief Supported configuration value types
 */
using ConfigValue = std::variant<
    bool,
    int,
    float,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4
>;

/**
 * @brief JSON-based configuration system for engine settings
 *
 * Replaces all hardcoded magic numbers with configurable values.
 * Supports hot-reloading and hierarchical configuration.
 */
class Config {
public:
    static Config& Instance();

    // Delete copy/move for singleton
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /**
     * @brief Load configuration from JSON file
     * @param filepath Path to configuration file
     * @return true if loaded successfully
     */
    bool Load(const std::filesystem::path& filepath);

    /**
     * @brief Save current configuration to JSON file
     * @param filepath Path to save to (uses loaded path if empty)
     * @return true if saved successfully
     */
    bool Save(const std::filesystem::path& filepath = "");

    /**
     * @brief Reload configuration from disk
     * @return true if reloaded successfully
     */
    bool Reload();

    /**
     * @brief Get a configuration value with type safety
     * @tparam T The expected type
     * @param key Dot-separated key path (e.g., "window.width")
     * @param defaultValue Value to return if key not found
     * @return The configuration value or default
     */
    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T{}) const;

    /**
     * @brief Set a configuration value
     * @tparam T The value type
     * @param key Dot-separated key path
     * @param value The value to set
     */
    template<typename T>
    void Set(const std::string& key, const T& value);

    /**
     * @brief Check if a key exists
     */
    bool Has(const std::string& key) const;

    /**
     * @brief Get the underlying JSON object for direct access
     */
    const nlohmann::json& GetJson() const { return m_data; }

    /**
     * @brief Create default configuration file
     */
    static void CreateDefault(const std::filesystem::path& filepath);

private:
    Config() = default;
    ~Config() = default;

    nlohmann::json m_data;
    std::filesystem::path m_filepath;
    mutable std::unordered_map<std::string, ConfigValue> m_cache;

    nlohmann::json* NavigateToKey(const std::string& key, bool create = false);
    const nlohmann::json* NavigateToKey(const std::string& key) const;
};

// Template implementations
template<typename T>
T Config::Get(const std::string& key, const T& defaultValue) const {
    const auto* node = NavigateToKey(key);
    if (!node || node->is_null()) {
        return defaultValue;
    }

    try {
        if constexpr (std::is_same_v<T, glm::vec2>) {
            if (node->is_array() && node->size() >= 2) {
                return glm::vec2((*node)[0].get<float>(), (*node)[1].get<float>());
            }
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (node->is_array() && node->size() >= 3) {
                return glm::vec3(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>()
                );
            }
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (node->is_array() && node->size() >= 4) {
                return glm::vec4(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>(),
                    (*node)[3].get<float>()
                );
            }
        } else {
            return node->get<T>();
        }
    } catch (...) {
        return defaultValue;
    }
    return defaultValue;
}

template<typename T>
void Config::Set(const std::string& key, const T& value) {
    auto* node = NavigateToKey(key, true);
    if (node) {
        if constexpr (std::is_same_v<T, glm::vec2>) {
            *node = nlohmann::json::array({value.x, value.y});
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            *node = nlohmann::json::array({value.x, value.y, value.z});
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            *node = nlohmann::json::array({value.x, value.y, value.z, value.w});
        } else {
            *node = value;
        }
    }
}

/**
 * @brief Window configuration defaults
 */
struct WindowConfig {
    int width = 1920;
    int height = 1080;
    std::string title = "Nova3D Engine";
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
    int samples = 4;  // MSAA samples
};

/**
 * @brief Camera configuration defaults
 */
struct CameraConfig {
    float fov = 45.0f;           // Degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float moveSpeed = 10.0f;
    float lookSpeed = 0.1f;
    glm::vec3 defaultPosition = glm::vec3(10.0f, 10.0f, 10.0f);
    glm::vec3 defaultTarget = glm::vec3(0.0f, 0.0f, 0.0f);
};

/**
 * @brief Rendering configuration defaults
 */
struct RenderConfig {
    glm::vec4 clearColor = glm::vec4(0.1f, 0.1f, 0.15f, 1.0f);
    bool enableShadows = true;
    int shadowMapSize = 2048;
    float shadowBias = 0.005f;
    bool enableHDR = false;
    float gamma = 2.2f;
};

/**
 * @brief Debug visualization configuration
 */
struct DebugConfig {
    bool showGrid = true;
    int gridSize = 20;
    float gridSpacing = 1.0f;
    glm::vec4 gridColor = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    glm::vec4 axisColorX = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 axisColorY = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 axisColorZ = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    bool showFPS = true;
    bool showStats = true;
};

/**
 * @brief Particle system configuration
 */
struct ParticleConfig {
    int maxParticles = 10000;
    float defaultLifespan = 2.0f;
    float defaultSize = 0.1f;
};

/**
 * @brief Terrain configuration
 */
struct TerrainConfig {
    int chunkSize = 64;
    int viewDistance = 4;
    float heightScale = 50.0f;
    float noiseFrequency = 0.02f;
    int octaves = 6;
    float persistence = 0.5f;
    float lacunarity = 2.0f;
};

} // namespace Nova
