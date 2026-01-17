/**
 * @file AssetThumbnailCache.cpp
 * @brief Implementation of automatic thumbnail generation system
 */

#include "AssetThumbnailCache.hpp"
#include <spdlog/spdlog.h>
#include "../graphics/SDFRenderer.hpp"
#include "../graphics/Framebuffer.hpp"
#include "../core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_image_write.h>

using json = nlohmann::json;

namespace Nova {

AssetThumbnailCache::AssetThumbnailCache() = default;

AssetThumbnailCache::~AssetThumbnailCache() {
    Shutdown();
}

bool AssetThumbnailCache::Initialize(const std::string& cacheDirectory, const std::string& assetDirectory) {
    if (m_initialized) {
        spdlog::warn("AssetThumbnailCache already initialized");
        return true;
    }

    m_cacheDirectory = cacheDirectory;
    m_assetDirectory = assetDirectory;

    // Create cache directory if it doesn't exist
    if (!fs::exists(m_cacheDirectory)) {
        fs::create_directories(m_cacheDirectory);
    }

    // Initialize SDF renderer
    m_renderer = std::make_unique<SDFRenderer>();
    if (!m_renderer->Initialize()) {
        spdlog::error("Failed to initialize SDF renderer for thumbnails");
        return false;
    }

    // Create framebuffer for rendering (will resize as needed)
    m_framebuffer = std::make_shared<Framebuffer>();

    // Create placeholder texture
    m_placeholderTexture = CreatePlaceholderTexture(256);

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

    m_initialized = false;
    spdlog::info("AssetThumbnailCache shut down");
}

std::shared_ptr<Texture> AssetThumbnailCache::GetThumbnail(const std::string& assetPath, int size, int priority) {
    if (!m_initialized) {
        return m_placeholderTexture;
    }

    std::string cacheKey = GetCacheKey(assetPath, size);

    // Check cache
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

    // Queue generation request
    ThumbnailRequest request;
    request.assetPath = assetPath;
    request.outputPath = GetThumbnailPath(assetPath, size);
    request.size = size;
    request.assetType = DetectAssetType(assetPath);
    request.fileTimestamp = GetFileTimestamp(assetPath);
    request.priority = priority;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(request);
    }

    // Return placeholder while generating
    return m_placeholderTexture;
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

        // Check if already being generated
        std::string cacheKey = GetCacheKey(request.assetPath, request.size);
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_cache.find(cacheKey);
            if (it != m_cache.end() && it->second.isGenerating) {
                continue; // Skip if already generating
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
                    entry.texture = m_placeholderTexture;
                    entry.isValid = false;
                }
            } else {
                entry.texture = m_placeholderTexture;
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

    for (const auto& entry : fs::recursive_directory_iterator(m_assetDirectory)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        if (ext != ".json" && ext != ".png" && ext != ".jpg") continue;

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

ThumbnailAssetType AssetThumbnailCache::DetectAssetType(const std::string& assetPath) {
    fs::path path(assetPath);
    std::string ext = path.extension().string();

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
        return ThumbnailAssetType::Texture;
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

        if (data.contains("type")) {
            std::string type = data["type"].get<std::string>();
            if (type == "unit" || type == "Unit") return ThumbnailAssetType::Unit;
            if (type == "building" || type == "Building") return ThumbnailAssetType::Building;
        }

        if (data.contains("animations") && !data["animations"].empty()) {
            return ThumbnailAssetType::Animated;
        }

        if (data.contains("sdfModel")) {
            return ThumbnailAssetType::Static;
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to detect asset type: {} - {}", assetPath, e.what());
    }

    return ThumbnailAssetType::Unknown;
}

std::string AssetThumbnailCache::GetCacheKey(const std::string& assetPath, int size) const {
    return assetPath + "_" + std::to_string(size);
}

std::string AssetThumbnailCache::GetThumbnailPath(const std::string& assetPath, int size) const {
    fs::path path(assetPath);
    std::string filename = path.stem().string() + "_" + std::to_string(size) + ".png";

    // Create subdirectory structure mirroring asset directory
    fs::path relativePath = fs::relative(path.parent_path(), m_assetDirectory);
    fs::path outputDir = fs::path(m_cacheDirectory) / relativePath;

    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    return (outputDir / filename).string();
}

bool AssetThumbnailCache::GenerateThumbnail(const ThumbnailRequest& request) {
    if (request.assetType == ThumbnailAssetType::Texture) {
        return CopyTextureThumbnail(request.assetPath, request.outputPath, request.size);
    }

    if (request.assetType == ThumbnailAssetType::Static ||
        request.assetType == ThumbnailAssetType::Animated ||
        request.assetType == ThumbnailAssetType::Unit ||
        request.assetType == ThumbnailAssetType::Building) {
        return RenderSDFThumbnail(request.assetPath, request.outputPath, request.size, request.assetType);
    }

    spdlog::warn("Unsupported asset type for thumbnail: {}", request.assetPath);
    return false;
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

        // Setup camera
        Camera camera;
        float distance = maxDim * 2.5f;
        float angleH = glm::radians(45.0f);
        float angleV = glm::radians(15.0f);

        glm::vec3 cameraPos = center + glm::vec3(
            distance * cos(angleV) * sin(angleH),
            distance * sin(angleV) + center.y,
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
        settings.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        settings.lightIntensity = 1.2f;

        // Render to framebuffer
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, size, size);

        m_framebuffer->Bind();
        m_framebuffer->Clear(glm::vec4(0, 0, 0, 0));

        m_renderer->RenderToTexture(*model, camera, m_framebuffer);

        Framebuffer::Unbind();

        // Save to file
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

bool AssetThumbnailCache::CopyTextureThumbnail(const std::string& assetPath, const std::string& outputPath, int size) {
    int width, height, channels;
    unsigned char* data = stbi_load(assetPath.c_str(), &width, &height, &channels, 4);

    if (!data) {
        spdlog::error("Failed to load texture: {}", assetPath);
        return false;
    }

    // Simple resize (nearest neighbor)
    std::vector<uint8_t> resized(size * size * 4);
    float scaleX = (float)width / (float)size;
    float scaleY = (float)height / (float)size;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int srcX = (int)(x * scaleX);
            int srcY = (int)(y * scaleY);
            int srcIndex = (srcY * width + srcX) * 4;
            int dstIndex = (y * size + x) * 4;

            resized[dstIndex + 0] = data[srcIndex + 0];
            resized[dstIndex + 1] = data[srcIndex + 1];
            resized[dstIndex + 2] = data[srcIndex + 2];
            resized[dstIndex + 3] = data[srcIndex + 3];
        }
    }

    stbi_image_free(data);

    int result = stbi_write_png(outputPath.c_str(), size, size, 4, resized.data(), size * 4);
    return result != 0;
}

std::unique_ptr<SDFModel> AssetThumbnailCache::LoadAssetModel(const std::string& assetPath) {
    // This is a simplified version - would use the same logic as asset_media_renderer.cpp
    // For brevity, returning nullptr (full implementation would parse JSON and build model)
    spdlog::error("LoadAssetModel not yet implemented");
    return nullptr;
}

std::shared_ptr<Texture> AssetThumbnailCache::CreatePlaceholderTexture(int size) {
    // Create simple checkerboard pattern
    std::vector<uint8_t> pixels(size * size * 4);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int checker = ((x / 16) + (y / 16)) % 2;
            uint8_t color = checker ? 64 : 96;
            int index = (y * size + x) * 4;
            pixels[index + 0] = color;
            pixels[index + 1] = color;
            pixels[index + 2] = color;
            pixels[index + 3] = 255;
        }
    }

    auto texture = std::make_shared<Texture>();
    texture->Create(size, size, TextureFormat::RGBA, pixels.data());
    return texture;
}

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

        // Parse cache entries
        if (data.contains("thumbnails")) {
            for (const auto& entry : data["thumbnails"]) {
                ThumbnailCache cache;
                cache.assetPath = entry.value("assetPath", "");
                cache.thumbnailPath = entry.value("thumbnailPath", "");
                cache.assetTimestamp = entry.value("assetTimestamp", 0ULL);
                cache.thumbnailTimestamp = entry.value("thumbnailTimestamp", 0ULL);
                cache.size = entry.value("size", 256);

                // Check if thumbnail file still exists
                if (fs::exists(cache.thumbnailPath)) {
                    cache.texture = std::make_shared<Texture>();
                    if (cache.texture->Load(cache.thumbnailPath)) {
                        cache.isValid = true;
                        std::string key = GetCacheKey(cache.assetPath, cache.size);
                        m_cache[key] = cache;
                    }
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

            json entry;
            entry["assetPath"] = cache.assetPath;
            entry["thumbnailPath"] = cache.thumbnailPath;
            entry["assetTimestamp"] = cache.assetTimestamp;
            entry["thumbnailTimestamp"] = cache.thumbnailTimestamp;
            entry["size"] = cache.size;
            thumbnails.push_back(entry);
        }

        data["thumbnails"] = thumbnails;
        data["version"] = "1.0";

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
