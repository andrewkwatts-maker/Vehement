#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <functional>
#include <cstdint>
#include <limits>

namespace Vehement {
namespace Systems {

/**
 * @brief Cell state for flow field
 */
enum class CellState : uint8_t {
    Walkable = 0,
    Blocked = 1,
    Goal = 2,
    Danger = 3      // High cost area
};

/**
 * @brief Single cell in a flow field
 */
struct FlowCell {
    glm::vec2 direction{0.0f};    // Normalized direction to flow
    float cost = 0.0f;            // Movement cost
    float integration = std::numeric_limits<float>::max(); // Distance to goal
    CellState state = CellState::Walkable;
    bool hasDirection = false;
};

/**
 * @brief Configuration for a flow field
 */
struct FlowFieldConfig {
    int width = 100;              // Grid width in cells
    int height = 100;             // Grid height in cells
    float cellSize = 1.0f;        // World units per cell
    glm::vec2 origin{0.0f};       // World position of grid origin

    // Cost weights
    float baseCost = 1.0f;
    float diagonalCost = 1.414f;  // sqrt(2)
    float dangerCost = 10.0f;
    float blockedCost = 1000.0f;
};

/**
 * @brief Cost function for custom terrain costs
 */
using CostFunction = std::function<float(int x, int y)>;

/**
 * @brief Obstacle query function
 */
using ObstacleFunction = std::function<bool(const glm::vec2& worldPos)>;

// ============================================================================
// Flow Field - Efficient Mass Pathfinding
// ============================================================================

/**
 * @brief Flow field for efficient mass unit pathfinding
 *
 * Features:
 * - Single goal, many units can use
 * - O(cells) computation, then O(1) per-unit query
 * - Smooth steering directions
 * - Dynamic obstacle support
 * - Multiple goals support
 * - Layered fields for different unit types
 */
class FlowField {
public:
    explicit FlowField(const FlowFieldConfig& config);
    ~FlowField() = default;

    // Non-copyable, movable
    FlowField(const FlowField&) = delete;
    FlowField& operator=(const FlowField&) = delete;
    FlowField(FlowField&&) noexcept = default;
    FlowField& operator=(FlowField&&) noexcept = default;

    // =========================================================================
    // Field Generation
    // =========================================================================

    /**
     * @brief Generate flow field toward a single goal
     * @param goalWorld Goal position in world coordinates
     */
    void GenerateToGoal(const glm::vec2& goalWorld);

    /**
     * @brief Generate flow field toward multiple goals
     * @param goalsWorld Goal positions in world coordinates
     */
    void GenerateToGoals(const std::vector<glm::vec2>& goalsWorld);

    /**
     * @brief Generate flow field away from threats (flee field)
     * @param threatWorld Threat position in world coordinates
     * @param radius Threat radius
     */
    void GenerateFleeField(const glm::vec2& threatWorld, float radius);

    /**
     * @brief Partially update field around changed area
     * More efficient than full regeneration for small changes
     */
    void PartialUpdate(const glm::vec2& centerWorld, float radius);

    // =========================================================================
    // Terrain Setup
    // =========================================================================

    /**
     * @brief Set cell state at grid coordinates
     */
    void SetCellState(int x, int y, CellState state);

    /**
     * @brief Set cell state at world position
     */
    void SetCellStateWorld(const glm::vec2& worldPos, CellState state);

    /**
     * @brief Set cell cost at grid coordinates
     */
    void SetCellCost(int x, int y, float cost);

    /**
     * @brief Fill rectangle with state
     */
    void FillRect(int x, int y, int width, int height, CellState state);

    /**
     * @brief Fill circle with state
     */
    void FillCircle(const glm::vec2& centerWorld, float radius, CellState state);

    /**
     * @brief Clear all obstacles
     */
    void ClearObstacles();

    /**
     * @brief Set custom cost function
     */
    void SetCostFunction(CostFunction func);

    /**
     * @brief Set obstacle query function (for dynamic obstacles)
     */
    void SetObstacleFunction(ObstacleFunction func);

    // =========================================================================
    // Flow Queries
    // =========================================================================

    /**
     * @brief Get flow direction at world position
     * @param worldPos Position in world coordinates
     * @return Normalized direction vector
     */
    [[nodiscard]] glm::vec2 GetFlowDirection(const glm::vec2& worldPos) const;

    /**
     * @brief Get flow direction with bilinear interpolation (smoother)
     */
    [[nodiscard]] glm::vec2 GetFlowDirectionSmooth(const glm::vec2& worldPos) const;

    /**
     * @brief Get integration cost at world position
     */
    [[nodiscard]] float GetIntegrationCost(const glm::vec2& worldPos) const;

    /**
     * @brief Check if position is walkable
     */
    [[nodiscard]] bool IsWalkable(const glm::vec2& worldPos) const;

    /**
     * @brief Check if position is at goal
     */
    [[nodiscard]] bool IsAtGoal(const glm::vec2& worldPos, float tolerance = 0.5f) const;

    /**
     * @brief Get cell at grid coordinates
     */
    [[nodiscard]] const FlowCell* GetCell(int x, int y) const;

    // =========================================================================
    // Coordinate Conversion
    // =========================================================================

    /**
     * @brief Convert world position to grid coordinates
     */
    [[nodiscard]] glm::ivec2 WorldToGrid(const glm::vec2& worldPos) const;

    /**
     * @brief Convert grid coordinates to world position (cell center)
     */
    [[nodiscard]] glm::vec2 GridToWorld(int x, int y) const;

    /**
     * @brief Check if grid coordinates are valid
     */
    [[nodiscard]] bool IsValidGrid(int x, int y) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const FlowFieldConfig& GetConfig() const { return m_config; }
    [[nodiscard]] int GetWidth() const { return m_config.width; }
    [[nodiscard]] int GetHeight() const { return m_config.height; }
    [[nodiscard]] float GetCellSize() const { return m_config.cellSize; }

    // =========================================================================
    // Debug/Visualization
    // =========================================================================

    /**
     * @brief Get all cells for visualization
     */
    [[nodiscard]] const std::vector<FlowCell>& GetCells() const { return m_cells; }

    /**
     * @brief Get goal positions
     */
    [[nodiscard]] const std::vector<glm::ivec2>& GetGoals() const { return m_goals; }

private:
    // Internal methods
    [[nodiscard]] size_t GetCellIndex(int x, int y) const;
    void ComputeIntegrationField();
    void ComputeFlowField();
    float GetCost(int x, int y) const;

    FlowFieldConfig m_config;
    std::vector<FlowCell> m_cells;
    std::vector<glm::ivec2> m_goals;
    CostFunction m_costFunc;
    ObstacleFunction m_obstacleFunc;
    bool m_needsUpdate = true;
};

// ============================================================================
// Flow Field Manager - Multiple Fields and Caching
// ============================================================================

/**
 * @brief Manager for multiple flow fields
 *
 * Handles:
 * - Caching flow fields by goal
 * - Layer system for different unit types
 * - Automatic field invalidation
 * - Async field generation
 */
class FlowFieldManager {
public:
    struct Config {
        FlowFieldConfig baseConfig;
        size_t maxCachedFields = 10;
        float cacheExpiration = 30.0f;  // Seconds
        bool asyncGeneration = false;
    };

    explicit FlowFieldManager(const Config& config);
    ~FlowFieldManager() = default;

    // =========================================================================
    // Field Access
    // =========================================================================

    /**
     * @brief Get or create a flow field to a goal
     * @param goal Goal position in world coordinates
     * @return Pointer to flow field (valid until next frame)
     */
    FlowField* GetFieldToGoal(const glm::vec2& goal);

    /**
     * @brief Get or create a flow field to multiple goals
     */
    FlowField* GetFieldToGoals(const std::vector<glm::vec2>& goals);

    /**
     * @brief Get flow direction for a unit
     * @param unitPos Unit's world position
     * @param goal Unit's goal
     * @return Flow direction
     */
    [[nodiscard]] glm::vec2 GetFlowDirection(const glm::vec2& unitPos,
                                              const glm::vec2& goal);

    // =========================================================================
    // Terrain Management
    // =========================================================================

    /**
     * @brief Mark area as blocked (invalidates affected fields)
     */
    void AddObstacle(const glm::vec2& center, float radius);

    /**
     * @brief Remove obstacle (invalidates affected fields)
     */
    void RemoveObstacle(const glm::vec2& center, float radius);

    /**
     * @brief Set cost for an area
     */
    void SetAreaCost(const glm::vec2& center, float radius, float cost);

    /**
     * @brief Clear all dynamic obstacles
     */
    void ClearDynamicObstacles();

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Update manager (call each frame)
     */
    void Update(float currentTime);

    /**
     * @brief Invalidate all cached fields
     */
    void InvalidateAll();

    /**
     * @brief Invalidate fields passing through an area
     */
    void InvalidateArea(const glm::vec2& center, float radius);

    /**
     * @brief Prune expired cache entries
     */
    void PruneExpired(float currentTime);

    // =========================================================================
    // Statistics
    // =========================================================================

    [[nodiscard]] size_t GetCachedFieldCount() const { return m_cache.size(); }
    [[nodiscard]] size_t GetCacheHits() const { return m_cacheHits; }
    [[nodiscard]] size_t GetCacheMisses() const { return m_cacheMisses; }

private:
    struct CachedField {
        std::unique_ptr<FlowField> field;
        std::vector<glm::vec2> goals;
        float timestamp = 0.0f;
        float lastAccess = 0.0f;
    };

    [[nodiscard]] uint64_t MakeGoalKey(const glm::vec2& goal) const;
    [[nodiscard]] uint64_t MakeGoalsKey(const std::vector<glm::vec2>& goals) const;
    void EvictLRU();

    Config m_config;
    std::unordered_map<uint64_t, CachedField> m_cache;

    // Dynamic obstacles
    struct DynamicObstacle {
        glm::vec2 center;
        float radius;
        bool isBlocked;  // vs high cost
        float cost = 0.0f;
    };
    std::vector<DynamicObstacle> m_dynamicObstacles;

    float m_currentTime = 0.0f;
    size_t m_cacheHits = 0;
    size_t m_cacheMisses = 0;
};

// ============================================================================
// Steering Helpers for Flow Fields
// ============================================================================

/**
 * @brief Steering behaviors for flow field following
 */
class FlowFieldSteering {
public:
    struct SteeringParams {
        float maxSpeed = 5.0f;
        float maxForce = 10.0f;
        float arrivalRadius = 2.0f;
        float separationRadius = 1.0f;
        float separationWeight = 1.5f;
        float flowWeight = 1.0f;
        float cohesionWeight = 0.3f;
    };

    /**
     * @brief Calculate steering force from flow field
     * @param currentPos Current position
     * @param currentVel Current velocity
     * @param field Flow field to follow
     * @param params Steering parameters
     * @return Steering force to apply
     */
    static glm::vec2 CalculateSteering(
        const glm::vec2& currentPos,
        const glm::vec2& currentVel,
        const FlowField& field,
        const SteeringParams& params);

    /**
     * @brief Calculate steering with local separation
     * @param currentPos Current position
     * @param currentVel Current velocity
     * @param field Flow field to follow
     * @param neighbors Nearby entity positions
     * @param params Steering parameters
     * @return Steering force to apply
     */
    static glm::vec2 CalculateSteeringWithSeparation(
        const glm::vec2& currentPos,
        const glm::vec2& currentVel,
        const FlowField& field,
        const std::vector<glm::vec2>& neighbors,
        const SteeringParams& params);

    /**
     * @brief Calculate steering for formation movement
     * @param currentPos Current position
     * @param currentVel Current velocity
     * @param field Flow field to follow
     * @param formationOffset Offset from group center
     * @param groupCenter Current group center
     * @param params Steering parameters
     */
    static glm::vec2 CalculateFormationSteering(
        const glm::vec2& currentPos,
        const glm::vec2& currentVel,
        const FlowField& field,
        const glm::vec2& formationOffset,
        const glm::vec2& groupCenter,
        const SteeringParams& params);

private:
    static glm::vec2 Seek(const glm::vec2& position, const glm::vec2& target,
                          const glm::vec2& velocity, float maxSpeed);
    static glm::vec2 Arrive(const glm::vec2& position, const glm::vec2& target,
                            const glm::vec2& velocity, float maxSpeed, float arrivalRadius);
    static glm::vec2 Separate(const glm::vec2& position,
                              const std::vector<glm::vec2>& neighbors,
                              float separationRadius);
};

// ============================================================================
// Formation System
// ============================================================================

/**
 * @brief Formation patterns for group movement
 */
enum class FormationType {
    Line,           // Single file
    Column,         // Double file
    Wedge,          // V-shape
    Circle,         // Circular formation
    Box,            // Square formation
    Scatter         // Random within radius
};

/**
 * @brief Formation offsets calculator
 */
class Formation {
public:
    struct FormationParams {
        FormationType type = FormationType::Wedge;
        float spacing = 2.0f;
        float facing = 0.0f;    // Radians
    };

    /**
     * @brief Calculate formation offsets for a group
     * @param unitCount Number of units
     * @param params Formation parameters
     * @return Vector of offsets from group center
     */
    static std::vector<glm::vec2> CalculateOffsets(size_t unitCount,
                                                    const FormationParams& params);

    /**
     * @brief Update formation facing direction based on movement
     * @param currentFacing Current facing angle (radians)
     * @param movementDirection Direction of movement
     * @param turnSpeed Maximum turn speed (radians per second)
     * @param deltaTime Time step
     * @return New facing angle
     */
    static float UpdateFacing(float currentFacing,
                              const glm::vec2& movementDirection,
                              float turnSpeed,
                              float deltaTime);

private:
    static std::vector<glm::vec2> CalculateLineOffsets(size_t count, float spacing);
    static std::vector<glm::vec2> CalculateColumnOffsets(size_t count, float spacing);
    static std::vector<glm::vec2> CalculateWedgeOffsets(size_t count, float spacing);
    static std::vector<glm::vec2> CalculateCircleOffsets(size_t count, float spacing);
    static std::vector<glm::vec2> CalculateBoxOffsets(size_t count, float spacing);
};

} // namespace Systems
} // namespace Vehement
