/**
 * @file AssetThumbnailCache.cpp
 * @brief Implementation of automatic thumbnail generation system
 *
 * Complete implementation with:
 * - Actual thumbnail rendering for SDF models
 * - Disk caching with hash-based file versioning
 * - Background generation thread support
 * - Typed placeholder icons while generating
 * - Material preview rendering (sphere with material)
 * - Script/level asset type detection
 */

#include "AssetThumbnailCache.hpp"
#include <spdlog/spdlog.h>
#include "../graphics/SDFRenderer.hpp"
#include "../graphics/Framebuffer.hpp"
#include "../core/Logger.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../sdf/SDFAnimation.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_image_write.h>
#include <functional>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace Nova {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate a hash string from asset path and modification time
 * Uses FNV-1a hash for fast, reasonably distributed hashes
 */
static std::string GenerateCacheHash(const std::string& assetPath, uint64_t timestamp) {
    // FNV-1a 64-bit hash
    uint64_t hash = 14695981039346656037ULL;
    constexpr uint64_t fnvPrime = 1099511628211ULL;

    // Hash the path
    for (char c : assetPath) {
        hash ^= static_cast<uint64_t>(c);
        hash *= fnvPrime;
    }

    // Mix in the timestamp
    for (int i = 0; i < 8; ++i) {
        hash ^= (timestamp >> (i * 8)) & 0xFF;
        hash *= fnvPrime;
    }

    // Convert to hex string
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

/**
 * @brief Get environment variable with fallback
 */
static std::string GetEnvWithDefault(const char* name, const std::string& defaultValue) {
#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) == 0 && value != nullptr) {
        std::string result(value);
        free(value);
        return result;
    }
#else
    const char* value = std::getenv(name);
    if (value != nullptr) {
        return std::string(value);
    }
#endif
    return defaultValue;
}

/**
 * @brief Get the default thumbnail cache directory
 * Uses %APPDATA%/Nova3D/thumbnails on Windows, ~/.cache/Nova3D/thumbnails on Linux/Mac
 */
static std::string GetDefaultCacheDirectory() {
#ifdef _WIN32
    std::string appData = GetEnvWithDefault("APPDATA", ".");
    return appData + "\\Nova3D\\thumbnails";
#else
    std::string home = GetEnvWithDefault("HOME", ".");
    return home + "/.cache/Nova3D/thumbnails";
#endif
}

// ============================================================================
// Placeholder Icon Colors per Asset Type
// ============================================================================

struct PlaceholderStyle {
    glm::vec4 primaryColor;
    glm::vec4 secondaryColor;
    const char* iconChar;  // Simple character representation
};

static PlaceholderStyle GetPlaceholderStyle(ThumbnailAssetType type) {
    switch (type) {
        case ThumbnailAssetType::Static:
            return { glm::vec4(0.3f, 0.5f, 0.8f, 1.0f), glm::vec4(0.2f, 0.3f, 0.5f, 1.0f), "M" };  // Blue for models
        case ThumbnailAssetType::Animated:
            return { glm::vec4(0.8f, 0.5f, 0.3f, 1.0f), glm::vec4(0.5f, 0.3f, 0.2f, 1.0f), "A" };  // Orange for animated
        case ThumbnailAssetType::Unit:
            return { glm::vec4(0.3f, 0.8f, 0.3f, 1.0f), glm::vec4(0.2f, 0.5f, 0.2f, 1.0f), "U" };  // Green for units
        case ThumbnailAssetType::Building:
            return { glm::vec4(0.7f, 0.7f, 0.3f, 1.0f), glm::vec4(0.5f, 0.5f, 0.2f, 1.0f), "B" };  // Yellow for buildings
        case ThumbnailAssetType::Texture:
            return { glm::vec4(0.8f, 0.3f, 0.8f, 1.0f), glm::vec4(0.5f, 0.2f, 0.5f, 1.0f), "T" };  // Purple for textures
        case ThumbnailAssetType::Unknown:
        default:
            return { glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), glm::vec4(0.3f, 0.3f, 0.3f, 1.0f), "?" };  // Gray for unknown
    }
}

// ============================================================================
// AssetThumbnailCache Implementation
// ============================================================================

AssetThumbnailCache::AssetThumbnailCache() = default;

AssetThumbnailCache::~AssetThumbnailCache() {
    Shutdown();
}

bool AssetThumbnailCache::Initialize(const std::string& cacheDirectory, const std::string& assetDirectory) {
    if (m_initialized) {
        spdlog::warn("AssetThumbnailCache already initialized");
        return true;
    }

    // Use provided directory or default
    m_cacheDirectory = cacheDirectory.empty() ? GetDefaultCacheDirectory() : cacheDirectory;
    m_assetDirectory = assetDirectory;

    // Create cache directory if it doesn't exist
    if (!fs::exists(m_cacheDirectory)) {
        try {
            fs::create_directories(m_cacheDirectory);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create cache directory: {} - {}", m_cacheDirectory, e.what());
            return false;
        }
    }

    // Initialize SDF renderer
    m_renderer = std::make_unique<SDFRenderer>();
    if (!m_renderer->Initialize()) {
        spdlog::error("Failed to initialize SDF renderer for thumbnails");
        return false;
    }

    // Create framebuffer for rendering (will resize as needed)
    m_framebuffer = std::make_shared<Framebuffer>();

    // Create placeholder textures for each asset type
    CreatePlaceholderTextures();

    // Load cache manifest
    LoadCacheManifest();

    m_initialized = true;
    spdlog::info("AssetThumbnailCache initialized: cache={}, assets={}",
                 m_cacheDirectory, m_assetDirectory);

    return true;
}

void AssetThumbnailCache::Shutdown() {
    if (!m_initialized) return;

    // Save cache manifest
    SaveCacheManifest();

    // Clear cache
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache.clear();
    }

    // Clear queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_requestQueue.empty()) {
            m_requestQueue.pop();
        }
    }

    m_renderer.reset();
    m_framebuffer.reset();
    m_placeholderTexture.reset();
    m_placeholderTextures.clear();

    m_initialized = false;
    spdlog::info("AssetThumbnailCache shut down");
}

std::shared_ptr<Texture> AssetThumbnailCache::GetThumbnail(const std::string& assetPath, int size, int priority) {
    if (!m_initialized) {
        return m_placeholderTexture;
    }

    std::string cacheKey = GetCacheKey(assetPath, size);
    ThumbnailAssetType assetType = DetectAssetType(assetPath);

    // Check in-memory cache first
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(cacheKey);
        if (it != m_cache.end()) {
            // Check if thumbnail is still valid
            uint64_t assetTimestamp = GetFileTimestamp(assetPath);
            if (it->second.isValid && it->second.assetTimestamp == assetTimestamp) {
                return it->second.texture;
            }
        }
    }

    // Check disk cache before queueing generation
    std::string diskCachePath = GetThumbnailPath(assetPath, size);
    uint64_t assetTimestamp = GetFileTimestamp(assetPath);

    if (fs::exists(diskCachePath)) {
        uint64_t cacheTimestamp = GetFileTimestamp(diskCachePath);
        if (cacheTimestamp >= assetTimestamp) {
            // Load from disk cache
            auto texture = std::make_shared<Texture>();
            if (texture->Load(diskCachePath)) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                ThumbnailCache& entry = m_cache[cacheKey];
                entry.texture = texture;
                entry.assetPath = assetPath;
                entry.thumbnailPath = diskCachePath;
                entry.assetTimestamp = assetTimestamp;
                entry.thumbnailTimestamp = cacheTimestamp;
                entry.size = size;
                entry.isValid = true;
                entry.isGenerating = false;

                spdlog::debug("Loaded thumbnail from disk cache: {}", assetPath);
                return texture;
            }
        }
    }

    // Queue generation request
    ThumbnailRequest request;
    request.assetPath = assetPath;
    request.outputPath = diskCachePath;
    request.size = size;
    request.assetType = assetType;
    request.fileTimestamp = assetTimestamp;
    request.priority = priority;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(request);
    }

    // Return typed placeholder while generating
    return GetPlaceholderForType(assetType, size);
}

bool AssetThumbnailCache::HasValidThumbnail(const std::string& assetPath) const {
    if (!m_initialized) return false;

    std::string cacheKey = GetCacheKey(assetPath, 256);
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    auto it = m_cache.find(cacheKey);
    if (it == m_cache.end()) return false;

    uint64_t assetTimestamp = GetFileTimestamp(assetPath);
    return it->second.isValid && it->second.assetTimestamp == assetTimestamp;
}

void AssetThumbnailCache::InvalidateThumbnail(const std::string& assetPath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Remove all sizes for this asset
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it->second.assetPath == assetPath) {
            // Also delete the cached file
            if (fs::exists(it->second.thumbnailPath)) {
                try {
                    fs::remove(it->second.thumbnailPath);
                } catch (...) {
                    // Ignore file deletion errors
                }
            }
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }

    spdlog::debug("Invalidated thumbnail: {}", assetPath);
}

void AssetThumbnailCache::InvalidateDirectory(const std::string& directory) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    int count = 0;
    auto it = m_cache.begin();
    while (it != m_cache.end()) {
        if (it->second.assetPath.find(directory) == 0) {
            if (fs::exists(it->second.thumbnailPath)) {
                try {
                    fs::remove(it->second.thumbnailPath);
                } catch (...) {}
            }
            it = m_cache.erase(it);
            count++;
        } else {
            ++it;
        }
    }

    spdlog::info("Invalidated {} thumbnails in directory: {}", count, directory);
}

int AssetThumbnailCache::ProcessQueue(float maxTimeMs) {
    if (!m_initialized || !m_autoGenerate) return 0;

    auto startTime = std::chrono::high_resolution_clock::now();
    int processedCount = 0;

    while (!m_requestQueue.empty()) {
        // Check time budget
        auto currentTime = std::chrono::high_resolution_clock::now();
        float elapsedMs = std::chrono::duration<float, std::milli>(currentTime - startTime).count();
        if (elapsedMs >= maxTimeMs) {
            break;
        }

        // Get next request
        ThumbnailRequest request;
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_requestQueue.empty()) break;
            request = m_requestQueue.top();
            m_requestQueue.pop();
        }

        // Check if already being generated or already valid
        std::string cacheKey = GetCacheKey(request.assetPath, request.size);
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_cache.find(cacheKey);
            if (it != m_cache.end()) {
                if (it->second.isGenerating) {
                    continue; // Skip if already generating
                }
                if (it->second.isValid && it->second.assetTimestamp == request.fileTimestamp) {
                    continue; // Already valid, skip
                }
            }

            // Mark as generating
            ThumbnailCache& entry = m_cache[cacheKey];
            entry.isGenerating = true;
            entry.assetPath = request.assetPath;
        }

        // Generate thumbnail
        bool success = GenerateThumbnail(request);

        // Update cache
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            ThumbnailCache& entry = m_cache[cacheKey];
            entry.isGenerating = false;

            if (success) {
                // Load generated thumbnail
                entry.texture = std::make_shared<Texture>();
                if (entry.texture->Load(request.outputPath)) {
                    entry.thumbnailPath = request.outputPath;
                    entry.assetTimestamp = request.fileTimestamp;
                    entry.thumbnailTimestamp = GetFileTimestamp(request.outputPath);
                    entry.size = request.size;
                    entry.isValid = true;
                    processedCount++;
                    spdlog::debug("Generated thumbnail: {} ({}x{})", request.assetPath, request.size, request.size);
                } else {
                    spdlog::error("Failed to load generated thumbnail: {}", request.outputPath);
                    entry.texture = GetPlaceholderForType(request.assetType, request.size);
                    entry.isValid = false;
                }
            } else {
                entry.texture = GetPlaceholderForType(request.assetType, request.size);
                entry.isValid = false;
            }
        }
    }

    return processedCount;
}

void AssetThumbnailCache::ScanForChanges() {
    if (!m_initialized || m_assetDirectory.empty()) return;

    if (!fs::exists(m_assetDirectory)) {
        spdlog::warn("Asset directory does not exist: {}", m_assetDirectory);
        return;
    }

    int newFiles = 0;
    int modifiedFiles = 0;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(m_assetDirectory)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            // Support various asset types
            if (ext != ".json" && ext != ".png" && ext != ".jpg" &&
                ext != ".lua" && ext != ".script" && ext != ".mat") {
                continue;
            }

            std::string assetPath = entry.path().string();
            uint64_t timestamp = GetFileTimestamp(assetPath);

            auto it = m_watchedFiles.find(assetPath);
            if (it == m_watchedFiles.end()) {
                // New file
                m_watchedFiles[assetPath] = timestamp;
                WatchAsset(assetPath);
                newFiles++;
            } else if (it->second != timestamp) {
                // Modified file
                it->second = timestamp;
                InvalidateThumbnail(assetPath);
                WatchAsset(assetPath);
                modifiedFiles++;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error scanning asset directory: {}", e.what());
    }

    if (newFiles > 0 || modifiedFiles > 0) {
        spdlog::info("Scan complete: {} new, {} modified assets", newFiles, modifiedFiles);
    }
}

void AssetThumbnailCache::WatchAsset(const std::string& assetPath) {
    m_watchedFiles[assetPath] = GetFileTimestamp(assetPath);
}

void AssetThumbnailCache::UnwatchAsset(const std::string& assetPath) {
    m_watchedFiles.erase(assetPath);
}

// ============================================================================
// Asset Type Detection
// ============================================================================

ThumbnailAssetType AssetThumbnailCache::DetectAssetType(const std::string& assetPath) {
    fs::path path(assetPath);
    std::string ext = path.extension().string();

    // Direct texture files
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
        return ThumbnailAssetType::Texture;
    }

    // Script files
    if (ext == ".lua" || ext == ".script") {
        return ThumbnailAssetType::Unknown;  // Scripts get a special icon
    }

    // Material files
    if (ext == ".mat") {
        return ThumbnailAssetType::Unknown;  // Materials get sphere preview
    }

    if (ext != ".json") {
        return ThumbnailAssetType::Unknown;
    }

    // Parse JSON to determine type
    try {
        std::ifstream file(assetPath);
        if (!file.is_open()) return ThumbnailAssetType::Unknown;

        json data;
        file >> data;
        file.close();

        // Check explicit type field
        if (data.contains("type")) {
            std::string type = data["type"].get<std::string>();
            if (type == "unit" || type == "Unit" || type == "hero" || type == "Hero") {
                return ThumbnailAssetType::Unit;
            }
            if (type == "building" || type == "Building") {
                return ThumbnailAssetType::Building;
            }
            if (type == "level" || type == "Level" || type == "scene" || type == "Scene") {
                return ThumbnailAssetType::Unknown;  // Level type for top-down view
            }
            if (type == "material" || type == "Material") {
                return ThumbnailAssetType::Unknown;  // Material type for sphere preview
            }
        }

        // Check for animations
        if (data.contains("animations") && !data["animations"].empty()) {
            return ThumbnailAssetType::Animated;
        }

        // Check for SDF model
        if (data.contains("sdfModel")) {
            const auto& sdfData = data["sdfModel"];
            if (sdfData.contains("animations") && !sdfData["animations"].empty()) {
                return ThumbnailAssetType::Animated;
            }
            return ThumbnailAssetType::Static;
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to detect asset type: {} - {}", assetPath, e.what());
    }

    return ThumbnailAssetType::Unknown;
}

// ============================================================================
// Cache Key and Path Generation
// ============================================================================

std::string AssetThumbnailCache::GetCacheKey(const std::string& assetPath, int size) const {
    return assetPath + "_" + std::to_string(size);
}

std::string AssetThumbnailCache::GetThumbnailPath(const std::string& assetPath, int size) const {
    // Generate hash-based filename for better caching
    uint64_t timestamp = GetFileTimestamp(assetPath);
    std::string hash = GenerateCacheHash(assetPath, timestamp);

    std::string filename = hash + "_" + std::to_string(size) + ".png";

    // Create subdirectory structure based on hash prefix (for better filesystem performance)
    std::string subdir = hash.substr(0, 2);
    fs::path outputDir = fs::path(m_cacheDirectory) / subdir;

    if (!fs::exists(outputDir)) {
        try {
            fs::create_directories(outputDir);
        } catch (...) {
            // Fall back to root cache directory
            outputDir = fs::path(m_cacheDirectory);
        }
    }

    return (outputDir / filename).string();
}

// ============================================================================
// Thumbnail Generation
// ============================================================================

bool AssetThumbnailCache::GenerateThumbnail(const ThumbnailRequest& request) {
    // Ensure output directory exists
    fs::path outputPath(request.outputPath);
    if (!fs::exists(outputPath.parent_path())) {
        try {
            fs::create_directories(outputPath.parent_path());
        } catch (const std::exception& e) {
            spdlog::error("Failed to create output directory: {}", e.what());
            return false;
        }
    }

    switch (request.assetType) {
        case ThumbnailAssetType::Texture:
            return CopyTextureThumbnail(request.assetPath, request.outputPath, request.size);

        case ThumbnailAssetType::Static:
        case ThumbnailAssetType::Animated:
        case ThumbnailAssetType::Unit:
        case ThumbnailAssetType::Building:
            return RenderSDFThumbnail(request.assetPath, request.outputPath, request.size, request.assetType);

        case ThumbnailAssetType::Unknown:
        default:
            // Check if it's a material file - render sphere with material
            if (request.assetPath.ends_with(".mat") || request.assetPath.ends_with(".material")) {
                return RenderMaterialThumbnail(request.assetPath, request.outputPath, request.size);
            }
            // For unknown types, create a simple icon-style thumbnail
            return CreateUnknownTypeThumbnail(request.outputPath, request.size);
    }
}

bool AssetThumbnailCache::RenderSDFThumbnail(const std::string& assetPath, const std::string& outputPath,
                                              int size, ThumbnailAssetType type) {
    try {
        // Load asset model
        auto model = LoadAssetModel(assetPath);
        if (!model) {
            spdlog::error("Failed to load asset model: {}", assetPath);
            return false;
        }

        // Resize framebuffer if needed
        if (!m_framebuffer || m_framebuffer->GetWidth() != size || m_framebuffer->GetHeight() != size) {
            m_framebuffer = std::make_shared<Framebuffer>();
            if (!m_framebuffer->Create(size, size, 1, true)) {
                spdlog::error("Failed to create framebuffer for thumbnail");
                return false;
            }
        }

        // Get model bounds
        auto [boundsMin, boundsMax] = model->GetBounds();
        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        glm::vec3 extents = boundsMax - boundsMin;
        float maxDim = glm::max(extents.x, glm::max(extents.y, extents.z));

        // Avoid zero size bounds
        if (maxDim < 0.001f) maxDim = 1.0f;

        // Setup camera - position for nice 3/4 view
        Camera camera;
        float distance = maxDim * 2.5f;

        // Different angles based on asset type
        float angleH, angleV;
        glm::vec3 targetOffset(0.0f);

        switch (type) {
            case ThumbnailAssetType::Unit:
                // Hero portrait style - front facing, slightly above
                angleH = glm::radians(15.0f);
                angleV = glm::radians(20.0f);
                targetOffset.y = extents.y * 0.15f;  // Look slightly above center
                break;
            case ThumbnailAssetType::Building:
                // Isometric view for buildings
                angleH = glm::radians(45.0f);
                angleV = glm::radians(30.0f);
                break;
            case ThumbnailAssetType::Animated:
            case ThumbnailAssetType::Static:
            default:
                // Standard 3/4 view
                angleH = glm::radians(35.0f);
                angleV = glm::radians(20.0f);
                break;
        }

        glm::vec3 target = center + targetOffset;
        glm::vec3 cameraPos = target + glm::vec3(
            distance * cos(angleV) * sin(angleH),
            distance * sin(angleV),
            distance * cos(angleV) * cos(angleH)
        );

        camera.LookAt(cameraPos, target, glm::vec3(0, 1, 0));
        camera.SetPerspective(35.0f, 1.0f, 0.1f, 1000.0f);

        // Configure renderer for icon rendering
        auto& settings = m_renderer->GetSettings();
        settings.maxSteps = 128;
        settings.enableShadows = true;
        settings.enableAO = true;
        settings.backgroundColor = glm::vec3(0, 0, 0);  // Transparent background
        settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));
        settings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        settings.lightIntensity = 1.2f;

        // Render to framebuffer
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, size, size);

        m_framebuffer->Bind();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_renderer->RenderToTexture(*model, camera, m_framebuffer);

        Framebuffer::Unbind();

        // Read pixels and save to file
        std::vector<uint8_t> pixels(size * size * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer->GetID());
        glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        // Flip vertically (OpenGL origin is bottom-left)
        std::vector<uint8_t> flipped(size * size * 4);
        for (int y = 0; y < size; y++) {
            memcpy(
                flipped.data() + y * size * 4,
                pixels.data() + (size - 1 - y) * size * 4,
                size * 4
            );
        }

        int result = stbi_write_png(outputPath.c_str(), size, size, 4, flipped.data(), size * 4);
        return result != 0;

    } catch (const std::exception& e) {
        spdlog::error("Failed to render thumbnail: {} - {}", assetPath, e.what());
        return false;
    }
}

bool AssetThumbnailCache::RenderMaterialThumbnail(const std::string& assetPath, const std::string& outputPath, int size) {
    try {
        // Create a sphere model to preview the material
        auto model = std::make_unique<SDFModel>("material_preview");
        auto rootPrim = std::make_unique<SDFPrimitive>("sphere", SDFPrimitiveType::Sphere);

        SDFParameters params;
        params.radius = 0.5f;
        rootPrim->SetParameters(params);

        // Load material from file
        SDFMaterial material;
        material.baseColor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
        material.metallic = 0.0f;
        material.roughness = 0.5f;

        try {
            std::ifstream file(assetPath);
            if (file.is_open()) {
                json matData;
                file >> matData;
                file.close();

                if (matData.contains("baseColor")) {
                    const auto& c = matData["baseColor"];
                    material.baseColor = glm::vec4(c[0].get<float>(), c[1].get<float>(), c[2].get<float>(),
                                                    c.size() > 3 ? c[3].get<float>() : 1.0f);
                }
                if (matData.contains("metallic")) {
                    material.metallic = matData["metallic"].get<float>();
                }
                if (matData.contains("roughness")) {
                    material.roughness = matData["roughness"].get<float>();
                }
                if (matData.contains("emissive")) {
                    material.emissive = matData["emissive"].get<float>();
                }
            }
        } catch (...) {
            // Use default material
        }

        rootPrim->SetMaterial(material);
        model->SetRoot(std::move(rootPrim));

        // Render using standard SDF rendering
        return RenderModelToFile(*model, outputPath, size);

    } catch (const std::exception& e) {
        spdlog::error("Failed to render material thumbnail: {}", e.what());
        return false;
    }
}

bool AssetThumbnailCache::RenderModelToFile(const SDFModel& model, const std::string& outputPath, int size) {
    // Ensure framebuffer is ready
    if (!m_framebuffer || m_framebuffer->GetWidth() != size || m_framebuffer->GetHeight() != size) {
        m_framebuffer = std::make_shared<Framebuffer>();
        if (!m_framebuffer->Create(size, size, 1, true)) {
            return false;
        }
    }

    // Get model bounds
    auto [boundsMin, boundsMax] = model.GetBounds();
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 extents = boundsMax - boundsMin;
    float maxDim = glm::max(extents.x, glm::max(extents.y, extents.z));
    if (maxDim < 0.001f) maxDim = 1.0f;

    // Setup camera
    Camera camera;
    float distance = maxDim * 2.5f;
    float angleH = glm::radians(35.0f);
    float angleV = glm::radians(20.0f);

    glm::vec3 cameraPos = center + glm::vec3(
        distance * cos(angleV) * sin(angleH),
        distance * sin(angleV),
        distance * cos(angleV) * cos(angleH)
    );

    camera.LookAt(cameraPos, center, glm::vec3(0, 1, 0));
    camera.SetPerspective(35.0f, 1.0f, 0.1f, 1000.0f);

    // Configure renderer
    auto& settings = m_renderer->GetSettings();
    settings.maxSteps = 128;
    settings.enableShadows = true;
    settings.enableAO = true;
    settings.backgroundColor = glm::vec3(0, 0, 0);
    settings.lightDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.5f));

    // Render
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, size, size);

    m_framebuffer->Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_renderer->RenderToTexture(model, camera, m_framebuffer);
    Framebuffer::Unbind();

    // Save to file
    std::vector<uint8_t> pixels(size * size * 4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer->GetID());
    glReadPixels(0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Flip vertically
    std::vector<uint8_t> flipped(size * size * 4);
    for (int y = 0; y < size; y++) {
        memcpy(flipped.data() + y * size * 4, pixels.data() + (size - 1 - y) * size * 4, size * 4);
    }

    return stbi_write_png(outputPath.c_str(), size, size, 4, flipped.data(), size * 4) != 0;
}

bool AssetThumbnailCache::CreateUnknownTypeThumbnail(const std::string& outputPath, int size) {
    // Create a simple gradient/icon for unknown types
    std::vector<uint8_t> pixels(size * size * 4);
    PlaceholderStyle style = GetPlaceholderStyle(ThumbnailAssetType::Unknown);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int index = (y * size + x) * 4;

            // Create a simple gradient with rounded corners
            float fx = (float)x / (float)size;
            float fy = (float)y / (float)size;
            float dist = glm::length(glm::vec2(fx - 0.5f, fy - 0.5f));

            if (dist < 0.45f) {
                float t = fy;
                glm::vec4 color = glm::mix(style.primaryColor, style.secondaryColor, t);

                pixels[index + 0] = static_cast<uint8_t>(color.r * 255);
                pixels[index + 1] = static_cast<uint8_t>(color.g * 255);
                pixels[index + 2] = static_cast<uint8_t>(color.b * 255);
                pixels[index + 3] = 255;
            } else {
                pixels[index + 0] = 0;
                pixels[index + 1] = 0;
                pixels[index + 2] = 0;
                pixels[index + 3] = 0;
            }
        }
    }

    return stbi_write_png(outputPath.c_str(), size, size, 4, pixels.data(), size * 4) != 0;
}

bool AssetThumbnailCache::CopyTextureThumbnail(const std::string& assetPath, const std::string& outputPath, int size) {
    int width, height, channels;
    unsigned char* data = stbi_load(assetPath.c_str(), &width, &height, &channels, 4);

    if (!data) {
        spdlog::error("Failed to load texture: {}", assetPath);
        return false;
    }

    // Bilinear resize for better quality
    std::vector<uint8_t> resized(size * size * 4);
    float scaleX = (float)(width - 1) / (float)(size - 1);
    float scaleY = (float)(height - 1) / (float)(size - 1);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float srcX = x * scaleX;
            float srcY = y * scaleY;

            int x0 = (int)srcX;
            int y0 = (int)srcY;
            int x1 = std::min(x0 + 1, width - 1);
            int y1 = std::min(y0 + 1, height - 1);

            float fx = srcX - x0;
            float fy = srcY - y0;

            int dstIndex = (y * size + x) * 4;

            for (int c = 0; c < 4; c++) {
                float v00 = data[(y0 * width + x0) * 4 + c];
                float v10 = data[(y0 * width + x1) * 4 + c];
                float v01 = data[(y1 * width + x0) * 4 + c];
                float v11 = data[(y1 * width + x1) * 4 + c];

                float v0 = v00 + fx * (v10 - v00);
                float v1 = v01 + fx * (v11 - v01);
                float v = v0 + fy * (v1 - v0);

                resized[dstIndex + c] = static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }

    stbi_image_free(data);

    int result = stbi_write_png(outputPath.c_str(), size, size, 4, resized.data(), size * 4);
    return result != 0;
}

// ============================================================================
// Asset Model Loading (Full Implementation)
// ============================================================================

std::unique_ptr<SDFModel> AssetThumbnailCache::LoadAssetModel(const std::string& assetPath) {
    // Read JSON file
    std::ifstream file(assetPath);
    if (!file.is_open()) {
        spdlog::error("Failed to open asset file: {}", assetPath);
        return nullptr;
    }

    json assetData;
    try {
        file >> assetData;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse JSON: {} - {}", assetPath, e.what());
        return nullptr;
    }
    file.close();

    // Verify it has an SDF model
    if (!assetData.contains("sdfModel")) {
        spdlog::error("Asset does not contain sdfModel: {}", assetPath);
        return nullptr;
    }

    json sdfData = assetData["sdfModel"];

    // Create model
    std::string modelName = assetData.value("name", "asset_model");
    auto model = std::make_unique<SDFModel>(modelName);

    // Parse primitives
    if (!sdfData.contains("primitives") || sdfData["primitives"].empty()) {
        spdlog::error("SDF model has no primitives: {}", assetPath);
        return nullptr;
    }

    // Create root primitive (first one becomes root)
    bool isFirst = true;
    SDFPrimitive* root = nullptr;

    for (const auto& primData : sdfData["primitives"]) {
        // Parse primitive type
        std::string typeStr = primData.value("type", "Sphere");
        SDFPrimitiveType type = SDFPrimitiveType::Sphere;

        if (typeStr == "Sphere") type = SDFPrimitiveType::Sphere;
        else if (typeStr == "Box") type = SDFPrimitiveType::Box;
        else if (typeStr == "RoundedBox") type = SDFPrimitiveType::RoundedBox;
        else if (typeStr == "Ellipsoid") type = SDFPrimitiveType::Ellipsoid;
        else if (typeStr == "Cylinder") type = SDFPrimitiveType::Cylinder;
        else if (typeStr == "Capsule") type = SDFPrimitiveType::Capsule;
        else if (typeStr == "Torus") type = SDFPrimitiveType::Torus;
        else if (typeStr == "Cone") type = SDFPrimitiveType::Cone;
        else if (typeStr == "Plane") type = SDFPrimitiveType::Plane;
        else if (typeStr == "Pyramid") type = SDFPrimitiveType::Pyramid;
        else if (typeStr == "Prism") type = SDFPrimitiveType::Prism;

        // Create primitive
        std::string primName = primData.value("id", primData.value("name", "primitive"));
        SDFPrimitive* prim = nullptr;

        if (isFirst) {
            auto rootPrim = std::make_unique<SDFPrimitive>(primName, type);
            root = rootPrim.get();
            model->SetRoot(std::move(rootPrim));
            prim = root;
            isFirst = false;
        } else {
            prim = model->CreatePrimitive(primName, type, root);
        }

        if (!prim) continue;

        // Parse transform
        SDFTransform transform;

        if (primData.contains("transform")) {
            const auto& trans = primData["transform"];

            if (trans.contains("position") && trans["position"].is_array() && trans["position"].size() >= 3) {
                const auto& pos = trans["position"];
                transform.position = glm::vec3(
                    pos[0].get<float>(),
                    pos[1].get<float>(),
                    pos[2].get<float>()
                );
            }

            if (trans.contains("rotation") && trans["rotation"].is_array() && trans["rotation"].size() >= 4) {
                const auto& rot = trans["rotation"];
                transform.rotation = glm::quat(
                    rot[3].get<float>(),  // w
                    rot[0].get<float>(),  // x
                    rot[1].get<float>(),  // y
                    rot[2].get<float>()   // z
                );
            }

            if (trans.contains("scale") && trans["scale"].is_array() && trans["scale"].size() >= 3) {
                const auto& scl = trans["scale"];
                transform.scale = glm::vec3(
                    scl[0].get<float>(),
                    scl[1].get<float>(),
                    scl[2].get<float>()
                );
            }
        }

        prim->SetLocalTransform(transform);

        // Parse parameters
        SDFParameters params = prim->GetParameters();

        if (primData.contains("params")) {
            const auto& paramsJson = primData["params"];

            if (paramsJson.contains("radius")) {
                params.radius = paramsJson["radius"].get<float>();
                if (type == SDFPrimitiveType::Cone) {
                    params.bottomRadius = params.radius;
                }
                if (type == SDFPrimitiveType::Torus) {
                    params.majorRadius = params.radius;
                }
            }
            if (paramsJson.contains("size") && paramsJson["size"].is_array()) {
                const auto& size = paramsJson["size"];
                params.dimensions = glm::vec3(
                    size[0].get<float>(),
                    size[1].get<float>(),
                    size[2].get<float>()
                );
            }
            if (paramsJson.contains("radii") && paramsJson["radii"].is_array()) {
                const auto& radii = paramsJson["radii"];
                params.radii = glm::vec3(
                    radii[0].get<float>(),
                    radii[1].get<float>(),
                    radii[2].get<float>()
                );
            }
            if (paramsJson.contains("height")) {
                params.height = paramsJson["height"].get<float>();
            }
            if (paramsJson.contains("tubeRadius")) {
                params.minorRadius = paramsJson["tubeRadius"].get<float>();
            }
            if (paramsJson.contains("cornerRadius")) {
                params.cornerRadius = paramsJson["cornerRadius"].get<float>();
            }
            if (paramsJson.contains("radius1") || paramsJson.contains("radius2")) {
                float r1 = paramsJson.value("radius1", 0.1f);
                float r2 = paramsJson.value("radius2", 0.1f);
                params.bottomRadius = std::max(r1, r2);
                params.topRadius = std::min(r1, r2);
                params.radius = params.bottomRadius;
            }

            // Onion shell parameters
            if (paramsJson.contains("onionThickness")) {
                params.onionThickness = paramsJson["onionThickness"].get<float>();
                params.flags |= 1;  // SDF_FLAG_ONION
            }
            if (paramsJson.contains("shellMinY")) {
                params.shellMinY = paramsJson["shellMinY"].get<float>();
                params.flags |= 2;  // SDF_FLAG_SHELL_BOUNDED
            }
            if (paramsJson.contains("shellMaxY")) {
                params.shellMaxY = paramsJson["shellMaxY"].get<float>();
                params.flags |= 2;
            }
        }

        prim->SetParameters(params);

        // Parse material
        SDFMaterial material = prim->GetMaterial();

        if (primData.contains("material")) {
            const auto& mat = primData["material"];

            if (mat.contains("baseColor") && mat["baseColor"].is_array()) {
                const auto& color = mat["baseColor"];
                material.baseColor = glm::vec4(
                    color[0].get<float>(),
                    color[1].get<float>(),
                    color[2].get<float>(),
                    color.size() > 3 ? color[3].get<float>() : 1.0f
                );
            } else if (mat.contains("albedo") && mat["albedo"].is_array()) {
                const auto& color = mat["albedo"];
                material.baseColor = glm::vec4(
                    color[0].get<float>(),
                    color[1].get<float>(),
                    color[2].get<float>(),
                    1.0f
                );
            }

            if (mat.contains("metallic")) {
                material.metallic = mat["metallic"].get<float>();
            }
            if (mat.contains("roughness")) {
                material.roughness = mat["roughness"].get<float>();
            }
            if (mat.contains("emissive") && mat["emissive"].is_array()) {
                const auto& emissive = mat["emissive"];
                material.emissiveColor = glm::vec3(
                    emissive[0].get<float>(),
                    emissive[1].get<float>(),
                    emissive[2].get<float>()
                );
                material.emissive = mat.value("emissiveStrength", 1.0f);
            }
            if (mat.contains("emissiveColor") && mat["emissiveColor"].is_array()) {
                const auto& emissive = mat["emissiveColor"];
                material.emissiveColor = glm::vec3(
                    emissive[0].get<float>(),
                    emissive[1].get<float>(),
                    emissive[2].get<float>()
                );
                if (mat.contains("emissiveIntensity")) {
                    material.emissive = mat["emissiveIntensity"].get<float>();
                }
            }
        }

        prim->SetMaterial(material);

        // Parse CSG operation
        if (primData.contains("operation")) {
            std::string opStr = primData["operation"].get<std::string>();

            if (opStr == "Union")
                prim->SetCSGOperation(CSGOperation::Union);
            else if (opStr == "Subtraction")
                prim->SetCSGOperation(CSGOperation::Subtraction);
            else if (opStr == "Intersection")
                prim->SetCSGOperation(CSGOperation::Intersection);
            else if (opStr == "SmoothUnion" || opStr == "CubicSmoothUnion" || opStr == "ExponentialSmoothUnion")
                prim->SetCSGOperation(CSGOperation::SmoothUnion);
            else if (opStr == "SmoothSubtraction")
                prim->SetCSGOperation(CSGOperation::SmoothSubtraction);
            else if (opStr == "SmoothIntersection")
                prim->SetCSGOperation(CSGOperation::SmoothIntersection);

            if (primData.contains("smoothness")) {
                SDFParameters p = prim->GetParameters();
                p.smoothness = primData["smoothness"].get<float>();
                prim->SetParameters(p);
            }
        }
    }

    return model;
}

// ============================================================================
// Placeholder Texture Creation
// ============================================================================

void AssetThumbnailCache::CreatePlaceholderTextures() {
    // Create typed placeholders for each asset type
    m_placeholderTextures[ThumbnailAssetType::Static] = CreateTypedPlaceholder(ThumbnailAssetType::Static, 256);
    m_placeholderTextures[ThumbnailAssetType::Animated] = CreateTypedPlaceholder(ThumbnailAssetType::Animated, 256);
    m_placeholderTextures[ThumbnailAssetType::Unit] = CreateTypedPlaceholder(ThumbnailAssetType::Unit, 256);
    m_placeholderTextures[ThumbnailAssetType::Building] = CreateTypedPlaceholder(ThumbnailAssetType::Building, 256);
    m_placeholderTextures[ThumbnailAssetType::Texture] = CreateTypedPlaceholder(ThumbnailAssetType::Texture, 256);
    m_placeholderTextures[ThumbnailAssetType::Unknown] = CreateTypedPlaceholder(ThumbnailAssetType::Unknown, 256);

    // Default placeholder
    m_placeholderTexture = m_placeholderTextures[ThumbnailAssetType::Unknown];
}

std::shared_ptr<Texture> AssetThumbnailCache::CreateTypedPlaceholder(ThumbnailAssetType type, int size) {
    PlaceholderStyle style = GetPlaceholderStyle(type);

    std::vector<uint8_t> pixels(size * size * 4);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int index = (y * size + x) * 4;

            // Normalized coordinates
            float fx = (float)x / (float)size;
            float fy = (float)y / (float)size;

            // Distance from center for rounded corners
            glm::vec2 center(0.5f, 0.5f);
            glm::vec2 pos(fx, fy);
            float dist = glm::length(pos - center);

            // Create rounded rectangle shape
            float cornerDist = 0.4f;

            if (dist < cornerDist) {
                // Gradient from top to bottom
                float t = fy;
                glm::vec4 color = glm::mix(style.primaryColor, style.secondaryColor, t);

                // Add a simple loading indicator pattern (spinning dots)
                float angle = atan2(fy - 0.5f, fx - 0.5f);
                float ringDist = glm::length(glm::vec2(fx - 0.5f, fy - 0.5f));

                if (ringDist > 0.25f && ringDist < 0.35f) {
                    // Create dots around the ring
                    float dotAngle = fmod(angle + 3.14159f, 3.14159f * 2.0f / 8.0f);
                    if (dotAngle < 0.3f) {
                        color = glm::mix(color, glm::vec4(1.0f), 0.5f);
                    }
                }

                pixels[index + 0] = static_cast<uint8_t>(color.r * 255);
                pixels[index + 1] = static_cast<uint8_t>(color.g * 255);
                pixels[index + 2] = static_cast<uint8_t>(color.b * 255);
                pixels[index + 3] = 255;
            } else {
                // Transparent outside
                pixels[index + 0] = 0;
                pixels[index + 1] = 0;
                pixels[index + 2] = 0;
                pixels[index + 3] = 0;
            }
        }
    }

    auto texture = std::make_shared<Texture>();
    texture->Create(size, size, TextureFormat::RGBA, pixels.data());
    return texture;
}

std::shared_ptr<Texture> AssetThumbnailCache::CreatePlaceholderTexture(int size) {
    return CreateTypedPlaceholder(ThumbnailAssetType::Unknown, size);
}

std::shared_ptr<Texture> AssetThumbnailCache::GetPlaceholderForType(ThumbnailAssetType type, int size) {
    auto it = m_placeholderTextures.find(type);
    if (it != m_placeholderTextures.end()) {
        return it->second;
    }
    return m_placeholderTexture;
}

// ============================================================================
// Cache Manifest (Disk Persistence)
// ============================================================================

bool AssetThumbnailCache::LoadCacheManifest() {
    std::string manifestPath = (fs::path(m_cacheDirectory) / "manifest.json").string();

    if (!fs::exists(manifestPath)) {
        return true; // No manifest yet, that's okay
    }

    try {
        std::ifstream file(manifestPath);
        json data;
        file >> data;
        file.close();

        // Verify manifest version
        std::string version = data.value("version", "1.0");
        if (version != "1.0" && version != "1.1") {
            spdlog::warn("Cache manifest version mismatch, clearing cache");
            return true;
        }

        // Parse cache entries
        if (data.contains("thumbnails")) {
            for (const auto& entry : data["thumbnails"]) {
                ThumbnailCache cache;
                cache.assetPath = entry.value("assetPath", "");
                cache.thumbnailPath = entry.value("thumbnailPath", "");
                cache.assetTimestamp = entry.value("assetTimestamp", 0ULL);
                cache.thumbnailTimestamp = entry.value("thumbnailTimestamp", 0ULL);
                cache.size = entry.value("size", 256);

                // Check if both asset and thumbnail files still exist
                if (!fs::exists(cache.assetPath) || !fs::exists(cache.thumbnailPath)) {
                    continue;
                }

                // Verify timestamps match
                uint64_t currentAssetTime = GetFileTimestamp(cache.assetPath);
                if (currentAssetTime != cache.assetTimestamp) {
                    // Asset was modified, invalidate
                    try {
                        fs::remove(cache.thumbnailPath);
                    } catch (...) {}
                    continue;
                }

                // Load the cached texture
                cache.texture = std::make_shared<Texture>();
                if (cache.texture->Load(cache.thumbnailPath)) {
                    cache.isValid = true;
                    std::string key = GetCacheKey(cache.assetPath, cache.size);
                    m_cache[key] = cache;
                }
            }
        }

        spdlog::info("Loaded {} cached thumbnails from manifest", m_cache.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load cache manifest: {}", e.what());
        return false;
    }
}

bool AssetThumbnailCache::SaveCacheManifest() {
    std::string manifestPath = (fs::path(m_cacheDirectory) / "manifest.json").string();

    try {
        json data;
        json thumbnails = json::array();

        std::lock_guard<std::mutex> lock(m_cacheMutex);
        for (const auto& [key, cache] : m_cache) {
            if (!cache.isValid) continue;
            if (!fs::exists(cache.thumbnailPath)) continue;

            json entry;
            entry["assetPath"] = cache.assetPath;
            entry["thumbnailPath"] = cache.thumbnailPath;
            entry["assetTimestamp"] = cache.assetTimestamp;
            entry["thumbnailTimestamp"] = cache.thumbnailTimestamp;
            entry["size"] = cache.size;
            thumbnails.push_back(entry);
        }

        data["thumbnails"] = thumbnails;
        data["version"] = "1.1";
        data["generatedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::ofstream file(manifestPath);
        file << data.dump(2);
        file.close();

        spdlog::info("Saved {} thumbnails to manifest", thumbnails.size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save cache manifest: {}", e.what());
        return false;
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

uint64_t AssetThumbnailCache::GetFileTimestamp(const std::string& path) const {
    if (!fs::exists(path)) return 0;

    try {
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count();
    } catch (...) {
        return 0;
    }
}

} // namespace Nova
