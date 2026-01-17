/**
 * @file GenericAssetEditor.cpp
 * @brief Implementation of generic asset editor system for JSON-based game assets
 */

#include "GenericAssetEditor.hpp"
#include <spdlog/spdlog.h>
#include "../core/Logger.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Asset Type Helpers
// ============================================================================

std::string GameAssetTypeToString(GameAssetType type) {
    switch (type) {
        case GameAssetType::SDFModel:      return "SDFModel";
        case GameAssetType::Skeleton:      return "Skeleton";
        case GameAssetType::Animation:     return "Animation";
        case GameAssetType::AnimationSet:  return "AnimationSet";
        case GameAssetType::Entity:        return "Entity";
        case GameAssetType::Hero:          return "Hero";
        case GameAssetType::ResourceNode:  return "ResourceNode";
        case GameAssetType::Projectile:    return "Projectile";
        case GameAssetType::Behavior:      return "Behavior";
        case GameAssetType::TechTree:      return "TechTree";
        case GameAssetType::Upgrade:       return "Upgrade";
        case GameAssetType::Campaign:      return "Campaign";
        case GameAssetType::Mission:       return "Mission";
        default:                           return "Unknown";
    }
}

GameAssetType StringToGameAssetType(const std::string& str) {
    static const std::unordered_map<std::string, GameAssetType> typeMap = {
        {"SDFModel", GameAssetType::SDFModel},
        {"sdfmodel", GameAssetType::SDFModel},
        {"Skeleton", GameAssetType::Skeleton},
        {"skeleton", GameAssetType::Skeleton},
        {"Animation", GameAssetType::Animation},
        {"animation", GameAssetType::Animation},
        {"AnimationSet", GameAssetType::AnimationSet},
        {"animationset", GameAssetType::AnimationSet},
        {"Entity", GameAssetType::Entity},
        {"entity", GameAssetType::Entity},
        {"Hero", GameAssetType::Hero},
        {"hero", GameAssetType::Hero},
        {"ResourceNode", GameAssetType::ResourceNode},
        {"resourcenode", GameAssetType::ResourceNode},
        {"Projectile", GameAssetType::Projectile},
        {"projectile", GameAssetType::Projectile},
        {"Behavior", GameAssetType::Behavior},
        {"behavior", GameAssetType::Behavior},
        {"TechTree", GameAssetType::TechTree},
        {"techtree", GameAssetType::TechTree},
        {"Upgrade", GameAssetType::Upgrade},
        {"upgrade", GameAssetType::Upgrade},
        {"Campaign", GameAssetType::Campaign},
        {"campaign", GameAssetType::Campaign},
        {"Mission", GameAssetType::Mission},
        {"mission", GameAssetType::Mission}
    };

    auto it = typeMap.find(str);
    return it != typeMap.end() ? it->second : GameAssetType::Unknown;
}

std::string GetAssetTypeDisplayName(GameAssetType type) {
    switch (type) {
        case GameAssetType::SDFModel:      return "SDF Model";
        case GameAssetType::Skeleton:      return "Skeleton";
        case GameAssetType::Animation:     return "Animation";
        case GameAssetType::AnimationSet:  return "Animation Set";
        case GameAssetType::Entity:        return "Entity";
        case GameAssetType::Hero:          return "Hero";
        case GameAssetType::ResourceNode:  return "Resource Node";
        case GameAssetType::Projectile:    return "Projectile";
        case GameAssetType::Behavior:      return "Behavior";
        case GameAssetType::TechTree:      return "Tech Tree";
        case GameAssetType::Upgrade:       return "Upgrade";
        case GameAssetType::Campaign:      return "Campaign";
        case GameAssetType::Mission:       return "Mission";
        default:                           return "Unknown";
    }
}

const char* GetAssetTypeIcon(GameAssetType type) {
    // FontAwesome icons (or placeholder text)
    switch (type) {
        case GameAssetType::SDFModel:      return "[3D]";
        case GameAssetType::Skeleton:      return "[SK]";
        case GameAssetType::Animation:     return "[AN]";
        case GameAssetType::AnimationSet:  return "[AS]";
        case GameAssetType::Entity:        return "[EN]";
        case GameAssetType::Hero:          return "[HR]";
        case GameAssetType::ResourceNode:  return "[RN]";
        case GameAssetType::Projectile:    return "[PJ]";
        case GameAssetType::Behavior:      return "[BH]";
        case GameAssetType::TechTree:      return "[TT]";
        case GameAssetType::Upgrade:       return "[UP]";
        case GameAssetType::Campaign:      return "[CP]";
        case GameAssetType::Mission:       return "[MS]";
        default:                           return "[??]";
    }
}

// ============================================================================
// JsonUndoManager Implementation
// ============================================================================

JsonUndoManager::JsonUndoManager(size_t maxHistory)
    : m_maxHistory(maxHistory) {
}

void JsonUndoManager::RecordAction(const std::string& description, const std::string& jsonPath,
                                    const nlohmann::json& oldValue, const nlohmann::json& newValue) {
    // Clear redo stack when new action is recorded
    m_redoStack.clear();

    // Create action
    UndoAction action;
    action.description = description;
    action.jsonPath = jsonPath;
    action.oldValue = oldValue;
    action.newValue = newValue;
    action.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Add to undo stack
    m_undoStack.push_back(std::move(action));

    // Limit stack size
    while (m_undoStack.size() > m_maxHistory) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

bool JsonUndoManager::Undo(nlohmann::json& root) {
    if (m_undoStack.empty()) return false;

    UndoAction action = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    // Apply old value
    ApplyAction(root, action.jsonPath, action.oldValue);

    // Push to redo stack
    m_redoStack.push_back(std::move(action));

    return true;
}

bool JsonUndoManager::Redo(nlohmann::json& root) {
    if (m_redoStack.empty()) return false;

    UndoAction action = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    // Apply new value
    ApplyAction(root, action.jsonPath, action.newValue);

    // Push back to undo stack
    m_undoStack.push_back(std::move(action));

    return true;
}

std::string JsonUndoManager::GetUndoDescription() const {
    if (m_undoStack.empty()) return "";
    return m_undoStack.back().description;
}

std::string JsonUndoManager::GetRedoDescription() const {
    if (m_redoStack.empty()) return "";
    return m_redoStack.back().description;
}

void JsonUndoManager::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void JsonUndoManager::ApplyAction(nlohmann::json& root, const std::string& path, const nlohmann::json& value) {
    try {
        if (path.empty() || path == "/") {
            root = value;
            return;
        }

        // Use JSON pointer for path navigation
        nlohmann::json::json_pointer ptr(path);

        if (value.is_null()) {
            // Remove the value
            // Get parent and key
            std::string parentPath = path.substr(0, path.rfind('/'));
            std::string key = path.substr(path.rfind('/') + 1);

            if (parentPath.empty()) {
                root.erase(key);
            } else {
                nlohmann::json::json_pointer parentPtr(parentPath);
                root[parentPtr].erase(key);
            }
        } else {
            root[ptr] = value;
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to apply undo action: {}", e.what());
    }
}

// ============================================================================
// GenericJSONAssetEditor Implementation
// ============================================================================

GenericJSONAssetEditor::GenericJSONAssetEditor()
    : m_undoManager(100) {
}

GenericJSONAssetEditor::~GenericJSONAssetEditor() {
    if (m_isOpen) {
        Close(true);
    }
}

bool GenericJSONAssetEditor::Open(const std::string& assetPath) {
    if (m_isOpen && m_dirty) {
        spdlog::warn("Closing dirty asset without saving: {}", m_assetPath);
    }

    // Clear previous state
    m_json = nlohmann::json();
    m_originalJson = nlohmann::json();
    m_undoManager.Clear();
    m_validationResult = ValidationResult();
    m_treeNeedsRebuild = true;
    m_selectedPath.clear();
    m_selectedNode = nullptr;

    // Load file
    try {
        std::ifstream file(assetPath);
        if (!file.is_open()) {
            spdlog::error("Failed to open asset file: {}", assetPath);
            return false;
        }

        file >> m_json;
        file.close();

        m_originalJson = m_json;
        m_assetPath = assetPath;
        m_isOpen = true;
        m_dirty = false;

        // Detect asset type from JSON
        m_assetType = AssetEditorFactory::Instance().DetectAssetType(assetPath);

        // Get schema if available
        if (auto* schema = AssetEditorFactory::Instance().GetSchema(m_assetType)) {
            m_schema = *schema;
        }

        // Initial validation
        if (m_autoValidate) {
            Validate();
        }

        spdlog::info("Opened asset: {} (type: {})", assetPath, GameAssetTypeToString(m_assetType));
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to parse JSON asset: {} - {}", assetPath, e.what());
        return false;
    }
}

bool GenericJSONAssetEditor::Save() {
    if (!m_isOpen) return false;
    return SaveAs(m_assetPath);
}

bool GenericJSONAssetEditor::SaveAs(const std::string& newPath) {
    if (!m_isOpen) return false;

    try {
        // Create parent directories if needed
        fs::path path(newPath);
        if (path.has_parent_path() && !fs::exists(path.parent_path())) {
            fs::create_directories(path.parent_path());
        }

        // Write JSON with pretty printing
        std::ofstream file(newPath);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", newPath);
            return false;
        }

        file << m_json.dump(2);
        file.close();

        m_assetPath = newPath;
        m_originalJson = m_json;
        ClearDirty();

        if (OnSaved) OnSaved();

        spdlog::info("Saved asset: {}", newPath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save asset: {} - {}", newPath, e.what());
        return false;
    }
}

bool GenericJSONAssetEditor::Close(bool force) {
    if (!m_isOpen) return true;

    if (m_dirty && !force) {
        return false;  // Caller should prompt for save
    }

    m_json = nlohmann::json();
    m_originalJson = nlohmann::json();
    m_assetPath.clear();
    m_assetType = GameAssetType::Unknown;
    m_isOpen = false;
    m_dirty = false;
    m_undoManager.Clear();
    m_rootNode = JsonTreeNode();
    m_selectedPath.clear();
    m_selectedNode = nullptr;

    if (OnClosed) OnClosed();

    return true;
}

void GenericJSONAssetEditor::Render() {
    if (!m_isOpen) {
        ImGui::TextDisabled("No asset open");
        return;
    }

    // Rebuild tree if needed
    if (m_treeNeedsRebuild) {
        BuildTreeFromJson();
        m_treeNeedsRebuild = false;
    }

    // Render toolbar
    RenderToolbar();
    ImGui::Separator();

    // Main content area with splitters
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float availableHeight = ImGui::GetContentRegionAvail().y;

    // Calculate widths
    float previewWidth = m_showPreview ? 300.0f : 0.0f;
    float remainingWidth = availableWidth - previewWidth - m_inspectorWidth;
    if (remainingWidth < 200.0f) remainingWidth = 200.0f;

    // Tree view panel
    ImGui::BeginChild("##TreeView", ImVec2(remainingWidth, availableHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    RenderTreeView();
    ImGui::EndChild();

    ImGui::SameLine();

    // Property inspector panel
    ImGui::BeginChild("##Inspector", ImVec2(m_inspectorWidth, availableHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    RenderPropertyInspector();
    ImGui::EndChild();

    // Preview panel (if enabled)
    if (m_showPreview && m_schema.supportsPreview) {
        ImGui::SameLine();
        ImGui::BeginChild("##Preview", ImVec2(previewWidth, availableHeight), ImGuiChildFlags_Borders);
        RenderPreview();
        ImGui::EndChild();
    }

    // Status bar
    RenderStatusBar();

    // Context menu
    RenderContextMenu();

    // Validation errors popup
    if (m_showValidationErrors) {
        ShowValidationErrors();
    }
}

void GenericJSONAssetEditor::Update(float deltaTime) {
    if (!m_isOpen) return;

    // Debounced preview update
    if (m_dirty && m_previewCallback) {
        m_previewUpdateTimer += deltaTime;
        if (m_previewUpdateTimer >= PREVIEW_UPDATE_DELAY) {
            m_previewCallback(m_json);
            m_previewUpdateTimer = 0.0f;
        }
    }
}

void GenericJSONAssetEditor::Undo() {
    if (m_undoManager.Undo(m_json)) {
        m_treeNeedsRebuild = true;
        MarkDirty();
        if (m_autoValidate) Validate();
    }
}

void GenericJSONAssetEditor::Redo() {
    if (m_undoManager.Redo(m_json)) {
        m_treeNeedsRebuild = true;
        MarkDirty();
        if (m_autoValidate) Validate();
    }
}

nlohmann::json& GenericJSONAssetEditor::GetMutableJson() {
    MarkDirty();
    return m_json;
}

void GenericJSONAssetEditor::SetSelectedPath(const std::string& path) {
    m_selectedPath = path;
    // Find and select the corresponding node in the tree
    // (Tree traversal would be implemented here)
}

// ============================================================================
// UI Rendering
// ============================================================================

void GenericJSONAssetEditor::RenderToolbar() {
    EditorWidgets::BeginToolbar("##AssetToolbar");

    // Save button
    if (EditorWidgets::ToolbarButton("[S]", "Save (Ctrl+S)", false)) {
        Save();
    }
    ImGui::SameLine();

    // Undo/Redo
    {
        ScopedDisable disable(!CanUndo());
        if (EditorWidgets::ToolbarButton("[<]", CanUndo() ? ("Undo: " + m_undoManager.GetUndoDescription()).c_str() : "Undo", false)) {
            Undo();
        }
    }
    ImGui::SameLine();

    {
        ScopedDisable disable(!CanRedo());
        if (EditorWidgets::ToolbarButton("[>]", CanRedo() ? ("Redo: " + m_undoManager.GetRedoDescription()).c_str() : "Redo", false)) {
            Redo();
        }
    }

    EditorWidgets::ToolbarSeparator();

    // Expand/Collapse all
    if (EditorWidgets::ToolbarButton("[+]", "Expand All", false)) {
        ExpandAll(m_rootNode);
    }
    ImGui::SameLine();

    if (EditorWidgets::ToolbarButton("[-]", "Collapse All", false)) {
        CollapseAll(m_rootNode);
    }

    EditorWidgets::ToolbarSeparator();

    // Validate button
    if (EditorWidgets::ToolbarButton("[V]", "Validate", false)) {
        Validate();
        m_showValidationErrors = !m_validationResult.isValid;
    }
    ImGui::SameLine();

    // Preview toggle
    if (m_schema.supportsPreview) {
        if (EditorWidgets::ToolbarButton("[P]", "Toggle Preview", m_showPreview)) {
            m_showPreview = !m_showPreview;
        }
    }

    EditorWidgets::ToolbarSpacer();

    // Asset type badge
    auto& theme = EditorTheme::Instance();
    EditorWidgets::Badge(GetAssetTypeDisplayName(m_assetType).c_str(), theme.GetColors().accent);

    EditorWidgets::EndToolbar();
}

void GenericJSONAssetEditor::RenderTreeView() {
    // Search bar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    if (EditorWidgets::SearchInput("##TreeSearch", m_searchBuffer, sizeof(m_searchBuffer), "Filter...")) {
        m_searchFilter = m_searchBuffer;
        // Filtering would expand matching nodes
    }
    ImGui::PopStyleVar();
    ImGui::Separator();

    // Tree view
    ImGui::BeginChild("##TreeContent", ImVec2(0, 0), ImGuiChildFlags_None);

    if (m_rootNode.children.empty() && !m_json.empty()) {
        BuildTreeFromJson();
    }

    // Render root node children
    for (auto& child : m_rootNode.children) {
        RenderTreeNode(child);
    }

    ImGui::EndChild();
}

void GenericJSONAssetEditor::RenderTreeNode(JsonTreeNode& node) {
    // Apply search filter
    if (!m_searchFilter.empty()) {
        std::string keyLower = node.key;
        std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
        std::string filterLower = m_searchFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        bool matches = keyLower.find(filterLower) != std::string::npos ||
                       node.displayValue.find(filterLower) != std::string::npos;

        // Check if any children match
        bool childMatches = false;
        for (auto& child : node.children) {
            // Simplified check - would recursively check in full implementation
            childMatches = true;
            break;
        }

        if (!matches && !childMatches && node.children.empty()) {
            return;  // Skip non-matching leaf nodes
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;

    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node.selected || node.path == m_selectedPath) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (node.expanded) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    // Build display text
    std::string displayText = node.key;
    if (!node.displayValue.empty() && node.children.empty()) {
        displayText += ": " + node.displayValue;
    } else if (node.isArray) {
        displayText += " [" + std::to_string(node.childCount) + "]";
    } else if (node.isObject && node.childCount > 0) {
        displayText += " {" + std::to_string(node.childCount) + "}";
    }

    // Color code by type
    auto& theme = EditorTheme::Instance();
    ImVec4 textColor = EditorTheme::ToImVec4(theme.GetColors().text);

    if (node.typeString == "string") {
        textColor = EditorTheme::ToImVec4(theme.GetColors().pinString);
    } else if (node.typeString == "number") {
        textColor = EditorTheme::ToImVec4(theme.GetColors().pinFloat);
    } else if (node.typeString == "boolean") {
        textColor = EditorTheme::ToImVec4(theme.GetColors().pinBool);
    } else if (node.typeString == "array") {
        textColor = EditorTheme::ToImVec4(theme.GetColors().pinVector);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    bool open = ImGui::TreeNodeEx(node.path.c_str(), flags, "%s", displayText.c_str());
    ImGui::PopStyleColor();

    // Handle selection
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        SelectNode(node);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        m_contextMenuPath = node.path;

        if (ImGui::MenuItem("Copy Path")) {
            ImGui::SetClipboardText(node.path.c_str());
        }

        if (ImGui::MenuItem("Copy Value")) {
            if (node.value) {
                ImGui::SetClipboardText(node.value->dump(2).c_str());
            }
        }

        ImGui::Separator();

        if (node.isObject || node.isArray) {
            if (ImGui::MenuItem("Add Property...")) {
                // Would open add property dialog
            }
        }

        if (!node.path.empty() && node.path != "/") {
            if (ImGui::MenuItem("Delete")) {
                RemoveProperty(node.path);
            }

            if (ImGui::MenuItem("Duplicate")) {
                DuplicateProperty(node.path);
            }
        }

        ImGui::EndPopup();
    }

    // Render children
    if (open) {
        node.expanded = true;
        for (auto& child : node.children) {
            RenderTreeNode(child);
        }
        ImGui::TreePop();
    } else {
        node.expanded = false;
    }
}

void GenericJSONAssetEditor::RenderPropertyInspector() {
    ImGui::Text("Properties");
    ImGui::Separator();

    if (m_selectedPath.empty()) {
        ImGui::TextDisabled("Select a node to edit its properties");
        return;
    }

    nlohmann::json* selectedJson = GetJsonAtPath(m_selectedPath);
    if (!selectedJson) {
        ImGui::TextDisabled("Invalid selection");
        return;
    }

    // Get schema for this path
    const PropertySchema* schema = GetSchemaForPath(m_selectedPath);

    // Property path display
    ImGui::TextDisabled("Path: %s", m_selectedPath.c_str());
    ImGui::Separator();

    // Edit the value based on type
    std::string key = GetKeyFromPath(m_selectedPath);
    if (RenderPropertyEditor(key, *selectedJson, schema)) {
        MarkDirty();
        m_treeNeedsRebuild = true;
        if (m_autoValidate) Validate();
    }
}

bool GenericJSONAssetEditor::RenderPropertyEditor(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    bool changed = false;

    // Determine type
    std::string typeStr = schema ? schema->type : "";

    if (value.is_string()) {
        changed = RenderStringProperty(key, value, schema);
    } else if (value.is_number_float()) {
        changed = RenderNumberProperty(key, value, schema);
    } else if (value.is_number_integer()) {
        changed = RenderNumberProperty(key, value, schema);
    } else if (value.is_boolean()) {
        changed = RenderBoolProperty(key, value, schema);
    } else if (value.is_array()) {
        changed = RenderArrayProperty(key, value, schema);
    } else if (value.is_object()) {
        changed = RenderObjectProperty(key, value, schema);
    } else if (value.is_null()) {
        ImGui::TextDisabled("null");
    }

    return changed;
}

bool GenericJSONAssetEditor::RenderStringProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string strValue = value.get<std::string>();
    std::string label = schema ? schema->displayName : key;

    // Check for special types
    if (schema) {
        if (schema->type == "color") {
            return RenderColorProperty(key, value, schema);
        } else if (schema->type == "asset") {
            return RenderAssetProperty(key, value, schema);
        } else if (schema->type == "enum" && !schema->enumOptions.empty()) {
            return RenderEnumProperty(key, value, schema);
        }
    }

    // Multi-line text for long strings
    if (strValue.length() > 100 || strValue.find('\n') != std::string::npos) {
        if (EditorWidgets::TextAreaInput(label.c_str(), strValue, glm::vec2(0, 100))) {
            nlohmann::json oldValue = value;
            value = strValue;
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            return true;
        }
    } else {
        if (EditorWidgets::Property(label.c_str(), strValue)) {
            nlohmann::json oldValue = value;
            value = strValue;
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            return true;
        }
    }

    // Show description tooltip
    if (schema && !schema->description.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", schema->description.c_str());
    }

    return false;
}

bool GenericJSONAssetEditor::RenderNumberProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;

    if (value.is_number_integer()) {
        int intValue = value.get<int>();
        int min = INT_MIN, max = INT_MAX;

        if (schema && schema->constraints.contains("minimum")) {
            min = schema->constraints["minimum"].get<int>();
        }
        if (schema && schema->constraints.contains("maximum")) {
            max = schema->constraints["maximum"].get<int>();
        }

        if (EditorWidgets::Property(label.c_str(), intValue, min, max)) {
            nlohmann::json oldValue = value;
            value = intValue;
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            return true;
        }
    } else {
        float floatValue = value.get<float>();
        float min = -FLT_MAX, max = FLT_MAX;
        float speed = 0.1f;

        if (schema && schema->constraints.contains("minimum")) {
            min = schema->constraints["minimum"].get<float>();
        }
        if (schema && schema->constraints.contains("maximum")) {
            max = schema->constraints["maximum"].get<float>();
        }
        if (schema && schema->constraints.contains("step")) {
            speed = schema->constraints["step"].get<float>();
        }

        if (EditorWidgets::Property(label.c_str(), floatValue, min, max, speed)) {
            nlohmann::json oldValue = value;
            value = floatValue;
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            return true;
        }
    }

    if (schema && !schema->description.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", schema->description.c_str());
    }

    return false;
}

bool GenericJSONAssetEditor::RenderBoolProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;
    bool boolValue = value.get<bool>();

    if (EditorWidgets::Property(label.c_str(), boolValue)) {
        nlohmann::json oldValue = value;
        value = boolValue;
        m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
        return true;
    }

    if (schema && !schema->description.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", schema->description.c_str());
    }

    return false;
}

bool GenericJSONAssetEditor::RenderArrayProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;
    bool changed = false;

    if (EditorWidgets::CollapsingHeader(label.c_str(), nullptr, true)) {
        ScopedIndent indent;

        // Array size info
        ImGui::TextDisabled("Items: %zu", value.size());

        // Add item button
        if (ImGui::SmallButton("+ Add Item")) {
            nlohmann::json oldValue = value;
            value.push_back(nlohmann::json());
            m_undoManager.RecordAction("Add array item", m_selectedPath, oldValue, value);
            changed = true;
        }

        ImGui::Separator();

        // Render array items
        int indexToRemove = -1;
        for (size_t i = 0; i < value.size(); i++) {
            ImGui::PushID(static_cast<int>(i));

            std::string itemLabel = "[" + std::to_string(i) + "]";

            // Delete button
            if (ImGui::SmallButton("X")) {
                indexToRemove = static_cast<int>(i);
            }
            ImGui::SameLine();

            // Edit item
            if (RenderPropertyEditor(itemLabel, value[i], nullptr)) {
                changed = true;
            }

            ImGui::PopID();
        }

        // Remove item if requested
        if (indexToRemove >= 0) {
            nlohmann::json oldValue = value;
            value.erase(value.begin() + indexToRemove);
            m_undoManager.RecordAction("Remove array item", m_selectedPath, oldValue, value);
            changed = true;
        }
    }

    return changed;
}

bool GenericJSONAssetEditor::RenderObjectProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;
    bool changed = false;

    if (EditorWidgets::CollapsingHeader(label.c_str(), nullptr, true)) {
        ScopedIndent indent;

        // Property count info
        ImGui::TextDisabled("Properties: %zu", value.size());

        // Add property button
        if (ImGui::SmallButton("+ Add Property")) {
            // Would open dialog to add new property
        }

        ImGui::Separator();

        // Render object properties
        for (auto& [propKey, propValue] : value.items()) {
            ImGui::PushID(propKey.c_str());

            // Find schema for this property
            const PropertySchema* propSchema = nullptr;
            if (schema) {
                // Search in schema properties
                for (const auto& s : m_schema.properties) {
                    if (s.name == propKey) {
                        propSchema = &s;
                        break;
                    }
                }
            }

            if (RenderPropertyEditor(propKey, propValue, propSchema)) {
                changed = true;
            }

            ImGui::PopID();
        }
    }

    return changed;
}

bool GenericJSONAssetEditor::RenderColorProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;

    // Parse color from string (supports "#RRGGBB" or "rgb(r,g,b)" or array)
    glm::vec4 color(1.0f);

    if (value.is_string()) {
        std::string colorStr = value.get<std::string>();
        // Parse hex color
        if (colorStr.length() >= 7 && colorStr[0] == '#') {
            unsigned int hex = std::stoul(colorStr.substr(1), nullptr, 16);
            color.r = ((hex >> 16) & 0xFF) / 255.0f;
            color.g = ((hex >> 8) & 0xFF) / 255.0f;
            color.b = (hex & 0xFF) / 255.0f;
            color.a = 1.0f;
        }
    } else if (value.is_array() && value.size() >= 3) {
        color.r = value[0].get<float>();
        color.g = value[1].get<float>();
        color.b = value[2].get<float>();
        color.a = value.size() > 3 ? value[3].get<float>() : 1.0f;
    }

    if (EditorWidgets::ColorProperty(label.c_str(), color)) {
        nlohmann::json oldValue = value;

        // Store as array
        value = {color.r, color.g, color.b, color.a};

        m_undoManager.RecordAction("Edit color " + key, m_selectedPath, oldValue, value);
        return true;
    }

    return false;
}

bool GenericJSONAssetEditor::RenderEnumProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    if (!schema || schema->enumOptions.empty()) {
        return RenderStringProperty(key, value, schema);
    }

    std::string label = schema->displayName.empty() ? key : schema->displayName;
    std::string currentValue = value.get<std::string>();

    int selectedIndex = -1;
    for (size_t i = 0; i < schema->enumOptions.size(); i++) {
        if (schema->enumOptions[i] == currentValue) {
            selectedIndex = static_cast<int>(i);
            break;
        }
    }

    if (EditorWidgets::SearchableCombo(label.c_str(), selectedIndex, schema->enumOptions)) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(schema->enumOptions.size())) {
            nlohmann::json oldValue = value;
            value = schema->enumOptions[selectedIndex];
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            return true;
        }
    }

    if (!schema->description.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", schema->description.c_str());
    }

    return false;
}

bool GenericJSONAssetEditor::RenderAssetProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;
    std::string assetPath = value.get<std::string>();
    std::string filter = schema ? schema->assetFilter : "*.*";

    if (EditorWidgets::AssetProperty(label.c_str(), assetPath, filter.c_str())) {
        nlohmann::json oldValue = value;
        value = assetPath;
        m_undoManager.RecordAction("Edit asset " + key, m_selectedPath, oldValue, value);
        return true;
    }

    if (schema && !schema->description.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", schema->description.c_str());
    }

    return false;
}

bool GenericJSONAssetEditor::RenderVectorProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema) {
    std::string label = schema ? schema->displayName : key;

    if (!value.is_array()) return false;

    size_t size = value.size();
    bool changed = false;

    if (size == 2) {
        glm::vec2 vec(value[0].get<float>(), value[1].get<float>());
        if (EditorWidgets::Property(label.c_str(), vec)) {
            nlohmann::json oldValue = value;
            value = {vec.x, vec.y};
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            changed = true;
        }
    } else if (size == 3) {
        glm::vec3 vec(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
        if (EditorWidgets::Property(label.c_str(), vec)) {
            nlohmann::json oldValue = value;
            value = {vec.x, vec.y, vec.z};
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            changed = true;
        }
    } else if (size == 4) {
        glm::vec4 vec(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>());
        if (EditorWidgets::Property(label.c_str(), vec)) {
            nlohmann::json oldValue = value;
            value = {vec.x, vec.y, vec.z, vec.w};
            m_undoManager.RecordAction("Edit " + key, m_selectedPath, oldValue, value);
            changed = true;
        }
    } else {
        // Fall back to array editor
        changed = RenderArrayProperty(key, value, schema);
    }

    return changed;
}

void GenericJSONAssetEditor::RenderPreview() {
    ImGui::Text("Preview");
    ImGui::Separator();

    if (m_previewCallback) {
        // Preview would be rendered by the callback
        ImGui::TextDisabled("Preview rendering handled by callback");
    } else {
        ImGui::TextDisabled("No preview available");
    }
}

void GenericJSONAssetEditor::RenderStatusBar() {
    auto& theme = EditorTheme::Instance();

    ImGui::Separator();
    ImGui::BeginChild("##StatusBar", ImVec2(0, theme.GetSizes().statusBarHeight), ImGuiChildFlags_None);

    // Dirty indicator
    if (m_dirty) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().warning));
        ImGui::Text("*");
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    // File path
    ImGui::TextDisabled("%s", m_assetPath.c_str());

    // Validation status
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 150);
    if (m_validationResult.isValid) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().success));
        ImGui::Text("Valid");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().error));
        ImGui::Text("Errors: %zu", m_validationResult.errors.size());
        ImGui::PopStyleColor();
        if (ImGui::IsItemClicked()) {
            m_showValidationErrors = true;
        }
    }

    // Undo count
    ImGui::SameLine();
    ImGui::TextDisabled("Undo: %zu", m_undoManager.GetUndoCount());

    ImGui::EndChild();
}

void GenericJSONAssetEditor::RenderContextMenu() {
    // Context menu is rendered inline with tree nodes
}

// ============================================================================
// Tree View Helpers
// ============================================================================

void GenericJSONAssetEditor::BuildTreeFromJson() {
    m_rootNode = JsonTreeNode();
    m_rootNode.key = "root";
    m_rootNode.value = &m_json;
    m_rootNode.path = "";
    m_rootNode.isObject = m_json.is_object();
    m_rootNode.isArray = m_json.is_array();
    m_rootNode.expanded = true;

    std::function<void(JsonTreeNode&, nlohmann::json&, const std::string&, int)> buildRecursive;
    buildRecursive = [&](JsonTreeNode& parent, nlohmann::json& json, const std::string& basePath, int depth) {
        if (json.is_object()) {
            for (auto& [key, val] : json.items()) {
                JsonTreeNode child;
                child.key = key;
                child.value = &val;
                child.path = basePath + "/" + key;
                child.depth = depth;
                child.isObject = val.is_object();
                child.isArray = val.is_array();

                if (val.is_object()) {
                    child.childCount = val.size();
                    child.typeString = "object";
                    buildRecursive(child, val, child.path, depth + 1);
                } else if (val.is_array()) {
                    child.childCount = val.size();
                    child.typeString = "array";
                    buildRecursive(child, val, child.path, depth + 1);
                } else if (val.is_string()) {
                    child.displayValue = "\"" + val.get<std::string>().substr(0, 50) + "\"";
                    child.typeString = "string";
                } else if (val.is_number()) {
                    child.displayValue = val.dump();
                    child.typeString = "number";
                } else if (val.is_boolean()) {
                    child.displayValue = val.get<bool>() ? "true" : "false";
                    child.typeString = "boolean";
                } else if (val.is_null()) {
                    child.displayValue = "null";
                    child.typeString = "null";
                }

                parent.children.push_back(std::move(child));
            }
        } else if (json.is_array()) {
            for (size_t i = 0; i < json.size(); i++) {
                JsonTreeNode child;
                child.key = "[" + std::to_string(i) + "]";
                child.value = &json[i];
                child.path = basePath + "/" + std::to_string(i);
                child.depth = depth;
                child.isObject = json[i].is_object();
                child.isArray = json[i].is_array();

                if (json[i].is_object()) {
                    child.childCount = json[i].size();
                    child.typeString = "object";
                    buildRecursive(child, json[i], child.path, depth + 1);
                } else if (json[i].is_array()) {
                    child.childCount = json[i].size();
                    child.typeString = "array";
                    buildRecursive(child, json[i], child.path, depth + 1);
                } else if (json[i].is_string()) {
                    child.displayValue = "\"" + json[i].get<std::string>().substr(0, 50) + "\"";
                    child.typeString = "string";
                } else if (json[i].is_number()) {
                    child.displayValue = json[i].dump();
                    child.typeString = "number";
                } else if (json[i].is_boolean()) {
                    child.displayValue = json[i].get<bool>() ? "true" : "false";
                    child.typeString = "boolean";
                } else if (json[i].is_null()) {
                    child.displayValue = "null";
                    child.typeString = "null";
                }

                parent.children.push_back(std::move(child));
            }
        }
    };

    buildRecursive(m_rootNode, m_json, "", 0);
    m_rootNode.childCount = m_rootNode.children.size();
}

void GenericJSONAssetEditor::SelectNode(JsonTreeNode& node) {
    // Deselect previous
    if (m_selectedNode) {
        m_selectedNode->selected = false;
    }

    // Select new
    node.selected = true;
    m_selectedNode = &node;
    m_selectedPath = node.path;
}

void GenericJSONAssetEditor::ExpandAll(JsonTreeNode& node) {
    node.expanded = true;
    for (auto& child : node.children) {
        ExpandAll(child);
    }
}

void GenericJSONAssetEditor::CollapseAll(JsonTreeNode& node) {
    node.expanded = false;
    for (auto& child : node.children) {
        CollapseAll(child);
    }
}

// ============================================================================
// JSON Operations
// ============================================================================

void GenericJSONAssetEditor::AddProperty(const std::string& parentPath, const std::string& key, const nlohmann::json& value) {
    nlohmann::json* parent = GetJsonAtPath(parentPath);
    if (!parent || !parent->is_object()) return;

    nlohmann::json oldParent = *parent;
    (*parent)[key] = value;

    m_undoManager.RecordAction("Add property " + key, parentPath, oldParent, *parent);
    m_treeNeedsRebuild = true;
    MarkDirty();
}

void GenericJSONAssetEditor::RemoveProperty(const std::string& path) {
    if (path.empty() || path == "/") return;

    std::string parentPath = GetParentPath(path);
    std::string key = GetKeyFromPath(path);

    nlohmann::json* parent = GetJsonAtPath(parentPath);
    if (!parent) return;

    nlohmann::json oldValue = (*parent)[key];

    if (parent->is_object()) {
        parent->erase(key);
    } else if (parent->is_array()) {
        int index = std::stoi(key);
        parent->erase(parent->begin() + index);
    }

    m_undoManager.RecordAction("Remove " + key, path, oldValue, nlohmann::json());
    m_treeNeedsRebuild = true;
    MarkDirty();

    // Clear selection if removed node was selected
    if (m_selectedPath == path || m_selectedPath.find(path + "/") == 0) {
        m_selectedPath.clear();
        m_selectedNode = nullptr;
    }
}

void GenericJSONAssetEditor::RenameProperty(const std::string& path, const std::string& newKey) {
    std::string parentPath = GetParentPath(path);
    std::string oldKey = GetKeyFromPath(path);

    nlohmann::json* parent = GetJsonAtPath(parentPath);
    if (!parent || !parent->is_object()) return;

    nlohmann::json value = (*parent)[oldKey];
    parent->erase(oldKey);
    (*parent)[newKey] = value;

    m_undoManager.RecordAction("Rename " + oldKey + " to " + newKey, parentPath, nlohmann::json(), nlohmann::json());
    m_treeNeedsRebuild = true;
    MarkDirty();
}

void GenericJSONAssetEditor::DuplicateProperty(const std::string& path) {
    std::string parentPath = GetParentPath(path);
    std::string key = GetKeyFromPath(path);

    nlohmann::json* parent = GetJsonAtPath(parentPath);
    if (!parent) return;

    nlohmann::json value = (*parent)[key];

    if (parent->is_object()) {
        // Find unique name
        std::string newKey = key + "_copy";
        int counter = 1;
        while (parent->contains(newKey)) {
            newKey = key + "_copy" + std::to_string(counter++);
        }
        (*parent)[newKey] = value;
    } else if (parent->is_array()) {
        parent->push_back(value);
    }

    m_treeNeedsRebuild = true;
    MarkDirty();
}

void GenericJSONAssetEditor::MoveProperty(const std::string& fromPath, const std::string& toPath) {
    // Implementation would handle drag-drop reordering
}

// ============================================================================
// Validation
// ============================================================================

void GenericJSONAssetEditor::Validate() {
    m_validationResult = ValidationResult();

    if (m_validationCallback) {
        m_validationResult = m_validationCallback(m_json);
    } else if (!m_schema.jsonSchema.empty()) {
        // Use JSON Schema validation
        // (Simplified - full implementation would use a JSON Schema validator)
        m_validationResult.isValid = true;
    } else {
        // Basic validation
        m_validationResult.isValid = !m_json.empty();
    }
}

void GenericJSONAssetEditor::ShowValidationErrors() {
    if (!ImGui::BeginPopupModal("Validation Errors", &m_showValidationErrors, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    auto& theme = EditorTheme::Instance();

    ImGui::Text("Found %zu errors and %zu warnings", m_validationResult.errors.size(), m_validationResult.warnings.size());
    ImGui::Separator();

    // Errors
    if (!m_validationResult.errors.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().error));
        for (const auto& error : m_validationResult.errors) {
            ImGui::BulletText("%s", error.c_str());
        }
        ImGui::PopStyleColor();
    }

    // Warnings
    if (!m_validationResult.warnings.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorTheme::ToImVec4(theme.GetColors().warning));
        for (const auto& warning : m_validationResult.warnings) {
            ImGui::BulletText("%s", warning.c_str());
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 0))) {
        m_showValidationErrors = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ============================================================================
// Internal Helpers
// ============================================================================

nlohmann::json* GenericJSONAssetEditor::GetJsonAtPath(const std::string& path) {
    if (path.empty()) {
        return &m_json;
    }

    try {
        nlohmann::json::json_pointer ptr(path);
        return &m_json[ptr];
    } catch (...) {
        return nullptr;
    }
}

const PropertySchema* GenericJSONAssetEditor::GetSchemaForPath(const std::string& path) const {
    // Find schema property that matches this path
    for (const auto& prop : m_schema.properties) {
        if ("/" + prop.name == path) {
            return &prop;
        }
    }
    return nullptr;
}

std::string GenericJSONAssetEditor::GetParentPath(const std::string& path) const {
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return "";
    }
    return path.substr(0, lastSlash);
}

std::string GenericJSONAssetEditor::GetKeyFromPath(const std::string& path) const {
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos) {
        return path;
    }
    return path.substr(lastSlash + 1);
}

void GenericJSONAssetEditor::MarkDirty() {
    if (!m_dirty) {
        m_dirty = true;
        if (OnDirtyChanged) OnDirtyChanged(true);
    }
    m_previewUpdateTimer = 0.0f;
}

void GenericJSONAssetEditor::ClearDirty() {
    if (m_dirty) {
        m_dirty = false;
        if (OnDirtyChanged) OnDirtyChanged(false);
    }
}

// ============================================================================
// AssetEditorFactory Implementation
// ============================================================================

AssetEditorFactory& AssetEditorFactory::Instance() {
    static AssetEditorFactory instance;
    return instance;
}

AssetEditorFactory::AssetEditorFactory() {
    // Register default factory for all game asset types
    auto defaultFactory = []() -> std::unique_ptr<IAssetEditor> {
        return std::make_unique<GenericJSONAssetEditor>();
    };

    m_factories[GameAssetType::SDFModel] = defaultFactory;
    m_factories[GameAssetType::Skeleton] = defaultFactory;
    m_factories[GameAssetType::Animation] = defaultFactory;
    m_factories[GameAssetType::AnimationSet] = defaultFactory;
    m_factories[GameAssetType::Entity] = defaultFactory;
    m_factories[GameAssetType::Hero] = defaultFactory;
    m_factories[GameAssetType::ResourceNode] = defaultFactory;
    m_factories[GameAssetType::Projectile] = defaultFactory;
    m_factories[GameAssetType::Behavior] = defaultFactory;
    m_factories[GameAssetType::TechTree] = defaultFactory;
    m_factories[GameAssetType::Upgrade] = defaultFactory;
    m_factories[GameAssetType::Campaign] = defaultFactory;
    m_factories[GameAssetType::Mission] = defaultFactory;

    // Initialize default schemas
    InitializeDefaultSchemas();
}

std::unique_ptr<IAssetEditor> AssetEditorFactory::CreateEditor(GameAssetType type) {
    auto it = m_factories.find(type);
    if (it == m_factories.end()) {
        return nullptr;
    }

    auto editor = it->second();

    // Apply schema if available
    if (auto* jsonEditor = dynamic_cast<GenericJSONAssetEditor*>(editor.get())) {
        if (auto* schema = GetSchema(type)) {
            jsonEditor->SetSchema(*schema);
        }
    }

    return editor;
}

std::unique_ptr<IAssetEditor> AssetEditorFactory::CreateEditorForFile(const std::string& filePath) {
    GameAssetType type = DetectAssetType(filePath);
    if (type == GameAssetType::Unknown) {
        return nullptr;
    }
    return CreateEditor(type);
}

void AssetEditorFactory::RegisterEditorFactory(GameAssetType type, std::function<std::unique_ptr<IAssetEditor>()> factory) {
    m_factories[type] = std::move(factory);
}

void AssetEditorFactory::RegisterSchema(const AssetTypeSchema& schema) {
    m_schemas[schema.type] = schema;
}

const AssetTypeSchema* AssetEditorFactory::GetSchema(GameAssetType type) const {
    auto it = m_schemas.find(type);
    return it != m_schemas.end() ? &it->second : nullptr;
}

GameAssetType AssetEditorFactory::DetectAssetType(const std::string& filePath) {
    // Try to detect from file content
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) return GameAssetType::Unknown;

        nlohmann::json json;
        file >> json;
        file.close();

        // Check for type field
        if (json.contains("type")) {
            std::string typeStr = json["type"].get<std::string>();
            return StringToGameAssetType(typeStr);
        }

        // Check for asset_type field
        if (json.contains("asset_type")) {
            std::string typeStr = json["asset_type"].get<std::string>();
            return StringToGameAssetType(typeStr);
        }

        // Heuristic detection based on fields
        if (json.contains("primitives") && json.contains("operations")) {
            return GameAssetType::SDFModel;
        }
        if (json.contains("bones") && json.contains("hierarchy")) {
            return GameAssetType::Skeleton;
        }
        if (json.contains("keyframes") && json.contains("duration")) {
            return GameAssetType::Animation;
        }
        if (json.contains("animations") && json.is_object()) {
            return GameAssetType::AnimationSet;
        }
        if (json.contains("components") && json.contains("entity_id")) {
            return GameAssetType::Entity;
        }
        if (json.contains("abilities") && json.contains("stats")) {
            return GameAssetType::Hero;
        }
        if (json.contains("resource_type") && json.contains("yield")) {
            return GameAssetType::ResourceNode;
        }
        if (json.contains("damage") && json.contains("speed") && json.contains("trajectory")) {
            return GameAssetType::Projectile;
        }
        if (json.contains("behavior_tree") || json.contains("states")) {
            return GameAssetType::Behavior;
        }
        if (json.contains("techs") && json.contains("dependencies")) {
            return GameAssetType::TechTree;
        }
        if (json.contains("effects") && json.contains("cost") && json.contains("research_time")) {
            return GameAssetType::Upgrade;
        }
        if (json.contains("missions") && json.contains("campaign_name")) {
            return GameAssetType::Campaign;
        }
        if (json.contains("objectives") && json.contains("map")) {
            return GameAssetType::Mission;
        }

    } catch (...) {
        // Failed to parse or detect
    }

    return GameAssetType::Unknown;
}

bool AssetEditorFactory::CanEdit(GameAssetType type) const {
    return m_factories.find(type) != m_factories.end();
}

std::vector<GameAssetType> AssetEditorFactory::GetEditableTypes() const {
    std::vector<GameAssetType> types;
    for (const auto& [type, factory] : m_factories) {
        types.push_back(type);
    }
    return types;
}

void AssetEditorFactory::InitializeDefaultSchemas() {
    // SDFModel Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::SDFModel;
        schema.name = "SDF Model";
        schema.description = "Signed Distance Field 3D model definition";
        schema.supportsPreview = true;

        schema.properties.push_back({"name", "Name", "Model name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"primitives", "Primitives", "SDF primitive shapes", "array", true, nlohmann::json::array(), {}, "Geometry", 1});
        schema.properties.push_back({"operations", "Operations", "CSG operations", "array", false, nlohmann::json::array(), {}, "Geometry", 2});
        schema.properties.push_back({"material", "Material", "Material reference", "asset", false, "", {}, "Rendering", 3});

        m_schemas[GameAssetType::SDFModel] = schema;
    }

    // Hero Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::Hero;
        schema.name = "Hero";
        schema.description = "Hero unit definition with abilities and stats";
        schema.supportsPreview = true;

        schema.properties.push_back({"name", "Name", "Hero display name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"description", "Description", "Hero description", "string", false, "", {}, "General", 1});
        schema.properties.push_back({"health", "Health", "Maximum health points", "integer", true, 100, {{"minimum", 1}, {"maximum", 10000}}, "Stats", 2});
        schema.properties.push_back({"mana", "Mana", "Maximum mana points", "integer", false, 100, {{"minimum", 0}, {"maximum", 10000}}, "Stats", 3});
        schema.properties.push_back({"armor", "Armor", "Armor value", "number", false, 0.0f, {{"minimum", 0.0f}}, "Stats", 4});
        schema.properties.push_back({"speed", "Movement Speed", "Base movement speed", "number", true, 5.0f, {{"minimum", 0.0f}, {"maximum", 100.0f}}, "Stats", 5});
        schema.properties.push_back({"abilities", "Abilities", "Hero abilities", "array", false, nlohmann::json::array(), {}, "Abilities", 6});
        schema.properties.push_back({"model", "Model", "3D model reference", "asset", true, "", {}, "Visuals", 7});
        schema.properties.push_back({"portrait", "Portrait", "Hero portrait image", "asset", false, "", {}, "Visuals", 8});

        m_schemas[GameAssetType::Hero] = schema;
    }

    // Entity Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::Entity;
        schema.name = "Entity";
        schema.description = "Game entity with components";
        schema.supportsPreview = true;

        schema.properties.push_back({"name", "Name", "Entity name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"entity_id", "Entity ID", "Unique entity identifier", "string", true, "", {}, "General", 1});
        schema.properties.push_back({"components", "Components", "Entity components", "array", true, nlohmann::json::array(), {}, "Components", 2});
        schema.properties.push_back({"tags", "Tags", "Entity tags for filtering", "array", false, nlohmann::json::array(), {}, "Metadata", 3});

        m_schemas[GameAssetType::Entity] = schema;
    }

    // Animation Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::Animation;
        schema.name = "Animation";
        schema.description = "Animation clip definition";
        schema.supportsPreview = true;

        schema.properties.push_back({"name", "Name", "Animation name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"duration", "Duration", "Animation duration in seconds", "number", true, 1.0f, {{"minimum", 0.0f}}, "Timing", 1});
        schema.properties.push_back({"fps", "FPS", "Frames per second", "integer", false, 30, {{"minimum", 1}, {"maximum", 120}}, "Timing", 2});
        schema.properties.push_back({"loop", "Loop", "Whether animation loops", "boolean", false, false, {}, "Playback", 3});
        schema.properties.push_back({"keyframes", "Keyframes", "Animation keyframes", "array", true, nlohmann::json::array(), {}, "Data", 4});

        m_schemas[GameAssetType::Animation] = schema;
    }

    // Upgrade Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::Upgrade;
        schema.name = "Upgrade";
        schema.description = "Research upgrade definition";
        schema.supportsPreview = false;

        schema.properties.push_back({"name", "Name", "Upgrade name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"description", "Description", "Upgrade description", "string", false, "", {}, "General", 1});
        schema.properties.push_back({"cost", "Cost", "Resource cost", "object", true, nlohmann::json::object(), {}, "Economy", 2});
        schema.properties.push_back({"research_time", "Research Time", "Time to research in seconds", "number", true, 30.0f, {{"minimum", 0.0f}}, "Timing", 3});
        schema.properties.push_back({"effects", "Effects", "Upgrade effects", "array", true, nlohmann::json::array(), {}, "Effects", 4});
        schema.properties.push_back({"prerequisites", "Prerequisites", "Required upgrades/buildings", "array", false, nlohmann::json::array(), {}, "Requirements", 5});
        schema.properties.push_back({"icon", "Icon", "Upgrade icon", "asset", false, "", {}, "Visuals", 6});

        m_schemas[GameAssetType::Upgrade] = schema;
    }

    // Mission Schema
    {
        AssetTypeSchema schema;
        schema.type = GameAssetType::Mission;
        schema.name = "Mission";
        schema.description = "Campaign mission definition";
        schema.supportsPreview = false;

        schema.properties.push_back({"name", "Name", "Mission name", "string", true, "", {}, "General", 0});
        schema.properties.push_back({"description", "Description", "Mission briefing", "string", false, "", {}, "General", 1});
        schema.properties.push_back({"map", "Map", "Map file reference", "asset", true, "", {}, "World", 2});
        schema.properties.push_back({"objectives", "Objectives", "Mission objectives", "array", true, nlohmann::json::array(), {}, "Gameplay", 3});
        schema.properties.push_back({"triggers", "Triggers", "Event triggers", "array", false, nlohmann::json::array(), {}, "Scripting", 4});
        schema.properties.push_back({"starting_units", "Starting Units", "Player starting units", "array", false, nlohmann::json::array(), {}, "Setup", 5});
        schema.properties.push_back({"enemy_factions", "Enemy Factions", "Enemy AI factions", "array", false, nlohmann::json::array(), {}, "Enemies", 6});

        m_schemas[GameAssetType::Mission] = schema;
    }

    // Add basic schemas for remaining types
    std::vector<std::pair<GameAssetType, std::string>> basicTypes = {
        {GameAssetType::Skeleton, "Skeleton"},
        {GameAssetType::AnimationSet, "Animation Set"},
        {GameAssetType::ResourceNode, "Resource Node"},
        {GameAssetType::Projectile, "Projectile"},
        {GameAssetType::Behavior, "Behavior"},
        {GameAssetType::TechTree, "Tech Tree"},
        {GameAssetType::Campaign, "Campaign"}
    };

    for (const auto& [type, name] : basicTypes) {
        if (m_schemas.find(type) == m_schemas.end()) {
            AssetTypeSchema schema;
            schema.type = type;
            schema.name = name;
            schema.description = name + " asset definition";
            schema.supportsPreview = false;
            schema.properties.push_back({"name", "Name", "Asset name", "string", true, "", {}, "General", 0});
            m_schemas[type] = schema;
        }
    }
}

// ============================================================================
// AssetEditorPanel Implementation
// ============================================================================

AssetEditorPanel::AssetEditorPanel() = default;

AssetEditorPanel::~AssetEditorPanel() {
    if (m_editor && m_editor->IsOpen()) {
        m_editor->Close(true);
    }
}

bool AssetEditorPanel::OpenAsset(const std::string& assetPath) {
    // Create editor for file type
    m_editor = AssetEditorFactory::Instance().CreateEditorForFile(assetPath);
    if (!m_editor) {
        spdlog::error("No editor available for: {}", assetPath);
        return false;
    }

    // Set up callbacks
    m_editor->OnDirtyChanged = [this](bool dirty) {
        if (dirty) {
            MarkDirty();
        } else {
            ClearDirty();
        }
    };

    m_editor->OnSaved = [this]() {
        ClearDirty();
    };

    // Open the asset
    if (!m_editor->Open(assetPath)) {
        m_editor.reset();
        return false;
    }

    m_currentAssetPath = assetPath;

    // Update panel title
    fs::path path(assetPath);
    SetTitle(path.filename().string());

    return true;
}

bool AssetEditorPanel::HasUnsavedChanges() const {
    return m_editor && m_editor->GetDirty();
}

void AssetEditorPanel::OnUndo() {
    if (m_editor) {
        m_editor->Undo();
    }
}

void AssetEditorPanel::OnRedo() {
    if (m_editor) {
        m_editor->Redo();
    }
}

bool AssetEditorPanel::CanUndo() const {
    return m_editor && m_editor->CanUndo();
}

bool AssetEditorPanel::CanRedo() const {
    return m_editor && m_editor->CanRedo();
}

void AssetEditorPanel::OnRender() {
    if (m_editor) {
        m_editor->Render();
    } else {
        ImGui::TextDisabled("No asset loaded");
    }
}

void AssetEditorPanel::OnRenderToolbar() {
    // Toolbar is rendered by the editor
}

void AssetEditorPanel::OnRenderMenuBar() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save", "Ctrl+S", false, m_editor && m_editor->GetDirty())) {
            if (m_editor) m_editor->Save();
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, m_editor != nullptr)) {
            // Would open save dialog
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close", nullptr, false, m_editor != nullptr)) {
            if (m_editor) m_editor->Close();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
            OnUndo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
            OnRedo();
        }
        ImGui::EndMenu();
    }
}

void AssetEditorPanel::OnRenderStatusBar() {
    // Status bar is rendered by the editor
}

void AssetEditorPanel::OnInitialize() {
    // Panel initialization
}

void AssetEditorPanel::OnShutdown() {
    if (m_editor) {
        m_editor->Close(true);
        m_editor.reset();
    }
}

// ============================================================================
// AssetEditorRegistry Implementation
// ============================================================================

AssetEditorRegistry& AssetEditorRegistry::Instance() {
    static AssetEditorRegistry instance;
    return instance;
}

AssetEditorPanel* AssetEditorRegistry::OpenAsset(const std::string& assetPath) {
    // Check if already open
    auto it = m_editors.find(assetPath);
    if (it != m_editors.end()) {
        it->second->Focus();
        return it->second.get();
    }

    // Create new editor panel
    auto panel = std::make_shared<AssetEditorPanel>();

    EditorPanel::Config config;
    config.title = fs::path(assetPath).filename().string();
    config.flags = EditorPanel::Flags::HasMenuBar | EditorPanel::Flags::HasStatusBar | EditorPanel::Flags::CanUndo;
    config.defaultSize = {800, 600};
    config.category = "Asset Editors";

    if (!panel->Initialize(config)) {
        return nullptr;
    }

    if (!panel->OpenAsset(assetPath)) {
        return nullptr;
    }

    m_editors[assetPath] = panel;

    // Register with panel registry
    PanelRegistry::Instance().Register("AssetEditor_" + assetPath, panel);

    return panel.get();
}

bool AssetEditorRegistry::CloseAsset(const std::string& assetPath, bool force) {
    auto it = m_editors.find(assetPath);
    if (it == m_editors.end()) {
        return true;
    }

    if (!force && it->second->HasUnsavedChanges()) {
        return false;  // Caller should prompt for save
    }

    PanelRegistry::Instance().Unregister("AssetEditor_" + assetPath);
    m_editors.erase(it);
    return true;
}

bool AssetEditorRegistry::IsAssetOpen(const std::string& assetPath) const {
    return m_editors.find(assetPath) != m_editors.end();
}

AssetEditorPanel* AssetEditorRegistry::GetEditor(const std::string& assetPath) {
    auto it = m_editors.find(assetPath);
    return it != m_editors.end() ? it->second.get() : nullptr;
}

std::vector<AssetEditorPanel*> AssetEditorRegistry::GetAllEditors() {
    std::vector<AssetEditorPanel*> result;
    for (auto& [path, panel] : m_editors) {
        result.push_back(panel.get());
    }
    return result;
}

bool AssetEditorRegistry::SaveAll() {
    bool allSaved = true;
    for (auto& [path, panel] : m_editors) {
        if (auto* editor = panel->GetEditor()) {
            if (editor->GetDirty()) {
                if (!editor->Save()) {
                    allSaved = false;
                }
            }
        }
    }
    return allSaved;
}

bool AssetEditorRegistry::CloseAll(bool force) {
    if (!force && HasUnsavedChanges()) {
        return false;
    }

    for (auto& [path, panel] : m_editors) {
        PanelRegistry::Instance().Unregister("AssetEditor_" + path);
    }
    m_editors.clear();
    return true;
}

bool AssetEditorRegistry::HasUnsavedChanges() const {
    for (const auto& [path, panel] : m_editors) {
        if (panel->HasUnsavedChanges()) {
            return true;
        }
    }
    return false;
}

void AssetEditorRegistry::UpdateAll(float deltaTime) {
    for (auto& [path, panel] : m_editors) {
        if (auto* editor = panel->GetEditor()) {
            editor->Update(deltaTime);
        }
    }
}

void AssetEditorRegistry::RenderAll() {
    // Panels are rendered through PanelRegistry
}

} // namespace Nova
