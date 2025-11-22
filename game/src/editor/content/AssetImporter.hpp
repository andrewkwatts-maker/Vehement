#pragma once

#include "ContentDatabase.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <optional>
#include <variant>

namespace Vehement {

class Editor;

namespace Content {

/**
 * @brief Import source types
 */
enum class ImportSourceType {
    LocalFile,
    LocalDirectory,
    Archive,         // ZIP, TAR, etc.
    URL,
    AssetPack
};

/**
 * @brief Model format types
 */
enum class ModelFormat {
    Unknown,
    OBJ,
    FBX,
    GLTF,
    GLB,
    DAE,        // Collada
    Blend       // Blender (requires conversion)
};

/**
 * @brief Texture format types
 */
enum class TextureFormat {
    Unknown,
    PNG,
    JPEG,
    TGA,
    BMP,
    DDS,
    KTX,
    WebP,
    PSD,
    EXR,
    HDR
};

/**
 * @brief Audio format types
 */
enum class AudioFormat {
    Unknown,
    WAV,
    MP3,
    OGG,
    FLAC,
    AIFF
};

/**
 * @brief Import status
 */
enum class ImportStatus {
    Pending,
    InProgress,
    Completed,
    Failed,
    Cancelled
};

/**
 * @brief Import warning/error message
 */
struct ImportMessage {
    enum class Level { Info, Warning, Error };
    Level level;
    std::string message;
    std::string file;
    int line = 0;
};

/**
 * @brief Import options for models
 */
struct ModelImportOptions {
    bool generateNormals = true;
    bool generateTangents = true;
    bool optimizeMesh = true;
    bool flipUVs = false;
    bool flipWindingOrder = false;
    float scale = 1.0f;
    glm::vec3 offset{0.0f};
    bool importAnimations = true;
    bool importMaterials = true;
    bool embedTextures = false;
    std::string outputFormat = "obj";
};

/**
 * @brief Import options for textures
 */
struct TextureImportOptions {
    bool generateMipmaps = true;
    bool compress = true;
    std::string compressionFormat = "BC7";
    int maxSize = 4096;
    bool powerOfTwo = true;
    bool flipVertical = false;
    bool premultiplyAlpha = false;
    std::string outputFormat = "png";
};

/**
 * @brief Import options for audio
 */
struct AudioImportOptions {
    int sampleRate = 44100;
    int channels = 2;        // 1 = mono, 2 = stereo
    bool normalize = true;
    bool compress = false;
    std::string compressionFormat = "ogg";
    float compressionQuality = 0.7f;
    std::string outputFormat = "wav";
};

/**
 * @brief Generic import options
 */
struct ImportOptions {
    std::string targetDirectory;
    bool overwriteExisting = false;
    bool generateConfig = true;
    bool importDependencies = true;
    bool preserveHierarchy = true;
    bool createSubfolder = true;
    std::string namingConvention = "snake_case";  // snake_case, PascalCase, camelCase

    ModelImportOptions modelOptions;
    TextureImportOptions textureOptions;
    AudioImportOptions audioOptions;

    // Auto-configuration
    bool autoDetectType = true;
    AssetType forceType = AssetType::Unknown;
    std::vector<std::string> tags;
};

/**
 * @brief Import result for a single file
 */
struct ImportResult {
    std::string sourcePath;
    std::string targetPath;
    std::string assetId;
    AssetType type = AssetType::Unknown;
    ImportStatus status = ImportStatus::Pending;
    std::vector<ImportMessage> messages;
    std::vector<std::string> createdFiles;
    std::string configPath;  // Generated JSON config

    [[nodiscard]] bool Success() const { return status == ImportStatus::Completed; }
    [[nodiscard]] bool HasErrors() const;
    [[nodiscard]] bool HasWarnings() const;
};

/**
 * @brief Batch import result
 */
struct BatchImportResult {
    std::vector<ImportResult> results;
    int successCount = 0;
    int failureCount = 0;
    int warningCount = 0;
    std::chrono::milliseconds duration{0};

    [[nodiscard]] bool AllSuccess() const { return failureCount == 0; }
};

/**
 * @brief Import job for tracking progress
 */
struct ImportJob {
    uint64_t id;
    std::vector<std::string> sourcePaths;
    ImportOptions options;
    ImportStatus status = ImportStatus::Pending;
    float progress = 0.0f;
    std::vector<ImportResult> results;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
};

/**
 * @brief Asset pack manifest
 */
struct AssetPackManifest {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> assets;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> dependencies;  // Other asset packs
};

/**
 * @brief Asset Importer
 *
 * Imports external assets into the project:
 * - Import 3D models (OBJ, FBX, GLTF)
 * - Import textures (PNG, JPG, TGA, etc.)
 * - Import audio (WAV, MP3, OGG)
 * - Auto-generate config JSON
 * - Validation and error reporting
 * - Batch import
 * - Asset pack import/export
 */
class AssetImporter {
public:
    explicit AssetImporter(Editor* editor);
    ~AssetImporter();

    // Non-copyable
    AssetImporter(const AssetImporter&) = delete;
    AssetImporter& operator=(const AssetImporter&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the importer
     */
    bool Initialize();

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update (process async imports)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Single File Import
    // =========================================================================

    /**
     * @brief Import a single file
     * @param sourcePath Path to source file
     * @param options Import options
     * @return Import result
     */
    ImportResult Import(const std::string& sourcePath, const ImportOptions& options = {});

    /**
     * @brief Import file asynchronously
     * @return Job ID for tracking
     */
    uint64_t ImportAsync(const std::string& sourcePath, const ImportOptions& options = {},
                         std::function<void(const ImportResult&)> callback = nullptr);

    // =========================================================================
    // Batch Import
    // =========================================================================

    /**
     * @brief Import multiple files
     */
    BatchImportResult ImportBatch(const std::vector<std::string>& sourcePaths,
                                   const ImportOptions& options = {});

    /**
     * @brief Import directory
     * @param dirPath Directory to import
     * @param recursive Include subdirectories
     */
    BatchImportResult ImportDirectory(const std::string& dirPath, bool recursive = true,
                                       const ImportOptions& options = {});

    /**
     * @brief Import batch asynchronously
     * @return Job ID
     */
    uint64_t ImportBatchAsync(const std::vector<std::string>& sourcePaths,
                              const ImportOptions& options = {},
                              std::function<void(const BatchImportResult&)> callback = nullptr);

    // =========================================================================
    // Drag-Drop Import
    // =========================================================================

    /**
     * @brief Check if paths can be imported
     */
    [[nodiscard]] bool CanImport(const std::vector<std::string>& paths) const;

    /**
     * @brief Get import preview (what would be imported)
     */
    [[nodiscard]] std::vector<std::pair<std::string, AssetType>> GetImportPreview(
        const std::vector<std::string>& paths) const;

    /**
     * @brief Import files from drag-drop
     */
    BatchImportResult ImportDroppedFiles(const std::vector<std::string>& paths,
                                          const std::string& targetFolder = "");

    // =========================================================================
    // Asset Packs
    // =========================================================================

    /**
     * @brief Import asset pack
     */
    BatchImportResult ImportAssetPack(const std::string& packPath,
                                       const ImportOptions& options = {});

    /**
     * @brief Export assets as pack
     */
    bool ExportAssetPack(const std::vector<std::string>& assetIds,
                         const std::string& outputPath,
                         const AssetPackManifest& manifest);

    /**
     * @brief Read asset pack manifest
     */
    std::optional<AssetPackManifest> ReadPackManifest(const std::string& packPath);

    /**
     * @brief Validate asset pack
     */
    std::vector<ImportMessage> ValidateAssetPack(const std::string& packPath);

    // =========================================================================
    // Format Detection
    // =========================================================================

    /**
     * @brief Detect model format
     */
    [[nodiscard]] ModelFormat DetectModelFormat(const std::string& path) const;

    /**
     * @brief Detect texture format
     */
    [[nodiscard]] TextureFormat DetectTextureFormat(const std::string& path) const;

    /**
     * @brief Detect audio format
     */
    [[nodiscard]] AudioFormat DetectAudioFormat(const std::string& path) const;

    /**
     * @brief Detect asset type from file
     */
    [[nodiscard]] AssetType DetectAssetType(const std::string& path) const;

    /**
     * @brief Get supported extensions
     */
    [[nodiscard]] std::vector<std::string> GetSupportedExtensions() const;

    /**
     * @brief Check if extension is supported
     */
    [[nodiscard]] bool IsExtensionSupported(const std::string& extension) const;

    // =========================================================================
    // Config Generation
    // =========================================================================

    /**
     * @brief Generate config JSON for imported asset
     */
    std::string GenerateConfig(const std::string& assetPath, AssetType type,
                               const ImportOptions& options = {});

    /**
     * @brief Get config template for type
     */
    std::string GetConfigTemplate(AssetType type);

    // =========================================================================
    // Job Management
    // =========================================================================

    /**
     * @brief Get import job status
     */
    std::optional<ImportJob> GetJob(uint64_t jobId);

    /**
     * @brief Get all active jobs
     */
    [[nodiscard]] std::vector<ImportJob> GetActiveJobs() const;

    /**
     * @brief Cancel import job
     */
    void CancelJob(uint64_t jobId);

    /**
     * @brief Cancel all jobs
     */
    void CancelAllJobs();

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Validate file before import
     */
    std::vector<ImportMessage> ValidateFile(const std::string& path);

    /**
     * @brief Validate model file
     */
    std::vector<ImportMessage> ValidateModel(const std::string& path);

    /**
     * @brief Validate texture file
     */
    std::vector<ImportMessage> ValidateTexture(const std::string& path);

    /**
     * @brief Validate audio file
     */
    std::vector<ImportMessage> ValidateAudio(const std::string& path);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const ImportResult&)> OnImportComplete;
    std::function<void(const BatchImportResult&)> OnBatchImportComplete;
    std::function<void(uint64_t, float)> OnImportProgress;  // jobId, progress
    std::function<void(uint64_t, const std::string&)> OnImportError;  // jobId, error

private:
    // Import implementations
    ImportResult ImportModel(const std::string& sourcePath, const ImportOptions& options);
    ImportResult ImportTexture(const std::string& sourcePath, const ImportOptions& options);
    ImportResult ImportAudio(const std::string& sourcePath, const ImportOptions& options);
    ImportResult ImportConfig(const std::string& sourcePath, const ImportOptions& options);

    // Model processing
    bool ProcessOBJ(const std::string& input, const std::string& output, const ModelImportOptions& options);
    bool ProcessFBX(const std::string& input, const std::string& output, const ModelImportOptions& options);
    bool ProcessGLTF(const std::string& input, const std::string& output, const ModelImportOptions& options);

    // Texture processing
    bool ProcessTexture(const std::string& input, const std::string& output, const TextureImportOptions& options);
    bool GenerateMipmaps(const std::string& path);
    bool CompressTexture(const std::string& path, const std::string& format);

    // Audio processing
    bool ProcessAudio(const std::string& input, const std::string& output, const AudioImportOptions& options);
    bool ConvertAudioFormat(const std::string& input, const std::string& output, const std::string& format);

    // Path helpers
    std::string GetTargetPath(const std::string& sourcePath, const ImportOptions& options) const;
    std::string SanitizeFileName(const std::string& name, const std::string& convention) const;
    std::string GenerateAssetId(const std::string& name, AssetType type) const;

    // Archive handling
    bool ExtractArchive(const std::string& archivePath, const std::string& outputPath);
    std::vector<std::string> ListArchiveContents(const std::string& archivePath);

    Editor* m_editor = nullptr;
    bool m_initialized = false;

    // Active jobs
    std::unordered_map<uint64_t, ImportJob> m_jobs;
    std::mutex m_jobsMutex;
    std::atomic<uint64_t> m_nextJobId{1};

    // Background processing
    std::thread m_workerThread;
    std::atomic<bool> m_running{false};
    std::queue<std::pair<uint64_t, std::function<void()>>> m_pendingTasks;
    std::mutex m_tasksMutex;
    std::condition_variable m_taskCondition;

    // Supported formats
    std::unordered_set<std::string> m_modelExtensions;
    std::unordered_set<std::string> m_textureExtensions;
    std::unordered_set<std::string> m_audioExtensions;
    std::unordered_set<std::string> m_configExtensions;
};

} // namespace Content
} // namespace Vehement
