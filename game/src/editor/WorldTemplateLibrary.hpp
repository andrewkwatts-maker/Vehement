#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include "../../../engine/procedural/WorldTemplate.hpp"

namespace Vehement {

/**
 * @brief Template metadata for quick lookups
 */
struct TemplateMetadata {
    std::string id;
    std::string name;
    std::string description;
    std::string filePath;
    std::string thumbnailPath;
    std::vector<std::string> tags;
    std::vector<std::string> biomeNames;
    size_t resourceCount = 0;
    size_t structureCount = 0;
    bool isValid = false;
    std::string validationError;

    // Timestamp for cache invalidation
    time_t lastModified = 0;
};

/**
 * @brief Search query for filtering templates
 */
struct TemplateSearchQuery {
    std::string searchText;
    std::vector<std::string> tags;
    std::vector<std::string> biomes;
    bool requireAllTags = false;
    bool requireAllBiomes = false;
};

/**
 * @brief World template library manager
 *
 * Manages all available world templates:
 * - Scans and loads templates from directories
 * - Caches template metadata
 * - Provides search and filter capabilities
 * - Generates and caches preview thumbnails
 * - Validates templates
 * - Supports user-created custom templates
 */
class WorldTemplateLibrary {
public:
    WorldTemplateLibrary();
    ~WorldTemplateLibrary();

    /**
     * @brief Initialize library with default template directories
     */
    void Initialize();

    /**
     * @brief Add a directory to scan for templates
     */
    void AddTemplateDirectory(const std::string& directory);

    /**
     * @brief Scan all registered directories and load templates
     */
    void ScanAndLoadTemplates();

    /**
     * @brief Reload all templates (clears cache)
     */
    void ReloadAll();

    /**
     * @brief Get template by ID
     */
    std::shared_ptr<Nova::ProcGen::WorldTemplate> GetTemplate(const std::string& id) const;

    /**
     * @brief Get template metadata by ID
     */
    const TemplateMetadata* GetTemplateMetadata(const std::string& id) const;

    /**
     * @brief Get all template IDs
     */
    std::vector<std::string> GetAllTemplateIds() const;

    /**
     * @brief Get all templates
     */
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> GetAllTemplates() const;

    /**
     * @brief Search templates
     */
    std::vector<TemplateMetadata> SearchTemplates(const TemplateSearchQuery& query) const;

    /**
     * @brief Filter templates by tag
     */
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> FilterByTag(const std::string& tag) const;

    /**
     * @brief Filter templates by biome
     */
    std::vector<std::shared_ptr<Nova::ProcGen::WorldTemplate>> FilterByBiome(const std::string& biome) const;

    /**
     * @brief Check if template exists
     */
    bool HasTemplate(const std::string& id) const;

    /**
     * @brief Register a template manually
     */
    bool RegisterTemplate(const std::string& id, std::shared_ptr<Nova::ProcGen::WorldTemplate> templ,
                          const std::string& filePath = "");

    /**
     * @brief Unregister a template
     */
    void UnregisterTemplate(const std::string& id);

    /**
     * @brief Validate a template
     */
    bool ValidateTemplate(const std::string& id, std::vector<std::string>& errors) const;

    /**
     * @brief Generate preview thumbnail for template
     */
    bool GenerateThumbnail(const std::string& id, int width = 256, int height = 256);

    /**
     * @brief Get thumbnail path for template
     */
    std::string GetThumbnailPath(const std::string& id) const;

    /**
     * @brief Check if thumbnail exists
     */
    bool HasThumbnail(const std::string& id) const;

    /**
     * @brief Save template to file
     */
    bool SaveTemplate(const std::string& id, const std::string& filePath) const;

    /**
     * @brief Create new template from existing one
     */
    std::shared_ptr<Nova::ProcGen::WorldTemplate> DuplicateTemplate(const std::string& sourceId,
                                                                     const std::string& newId,
                                                                     const std::string& newName);

    /**
     * @brief Get library statistics
     */
    struct LibraryStats {
        size_t totalTemplates = 0;
        size_t validTemplates = 0;
        size_t invalidTemplates = 0;
        size_t cachedThumbnails = 0;
        size_t totalBiomes = 0;
        size_t totalTags = 0;
    };
    LibraryStats GetStatistics() const;

    /**
     * @brief Get all unique tags across all templates
     */
    std::vector<std::string> GetAllTags() const;

    /**
     * @brief Get all unique biomes across all templates
     */
    std::vector<std::string> GetAllBiomes() const;

    /**
     * @brief Set callback for template loading progress
     */
    void SetProgressCallback(std::function<void(int current, int total, const std::string& templateName)> callback) {
        m_progressCallback = callback;
    }

    /**
     * @brief Clear metadata cache
     */
    void ClearCache();

    /**
     * @brief Save metadata cache to disk
     */
    bool SaveMetadataCache();

    /**
     * @brief Load metadata cache from disk
     */
    bool LoadMetadataCache();

private:
    // Template storage
    std::unordered_map<std::string, std::shared_ptr<Nova::ProcGen::WorldTemplate>> m_templates;
    std::unordered_map<std::string, TemplateMetadata> m_metadata;

    // Template directories
    std::vector<std::string> m_templateDirectories;

    // Callback for loading progress
    std::function<void(int, int, const std::string&)> m_progressCallback;

    // Cache
    std::string m_cacheDirectory;
    std::string m_thumbnailDirectory;

    /**
     * @brief Load template from file
     */
    bool LoadTemplateFromFile(const std::string& filePath);

    /**
     * @brief Extract metadata from template
     */
    TemplateMetadata ExtractMetadata(const std::shared_ptr<Nova::ProcGen::WorldTemplate>& templ,
                                     const std::string& filePath) const;

    /**
     * @brief Generate template ID from file path
     */
    std::string GenerateIdFromPath(const std::string& filePath) const;

    /**
     * @brief Check if file has been modified since last load
     */
    bool HasFileChanged(const std::string& filePath, time_t lastModified) const;

    /**
     * @brief Get file modification time
     */
    time_t GetFileModificationTime(const std::string& filePath) const;

    /**
     * @brief Match template against search query
     */
    bool MatchesQuery(const TemplateMetadata& metadata, const TemplateSearchQuery& query) const;

    /**
     * @brief Generate simple heightmap preview
     */
    bool GenerateHeightmapPreview(const std::shared_ptr<Nova::ProcGen::WorldTemplate>& templ,
                                  const std::string& outputPath, int width, int height);
};

/**
 * @brief Helper functions for template management
 */
namespace TemplateLibraryUtils {
    /**
     * @brief Get default template directories
     */
    std::vector<std::string> GetDefaultTemplateDirectories();

    /**
     * @brief Get user templates directory
     */
    std::string GetUserTemplatesDirectory();

    /**
     * @brief Get built-in templates directory
     */
    std::string GetBuiltInTemplatesDirectory();

    /**
     * @brief Validate template file format
     */
    bool ValidateTemplateFile(const std::string& filePath, std::vector<std::string>& errors);

    /**
     * @brief Extract template name from file path
     */
    std::string ExtractTemplateName(const std::string& filePath);
}

} // namespace Vehement
