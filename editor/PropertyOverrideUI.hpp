#pragma once

#include "../engine/core/PropertySystem.hpp"
#include <imgui.h>
#include <functional>
#include <glm/glm.hpp>

namespace Nova3D {
namespace PropertyOverrideUI {

// Color scheme for override visualization
struct OverrideColors {
    ImVec4 inherited = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);    // White: Inherited from parent
    ImVec4 overridden = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);   // Yellow: Overridden at this level
    ImVec4 defaultValue = ImVec4(0.5f, 0.7f, 1.0f, 1.0f); // Blue: Using default value
    ImVec4 resetButton = ImVec4(0.7f, 0.3f, 0.3f, 1.0f);  // Red: Reset button
};

extern OverrideColors g_overrideColors;

// Get color based on override status
ImVec4 GetPropertyColor(PropertyLevel currentLevel, PropertyLevel overrideLevel);

// Helper to show override tooltip
void ShowOverrideTooltip(const PropertyMetadata& metadata, PropertyLevel currentLevel);

// Helper marker for tooltips
void HelpMarker(const char* desc);

// Render float property with override visualization
bool RenderFloat(const char* label,
                float& value,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(float)> onChange = nullptr,
                float min = 0.0f,
                float max = 1.0f,
                const char* tooltip = nullptr,
                const char* format = "%.3f");

// Render int property with override visualization
bool RenderInt(const char* label,
              int& value,
              PropertyContainer* container,
              PropertyLevel currentLevel,
              std::function<void(int)> onChange = nullptr,
              int min = 0,
              int max = 100,
              const char* tooltip = nullptr);

// Render bool property with override visualization
bool RenderBool(const char* label,
               bool& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(bool)> onChange = nullptr,
               const char* tooltip = nullptr);

// Render vec2 property with override visualization
bool RenderVec2(const char* label,
               glm::vec2& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec2)> onChange = nullptr,
               float min = 0.0f,
               float max = 1.0f,
               const char* tooltip = nullptr);

// Render vec3 property with override visualization
bool RenderVec3(const char* label,
               glm::vec3& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec3)> onChange = nullptr,
               float min = 0.0f,
               float max = 1.0f,
               const char* tooltip = nullptr);

// Render vec4/color property with override visualization
bool RenderVec4(const char* label,
               glm::vec4& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec4)> onChange = nullptr,
               float min = 0.0f,
               float max = 1.0f,
               const char* tooltip = nullptr);

// Render color property with color picker
bool RenderColor3(const char* label,
                 glm::vec3& color,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(glm::vec3)> onChange = nullptr,
                 const char* tooltip = nullptr);

// Render color property with alpha
bool RenderColor4(const char* label,
                 glm::vec4& color,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(glm::vec4)> onChange = nullptr,
                 const char* tooltip = nullptr);

// Render angle property (converts to/from degrees)
bool RenderAngle(const char* label,
                float& radians,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(float)> onChange = nullptr,
                const char* tooltip = nullptr);

// Render percentage property (0-1 displayed as 0-100%)
bool RenderPercentage(const char* label,
                     float& value,
                     PropertyContainer* container,
                     PropertyLevel currentLevel,
                     std::function<void(float)> onChange = nullptr,
                     const char* tooltip = nullptr);

// Render string property
bool RenderString(const char* label,
                 std::string& value,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(const std::string&)> onChange = nullptr,
                 const char* tooltip = nullptr);

// Render combo box property
bool RenderCombo(const char* label,
                int& currentItem,
                const char* const* items,
                int itemCount,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(int)> onChange = nullptr,
                const char* tooltip = nullptr);

// Render texture slot
bool RenderTextureSlot(const char* label,
                      std::string& texturePath,
                      PropertyContainer* container,
                      PropertyLevel currentLevel,
                      std::function<void(const std::string&)> onChange = nullptr,
                      const char* tooltip = nullptr);

// Property group header
void BeginPropertyGroup(const char* name, bool defaultOpen = true);
void EndPropertyGroup();

// Property category section
void BeginCategory(const char* name);
void EndCategory();

// Override indicator widgets
void RenderOverrideIndicator(PropertyLevel currentLevel, PropertyLevel overrideLevel);
void RenderResetButton(const char* id, std::function<void()> onReset);

// Bulk edit helpers
struct BulkEditContext {
    bool enabled = false;
    int selectionCount = 0;
    std::vector<PropertyContainer*> containers;
};

void BeginBulkEdit(const BulkEditContext& context);
void EndBulkEdit();

// Filter for "show only overridden" mode
bool ShouldShowProperty(const PropertyMetadata& metadata, PropertyLevel currentLevel, bool showOnlyOverridden);

} // namespace PropertyOverrideUI
} // namespace Nova3D
