#pragma once

#include "ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>

namespace Vehement {

class Editor;
class EditorCommand;

namespace Content {

/**
 * @brief Result of a content action
 */
struct ActionResult {
    bool success = false;
    std::string message;
    std::string assetId;        // New/affected asset ID
    std::string assetPath;      // New/affected asset path
    std::vector<std::string> affectedAssets;  // Other assets affected by the action

    static ActionResult Success(const std::string& msg = "") {
        return {true, msg, "", "", {}};
    }

    static ActionResult Success(const std::string& assetId, const std::string& path) {
        return {true, "", assetId, path, {}};
    }

    static ActionResult Failure(const std::string& msg) {
        return {false, msg, "", "", {}};
    }
};

/**
 * @brief Options for creating new assets
 */
struct CreateOptions {
    std::string name;
    AssetType type = AssetType::Unknown;
    std::string templateId;     // Template to use (empty for default)
    std::string targetFolder;   // Target directory
    std::string description;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> initialValues;  // Property overrides
    bool openAfterCreate = true;
};

/**
 * @brief Options for duplicating assets
 */
struct DuplicateOptions {
    std::string newName;        // New name (auto-generated if empty)
    std::string targetFolder;   // Target folder (same as source if empty)
    bool duplicateDependencies = false;
    bool updateReferences = true;
    std::string suffix = "_copy";
};

/**
 * @brief Options for renaming assets
 */
struct RenameOptions {
    std::string newName;
    bool updateReferences = true;
    bool renameFile = true;     // Also rename the file on disk
};

/**
 * @brief Options for moving assets
 */
struct MoveOptions {
    std::string targetFolder;
    bool updateReferences = true;
    bool createFolderIfNeeded = true;
};

/**
 * @brief Options for deleting assets
 */
struct DeleteOptions {
    bool checkDependencies = true;
    bool deleteDependents = false;
    bool moveToTrash = true;    // Move to trash instead of permanent delete
    bool force = false;         // Skip confirmation
};

/**
 * @brief Options for exporting asset packs
 */
struct ExportOptions {
    std::string outputPath;
    std::string packName;
    std::string packVersion = "1.0.0";
    std::string author;
    std::string description;
    bool includeDependencies = true;
    bool compressAssets = true;
    std::string compressionLevel = "normal";  // none, fast, normal, best
    std::vector<std::string> excludePatterns;
};

/**
 * @brief Template for creating new assets
 */
struct AssetTemplate {
    std::string id;
    std::string name;
    std::string description;
    AssetType type;
    std::string iconPath;
    std::string templateJson;   // JSON template content
    std::vector<std::string> tags;
    bool isBuiltIn = false;
};

/**
 * @brief Content Actions
 *
 * Provides all content operations with undo/redo support:
 * - Create new asset from template
 * - Duplicate asset
 * - Delete with dependency check
 * - Rename with reference update
 * - Move to folder
 * - Export/Import asset packs
 * - Batch operations
 */
class ContentActions {
public:
    explicit ContentActions(Editor* editor);
    ~ContentActions();

    // Non-copyable
    ContentActions(const ContentActions&) = delete;
    ContentActions& operator=(const ContentActions&) = delete;

    /**
     * @brief Initialize content actions
     */
    bool Initialize();

    /**
     * @brief Shutdown
     */
    void Shutdown();

    // =========================================================================
    // Create Operations
    // =========================================================================

    /**
     * @brief Create new asset
     */
    ActionResult Create(const CreateOptions& options);

    /**
     * @brief Create new asset from template
     */
    ActionResult CreateFromTemplate(const std::string& templateId, const CreateOptions& options);

    /**
     * @brief Create new folder
     */
    ActionResult CreateFolder(const std::string& path, const std::string& name);

    /**
     * @brief Quick create with just type and name
     */
    ActionResult QuickCreate(AssetType type, const std::string& name);

    // =========================================================================
    // Duplicate Operations
    // =========================================================================

    /**
     * @brief Duplicate asset
     */
    ActionResult Duplicate(const std::string& assetId, const DuplicateOptions& options = {});

    /**
     * @brief Duplicate multiple assets
     */
    std::vector<ActionResult> DuplicateBatch(const std::vector<std::string>& assetIds,
                                              const DuplicateOptions& options = {});

    // =========================================================================
    // Delete Operations
    // =========================================================================

    /**
     * @brief Delete asset
     */
    ActionResult Delete(const std::string& assetId, const DeleteOptions& options = {});

    /**
     * @brief Delete multiple assets
     */
    std::vector<ActionResult> DeleteBatch(const std::vector<std::string>& assetIds,
                                           const DeleteOptions& options = {});

    /**
     * @brief Delete folder and contents
     */
    ActionResult DeleteFolder(const std::string& path, const DeleteOptions& options = {});

    /**
     * @brief Check if asset can be safely deleted
     * @return List of dependents that would be affected
     */
    [[nodiscard]] std::vector<std::string> GetDeleteDependents(const std::string& assetId) const;

    /**
     * @brief Restore deleted asset from trash
     */
    ActionResult Restore(const std::string& assetId);

    /**
     * @brief Empty trash
     */
    ActionResult EmptyTrash();

    /**
     * @brief Get assets in trash
     */
    [[nodiscard]] std::vector<AssetMetadata> GetTrashContents() const;

    // =========================================================================
    // Rename Operations
    // =========================================================================

    /**
     * @brief Rename asset
     */
    ActionResult Rename(const std::string& assetId, const RenameOptions& options);

    /**
     * @brief Batch rename with pattern
     * @param pattern Pattern with placeholders: {name}, {index}, {type}
     */
    std::vector<ActionResult> RenameBatch(const std::vector<std::string>& assetIds,
                                           const std::string& pattern);

    /**
     * @brief Validate new name
     */
    [[nodiscard]] bool IsValidName(const std::string& name, std::string* errorMessage = nullptr) const;

    /**
     * @brief Generate unique name
     */
    [[nodiscard]] std::string GenerateUniqueName(const std::string& baseName, AssetType type) const;

    // =========================================================================
    // Move Operations
    // =========================================================================

    /**
     * @brief Move asset to folder
     */
    ActionResult Move(const std::string& assetId, const MoveOptions& options);

    /**
     * @brief Move multiple assets
     */
    std::vector<ActionResult> MoveBatch(const std::vector<std::string>& assetIds,
                                         const MoveOptions& options);

    /**
     * @brief Move folder
     */
    ActionResult MoveFolder(const std::string& sourcePath, const std::string& targetPath);

    // =========================================================================
    // Copy Operations
    // =========================================================================

    /**
     * @brief Copy asset to clipboard
     */
    void CopyToClipboard(const std::string& assetId);

    /**
     * @brief Copy multiple assets to clipboard
     */
    void CopyToClipboard(const std::vector<std::string>& assetIds);

    /**
     * @brief Paste from clipboard
     */
    std::vector<ActionResult> PasteFromClipboard(const std::string& targetFolder = "");

    /**
     * @brief Check if clipboard has content
     */
    [[nodiscard]] bool HasClipboardContent() const;

    /**
     * @brief Get clipboard content preview
     */
    [[nodiscard]] std::vector<std::string> GetClipboardPreview() const;

    // =========================================================================
    // Export/Import Operations
    // =========================================================================

    /**
     * @brief Export assets as pack
     */
    ActionResult ExportPack(const std::vector<std::string>& assetIds, const ExportOptions& options);

    /**
     * @brief Export folder as pack
     */
    ActionResult ExportFolder(const std::string& folderPath, const ExportOptions& options);

    /**
     * @brief Import asset pack
     */
    std::vector<ActionResult> ImportPack(const std::string& packPath, const std::string& targetFolder = "");

    // =========================================================================
    // Template Management
    // =========================================================================

    /**
     * @brief Get all templates
     */
    [[nodiscard]] std::vector<AssetTemplate> GetTemplates() const;

    /**
     * @brief Get templates for type
     */
    [[nodiscard]] std::vector<AssetTemplate> GetTemplatesForType(AssetType type) const;

    /**
     * @brief Get template by ID
     */
    [[nodiscard]] std::optional<AssetTemplate> GetTemplate(const std::string& templateId) const;

    /**
     * @brief Register custom template
     */
    bool RegisterTemplate(const AssetTemplate& tmpl);

    /**
     * @brief Remove custom template
     */
    bool RemoveTemplate(const std::string& templateId);

    /**
     * @brief Create template from existing asset
     */
    ActionResult CreateTemplateFromAsset(const std::string& assetId, const std::string& templateName);

    /**
     * @brief Load templates from directory
     */
    void LoadTemplates(const std::string& directory);

    /**
     * @brief Save custom templates
     */
    void SaveTemplates(const std::string& directory);

    // =========================================================================
    // Reference Update
    // =========================================================================

    /**
     * @brief Update all references to an asset
     */
    int UpdateReferences(const std::string& oldId, const std::string& newId);

    /**
     * @brief Find all references to an asset
     */
    [[nodiscard]] std::vector<std::string> FindReferences(const std::string& assetId) const;

    /**
     * @brief Check for broken references
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>> FindBrokenReferences() const;

    /**
     * @brief Fix broken reference
     */
    bool FixBrokenReference(const std::string& assetId, const std::string& brokenRef,
                            const std::string& newRef);

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate asset
     */
    [[nodiscard]] std::vector<std::string> ValidateAsset(const std::string& assetId) const;

    /**
     * @brief Validate all assets
     */
    [[nodiscard]] std::unordered_map<std::string, std::vector<std::string>> ValidateAll() const;

    /**
     * @brief Fix common validation issues
     */
    ActionResult AutoFix(const std::string& assetId);

    // =========================================================================
    // Undo/Redo
    // =========================================================================

    /**
     * @brief Create undo command for action
     */
    std::unique_ptr<EditorCommand> CreateUndoCommand(const std::string& actionType,
                                                      const std::string& assetId);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnAssetCreated;
    std::function<void(const std::string&)> OnAssetDeleted;
    std::function<void(const std::string&, const std::string&)> OnAssetRenamed;  // oldId, newId
    std::function<void(const std::string&, const std::string&)> OnAssetMoved;    // assetId, newPath
    std::function<void(const std::string&)> OnAssetDuplicated;
    std::function<void(const ActionResult&)> OnActionCompleted;

private:
    // Template initialization
    void InitializeBuiltInTemplates();
    std::string GetDefaultTemplate(AssetType type) const;
    std::string ApplyTemplateValues(const std::string& templateJson,
                                     const std::unordered_map<std::string, std::string>& values) const;

    // File operations
    bool WriteAssetFile(const std::string& path, const std::string& content);
    bool DeleteAssetFile(const std::string& path);
    bool MoveAssetFile(const std::string& sourcePath, const std::string& targetPath);
    bool CopyAssetFile(const std::string& sourcePath, const std::string& targetPath);

    // Reference tracking
    void UpdateAssetReferences(const std::string& filePath, const std::string& oldId,
                               const std::string& newId);
    std::vector<std::string> ExtractReferences(const std::string& content) const;

    // Name generation
    std::string SanitizeName(const std::string& name) const;
    std::string GenerateCopyName(const std::string& originalName) const;

    // Trash management
    std::string GetTrashPath() const;
    std::string MoveToTrash(const std::string& assetPath);
    bool RestoreFromTrash(const std::string& trashPath, const std::string& originalPath);

    Editor* m_editor = nullptr;
    bool m_initialized = false;

    // Templates
    std::vector<AssetTemplate> m_templates;
    std::unordered_map<std::string, size_t> m_templateIndex;

    // Clipboard
    std::vector<std::string> m_clipboard;
    bool m_clipboardIsCut = false;

    // Trash
    std::unordered_map<std::string, std::string> m_trashMapping;  // trashPath -> originalPath
};

// =========================================================================
// Editor Commands for Undo/Redo
// =========================================================================

/**
 * @brief Create asset command
 */
class CreateAssetCommand : public EditorCommand {
public:
    CreateAssetCommand(ContentActions* actions, const CreateOptions& options);

    void Execute() override;
    void Undo() override;
    [[nodiscard]] std::string GetDescription() const override;

private:
    ContentActions* m_actions;
    CreateOptions m_options;
    std::string m_createdId;
    std::string m_createdPath;
};

/**
 * @brief Delete asset command
 */
class DeleteAssetCommand : public EditorCommand {
public:
    DeleteAssetCommand(ContentActions* actions, const std::string& assetId,
                       const DeleteOptions& options);

    void Execute() override;
    void Undo() override;
    [[nodiscard]] std::string GetDescription() const override;

private:
    ContentActions* m_actions;
    std::string m_assetId;
    DeleteOptions m_options;
    std::string m_backupContent;
    std::string m_originalPath;
};

/**
 * @brief Rename asset command
 */
class RenameAssetCommand : public EditorCommand {
public:
    RenameAssetCommand(ContentActions* actions, const std::string& assetId,
                       const RenameOptions& options);

    void Execute() override;
    void Undo() override;
    [[nodiscard]] std::string GetDescription() const override;

private:
    ContentActions* m_actions;
    std::string m_assetId;
    std::string m_oldName;
    RenameOptions m_options;
};

/**
 * @brief Move asset command
 */
class MoveAssetCommand : public EditorCommand {
public:
    MoveAssetCommand(ContentActions* actions, const std::string& assetId,
                     const MoveOptions& options);

    void Execute() override;
    void Undo() override;
    [[nodiscard]] std::string GetDescription() const override;

private:
    ContentActions* m_actions;
    std::string m_assetId;
    std::string m_oldPath;
    MoveOptions m_options;
};

} // namespace Content
} // namespace Vehement
