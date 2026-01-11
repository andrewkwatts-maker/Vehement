/**
 * @file GenericAssetEditor.hpp
 * @brief Generic asset editor system for JSON-based game assets
 *
 * Provides a unified editing interface for the 13 JSON-based asset types:
 * - SDFModel, Skeleton, Animation, AnimationSet
 * - Entity, Hero, ResourceNode, Projectile
 * - Behavior, TechTree, Upgrade, Campaign, Mission
 *
 * Features:
 * - Template-based asset editor with type-specific validation
 * - JSON tree view with expand/collapse
 * - Property inspector with type-appropriate widgets
 * - Live preview where applicable
 * - Undo/redo integration
 */

#pragma once

#include "../ui/EditorPanel.hpp"
#include "../ui/EditorWidgets.hpp"
#include "../ui/EditorTheme.hpp"
#include "../assets/JsonAssetSerializer.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stack>
#include <unordered_map>
#include <variant>

namespace Nova {

// Forward declarations
class AssetThumbnailCache;

// ============================================================================
// Asset Type Definitions
// ============================================================================

/**
 * @brief Extended asset types for game-specific JSON assets
 */
enum class GameAssetType {
    Unknown,
    SDFModel,
    Skeleton,
    Animation,
    AnimationSet,
    Entity,
    Hero,
    ResourceNode,
    Projectile,
    Behavior,
    TechTree,
    Upgrade,
    Campaign,
    Mission
};

/**
 * @brief Convert GameAssetType to string
 */
std::string GameAssetTypeToString(GameAssetType type);

/**
 * @brief Convert string to GameAssetType
 */
GameAssetType StringToGameAssetType(const std::string& str);

/**
 * @brief Get display name for asset type
 */
std::string GetAssetTypeDisplayName(GameAssetType type);

/**
 * @brief Get icon for asset type (FontAwesome or similar)
 */
const char* GetAssetTypeIcon(GameAssetType type);

// ============================================================================
// IAssetEditor Interface
// ============================================================================

/**
 * @brief Base interface for all asset editors
 */
class IAssetEditor {
public:
    virtual ~IAssetEditor() = default;

    /**
     * @brief Open an asset for editing
     * @param assetPath Path to the asset file
     * @return true if asset was opened successfully
     */
    virtual bool Open(const std::string& assetPath) = 0;

    /**
     * @brief Save the current asset
     * @return true if save was successful
     */
    virtual bool Save() = 0;

    /**
     * @brief Save the asset to a new path
     * @param newPath New file path
     * @return true if save was successful
     */
    virtual bool SaveAs(const std::string& newPath) = 0;

    /**
     * @brief Close the current asset
     * @param force If true, discard unsaved changes
     * @return true if closed successfully
     */
    virtual bool Close(bool force = false) = 0;

    /**
     * @brief Check if the asset has unsaved changes
     */
    virtual bool GetDirty() const = 0;

    /**
     * @brief Render the editor UI
     */
    virtual void Render() = 0;

    /**
     * @brief Update the editor state
     * @param deltaTime Time since last update
     */
    virtual void Update(float deltaTime) = 0;

    /**
     * @brief Get the current asset path
     */
    virtual const std::string& GetAssetPath() const = 0;

    /**
     * @brief Get the asset type
     */
    virtual GameAssetType GetAssetType() const = 0;

    /**
     * @brief Check if an asset is currently open
     */
    virtual bool IsOpen() const = 0;

    /**
     * @brief Undo the last action
     */
    virtual void Undo() = 0;

    /**
     * @brief Redo the last undone action
     */
    virtual void Redo() = 0;

    /**
     * @brief Check if undo is available
     */
    virtual bool CanUndo() const = 0;

    /**
     * @brief Check if redo is available
     */
    virtual bool CanRedo() const = 0;

    // Callbacks
    std::function<void()> OnSaved;
    std::function<void()> OnClosed;
    std::function<void(bool)> OnDirtyChanged;
};

// ============================================================================
// JSON Tree Node
// ============================================================================

/**
 * @brief Represents a node in the JSON tree view
 */
struct JsonTreeNode {
    std::string key;
    nlohmann::json* value = nullptr;
    std::vector<JsonTreeNode> children;
    bool expanded = false;
    bool selected = false;
    std::string path;  // JSON pointer path
    int depth = 0;

    // Cached display info
    std::string displayValue;
    std::string typeString;
    bool isArray = false;
    bool isObject = false;
    size_t childCount = 0;
};

// ============================================================================
// Property Schema
// ============================================================================

/**
 * @brief Schema for property validation and display
 */
struct PropertySchema {
    std::string name;
    std::string displayName;
    std::string description;
    std::string type;  // "string", "number", "integer", "boolean", "array", "object", "color", "asset", "enum"
    bool required = false;
    nlohmann::json defaultValue;
    nlohmann::json constraints;  // min, max, step, options, etc.
    std::string category;
    int order = 0;
    bool readOnly = false;
    std::string assetFilter;  // For asset references
    std::vector<std::string> enumOptions;  // For enum types
};

/**
 * @brief Schema for entire asset type
 */
struct AssetTypeSchema {
    GameAssetType type;
    std::string name;
    std::string description;
    std::vector<PropertySchema> properties;
    nlohmann::json jsonSchema;  // Full JSON Schema for validation
    bool supportsPreview = false;
};

// ============================================================================
// Undo/Redo System
// ============================================================================

/**
 * @brief Represents a single undoable action
 */
struct UndoAction {
    std::string description;
    std::string jsonPath;  // JSON pointer path
    nlohmann::json oldValue;
    nlohmann::json newValue;
    uint64_t timestamp;
};

/**
 * @brief Manages undo/redo history for JSON editing
 */
class JsonUndoManager {
public:
    JsonUndoManager(size_t maxHistory = 100);

    /**
     * @brief Record an action for undo
     */
    void RecordAction(const std::string& description, const std::string& jsonPath,
                      const nlohmann::json& oldValue, const nlohmann::json& newValue);

    /**
     * @brief Undo the last action
     * @param root The root JSON object to modify
     * @return true if undo was performed
     */
    bool Undo(nlohmann::json& root);

    /**
     * @brief Redo the last undone action
     * @param root The root JSON object to modify
     * @return true if redo was performed
     */
    bool Redo(nlohmann::json& root);

    /**
     * @brief Check if undo is available
     */
    bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    bool CanRedo() const { return !m_redoStack.empty(); }

    /**
     * @brief Get description of next undo action
     */
    std::string GetUndoDescription() const;

    /**
     * @brief Get description of next redo action
     */
    std::string GetRedoDescription() const;

    /**
     * @brief Clear all history
     */
    void Clear();

    /**
     * @brief Get undo stack size
     */
    size_t GetUndoCount() const { return m_undoStack.size(); }

    /**
     * @brief Get redo stack size
     */
    size_t GetRedoCount() const { return m_redoStack.size(); }

private:
    std::vector<UndoAction> m_undoStack;
    std::vector<UndoAction> m_redoStack;
    size_t m_maxHistory;

    void ApplyAction(nlohmann::json& root, const std::string& path, const nlohmann::json& value);
};

// ============================================================================
// GenericJSONAssetEditor
// ============================================================================

/**
 * @brief Generic editor for JSON-based game assets
 *
 * Provides:
 * - JSON tree view with expand/collapse
 * - Property inspector with type-appropriate widgets
 * - Type-specific validation
 * - Undo/redo support
 * - Live preview integration
 */
class GenericJSONAssetEditor : public IAssetEditor {
public:
    GenericJSONAssetEditor();
    ~GenericJSONAssetEditor() override;

    // IAssetEditor implementation
    bool Open(const std::string& assetPath) override;
    bool Save() override;
    bool SaveAs(const std::string& newPath) override;
    bool Close(bool force = false) override;
    bool GetDirty() const override { return m_dirty; }
    void Render() override;
    void Update(float deltaTime) override;
    const std::string& GetAssetPath() const override { return m_assetPath; }
    GameAssetType GetAssetType() const override { return m_assetType; }
    bool IsOpen() const override { return m_isOpen; }
    void Undo() override;
    void Redo() override;
    bool CanUndo() const override { return m_undoManager.CanUndo(); }
    bool CanRedo() const override { return m_undoManager.CanRedo(); }

    // Configuration
    void SetSchema(const AssetTypeSchema& schema) { m_schema = schema; }
    const AssetTypeSchema& GetSchema() const { return m_schema; }

    /**
     * @brief Set preview callback for live preview
     */
    void SetPreviewCallback(std::function<void(const nlohmann::json&)> callback) {
        m_previewCallback = std::move(callback);
    }

    /**
     * @brief Register validation callback
     */
    void SetValidationCallback(std::function<ValidationResult(const nlohmann::json&)> callback) {
        m_validationCallback = std::move(callback);
    }

    /**
     * @brief Get the current JSON data
     */
    const nlohmann::json& GetJson() const { return m_json; }

    /**
     * @brief Get mutable JSON data (marks dirty)
     */
    nlohmann::json& GetMutableJson();

    /**
     * @brief Get selected JSON path
     */
    const std::string& GetSelectedPath() const { return m_selectedPath; }

    /**
     * @brief Set selected JSON path
     */
    void SetSelectedPath(const std::string& path);

protected:
    // UI Rendering
    void RenderToolbar();
    void RenderTreeView();
    void RenderPropertyInspector();
    void RenderPreview();
    void RenderStatusBar();
    void RenderContextMenu();

    // Tree view helpers
    void BuildTreeFromJson();
    void RenderTreeNode(JsonTreeNode& node);
    void SelectNode(JsonTreeNode& node);
    void ExpandAll(JsonTreeNode& node);
    void CollapseAll(JsonTreeNode& node);

    // Property editing
    bool RenderPropertyEditor(const std::string& key, nlohmann::json& value, const PropertySchema* schema = nullptr);
    bool RenderStringProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderNumberProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderBoolProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderArrayProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderObjectProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderColorProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderEnumProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderAssetProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);
    bool RenderVectorProperty(const std::string& key, nlohmann::json& value, const PropertySchema* schema);

    // JSON operations
    void AddProperty(const std::string& parentPath, const std::string& key, const nlohmann::json& value);
    void RemoveProperty(const std::string& path);
    void RenameProperty(const std::string& path, const std::string& newKey);
    void DuplicateProperty(const std::string& path);
    void MoveProperty(const std::string& fromPath, const std::string& toPath);

    // Validation
    void Validate();
    void ShowValidationErrors();

    // Internal helpers
    nlohmann::json* GetJsonAtPath(const std::string& path);
    const PropertySchema* GetSchemaForPath(const std::string& path) const;
    std::string GetParentPath(const std::string& path) const;
    std::string GetKeyFromPath(const std::string& path) const;
    void MarkDirty();
    void ClearDirty();

private:
    // State
    bool m_isOpen = false;
    bool m_dirty = false;
    std::string m_assetPath;
    GameAssetType m_assetType = GameAssetType::Unknown;
    nlohmann::json m_json;
    nlohmann::json m_originalJson;  // For dirty detection

    // Schema
    AssetTypeSchema m_schema;

    // Tree view state
    JsonTreeNode m_rootNode;
    std::string m_selectedPath;
    JsonTreeNode* m_selectedNode = nullptr;
    bool m_treeNeedsRebuild = true;
    char m_searchBuffer[256] = "";
    std::string m_searchFilter;

    // Undo/Redo
    JsonUndoManager m_undoManager;

    // Validation
    ValidationResult m_validationResult;
    bool m_showValidationErrors = false;

    // Callbacks
    std::function<void(const nlohmann::json&)> m_previewCallback;
    std::function<ValidationResult(const nlohmann::json&)> m_validationCallback;

    // UI state
    float m_treeWidth = 300.0f;
    float m_inspectorWidth = 350.0f;
    bool m_showPreview = true;
    bool m_autoValidate = true;
    float m_previewUpdateTimer = 0.0f;
    static constexpr float PREVIEW_UPDATE_DELAY = 0.3f;

    // Context menu state
    bool m_showContextMenu = false;
    std::string m_contextMenuPath;
};

// ============================================================================
// AssetEditorFactory
// ============================================================================

/**
 * @brief Factory for creating appropriate asset editors
 */
class AssetEditorFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static AssetEditorFactory& Instance();

    /**
     * @brief Create an editor for the specified asset type
     * @param type The asset type
     * @return Unique pointer to the created editor
     */
    std::unique_ptr<IAssetEditor> CreateEditor(GameAssetType type);

    /**
     * @brief Create an editor for the specified file
     * @param filePath Path to the asset file
     * @return Unique pointer to the created editor
     */
    std::unique_ptr<IAssetEditor> CreateEditorForFile(const std::string& filePath);

    /**
     * @brief Register a custom editor factory
     * @param type The asset type
     * @param factory Factory function
     */
    void RegisterEditorFactory(GameAssetType type, std::function<std::unique_ptr<IAssetEditor>()> factory);

    /**
     * @brief Register asset type schema
     * @param schema The schema to register
     */
    void RegisterSchema(const AssetTypeSchema& schema);

    /**
     * @brief Get schema for asset type
     * @param type The asset type
     * @return Pointer to schema or nullptr
     */
    const AssetTypeSchema* GetSchema(GameAssetType type) const;

    /**
     * @brief Detect asset type from file
     * @param filePath Path to the asset file
     * @return Detected asset type
     */
    GameAssetType DetectAssetType(const std::string& filePath);

    /**
     * @brief Check if an editor can be created for the type
     * @param type The asset type
     * @return true if editor is available
     */
    bool CanEdit(GameAssetType type) const;

    /**
     * @brief Get list of editable asset types
     */
    std::vector<GameAssetType> GetEditableTypes() const;

    /**
     * @brief Initialize default schemas for all game asset types
     */
    void InitializeDefaultSchemas();

private:
    AssetEditorFactory();

    std::unordered_map<GameAssetType, std::function<std::unique_ptr<IAssetEditor>()>> m_factories;
    std::unordered_map<GameAssetType, AssetTypeSchema> m_schemas;
};

// ============================================================================
// AssetEditorPanel
// ============================================================================

/**
 * @brief Editor panel wrapper for IAssetEditor
 *
 * Integrates with the panel system for docking support
 */
class AssetEditorPanel : public EditorPanel {
public:
    AssetEditorPanel();
    ~AssetEditorPanel() override;

    /**
     * @brief Open an asset in this panel
     */
    bool OpenAsset(const std::string& assetPath);

    /**
     * @brief Get the underlying editor
     */
    IAssetEditor* GetEditor() { return m_editor.get(); }
    const IAssetEditor* GetEditor() const { return m_editor.get(); }

    /**
     * @brief Check if panel has unsaved changes
     */
    bool HasUnsavedChanges() const;

    // EditorPanel overrides
    void OnUndo() override;
    void OnRedo() override;
    bool CanUndo() const override;
    bool CanRedo() const override;

protected:
    void OnRender() override;
    void OnRenderToolbar() override;
    void OnRenderMenuBar() override;
    void OnRenderStatusBar() override;
    void OnInitialize() override;
    void OnShutdown() override;

private:
    std::unique_ptr<IAssetEditor> m_editor;
    std::string m_currentAssetPath;
};

// ============================================================================
// Asset Editor Registry
// ============================================================================

/**
 * @brief Manages open asset editors
 */
class AssetEditorRegistry {
public:
    static AssetEditorRegistry& Instance();

    /**
     * @brief Open an asset for editing
     * @param assetPath Path to the asset
     * @return Pointer to the editor panel
     */
    AssetEditorPanel* OpenAsset(const std::string& assetPath);

    /**
     * @brief Close an asset editor
     * @param assetPath Path to the asset
     * @param force If true, discard unsaved changes
     * @return true if closed successfully
     */
    bool CloseAsset(const std::string& assetPath, bool force = false);

    /**
     * @brief Check if asset is open
     */
    bool IsAssetOpen(const std::string& assetPath) const;

    /**
     * @brief Get editor for asset
     */
    AssetEditorPanel* GetEditor(const std::string& assetPath);

    /**
     * @brief Get all open editors
     */
    std::vector<AssetEditorPanel*> GetAllEditors();

    /**
     * @brief Save all open assets
     */
    bool SaveAll();

    /**
     * @brief Close all editors
     * @param force If true, discard unsaved changes
     * @return true if all closed successfully
     */
    bool CloseAll(bool force = false);

    /**
     * @brief Check if any editors have unsaved changes
     */
    bool HasUnsavedChanges() const;

    /**
     * @brief Update all open editors
     */
    void UpdateAll(float deltaTime);

    /**
     * @brief Render all open editors
     */
    void RenderAll();

private:
    AssetEditorRegistry() = default;

    std::unordered_map<std::string, std::shared_ptr<AssetEditorPanel>> m_editors;
};

} // namespace Nova
