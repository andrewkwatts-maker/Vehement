#include "PropertyInspector.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Editor {

PropertyInspector::PropertyInspector() = default;
PropertyInspector::~PropertyInspector() = default;

void PropertyInspector::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void PropertyInspector::Shutdown() {
    ClearInspectedObject();
    m_propertyHistory.clear();
    m_initialized = false;
}

void PropertyInspector::SetInspectedObject(void* object, const Nova::Reflect::TypeInfo* typeInfo) {
    // Clear previous watches
    ClearInspectedObject();

    m_inspectedObject = object;
    m_typeInfo = typeInfo;

    // Initialize history for observable properties
    if (m_config.enableHistoryGraph && typeInfo) {
        for (const auto& prop : typeInfo->properties) {
            if (prop.IsObservable()) {
                m_propertyHistory[prop.name] = PropertyHistory();
            }
        }
    }
}

void PropertyInspector::ClearInspectedObject() {
    // Remove all watches
    for (const auto& [propPath, watchId] : m_watchIds) {
        m_watcher.Unwatch(watchId);
    }
    m_watchIds.clear();

    m_inspectedObject = nullptr;
    m_typeInfo = nullptr;
    m_propertyHistory.clear();
}

void PropertyInspector::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update property watcher
    m_watcher.Update(deltaTime);

    // Update history
    if (m_config.enableHistoryGraph && m_inspectedObject && m_typeInfo) {
        UpdatePropertyHistory(deltaTime);
    }

    m_totalTime += deltaTime;
}

void PropertyInspector::Render() {
    if (!m_initialized || !m_visible) return;

    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Property Inspector", &m_visible)) {
        RenderHeader();

        if (m_inspectedObject && m_typeInfo) {
            RenderPropertyList();
        } else {
            ImGui::TextDisabled("No object selected");
        }
    }
    ImGui::End();

    // Render history popup if active
    if (!m_historyPopupProperty.empty()) {
        RenderHistoryPopup(m_historyPopupProperty);
    }
}

void PropertyInspector::RenderHeader() {
    if (m_typeInfo) {
        ImGui::Text("Type: %s", m_typeInfo->name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu bytes)", m_typeInfo->size);
    }

    // Search filter
    char filterBuffer[256] = {0};
    strncpy(filterBuffer, m_searchFilter.c_str(), sizeof(filterBuffer) - 1);
    ImGui::SetNextItemWidth(-100);
    if (ImGui::InputTextWithHint("##filter", "Search properties...", filterBuffer, sizeof(filterBuffer))) {
        m_searchFilter = filterBuffer;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Observable", &m_showOnlyObservable);

    ImGui::Separator();
}

void PropertyInspector::RenderPropertyList() {
    ImGui::BeginChild("property_list");

    auto allProperties = m_typeInfo->GetAllProperties();

    // Group by category if enabled
    std::unordered_map<std::string, std::vector<const Nova::Reflect::PropertyInfo*>> categorized;
    std::vector<const Nova::Reflect::PropertyInfo*> uncategorized;

    for (const auto* prop : allProperties) {
        // Apply filters
        if (m_showOnlyObservable && !prop->IsObservable()) continue;
        if (!m_showReadOnly && prop->IsReadOnly()) continue;

        if (!m_searchFilter.empty()) {
            bool matches = prop->name.find(m_searchFilter) != std::string::npos ||
                          prop->displayName.find(m_searchFilter) != std::string::npos;
            if (!matches) continue;
        }

        if (m_config.groupByCategory && !prop->category.empty()) {
            categorized[prop->category].push_back(prop);
        } else {
            uncategorized.push_back(prop);
        }
    }

    // Render categorized properties
    for (const auto& [category, props] : categorized) {
        if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto* prop : props) {
                RenderProperty(*prop, m_inspectedObject);
            }
        }
    }

    // Render uncategorized
    if (!uncategorized.empty()) {
        if (categorized.empty() || ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto* prop : uncategorized) {
                RenderProperty(*prop, m_inspectedObject);
            }
        }
    }

    ImGui::EndChild();
}

void PropertyInspector::RenderProperty(const Nova::Reflect::PropertyInfo& prop, void* object) {
    ImGui::PushID(prop.name.c_str());

    // Property label with indicators
    std::string label = prop.displayName.empty() ? prop.name : prop.displayName;

    // Observable indicator
    if (m_config.showObservableIndicator && prop.IsObservable()) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "[O]");
        ImGui::SameLine();
    }

    // Read-only indicator
    if (prop.IsReadOnly()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[R]");
        ImGui::SameLine();
    }

    // Watch indicator
    if (IsPropertyWatched(prop.name)) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "[W]");
        ImGui::SameLine();
    }

    // Expand/collapse for history
    if (m_config.enableHistoryGraph && prop.IsObservable()) {
        bool isSelected = (m_selectedProperty == prop.name);
        if (ImGui::Selectable(("##sel_" + prop.name).c_str(), isSelected,
            ImGuiSelectableFlags_AllowDoubleClick, ImVec2(20, 0))) {
            m_selectedProperty = isSelected ? "" : prop.name;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            m_historyPopupProperty = prop.name;
        }
        ImGui::SameLine();
    }

    // Property value editor
    RenderPropertyValue(prop, object);

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (prop.IsObservable()) {
            bool watched = IsPropertyWatched(prop.name);
            if (ImGui::MenuItem(watched ? "Stop Watching" : "Watch Property")) {
                if (watched) {
                    UnwatchProperty(prop.name);
                } else {
                    WatchProperty(prop.name);
                }
            }
        }

        if (m_config.showBindingCreation) {
            if (ImGui::MenuItem("Create Binding...")) {
                if (OnCreateBindingRequested) {
                    OnCreateBindingRequested(prop.name);
                }
            }
        }

        if (m_config.enableHistoryGraph) {
            if (ImGui::MenuItem("Show History")) {
                m_historyPopupProperty = prop.name;
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Copy Value")) {
            // Copy to clipboard
        }

        ImGui::EndPopup();
    }

    // Tooltip with description
    if (ImGui::IsItemHovered() && !prop.description.empty()) {
        ImGui::SetTooltip("%s", prop.description.c_str());
    }

    // Show inline history graph if selected
    if (m_selectedProperty == prop.name && m_config.enableHistoryGraph) {
        RenderPropertyHistory(prop.name);
    }

    // Binding button
    if (m_config.showBindingCreation && prop.IsObservable()) {
        ImGui::SameLine();
        RenderBindingButton(prop.name);
    }

    ImGui::PopID();
}

void PropertyInspector::RenderPropertyValue(const Nova::Reflect::PropertyInfo& prop, void* object) {
    if (!prop.getterAny) {
        ImGui::TextDisabled("(no getter)");
        return;
    }

    std::any value = prop.getterAny(object);
    bool readOnly = prop.IsReadOnly() || !prop.setterAny;
    bool changed = false;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);

    if (readOnly) {
        ImGui::BeginDisabled();
    }

    std::string label = "##" + prop.name;

    // Type-specific editors
    if (value.type() == typeid(bool)) {
        bool b = std::any_cast<bool>(value);
        changed = RenderBoolEditor(label, b);
        if (changed && prop.setterAny) {
            prop.setterAny(object, b);
        }
    }
    else if (value.type() == typeid(int)) {
        int i = std::any_cast<int>(value);
        changed = RenderIntEditor(label, i, prop);
        if (changed && prop.setterAny) {
            prop.setterAny(object, i);
        }
    }
    else if (value.type() == typeid(float)) {
        float f = std::any_cast<float>(value);
        changed = RenderFloatEditor(label, f, prop);
        if (changed && prop.setterAny) {
            prop.setterAny(object, f);
        }
    }
    else if (value.type() == typeid(double)) {
        double d = std::any_cast<double>(value);
        float f = static_cast<float>(d);
        changed = RenderFloatEditor(label, f, prop);
        if (changed && prop.setterAny) {
            prop.setterAny(object, static_cast<double>(f));
        }
    }
    else if (value.type() == typeid(std::string)) {
        std::string s = std::any_cast<std::string>(value);
        changed = RenderStringEditor(label, s);
        if (changed && prop.setterAny) {
            prop.setterAny(object, s);
        }
    }
    else {
        ImGui::TextDisabled("(unsupported type: %s)", value.type().name());
    }

    if (readOnly) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    ImGui::Text("%s", (prop.displayName.empty() ? prop.name : prop.displayName).c_str());

    // Notify if changed
    if (changed && OnPropertyChanged) {
        OnPropertyChanged(prop.name, value);
    }
}

bool PropertyInspector::RenderBoolEditor(const std::string& label, bool& value) {
    return ImGui::Checkbox(label.c_str(), &value);
}

bool PropertyInspector::RenderIntEditor(const std::string& label, int& value, const Nova::Reflect::PropertyInfo& prop) {
    if (prop.hasRange) {
        return ImGui::SliderInt(label.c_str(), &value,
            static_cast<int>(prop.minValue), static_cast<int>(prop.maxValue));
    }
    return ImGui::InputInt(label.c_str(), &value);
}

bool PropertyInspector::RenderFloatEditor(const std::string& label, float& value, const Nova::Reflect::PropertyInfo& prop) {
    if (prop.hasRange) {
        return ImGui::SliderFloat(label.c_str(), &value, prop.minValue, prop.maxValue);
    }
    return ImGui::DragFloat(label.c_str(), &value, 0.1f);
}

bool PropertyInspector::RenderStringEditor(const std::string& label, std::string& value) {
    char buffer[256];
    strncpy(buffer, value.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
        value = buffer;
        return true;
    }
    return false;
}

bool PropertyInspector::RenderVec3Editor(const std::string& label, float* values) {
    return ImGui::DragFloat3(label.c_str(), values, 0.1f);
}

bool PropertyInspector::RenderColorEditor(const std::string& label, float* values) {
    return ImGui::ColorEdit4(label.c_str(), values);
}

void PropertyInspector::RenderPropertyHistory(const std::string& propertyPath) {
    auto it = m_propertyHistory.find(propertyPath);
    if (it == m_propertyHistory.end() || it->second.values.empty()) {
        ImGui::TextDisabled("No history data");
        return;
    }

    const auto& history = it->second;

    // Convert to array for plotting
    std::vector<float> plotValues;
    plotValues.reserve(history.values.size());
    for (const auto& [time, val] : history.values) {
        plotValues.push_back(val);
    }

    ImGui::PlotLines(
        ("##history_" + propertyPath).c_str(),
        plotValues.data(),
        static_cast<int>(plotValues.size()),
        0,
        nullptr,
        history.minValue,
        history.maxValue,
        ImVec2(0, 50)
    );

    ImGui::TextDisabled("Min: %.2f  Max: %.2f  Current: %.2f",
        history.minValue, history.maxValue,
        plotValues.empty() ? 0.0f : plotValues.back());
}

void PropertyInspector::RenderBindingButton(const std::string& propertyPath) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 0.5f));
    if (ImGui::SmallButton("+B")) {
        if (OnCreateBindingRequested) {
            OnCreateBindingRequested(propertyPath);
        }
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Create event binding for this property");
    }
}

void PropertyInspector::RenderHistoryPopup(const std::string& propertyPath) {
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_FirstUseEver);
    bool open = true;

    if (ImGui::Begin(("Property History: " + propertyPath).c_str(), &open)) {
        auto it = m_propertyHistory.find(propertyPath);
        if (it != m_propertyHistory.end() && !it->second.values.empty()) {
            const auto& history = it->second;

            std::vector<float> plotValues;
            plotValues.reserve(history.values.size());
            for (const auto& [time, val] : history.values) {
                plotValues.push_back(val);
            }

            ImGui::PlotLines(
                "##history_large",
                plotValues.data(),
                static_cast<int>(plotValues.size()),
                0,
                nullptr,
                history.minValue * 0.9f,
                history.maxValue * 1.1f,
                ImVec2(-1, 150)
            );

            ImGui::Separator();
            ImGui::Text("Statistics:");
            ImGui::BulletText("Min: %.4f", history.minValue);
            ImGui::BulletText("Max: %.4f", history.maxValue);
            ImGui::BulletText("Current: %.4f", plotValues.empty() ? 0.0f : plotValues.back());
            ImGui::BulletText("Samples: %zu", plotValues.size());
        } else {
            ImGui::TextDisabled("No history data available");
        }
    }
    ImGui::End();

    if (!open) {
        m_historyPopupProperty.clear();
    }
}

void PropertyInspector::WatchProperty(const std::string& propertyPath) {
    if (!m_inspectedObject || !m_typeInfo) return;
    if (m_watchIds.contains(propertyPath)) return;

    std::string watchId = m_watcher.Watch(
        m_inspectedObject, m_typeInfo, propertyPath,
        [this, propertyPath](const Nova::Events::PropertyChangeData& data) {
            RecordPropertyValue(propertyPath, data.newValue);
        }
    );

    m_watchIds[propertyPath] = watchId;
}

void PropertyInspector::UnwatchProperty(const std::string& propertyPath) {
    auto it = m_watchIds.find(propertyPath);
    if (it != m_watchIds.end()) {
        m_watcher.Unwatch(it->second);
        m_watchIds.erase(it);
    }
}

bool PropertyInspector::IsPropertyWatched(const std::string& propertyPath) const {
    return m_watchIds.contains(propertyPath);
}

void PropertyInspector::RecordPropertyValue(const std::string& propertyPath, const std::any& value) {
    if (!m_config.enableHistoryGraph) return;

    auto& history = m_propertyHistory[propertyPath];

    // Convert to float if possible
    float floatValue = 0.0f;
    if (value.type() == typeid(float)) {
        floatValue = std::any_cast<float>(value);
    } else if (value.type() == typeid(double)) {
        floatValue = static_cast<float>(std::any_cast<double>(value));
    } else if (value.type() == typeid(int)) {
        floatValue = static_cast<float>(std::any_cast<int>(value));
    } else if (value.type() == typeid(bool)) {
        floatValue = std::any_cast<bool>(value) ? 1.0f : 0.0f;
    } else {
        history.isNumeric = false;
        return;
    }

    history.values.push_back({m_totalTime, floatValue});

    // Update min/max
    if (history.values.size() == 1) {
        history.minValue = floatValue;
        history.maxValue = floatValue;
    } else {
        history.minValue = std::min(history.minValue, floatValue);
        history.maxValue = std::max(history.maxValue, floatValue);
    }

    // Trim old values
    while (history.values.size() > m_config.maxHistoryPoints) {
        history.values.pop_front();
    }
}

void PropertyInspector::UpdatePropertyHistory(float deltaTime) {
    m_historyTimer += deltaTime;

    if (m_historyTimer >= m_config.historyUpdateInterval) {
        m_historyTimer = 0.0f;

        // Record current values for all observable properties
        for (const auto& prop : m_typeInfo->properties) {
            if (prop.IsObservable() && prop.getterAny) {
                std::any value = prop.getterAny(m_inspectedObject);
                RecordPropertyValue(prop.name, value);
            }
        }
    }
}

} // namespace Editor
} // namespace Vehement
