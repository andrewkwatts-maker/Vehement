#include "ScriptStorage.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <regex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Nova {
namespace Scripting {

ScriptStorage::ScriptStorage() = default;
ScriptStorage::~ScriptStorage() {
    Shutdown();
}

bool ScriptStorage::Initialize(const std::string& basePath) {
    if (m_initialized) return true;

    m_basePath = basePath;
    m_scriptsPath = basePath + "/scripts";

    // Ensure scripts directory exists
    EnsureDirectoryExists(m_scriptsPath);

    // Create default category directories
    std::vector<std::string> defaultCategories = {
        "ai", "events", "pcg", "combat", "utility", "templates", "examples"
    };
    for (const auto& cat : defaultCategories) {
        EnsureDirectoryExists(m_scriptsPath + "/" + cat);
    }

    // Discover existing scripts
    DiscoverScripts(m_scriptsPath);

    m_initialized = true;
    return true;
}

void ScriptStorage::Shutdown() {
    ClearCache();
    m_scriptIndex.clear();
    m_watchedFiles.clear();
    m_initialized = false;
}

// =========================================================================
// Inline Storage
// =========================================================================

bool ScriptStorage::StoreInline(const std::string& configPath,
                                const std::string& functionName,
                                const std::string& code,
                                const std::string& jsonPath) {
    std::string path = jsonPath.empty() ? "scripts." + functionName : jsonPath;
    return UpdateJsonScript(ResolvePath(configPath), path, code);
}

std::string ScriptStorage::GetInlineScript(const std::string& configPath,
                                           const std::string& functionName) {
    std::string path = "scripts." + functionName;
    return ReadJsonScript(ResolvePath(configPath), path);
}

std::vector<std::string> ScriptStorage::ListInlineScripts(const std::string& configPath) {
    std::vector<std::string> result;

    std::string resolved = ResolvePath(configPath);
    std::string content = ReadFile(resolved);
    if (content.empty()) return result;

    try {
        json j = json::parse(content);
        if (j.contains("scripts") && j["scripts"].is_object()) {
            for (auto& [key, value] : j["scripts"].items()) {
                result.push_back(key);
            }
        }
    } catch (...) {}

    return result;
}

bool ScriptStorage::RemoveInlineScript(const std::string& configPath,
                                       const std::string& functionName) {
    std::string resolved = ResolvePath(configPath);
    std::string content = ReadFile(resolved);
    if (content.empty()) return false;

    try {
        json j = json::parse(content);
        if (j.contains("scripts") && j["scripts"].contains(functionName)) {
            j["scripts"].erase(functionName);
            return WriteFile(resolved, j.dump(2));
        }
    } catch (...) {}

    return false;
}

// =========================================================================
// Adjacent Storage
// =========================================================================

std::string ScriptStorage::StoreAdjacent(const std::string& configPath,
                                         const std::string& code,
                                         const std::string& scriptName) {
    std::string scriptPath = GetAdjacentScriptPath(configPath, scriptName);

    if (WriteFile(scriptPath, code)) {
        IndexScript(scriptPath);
        return scriptPath;
    }

    return "";
}

std::string ScriptStorage::GetAdjacentScriptPath(const std::string& configPath,
                                                 const std::string& scriptName) const {
    fs::path config(configPath);
    std::string baseName = scriptName.empty() ? config.stem().string() : scriptName;
    return (config.parent_path() / (baseName + ".py")).string();
}

bool ScriptStorage::HasAdjacentScript(const std::string& configPath,
                                      const std::string& scriptName) const {
    return fs::exists(GetAdjacentScriptPath(configPath, scriptName));
}

// =========================================================================
// Central Storage
// =========================================================================

std::string ScriptStorage::StoreCentral(const std::string& category,
                                        const std::string& functionName,
                                        const std::string& code) {
    std::string categoryPath = m_scriptsPath + "/" + category;
    EnsureDirectoryExists(categoryPath);

    std::string scriptPath = categoryPath + "/" + functionName + ".py";

    if (WriteFile(scriptPath, code)) {
        IndexScript(scriptPath);
        return scriptPath;
    }

    return "";
}

std::string ScriptStorage::GetCentralScriptPath(const std::string& category,
                                                const std::string& functionName) const {
    return m_scriptsPath + "/" + category + "/" + functionName + ".py";
}

std::vector<std::string> ScriptStorage::ListCategories() const {
    std::vector<std::string> categories;

    if (fs::exists(m_scriptsPath)) {
        for (const auto& entry : fs::directory_iterator(m_scriptsPath)) {
            if (entry.is_directory()) {
                categories.push_back(entry.path().filename().string());
            }
        }
    }

    std::sort(categories.begin(), categories.end());
    return categories;
}

std::vector<std::string> ScriptStorage::ListCategoryScripts(const std::string& category) const {
    std::vector<std::string> scripts;
    std::string categoryPath = m_scriptsPath + "/" + category;

    if (fs::exists(categoryPath)) {
        for (const auto& entry : fs::directory_iterator(categoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".py") {
                scripts.push_back(entry.path().stem().string());
            }
        }
    }

    std::sort(scripts.begin(), scripts.end());
    return scripts;
}

// =========================================================================
// Generic Operations
// =========================================================================

std::string ScriptStorage::GetScript(const std::string& path) {
    std::string resolved = ResolvePath(path);

    // Check cache first
    auto it = m_cache.find(resolved);
    if (it != m_cache.end()) {
        // Check if file has been modified
        auto modTime = GetFileModTime(resolved);
        if (modTime <= it->second.fileModTime) {
            m_cacheStats.cacheHits++;
            return it->second.content;
        }
    }

    m_cacheStats.cacheMisses++;

    // Read from disk
    std::string content = ReadFile(resolved);

    // Update cache
    if (!content.empty()) {
        CacheEntry entry;
        entry.content = content;
        entry.hash = ComputeHash(content);
        entry.loadTime = std::chrono::system_clock::now();
        entry.fileModTime = GetFileModTime(resolved);
        m_cache[resolved] = entry;
        m_cacheStats.cachedScripts = m_cache.size();
        m_cacheStats.totalBytes += content.size();
    }

    return content;
}

bool ScriptStorage::SaveScript(const std::string& path, const std::string& code) {
    std::string resolved = ResolvePath(path);

    // Ensure directory exists
    fs::path filePath(resolved);
    EnsureDirectoryExists(filePath.parent_path().string());

    if (WriteFile(resolved, code)) {
        // Update cache
        CacheEntry entry;
        entry.content = code;
        entry.hash = ComputeHash(code);
        entry.loadTime = std::chrono::system_clock::now();
        entry.fileModTime = std::chrono::system_clock::now();
        m_cache[resolved] = entry;

        // Update index
        IndexScript(resolved);

        // Notify watchers
        if (m_onScriptChanged) {
            m_onScriptChanged(resolved);
        }

        return true;
    }

    return false;
}

bool ScriptStorage::DeleteScript(const std::string& path) {
    std::string resolved = ResolvePath(path);

    try {
        if (fs::exists(resolved)) {
            fs::remove(resolved);

            // Remove from cache
            m_cache.erase(resolved);

            // Remove from index
            m_scriptIndex.erase(resolved);

            return true;
        }
    } catch (...) {}

    return false;
}

bool ScriptStorage::ScriptExists(const std::string& path) const {
    return fs::exists(ResolvePath(path));
}

std::optional<ScriptInfo> ScriptStorage::GetScriptInfo(const std::string& path) const {
    std::string resolved = ResolvePath(path);

    auto it = m_scriptIndex.find(resolved);
    if (it != m_scriptIndex.end()) {
        return it->second;
    }

    // Generate info from file
    if (fs::exists(resolved)) {
        ScriptInfo info;
        info.path = resolved;
        info.name = fs::path(resolved).stem().string();
        info.location = StorageLocation::Central;  // Assume central
        info.lastModified = GetFileModTime(resolved);

        // Read content and extract metadata
        std::string content = ReadFile(resolved);
        if (!content.empty()) {
            info.content = content;
            info.contentHash = ComputeHash(content);

            // Extract metadata from header
            ScriptInfo metadata = ExtractMetadata(content);
            info.author = metadata.author;
            info.description = metadata.description;
            info.version = metadata.version;
            info.tags = metadata.tags;
        }

        return info;
    }

    return std::nullopt;
}

std::vector<ScriptInfo> ScriptStorage::GetAllScripts(const ScriptSearchCriteria& criteria) {
    std::vector<ScriptInfo> result;

    for (const auto& [path, info] : m_scriptIndex) {
        bool matches = true;

        // Filter by category
        if (!criteria.category.empty() && info.category != criteria.category) {
            matches = false;
        }

        // Filter by location
        if (info.location != criteria.location) {
            // Allow if no specific filter
        }

        // Filter by validity
        if (criteria.validOnly && !info.isValid) {
            matches = false;
        }

        // Filter by name pattern
        if (!criteria.namePattern.empty()) {
            std::regex pattern(criteria.namePattern, std::regex::icase);
            if (!std::regex_search(info.name, pattern)) {
                matches = false;
            }
        }

        // Filter by tags
        if (!criteria.tags.empty()) {
            for (const auto& tag : criteria.tags) {
                if (std::find(info.tags.begin(), info.tags.end(), tag) == info.tags.end()) {
                    matches = false;
                    break;
                }
            }
        }

        if (matches) {
            result.push_back(info);
        }
    }

    // Sort by name
    std::sort(result.begin(), result.end(), [](const ScriptInfo& a, const ScriptInfo& b) {
        return a.name < b.name;
    });

    return result;
}

std::vector<ScriptInfo> ScriptStorage::SearchScripts(const std::string& pattern) {
    ScriptSearchCriteria criteria;
    criteria.namePattern = ".*" + pattern + ".*";
    return GetAllScripts(criteria);
}

// =========================================================================
// Metadata
// =========================================================================

ScriptInfo ScriptStorage::ExtractMetadata(const std::string& code) const {
    ScriptInfo info;

    // Look for metadata in header docstring
    // Note: std::regex doesn't have multiline flag; [\s\S] already matches newlines
    std::regex docstringRegex(R"(^\"\"\"([\s\S]*?)\"\"\")");
    std::smatch match;

    if (std::regex_search(code, match, docstringRegex)) {
        std::string docstring = match[1].str();

        // Extract @author
        std::regex authorRegex(R"(@author:\s*(.+))");
        if (std::regex_search(docstring, match, authorRegex)) {
            info.author = match[1].str();
        }

        // Extract @description
        std::regex descRegex(R"(@description:\s*(.+))");
        if (std::regex_search(docstring, match, descRegex)) {
            info.description = match[1].str();
        }

        // Extract @version
        std::regex versionRegex(R"(@version:\s*(.+))");
        if (std::regex_search(docstring, match, versionRegex)) {
            info.version = match[1].str();
        }

        // Extract @tags
        std::regex tagsRegex(R"(@tags:\s*(.+))");
        if (std::regex_search(docstring, match, tagsRegex)) {
            std::string tagsStr = match[1].str();
            std::regex tagSplit(R"(\s*,\s*)");
            std::sregex_token_iterator it(tagsStr.begin(), tagsStr.end(), tagSplit, -1);
            std::sregex_token_iterator end;
            for (; it != end; ++it) {
                info.tags.push_back(*it);
            }
        }
    }

    return info;
}

std::string ScriptStorage::AddMetadataHeader(const std::string& code, const ScriptInfo& info) const {
    std::stringstream header;
    header << "\"\"\"\n";
    header << "@name: " << info.name << "\n";
    if (!info.author.empty()) {
        header << "@author: " << info.author << "\n";
    }
    if (!info.description.empty()) {
        header << "@description: " << info.description << "\n";
    }
    if (!info.version.empty()) {
        header << "@version: " << info.version << "\n";
    }
    if (!info.tags.empty()) {
        header << "@tags: ";
        for (size_t i = 0; i < info.tags.size(); ++i) {
            if (i > 0) header << ", ";
            header << info.tags[i];
        }
        header << "\n";
    }
    header << "\"\"\"\n\n";

    // Remove existing docstring if present
    std::string result = code;
    std::regex docstringRegex(R"(^\"\"\"[\s\S]*?\"\"\"\s*)");
    result = std::regex_replace(result, docstringRegex, "");

    return header.str() + result;
}

bool ScriptStorage::SetScriptDescription(const std::string& path, const std::string& description) {
    auto info = GetScriptInfo(path);
    if (!info) return false;

    info->description = description;
    std::string code = info->content;
    code = AddMetadataHeader(code, *info);

    return SaveScript(path, code);
}

bool ScriptStorage::AddScriptTag(const std::string& path, const std::string& tag) {
    auto info = GetScriptInfo(path);
    if (!info) return false;

    if (std::find(info->tags.begin(), info->tags.end(), tag) == info->tags.end()) {
        info->tags.push_back(tag);
        std::string code = info->content;
        code = AddMetadataHeader(code, *info);
        return SaveScript(path, code);
    }

    return true;
}

// =========================================================================
// File Watching
// =========================================================================

void ScriptStorage::EnableFileWatching(bool enable) {
    m_fileWatchingEnabled = enable;

    if (enable) {
        // Initialize watched files with current mod times
        for (const auto& [path, info] : m_scriptIndex) {
            m_watchedFiles[path] = GetFileModTime(path);
        }
    } else {
        m_watchedFiles.clear();
    }
}

std::vector<std::string> ScriptStorage::CheckForChanges() {
    std::vector<std::string> changed;

    if (!m_fileWatchingEnabled) return changed;

    for (auto& [path, lastTime] : m_watchedFiles) {
        auto currentTime = GetFileModTime(path);
        if (currentTime > lastTime) {
            changed.push_back(path);
            lastTime = currentTime;

            // Clear cache entry
            m_cache.erase(path);

            // Notify callback
            if (m_onScriptChanged) {
                m_onScriptChanged(path);
            }
        }
    }

    return changed;
}

// =========================================================================
// Import/Export
// =========================================================================

int ScriptStorage::ImportScripts(const std::string& sourcePath, const std::string& category) {
    int imported = 0;

    if (fs::is_directory(sourcePath)) {
        for (const auto& entry : fs::recursive_directory_iterator(sourcePath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".py") {
                std::string content = ReadFile(entry.path().string());
                if (!content.empty()) {
                    std::string name = entry.path().stem().string();
                    if (!StoreCentral(category, name, content).empty()) {
                        imported++;
                    }
                }
            }
        }
    }

    return imported;
}

bool ScriptStorage::ExportScripts(const std::vector<std::string>& paths, const std::string& destPath) {
    EnsureDirectoryExists(destPath);

    for (const auto& path : paths) {
        std::string content = GetScript(path);
        if (!content.empty()) {
            fs::path src(path);
            std::string destFile = destPath + "/" + src.filename().string();
            WriteFile(destFile, content);
        }
    }

    return true;
}

bool ScriptStorage::ExportCategory(const std::string& category, const std::string& destPath) {
    std::vector<std::string> scripts;
    std::string categoryPath = m_scriptsPath + "/" + category;

    if (fs::exists(categoryPath)) {
        for (const auto& entry : fs::directory_iterator(categoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".py") {
                scripts.push_back(entry.path().string());
            }
        }
    }

    return ExportScripts(scripts, destPath);
}

// =========================================================================
// Caching
// =========================================================================

void ScriptStorage::ClearCache() {
    m_cache.clear();
    m_cacheStats = CacheStats();
}

std::string ScriptStorage::GetCached(const std::string& path) const {
    auto it = m_cache.find(ResolvePath(path));
    if (it != m_cache.end()) {
        return it->second.content;
    }
    return "";
}

void ScriptStorage::PreloadScripts(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        GetScript(path);  // This will cache the script
    }
}

// =========================================================================
// Private Helpers
// =========================================================================

std::string ScriptStorage::ResolvePath(const std::string& path) const {
    if (path.empty()) return "";

    // Already absolute
    if (fs::path(path).is_absolute()) {
        return NormalizePath(path);
    }

    // Check if it's a relative path from base
    std::string fullPath = m_basePath + "/" + path;
    if (fs::exists(fullPath)) {
        return NormalizePath(fullPath);
    }

    // Check in scripts folder
    fullPath = m_scriptsPath + "/" + path;
    if (fs::exists(fullPath)) {
        return NormalizePath(fullPath);
    }

    // Add .py extension if missing
    if (fs::path(path).extension() != ".py") {
        fullPath = m_scriptsPath + "/" + path + ".py";
        if (fs::exists(fullPath)) {
            return NormalizePath(fullPath);
        }
    }

    // Return as-is for new files
    return NormalizePath(m_basePath + "/" + path);
}

std::string ScriptStorage::NormalizePath(const std::string& path) const {
    return fs::weakly_canonical(path).string();
}

bool ScriptStorage::EnsureDirectoryExists(const std::string& path) {
    try {
        if (!fs::exists(path)) {
            return fs::create_directories(path);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string ScriptStorage::ReadFile(const std::string& path) const {
    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool ScriptStorage::WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << content;
    return file.good();
}

size_t ScriptStorage::ComputeHash(const std::string& content) const {
    return std::hash<std::string>{}(content);
}

std::chrono::system_clock::time_point ScriptStorage::GetFileModTime(const std::string& path) const {
    try {
        auto ftime = fs::last_write_time(path);
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
    } catch (...) {
        return std::chrono::system_clock::time_point();
    }
}

bool ScriptStorage::UpdateJsonScript(const std::string& configPath, const std::string& jsonPath,
                                     const std::string& code) {
    std::string content = ReadFile(configPath);
    json j;

    try {
        if (!content.empty()) {
            j = json::parse(content);
        }
    } catch (...) {
        return false;
    }

    // Navigate to/create the path
    std::vector<std::string> parts;
    std::stringstream ss(jsonPath);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    json* current = &j;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->contains(parts[i])) {
            (*current)[parts[i]] = json::object();
        }
        current = &(*current)[parts[i]];
    }

    (*current)[parts.back()] = code;

    return WriteFile(configPath, j.dump(2));
}

std::string ScriptStorage::ReadJsonScript(const std::string& configPath, const std::string& jsonPath) {
    std::string content = ReadFile(configPath);
    if (content.empty()) return "";

    try {
        json j = json::parse(content);

        // Navigate the path
        std::vector<std::string> parts;
        std::stringstream ss(jsonPath);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        const json* current = &j;
        for (const auto& p : parts) {
            if (!current->contains(p)) return "";
            current = &(*current)[p];
        }

        if (current->is_string()) {
            return current->get<std::string>();
        }
    } catch (...) {}

    return "";
}

void ScriptStorage::DiscoverScripts(const std::string& directory) {
    if (!fs::exists(directory)) return;

    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".py") {
            IndexScript(entry.path().string());
        }
    }
}

void ScriptStorage::IndexScript(const std::string& path) {
    ScriptInfo info;
    info.path = NormalizePath(path);
    info.name = fs::path(path).stem().string();
    info.location = StorageLocation::Central;
    info.lastModified = GetFileModTime(path);

    // Determine category from path
    fs::path relativePath = fs::relative(path, m_scriptsPath);
    if (relativePath.has_parent_path()) {
        info.category = relativePath.parent_path().string();
    }

    // Read and extract metadata
    std::string content = ReadFile(path);
    if (!content.empty()) {
        info.contentHash = ComputeHash(content);
        ScriptInfo metadata = ExtractMetadata(content);
        info.author = metadata.author;
        info.description = metadata.description;
        info.version = metadata.version;
        info.tags = metadata.tags;
    }

    m_scriptIndex[info.path] = info;

    // Add to watched files if watching enabled
    if (m_fileWatchingEnabled) {
        m_watchedFiles[info.path] = info.lastModified;
    }
}

} // namespace Scripting
} // namespace Nova
