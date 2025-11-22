#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

// ============================================================================
// NavMesh Types
// ============================================================================

/**
 * @brief Navigation polygon (convex)
 */
struct NavPolygon {
    uint32_t id;
    std::vector<glm::vec3> vertices;
    glm::vec3 center;
    std::vector<uint32_t> neighbors;   ///< Adjacent polygon IDs
    std::vector<uint32_t> edges;       ///< Edge indices for neighbor connections
    float area;
    uint16_t flags;                    ///< Area type flags (walkable, water, etc.)
    float cost;                        ///< Traversal cost multiplier

    [[nodiscard]] bool Contains(const glm::vec3& point) const;
    [[nodiscard]] float GetHeight(const glm::vec3& point) const;
};

/**
 * @brief Off-mesh link (jump, ladder, teleport)
 */
struct OffMeshLink {
    uint32_t id;
    glm::vec3 startPos;
    glm::vec3 endPos;
    uint32_t startPoly;
    uint32_t endPoly;
    float radius;
    bool bidirectional;
    uint16_t flags;                    ///< Type flags (jump, climb, etc.)
    float cost;                        ///< Custom cost for this link
};

/**
 * @brief Dynamic obstacle for navmesh carving
 */
struct NavObstacle {
    uint32_t id;
    glm::vec3 position;
    glm::vec3 halfExtents;             ///< For box obstacles
    float radius;                      ///< For cylinder obstacles
    float height;
    bool isBox;
    bool carving;                      ///< Whether to carve into navmesh
    std::vector<uint32_t> affectedPolys;
};

/**
 * @brief Path point with metadata
 */
struct PathPoint {
    glm::vec3 position;
    uint32_t polyId;
    uint16_t flags;
    float cost;                        ///< Cost to reach this point
};

/**
 * @brief Navigation path result
 */
struct NavPath {
    std::vector<PathPoint> points;
    float totalCost;
    bool complete;                     ///< True if path reaches destination

    [[nodiscard]] bool IsValid() const { return !points.empty(); }
    [[nodiscard]] size_t GetPointCount() const { return points.size(); }
    [[nodiscard]] float GetLength() const;

    /**
     * @brief Get smoothed path for steering
     */
    [[nodiscard]] std::vector<glm::vec3> GetSmoothedPath(float maxDeviation = 0.5f) const;
};

/**
 * @brief Area type flags
 */
namespace NavAreaFlags {
    constexpr uint16_t Walkable = 1 << 0;
    constexpr uint16_t Water = 1 << 1;
    constexpr uint16_t Road = 1 << 2;
    constexpr uint16_t Grass = 1 << 3;
    constexpr uint16_t Door = 1 << 4;
    constexpr uint16_t Jump = 1 << 5;
    constexpr uint16_t Disabled = 1 << 15;

    constexpr uint16_t All = 0xFFFF;
    constexpr uint16_t Default = Walkable | Road | Grass;
}

// ============================================================================
// NavMesh Query Filter
// ============================================================================

/**
 * @brief Filter for pathfinding queries
 */
struct NavQueryFilter {
    uint16_t includeMask = NavAreaFlags::All;
    uint16_t excludeMask = NavAreaFlags::Disabled;
    std::unordered_map<uint16_t, float> areaCosts;

    NavQueryFilter() {
        areaCosts[NavAreaFlags::Walkable] = 1.0f;
        areaCosts[NavAreaFlags::Water] = 10.0f;
        areaCosts[NavAreaFlags::Road] = 0.5f;
        areaCosts[NavAreaFlags::Grass] = 1.0f;
    }

    [[nodiscard]] bool PassFilter(uint16_t flags) const {
        return (flags & includeMask) && !(flags & excludeMask);
    }

    [[nodiscard]] float GetCost(uint16_t flags) const {
        float cost = 1.0f;
        for (const auto& [flag, c] : areaCosts) {
            if (flags & flag) cost *= c;
        }
        return cost;
    }
};

// ============================================================================
// NavMesh Generation Settings
// ============================================================================

/**
 * @brief Settings for navmesh generation
 */
struct NavMeshBuildSettings {
    // Agent properties
    float agentHeight = 2.0f;          ///< Agent height
    float agentRadius = 0.5f;          ///< Agent radius
    float agentMaxClimb = 0.4f;        ///< Max step height
    float agentMaxSlope = 45.0f;       ///< Max walkable slope

    // Rasterization
    float cellSize = 0.3f;             ///< Voxel cell size
    float cellHeight = 0.2f;           ///< Voxel cell height

    // Region
    int regionMinSize = 8;             ///< Min region area
    int regionMergeSize = 20;          ///< Region merge size

    // Polygon mesh
    float edgeMaxLen = 12.0f;          ///< Max edge length
    float edgeMaxError = 1.3f;         ///< Max edge error
    int vertsPerPoly = 6;              ///< Vertices per polygon

    // Detail mesh
    float detailSampleDist = 6.0f;
    float detailSampleMaxError = 1.0f;

    // Tile settings
    int tileSize = 48;                 ///< Tile size in cells
    bool buildBVTree = true;           ///< Build BV tree for faster queries
};

// ============================================================================
// Crowd Simulation
// ============================================================================

/**
 * @brief Crowd agent state
 */
enum class CrowdAgentState : uint8_t {
    Invalid,
    Walking,
    OffMesh,
    Waiting,
    Arrived
};

/**
 * @brief Crowd agent parameters
 */
struct CrowdAgentParams {
    float radius = 0.5f;
    float height = 2.0f;
    float maxAcceleration = 8.0f;
    float maxSpeed = 3.5f;
    float collisionQueryRange = 12.0f;
    float pathOptimizationRange = 30.0f;
    float separationWeight = 2.0f;
    uint16_t queryFilterMask = NavAreaFlags::Default;
    uint8_t avoidanceQuality = 3;      ///< 0-3, higher = better avoidance
    bool anticipateTurns = true;
};

/**
 * @brief Crowd agent
 */
struct CrowdAgent {
    uint32_t id;
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 targetPos;
    CrowdAgentParams params;
    CrowdAgentState state = CrowdAgentState::Invalid;
    NavPath path;
    size_t pathIndex = 0;

    [[nodiscard]] bool HasTarget() const { return state != CrowdAgentState::Invalid; }
    [[nodiscard]] bool HasArrived() const { return state == CrowdAgentState::Arrived; }
};

// ============================================================================
// NavMesh Class
// ============================================================================

/**
 * @brief Navigation mesh for AI pathfinding
 *
 * Features:
 * - Polygon-based navigation mesh
 * - A* pathfinding with area costs
 * - Dynamic obstacles with carving
 * - Off-mesh links (jumps, ladders)
 * - Crowd simulation with avoidance
 * - Runtime generation and updates
 *
 * Usage:
 * @code
 * NavMesh navmesh;
 * navmesh.Build(vertices, indices, settings);
 *
 * // Find path
 * NavPath path = navmesh.FindPath(startPos, endPos);
 *
 * // Use crowd for multiple agents
 * uint32_t agentId = navmesh.AddCrowdAgent(position, params);
 * navmesh.SetAgentTarget(agentId, targetPos);
 * navmesh.UpdateCrowd(deltaTime);
 * @endcode
 */
class NavMesh {
public:
    NavMesh() = default;
    ~NavMesh() = default;

    // =========== Building ===========

    /**
     * @brief Build navmesh from geometry
     */
    bool Build(const std::vector<glm::vec3>& vertices,
               const std::vector<uint32_t>& indices,
               const NavMeshBuildSettings& settings = {});

    /**
     * @brief Build from height field
     */
    bool BuildFromHeightfield(const std::vector<float>& heights,
                               int width, int height,
                               const glm::vec3& origin,
                               float cellSize,
                               const NavMeshBuildSettings& settings = {});

    /**
     * @brief Clear navmesh
     */
    void Clear();

    /**
     * @brief Save to file
     */
    bool Save(const std::string& path) const;

    /**
     * @brief Load from file
     */
    bool Load(const std::string& path);

    // =========== Queries ===========

    /**
     * @brief Find nearest point on navmesh
     */
    [[nodiscard]] glm::vec3 FindNearestPoint(const glm::vec3& pos, float radius = 5.0f) const;

    /**
     * @brief Find containing polygon
     */
    [[nodiscard]] uint32_t FindNearestPoly(const glm::vec3& pos, float radius = 5.0f) const;

    /**
     * @brief Check if point is on navmesh
     */
    [[nodiscard]] bool IsOnNavMesh(const glm::vec3& pos, float radius = 0.5f) const;

    /**
     * @brief Get height at position
     */
    [[nodiscard]] float GetHeight(const glm::vec3& pos) const;

    /**
     * @brief Raycast on navmesh
     */
    [[nodiscard]] bool Raycast(const glm::vec3& start, const glm::vec3& end,
                                glm::vec3& hitPoint, uint32_t& hitPoly) const;

    // =========== Pathfinding ===========

    /**
     * @brief Find path between two points
     */
    [[nodiscard]] NavPath FindPath(const glm::vec3& start, const glm::vec3& end,
                                    const NavQueryFilter& filter = {}) const;

    /**
     * @brief Find path asynchronously
     */
    void FindPathAsync(const glm::vec3& start, const glm::vec3& end,
                       std::function<void(NavPath)> callback,
                       const NavQueryFilter& filter = {});

    /**
     * @brief Get random point on navmesh
     */
    [[nodiscard]] glm::vec3 GetRandomPoint(const NavQueryFilter& filter = {}) const;

    /**
     * @brief Get random point in radius
     */
    [[nodiscard]] glm::vec3 GetRandomPointInRadius(const glm::vec3& center, float radius,
                                                    const NavQueryFilter& filter = {}) const;

    // =========== Dynamic Obstacles ===========

    /**
     * @brief Add a dynamic obstacle
     */
    uint32_t AddObstacle(const glm::vec3& pos, float radius, float height);

    /**
     * @brief Add a box obstacle
     */
    uint32_t AddBoxObstacle(const glm::vec3& pos, const glm::vec3& halfExtents);

    /**
     * @brief Remove an obstacle
     */
    void RemoveObstacle(uint32_t id);

    /**
     * @brief Update obstacle position
     */
    void UpdateObstacle(uint32_t id, const glm::vec3& pos);

    /**
     * @brief Process obstacle changes
     */
    void UpdateObstacles();

    // =========== Off-Mesh Links ===========

    /**
     * @brief Add an off-mesh link
     */
    uint32_t AddOffMeshLink(const glm::vec3& start, const glm::vec3& end,
                             float radius = 0.5f, bool bidirectional = true,
                             uint16_t flags = NavAreaFlags::Jump);

    /**
     * @brief Remove an off-mesh link
     */
    void RemoveOffMeshLink(uint32_t id);

    /**
     * @brief Set link enabled/disabled
     */
    void SetOffMeshLinkEnabled(uint32_t id, bool enabled);

    // =========== Crowd Simulation ===========

    /**
     * @brief Initialize crowd simulation
     */
    void InitCrowd(int maxAgents = 100);

    /**
     * @brief Add agent to crowd
     */
    uint32_t AddCrowdAgent(const glm::vec3& pos, const CrowdAgentParams& params = {});

    /**
     * @brief Remove agent from crowd
     */
    void RemoveCrowdAgent(uint32_t id);

    /**
     * @brief Set agent target position
     */
    void SetAgentTarget(uint32_t id, const glm::vec3& target);

    /**
     * @brief Stop agent
     */
    void StopAgent(uint32_t id);

    /**
     * @brief Update agent parameters
     */
    void UpdateAgentParams(uint32_t id, const CrowdAgentParams& params);

    /**
     * @brief Get agent data
     */
    [[nodiscard]] const CrowdAgent* GetAgent(uint32_t id) const;

    /**
     * @brief Get all agents
     */
    [[nodiscard]] const std::vector<CrowdAgent>& GetAgents() const { return m_agents; }

    /**
     * @brief Update crowd simulation
     */
    void UpdateCrowd(float deltaTime);

    // =========== Debug ===========

    /**
     * @brief Get polygons for debug rendering
     */
    [[nodiscard]] const std::vector<NavPolygon>& GetPolygons() const { return m_polygons; }

    /**
     * @brief Get off-mesh links
     */
    [[nodiscard]] const std::vector<OffMeshLink>& GetOffMeshLinks() const { return m_offMeshLinks; }

    /**
     * @brief Get obstacles
     */
    [[nodiscard]] const std::vector<NavObstacle>& GetObstacles() const { return m_obstacles; }

    // =========== Statistics ===========

    [[nodiscard]] size_t GetPolyCount() const { return m_polygons.size(); }
    [[nodiscard]] size_t GetVertexCount() const { return m_vertices.size(); }
    [[nodiscard]] size_t GetAgentCount() const { return m_agents.size(); }
    [[nodiscard]] bool IsBuilt() const { return !m_polygons.empty(); }

private:
    struct AStarNode {
        uint32_t polyId;
        float gCost;
        float fCost;
        uint32_t parent;

        bool operator>(const AStarNode& other) const { return fCost > other.fCost; }
    };

    float HeuristicCost(const glm::vec3& a, const glm::vec3& b) const;
    std::vector<uint32_t> ReconstructPath(const std::unordered_map<uint32_t, uint32_t>& cameFrom,
                                           uint32_t current) const;
    glm::vec3 StringPull(const std::vector<uint32_t>& polyPath,
                         const glm::vec3& start, const glm::vec3& end) const;
    void UpdateAgentVelocity(CrowdAgent& agent, float deltaTime);
    glm::vec3 ComputeAvoidance(const CrowdAgent& agent) const;

    std::vector<NavPolygon> m_polygons;
    std::vector<glm::vec3> m_vertices;
    std::vector<OffMeshLink> m_offMeshLinks;
    std::vector<NavObstacle> m_obstacles;
    std::vector<CrowdAgent> m_agents;

    NavMeshBuildSettings m_settings;
    uint32_t m_nextObstacleId = 1;
    uint32_t m_nextLinkId = 1;
    uint32_t m_nextAgentId = 1;
    int m_maxAgents = 100;
};

// ============================================================================
// Implementation Snippets
// ============================================================================

inline bool NavPolygon::Contains(const glm::vec3& point) const {
    // Point in polygon test (2D, XZ plane)
    size_t n = vertices.size();
    bool inside = false;

    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const auto& vi = vertices[i];
        const auto& vj = vertices[j];

        if ((vi.z > point.z) != (vj.z > point.z) &&
            point.x < (vj.x - vi.x) * (point.z - vi.z) / (vj.z - vi.z) + vi.x) {
            inside = !inside;
        }
    }

    return inside;
}

inline float NavPolygon::GetHeight(const glm::vec3& point) const {
    if (vertices.size() < 3) return point.y;

    // Interpolate height using barycentric coordinates
    const auto& v0 = vertices[0];
    const auto& v1 = vertices[1];
    const auto& v2 = vertices[2];

    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 ep = point - v0;

    float d11 = e1.x * e1.x + e1.z * e1.z;
    float d12 = e1.x * e2.x + e1.z * e2.z;
    float d22 = e2.x * e2.x + e2.z * e2.z;
    float dp1 = ep.x * e1.x + ep.z * e1.z;
    float dp2 = ep.x * e2.x + ep.z * e2.z;

    float denom = d11 * d22 - d12 * d12;
    if (std::abs(denom) < 1e-6f) return center.y;

    float u = (d22 * dp1 - d12 * dp2) / denom;
    float v = (d11 * dp2 - d12 * dp1) / denom;

    return v0.y + u * e1.y + v * e2.y;
}

inline float NavPath::GetLength() const {
    float len = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        len += glm::length(points[i].position - points[i-1].position);
    }
    return len;
}

inline std::vector<glm::vec3> NavPath::GetSmoothedPath(float /*maxDeviation*/) const {
    std::vector<glm::vec3> smoothed;
    smoothed.reserve(points.size());
    for (const auto& p : points) {
        smoothed.push_back(p.position);
    }
    // String pulling / funnel algorithm would go here
    return smoothed;
}

inline NavPath NavMesh::FindPath(const glm::vec3& start, const glm::vec3& end,
                                  const NavQueryFilter& filter) const {
    NavPath result;
    result.complete = false;
    result.totalCost = 0.0f;

    uint32_t startPoly = FindNearestPoly(start);
    uint32_t endPoly = FindNearestPoly(end);

    if (startPoly == UINT32_MAX || endPoly == UINT32_MAX) {
        return result;
    }

    // A* search
    std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<>> openSet;
    std::unordered_set<uint32_t> closedSet;
    std::unordered_map<uint32_t, uint32_t> cameFrom;
    std::unordered_map<uint32_t, float> gScore;

    openSet.push({startPoly, 0.0f, HeuristicCost(m_polygons[startPoly].center, end), UINT32_MAX});
    gScore[startPoly] = 0.0f;

    while (!openSet.empty()) {
        AStarNode current = openSet.top();
        openSet.pop();

        if (current.polyId == endPoly) {
            // Reconstruct path
            auto polyPath = ReconstructPath(cameFrom, current.polyId);

            for (uint32_t polyId : polyPath) {
                PathPoint pp;
                pp.position = m_polygons[polyId].center;
                pp.polyId = polyId;
                pp.flags = m_polygons[polyId].flags;
                result.points.push_back(pp);
            }

            result.points.front().position = start;
            result.points.back().position = end;
            result.totalCost = current.gCost;
            result.complete = true;
            return result;
        }

        if (closedSet.count(current.polyId)) continue;
        closedSet.insert(current.polyId);

        const NavPolygon& poly = m_polygons[current.polyId];

        for (uint32_t neighborId : poly.neighbors) {
            if (closedSet.count(neighborId)) continue;

            const NavPolygon& neighbor = m_polygons[neighborId];
            if (!filter.PassFilter(neighbor.flags)) continue;

            float tentativeG = current.gCost +
                glm::length(neighbor.center - poly.center) *
                filter.GetCost(neighbor.flags) * neighbor.cost;

            if (gScore.find(neighborId) == gScore.end() || tentativeG < gScore[neighborId]) {
                cameFrom[neighborId] = current.polyId;
                gScore[neighborId] = tentativeG;

                float f = tentativeG + HeuristicCost(neighbor.center, end);
                openSet.push({neighborId, tentativeG, f, current.polyId});
            }
        }
    }

    return result;  // No path found
}

inline float NavMesh::HeuristicCost(const glm::vec3& a, const glm::vec3& b) const {
    return glm::length(b - a);
}

inline std::vector<uint32_t> NavMesh::ReconstructPath(const std::unordered_map<uint32_t, uint32_t>& cameFrom,
                                                       uint32_t current) const {
    std::vector<uint32_t> path;
    while (cameFrom.find(current) != cameFrom.end()) {
        path.push_back(current);
        current = cameFrom.at(current);
    }
    path.push_back(current);
    std::reverse(path.begin(), path.end());
    return path;
}

inline uint32_t NavMesh::FindNearestPoly(const glm::vec3& pos, float radius) const {
    uint32_t nearest = UINT32_MAX;
    float nearestDist = radius * radius;

    for (const auto& poly : m_polygons) {
        float dist = glm::length(poly.center - pos);
        if (dist < nearestDist && poly.Contains(pos)) {
            nearest = poly.id;
            nearestDist = dist;
        }
    }

    return nearest;
}

inline glm::vec3 NavMesh::FindNearestPoint(const glm::vec3& pos, float radius) const {
    uint32_t poly = FindNearestPoly(pos, radius);
    if (poly == UINT32_MAX) return pos;

    glm::vec3 result = pos;
    result.y = m_polygons[poly].GetHeight(pos);
    return result;
}

inline bool NavMesh::IsOnNavMesh(const glm::vec3& pos, float radius) const {
    return FindNearestPoly(pos, radius) != UINT32_MAX;
}

inline void NavMesh::UpdateCrowd(float deltaTime) {
    for (auto& agent : m_agents) {
        if (agent.state == CrowdAgentState::Invalid ||
            agent.state == CrowdAgentState::Arrived) continue;

        UpdateAgentVelocity(agent, deltaTime);

        // Move agent
        agent.position += agent.velocity * deltaTime;

        // Check if arrived
        if (agent.path.IsValid() && agent.pathIndex < agent.path.points.size()) {
            float dist = glm::length(agent.position - agent.path.points[agent.pathIndex].position);
            if (dist < agent.params.radius) {
                ++agent.pathIndex;
                if (agent.pathIndex >= agent.path.points.size()) {
                    agent.state = CrowdAgentState::Arrived;
                    agent.velocity = glm::vec3(0.0f);
                }
            }
        }
    }
}

inline void NavMesh::UpdateAgentVelocity(CrowdAgent& agent, float deltaTime) {
    if (!agent.path.IsValid() || agent.pathIndex >= agent.path.points.size()) {
        agent.velocity *= 0.9f;
        return;
    }

    glm::vec3 target = agent.path.points[agent.pathIndex].position;
    glm::vec3 toTarget = target - agent.position;
    toTarget.y = 0;

    float dist = glm::length(toTarget);
    if (dist < 0.01f) return;

    glm::vec3 desiredVel = glm::normalize(toTarget) * agent.params.maxSpeed;

    // Apply avoidance
    glm::vec3 avoidance = ComputeAvoidance(agent);
    desiredVel += avoidance * agent.params.separationWeight;

    // Clamp speed
    float speed = glm::length(desiredVel);
    if (speed > agent.params.maxSpeed) {
        desiredVel = desiredVel / speed * agent.params.maxSpeed;
    }

    // Smooth velocity change
    glm::vec3 velDiff = desiredVel - agent.velocity;
    float maxAccel = agent.params.maxAcceleration * deltaTime;
    if (glm::length(velDiff) > maxAccel) {
        velDiff = glm::normalize(velDiff) * maxAccel;
    }

    agent.velocity += velDiff;
}

inline glm::vec3 NavMesh::ComputeAvoidance(const CrowdAgent& agent) const {
    glm::vec3 avoidance(0.0f);

    for (const auto& other : m_agents) {
        if (other.id == agent.id) continue;

        glm::vec3 toOther = other.position - agent.position;
        toOther.y = 0;
        float dist = glm::length(toOther);

        float minDist = agent.params.radius + other.params.radius + 0.5f;
        if (dist < minDist && dist > 0.01f) {
            float strength = (minDist - dist) / minDist;
            avoidance -= glm::normalize(toOther) * strength;
        }
    }

    return avoidance;
}

} // namespace Nova
