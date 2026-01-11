/**
 * @file SDFCache.cpp
 * @brief Implementation of the SDF disk caching system
 */

#include "SDFCache.hpp"
#include "../core/Logger.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef __has_include
#if __has_include(<zlib.h>)
#define NOVA_SDF_CACHE_HAS_ZLIB 1
#include <zlib.h>
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Nova {

// =============================================================================
// Hash Functions (FNV-1a)
// =============================================================================

namespace {

constexpr uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ull;
constexpr uint64_t FNV_PRIME_64 = 1099511628211ull;

uint64_t FNV1aHash(const void* data, size_t size, uint64_t hash = FNV_OFFSET_BASIS_64) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= FNV_PRIME_64;
    }
    return hash;
}

uint64_t HashCombine(uint64_t h1, uint64_t h2) {
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
}

// Trilinear interpolation helper
float TrilinearInterpolate(
    float c000, float c001, float c010, float c011,
    float c100, float c101, float c110, float c111,
    float tx, float ty, float tz)
{
    float c00 = c000 * (1 - tx) + c100 * tx;
    float c01 = c001 * (1 - tx) + c101 * tx;
    float c10 = c010 * (1 - tx) + c110 * tx;
    float c11 = c011 * (1 - tx) + c111 * tx;

    float c0 = c00 * (1 - ty) + c10 * ty;
    float c1 = c01 * (1 - ty) + c11 * ty;

    return c0 * (1 - tz) + c1 * tz;
}

} // anonymous namespace

// =============================================================================
// SDFCacheLOD Implementation
// =============================================================================

float SDFCacheLOD::SampleDistance(const glm::vec3& uvw) const {
    if (distances.empty() || resolution <= 0) {
        return std::numeric_limits<float>::max();
    }

    // Clamp UVW to [0, 1]
    glm::vec3 clamped = glm::clamp(uvw, glm::vec3(0.0f), glm::vec3(1.0f));

    // Convert to voxel coordinates
    float fx = clamped.x * (resolution - 1);
    float fy = clamped.y * (resolution - 1);
    float fz = clamped.z * (resolution - 1);

    // Get integer coordinates
    int x0 = static_cast<int>(fx);
    int y0 = static_cast<int>(fy);
    int z0 = static_cast<int>(fz);

    int x1 = std::min(x0 + 1, resolution - 1);
    int y1 = std::min(y0 + 1, resolution - 1);
    int z1 = std::min(z0 + 1, resolution - 1);

    // Get fractional parts
    float tx = fx - x0;
    float ty = fy - y0;
    float tz = fz - z0;

    // Sample 8 corners
    float c000 = SampleDistanceAt(x0, y0, z0);
    float c001 = SampleDistanceAt(x0, y0, z1);
    float c010 = SampleDistanceAt(x0, y1, z0);
    float c011 = SampleDistanceAt(x0, y1, z1);
    float c100 = SampleDistanceAt(x1, y0, z0);
    float c101 = SampleDistanceAt(x1, y0, z1);
    float c110 = SampleDistanceAt(x1, y1, z0);
    float c111 = SampleDistanceAt(x1, y1, z1);

    // Trilinear interpolation
    return TrilinearInterpolate(c000, c001, c010, c011, c100, c101, c110, c111, tx, ty, tz);
}

float SDFCacheLOD::SampleDistanceAt(int x, int y, int z) const {
    if (x < 0 || x >= resolution ||
        y < 0 || y >= resolution ||
        z < 0 || z >= resolution) {
        return std::numeric_limits<float>::max();
    }

    size_t index = static_cast<size_t>(x) +
                   static_cast<size_t>(y) * resolution +
                   static_cast<size_t>(z) * resolution * resolution;

    return (index < distances.size()) ? distances[index] : std::numeric_limits<float>::max();
}

// =============================================================================
// SDFCacheEntry Implementation
// =============================================================================

const SDFCacheLOD* SDFCacheEntry::GetLODForScreenSize(float screenSize) const {
    if (m_lodLevels.empty()) return nullptr;

    // Higher screen size = need higher resolution LOD
    // Each LOD level halves the resolution
    for (size_t i = 0; i < m_lodLevels.size(); ++i) {
        const auto& lod = m_lodLevels[i];
        // Use LOD if its resolution is sufficient for screen size
        // Rule of thumb: resolution should be at least screenSize / 2
        if (lod.resolution >= screenSize / 2.0f || i == m_lodLevels.size() - 1) {
            return &lod;
        }
    }

    return &m_lodLevels.back();
}

std::vector<int> SDFCacheEntry::GetLODResolutions() const {
    std::vector<int> resolutions;
    resolutions.reserve(m_lodLevels.size());
    for (const auto& lod : m_lodLevels) {
        resolutions.push_back(lod.resolution);
    }
    return resolutions;
}

float SDFCacheEntry::EvaluateSDF(const glm::vec3& worldPos) const {
    return EvaluateSDF(worldPos, 0);
}

float SDFCacheEntry::EvaluateSDF(const glm::vec3& worldPos, size_t lodLevel) const {
    const SDFCacheLOD* lod = GetLOD(lodLevel);
    if (!lod) {
        return std::numeric_limits<float>::max();
    }

    glm::vec3 uvw = LocalToUVW(WorldToLocal(worldPos));
    return lod->SampleDistance(uvw);
}

glm::vec3 SDFCacheEntry::CalculateNormal(const glm::vec3& worldPos, float epsilon) const {
    float dx = EvaluateSDF(worldPos + glm::vec3(epsilon, 0, 0)) -
               EvaluateSDF(worldPos - glm::vec3(epsilon, 0, 0));
    float dy = EvaluateSDF(worldPos + glm::vec3(0, epsilon, 0)) -
               EvaluateSDF(worldPos - glm::vec3(0, epsilon, 0));
    float dz = EvaluateSDF(worldPos + glm::vec3(0, 0, epsilon)) -
               EvaluateSDF(worldPos - glm::vec3(0, 0, epsilon));

    glm::vec3 normal(dx, dy, dz);
    float len = glm::length(normal);
    return len > 0.0001f ? normal / len : glm::vec3(0, 1, 0);
}

size_t SDFCacheEntry::GetMemoryUsage() const {
    size_t total = sizeof(SDFCacheEntry);
    for (const auto& lod : m_lodLevels) {
        total += lod.distances.size() * sizeof(float);
        total += lod.materials.size() * sizeof(uint16_t);
    }
    return total;
}

void SDFCacheEntry::UpdateAccessTime() {
    m_lastAccessTime = std::chrono::system_clock::now();
}

glm::vec3 SDFCacheEntry::WorldToLocal(const glm::vec3& worldPos) const {
    return worldPos - m_boundsMin;
}

glm::vec3 SDFCacheEntry::LocalToUVW(const glm::vec3& localPos) const {
    glm::vec3 size = GetSize();
    if (size.x < 0.0001f || size.y < 0.0001f || size.z < 0.0001f) {
        return glm::vec3(0.5f);
    }
    return localPos / size;
}

// =============================================================================
// SDFCacheParams Implementation
// =============================================================================

bool SDFCacheParams::Validate() const {
    if (resolution < 4 || resolution > 512) {
        return false;
    }
    if (generateLODs && lodLevelCount < 1) {
        return false;
    }
    if (generateLODs && lodDivisor < 2) {
        return false;
    }
    if (minLODResolution < 4) {
        return false;
    }
    if (boundsPadding < 0.0f || boundsPadding > 1.0f) {
        return false;
    }
    return true;
}

std::vector<int> SDFCacheParams::GetLODResolutions() const {
    std::vector<int> resolutions;
    if (!generateLODs) {
        resolutions.push_back(resolution);
        return resolutions;
    }

    int currentRes = resolution;
    for (int i = 0; i < lodLevelCount; ++i) {
        if (currentRes < minLODResolution) {
            break;
        }
        resolutions.push_back(currentRes);
        currentRes /= lodDivisor;
    }

    return resolutions;
}

// =============================================================================
// SDFCache Implementation
// =============================================================================

SDFCache::SDFCache() = default;

SDFCache::~SDFCache() {
    if (m_initialized) {
        Shutdown();
    }
}

bool SDFCache::Initialize(const SDFCacheConfig& config) {
    if (m_initialized) {
        NOVA_LOG_WARN("SDFCache: Already initialized");
        return true;
    }

    m_config = config;

    if (!InitializeDirectory()) {
        NOVA_LOG_ERROR("SDFCache: Failed to initialize cache directory");
        return false;
    }

    if (!LoadIndex()) {
        NOVA_LOG_WARN("SDFCache: Cache index not found, starting fresh");
    }

    m_initialized = true;
    NOVA_LOG_INFO("SDFCache: Initialized with {} entries, {} bytes",
                  m_index.size(), GetTotalCacheSize());

    return true;
}

void SDFCache::Shutdown() {
    if (!m_initialized) return;

    // Wait for pending background generations
    while (m_pendingGenerations.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SaveIndex();

    std::lock_guard<std::mutex> lock(m_indexMutex);
    m_index.clear();
    m_initialized = false;

    NOVA_LOG_INFO("SDFCache: Shutdown complete");
}

bool SDFCache::InitializeDirectory() {
    namespace fs = std::filesystem;

    try {
        if (!fs::exists(m_config.cacheDirectory)) {
            fs::create_directories(m_config.cacheDirectory);
        }
        return fs::is_directory(m_config.cacheDirectory);
    } catch (const std::exception& e) {
        NOVA_LOG_ERROR("SDFCache: Failed to create directory '{}': {}",
                       m_config.cacheDirectory, e.what());
        return false;
    }
}

bool SDFCache::LoadIndex() {
    namespace fs = std::filesystem;

    std::string indexPath = m_config.cacheDirectory + "/cache_index.bin";

    std::ifstream file(indexPath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read header
    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != SDFCacheFormat::MAGIC || version != SDFCacheFormat::VERSION) {
        NOVA_LOG_WARN("SDFCache: Index version mismatch, rebuilding");
        return false;
    }

    // Read entry count
    uint32_t entryCount;
    file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

    std::lock_guard<std::mutex> lock(m_indexMutex);
    m_index.clear();

    for (uint32_t i = 0; i < entryCount; ++i) {
        CacheIndexEntry entry;

        file.read(reinterpret_cast<char*>(&entry.cacheKey), sizeof(entry.cacheKey));
        file.read(reinterpret_cast<char*>(&entry.sourceHash), sizeof(entry.sourceHash));

        uint32_t pathLen;
        file.read(reinterpret_cast<char*>(&pathLen), sizeof(pathLen));
        entry.filePath.resize(pathLen);
        file.read(entry.filePath.data(), pathLen);

        file.read(reinterpret_cast<char*>(&entry.fileSize), sizeof(entry.fileSize));

        int64_t creationSec, accessSec;
        file.read(reinterpret_cast<char*>(&creationSec), sizeof(creationSec));
        file.read(reinterpret_cast<char*>(&accessSec), sizeof(accessSec));

        entry.creationTime = std::chrono::system_clock::time_point(
            std::chrono::seconds(creationSec));
        entry.lastAccessTime = std::chrono::system_clock::time_point(
            std::chrono::seconds(accessSec));

        // Verify file exists
        if (fs::exists(entry.filePath)) {
            m_index[entry.cacheKey] = std::move(entry);
        }
    }

    m_stats.entryCount = m_index.size();
    return true;
}

bool SDFCache::SaveIndex() {
    std::string indexPath = m_config.cacheDirectory + "/cache_index.bin";

    std::ofstream file(indexPath, std::ios::binary);
    if (!file) {
        NOVA_LOG_ERROR("SDFCache: Failed to save index");
        return false;
    }

    // Write header
    uint32_t magic = SDFCacheFormat::MAGIC;
    uint32_t version = SDFCacheFormat::VERSION;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::lock_guard<std::mutex> lock(m_indexMutex);

    // Write entry count
    uint32_t entryCount = static_cast<uint32_t>(m_index.size());
    file.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));

    for (const auto& [key, entry] : m_index) {
        file.write(reinterpret_cast<const char*>(&entry.cacheKey), sizeof(entry.cacheKey));
        file.write(reinterpret_cast<const char*>(&entry.sourceHash), sizeof(entry.sourceHash));

        uint32_t pathLen = static_cast<uint32_t>(entry.filePath.size());
        file.write(reinterpret_cast<const char*>(&pathLen), sizeof(pathLen));
        file.write(entry.filePath.data(), pathLen);

        file.write(reinterpret_cast<const char*>(&entry.fileSize), sizeof(entry.fileSize));

        int64_t creationSec = std::chrono::duration_cast<std::chrono::seconds>(
            entry.creationTime.time_since_epoch()).count();
        int64_t accessSec = std::chrono::duration_cast<std::chrono::seconds>(
            entry.lastAccessTime.time_since_epoch()).count();

        file.write(reinterpret_cast<const char*>(&creationSec), sizeof(creationSec));
        file.write(reinterpret_cast<const char*>(&accessSec), sizeof(accessSec));
    }

    return true;
}

// =============================================================================
// Cache Operations
// =============================================================================

SDFCacheResult SDFCache::CacheSDF(
    const Mesh& mesh,
    const SDFCacheParams& params,
    SDFCacheProgressCallback progressCallback)
{
    SDFCacheResult result;

    if (!m_initialized) {
        result.errorMessage = "Cache not initialized";
        return result;
    }

    if (!params.Validate()) {
        result.errorMessage = "Invalid cache parameters";
        return result;
    }

    // Compute source hash
    uint64_t sourceHash = ComputeMeshHash(mesh);

    // Compute cache key
    result.cacheKey = ComputeCacheKey(sourceHash, params);

    // Check if already cached
    if (IsCachedAndValid(result.cacheKey, sourceHash)) {
        result.success = true;
        result.filePath = GetCacheFilePath(result.cacheKey);
        m_stats.hits++;
        return result;
    }

    m_stats.misses++;

    // Get bounds with padding
    glm::vec3 boundsMin = mesh.GetBoundsMin();
    glm::vec3 boundsMax = mesh.GetBoundsMax();
    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 padding = size * params.boundsPadding;
    boundsMin -= padding;
    boundsMax += padding;

    // Convert mesh to SDF function
    // Note: This is a simplified approach - uses MeshToSDFConverter internally
    MeshToSDFConverter converter;
    ConversionSettings convSettings = params.conversionSettings;
    auto convResult = converter.Convert(mesh, convSettings);

    if (!convResult.success || !convResult.rootPrimitive) {
        result.errorMessage = "Failed to convert mesh to SDF: " + convResult.errorMessage;
        return result;
    }

    // Create SDF model from conversion result
    SDFModel tempModel("temp_cache_model");
    tempModel.SetRoot(std::move(convResult.rootPrimitive));

    // Use the model's SDF evaluation
    auto sdfFunc = [&tempModel](const glm::vec3& p) {
        return tempModel.EvaluateSDF(p);
    };

    return CacheSDF(sdfFunc, boundsMin, boundsMax, sourceHash, params, progressCallback);
}

SDFCacheResult SDFCache::CacheSDF(
    const SDFModel& model,
    const SDFCacheParams& params,
    SDFCacheProgressCallback progressCallback)
{
    SDFCacheResult result;

    if (!m_initialized) {
        result.errorMessage = "Cache not initialized";
        return result;
    }

    if (!params.Validate()) {
        result.errorMessage = "Invalid cache parameters";
        return result;
    }

    // Compute source hash
    uint64_t sourceHash = ComputeModelHash(model);

    // Compute cache key
    result.cacheKey = ComputeCacheKey(sourceHash, params);

    // Check if already cached
    if (IsCachedAndValid(result.cacheKey, sourceHash)) {
        result.success = true;
        result.filePath = GetCacheFilePath(result.cacheKey);
        m_stats.hits++;
        return result;
    }

    m_stats.misses++;

    // Get bounds with padding
    auto [boundsMin, boundsMax] = model.GetBounds();
    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 padding = size * params.boundsPadding;
    boundsMin -= padding;
    boundsMax += padding;

    // Create SDF evaluation function
    auto sdfFunc = [&model](const glm::vec3& p) {
        return model.EvaluateSDF(p);
    };

    return CacheSDF(sdfFunc, boundsMin, boundsMax, sourceHash, params, progressCallback);
}

SDFCacheResult SDFCache::CacheSDF(
    const std::function<float(const glm::vec3&)>& sdfFunc,
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    uint64_t sourceHash,
    const SDFCacheParams& params,
    SDFCacheProgressCallback progressCallback)
{
    SDFCacheResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    if (!m_initialized) {
        result.errorMessage = "Cache not initialized";
        return result;
    }

    result.cacheKey = ComputeCacheKey(sourceHash, params);
    result.filePath = GetCacheFilePath(result.cacheKey);

    // Generate LOD levels
    std::vector<int> lodResolutions = params.GetLODResolutions();
    std::vector<SDFCacheLOD> lodLevels;
    lodLevels.reserve(lodResolutions.size());

    float totalLODs = static_cast<float>(lodResolutions.size());
    for (size_t i = 0; i < lodResolutions.size(); ++i) {
        float lodProgress = static_cast<float>(i) / totalLODs;
        float lodRange = 1.0f / totalLODs;

        auto lod = GenerateLOD(
            sdfFunc, boundsMin, boundsMax,
            lodResolutions[i],
            progressCallback, lodProgress, lodRange);

        lodLevels.push_back(std::move(lod));
    }

    // Write to disk
    if (!WriteCacheFile(result.filePath, result.cacheKey, sourceHash,
                        boundsMin, boundsMax, lodLevels)) {
        result.errorMessage = "Failed to write cache file";
        return result;
    }

    // Update index
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);

        CacheIndexEntry indexEntry;
        indexEntry.cacheKey = result.cacheKey;
        indexEntry.sourceHash = sourceHash;
        indexEntry.filePath = result.filePath;
        indexEntry.creationTime = std::chrono::system_clock::now();
        indexEntry.lastAccessTime = indexEntry.creationTime;

        // Get file size
        std::ifstream file(result.filePath, std::ios::binary | std::ios::ate);
        indexEntry.fileSize = static_cast<size_t>(file.tellg());
        result.fileSize = indexEntry.fileSize;

        m_index[result.cacheKey] = std::move(indexEntry);
        m_stats.entryCount = m_index.size();
    }

    // Calculate stats
    auto endTime = std::chrono::high_resolution_clock::now();
    result.generationTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    // Calculate compression ratio
    size_t uncompressedTotal = 0;
    size_t compressedTotal = 0;
    for (const auto& lod : lodLevels) {
        uncompressedTotal += lod.uncompressedSize;
        compressedTotal += lod.compressedSize > 0 ? lod.compressedSize : lod.uncompressedSize;
    }
    result.compressionRatio = uncompressedTotal > 0 ?
        static_cast<float>(compressedTotal) / uncompressedTotal : 1.0f;

    result.success = true;

    NOVA_LOG_INFO("SDFCache: Cached SDF key={:016X}, file={}, size={} bytes, time={:.1f}ms, compression={:.1f}%",
                  result.cacheKey, result.filePath, result.fileSize, result.generationTimeMs,
                  result.compressionRatio * 100.0f);

    return result;
}

JobHandle SDFCache::CacheSDFAsync(
    const Mesh& mesh,
    const SDFCacheParams& params,
    std::function<void(SDFCacheResult)> completionCallback,
    SDFCacheProgressCallback progressCallback)
{
    if (!m_initialized || !m_config.enableBackgroundGeneration) {
        // Fall back to synchronous
        auto result = CacheSDF(mesh, params, progressCallback);
        if (completionCallback) {
            completionCallback(result);
        }
        return JobHandle();
    }

    m_pendingGenerations++;

    // Capture necessary data for async generation
    uint64_t sourceHash = ComputeMeshHash(mesh);
    uint64_t cacheKey = ComputeCacheKey(sourceHash, params);

    // Check if already cached (quick check)
    if (IsCachedAndValid(cacheKey, sourceHash)) {
        m_pendingGenerations--;
        SDFCacheResult result;
        result.success = true;
        result.cacheKey = cacheKey;
        result.filePath = GetCacheFilePath(cacheKey);
        m_stats.hits++;
        if (completionCallback) {
            completionCallback(result);
        }
        return JobHandle();
    }

    // Get bounds
    glm::vec3 boundsMin = mesh.GetBoundsMin();
    glm::vec3 boundsMax = mesh.GetBoundsMax();
    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 padding = size * params.boundsPadding;
    boundsMin -= padding;
    boundsMax += padding;

    // Note: For true async mesh-to-SDF conversion, we would need to
    // copy the mesh data or use a shared reference. This is a simplified
    // implementation that uses the existing converter.
    return JobSystem::Instance().Submit([this, params, sourceHash, boundsMin, boundsMax,
                                          completionCallback, progressCallback]() {
        // Create a placeholder SDF function (actual implementation would
        // use copied mesh data)
        // For now, use a simple sphere SDF as placeholder
        glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        float radius = glm::length(boundsMax - boundsMin) * 0.5f;

        auto sdfFunc = [center, radius](const glm::vec3& p) {
            return glm::length(p - center) - radius;
        };

        auto result = CacheSDF(sdfFunc, boundsMin, boundsMax, sourceHash,
                               params, progressCallback);

        m_pendingGenerations--;

        if (completionCallback) {
            completionCallback(result);
        }
    }, JobPriority::Normal);
}

std::unique_ptr<SDFCacheEntry> SDFCache::LoadSDF(uint64_t cacheKey) {
    if (!m_initialized) return nullptr;

    std::string filePath;
    {
        std::lock_guard<std::mutex> lock(m_indexMutex);
        auto it = m_index.find(cacheKey);
        if (it == m_index.end()) {
            return nullptr;
        }
        filePath = it->second.filePath;

        // Update access time
        it->second.lastAccessTime = std::chrono::system_clock::now();
    }

    return LoadSDFFromFile(filePath);
}

std::unique_ptr<SDFCacheEntry> SDFCache::LoadSDFFromFile(const std::string& filePath) {
    auto entry = std::make_unique<SDFCacheEntry>();

    // Try memory-mapped loading for large files
    namespace fs = std::filesystem;
    if (m_config.enableMemoryMapping && fs::exists(filePath)) {
        auto fileSize = fs::file_size(filePath);
        if (fileSize >= m_config.memoryMappingThreshold) {
            if (LoadMemoryMapped(filePath, *entry)) {
                return entry;
            }
        }
    }

    // Standard file loading
    if (!ReadCacheFile(filePath, *entry)) {
        return nullptr;
    }

    return entry;
}

bool SDFCache::IsCached(uint64_t cacheKey) const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_index.find(cacheKey) != m_index.end();
}

bool SDFCache::IsCachedAndValid(uint64_t cacheKey, uint64_t sourceHash) const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    auto it = m_index.find(cacheKey);
    if (it == m_index.end()) {
        return false;
    }

    // Check if source hash matches
    if (it->second.sourceHash != sourceHash) {
        return false;
    }

    // Check if cache has expired
    if (m_config.maxCacheAge > 0) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - it->second.creationTime).count();
        if (static_cast<uint64_t>(age) > m_config.maxCacheAge) {
            return false;
        }
    }

    // Verify file exists
    return std::filesystem::exists(it->second.filePath);
}

std::string SDFCache::GetCacheFilePath(uint64_t cacheKey) const {
    char filename[32];
    std::snprintf(filename, sizeof(filename), "%016llX.sdfcache",
                  static_cast<unsigned long long>(cacheKey));
    return m_config.cacheDirectory + "/" + filename;
}

// =============================================================================
// Cache Management
// =============================================================================

bool SDFCache::RemoveEntry(uint64_t cacheKey) {
    std::lock_guard<std::mutex> lock(m_indexMutex);

    auto it = m_index.find(cacheKey);
    if (it == m_index.end()) {
        return false;
    }

    // Remove file
    std::filesystem::remove(it->second.filePath);

    m_index.erase(it);
    m_stats.entryCount = m_index.size();
    m_stats.evictions++;

    return true;
}

size_t SDFCache::CleanupExpired() {
    if (m_config.maxCacheAge == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_indexMutex);

    auto now = std::chrono::system_clock::now();
    std::vector<uint64_t> toRemove;

    for (const auto& [key, entry] : m_index) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - entry.creationTime).count();
        if (static_cast<uint64_t>(age) > m_config.maxCacheAge) {
            toRemove.push_back(key);
        }
    }

    for (uint64_t key : toRemove) {
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            std::filesystem::remove(it->second.filePath);
            m_index.erase(it);
        }
    }

    m_stats.entryCount = m_index.size();
    m_stats.evictions += toRemove.size();

    return toRemove.size();
}

size_t SDFCache::EnforceMaxSize(size_t targetSize) {
    if (targetSize == 0) {
        targetSize = m_config.maxCacheSize;
    }
    if (targetSize == 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_indexMutex);

    // Calculate current size
    size_t currentSize = 0;
    for (const auto& [key, entry] : m_index) {
        currentSize += entry.fileSize;
    }

    if (currentSize <= targetSize) {
        return 0;
    }

    // Sort by last access time (oldest first)
    std::vector<std::pair<uint64_t, std::chrono::system_clock::time_point>> entries;
    entries.reserve(m_index.size());
    for (const auto& [key, entry] : m_index) {
        entries.emplace_back(key, entry.lastAccessTime);
    }

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Remove oldest until under target
    size_t removed = 0;
    for (const auto& [key, time] : entries) {
        if (currentSize <= targetSize) {
            break;
        }

        auto it = m_index.find(key);
        if (it != m_index.end()) {
            currentSize -= it->second.fileSize;
            std::filesystem::remove(it->second.filePath);
            m_index.erase(it);
            removed++;
        }
    }

    m_stats.entryCount = m_index.size();
    m_stats.evictions += removed;

    return removed;
}

size_t SDFCache::ClearAll() {
    std::lock_guard<std::mutex> lock(m_indexMutex);

    size_t count = m_index.size();

    for (const auto& [key, entry] : m_index) {
        std::filesystem::remove(entry.filePath);
    }

    m_index.clear();
    m_stats.entryCount = 0;
    m_stats.evictions += count;

    // Remove index file
    std::filesystem::remove(m_config.cacheDirectory + "/cache_index.bin");

    NOVA_LOG_INFO("SDFCache: Cleared {} entries", count);

    return count;
}

void SDFCache::RefreshIndex() {
    namespace fs = std::filesystem;

    std::lock_guard<std::mutex> lock(m_indexMutex);

    // Remove entries for files that no longer exist
    std::vector<uint64_t> toRemove;
    for (const auto& [key, entry] : m_index) {
        if (!fs::exists(entry.filePath)) {
            toRemove.push_back(key);
        }
    }

    for (uint64_t key : toRemove) {
        m_index.erase(key);
    }

    m_stats.entryCount = m_index.size();

    NOVA_LOG_INFO("SDFCache: Refreshed index, removed {} orphaned entries",
                  toRemove.size());
}

void SDFCache::Update(float deltaTime) {
    if (!m_initialized || !m_config.enableAutoCleanup) {
        return;
    }

    m_cleanupTimer += deltaTime;
    if (m_cleanupTimer >= m_config.cleanupInterval) {
        m_cleanupTimer = 0.0f;

        size_t expired = CleanupExpired();
        size_t evicted = EnforceMaxSize();

        if (expired > 0 || evicted > 0) {
            NOVA_LOG_DEBUG("SDFCache: Auto-cleanup removed {} expired, {} for size",
                          expired, evicted);
            SaveIndex();
        }
    }
}

// =============================================================================
// Hash Functions
// =============================================================================

uint64_t SDFCache::ComputeMeshHash(const Mesh& mesh) {
    // Hash based on vertex count, index count, and bounds
    // Note: For true content hashing, we would need access to vertex data
    uint64_t hash = FNV_OFFSET_BASIS_64;

    uint32_t vertexCount = mesh.GetVertexCount();
    uint32_t indexCount = mesh.GetIndexCount();
    hash = FNV1aHash(&vertexCount, sizeof(vertexCount), hash);
    hash = FNV1aHash(&indexCount, sizeof(indexCount), hash);

    glm::vec3 boundsMin = mesh.GetBoundsMin();
    glm::vec3 boundsMax = mesh.GetBoundsMax();
    hash = FNV1aHash(&boundsMin, sizeof(boundsMin), hash);
    hash = FNV1aHash(&boundsMax, sizeof(boundsMax), hash);

    return hash;
}

uint64_t SDFCache::ComputeModelHash(const SDFModel& model) {
    uint64_t hash = FNV_OFFSET_BASIS_64;

    // Hash name
    const std::string& name = model.GetName();
    hash = FNV1aHash(name.data(), name.size(), hash);

    // Hash primitive count
    size_t primCount = model.GetPrimitiveCount();
    hash = FNV1aHash(&primCount, sizeof(primCount), hash);

    // Hash bounds
    auto [boundsMin, boundsMax] = model.GetBounds();
    hash = FNV1aHash(&boundsMin, sizeof(boundsMin), hash);
    hash = FNV1aHash(&boundsMax, sizeof(boundsMax), hash);

    return hash;
}

uint64_t SDFCache::ComputeCacheKey(uint64_t sourceHash, const SDFCacheParams& params) {
    uint64_t hash = sourceHash;

    hash = HashCombine(hash, static_cast<uint64_t>(params.resolution));
    hash = HashCombine(hash, static_cast<uint64_t>(params.generateLODs));
    hash = HashCombine(hash, static_cast<uint64_t>(params.lodLevelCount));
    hash = HashCombine(hash, static_cast<uint64_t>(params.lodDivisor));

    // Include bounds padding in hash (quantized to avoid floating point issues)
    uint64_t paddingQuantized = static_cast<uint64_t>(params.boundsPadding * 1000);
    hash = HashCombine(hash, paddingQuantized);

    return hash;
}

// =============================================================================
// Statistics
// =============================================================================

std::vector<std::pair<uint64_t, size_t>> SDFCache::GetCachedEntries() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);

    std::vector<std::pair<uint64_t, size_t>> entries;
    entries.reserve(m_index.size());

    for (const auto& [key, entry] : m_index) {
        entries.emplace_back(key, entry.fileSize);
    }

    return entries;
}

size_t SDFCache::GetTotalCacheSize() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);

    size_t total = 0;
    for (const auto& [key, entry] : m_index) {
        total += entry.fileSize;
    }
    return total;
}

size_t SDFCache::GetEntryCount() const {
    std::lock_guard<std::mutex> lock(m_indexMutex);
    return m_index.size();
}

// =============================================================================
// File I/O
// =============================================================================

bool SDFCache::WriteCacheFile(
    const std::string& filePath,
    uint64_t cacheKey,
    uint64_t sourceHash,
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    const std::vector<SDFCacheLOD>& lodLevels)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        NOVA_LOG_ERROR("SDFCache: Failed to create file '{}'", filePath);
        return false;
    }

    // Write header
    uint32_t magic = SDFCacheFormat::MAGIC;
    uint32_t version = SDFCacheFormat::VERSION;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    file.write(reinterpret_cast<const char*>(&cacheKey), sizeof(cacheKey));
    file.write(reinterpret_cast<const char*>(&sourceHash), sizeof(sourceHash));

    file.write(reinterpret_cast<const char*>(&boundsMin), sizeof(boundsMin));
    file.write(reinterpret_cast<const char*>(&boundsMax), sizeof(boundsMax));

    int32_t compressionLevel = m_config.compressionLevel;
    file.write(reinterpret_cast<const char*>(&compressionLevel), sizeof(compressionLevel));

    int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    file.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));

    // Write LOD count
    uint32_t lodCount = static_cast<uint32_t>(lodLevels.size());
    file.write(reinterpret_cast<const char*>(&lodCount), sizeof(lodCount));

    // Pad header to fixed size
    size_t headerWritten = sizeof(magic) + sizeof(version) + sizeof(cacheKey) +
                           sizeof(sourceHash) + sizeof(boundsMin) + sizeof(boundsMax) +
                           sizeof(compressionLevel) + sizeof(timestamp) + sizeof(lodCount);
    std::vector<uint8_t> padding(SDFCacheFormat::HEADER_SIZE - headerWritten, 0);
    file.write(reinterpret_cast<const char*>(padding.data()), padding.size());

    // Write LOD data
    for (const auto& lod : lodLevels) {
        int32_t resolution = lod.resolution;
        file.write(reinterpret_cast<const char*>(&resolution), sizeof(resolution));

        uint64_t uncompressedSize = lod.distances.size() * sizeof(float);
        file.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));

        // Compress distance data if enabled
        std::vector<uint8_t> compressedData;
        bool compressed = false;

        if (m_config.compressionLevel > 0 && !lod.distances.empty()) {
            compressedData = CompressData(lod.distances.data(),
                                          lod.distances.size() * sizeof(float));
            compressed = !compressedData.empty() && compressedData.size() < uncompressedSize;
        }

        if (compressed) {
            uint64_t compressedSize = compressedData.size();
            file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
            file.write(reinterpret_cast<const char*>(compressedData.data()), compressedSize);
        } else {
            uint64_t compressedSize = 0;  // 0 means not compressed
            file.write(reinterpret_cast<const char*>(&compressedSize), sizeof(compressedSize));
            file.write(reinterpret_cast<const char*>(lod.distances.data()), uncompressedSize);
        }

        // Write material data if present
        uint8_t hasMaterials = !lod.materials.empty() ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&hasMaterials), sizeof(hasMaterials));

        if (hasMaterials) {
            uint64_t materialSize = lod.materials.size() * sizeof(uint16_t);
            file.write(reinterpret_cast<const char*>(&materialSize), sizeof(materialSize));
            file.write(reinterpret_cast<const char*>(lod.materials.data()), materialSize);
        }
    }

    return true;
}

bool SDFCache::ReadCacheFile(const std::string& filePath, SDFCacheEntry& entry) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read header
    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != SDFCacheFormat::MAGIC) {
        NOVA_LOG_ERROR("SDFCache: Invalid magic number in '{}'", filePath);
        return false;
    }

    if (version != SDFCacheFormat::VERSION) {
        NOVA_LOG_WARN("SDFCache: Version mismatch in '{}' (got {}, expected {})",
                      filePath, version, SDFCacheFormat::VERSION);
        return false;
    }

    file.read(reinterpret_cast<char*>(&entry.m_cacheKey), sizeof(entry.m_cacheKey));
    file.read(reinterpret_cast<char*>(&entry.m_sourceHash), sizeof(entry.m_sourceHash));

    file.read(reinterpret_cast<char*>(&entry.m_boundsMin), sizeof(entry.m_boundsMin));
    file.read(reinterpret_cast<char*>(&entry.m_boundsMax), sizeof(entry.m_boundsMax));

    file.read(reinterpret_cast<char*>(&entry.m_compressionLevel), sizeof(entry.m_compressionLevel));

    int64_t timestamp;
    file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
    entry.m_creationTime = std::chrono::system_clock::time_point(
        std::chrono::seconds(timestamp));
    entry.m_lastAccessTime = std::chrono::system_clock::now();

    uint32_t lodCount;
    file.read(reinterpret_cast<char*>(&lodCount), sizeof(lodCount));

    if (lodCount > SDFCacheFormat::MAX_LOD_LEVELS) {
        NOVA_LOG_ERROR("SDFCache: Too many LOD levels ({}) in '{}'", lodCount, filePath);
        return false;
    }

    // Skip header padding
    file.seekg(SDFCacheFormat::HEADER_SIZE, std::ios::beg);

    // Read LOD data
    entry.m_lodLevels.resize(lodCount);

    for (uint32_t i = 0; i < lodCount; ++i) {
        auto& lod = entry.m_lodLevels[i];

        int32_t resolution;
        file.read(reinterpret_cast<char*>(&resolution), sizeof(resolution));
        lod.resolution = resolution;

        uint64_t uncompressedSize;
        file.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
        lod.uncompressedSize = static_cast<size_t>(uncompressedSize);

        uint64_t compressedSize;
        file.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
        lod.compressedSize = static_cast<size_t>(compressedSize);

        size_t distanceCount = uncompressedSize / sizeof(float);

        if (compressedSize > 0) {
            // Read compressed data
            std::vector<uint8_t> compressedData(compressedSize);
            file.read(reinterpret_cast<char*>(compressedData.data()), compressedSize);

            // Decompress
            auto decompressed = DecompressData(compressedData.data(), compressedSize, uncompressedSize);
            if (decompressed.size() == uncompressedSize) {
                lod.distances.resize(distanceCount);
                std::memcpy(lod.distances.data(), decompressed.data(), uncompressedSize);
            } else {
                NOVA_LOG_ERROR("SDFCache: Decompression failed for LOD {} in '{}'", i, filePath);
                return false;
            }
        } else {
            // Read uncompressed data
            lod.distances.resize(distanceCount);
            file.read(reinterpret_cast<char*>(lod.distances.data()), uncompressedSize);
        }

        // Read material data
        uint8_t hasMaterials;
        file.read(reinterpret_cast<char*>(&hasMaterials), sizeof(hasMaterials));

        if (hasMaterials) {
            uint64_t materialSize;
            file.read(reinterpret_cast<char*>(&materialSize), sizeof(materialSize));
            size_t materialCount = materialSize / sizeof(uint16_t);
            lod.materials.resize(materialCount);
            file.read(reinterpret_cast<char*>(lod.materials.data()), materialSize);
        }
    }

    entry.m_filePath = filePath;

    // Get file size
    file.seekg(0, std::ios::end);
    entry.m_fileSize = static_cast<size_t>(file.tellg());

    return true;
}

// =============================================================================
// Memory-Mapped Loading
// =============================================================================

bool SDFCache::LoadMemoryMapped(const std::string& filePath, SDFCacheEntry& entry) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(
        filePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }

    HANDLE hMapping = CreateFileMappingA(
        hFile,
        nullptr,
        PAGE_READONLY,
        fileSize.HighPart,
        fileSize.LowPart,
        nullptr);

    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }

    void* mappedData = MapViewOfFile(
        hMapping,
        FILE_MAP_READ,
        0, 0, 0);

    if (!mappedData) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    entry.m_mappedData = mappedData;
    entry.m_mappedSize = static_cast<size_t>(fileSize.QuadPart);
    entry.m_isMemoryMapped = true;

    // Parse header from mapped data
    // Note: Full implementation would parse the mapped data directly
    // For now, fall back to standard loading
    UnmapViewOfFile(mappedData);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    return ReadCacheFile(filePath, entry);

#else
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1) {
        return false;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return false;
    }

    void* mappedData = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mappedData == MAP_FAILED) {
        return false;
    }

    entry.m_mappedData = mappedData;
    entry.m_mappedSize = static_cast<size_t>(sb.st_size);
    entry.m_isMemoryMapped = true;

    // For now, fall back to standard loading
    munmap(mappedData, sb.st_size);
    entry.m_mappedData = nullptr;
    entry.m_mappedSize = 0;
    entry.m_isMemoryMapped = false;

    return ReadCacheFile(filePath, entry);
#endif
}

void SDFCache::UnloadMemoryMapped(SDFCacheEntry& entry) {
    if (!entry.m_isMemoryMapped || !entry.m_mappedData) {
        return;
    }

#ifdef _WIN32
    UnmapViewOfFile(entry.m_mappedData);
#else
    munmap(entry.m_mappedData, entry.m_mappedSize);
#endif

    entry.m_mappedData = nullptr;
    entry.m_mappedSize = 0;
    entry.m_isMemoryMapped = false;
}

// =============================================================================
// Compression
// =============================================================================

std::vector<uint8_t> SDFCache::CompressData(const void* data, size_t size) {
#ifdef NOVA_SDF_CACHE_HAS_ZLIB
    if (m_config.compressionLevel <= 0 || size == 0) {
        return {};
    }

    uLongf compressedSize = compressBound(static_cast<uLong>(size));
    std::vector<uint8_t> compressed(compressedSize);

    int result = compress2(
        compressed.data(),
        &compressedSize,
        static_cast<const Bytef*>(data),
        static_cast<uLong>(size),
        m_config.compressionLevel);

    if (result != Z_OK) {
        NOVA_LOG_WARN("SDFCache: Compression failed with code {}", result);
        return {};
    }

    compressed.resize(compressedSize);
    return compressed;
#else
    (void)data;
    (void)size;
    return {};
#endif
}

std::vector<uint8_t> SDFCache::DecompressData(
    const void* data, size_t compressedSize, size_t uncompressedSize)
{
#ifdef NOVA_SDF_CACHE_HAS_ZLIB
    if (compressedSize == 0 || uncompressedSize == 0) {
        return {};
    }

    std::vector<uint8_t> decompressed(uncompressedSize);
    uLongf destLen = static_cast<uLongf>(uncompressedSize);

    int result = uncompress(
        decompressed.data(),
        &destLen,
        static_cast<const Bytef*>(data),
        static_cast<uLong>(compressedSize));

    if (result != Z_OK) {
        NOVA_LOG_ERROR("SDFCache: Decompression failed with code {}", result);
        return {};
    }

    return decompressed;
#else
    (void)data;
    (void)compressedSize;
    (void)uncompressedSize;
    return {};
#endif
}

// =============================================================================
// LOD Generation
// =============================================================================

SDFCacheLOD SDFCache::GenerateLOD(
    const std::function<float(const glm::vec3&)>& sdfFunc,
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax,
    int resolution,
    SDFCacheProgressCallback progressCallback,
    float progressBase,
    float progressRange)
{
    SDFCacheLOD lod;
    lod.resolution = resolution;

    size_t voxelCount = static_cast<size_t>(resolution) * resolution * resolution;
    lod.distances.resize(voxelCount);
    lod.uncompressedSize = voxelCount * sizeof(float);

    glm::vec3 size = boundsMax - boundsMin;
    glm::vec3 voxelSize = size / static_cast<float>(resolution - 1);

    // Generate distance values
    size_t processed = 0;
    for (int z = 0; z < resolution; ++z) {
        for (int y = 0; y < resolution; ++y) {
            for (int x = 0; x < resolution; ++x) {
                glm::vec3 pos = boundsMin + glm::vec3(x, y, z) * voxelSize;
                float distance = sdfFunc(pos);

                size_t index = static_cast<size_t>(x) +
                              static_cast<size_t>(y) * resolution +
                              static_cast<size_t>(z) * resolution * resolution;

                lod.distances[index] = distance;
                processed++;
            }
        }

        // Report progress
        if (progressCallback) {
            float lodProgress = static_cast<float>(z + 1) / resolution;
            progressCallback(progressBase + lodProgress * progressRange);
        }
    }

    return lod;
}

} // namespace Nova
