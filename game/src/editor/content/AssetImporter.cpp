#include "AssetImporter.hpp"
#include "../Editor.hpp"
#include <Json/Json.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

namespace fs = std::filesystem;

namespace Vehement {
namespace Content {

// =============================================================================
// ImportResult implementation
// =============================================================================

bool ImportResult::HasErrors() const {
    return std::any_of(messages.begin(), messages.end(),
        [](const ImportMessage& m) { return m.level == ImportMessage::Level::Error; });
}

bool ImportResult::HasWarnings() const {
    return std::any_of(messages.begin(), messages.end(),
        [](const ImportMessage& m) { return m.level == ImportMessage::Level::Warning; });
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

AssetImporter::AssetImporter(Editor* editor)
    : m_editor(editor)
{
    // Initialize supported extensions
    m_modelExtensions = { "obj", "fbx", "gltf", "glb", "dae", "blend" };
    m_textureExtensions = { "png", "jpg", "jpeg", "tga", "bmp", "dds", "ktx", "webp", "psd", "exr", "hdr" };
    m_audioExtensions = { "wav", "mp3", "ogg", "flac", "aiff" };
    m_configExtensions = { "json" };
}

AssetImporter::~AssetImporter() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

bool AssetImporter::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Start worker thread
    m_running = true;
    m_workerThread = std::thread([this]() {
        while (m_running) {
            std::pair<uint64_t, std::function<void()>> task;
            {
                std::unique_lock<std::mutex> lock(m_tasksMutex);
                m_taskCondition.wait(lock, [this]() {
                    return !m_pendingTasks.empty() || !m_running;
                });

                if (!m_running && m_pendingTasks.empty()) {
                    break;
                }

                if (!m_pendingTasks.empty()) {
                    task = std::move(m_pendingTasks.front());
                    m_pendingTasks.pop();
                }
            }

            if (task.second) {
                // Update job status
                {
                    std::lock_guard<std::mutex> lock(m_jobsMutex);
                    auto it = m_jobs.find(task.first);
                    if (it != m_jobs.end()) {
                        it->second.status = ImportStatus::InProgress;
                        it->second.startTime = std::chrono::system_clock::now();
                    }
                }

                // Execute task
                task.second();
            }
        }
    });

    m_initialized = true;
    return true;
}

void AssetImporter::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Stop worker thread
    m_running = false;
    m_taskCondition.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Clear pending tasks
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        while (!m_pendingTasks.empty()) {
            m_pendingTasks.pop();
        }
    }

    // Clear jobs
    {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        m_jobs.clear();
    }

    m_initialized = false;
}

void AssetImporter::Update(float deltaTime) {
    // Process completed jobs callbacks on main thread if needed
    // This could be expanded for progress reporting
}

// =============================================================================
// Single File Import
// =============================================================================

ImportResult AssetImporter::Import(const std::string& sourcePath, const ImportOptions& options) {
    ImportResult result;
    result.sourcePath = sourcePath;
    result.status = ImportStatus::InProgress;

    // Check if file exists
    if (!fs::exists(sourcePath)) {
        result.status = ImportStatus::Failed;
        result.messages.push_back({
            ImportMessage::Level::Error,
            "Source file does not exist",
            sourcePath,
            0
        });
        return result;
    }

    // Detect asset type
    AssetType type = options.autoDetectType ? DetectAssetType(sourcePath) : options.forceType;
    result.type = type;

    // Determine target path
    result.targetPath = GetTargetPath(sourcePath, options);

    // Create target directory if needed
    fs::path targetDir = fs::path(result.targetPath).parent_path();
    if (!targetDir.empty() && !fs::exists(targetDir)) {
        try {
            fs::create_directories(targetDir);
        } catch (const std::exception& e) {
            result.status = ImportStatus::Failed;
            result.messages.push_back({
                ImportMessage::Level::Error,
                std::string("Failed to create target directory: ") + e.what(),
                targetDir.string(),
                0
            });
            return result;
        }
    }

    // Check for existing file
    if (fs::exists(result.targetPath) && !options.overwriteExisting) {
        result.status = ImportStatus::Failed;
        result.messages.push_back({
            ImportMessage::Level::Error,
            "Target file already exists and overwrite is disabled",
            result.targetPath,
            0
        });
        return result;
    }

    // Import based on type
    ImportResult importResult;
    switch (type) {
        case AssetType::Model:
            importResult = ImportModel(sourcePath, options);
            break;
        case AssetType::Texture:
            importResult = ImportTexture(sourcePath, options);
            break;
        case AssetType::Sound:
            importResult = ImportAudio(sourcePath, options);
            break;
        case AssetType::Unknown:
        default:
            // Try to copy as generic file
            importResult = ImportConfig(sourcePath, options);
            break;
    }

    result = importResult;
    result.sourcePath = sourcePath;

    // Generate config if requested
    if (options.generateConfig && result.status == ImportStatus::Completed) {
        std::string configContent = GenerateConfig(result.targetPath, type, options);
        if (!configContent.empty()) {
            std::string configPath = result.targetPath;
            size_t dotPos = configPath.find_last_of('.');
            if (dotPos != std::string::npos) {
                configPath = configPath.substr(0, dotPos) + ".json";
            } else {
                configPath += ".json";
            }

            std::ofstream configFile(configPath);
            if (configFile.is_open()) {
                configFile << configContent;
                configFile.close();
                result.configPath = configPath;
                result.createdFiles.push_back(configPath);
            }
        }
    }

    // Generate asset ID
    result.assetId = GenerateAssetId(fs::path(result.targetPath).stem().string(), type);

    // Notify callback
    if (OnImportComplete) {
        OnImportComplete(result);
    }

    return result;
}

uint64_t AssetImporter::ImportAsync(const std::string& sourcePath, const ImportOptions& options,
                                     std::function<void(const ImportResult&)> callback) {
    uint64_t jobId = m_nextJobId++;

    // Create job
    {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        ImportJob job;
        job.id = jobId;
        job.sourcePaths = { sourcePath };
        job.options = options;
        job.status = ImportStatus::Pending;
        m_jobs[jobId] = job;
    }

    // Queue task
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_pendingTasks.push({jobId, [this, sourcePath, options, callback, jobId]() {
            ImportResult result = Import(sourcePath, options);

            // Update job
            {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                auto it = m_jobs.find(jobId);
                if (it != m_jobs.end()) {
                    it->second.results.push_back(result);
                    it->second.status = result.status;
                    it->second.progress = 1.0f;
                    it->second.endTime = std::chrono::system_clock::now();
                }
            }

            if (callback) {
                callback(result);
            }
        }});
    }

    m_taskCondition.notify_one();
    return jobId;
}

// =============================================================================
// Batch Import
// =============================================================================

BatchImportResult AssetImporter::ImportBatch(const std::vector<std::string>& sourcePaths,
                                              const ImportOptions& options) {
    BatchImportResult batch;
    auto startTime = std::chrono::steady_clock::now();

    for (size_t i = 0; i < sourcePaths.size(); ++i) {
        ImportResult result = Import(sourcePaths[i], options);
        batch.results.push_back(result);

        if (result.Success()) {
            batch.successCount++;
        } else {
            batch.failureCount++;
        }
        if (result.HasWarnings()) {
            batch.warningCount++;
        }

        // Progress callback
        if (OnImportProgress) {
            OnImportProgress(0, static_cast<float>(i + 1) / sourcePaths.size());
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    batch.duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    if (OnBatchImportComplete) {
        OnBatchImportComplete(batch);
    }

    return batch;
}

BatchImportResult AssetImporter::ImportDirectory(const std::string& dirPath, bool recursive,
                                                  const ImportOptions& options) {
    std::vector<std::string> files;

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
                if (entry.is_regular_file() && IsExtensionSupported(entry.path().extension().string())) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dirPath)) {
                if (entry.is_regular_file() && IsExtensionSupported(entry.path().extension().string())) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        BatchImportResult result;
        ImportResult error;
        error.sourcePath = dirPath;
        error.status = ImportStatus::Failed;
        error.messages.push_back({
            ImportMessage::Level::Error,
            std::string("Failed to scan directory: ") + e.what(),
            dirPath,
            0
        });
        result.results.push_back(error);
        result.failureCount = 1;
        return result;
    }

    return ImportBatch(files, options);
}

uint64_t AssetImporter::ImportBatchAsync(const std::vector<std::string>& sourcePaths,
                                          const ImportOptions& options,
                                          std::function<void(const BatchImportResult&)> callback) {
    uint64_t jobId = m_nextJobId++;

    // Create job
    {
        std::lock_guard<std::mutex> lock(m_jobsMutex);
        ImportJob job;
        job.id = jobId;
        job.sourcePaths = sourcePaths;
        job.options = options;
        job.status = ImportStatus::Pending;
        m_jobs[jobId] = job;
    }

    // Queue task
    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);
        m_pendingTasks.push({jobId, [this, sourcePaths, options, callback, jobId]() {
            BatchImportResult result = ImportBatch(sourcePaths, options);

            // Update job
            {
                std::lock_guard<std::mutex> lock(m_jobsMutex);
                auto it = m_jobs.find(jobId);
                if (it != m_jobs.end()) {
                    it->second.results = result.results;
                    it->second.status = result.AllSuccess() ? ImportStatus::Completed : ImportStatus::Failed;
                    it->second.progress = 1.0f;
                    it->second.endTime = std::chrono::system_clock::now();
                }
            }

            if (callback) {
                callback(result);
            }
        }});
    }

    m_taskCondition.notify_one();
    return jobId;
}

// =============================================================================
// Drag-Drop Import
// =============================================================================

bool AssetImporter::CanImport(const std::vector<std::string>& paths) const {
    for (const auto& path : paths) {
        if (fs::is_regular_file(path)) {
            std::string ext = fs::path(path).extension().string();
            if (!ext.empty() && ext[0] == '.') {
                ext = ext.substr(1);
            }
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (IsExtensionSupported(ext)) {
                return true;
            }
        } else if (fs::is_directory(path)) {
            return true;  // Can import directories
        }
    }
    return false;
}

std::vector<std::pair<std::string, AssetType>> AssetImporter::GetImportPreview(
    const std::vector<std::string>& paths) const {
    std::vector<std::pair<std::string, AssetType>> preview;

    for (const auto& path : paths) {
        if (fs::is_regular_file(path)) {
            preview.push_back({path, DetectAssetType(path)});
        } else if (fs::is_directory(path)) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') {
                        ext = ext.substr(1);
                    }
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (IsExtensionSupported(ext)) {
                        preview.push_back({entry.path().string(), DetectAssetType(entry.path().string())});
                    }
                }
            }
        }
    }

    return preview;
}

BatchImportResult AssetImporter::ImportDroppedFiles(const std::vector<std::string>& paths,
                                                     const std::string& targetFolder) {
    std::vector<std::string> files;

    for (const auto& path : paths) {
        if (fs::is_regular_file(path)) {
            files.push_back(path);
        } else if (fs::is_directory(path)) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') {
                        ext = ext.substr(1);
                    }
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (IsExtensionSupported(ext)) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        }
    }

    ImportOptions options;
    if (!targetFolder.empty()) {
        options.targetDirectory = targetFolder;
    }

    return ImportBatch(files, options);
}

// =============================================================================
// Asset Packs
// =============================================================================

BatchImportResult AssetImporter::ImportAssetPack(const std::string& packPath,
                                                  const ImportOptions& options) {
    BatchImportResult result;

    // Read manifest
    auto manifest = ReadPackManifest(packPath);
    if (!manifest) {
        ImportResult error;
        error.sourcePath = packPath;
        error.status = ImportStatus::Failed;
        error.messages.push_back({
            ImportMessage::Level::Error,
            "Failed to read asset pack manifest",
            packPath,
            0
        });
        result.results.push_back(error);
        result.failureCount = 1;
        return result;
    }

    // Extract pack to temp directory
    std::string tempDir = fs::temp_directory_path().string() + "/vehement_pack_" +
                          std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    if (!ExtractArchive(packPath, tempDir)) {
        ImportResult error;
        error.sourcePath = packPath;
        error.status = ImportStatus::Failed;
        error.messages.push_back({
            ImportMessage::Level::Error,
            "Failed to extract asset pack",
            packPath,
            0
        });
        result.results.push_back(error);
        result.failureCount = 1;
        return result;
    }

    // Import extracted files
    result = ImportDirectory(tempDir, true, options);

    // Cleanup temp directory
    try {
        fs::remove_all(tempDir);
    } catch (...) {
        // Ignore cleanup errors
    }

    return result;
}

bool AssetImporter::ExportAssetPack(const std::vector<std::string>& assetIds,
                                     const std::string& outputPath,
                                     const AssetPackManifest& manifest) {
    // Create manifest JSON
    Json::Value root;
    root["id"] = manifest.id;
    root["name"] = manifest.name;
    root["version"] = manifest.version;
    root["author"] = manifest.author;
    root["description"] = manifest.description;

    Json::Value assets(Json::arrayValue);
    for (const auto& id : assetIds) {
        assets.append(id);
    }
    root["assets"] = assets;

    Json::Value deps(Json::arrayValue);
    for (const auto& dep : manifest.dependencies) {
        deps.append(dep);
    }
    root["dependencies"] = deps;

    // Write manifest to temp file
    std::string tempDir = fs::temp_directory_path().string() + "/vehement_export_" +
                          std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    try {
        fs::create_directories(tempDir);

        // Write manifest
        std::ofstream manifestFile(tempDir + "/manifest.json");
        if (manifestFile.is_open()) {
            Json::StreamWriterBuilder writer;
            writer["indentation"] = "    ";
            manifestFile << Json::writeString(writer, root);
            manifestFile.close();
        }

        // Create assets subdirectory in temp folder
        std::string assetsDir = tempDir + "/assets";
        fs::create_directories(assetsDir);

        // Copy asset files to the temp directory
        for (const auto& assetId : assetIds) {
            // Find the asset file by ID - in practice this would look up
            // the asset path from a registry or database
            // For now, we search common asset directories

            std::vector<std::string> searchPaths = {
                "game/assets/configs/units",
                "game/assets/configs/buildings",
                "game/assets/configs/tiles",
                "game/assets/configs/spells",
                "game/assets/configs/items",
                "game/assets/models",
                "game/assets/textures"
            };

            bool found = false;
            for (const auto& searchPath : searchPaths) {
                if (!fs::exists(searchPath)) continue;

                for (const auto& entry : fs::recursive_directory_iterator(searchPath)) {
                    if (!entry.is_regular_file()) continue;

                    std::string filename = entry.path().stem().string();
                    if (filename == assetId || entry.path().string().find(assetId) != std::string::npos) {
                        // Copy this file to assets dir
                        std::string destPath = assetsDir + "/" + entry.path().filename().string();
                        fs::copy_file(entry.path(), destPath, fs::copy_options::overwrite_existing);
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }

        // Create the output pack as a directory (uncompressed pack format)
        // A ZIP library (like libzip, miniz, or zlib) would be needed for proper ZIP creation
        // For now, we create an uncompressed directory-based pack

        if (fs::exists(outputPath)) {
            fs::remove_all(outputPath);
        }

        // Move temp directory to output path
        fs::rename(tempDir, outputPath);

        return true;
    } catch (const std::exception&) {
        try {
            fs::remove_all(tempDir);
        } catch (...) {}
        return false;
    }
}

std::optional<AssetPackManifest> AssetImporter::ReadPackManifest(const std::string& packPath) {
    // For now, assume pack is a directory with manifest.json
    // In a real implementation, would extract from ZIP
    std::string manifestPath = packPath;
    if (fs::is_directory(packPath)) {
        manifestPath = packPath + "/manifest.json";
    }

    if (!fs::exists(manifestPath)) {
        return std::nullopt;
    }

    std::ifstream file(manifestPath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errors;
    if (!Json::parseFromStream(reader, file, &root, &errors)) {
        return std::nullopt;
    }

    AssetPackManifest manifest;
    manifest.id = root.get("id", "").asString();
    manifest.name = root.get("name", "").asString();
    manifest.version = root.get("version", "1.0.0").asString();
    manifest.author = root.get("author", "").asString();
    manifest.description = root.get("description", "").asString();

    if (root.isMember("assets")) {
        for (const auto& asset : root["assets"]) {
            manifest.assets.push_back(asset.asString());
        }
    }

    if (root.isMember("dependencies")) {
        for (const auto& dep : root["dependencies"]) {
            manifest.dependencies.push_back(dep.asString());
        }
    }

    return manifest;
}

std::vector<ImportMessage> AssetImporter::ValidateAssetPack(const std::string& packPath) {
    std::vector<ImportMessage> messages;

    auto manifest = ReadPackManifest(packPath);
    if (!manifest) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Cannot read pack manifest",
            packPath,
            0
        });
        return messages;
    }

    if (manifest->id.empty()) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Pack manifest missing 'id' field",
            packPath,
            0
        });
    }

    if (manifest->name.empty()) {
        messages.push_back({
            ImportMessage::Level::Warning,
            "Pack manifest missing 'name' field",
            packPath,
            0
        });
    }

    if (manifest->assets.empty()) {
        messages.push_back({
            ImportMessage::Level::Warning,
            "Pack contains no assets",
            packPath,
            0
        });
    }

    return messages;
}

// =============================================================================
// Format Detection
// =============================================================================

ModelFormat AssetImporter::DetectModelFormat(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "obj") return ModelFormat::OBJ;
    if (ext == "fbx") return ModelFormat::FBX;
    if (ext == "gltf") return ModelFormat::GLTF;
    if (ext == "glb") return ModelFormat::GLB;
    if (ext == "dae") return ModelFormat::DAE;
    if (ext == "blend") return ModelFormat::Blend;

    return ModelFormat::Unknown;
}

TextureFormat AssetImporter::DetectTextureFormat(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "png") return TextureFormat::PNG;
    if (ext == "jpg" || ext == "jpeg") return TextureFormat::JPEG;
    if (ext == "tga") return TextureFormat::TGA;
    if (ext == "bmp") return TextureFormat::BMP;
    if (ext == "dds") return TextureFormat::DDS;
    if (ext == "ktx") return TextureFormat::KTX;
    if (ext == "webp") return TextureFormat::WebP;
    if (ext == "psd") return TextureFormat::PSD;
    if (ext == "exr") return TextureFormat::EXR;
    if (ext == "hdr") return TextureFormat::HDR;

    return TextureFormat::Unknown;
}

AudioFormat AssetImporter::DetectAudioFormat(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "wav") return AudioFormat::WAV;
    if (ext == "mp3") return AudioFormat::MP3;
    if (ext == "ogg") return AudioFormat::OGG;
    if (ext == "flac") return AudioFormat::FLAC;
    if (ext == "aiff") return AudioFormat::AIFF;

    return AudioFormat::Unknown;
}

AssetType AssetImporter::DetectAssetType(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (m_modelExtensions.count(ext)) return AssetType::Model;
    if (m_textureExtensions.count(ext)) return AssetType::Texture;
    if (m_audioExtensions.count(ext)) return AssetType::Sound;

    // Check for config types based on JSON content
    if (ext == "json") {
        std::ifstream file(path);
        if (file.is_open()) {
            Json::Value root;
            Json::CharReaderBuilder reader;
            std::string errors;
            if (Json::parseFromStream(reader, file, &root, &errors)) {
                std::string type = root.get("type", "").asString();
                if (type == "unit") return AssetType::Unit;
                if (type == "building") return AssetType::Building;
                if (type == "spell" || type == "ability") return AssetType::Spell;
                if (type == "tile" || type == "terrain") return AssetType::Tile;
                if (type == "effect" || type == "particle") return AssetType::Effect;
                if (type == "projectile") return AssetType::Projectile;
                if (type == "hero") return AssetType::Hero;
                if (type == "techtree" || type == "tech") return AssetType::TechTree;
            }
        }
    }

    return AssetType::Unknown;
}

std::vector<std::string> AssetImporter::GetSupportedExtensions() const {
    std::vector<std::string> extensions;

    for (const auto& ext : m_modelExtensions) {
        extensions.push_back(ext);
    }
    for (const auto& ext : m_textureExtensions) {
        extensions.push_back(ext);
    }
    for (const auto& ext : m_audioExtensions) {
        extensions.push_back(ext);
    }
    for (const auto& ext : m_configExtensions) {
        extensions.push_back(ext);
    }

    return extensions;
}

bool AssetImporter::IsExtensionSupported(const std::string& extension) const {
    std::string ext = extension;
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return m_modelExtensions.count(ext) ||
           m_textureExtensions.count(ext) ||
           m_audioExtensions.count(ext) ||
           m_configExtensions.count(ext);
}

// =============================================================================
// Config Generation
// =============================================================================

std::string AssetImporter::GenerateConfig(const std::string& assetPath, AssetType type,
                                           const ImportOptions& options) {
    Json::Value root;

    std::string filename = fs::path(assetPath).stem().string();
    std::string id = GenerateAssetId(filename, type);

    root["id"] = id;
    root["name"] = filename;

    switch (type) {
        case AssetType::Model:
            root["type"] = "model";
            root["model"]["path"] = assetPath;
            root["model"]["scale"] = Json::Value(Json::arrayValue);
            root["model"]["scale"].append(1.0);
            root["model"]["scale"].append(1.0);
            root["model"]["scale"].append(1.0);
            break;

        case AssetType::Texture:
            root["type"] = "texture";
            root["texture"]["path"] = assetPath;
            root["texture"]["generateMipmaps"] = true;
            break;

        case AssetType::Sound:
            root["type"] = "sound";
            root["sound"]["path"] = assetPath;
            root["sound"]["volume"] = 1.0;
            root["sound"]["loop"] = false;
            break;

        default:
            return "";
    }

    // Add tags
    Json::Value tags(Json::arrayValue);
    for (const auto& tag : options.tags) {
        tags.append(tag);
    }
    root["tags"] = tags;

    // Write JSON
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "    ";
    return Json::writeString(writer, root);
}

std::string AssetImporter::GetConfigTemplate(AssetType type) {
    switch (type) {
        case AssetType::Unit:
            return R"({
    "id": "unit_new",
    "type": "unit",
    "name": "New Unit",
    "description": "",
    "tags": [],
    "model": {
        "path": "",
        "scale": [1.0, 1.0, 1.0]
    },
    "combat": {
        "health": 100,
        "maxHealth": 100,
        "attackDamage": 10,
        "attackSpeed": 1.0,
        "attackRange": 1.0
    },
    "movement": {
        "speed": 5.0,
        "turnRate": 360.0
    },
    "properties": {
        "trainingTime": 10.0,
        "populationCost": 1,
        "goldCost": 50
    }
})";

        case AssetType::Building:
            return R"({
    "id": "building_new",
    "type": "building",
    "name": "New Building",
    "description": "",
    "tags": [],
    "model": {
        "path": "",
        "scale": [1.0, 1.0, 1.0]
    },
    "footprint": {
        "width": 2,
        "height": 2
    },
    "stats": {
        "health": 500,
        "maxHealth": 500,
        "armor": 5
    },
    "construction": {
        "buildTime": 30.0
    },
    "costs": {
        "gold": 100,
        "wood": 50
    }
})";

        case AssetType::Spell:
            return R"({
    "id": "spell_new",
    "type": "spell",
    "name": "New Spell",
    "description": "",
    "tags": [],
    "targeting": {
        "type": "single_target",
        "range": 10.0
    },
    "damage": {
        "amount": 50,
        "type": "fire"
    },
    "costs": {
        "manaCost": 25,
        "cooldown": 10.0
    }
})";

        case AssetType::Tile:
            return R"({
    "id": "tile_new",
    "type": "tile",
    "name": "New Tile",
    "description": "",
    "tags": [],
    "terrain": "ground",
    "walkability": "walkable",
    "movementCost": 1.0,
    "texture": {
        "path": "",
        "variations": 1
    }
})";

        default:
            return "{}";
    }
}

// =============================================================================
// Job Management
// =============================================================================

std::optional<ImportJob> AssetImporter::GetJob(uint64_t jobId) {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ImportJob> AssetImporter::GetActiveJobs() const {
    std::vector<ImportJob> active;
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_jobsMutex));

    for (const auto& [id, job] : m_jobs) {
        if (job.status == ImportStatus::Pending || job.status == ImportStatus::InProgress) {
            active.push_back(job);
        }
    }

    return active;
}

void AssetImporter::CancelJob(uint64_t jobId) {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    auto it = m_jobs.find(jobId);
    if (it != m_jobs.end() &&
        (it->second.status == ImportStatus::Pending || it->second.status == ImportStatus::InProgress)) {
        it->second.status = ImportStatus::Cancelled;
    }
}

void AssetImporter::CancelAllJobs() {
    std::lock_guard<std::mutex> lock(m_jobsMutex);
    for (auto& [id, job] : m_jobs) {
        if (job.status == ImportStatus::Pending || job.status == ImportStatus::InProgress) {
            job.status = ImportStatus::Cancelled;
        }
    }
}

// =============================================================================
// Validation
// =============================================================================

std::vector<ImportMessage> AssetImporter::ValidateFile(const std::string& path) {
    std::vector<ImportMessage> messages;

    if (!fs::exists(path)) {
        messages.push_back({
            ImportMessage::Level::Error,
            "File does not exist",
            path,
            0
        });
        return messages;
    }

    std::string ext = fs::path(path).extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (!IsExtensionSupported(ext)) {
        messages.push_back({
            ImportMessage::Level::Warning,
            "File extension not recognized: " + ext,
            path,
            0
        });
    }

    // Check file size
    auto fileSize = fs::file_size(path);
    if (fileSize > 100 * 1024 * 1024) {  // 100MB
        messages.push_back({
            ImportMessage::Level::Warning,
            "File is very large (" + std::to_string(fileSize / 1024 / 1024) + "MB), import may take time",
            path,
            0
        });
    }

    // Type-specific validation
    AssetType type = DetectAssetType(path);
    switch (type) {
        case AssetType::Model:
            {
                auto modelMessages = ValidateModel(path);
                messages.insert(messages.end(), modelMessages.begin(), modelMessages.end());
            }
            break;
        case AssetType::Texture:
            {
                auto texMessages = ValidateTexture(path);
                messages.insert(messages.end(), texMessages.begin(), texMessages.end());
            }
            break;
        case AssetType::Sound:
            {
                auto audioMessages = ValidateAudio(path);
                messages.insert(messages.end(), audioMessages.begin(), audioMessages.end());
            }
            break;
        default:
            break;
    }

    return messages;
}

std::vector<ImportMessage> AssetImporter::ValidateModel(const std::string& path) {
    std::vector<ImportMessage> messages;

    ModelFormat format = DetectModelFormat(path);
    if (format == ModelFormat::Unknown) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Unknown model format",
            path,
            0
        });
        return messages;
    }

    if (format == ModelFormat::Blend) {
        messages.push_back({
            ImportMessage::Level::Warning,
            "Blender files require external conversion, consider exporting to FBX or GLTF",
            path,
            0
        });
    }

    // Basic file read check
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Cannot read file",
            path,
            0
        });
    }

    return messages;
}

std::vector<ImportMessage> AssetImporter::ValidateTexture(const std::string& path) {
    std::vector<ImportMessage> messages;

    TextureFormat format = DetectTextureFormat(path);
    if (format == TextureFormat::Unknown) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Unknown texture format",
            path,
            0
        });
        return messages;
    }

    if (format == TextureFormat::PSD) {
        messages.push_back({
            ImportMessage::Level::Warning,
            "PSD files should be exported to PNG for best compatibility",
            path,
            0
        });
    }

    return messages;
}

std::vector<ImportMessage> AssetImporter::ValidateAudio(const std::string& path) {
    std::vector<ImportMessage> messages;

    AudioFormat format = DetectAudioFormat(path);
    if (format == AudioFormat::Unknown) {
        messages.push_back({
            ImportMessage::Level::Error,
            "Unknown audio format",
            path,
            0
        });
        return messages;
    }

    if (format == AudioFormat::FLAC) {
        messages.push_back({
            ImportMessage::Level::Info,
            "FLAC is lossless but large, consider OGG for smaller file size",
            path,
            0
        });
    }

    return messages;
}

// =============================================================================
// Private Import Implementations
// =============================================================================

ImportResult AssetImporter::ImportModel(const std::string& sourcePath, const ImportOptions& options) {
    ImportResult result;
    result.sourcePath = sourcePath;
    result.type = AssetType::Model;
    result.targetPath = GetTargetPath(sourcePath, options);

    ModelFormat format = DetectModelFormat(sourcePath);

    // Validate
    auto validationMessages = ValidateModel(sourcePath);
    result.messages.insert(result.messages.end(), validationMessages.begin(), validationMessages.end());

    if (std::any_of(validationMessages.begin(), validationMessages.end(),
        [](const ImportMessage& m) { return m.level == ImportMessage::Level::Error; })) {
        result.status = ImportStatus::Failed;
        return result;
    }

    // Process based on format
    bool success = false;
    switch (format) {
        case ModelFormat::OBJ:
            success = ProcessOBJ(sourcePath, result.targetPath, options.modelOptions);
            break;
        case ModelFormat::FBX:
            success = ProcessFBX(sourcePath, result.targetPath, options.modelOptions);
            break;
        case ModelFormat::GLTF:
        case ModelFormat::GLB:
            success = ProcessGLTF(sourcePath, result.targetPath, options.modelOptions);
            break;
        default:
            // Just copy the file
            try {
                fs::copy_file(sourcePath, result.targetPath, fs::copy_options::overwrite_existing);
                success = true;
            } catch (const std::exception& e) {
                result.messages.push_back({
                    ImportMessage::Level::Error,
                    std::string("Failed to copy file: ") + e.what(),
                    sourcePath,
                    0
                });
            }
            break;
    }

    if (success) {
        result.status = ImportStatus::Completed;
        result.createdFiles.push_back(result.targetPath);
    } else {
        result.status = ImportStatus::Failed;
    }

    return result;
}

ImportResult AssetImporter::ImportTexture(const std::string& sourcePath, const ImportOptions& options) {
    ImportResult result;
    result.sourcePath = sourcePath;
    result.type = AssetType::Texture;
    result.targetPath = GetTargetPath(sourcePath, options);

    // Validate
    auto validationMessages = ValidateTexture(sourcePath);
    result.messages.insert(result.messages.end(), validationMessages.begin(), validationMessages.end());

    if (result.HasErrors()) {
        result.status = ImportStatus::Failed;
        return result;
    }

    // Process texture
    bool success = ProcessTexture(sourcePath, result.targetPath, options.textureOptions);

    if (success) {
        result.status = ImportStatus::Completed;
        result.createdFiles.push_back(result.targetPath);
    } else {
        result.status = ImportStatus::Failed;
    }

    return result;
}

ImportResult AssetImporter::ImportAudio(const std::string& sourcePath, const ImportOptions& options) {
    ImportResult result;
    result.sourcePath = sourcePath;
    result.type = AssetType::Sound;
    result.targetPath = GetTargetPath(sourcePath, options);

    // Validate
    auto validationMessages = ValidateAudio(sourcePath);
    result.messages.insert(result.messages.end(), validationMessages.begin(), validationMessages.end());

    if (result.HasErrors()) {
        result.status = ImportStatus::Failed;
        return result;
    }

    // Process audio
    bool success = ProcessAudio(sourcePath, result.targetPath, options.audioOptions);

    if (success) {
        result.status = ImportStatus::Completed;
        result.createdFiles.push_back(result.targetPath);
    } else {
        result.status = ImportStatus::Failed;
    }

    return result;
}

ImportResult AssetImporter::ImportConfig(const std::string& sourcePath, const ImportOptions& options) {
    ImportResult result;
    result.sourcePath = sourcePath;
    result.type = DetectAssetType(sourcePath);
    result.targetPath = GetTargetPath(sourcePath, options);

    try {
        // Create parent directory
        fs::path targetDir = fs::path(result.targetPath).parent_path();
        if (!targetDir.empty() && !fs::exists(targetDir)) {
            fs::create_directories(targetDir);
        }

        // Copy file
        fs::copy_file(sourcePath, result.targetPath, fs::copy_options::overwrite_existing);
        result.status = ImportStatus::Completed;
        result.createdFiles.push_back(result.targetPath);
    } catch (const std::exception& e) {
        result.status = ImportStatus::Failed;
        result.messages.push_back({
            ImportMessage::Level::Error,
            std::string("Failed to copy file: ") + e.what(),
            sourcePath,
            0
        });
    }

    return result;
}

// =============================================================================
// Processing Functions
// =============================================================================

bool AssetImporter::ProcessOBJ(const std::string& input, const std::string& output,
                                const ModelImportOptions& options) {
    // For now, just copy the file
    // A real implementation would parse and process the OBJ
    try {
        fs::path outputDir = fs::path(output).parent_path();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        fs::copy_file(input, output, fs::copy_options::overwrite_existing);

        // Also copy MTL file if it exists
        std::string mtlPath = fs::path(input).replace_extension(".mtl").string();
        if (fs::exists(mtlPath)) {
            std::string mtlOutput = fs::path(output).replace_extension(".mtl").string();
            fs::copy_file(mtlPath, mtlOutput, fs::copy_options::overwrite_existing);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool AssetImporter::ProcessFBX(const std::string& input, const std::string& output,
                                const ModelImportOptions& options) {
    // FBX processing would require the FBX SDK
    // For now, just copy
    try {
        fs::path outputDir = fs::path(output).parent_path();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        fs::copy_file(input, output, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool AssetImporter::ProcessGLTF(const std::string& input, const std::string& output,
                                 const ModelImportOptions& options) {
    // GLTF processing - for now just copy
    try {
        fs::path outputDir = fs::path(output).parent_path();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        fs::copy_file(input, output, fs::copy_options::overwrite_existing);

        // Copy associated BIN file if GLTF (not GLB)
        if (fs::path(input).extension() == ".gltf") {
            std::string binPath = fs::path(input).replace_extension(".bin").string();
            if (fs::exists(binPath)) {
                std::string binOutput = fs::path(output).replace_extension(".bin").string();
                fs::copy_file(binPath, binOutput, fs::copy_options::overwrite_existing);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool AssetImporter::ProcessTexture(const std::string& input, const std::string& output,
                                    const TextureImportOptions& options) {
    // Texture processing - for now just copy
    // Real implementation would use image processing library
    try {
        fs::path outputDir = fs::path(output).parent_path();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        fs::copy_file(input, output, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool AssetImporter::GenerateMipmaps(const std::string& path) {
    // Mipmap generation would require image processing
    return true;  // Placeholder
}

bool AssetImporter::CompressTexture(const std::string& path, const std::string& format) {
    // Texture compression would require specialized library
    return true;  // Placeholder
}

bool AssetImporter::ProcessAudio(const std::string& input, const std::string& output,
                                  const AudioImportOptions& options) {
    // Audio processing - for now just copy
    try {
        fs::path outputDir = fs::path(output).parent_path();
        if (!outputDir.empty() && !fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        fs::copy_file(input, output, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool AssetImporter::ConvertAudioFormat(const std::string& input, const std::string& output,
                                        const std::string& format) {
    // Audio format conversion would require audio library
    return true;  // Placeholder
}

// =============================================================================
// Path Helpers
// =============================================================================

std::string AssetImporter::GetTargetPath(const std::string& sourcePath, const ImportOptions& options) const {
    fs::path source(sourcePath);
    std::string filename = SanitizeFileName(source.stem().string(), options.namingConvention);
    std::string extension = source.extension().string();

    std::string targetDir = options.targetDirectory;
    if (targetDir.empty() && m_editor) {
        // Default to appropriate content folder based on type
        AssetType type = DetectAssetType(sourcePath);
        switch (type) {
            case AssetType::Model:
                targetDir = "models";
                break;
            case AssetType::Texture:
                targetDir = "textures";
                break;
            case AssetType::Sound:
                targetDir = "sounds";
                break;
            default:
                targetDir = "assets";
                break;
        }
    }

    if (options.createSubfolder) {
        targetDir = (fs::path(targetDir) / filename).string();
    }

    return (fs::path(targetDir) / (filename + extension)).string();
}

std::string AssetImporter::SanitizeFileName(const std::string& name, const std::string& convention) const {
    std::string result;

    // Remove invalid characters
    for (char c : name) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            result += c;
        } else if (c == ' ') {
            result += '_';
        }
    }

    // Apply naming convention
    if (convention == "snake_case") {
        // Convert to lowercase with underscores
        std::string snake;
        for (size_t i = 0; i < result.size(); ++i) {
            if (std::isupper(result[i])) {
                if (i > 0 && result[i-1] != '_') {
                    snake += '_';
                }
                snake += static_cast<char>(std::tolower(result[i]));
            } else {
                snake += result[i];
            }
        }
        result = snake;
    } else if (convention == "PascalCase") {
        // Capitalize first letter of each word
        bool capitalizeNext = true;
        std::string pascal;
        for (char c : result) {
            if (c == '_' || c == '-') {
                capitalizeNext = true;
            } else if (capitalizeNext) {
                pascal += static_cast<char>(std::toupper(c));
                capitalizeNext = false;
            } else {
                pascal += static_cast<char>(std::tolower(c));
            }
        }
        result = pascal;
    } else if (convention == "camelCase") {
        // Like PascalCase but first letter lowercase
        bool capitalizeNext = false;
        bool firstLetter = true;
        std::string camel;
        for (char c : result) {
            if (c == '_' || c == '-') {
                capitalizeNext = true;
            } else if (firstLetter) {
                camel += static_cast<char>(std::tolower(c));
                firstLetter = false;
            } else if (capitalizeNext) {
                camel += static_cast<char>(std::toupper(c));
                capitalizeNext = false;
            } else {
                camel += static_cast<char>(std::tolower(c));
            }
        }
        result = camel;
    }

    return result;
}

std::string AssetImporter::GenerateAssetId(const std::string& name, AssetType type) const {
    std::string prefix;
    switch (type) {
        case AssetType::Unit: prefix = "unit_"; break;
        case AssetType::Building: prefix = "building_"; break;
        case AssetType::Spell: prefix = "spell_"; break;
        case AssetType::Tile: prefix = "tile_"; break;
        case AssetType::Effect: prefix = "effect_"; break;
        case AssetType::Model: prefix = "model_"; break;
        case AssetType::Texture: prefix = "texture_"; break;
        case AssetType::Sound: prefix = "sound_"; break;
        case AssetType::Hero: prefix = "hero_"; break;
        case AssetType::Projectile: prefix = "projectile_"; break;
        case AssetType::TechTree: prefix = "tech_"; break;
        default: prefix = "asset_"; break;
    }

    return prefix + SanitizeFileName(name, "snake_case");
}

bool AssetImporter::ExtractArchive(const std::string& archivePath, const std::string& outputPath) {
    // Archive extraction would require libzip or similar
    // Placeholder implementation
    return false;
}

std::vector<std::string> AssetImporter::ListArchiveContents(const std::string& archivePath) {
    // Archive listing would require libzip or similar
    return {};
}

} // namespace Content
} // namespace Vehement
