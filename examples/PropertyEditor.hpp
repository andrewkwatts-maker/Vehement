#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <glm/glm.hpp>

/**
 * @brief Helper functions for rendering property editors in ImGui
 *
 * Supports nested JSON property editing with type detection and
 * appropriate widgets for different data types.
 */
namespace PropertyEditor {

/**
 * @brief Render a JSON property tree with edit widgets
 * @param label Label for the property tree
 * @param json JSON object to edit
 * @param readOnly Whether properties are read-only
 * @param overrides JSON object tracking which properties are overridden
 * @param onPropertyChanged Callback when a property value changes
 * @return True if any property was modified
 */
bool RenderPropertyTree(const char* label, nlohmann::json& json, bool readOnly = false,
                        const nlohmann::json* overrides = nullptr,
                        std::function<void(const std::string&)> onPropertyChanged = nullptr);

/**
 * @brief Render a single JSON value with appropriate widget
 * @param key Property key
 * @param value JSON value to edit
 * @param readOnly Whether the value is read-only
 * @param isOverridden Whether this property is overridden
 * @return True if the value was modified
 */
bool RenderJsonValue(const char* key, nlohmann::json& value, bool readOnly = false, bool isOverridden = false);

/**
 * @brief Render transform properties (position, rotation, scale)
 * @param position Position vector
 * @param rotation Rotation euler angles (degrees)
 * @param scale Scale vector
 * @return True if any transform was modified
 */
bool RenderTransformProperties(glm::vec3& position, glm::vec3& rotation, glm::vec3& scale);

/**
 * @brief Render a search/filter bar
 * @param filter String to filter by
 * @param placeholder Placeholder text
 */
void RenderFilterBar(std::string& filter, const char* placeholder = "Search properties...");

/**
 * @brief Check if a property path matches a filter string
 * @param propertyPath Full property path (e.g., "stats.health")
 * @param filter Filter string
 * @return True if matches
 */
bool MatchesFilter(const std::string& propertyPath, const std::string& filter);

/**
 * @brief Render an "Override" button next to a read-only property
 * @param propertyPath Full property path
 * @return True if button was clicked
 */
bool RenderOverrideButton(const std::string& propertyPath);

/**
 * @brief Render a "Reset to Default" button next to an overridden property
 * @param propertyPath Full property path
 * @return True if button was clicked
 */
bool RenderResetButton(const std::string& propertyPath);

/**
 * @brief Get color for property based on override status
 * @param isOverridden Whether property is overridden
 * @param isReadOnly Whether property is read-only
 * @return ImVec4 color
 */
ImVec4 GetPropertyColor(bool isOverridden, bool isReadOnly);

/**
 * @brief Helper to build property path from keys
 */
std::string BuildPropertyPath(const std::string& parentPath, const std::string& key);

} // namespace PropertyEditor
