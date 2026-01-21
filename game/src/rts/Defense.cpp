#include "Defense.hpp"
#include "../world/World.hpp"
#include "../entities/Zombie.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// Wall Segment Implementation
// ============================================================================

bool WallSegment::BlocksPoint(const glm::vec2& point, float tolerance) const {
    // Check if point is on the wall segment line with tolerance
    if (isHorizontal) {
        float minX = std::min(start.x, end.x) - tolerance;
        float maxX = std::max(start.x, end.x) + tolerance;
        return point.x >= minX && point.x <= maxX &&
               std::abs(point.y - start.y) <= tolerance;
    } else {
        float minY = std::min(start.y, end.y) - tolerance;
        float maxY = std::max(start.y, end.y) + tolerance;
        return point.y >= minY && point.y <= maxY &&
               std::abs(point.x - start.x) <= tolerance;
    }
}

// ============================================================================
// Defense Manager Implementation
// ============================================================================

DefenseManager::DefenseManager() = default;

void DefenseManager::Initialize(World* world, Construction* construction, TileMap* tileMap) {
    m_world = world;
    m_construction = construction;
    m_tileMap = tileMap;
    m_wallSegmentsDirty = true;
}

void DefenseManager::Update(float deltaTime) {
    if (!m_construction) return;

    // Rebuild wall segments if needed
    if (m_wallSegmentsDirty) {
        RebuildWallSegments();
        m_wallSegmentsDirty = false;
    }

    // Update all defensive buildings
    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->IsOperational() &&
            IsDefensiveBuilding(building->GetBuildingType())) {
            UpdateDefensiveBuilding(building.get(), deltaTime);
        }
    }

    // Update gate auto-close
    for (Building* gate : m_autoCloseGates) {
        if (gate && gate->IsOperational()) {
            UpdateGateAutoClose(gate);
        }
    }

    // Update projectiles
    for (auto& projectile : m_projectiles) {
        projectile.Update(deltaTime);
    }

    // Remove inactive projectiles
    m_projectiles.erase(
        std::remove_if(m_projectiles.begin(), m_projectiles.end(),
            [](const DefenseProjectile& p) { return !p.active || p.HasReachedTarget(); }),
        m_projectiles.end());
}

void DefenseManager::Render(Nova::Renderer& renderer) {
    // Render targeting lines, projectiles, etc.
    // This would integrate with the Nova renderer

    // Draw attack range circles for selected buildings (debug)
    // Draw targeting lines to current targets

    // Render active projectiles
    for (const auto& projectile : m_projectiles) {
        if (!projectile.active) continue;

        // Projectile rendering would use the renderer to draw sprites
        // at the projectile position using projectile.texturePath
        // For now, the projectile data is available for the rendering
        // system to consume via iteration or a getter method.
        // Example integration:
        // renderer.DrawSprite(projectile.texturePath, projectile.position, size);
    }
}

void DefenseManager::UpdateDefensiveBuilding(Building* building, float deltaTime) {
    if (!building) return;

    DefenseType defenseType = GetDefenseType(building->GetBuildingType());

    // Update attack cooldown
    auto& state = m_attackStates[building];
    if (state.cooldownTimer > 0.0f) {
        state.cooldownTimer -= deltaTime;
    }

    // Only ranged and area defenses attack
    if (defenseType != DefenseType::Ranged && defenseType != DefenseType::Area) {
        return;
    }

    // Check if can attack
    DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());
    if (state.cooldownTimer > 0.0f || stats.damage <= 0.0f) {
        return;
    }

    // Find targets
    if (defenseType == DefenseType::Area) {
        // Area attack: hit multiple targets
        std::vector<TargetInfo> targets = GetEnemiesInRange(building);

        int targetsHit = 0;
        for (const auto& target : targets) {
            if (targetsHit >= stats.maxTargets) break;
            if (target.entity) {
                PerformAttack(building, target.entity);
                targetsHit++;
            }
        }

        if (targetsHit > 0) {
            state.cooldownTimer = stats.attackCooldown;
        }
    } else {
        // Single target attack
        Entity* target = FindBestTarget(building);
        if (target) {
            PerformAttack(building, target);
            state.cooldownTimer = stats.attackCooldown;
            state.currentTarget = target;
        } else {
            state.currentTarget = nullptr;
        }
    }
}

void DefenseManager::PerformAttack(Building* building, Entity* target) {
    if (!building || !target) return;

    DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());

    // Calculate damage with guard bonus
    float damage = stats.damage + GetGuardBonusDamage(building);

    // Apply damage
    float actualDamage = target->TakeDamage(damage, building->GetId());
    m_totalDamageDealt += actualDamage;

    // Callback
    if (m_onAttack) {
        m_onAttack(building, target, actualDamage);
    }

    // Check for kill
    if (!target->IsAlive()) {
        m_totalKills++;
        if (m_onKill) {
            m_onKill(building, target);
        }
    }

    // Create projectile for visual effect (damage already applied)
    DefenseProjectile projectile;
    projectile.position = building->GetPosition();
    projectile.targetPosition = target->GetPosition();
    projectile.target = target;
    projectile.source = building;
    projectile.damage = 0.0f;  // Damage already applied above
    projectile.splashRadius = stats.splashRadius;
    projectile.lifetime = 2.0f;
    projectile.active = true;
    projectile.texturePath = stats.projectileTexture;

    // Calculate velocity towards target
    glm::vec3 direction = projectile.targetPosition - projectile.position;
    float distance = glm::length(direction);
    if (distance > 0.01f) {
        direction = glm::normalize(direction);
        projectile.velocity = direction * stats.projectileSpeed;
    }

    m_projectiles.push_back(projectile);
}

Entity* DefenseManager::FindBestTarget(Building* building) const {
    if (!building) return nullptr;

    std::vector<TargetInfo> targets = GetEnemiesInRange(building);
    if (targets.empty()) return nullptr;

    // Sort by threat (highest first)
    std::sort(targets.begin(), targets.end());

    return targets[0].entity;
}

std::vector<TargetInfo> DefenseManager::GetEnemiesInRange(Building* building) const {
    std::vector<TargetInfo> result;
    if (!building || !m_world) return result;

    DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());
    glm::vec3 buildingPos = building->GetPosition();

    // Get all entities from world
    for (const auto& entity : m_world->GetEntities()) {
        if (!entity || !entity->IsAlive()) continue;

        // Check if enemy (zombie)
        if (entity->GetType() != EntityType::Zombie) continue;

        float distance = building->DistanceTo(*entity);
        if (distance <= stats.attackRange) {
            TargetInfo info;
            info.entity = entity.get();
            info.distance = distance;
            info.health = entity->GetHealth();
            info.maxHealth = entity->GetMaxHealth();
            info.isAttackingBuilding = false;  // Would check zombie target
            info.isAttackingWorker = false;
            info.threat = CalculateThreat(info);

            result.push_back(info);
        }
    }

    return result;
}

std::vector<Entity*> DefenseManager::GetEnemiesInRange(const glm::vec3& position, float range) const {
    std::vector<Entity*> result;
    if (!m_world) return result;

    for (const auto& entity : m_world->GetEntities()) {
        if (!entity || !entity->IsAlive()) continue;
        if (entity->GetType() != EntityType::Zombie) continue;

        float distance = glm::length(entity->GetPosition() - position);
        if (distance <= range) {
            result.push_back(entity.get());
        }
    }

    return result;
}

float DefenseManager::CalculateThreat(const TargetInfo& target) const {
    float threat = 0.0f;

    switch (m_targetPriority) {
        case TargetPriority::Nearest:
            threat = 1000.0f - target.distance;  // Closer = higher threat
            break;

        case TargetPriority::Weakest:
            threat = 1000.0f - target.health;  // Lower HP = higher threat
            break;

        case TargetPriority::Strongest:
            threat = target.health;  // Higher HP = higher threat
            break;

        case TargetPriority::AttackingBuilding:
            threat = target.isAttackingBuilding ? 1000.0f : 0.0f;
            threat += 500.0f - target.distance;
            break;

        case TargetPriority::AttackingWorker:
            threat = target.isAttackingWorker ? 1000.0f : 0.0f;
            threat += 500.0f - target.distance;
            break;
    }

    return threat;
}

// =========================================================================
// Gate Control
// =========================================================================

void DefenseManager::OpenGate(Building* gate) {
    if (!gate || gate->GetBuildingType() != BuildingType::Gate) return;
    gate->SetGateOpen(true);

    // Update tile walkability
    if (m_tileMap) {
        for (const auto& tilePos : gate->GetOccupiedTiles()) {
            Tile* tile = m_tileMap->GetTile(tilePos.x, tilePos.y);
            if (tile) {
                tile->isWalkable = true;
            }
        }
        m_tileMap->MarkDirty(gate->GetGridPosition().x, gate->GetGridPosition().y,
                             gate->GetSize().x, gate->GetSize().y);
    }

    // Rebuild nav graph
    if (m_world) {
        m_world->RebuildNavigationGraph();
    }
}

void DefenseManager::CloseGate(Building* gate) {
    if (!gate || gate->GetBuildingType() != BuildingType::Gate) return;
    gate->SetGateOpen(false);

    // Update tile walkability
    if (m_tileMap) {
        for (const auto& tilePos : gate->GetOccupiedTiles()) {
            Tile* tile = m_tileMap->GetTile(tilePos.x, tilePos.y);
            if (tile) {
                tile->isWalkable = false;
            }
        }
        m_tileMap->MarkDirty(gate->GetGridPosition().x, gate->GetGridPosition().y,
                             gate->GetSize().x, gate->GetSize().y);
    }

    // Rebuild nav graph
    if (m_world) {
        m_world->RebuildNavigationGraph();
    }
}

void DefenseManager::ToggleGate(Building* gate) {
    if (!gate || gate->GetBuildingType() != BuildingType::Gate) return;

    if (gate->IsGateOpen()) {
        CloseGate(gate);
    } else {
        OpenGate(gate);
    }
}

void DefenseManager::OpenAllGates() {
    if (!m_construction) return;

    for (Building* gate : GetAllGates()) {
        OpenGate(gate);
    }
}

void DefenseManager::CloseAllGates() {
    if (!m_construction) return;

    for (Building* gate : GetAllGates()) {
        CloseGate(gate);
    }
}

void DefenseManager::SetGateAutoClose(Building* gate, bool autoClose) {
    if (!gate || gate->GetBuildingType() != BuildingType::Gate) return;

    if (autoClose) {
        m_autoCloseGates.insert(gate);
    } else {
        m_autoCloseGates.erase(gate);
    }
}

void DefenseManager::UpdateGateAutoClose(Building* gate) {
    if (!gate || gate->GetBuildingType() != BuildingType::Gate) return;

    // Check for enemies near gate
    std::vector<Entity*> nearbyEnemies = GetEnemiesInRange(gate->GetPosition(), 8.0f);

    if (!nearbyEnemies.empty() && gate->IsGateOpen()) {
        // Close gate when enemies approach
        CloseGate(gate);
    }
}

std::vector<Building*> DefenseManager::GetAllGates() const {
    std::vector<Building*> result;
    if (!m_construction) return result;

    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->GetBuildingType() == BuildingType::Gate) {
            result.push_back(building.get());
        }
    }

    return result;
}

// =========================================================================
// Wall Management
// =========================================================================

void DefenseManager::RebuildWallSegments() {
    m_wallSegments.clear();
    if (!m_construction || !m_tileMap) return;

    // Get all wall buildings
    std::vector<Building*> walls = m_construction->GetBuildingsByType(BuildingType::Wall);
    std::vector<Building*> gates = m_construction->GetBuildingsByType(BuildingType::Gate);

    // Create a grid of wall tiles
    int width = m_tileMap->GetWidth();
    int height = m_tileMap->GetHeight();
    std::vector<std::vector<bool>> wallGrid(height, std::vector<bool>(width, false));

    for (Building* wall : walls) {
        if (!wall) continue;
        for (const auto& tile : wall->GetOccupiedTiles()) {
            if (tile.x >= 0 && tile.x < width && tile.y >= 0 && tile.y < height) {
                wallGrid[tile.y][tile.x] = true;
            }
        }
    }

    // Find horizontal segments
    for (int y = 0; y < height; ++y) {
        int startX = -1;
        for (int x = 0; x <= width; ++x) {
            bool isWall = (x < width) && wallGrid[y][x];

            if (isWall && startX < 0) {
                startX = x;
            } else if (!isWall && startX >= 0) {
                // End of segment
                if (x - startX >= 2) {  // Only count segments of 2+ tiles
                    WallSegment segment;
                    segment.isHorizontal = true;
                    segment.start = glm::vec2(startX, y);
                    segment.end = glm::vec2(x, y);
                    segment.hasGate = false;

                    for (int sx = startX; sx < x; ++sx) {
                        segment.tiles.emplace_back(sx, y);
                    }

                    m_wallSegments.push_back(segment);
                }
                startX = -1;
            }
        }
    }

    // Find vertical segments
    for (int x = 0; x < width; ++x) {
        int startY = -1;
        for (int y = 0; y <= height; ++y) {
            bool isWall = (y < height) && wallGrid[y][x];

            if (isWall && startY < 0) {
                startY = y;
            } else if (!isWall && startY >= 0) {
                // End of segment
                if (y - startY >= 2) {  // Only count segments of 2+ tiles
                    WallSegment segment;
                    segment.isHorizontal = false;
                    segment.start = glm::vec2(x, startY);
                    segment.end = glm::vec2(x, y);
                    segment.hasGate = false;

                    for (int sy = startY; sy < y; ++sy) {
                        segment.tiles.emplace_back(x, sy);
                    }

                    m_wallSegments.push_back(segment);
                }
                startY = -1;
            }
        }
    }

    // Mark segments that have gates
    for (Building* gate : gates) {
        if (!gate) continue;
        glm::ivec2 gatePos = gate->GetGridPosition();

        for (auto& segment : m_wallSegments) {
            for (const auto& tile : segment.tiles) {
                if (std::abs(tile.x - gatePos.x) <= 1 && std::abs(tile.y - gatePos.y) <= 1) {
                    segment.hasGate = true;
                    break;
                }
            }
        }
    }
}

bool DefenseManager::IsBlockedByWall(const glm::vec3& position) const {
    if (!m_construction) return false;

    Building* building = m_construction->GetBuildingAtWorld(position);
    if (building) {
        BuildingType type = building->GetBuildingType();
        if (type == BuildingType::Wall) {
            return true;
        }
        if (type == BuildingType::Gate && !building->IsGateOpen()) {
            return true;
        }
    }

    return false;
}

bool DefenseManager::IsPathBlockedByWall(const glm::vec3& from, const glm::vec3& to) const {
    // Simple line check against wall segments
    glm::vec2 start(from.x, from.z);
    glm::vec2 end(to.x, to.z);

    for (const auto& segment : m_wallSegments) {
        // Skip if segment has an open gate
        if (segment.hasGate) {
            // Check if gate is actually open
            // For simplicity, assume gates block
        }

        // Line-line intersection test
        glm::vec2 p1 = segment.start;
        glm::vec2 p2 = segment.end;

        float d1 = (end.x - start.x) * (p1.y - start.y) - (end.y - start.y) * (p1.x - start.x);
        float d2 = (end.x - start.x) * (p2.y - start.y) - (end.y - start.y) * (p2.x - start.x);
        float d3 = (p2.x - p1.x) * (start.y - p1.y) - (p2.y - p1.y) * (start.x - p1.x);
        float d4 = (p2.x - p1.x) * (end.y - p1.y) - (p2.y - p1.y) * (end.x - p1.x);

        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
            ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
            return true;
        }
    }

    return false;
}

float DefenseManager::GetWallHealthAt(int x, int y) const {
    if (!m_construction) return 0.0f;

    Building* building = m_construction->GetBuildingAt(x, y);
    if (building && building->GetBuildingType() == BuildingType::Wall) {
        return building->GetHealth();
    }

    return 0.0f;
}

// =========================================================================
// Vision System
// =========================================================================

std::vector<glm::ivec2> DefenseManager::GetRevealedTiles() const {
    std::vector<glm::ivec2> revealed;
    if (!m_construction || !m_tileMap) return revealed;

    int width = m_tileMap->GetWidth();
    int height = m_tileMap->GetHeight();

    // Create a set for efficient lookup
    std::vector<std::vector<bool>> revealedGrid(height, std::vector<bool>(width, false));

    for (const auto& building : m_construction->GetBuildings()) {
        if (!building || !building->IsOperational()) continue;

        DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());
        if (stats.visionRange <= 0.0f) continue;

        glm::ivec2 center = building->GetGridPosition();
        glm::ivec2 size = building->GetSize();
        center.x += size.x / 2;
        center.y += size.y / 2;

        int range = static_cast<int>(std::ceil(stats.visionRange));

        for (int dy = -range; dy <= range; ++dy) {
            for (int dx = -range; dx <= range; ++dx) {
                int tx = center.x + dx;
                int ty = center.y + dy;

                if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                    if (dist <= stats.visionRange) {
                        revealedGrid[ty][tx] = true;
                    }
                }
            }
        }
    }

    // Convert grid to vector
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (revealedGrid[y][x]) {
                revealed.emplace_back(x, y);
            }
        }
    }

    return revealed;
}

bool DefenseManager::IsTileRevealed(int x, int y) const {
    if (!m_construction) return false;

    for (const auto& building : m_construction->GetBuildings()) {
        if (!building || !building->IsOperational()) continue;

        DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());
        if (stats.visionRange <= 0.0f) continue;

        glm::ivec2 center = building->GetGridPosition();
        glm::ivec2 size = building->GetSize();
        center.x += size.x / 2;
        center.y += size.y / 2;

        float dx = static_cast<float>(x - center.x);
        float dy = static_cast<float>(y - center.y);
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= stats.visionRange) {
            return true;
        }
    }

    return false;
}

float DefenseManager::GetVisionCoverage() const {
    if (!m_tileMap) return 0.0f;

    std::vector<glm::ivec2> revealed = GetRevealedTiles();
    int totalTiles = m_tileMap->GetWidth() * m_tileMap->GetHeight();

    return static_cast<float>(revealed.size()) / static_cast<float>(totalTiles) * 100.0f;
}

// =========================================================================
// Hero Revival
// =========================================================================

Building* DefenseManager::GetHeroRevivalPoint() const {
    if (!m_construction) return nullptr;

    for (const auto& building : m_construction->GetBuildings()) {
        if (building && building->GetBuildingType() == BuildingType::Fortress &&
            building->IsOperational()) {
            return building.get();
        }
    }

    return nullptr;
}

glm::vec3 DefenseManager::GetRevivalPosition() const {
    Building* fortress = GetHeroRevivalPoint();
    if (fortress) {
        return fortress->GetPosition();
    }

    // Fallback to command center
    if (m_construction) {
        Building* commandCenter = m_construction->GetCommandCenter();
        if (commandCenter) {
            return commandCenter->GetPosition();
        }
    }

    return glm::vec3(0.0f);
}

bool DefenseManager::CanReviveHero() const {
    return GetHeroRevivalPoint() != nullptr;
}

// =========================================================================
// Guard Assignment
// =========================================================================

bool DefenseManager::AssignGuard(Worker* worker, Building* defense) {
    if (!worker || !defense) return false;
    if (!IsDefensiveBuilding(defense->GetBuildingType())) return false;

    // Check worker capacity
    if (!defense->HasWorkerSpace()) return false;

    // Assign worker to building
    if (defense->AssignWorker(worker)) {
        m_guards[defense].push_back(worker);
        return true;
    }

    return false;
}

bool DefenseManager::RemoveGuard(Worker* worker, Building* defense) {
    if (!worker || !defense) return false;

    if (defense->RemoveWorker(worker)) {
        auto& guards = m_guards[defense];
        guards.erase(std::remove(guards.begin(), guards.end(), worker), guards.end());
        return true;
    }

    return false;
}

std::vector<Worker*> DefenseManager::GetGuards(Building* defense) const {
    auto it = m_guards.find(defense);
    if (it != m_guards.end()) {
        return it->second;
    }
    return {};
}

float DefenseManager::GetGuardBonusDamage(Building* defense) const {
    if (!defense) return 0.0f;

    auto it = m_guards.find(defense);
    if (it == m_guards.end()) return 0.0f;

    float bonus = 0.0f;
    for (const Worker* guard : it->second) {
        if (guard) {
            // Each guard adds 10% of base damage, scaled by skill
            bonus += 0.1f * guard->GetSkillLevel();
        }
    }

    DefenseStats stats = GetDefenseStats(defense->GetBuildingType(), defense->GetLevel());
    return stats.damage * bonus;
}

// =========================================================================
// Statistics
// =========================================================================

float DefenseManager::GetDefenseScore() const {
    float score = 0.0f;
    if (!m_construction) return score;

    for (const auto& building : m_construction->GetBuildings()) {
        if (!building || !IsDefensiveBuilding(building->GetBuildingType())) continue;

        DefenseStats stats = GetDefenseStats(building->GetBuildingType(), building->GetLevel());

        // Score based on various factors
        score += building->GetHealth() * 0.1f;
        score += stats.damage * 2.0f;
        score += stats.attackRange * 1.0f;
        score += stats.visionRange * 0.5f;
    }

    return score;
}

void DefenseManager::SetBuildingTargetPriority(Building* building, TargetPriority priority) {
    if (building) {
        m_buildingPriorities[building] = priority;
    }
}

} // namespace Vehement
