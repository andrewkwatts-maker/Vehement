#include "ElevationProvider.hpp"
#include "GeoTileCache.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Vehement {
namespace Geo {

// =============================================================================
// ElevationConfig Implementation
// =============================================================================

ElevationConfig ElevationConfig::LoadFromFile(const std::string& path) {
    ElevationConfig config;

    std::ifstream file(path);
    if (!file.is_open()) return config;

    try {
        nlohmann::json json;
        file >> json;

        if (json.contains("openElevationEndpoint")) config.openElevationEndpoint = json["openElevationEndpoint"];
        if (json.contains("mapzenEndpoint")) config.mapzenEndpoint = json["mapzenEndpoint"];
        if (json.contains("srtmEndpoint")) config.srtmEndpoint = json["srtmEndpoint"];
        if (json.contains("requestsPerSecond")) config.requestsPerSecond = json["requestsPerSecond"];
        if (json.contains("batchSize")) config.batchSize = json["batchSize"];
        if (json.contains("defaultResolution")) config.defaultResolution = json["defaultResolution"];
        if (json.contains("localDEMPath")) config.localDEMPath = json["localDEMPath"];

    } catch (...) {}

    return config;
}

void ElevationConfig::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    json["openElevationEndpoint"] = openElevationEndpoint;
    json["mapzenEndpoint"] = mapzenEndpoint;
    json["srtmEndpoint"] = srtmEndpoint;
    json["requestsPerSecond"] = requestsPerSecond;
    json["batchSize"] = batchSize;
    json["defaultResolution"] = defaultResolution;
    json["localDEMPath"] = localDEMPath;

    std::ofstream file(path);
    if (file.is_open()) {
        file << json.dump(2);
    }
}

// =============================================================================
// ElevationProvider Implementation
// =============================================================================

ElevationProvider::ElevationProvider()
    : m_rateLimiter(1.0, 3) {
    m_httpClient = std::make_unique<CurlHttpClient>();
}

ElevationProvider::~ElevationProvider() {
    Shutdown();
}

bool ElevationProvider::Initialize(const std::string& configPath) {
    if (!configPath.empty()) {
        m_config = ElevationConfig::LoadFromFile(configPath);
    }

    m_httpClient->SetTimeout(30);
    m_httpClient->SetUserAgent("Vehement2-GeoData/1.0");
    m_rateLimiter.SetRate(m_config.requestsPerSecond);

    // Load local DEM files if configured
    if (!m_config.localDEMPath.empty()) {
        LoadLocalDEM(m_config.localDEMPath);
    }

    return true;
}

void ElevationProvider::Shutdown() {
    std::lock_guard<std::mutex> lock(m_demMutex);
    m_localDEMs.clear();
}

float ElevationProvider::GetElevation(const GeoCoordinate& coord) {
    auto elevations = GetElevations({coord});
    return elevations.empty() ? std::numeric_limits<float>::quiet_NaN() : elevations[0];
}

std::vector<float> ElevationProvider::GetElevations(const std::vector<GeoCoordinate>& coords) {
    if (coords.empty()) return {};

    // Try local DEM first
    {
        std::lock_guard<std::mutex> lock(m_demMutex);
        for (const auto& dem : m_localDEMs) {
            bool allCovered = true;
            for (const auto& c : coords) {
                if (!dem.bounds.Contains(c)) {
                    allCovered = false;
                    break;
                }
            }
            if (allCovered) {
                return FetchFromLocalDEM(coords);
            }
        }
    }

    // Fall back to API
    return FetchFromOpenElevation(coords);
}

ElevationGrid ElevationProvider::GetElevationGrid(const GeoBoundingBox& bounds, int resolution) {
    if (resolution <= 0) resolution = m_config.defaultResolution;

    // Calculate grid dimensions
    double widthMeters = bounds.GetWidthMeters();
    double heightMeters = bounds.GetHeightMeters();

    int width = std::max(2, static_cast<int>(widthMeters / resolution) + 1);
    int height = std::max(2, static_cast<int>(heightMeters / resolution) + 1);

    // Cap dimensions
    width = std::min(width, 256);
    height = std::min(height, 256);

    // Create sample points
    auto samplePoints = CreateGridSamplePoints(bounds, width, height);

    // Fetch elevations
    auto elevations = GetElevations(samplePoints);

    // Build grid
    ElevationGrid grid;
    grid.bounds = bounds;
    grid.width = width;
    grid.height = height;
    grid.data = std::move(elevations);
    grid.noDataValue = -9999.0f;

    return grid;
}

ElevationGrid ElevationProvider::GetElevationGridForTile(const TileID& tile, int resolution) {
    return GetElevationGrid(tile.GetBounds(), resolution);
}

float ElevationProvider::GetSlope(const GeoCoordinate& coord) {
    // Get elevations at cardinal neighbors
    double offset = 0.0001;  // ~11 meters

    float eN = GetElevation(GeoCoordinate(coord.latitude + offset, coord.longitude));
    float eS = GetElevation(GeoCoordinate(coord.latitude - offset, coord.longitude));
    float eE = GetElevation(GeoCoordinate(coord.latitude, coord.longitude + offset));
    float eW = GetElevation(GeoCoordinate(coord.latitude, coord.longitude - offset));

    if (std::isnan(eN) || std::isnan(eS) || std::isnan(eE) || std::isnan(eW)) {
        return 0.0f;
    }

    // Calculate distance in meters
    double distNS = GeoCoordinate(coord.latitude - offset, coord.longitude)
                   .DistanceTo(GeoCoordinate(coord.latitude + offset, coord.longitude));
    double distEW = GeoCoordinate(coord.latitude, coord.longitude - offset)
                   .DistanceTo(GeoCoordinate(coord.latitude, coord.longitude + offset));

    float dzdx = (eE - eW) / static_cast<float>(distEW);
    float dzdy = (eN - eS) / static_cast<float>(distNS);

    float slope = std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy));
    return static_cast<float>(RadToDeg(slope));
}

float ElevationProvider::GetAspect(const GeoCoordinate& coord) {
    double offset = 0.0001;

    float eN = GetElevation(GeoCoordinate(coord.latitude + offset, coord.longitude));
    float eS = GetElevation(GeoCoordinate(coord.latitude - offset, coord.longitude));
    float eE = GetElevation(GeoCoordinate(coord.latitude, coord.longitude + offset));
    float eW = GetElevation(GeoCoordinate(coord.latitude, coord.longitude - offset));

    if (std::isnan(eN) || std::isnan(eS) || std::isnan(eE) || std::isnan(eW)) {
        return 0.0f;
    }

    float dzdx = eE - eW;
    float dzdy = eN - eS;

    float aspect = static_cast<float>(RadToDeg(std::atan2(-dzdy, dzdx)));
    return std::fmod(aspect + 360.0f, 360.0f);
}

float ElevationProvider::GetRoughness(const GeoCoordinate& coord, float radius) {
    // Sample points in a circle around the coordinate
    const int numSamples = 16;
    std::vector<GeoCoordinate> samples;
    samples.push_back(coord);

    for (int i = 0; i < numSamples; ++i) {
        float angle = static_cast<float>(i) / numSamples * 360.0f;
        samples.push_back(coord.Offset(radius, angle));
    }

    auto elevations = GetElevations(samples);

    // Calculate variance
    float sum = 0.0f;
    float sumSq = 0.0f;
    int count = 0;

    for (float e : elevations) {
        if (!std::isnan(e)) {
            sum += e;
            sumSq += e * e;
            ++count;
        }
    }

    if (count < 2) return 0.0f;

    float mean = sum / count;
    float variance = (sumSq / count) - (mean * mean);
    float stdDev = std::sqrt(std::max(0.0f, variance));

    // Normalize roughness to 0-1 (assuming max roughness ~100m std dev)
    return std::min(1.0f, stdDev / 100.0f);
}

std::vector<std::vector<bool>> ElevationProvider::CalculateViewshed(
    const GeoCoordinate& observer,
    float height,
    float radius,
    int resolution) {

    // Create grid around observer
    GeoBoundingBox bounds = GeoBoundingBox::FromCenterRadius(observer, radius);
    auto grid = GetElevationGrid(bounds, resolution);

    float observerElev = grid.SampleElevation(observer) + height;

    std::vector<std::vector<bool>> viewshed(grid.height, std::vector<bool>(grid.width, false));

    // Simple ray-casting viewshed algorithm
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            // Calculate coordinate of this cell
            double fx = static_cast<double>(x) / (grid.width - 1);
            double fy = static_cast<double>(y) / (grid.height - 1);

            GeoCoordinate target(
                bounds.max.latitude - fy * bounds.GetHeightDegrees(),
                bounds.min.longitude + fx * bounds.GetWidthDegrees()
            );

            float targetElev = grid.GetElevation(x, y);
            if (targetElev == grid.noDataValue) continue;

            // Check line of sight
            bool visible = true;
            float dist = static_cast<float>(observer.DistanceTo(target));
            int steps = std::max(2, static_cast<int>(dist / resolution));

            float maxAngle = -90.0f;

            for (int s = 1; s < steps && visible; ++s) {
                float t = static_cast<float>(s) / steps;
                GeoCoordinate sample(
                    observer.latitude + t * (target.latitude - observer.latitude),
                    observer.longitude + t * (target.longitude - observer.longitude)
                );

                float sampleElev = grid.SampleElevation(sample);
                if (sampleElev == grid.noDataValue) continue;

                float sampleDist = static_cast<float>(observer.DistanceTo(sample));
                float angle = static_cast<float>(RadToDeg(std::atan2(sampleElev - observerElev, sampleDist)));

                if (angle > maxAngle) {
                    maxAngle = angle;
                }
            }

            // Final angle to target
            float targetAngle = static_cast<float>(RadToDeg(std::atan2(targetElev - observerElev, dist)));
            viewshed[y][x] = (targetAngle >= maxAngle);
        }
    }

    return viewshed;
}

std::vector<uint8_t> ElevationProvider::GenerateHeightmap(
    const ElevationGrid& grid,
    float minElev,
    float maxElev) {

    if (minElev < 0 || maxElev < 0) {
        auto [mi, ma] = grid.GetMinMax();
        if (minElev < 0) minElev = mi;
        if (maxElev < 0) maxElev = ma;
    }

    return grid.GenerateHeightmap();
}

std::vector<uint16_t> ElevationProvider::GenerateHeightmap16(
    const ElevationGrid& grid,
    float minElev,
    float maxElev) {

    if (minElev < 0 || maxElev < 0) {
        auto [mi, ma] = grid.GetMinMax();
        if (minElev < 0) minElev = mi;
        if (maxElev < 0) maxElev = ma;
    }

    std::vector<uint16_t> heightmap(grid.width * grid.height);
    float range = maxElev - minElev;
    if (range < 0.001f) range = 1.0f;

    for (size_t i = 0; i < grid.data.size(); ++i) {
        float v = grid.data[i];
        if (v == grid.noDataValue) {
            heightmap[i] = 0;
        } else {
            float normalized = (v - minElev) / range;
            heightmap[i] = static_cast<uint16_t>(std::clamp(normalized * 65535.0f, 0.0f, 65535.0f));
        }
    }

    return heightmap;
}

std::vector<uint8_t> ElevationProvider::GenerateNormalMap(
    const ElevationGrid& grid,
    float strength) {

    auto baseNormals = grid.GenerateNormalMap();

    // Apply strength modifier
    if (std::abs(strength - 1.0f) > 0.001f) {
        for (size_t i = 0; i < baseNormals.size(); i += 3) {
            float nx = (baseNormals[i + 0] / 255.0f - 0.5f) * 2.0f * strength;
            float ny = (baseNormals[i + 1] / 255.0f - 0.5f) * 2.0f * strength;
            float nz = baseNormals[i + 2] / 255.0f;

            // Renormalize
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.001f) {
                nx /= len;
                ny /= len;
                nz /= len;
            }

            baseNormals[i + 0] = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255.0f);
            baseNormals[i + 1] = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255.0f);
            baseNormals[i + 2] = static_cast<uint8_t>((nz * 0.5f + 0.5f) * 255.0f);
        }
    }

    return baseNormals;
}

std::vector<uint8_t> ElevationProvider::GenerateSlopeMap(const ElevationGrid& grid) {
    std::vector<uint8_t> slopeMap(grid.width * grid.height);

    float cellSizeX = static_cast<float>(grid.bounds.GetWidthMeters()) / (grid.width - 1);
    float cellSizeY = static_cast<float>(grid.bounds.GetHeightMeters()) / (grid.height - 1);

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            float eL = grid.GetElevation(std::max(0, x - 1), y);
            float eR = grid.GetElevation(std::min(grid.width - 1, x + 1), y);
            float eU = grid.GetElevation(x, std::max(0, y - 1));
            float eD = grid.GetElevation(x, std::min(grid.height - 1, y + 1));

            float slope = 0.0f;
            if (eL != grid.noDataValue && eR != grid.noDataValue &&
                eU != grid.noDataValue && eD != grid.noDataValue) {
                float dzdx = (eR - eL) / (2.0f * cellSizeX);
                float dzdy = (eD - eU) / (2.0f * cellSizeY);
                slope = static_cast<float>(RadToDeg(std::atan(std::sqrt(dzdx * dzdx + dzdy * dzdy))));
            }

            // Map 0-90 degrees to 0-255
            slopeMap[y * grid.width + x] = static_cast<uint8_t>(std::clamp(slope / 90.0f * 255.0f, 0.0f, 255.0f));
        }
    }

    return slopeMap;
}

std::vector<uint8_t> ElevationProvider::GenerateAspectMap(const ElevationGrid& grid) {
    std::vector<uint8_t> aspectMap(grid.width * grid.height);

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            float eL = grid.GetElevation(std::max(0, x - 1), y);
            float eR = grid.GetElevation(std::min(grid.width - 1, x + 1), y);
            float eU = grid.GetElevation(x, std::max(0, y - 1));
            float eD = grid.GetElevation(x, std::min(grid.height - 1, y + 1));

            float aspect = 0.0f;
            if (eL != grid.noDataValue && eR != grid.noDataValue &&
                eU != grid.noDataValue && eD != grid.noDataValue) {
                float dzdx = eR - eL;
                float dzdy = eD - eU;
                aspect = static_cast<float>(RadToDeg(std::atan2(-dzdy, dzdx)));
                aspect = std::fmod(aspect + 360.0f, 360.0f);
            }

            // Map 0-360 degrees to 0-255
            aspectMap[y * grid.width + x] = static_cast<uint8_t>(aspect / 360.0f * 255.0f);
        }
    }

    return aspectMap;
}

bool ElevationProvider::LoadLocalDEM(const std::string& path) {
    // Note: Full GeoTIFF parsing would require GDAL or similar library
    // This is a placeholder that shows the interface
    // In production, integrate with GDAL for proper GeoTIFF support

    std::lock_guard<std::mutex> lock(m_demMutex);

    LocalDEM dem;
    dem.path = path;

    // Placeholder: In real implementation, parse GeoTIFF header
    // to get bounds, dimensions, and data

    m_localDEMs.push_back(std::move(dem));
    return true;
}

bool ElevationProvider::HasLocalCoverage(const GeoCoordinate& coord) const {
    std::lock_guard<std::mutex> lock(m_demMutex);

    for (const auto& dem : m_localDEMs) {
        if (dem.bounds.Contains(coord)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ElevationProvider::GetLoadedDEMFiles() const {
    std::lock_guard<std::mutex> lock(m_demMutex);

    std::vector<std::string> files;
    for (const auto& dem : m_localDEMs) {
        files.push_back(dem.path);
    }
    return files;
}

std::vector<float> ElevationProvider::FetchFromOpenElevation(
    const std::vector<GeoCoordinate>& coords) {

    std::vector<float> results(coords.size(), std::numeric_limits<float>::quiet_NaN());

    // Batch requests
    for (size_t start = 0; start < coords.size(); start += m_config.batchSize) {
        size_t end = std::min(start + m_config.batchSize, coords.size());

        // Build request body
        nlohmann::json locations = nlohmann::json::array();
        for (size_t i = start; i < end; ++i) {
            locations.push_back({
                {"latitude", coords[i].latitude},
                {"longitude", coords[i].longitude}
            });
        }

        nlohmann::json requestBody;
        requestBody["locations"] = locations;

        // Rate limit and send request
        m_rateLimiter.Acquire();
        ++m_requestCount;

        HttpResponse response = m_httpClient->Post(
            m_config.openElevationEndpoint,
            requestBody.dump(),
            "application/json"
        );

        if (!response.IsSuccess()) continue;

        try {
            auto json = nlohmann::json::parse(response.body);
            if (json.contains("results")) {
                for (size_t i = 0; i < json["results"].size() && (start + i) < results.size(); ++i) {
                    if (json["results"][i].contains("elevation")) {
                        results[start + i] = json["results"][i]["elevation"];
                    }
                }
            }
        } catch (...) {
            // Continue with NaN values
        }
    }

    return results;
}

std::vector<float> ElevationProvider::FetchFromLocalDEM(
    const std::vector<GeoCoordinate>& coords) {

    std::vector<float> results(coords.size(), std::numeric_limits<float>::quiet_NaN());

    std::lock_guard<std::mutex> lock(m_demMutex);

    for (size_t i = 0; i < coords.size(); ++i) {
        for (const auto& dem : m_localDEMs) {
            if (dem.bounds.Contains(coords[i]) && !dem.data.empty()) {
                // Calculate grid position
                double fx = (coords[i].longitude - dem.bounds.min.longitude) /
                           dem.bounds.GetWidthDegrees() * (dem.width - 1);
                double fy = (dem.bounds.max.latitude - coords[i].latitude) /
                           dem.bounds.GetHeightDegrees() * (dem.height - 1);

                results[i] = InterpolateElevation(
                    ElevationGrid{dem.bounds, dem.width, dem.height, dem.data, dem.noDataValue},
                    fx, fy);
                break;
            }
        }
    }

    return results;
}

float ElevationProvider::InterpolateElevation(const ElevationGrid& grid, double x, double y) {
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, grid.width - 1);
    int y1 = std::min(y0 + 1, grid.height - 1);

    float fracX = static_cast<float>(x - x0);
    float fracY = static_cast<float>(y - y0);

    float e00 = grid.GetElevation(x0, y0);
    float e10 = grid.GetElevation(x1, y0);
    float e01 = grid.GetElevation(x0, y1);
    float e11 = grid.GetElevation(x1, y1);

    if (e00 == grid.noDataValue || e10 == grid.noDataValue ||
        e01 == grid.noDataValue || e11 == grid.noDataValue) {
        return grid.noDataValue;
    }

    float e0 = e00 * (1 - fracX) + e10 * fracX;
    float e1 = e01 * (1 - fracX) + e11 * fracX;

    return e0 * (1 - fracY) + e1 * fracY;
}

std::vector<GeoCoordinate> ElevationProvider::CreateGridSamplePoints(
    const GeoBoundingBox& bounds, int width, int height) {

    std::vector<GeoCoordinate> points;
    points.reserve(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double fx = static_cast<double>(x) / (width - 1);
            double fy = static_cast<double>(y) / (height - 1);

            double lat = bounds.max.latitude - fy * bounds.GetHeightDegrees();
            double lon = bounds.min.longitude + fx * bounds.GetWidthDegrees();

            points.emplace_back(lat, lon);
        }
    }

    return points;
}

// =============================================================================
// TerrainMeshGenerator Implementation
// =============================================================================

TerrainMeshGenerator::TerrainMesh TerrainMeshGenerator::GenerateMesh(
    const ElevationGrid& grid,
    float scale,
    float uvScale) {

    TerrainMesh mesh;
    mesh.bounds = grid.bounds;

    if (grid.width < 2 || grid.height < 2) return mesh;

    auto [minElev, maxElev] = grid.GetMinMax();
    mesh.minElevation = minElev;
    mesh.maxElevation = maxElev;

    // Generate vertices
    mesh.vertices.reserve(grid.width * grid.height);

    float cellSizeX = static_cast<float>(grid.bounds.GetWidthMeters()) / (grid.width - 1) * scale;
    float cellSizeY = static_cast<float>(grid.bounds.GetHeightMeters()) / (grid.height - 1) * scale;

    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            MeshVertex vertex;

            float elev = grid.GetElevation(x, y);
            if (elev == grid.noDataValue) elev = minElev;

            vertex.position = glm::vec3(
                x * cellSizeX,
                elev * scale,
                y * cellSizeY
            );

            vertex.texCoord = glm::vec2(
                static_cast<float>(x) / (grid.width - 1) * uvScale,
                static_cast<float>(y) / (grid.height - 1) * uvScale
            );

            // Calculate normal from neighbors
            float eL = grid.GetElevation(std::max(0, x - 1), y);
            float eR = grid.GetElevation(std::min(grid.width - 1, x + 1), y);
            float eU = grid.GetElevation(x, std::max(0, y - 1));
            float eD = grid.GetElevation(x, std::min(grid.height - 1, y + 1));

            if (eL == grid.noDataValue) eL = elev;
            if (eR == grid.noDataValue) eR = elev;
            if (eU == grid.noDataValue) eU = elev;
            if (eD == grid.noDataValue) eD = elev;

            glm::vec3 tangentX(2.0f * cellSizeX, (eR - eL) * scale, 0.0f);
            glm::vec3 tangentZ(0.0f, (eD - eU) * scale, 2.0f * cellSizeY);
            vertex.normal = glm::normalize(glm::cross(tangentZ, tangentX));

            mesh.vertices.push_back(vertex);
        }
    }

    // Generate indices (triangle strip could be more efficient)
    mesh.indices.reserve((grid.width - 1) * (grid.height - 1) * 6);

    for (int y = 0; y < grid.height - 1; ++y) {
        for (int x = 0; x < grid.width - 1; ++x) {
            uint32_t topLeft = y * grid.width + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * grid.width + x;
            uint32_t bottomRight = bottomLeft + 1;

            // First triangle
            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            // Second triangle
            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    return mesh;
}

TerrainMeshGenerator::TerrainMesh TerrainMeshGenerator::GenerateLODMesh(
    const ElevationGrid& grid,
    int lodLevel,
    float scale) {

    int step = 1 << lodLevel;  // 1, 2, 4, 8, etc.

    // Create reduced grid
    int newWidth = (grid.width - 1) / step + 1;
    int newHeight = (grid.height - 1) / step + 1;

    ElevationGrid lodGrid;
    lodGrid.bounds = grid.bounds;
    lodGrid.width = newWidth;
    lodGrid.height = newHeight;
    lodGrid.noDataValue = grid.noDataValue;
    lodGrid.data.resize(newWidth * newHeight);

    for (int y = 0; y < newHeight; ++y) {
        for (int x = 0; x < newWidth; ++x) {
            int srcX = std::min(x * step, grid.width - 1);
            int srcY = std::min(y * step, grid.height - 1);
            lodGrid.data[y * newWidth + x] = grid.GetElevation(srcX, srcY);
        }
    }

    return GenerateMesh(lodGrid, scale);
}

TerrainMeshGenerator::TerrainMesh TerrainMeshGenerator::GenerateAdaptiveMesh(
    const ElevationGrid& grid,
    float slopeThreshold,
    float scale) {

    // For now, use standard mesh generation
    // A full implementation would use quadtree subdivision based on slope
    return GenerateMesh(grid, scale);
}

} // namespace Geo
} // namespace Vehement
