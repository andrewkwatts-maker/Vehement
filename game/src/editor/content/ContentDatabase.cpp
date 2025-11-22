#include "ContentDatabase.hpp"
#include "../Editor.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <json/json.h>

namespace Vehement {
namespace Content {

ContentDatabase::ContentDatabase(Editor* editor)
    : m_editor(editor)
{
}

ContentDatabase::~ContentDatabase() {
    Shutdown();
}

bool ContentDatabase::Initialize(const ContentDatabaseConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Load cache if exists
    LoadCache();

    // Start initial scan
    ScanContent(true);

    // Start file watcher if enabled
    if (m_config.enableFileWatcher) {
        StartFileWatcher();
    }

    m_initialized = true;
    return true;
}

void ContentDatabase::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Stop file watcher
    StopFileWatcher();

    // Wait for scan to complete
    if (m_scanThread.joinable()) {
        m_scanning = false;
        m_scanThread.join();
    }

    // Save cache
    SaveCache();

    // Clear data
    {
        std::lock_guard<std::mutex> lock(m_assetsMutex);
        m_assets.clear();
    }

    m_typeIndex.clear();
    m_tagIndex.clear();
    m_directoryIndex.clear();
    m_searchIndex.clear();
    m_dependencyGraph.clear();
    m_dependentGraph.clear();

    m_initialized = false;
}

void ContentDatabase::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Process pending file changes
    std::vector<FileChangeEvent> changes;
    {
        std::lock_guard<std::mutex> lock(m_changesMutex);
        while (!m_pendingChanges.empty()) {
            changes.push_back(m_pendingChanges.front());
            m_pendingChanges.pop();
        }
    }

    for (const auto& change : changes) {
        ProcessFileChange(change);
        if (OnFileChanged) {
            OnFileChanged(change);
        }
    }
}

void ContentDatabase::ScanContent(bool async) {
    if (m_scanning.load()) {
        return;  // Already scanning
    }

    auto scanFunc = [this]() {
        m_scanning = true;
        m_scanProgress = 0.0f;

        // Count files first for progress
        size_t totalFiles = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_config.contentRoot)) {
            if (entry.is_regular_file()) {
                totalFiles++;
            }
        }

        size_t processedFiles = 0;

        // Clear existing data
        {
            std::lock_guard<std::mutex> lock(m_assetsMutex);
            m_assets.clear();
        }
        m_typeIndex.clear();
        m_tagIndex.clear();
        m_directoryIndex.clear();

        // Scan all subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(m_config.contentRoot)) {
            if (!m_scanning.load()) {
                break;  // Cancelled
            }

            if (entry.is_regular_file()) {
                ProcessFile(entry.path());
                processedFiles++;
                m_scanProgress = static_cast<float>(processedFiles) / static_cast<float>(totalFiles);
            }
        }

        // Build search index
        if (m_config.enableFullTextSearch) {
            BuildSearchIndex();
        }

        // Update dependency graph
        if (m_config.enableDependencyTracking) {
            UpdateDependencyGraph();
        }

        m_scanning = false;
        m_scanProgress = 1.0f;

        if (OnScanComplete) {
            OnScanComplete();
        }
    };

    if (async) {
        if (m_scanThread.joinable()) {
            m_scanThread.join();
        }
        m_scanThread = std::thread(scanFunc);
    } else {
        scanFunc();
    }
}

void ContentDatabase::RescanDirectory(const std::string& path) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            ProcessFile(entry.path());
        }
    }
}

void ContentDatabase::RescanAsset(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    auto it = m_assets.find(assetId);
    if (it != m_assets.end()) {
        auto newMetadata = ExtractMetadata(std::filesystem::path(it->second.filePath));
        it->second = newMetadata;

        if (m_config.enableFullTextSearch) {
            UpdateSearchIndex(assetId);
        }

        if (OnAssetModified) {
            OnAssetModified(assetId);
        }
    }
}

void ContentDatabase::ProcessFile(const std::filesystem::path& path) {
    // Only process JSON files
    if (path.extension() != ".json") {
        return;
    }

    auto metadata = ExtractMetadata(path);
    if (metadata.id.empty()) {
        return;
    }

    // Add to main storage
    {
        std::lock_guard<std::mutex> lock(m_assetsMutex);
        m_assets[metadata.id] = metadata;
    }

    // Update indices
    m_typeIndex[metadata.type].insert(metadata.id);

    for (const auto& tag : metadata.tags) {
        m_tagIndex[tag].insert(metadata.id);
    }

    std::string dir = path.parent_path().string();
    m_directoryIndex[dir].insert(metadata.id);

    // Store file timestamp for watcher
    m_fileTimestamps[path.string()] = std::filesystem::last_write_time(path);
}

AssetMetadata ContentDatabase::ExtractMetadata(const std::filesystem::path& path) {
    AssetMetadata metadata;

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return metadata;
        }

        // Read JSON with comments support
        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["collectComments"] = false;
        builder["allowComments"] = true;

        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            metadata.validationStatus = ValidationStatus::Error;
            metadata.validationMessage = errors;
            return metadata;
        }

        // Extract basic info
        metadata.id = root.get("id", "").asString();
        if (metadata.id.empty()) {
            metadata.id = GenerateAssetId(path);
        }

        metadata.name = root.get("name", path.stem().string()).asString();
        metadata.description = root.get("description", "").asString();
        metadata.type = DetectAssetType(path);
        metadata.filePath = path.string();
        metadata.relativePath = std::filesystem::relative(path, m_config.contentRoot).string();

        // Extract tags
        if (root.isMember("tags") && root["tags"].isArray()) {
            for (const auto& tag : root["tags"]) {
                metadata.tags.push_back(tag.asString());
            }
        }

        // File info
        metadata.fileSize = std::filesystem::file_size(path);
        auto ftime = std::filesystem::last_write_time(path);
        metadata.modifiedTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        metadata.checksum = ComputeChecksum(path);

        // Build searchable text
        std::stringstream ss;
        ss << metadata.name << " " << metadata.description;
        for (const auto& tag : metadata.tags) {
            ss << " " << tag;
        }
        metadata.searchableText = ss.str();

        // Extract important properties for filtering
        if (root.isMember("combat")) {
            const auto& combat = root["combat"];
            if (combat.isMember("damage")) {
                metadata.properties["damage"] = std::to_string(combat["damage"].asInt());
            }
            if (combat.isMember("health")) {
                metadata.properties["health"] = std::to_string(combat["health"].asInt());
            }
        }

        if (root.isMember("faction")) {
            metadata.properties["faction"] = root["faction"].asString();
        }

        if (root.isMember("tier")) {
            metadata.properties["tier"] = std::to_string(root["tier"].asInt());
        }

        metadata.validationStatus = ValidationStatus::Valid;

    } catch (const std::exception& e) {
        metadata.validationStatus = ValidationStatus::Error;
        metadata.validationMessage = e.what();
    }

    return metadata;
}

AssetType ContentDatabase::DetectAssetType(const std::filesystem::path& path) {
    // Get parent directory name
    std::string parent = path.parent_path().filename().string();

    return StringToAssetType(parent);
}

std::string ContentDatabase::GenerateAssetId(const std::filesystem::path& path) {
    std::string stem = path.stem().string();
    std::string parent = path.parent_path().filename().string();
    return parent + "_" + stem;
}

std::string ContentDatabase::ComputeChecksum(const std::filesystem::path& path) {
    // Simple checksum based on file size and modification time
    auto size = std::filesystem::file_size(path);
    auto time = std::filesystem::last_write_time(path);
    auto timeCount = time.time_since_epoch().count();

    std::stringstream ss;
    ss << std::hex << size << "_" << timeCount;
    return ss.str();
}

std::vector<AssetMetadata> ContentDatabase::GetAllAssets() const {
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    std::vector<AssetMetadata> result;
    result.reserve(m_assets.size());

    for (const auto& [id, metadata] : m_assets) {
        result.push_back(metadata);
    }

    return result;
}

std::optional<AssetMetadata> ContentDatabase::GetAsset(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    auto it = m_assets.find(id);
    if (it != m_assets.end()) {
        m_cacheStats.cacheHits++;
        return it->second;
    }
    m_cacheStats.cacheMisses++;
    return std::nullopt;
}

std::vector<AssetMetadata> ContentDatabase::GetAssetsByType(AssetType type) const {
    std::vector<AssetMetadata> result;

    auto typeIt = m_typeIndex.find(type);
    if (typeIt == m_typeIndex.end()) {
        return result;
    }

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& id : typeIt->second) {
        auto it = m_assets.find(id);
        if (it != m_assets.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<AssetMetadata> ContentDatabase::GetAssetsByTag(const std::string& tag) const {
    std::vector<AssetMetadata> result;

    auto tagIt = m_tagIndex.find(tag);
    if (tagIt == m_tagIndex.end()) {
        return result;
    }

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& id : tagIt->second) {
        auto it = m_assets.find(id);
        if (it != m_assets.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<AssetMetadata> ContentDatabase::GetAssetsInDirectory(const std::string& path) const {
    std::vector<AssetMetadata> result;

    auto dirIt = m_directoryIndex.find(path);
    if (dirIt == m_directoryIndex.end()) {
        return result;
    }

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& id : dirIt->second) {
        auto it = m_assets.find(id);
        if (it != m_assets.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<AssetMetadata> ContentDatabase::GetFavorites() const {
    std::vector<AssetMetadata> result;

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& id : m_favorites) {
        auto it = m_assets.find(id);
        if (it != m_assets.end()) {
            result.push_back(it->second);
        }
    }

    return result;
}

std::vector<AssetMetadata> ContentDatabase::GetRecentAssets(size_t count) const {
    std::vector<AssetMetadata> allAssets = GetAllAssets();

    // Sort by modified time (newest first)
    std::sort(allAssets.begin(), allAssets.end(),
              [](const AssetMetadata& a, const AssetMetadata& b) {
                  return a.modifiedTime > b.modifiedTime;
              });

    if (allAssets.size() > count) {
        allAssets.resize(count);
    }

    return allAssets;
}

bool ContentDatabase::HasAsset(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    return m_assets.find(id) != m_assets.end();
}

size_t ContentDatabase::GetAssetCount() const {
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    return m_assets.size();
}

size_t ContentDatabase::GetAssetCountByType(AssetType type) const {
    auto it = m_typeIndex.find(type);
    return it != m_typeIndex.end() ? it->second.size() : 0;
}

// ============================================================================
// Full-Text Search
// ============================================================================

void ContentDatabase::BuildSearchIndex() {
    m_searchIndex.clear();
    m_termFrequency.clear();

    std::lock_guard<std::mutex> lock(m_assetsMutex);

    for (const auto& [id, metadata] : m_assets) {
        std::vector<std::string> tokens = TokenizeQuery(metadata.searchableText);

        std::unordered_map<std::string, int> termCounts;
        for (const auto& token : tokens) {
            m_searchIndex[token].insert(id);
            termCounts[token]++;
        }

        // Calculate term frequency
        for (const auto& [term, count] : termCounts) {
            m_termFrequency[id][term] = static_cast<float>(count) / static_cast<float>(tokens.size());
        }
    }
}

void ContentDatabase::UpdateSearchIndex(const std::string& assetId) {
    RemoveFromSearchIndex(assetId);

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return;
    }

    std::vector<std::string> tokens = TokenizeQuery(it->second.searchableText);

    std::unordered_map<std::string, int> termCounts;
    for (const auto& token : tokens) {
        m_searchIndex[token].insert(assetId);
        termCounts[token]++;
    }

    for (const auto& [term, count] : termCounts) {
        m_termFrequency[assetId][term] = static_cast<float>(count) / static_cast<float>(tokens.size());
    }
}

void ContentDatabase::RemoveFromSearchIndex(const std::string& assetId) {
    for (auto& [term, ids] : m_searchIndex) {
        ids.erase(assetId);
    }
    m_termFrequency.erase(assetId);
}

std::vector<std::string> ContentDatabase::TokenizeQuery(const std::string& query) const {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : query) {
        if (std::isalnum(c)) {
            current += std::tolower(c);
        } else if (!current.empty()) {
            if (current.length() >= 2) {  // Skip single-character tokens
                tokens.push_back(current);
            }
            current.clear();
        }
    }

    if (!current.empty() && current.length() >= 2) {
        tokens.push_back(current);
    }

    return tokens;
}

float ContentDatabase::ComputeRelevance(const AssetMetadata& asset,
                                         const std::vector<std::string>& tokens) const {
    float score = 0.0f;
    int matchedTokens = 0;

    auto tfIt = m_termFrequency.find(asset.id);
    if (tfIt == m_termFrequency.end()) {
        return 0.0f;
    }

    for (const auto& token : tokens) {
        auto termIt = tfIt->second.find(token);
        if (termIt != tfIt->second.end()) {
            // TF-IDF scoring
            float tf = termIt->second;
            float idf = std::log(static_cast<float>(m_assets.size()) /
                                (static_cast<float>(m_searchIndex.at(token).size()) + 1.0f));
            score += tf * idf;
            matchedTokens++;
        }

        // Bonus for exact name match
        std::string lowerName = asset.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        if (lowerName.find(token) != std::string::npos) {
            score += 2.0f;
        }
    }

    // Bonus for matching all tokens
    if (matchedTokens == static_cast<int>(tokens.size())) {
        score *= 1.5f;
    }

    return score;
}

std::vector<SearchResult> ContentDatabase::Search(const std::string& query, size_t maxResults) const {
    std::vector<SearchResult> results;

    if (query.empty()) {
        return results;
    }

    std::vector<std::string> tokens = TokenizeQuery(query);
    if (tokens.empty()) {
        return results;
    }

    // Find candidate assets
    std::unordered_set<std::string> candidates;
    for (const auto& token : tokens) {
        auto it = m_searchIndex.find(token);
        if (it != m_searchIndex.end()) {
            for (const auto& id : it->second) {
                candidates.insert(id);
            }
        }
    }

    // Score candidates
    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& id : candidates) {
        auto it = m_assets.find(id);
        if (it == m_assets.end()) {
            continue;
        }

        float score = ComputeRelevance(it->second, tokens);
        if (score > 0.0f) {
            SearchResult result;
            result.assetId = id;
            result.relevanceScore = score;
            result.matchedTerms = tokens;
            results.push_back(result);
        }
    }

    // Sort by relevance
    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.relevanceScore > b.relevanceScore;
              });

    // Limit results
    if (results.size() > maxResults) {
        results.resize(maxResults);
    }

    return results;
}

std::vector<SearchResult> ContentDatabase::SearchByType(const std::string& query, AssetType type,
                                                         size_t maxResults) const {
    auto results = Search(query, maxResults * 2);

    // Filter by type
    results.erase(
        std::remove_if(results.begin(), results.end(),
                       [this, type](const SearchResult& r) {
                           auto asset = GetAsset(r.assetId);
                           return !asset || asset->type != type;
                       }),
        results.end());

    if (results.size() > maxResults) {
        results.resize(maxResults);
    }

    return results;
}

std::vector<std::string> ContentDatabase::GetSearchSuggestions(const std::string& prefix,
                                                                size_t maxSuggestions) const {
    std::vector<std::string> suggestions;

    if (prefix.empty()) {
        return suggestions;
    }

    std::string lowerPrefix = prefix;
    std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

    // Search in indexed terms
    for (const auto& [term, ids] : m_searchIndex) {
        if (term.find(lowerPrefix) == 0) {
            suggestions.push_back(term);
            if (suggestions.size() >= maxSuggestions) {
                break;
            }
        }
    }

    return suggestions;
}

// ============================================================================
// Tags
// ============================================================================

std::vector<std::string> ContentDatabase::GetAllTags() const {
    std::vector<std::string> tags;
    tags.reserve(m_tagIndex.size());

    for (const auto& [tag, ids] : m_tagIndex) {
        tags.push_back(tag);
    }

    std::sort(tags.begin(), tags.end());
    return tags;
}

std::unordered_map<std::string, size_t> ContentDatabase::GetTagCounts() const {
    std::unordered_map<std::string, size_t> counts;

    for (const auto& [tag, ids] : m_tagIndex) {
        counts[tag] = ids.size();
    }

    return counts;
}

bool ContentDatabase::AddTag(const std::string& assetId, const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return false;
    }

    // Check if tag already exists
    auto& tags = it->second.tags;
    if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
        return true;  // Already has tag
    }

    tags.push_back(tag);
    m_tagIndex[tag].insert(assetId);

    return true;
}

bool ContentDatabase::RemoveTag(const std::string& assetId, const std::string& tag) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return false;
    }

    auto& tags = it->second.tags;
    auto tagIt = std::find(tags.begin(), tags.end(), tag);
    if (tagIt == tags.end()) {
        return false;
    }

    tags.erase(tagIt);
    m_tagIndex[tag].erase(assetId);

    return true;
}

bool ContentDatabase::SetTags(const std::string& assetId, const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return false;
    }

    // Remove from old tag indices
    for (const auto& oldTag : it->second.tags) {
        m_tagIndex[oldTag].erase(assetId);
    }

    // Set new tags
    it->second.tags = tags;

    // Add to new tag indices
    for (const auto& tag : tags) {
        m_tagIndex[tag].insert(assetId);
    }

    return true;
}

// ============================================================================
// Favorites
// ============================================================================

void ContentDatabase::ToggleFavorite(const std::string& assetId) {
    if (m_favorites.count(assetId)) {
        m_favorites.erase(assetId);
    } else {
        m_favorites.insert(assetId);
    }
}

void ContentDatabase::SetFavorite(const std::string& assetId, bool favorite) {
    if (favorite) {
        m_favorites.insert(assetId);
    } else {
        m_favorites.erase(assetId);
    }
}

bool ContentDatabase::IsFavorite(const std::string& assetId) const {
    return m_favorites.count(assetId) > 0;
}

// ============================================================================
// Dependencies
// ============================================================================

std::vector<std::string> ContentDatabase::GetDependencies(const std::string& assetId) const {
    auto it = m_dependencyGraph.find(assetId);
    if (it == m_dependencyGraph.end()) {
        return {};
    }

    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> ContentDatabase::GetDependents(const std::string& assetId) const {
    auto it = m_dependentGraph.find(assetId);
    if (it == m_dependentGraph.end()) {
        return {};
    }

    return std::vector<std::string>(it->second.begin(), it->second.end());
}

std::vector<std::string> ContentDatabase::GetDependencyTree(const std::string& assetId, bool upstream) const {
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    std::vector<std::string> stack;
    stack.push_back(assetId);

    while (!stack.empty()) {
        std::string current = stack.back();
        stack.pop_back();

        if (visited.count(current)) {
            continue;
        }
        visited.insert(current);

        if (current != assetId) {
            result.push_back(current);
        }

        const auto& graph = upstream ? m_dependencyGraph : m_dependentGraph;
        auto it = graph.find(current);
        if (it != graph.end()) {
            for (const auto& dep : it->second) {
                stack.push_back(dep);
            }
        }
    }

    return result;
}

bool ContentDatabase::HasCircularDependency(const std::string& assetId) const {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;

    std::function<bool(const std::string&)> hasCycle = [&](const std::string& id) -> bool {
        if (recursionStack.count(id)) {
            return true;  // Cycle detected
        }
        if (visited.count(id)) {
            return false;
        }

        visited.insert(id);
        recursionStack.insert(id);

        auto it = m_dependencyGraph.find(id);
        if (it != m_dependencyGraph.end()) {
            for (const auto& dep : it->second) {
                if (hasCycle(dep)) {
                    return true;
                }
            }
        }

        recursionStack.erase(id);
        return false;
    };

    return hasCycle(assetId);
}

void ContentDatabase::ExtractDependencies(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return;
    }

    // Clear existing dependencies
    m_dependencyGraph[assetId].clear();

    try {
        std::ifstream file(it->second.filePath);
        if (!file.is_open()) {
            return;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["allowComments"] = true;
        std::string errors;
        Json::parseFromStream(builder, file, &root, &errors);

        // Extract references from common fields
        std::function<void(const Json::Value&)> extractRefs = [&](const Json::Value& value) {
            if (value.isString()) {
                std::string str = value.asString();
                // Check if it's a reference to another asset
                if (str.find("_") != std::string::npos &&
                    m_assets.count(str)) {
                    m_dependencyGraph[assetId].insert(str);
                    m_dependentGraph[str].insert(assetId);
                }
            } else if (value.isArray()) {
                for (const auto& item : value) {
                    extractRefs(item);
                }
            } else if (value.isObject()) {
                for (const auto& key : value.getMemberNames()) {
                    extractRefs(value[key]);
                }
            }
        };

        extractRefs(root);

    } catch (const std::exception&) {
        // Ignore errors
    }
}

void ContentDatabase::UpdateDependencyGraph() {
    m_dependencyGraph.clear();
    m_dependentGraph.clear();

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& [id, metadata] : m_assets) {
        ExtractDependencies(id);
    }
}

// ============================================================================
// Validation
// ============================================================================

ValidationStatus ContentDatabase::ValidateAsset(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return ValidationStatus::Error;
    }

    std::string message;

    // Validate JSON
    auto jsonStatus = ValidateJson(std::filesystem::path(it->second.filePath), message);
    if (jsonStatus != ValidationStatus::Valid) {
        it->second.validationStatus = jsonStatus;
        it->second.validationMessage = message;
        return jsonStatus;
    }

    // Validate schema
    auto schemaStatus = ValidateSchema(std::filesystem::path(it->second.filePath),
                                       it->second.type, message);
    if (schemaStatus != ValidationStatus::Valid) {
        it->second.validationStatus = schemaStatus;
        it->second.validationMessage = message;
        return schemaStatus;
    }

    // Validate references
    auto refStatus = ValidateReferences(assetId, message);
    it->second.validationStatus = refStatus;
    it->second.validationMessage = message;

    return refStatus;
}

void ContentDatabase::ValidateAll() {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    for (auto& [id, metadata] : m_assets) {
        std::string message;

        auto jsonStatus = ValidateJson(std::filesystem::path(metadata.filePath), message);
        if (jsonStatus != ValidationStatus::Valid) {
            metadata.validationStatus = jsonStatus;
            metadata.validationMessage = message;
            continue;
        }

        auto schemaStatus = ValidateSchema(std::filesystem::path(metadata.filePath),
                                           metadata.type, message);
        if (schemaStatus != ValidationStatus::Valid) {
            metadata.validationStatus = schemaStatus;
            metadata.validationMessage = message;
            continue;
        }

        auto refStatus = ValidateReferences(id, message);
        metadata.validationStatus = refStatus;
        metadata.validationMessage = message;
    }
}

std::vector<AssetMetadata> ContentDatabase::GetInvalidAssets() const {
    std::vector<AssetMetadata> result;

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& [id, metadata] : m_assets) {
        if (metadata.validationStatus == ValidationStatus::Error) {
            result.push_back(metadata);
        }
    }

    return result;
}

std::vector<AssetMetadata> ContentDatabase::GetAssetsWithWarnings() const {
    std::vector<AssetMetadata> result;

    std::lock_guard<std::mutex> lock(m_assetsMutex);
    for (const auto& [id, metadata] : m_assets) {
        if (metadata.validationStatus == ValidationStatus::Warning) {
            result.push_back(metadata);
        }
    }

    return result;
}

ValidationStatus ContentDatabase::ValidateJson(const std::filesystem::path& path, std::string& message) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            message = "Cannot open file";
            return ValidationStatus::Error;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["allowComments"] = true;
        std::string errors;

        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            message = "JSON parse error: " + errors;
            return ValidationStatus::Error;
        }

        return ValidationStatus::Valid;
    } catch (const std::exception& e) {
        message = e.what();
        return ValidationStatus::Error;
    }
}

ValidationStatus ContentDatabase::ValidateSchema(const std::filesystem::path& path,
                                                  AssetType type, std::string& message) {
    // Schema validation would go here
    // For now, just do basic field checking
    try {
        std::ifstream file(path);
        Json::Value root;
        Json::CharReaderBuilder builder;
        builder["allowComments"] = true;
        std::string errors;
        Json::parseFromStream(builder, file, &root, &errors);

        // Check for required fields based on type
        if (!root.isMember("id") && !root.isMember("name")) {
            message = "Missing required field: id or name";
            return ValidationStatus::Warning;
        }

        return ValidationStatus::Valid;
    } catch (const std::exception& e) {
        message = e.what();
        return ValidationStatus::Error;
    }
}

ValidationStatus ContentDatabase::ValidateReferences(const std::string& assetId, std::string& message) {
    auto deps = GetDependencies(assetId);

    for (const auto& dep : deps) {
        if (!HasAsset(dep)) {
            message = "Broken reference: " + dep;
            return ValidationStatus::Error;
        }
    }

    if (HasCircularDependency(assetId)) {
        message = "Circular dependency detected";
        return ValidationStatus::Warning;
    }

    return ValidationStatus::Valid;
}

// ============================================================================
// File Watching
// ============================================================================

void ContentDatabase::SetFileWatcherEnabled(bool enabled) {
    if (enabled && !m_fileWatcherRunning) {
        StartFileWatcher();
    } else if (!enabled && m_fileWatcherRunning) {
        StopFileWatcher();
    }
    m_fileWatcherEnabled = enabled;
}

void ContentDatabase::StartFileWatcher() {
    if (m_fileWatcherRunning) {
        return;
    }

    m_fileWatcherRunning = true;
    m_fileWatcherThread = std::thread(&ContentDatabase::FileWatcherThread, this);
}

void ContentDatabase::StopFileWatcher() {
    m_fileWatcherRunning = false;
    if (m_fileWatcherThread.joinable()) {
        m_fileWatcherThread.join();
    }
}

void ContentDatabase::FileWatcherThread() {
    while (m_fileWatcherRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_config.scanIntervalMs));

        if (!m_fileWatcherRunning) {
            break;
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_config.contentRoot)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                    continue;
                }

                std::string pathStr = entry.path().string();
                auto currentTime = std::filesystem::last_write_time(entry.path());

                auto it = m_fileTimestamps.find(pathStr);
                if (it == m_fileTimestamps.end()) {
                    // New file
                    FileChangeEvent event;
                    event.type = FileChangeEvent::Type::Created;
                    event.path = pathStr;
                    event.timestamp = std::chrono::system_clock::now();

                    std::lock_guard<std::mutex> lock(m_changesMutex);
                    m_pendingChanges.push(event);
                    m_fileTimestamps[pathStr] = currentTime;
                } else if (it->second != currentTime) {
                    // Modified file
                    FileChangeEvent event;
                    event.type = FileChangeEvent::Type::Modified;
                    event.path = pathStr;
                    event.timestamp = std::chrono::system_clock::now();

                    std::lock_guard<std::mutex> lock(m_changesMutex);
                    m_pendingChanges.push(event);
                    m_fileTimestamps[pathStr] = currentTime;
                }
            }

            // Check for deleted files
            std::vector<std::string> deletedPaths;
            for (const auto& [path, time] : m_fileTimestamps) {
                if (!std::filesystem::exists(path)) {
                    deletedPaths.push_back(path);
                }
            }

            for (const auto& path : deletedPaths) {
                FileChangeEvent event;
                event.type = FileChangeEvent::Type::Deleted;
                event.path = path;
                event.timestamp = std::chrono::system_clock::now();

                std::lock_guard<std::mutex> lock(m_changesMutex);
                m_pendingChanges.push(event);
                m_fileTimestamps.erase(path);
            }

        } catch (const std::exception&) {
            // Ignore filesystem errors
        }
    }
}

void ContentDatabase::ProcessFileChange(const FileChangeEvent& event) {
    switch (event.type) {
        case FileChangeEvent::Type::Created:
            ProcessFile(std::filesystem::path(event.path));
            if (OnAssetAdded) {
                // Find the asset ID
                std::string assetId = GenerateAssetId(std::filesystem::path(event.path));
                OnAssetAdded(assetId);
            }
            break;

        case FileChangeEvent::Type::Modified: {
            std::string assetId = GenerateAssetId(std::filesystem::path(event.path));
            RescanAsset(assetId);
            break;
        }

        case FileChangeEvent::Type::Deleted: {
            std::string assetId = GenerateAssetId(std::filesystem::path(event.path));
            {
                std::lock_guard<std::mutex> lock(m_assetsMutex);
                auto it = m_assets.find(assetId);
                if (it != m_assets.end()) {
                    // Remove from indices
                    m_typeIndex[it->second.type].erase(assetId);
                    for (const auto& tag : it->second.tags) {
                        m_tagIndex[tag].erase(assetId);
                    }
                    std::string dir = std::filesystem::path(it->second.filePath).parent_path().string();
                    m_directoryIndex[dir].erase(assetId);

                    // Remove from search index
                    RemoveFromSearchIndex(assetId);

                    // Remove from dependency graph
                    m_dependencyGraph.erase(assetId);
                    m_dependentGraph.erase(assetId);

                    // Remove from assets
                    m_assets.erase(it);
                }
            }
            if (OnAssetRemoved) {
                OnAssetRemoved(assetId);
            }
            break;
        }

        default:
            break;
    }
}

std::vector<FileChangeEvent> ContentDatabase::GetPendingChanges() {
    std::lock_guard<std::mutex> lock(m_changesMutex);
    std::vector<FileChangeEvent> changes;

    while (!m_pendingChanges.empty()) {
        changes.push_back(m_pendingChanges.front());
        m_pendingChanges.pop();
    }

    return changes;
}

// ============================================================================
// Metadata Management
// ============================================================================

bool ContentDatabase::UpdateMetadata(const std::string& assetId, const AssetMetadata& metadata) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return false;
    }

    it->second = metadata;
    return true;
}

bool ContentDatabase::SetCustomData(const std::string& assetId, const std::string& key,
                                     const std::string& value) {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return false;
    }

    it->second.customData[key] = value;
    return true;
}

std::string ContentDatabase::GetCustomData(const std::string& assetId, const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_assetsMutex);

    auto it = m_assets.find(assetId);
    if (it == m_assets.end()) {
        return "";
    }

    auto dataIt = it->second.customData.find(key);
    return dataIt != it->second.customData.end() ? dataIt->second : "";
}

// ============================================================================
// Cache
// ============================================================================

bool ContentDatabase::SaveCache() {
    try {
        std::filesystem::create_directories(m_config.cacheDirectory);
        std::string cachePath = m_config.cacheDirectory + "/database_cache.json";

        Json::Value root;
        root["version"] = 1;

        Json::Value assets(Json::arrayValue);
        {
            std::lock_guard<std::mutex> lock(m_assetsMutex);
            for (const auto& [id, metadata] : m_assets) {
                Json::Value asset;
                asset["id"] = metadata.id;
                asset["name"] = metadata.name;
                asset["type"] = AssetTypeToString(metadata.type);
                asset["filePath"] = metadata.filePath;
                asset["checksum"] = metadata.checksum;

                Json::Value tags(Json::arrayValue);
                for (const auto& tag : metadata.tags) {
                    tags.append(tag);
                }
                asset["tags"] = tags;

                assets.append(asset);
            }
        }
        root["assets"] = assets;

        // Save favorites
        Json::Value favorites(Json::arrayValue);
        for (const auto& id : m_favorites) {
            favorites.append(id);
        }
        root["favorites"] = favorites;

        std::ofstream file(cachePath);
        file << root;

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ContentDatabase::LoadCache() {
    try {
        std::string cachePath = m_config.cacheDirectory + "/database_cache.json";

        if (!std::filesystem::exists(cachePath)) {
            return false;
        }

        std::ifstream file(cachePath);
        Json::Value root;
        file >> root;

        // Load favorites
        if (root.isMember("favorites")) {
            for (const auto& id : root["favorites"]) {
                m_favorites.insert(id.asString());
            }
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void ContentDatabase::ClearCache() {
    try {
        std::filesystem::remove_all(m_config.cacheDirectory);
    } catch (const std::exception&) {
        // Ignore
    }
}

ContentDatabase::CacheStats ContentDatabase::GetCacheStats() const {
    m_cacheStats.totalAssets = GetAssetCount();
    m_cacheStats.cachedAssets = m_cacheStats.totalAssets;
    return m_cacheStats;
}

} // namespace Content
} // namespace Vehement
