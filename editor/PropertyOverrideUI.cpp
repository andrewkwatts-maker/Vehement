#include "PropertyOverrideUI.hpp"
#include <cmath>

namespace Nova3D {
namespace PropertyOverrideUI {

OverrideColors g_overrideColors;

ImVec4 GetPropertyColor(PropertyLevel currentLevel, PropertyLevel overrideLevel) {
    if (overrideLevel == currentLevel) {
        return g_overrideColors.overridden;  // Overridden at this level
    } else if (static_cast<int>(overrideLevel) < static_cast<int>(currentLevel)) {
        return g_overrideColors.inherited;   // Inherited from parent
    } else {
        return g_overrideColors.defaultValue; // Default value
    }
}

void ShowOverrideTooltip(const PropertyMetadata& metadata, PropertyLevel currentLevel) {
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();

        // Show override level
        ImGui::Text("Override Level: %s", PropertyLevelToString(metadata.overrideLevel));

        // Show current level
        ImGui::Text("Current Level: %s", PropertyLevelToString(currentLevel));

        // Show if overridden
        if (metadata.overrideLevel == currentLevel) {
            ImGui::TextColored(g_overrideColors.overridden, "Overridden at this level");
        } else if (static_cast<int>(metadata.overrideLevel) < static_cast<int>(currentLevel)) {
            ImGui::TextColored(g_overrideColors.inherited, "Inherited from %s",
                             PropertyLevelToString(metadata.overrideLevel));
        }

        // Show if can override
        if (!metadata.allowOverride) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Cannot be overridden");
        }

        // Show tooltip if available
        if (!metadata.tooltip.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", metadata.tooltip.c_str());
        }

        ImGui::EndTooltip();
    }
}

void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void RenderOverrideIndicator(PropertyLevel currentLevel, PropertyLevel overrideLevel) {
    ImVec4 color = GetPropertyColor(currentLevel, overrideLevel);
    ImGui::TextColored(color, "[%s]", PropertyLevelToString(overrideLevel));
}

void RenderResetButton(const char* id, std::function<void()> onReset) {
    ImGui::PushStyleColor(ImGuiCol_Button, g_overrideColors.resetButton);
    if (ImGui::SmallButton(id)) {
        if (onReset) {
            onReset();
        }
    }
    ImGui::PopStyleColor();
}

bool RenderFloat(const char* label,
                float& value,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(float)> onChange,
                float min,
                float max,
                const char* tooltip,
                const char* format) {
    bool changed = false;

    // Get metadata if available
    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    // Color-code label based on override status
    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && tooltip) {
            ImGui::SetTooltip("%s", tooltip);
        }
    } else {
        ImGui::Text("%s", label);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
    }

    ImGui::SameLine();

    // Slider
    ImGui::PushItemWidth(-100);
    if (ImGui::SliderFloat(("##" + std::string(label)).c_str(), &value, min, max, format)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    // Show tooltip on hover
    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    // Reset button
    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<float>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderInt(const char* label,
              int& value,
              PropertyContainer* container,
              PropertyLevel currentLevel,
              std::function<void(int)> onChange,
              int min,
              int max,
              const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::SliderInt(("##" + std::string(label)).c_str(), &value, min, max)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<int>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderBool(const char* label,
               bool& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(bool)> onChange,
               const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }

    if (ImGui::Checkbox(label, &value)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }

    if (metadata) {
        ImGui::PopStyleColor();
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<bool>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderVec2(const char* label,
               glm::vec2& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec2)> onChange,
               float min,
               float max,
               const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::SliderFloat2(("##" + std::string(label)).c_str(), &value.x, min, max)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<glm::vec2>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderVec3(const char* label,
               glm::vec3& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec3)> onChange,
               float min,
               float max,
               const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::SliderFloat3(("##" + std::string(label)).c_str(), &value.x, min, max)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<glm::vec3>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderVec4(const char* label,
               glm::vec4& value,
               PropertyContainer* container,
               PropertyLevel currentLevel,
               std::function<void(glm::vec4)> onChange,
               float min,
               float max,
               const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::SliderFloat4(("##" + std::string(label)).c_str(), &value.x, min, max)) {
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<glm::vec4>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderColor3(const char* label,
                 glm::vec3& color,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(glm::vec3)> onChange,
                 const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 col = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::ColorEdit3(("##" + std::string(label)).c_str(), &color.x)) {
        if (onChange) {
            onChange(color);
        }
        if (container) {
            container->SetProperty(label, color, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                color = container->ResetToParent<glm::vec3>(label, currentLevel);
                if (onChange) {
                    onChange(color);
                }
            }
        });
    }

    return changed;
}

bool RenderColor4(const char* label,
                 glm::vec4& color,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(glm::vec4)> onChange,
                 const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 col = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::ColorEdit4(("##" + std::string(label)).c_str(), &color.x)) {
        if (onChange) {
            onChange(color);
        }
        if (container) {
            container->SetProperty(label, color, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                color = container->ResetToParent<glm::vec4>(label, currentLevel);
                if (onChange) {
                    onChange(color);
                }
            }
        });
    }

    return changed;
}

bool RenderAngle(const char* label,
                float& radians,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(float)> onChange,
                const char* tooltip) {
    float degrees = glm::degrees(radians);
    bool changed = RenderFloat(label, degrees, container, currentLevel,
                              [&](float deg) {
                                  radians = glm::radians(deg);
                                  if (onChange) onChange(radians);
                              },
                              0.0f, 360.0f, tooltip, "%.1f deg");
    if (changed) {
        radians = glm::radians(degrees);
    }
    return changed;
}

bool RenderPercentage(const char* label,
                     float& value,
                     PropertyContainer* container,
                     PropertyLevel currentLevel,
                     std::function<void(float)> onChange,
                     const char* tooltip) {
    float percentage = value * 100.0f;
    bool changed = RenderFloat(label, percentage, container, currentLevel,
                              [&](float pct) {
                                  value = pct / 100.0f;
                                  if (onChange) onChange(value);
                              },
                              0.0f, 100.0f, tooltip, "%.1f%%");
    if (changed) {
        value = percentage / 100.0f;
    }
    return changed;
}

bool RenderString(const char* label,
                 std::string& value,
                 PropertyContainer* container,
                 PropertyLevel currentLevel,
                 std::function<void(const std::string&)> onChange,
                 const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    char buffer[256];
    strncpy_s(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    ImGui::PushItemWidth(-100);
    if (ImGui::InputText(("##" + std::string(label)).c_str(), buffer, sizeof(buffer))) {
        value = buffer;
        if (onChange) {
            onChange(value);
        }
        if (container) {
            container->SetProperty(label, value, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                value = container->ResetToParent<std::string>(label, currentLevel);
                if (onChange) {
                    onChange(value);
                }
            }
        });
    }

    return changed;
}

bool RenderCombo(const char* label,
                int& currentItem,
                const char* const* items,
                int itemCount,
                PropertyContainer* container,
                PropertyLevel currentLevel,
                std::function<void(int)> onChange,
                const char* tooltip) {
    bool changed = false;

    const PropertyMetadata* metadata = container ? container->GetMetadata(label) : nullptr;

    if (metadata) {
        ImVec4 color = GetPropertyColor(currentLevel, metadata->overrideLevel);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();
    } else {
        ImGui::Text("%s", label);
    }

    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }

    ImGui::SameLine();

    ImGui::PushItemWidth(-100);
    if (ImGui::Combo(("##" + std::string(label)).c_str(), &currentItem, items, itemCount)) {
        if (onChange) {
            onChange(currentItem);
        }
        if (container) {
            container->SetProperty(label, currentItem, currentLevel);
        }
        changed = true;
    }
    ImGui::PopItemWidth();

    if (metadata) {
        ShowOverrideTooltip(*metadata, currentLevel);
    }

    ImGui::SameLine();

    if (currentLevel != PropertyLevel::Global) {
        std::string resetId = "Reset##" + std::string(label);
        RenderResetButton(resetId.c_str(), [&]() {
            if (container) {
                currentItem = container->ResetToParent<int>(label, currentLevel);
                if (onChange) {
                    onChange(currentItem);
                }
            }
        });
    }

    return changed;
}

bool RenderTextureSlot(const char* label,
                      std::string& texturePath,
                      PropertyContainer* container,
                      PropertyLevel currentLevel,
                      std::function<void(const std::string&)> onChange,
                      const char* tooltip) {
    bool changed = RenderString(label, texturePath, container, currentLevel, onChange, tooltip);

    // Add browse button
    ImGui::SameLine();
    if (ImGui::SmallButton(("Browse##" + std::string(label)).c_str())) {
        // TODO: Open file dialog
        // For now, just clear the path
        texturePath = "";
        if (onChange) {
            onChange(texturePath);
        }
        changed = true;
    }

    return changed;
}

void BeginPropertyGroup(const char* name, bool defaultOpen) {
    ImGui::SetNextItemOpen(defaultOpen, ImGuiCond_Once);
    if (ImGui::CollapsingHeader(name)) {
        ImGui::Indent();
    }
}

void EndPropertyGroup() {
    ImGui::Unindent();
}

void BeginCategory(const char* name) {
    ImGui::Separator();
    ImGui::Text("%s", name);
    ImGui::Separator();
}

void EndCategory() {
    ImGui::Spacing();
}

void BeginBulkEdit(const BulkEditContext& context) {
    if (context.enabled && context.selectionCount > 1) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        ImGui::Text("Bulk editing %d items", context.selectionCount);
        ImGui::PopStyleColor();
        ImGui::Separator();
    }
}

void EndBulkEdit() {
    // Nothing to do for now
}

bool ShouldShowProperty(const PropertyMetadata& metadata, PropertyLevel currentLevel, bool showOnlyOverridden) {
    if (!showOnlyOverridden) {
        return true;
    }
    return metadata.overrideLevel == currentLevel;
}

} // namespace PropertyOverrideUI
} // namespace Nova3D
