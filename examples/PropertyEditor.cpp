#include "PropertyEditor.hpp"
#include <algorithm>
#include <cctype>

namespace PropertyEditor {

bool RenderPropertyTree(const char* label, nlohmann::json& json, bool readOnly,
                        const nlohmann::json* overrides, std::function<void(const std::string&)> onPropertyChanged) {
    bool modified = false;

    if (!json.is_object()) {
        return false;
    }

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto it = json.begin(); it != json.end(); ++it) {
            const std::string& key = it.key();
            nlohmann::json& value = it.value();

            bool isOverridden = overrides && overrides->contains(key);

            if (value.is_object()) {
                // Recursively render nested objects
                const nlohmann::json* childOverrides = isOverridden ? &(*overrides)[key] : nullptr;
                if (RenderPropertyTree(key.c_str(), value, readOnly, childOverrides, onPropertyChanged)) {
                    modified = true;
                    if (onPropertyChanged) {
                        onPropertyChanged(key);
                    }
                }
            } else {
                // Render single value
                if (RenderJsonValue(key.c_str(), value, readOnly, isOverridden)) {
                    modified = true;
                    if (onPropertyChanged) {
                        onPropertyChanged(key);
                    }
                }
            }
        }

        ImGui::TreePop();
    }

    return modified;
}

bool RenderJsonValue(const char* key, nlohmann::json& value, bool readOnly, bool isOverridden) {
    bool modified = false;

    // Apply styling based on override status
    ImVec4 color = GetPropertyColor(isOverridden, readOnly);
    if (isOverridden) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }

    ImGui::PushID(key);

    if (value.is_boolean()) {
        bool val = value.get<bool>();
        if (!readOnly && ImGui::Checkbox(key, &val)) {
            value = val;
            modified = true;
        } else if (readOnly) {
            ImGui::Text("%s: %s", key, val ? "true" : "false");
        }
    } else if (value.is_number_integer()) {
        int val = value.get<int>();
        if (!readOnly && ImGui::DragInt(key, &val, 1.0f)) {
            value = val;
            modified = true;
        } else if (readOnly) {
            ImGui::Text("%s: %d", key, val);
        }
    } else if (value.is_number_float()) {
        float val = value.get<float>();
        if (!readOnly && ImGui::DragFloat(key, &val, 0.1f)) {
            value = val;
            modified = true;
        } else if (readOnly) {
            ImGui::Text("%s: %.2f", key, val);
        }
    } else if (value.is_string()) {
        std::string val = value.get<std::string>();
        char buffer[256];
        strncpy_s(buffer, sizeof(buffer), val.c_str(), _TRUNCATE);

        if (!readOnly && ImGui::InputText(key, buffer, sizeof(buffer))) {
            value = std::string(buffer);
            modified = true;
        } else if (readOnly) {
            ImGui::Text("%s: %s", key, val.c_str());
        }
    } else if (value.is_array() && value.size() <= 4) {
        // Handle small arrays as vectors
        if (value.size() == 3 || value.size() == 4) {
            bool isFloatArray = true;
            for (size_t i = 0; i < value.size(); ++i) {
                if (!value[i].is_number()) {
                    isFloatArray = false;
                    break;
                }
            }

            if (isFloatArray) {
                if (value.size() == 3) {
                    float vec[3] = {
                        value[0].get<float>(),
                        value[1].get<float>(),
                        value[2].get<float>()
                    };

                    if (!readOnly && ImGui::DragFloat3(key, vec, 0.1f)) {
                        value = {vec[0], vec[1], vec[2]};
                        modified = true;
                    } else if (readOnly) {
                        ImGui::Text("%s: [%.2f, %.2f, %.2f]", key, vec[0], vec[1], vec[2]);
                    }
                } else if (value.size() == 4) {
                    float vec[4] = {
                        value[0].get<float>(),
                        value[1].get<float>(),
                        value[2].get<float>(),
                        value[3].get<float>()
                    };

                    if (!readOnly && ImGui::DragFloat4(key, vec, 0.1f)) {
                        value = {vec[0], vec[1], vec[2], vec[3]};
                        modified = true;
                    } else if (readOnly) {
                        ImGui::Text("%s: [%.2f, %.2f, %.2f, %.2f]", key, vec[0], vec[1], vec[2], vec[3]);
                    }
                }
            }
        } else {
            // Generic array display
            ImGui::Text("%s: [Array of %zu items]", key, value.size());
        }
    } else {
        // Unknown type
        ImGui::Text("%s: [Complex type]", key);
    }

    // Show override/reset buttons
    if (readOnly && !isOverridden) {
        ImGui::SameLine();
        if (RenderOverrideButton(key)) {
            // Signal to override this property
            modified = true;
        }
    } else if (isOverridden && !readOnly) {
        ImGui::SameLine();
        if (RenderResetButton(key)) {
            // Signal to reset this property
            modified = true;
        }
    }

    ImGui::PopID();

    if (isOverridden) {
        ImGui::PopStyleColor();
    }

    return modified;
}

bool RenderTransformProperties(glm::vec3& position, glm::vec3& rotation, glm::vec3& scale) {
    bool modified = false;

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
            modified = true;
        }

        if (ImGui::DragFloat3("Rotation", &rotation.x, 1.0f, -360.0f, 360.0f)) {
            modified = true;
        }

        if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 100.0f)) {
            modified = true;
        }
    }

    return modified;
}

void RenderFilterBar(std::string& filter, const char* placeholder) {
    char buffer[256];
    strncpy_s(buffer, sizeof(buffer), filter.c_str(), _TRUNCATE);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##filter", placeholder, buffer, sizeof(buffer))) {
        filter = buffer;
    }
}

bool MatchesFilter(const std::string& propertyPath, const std::string& filter) {
    if (filter.empty()) {
        return true;
    }

    // Case-insensitive search
    std::string lowerPath = propertyPath;
    std::string lowerFilter = filter;

    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

    return lowerPath.find(lowerFilter) != std::string::npos;
}

bool RenderOverrideButton(const std::string& propertyPath) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));

    bool clicked = ImGui::SmallButton("Override");

    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Override this property for this instance");
    }

    return clicked;
}

bool RenderResetButton(const std::string& propertyPath) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.2f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.5f, 0.3f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.6f, 0.4f, 1.0f));

    bool clicked = ImGui::SmallButton("Reset");

    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset to archetype default");
    }

    return clicked;
}

ImVec4 GetPropertyColor(bool isOverridden, bool isReadOnly) {
    if (isOverridden) {
        return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White for overridden
    } else if (isReadOnly) {
        return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);  // Gray for read-only
    } else {
        return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);  // Default
    }
}

std::string BuildPropertyPath(const std::string& parentPath, const std::string& key) {
    if (parentPath.empty()) {
        return key;
    }
    return parentPath + "." + key;
}

} // namespace PropertyEditor
