#include "World.hpp"
#include "../../engine/graphics/Renderer.hpp"
#include "../../engine/scene/Camera.hpp"
#include "../../engine/pathfinding/Graph.hpp"
#include "../../engine/pathfinding/Pathfinder.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cmath>

namespace Vehement {

// ========== SpawnPoint Implementation ==========

glm::vec3 SpawnPoint::GetRandomPosition() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    float angle = dist(gen) * 3.14159f;
    float r = std::sqrt(std::abs(dist(gen))) * radius;

    return position + glm::vec3(
        std::cos(angle) * r,
        0.0f,
        std::sin(angle) * r
    );
}

// ========== Zone Implementation ==========

bool Zone::Contains(const glm::vec3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool Zone::Intersects(const glm::vec3& center, float radius) const {
    // Find closest point on AABB to sphere center
    glm::vec3 closest;
    closest.x = std::max(min.x, std::min(center.x, max.x));
    closest.y = std::max(min.y, std::min(center.y, max.y));
    closest.z = std::max(min.z, std::min(center.z, max.z));

    // Check if closest point is within sphere
    float distSq = glm::dot(closest - center, closest - center);
    return distSq <= radius * radius;
}

// ========== World Implementation ==========

World::World() = default;

World::~World() {
    Shutdown();
}

bool World::Initialize(Nova::Renderer& renderer, const WorldConfig& config) {
    m_renderer = &renderer;
    m_config = config;

    // Initialize tile map
    TileMapConfig mapConfig;
    mapConfig.width = config.mapWidth;
    mapConfig.height = config.mapHeight;
    mapConfig.tileSize = config.tileSize;
    mapConfig.useChunks = config.enableChunks;
    mapConfig.defaultTile = config.defaultGroundTile;

    m_tileMap = TileMap(mapConfig);

    // Initialize tile atlas
    TileAtlasConfig atlasConfig;
    atlasConfig.textureBasePath = config.textureBasePath;

    if (!m_tileAtlas.Initialize(renderer.GetTextureManager(), atlasConfig)) {
        return false;
    }

    // Load textures
    m_tileAtlas.LoadTextures();

    // Initialize tile renderer
    TileRendererConfig renderConfig;
    if (!m_tileRenderer.Initialize(renderer, m_tileAtlas, renderConfig)) {
        return false;
    }

    // Initialize navigation graph
    m_navGraph = std::make_unique<Nova::Graph>();
    RebuildNavigationGraph();

    m_initialized = true;
    return true;
}

void World::Shutdown() {
    m_tileRenderer.Shutdown();
    m_tileMap.Clear();
    m_entities.clear();
    m_spawnPoints.clear();
    m_zones.clear();
    m_navGraph.reset();
    m_initialized = false;
}

void World::Update(float deltaTime) {
    if (!m_initialized) return;

    m_totalTime += deltaTime;

    // Update tile renderer animations
    m_tileRenderer.Update(deltaTime);

    // Update entities
    UpdateEntities(deltaTime);

    // Update spawns
    UpdateSpawns(deltaTime);

    // Rebuild nav graph if dirty
    if (m_navGraphDirty) {
        RebuildNavigationGraph();
        m_navGraphDirty = false;
    }
}

void World::Render(const Nova::Camera& camera) {
    if (!m_initialized || !m_renderer) return;

    // Render tile map
    m_tileRenderer.Render(m_tileMap, camera);

    // Entity rendering would be handled separately by game code
}

// ========== Entity Management ==========

uint32_t World::AddEntity(std::shared_ptr<Entity> entity) {
    uint32_t id = m_nextEntityId++;
    m_entities.push_back(entity);
    return id;
}

void World::RemoveEntity(uint32_t entityId) {
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [](const std::shared_ptr<Entity>& e) { return e == nullptr; }),
        m_entities.end()
    );
}

std::shared_ptr<Entity> World::GetEntity(uint32_t entityId) {
    // Simple linear search - for production, use a map
    if (entityId < m_entities.size()) {
        return m_entities[entityId];
    }
    return nullptr;
}

std::vector<std::shared_ptr<Entity>> World::GetEntitiesInRadius(
    const glm::vec3& center, float radius) const {
    std::vector<std::shared_ptr<Entity>> result;
    float radiusSq = radius * radius;

    for (const auto& entity : m_entities) {
        if (!entity) continue;
        // Entities would need a GetPosition() method
        // glm::vec3 pos = entity->GetPosition();
        // float distSq = glm::dot(pos - center, pos - center);
        // if (distSq <= radiusSq) {
        //     result.push_back(entity);
        // }
    }

    return result;
}

std::vector<std::shared_ptr<Entity>> World::GetEntitiesInZone(const Zone& zone) const {
    std::vector<std::shared_ptr<Entity>> result;

    for (const auto& entity : m_entities) {
        if (!entity) continue;
        // Entities would need a GetPosition() method
        // if (zone.Contains(entity->GetPosition())) {
        //     result.push_back(entity);
        // }
    }

    return result;
}

// ========== Spawn Points ==========

void World::AddSpawnPoint(const SpawnPoint& spawnPoint) {
    m_spawnPoints.push_back(spawnPoint);
}

std::vector<SpawnPoint*> World::GetSpawnPoints(const std::string& tag) {
    std::vector<SpawnPoint*> result;

    for (auto& sp : m_spawnPoints) {
        if (tag.empty() || sp.tag == tag) {
            result.push_back(&sp);
        }
    }

    return result;
}

SpawnPoint* World::GetRandomSpawnPoint(const std::string& tag) {
    auto points = GetSpawnPoints(tag);
    if (points.empty()) return nullptr;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, points.size() - 1);

    return points[dist(gen)];
}

void World::ClearSpawnPoints() {
    m_spawnPoints.clear();
}

// ========== Zones ==========

void World::AddZone(const Zone& zone) {
    m_zones.push_back(zone);
}

Zone* World::GetZoneAt(const glm::vec3& position) {
    for (auto& zone : m_zones) {
        if (zone.active && zone.Contains(position)) {
            return &zone;
        }
    }
    return nullptr;
}

std::vector<Zone*> World::GetZones(ZoneType type) {
    std::vector<Zone*> result;

    for (auto& zone : m_zones) {
        if (type == ZoneType::None || zone.type == type) {
            result.push_back(&zone);
        }
    }

    return result;
}

bool World::IsInSafeZone(const glm::vec3& position) const {
    for (const auto& zone : m_zones) {
        if (zone.active && zone.type == ZoneType::SafeZone && zone.Contains(position)) {
            return true;
        }
    }
    return false;
}

float World::GetDangerLevel(const glm::vec3& position) const {
    float danger = 0.0f;

    for (const auto& zone : m_zones) {
        if (zone.active && zone.Contains(position)) {
            if (zone.type == ZoneType::SafeZone) {
                return 0.0f;  // Safe zones override danger
            }
            danger = std::max(danger, zone.dangerLevel);
        }
    }

    return danger;
}

void World::ClearZones() {
    m_zones.clear();
}

// ========== Collision & Physics ==========

bool World::IsWalkable(const glm::vec3& position) const {
    return m_tileMap.IsWalkableWorld(position.x, position.z);
}

CollisionResult World::CheckCollision(const glm::vec3& start, const glm::vec3& end) const {
    CollisionResult result;

    // Simple line-based collision check through tiles
    glm::vec3 dir = end - start;
    float length = glm::length(dir);
    if (length < 0.001f) return result;

    dir /= length;
    float step = m_config.tileSize * 0.5f;

    for (float t = 0.0f; t <= length; t += step) {
        glm::vec3 point = start + dir * t;
        glm::ivec2 tileCoord = m_tileMap.WorldToTile(point.x, point.z);
        const Tile* tile = m_tileMap.GetTile(tileCoord.x, tileCoord.y);

        if (tile && tile->BlocksMovement()) {
            result.hit = true;
            result.point = point;
            result.distance = t;
            result.tileX = tileCoord.x;
            result.tileY = tileCoord.y;
            result.tile = tile;

            // Calculate normal (simplified - points away from tile center)
            glm::vec3 tileCenter = m_tileMap.TileToWorld(tileCoord.x, tileCoord.y);
            result.normal = glm::normalize(point - tileCenter);
            result.normal.y = 0.0f;
            if (glm::length(result.normal) > 0.01f) {
                result.normal = glm::normalize(result.normal);
            } else {
                result.normal = glm::vec3(0, 0, 1);
            }

            return result;
        }
    }

    return result;
}

CollisionResult World::CheckSphereCollision(const glm::vec3& center, float radius) const {
    CollisionResult result;

    // Check surrounding tiles
    glm::ivec2 tileCoord = m_tileMap.WorldToTile(center.x, center.z);
    int checkRadius = static_cast<int>(std::ceil(radius / m_config.tileSize)) + 1;

    for (int dy = -checkRadius; dy <= checkRadius; ++dy) {
        for (int dx = -checkRadius; dx <= checkRadius; ++dx) {
            int tx = tileCoord.x + dx;
            int ty = tileCoord.y + dy;

            const Tile* tile = m_tileMap.GetTile(tx, ty);
            if (!tile || !tile->BlocksMovement()) continue;

            // Check sphere-AABB collision
            glm::vec3 tileMin = m_tileMap.TileToWorldCorner(tx, ty);
            glm::vec3 tileMax = tileMin + glm::vec3(m_config.tileSize, tile->wallHeight, m_config.tileSize);

            // Find closest point on AABB
            glm::vec3 closest;
            closest.x = std::max(tileMin.x, std::min(center.x, tileMax.x));
            closest.y = std::max(tileMin.y, std::min(center.y, tileMax.y));
            closest.z = std::max(tileMin.z, std::min(center.z, tileMax.z));

            float distSq = glm::dot(closest - center, closest - center);
            if (distSq <= radius * radius) {
                result.hit = true;
                result.point = closest;
                result.distance = std::sqrt(distSq);
                result.tileX = tx;
                result.tileY = ty;
                result.tile = tile;

                // Normal points from collision point to sphere center
                result.normal = center - closest;
                if (glm::length(result.normal) > 0.001f) {
                    result.normal = glm::normalize(result.normal);
                } else {
                    result.normal = glm::vec3(0, 1, 0);
                }

                return result;  // Return first collision
            }
        }
    }

    return result;
}

glm::vec3 World::ResolveCollision(const glm::vec3& position, const glm::vec3& velocity,
                                   float radius) const {
    glm::vec3 newPos = position + velocity;
    glm::vec3 resolvedVel = velocity;

    // Check collision at new position
    CollisionResult collision = CheckSphereCollision(newPos, radius);

    if (collision.hit) {
        // Slide along the wall
        float penetration = radius - collision.distance;
        if (penetration > 0.0f) {
            // Push out of collision
            newPos += collision.normal * penetration;

            // Remove velocity component into the wall
            float velIntoWall = glm::dot(velocity, -collision.normal);
            if (velIntoWall > 0.0f) {
                resolvedVel += collision.normal * velIntoWall;
            }
        }
    }

    return resolvedVel;
}

CollisionResult World::Raycast(const glm::vec3& origin, const glm::vec3& direction,
                               float maxDistance) const {
    return CheckCollision(origin, origin + direction * maxDistance);
}

bool World::HasLineOfSight(const glm::vec3& from, const glm::vec3& to) const {
    CollisionResult result = CheckCollision(from, to);
    return !result.hit;
}

// ========== Pathfinding ==========

void World::RebuildNavigationGraph() {
    if (!m_navGraph) {
        m_navGraph = std::make_unique<Nova::Graph>();
    }

    m_tileMap.BuildNavigationGraph(*m_navGraph, true);
}

std::vector<glm::vec3> World::FindPath(const glm::vec3& from, const glm::vec3& to) const {
    std::vector<glm::vec3> result;

    if (!m_navGraph) return result;

    // Convert world positions to node IDs
    glm::ivec2 fromTile = m_tileMap.WorldToTile(from.x, from.z);
    glm::ivec2 toTile = m_tileMap.WorldToTile(to.x, to.z);

    int startNode = m_navGraph->GetNearestWalkableNode(m_tileMap.TileToWorld(fromTile.x, fromTile.y));
    int endNode = m_navGraph->GetNearestWalkableNode(m_tileMap.TileToWorld(toTile.x, toTile.y));

    if (startNode < 0 || endNode < 0) return result;

    // Find path
    Nova::PathResult pathResult = Nova::Pathfinder::AStar(*m_navGraph, startNode, endNode);

    if (pathResult.found) {
        result = pathResult.positions;
    }

    return result;
}

// ========== Serialization ==========

std::string World::SaveToJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": 1,\n";

    // Save tile map
    json << "  \"tileMap\": " << m_tileMap.SaveToJson() << ",\n";

    // Save spawn points
    json << "  \"spawnPoints\": [\n";
    bool firstSpawn = true;
    for (const auto& sp : m_spawnPoints) {
        if (!firstSpawn) json << ",\n";
        firstSpawn = false;
        json << "    {";
        json << "\"x\":" << sp.position.x << ",";
        json << "\"y\":" << sp.position.y << ",";
        json << "\"z\":" << sp.position.z << ",";
        json << "\"r\":" << sp.radius << ",";
        json << "\"tag\":\"" << sp.tag << "\",";
        json << "\"enabled\":" << (sp.enabled ? "true" : "false") << ",";
        json << "\"maxSpawns\":" << sp.maxSpawns << ",";
        json << "\"respawnTime\":" << sp.respawnTime;
        json << "}";
    }
    json << "\n  ],\n";

    // Save zones
    json << "  \"zones\": [\n";
    bool firstZone = true;
    for (const auto& zone : m_zones) {
        if (!firstZone) json << ",\n";
        firstZone = false;
        json << "    {";
        json << "\"name\":\"" << zone.name << "\",";
        json << "\"type\":" << static_cast<int>(zone.type) << ",";
        json << "\"minX\":" << zone.min.x << ",";
        json << "\"minY\":" << zone.min.y << ",";
        json << "\"minZ\":" << zone.min.z << ",";
        json << "\"maxX\":" << zone.max.x << ",";
        json << "\"maxY\":" << zone.max.y << ",";
        json << "\"maxZ\":" << zone.max.z << ",";
        json << "\"active\":" << (zone.active ? "true" : "false") << ",";
        json << "\"danger\":" << zone.dangerLevel << ",";
        json << "\"loot\":" << zone.lootMultiplier;
        json << "}";
    }
    json << "\n  ]\n";

    json << "}\n";
    return json.str();
}

bool World::LoadFromJson(const std::string& json) {
    // Simplified parsing - in production, use a proper JSON library
    // This is a basic implementation that matches the SaveToJson format

    // For now, just load the tile map
    size_t mapStart = json.find("\"tileMap\":");
    if (mapStart != std::string::npos) {
        mapStart = json.find("{", mapStart);
        if (mapStart != std::string::npos) {
            int depth = 1;
            size_t mapEnd = mapStart + 1;
            while (mapEnd < json.size() && depth > 0) {
                if (json[mapEnd] == '{') ++depth;
                else if (json[mapEnd] == '}') --depth;
                ++mapEnd;
            }

            std::string mapJson = json.substr(mapStart, mapEnd - mapStart);
            if (!m_tileMap.LoadFromJson(mapJson)) {
                return false;
            }
        }
    }

    m_navGraphDirty = true;
    return true;
}

bool World::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << SaveToJson();
    return file.good();
}

bool World::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadFromJson(buffer.str());
}

// ========== Utility ==========

glm::vec3 World::ClampToWorld(const glm::vec3& position) const {
    glm::vec2 worldMin = GetWorldMin();
    glm::vec2 worldMax = GetWorldMax();

    return glm::vec3(
        std::max(worldMin.x, std::min(position.x, worldMax.x)),
        position.y,
        std::max(worldMin.y, std::min(position.z, worldMax.y))
    );
}

glm::vec3 World::GetRandomWalkablePosition() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> xDist(0, m_tileMap.GetWidth() - 1);
    std::uniform_int_distribution<int> yDist(0, m_tileMap.GetHeight() - 1);

    // Try to find a walkable position (limited attempts)
    for (int i = 0; i < 100; ++i) {
        int x = xDist(gen);
        int y = yDist(gen);

        if (m_tileMap.IsWalkable(x, y)) {
            return m_tileMap.TileToWorld(x, y);
        }
    }

    // Fallback: return center of map
    return m_tileMap.TileToWorld(m_tileMap.GetWidth() / 2, m_tileMap.GetHeight() / 2);
}

void World::UpdateEntities(float deltaTime) {
    for (auto& entity : m_entities) {
        if (!entity) continue;

        if (m_entityUpdateCallback) {
            m_entityUpdateCallback(*entity, deltaTime);
        }
    }
}

void World::UpdateSpawns(float deltaTime) {
    for (auto& sp : m_spawnPoints) {
        if (!sp.enabled) continue;

        sp.lastSpawnTime += deltaTime;

        // Check if it's time to spawn
        if (sp.respawnTime > 0.0f && sp.lastSpawnTime >= sp.respawnTime) {
            sp.lastSpawnTime = 0.0f;
            // Actual spawning would be handled by game code via callback
        }
    }
}

bool World::CheckTileCollision(const glm::vec3& position, float radius) const {
    CollisionResult result = CheckSphereCollision(position, radius);
    return result.hit;
}

} // namespace Vehement
