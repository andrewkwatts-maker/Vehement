#include "pathfinding/Graph.hpp"
#include "math/Random.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace Nova {

// PathNode implementation
bool PathNode::HasNeighbor(int neighborId) const noexcept {
    return std::find(neighbors.begin(), neighbors.end(), neighborId) != neighbors.end();
}

// Graph implementation
Graph::Graph(const SpatialHashConfig& spatialConfig)
    : m_spatialConfig(spatialConfig) {
}

int Graph::AddNode(const glm::vec3& position) {
    return AddNode(position, 1.0f);
}

int Graph::AddNode(const glm::vec3& position, float traversalCost) {
    int id = m_nextId++;
    PathNode node;
    node.id = id;
    node.position = position;
    node.traversalCost = traversalCost;
    node.walkable = true;
    m_nodes[id] = std::move(node);

    if (m_spatialConfig.enabled) {
        AddToSpatialHash(id, position);
    }

    return id;
}

void Graph::RemoveNode(int nodeId) {
    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return;
    }

    // Remove from spatial hash before removing node
    if (m_spatialConfig.enabled) {
        RemoveFromSpatialHash(nodeId, it->second.position);
    }

    // Remove all edges to/from this node
    for (auto& [id, node] : m_nodes) {
        auto neighborIt = std::find(node.neighbors.begin(), node.neighbors.end(), nodeId);
        if (neighborIt != node.neighbors.end()) {
            node.neighbors.erase(neighborIt);
        }
    }

    m_nodes.erase(nodeId);
    m_weights.erase(nodeId);

    for (auto& [from, edges] : m_weights) {
        edges.erase(nodeId);
    }
}

void Graph::AddEdge(int from, int to, float weight) {
    auto fromIt = m_nodes.find(from);
    auto toIt = m_nodes.find(to);

    if (fromIt == m_nodes.end() || toIt == m_nodes.end()) {
        return;
    }

    // Add to neighbors if not already present
    auto& neighbors = fromIt->second.neighbors;
    if (std::find(neighbors.begin(), neighbors.end(), to) == neighbors.end()) {
        neighbors.push_back(to);
    }

    // Calculate weight if not provided
    if (weight < 0.0f) {
        weight = glm::distance(fromIt->second.position, toIt->second.position);
    }

    // Factor in destination node's traversal cost
    weight *= toIt->second.traversalCost;

    m_weights[from][to] = weight;
}

void Graph::AddBidirectionalEdge(int nodeA, int nodeB, float weight) {
    // Calculate base weight once if auto-calculating
    if (weight < 0.0f) {
        auto nodeAIt = m_nodes.find(nodeA);
        auto nodeBIt = m_nodes.find(nodeB);
        if (nodeAIt != m_nodes.end() && nodeBIt != m_nodes.end()) {
            weight = glm::distance(nodeAIt->second.position, nodeBIt->second.position);
        }
    }

    // Add edges with individual traversal cost multipliers applied in AddEdge
    AddEdge(nodeA, nodeB, weight);
    AddEdge(nodeB, nodeA, weight);
}

void Graph::RemoveEdge(int from, int to) {
    auto it = m_nodes.find(from);
    if (it != m_nodes.end()) {
        auto& neighbors = it->second.neighbors;
        neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), to), neighbors.end());
    }

    auto weightIt = m_weights.find(from);
    if (weightIt != m_weights.end()) {
        weightIt->second.erase(to);
    }
}

void Graph::RemoveBidirectionalEdge(int nodeA, int nodeB) {
    RemoveEdge(nodeA, nodeB);
    RemoveEdge(nodeB, nodeA);
}

PathNode* Graph::GetNode(int id) {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

const PathNode* Graph::GetNode(int id) const {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

int Graph::GetNearestNode(const glm::vec3& position) const {
    if (m_nodes.empty()) {
        return -1;
    }

    // Use spatial hash if enabled
    if (m_spatialConfig.enabled) {
        int64_t centerKey = GetSpatialKey(position);
        float minDist = std::numeric_limits<float>::max();
        int nearestId = -1;

        // Check surrounding cells (3x3x3 grid)
        const int searchRadius = 2;
        for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
            for (int dz = -searchRadius; dz <= searchRadius; ++dz) {
                glm::vec3 offset(dx * m_spatialConfig.cellSize, 0, dz * m_spatialConfig.cellSize);
                int64_t key = GetSpatialKey(position + offset);

                auto cellIt = m_spatialHash.find(key);
                if (cellIt != m_spatialHash.end()) {
                    for (int nodeId : cellIt->second.nodeIds) {
                        const PathNode* node = GetNode(nodeId);
                        if (node) {
                            float dist = glm::distance2(position, node->position);
                            if (dist < minDist) {
                                minDist = dist;
                                nearestId = nodeId;
                            }
                        }
                    }
                }
            }
        }

        // If found in spatial hash, return it
        if (nearestId >= 0) {
            return nearestId;
        }
        // Fall through to brute force if spatial hash didn't find anything
    }

    // Brute force search
    int nearestId = -1;
    float minDist = std::numeric_limits<float>::max();

    for (const auto& [id, node] : m_nodes) {
        float dist = glm::distance2(position, node.position);
        if (dist < minDist) {
            minDist = dist;
            nearestId = id;
        }
    }

    return nearestId;
}

int Graph::GetNearestWalkableNode(const glm::vec3& position) const {
    int nearestId = -1;
    float minDist = std::numeric_limits<float>::max();

    for (const auto& [id, node] : m_nodes) {
        if (!node.walkable) continue;

        float dist = glm::distance2(position, node.position);
        if (dist < minDist) {
            minDist = dist;
            nearestId = id;
        }
    }

    return nearestId;
}

std::vector<int> Graph::GetNodesInRadius(const glm::vec3& position, float radius) const {
    std::vector<int> result;
    float radiusSq = radius * radius;

    if (m_spatialConfig.enabled) {
        // Use spatial hash for faster queries
        int cellsToCheck = static_cast<int>(std::ceil(radius / m_spatialConfig.cellSize)) + 1;

        for (int dx = -cellsToCheck; dx <= cellsToCheck; ++dx) {
            for (int dz = -cellsToCheck; dz <= cellsToCheck; ++dz) {
                glm::vec3 offset(dx * m_spatialConfig.cellSize, 0, dz * m_spatialConfig.cellSize);
                int64_t key = GetSpatialKey(position + offset);

                auto cellIt = m_spatialHash.find(key);
                if (cellIt != m_spatialHash.end()) {
                    for (int nodeId : cellIt->second.nodeIds) {
                        const PathNode* node = GetNode(nodeId);
                        if (node && glm::distance2(position, node->position) <= radiusSq) {
                            result.push_back(nodeId);
                        }
                    }
                }
            }
        }
    } else {
        // Brute force
        for (const auto& [id, node] : m_nodes) {
            if (glm::distance2(position, node.position) <= radiusSq) {
                result.push_back(id);
            }
        }
    }

    return result;
}

float Graph::GetEdgeWeight(int from, int to) const {
    auto it = m_weights.find(from);
    if (it != m_weights.end()) {
        auto edgeIt = it->second.find(to);
        if (edgeIt != it->second.end()) {
            return edgeIt->second;
        }
    }

    return std::numeric_limits<float>::infinity();
}

bool Graph::HasEdge(int from, int to) const {
    auto it = m_weights.find(from);
    if (it != m_weights.end()) {
        return it->second.find(to) != it->second.end();
    }
    return false;
}

void Graph::BuildGrid(int width, int height, float spacing, bool diagonals) {
    Clear();

    // Reserve space for efficiency
    std::vector<std::vector<int>> grid(height, std::vector<int>(width));

    // Create nodes
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            glm::vec3 pos(x * spacing, 0.0f, y * spacing);
            grid[y][x] = AddNode(pos);
        }
    }

    // Pre-calculate diagonal weight
    const float diagonalWeight = spacing * 1.41421356f; // sqrt(2)

    // Create edges
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int current = grid[y][x];

            // 4-directional connectivity (cardinal directions)
            if (x > 0) {
                AddBidirectionalEdge(current, grid[y][x - 1], spacing);
            }
            if (y > 0) {
                AddBidirectionalEdge(current, grid[y - 1][x], spacing);
            }

            // 8-directional (diagonals)
            if (diagonals) {
                if (x > 0 && y > 0) {
                    AddBidirectionalEdge(current, grid[y - 1][x - 1], diagonalWeight);
                }
                if (x < width - 1 && y > 0) {
                    AddBidirectionalEdge(current, grid[y - 1][x + 1], diagonalWeight);
                }
            }
        }
    }
}

void Graph::BuildHexGrid(int width, int height, float radius) {
    Clear();

    const float horizontalSpacing = radius * 1.5f;
    const float verticalSpacing = radius * 1.732050808f; // sqrt(3)
    const float rowOffset = radius * 0.866025404f; // sqrt(3)/2

    std::vector<std::vector<int>> grid(height, std::vector<int>(width, -1));

    // Create nodes
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float xOffset = (y % 2 == 1) ? rowOffset : 0.0f;
            glm::vec3 pos(x * horizontalSpacing + xOffset, 0.0f, y * verticalSpacing);
            grid[y][x] = AddNode(pos);
        }
    }

    // Create edges (6 neighbors per hex)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int current = grid[y][x];
            bool evenRow = (y % 2 == 0);

            // Horizontal neighbors
            if (x > 0) {
                AddBidirectionalEdge(current, grid[y][x - 1]);
            }

            // Diagonal neighbors depend on row parity
            if (y > 0) {
                if (evenRow) {
                    if (x > 0) AddBidirectionalEdge(current, grid[y - 1][x - 1]);
                    AddBidirectionalEdge(current, grid[y - 1][x]);
                } else {
                    AddBidirectionalEdge(current, grid[y - 1][x]);
                    if (x < width - 1) AddBidirectionalEdge(current, grid[y - 1][x + 1]);
                }
            }
        }
    }
}

void Graph::BuildRandom(int nodeCount, float connectionRadius, float areaSize) {
    Clear();

    // Create random nodes
    for (int i = 0; i < nodeCount; ++i) {
        glm::vec3 pos(
            Random::Range(0.0f, areaSize),
            0.0f,
            Random::Range(0.0f, areaSize)
        );
        AddNode(pos);
    }

    // Connect nearby nodes
    const float connectionRadiusSq = connectionRadius * connectionRadius;
    for (const auto& [id1, node1] : m_nodes) {
        for (const auto& [id2, node2] : m_nodes) {
            if (id1 >= id2) continue;

            float distSq = glm::distance2(node1.position, node2.position);
            if (distSq <= connectionRadiusSq) {
                float dist = std::sqrt(distSq);
                AddBidirectionalEdge(id1, id2, dist);
            }
        }
    }
}

void Graph::Clear() {
    m_nodes.clear();
    m_weights.clear();
    m_spatialHash.clear();
    m_nextId = 0;
}

void Graph::SetNodeWalkable(int nodeId, bool walkable) {
    auto it = m_nodes.find(nodeId);
    if (it != m_nodes.end()) {
        it->second.walkable = walkable;
    }
}

void Graph::RebuildSpatialIndex() {
    m_spatialHash.clear();

    if (!m_spatialConfig.enabled) {
        return;
    }

    for (const auto& [id, node] : m_nodes) {
        AddToSpatialHash(id, node.position);
    }
}

void Graph::SetSpatialIndexEnabled(bool enabled) {
    m_spatialConfig.enabled = enabled;
    if (enabled) {
        RebuildSpatialIndex();
    } else {
        m_spatialHash.clear();
    }
}

int64_t Graph::GetSpatialKey(const glm::vec3& position) const {
    int32_t cellX = static_cast<int32_t>(std::floor(position.x / m_spatialConfig.cellSize));
    int32_t cellZ = static_cast<int32_t>(std::floor(position.z / m_spatialConfig.cellSize));

    // Pack into 64-bit key (assumes coordinates fit in 32 bits)
    return (static_cast<int64_t>(cellX) << 32) | (static_cast<uint32_t>(cellZ));
}

void Graph::AddToSpatialHash(int nodeId, const glm::vec3& position) {
    int64_t key = GetSpatialKey(position);
    m_spatialHash[key].nodeIds.push_back(nodeId);
}

void Graph::RemoveFromSpatialHash(int nodeId, const glm::vec3& position) {
    int64_t key = GetSpatialKey(position);
    auto it = m_spatialHash.find(key);
    if (it != m_spatialHash.end()) {
        auto& nodeIds = it->second.nodeIds;
        nodeIds.erase(std::remove(nodeIds.begin(), nodeIds.end(), nodeId), nodeIds.end());
        if (nodeIds.empty()) {
            m_spatialHash.erase(it);
        }
    }
}

// PathfindingContext implementation
PathfindingContext::PathfindingContext(const Graph& graph)
    : m_graph(&graph) {
}

void PathfindingContext::Reset() {
    m_states.clear();
}

float PathfindingContext::GetGCost(int nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() ? it->second.gCost : std::numeric_limits<float>::infinity();
}

float PathfindingContext::GetHCost(int nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() ? it->second.hCost : 0.0f;
}

float PathfindingContext::GetFCost(int nodeId) const {
    return GetGCost(nodeId) + GetHCost(nodeId);
}

int PathfindingContext::GetParent(int nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() ? it->second.parent : -1;
}

bool PathfindingContext::IsVisited(int nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() && it->second.visited;
}

bool PathfindingContext::IsInOpenSet(int nodeId) const {
    auto it = m_states.find(nodeId);
    return it != m_states.end() && it->second.inOpenSet;
}

void PathfindingContext::SetGCost(int nodeId, float cost) {
    m_states[nodeId].gCost = cost;
}

void PathfindingContext::SetHCost(int nodeId, float cost) {
    m_states[nodeId].hCost = cost;
}

void PathfindingContext::SetParent(int nodeId, int parentId) {
    m_states[nodeId].parent = parentId;
}

void PathfindingContext::SetVisited(int nodeId, bool visited) {
    m_states[nodeId].visited = visited;
}

void PathfindingContext::SetInOpenSet(int nodeId, bool inOpenSet) {
    m_states[nodeId].inOpenSet = inOpenSet;
}

} // namespace Nova
