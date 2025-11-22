#include "ObjectEditor.hpp"
#include "InGameEditor.hpp"

#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Vehement {

const char* GetCategoryName(ObjectCategory category) {
    switch (category) {
        case ObjectCategory::Unit: return "Units";
        case ObjectCategory::Building: return "Buildings";
        case ObjectCategory::Ability: return "Abilities";
        case ObjectCategory::Upgrade: return "Upgrades";
        case ObjectCategory::Item: return "Items";
        case ObjectCategory::Doodad: return "Doodads";
        default: return "Unknown";
    }
}

const char* GetStatOperationName(StatOperation op) {
    switch (op) {
        case StatOperation::Set: return "Set";
        case StatOperation::Add: return "Add";
        case StatOperation::Multiply: return "Multiply";
        case StatOperation::Percent: return "Percent";
        default: return "Unknown";
    }
}

ObjectEditor::ObjectEditor() = default;
ObjectEditor::~ObjectEditor() = default;

bool ObjectEditor::Initialize(InGameEditor& parent) {
    if (m_initialized) {
        return true;
    }

    m_parent = &parent;

    // Load templates from game data
    LoadTemplates();

    m_initialized = true;
    return true;
}

void ObjectEditor::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
    m_templates.clear();
    m_customObjects.clear();
    ClearHistory();
}

void ObjectEditor::LoadTemplates() {
    // Load unit templates
    {
        std::vector<ObjectTemplate> units;

        ObjectTemplate worker;
        worker.id = "unit_worker";
        worker.category = ObjectCategory::Unit;
        worker.name = "Worker";
        worker.description = "Basic worker unit";
        worker.stats["health"] = 100.0f;
        worker.stats["damage"] = 5.0f;
        worker.stats["armor"] = 0.0f;
        worker.stats["moveSpeed"] = 3.0f;
        worker.stats["attackSpeed"] = 1.0f;
        worker.stats["attackRange"] = 1.0f;
        units.push_back(worker);

        ObjectTemplate soldier;
        soldier.id = "unit_soldier";
        soldier.category = ObjectCategory::Unit;
        soldier.name = "Soldier";
        soldier.description = "Infantry combat unit";
        soldier.stats["health"] = 200.0f;
        soldier.stats["damage"] = 20.0f;
        soldier.stats["armor"] = 2.0f;
        soldier.stats["moveSpeed"] = 2.5f;
        soldier.stats["attackSpeed"] = 1.0f;
        soldier.stats["attackRange"] = 1.0f;
        units.push_back(soldier);

        ObjectTemplate archer;
        archer.id = "unit_archer";
        archer.category = ObjectCategory::Unit;
        archer.name = "Archer";
        archer.description = "Ranged combat unit";
        archer.stats["health"] = 100.0f;
        archer.stats["damage"] = 15.0f;
        archer.stats["armor"] = 0.0f;
        archer.stats["moveSpeed"] = 2.8f;
        archer.stats["attackSpeed"] = 1.2f;
        archer.stats["attackRange"] = 6.0f;
        units.push_back(archer);

        m_templates[ObjectCategory::Unit] = units;
    }

    // Load building templates
    {
        std::vector<ObjectTemplate> buildings;

        ObjectTemplate barracks;
        barracks.id = "building_barracks";
        barracks.category = ObjectCategory::Building;
        barracks.name = "Barracks";
        barracks.description = "Trains military units";
        barracks.stats["health"] = 1500.0f;
        barracks.stats["armor"] = 5.0f;
        barracks.stats["buildTime"] = 60.0f;
        barracks.properties["goldCost"] = "150";
        barracks.properties["woodCost"] = "50";
        buildings.push_back(barracks);

        ObjectTemplate tower;
        tower.id = "building_tower";
        tower.category = ObjectCategory::Building;
        tower.name = "Watch Tower";
        tower.description = "Defensive structure";
        tower.stats["health"] = 800.0f;
        tower.stats["armor"] = 3.0f;
        tower.stats["damage"] = 25.0f;
        tower.stats["attackRange"] = 8.0f;
        tower.stats["buildTime"] = 45.0f;
        buildings.push_back(tower);

        m_templates[ObjectCategory::Building] = buildings;
    }

    // Load ability templates
    {
        std::vector<ObjectTemplate> abilities;

        ObjectTemplate fireball;
        fireball.id = "ability_fireball";
        fireball.category = ObjectCategory::Ability;
        fireball.name = "Fireball";
        fireball.description = "Launches a fiery projectile";
        fireball.stats["damage"] = 50.0f;
        fireball.stats["cooldown"] = 10.0f;
        fireball.stats["manaCost"] = 25.0f;
        fireball.stats["range"] = 8.0f;
        fireball.stats["aoeRadius"] = 2.0f;
        abilities.push_back(fireball);

        ObjectTemplate heal;
        heal.id = "ability_heal";
        heal.category = ObjectCategory::Ability;
        heal.name = "Heal";
        heal.description = "Restores health";
        heal.stats["healing"] = 100.0f;
        heal.stats["cooldown"] = 15.0f;
        heal.stats["manaCost"] = 30.0f;
        heal.stats["range"] = 5.0f;
        abilities.push_back(heal);

        m_templates[ObjectCategory::Ability] = abilities;
    }

    // Load upgrade templates
    {
        std::vector<ObjectTemplate> upgrades;

        ObjectTemplate improvedArmor;
        improvedArmor.id = "upgrade_improved_armor";
        improvedArmor.category = ObjectCategory::Upgrade;
        improvedArmor.name = "Improved Armor";
        improvedArmor.description = "+2 armor to all units";
        improvedArmor.stats["armorBonus"] = 2.0f;
        improvedArmor.stats["researchTime"] = 45.0f;
        improvedArmor.properties["goldCost"] = "100";
        upgrades.push_back(improvedArmor);

        m_templates[ObjectCategory::Upgrade] = upgrades;
    }

    // Initialize empty templates for other categories
    m_templates[ObjectCategory::Item] = {};
    m_templates[ObjectCategory::Doodad] = {};
}

const std::vector<ObjectTemplate>& ObjectEditor::GetTemplates(ObjectCategory category) const {
    static const std::vector<ObjectTemplate> empty;
    auto it = m_templates.find(category);
    return it != m_templates.end() ? it->second : empty;
}

const ObjectTemplate* ObjectEditor::GetTemplate(const std::string& id) const {
    for (const auto& [category, templates] : m_templates) {
        for (const auto& tmpl : templates) {
            if (tmpl.id == id) {
                return &tmpl;
            }
        }
    }
    return nullptr;
}

std::string ObjectEditor::CreateCustomObject(const std::string& baseId) {
    const ObjectTemplate* tmpl = GetTemplate(baseId);
    if (!tmpl) {
        return "";
    }

    CustomObject obj;
    obj.id = GenerateCustomId(baseId);
    obj.baseId = baseId;
    obj.category = tmpl->category;
    obj.name = tmpl->name + " (Custom)";
    obj.description = tmpl->description;
    obj.iconPath = tmpl->iconPath;

    m_customObjects.push_back(obj);

    if (OnObjectCreated) {
        OnObjectCreated(obj.id);
    }

    return obj.id;
}

void ObjectEditor::DeleteCustomObject(const std::string& id) {
    auto it = std::find_if(m_customObjects.begin(), m_customObjects.end(),
                          [&id](const CustomObject& o) { return o.id == id; });
    if (it != m_customObjects.end()) {
        m_customObjects.erase(it);

        if (m_selectedObjectId == id) {
            m_selectedObjectId.clear();
        }

        if (OnObjectDeleted) {
            OnObjectDeleted(id);
        }
    }
}

void ObjectEditor::UpdateCustomObject(const std::string& id, const CustomObject& obj) {
    auto it = std::find_if(m_customObjects.begin(), m_customObjects.end(),
                          [&id](const CustomObject& o) { return o.id == id; });
    if (it != m_customObjects.end()) {
        std::string oldId = it->id;
        *it = obj;
        it->id = oldId;

        if (OnObjectModified) {
            OnObjectModified(id);
        }
    }
}

CustomObject* ObjectEditor::GetCustomObject(const std::string& id) {
    auto it = std::find_if(m_customObjects.begin(), m_customObjects.end(),
                          [&id](const CustomObject& o) { return o.id == id; });
    return it != m_customObjects.end() ? &(*it) : nullptr;
}

void ObjectEditor::SelectObject(const std::string& id) {
    m_selectedObjectId = id;

    if (OnObjectSelected) {
        OnObjectSelected(id);
    }
}

void ObjectEditor::AddStatMod(const std::string& objectId, const StatModification& mod) {
    CustomObject* obj = GetCustomObject(objectId);
    if (!obj) return;

    // Remove existing mod for same stat
    obj->statMods.erase(
        std::remove_if(obj->statMods.begin(), obj->statMods.end(),
                      [&mod](const StatModification& m) { return m.statName == mod.statName; }),
        obj->statMods.end());

    obj->statMods.push_back(mod);

    if (OnObjectModified) {
        OnObjectModified(objectId);
    }
}

void ObjectEditor::RemoveStatMod(const std::string& objectId, const std::string& statName) {
    CustomObject* obj = GetCustomObject(objectId);
    if (!obj) return;

    obj->statMods.erase(
        std::remove_if(obj->statMods.begin(), obj->statMods.end(),
                      [&statName](const StatModification& m) { return m.statName == statName; }),
        obj->statMods.end());

    if (OnObjectModified) {
        OnObjectModified(objectId);
    }
}

void ObjectEditor::ClearStatMods(const std::string& objectId) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->statMods.clear();

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

float ObjectEditor::GetEffectiveStat(const std::string& objectId, const std::string& statName) const {
    // Find custom object
    auto customIt = std::find_if(m_customObjects.begin(), m_customObjects.end(),
                                 [&objectId](const CustomObject& o) { return o.id == objectId; });

    // Get base template
    const ObjectTemplate* tmpl = nullptr;
    if (customIt != m_customObjects.end()) {
        tmpl = GetTemplate(customIt->baseId);
    } else {
        tmpl = GetTemplate(objectId);
    }

    if (!tmpl) {
        return 0.0f;
    }

    // Get base stat value
    auto statIt = tmpl->stats.find(statName);
    float baseValue = (statIt != tmpl->stats.end()) ? statIt->second : 0.0f;

    // If no custom object, return base value
    if (customIt == m_customObjects.end()) {
        return baseValue;
    }

    // Apply modifications
    float finalValue = baseValue;
    for (const auto& mod : customIt->statMods) {
        if (mod.statName == statName) {
            switch (mod.operation) {
                case StatOperation::Set:
                    finalValue = mod.value;
                    break;
                case StatOperation::Add:
                    finalValue += mod.value;
                    break;
                case StatOperation::Multiply:
                    finalValue *= mod.value;
                    break;
                case StatOperation::Percent:
                    finalValue *= (1.0f + mod.value / 100.0f);
                    break;
            }
        }
    }

    return finalValue;
}

void ObjectEditor::SetProperty(const std::string& objectId, const std::string& key, const std::string& value) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->properties[key] = value;

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

void ObjectEditor::RemoveProperty(const std::string& objectId, const std::string& key) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->properties.erase(key);

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

void ObjectEditor::SetCustomModel(const std::string& objectId, const std::string& modelPath) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->customModel = modelPath;

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

void ObjectEditor::SetModelScale(const std::string& objectId, float scale) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->modelScale = scale;

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

void ObjectEditor::SetTint(const std::string& objectId, float r, float g, float b, float a) {
    CustomObject* obj = GetCustomObject(objectId);
    if (obj) {
        obj->tint = {r, g, b, a};

        if (OnObjectModified) {
            OnObjectModified(objectId);
        }
    }
}

bool ObjectEditor::ExportCustomObjects(const std::string& path) const {
    // Export to JSON format
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << "{\n  \"customObjects\": [\n";

    for (size_t i = 0; i < m_customObjects.size(); ++i) {
        const auto& obj = m_customObjects[i];
        file << "    {\n";
        file << "      \"id\": \"" << obj.id << "\",\n";
        file << "      \"baseId\": \"" << obj.baseId << "\",\n";
        file << "      \"name\": \"" << obj.name << "\",\n";
        file << "      \"description\": \"" << obj.description << "\"\n";
        file << "    }";
        if (i < m_customObjects.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n}\n";
    return true;
}

bool ObjectEditor::ImportCustomObjects(const std::string& path) {
    // Simplified import - real implementation would parse JSON
    return true;
}

bool ObjectEditor::ValidateCustomObject(const std::string& id, std::string& error) const {
    auto it = std::find_if(m_customObjects.begin(), m_customObjects.end(),
                          [&id](const CustomObject& o) { return o.id == id; });
    if (it == m_customObjects.end()) {
        error = "Object not found";
        return false;
    }

    // Check base template exists
    if (!GetTemplate(it->baseId)) {
        error = "Base template not found";
        return false;
    }

    return true;
}

bool ObjectEditor::ValidateAll(std::vector<std::string>& errors) const {
    errors.clear();

    for (const auto& obj : m_customObjects) {
        std::string error;
        if (!ValidateCustomObject(obj.id, error)) {
            errors.push_back(obj.name + ": " + error);
        }
    }

    return errors.empty();
}

void ObjectEditor::ExecuteCommand(std::unique_ptr<ObjectEditorCommand> command) {
    command->Execute();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();

    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        m_undoStack.pop_front();
    }
}

void ObjectEditor::Undo() {
    if (m_undoStack.empty()) return;

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(std::move(command));
}

void ObjectEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(std::move(command));
}

void ObjectEditor::ClearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void ObjectEditor::Update(float deltaTime) {
    if (!m_initialized) return;
}

void ObjectEditor::Render() {
    if (!m_initialized) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Object Editor")) {
        // Three-column layout
        float columnWidth = 200.0f;

        // Left column - Categories
        ImGui::BeginChild("Categories", ImVec2(columnWidth, 0), true);
        RenderCategoryList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Middle column - Object list
        ImGui::BeginChild("ObjectList", ImVec2(columnWidth, 0), true);
        RenderObjectList();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right column - Details
        ImGui::BeginChild("Details", ImVec2(0, 0), true);
        RenderObjectDetails();
        ImGui::EndChild();
    }
    ImGui::End();
}

void ObjectEditor::ProcessInput() {
    if (!m_initialized) return;

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_selectedObjectId.empty()) {
            // Only delete custom objects
            if (GetCustomObject(m_selectedObjectId)) {
                DeleteCustomObject(m_selectedObjectId);
            }
        }
    }
}

void ObjectEditor::RenderCategoryList() {
    ImGui::Text("Categories");
    ImGui::Separator();

    const ObjectCategory categories[] = {
        ObjectCategory::Unit,
        ObjectCategory::Building,
        ObjectCategory::Ability,
        ObjectCategory::Upgrade,
        ObjectCategory::Item,
        ObjectCategory::Doodad
    };

    for (ObjectCategory cat : categories) {
        bool isSelected = (m_selectedCategory == cat);
        if (ImGui::Selectable(GetCategoryName(cat), isSelected)) {
            m_selectedCategory = cat;
            m_selectedObjectId.clear();
        }
    }

    ImGui::Separator();
    ImGui::Checkbox("Custom Only", &m_showingCustomOnly);
}

void ObjectEditor::RenderObjectList() {
    ImGui::Text("Objects");
    ImGui::Separator();

    // Search filter
    char searchBuffer[128] = "";
    strncpy(searchBuffer, m_searchFilter.c_str(), sizeof(searchBuffer) - 1);
    if (ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer))) {
        m_searchFilter = searchBuffer;
    }

    ImGui::Separator();

    // Create new button
    if (ImGui::Button("+ New Custom")) {
        ImGui::OpenPopup("NewCustomPopup");
    }

    if (ImGui::BeginPopup("NewCustomPopup")) {
        const auto& templates = GetTemplates(m_selectedCategory);
        for (const auto& tmpl : templates) {
            if (ImGui::MenuItem(tmpl.name.c_str())) {
                std::string newId = CreateCustomObject(tmpl.id);
                SelectObject(newId);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // List templates
    if (!m_showingCustomOnly) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Templates:");
        const auto& templates = GetTemplates(m_selectedCategory);
        for (const auto& tmpl : templates) {
            if (!m_searchFilter.empty() &&
                tmpl.name.find(m_searchFilter) == std::string::npos) {
                continue;
            }

            bool isSelected = (m_selectedObjectId == tmpl.id);
            if (ImGui::Selectable(tmpl.name.c_str(), isSelected)) {
                SelectObject(tmpl.id);
            }
        }
    }

    // List custom objects
    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Custom:");
    for (const auto& obj : m_customObjects) {
        if (obj.category != m_selectedCategory) continue;

        if (!m_searchFilter.empty() &&
            obj.name.find(m_searchFilter) == std::string::npos) {
            continue;
        }

        bool isSelected = (m_selectedObjectId == obj.id);
        if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
            SelectObject(obj.id);
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                DeleteCustomObject(obj.id);
            }
            ImGui::EndPopup();
        }
    }
}

void ObjectEditor::RenderObjectDetails() {
    if (m_selectedObjectId.empty()) {
        ImGui::Text("Select an object to edit");
        return;
    }

    // Check if custom or template
    CustomObject* customObj = GetCustomObject(m_selectedObjectId);
    const ObjectTemplate* tmpl = customObj ? GetTemplate(customObj->baseId) : GetTemplate(m_selectedObjectId);

    if (!tmpl) {
        ImGui::Text("Object not found");
        return;
    }

    // Header
    if (customObj) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Custom Object");

        char nameBuffer[256];
        strncpy(nameBuffer, customObj->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            customObj->name = nameBuffer;
        }

        char descBuffer[512];
        strncpy(descBuffer, customObj->description.c_str(), sizeof(descBuffer) - 1);
        if (ImGui::InputTextMultiline("Description", descBuffer, sizeof(descBuffer), ImVec2(0, 60))) {
            customObj->description = descBuffer;
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Template (Read-only)");
        ImGui::Text("Name: %s", tmpl->name.c_str());
        ImGui::TextWrapped("Description: %s", tmpl->description.c_str());
    }

    ImGui::Separator();

    // Tabs for different sections
    if (ImGui::BeginTabBar("ObjectTabs")) {
        if (ImGui::BeginTabItem("Stats")) {
            RenderStatEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Properties")) {
            RenderPropertyEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Visuals")) {
            RenderVisualEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview")) {
            RenderPreview();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ObjectEditor::RenderStatEditor() {
    CustomObject* customObj = GetCustomObject(m_selectedObjectId);
    const ObjectTemplate* tmpl = customObj ? GetTemplate(customObj->baseId) : GetTemplate(m_selectedObjectId);

    if (!tmpl) return;

    ImGui::Text("Stats");
    ImGui::Separator();

    // Show all stats from template
    for (const auto& [statName, baseValue] : tmpl->stats) {
        ImGui::PushID(statName.c_str());

        float effectiveValue = GetEffectiveStat(customObj ? customObj->id : tmpl->id, statName);

        if (customObj) {
            // Find existing mod
            auto modIt = std::find_if(customObj->statMods.begin(), customObj->statMods.end(),
                                      [&statName](const StatModification& m) { return m.statName == statName; });

            // Show base and effective
            ImGui::Text("%s:", statName.c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Base: %.1f)", baseValue);
            ImGui::SameLine();

            float modValue = (modIt != customObj->statMods.end()) ? modIt->value : baseValue;
            if (ImGui::InputFloat("##value", &modValue, 0.1f, 1.0f, "%.2f")) {
                StatModification mod;
                mod.statName = statName;
                mod.operation = StatOperation::Set;
                mod.value = modValue;
                AddStatMod(customObj->id, mod);
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset")) {
                RemoveStatMod(customObj->id, statName);
            }
        } else {
            ImGui::Text("%s: %.1f", statName.c_str(), baseValue);
        }

        ImGui::PopID();
    }
}

void ObjectEditor::RenderPropertyEditor() {
    CustomObject* customObj = GetCustomObject(m_selectedObjectId);
    const ObjectTemplate* tmpl = customObj ? GetTemplate(customObj->baseId) : GetTemplate(m_selectedObjectId);

    if (!tmpl) return;

    ImGui::Text("Properties");
    ImGui::Separator();

    // Show template properties
    for (const auto& [key, value] : tmpl->properties) {
        if (customObj) {
            auto it = customObj->properties.find(key);
            std::string currentValue = (it != customObj->properties.end()) ? it->second : value;

            char buffer[256];
            strncpy(buffer, currentValue.c_str(), sizeof(buffer) - 1);

            ImGui::PushID(key.c_str());
            if (ImGui::InputText(key.c_str(), buffer, sizeof(buffer))) {
                SetProperty(customObj->id, key, buffer);
            }
            ImGui::PopID();
        } else {
            ImGui::Text("%s: %s", key.c_str(), value.c_str());
        }
    }

    // Custom properties
    if (customObj) {
        ImGui::Separator();
        ImGui::Text("Custom Properties");

        if (ImGui::Button("Add Property")) {
            SetProperty(customObj->id, "NewProperty", "");
        }
    }
}

void ObjectEditor::RenderVisualEditor() {
    CustomObject* customObj = GetCustomObject(m_selectedObjectId);

    if (!customObj) {
        ImGui::Text("Visual editing only available for custom objects");
        return;
    }

    ImGui::Text("Visual Settings");
    ImGui::Separator();

    // Model
    char modelBuffer[256];
    strncpy(modelBuffer, customObj->customModel.c_str(), sizeof(modelBuffer) - 1);
    if (ImGui::InputText("Custom Model", modelBuffer, sizeof(modelBuffer))) {
        customObj->customModel = modelBuffer;
    }

    // Scale
    if (ImGui::SliderFloat("Model Scale", &customObj->modelScale, 0.1f, 3.0f)) {
        // Value updated directly
    }

    // Tint
    if (ImGui::ColorEdit4("Tint", customObj->tint.data())) {
        // Value updated directly
    }
}

void ObjectEditor::RenderPreview() {
    ImGui::Text("Preview");
    ImGui::Separator();

    // Show a preview of the object with current settings
    ImGui::Text("3D Preview would be rendered here");

    // Effective stats summary
    ImGui::Separator();
    ImGui::Text("Effective Stats:");

    CustomObject* customObj = GetCustomObject(m_selectedObjectId);
    const ObjectTemplate* tmpl = customObj ? GetTemplate(customObj->baseId) : GetTemplate(m_selectedObjectId);

    if (tmpl) {
        for (const auto& [statName, baseValue] : tmpl->stats) {
            float effective = GetEffectiveStat(m_selectedObjectId, statName);
            ImGui::BulletText("%s: %.1f", statName.c_str(), effective);
        }
    }
}

std::string ObjectEditor::GenerateCustomId(const std::string& baseId) {
    return "custom_" + baseId + "_" + std::to_string(m_nextCustomId++);
}

} // namespace Vehement
