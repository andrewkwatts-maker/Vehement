#pragma once

#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace Nova {

/**
 * @brief Instance-specific data for scene objects
 *
 * Stores per-instance property overrides and custom data that differ from
 * the base archetype configuration. This allows objects to share a common
 * archetype while having unique instance-specific properties.
 */
struct InstanceData {
    // Identification
    std::string archetypeId;  // Reference to base config (e.g., "humans.units.footman")
    std::string instanceId;   // Unique instance ID (generated UUID or custom)

    // Transform
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Property overrides (nested JSON structure)
    // Example: {"stats.health": 150, "stats.damage": 15, "name": "Captain Footman"}
    nlohmann::json overrides;

    // Instance-specific custom data
    // Example: {"quest_giver": true, "dialog_id": "quest_001"}
    nlohmann::json customData;

    // Metadata
    std::string name;  // Display name for this instance
    bool isDirty = false;  // Has unsaved changes

    /**
     * @brief Default constructor
     */
    InstanceData() = default;

    /**
     * @brief Create instance from archetype
     */
    explicit InstanceData(const std::string& archetype)
        : archetypeId(archetype) {}

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static InstanceData FromJson(const nlohmann::json& json);

    /**
     * @brief Load from file
     */
    static InstanceData LoadFromFile(const std::string& path);

    /**
     * @brief Save to file
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief Generate a unique instance ID
     */
    static std::string GenerateInstanceId();

    /**
     * @brief Check if a property is overridden
     */
    [[nodiscard]] bool HasOverride(const std::string& propertyPath) const;

    /**
     * @brief Get an overridden property value
     */
    template<typename T>
    [[nodiscard]] T GetOverride(const std::string& propertyPath, const T& defaultValue) const {
        try {
            // Navigate nested path (e.g., "stats.health")
            nlohmann::json current = overrides;
            size_t start = 0;
            size_t end = propertyPath.find('.');

            while (end != std::string::npos) {
                std::string key = propertyPath.substr(start, end - start);
                if (!current.contains(key)) {
                    return defaultValue;
                }
                current = current[key];
                start = end + 1;
                end = propertyPath.find('.', start);
            }

            std::string finalKey = propertyPath.substr(start);
            if (current.contains(finalKey)) {
                return current[finalKey].get<T>();
            }
        } catch (...) {
            // Return default if any error occurs
        }
        return defaultValue;
    }

    /**
     * @brief Set an overridden property value
     */
    template<typename T>
    void SetOverride(const std::string& propertyPath, const T& value) {
        // Navigate/create nested path
        nlohmann::json* current = &overrides;
        size_t start = 0;
        size_t end = propertyPath.find('.');

        while (end != std::string::npos) {
            std::string key = propertyPath.substr(start, end - start);
            if (!current->contains(key)) {
                (*current)[key] = nlohmann::json::object();
            }
            current = &(*current)[key];
            start = end + 1;
            end = propertyPath.find('.', start);
        }

        std::string finalKey = propertyPath.substr(start);
        (*current)[finalKey] = value;
        isDirty = true;
    }

    /**
     * @brief Remove a property override
     */
    void RemoveOverride(const std::string& propertyPath);

    /**
     * @brief Clear all overrides
     */
    void ClearOverrides() {
        overrides = nlohmann::json::object();
        isDirty = true;
    }

    /**
     * @brief Set custom data property
     */
    template<typename T>
    void SetCustomData(const std::string& key, const T& value) {
        customData[key] = value;
        isDirty = true;
    }

    /**
     * @brief Get custom data property
     */
    template<typename T>
    [[nodiscard]] T GetCustomData(const std::string& key, const T& defaultValue) const {
        if (customData.contains(key)) {
            try {
                return customData[key].get<T>();
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    /**
     * @brief Check if has custom data
     */
    [[nodiscard]] bool HasCustomData(const std::string& key) const {
        return customData.contains(key);
    }
};

} // namespace Nova
