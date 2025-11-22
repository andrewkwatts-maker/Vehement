#include "pathfinding/Graph.hpp"
#include "math/Random.hpp"
#include <algorithm>
#include <limits>

namespace Nova {

int Graph::AddNode(const glm::vec3& position) {
    int id = m_nextId++;
    PathNode node;
    node.id = id;
    node.position = position;
    m_nodes[id] = node;
    return id;
}

void Graph::RemoveNode(int nodeId) {
    // Remove all edges to/from this node
    for (auto& [id, node] : m_nodes) {
        auto it = std::find(node.neighbors.begin(), node.neighbors.end(), nodeId);
        if (it != node.neighbors.end()) {
            node.neighbors.erase(it);
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
    if (weight < 0) {
        weight = glm::distance(fromIt->second.position, toIt->second.position);
    }

    m_weights[from][to] = weight;
}

void Graph::RemoveEdge(int from, int to) {
    auto it = m_nodes.find(from);
    if (it != m_nodes.end()) {
        auto& neighbors = it->second.neighbors;
        neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), to), neighbors.end());
    }

    if (m_weights.count(from)) {
        m_weights[from].erase(to);
    }
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

float Graph::GetEdgeWeight(int from, int to) const {
    auto it = m_weights.find(from);
    if (it != m_weights.end()) {
        auto edgeIt = it->second.find(to);
        if (edgeIt != it->second.end()) {
            return edgeIt->second;
        }
    }

    // Calculate distance as fallback
    const PathNode* fromNode = GetNode(from);
    const PathNode* toNode = GetNode(to);
    if (fromNode && toNode) {
        return glm::distance(fromNode->position, toNode->position);
    }

    return std::numeric_limits<float>::max();
}

void Graph::ResetPathfindingState() {
    for (auto& [id, node] : m_nodes) {
        node.gCost = 0;
        node.hCost = 0;
        node.parent = -1;
        node.visited = false;
        node.inOpenSet = false;
    }
}

void Graph::BuildGrid(int width, int height, float spacing) {
    Clear();

    std::vector<std::vector<int>> grid(height, std::vector<int>(width));

    // Create nodes
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            glm::vec3 pos(x * spacing, 0, y * spacing);
            grid[y][x] = AddNode(pos);
        }
    }

    // Create edges
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int current = grid[y][x];

            // 4-directional connectivity
            if (x > 0) {
                AddEdge(current, grid[y][x - 1]);
                AddEdge(grid[y][x - 1], current);
            }
            if (y > 0) {
                AddEdge(current, grid[y - 1][x]);
                AddEdge(grid[y - 1][x], current);
            }

            // 8-directional (diagonals)
            if (x > 0 && y > 0) {
                AddEdge(current, grid[y - 1][x - 1]);
                AddEdge(grid[y - 1][x - 1], current);
            }
            if (x < width - 1 && y > 0) {
                AddEdge(current, grid[y - 1][x + 1]);
                AddEdge(grid[y - 1][x + 1], current);
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
    for (const auto& [id1, node1] : m_nodes) {
        for (const auto& [id2, node2] : m_nodes) {
            if (id1 >= id2) continue;

            float dist = glm::distance(node1.position, node2.position);
            if (dist <= connectionRadius) {
                AddEdge(id1, id2, dist);
                AddEdge(id2, id1, dist);
            }
        }
    }
}

void Graph::Clear() {
    m_nodes.clear();
    m_weights.clear();
    m_nextId = 0;
}

} // namespace Nova
