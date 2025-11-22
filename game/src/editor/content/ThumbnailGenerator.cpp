#include "ThumbnailGenerator.hpp"
#include "../Editor.hpp"

#include <fstream>
#include <filesystem>
#include <cstring>

// Simple image writing (would use stb_image_write in real implementation)
namespace {
    bool WritePNG(const std::string& path, const unsigned char* pixels, int width, int height) {
        // Simplified PNG header + IDAT chunk
        // In real implementation, use stb_image_write or libpng
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        // PNG signature
        unsigned char signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
        file.write(reinterpret_cast<char*>(signature), 8);

        // For now, just write placeholder
        return file.good();
    }
}

namespace Vehement {
namespace Content {

ThumbnailGenerator::ThumbnailGenerator(Editor* editor)
    : m_editor(editor)
{
}

ThumbnailGenerator::~ThumbnailGenerator() {
    Shutdown();
}

bool ThumbnailGenerator::Initialize(const ThumbnailGeneratorConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Create cache directory
    if (!EnsureCacheDirectory()) {
        return false;
    }

    // Generate default placeholders
    GenerateIconThumbnail(AssetType::Unknown, m_loadingPlaceholder,
                          static_cast<int>(ThumbnailSize::Medium));
    GenerateIconThumbnail(AssetType::Unknown, m_errorPlaceholder,
                          static_cast<int>(ThumbnailSize::Medium));

    // Start worker threads
    StartWorkerThreads();

    m_initialized = true;
    return true;
}

void ThumbnailGenerator::Shutdown() {
    if (!m_initialized) {
        return;
    }

    StopWorkerThreads();

    // Clear queues
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_requestQueue.empty()) {
            m_requestQueue.pop();
        }
    }

    m_cache.clear();
    m_initialized = false;
}

void ThumbnailGenerator::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Process completed thumbnails on main thread
    std::vector<CompletedThumbnail> completed;
    {
        std::lock_guard<std::mutex> lock(m_completedMutex);
        while (!m_completedThumbnails.empty()) {
            completed.push_back(m_completedThumbnails.front());
            m_completedThumbnails.pop();
        }
    }

    for (const auto& item : completed) {
        if (item.success) {
            if (item.callback) {
                item.callback(item.path);
            }
            if (OnThumbnailReady) {
                OnThumbnailReady(item.assetId, item.path);
            }
        } else {
            if (OnThumbnailFailed) {
                OnThumbnailFailed(item.assetId, item.error);
            }
        }
    }

    // Check if all thumbnails generated
    if (m_pendingCount == 0 && m_completedRequests > 0 && m_totalRequests > 0 &&
        m_completedRequests >= m_totalRequests) {
        if (OnAllThumbnailsGenerated) {
            OnAllThumbnailsGenerated();
        }
        m_totalRequests = 0;
        m_completedRequests = 0;
    }
}

uint64_t ThumbnailGenerator::RequestThumbnail(const std::string& assetId,
                                               std::function<void(const std::string&)> callback) {
    ThumbnailRequest request;
    request.assetId = assetId;
    request.size = m_config.defaultSize;
    request.format = m_config.format;
    request.callback = callback;

    return RequestThumbnail(request);
}

uint64_t ThumbnailGenerator::RequestThumbnail(const ThumbnailRequest& request) {
    // Check cache first
    std::string cacheKey = request.assetId + "_" + std::to_string(static_cast<int>(request.size));

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cache.find(cacheKey);
        if (it != m_cache.end() && it->second.valid) {
            m_cacheStats.hitCount++;

            // Callback with cached path
            if (request.callback) {
                request.callback(it->second.thumbnailPath);
            }
            return 0;  // No request needed
        }
        m_cacheStats.missCount++;
    }

    uint64_t requestId = m_nextRequestId++;
    m_pendingCount++;
    m_totalRequests++;

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(request);
    }
    m_queueCondition.notify_one();

    return requestId;
}

std::string ThumbnailGenerator::GenerateThumbnail(const std::string& assetId, ThumbnailSize size) {
    std::string outputPath = GetCachePath(assetId, size);
    std::string sourcePath = GetAssetSourcePath(assetId);
    int sizeInt = static_cast<int>(size);

    // Determine asset type and generate appropriate thumbnail
    AssetType type = AssetType::Unknown;

    // Parse type from asset ID
    if (assetId.find("unit") != std::string::npos) {
        type = AssetType::Unit;
    } else if (assetId.find("spell") != std::string::npos) {
        type = AssetType::Spell;
    } else if (assetId.find("building") != std::string::npos) {
        type = AssetType::Building;
    } else if (assetId.find("tile") != std::string::npos) {
        type = AssetType::Tile;
    } else if (assetId.find("effect") != std::string::npos) {
        type = AssetType::Effect;
    } else if (assetId.find("hero") != std::string::npos) {
        type = AssetType::Hero;
    } else if (assetId.find("ability") != std::string::npos) {
        type = AssetType::Ability;
    }

    // Check for custom renderer
    auto rendererIt = m_customRenderers.find(type);
    if (rendererIt != m_customRenderers.end()) {
        if (rendererIt->second(sourcePath, outputPath, sizeInt)) {
            return outputPath;
        }
    }

    // Generate config thumbnail (icon based on type)
    if (GenerateConfigThumbnail(sourcePath, type, outputPath, sizeInt)) {
        // Add to cache
        ThumbnailCacheEntry entry;
        entry.assetId = assetId;
        entry.thumbnailPath = outputPath;
        entry.size = size;
        entry.generatedTime = std::chrono::system_clock::now();
        entry.valid = true;

        std::string cacheKey = assetId + "_" + std::to_string(sizeInt);
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cache[cacheKey] = entry;
            m_cacheStats.generatedCount++;
        }

        return outputPath;
    }

    return "";
}

void ThumbnailGenerator::GenerateAllThumbnails(bool async) {
    // Would iterate all assets from database and queue thumbnails
    // For now, this is a placeholder
}

void ThumbnailGenerator::CancelRequest(uint64_t requestId) {
    // Would need to track requests by ID to cancel
    // For simplicity, not implemented
}

void ThumbnailGenerator::CancelAllRequests() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    while (!m_requestQueue.empty()) {
        m_requestQueue.pop();
    }
    m_pendingCount = 0;
}

std::string ThumbnailGenerator::GetThumbnailPath(const std::string& assetId, ThumbnailSize size) {
    std::string cacheKey = assetId + "_" + std::to_string(static_cast<int>(size));

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cache.find(cacheKey);
    if (it != m_cache.end() && it->second.valid) {
        return it->second.thumbnailPath;
    }

    return GetPlaceholder(AssetType::Unknown, size);
}

bool ThumbnailGenerator::HasValidThumbnail(const std::string& assetId, ThumbnailSize size) {
    std::string cacheKey = assetId + "_" + std::to_string(static_cast<int>(size));

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_cache.find(cacheKey);
    return it != m_cache.end() && it->second.valid;
}

void ThumbnailGenerator::InvalidateThumbnail(const std::string& assetId) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // Remove all sizes for this asset
    std::vector<std::string> keysToRemove;
    for (const auto& [key, entry] : m_cache) {
        if (entry.assetId == assetId) {
            keysToRemove.push_back(key);
        }
    }

    for (const auto& key : keysToRemove) {
        m_cache.erase(key);
    }
}

void ThumbnailGenerator::InvalidateAllThumbnails() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
}

void ThumbnailGenerator::ClearCache() {
    InvalidateAllThumbnails();

    // Delete cache directory
    try {
        std::filesystem::remove_all(m_config.cacheDirectory);
        EnsureCacheDirectory();
    } catch (...) {
        // Ignore errors
    }
}

void ThumbnailGenerator::TrimCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    if (m_cache.size() <= static_cast<size_t>(m_config.maxCacheEntries)) {
        return;
    }

    // Sort by access time and remove oldest
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> entries;
    for (const auto& [key, entry] : m_cache) {
        entries.emplace_back(key, entry.generatedTime);
    }

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });

    // Remove oldest half
    size_t removeCount = entries.size() / 2;
    for (size_t i = 0; i < removeCount; ++i) {
        m_cache.erase(entries[i].first);
    }
}

ThumbnailGenerator::CacheStats ThumbnailGenerator::GetCacheStats() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cacheStats.totalEntries = m_cache.size();
    m_cacheStats.validEntries = std::count_if(m_cache.begin(), m_cache.end(),
                                               [](const auto& p) { return p.second.valid; });
    return m_cacheStats;
}

std::string ThumbnailGenerator::GetPlaceholder(AssetType type, ThumbnailSize size) {
    auto it = m_placeholders.find(type);
    if (it != m_placeholders.end()) {
        return it->second;
    }

    // Generate placeholder on demand
    std::string path = m_config.cacheDirectory + "/placeholder_" +
                       AssetTypeToString(type) + "_" +
                       std::to_string(static_cast<int>(size)) + ".png";

    GenerateIconThumbnail(type, path, static_cast<int>(size));
    m_placeholders[type] = path;

    return path;
}

std::string ThumbnailGenerator::GetLoadingPlaceholder(ThumbnailSize size) {
    return m_loadingPlaceholder;
}

std::string ThumbnailGenerator::GetErrorPlaceholder(ThumbnailSize size) {
    return m_errorPlaceholder;
}

void ThumbnailGenerator::RegisterRenderer(AssetType type, CustomRenderer renderer) {
    m_customRenderers[type] = std::move(renderer);
}

void ThumbnailGenerator::UnregisterRenderer(AssetType type) {
    m_customRenderers.erase(type);
}

size_t ThumbnailGenerator::GetPendingCount() const {
    return m_pendingCount.load();
}

float ThumbnailGenerator::GetProgress() const {
    size_t total = m_totalRequests.load();
    size_t completed = m_completedRequests.load();

    if (total == 0) return 1.0f;
    return static_cast<float>(completed) / static_cast<float>(total);
}

void ThumbnailGenerator::StartWorkerThreads() {
    m_running = true;

    for (int i = 0; i < m_config.workerThreads; ++i) {
        m_workerThreads.emplace_back(&ThumbnailGenerator::WorkerThread, this);
    }
}

void ThumbnailGenerator::StopWorkerThreads() {
    m_running = false;
    m_queueCondition.notify_all();

    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
}

void ThumbnailGenerator::WorkerThread() {
    while (m_running) {
        ThumbnailRequest request;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this] {
                return !m_running || !m_requestQueue.empty();
            });

            if (!m_running && m_requestQueue.empty()) {
                break;
            }

            if (m_requestQueue.empty()) {
                continue;
            }

            request = m_requestQueue.top();
            m_requestQueue.pop();
        }

        // Generate thumbnail
        CompletedThumbnail completed;
        completed.assetId = request.assetId;
        completed.callback = request.callback;

        try {
            std::string path = GenerateThumbnail(request.assetId, request.size);
            if (!path.empty()) {
                completed.success = true;
                completed.path = path;
            } else {
                completed.success = false;
                completed.error = "Failed to generate thumbnail";
            }
        } catch (const std::exception& e) {
            completed.success = false;
            completed.error = e.what();
        }

        {
            std::lock_guard<std::mutex> lock(m_completedMutex);
            m_completedThumbnails.push(completed);
        }

        m_pendingCount--;
        m_completedRequests++;
    }
}

bool ThumbnailGenerator::GenerateModelThumbnail(const std::string& assetPath,
                                                 const std::string& outputPath, int size) {
    // Would render 3D model to framebuffer
    // For now, generate icon
    return GenerateIconThumbnail(AssetType::Model, outputPath, size);
}

bool ThumbnailGenerator::GenerateTextureThumbnail(const std::string& assetPath,
                                                   const std::string& outputPath, int size) {
    // Would load and resize texture
    // For now, generate icon
    return GenerateIconThumbnail(AssetType::Texture, outputPath, size);
}

bool ThumbnailGenerator::GenerateConfigThumbnail(const std::string& assetPath, AssetType type,
                                                  const std::string& outputPath, int size) {
    return GenerateIconThumbnail(type, outputPath, size);
}

bool ThumbnailGenerator::GenerateIconThumbnail(AssetType type, const std::string& outputPath, int size) {
    // Create pixel buffer
    std::vector<unsigned char> pixels(size * size * 4);

    // Generate icon based on type
    switch (type) {
        case AssetType::Unit:
            GenerateUnitIcon(pixels.data(), size);
            break;
        case AssetType::Building:
            GenerateBuildingIcon(pixels.data(), size);
            break;
        case AssetType::Spell:
            GenerateSpellIcon(pixels.data(), size);
            break;
        case AssetType::Effect:
            GenerateEffectIcon(pixels.data(), size);
            break;
        case AssetType::Tile:
            GenerateTileIcon(pixels.data(), size);
            break;
        case AssetType::Hero:
            GenerateHeroIcon(pixels.data(), size);
            break;
        case AssetType::Ability:
            GenerateAbilityIcon(pixels.data(), size);
            break;
        case AssetType::TechTree:
            GenerateTechTreeIcon(pixels.data(), size);
            break;
        default:
            GenerateDefaultIcon(pixels.data(), size);
            break;
    }

    return SaveThumbnail(pixels.data(), size, size, outputPath);
}

void ThumbnailGenerator::GenerateUnitIcon(unsigned char* pixels, int size) {
    // Blue gradient with sword icon silhouette
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Background gradient
            pixels[idx + 0] = static_cast<unsigned char>(60 + 40 * fy);   // R
            pixels[idx + 1] = static_cast<unsigned char>(80 + 60 * fy);   // G
            pixels[idx + 2] = static_cast<unsigned char>(150 + 50 * fy);  // B
            pixels[idx + 3] = 255;  // A

            // Simple sword silhouette
            float cx = 0.5f, cy = 0.5f;
            float dx = fx - cx, dy = fy - cy;

            if (std::abs(dx) < 0.05f && dy > -0.3f && dy < 0.4f) {
                // Blade
                pixels[idx + 0] = 200;
                pixels[idx + 1] = 200;
                pixels[idx + 2] = 220;
            }
            if (std::abs(dy) < 0.05f && dx > -0.2f && dx < 0.2f && dy > 0.1f) {
                // Guard
                pixels[idx + 0] = 139;
                pixels[idx + 1] = 90;
                pixels[idx + 2] = 43;
            }
        }
    }
}

void ThumbnailGenerator::GenerateBuildingIcon(unsigned char* pixels, int size) {
    // Brown/gray building silhouette
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Background
            pixels[idx + 0] = static_cast<unsigned char>(100 + 20 * fy);
            pixels[idx + 1] = static_cast<unsigned char>(80 + 20 * fy);
            pixels[idx + 2] = static_cast<unsigned char>(60 + 20 * fy);
            pixels[idx + 3] = 255;

            // Building shape
            if (fx > 0.2f && fx < 0.8f && fy > 0.3f && fy < 0.9f) {
                pixels[idx + 0] = 139;
                pixels[idx + 1] = 119;
                pixels[idx + 2] = 101;
            }
            // Roof
            if (fy > 0.15f && fy < 0.35f) {
                float roofWidth = 0.7f - (fy - 0.15f) * 2.0f;
                if (fx > 0.5f - roofWidth / 2 && fx < 0.5f + roofWidth / 2) {
                    pixels[idx + 0] = 165;
                    pixels[idx + 1] = 42;
                    pixels[idx + 2] = 42;
                }
            }
        }
    }
}

void ThumbnailGenerator::GenerateSpellIcon(unsigned char* pixels, int size) {
    // Purple/blue magical effect
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            float cx = 0.5f, cy = 0.5f;
            float dist = std::sqrt((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));

            // Radial gradient
            float intensity = 1.0f - dist * 2.0f;
            intensity = std::max(0.0f, intensity);

            pixels[idx + 0] = static_cast<unsigned char>(100 + 100 * intensity);
            pixels[idx + 1] = static_cast<unsigned char>(50 + 50 * intensity);
            pixels[idx + 2] = static_cast<unsigned char>(150 + 80 * intensity);
            pixels[idx + 3] = 255;
        }
    }
}

void ThumbnailGenerator::GenerateEffectIcon(unsigned char* pixels, int size) {
    // Yellow/orange sparkle effect
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            pixels[idx + 0] = static_cast<unsigned char>(200 + 55 * fx);
            pixels[idx + 1] = static_cast<unsigned char>(150 + 50 * fy);
            pixels[idx + 2] = static_cast<unsigned char>(50);
            pixels[idx + 3] = 255;

            // Star pattern
            float cx = 0.5f, cy = 0.5f;
            float dx = fx - cx, dy = fy - cy;
            if (std::abs(dx) < 0.03f || std::abs(dy) < 0.03f ||
                std::abs(dx - dy) < 0.03f || std::abs(dx + dy) < 0.03f) {
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist < 0.35f) {
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 200;
                }
            }
        }
    }
}

void ThumbnailGenerator::GenerateTileIcon(unsigned char* pixels, int size) {
    // Green/brown terrain grid
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;

            int gridX = x / (size / 4);
            int gridY = y / (size / 4);
            bool checker = (gridX + gridY) % 2 == 0;

            if (checker) {
                pixels[idx + 0] = 80;
                pixels[idx + 1] = 150;
                pixels[idx + 2] = 80;
            } else {
                pixels[idx + 0] = 60;
                pixels[idx + 1] = 120;
                pixels[idx + 2] = 60;
            }
            pixels[idx + 3] = 255;
        }
    }
}

void ThumbnailGenerator::GenerateHeroIcon(unsigned char* pixels, int size) {
    // Gold/yellow with crown
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            // Background
            pixels[idx + 0] = static_cast<unsigned char>(50 + 30 * fy);
            pixels[idx + 1] = static_cast<unsigned char>(40 + 20 * fy);
            pixels[idx + 2] = static_cast<unsigned char>(80 + 40 * fy);
            pixels[idx + 3] = 255;

            // Crown base
            if (fy > 0.3f && fy < 0.5f && fx > 0.25f && fx < 0.75f) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 215;
                pixels[idx + 2] = 0;
            }
            // Crown points
            if (fy < 0.35f && fy > 0.15f) {
                if ((fx > 0.25f && fx < 0.32f) ||
                    (fx > 0.45f && fx < 0.55f) ||
                    (fx > 0.68f && fx < 0.75f)) {
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 215;
                    pixels[idx + 2] = 0;
                }
            }
        }
    }
}

void ThumbnailGenerator::GenerateAbilityIcon(unsigned char* pixels, int size) {
    // Teal circular icon
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            float fx = static_cast<float>(x) / size;
            float fy = static_cast<float>(y) / size;

            float cx = 0.5f, cy = 0.5f;
            float dist = std::sqrt((fx - cx) * (fx - cx) + (fy - cy) * (fy - cy));

            if (dist < 0.4f) {
                float intensity = 1.0f - dist / 0.4f;
                pixels[idx + 0] = static_cast<unsigned char>(50 + 100 * intensity);
                pixels[idx + 1] = static_cast<unsigned char>(150 + 50 * intensity);
                pixels[idx + 2] = static_cast<unsigned char>(150 + 50 * intensity);
            } else {
                pixels[idx + 0] = 40;
                pixels[idx + 1] = 60;
                pixels[idx + 2] = 80;
            }
            pixels[idx + 3] = 255;
        }
    }
}

void ThumbnailGenerator::GenerateTechTreeIcon(unsigned char* pixels, int size) {
    // Network/tree pattern
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;

            pixels[idx + 0] = 60;
            pixels[idx + 1] = 60;
            pixels[idx + 2] = 80;
            pixels[idx + 3] = 255;
        }
    }

    // Draw nodes and connections
    auto drawCircle = [&](int cx, int cy, int r, unsigned char r_, unsigned char g_, unsigned char b_) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy <= r * r) {
                    int px = cx + dx;
                    int py = cy + dy;
                    if (px >= 0 && px < size && py >= 0 && py < size) {
                        int idx = (py * size + px) * 4;
                        pixels[idx + 0] = r_;
                        pixels[idx + 1] = g_;
                        pixels[idx + 2] = b_;
                    }
                }
            }
        }
    };

    int nodeSize = size / 12;
    drawCircle(size / 2, size / 4, nodeSize, 100, 200, 100);
    drawCircle(size / 4, size / 2, nodeSize, 100, 150, 200);
    drawCircle(3 * size / 4, size / 2, nodeSize, 100, 150, 200);
    drawCircle(size / 2, 3 * size / 4, nodeSize, 200, 150, 100);
}

void ThumbnailGenerator::GenerateDefaultIcon(unsigned char* pixels, int size) {
    // Gray placeholder with question mark
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int idx = (y * size + x) * 4;
            pixels[idx + 0] = 80;
            pixels[idx + 1] = 80;
            pixels[idx + 2] = 90;
            pixels[idx + 3] = 255;
        }
    }
}

std::string ThumbnailGenerator::GetCachePath(const std::string& assetId, ThumbnailSize size) const {
    std::string ext = ".png";
    if (m_config.format == ThumbnailFormat::JPEG) ext = ".jpg";
    else if (m_config.format == ThumbnailFormat::WebP) ext = ".webp";

    return m_config.cacheDirectory + "/" + assetId + "_" +
           std::to_string(static_cast<int>(size)) + ext;
}

std::string ThumbnailGenerator::GetAssetSourcePath(const std::string& assetId) const {
    // Would query database for actual path
    return "";
}

bool ThumbnailGenerator::EnsureCacheDirectory() {
    try {
        std::filesystem::create_directories(m_config.cacheDirectory);
        return true;
    } catch (...) {
        return false;
    }
}

bool ThumbnailGenerator::SaveThumbnail(const unsigned char* pixels, int width, int height,
                                        const std::string& path) {
    // Ensure parent directory exists
    std::filesystem::path p(path);
    std::filesystem::create_directories(p.parent_path());

    // Would use stb_image_write or similar in real implementation
    // For now, write a simple PPM file that can be converted
    std::string ppmPath = path + ".ppm";
    std::ofstream file(ppmPath, std::ios::binary);
    if (!file.is_open()) return false;

    file << "P6\n" << width << " " << height << "\n255\n";
    for (int i = 0; i < width * height; ++i) {
        file.put(static_cast<char>(pixels[i * 4 + 0]));
        file.put(static_cast<char>(pixels[i * 4 + 1]));
        file.put(static_cast<char>(pixels[i * 4 + 2]));
    }

    return file.good();
}

bool ThumbnailGenerator::ResizeImage(const unsigned char* src, int srcW, int srcH,
                                      unsigned char* dst, int dstW, int dstH) {
    // Simple nearest-neighbor resize
    float xRatio = static_cast<float>(srcW) / dstW;
    float yRatio = static_cast<float>(srcH) / dstH;

    for (int y = 0; y < dstH; ++y) {
        for (int x = 0; x < dstW; ++x) {
            int srcX = static_cast<int>(x * xRatio);
            int srcY = static_cast<int>(y * yRatio);

            int srcIdx = (srcY * srcW + srcX) * 4;
            int dstIdx = (y * dstW + x) * 4;

            dst[dstIdx + 0] = src[srcIdx + 0];
            dst[dstIdx + 1] = src[srcIdx + 1];
            dst[dstIdx + 2] = src[srcIdx + 2];
            dst[dstIdx + 3] = src[srcIdx + 3];
        }
    }

    return true;
}

} // namespace Content
} // namespace Vehement
