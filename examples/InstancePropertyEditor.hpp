#pragma once

#include "scene/InstanceManager.hpp"
#include "scene/InstanceData.hpp"
#include "PropertyEditor.hpp"
#include <memory>
#include <string>
#include <chrono>

/**
 * @brief Extended property editor for instance-specific properties
 *
 * Integrates with StandaloneEditor to provide:
 * - Archetype property viewing (read-only)
 * - Instance override editing
 * - Transform editing
 * - Custom data editing
 * - Auto-save with debouncing
 */
class InstancePropertyEditor {
public:
    InstancePropertyEditor();
    ~InstancePropertyEditor() = default;

    /**
     * @brief Initialize the property editor
     * @param instanceManager Pointer to instance manager
     */
    void Initialize(Nova::InstanceManager* instanceManager);

    /**
     * @brief Render the property editor panel for a selected object
     * @param selectedInstanceId Instance ID of selected object (empty if none)
     * @param transform Current transform (position, rotation, scale) for preview
     */
    void RenderPanel(const std::string& selectedInstanceId,
                     glm::vec3& position, glm::vec3& rotation, glm::vec3& scale);

    /**
     * @brief Update auto-save logic
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Force save all pending changes
     * @param mapName Current map name
     */
    void SaveAll(const std::string& mapName);

    /**
     * @brief Check if there are unsaved changes
     */
    bool HasUnsavedChanges() const;

    /**
     * @brief Get number of dirty instances
     */
    int GetDirtyCount() const;

    /**
     * @brief Set auto-save delay (seconds)
     */
    void SetAutoSaveDelay(float seconds) { m_autoSaveDelay = seconds; }

    /**
     * @brief Enable/disable auto-save
     */
    void SetAutoSaveEnabled(bool enabled) { m_autoSaveEnabled = enabled; }

private:
    /**
     * @brief Render archetype properties (read-only)
     */
    void RenderArchetypeProperties(const nlohmann::json& archetypeConfig,
                                    const Nova::InstanceData& instance);

    /**
     * @brief Render instance overrides (editable)
     */
    void RenderInstanceOverrides(Nova::InstanceData& instance);

    /**
     * @brief Render custom data section
     */
    void RenderCustomData(Nova::InstanceData& instance);

    /**
     * @brief Render header with instance info and actions
     */
    void RenderHeader(const Nova::InstanceData& instance);

    /**
     * @brief Trigger auto-save timer
     */
    void MarkDirty();

    /**
     * @brief Check if auto-save should trigger
     */
    bool ShouldAutoSave() const;

    Nova::InstanceManager* m_instanceManager = nullptr;

    // Auto-save state
    bool m_autoSaveEnabled = true;
    float m_autoSaveDelay = 2.0f;  // Seconds
    std::chrono::steady_clock::time_point m_lastChangeTime;
    bool m_hasPendingChanges = false;

    // UI state
    std::string m_propertyFilter;
    bool m_showArchetypeProperties = true;
    bool m_showInstanceOverrides = true;
    bool m_showCustomData = true;
    bool m_showTransform = true;

    // Current editing state
    std::string m_currentInstanceId;
    std::string m_currentMapName;
};
