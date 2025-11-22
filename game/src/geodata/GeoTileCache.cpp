#include "GeoTileCache.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Vehement {
namespace Geo {

// =============================================================================
// CacheConfig Implementation
// =============================================================================

CacheConfig CacheConfig::LoadFromFile(const std::string& path) {
    CacheConfig config;

    std::ifstream file(path);
    if (!file.is_open()) return config;

    try {
        nlohmann::json json;
        file >> json;

        if (json.contains("cachePath")) config.cachePath = json["cachePath"];
        if (json.contains("maxMemoryCacheMB")) config.maxMemoryCacheMB = json["maxMemoryCacheMB"];
        if (json.contains("maxDiskCacheMB")) config.maxDiskCacheMB = json["maxDiskCacheMB"];
        if (json.contains("defaultExpiryHours")) config.defaultExpiryHours = json["defaultExpiryHours"];
        if (json.contains("enableDiskCache")) config.enableDiskCache = json["enableDiskCache"];
        if (json.contains("enableCompression")) config.enableCompression = json["enableCompression"];

    } catch (...) {}

    return config;
}

void CacheConfig::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    json["cachePath"] = cachePath;
    json["maxMemoryCacheMB"] = maxMemoryCacheMB;
    json["maxDiskCacheMB"] = maxDiskCacheMB;
    json["defaultExpiryHours"] = defaultExpiryHours;
    json["enableDiskCache"] = enableDiskCache;
    json["enableCompression"] = enableCompression;

    std::ofstream file(path);
    if (file.is_open()) {
        file << json.dump(2);
    }
}

// =============================================================================
// CacheEntry Implementation
// =============================================================================

bool CacheEntry::IsExpired() const {
    if (expiryTimestamp <= 0) return false;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    return timestamp > expiryTimestamp;
}

void CacheEntry::Touch() {
    auto now = std::chrono::system_clock::now();
    lastAccessTime = std::chrono::system_clock::to_time_t(now);
}

// =============================================================================
// GeoTileCache Implementation
// =============================================================================

GeoTileCache::GeoTileCache() = default;

GeoTileCache::~GeoTileCache() {
    Shutdown();
}

bool GeoTileCache::Initialize(const std::string& configPath) {
    if (!configPath.empty()) {
        m_config = CacheConfig::LoadFromFile(configPath);
    }
    return Initialize(m_config);
}

bool GeoTileCache::Initialize(const CacheConfig& config) {
    m_config = config;

    // Create cache directory
    if (m_config.enableDiskCache) {
        try {
            std::filesystem::create_directories(m_config.cachePath);
        } catch (const std::exception& e) {
            // Failed to create directory
            m_config.enableDiskCache = false;
        }
    }

    // Scan existing disk cache
    if (m_config.enableDiskCache) {
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(m_config.cachePath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".tile") {
                    m_currentDiskBytes += entry.file_size();
                }
            }
        } catch (...) {}
    }

    return true;
}

void GeoTileCache::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memoryCache.clear();
    m_lruList.clear();
    m_lruMap.clear();
    m_entries.clear();
    m_currentMemoryBytes = 0;
}

bool GeoTileCache::Get(const TileID& tile, GeoTileData& outData) {
    std::string key = tile.ToKey();

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check memory cache first
    auto memIt = m_memoryCache.find(key);
    if (memIt != m_memoryCache.end()) {
        auto entryIt = m_entries.find(key);
        if (entryIt != m_entries.end() && !entryIt->second.IsExpired()) {
            outData = memIt->second;
            entryIt->second.Touch();
            TouchLRU(tile);
            ++m_hitCount;
            return true;
        }
    }

    // Check disk cache
    if (m_config.enableDiskCache) {
        if (LoadFromDisk(tile, outData)) {
            auto entryIt = m_entries.find(key);
            if (entryIt != m_entries.end() && !entryIt->second.IsExpired()) {
                // Add to memory cache
                size_t dataSize = EstimateSize(outData);
                if (m_currentMemoryBytes + dataSize > m_config.maxMemoryCacheMB * 1024 * 1024) {
                    EvictMemory(m_config.maxMemoryCacheMB * 1024 * 1024 / 2);
                }

                m_memoryCache[key] = outData;
                m_lruList.push_front(key);
                m_lruMap[key] = m_lruList.begin();
                m_currentMemoryBytes += dataSize;

                if (entryIt != m_entries.end()) {
                    entryIt->second.inMemory = true;
                    entryIt->second.Touch();
                }

                ++m_hitCount;
                return true;
            }
        }
    }

    ++m_missCount;
    return false;
}

void GeoTileCache::Put(const TileID& tile, const GeoTileData& data) {
    std::string key = tile.ToKey();
    size_t dataSize = EstimateSize(data);

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check memory limits
    if (m_currentMemoryBytes + dataSize > m_config.maxMemoryCacheMB * 1024 * 1024) {
        EvictMemory(m_config.maxMemoryCacheMB * 1024 * 1024 / 2);
    }

    // Remove existing entry if present
    auto memIt = m_memoryCache.find(key);
    if (memIt != m_memoryCache.end()) {
        auto entryIt = m_entries.find(key);
        if (entryIt != m_entries.end()) {
            m_currentMemoryBytes -= entryIt->second.dataSize;
        }
        m_memoryCache.erase(memIt);

        auto lruIt = m_lruMap.find(key);
        if (lruIt != m_lruMap.end()) {
            m_lruList.erase(lruIt->second);
            m_lruMap.erase(lruIt);
        }
    }

    // Add to memory cache
    m_memoryCache[key] = data;
    m_lruList.push_front(key);
    m_lruMap[key] = m_lruList.begin();
    m_currentMemoryBytes += dataSize;

    // Create/update entry
    CacheEntry& entry = m_entries[key];
    entry.tileId = tile;
    entry.fetchTimestamp = data.fetchTimestamp;
    entry.expiryTimestamp = data.expiryTimestamp;
    entry.dataSize = dataSize;
    entry.inMemory = true;
    entry.Touch();

    // Save to disk cache
    if (m_config.enableDiskCache) {
        // Check disk limits
        if (m_currentDiskBytes + dataSize > m_config.maxDiskCacheMB * 1024 * 1024) {
            EvictDisk(m_config.maxDiskCacheMB * 1024 * 1024 / 2);
        }

        if (SaveToDisk(tile, data)) {
            entry.onDisk = true;
            entry.diskPath = GetDiskPath(tile);
            m_currentDiskBytes += dataSize;
        }
    }
}

bool GeoTileCache::Contains(const TileID& tile) const {
    std::string key = tile.ToKey();
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.find(key) != m_entries.end();
}

bool GeoTileCache::IsValid(const TileID& tile) const {
    std::string key = tile.ToKey();
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(key);
    return it != m_entries.end() && !it->second.IsExpired();
}

void GeoTileCache::Remove(const TileID& tile) {
    std::string key = tile.ToKey();
    std::lock_guard<std::mutex> lock(m_mutex);

    auto entryIt = m_entries.find(key);
    if (entryIt != m_entries.end()) {
        // Remove from memory
        auto memIt = m_memoryCache.find(key);
        if (memIt != m_memoryCache.end()) {
            m_currentMemoryBytes -= entryIt->second.dataSize;
            m_memoryCache.erase(memIt);
        }

        auto lruIt = m_lruMap.find(key);
        if (lruIt != m_lruMap.end()) {
            m_lruList.erase(lruIt->second);
            m_lruMap.erase(lruIt);
        }

        // Remove from disk
        if (!entryIt->second.diskPath.empty()) {
            try {
                std::filesystem::remove(entryIt->second.diskPath);
                m_currentDiskBytes -= entryIt->second.dataSize;
            } catch (...) {}
        }

        m_entries.erase(entryIt);
    }
}

void GeoTileCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_memoryCache.clear();
    m_lruList.clear();
    m_lruMap.clear();
    m_entries.clear();
    m_currentMemoryBytes = 0;

    // Clear disk cache
    if (m_config.enableDiskCache) {
        try {
            std::filesystem::remove_all(m_config.cachePath);
            std::filesystem::create_directories(m_config.cachePath);
        } catch (...) {}
        m_currentDiskBytes = 0;
    }
}

int GeoTileCache::ClearExpired() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> toRemove;

    for (const auto& [key, entry] : m_entries) {
        if (entry.IsExpired()) {
            toRemove.push_back(key);
        }
    }

    for (const std::string& key : toRemove) {
        auto entryIt = m_entries.find(key);
        if (entryIt != m_entries.end()) {
            // Remove from memory
            auto memIt = m_memoryCache.find(key);
            if (memIt != m_memoryCache.end()) {
                m_currentMemoryBytes -= entryIt->second.dataSize;
                m_memoryCache.erase(memIt);
            }

            auto lruIt = m_lruMap.find(key);
            if (lruIt != m_lruMap.end()) {
                m_lruList.erase(lruIt->second);
                m_lruMap.erase(lruIt);
            }

            // Remove from disk
            if (!entryIt->second.diskPath.empty()) {
                try {
                    std::filesystem::remove(entryIt->second.diskPath);
                    m_currentDiskBytes -= entryIt->second.dataSize;
                } catch (...) {}
            }

            m_entries.erase(entryIt);
        }
    }

    return static_cast<int>(toRemove.size());
}

std::unordered_map<std::string, GeoTileData> GeoTileCache::GetMultiple(
    const std::vector<TileID>& tiles) {

    std::unordered_map<std::string, GeoTileData> result;

    for (const auto& tile : tiles) {
        GeoTileData data;
        if (Get(tile, data)) {
            result[tile.ToKey()] = std::move(data);
        }
    }

    return result;
}

void GeoTileCache::PutMultiple(const std::vector<std::pair<TileID, GeoTileData>>& tiles) {
    for (const auto& [tile, data] : tiles) {
        Put(tile, data);
    }
}

std::vector<TileID> GeoTileCache::GetCachedTiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TileID> result;
    for (const auto& [key, entry] : m_entries) {
        result.push_back(entry.tileId);
    }

    return result;
}

std::vector<TileID> GeoTileCache::GetTilesInBounds(const GeoBoundingBox& bounds, int zoom) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TileID> result;

    for (const auto& [key, entry] : m_entries) {
        if (entry.tileId.zoom == zoom) {
            GeoBoundingBox tileBounds = entry.tileId.GetBounds();
            if (bounds.Intersects(tileBounds)) {
                result.push_back(entry.tileId);
            }
        }
    }

    return result;
}

size_t GeoTileCache::GetDiskCacheSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t count = 0;
    for (const auto& [key, entry] : m_entries) {
        if (entry.onDisk) ++count;
    }
    return count;
}

void GeoTileCache::EvictMemory(size_t targetBytes) {
    // LRU eviction (no lock - called from locked context)
    while (m_currentMemoryBytes > targetBytes && !m_lruList.empty()) {
        std::string key = m_lruList.back();
        m_lruList.pop_back();
        m_lruMap.erase(key);

        auto memIt = m_memoryCache.find(key);
        if (memIt != m_memoryCache.end()) {
            auto entryIt = m_entries.find(key);
            if (entryIt != m_entries.end()) {
                m_currentMemoryBytes -= entryIt->second.dataSize;
                entryIt->second.inMemory = false;
            }
            m_memoryCache.erase(memIt);
        }
    }
}

void GeoTileCache::EvictDisk(size_t targetBytes) {
    // Simple oldest-first eviction for disk
    std::vector<std::pair<int64_t, std::string>> timeKeys;

    for (const auto& [key, entry] : m_entries) {
        if (entry.onDisk) {
            timeKeys.emplace_back(entry.lastAccessTime, key);
        }
    }

    std::sort(timeKeys.begin(), timeKeys.end());

    for (const auto& [time, key] : timeKeys) {
        if (m_currentDiskBytes <= targetBytes) break;

        auto entryIt = m_entries.find(key);
        if (entryIt != m_entries.end() && entryIt->second.onDisk) {
            try {
                std::filesystem::remove(entryIt->second.diskPath);
                m_currentDiskBytes -= entryIt->second.dataSize;
                entryIt->second.onDisk = false;
                entryIt->second.diskPath.clear();
            } catch (...) {}
        }
    }
}

bool GeoTileCache::ExportToBundle(const std::string& bundlePath,
                                    const std::vector<TileID>& tiles) {
    // Create bundle directory
    try {
        std::filesystem::create_directories(bundlePath);
    } catch (...) {
        return false;
    }

    nlohmann::json manifest;
    manifest["version"] = "1.0";
    manifest["tiles"] = nlohmann::json::array();

    for (const auto& tile : tiles) {
        GeoTileData data;
        if (Get(tile, data)) {
            std::string filename = std::to_string(tile.zoom) + "_" +
                                   std::to_string(tile.x) + "_" +
                                   std::to_string(tile.y) + ".tile";

            std::string filepath = bundlePath + "/" + filename;

            std::ofstream file(filepath, std::ios::binary);
            if (file.is_open()) {
                std::string json = SerializeTileData(data);
                file << json;
                file.close();

                nlohmann::json tileInfo;
                tileInfo["zoom"] = tile.zoom;
                tileInfo["x"] = tile.x;
                tileInfo["y"] = tile.y;
                tileInfo["file"] = filename;
                manifest["tiles"].push_back(tileInfo);
            }
        }
    }

    // Write manifest
    std::ofstream manifestFile(bundlePath + "/manifest.json");
    if (manifestFile.is_open()) {
        manifestFile << manifest.dump(2);
        return true;
    }

    return false;
}

bool GeoTileCache::ImportFromBundle(const std::string& bundlePath) {
    std::ifstream manifestFile(bundlePath + "/manifest.json");
    if (!manifestFile.is_open()) return false;

    try {
        nlohmann::json manifest;
        manifestFile >> manifest;

        for (const auto& tileInfo : manifest["tiles"]) {
            TileID tile(tileInfo["x"], tileInfo["y"], tileInfo["zoom"]);
            std::string filename = tileInfo["file"];
            std::string filepath = bundlePath + "/" + filename;

            std::ifstream file(filepath, std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();

                GeoTileData data;
                if (DeserializeTileData(buffer.str(), data)) {
                    Put(tile, data);
                }
            }
        }

        return true;

    } catch (...) {
        return false;
    }
}

GeoTileCache::CoverageInfo GeoTileCache::GetCoverageInfo() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    CoverageInfo info;
    info.minZoom = 99;
    info.maxZoom = 0;

    bool first = true;
    for (const auto& [key, entry] : m_entries) {
        GeoBoundingBox tileBounds = entry.tileId.GetBounds();

        if (first) {
            info.bounds = tileBounds;
            first = false;
        } else {
            info.bounds.Expand(tileBounds.min);
            info.bounds.Expand(tileBounds.max);
        }

        info.minZoom = std::min(info.minZoom, entry.tileId.zoom);
        info.maxZoom = std::max(info.maxZoom, entry.tileId.zoom);
        info.tileCount++;
        info.totalSize += entry.dataSize;
    }

    return info;
}

float GeoTileCache::GetHitRate() const {
    size_t total = m_hitCount + m_missCount;
    return total > 0 ? static_cast<float>(m_hitCount) / total : 0.0f;
}

void GeoTileCache::ResetStatistics() {
    m_hitCount = 0;
    m_missCount = 0;
}

bool GeoTileCache::LoadFromDisk(const TileID& tile, GeoTileData& outData) {
    std::string path = GetDiskPath(tile);

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    try {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return DeserializeTileData(buffer.str(), outData);
    } catch (...) {
        return false;
    }
}

bool GeoTileCache::SaveToDisk(const TileID& tile, const GeoTileData& data) {
    std::string path = GetDiskPath(tile);

    // Create subdirectories
    std::filesystem::path filePath(path);
    try {
        std::filesystem::create_directories(filePath.parent_path());
    } catch (...) {
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    try {
        std::string json = SerializeTileData(data);
        file << json;
        return true;
    } catch (...) {
        return false;
    }
}

std::string GeoTileCache::GetDiskPath(const TileID& tile) const {
    return m_config.cachePath + "/" +
           std::to_string(tile.zoom) + "/" +
           std::to_string(tile.x) + "/" +
           std::to_string(tile.y) + ".tile";
}

std::string GeoTileCache::SerializeTileData(const GeoTileData& data) const {
    nlohmann::json json;

    json["tileId"] = {data.tileId.x, data.tileId.y, data.tileId.zoom};
    json["bounds"] = {
        data.bounds.min.latitude, data.bounds.min.longitude,
        data.bounds.max.latitude, data.bounds.max.longitude
    };
    json["fetchTimestamp"] = data.fetchTimestamp;
    json["expiryTimestamp"] = data.expiryTimestamp;

    // Serialize roads
    json["roads"] = nlohmann::json::array();
    for (const auto& road : data.roads) {
        nlohmann::json r;
        r["id"] = road.id;
        r["name"] = road.name;
        r["type"] = static_cast<int>(road.type);

        r["points"] = nlohmann::json::array();
        for (const auto& p : road.points) {
            r["points"].push_back({p.latitude, p.longitude});
        }

        r["width"] = road.width;
        r["lanes"] = road.lanes;
        r["oneway"] = road.oneway;

        json["roads"].push_back(r);
    }

    // Serialize buildings (abbreviated)
    json["buildings"] = nlohmann::json::array();
    for (const auto& building : data.buildings) {
        nlohmann::json b;
        b["id"] = building.id;
        b["type"] = static_cast<int>(building.type);
        b["height"] = building.height;
        b["levels"] = building.levels;

        b["outline"] = nlohmann::json::array();
        for (const auto& p : building.outline) {
            b["outline"].push_back({p.latitude, p.longitude});
        }

        json["buildings"].push_back(b);
    }

    // Other features serialized similarly...

    return json.dump();
}

bool GeoTileCache::DeserializeTileData(const std::string& jsonStr, GeoTileData& outData) const {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);

        auto tileId = json["tileId"];
        outData.tileId = TileID(tileId[0], tileId[1], tileId[2]);

        auto bounds = json["bounds"];
        outData.bounds = GeoBoundingBox(bounds[0], bounds[1], bounds[2], bounds[3]);

        outData.fetchTimestamp = json["fetchTimestamp"];
        outData.expiryTimestamp = json["expiryTimestamp"];
        outData.status = DataStatus::Cached;

        // Deserialize roads
        for (const auto& r : json["roads"]) {
            GeoRoad road;
            road.id = r["id"];
            road.name = r.value("name", "");
            road.type = static_cast<RoadType>(r["type"].get<int>());
            road.width = r.value("width", 0.0f);
            road.lanes = r.value("lanes", 0);
            road.oneway = r.value("oneway", false);

            for (const auto& p : r["points"]) {
                road.points.emplace_back(p[0], p[1]);
            }

            outData.roads.push_back(std::move(road));
        }

        // Deserialize buildings
        for (const auto& b : json["buildings"]) {
            GeoBuilding building;
            building.id = b["id"];
            building.type = static_cast<BuildingType>(b["type"].get<int>());
            building.height = b.value("height", 0.0f);
            building.levels = b.value("levels", 0);

            for (const auto& p : b["outline"]) {
                building.outline.emplace_back(p[0], p[1]);
            }

            outData.buildings.push_back(std::move(building));
        }

        return true;

    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> GeoTileCache::Compress(const std::string& data) const {
    // Placeholder - would use zlib in production
    return std::vector<uint8_t>(data.begin(), data.end());
}

std::string GeoTileCache::Decompress(const std::vector<uint8_t>& data) const {
    // Placeholder
    return std::string(data.begin(), data.end());
}

size_t GeoTileCache::EstimateSize(const GeoTileData& data) const {
    size_t size = sizeof(GeoTileData);

    for (const auto& road : data.roads) {
        size += sizeof(GeoRoad) + road.points.size() * sizeof(GeoCoordinate);
        size += road.name.size() + road.ref.size();
    }

    for (const auto& building : data.buildings) {
        size += sizeof(GeoBuilding) + building.outline.size() * sizeof(GeoCoordinate);
        size += building.name.size();
    }

    for (const auto& water : data.waterBodies) {
        size += sizeof(GeoWaterBody);
        size += water.outline.size() * sizeof(GeoCoordinate);
        size += water.centerline.size() * sizeof(GeoCoordinate);
    }

    for (const auto& poi : data.pois) {
        size += sizeof(GeoPOI) + poi.name.size();
    }

    for (const auto& lu : data.landUse) {
        size += sizeof(GeoLandUse) + lu.outline.size() * sizeof(GeoCoordinate);
    }

    size += data.elevation.data.size() * sizeof(float);

    return size;
}

void GeoTileCache::TouchLRU(const TileID& tile) {
    std::string key = tile.ToKey();
    auto it = m_lruMap.find(key);
    if (it != m_lruMap.end()) {
        m_lruList.erase(it->second);
        m_lruList.push_front(key);
        it->second = m_lruList.begin();
    }
}

} // namespace Geo
} // namespace Vehement
