#include "FlowField.hpp"
#include <algorithm>
#include <cmath>

namespace Vehement {
namespace Systems {

// ============================================================================
// FlowField Implementation
// ============================================================================

FlowField::FlowField(const FlowFieldConfig& config)
    : m_config(config)
{
    m_cells.resize(config.width * config.height);
}

void FlowField::GenerateToGoal(const glm::vec2& goalWorld) {
    GenerateToGoals({goalWorld});
}

void FlowField::GenerateToGoals(const std::vector<glm::vec2>& goalsWorld) {
    m_goals.clear();

    // Convert goals to grid coordinates
    for (const auto& goal : goalsWorld) {
        glm::ivec2 gridPos = WorldToGrid(goal);
        if (IsValidGrid(gridPos.x, gridPos.y)) {
            m_goals.push_back(gridPos);
        }
    }

    if (m_goals.empty()) return;

    // Reset all cells
    for (auto& cell : m_cells) {
        cell.integration = std::numeric_limits<float>::max();
        cell.direction = glm::vec2(0.0f);
        cell.hasDirection = false;
    }

    // Mark goals
    for (const auto& goal : m_goals) {
        size_t idx = GetCellIndex(goal.x, goal.y);
        m_cells[idx].state = CellState::Goal;
        m_cells[idx].integration = 0.0f;
    }

    // Compute integration field (Dijkstra from goals)
    ComputeIntegrationField();

    // Compute flow directions
    ComputeFlowField();

    m_needsUpdate = false;
}

void FlowField::GenerateFleeField(const glm::vec2& threatWorld, float radius) {
    glm::ivec2 threatGrid = WorldToGrid(threatWorld);

    // Reset cells
    for (auto& cell : m_cells) {
        cell.integration = 0.0f;
        cell.direction = glm::vec2(0.0f);
        cell.hasDirection = false;
    }

    // Set threat as high cost center
    int radiusCells = static_cast<int>(std::ceil(radius / m_config.cellSize));

    for (int dy = -radiusCells; dy <= radiusCells; ++dy) {
        for (int dx = -radiusCells; dx <= radiusCells; ++dx) {
            int x = threatGrid.x + dx;
            int y = threatGrid.y + dy;

            if (!IsValidGrid(x, y)) continue;

            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy)) * m_config.cellSize;
            if (dist <= radius) {
                // Higher integration = want to move away
                float factor = 1.0f - (dist / radius);
                size_t idx = GetCellIndex(x, y);
                m_cells[idx].integration = factor * 100.0f;  // High cost near threat
            }
        }
    }

    // For flee field, we compute directions pointing away from high integration
    for (int y = 0; y < m_config.height; ++y) {
        for (int x = 0; x < m_config.width; ++x) {
            size_t idx = GetCellIndex(x, y);
            FlowCell& cell = m_cells[idx];

            if (cell.state == CellState::Blocked) continue;

            // Find direction of steepest descent (toward lower integration)
            float minIntegration = cell.integration;
            glm::vec2 bestDir(0.0f);

            // Check all 8 neighbors
            const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
            const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

            for (int i = 0; i < 8; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];

                if (!IsValidGrid(nx, ny)) continue;

                const FlowCell& neighbor = m_cells[GetCellIndex(nx, ny)];
                if (neighbor.state == CellState::Blocked) continue;

                // For flee field, we want LOWER integration (away from threat)
                if (neighbor.integration < minIntegration) {
                    minIntegration = neighbor.integration;
                    bestDir = glm::vec2(dx[i], dy[i]);
                }
            }

            if (glm::length(bestDir) > 0.0f) {
                cell.direction = glm::normalize(bestDir);
                cell.hasDirection = true;
            }
        }
    }
}

void FlowField::PartialUpdate(const glm::vec2& centerWorld, float radius) {
    // For now, do full update
    // Could optimize to only update affected area
    if (!m_goals.empty()) {
        std::vector<glm::vec2> goalsWorld;
        for (const auto& goal : m_goals) {
            goalsWorld.push_back(GridToWorld(goal.x, goal.y));
        }
        GenerateToGoals(goalsWorld);
    }
}

void FlowField::SetCellState(int x, int y, CellState state) {
    if (!IsValidGrid(x, y)) return;

    m_cells[GetCellIndex(x, y)].state = state;
    m_needsUpdate = true;
}

void FlowField::SetCellStateWorld(const glm::vec2& worldPos, CellState state) {
    glm::ivec2 grid = WorldToGrid(worldPos);
    SetCellState(grid.x, grid.y, state);
}

void FlowField::SetCellCost(int x, int y, float cost) {
    if (!IsValidGrid(x, y)) return;

    m_cells[GetCellIndex(x, y)].cost = cost;
    m_needsUpdate = true;
}

void FlowField::FillRect(int x, int y, int width, int height, CellState state) {
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            SetCellState(x + dx, y + dy, state);
        }
    }
}

void FlowField::FillCircle(const glm::vec2& centerWorld, float radius, CellState state) {
    glm::ivec2 center = WorldToGrid(centerWorld);
    int radiusCells = static_cast<int>(std::ceil(radius / m_config.cellSize));

    for (int dy = -radiusCells; dy <= radiusCells; ++dy) {
        for (int dx = -radiusCells; dx <= radiusCells; ++dx) {
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy)) * m_config.cellSize;
            if (dist <= radius) {
                SetCellState(center.x + dx, center.y + dy, state);
            }
        }
    }
}

void FlowField::ClearObstacles() {
    for (auto& cell : m_cells) {
        if (cell.state == CellState::Blocked || cell.state == CellState::Danger) {
            cell.state = CellState::Walkable;
        }
        cell.cost = 0.0f;
    }
    m_needsUpdate = true;
}

void FlowField::SetCostFunction(CostFunction func) {
    m_costFunc = func;
    m_needsUpdate = true;
}

void FlowField::SetObstacleFunction(ObstacleFunction func) {
    m_obstacleFunc = func;
}

glm::vec2 FlowField::GetFlowDirection(const glm::vec2& worldPos) const {
    glm::ivec2 grid = WorldToGrid(worldPos);

    if (!IsValidGrid(grid.x, grid.y)) {
        return glm::vec2(0.0f);
    }

    const FlowCell& cell = m_cells[GetCellIndex(grid.x, grid.y)];
    return cell.hasDirection ? cell.direction : glm::vec2(0.0f);
}

glm::vec2 FlowField::GetFlowDirectionSmooth(const glm::vec2& worldPos) const {
    // Get fractional grid position
    glm::vec2 gridPosF = (worldPos - m_config.origin) / m_config.cellSize;
    int x0 = static_cast<int>(std::floor(gridPosF.x));
    int y0 = static_cast<int>(std::floor(gridPosF.y));
    float fx = gridPosF.x - x0;
    float fy = gridPosF.y - y0;

    // Bilinear interpolation of directions
    glm::vec2 dirs[4];
    for (int i = 0; i < 4; ++i) {
        int x = x0 + (i & 1);
        int y = y0 + ((i >> 1) & 1);

        if (IsValidGrid(x, y)) {
            const FlowCell& cell = m_cells[GetCellIndex(x, y)];
            dirs[i] = cell.hasDirection ? cell.direction : glm::vec2(0.0f);
        } else {
            dirs[i] = glm::vec2(0.0f);
        }
    }

    // Interpolate
    glm::vec2 top = glm::mix(dirs[0], dirs[1], fx);
    glm::vec2 bottom = glm::mix(dirs[2], dirs[3], fx);
    glm::vec2 result = glm::mix(top, bottom, fy);

    if (glm::length(result) > 0.001f) {
        return glm::normalize(result);
    }
    return glm::vec2(0.0f);
}

float FlowField::GetIntegrationCost(const glm::vec2& worldPos) const {
    glm::ivec2 grid = WorldToGrid(worldPos);

    if (!IsValidGrid(grid.x, grid.y)) {
        return std::numeric_limits<float>::max();
    }

    return m_cells[GetCellIndex(grid.x, grid.y)].integration;
}

bool FlowField::IsWalkable(const glm::vec2& worldPos) const {
    glm::ivec2 grid = WorldToGrid(worldPos);

    if (!IsValidGrid(grid.x, grid.y)) {
        return false;
    }

    CellState state = m_cells[GetCellIndex(grid.x, grid.y)].state;
    return state != CellState::Blocked;
}

bool FlowField::IsAtGoal(const glm::vec2& worldPos, float tolerance) const {
    for (const auto& goal : m_goals) {
        glm::vec2 goalWorld = GridToWorld(goal.x, goal.y);
        if (glm::distance(worldPos, goalWorld) <= tolerance) {
            return true;
        }
    }
    return false;
}

const FlowCell* FlowField::GetCell(int x, int y) const {
    if (!IsValidGrid(x, y)) return nullptr;
    return &m_cells[GetCellIndex(x, y)];
}

glm::ivec2 FlowField::WorldToGrid(const glm::vec2& worldPos) const {
    glm::vec2 local = worldPos - m_config.origin;
    return glm::ivec2(
        static_cast<int>(std::floor(local.x / m_config.cellSize)),
        static_cast<int>(std::floor(local.y / m_config.cellSize))
    );
}

glm::vec2 FlowField::GridToWorld(int x, int y) const {
    return m_config.origin + glm::vec2(
        (x + 0.5f) * m_config.cellSize,
        (y + 0.5f) * m_config.cellSize
    );
}

bool FlowField::IsValidGrid(int x, int y) const {
    return x >= 0 && x < m_config.width && y >= 0 && y < m_config.height;
}

size_t FlowField::GetCellIndex(int x, int y) const {
    return static_cast<size_t>(y * m_config.width + x);
}

void FlowField::ComputeIntegrationField() {
    // Dijkstra's algorithm from all goals
    using PQEntry = std::pair<float, glm::ivec2>;  // (cost, position)
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> openList;

    // Initialize with goals
    for (const auto& goal : m_goals) {
        openList.push({0.0f, goal});
    }

    // Neighbor offsets (8-directional)
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    const float costs[] = {
        m_config.diagonalCost, m_config.baseCost, m_config.diagonalCost,
        m_config.baseCost, m_config.baseCost,
        m_config.diagonalCost, m_config.baseCost, m_config.diagonalCost
    };

    while (!openList.empty()) {
        auto [currentCost, pos] = openList.top();
        openList.pop();

        size_t currentIdx = GetCellIndex(pos.x, pos.y);

        // Skip if we already found a better path
        if (currentCost > m_cells[currentIdx].integration) {
            continue;
        }

        // Check all neighbors
        for (int i = 0; i < 8; ++i) {
            int nx = pos.x + dx[i];
            int ny = pos.y + dy[i];

            if (!IsValidGrid(nx, ny)) continue;

            size_t neighborIdx = GetCellIndex(nx, ny);
            FlowCell& neighbor = m_cells[neighborIdx];

            if (neighbor.state == CellState::Blocked) continue;

            // Calculate cost to neighbor
            float moveCost = costs[i];
            float cellCost = GetCost(nx, ny);
            float totalCost = currentCost + moveCost + cellCost;

            if (totalCost < neighbor.integration) {
                neighbor.integration = totalCost;
                openList.push({totalCost, glm::ivec2(nx, ny)});
            }
        }
    }
}

void FlowField::ComputeFlowField() {
    // Neighbor offsets (8-directional)
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int y = 0; y < m_config.height; ++y) {
        for (int x = 0; x < m_config.width; ++x) {
            size_t idx = GetCellIndex(x, y);
            FlowCell& cell = m_cells[idx];

            if (cell.state == CellState::Blocked) continue;
            if (cell.state == CellState::Goal) {
                cell.hasDirection = false;
                continue;
            }

            // Find direction of steepest descent
            float minIntegration = cell.integration;
            glm::vec2 bestDir(0.0f);

            for (int i = 0; i < 8; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];

                if (!IsValidGrid(nx, ny)) continue;

                const FlowCell& neighbor = m_cells[GetCellIndex(nx, ny)];
                if (neighbor.state == CellState::Blocked) continue;

                if (neighbor.integration < minIntegration) {
                    minIntegration = neighbor.integration;
                    bestDir = glm::vec2(static_cast<float>(dx[i]),
                                        static_cast<float>(dy[i]));
                }
            }

            if (glm::length(bestDir) > 0.0f) {
                cell.direction = glm::normalize(bestDir);
                cell.hasDirection = true;
            }
        }
    }
}

float FlowField::GetCost(int x, int y) const {
    if (!IsValidGrid(x, y)) return m_config.blockedCost;

    const FlowCell& cell = m_cells[GetCellIndex(x, y)];

    float cost = m_config.baseCost + cell.cost;

    if (cell.state == CellState::Danger) {
        cost += m_config.dangerCost;
    }

    // Custom cost function
    if (m_costFunc) {
        cost += m_costFunc(x, y);
    }

    // Dynamic obstacle check
    if (m_obstacleFunc) {
        glm::vec2 worldPos = GridToWorld(x, y);
        if (m_obstacleFunc(worldPos)) {
            cost += m_config.blockedCost;
        }
    }

    return cost;
}

// ============================================================================
// FlowFieldManager Implementation
// ============================================================================

FlowFieldManager::FlowFieldManager(const Config& config)
    : m_config(config)
{
}

FlowField* FlowFieldManager::GetFieldToGoal(const glm::vec2& goal) {
    return GetFieldToGoals({goal});
}

FlowField* FlowFieldManager::GetFieldToGoals(const std::vector<glm::vec2>& goals) {
    uint64_t key = MakeGoalsKey(goals);

    // Check cache
    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        it->second.lastAccess = m_currentTime;
        ++m_cacheHits;
        return it->second.field.get();
    }

    // Create new field
    ++m_cacheMisses;

    // Evict if at capacity
    while (m_cache.size() >= m_config.maxCachedFields) {
        EvictLRU();
    }

    auto field = std::make_unique<FlowField>(m_config.baseConfig);

    // Apply dynamic obstacles
    for (const auto& obstacle : m_dynamicObstacles) {
        if (obstacle.isBlocked) {
            field->FillCircle(obstacle.center, obstacle.radius, CellState::Blocked);
        } else {
            glm::ivec2 grid = field->WorldToGrid(obstacle.center);
            int radiusCells = static_cast<int>(std::ceil(obstacle.radius / m_config.baseConfig.cellSize));
            for (int dy = -radiusCells; dy <= radiusCells; ++dy) {
                for (int dx = -radiusCells; dx <= radiusCells; ++dx) {
                    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy)) *
                                 m_config.baseConfig.cellSize;
                    if (dist <= obstacle.radius) {
                        field->SetCellCost(grid.x + dx, grid.y + dy, obstacle.cost);
                    }
                }
            }
        }
    }

    // Generate field
    field->GenerateToGoals(goals);

    // Cache
    CachedField cached;
    cached.field = std::move(field);
    cached.goals = goals;
    cached.timestamp = m_currentTime;
    cached.lastAccess = m_currentTime;

    auto [insertedIt, success] = m_cache.emplace(key, std::move(cached));
    return insertedIt->second.field.get();
}

glm::vec2 FlowFieldManager::GetFlowDirection(const glm::vec2& unitPos,
                                              const glm::vec2& goal) {
    FlowField* field = GetFieldToGoal(goal);
    if (field) {
        return field->GetFlowDirectionSmooth(unitPos);
    }
    return glm::vec2(0.0f);
}

void FlowFieldManager::AddObstacle(const glm::vec2& center, float radius) {
    DynamicObstacle obstacle;
    obstacle.center = center;
    obstacle.radius = radius;
    obstacle.isBlocked = true;
    m_dynamicObstacles.push_back(obstacle);

    InvalidateArea(center, radius);
}

void FlowFieldManager::RemoveObstacle(const glm::vec2& center, float radius) {
    m_dynamicObstacles.erase(
        std::remove_if(m_dynamicObstacles.begin(), m_dynamicObstacles.end(),
            [&center, radius](const DynamicObstacle& obs) {
                return glm::distance(obs.center, center) < 0.1f &&
                       std::abs(obs.radius - radius) < 0.1f;
            }),
        m_dynamicObstacles.end()
    );

    InvalidateArea(center, radius);
}

void FlowFieldManager::SetAreaCost(const glm::vec2& center, float radius, float cost) {
    DynamicObstacle obstacle;
    obstacle.center = center;
    obstacle.radius = radius;
    obstacle.isBlocked = false;
    obstacle.cost = cost;
    m_dynamicObstacles.push_back(obstacle);

    InvalidateArea(center, radius);
}

void FlowFieldManager::ClearDynamicObstacles() {
    m_dynamicObstacles.clear();
    InvalidateAll();
}

void FlowFieldManager::Update(float currentTime) {
    m_currentTime = currentTime;

    // Periodically prune expired entries
    static float lastPrune = 0.0f;
    if (currentTime - lastPrune > 5.0f) {
        PruneExpired(currentTime);
        lastPrune = currentTime;
    }
}

void FlowFieldManager::InvalidateAll() {
    m_cache.clear();
}

void FlowFieldManager::InvalidateArea(const glm::vec2& center, float radius) {
    // Invalidate all fields (could optimize to only invalidate affected)
    InvalidateAll();
}

void FlowFieldManager::PruneExpired(float currentTime) {
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (currentTime - it->second.timestamp > m_config.cacheExpiration) {
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t FlowFieldManager::MakeGoalKey(const glm::vec2& goal) const {
    // Quantize position for cache key
    int qx = static_cast<int>(std::round(goal.x * 10.0f));
    int qy = static_cast<int>(std::round(goal.y * 10.0f));
    return (static_cast<uint64_t>(qx) << 32) | static_cast<uint32_t>(qy);
}

uint64_t FlowFieldManager::MakeGoalsKey(const std::vector<glm::vec2>& goals) const {
    if (goals.empty()) return 0;
    if (goals.size() == 1) return MakeGoalKey(goals[0]);

    // Hash multiple goals together
    uint64_t hash = 0;
    for (const auto& goal : goals) {
        hash ^= MakeGoalKey(goal) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

void FlowFieldManager::EvictLRU() {
    if (m_cache.empty()) return;

    auto lru = m_cache.begin();
    float oldestAccess = std::numeric_limits<float>::max();

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.lastAccess < oldestAccess) {
            oldestAccess = it->second.lastAccess;
            lru = it;
        }
    }

    m_cache.erase(lru);
}

// ============================================================================
// FlowFieldSteering Implementation
// ============================================================================

glm::vec2 FlowFieldSteering::CalculateSteering(
    const glm::vec2& currentPos,
    const glm::vec2& currentVel,
    const FlowField& field,
    const SteeringParams& params) {

    // Get desired direction from flow field
    glm::vec2 flowDir = field.GetFlowDirectionSmooth(currentPos);

    if (glm::length(flowDir) < 0.001f) {
        return glm::vec2(0.0f);
    }

    // Check if at goal
    if (field.IsAtGoal(currentPos, params.arrivalRadius)) {
        return Arrive(currentPos, currentPos, currentVel,
                      params.maxSpeed, params.arrivalRadius);
    }

    // Calculate desired velocity
    glm::vec2 desiredVel = flowDir * params.maxSpeed;

    // Steering = desired - current
    glm::vec2 steering = desiredVel - currentVel;

    // Limit steering force
    float steeringMag = glm::length(steering);
    if (steeringMag > params.maxForce) {
        steering = (steering / steeringMag) * params.maxForce;
    }

    return steering * params.flowWeight;
}

glm::vec2 FlowFieldSteering::CalculateSteeringWithSeparation(
    const glm::vec2& currentPos,
    const glm::vec2& currentVel,
    const FlowField& field,
    const std::vector<glm::vec2>& neighbors,
    const SteeringParams& params) {

    glm::vec2 flowSteering = CalculateSteering(currentPos, currentVel, field, params);
    glm::vec2 separationSteering = Separate(currentPos, neighbors, params.separationRadius);

    return flowSteering + separationSteering * params.separationWeight;
}

glm::vec2 FlowFieldSteering::CalculateFormationSteering(
    const glm::vec2& currentPos,
    const glm::vec2& currentVel,
    const FlowField& field,
    const glm::vec2& formationOffset,
    const glm::vec2& groupCenter,
    const SteeringParams& params) {

    // Calculate formation position
    glm::vec2 formationPos = groupCenter + formationOffset;

    // Get flow direction at formation position
    glm::vec2 flowDir = field.GetFlowDirectionSmooth(formationPos);

    // Blend between formation keeping and flow following
    glm::vec2 toFormation = formationPos - currentPos;
    float distToFormation = glm::length(toFormation);

    glm::vec2 steering;
    if (distToFormation > params.separationRadius * 2.0f) {
        // Too far from formation, seek formation position
        steering = Seek(currentPos, formationPos, currentVel, params.maxSpeed);
    } else {
        // Follow flow while maintaining formation
        glm::vec2 desiredVel = flowDir * params.maxSpeed;
        steering = desiredVel - currentVel;
    }

    // Limit steering force
    float steeringMag = glm::length(steering);
    if (steeringMag > params.maxForce) {
        steering = (steering / steeringMag) * params.maxForce;
    }

    return steering;
}

glm::vec2 FlowFieldSteering::Seek(const glm::vec2& position, const glm::vec2& target,
                                   const glm::vec2& velocity, float maxSpeed) {
    glm::vec2 desired = target - position;
    float dist = glm::length(desired);

    if (dist > 0.001f) {
        desired = (desired / dist) * maxSpeed;
    }

    return desired - velocity;
}

glm::vec2 FlowFieldSteering::Arrive(const glm::vec2& position, const glm::vec2& target,
                                     const glm::vec2& velocity, float maxSpeed,
                                     float arrivalRadius) {
    glm::vec2 desired = target - position;
    float dist = glm::length(desired);

    if (dist > 0.001f) {
        float speed = maxSpeed;
        if (dist < arrivalRadius) {
            speed = maxSpeed * (dist / arrivalRadius);
        }
        desired = (desired / dist) * speed;
    }

    return desired - velocity;
}

glm::vec2 FlowFieldSteering::Separate(const glm::vec2& position,
                                       const std::vector<glm::vec2>& neighbors,
                                       float separationRadius) {
    glm::vec2 steering(0.0f);
    int count = 0;

    for (const auto& neighbor : neighbors) {
        glm::vec2 diff = position - neighbor;
        float dist = glm::length(diff);

        if (dist > 0.001f && dist < separationRadius) {
            // Weight by inverse distance
            steering += glm::normalize(diff) / dist;
            ++count;
        }
    }

    if (count > 0) {
        steering /= static_cast<float>(count);
    }

    return steering;
}

// ============================================================================
// Formation Implementation
// ============================================================================

std::vector<glm::vec2> Formation::CalculateOffsets(size_t unitCount,
                                                    const FormationParams& params) {
    std::vector<glm::vec2> offsets;

    switch (params.type) {
        case FormationType::Line:
            offsets = CalculateLineOffsets(unitCount, params.spacing);
            break;
        case FormationType::Column:
            offsets = CalculateColumnOffsets(unitCount, params.spacing);
            break;
        case FormationType::Wedge:
            offsets = CalculateWedgeOffsets(unitCount, params.spacing);
            break;
        case FormationType::Circle:
            offsets = CalculateCircleOffsets(unitCount, params.spacing);
            break;
        case FormationType::Box:
            offsets = CalculateBoxOffsets(unitCount, params.spacing);
            break;
        case FormationType::Scatter:
        default:
            offsets.resize(unitCount, glm::vec2(0.0f));
            break;
    }

    // Rotate all offsets by facing direction
    float cosF = std::cos(params.facing);
    float sinF = std::sin(params.facing);

    for (auto& offset : offsets) {
        glm::vec2 rotated(
            offset.x * cosF - offset.y * sinF,
            offset.x * sinF + offset.y * cosF
        );
        offset = rotated;
    }

    return offsets;
}

float Formation::UpdateFacing(float currentFacing,
                               const glm::vec2& movementDirection,
                               float turnSpeed,
                               float deltaTime) {
    if (glm::length(movementDirection) < 0.001f) {
        return currentFacing;
    }

    float targetFacing = std::atan2(movementDirection.y, movementDirection.x);

    // Calculate shortest rotation
    float diff = targetFacing - currentFacing;
    while (diff > 3.14159f) diff -= 6.28318f;
    while (diff < -3.14159f) diff += 6.28318f;

    // Apply turn speed limit
    float maxTurn = turnSpeed * deltaTime;
    if (std::abs(diff) > maxTurn) {
        diff = (diff > 0.0f) ? maxTurn : -maxTurn;
    }

    return currentFacing + diff;
}

std::vector<glm::vec2> Formation::CalculateLineOffsets(size_t count, float spacing) {
    std::vector<glm::vec2> offsets(count);

    float startX = -static_cast<float>(count - 1) * spacing * 0.5f;
    for (size_t i = 0; i < count; ++i) {
        offsets[i] = glm::vec2(startX + i * spacing, 0.0f);
    }

    return offsets;
}

std::vector<glm::vec2> Formation::CalculateColumnOffsets(size_t count, float spacing) {
    std::vector<glm::vec2> offsets(count);

    size_t rows = (count + 1) / 2;
    float startY = -static_cast<float>(rows - 1) * spacing * 0.5f;

    for (size_t i = 0; i < count; ++i) {
        size_t row = i / 2;
        bool leftSide = (i % 2 == 0);

        float x = leftSide ? -spacing * 0.5f : spacing * 0.5f;
        float y = startY + row * spacing;

        offsets[i] = glm::vec2(x, y);
    }

    return offsets;
}

std::vector<glm::vec2> Formation::CalculateWedgeOffsets(size_t count, float spacing) {
    std::vector<glm::vec2> offsets(count);

    if (count > 0) {
        offsets[0] = glm::vec2(0.0f, 0.0f);  // Leader at front
    }

    for (size_t i = 1; i < count; ++i) {
        size_t row = (i + 1) / 2;
        bool leftSide = (i % 2 == 1);

        float x = leftSide ? -row * spacing : row * spacing;
        float y = -row * spacing;  // Behind the leader

        offsets[i] = glm::vec2(x, y);
    }

    return offsets;
}

std::vector<glm::vec2> Formation::CalculateCircleOffsets(size_t count, float spacing) {
    std::vector<glm::vec2> offsets(count);

    float radius = count * spacing / (2.0f * 3.14159f);
    radius = std::max(radius, spacing);

    for (size_t i = 0; i < count; ++i) {
        float angle = static_cast<float>(i) / count * 2.0f * 3.14159f;
        offsets[i] = glm::vec2(std::cos(angle), std::sin(angle)) * radius;
    }

    return offsets;
}

std::vector<glm::vec2> Formation::CalculateBoxOffsets(size_t count, float spacing) {
    std::vector<glm::vec2> offsets(count);

    size_t side = static_cast<size_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    float startX = -static_cast<float>(side - 1) * spacing * 0.5f;
    float startY = -static_cast<float>(side - 1) * spacing * 0.5f;

    for (size_t i = 0; i < count; ++i) {
        size_t row = i / side;
        size_t col = i % side;

        offsets[i] = glm::vec2(startX + col * spacing, startY + row * spacing);
    }

    return offsets;
}

} // namespace Systems
} // namespace Vehement
