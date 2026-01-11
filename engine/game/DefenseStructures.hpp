#pragma once

#include "BuildingComponentSystem.hpp"
#include "WallPlacementSystem.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <optional>

namespace Nova {
namespace Buildings {

// Forward declarations
class WallSegmentComponent;
class TowerComponent;
class GateComponent;

using WallSegmentPtr = std::shared_ptr<WallSegmentComponent>;
using TowerComponentPtr = std::shared_ptr<TowerComponent>;
using GateComponentPtr = std::shared_ptr<GateComponent>;

// =============================================================================
// Wall Segment Component
// =============================================================================

/**
 * @brief Represents a single wall segment that can be upgraded independently
 */
class WallSegmentComponent : public BuildingComponent {
public:
    enum class WallType {
        Barricade,      // Simple wooden barrier
        WoodenWall,     // Basic defensive wall
        StoneWall,      // Solid stone fortification
        ReinforcedWall, // Iron-reinforced stone
        FortifiedWall   // Heavy fortified wall
    };

    WallSegmentComponent(const std::string& id, const std::string& name);

    // Wall properties
    void SetWallType(WallType type) { m_wallType = type; }
    WallType GetWallType() const { return m_wallType; }

    void SetStartPosition(const glm::vec3& pos) { m_startPosition = pos; }
    void SetEndPosition(const glm::vec3& pos) { m_endPosition = pos; }
    glm::vec3 GetStartPosition() const { return m_startPosition; }
    glm::vec3 GetEndPosition() const { return m_endPosition; }

    float GetLength() const { return glm::distance(m_startPosition, m_endPosition); }
    glm::vec3 GetMidpoint() const { return (m_startPosition + m_endPosition) * 0.5f; }
    glm::vec3 GetDirection() const { return glm::normalize(m_endPosition - m_startPosition); }

    // Wall stats based on type
    float GetHeight() const;
    float GetThickness() const;
    float GetHealthPoints() const;
    float GetArmor() const;

    // Upgrade system
    int GetUpgradeLevel() const { return m_upgradeLevel; }
    void SetUpgradeLevel(int level);
    bool CanUpgrade() const;
    WallType GetUpgradedType() const;

    struct UpgradeCost {
        std::unordered_map<std::string, float> resources;
        float buildTime;
    };
    UpgradeCost GetUpgradeCost() const;

    // Curve support (reuse from WallSegment)
    void SetCurvature(float curvature) { m_curvature = curvature; }
    float GetCurvature() const { return m_curvature; }
    std::vector<glm::vec3> GenerateWallPath(int subdivisions = 20) const;

    // Connections
    void SetStartTower(TowerComponentPtr tower) { m_startTower = tower; }
    void SetEndTower(TowerComponentPtr tower) { m_endTower = tower; }
    TowerComponentPtr GetStartTower() const { return m_startTower.lock(); }
    TowerComponentPtr GetEndTower() const { return m_endTower.lock(); }

    // Gate replacement
    void SetGateReplacement(GateComponentPtr gate) { m_gateReplacement = gate; }
    GateComponentPtr GetGateReplacement() const { return m_gateReplacement.lock(); }
    bool HasGate() const { return !m_gateReplacement.expired(); }

    // Serialization
    nlohmann::json Serialize() const override;
    static WallSegmentPtr Deserialize(const nlohmann::json& json);

private:
    WallType m_wallType = WallType::WoodenWall;
    int m_upgradeLevel = 1;

    glm::vec3 m_startPosition{0, 0, 0};
    glm::vec3 m_endPosition{1, 0, 0};
    float m_curvature = 0.0f;

    // Connections (weak pointers to avoid circular references)
    std::weak_ptr<TowerComponent> m_startTower;
    std::weak_ptr<TowerComponent> m_endTower;
    std::weak_ptr<GateComponent> m_gateReplacement;

    // Stats cache
    void UpdateStats();
    float m_cachedHeight = 3.0f;
    float m_cachedThickness = 0.5f;
    float m_cachedHealth = 1000.0f;
    float m_cachedArmor = 10.0f;
};

// =============================================================================
// Tower Component
// =============================================================================

/**
 * @brief Tower structure that connects to wall endpoints
 */
class TowerComponent : public BuildingComponent {
public:
    enum class TowerType {
        WatchPost,      // Simple elevated platform
        WoodenTower,    // Basic wooden tower
        StoneTower,     // Solid stone tower
        GuardTower,     // Fortified tower with battlements
        Citadel         // Massive fortified tower
    };

    TowerComponent(const std::string& id, const std::string& name);

    // Tower properties
    void SetTowerType(TowerType type) { m_towerType = type; }
    TowerType GetTowerType() const { return m_towerType; }

    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    glm::vec3 GetPosition() const { return m_position; }

    // Tower stats
    float GetHeight() const;
    float GetRadius() const;
    float GetHealthPoints() const;
    float GetArmor() const;
    float GetVisionRange() const;
    int GetGarrisonCapacity() const;

    // Upgrade system
    int GetUpgradeLevel() const { return m_upgradeLevel; }
    void SetUpgradeLevel(int level);
    bool CanUpgrade() const;
    TowerType GetUpgradedType() const;

    struct UpgradeCost {
        std::unordered_map<std::string, float> resources;
        float buildTime;
    };
    UpgradeCost GetUpgradeCost() const;

    // Wall connections
    void AddConnectedWall(WallSegmentPtr wall);
    void RemoveConnectedWall(const std::string& wallId);
    std::vector<WallSegmentPtr> GetConnectedWalls() const;
    int GetConnectionCount() const;

    // Serialization
    nlohmann::json Serialize() const override;
    static TowerComponentPtr Deserialize(const nlohmann::json& json);

private:
    TowerType m_towerType = TowerType::WoodenTower;
    int m_upgradeLevel = 1;
    glm::vec3 m_position{0, 0, 0};

    // Connected walls
    std::vector<std::weak_ptr<WallSegmentComponent>> m_connectedWalls;

    // Stats cache
    void UpdateStats();
    float m_cachedHeight = 6.0f;
    float m_cachedRadius = 1.5f;
    float m_cachedHealth = 2000.0f;
    float m_cachedArmor = 20.0f;
    float m_cachedVisionRange = 15.0f;
    int m_cachedGarrisonCapacity = 4;
};

// =============================================================================
// Gate Component
// =============================================================================

/**
 * @brief Gate that replaces a wall segment
 */
class GateComponent : public BuildingComponent {
public:
    enum class GateType {
        WoodenGate,     // Basic wooden gate
        ReinforcedGate, // Iron-reinforced gate
        SteelGate,      // Heavy steel gate
        FortifiedGate   // Fortified entrance
    };

    GateComponent(const std::string& id, const std::string& name);

    // Gate properties
    void SetGateType(GateType type) { m_gateType = type; }
    GateType GetGateType() const { return m_gateType; }

    void SetPosition(const glm::vec3& pos) { m_position = pos; }
    glm::vec3 GetPosition() const { return m_position; }

    void SetRotation(float angleY) { m_rotationY = angleY; }
    float GetRotation() const { return m_rotationY; }

    // Gate stats
    float GetWidth() const;
    float GetHeight() const;
    float GetHealthPoints() const;
    float GetArmor() const;

    // Gate state
    bool IsOpen() const { return m_isOpen; }
    void SetOpen(bool open) { m_isOpen = open; }
    float GetOpenCloseTime() const;

    // Upgrade system
    int GetUpgradeLevel() const { return m_upgradeLevel; }
    void SetUpgradeLevel(int level);
    bool CanUpgrade() const;
    GateType GetUpgradedType() const;

    struct UpgradeCost {
        std::unordered_map<std::string, float> resources;
        float buildTime;
    };
    UpgradeCost GetUpgradeCost() const;

    // Wall connection
    void SetReplacedWall(WallSegmentPtr wall) { m_replacedWall = wall; }
    WallSegmentPtr GetReplacedWall() const { return m_replacedWall.lock(); }

    // Serialization
    nlohmann::json Serialize() const override;
    static GateComponentPtr Deserialize(const nlohmann::json& json);

private:
    GateType m_gateType = GateType::WoodenGate;
    int m_upgradeLevel = 1;
    glm::vec3 m_position{0, 0, 0};
    float m_rotationY = 0.0f;
    bool m_isOpen = false;

    std::weak_ptr<WallSegmentComponent> m_replacedWall;

    // Stats cache
    void UpdateStats();
    float m_cachedWidth = 3.0f;
    float m_cachedHeight = 3.5f;
    float m_cachedHealth = 800.0f;
    float m_cachedArmor = 8.0f;
    float m_cachedOpenCloseTime = 2.0f;
};

// =============================================================================
// Standalone Wall Placement Controller
// =============================================================================

/**
 * @brief Placement controller for individual wall segments (no closed loop required)
 */
class StandaloneWallPlacementController {
public:
    enum class PlacementState {
        PlacingStartPoint,   // First click - place start
        PlacingEndPoint,     // Second click - place end
        AdjustingCurve,      // Optional - adjust wall curve
        Finished             // Wall segment complete
    };

    StandaloneWallPlacementController(BuildingInstancePtr building);

    // Placement flow
    void StartPlacingWall(WallSegmentComponent::WallType wallType);
    bool PlacePoint(const glm::vec3& position);  // Returns true when segment complete
    void CancelPlacement();
    void FinishPlacement();

    // State
    PlacementState GetState() const { return m_state; }
    bool IsPlacing() const { return m_state != PlacementState::Finished; }

    // Preview
    struct PreviewState {
        glm::vec3 startPoint{0, 0, 0};
        glm::vec3 endPoint{0, 0, 0};
        glm::vec3 currentMousePosition{0, 0, 0};
        bool valid = false;
        std::vector<std::string> errors;
        glm::vec4 glowColor{0.0f, 1.0f, 0.0f, 0.5f};
    };
    void UpdatePreview(const glm::vec3& mousePosition);
    const PreviewState& GetPreview() const { return m_preview; }

    // Curve editing
    void SetCurvature(float curvature);
    float GetCurvature() const { return m_curvature; }

    // Tower attachment
    void EnableAutoTowerPlacement(bool enable) { m_autoPlaceTowers = enable; }
    bool IsAutoTowerPlacementEnabled() const { return m_autoPlaceTowers; }

    // Wall type
    void SetWallType(WallSegmentComponent::WallType type) { m_wallType = type; }
    WallSegmentComponent::WallType GetWallType() const { return m_wallType; }

    // Get created segment
    WallSegmentPtr GetCreatedWall() const { return m_createdWall; }
    TowerComponentPtr GetCreatedStartTower() const { return m_createdStartTower; }
    TowerComponentPtr GetCreatedEndTower() const { return m_createdEndTower; }

private:
    BuildingInstancePtr m_building;
    PlacementState m_state = PlacementState::PlacingStartPoint;

    WallSegmentComponent::WallType m_wallType;
    glm::vec3 m_startPoint{0, 0, 0};
    glm::vec3 m_endPoint{0, 0, 0};
    float m_curvature = 0.0f;

    bool m_autoPlaceTowers = true;

    PreviewState m_preview;

    // Created components
    WallSegmentPtr m_createdWall;
    TowerComponentPtr m_createdStartTower;
    TowerComponentPtr m_createdEndTower;

    // Validation
    bool ValidatePlacement(const glm::vec3& position, std::vector<std::string>& errors) const;
    float GetMinWallLength() const { return 2.0f; }
    float GetMaxWallLength() const { return 20.0f; }

    // Creation
    void CreateWallSegment();
    TowerComponentPtr CreateTowerAt(const glm::vec3& position);
};

// =============================================================================
// Defense Structure Manager
// =============================================================================

/**
 * @brief Manages all defense structures (walls, towers, gates) in a building
 */
class DefenseStructureManager {
public:
    DefenseStructureManager(BuildingInstancePtr building);

    // Wall management
    void AddWallSegment(WallSegmentPtr wall);
    void RemoveWallSegment(const std::string& wallId);
    WallSegmentPtr GetWall(const std::string& wallId) const;
    std::vector<WallSegmentPtr> GetAllWalls() const;

    // Tower management
    void AddTower(TowerComponentPtr tower);
    void RemoveTower(const std::string& towerId);
    TowerComponentPtr GetTower(const std::string& towerId) const;
    std::vector<TowerComponentPtr> GetAllTowers() const;

    // Gate management
    void AddGate(GateComponentPtr gate);
    void RemoveGate(const std::string& gateId);
    GateComponentPtr GetGate(const std::string& gateId) const;
    std::vector<GateComponentPtr> GetAllGates() const;

    // Upgrade operations
    bool UpgradeWall(const std::string& wallId);
    bool UpgradeTower(const std::string& towerId);
    bool UpgradeGate(const std::string& gateId);

    // Replace wall with gate
    bool ReplaceWallWithGate(const std::string& wallId, GateComponent::GateType gateType);

    // Queries
    TowerComponentPtr FindNearestTower(const glm::vec3& position, float maxDistance = 2.0f) const;
    std::vector<WallSegmentPtr> GetWallsConnectedToTower(const std::string& towerId) const;

    // Statistics
    int GetTotalWallCount() const { return static_cast<int>(m_walls.size()); }
    int GetTotalTowerCount() const { return static_cast<int>(m_towers.size()); }
    int GetTotalGateCount() const { return static_cast<int>(m_gates.size()); }
    float GetTotalDefenseValue() const;

    // Serialization
    nlohmann::json Serialize() const;
    static std::shared_ptr<DefenseStructureManager> Deserialize(const nlohmann::json& json,
                                                                BuildingInstancePtr building);

private:
    BuildingInstancePtr m_building;

    std::unordered_map<std::string, WallSegmentPtr> m_walls;
    std::unordered_map<std::string, TowerComponentPtr> m_towers;
    std::unordered_map<std::string, GateComponentPtr> m_gates;

    // Helper methods
    void ConnectWallToTowers(WallSegmentPtr wall);
    void DisconnectWallFromTowers(WallSegmentPtr wall);
};

} // namespace Buildings
} // namespace Nova
