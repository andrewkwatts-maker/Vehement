#include "PCGContext.hpp"
#include <algorithm>
#include <cmath>
#include <queue>

namespace Vehement {

// ============================================================================
// Noise Implementation (Simple Perlin/Simplex implementation)
// ============================================================================

namespace {

// Permutation table for noise functions
static const int perm[512] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
    134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
    55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
    18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
    250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
    189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
    172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
    228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
    107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

inline float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

inline float grad(int hash, float x, float y) {
    int h = hash & 7;
    float u = h < 4 ? x : y;
    float v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

float perlin2D(float x, float y) {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);

    float u = fade(x);
    float v = fade(y);

    int A = perm[X] + Y;
    int AA = perm[A];
    int AB = perm[A + 1];
    int B = perm[X + 1] + Y;
    int BA = perm[B];
    int BB = perm[B + 1];

    return lerp(v,
        lerp(u, grad(perm[AA], x, y), grad(perm[BA], x - 1, y)),
        lerp(u, grad(perm[AB], x, y - 1), grad(perm[BB], x - 1, y - 1)));
}

// Simplex noise constants
const float F2 = 0.5f * (std::sqrt(3.0f) - 1.0f);
const float G2 = (3.0f - std::sqrt(3.0f)) / 6.0f;

float simplex2D(float x, float y) {
    float n0, n1, n2;

    float s = (x + y) * F2;
    int i = static_cast<int>(std::floor(x + s));
    int j = static_cast<int>(std::floor(y + s));

    float t = (i + j) * G2;
    float X0 = i - t;
    float Y0 = j - t;
    float x0 = x - X0;
    float y0 = y - Y0;

    int i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else { i1 = 0; j1 = 1; }

    float x1 = x0 - i1 + G2;
    float y1 = y0 - j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    int ii = i & 255;
    int jj = j & 255;
    int gi0 = perm[ii + perm[jj]] % 12;
    int gi1 = perm[ii + i1 + perm[jj + j1]] % 12;
    int gi2 = perm[ii + 1 + perm[jj + 1]] % 12;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 < 0) n0 = 0.0f;
    else {
        t0 *= t0;
        n0 = t0 * t0 * grad(gi0, x0, y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 < 0) n1 = 0.0f;
    else {
        t1 *= t1;
        n1 = t1 * t1 * grad(gi1, x1, y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 < 0) n2 = 0.0f;
    else {
        t2 *= t2;
        n2 = t2 * t2 * grad(gi2, x2, y2);
    }

    return 70.0f * (n0 + n1 + n2);
}

} // anonymous namespace

// ============================================================================
// PCGContext Implementation
// ============================================================================

PCGContext::PCGContext(int width, int height, uint64_t seed)
    : m_width(width)
    , m_height(height)
    , m_seed(seed == 0 ? static_cast<uint64_t>(std::random_device{}()) : seed)
    , m_rng(static_cast<unsigned int>(m_seed))
    , m_tiles(width * height)
    , m_elevations(width * height, 0.0f)
    , m_occupied(width * height, false)
    , m_zones(width * height)
{
    // Initialize tiles with default grass
    for (auto& tile : m_tiles) {
        tile = Tile::Ground(TileType::GroundGrass1);
    }
}

PCGContext::~PCGContext() = default;

// ========== World Coordinates ==========

void PCGContext::SetWorldOffset(int worldX, int worldY) {
    m_worldX = worldX;
    m_worldY = worldY;
}

glm::ivec2 PCGContext::LocalToWorld(int localX, int localY) const {
    return glm::ivec2(m_worldX + localX, m_worldY + localY);
}

glm::ivec2 PCGContext::WorldToLocal(int worldX, int worldY) const {
    return glm::ivec2(worldX - m_worldX, worldY - m_worldY);
}

// ========== Random Number Generation ==========

float PCGContext::Random() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng);
}

float PCGContext::Random(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

int PCGContext::RandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

bool PCGContext::RandomBool(float probability) {
    return Random() < probability;
}

void PCGContext::ResetRNG() {
    m_rng.seed(static_cast<unsigned int>(m_seed));
}

// ========== Noise Functions ==========

float PCGContext::PerlinNoise(float x, float y, float frequency, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    float freq = frequency;

    for (int i = 0; i < octaves; ++i) {
        result += perlin2D(x * freq, y * freq) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }

    return result / maxValue;
}

float PCGContext::SimplexNoise(float x, float y, float frequency, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    float freq = frequency;

    for (int i = 0; i < octaves; ++i) {
        result += simplex2D(x * freq, y * freq) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }

    return result / maxValue;
}

float PCGContext::WorleyNoise(float x, float y, float frequency) {
    x *= frequency;
    y *= frequency;

    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));

    float minDist = 1000.0f;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int cx = xi + dx;
            int cy = yi + dy;

            // Generate cell point using hash
            int hash = perm[(perm[cx & 255] + cy) & 255];
            float px = cx + (hash & 127) / 127.0f;
            float py = cy + ((hash >> 7) & 127) / 127.0f;

            float dist = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
            minDist = std::min(minDist, dist);
        }
    }

    return std::min(1.0f, minDist);
}

float PCGContext::RidgedNoise(float x, float y, float frequency, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float freq = frequency;
    float weight = 1.0f;

    for (int i = 0; i < octaves; ++i) {
        float signal = perlin2D(x * freq, y * freq);
        signal = 1.0f - std::abs(signal);
        signal *= signal * weight;
        weight = std::min(1.0f, std::max(0.0f, signal * 2.0f));
        result += signal * amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }

    return result;
}

float PCGContext::BillowNoise(float x, float y, float frequency, int octaves) {
    float result = 0.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;
    float freq = frequency;

    for (int i = 0; i < octaves; ++i) {
        result += std::abs(perlin2D(x * freq, y * freq)) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        freq *= 2.0f;
    }

    return result / maxValue;
}

// ========== Geographic Data Access ==========

void PCGContext::SetGeoData(std::shared_ptr<GeoTileData> geoData) {
    m_geoData = std::move(geoData);
    m_cacheValid = false;
}

void PCGContext::BuildCache() const {
    if (m_cacheValid || !m_geoData) return;

    m_roadCache.resize(m_height, std::vector<RoadType>(m_width, RoadType::None));
    m_buildingCache.resize(m_height, std::vector<int>(m_width, -1));

    // Rasterize roads into cache
    for (size_t ri = 0; ri < m_geoData->roads.size(); ++ri) {
        const auto& road = m_geoData->roads[ri];
        for (size_t i = 1; i < road.points.size(); ++i) {
            // Simple line rasterization
            glm::vec2 p0 = road.points[i - 1];
            glm::vec2 p1 = road.points[i];

            float dx = std::abs(p1.x - p0.x);
            float dy = std::abs(p1.y - p0.y);
            int steps = static_cast<int>(std::max(dx, dy) * 2.0f);
            if (steps < 1) steps = 1;

            for (int s = 0; s <= steps; ++s) {
                float t = static_cast<float>(s) / steps;
                int x = static_cast<int>(p0.x + (p1.x - p0.x) * t);
                int y = static_cast<int>(p0.y + (p1.y - p0.y) * t);

                if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                    // Mark road and neighbors based on width
                    int halfWidth = static_cast<int>(road.width / 2.0f);
                    for (int oy = -halfWidth; oy <= halfWidth; ++oy) {
                        for (int ox = -halfWidth; ox <= halfWidth; ++ox) {
                            int nx = x + ox;
                            int ny = y + oy;
                            if (nx >= 0 && nx < m_width && ny >= 0 && ny < m_height) {
                                m_roadCache[ny][nx] = road.type;
                            }
                        }
                    }
                }
            }
        }
    }

    // Rasterize buildings into cache
    for (size_t bi = 0; bi < m_geoData->buildings.size(); ++bi) {
        const auto& building = m_geoData->buildings[bi];
        if (building.footprint.empty()) continue;

        // Find bounding box
        float minX = building.footprint[0].x, maxX = building.footprint[0].x;
        float minY = building.footprint[0].y, maxY = building.footprint[0].y;
        for (const auto& pt : building.footprint) {
            minX = std::min(minX, pt.x);
            maxX = std::max(maxX, pt.x);
            minY = std::min(minY, pt.y);
            maxY = std::max(maxY, pt.y);
        }

        // Simple point-in-polygon for each cell
        for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y) {
            for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); ++x) {
                if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

                // Ray casting point-in-polygon test
                float px = x + 0.5f;
                float py = y + 0.5f;
                bool inside = false;

                for (size_t i = 0, j = building.footprint.size() - 1; i < building.footprint.size(); j = i++) {
                    const auto& vi = building.footprint[i];
                    const auto& vj = building.footprint[j];

                    if ((vi.y > py) != (vj.y > py) &&
                        px < (vj.x - vi.x) * (py - vi.y) / (vj.y - vi.y) + vi.x) {
                        inside = !inside;
                    }
                }

                if (inside) {
                    m_buildingCache[y][x] = static_cast<int>(bi);
                }
            }
        }
    }

    m_cacheValid = true;
}

float PCGContext::GetElevation(int x, int y) const {
    if (!InBounds(x, y)) return 0.0f;
    if (m_geoData) return m_geoData->elevation;
    return m_elevations[GetIndex(x, y)];
}

BiomeType PCGContext::GetBiome(int x, int y) const {
    if (!InBounds(x, y)) return BiomeType::Unknown;
    if (m_geoData) return m_geoData->biome;
    return BiomeType::Grassland;
}

std::string PCGContext::GetBiomeName(int x, int y) const {
    return GetBiomeTypeName(GetBiome(x, y));
}

bool PCGContext::IsWater(int x, int y) const {
    if (!InBounds(x, y)) return false;
    if (m_geoData) return m_geoData->hasWater;
    return false;
}

bool PCGContext::IsRoad(int x, int y) const {
    if (!InBounds(x, y)) return false;
    BuildCache();
    if (m_cacheValid && !m_roadCache.empty()) {
        return m_roadCache[y][x] != RoadType::None;
    }
    return false;
}

RoadType PCGContext::GetRoadType(int x, int y) const {
    if (!InBounds(x, y)) return RoadType::None;
    BuildCache();
    if (m_cacheValid && !m_roadCache.empty()) {
        return m_roadCache[y][x];
    }
    return RoadType::None;
}

std::string PCGContext::GetRoadTypeName(int x, int y) const {
    return Vehement::GetRoadTypeName(GetRoadType(x, y));
}

const GeoBuilding* PCGContext::GetBuilding(int x, int y) const {
    if (!InBounds(x, y) || !m_geoData) return nullptr;
    BuildCache();
    if (m_cacheValid && !m_buildingCache.empty()) {
        int idx = m_buildingCache[y][x];
        if (idx >= 0 && idx < static_cast<int>(m_geoData->buildings.size())) {
            return &m_geoData->buildings[idx];
        }
    }
    return nullptr;
}

std::vector<const GeoRoad*> PCGContext::GetNearbyRoads(int x, int y, float radius) const {
    std::vector<const GeoRoad*> result;
    if (!m_geoData) return result;

    float r2 = radius * radius;
    for (const auto& road : m_geoData->roads) {
        for (const auto& pt : road.points) {
            float dx = pt.x - x;
            float dy = pt.y - y;
            if (dx * dx + dy * dy <= r2) {
                result.push_back(&road);
                break;
            }
        }
    }
    return result;
}

std::vector<const GeoBuilding*> PCGContext::GetNearbyBuildings(int x, int y, float radius) const {
    std::vector<const GeoBuilding*> result;
    if (!m_geoData) return result;

    float r2 = radius * radius;
    for (const auto& building : m_geoData->buildings) {
        for (const auto& pt : building.footprint) {
            float dx = pt.x - x;
            float dy = pt.y - y;
            if (dx * dx + dy * dy <= r2) {
                result.push_back(&building);
                break;
            }
        }
    }
    return result;
}

float PCGContext::GetPopulationDensity(int x, int y) const {
    if (!InBounds(x, y)) return 0.0f;
    if (m_geoData) return m_geoData->populationDensity;
    return 0.0f;
}

float PCGContext::GetTreeDensity(int x, int y) const {
    if (!InBounds(x, y)) return 0.0f;
    if (m_geoData) return m_geoData->treeDensity;
    return 0.0f;
}

// ========== Tile Output ==========

TileType PCGContext::TileTypeFromName(const std::string& name) {
    // Map string names to tile types
    static const std::unordered_map<std::string, TileType> nameMap = {
        {"none", TileType::None},
        {"grass", TileType::GroundGrass1},
        {"grass1", TileType::GroundGrass1},
        {"grass2", TileType::GroundGrass2},
        {"dirt", TileType::GroundDirt},
        {"forest", TileType::GroundForest1},
        {"forest1", TileType::GroundForest1},
        {"forest2", TileType::GroundForest2},
        {"rocks", TileType::GroundRocks},
        {"asphalt", TileType::ConcreteAsphalt1},
        {"asphalt1", TileType::ConcreteAsphalt1},
        {"asphalt2", TileType::ConcreteAsphalt2},
        {"road", TileType::ConcreteAsphalt1},
        {"highway", TileType::ConcreteAsphalt2},
        {"main_road", TileType::ConcreteAsphalt1},
        {"secondary_road", TileType::ConcreteAsphalt1},
        {"residential", TileType::ConcreteAsphalt1},
        {"path", TileType::GroundDirt},
        {"concrete", TileType::ConcreteBlocks1},
        {"concrete_pad", TileType::ConcretePad},
        {"tiles", TileType::ConcreteTiles1},
        {"bricks", TileType::BricksRock},
        {"bricks_black", TileType::BricksBlack},
        {"bricks_grey", TileType::BricksGrey},
        {"wood", TileType::Wood1},
        {"wood_floor", TileType::WoodFlooring1},
        {"water", TileType::Water1},
        {"metal", TileType::Metal1},
        {"stone", TileType::StoneRaw},
        {"marble", TileType::StoneMarble1}
    };

    auto it = nameMap.find(name);
    if (it != nameMap.end()) {
        return it->second;
    }
    return TileType::GroundGrass1;
}

void PCGContext::SetTile(int x, int y, TileType type) {
    if (!InBounds(x, y)) return;
    m_tiles[GetIndex(x, y)] = Tile::Ground(type);
}

void PCGContext::SetTile(int x, int y, const std::string& typeName) {
    SetTile(x, y, TileTypeFromName(typeName));
}

TileType PCGContext::GetTile(int x, int y) const {
    if (!InBounds(x, y)) return TileType::None;
    return m_tiles[GetIndex(x, y)].type;
}

void PCGContext::SetElevation(int x, int y, float elevation) {
    if (!InBounds(x, y)) return;
    m_elevations[GetIndex(x, y)] = elevation;
}

void PCGContext::SetWall(int x, int y, TileType type, float height) {
    if (!InBounds(x, y)) return;
    m_tiles[GetIndex(x, y)] = Tile::Wall(type, type, height);
}

bool PCGContext::IsWalkable(int x, int y) const {
    if (!InBounds(x, y)) return false;
    return !m_tiles[GetIndex(x, y)].BlocksMovement();
}

void PCGContext::ClearTile(int x, int y) {
    if (!InBounds(x, y)) return;
    m_tiles[GetIndex(x, y)] = Tile::Ground(TileType::GroundGrass1);
}

void PCGContext::FillRect(int x, int y, int width, int height, TileType type) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            SetTile(x + dx, y + dy, type);
        }
    }
}

void PCGContext::DrawLine(int x1, int y1, int x2, int y2, TileType type) {
    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        SetTile(x1, y1, type);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

// ========== Foliage Output ==========

void PCGContext::SpawnFoliage(int x, int y, const std::string& foliageType, float scale) {
    if (!InBounds(x, y)) return;

    PCGFoliage foliage;
    foliage.foliageType = foliageType;
    foliage.position = glm::vec3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(y) + 0.5f);
    foliage.scale = scale;
    foliage.rotation = Random() * 6.28318f;
    m_foliageSpawns.push_back(foliage);
}

void PCGContext::SpawnFoliageWorld(float worldX, float worldY, const std::string& foliageType, float scale) {
    PCGFoliage foliage;
    foliage.foliageType = foliageType;
    foliage.position = glm::vec3(worldX, 0.0f, worldY);
    foliage.scale = scale;
    foliage.rotation = Random() * 6.28318f;
    m_foliageSpawns.push_back(foliage);
}

void PCGContext::ClearFoliageSpawns() {
    m_foliageSpawns.clear();
}

// ========== Entity Output ==========

void PCGContext::SpawnEntity(int x, int y, const std::string& entityType) {
    if (!InBounds(x, y)) return;

    PCGEntitySpawn spawn;
    spawn.entityType = entityType;
    spawn.position = glm::vec3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(y) + 0.5f);
    spawn.rotation = Random() * 6.28318f;
    m_entitySpawns.push_back(spawn);
}

void PCGContext::SpawnEntity(int x, int y, const std::string& entityType,
                             const std::unordered_map<std::string, std::string>& properties) {
    if (!InBounds(x, y)) return;

    PCGEntitySpawn spawn;
    spawn.entityType = entityType;
    spawn.position = glm::vec3(static_cast<float>(x) + 0.5f, 0.0f, static_cast<float>(y) + 0.5f);
    spawn.rotation = Random() * 6.28318f;
    spawn.properties = properties;
    m_entitySpawns.push_back(spawn);
}

void PCGContext::SpawnEntityWorld(float worldX, float worldY, const std::string& entityType) {
    PCGEntitySpawn spawn;
    spawn.entityType = entityType;
    spawn.position = glm::vec3(worldX, 0.0f, worldY);
    spawn.rotation = Random() * 6.28318f;
    m_entitySpawns.push_back(spawn);
}

void PCGContext::ClearEntitySpawns() {
    m_entitySpawns.clear();
}

// ========== Utility Functions ==========

bool PCGContext::InBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

glm::ivec2 PCGContext::Clamp(int x, int y) const {
    return glm::ivec2(
        std::max(0, std::min(x, m_width - 1)),
        std::max(0, std::min(y, m_height - 1))
    );
}

float PCGContext::Distance(int x1, int y1, int x2, int y2) const {
    float dx = static_cast<float>(x2 - x1);
    float dy = static_cast<float>(y2 - y1);
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<glm::ivec2> PCGContext::FindPath(int startX, int startY, int endX, int endY) const {
    std::vector<glm::ivec2> path;
    if (!InBounds(startX, startY) || !InBounds(endX, endY)) return path;

    // A* pathfinding
    struct Node {
        int x, y;
        float g, h;
        int parentX, parentY;
        bool operator>(const Node& other) const { return (g + h) > (other.g + other.h); }
    };

    std::vector<std::vector<bool>> closed(m_height, std::vector<bool>(m_width, false));
    std::vector<std::vector<float>> gScore(m_height, std::vector<float>(m_width, 1e9f));
    std::vector<std::vector<glm::ivec2>> parent(m_height, std::vector<glm::ivec2>(m_width, {-1, -1}));

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    gScore[startY][startX] = 0.0f;
    open.push({startX, startY, 0.0f, Distance(startX, startY, endX, endY), -1, -1});

    const int dx[] = {0, 1, 0, -1, 1, 1, -1, -1};
    const int dy[] = {-1, 0, 1, 0, -1, 1, 1, -1};

    while (!open.empty()) {
        Node current = open.top();
        open.pop();

        if (current.x == endX && current.y == endY) {
            // Reconstruct path
            int cx = endX, cy = endY;
            while (cx != -1 && cy != -1) {
                path.push_back({cx, cy});
                glm::ivec2 p = parent[cy][cx];
                cx = p.x;
                cy = p.y;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (closed[current.y][current.x]) continue;
        closed[current.y][current.x] = true;

        for (int i = 0; i < 8; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (!InBounds(nx, ny) || closed[ny][nx] || !IsWalkable(nx, ny)) continue;

            float moveCost = (i < 4) ? 1.0f : 1.414f;
            float newG = current.g + moveCost;

            if (newG < gScore[ny][nx]) {
                gScore[ny][nx] = newG;
                parent[ny][nx] = {current.x, current.y};
                open.push({nx, ny, newG, Distance(nx, ny, endX, endY), current.x, current.y});
            }
        }
    }

    return path; // Empty if no path found
}

bool PCGContext::HasLineOfSight(int x1, int y1, int x2, int y2) const {
    // Bresenham-based line of sight
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (!InBounds(x1, y1) || m_tiles[GetIndex(x1, y1)].blocksSight) {
            return false;
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }

    return true;
}

// ========== Context Metadata ==========

void PCGContext::SetData(const std::string& key, const std::string& value) {
    m_customData[key] = value;
}

std::string PCGContext::GetData(const std::string& key) const {
    auto it = m_customData.find(key);
    return (it != m_customData.end()) ? it->second : "";
}

bool PCGContext::HasData(const std::string& key) const {
    return m_customData.find(key) != m_customData.end();
}

// ========== Stage Communication ==========

void PCGContext::MarkOccupied(int x, int y) {
    if (InBounds(x, y)) {
        m_occupied[GetIndex(x, y)] = true;
    }
}

bool PCGContext::IsOccupied(int x, int y) const {
    if (!InBounds(x, y)) return true;
    return m_occupied[GetIndex(x, y)];
}

void PCGContext::ClearOccupied() {
    std::fill(m_occupied.begin(), m_occupied.end(), false);
}

void PCGContext::MarkZone(int x, int y, int width, int height, const std::string& zoneType) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (InBounds(px, py)) {
                m_zones[GetIndex(px, py)] = zoneType;
            }
        }
    }
}

std::string PCGContext::GetZone(int x, int y) const {
    if (!InBounds(x, y)) return "";
    return m_zones[GetIndex(x, y)];
}

} // namespace Vehement
