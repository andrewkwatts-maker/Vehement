#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace Nova {

using json = nlohmann::json;

/**
 * @brief Platform types for platform-specific overrides
 */
enum class Platform {
    All,
    Windows,
    Mac,
    Linux,
    iOS,
    Android,
    WebGL
};

/**
 * @brief Animation configuration validation error
 */
struct ValidationError {
    std::string path;           // JSON path to error
    std::string message;
    std::string schemaRule;     // Schema rule that failed
    bool isWarning = false;

    [[nodiscard]] std::string ToString() const;
};

/**
 * @brief Schema validation result
 */
struct ValidationResult {
    bool valid = true;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;

    [[nodiscard]] std::string GetErrorSummary() const;
};

/**
 * @brief JSON schema validator for animation configs
 */
class AnimationSchemaValidator {
public:
    AnimationSchemaValidator() = default;

    /**
     * @brief Load schema from file
     */
    bool LoadSchema(const std::string& filepath);

    /**
     * @brief Load schema from JSON
     */
    bool LoadSchemaFromJson(const json& schema);

    /**
     * @brief Validate config against loaded schema
     */
    [[nodiscard]] ValidationResult Validate(const json& config) const;

    /**
     * @brief Validate config file
     */
    [[nodiscard]] ValidationResult ValidateFile(const std::string& filepath) const;

    /**
     * @brief Get registered schema
     */
    [[nodiscard]] const json& GetSchema() const { return m_schema; }

    /**
     * @brief Check if schema is loaded
     */
    [[nodiscard]] bool HasSchema() const { return !m_schema.is_null(); }

private:
    void ValidateNode(const json& config, const json& schema,
                      const std::string& path, ValidationResult& result) const;
    void ValidateType(const json& value, const std::string& expectedType,
                      const std::string& path, ValidationResult& result) const;
    void ValidateEnum(const json& value, const json& enumValues,
                      const std::string& path, ValidationResult& result) const;
    void ValidatePattern(const std::string& value, const std::string& pattern,
                         const std::string& path, ValidationResult& result) const;

    json m_schema;
};

/**
 * @brief Hot-reloadable animation configuration
 */
class AnimationConfig {
public:
    using ReloadCallback = std::function<void(const AnimationConfig&)>;

    AnimationConfig() = default;
    explicit AnimationConfig(const std::string& filepath);

    /**
     * @brief Load configuration from file
     */
    bool LoadFromFile(const std::string& filepath);

    /**
     * @brief Load configuration from JSON
     */
    bool LoadFromJson(const json& config);

    /**
     * @brief Save configuration to file
     */
    bool SaveToFile(const std::string& filepath) const;
    bool SaveToFile() const;

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Reload configuration from file
     */
    bool Reload();

    /**
     * @brief Enable/disable hot-reload watching
     */
    void SetHotReload(bool enable);
    [[nodiscard]] bool IsHotReloadEnabled() const { return m_hotReloadEnabled; }

    /**
     * @brief Check for file changes and reload if necessary
     */
    void CheckAndReload();

    /**
     * @brief Register reload callback
     */
    void OnReload(ReloadCallback callback);

    // -------------------------------------------------------------------------
    // Configuration Access
    // -------------------------------------------------------------------------

    /**
     * @brief Get configuration ID
     */
    [[nodiscard]] const std::string& GetId() const { return m_id; }

    /**
     * @brief Get configuration name
     */
    [[nodiscard]] const std::string& GetName() const { return m_name; }

    /**
     * @brief Get raw JSON data
     */
    [[nodiscard]] const json& GetData() const { return m_data; }

    /**
     * @brief Get value by path (e.g., "states.idle.speed")
     */
    [[nodiscard]] json GetValue(const std::string& path) const;

    /**
     * @brief Get value with default
     */
    template<typename T>
    [[nodiscard]] T GetValue(const std::string& path, const T& defaultValue) const {
        json value = GetValue(path);
        if (value.is_null()) {
            return defaultValue;
        }
        try {
            return value.get<T>();
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Set value by path
     */
    void SetValue(const std::string& path, const json& value);

    // -------------------------------------------------------------------------
    // Inheritance
    // -------------------------------------------------------------------------

    /**
     * @brief Set base configuration to inherit from
     */
    void SetBase(const std::string& baseConfigPath);

    /**
     * @brief Apply inheritance from base config
     */
    bool ApplyInheritance(const std::unordered_map<std::string, std::shared_ptr<AnimationConfig>>& configs);

    /**
     * @brief Get base config path
     */
    [[nodiscard]] const std::string& GetBasePath() const { return m_basePath; }

    // -------------------------------------------------------------------------
    // Platform Overrides
    // -------------------------------------------------------------------------

    /**
     * @brief Set current platform for platform-specific values
     */
    static void SetCurrentPlatform(Platform platform);
    [[nodiscard]] static Platform GetCurrentPlatform();

    /**
     * @brief Apply platform-specific overrides
     */
    void ApplyPlatformOverrides();

    /**
     * @brief Get platform-specific value
     */
    [[nodiscard]] json GetPlatformValue(const std::string& path) const;

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    /**
     * @brief Validate configuration
     */
    [[nodiscard]] ValidationResult Validate() const;

    /**
     * @brief Set schema for validation
     */
    void SetSchema(std::shared_ptr<AnimationSchemaValidator> schema);

    /**
     * @brief Get file path
     */
    [[nodiscard]] const std::string& GetFilePath() const { return m_filepath; }

    /**
     * @brief Check if configuration is loaded
     */
    [[nodiscard]] bool IsLoaded() const { return m_loaded; }

    /**
     * @brief Get last modified time
     */
    [[nodiscard]] std::time_t GetLastModified() const { return m_lastModified; }

private:
    [[nodiscard]] json ResolveReferences(const json& data) const;
    void MergeJson(json& target, const json& source);
    [[nodiscard]] std::time_t GetFileModifiedTime(const std::string& filepath) const;

    std::string m_id;
    std::string m_name;
    std::string m_filepath;
    std::string m_basePath;
    json m_data;
    json m_platformOverrides;
    bool m_loaded = false;
    bool m_hotReloadEnabled = false;
    std::time_t m_lastModified = 0;

    std::vector<ReloadCallback> m_reloadCallbacks;
    std::shared_ptr<AnimationSchemaValidator> m_schema;

    static Platform s_currentPlatform;
};

/**
 * @brief Manager for animation configurations
 */
class AnimationConfigManager {
public:
    AnimationConfigManager() = default;

    /**
     * @brief Load configuration from file
     */
    AnimationConfig* Load(const std::string& filepath);

    /**
     * @brief Load all configurations from directory
     */
    void LoadDirectory(const std::string& directory, bool recursive = true);

    /**
     * @brief Get configuration by ID
     */
    [[nodiscard]] AnimationConfig* Get(const std::string& id);
    [[nodiscard]] const AnimationConfig* Get(const std::string& id) const;

    /**
     * @brief Get configuration by file path
     */
    [[nodiscard]] AnimationConfig* GetByPath(const std::string& filepath);

    /**
     * @brief Remove configuration
     */
    bool Remove(const std::string& id);

    /**
     * @brief Clear all configurations
     */
    void Clear();

    /**
     * @brief Get all configuration IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllIds() const;

    /**
     * @brief Reload all configurations
     */
    void ReloadAll();

    /**
     * @brief Check for file changes and reload modified configs
     */
    void CheckAndReloadAll();

    /**
     * @brief Apply inheritance for all configs
     */
    void ApplyAllInheritance();

    /**
     * @brief Set schema for validation
     */
    void SetSchema(const std::string& schemaPath);

    /**
     * @brief Validate all configurations
     */
    [[nodiscard]] std::unordered_map<std::string, ValidationResult> ValidateAll() const;

    /**
     * @brief Register global reload callback
     */
    void OnAnyReload(std::function<void(const std::string&, const AnimationConfig&)> callback);

private:
    std::unordered_map<std::string, std::shared_ptr<AnimationConfig>> m_configs;
    std::unordered_map<std::string, std::string> m_pathToId;
    std::shared_ptr<AnimationSchemaValidator> m_schema;
    std::vector<std::function<void(const std::string&, const AnimationConfig&)>> m_reloadCallbacks;
};

/**
 * @brief Builder for animation configurations
 */
class AnimationConfigBuilder {
public:
    AnimationConfigBuilder() = default;

    AnimationConfigBuilder& SetId(const std::string& id);
    AnimationConfigBuilder& SetName(const std::string& name);
    AnimationConfigBuilder& SetBase(const std::string& basePath);
    AnimationConfigBuilder& AddState(const std::string& name, const json& stateConfig);
    AnimationConfigBuilder& AddTransition(const std::string& from, const std::string& to, const json& config);
    AnimationConfigBuilder& AddParameter(const std::string& name, const std::string& type, const json& defaultValue);
    AnimationConfigBuilder& AddEvent(const std::string& stateName, float time, const std::string& eventName, const json& data = {});
    AnimationConfigBuilder& SetPlatformOverride(Platform platform, const std::string& path, const json& value);

    [[nodiscard]] AnimationConfig Build();
    [[nodiscard]] json ToJson() const;

private:
    json m_config;
};

} // namespace Nova
