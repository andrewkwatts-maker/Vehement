#include "AssetProcessor.hpp"
#include "TextureImporter.hpp"
#include "ModelImporter.hpp"
#include "AnimationImporter.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <chrono>

namespace fs = std::filesystem;

namespace Nova {

// ============================================================================
// Constructor/Destructor
// ============================================================================

AssetProcessor::AssetProcessor()
    : m_textureImporter(std::make_unique<TextureImporter>())
    , m_modelImporter(std::make_unique<ModelImporter>())
    , m_animationImporter(std::make_unique<AnimationImporter>()) {
}

AssetProcessor::~AssetProcessor() {
    Shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

bool AssetProcessor::Initialize(const std::string& projectRoot, const std::string& cacheDirectory) {
    m_projectRoot = projectRoot;
    m_cacheDirectory = cacheDirectory;
    m_outputDirectory = projectRoot + "/Build/Assets";

    // Create directories
    fs::create_directories(cacheDirectory);
    fs::create_directories(m_outputDirectory);

    // Load cache
    LoadCache();

    m_initialized = true;
    return true;
}

void AssetProcessor::Shutdown() {
    StopWorkers();
    SaveCache();
    m_initialized = false;
}

// ============================================================================
// Asset Processing
// ============================================================================

bool AssetProcessor::ProcessAsset(const std::string& assetPath, ImportProgress* progress) {
    if (!m_initialized) return false;

    // Check cache
    if (!NeedsProcessing(assetPath)) {
        if (progress) {
            progress->Info("Asset up to date, skipping");
            progress->SetStatus(ImportStatus::Completed);
        }
        return true;
    }

    return ProcessAssetInternal(assetPath, progress);
}

bool AssetProcessor::ProcessAsset(const std::string& assetPath, const ImportSettingsBase& settings,
                                   ImportProgress* progress) {
    if (!m_initialized) return false;
    return ProcessAssetInternal(assetPath, progress);
}

bool AssetProcessor::ProcessAssetInternal(const std::string& assetPath, ImportProgress* progress) {
    std::string assetType = GetAssetType(assetPath);

    auto startTime = std::chrono::steady_clock::now();
    bool success = false;
    std::vector<AssetDependency> dependencies;

    if (assetType == "Texture") {
        TextureImportSettings settings;
        settings.AutoDetectType(assetPath);

        auto result = m_textureImporter->Import(assetPath, settings, progress);
        success = result.success;

        if (success) {
            // Save output
            std::string outputPath = GetOutputPath(assetPath);
            fs::create_directories(fs::path(outputPath).parent_path());
            m_textureImporter->SaveEngineFormat(result, outputPath);
        }
    }
    else if (assetType == "Model") {
        ModelImportSettings settings;

        auto result = m_modelImporter->Import(assetPath, settings, progress);
        success = result.success;

        if (success) {
            // Track texture dependencies
            for (const auto& material : result.materials) {
                for (const auto& texture : material.textures) {
                    if (!texture.embedded) {
                        AssetDependency dep;
                        dep.assetPath = texture.path;
                        dep.dependencyType = "texture";
                        dep.required = true;
                        dependencies.push_back(dep);
                    }
                }
            }

            std::string outputPath = GetOutputPath(assetPath);
            fs::create_directories(fs::path(outputPath).parent_path());
            m_modelImporter->SaveEngineFormat(result, outputPath);
        }
    }
    else if (assetType == "Animation") {
        AnimationImportSettings settings;

        auto result = m_animationImporter->Import(assetPath, settings, progress);
        success = result.success;

        if (success) {
            std::string outputPath = GetOutputPath(assetPath);
            fs::create_directories(fs::path(outputPath).parent_path());
            m_animationImporter->SaveEngineFormat(result, outputPath);
        }
    }
    else {
        if (progress) progress->Error("Unknown asset type: " + assetType);
        return false;
    }

    // Update cache
    if (success) {
        AssetCacheEntry entry;
        entry.sourcePath = assetPath;
        entry.outputPath = GetOutputPath(assetPath);
        entry.assetType = assetType;
        entry.sourceHash = CalculateFileHash(assetPath);
        entry.importTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        entry.dependencies = dependencies;
        entry.valid = true;

        UpdateCacheEntry(assetPath, entry);
        UpdateDependencies(assetPath, dependencies);
    }

    // Callbacks
    if (m_assetProcessedCallback) {
        m_assetProcessedCallback(assetPath, success);
    }

    if (!success && m_errorCallback) {
        m_errorCallback(assetPath, "Import failed");
    }

    return success;
}

CookingResult AssetProcessor::ProcessAssets(const std::vector<std::string>& assetPaths,
                                             ImportProgressTracker* tracker) {
    CookingResult result;
    result.totalAssets = assetPaths.size();

    auto startTime = std::chrono::steady_clock::now();

    // Get processing order based on dependencies
    std::vector<std::string> orderedAssets = GetProcessingOrder(assetPaths);

    for (const auto& path : orderedAssets) {
        ImportProgress* progress = tracker ? tracker->AddImport(path) : nullptr;

        if (!NeedsProcessing(path)) {
            result.skippedAssets++;
            if (progress) {
                progress->SetStatus(ImportStatus::Completed);
            }
            continue;
        }

        if (ProcessAssetInternal(path, progress)) {
            result.processedAssets++;
            if (fs::exists(path)) {
                result.totalInputSize += fs::file_size(path);
            }
            std::string outputPath = GetOutputPath(path);
            if (fs::exists(outputPath)) {
                result.totalOutputSize += fs::file_size(outputPath);
            }
        } else {
            result.failedAssets++;
            result.errors.push_back("Failed to process: " + path);
        }

        if (m_progressCallback) {
            float progress = static_cast<float>(result.processedAssets + result.skippedAssets + result.failedAssets)
                           / result.totalAssets;
            m_progressCallback(progress, path);
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    result.totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    return result;
}

CookingResult AssetProcessor::ProcessDirectory(const std::string& directory, bool recursive,
                                                ImportProgressTracker* tracker) {
    std::vector<std::string> assets = ScanForAssets(directory, recursive);
    return ProcessAssets(assets, tracker);
}

void AssetProcessor::QueueAsset(const std::string& assetPath, int priority,
                                 std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(m_queueMutex);

    ProcessingJob job;
    job.assetPath = assetPath;
    job.assetType = GetAssetType(assetPath);
    job.priority = priority;
    job.callback = callback;

    m_jobQueue.push(std::move(job));
    m_queueCondition.notify_one();
}

CookingResult AssetProcessor::ProcessQueue(ImportProgressTracker* tracker) {
    CookingResult result;

    std::vector<ProcessingJob> jobs;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_jobQueue.empty()) {
            jobs.push_back(std::move(const_cast<ProcessingJob&>(m_jobQueue.top())));
            m_jobQueue.pop();
        }
    }

    result.totalAssets = jobs.size();

    for (auto& job : jobs) {
        ImportProgress* progress = tracker ? tracker->AddImport(job.assetPath) : nullptr;

        bool success = ProcessAssetInternal(job.assetPath, progress);

        if (success) {
            result.processedAssets++;
        } else {
            result.failedAssets++;
        }

        if (job.callback) {
            job.callback(success);
        }
    }

    return result;
}

// ============================================================================
// Platform Cooking
// ============================================================================

CookingResult AssetProcessor::CookForPlatform(const CookingSettings& settings,
                                               ImportProgressTracker* tracker) {
    std::vector<std::string> allAssets = ScanForAssets(m_projectRoot, true);
    return CookAssetsForPlatform(allAssets, settings, tracker);
}

CookingResult AssetProcessor::CookAssetsForPlatform(const std::vector<std::string>& assets,
                                                     const CookingSettings& settings,
                                                     ImportProgressTracker* tracker) {
    CookingResult result;
    result.totalAssets = assets.size();

    auto startTime = std::chrono::steady_clock::now();

    // Setup platform-specific output directory
    std::string platformOutput = settings.outputDirectory;
    if (platformOutput.empty()) {
        platformOutput = m_outputDirectory + "/" + std::to_string(static_cast<int>(settings.platform));
    }
    fs::create_directories(platformOutput);

    // Process assets with platform-specific settings
    for (const auto& assetPath : assets) {
        ImportProgress* progress = tracker ? tracker->AddImport(assetPath) : nullptr;

        // Check if needs cooking
        if (settings.incrementalBuild && !NeedsProcessing(assetPath)) {
            result.skippedAssets++;
            if (progress) progress->SetStatus(ImportStatus::Completed);
            continue;
        }

        // Get platform-specific settings
        std::string assetType = GetAssetType(assetPath);
        bool success = false;

        if (assetType == "Texture") {
            TextureImportSettings texSettings;
            texSettings.AutoDetectType(assetPath);
            texSettings.targetPlatform = settings.platform;
            texSettings.ApplyPreset(settings.platform == TargetPlatform::Mobile ?
                                    ImportPreset::Mobile : ImportPreset::Desktop);

            auto imported = m_textureImporter->Import(assetPath, texSettings, progress);
            success = imported.success;

            if (success) {
                std::string outputPath = platformOutput + "/" +
                    fs::path(assetPath).stem().string() + ".ntex";
                m_textureImporter->SaveEngineFormat(imported, outputPath);
            }
        }
        else if (assetType == "Model") {
            ModelImportSettings modelSettings;
            modelSettings.targetPlatform = settings.platform;
            modelSettings.ApplyPreset(settings.platform == TargetPlatform::Mobile ?
                                      ImportPreset::Mobile : ImportPreset::Desktop);

            auto imported = m_modelImporter->Import(assetPath, modelSettings, progress);
            success = imported.success;

            if (success) {
                std::string outputPath = platformOutput + "/" +
                    fs::path(assetPath).stem().string() + ".nmdl";
                m_modelImporter->SaveEngineFormat(imported, outputPath);
            }
        }
        else if (assetType == "Animation") {
            AnimationImportSettings animSettings;
            animSettings.targetPlatform = settings.platform;
            animSettings.ApplyPreset(settings.platform == TargetPlatform::Mobile ?
                                     ImportPreset::Mobile : ImportPreset::Desktop);

            auto imported = m_animationImporter->Import(assetPath, animSettings, progress);
            success = imported.success;

            if (success) {
                std::string outputPath = platformOutput + "/" +
                    fs::path(assetPath).stem().string() + ".nanm";
                m_animationImporter->SaveEngineFormat(imported, outputPath);
            }
        }

        if (success) {
            result.processedAssets++;
        } else {
            result.failedAssets++;
            result.errors.push_back("Failed to cook: " + assetPath);
        }
    }

    // Generate manifest
    if (settings.generateManifest) {
        AssetManifest manifest;
        // Populate manifest...

        manifest.Save(platformOutput + "/manifest.json");
    }

    auto endTime = std::chrono::steady_clock::now();
    result.totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    return result;
}

std::string AssetProcessor::GetPlatformOutputPath(const std::string& assetPath,
                                                   TargetPlatform platform) const {
    std::string platformStr;
    switch (platform) {
        case TargetPlatform::Desktop: platformStr = "Desktop"; break;
        case TargetPlatform::Mobile: platformStr = "Mobile"; break;
        case TargetPlatform::WebGL: platformStr = "WebGL"; break;
        case TargetPlatform::Console: platformStr = "Console"; break;
    }

    return m_outputDirectory + "/" + platformStr + "/" +
           fs::path(assetPath).stem().string() + ".nova";
}

// ============================================================================
// Dependency Tracking
// ============================================================================

std::vector<AssetDependency> AssetProcessor::GetDependencies(const std::string& assetPath) const {
    std::lock_guard<std::mutex> lock(m_depMutex);

    auto it = m_dependencies.find(assetPath);
    if (it != m_dependencies.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> AssetProcessor::GetDependents(const std::string& assetPath) const {
    std::lock_guard<std::mutex> lock(m_depMutex);

    auto it = m_dependents.find(assetPath);
    if (it != m_dependents.end()) {
        return std::vector<std::string>(it->second.begin(), it->second.end());
    }
    return {};
}

bool AssetProcessor::NeedsProcessing(const std::string& assetPath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(assetPath);
    if (it == m_cache.end()) {
        return true;  // Not in cache
    }

    const AssetCacheEntry& entry = it->second;
    if (!entry.valid) {
        return true;
    }

    // Check if source file changed
    if (!fs::exists(assetPath)) {
        return true;
    }

    uint64_t currentHash = CalculateFileHash(assetPath);
    if (currentHash != entry.sourceHash) {
        return true;
    }

    // Check if output exists
    if (!fs::exists(entry.outputPath)) {
        return true;
    }

    // Check dependencies
    for (const auto& dep : entry.dependencies) {
        if (!fs::exists(dep.assetPath)) {
            if (dep.required) return true;
            continue;
        }

        uint64_t depHash = CalculateFileHash(dep.assetPath);
        if (depHash != dep.fileHash) {
            return true;
        }
    }

    return false;
}

std::vector<std::string> AssetProcessor::GetOutdatedAssets() const {
    std::vector<std::string> outdated;

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    for (const auto& [path, entry] : m_cache) {
        if (NeedsProcessing(path)) {
            outdated.push_back(path);
        }
    }

    return outdated;
}

void AssetProcessor::RebuildDependencyGraph() {
    std::lock_guard<std::mutex> lock(m_depMutex);

    m_dependencies.clear();
    m_dependents.clear();

    // Rebuild from cache
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
    for (const auto& [path, entry] : m_cache) {
        m_dependencies[path] = entry.dependencies;

        for (const auto& dep : entry.dependencies) {
            m_dependents[dep.assetPath].insert(path);
        }
    }
}

void AssetProcessor::UpdateDependencies(const std::string& assetPath,
                                         const std::vector<AssetDependency>& deps) {
    std::lock_guard<std::mutex> lock(m_depMutex);

    // Remove old dependencies
    auto oldIt = m_dependencies.find(assetPath);
    if (oldIt != m_dependencies.end()) {
        for (const auto& oldDep : oldIt->second) {
            auto& dependents = m_dependents[oldDep.assetPath];
            dependents.erase(assetPath);
        }
    }

    // Add new dependencies
    m_dependencies[assetPath] = deps;
    for (const auto& dep : deps) {
        m_dependents[dep.assetPath].insert(assetPath);
    }
}

std::vector<std::string> AssetProcessor::GetProcessingOrder(const std::vector<std::string>& assets) {
    // Topological sort based on dependencies
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adjList;

    // Initialize
    for (const auto& asset : assets) {
        inDegree[asset] = 0;
    }

    // Build graph
    for (const auto& asset : assets) {
        auto deps = GetDependencies(asset);
        for (const auto& dep : deps) {
            if (inDegree.count(dep.assetPath)) {
                adjList[dep.assetPath].push_back(asset);
                inDegree[asset]++;
            }
        }
    }

    // Kahn's algorithm
    std::vector<std::string> result;
    std::queue<std::string> queue;

    for (const auto& [asset, degree] : inDegree) {
        if (degree == 0) {
            queue.push(asset);
        }
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);

        for (const auto& neighbor : adjList[current]) {
            if (--inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    // Add any remaining assets (circular dependencies)
    for (const auto& asset : assets) {
        if (std::find(result.begin(), result.end(), asset) == result.end()) {
            result.push_back(asset);
        }
    }

    return result;
}

// ============================================================================
// Cache Management
// ============================================================================

const AssetCacheEntry* AssetProcessor::GetCacheEntry(const std::string& assetPath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(assetPath);
    if (it != m_cache.end()) {
        return &it->second;
    }
    return nullptr;
}

void AssetProcessor::UpdateCacheEntry(const std::string& assetPath, const AssetCacheEntry& entry) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache[assetPath] = entry;
}

void AssetProcessor::InvalidateCache(const std::string& assetPath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(assetPath);
    if (it != m_cache.end()) {
        it->second.valid = false;
    }

    // Invalidate dependents
    auto dependents = GetDependents(assetPath);
    for (const auto& dep : dependents) {
        auto depIt = m_cache.find(dep);
        if (depIt != m_cache.end()) {
            depIt->second.valid = false;
        }
    }
}

void AssetProcessor::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
}

bool AssetProcessor::SaveCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::string cachePath = m_cacheDirectory + "/asset_cache.json";
    std::ofstream file(cachePath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"entries\": [\n";

    bool first = true;
    for (const auto& [path, entry] : m_cache) {
        if (!first) file << ",\n";
        first = false;

        file << "    {\n";
        file << "      \"sourcePath\": \"" << entry.sourcePath << "\",\n";
        file << "      \"outputPath\": \"" << entry.outputPath << "\",\n";
        file << "      \"assetType\": \"" << entry.assetType << "\",\n";
        file << "      \"sourceHash\": " << entry.sourceHash << ",\n";
        file << "      \"importTime\": " << entry.importTime << ",\n";
        file << "      \"valid\": " << (entry.valid ? "true" : "false") << "\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";

    return true;
}

bool AssetProcessor::LoadCache() {
    std::string cachePath = m_cacheDirectory + "/asset_cache.json";

    std::ifstream file(cachePath);
    if (!file.is_open()) return false;

    // Simplified JSON parsing
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    // Parse entries... (simplified)
    return true;
}

AssetProcessor::CacheStats AssetProcessor::GetCacheStats() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    CacheStats stats;
    stats.totalEntries = m_cache.size();

    for (const auto& [path, entry] : m_cache) {
        if (entry.valid) {
            stats.validEntries++;
        } else {
            stats.invalidEntries++;
        }
    }

    return stats;
}

void AssetProcessor::PruneCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::vector<std::string> toRemove;
    for (const auto& [path, entry] : m_cache) {
        if (!entry.valid || !fs::exists(path)) {
            toRemove.push_back(path);
        }
    }

    for (const auto& path : toRemove) {
        m_cache.erase(path);
    }
}

// ============================================================================
// Asset Discovery
// ============================================================================

std::vector<std::string> AssetProcessor::ScanForAssets(const std::string& directory, bool recursive) {
    std::vector<std::string> assets;

    if (!fs::exists(directory)) {
        return assets;
    }

    auto processDir = [&](const fs::path& dir) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                if (IsImportableAsset(path)) {
                    assets.push_back(path);
                }
            }
        }
    };

    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                if (IsImportableAsset(path)) {
                    assets.push_back(path);
                }
            }
        }
    } else {
        processDir(directory);
    }

    return assets;
}

std::string AssetProcessor::GetAssetType(const std::string& assetPath) const {
    return ImportSettingsManager::DetectAssetType(assetPath);
}

bool AssetProcessor::IsImportableAsset(const std::string& path) const {
    std::string type = GetAssetType(path);
    return type != "Unknown";
}

// ============================================================================
// Parallel Processing
// ============================================================================

void AssetProcessor::SetWorkerCount(int count) {
    if (m_workersRunning) {
        StopWorkers();
        m_workerCount = count;
        StartWorkers();
    } else {
        m_workerCount = count;
    }
}

void AssetProcessor::StartWorkers() {
    if (m_workersRunning) return;

    m_shutdownRequested = false;
    m_workersRunning = true;

    for (int i = 0; i < m_workerCount; ++i) {
        m_workers.emplace_back(&AssetProcessor::WorkerThread, this);
    }
}

void AssetProcessor::StopWorkers() {
    if (!m_workersRunning) return;

    m_shutdownRequested = true;
    m_queueCondition.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    m_workers.clear();
    m_workersRunning = false;
}

void AssetProcessor::WorkerThread() {
    while (!m_shutdownRequested) {
        ProcessingJob job;
        bool hasJob = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return m_shutdownRequested || !m_jobQueue.empty();
            });

            if (m_shutdownRequested && m_jobQueue.empty()) {
                break;
            }

            if (!m_jobQueue.empty()) {
                job = std::move(const_cast<ProcessingJob&>(m_jobQueue.top()));
                m_jobQueue.pop();
                hasJob = true;
            }
        }

        if (hasJob) {
            bool success = ProcessAssetInternal(job.assetPath, nullptr);

            if (job.callback) {
                job.callback(success);
            }
        }
    }
}

// ============================================================================
// Utilities
// ============================================================================

uint64_t AssetProcessor::CalculateFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return 0;

    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    char buffer[4096];

    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= 1099511628211ULL;
        }
    }

    return hash;
}

uint64_t AssetProcessor::CalculateSettingsHash(const ImportSettingsBase& settings) {
    std::string json = settings.ToJson();

    uint64_t hash = 14695981039346656037ULL;
    for (char c : json) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ULL;
    }

    return hash;
}

std::string AssetProcessor::GetOutputPath(const std::string& assetPath) const {
    fs::path relative = fs::relative(assetPath, m_projectRoot);
    return m_outputDirectory + "/" + relative.string() + ".nova";
}

// ============================================================================
// Asset Manifest
// ============================================================================

void AssetManifest::AddEntry(const Entry& entry) {
    m_entries[entry.assetId] = entry;
}

const AssetManifest::Entry* AssetManifest::GetEntry(const std::string& assetId) const {
    auto it = m_entries.find(assetId);
    return it != m_entries.end() ? &it->second : nullptr;
}

std::vector<const AssetManifest::Entry*> AssetManifest::GetEntriesByType(const std::string& type) const {
    std::vector<const Entry*> result;
    for (const auto& [id, entry] : m_entries) {
        if (entry.assetType == type) {
            result.push_back(&entry);
        }
    }
    return result;
}

std::vector<const AssetManifest::Entry*> AssetManifest::GetEntriesByTag(const std::string& tag) const {
    std::vector<const Entry*> result;
    for (const auto& [id, entry] : m_entries) {
        if (std::find(entry.tags.begin(), entry.tags.end(), tag) != entry.tags.end()) {
            result.push_back(&entry);
        }
    }
    return result;
}

bool AssetManifest::Save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"assets\": [\n";

    bool first = true;
    for (const auto& [id, entry] : m_entries) {
        if (!first) file << ",\n";
        first = false;

        file << "    {\n";
        file << "      \"assetId\": \"" << entry.assetId << "\",\n";
        file << "      \"sourcePath\": \"" << entry.sourcePath << "\",\n";
        file << "      \"cookedPath\": \"" << entry.cookedPath << "\",\n";
        file << "      \"assetType\": \"" << entry.assetType << "\",\n";
        file << "      \"cookedSize\": " << entry.cookedSize << "\n";
        file << "    }";
    }

    file << "\n  ]\n";
    file << "}\n";

    return true;
}

bool AssetManifest::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    m_entries.clear();

    // Simplified parsing
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return true;
}

} // namespace Nova
