#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <optional>
#include <optional>
#include <filesystem>
#include <fstream>
#include <shared_mutex>
#include <concepts>
#include <algorithm>
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
 * @brief Configuration error types
 */
enum class ConfigError {
    FileNotFound,
    ParseError,
    WriteError,
    KeyNotFound,
    TypeMismatch,
    ValidationFailed
};

/**
 * @brief Concept for types that can be stored in config
 */
template<typename T>
concept ConfigStorable = std::same_as<T, bool> ||
                         std::same_as<T, int> ||
                         std::same_as<T, float> ||
                         std::same_as<T, double> ||
                         std::same_as<T, std::string> ||
                         std::same_as<T, glm::vec2> ||
                         std::same_as<T, glm::vec3> ||
                         std::same_as<T, glm::vec4>;

/**
 * @brief Concept for numeric config types that support range validation
 */
template<typename T>
concept ConfigNumeric = std::same_as<T, int> ||
                        std::same_as<T, float> ||
                        std::same_as<T, double>;

/**
 * @brief JSON-based configuration system for engine settings
 *
 * Thread-safe, cached configuration with validation support.
 * Supports hot-reloading and hierarchical configuration.
 */
class Config {
public:
    static Config& Instance();

    // Delete copy/move for singleton
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;

    /**
     * @brief Load configuration from JSON file
     * @param filepath Path to configuration file
     * @return Expected success or error
     */
    [[nodiscard]] std::optional<ConfigError> Load(const std::filesystem::path& filepath);

    /**
     * @brief Save current configuration to JSON file
     * @param filepath Path to save to (uses loaded path if empty)
     * @return Expected success or error
     */
    [[nodiscard]] std::optional<ConfigError> Save(const std::filesystem::path& filepath = "");

    /**
     * @brief Reload configuration from disk
     * @return Expected success or error
     */
    [[nodiscard]] std::optional<ConfigError> Reload();

    /**
     * @brief Get a configuration value with type safety
     * @tparam T The expected type (must satisfy ConfigStorable)
     * @param key Dot-separated key path (e.g., "window.width")
     * @param defaultValue Value to return if key not found
     * @return The configuration value or default
     */
    template<ConfigStorable T>
    [[nodiscard]] T Get(std::string_view key, const T& defaultValue = T{}) const;

    /**
     * @brief Get a configuration value with error reporting
     * @tparam T The expected type (must satisfy ConfigStorable)
     * @param key Dot-separated key path
     * @return Expected value or error
     */
    template<ConfigStorable T>
    [[nodiscard]] std::optional<T> GetExpected(std::string_view key) const;

    /**
     * @brief Get a numeric value with range validation
     * @tparam T The numeric type
     * @param key Dot-separated key path
     * @param minVal Minimum allowed value
     * @param maxVal Maximum allowed value
     * @param defaultValue Value to return if key not found or out of range
     * @return The clamped configuration value or default
     */
    template<ConfigNumeric T>
    [[nodiscard]] T GetClamped(std::string_view key, T minVal, T maxVal, const T& defaultValue = T{}) const;

    /**
     * @brief Set a configuration value
     * @tparam T The value type (must satisfy ConfigStorable)
     * @param key Dot-separated key path
     * @param value The value to set
     */
    template<ConfigStorable T>
    void Set(std::string_view key, const T& value);

    /**
     * @brief Set a numeric value with range validation
     * @tparam T The numeric type
     * @param key Dot-separated key path
     * @param value The value to set
     * @param minVal Minimum allowed value
     * @param maxVal Maximum allowed value
     * @return true if value was within range, false if clamped
     */
    template<ConfigNumeric T>
    bool SetValidated(std::string_view key, T value, T minVal, T maxVal);

    /**
     * @brief Check if a key exists
     */
    [[nodiscard]] bool Has(std::string_view key) const;

    /**
     * @brief Get the underlying JSON object for direct access
     */
    [[nodiscard]] const nlohmann::json& GetJson() const {
        std::shared_lock lock(m_mutex);
        return m_data;
    }

    /**
     * @brief Clear the value cache (useful after external modifications)
     */
    void ClearCache();

    /**
     * @brief Create default configuration file
     */
    static void CreateDefault(const std::filesystem::path& filepath);

    // =========================================================================
    // Performance Optimizations
    // =========================================================================

    /**
     * @brief FNV-1a hash for compile-time string hashing
     */
    static constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
    static constexpr uint64_t FNV_PRIME = 1099511628211ull;

    static constexpr uint64_t HashKey(std::string_view key) {
        uint64_t hash = FNV_OFFSET;
        for (char c : key) {
            hash ^= static_cast<uint64_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    /**
     * @brief Get value with pre-computed hash (faster lookup)
     */
    template<ConfigStorable T>
    [[nodiscard]] T GetHashed(std::string_view key, uint64_t hash, const T& defaultValue = T{}) const;

    /**
     * @brief Register a key for fast lookup (pre-cache during initialization)
     */
    void RegisterFastLookup(std::string_view key);

    /**
     * @brief Pre-cache commonly accessed values for faster access
     */
    void BuildFastLookupTable();

private:
    Config() = default;
    ~Config() = default;

    nlohmann::json m_data;
    std::filesystem::path m_filepath;
    mutable std::unordered_map<std::string, ConfigValue> m_cache;
    mutable std::shared_mutex m_mutex;

    // Hash-based cache for O(1) lookups
    mutable std::unordered_map<uint64_t, ConfigValue> m_hashCache;

    nlohmann::json* NavigateToKey(std::string_view key, bool create = false);
    const nlohmann::json* NavigateToKey(std::string_view key) const;

    // Cache key helper
    static std::string MakeCacheKey(std::string_view key) { return std::string(key); }
};

/**
 * @brief Compile-time config key hashing macro
 */
#define NOVA_CONFIG_KEY(key) ::Nova::Config::HashKey(key)

// Template implementations
template<ConfigStorable T>
T Config::Get(std::string_view key, const T& defaultValue) const {
    std::shared_lock lock(m_mutex);

    // Check cache first
    auto cacheKey = MakeCacheKey(key);
    if (auto it = m_cache.find(cacheKey); it != m_cache.end()) {
        if (auto* val = std::get_if<T>(&it->second)) {
            return *val;
        }
    }

    const auto* node = NavigateToKey(key);
    if (!node || node->is_null()) {
        return defaultValue;
    }

    try {
        T result;
        if constexpr (std::is_same_v<T, glm::vec2>) {
            if (node->is_array() && node->size() >= 2) {
                result = glm::vec2((*node)[0].get<float>(), (*node)[1].get<float>());
            } else {
                return defaultValue;
            }
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (node->is_array() && node->size() >= 3) {
                result = glm::vec3(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>()
                );
            } else {
                return defaultValue;
            }
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (node->is_array() && node->size() >= 4) {
                result = glm::vec4(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>(),
                    (*node)[3].get<float>()
                );
            } else {
                return defaultValue;
            }
        } else {
            result = node->get<T>();
        }

        // Cache the result
        m_cache[cacheKey] = result;
        return result;
    } catch (...) {
        return defaultValue;
    }
}

template<ConfigStorable T>
std::optional<T> Config::GetExpected(std::string_view key) const {
    std::shared_lock lock(m_mutex);

    const auto* node = NavigateToKey(key);
    if (!node || node->is_null()) {
        return std::nullopt;
    }

    try {
        if constexpr (std::is_same_v<T, glm::vec2>) {
            if (node->is_array() && node->size() >= 2) {
                return glm::vec2((*node)[0].get<float>(), (*node)[1].get<float>());
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (node->is_array() && node->size() >= 3) {
                return glm::vec3(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>()
                );
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (node->is_array() && node->size() >= 4) {
                return glm::vec4(
                    (*node)[0].get<float>(),
                    (*node)[1].get<float>(),
                    (*node)[2].get<float>(),
                    (*node)[3].get<float>()
                );
            }
            return std::nullopt;
        } else {
            return node->get<T>();
        }
    } catch (...) {
        return std::nullopt;
    }
}

template<ConfigNumeric T>
T Config::GetClamped(std::string_view key, T minVal, T maxVal, const T& defaultValue) const {
    T value = Get<T>(key, defaultValue);
    return std::clamp(value, minVal, maxVal);
}

template<ConfigStorable T>
void Config::Set(std::string_view key, const T& value) {
    std::unique_lock lock(m_mutex);

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

        // Update cache
        m_cache[MakeCacheKey(key)] = value;
    }
}

template<ConfigNumeric T>
bool Config::SetValidated(std::string_view key, T value, T minVal, T maxVal) {
    bool inRange = (value >= minVal && value <= maxVal);
    T clampedValue = std::clamp(value, minVal, maxVal);
    Set<T>(key, clampedValue);
    return inRange;
}

// GetHashed implementation for optimized config access
template<ConfigStorable T>
T Config::GetHashed(std::string_view key, uint64_t hash, const T& defaultValue) const {
    std::shared_lock lock(m_mutex);

    // Check hash cache first (O(1) lookup)
    auto it = m_hashCache.find(hash);
    if (it != m_hashCache.end()) {
        if (auto* val = std::get_if<T>(&it->second)) {
            return *val;
        }
    }

    // Fall back to normal lookup and cache with hash
    const auto* node = NavigateToKey(key);
    if (!node || node->is_null()) {
        return defaultValue;
    }

    try {
        T result;
        if constexpr (std::is_same_v<T, glm::vec2>) {
            if (node->is_array() && node->size() >= 2) {
                result = glm::vec2((*node)[0].get<float>(), (*node)[1].get<float>());
            } else {
                return defaultValue;
            }
        } else if constexpr (std::is_same_v<T, glm::vec3>) {
            if (node->is_array() && node->size() >= 3) {
                result = glm::vec3((*node)[0].get<float>(), (*node)[1].get<float>(), (*node)[2].get<float>());
            } else {
                return defaultValue;
            }
        } else if constexpr (std::is_same_v<T, glm::vec4>) {
            if (node->is_array() && node->size() >= 4) {
                result = glm::vec4((*node)[0].get<float>(), (*node)[1].get<float>(),
                                   (*node)[2].get<float>(), (*node)[3].get<float>());
            } else {
                return defaultValue;
            }
        } else {
            result = node->get<T>();
        }

        // Cache with hash for future lookups
        m_hashCache[hash] = result;
        return result;
    } catch (...) {
        return defaultValue;
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
