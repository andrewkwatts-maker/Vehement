#pragma once

#include "../entities/Entity.hpp"
#include "../world/Tile.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
    class Renderer;
    class Texture;
}

namespace Vehement {

// Forward declarations
class Worker;
class World;
class TileMap;

// ============================================================================
// Building Types
// ============================================================================

/**
 * @brief All available building types in the RTS system
 */
enum class BuildingType : uint8_t {
    // Housing (shelter for workers)
    Shelter,            // Basic housing (2 workers)
    House,              // Standard housing (4 workers)
    Barracks,           // Military housing (8 workers)

    // Production (generate resources)
    Farm,               // Produces food
    LumberMill,         // Processes wood
    Quarry,             // Processes stone
    Workshop,           // Crafts items/equipment

    // Defense (protect base)
    WatchTower,         // Vision and light defense
    Wall,               // Blocks movement
    Gate,               // Controlled passage (can open/close)
    Fortress,           // Heavy defense, hero respawn point

    // Special (utility buildings)
    TradingPost,        // Buy/sell resources
    Hospital,           // Heal workers
    Warehouse,          // Store extra resources
    CommandCenter,      // Main base building (required)

    COUNT
};

/**
 * @brief Building category for UI grouping
 */
enum class BuildingCategory : uint8_t {
    Housing,
    Production,
    Defense,
    Special
};

/**
 * @brief Get category for a building type
 */
inline BuildingCategory GetBuildingCategory(BuildingType type) {
    switch (type) {
        case BuildingType::Shelter:
        case BuildingType::House:
        case BuildingType::Barracks:
            return BuildingCategory::Housing;

        case BuildingType::Farm:
        case BuildingType::LumberMill:
        case BuildingType::Quarry:
        case BuildingType::Workshop:
            return BuildingCategory::Production;

        case BuildingType::WatchTower:
        case BuildingType::Wall:
        case BuildingType::Gate:
        case BuildingType::Fortress:
            return BuildingCategory::Defense;

        default:
            return BuildingCategory::Special;
    }
}

/**
 * @brief Get display name for building type
 */
inline const char* GetBuildingTypeName(BuildingType type) {
    switch (type) {
        case BuildingType::Shelter:         return "Shelter";
        case BuildingType::House:           return "House";
        case BuildingType::Barracks:        return "Barracks";
        case BuildingType::Farm:            return "Farm";
        case BuildingType::LumberMill:      return "Lumber Mill";
        case BuildingType::Quarry:          return "Quarry";
        case BuildingType::Workshop:        return "Workshop";
        case BuildingType::WatchTower:      return "Watch Tower";
        case BuildingType::Wall:            return "Wall";
        case BuildingType::Gate:            return "Gate";
        case BuildingType::Fortress:        return "Fortress";
        case BuildingType::TradingPost:     return "Trading Post";
        case BuildingType::Hospital:        return "Hospital";
        case BuildingType::Warehouse:       return "Warehouse";
        case BuildingType::CommandCenter:   return "Command Center";
        default:                            return "Unknown";
    }
}

/**
 * @brief Get description for building type
 */
inline const char* GetBuildingDescription(BuildingType type) {
    switch (type) {
        case BuildingType::Shelter:
            return "Basic shelter providing housing for 2 workers.";
        case BuildingType::House:
            return "Standard house with room for 4 workers.";
        case BuildingType::Barracks:
            return "Military housing for up to 8 soldiers.";
        case BuildingType::Farm:
            return "Produces food to sustain your population.";
        case BuildingType::LumberMill:
            return "Processes wood from nearby trees.";
        case BuildingType::Quarry:
            return "Extracts and processes stone.";
        case BuildingType::Workshop:
            return "Crafts tools and equipment.";
        case BuildingType::WatchTower:
            return "Reveals fog of war and provides light defense.";
        case BuildingType::Wall:
            return "Blocks zombie pathfinding and movement.";
        case BuildingType::Gate:
            return "Controlled passage that can be opened or closed.";
        case BuildingType::Fortress:
            return "Heavy fortification and hero revival point.";
        case BuildingType::TradingPost:
            return "Trade resources with other survivors.";
        case BuildingType::Hospital:
            return "Heals injured workers over time.";
        case BuildingType::Warehouse:
            return "Increases resource storage capacity.";
        case BuildingType::CommandCenter:
            return "Main base of operations. Protect at all costs!";
        default:
            return "Unknown building.";
    }
}

// ============================================================================
// Building Size Configuration
// ============================================================================

/**
 * @brief Get building footprint size in tiles
 */
inline glm::ivec2 GetBuildingSize(BuildingType type) {
    switch (type) {
        case BuildingType::Shelter:         return {2, 2};
        case BuildingType::House:           return {3, 3};
        case BuildingType::Barracks:        return {4, 4};
        case BuildingType::Farm:            return {4, 3};
        case BuildingType::LumberMill:      return {3, 3};
        case BuildingType::Quarry:          return {4, 4};
        case BuildingType::Workshop:        return {3, 3};
        case BuildingType::WatchTower:      return {2, 2};
        case BuildingType::Wall:            return {1, 1};
        case BuildingType::Gate:            return {2, 1};
        case BuildingType::Fortress:        return {5, 5};
        case BuildingType::TradingPost:     return {3, 3};
        case BuildingType::Hospital:        return {4, 3};
        case BuildingType::Warehouse:       return {4, 4};
        case BuildingType::CommandCenter:   return {5, 5};
        default:                            return {1, 1};
    }
}

// ============================================================================
// Building State
// ============================================================================

/**
 * @brief Current state of a building
 */
enum class BuildingState : uint8_t {
    Blueprint,          // Placed but not started
    UnderConstruction,  // Being built
    Operational,        // Fully functional
    Damaged,            // Reduced efficiency
    Destroyed,          // Non-functional, can be repaired
    Upgrading           // Being upgraded to next level
};

/**
 * @brief Get display name for building state
 */
inline const char* GetBuildingStateName(BuildingState state) {
    switch (state) {
        case BuildingState::Blueprint:          return "Blueprint";
        case BuildingState::UnderConstruction:  return "Under Construction";
        case BuildingState::Operational:        return "Operational";
        case BuildingState::Damaged:            return "Damaged";
        case BuildingState::Destroyed:          return "Destroyed";
        case BuildingState::Upgrading:          return "Upgrading";
        default:                                return "Unknown";
    }
}

// ============================================================================
// Building Textures
// ============================================================================

/**
 * @brief Texture paths for buildings using Vehement2 assets
 */
struct BuildingTextures {
    std::string base;           // Main building texture
    std::string roof;           // Top/roof texture
    std::string walls;          // Wall texture
    std::string damaged;        // Damaged overlay
    std::string construction;   // Under construction texture

    static BuildingTextures GetForType(BuildingType type);
};

inline BuildingTextures BuildingTextures::GetForType(BuildingType type) {
    BuildingTextures tex;
    const std::string basePath = "Vehement2/images/";

    switch (type) {
        case BuildingType::Shelter:
        case BuildingType::House:
            tex.base = basePath + "Wood/WoodFlooring1.png";
            tex.roof = basePath + "Wood/Wood1.png";
            tex.walls = basePath + "Wood/WoodCrate1.png";
            break;

        case BuildingType::Barracks:
        case BuildingType::Fortress:
            tex.base = basePath + "Stone/StoneMarble1.png";
            tex.roof = basePath + "Stone/StoneBlack.png";
            tex.walls = basePath + "Bricks/BricksStacked.png";
            break;

        case BuildingType::Farm:
            tex.base = basePath + "Wood/WoodFlooring2.png";
            tex.roof = basePath + "Wood/Wood1.png";
            tex.walls = basePath + "Wood/WoodCrate2.png";
            break;

        case BuildingType::LumberMill:
            tex.base = basePath + "Wood/WoodFlooring1.png";
            tex.roof = basePath + "Wood/WoodCrate1.png";
            tex.walls = basePath + "Wood/Wood1.png";
            break;

        case BuildingType::Quarry:
            tex.base = basePath + "Stone/StoneRaw.png";
            tex.roof = basePath + "Stone/StoneMarble2.png";
            tex.walls = basePath + "Stone/StoneBlack.png";
            break;

        case BuildingType::Workshop:
            tex.base = basePath + "Metal/MetalTile1.png";
            tex.roof = basePath + "Metal/Metal2.png";
            tex.walls = basePath + "Metal/Metal1.png";
            break;

        case BuildingType::WatchTower:
            tex.base = basePath + "Wood/WoodFlooring1.png";
            tex.roof = basePath + "Wood/WoodCrate1.png";
            tex.walls = basePath + "Bricks/BricksRock.png";
            break;

        case BuildingType::Wall:
        case BuildingType::Gate:
            tex.base = basePath + "Bricks/BricksStacked.png";
            tex.roof = basePath + "Bricks/BricksRock.png";
            tex.walls = basePath + "Bricks/BricksGrey.png";
            break;

        case BuildingType::TradingPost:
            tex.base = basePath + "Wood/WoodFlooring2.png";
            tex.roof = basePath + "Metal/Metal3.png";
            tex.walls = basePath + "Wood/WoodCrate2.png";
            break;

        case BuildingType::Hospital:
            tex.base = basePath + "Stone/StoneMarble1.png";
            tex.roof = basePath + "Stone/StoneMarble2.png";
            tex.walls = basePath + "Bricks/BricksBlack.png";
            break;

        case BuildingType::Warehouse:
            tex.base = basePath + "Metal/MetalTile2.png";
            tex.roof = basePath + "Metal/Metal4.png";
            tex.walls = basePath + "Metal/Metal1.png";
            break;

        case BuildingType::CommandCenter:
            tex.base = basePath + "Stone/StoneMarble2.png";
            tex.roof = basePath + "Metal/MetalTile3.png";
            tex.walls = basePath + "Bricks/BricksStacked.png";
            break;

        default:
            tex.base = basePath + "Bricks/BricksGrey.png";
            tex.roof = basePath + "Bricks/BricksBlack.png";
            tex.walls = basePath + "Bricks/BricksRock.png";
            break;
    }

    // Common textures
    tex.damaged = basePath + "Stone/StoneRaw.png";
    tex.construction = basePath + "Wood/WoodCrate2.png";

    return tex;
}

// ============================================================================
// Building Class
// ============================================================================

/**
 * @brief Represents a building in the RTS game
 *
 * Buildings can be:
 * - Housing: Provides living space for workers
 * - Production: Generates resources over time
 * - Defense: Protects against zombie attacks
 * - Special: Utility buildings (trading, healing, storage)
 */
class Building : public Entity {
public:
    using BuildingId = uint32_t;
    static constexpr BuildingId INVALID_BUILDING_ID = 0;

    Building();
    explicit Building(BuildingType type);
    ~Building() override = default;

    // =========================================================================
    // Core Update/Render
    // =========================================================================

    void Update(float deltaTime) override;
    void Render(Nova::Renderer& renderer) override;

    // =========================================================================
    // Building Properties
    // =========================================================================

    /** @brief Get building type */
    [[nodiscard]] BuildingType GetBuildingType() const noexcept { return m_buildingType; }

    /** @brief Get building category */
    [[nodiscard]] BuildingCategory GetCategory() const noexcept {
        return GetBuildingCategory(m_buildingType);
    }

    /** @brief Get building level (1-3) */
    [[nodiscard]] int GetLevel() const noexcept { return m_level; }

    /** @brief Get maximum level */
    [[nodiscard]] int GetMaxLevel() const noexcept { return 3; }

    /** @brief Check if building can be upgraded */
    [[nodiscard]] bool CanUpgrade() const noexcept { return m_level < GetMaxLevel(); }

    /** @brief Get building state */
    [[nodiscard]] BuildingState GetState() const noexcept { return m_state; }

    /** @brief Set building state */
    void SetState(BuildingState state) noexcept;

    /** @brief Check if building is operational */
    [[nodiscard]] bool IsOperational() const noexcept {
        return m_state == BuildingState::Operational;
    }

    /** @brief Check if building is under construction */
    [[nodiscard]] bool IsUnderConstruction() const noexcept {
        return m_state == BuildingState::UnderConstruction ||
               m_state == BuildingState::Blueprint;
    }

    // =========================================================================
    // Construction Progress
    // =========================================================================

    /** @brief Get construction progress (0-100%) */
    [[nodiscard]] float GetConstructionProgress() const noexcept { return m_constructionProgress; }

    /** @brief Add construction progress */
    void AddConstructionProgress(float amount);

    /** @brief Set construction progress directly */
    void SetConstructionProgress(float progress) noexcept;

    /** @brief Check if construction is complete */
    [[nodiscard]] bool IsConstructionComplete() const noexcept {
        return m_constructionProgress >= 100.0f;
    }

    // =========================================================================
    // Worker Management
    // =========================================================================

    /** @brief Get worker capacity (how many can work here) */
    [[nodiscard]] int GetWorkerCapacity() const noexcept { return m_workerCapacity; }

    /** @brief Get housing capacity (how many can live here) */
    [[nodiscard]] int GetHousingCapacity() const noexcept { return m_housingCapacity; }

    /** @brief Get number of assigned workers */
    [[nodiscard]] int GetAssignedWorkerCount() const noexcept {
        return static_cast<int>(m_assignedWorkers.size());
    }

    /** @brief Check if building has room for more workers */
    [[nodiscard]] bool HasWorkerSpace() const noexcept {
        return GetAssignedWorkerCount() < m_workerCapacity;
    }

    /** @brief Assign a worker to this building */
    bool AssignWorker(Worker* worker);

    /** @brief Remove a worker from this building */
    bool RemoveWorker(Worker* worker);

    /** @brief Remove all workers */
    void ClearWorkers();

    /** @brief Get all assigned workers */
    [[nodiscard]] const std::vector<Worker*>& GetAssignedWorkers() const {
        return m_assignedWorkers;
    }

    // =========================================================================
    // Grid Position
    // =========================================================================

    /** @brief Get grid position (tile coordinates) */
    [[nodiscard]] const glm::ivec2& GetGridPosition() const noexcept { return m_gridPosition; }

    /** @brief Set grid position */
    void SetGridPosition(int x, int y) noexcept;
    void SetGridPosition(const glm::ivec2& pos) noexcept { SetGridPosition(pos.x, pos.y); }

    /** @brief Get building size in tiles */
    [[nodiscard]] glm::ivec2 GetSize() const noexcept {
        return GetBuildingSize(m_buildingType);
    }

    /** @brief Get all tiles occupied by this building */
    [[nodiscard]] std::vector<glm::ivec2> GetOccupiedTiles() const;

    /** @brief Check if a tile is within this building's footprint */
    [[nodiscard]] bool OccupiesTile(int x, int y) const;

    // =========================================================================
    // Building-Specific Features
    // =========================================================================

    /** @brief Get wall height for rendering */
    [[nodiscard]] float GetWallHeight() const noexcept { return m_wallHeight; }

    /** @brief Set wall height */
    void SetWallHeight(float height) noexcept { m_wallHeight = height; }

    // Gate-specific
    /** @brief Check if gate is open (Gate building only) */
    [[nodiscard]] bool IsGateOpen() const noexcept { return m_gateOpen; }

    /** @brief Open/close gate */
    void SetGateOpen(bool open) noexcept;

    /** @brief Toggle gate state */
    void ToggleGate() noexcept { SetGateOpen(!m_gateOpen); }

    // Defense-specific
    /** @brief Get attack damage per hit */
    [[nodiscard]] float GetAttackDamage() const noexcept { return m_attackDamage; }

    /** @brief Get attack range */
    [[nodiscard]] float GetAttackRange() const noexcept { return m_attackRange; }

    /** @brief Get attack cooldown */
    [[nodiscard]] float GetAttackCooldown() const noexcept { return m_attackCooldown; }

    /** @brief Check if building can attack */
    [[nodiscard]] bool CanAttack() const noexcept { return m_attackDamage > 0.0f; }

    // Vision
    /** @brief Get vision range */
    [[nodiscard]] float GetVisionRange() const noexcept { return m_visionRange; }

    // =========================================================================
    // Textures
    // =========================================================================

    /** @brief Get building textures */
    [[nodiscard]] const BuildingTextures& GetTextures() const { return m_textures; }

    /** @brief Load textures for this building */
    void LoadTextures(Nova::Renderer& renderer);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using StateChangeCallback = std::function<void(Building&, BuildingState oldState, BuildingState newState)>;
    void SetOnStateChange(StateChangeCallback callback) { m_onStateChange = std::move(callback); }

    using CompletionCallback = std::function<void(Building&)>;
    void SetOnConstructionComplete(CompletionCallback callback) {
        m_onConstructionComplete = std::move(callback);
    }

    using DestroyedCallback = std::function<void(Building&)>;
    void SetOnDestroyed(DestroyedCallback callback) { m_onDestroyed = std::move(callback); }

protected:
    void Die() override;

private:
    void InitializeForType(BuildingType type);
    void UpdateConstruction(float deltaTime);
    void UpdateProduction(float deltaTime);
    void UpdateDefense(float deltaTime);

    // Building identity
    BuildingType m_buildingType = BuildingType::Shelter;
    BuildingState m_state = BuildingState::Blueprint;
    int m_level = 1;

    // Construction
    float m_constructionProgress = 0.0f;

    // Capacity
    int m_workerCapacity = 0;       // How many can work here
    int m_housingCapacity = 0;      // How many can live here

    // Workers
    std::vector<Worker*> m_assignedWorkers;

    // Grid position
    glm::ivec2 m_gridPosition{0, 0};
    float m_wallHeight = 2.0f;

    // Gate state
    bool m_gateOpen = false;

    // Defense stats
    float m_attackDamage = 0.0f;
    float m_attackRange = 0.0f;
    float m_attackCooldown = 1.0f;
    float m_attackTimer = 0.0f;
    Entity* m_currentTarget = nullptr;

    // Vision
    float m_visionRange = 0.0f;

    // Textures
    BuildingTextures m_textures;
    std::shared_ptr<Nova::Texture> m_baseTexture;
    std::shared_ptr<Nova::Texture> m_roofTexture;
    std::shared_ptr<Nova::Texture> m_wallsTexture;

    // Callbacks
    StateChangeCallback m_onStateChange;
    CompletionCallback m_onConstructionComplete;
    DestroyedCallback m_onDestroyed;

    // Building ID generation
    static BuildingId s_nextBuildingId;
    BuildingId m_buildingId = INVALID_BUILDING_ID;
};

// ============================================================================
// Worker Class (Forward declaration implementation hint)
// ============================================================================

/**
 * @brief Worker entity that can be assigned to buildings
 *
 * Workers can:
 * - Construct buildings
 * - Work at production buildings
 * - Guard defensive structures
 * - Live in housing
 */
class Worker : public Entity {
public:
    Worker();
    ~Worker() override = default;

    void Update(float deltaTime) override;
    void Render(Nova::Renderer& renderer) override;

    /** @brief Get worker skill level (affects work speed) */
    [[nodiscard]] float GetSkillLevel() const noexcept { return m_skillLevel; }

    /** @brief Set skill level */
    void SetSkillLevel(float level) noexcept { m_skillLevel = std::clamp(level, 0.1f, 2.0f); }

    /** @brief Get current workplace */
    [[nodiscard]] Building* GetWorkplace() const { return m_workplace; }

    /** @brief Set workplace */
    void SetWorkplace(Building* building) { m_workplace = building; }

    /** @brief Get home building */
    [[nodiscard]] Building* GetHome() const { return m_home; }

    /** @brief Set home building */
    void SetHome(Building* building) { m_home = building; }

    /** @brief Check if worker is idle */
    [[nodiscard]] bool IsIdle() const noexcept { return m_workplace == nullptr; }

    /** @brief Get work speed multiplier */
    [[nodiscard]] float GetWorkSpeed() const noexcept { return m_skillLevel; }

private:
    float m_skillLevel = 1.0f;
    Building* m_workplace = nullptr;
    Building* m_home = nullptr;
};

} // namespace Vehement
