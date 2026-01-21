#include "DefenseStructures.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {
namespace Buildings {

// =============================================================================
// WallSegmentComponent Implementation
// =============================================================================

WallSegmentComponent::WallSegmentComponent(const std::string& id, const std::string& name)
    : BuildingComponent(id, name) {
    SetCategory("Defense");
    UpdateStats();
}

float WallSegmentComponent::GetHeight() const {
    return m_cachedHeight;
}

float WallSegmentComponent::GetThickness() const {
    return m_cachedThickness;
}

float WallSegmentComponent::GetHealthPoints() const {
    return m_cachedHealth;
}

float WallSegmentComponent::GetArmor() const {
    return m_cachedArmor;
}

void WallSegmentComponent::SetUpgradeLevel(int level) {
    m_upgradeLevel = level;
    UpdateStats();
}

bool WallSegmentComponent::CanUpgrade() const {
    return m_upgradeLevel < 5 && static_cast<int>(m_wallType) < static_cast<int>(WallType::FortifiedWall);
}

WallSegmentComponent::WallType WallSegmentComponent::GetUpgradedType() const {
    if (!CanUpgrade()) return m_wallType;
    return static_cast<WallType>(static_cast<int>(m_wallType) + 1);
}

WallSegmentComponent::UpgradeCost WallSegmentComponent::GetUpgradeCost() const {
    UpgradeCost cost;
    float length = GetLength();

    switch (m_wallType) {
        case WallType::Barricade:
            cost.resources["wood"] = 20.0f * length;
            cost.buildTime = 5.0f * length;
            break;
        case WallType::WoodenWall:
            cost.resources["wood"] = 50.0f * length;
            cost.resources["stone"] = 25.0f * length;
            cost.buildTime = 10.0f * length;
            break;
        case WallType::StoneWall:
            cost.resources["stone"] = 100.0f * length;
            cost.resources["gold"] = 20.0f * length;
            cost.buildTime = 15.0f * length;
            break;
        case WallType::ReinforcedWall:
            cost.resources["stone"] = 150.0f * length;
            cost.resources["iron"] = 50.0f * length;
            cost.resources["gold"] = 40.0f * length;
            cost.buildTime = 20.0f * length;
            break;
        case WallType::FortifiedWall:
            cost.resources["stone"] = 200.0f * length;
            cost.resources["iron"] = 100.0f * length;
            cost.resources["gold"] = 80.0f * length;
            cost.buildTime = 30.0f * length;
            break;
    }

    return cost;
}

std::vector<glm::vec3> WallSegmentComponent::GenerateWallPath(int subdivisions) const {
    std::vector<glm::vec3> path;

    if (std::abs(m_curvature) < 0.01f) {
        // Straight wall
        for (int i = 0; i <= subdivisions; ++i) {
            float t = static_cast<float>(i) / subdivisions;
            path.push_back(glm::mix(m_startPosition, m_endPosition, t));
        }
    } else {
        // Curved wall using quadratic Bezier
        glm::vec3 mid = (m_startPosition + m_endPosition) * 0.5f;
        glm::vec3 dir = glm::normalize(m_endPosition - m_startPosition);
        glm::vec3 perpendicular(-dir.z, 0, dir.x);
        float offset = glm::length(m_endPosition - m_startPosition) * 0.3f * m_curvature;
        glm::vec3 controlPoint = mid + perpendicular * offset;

        for (int i = 0; i <= subdivisions; ++i) {
            float t = static_cast<float>(i) / subdivisions;
            float t2 = t * t;
            float mt = 1.0f - t;
            float mt2 = mt * mt;

            glm::vec3 point = mt2 * m_startPosition + 2.0f * mt * t * controlPoint + t2 * m_endPosition;
            path.push_back(point);
        }
    }

    return path;
}

void WallSegmentComponent::UpdateStats() {
    // Base stats on wall type
    switch (m_wallType) {
        case WallType::Barricade:
            m_cachedHeight = 1.5f;
            m_cachedThickness = 0.2f;
            m_cachedHealth = 300.0f;
            m_cachedArmor = 2.0f;
            break;
        case WallType::WoodenWall:
            m_cachedHeight = 3.0f;
            m_cachedThickness = 0.4f;
            m_cachedHealth = 800.0f;
            m_cachedArmor = 5.0f;
            break;
        case WallType::StoneWall:
            m_cachedHeight = 4.0f;
            m_cachedThickness = 0.6f;
            m_cachedHealth = 2000.0f;
            m_cachedArmor = 15.0f;
            break;
        case WallType::ReinforcedWall:
            m_cachedHeight = 5.0f;
            m_cachedThickness = 0.8f;
            m_cachedHealth = 4000.0f;
            m_cachedArmor = 25.0f;
            break;
        case WallType::FortifiedWall:
            m_cachedHeight = 6.0f;
            m_cachedThickness = 1.0f;
            m_cachedHealth = 8000.0f;
            m_cachedArmor = 40.0f;
            break;
    }

    // Apply upgrade level multiplier
    float multiplier = 1.0f + (m_upgradeLevel - 1) * 0.2f;
    m_cachedHealth *= multiplier;
    m_cachedArmor *= multiplier;
}

nlohmann::json WallSegmentComponent::Serialize() const {
    nlohmann::json json = BuildingComponent::Serialize();
    json["wallType"] = static_cast<int>(m_wallType);
    json["upgradeLevel"] = m_upgradeLevel;
    json["startPosition"] = {m_startPosition.x, m_startPosition.y, m_startPosition.z};
    json["endPosition"] = {m_endPosition.x, m_endPosition.y, m_endPosition.z};
    json["curvature"] = m_curvature;
    return json;
}

WallSegmentPtr WallSegmentComponent::Deserialize(const nlohmann::json& json) {
    auto wall = std::make_shared<WallSegmentComponent>(
        json.value("id", ""),
        json.value("name", "")
    );

    wall->m_wallType = static_cast<WallType>(json.value("wallType", 0));
    wall->m_upgradeLevel = json.value("upgradeLevel", 1);

    if (json.contains("startPosition")) {
        auto sp = json["startPosition"];
        wall->m_startPosition = glm::vec3(sp[0], sp[1], sp[2]);
    }

    if (json.contains("endPosition")) {
        auto ep = json["endPosition"];
        wall->m_endPosition = glm::vec3(ep[0], ep[1], ep[2]);
    }

    wall->m_curvature = json.value("curvature", 0.0f);
    wall->UpdateStats();

    return wall;
}

// =============================================================================
// TowerComponent Implementation
// =============================================================================

TowerComponent::TowerComponent(const std::string& id, const std::string& name)
    : BuildingComponent(id, name) {
    SetCategory("Defense");
    UpdateStats();
}

float TowerComponent::GetHeight() const {
    return m_cachedHeight;
}

float TowerComponent::GetRadius() const {
    return m_cachedRadius;
}

float TowerComponent::GetHealthPoints() const {
    return m_cachedHealth;
}

float TowerComponent::GetArmor() const {
    return m_cachedArmor;
}

float TowerComponent::GetVisionRange() const {
    return m_cachedVisionRange;
}

int TowerComponent::GetGarrisonCapacity() const {
    return m_cachedGarrisonCapacity;
}

void TowerComponent::SetUpgradeLevel(int level) {
    m_upgradeLevel = level;
    UpdateStats();
}

bool TowerComponent::CanUpgrade() const {
    return m_upgradeLevel < 5 && static_cast<int>(m_towerType) < static_cast<int>(TowerType::Citadel);
}

TowerComponent::TowerType TowerComponent::GetUpgradedType() const {
    if (!CanUpgrade()) return m_towerType;
    return static_cast<TowerType>(static_cast<int>(m_towerType) + 1);
}

TowerComponent::UpgradeCost TowerComponent::GetUpgradeCost() const {
    UpgradeCost cost;

    switch (m_towerType) {
        case TowerType::WatchPost:
            cost.resources["wood"] = 100.0f;
            cost.buildTime = 30.0f;
            break;
        case TowerType::WoodenTower:
            cost.resources["wood"] = 200.0f;
            cost.resources["stone"] = 50.0f;
            cost.buildTime = 60.0f;
            break;
        case TowerType::StoneTower:
            cost.resources["stone"] = 300.0f;
            cost.resources["gold"] = 50.0f;
            cost.buildTime = 120.0f;
            break;
        case TowerType::GuardTower:
            cost.resources["stone"] = 500.0f;
            cost.resources["iron"] = 100.0f;
            cost.resources["gold"] = 100.0f;
            cost.buildTime = 180.0f;
            break;
        case TowerType::Citadel:
            cost.resources["stone"] = 800.0f;
            cost.resources["iron"] = 200.0f;
            cost.resources["gold"] = 200.0f;
            cost.buildTime = 300.0f;
            break;
    }

    return cost;
}

void TowerComponent::AddConnectedWall(WallSegmentPtr wall) {
    m_connectedWalls.push_back(wall);
}

void TowerComponent::RemoveConnectedWall(const std::string& wallId) {
    m_connectedWalls.erase(
        std::remove_if(m_connectedWalls.begin(), m_connectedWalls.end(),
            [&](const std::weak_ptr<WallSegmentComponent>& w) {
                auto wall = w.lock();
                return !wall || wall->GetId() == wallId;
            }),
        m_connectedWalls.end()
    );
}

std::vector<WallSegmentPtr> TowerComponent::GetConnectedWalls() const {
    std::vector<WallSegmentPtr> walls;
    for (const auto& weakWall : m_connectedWalls) {
        if (auto wall = weakWall.lock()) {
            walls.push_back(wall);
        }
    }
    return walls;
}

int TowerComponent::GetConnectionCount() const {
    return static_cast<int>(GetConnectedWalls().size());
}

void TowerComponent::UpdateStats() {
    switch (m_towerType) {
        case TowerType::WatchPost:
            m_cachedHeight = 4.0f;
            m_cachedRadius = 0.8f;
            m_cachedHealth = 500.0f;
            m_cachedArmor = 5.0f;
            m_cachedVisionRange = 12.0f;
            m_cachedGarrisonCapacity = 2;
            break;
        case TowerType::WoodenTower:
            m_cachedHeight = 6.0f;
            m_cachedRadius = 1.2f;
            m_cachedHealth = 1500.0f;
            m_cachedArmor = 10.0f;
            m_cachedVisionRange = 15.0f;
            m_cachedGarrisonCapacity = 4;
            break;
        case TowerType::StoneTower:
            m_cachedHeight = 8.0f;
            m_cachedRadius = 1.5f;
            m_cachedHealth = 3500.0f;
            m_cachedArmor = 20.0f;
            m_cachedVisionRange = 18.0f;
            m_cachedGarrisonCapacity = 6;
            break;
        case TowerType::GuardTower:
            m_cachedHeight = 10.0f;
            m_cachedRadius = 1.8f;
            m_cachedHealth = 7000.0f;
            m_cachedArmor = 35.0f;
            m_cachedVisionRange = 22.0f;
            m_cachedGarrisonCapacity = 8;
            break;
        case TowerType::Citadel:
            m_cachedHeight = 14.0f;
            m_cachedRadius = 2.5f;
            m_cachedHealth = 15000.0f;
            m_cachedArmor = 60.0f;
            m_cachedVisionRange = 28.0f;
            m_cachedGarrisonCapacity = 12;
            break;
    }

    float multiplier = 1.0f + (m_upgradeLevel - 1) * 0.2f;
    m_cachedHealth *= multiplier;
    m_cachedArmor *= multiplier;
    m_cachedVisionRange *= (1.0f + (m_upgradeLevel - 1) * 0.1f);
}

nlohmann::json TowerComponent::Serialize() const {
    nlohmann::json json = BuildingComponent::Serialize();
    json["towerType"] = static_cast<int>(m_towerType);
    json["upgradeLevel"] = m_upgradeLevel;
    json["position"] = {m_position.x, m_position.y, m_position.z};
    return json;
}

TowerComponentPtr TowerComponent::Deserialize(const nlohmann::json& json) {
    auto tower = std::make_shared<TowerComponent>(
        json.value("id", ""),
        json.value("name", "")
    );

    tower->m_towerType = static_cast<TowerType>(json.value("towerType", 0));
    tower->m_upgradeLevel = json.value("upgradeLevel", 1);

    if (json.contains("position")) {
        auto pos = json["position"];
        tower->m_position = glm::vec3(pos[0], pos[1], pos[2]);
    }

    tower->UpdateStats();
    return tower;
}

// =============================================================================
// GateComponent Implementation
// =============================================================================

GateComponent::GateComponent(const std::string& id, const std::string& name)
    : BuildingComponent(id, name) {
    SetCategory("Defense");
    UpdateStats();
}

float GateComponent::GetWidth() const {
    return m_cachedWidth;
}

float GateComponent::GetHeight() const {
    return m_cachedHeight;
}

float GateComponent::GetHealthPoints() const {
    return m_cachedHealth;
}

float GateComponent::GetArmor() const {
    return m_cachedArmor;
}

float GateComponent::GetOpenCloseTime() const {
    return m_cachedOpenCloseTime;
}

void GateComponent::SetUpgradeLevel(int level) {
    m_upgradeLevel = level;
    UpdateStats();
}

bool GateComponent::CanUpgrade() const {
    return m_upgradeLevel < 4 && static_cast<int>(m_gateType) < static_cast<int>(GateType::FortifiedGate);
}

GateComponent::GateType GateComponent::GetUpgradedType() const {
    if (!CanUpgrade()) return m_gateType;
    return static_cast<GateType>(static_cast<int>(m_gateType) + 1);
}

GateComponent::UpgradeCost GateComponent::GetUpgradeCost() const {
    UpgradeCost cost;

    switch (m_gateType) {
        case GateType::WoodenGate:
            cost.resources["wood"] = 150.0f;
            cost.resources["iron"] = 25.0f;
            cost.buildTime = 45.0f;
            break;
        case GateType::ReinforcedGate:
            cost.resources["wood"] = 200.0f;
            cost.resources["iron"] = 75.0f;
            cost.resources["gold"] = 30.0f;
            cost.buildTime = 90.0f;
            break;
        case GateType::SteelGate:
            cost.resources["iron"] = 200.0f;
            cost.resources["steel"] = 50.0f;
            cost.resources["gold"] = 60.0f;
            cost.buildTime = 150.0f;
            break;
        case GateType::FortifiedGate:
            cost.resources["iron"] = 300.0f;
            cost.resources["steel"] = 100.0f;
            cost.resources["gold"] = 120.0f;
            cost.buildTime = 240.0f;
            break;
    }

    return cost;
}

void GateComponent::UpdateStats() {
    switch (m_gateType) {
        case GateType::WoodenGate:
            m_cachedWidth = 3.0f;
            m_cachedHeight = 3.5f;
            m_cachedHealth = 600.0f;
            m_cachedArmor = 5.0f;
            m_cachedOpenCloseTime = 2.5f;
            break;
        case GateType::ReinforcedGate:
            m_cachedWidth = 3.5f;
            m_cachedHeight = 4.0f;
            m_cachedHealth = 1500.0f;
            m_cachedArmor = 12.0f;
            m_cachedOpenCloseTime = 3.0f;
            break;
        case GateType::SteelGate:
            m_cachedWidth = 4.0f;
            m_cachedHeight = 4.5f;
            m_cachedHealth = 3500.0f;
            m_cachedArmor = 25.0f;
            m_cachedOpenCloseTime = 4.0f;
            break;
        case GateType::FortifiedGate:
            m_cachedWidth = 4.5f;
            m_cachedHeight = 5.0f;
            m_cachedHealth = 7000.0f;
            m_cachedArmor = 45.0f;
            m_cachedOpenCloseTime = 5.0f;
            break;
    }

    float multiplier = 1.0f + (m_upgradeLevel - 1) * 0.2f;
    m_cachedHealth *= multiplier;
    m_cachedArmor *= multiplier;
}

nlohmann::json GateComponent::Serialize() const {
    nlohmann::json json = BuildingComponent::Serialize();
    json["gateType"] = static_cast<int>(m_gateType);
    json["upgradeLevel"] = m_upgradeLevel;
    json["position"] = {m_position.x, m_position.y, m_position.z};
    json["rotationY"] = m_rotationY;
    json["isOpen"] = m_isOpen;
    return json;
}

GateComponentPtr GateComponent::Deserialize(const nlohmann::json& json) {
    auto gate = std::make_shared<GateComponent>(
        json.value("id", ""),
        json.value("name", "")
    );

    gate->m_gateType = static_cast<GateType>(json.value("gateType", 0));
    gate->m_upgradeLevel = json.value("upgradeLevel", 1);

    if (json.contains("position")) {
        auto pos = json["position"];
        gate->m_position = glm::vec3(pos[0], pos[1], pos[2]);
    }

    gate->m_rotationY = json.value("rotationY", 0.0f);
    gate->m_isOpen = json.value("isOpen", false);

    gate->UpdateStats();
    return gate;
}

// =============================================================================
// StandaloneWallPlacementController Implementation
// =============================================================================

StandaloneWallPlacementController::StandaloneWallPlacementController(BuildingInstancePtr building)
    : m_building(building) {}

void StandaloneWallPlacementController::StartPlacingWall(WallSegmentComponent::WallType wallType) {
    m_wallType = wallType;
    m_state = PlacementState::PlacingStartPoint;
    m_curvature = 0.0f;
    m_createdWall.reset();
    m_createdStartTower.reset();
    m_createdEndTower.reset();
}

bool StandaloneWallPlacementController::PlacePoint(const glm::vec3& position) {
    std::vector<std::string> errors;

    if (m_state == PlacementState::PlacingStartPoint) {
        if (!ValidatePlacement(position, errors)) {
            m_preview.errors = errors;
            m_preview.valid = false;
            return false;
        }

        m_startPoint = position;
        m_state = PlacementState::PlacingEndPoint;
        return false; // Not finished yet
    }
    else if (m_state == PlacementState::PlacingEndPoint) {
        if (!ValidatePlacement(position, errors)) {
            m_preview.errors = errors;
            m_preview.valid = false;
            return false;
        }

        m_endPoint = position;

        // Check length constraints
        float length = glm::distance(m_startPoint, m_endPoint);
        if (length < GetMinWallLength()) {
            errors.push_back("Wall too short (min: " + std::to_string(GetMinWallLength()) + "m)");
            m_preview.errors = errors;
            m_preview.valid = false;
            return false;
        }

        if (length > GetMaxWallLength()) {
            errors.push_back("Wall too long (max: " + std::to_string(GetMaxWallLength()) + "m)");
            m_preview.errors = errors;
            m_preview.valid = false;
            return false;
        }

        CreateWallSegment();
        m_state = PlacementState::Finished;
        return true; // Segment complete
    }

    return false;
}

void StandaloneWallPlacementController::CancelPlacement() {
    m_state = PlacementState::Finished;
    m_createdWall.reset();
    m_createdStartTower.reset();
    m_createdEndTower.reset();
}

void StandaloneWallPlacementController::FinishPlacement() {
    m_state = PlacementState::Finished;
}

void StandaloneWallPlacementController::UpdatePreview(const glm::vec3& mousePosition) {
    m_preview.currentMousePosition = mousePosition;
    m_preview.errors.clear();

    if (m_state == PlacementState::PlacingStartPoint) {
        m_preview.startPoint = mousePosition;
        m_preview.valid = ValidatePlacement(mousePosition, m_preview.errors);
    }
    else if (m_state == PlacementState::PlacingEndPoint) {
        m_preview.endPoint = mousePosition;

        float length = glm::distance(m_startPoint, mousePosition);
        m_preview.valid = (length >= GetMinWallLength() &&
                          length <= GetMaxWallLength() &&
                          ValidatePlacement(mousePosition, m_preview.errors));
    }

    m_preview.glowColor = m_preview.valid ?
        glm::vec4(0.0f, 1.0f, 0.0f, 0.5f) :
        glm::vec4(1.0f, 0.0f, 0.0f, 0.5f);
}

void StandaloneWallPlacementController::SetCurvature(float curvature) {
    m_curvature = glm::clamp(curvature, -1.0f, 1.0f);
    if (m_createdWall) {
        m_createdWall->SetCurvature(m_curvature);
    }
}

bool StandaloneWallPlacementController::ValidatePlacement(const glm::vec3& position,
                                                         std::vector<std::string>& errors) const {
    // Check if within building bounds
    glm::vec3 minBounds = m_building->GetTotalBoundsMin();
    glm::vec3 maxBounds = m_building->GetTotalBoundsMax();

    if (position.x < minBounds.x || position.x > maxBounds.x ||
        position.z < minBounds.z || position.z > maxBounds.z) {
        errors.push_back("Position outside building bounds");
        return false;
    }

    return true;
}

void StandaloneWallPlacementController::CreateWallSegment() {
    static size_t wallId = 0;

    m_createdWall = std::make_shared<WallSegmentComponent>(
        "wall_" + std::to_string(wallId++),
        "Wall Segment"
    );

    m_createdWall->SetWallType(m_wallType);
    m_createdWall->SetStartPosition(m_startPoint);
    m_createdWall->SetEndPosition(m_endPoint);
    m_createdWall->SetCurvature(m_curvature);

    if (m_autoPlaceTowers) {
        m_createdStartTower = CreateTowerAt(m_startPoint);
        m_createdEndTower = CreateTowerAt(m_endPoint);

        m_createdWall->SetStartTower(m_createdStartTower);
        m_createdWall->SetEndTower(m_createdEndTower);

        m_createdStartTower->AddConnectedWall(m_createdWall);
        m_createdEndTower->AddConnectedWall(m_createdWall);
    }
}

TowerComponentPtr StandaloneWallPlacementController::CreateTowerAt(const glm::vec3& position) {
    static size_t towerId = 0;

    auto tower = std::make_shared<TowerComponent>(
        "tower_" + std::to_string(towerId++),
        "Tower"
    );

    tower->SetPosition(position);

    // Match tower type to wall type
    switch (m_wallType) {
        case WallSegmentComponent::WallType::Barricade:
        case WallSegmentComponent::WallType::WoodenWall:
            tower->SetTowerType(TowerComponent::TowerType::WatchPost);
            break;
        case WallSegmentComponent::WallType::StoneWall:
            tower->SetTowerType(TowerComponent::TowerType::WoodenTower);
            break;
        case WallSegmentComponent::WallType::ReinforcedWall:
            tower->SetTowerType(TowerComponent::TowerType::StoneTower);
            break;
        case WallSegmentComponent::WallType::FortifiedWall:
            tower->SetTowerType(TowerComponent::TowerType::GuardTower);
            break;
    }

    return tower;
}

// =============================================================================
// DefenseStructureManager Implementation
// =============================================================================

DefenseStructureManager::DefenseStructureManager(BuildingInstancePtr building)
    : m_building(building) {}

void DefenseStructureManager::AddWallSegment(WallSegmentPtr wall) {
    m_walls[wall->GetId()] = wall;
    ConnectWallToTowers(wall);
}

void DefenseStructureManager::RemoveWallSegment(const std::string& wallId) {
    auto it = m_walls.find(wallId);
    if (it != m_walls.end()) {
        DisconnectWallFromTowers(it->second);
        m_walls.erase(it);
    }
}

WallSegmentPtr DefenseStructureManager::GetWall(const std::string& wallId) const {
    auto it = m_walls.find(wallId);
    return it != m_walls.end() ? it->second : nullptr;
}

std::vector<WallSegmentPtr> DefenseStructureManager::GetAllWalls() const {
    std::vector<WallSegmentPtr> walls;
    for (const auto& [id, wall] : m_walls) {
        walls.push_back(wall);
    }
    return walls;
}

void DefenseStructureManager::AddTower(TowerComponentPtr tower) {
    m_towers[tower->GetId()] = tower;
}

void DefenseStructureManager::RemoveTower(const std::string& towerId) {
    m_towers.erase(towerId);
}

TowerComponentPtr DefenseStructureManager::GetTower(const std::string& towerId) const {
    auto it = m_towers.find(towerId);
    return it != m_towers.end() ? it->second : nullptr;
}

std::vector<TowerComponentPtr> DefenseStructureManager::GetAllTowers() const {
    std::vector<TowerComponentPtr> towers;
    for (const auto& [id, tower] : m_towers) {
        towers.push_back(tower);
    }
    return towers;
}

void DefenseStructureManager::AddGate(GateComponentPtr gate) {
    m_gates[gate->GetId()] = gate;
}

void DefenseStructureManager::RemoveGate(const std::string& gateId) {
    m_gates.erase(gateId);
}

GateComponentPtr DefenseStructureManager::GetGate(const std::string& gateId) const {
    auto it = m_gates.find(gateId);
    return it != m_gates.end() ? it->second : nullptr;
}

std::vector<GateComponentPtr> DefenseStructureManager::GetAllGates() const {
    std::vector<GateComponentPtr> gates;
    for (const auto& [id, gate] : m_gates) {
        gates.push_back(gate);
    }
    return gates;
}

bool DefenseStructureManager::UpgradeWall(const std::string& wallId) {
    auto wall = GetWall(wallId);
    if (!wall || !wall->CanUpgrade()) {
        return false;
    }

    wall->SetWallType(wall->GetUpgradedType());
    wall->SetUpgradeLevel(wall->GetUpgradeLevel() + 1);
    return true;
}

bool DefenseStructureManager::UpgradeTower(const std::string& towerId) {
    auto tower = GetTower(towerId);
    if (!tower || !tower->CanUpgrade()) {
        return false;
    }

    tower->SetTowerType(tower->GetUpgradedType());
    tower->SetUpgradeLevel(tower->GetUpgradeLevel() + 1);
    return true;
}

bool DefenseStructureManager::UpgradeGate(const std::string& gateId) {
    auto gate = GetGate(gateId);
    if (!gate || !gate->CanUpgrade()) {
        return false;
    }

    gate->SetGateType(gate->GetUpgradedType());
    gate->SetUpgradeLevel(gate->GetUpgradeLevel() + 1);
    return true;
}

bool DefenseStructureManager::ReplaceWallWithGate(const std::string& wallId,
                                                  GateComponent::GateType gateType) {
    auto wall = GetWall(wallId);
    if (!wall) return false;

    static size_t gateId = 0;
    auto gate = std::make_shared<GateComponent>(
        "gate_" + std::to_string(gateId++),
        "Gate"
    );

    gate->SetGateType(gateType);
    gate->SetPosition(wall->GetMidpoint());
    gate->SetReplacedWall(wall);

    wall->SetGateReplacement(gate);

    AddGate(gate);
    return true;
}

TowerComponentPtr DefenseStructureManager::FindNearestTower(const glm::vec3& position,
                                                            float maxDistance) const {
    TowerComponentPtr nearest = nullptr;
    float minDist = maxDistance;

    for (const auto& [id, tower] : m_towers) {
        float dist = glm::distance(position, tower->GetPosition());
        if (dist < minDist) {
            minDist = dist;
            nearest = tower;
        }
    }

    return nearest;
}

std::vector<WallSegmentPtr> DefenseStructureManager::GetWallsConnectedToTower(const std::string& towerId) const {
    auto tower = GetTower(towerId);
    if (!tower) return {};
    return tower->GetConnectedWalls();
}

float DefenseStructureManager::GetTotalDefenseValue() const {
    float value = 0.0f;

    for (const auto& [id, wall] : m_walls) {
        value += wall->GetHealthPoints() * 0.5f + wall->GetArmor() * 10.0f;
    }

    for (const auto& [id, tower] : m_towers) {
        value += tower->GetHealthPoints() * 0.8f + tower->GetArmor() * 15.0f;
    }

    for (const auto& [id, gate] : m_gates) {
        value += gate->GetHealthPoints() * 0.6f + gate->GetArmor() * 12.0f;
    }

    return value;
}

void DefenseStructureManager::ConnectWallToTowers(WallSegmentPtr wall) {
    // Find towers at start and end positions
    auto startTower = FindNearestTower(wall->GetStartPosition(), 0.5f);
    auto endTower = FindNearestTower(wall->GetEndPosition(), 0.5f);

    if (startTower) {
        wall->SetStartTower(startTower);
        startTower->AddConnectedWall(wall);
    }

    if (endTower) {
        wall->SetEndTower(endTower);
        endTower->AddConnectedWall(wall);
    }
}

void DefenseStructureManager::DisconnectWallFromTowers(WallSegmentPtr wall) {
    if (auto startTower = wall->GetStartTower()) {
        startTower->RemoveConnectedWall(wall->GetId());
    }

    if (auto endTower = wall->GetEndTower()) {
        endTower->RemoveConnectedWall(wall->GetId());
    }
}

} // namespace Buildings
} // namespace Nova
