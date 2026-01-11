#include "WorldTemplateLibrary.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace Vehement {

// =============================================================================
// WorldTemplateLibrary Implementation
// =============================================================================

WorldTemplateLibrary::WorldTemplateLibrary()
    : m_cacheDirectory("cache/templates/")
    , m_thumbnailDirectory("cache/templates/thumbnails/")
{
}

WorldTemplateLibrary::~WorldTemplateLibrary() = default;

void WorldTemplateLibrary::Initialize() {
    // Create cache directories
    fs::create_directories(m_cacheDirectory);
    fs::create_directories(m_thumbnailDirectory);

    // Add default template directories
    for (const auto& dir : TemplateLibraryUtils::GetDefaultTemplateDirectories()) {
        AddTemplateDirectory(dir);
    }

    // Load metadata cache
    LoadMetadataCache();

    // Scan and load templates
    ScanAndLoadTemplates();
}

void WorldTemplateLibrary::AddTemplateDirectory(const std::string& directory) {
    if (fs::exists(directory) && fs::is_directory(directory)) {
        m_templateDirectories.push_back(directory);
    }
}

void WorldTemplateLibrary::ScanAndLoadTemplates() {
    int totalFiles = 0;
    int currentFile = 0;

    // Count total JSON files
    for (const auto& directory : m_templateDirectories) {
        if (fs::exists(directory)) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    totalFiles++;
                }
            }
        }
    }

    // Load templates
    for (const auto& directory : m_templateDirectories) {
        if (!fs::exists(directory)) continue;

        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                currentFile++;
                std::string filePath = entry.path().string();

                if (m_progressCallback) {
                    std::string name = entry.path().stem().string();
                    m_progressCallback(currentFile, totalFiles, name);
                }

                LoadTemplateFromFile(filePath);
            }
        }
    }
}

void WorldTemplateLibrary::ReloadAll() {
    m_templates.clear();
    m_metadata.clear();
    ScanAndLoadTemplates();
}

std::shared_ptr<Nova::ProcGen::WorldTemplate> WorldTemplateLibrary::GetTemplate(const std::string& id) const {
    auto it = m_templates.find(id);
    if (it != m_templates.end()) {
        return it->second;
    }
    return nullptr;
}

const TemplateMetadata* WorldTemplateLibrary::GetTemplateMetadata(const std::string& id) const {
    auto it = m_metadata.find(id);
    if (it != m_metadata.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> WorldTemplateLibrary::GetAllTemplateIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_templates.size());
    for (const auto& pair : m_templates) {
        ids.push_back(pair.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> WorldTemplateLibrary::GetAllTemplates() const {
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> templates;
    templates.reserve(m_templates.size());
    for (const auto& pair : m_templates) {
        templates.push_back(pair.second);
    }
    return templates;
}

std::vector<TemplateMetadata> WorldTemplateLibrary::SearchTemplates(const TemplateSearchQuery& query) const {
    std::vector<TemplateMetadata> results;

    for (const auto& pair : m_metadata) {
        if (MatchesQuery(pair.second, query)) {
            results.push_back(pair.second);
        }
    }

    return results;
}

std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>>
WorldTemplateLibrary::FilterByTag(const std::string& tag) const {
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> results;

    for (const auto& pair : m_metadata) {
        auto it = std::find(pair.second.tags.begin(), pair.second.tags.end(), tag);
        if (it != pair.second.tags.end()) {
            auto templ = GetTemplate(pair.first);
            if (templ) {
                results.push_back(templ);
            }
        }
    }

    return results;
}

std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>>
WorldTemplateLibrary::FilterByBiome(const std::string& biome) const {
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> results;

    for (const auto& pair : m_metadata) {
        auto it = std::find(pair.second.biomeNames.begin(), pair.second.biomeNames.end(), biome);
        if (it != pair.second.biomeNames.end()) {
            auto templ = GetTemplate(pair.first);
            if (templ) {
                results.push_back(templ);
            }
        }
    }

    return results;
}

bool WorldTemplateLibrary::HasTemplate(const std::string& id) const {
    return m_templates.find(id) != m_templates.end();
}

bool WorldTemplateLibrary::RegisterTemplate(const std::string& id,
                                           std::shared_ptr<Nova::ProcGen::WorldTemplate> templ,
                                           const std::string& filePath) {
    if (!templ) return false;

    m_templates[id] = templ;
    m_metadata[id] = ExtractMetadata(templ, filePath);
    m_metadata[id].id = id;

    return true;
}

void WorldTemplateLibrary::UnregisterTemplate(const std::string& id) {
    m_templates.erase(id);
    m_metadata.erase(id);
}

bool WorldTemplateLibrary::ValidateTemplate(const std::string& id, std::vector<std::string>& errors) const {
    auto templ = GetTemplate(id);
    if (!templ) {
        errors.push_back("Template not found: " + id);
        return false;
    }

    return templ->Validate(errors);
}

bool WorldTemplateLibrary::GenerateThumbnail(const std::string& id, int width, int height) {
    auto templ = GetTemplate(id);
    if (!templ) return false;

    std::string thumbnailPath = GetThumbnailPath(id);
    return GenerateHeightmapPreview(templ, thumbnailPath, width, height);
}

std::string WorldTemplateLibrary::GetThumbnailPath(const std::string& id) const {
    return m_thumbnailDirectory + id + ".png";
}

bool WorldTemplateLibrary::HasThumbnail(const std::string& id) const {
    return fs::exists(GetThumbnailPath(id));
}

bool WorldTemplateLibrary::SaveTemplate(const std::string& id, const std::string& filePath) const {
    auto templ = GetTemplate(id);
    if (!templ) return false;

    return templ->SaveToFile(filePath);
}

std::shared_ptr<Nova::ProcGen::WorldTemplate>
WorldTemplateLibrary::DuplicateTemplate(const std::string& sourceId,
                                       const std::string& newId,
                                       const std::string& newName) {
    auto source = GetTemplate(sourceId);
    if (!source) return nullptr;

    // Create a deep copy by serializing and deserializing
    auto json = source->SaveToJson();
    auto duplicate = Nova::ProcGen::WorldTemplate::LoadFromJson(json);

    if (duplicate) {
        duplicate->name = newName;
        RegisterTemplate(newId, duplicate);
    }

    return duplicate;
}

WorldTemplateLibrary::LibraryStats WorldTemplateLibrary::GetStatistics() const {
    LibraryStats stats;
    stats.totalTemplates = m_templates.size();

    for (const auto& pair : m_metadata) {
        if (pair.second.isValid) {
            stats.validTemplates++;
        } else {
            stats.invalidTemplates++;
        }

        if (HasThumbnail(pair.first)) {
            stats.cachedThumbnails++;
        }
    }

    auto allTags = GetAllTags();
    auto allBiomes = GetAllBiomes();
    stats.totalTags = allTags.size();
    stats.totalBiomes = allBiomes.size();

    return stats;
}

std::vector<std::string> WorldTemplateLibrary::GetAllTags() const {
    std::set<std::string> uniqueTags;
    for (const auto& pair : m_metadata) {
        for (const auto& tag : pair.second.tags) {
            uniqueTags.insert(tag);
        }
    }
    return std::vector<std::string>(uniqueTags.begin(), uniqueTags.end());
}

std::vector<std::string> WorldTemplateLibrary::GetAllBiomes() const {
    std::set<std::string> uniqueBiomes;
    for (const auto& pair : m_metadata) {
        for (const auto& biome : pair.second.biomeNames) {
            uniqueBiomes.insert(biome);
        }
    }
    return std::vector<std::string>(uniqueBiomes.begin(), uniqueBiomes.end());
}

void WorldTemplateLibrary::ClearCache() {
    m_metadata.clear();
}

bool WorldTemplateLibrary::SaveMetadataCache() {
    // For now, just return true
    // In a full implementation, serialize m_metadata to JSON
    return true;
}

bool WorldTemplateLibrary::LoadMetadataCache() {
    // For now, just return true
    // In a full implementation, deserialize m_metadata from JSON
    return true;
}

bool WorldTemplateLibrary::LoadTemplateFromFile(const std::string& filePath) {
    try {
        auto templ = Nova::ProcGen::WorldTemplate::LoadFromFile(filePath);
        if (!templ) return false;

        std::string id = GenerateIdFromPath(filePath);
        m_templates[id] = templ;
        m_metadata[id] = ExtractMetadata(templ, filePath);
        m_metadata[id].id = id;

        return true;
    } catch (const std::exception& e) {
        // Log error
        return false;
    }
}

TemplateMetadata WorldTemplateLibrary::ExtractMetadata(
    const std::shared_ptr<Nova::ProcGen::WorldTemplate>& templ,
    const std::string& filePath) const {

    TemplateMetadata metadata;
    metadata.name = templ->name;
    metadata.description = templ->description;
    metadata.filePath = filePath;
    metadata.tags = templ->tags;
    metadata.lastModified = GetFileModificationTime(filePath);

    // Extract biome names
    for (const auto& biome : templ->biomes) {
        metadata.biomeNames.push_back(biome.name);
    }

    // Count resources and structures
    metadata.resourceCount = templ->ores.size() + templ->vegetation.size();
    metadata.structureCount = templ->ruins.size() + templ->ancients.size() + templ->buildings.size();

    // Validate
    std::vector<std::string> errors;
    metadata.isValid = templ->Validate(errors);
    if (!errors.empty()) {
        metadata.validationError = errors[0];
    }

    // Set thumbnail path
    metadata.thumbnailPath = GetThumbnailPath(metadata.id);

    return metadata;
}

std::string WorldTemplateLibrary::GenerateIdFromPath(const std::string& filePath) const {
    fs::path path(filePath);
    return path.stem().string();
}

bool WorldTemplateLibrary::HasFileChanged(const std::string& filePath, time_t lastModified) const {
    time_t currentModified = GetFileModificationTime(filePath);
    return currentModified > lastModified;
}

time_t WorldTemplateLibrary::GetFileModificationTime(const std::string& filePath) const {
    struct stat fileInfo;
    if (stat(filePath.c_str(), &fileInfo) == 0) {
        return fileInfo.st_mtime;
    }
    return 0;
}

bool WorldTemplateLibrary::MatchesQuery(const TemplateMetadata& metadata,
                                       const TemplateSearchQuery& query) const {
    // Check search text
    if (!query.searchText.empty()) {
        std::string lowerSearch = query.searchText;
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

        std::string lowerName = metadata.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        std::string lowerDesc = metadata.description;
        std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

        if (lowerName.find(lowerSearch) == std::string::npos &&
            lowerDesc.find(lowerSearch) == std::string::npos) {
            return false;
        }
    }

    // Check tags
    if (!query.tags.empty()) {
        if (query.requireAllTags) {
            // All tags must match
            for (const auto& tag : query.tags) {
                auto it = std::find(metadata.tags.begin(), metadata.tags.end(), tag);
                if (it == metadata.tags.end()) {
                    return false;
                }
            }
        } else {
            // At least one tag must match
            bool hasMatch = false;
            for (const auto& tag : query.tags) {
                auto it = std::find(metadata.tags.begin(), metadata.tags.end(), tag);
                if (it != metadata.tags.end()) {
                    hasMatch = true;
                    break;
                }
            }
            if (!hasMatch) {
                return false;
            }
        }
    }

    // Check biomes
    if (!query.biomes.empty()) {
        if (query.requireAllBiomes) {
            // All biomes must match
            for (const auto& biome : query.biomes) {
                auto it = std::find(metadata.biomeNames.begin(), metadata.biomeNames.end(), biome);
                if (it == metadata.biomeNames.end()) {
                    return false;
                }
            }
        } else {
            // At least one biome must match
            bool hasMatch = false;
            for (const auto& biome : query.biomes) {
                auto it = std::find(metadata.biomeNames.begin(), metadata.biomeNames.end(), biome);
                if (it != metadata.biomeNames.end()) {
                    hasMatch = true;
                    break;
                }
            }
            if (!hasMatch) {
                return false;
            }
        }
    }

    return true;
}

bool WorldTemplateLibrary::GenerateHeightmapPreview(
    const std::shared_ptr<Nova::ProcGen::WorldTemplate>& templ,
    const std::string& outputPath, int width, int height) {

    // For now, just create a placeholder
    // In a full implementation, this would:
    // 1. Create a ProcGenGraph from the template
    // 2. Generate a small chunk
    // 3. Render it to an image
    // 4. Save as PNG

    return false; // Not implemented yet
}

// =============================================================================
// TemplateLibraryUtils Implementation
// =============================================================================

std::vector<std::string> TemplateLibraryUtils::GetDefaultTemplateDirectories() {
    std::vector<std::string> directories;
    directories.push_back(GetBuiltInTemplatesDirectory());
    directories.push_back(GetUserTemplatesDirectory());
    return directories;
}

std::string TemplateLibraryUtils::GetUserTemplatesDirectory() {
    return "game/assets/procgen/templates/user/";
}

std::string TemplateLibraryUtils::GetBuiltInTemplatesDirectory() {
    return "game/assets/procgen/templates/";
}

bool TemplateLibraryUtils::ValidateTemplateFile(const std::string& filePath,
                                                std::vector<std::string>& errors) {
    if (!fs::exists(filePath)) {
        errors.push_back("File does not exist: " + filePath);
        return false;
    }

    if (!fs::is_regular_file(filePath)) {
        errors.push_back("Not a regular file: " + filePath);
        return false;
    }

    if (fs::path(filePath).extension() != ".json") {
        errors.push_back("File is not a JSON file: " + filePath);
        return false;
    }

    // Try to load as template
    try {
        auto templ = Nova::ProcGen::WorldTemplate::LoadFromFile(filePath);
        if (!templ) {
            errors.push_back("Failed to parse template file");
            return false;
        }

        return templ->Validate(errors);
    } catch (const std::exception& e) {
        errors.push_back(std::string("Exception: ") + e.what());
        return false;
    }
}

std::string TemplateLibraryUtils::ExtractTemplateName(const std::string& filePath) {
    fs::path path(filePath);
    return path.stem().string();
}

} // namespace Vehement
