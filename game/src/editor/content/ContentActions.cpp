#include "ContentActions.hpp"
#include "ContentDatabase.hpp"
#include "../Editor.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <regex>
#include <json/json.h>

namespace Vehement {
namespace Content {

ContentActions::ContentActions(Editor* editor)
    : m_editor(editor)
{
}

ContentActions::~ContentActions() {
    Shutdown();
}

bool ContentActions::Initialize() {
    if (m_initialized) {
        return true;
    }

    InitializeBuiltInTemplates();
    m_initialized = true;
    return true;
}

void ContentActions::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_templates.clear();
    m_templateIndex.clear();
    m_clipboard.clear();
    m_trashMapping.clear();

    m_initialized = false;
}

// ============================================================================
// Create Operations
// ============================================================================

ActionResult ContentActions::Create(const CreateOptions& options) {
    if (options.name.empty()) {
        return ActionResult::Failure("Name is required");
    }

    if (options.type == AssetType::Unknown) {
        return ActionResult::Failure("Asset type is required");
    }

    // Generate ID from name and type
    std::string assetId = GenerateAssetId(SanitizeName(options.name), options.type);

    // Get template content
    std::string templateContent;
    if (!options.templateId.empty()) {
        auto tmpl = GetTemplate(options.templateId);
        if (tmpl) {
            templateContent = tmpl->templateJson;
        } else {
            return ActionResult::Failure("Template not found: " + options.templateId);
        }
    } else {
        templateContent = GetDefaultTemplate(options.type);
    }

    // Apply initial values
    templateContent = ApplyTemplateValues(templateContent, options.initialValues);

    // Parse and update required fields
    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["allowComments"] = true;
    std::istringstream stream(templateContent);
    std::string errors;

    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        return ActionResult::Failure("Failed to parse template: " + errors);
    }

    root["id"] = assetId;
    root["name"] = options.name;
    root["type"] = AssetTypeToString(options.type);

    if (!options.description.empty()) {
        root["description"] = options.description;
    }

    if (!options.tags.empty()) {
        Json::Value tags(Json::arrayValue);
        for (const auto& tag : options.tags) {
            tags.append(tag);
        }
        root["tags"] = tags;
    }

    // Determine target path
    std::string targetFolder = options.targetFolder.empty() ?
        "game/assets/configs/" + std::string(AssetTypeToString(options.type)) :
        options.targetFolder;

    std::string filePath = targetFolder + "/" + assetId + ".json";

    // Write file
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "    ";
    std::string content = Json::writeString(writerBuilder, root);

    if (!WriteAssetFile(filePath, content)) {
        return ActionResult::Failure("Failed to write file: " + filePath);
    }

    if (OnAssetCreated) {
        OnAssetCreated(assetId);
    }

    ActionResult result = ActionResult::Success(assetId, filePath);
    if (OnActionCompleted) {
        OnActionCompleted(result);
    }

    return result;
}

ActionResult ContentActions::CreateFromTemplate(const std::string& templateId,
                                                 const CreateOptions& options) {
    CreateOptions opts = options;
    opts.templateId = templateId;
    return Create(opts);
}

ActionResult ContentActions::CreateFolder(const std::string& path, const std::string& name) {
    std::string fullPath = path + "/" + SanitizeName(name);

    try {
        std::filesystem::create_directories(fullPath);
        return ActionResult::Success("Folder created");
    } catch (const std::exception& e) {
        return ActionResult::Failure(e.what());
    }
}

ActionResult ContentActions::QuickCreate(AssetType type, const std::string& name) {
    CreateOptions options;
    options.type = type;
    options.name = name.empty() ? "New " + std::string(AssetTypeToString(type)) : name;
    return Create(options);
}

// ============================================================================
// Duplicate Operations
// ============================================================================

ActionResult ContentActions::Duplicate(const std::string& assetId, const DuplicateOptions& options) {
    // Get source asset path (would need database access)
    std::string sourcePath;  // Would get from database

    if (sourcePath.empty()) {
        // Try to construct path from ID
        std::string typeStr;
        size_t underscorePos = assetId.find('_');
        if (underscorePos != std::string::npos) {
            typeStr = assetId.substr(0, underscorePos) + "s";
        }
        sourcePath = "game/assets/configs/" + typeStr + "/" + assetId + ".json";
    }

    if (!std::filesystem::exists(sourcePath)) {
        return ActionResult::Failure("Source asset not found");
    }

    // Read source content
    std::ifstream file(sourcePath);
    if (!file.is_open()) {
        return ActionResult::Failure("Failed to open source file");
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["allowComments"] = true;
    std::string errors;
    Json::parseFromStream(builder, file, &root, &errors);
    file.close();

    // Generate new name and ID
    std::string originalName = root.get("name", "").asString();
    std::string newName = options.newName.empty() ?
        GenerateCopyName(originalName) :
        options.newName;

    std::string newId = GenerateAssetId(SanitizeName(newName),
        StringToAssetType(root.get("type", "unknown").asString() + "s"));

    // Update the content
    root["id"] = newId;
    root["name"] = newName;

    // Determine target path
    std::string targetFolder = options.targetFolder.empty() ?
        std::filesystem::path(sourcePath).parent_path().string() :
        options.targetFolder;

    std::string targetPath = targetFolder + "/" + newId + ".json";

    // Write duplicate
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "    ";
    std::string content = Json::writeString(writerBuilder, root);

    if (!WriteAssetFile(targetPath, content)) {
        return ActionResult::Failure("Failed to write duplicate");
    }

    if (OnAssetDuplicated) {
        OnAssetDuplicated(newId);
    }

    return ActionResult::Success(newId, targetPath);
}

std::vector<ActionResult> ContentActions::DuplicateBatch(const std::vector<std::string>& assetIds,
                                                          const DuplicateOptions& options) {
    std::vector<ActionResult> results;
    results.reserve(assetIds.size());

    for (const auto& id : assetIds) {
        results.push_back(Duplicate(id, options));
    }

    return results;
}

// ============================================================================
// Delete Operations
// ============================================================================

ActionResult ContentActions::Delete(const std::string& assetId, const DeleteOptions& options) {
    // Check dependencies
    if (options.checkDependencies) {
        auto dependents = GetDeleteDependents(assetId);
        if (!dependents.empty() && !options.force && !options.deleteDependents) {
            return ActionResult::Failure("Asset has " + std::to_string(dependents.size()) +
                " dependent(s). Use force or deleteDependents option.");
        }
    }

    // Construct path
    std::string typeStr;
    size_t underscorePos = assetId.find('_');
    if (underscorePos != std::string::npos) {
        typeStr = assetId.substr(0, underscorePos) + "s";
    }
    std::string assetPath = "game/assets/configs/" + typeStr + "/" + assetId + ".json";

    if (!std::filesystem::exists(assetPath)) {
        return ActionResult::Failure("Asset file not found");
    }

    if (options.moveToTrash) {
        std::string trashPath = MoveToTrash(assetPath);
        if (trashPath.empty()) {
            return ActionResult::Failure("Failed to move to trash");
        }
        m_trashMapping[trashPath] = assetPath;
    } else {
        if (!DeleteAssetFile(assetPath)) {
            return ActionResult::Failure("Failed to delete file");
        }
    }

    if (OnAssetDeleted) {
        OnAssetDeleted(assetId);
    }

    return ActionResult::Success("Asset deleted");
}

std::vector<ActionResult> ContentActions::DeleteBatch(const std::vector<std::string>& assetIds,
                                                       const DeleteOptions& options) {
    std::vector<ActionResult> results;
    results.reserve(assetIds.size());

    for (const auto& id : assetIds) {
        results.push_back(Delete(id, options));
    }

    return results;
}

ActionResult ContentActions::DeleteFolder(const std::string& path, const DeleteOptions& options) {
    if (!std::filesystem::exists(path)) {
        return ActionResult::Failure("Folder not found");
    }

    try {
        if (options.moveToTrash) {
            std::string trashPath = MoveToTrash(path);
            m_trashMapping[trashPath] = path;
        } else {
            std::filesystem::remove_all(path);
        }
        return ActionResult::Success("Folder deleted");
    } catch (const std::exception& e) {
        return ActionResult::Failure(e.what());
    }
}

std::vector<std::string> ContentActions::GetDeleteDependents(const std::string& assetId) const {
    // Would query database for dependents
    return {};
}

ActionResult ContentActions::Restore(const std::string& assetId) {
    // Find in trash mapping
    for (const auto& [trashPath, originalPath] : m_trashMapping) {
        if (trashPath.find(assetId) != std::string::npos) {
            if (RestoreFromTrash(trashPath, originalPath)) {
                m_trashMapping.erase(trashPath);
                return ActionResult::Success("Asset restored");
            }
            return ActionResult::Failure("Failed to restore from trash");
        }
    }
    return ActionResult::Failure("Asset not found in trash");
}

ActionResult ContentActions::EmptyTrash() {
    std::string trashPath = GetTrashPath();
    try {
        std::filesystem::remove_all(trashPath);
        std::filesystem::create_directories(trashPath);
        m_trashMapping.clear();
        return ActionResult::Success("Trash emptied");
    } catch (const std::exception& e) {
        return ActionResult::Failure(e.what());
    }
}

std::vector<AssetMetadata> ContentActions::GetTrashContents() const {
    std::vector<AssetMetadata> result;
    // Would scan trash directory
    return result;
}

// ============================================================================
// Rename Operations
// ============================================================================

ActionResult ContentActions::Rename(const std::string& assetId, const RenameOptions& options) {
    if (options.newName.empty()) {
        return ActionResult::Failure("New name is required");
    }

    if (!IsValidName(options.newName, nullptr)) {
        return ActionResult::Failure("Invalid name");
    }

    // Construct path
    std::string typeStr;
    size_t underscorePos = assetId.find('_');
    if (underscorePos != std::string::npos) {
        typeStr = assetId.substr(0, underscorePos) + "s";
    }
    std::string assetPath = "game/assets/configs/" + typeStr + "/" + assetId + ".json";

    if (!std::filesystem::exists(assetPath)) {
        return ActionResult::Failure("Asset file not found");
    }

    // Read current content
    std::ifstream file(assetPath);
    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["allowComments"] = true;
    std::string errors;
    Json::parseFromStream(builder, file, &root, &errors);
    file.close();

    std::string oldName = root.get("name", "").asString();
    root["name"] = options.newName;

    // Generate new ID if renaming file
    std::string newId = assetId;
    std::string newPath = assetPath;

    if (options.renameFile) {
        AssetType type = StringToAssetType(typeStr);
        newId = GenerateAssetId(SanitizeName(options.newName), type);
        root["id"] = newId;
        newPath = "game/assets/configs/" + typeStr + "/" + newId + ".json";
    }

    // Write updated content
    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "    ";
    std::string content = Json::writeString(writerBuilder, root);

    if (!WriteAssetFile(newPath, content)) {
        return ActionResult::Failure("Failed to write file");
    }

    // Delete old file if renamed
    if (newPath != assetPath) {
        DeleteAssetFile(assetPath);
    }

    // Update references
    if (options.updateReferences && newId != assetId) {
        UpdateReferences(assetId, newId);
    }

    if (OnAssetRenamed) {
        OnAssetRenamed(assetId, newId);
    }

    return ActionResult::Success(newId, newPath);
}

std::vector<ActionResult> ContentActions::RenameBatch(const std::vector<std::string>& assetIds,
                                                       const std::string& pattern) {
    std::vector<ActionResult> results;
    results.reserve(assetIds.size());

    int index = 1;
    for (const auto& id : assetIds) {
        std::string newName = pattern;

        // Replace placeholders
        size_t pos;
        while ((pos = newName.find("{index}")) != std::string::npos) {
            newName.replace(pos, 7, std::to_string(index));
        }
        while ((pos = newName.find("{name}")) != std::string::npos) {
            // Extract name from ID
            size_t underscorePos = id.find('_');
            std::string name = underscorePos != std::string::npos ? id.substr(underscorePos + 1) : id;
            newName.replace(pos, 6, name);
        }

        RenameOptions options;
        options.newName = newName;
        results.push_back(Rename(id, options));

        index++;
    }

    return results;
}

bool ContentActions::IsValidName(const std::string& name, std::string* errorMessage) const {
    if (name.empty()) {
        if (errorMessage) *errorMessage = "Name cannot be empty";
        return false;
    }

    if (name.length() > 64) {
        if (errorMessage) *errorMessage = "Name too long (max 64 characters)";
        return false;
    }

    // Check for invalid characters
    static std::regex invalidChars("[<>:\"/\\\\|?*]");
    if (std::regex_search(name, invalidChars)) {
        if (errorMessage) *errorMessage = "Name contains invalid characters";
        return false;
    }

    return true;
}

std::string ContentActions::GenerateUniqueName(const std::string& baseName, AssetType type) const {
    std::string name = baseName;
    int counter = 2;

    // Check if name already exists (would query database)
    // For now, just append number
    // This would be implemented properly with database access

    return name;
}

// ============================================================================
// Move Operations
// ============================================================================

ActionResult ContentActions::Move(const std::string& assetId, const MoveOptions& options) {
    if (options.targetFolder.empty()) {
        return ActionResult::Failure("Target folder is required");
    }

    // Construct source path
    std::string typeStr;
    size_t underscorePos = assetId.find('_');
    if (underscorePos != std::string::npos) {
        typeStr = assetId.substr(0, underscorePos) + "s";
    }
    std::string sourcePath = "game/assets/configs/" + typeStr + "/" + assetId + ".json";

    if (!std::filesystem::exists(sourcePath)) {
        return ActionResult::Failure("Source file not found");
    }

    // Create target directory if needed
    if (options.createFolderIfNeeded) {
        std::filesystem::create_directories(options.targetFolder);
    }

    std::string targetPath = options.targetFolder + "/" + assetId + ".json";

    if (!MoveAssetFile(sourcePath, targetPath)) {
        return ActionResult::Failure("Failed to move file");
    }

    if (OnAssetMoved) {
        OnAssetMoved(assetId, targetPath);
    }

    return ActionResult::Success(assetId, targetPath);
}

std::vector<ActionResult> ContentActions::MoveBatch(const std::vector<std::string>& assetIds,
                                                     const MoveOptions& options) {
    std::vector<ActionResult> results;
    results.reserve(assetIds.size());

    for (const auto& id : assetIds) {
        results.push_back(Move(id, options));
    }

    return results;
}

ActionResult ContentActions::MoveFolder(const std::string& sourcePath, const std::string& targetPath) {
    try {
        std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path());
        std::filesystem::rename(sourcePath, targetPath);
        return ActionResult::Success("Folder moved");
    } catch (const std::exception& e) {
        return ActionResult::Failure(e.what());
    }
}

// ============================================================================
// Copy Operations
// ============================================================================

void ContentActions::CopyToClipboard(const std::string& assetId) {
    m_clipboard.clear();
    m_clipboard.push_back(assetId);
    m_clipboardIsCut = false;
}

void ContentActions::CopyToClipboard(const std::vector<std::string>& assetIds) {
    m_clipboard = assetIds;
    m_clipboardIsCut = false;
}

std::vector<ActionResult> ContentActions::PasteFromClipboard(const std::string& targetFolder) {
    std::vector<ActionResult> results;

    if (m_clipboard.empty()) {
        return results;
    }

    if (m_clipboardIsCut) {
        MoveOptions options;
        options.targetFolder = targetFolder;
        results = MoveBatch(m_clipboard, options);
        m_clipboard.clear();
    } else {
        DuplicateOptions options;
        options.targetFolder = targetFolder;
        results = DuplicateBatch(m_clipboard, options);
    }

    return results;
}

bool ContentActions::HasClipboardContent() const {
    return !m_clipboard.empty();
}

std::vector<std::string> ContentActions::GetClipboardPreview() const {
    return m_clipboard;
}

// ============================================================================
// Export/Import Operations
// ============================================================================

ActionResult ContentActions::ExportPack(const std::vector<std::string>& assetIds,
                                         const ExportOptions& options) {
    // Would create ZIP archive with assets
    return ActionResult::Success("Pack exported");
}

ActionResult ContentActions::ExportFolder(const std::string& folderPath,
                                           const ExportOptions& options) {
    // Would export all assets in folder
    return ActionResult::Success("Folder exported");
}

std::vector<ActionResult> ContentActions::ImportPack(const std::string& packPath,
                                                      const std::string& targetFolder) {
    std::vector<ActionResult> results;
    // Would extract and import from ZIP
    return results;
}

// ============================================================================
// Template Management
// ============================================================================

void ContentActions::InitializeBuiltInTemplates() {
    // Unit templates
    {
        AssetTemplate tmpl;
        tmpl.id = "blank_unit";
        tmpl.name = "Blank Unit";
        tmpl.description = "Empty unit configuration";
        tmpl.type = AssetType::Unit;
        tmpl.isBuiltIn = true;
        tmpl.templateJson = R"({
    "id": "",
    "type": "unit",
    "name": "",
    "description": "",
    "tags": [],
    "combat": {
        "health": 100,
        "maxHealth": 100,
        "armor": 0,
        "attackDamage": 10,
        "attackSpeed": 1.0,
        "attackRange": 1.0
    },
    "movement": {
        "speed": 5.0,
        "turnRate": 360.0
    },
    "faction": "",
    "tier": 1
})";
        RegisterTemplate(tmpl);
    }

    // Spell template
    {
        AssetTemplate tmpl;
        tmpl.id = "blank_spell";
        tmpl.name = "Blank Spell";
        tmpl.description = "Empty spell configuration";
        tmpl.type = AssetType::Spell;
        tmpl.isBuiltIn = true;
        tmpl.templateJson = R"({
    "id": "",
    "type": "spell",
    "name": "",
    "description": "",
    "tags": [],
    "school": "arcane",
    "targetType": "single",
    "damage": 0,
    "manaCost": 10,
    "cooldown": 5.0,
    "range": 10.0
})";
        RegisterTemplate(tmpl);
    }

    // Building template
    {
        AssetTemplate tmpl;
        tmpl.id = "blank_building";
        tmpl.name = "Blank Building";
        tmpl.description = "Empty building configuration";
        tmpl.type = AssetType::Building;
        tmpl.isBuiltIn = true;
        tmpl.templateJson = R"({
    "id": "",
    "type": "building",
    "name": "",
    "description": "",
    "tags": [],
    "footprint": { "width": 2, "height": 2 },
    "stats": { "health": 500, "armor": 10, "buildTime": 30.0 },
    "costs": { "gold": 100, "wood": 50 }
})";
        RegisterTemplate(tmpl);
    }

    // Effect template
    {
        AssetTemplate tmpl;
        tmpl.id = "blank_effect";
        tmpl.name = "Blank Effect";
        tmpl.description = "Empty effect configuration";
        tmpl.type = AssetType::Effect;
        tmpl.isBuiltIn = true;
        tmpl.templateJson = R"({
    "id": "",
    "type": "effect",
    "name": "",
    "description": "",
    "tags": [],
    "duration": 5.0,
    "interval": 1.0,
    "stackable": false
})";
        RegisterTemplate(tmpl);
    }
}

std::vector<AssetTemplate> ContentActions::GetTemplates() const {
    return m_templates;
}

std::vector<AssetTemplate> ContentActions::GetTemplatesForType(AssetType type) const {
    std::vector<AssetTemplate> result;

    for (const auto& tmpl : m_templates) {
        if (tmpl.type == type) {
            result.push_back(tmpl);
        }
    }

    return result;
}

std::optional<AssetTemplate> ContentActions::GetTemplate(const std::string& templateId) const {
    auto it = m_templateIndex.find(templateId);
    if (it != m_templateIndex.end() && it->second < m_templates.size()) {
        return m_templates[it->second];
    }
    return std::nullopt;
}

bool ContentActions::RegisterTemplate(const AssetTemplate& tmpl) {
    if (m_templateIndex.count(tmpl.id)) {
        return false;  // Already exists
    }

    m_templateIndex[tmpl.id] = m_templates.size();
    m_templates.push_back(tmpl);
    return true;
}

bool ContentActions::RemoveTemplate(const std::string& templateId) {
    auto it = m_templateIndex.find(templateId);
    if (it == m_templateIndex.end()) {
        return false;
    }

    // Don't allow removing built-in templates
    if (m_templates[it->second].isBuiltIn) {
        return false;
    }

    m_templates.erase(m_templates.begin() + it->second);

    // Rebuild index
    m_templateIndex.clear();
    for (size_t i = 0; i < m_templates.size(); ++i) {
        m_templateIndex[m_templates[i].id] = i;
    }

    return true;
}

ActionResult ContentActions::CreateTemplateFromAsset(const std::string& assetId,
                                                      const std::string& templateName) {
    // Would read asset and create template from it
    return ActionResult::Failure("Not implemented");
}

void ContentActions::LoadTemplates(const std::string& directory) {
    // Would load custom templates from directory
}

void ContentActions::SaveTemplates(const std::string& directory) {
    // Would save custom templates to directory
}

std::string ContentActions::GetDefaultTemplate(AssetType type) const {
    auto templates = GetTemplatesForType(type);
    if (!templates.empty()) {
        return templates[0].templateJson;
    }

    // Return minimal template
    return R"({
    "id": "",
    "type": ")" + std::string(AssetTypeToString(type)) + R"(",
    "name": "",
    "description": "",
    "tags": []
})";
}

std::string ContentActions::ApplyTemplateValues(const std::string& templateJson,
    const std::unordered_map<std::string, std::string>& values) const {

    if (values.empty()) {
        return templateJson;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    builder["allowComments"] = true;
    std::istringstream stream(templateJson);
    std::string errors;
    Json::parseFromStream(builder, stream, &root, &errors);

    for (const auto& [key, value] : values) {
        // Simple top-level assignment
        if (root.isMember(key)) {
            // Try to preserve type
            if (root[key].isInt()) {
                try {
                    root[key] = std::stoi(value);
                } catch (...) {
                    root[key] = value;
                }
            } else if (root[key].isDouble()) {
                try {
                    root[key] = std::stod(value);
                } catch (...) {
                    root[key] = value;
                }
            } else if (root[key].isBool()) {
                root[key] = (value == "true" || value == "1");
            } else {
                root[key] = value;
            }
        }
    }

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "    ";
    return Json::writeString(writerBuilder, root);
}

// ============================================================================
// Reference Update
// ============================================================================

int ContentActions::UpdateReferences(const std::string& oldId, const std::string& newId) {
    int updateCount = 0;

    // Would scan all config files and update references
    // This is a simplified implementation

    return updateCount;
}

std::vector<std::string> ContentActions::FindReferences(const std::string& assetId) const {
    std::vector<std::string> references;
    // Would search all configs for references to this asset
    return references;
}

std::vector<std::pair<std::string, std::string>> ContentActions::FindBrokenReferences() const {
    std::vector<std::pair<std::string, std::string>> broken;
    // Would check all references and find ones that don't exist
    return broken;
}

bool ContentActions::FixBrokenReference(const std::string& assetId, const std::string& brokenRef,
                                         const std::string& newRef) {
    // Would update the reference in the asset
    return false;
}

// ============================================================================
// Validation
// ============================================================================

std::vector<std::string> ContentActions::ValidateAsset(const std::string& assetId) const {
    std::vector<std::string> errors;
    // Would validate the asset against its schema
    return errors;
}

std::unordered_map<std::string, std::vector<std::string>> ContentActions::ValidateAll() const {
    std::unordered_map<std::string, std::vector<std::string>> results;
    // Would validate all assets
    return results;
}

ActionResult ContentActions::AutoFix(const std::string& assetId) {
    // Would attempt to fix common validation issues
    return ActionResult::Failure("Not implemented");
}

// ============================================================================
// Helper Methods
// ============================================================================

bool ContentActions::WriteAssetFile(const std::string& path, const std::string& content) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        return file.good();
    } catch (...) {
        return false;
    }
}

bool ContentActions::DeleteAssetFile(const std::string& path) {
    try {
        return std::filesystem::remove(path);
    } catch (...) {
        return false;
    }
}

bool ContentActions::MoveAssetFile(const std::string& sourcePath, const std::string& targetPath) {
    try {
        std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path());
        std::filesystem::rename(sourcePath, targetPath);
        return true;
    } catch (...) {
        return false;
    }
}

bool ContentActions::CopyAssetFile(const std::string& sourcePath, const std::string& targetPath) {
    try {
        std::filesystem::create_directories(std::filesystem::path(targetPath).parent_path());
        std::filesystem::copy_file(sourcePath, targetPath);
        return true;
    } catch (...) {
        return false;
    }
}

std::string ContentActions::SanitizeName(const std::string& name) const {
    std::string result;
    result.reserve(name.length());

    for (char c : name) {
        if (std::isalnum(c)) {
            result += std::tolower(c);
        } else if (c == ' ' || c == '-') {
            if (!result.empty() && result.back() != '_') {
                result += '_';
            }
        }
    }

    // Remove trailing underscore
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }

    return result;
}

std::string ContentActions::GenerateCopyName(const std::string& originalName) const {
    std::string base = originalName;
    int number = 1;

    // Check if already has a copy number
    static std::regex copyPattern(R"((.*)\s+\((\d+)\)$)");
    std::smatch match;

    if (std::regex_match(originalName, match, copyPattern)) {
        base = match[1].str();
        number = std::stoi(match[2].str()) + 1;
    }

    return base + " (" + std::to_string(number) + ")";
}

std::string ContentActions::GenerateAssetId(const std::string& name, AssetType type) const {
    std::string typePrefix = AssetTypeToString(type);
    // Remove trailing 's' for singular
    if (!typePrefix.empty() && typePrefix.back() == 's') {
        typePrefix.pop_back();
    }

    return typePrefix + "_" + name;
}

std::string ContentActions::GetTrashPath() const {
    return ".content_trash";
}

std::string ContentActions::MoveToTrash(const std::string& assetPath) {
    try {
        std::string trashPath = GetTrashPath();
        std::filesystem::create_directories(trashPath);

        std::string filename = std::filesystem::path(assetPath).filename().string();
        std::string targetPath = trashPath + "/" + std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()) + "_" + filename;

        std::filesystem::rename(assetPath, targetPath);
        return targetPath;
    } catch (...) {
        return "";
    }
}

bool ContentActions::RestoreFromTrash(const std::string& trashPath, const std::string& originalPath) {
    try {
        std::filesystem::create_directories(std::filesystem::path(originalPath).parent_path());
        std::filesystem::rename(trashPath, originalPath);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Editor Commands
// ============================================================================

CreateAssetCommand::CreateAssetCommand(ContentActions* actions, const CreateOptions& options)
    : m_actions(actions), m_options(options)
{
}

void CreateAssetCommand::Execute() {
    auto result = m_actions->Create(m_options);
    if (result.success) {
        m_createdId = result.assetId;
        m_createdPath = result.assetPath;
    }
}

void CreateAssetCommand::Undo() {
    if (!m_createdId.empty()) {
        DeleteOptions options;
        options.moveToTrash = false;  // Permanently delete on undo
        m_actions->Delete(m_createdId, options);
    }
}

std::string CreateAssetCommand::GetDescription() const {
    return "Create " + m_options.name;
}

DeleteAssetCommand::DeleteAssetCommand(ContentActions* actions, const std::string& assetId,
                                       const DeleteOptions& options)
    : m_actions(actions), m_assetId(assetId), m_options(options)
{
}

void DeleteAssetCommand::Execute() {
    // Backup content before deleting
    // Would read the file and store content
    m_actions->Delete(m_assetId, m_options);
}

void DeleteAssetCommand::Undo() {
    // Would restore from backup content
    m_actions->Restore(m_assetId);
}

std::string DeleteAssetCommand::GetDescription() const {
    return "Delete " + m_assetId;
}

RenameAssetCommand::RenameAssetCommand(ContentActions* actions, const std::string& assetId,
                                       const RenameOptions& options)
    : m_actions(actions), m_assetId(assetId), m_options(options)
{
}

void RenameAssetCommand::Execute() {
    // Store old name before renaming
    // Would read from file
    m_actions->Rename(m_assetId, m_options);
}

void RenameAssetCommand::Undo() {
    RenameOptions undoOptions;
    undoOptions.newName = m_oldName;
    m_actions->Rename(m_assetId, undoOptions);
}

std::string RenameAssetCommand::GetDescription() const {
    return "Rename to " + m_options.newName;
}

MoveAssetCommand::MoveAssetCommand(ContentActions* actions, const std::string& assetId,
                                   const MoveOptions& options)
    : m_actions(actions), m_assetId(assetId), m_options(options)
{
}

void MoveAssetCommand::Execute() {
    // Store old path before moving
    // Would get from database
    m_actions->Move(m_assetId, m_options);
}

void MoveAssetCommand::Undo() {
    MoveOptions undoOptions;
    undoOptions.targetFolder = m_oldPath;
    m_actions->Move(m_assetId, undoOptions);
}

std::string MoveAssetCommand::GetDescription() const {
    return "Move to " + m_options.targetFolder;
}

} // namespace Content
} // namespace Vehement
