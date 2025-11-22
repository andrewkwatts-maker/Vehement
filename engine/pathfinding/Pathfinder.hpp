#pragma once

#include <vector>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {

class Graph;

/**
 * @brief Path result
 */
struct PathResult {
    std::vector<int> nodeIds;
    std::vector<glm::vec3> positions;
    float totalCost = 0;
    bool found = false;
};

/**
 * @brief Heuristic function type
 */
using HeuristicFunc = std::function<float(const glm::vec3&, const glm::vec3&)>;

/**
 * @brief Pathfinding algorithms
 */
class Pathfinder {
public:
    /**
     * @brief A* pathfinding algorithm
     */
    static PathResult AStar(Graph& graph, int startId, int goalId,
                           HeuristicFunc heuristic = nullptr);

    /**
     * @brief Dijkstra's algorithm (A* with zero heuristic)
     */
    static PathResult Dijkstra(Graph& graph, int startId, int goalId);

    /**
     * @brief Breadth-first search
     */
    static PathResult BFS(Graph& graph, int startId, int goalId);

    /**
     * @brief Depth-first search
     */
    static PathResult DFS(Graph& graph, int startId, int goalId);

    // Common heuristics
    static float EuclideanHeuristic(const glm::vec3& a, const glm::vec3& b);
    static float ManhattanHeuristic(const glm::vec3& a, const glm::vec3& b);
    static float ChebyshevHeuristic(const glm::vec3& a, const glm::vec3& b);

private:
    static PathResult ReconstructPath(Graph& graph, int startId, int goalId);
};

} // namespace Nova
