#include "EntityPlacer.hpp"
#include "../entities/EntityManager.hpp"
#include "../world/TileMap.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

EntityPlacer::EntityPlacer() {
    ResetSpawnParams();
}

// =============================================================================
// Entity Type Selection
// =============================================================================

std::vector<EntityType> EntityPlacer::GetAvailableTypes() const {
    return {
        EntityType::NPC,
        EntityType::Zombie,
        EntityType::Pickup
    };
}

void EntityPlacer::SetSelectedType(EntityType type) {
    m_selectedType = type;
    m_spawnParams.type = type;

    // Update defaults based on type
    switch (type) {
        case EntityType::Zombie:
            m_spawnParams.isHostile = true;
            m_spawnParams.respawns = true;
            break;
        case EntityType::NPC:
            m_spawnParams.isHostile = false;
            m_spawnParams.respawns = false;
            break;
        case EntityType::Pickup:
            m_spawnParams.isHostile = false;
            m_spawnParams.respawns = true;
            break;
        default:
            break;
    }
}

std::vector<std::string> EntityPlacer::GetTemplates() const {
    std::vector<std::string> templates;

    switch (m_selectedType) {
        case EntityType::NPC:
            templates = {"Merchant", "Guard", "Villager", "Quest Giver", "Blacksmith"};
            break;
        case EntityType::Zombie:
            templates = {"Walker", "Runner", "Brute", "Spitter", "Boss"};
            break;
        case EntityType::Pickup:
            templates = {"Health Pack", "Ammo Crate", "Weapon Drop", "Resource Node"};
            break;
        default:
            break;
    }

    return templates;
}

void EntityPlacer::SetSelectedTemplate(const std::string& templateName) {
    m_selectedTemplate = templateName;

    // Apply template defaults
    if (m_selectedType == EntityType::Zombie) {
        if (templateName == "Walker") {
            m_spawnParams.health = 50.0f;
            m_spawnParams.level = 1;
        } else if (templateName == "Runner") {
            m_spawnParams.health = 30.0f;
            m_spawnParams.level = 2;
        } else if (templateName == "Brute") {
            m_spawnParams.health = 200.0f;
            m_spawnParams.level = 5;
        } else if (templateName == "Boss") {
            m_spawnParams.health = 1000.0f;
            m_spawnParams.level = 10;
        }
    }
}

void EntityPlacer::ResetSpawnParams() {
    m_spawnParams = EntitySpawnParams{};
    m_spawnParams.type = m_selectedType;
}

// =============================================================================
// Preview Position
// =============================================================================

void EntityPlacer::SetPreviewPosition(const glm::vec3& position) {
    m_previewPosition = position;
}

// =============================================================================
// Entity Placement
// =============================================================================

bool EntityPlacer::ValidatePlacement(const TileMap& map) const {
    int tileX = static_cast<int>(std::floor(m_previewPosition.x));
    int tileY = static_cast<int>(std::floor(m_previewPosition.z));

    if (!map.IsValidPosition(tileX, tileY)) {
        return false;
    }

    const Tile& tile = map.GetTile(tileX, tileY);

    // Can't place on walls
    if (tile.isWall || !tile.isWalkable) {
        return false;
    }

    return true;
}

Entity* EntityPlacer::PlaceEntity(EntityManager& entityManager) {
    auto entity = std::make_unique<Entity>(m_spawnParams.type);

    entity->SetPosition(m_previewPosition);
    entity->SetRotation(m_previewRotation);
    entity->SetHealth(m_spawnParams.health);
    entity->SetMaxHealth(m_spawnParams.health);

    Entity* ptr = entity.get();

    // Add to entity manager
    entityManager.AddEntity(std::move(entity));

    if (m_onEntityPlaced) {
        m_onEntityPlaced(*ptr);
    }

    return ptr;
}

std::unique_ptr<Entity> EntityPlacer::CreatePreviewEntity() const {
    auto entity = std::make_unique<Entity>(m_spawnParams.type);
    entity->SetPosition(m_previewPosition);
    entity->SetRotation(m_previewRotation);
    return entity;
}

// =============================================================================
// Patrol Path Editor
// =============================================================================

void EntityPlacer::StartNewPatrolPath(const std::string& name) {
    m_currentPatrolPath = PatrolPath{};
    m_currentPatrolPath.name = name;
    m_editingPatrolPath = true;
}

void EntityPlacer::AddPatrolWaypoint(const glm::vec3& position, float waitTime) {
    PatrolWaypoint waypoint;
    waypoint.position = position;
    waypoint.waitTime = waitTime;
    m_currentPatrolPath.waypoints.push_back(waypoint);
}

void EntityPlacer::RemoveLastWaypoint() {
    if (!m_currentPatrolPath.waypoints.empty()) {
        m_currentPatrolPath.waypoints.pop_back();
    }
}

void EntityPlacer::ClearPatrolPath() {
    m_currentPatrolPath.waypoints.clear();
}

void EntityPlacer::FinishPatrolPath() {
    if (!m_currentPatrolPath.name.empty() && !m_currentPatrolPath.waypoints.empty()) {
        // Check for existing path with same name
        auto it = std::find_if(m_patrolPaths.begin(), m_patrolPaths.end(),
            [this](const PatrolPath& p) { return p.name == m_currentPatrolPath.name; });

        if (it != m_patrolPaths.end()) {
            *it = m_currentPatrolPath;
        } else {
            m_patrolPaths.push_back(m_currentPatrolPath);
        }

        if (m_onPatrolPathCreated) {
            m_onPatrolPathCreated(m_currentPatrolPath);
        }
    }

    m_editingPatrolPath = false;
    m_currentPatrolPath = PatrolPath{};
}

bool EntityPlacer::DeletePatrolPath(const std::string& name) {
    auto it = std::find_if(m_patrolPaths.begin(), m_patrolPaths.end(),
        [&name](const PatrolPath& p) { return p.name == name; });

    if (it != m_patrolPaths.end()) {
        m_patrolPaths.erase(it);
        return true;
    }

    return false;
}

void EntityPlacer::AssignPatrolPath(Entity& entity, const std::string& pathName) {
    // Find path
    auto it = std::find_if(m_patrolPaths.begin(), m_patrolPaths.end(),
        [&pathName](const PatrolPath& p) { return p.name == pathName; });

    if (it != m_patrolPaths.end()) {
        // In a real implementation, this would set up AI patrol behavior
        entity.SetName(entity.GetName() + " [Patrol: " + pathName + "]");
    }
}

// =============================================================================
// Spawn Zone Editor
// =============================================================================

void EntityPlacer::StartNewSpawnZone(const std::string& name) {
    m_currentSpawnZone = SpawnZone{};
    m_currentSpawnZone.name = name;
    m_currentSpawnZone.spawnParams = m_spawnParams;
    m_editingSpawnZone = true;
}

void EntityPlacer::SetSpawnZoneCenter(const glm::vec3& center) {
    m_currentSpawnZone.center = center;
}

void EntityPlacer::SetSpawnZoneSize(const glm::vec3& size) {
    m_currentSpawnZone.size = size;
}

void EntityPlacer::FinishSpawnZone() {
    if (!m_currentSpawnZone.name.empty()) {
        // Check for existing zone with same name
        auto it = std::find_if(m_spawnZones.begin(), m_spawnZones.end(),
            [this](const SpawnZone& z) { return z.name == m_currentSpawnZone.name; });

        if (it != m_spawnZones.end()) {
            *it = m_currentSpawnZone;
        } else {
            m_spawnZones.push_back(m_currentSpawnZone);
        }

        if (m_onSpawnZoneCreated) {
            m_onSpawnZoneCreated(m_currentSpawnZone);
        }
    }

    m_editingSpawnZone = false;
    m_currentSpawnZone = SpawnZone{};
}

bool EntityPlacer::DeleteSpawnZone(const std::string& name) {
    auto it = std::find_if(m_spawnZones.begin(), m_spawnZones.end(),
        [&name](const SpawnZone& z) { return z.name == name; });

    if (it != m_spawnZones.end()) {
        m_spawnZones.erase(it);
        return true;
    }

    return false;
}

void EntityPlacer::SetSpawnZoneEnabled(const std::string& name, bool enabled) {
    auto it = std::find_if(m_spawnZones.begin(), m_spawnZones.end(),
        [&name](const SpawnZone& z) { return z.name == name; });

    if (it != m_spawnZones.end()) {
        it->enabled = enabled;
    }
}

// =============================================================================
// Serialization
// =============================================================================

std::string EntityPlacer::SavePatrolPathsToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"patrolPaths\": [\n";

    for (size_t i = 0; i < m_patrolPaths.size(); ++i) {
        const auto& path = m_patrolPaths[i];
        if (i > 0) ss << ",\n";

        ss << "    {\n";
        ss << "      \"name\": \"" << path.name << "\",\n";
        ss << "      \"loop\": " << (path.loop ? "true" : "false") << ",\n";
        ss << "      \"pingPong\": " << (path.pingPong ? "true" : "false") << ",\n";
        ss << "      \"speed\": " << path.speed << ",\n";
        ss << "      \"waypoints\": [\n";

        for (size_t j = 0; j < path.waypoints.size(); ++j) {
            const auto& wp = path.waypoints[j];
            if (j > 0) ss << ",\n";
            ss << "        {\"x\": " << wp.position.x
               << ", \"y\": " << wp.position.y
               << ", \"z\": " << wp.position.z
               << ", \"wait\": " << wp.waitTime << "}";
        }

        ss << "\n      ]\n    }";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool EntityPlacer::LoadPatrolPathsFromJson(const std::string& /*json*/) {
    // Simplified - would parse JSON in real implementation
    return true;
}

std::string EntityPlacer::SaveSpawnZonesToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"spawnZones\": [\n";

    for (size_t i = 0; i < m_spawnZones.size(); ++i) {
        const auto& zone = m_spawnZones[i];
        if (i > 0) ss << ",\n";

        ss << "    {\n";
        ss << "      \"name\": \"" << zone.name << "\",\n";
        ss << "      \"center\": {\"x\": " << zone.center.x
           << ", \"y\": " << zone.center.y
           << ", \"z\": " << zone.center.z << "},\n";
        ss << "      \"size\": {\"x\": " << zone.size.x
           << ", \"y\": " << zone.size.y
           << ", \"z\": " << zone.size.z << "},\n";
        ss << "      \"maxEntities\": " << zone.maxEntities << ",\n";
        ss << "      \"spawnInterval\": " << zone.spawnInterval << ",\n";
        ss << "      \"enabled\": " << (zone.enabled ? "true" : "false") << "\n";
        ss << "    }";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool EntityPlacer::LoadSpawnZonesFromJson(const std::string& /*json*/) {
    // Simplified - would parse JSON in real implementation
    return true;
}

} // namespace Vehement
