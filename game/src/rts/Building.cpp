#include "Building.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {

// Static member initialization
Building::BuildingId Building::s_nextBuildingId = 1;

// ============================================================================
// Building Implementation
// ============================================================================

Building::Building()
    : Entity(EntityType::None)  // Buildings use custom type handling
{
    m_buildingId = s_nextBuildingId++;
    SetCollidable(true);
    SetActive(true);
    InitializeForType(BuildingType::Shelter);
}

Building::Building(BuildingType type)
    : Entity(EntityType::None)
{
    m_buildingId = s_nextBuildingId++;
    m_buildingType = type;
    SetCollidable(true);
    SetActive(true);
    InitializeForType(type);
}

void Building::InitializeForType(BuildingType type) {
    m_buildingType = type;
    m_textures = BuildingTextures::GetForType(type);

    // Set building size-based collision radius
    glm::ivec2 size = GetBuildingSize(type);
    m_collisionRadius = std::max(size.x, size.y) * 0.5f;

    // Initialize based on building type
    switch (type) {
        // Housing buildings
        case BuildingType::Shelter:
            m_maxHealth = 100.0f;
            m_housingCapacity = 2;
            m_workerCapacity = 0;
            m_wallHeight = 2.0f;
            m_visionRange = 5.0f;
            break;

        case BuildingType::House:
            m_maxHealth = 200.0f;
            m_housingCapacity = 4;
            m_workerCapacity = 0;
            m_wallHeight = 3.0f;
            m_visionRange = 6.0f;
            break;

        case BuildingType::Barracks:
            m_maxHealth = 400.0f;
            m_housingCapacity = 8;
            m_workerCapacity = 0;
            m_wallHeight = 4.0f;
            m_visionRange = 8.0f;
            break;

        // Production buildings
        case BuildingType::Farm:
            m_maxHealth = 150.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 4;
            m_wallHeight = 1.5f;
            m_visionRange = 4.0f;
            break;

        case BuildingType::LumberMill:
            m_maxHealth = 200.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 3;
            m_wallHeight = 2.5f;
            m_visionRange = 5.0f;
            break;

        case BuildingType::Quarry:
            m_maxHealth = 300.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 5;
            m_wallHeight = 1.0f;  // Open pit
            m_visionRange = 6.0f;
            break;

        case BuildingType::Workshop:
            m_maxHealth = 250.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 4;
            m_wallHeight = 3.0f;
            m_visionRange = 5.0f;
            break;

        // Defense buildings
        case BuildingType::WatchTower:
            m_maxHealth = 300.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 2;
            m_wallHeight = 6.0f;
            m_attackDamage = 15.0f;
            m_attackRange = 12.0f;
            m_attackCooldown = 1.5f;
            m_visionRange = 15.0f;  // Great vision
            break;

        case BuildingType::Wall:
            m_maxHealth = 500.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 0;
            m_wallHeight = 3.0f;
            m_visionRange = 2.0f;
            break;

        case BuildingType::Gate:
            m_maxHealth = 400.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 0;
            m_wallHeight = 3.0f;
            m_gateOpen = false;
            m_visionRange = 3.0f;
            break;

        case BuildingType::Fortress:
            m_maxHealth = 1000.0f;
            m_housingCapacity = 4;
            m_workerCapacity = 6;
            m_wallHeight = 5.0f;
            m_attackDamage = 30.0f;
            m_attackRange = 15.0f;
            m_attackCooldown = 1.0f;
            m_visionRange = 20.0f;
            break;

        // Special buildings
        case BuildingType::TradingPost:
            m_maxHealth = 200.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 2;
            m_wallHeight = 2.5f;
            m_visionRange = 6.0f;
            break;

        case BuildingType::Hospital:
            m_maxHealth = 300.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 4;
            m_wallHeight = 3.0f;
            m_visionRange = 5.0f;
            break;

        case BuildingType::Warehouse:
            m_maxHealth = 400.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 2;
            m_wallHeight = 4.0f;
            m_visionRange = 4.0f;
            break;

        case BuildingType::CommandCenter:
            m_maxHealth = 800.0f;
            m_housingCapacity = 2;
            m_workerCapacity = 4;
            m_wallHeight = 5.0f;
            m_attackDamage = 10.0f;
            m_attackRange = 8.0f;
            m_attackCooldown = 2.0f;
            m_visionRange = 12.0f;
            break;

        default:
            m_maxHealth = 100.0f;
            m_housingCapacity = 0;
            m_workerCapacity = 0;
            m_wallHeight = 2.0f;
            m_visionRange = 4.0f;
            break;
    }

    m_health = m_maxHealth;
    SetName(GetBuildingTypeName(type));
}

void Building::Update(float deltaTime) {
    if (!m_active) return;

    switch (m_state) {
        case BuildingState::Blueprint:
            // Waiting for construction to start
            break;

        case BuildingState::UnderConstruction:
            UpdateConstruction(deltaTime);
            break;

        case BuildingState::Operational:
            UpdateProduction(deltaTime);
            if (CanAttack()) {
                UpdateDefense(deltaTime);
            }
            break;

        case BuildingState::Upgrading:
            UpdateConstruction(deltaTime);
            break;

        case BuildingState::Damaged:
            // Reduced efficiency, slower production
            UpdateProduction(deltaTime * 0.5f);
            if (CanAttack()) {
                UpdateDefense(deltaTime);
            }
            break;

        case BuildingState::Destroyed:
            // No updates when destroyed
            break;
    }

    // Update base entity
    Entity::Update(deltaTime);
}

void Building::UpdateConstruction(float deltaTime) {
    // Construction progress is added by workers via AddConstructionProgress
    // Here we just check for completion
    if (m_constructionProgress >= 100.0f) {
        BuildingState oldState = m_state;

        if (m_state == BuildingState::Upgrading) {
            m_level++;
            // Increase stats with level
            m_maxHealth *= 1.25f;
            m_health = m_maxHealth;
            if (m_attackDamage > 0) {
                m_attackDamage *= 1.2f;
                m_attackRange *= 1.1f;
            }
            m_housingCapacity = static_cast<int>(m_housingCapacity * 1.5f);
            if (m_workerCapacity > 0) {
                m_workerCapacity++;
            }
        }

        m_state = BuildingState::Operational;
        m_constructionProgress = 100.0f;

        if (m_onStateChange) {
            m_onStateChange(*this, oldState, m_state);
        }

        if (m_onConstructionComplete) {
            m_onConstructionComplete(*this);
        }
    }
}

void Building::UpdateProduction(float deltaTime) {
    // Production is handled by BuildingProduction system
    // This is just for building-internal updates
}

void Building::UpdateDefense(float deltaTime) {
    if (!CanAttack()) return;

    m_attackTimer -= deltaTime;

    // Attack logic would go here
    // Check for enemies in range
    // Fire at nearest enemy
    // This would integrate with the combat system
}

void Building::Render(Nova::Renderer& renderer) {
    if (!m_active) return;

    // Building rendering is handled by the BuildingRenderer system
    // which uses the tile grid and textures

    // For now, just call base render
    Entity::Render(renderer);
}

void Building::SetState(BuildingState state) noexcept {
    if (m_state != state) {
        BuildingState oldState = m_state;
        m_state = state;

        if (m_onStateChange) {
            m_onStateChange(*this, oldState, state);
        }
    }
}

void Building::AddConstructionProgress(float amount) {
    if (m_state != BuildingState::UnderConstruction &&
        m_state != BuildingState::Upgrading) {
        return;
    }

    m_constructionProgress = std::clamp(m_constructionProgress + amount, 0.0f, 100.0f);
}

void Building::SetConstructionProgress(float progress) noexcept {
    m_constructionProgress = std::clamp(progress, 0.0f, 100.0f);
}

void Building::SetGridPosition(int x, int y) noexcept {
    m_gridPosition = glm::ivec2(x, y);

    // Also update world position (center of building)
    glm::ivec2 size = GetSize();
    float worldX = x + size.x * 0.5f;
    float worldZ = y + size.y * 0.5f;
    SetPosition(glm::vec3(worldX, 0.0f, worldZ));
}

std::vector<glm::ivec2> Building::GetOccupiedTiles() const {
    std::vector<glm::ivec2> tiles;
    glm::ivec2 size = GetSize();

    tiles.reserve(size.x * size.y);
    for (int dy = 0; dy < size.y; ++dy) {
        for (int dx = 0; dx < size.x; ++dx) {
            tiles.emplace_back(m_gridPosition.x + dx, m_gridPosition.y + dy);
        }
    }

    return tiles;
}

bool Building::OccupiesTile(int x, int y) const {
    glm::ivec2 size = GetSize();
    return x >= m_gridPosition.x && x < m_gridPosition.x + size.x &&
           y >= m_gridPosition.y && y < m_gridPosition.y + size.y;
}

void Building::SetGateOpen(bool open) noexcept {
    if (m_buildingType != BuildingType::Gate) return;
    m_gateOpen = open;
}

bool Building::AssignWorker(Worker* worker) {
    if (!worker) return false;
    if (!HasWorkerSpace()) return false;

    // Check if worker is already assigned
    auto it = std::find(m_assignedWorkers.begin(), m_assignedWorkers.end(), worker);
    if (it != m_assignedWorkers.end()) {
        return false;  // Already assigned
    }

    m_assignedWorkers.push_back(worker);
    worker->SetWorkplace(this);
    return true;
}

bool Building::RemoveWorker(Worker* worker) {
    if (!worker) return false;

    auto it = std::find(m_assignedWorkers.begin(), m_assignedWorkers.end(), worker);
    if (it != m_assignedWorkers.end()) {
        m_assignedWorkers.erase(it);
        worker->SetWorkplace(nullptr);
        return true;
    }

    return false;
}

void Building::ClearWorkers() {
    for (Worker* worker : m_assignedWorkers) {
        if (worker) {
            worker->SetWorkplace(nullptr);
        }
    }
    m_assignedWorkers.clear();
}

void Building::LoadTextures(Nova::Renderer& renderer) {
    // Texture loading would integrate with Nova::Renderer
    // This is a placeholder for the actual implementation
}

void Building::Die() {
    BuildingState oldState = m_state;
    m_state = BuildingState::Destroyed;

    // Clear all workers
    ClearWorkers();

    if (m_onStateChange) {
        m_onStateChange(*this, oldState, m_state);
    }

    if (m_onDestroyed) {
        m_onDestroyed(*this);
    }

    Entity::Die();
}

// ============================================================================
// Worker Implementation
// ============================================================================

Worker::Worker()
    : Entity(EntityType::NPC)  // Workers are NPC type
{
    SetName("Worker");
    m_maxHealth = 50.0f;
    m_health = m_maxHealth;
    m_moveSpeed = 3.0f;
    m_collisionRadius = 0.4f;
}

void Worker::Update(float deltaTime) {
    if (!m_active) return;

    // Worker AI and movement would go here
    // - Move to workplace
    // - Perform work
    // - Return home when off duty

    Entity::Update(deltaTime);
}

void Worker::Render(Nova::Renderer& renderer) {
    if (!m_active) return;

    Entity::Render(renderer);
}

} // namespace Vehement
