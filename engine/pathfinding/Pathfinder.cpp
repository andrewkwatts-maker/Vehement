#include "pathfinding/Pathfinder.hpp"
#include "pathfinding/Graph.hpp"
#include <queue>
#include <stack>
#include <algorithm>
#include <cmath>

namespace Nova {

PathResult Pathfinder::AStar(Graph& graph, int startId, int goalId, HeuristicFunc heuristic) {
    graph.ResetPathfindingState();

    if (!heuristic) {
        heuristic = EuclideanHeuristic;
    }

    PathNode* startNode = graph.GetNode(startId);
    PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    // Priority queue (min heap by f-cost)
    auto compare = [&graph](int a, int b) {
        return graph.GetNode(a)->fCost() > graph.GetNode(b)->fCost();
    };
    std::priority_queue<int, std::vector<int>, decltype(compare)> openSet(compare);

    startNode->gCost = 0;
    startNode->hCost = heuristic(startNode->position, goalNode->position);
    startNode->inOpenSet = true;
    openSet.push(startId);

    while (!openSet.empty()) {
        int currentId = openSet.top();
        openSet.pop();

        PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        current->inOpenSet = false;
        current->visited = true;

        // Found goal
        if (currentId == goalId) {
            return ReconstructPath(graph, startId, goalId);
        }

        // Explore neighbors
        for (int neighborId : current->neighbors) {
            PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || neighbor->visited) continue;

            float tentativeG = current->gCost + graph.GetEdgeWeight(currentId, neighborId);

            if (!neighbor->inOpenSet || tentativeG < neighbor->gCost) {
                neighbor->parent = currentId;
                neighbor->gCost = tentativeG;
                neighbor->hCost = heuristic(neighbor->position, goalNode->position);

                if (!neighbor->inOpenSet) {
                    neighbor->inOpenSet = true;
                    openSet.push(neighborId);
                }
            }
        }
    }

    return PathResult{};  // No path found
}

PathResult Pathfinder::Dijkstra(Graph& graph, int startId, int goalId) {
    // A* with zero heuristic
    return AStar(graph, startId, goalId, [](const glm::vec3&, const glm::vec3&) { return 0.0f; });
}

PathResult Pathfinder::BFS(Graph& graph, int startId, int goalId) {
    graph.ResetPathfindingState();

    PathNode* startNode = graph.GetNode(startId);
    PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    std::queue<int> queue;
    queue.push(startId);
    startNode->visited = true;

    while (!queue.empty()) {
        int currentId = queue.front();
        queue.pop();

        if (currentId == goalId) {
            return ReconstructPath(graph, startId, goalId);
        }

        PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        for (int neighborId : current->neighbors) {
            PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || neighbor->visited) continue;

            neighbor->visited = true;
            neighbor->parent = currentId;
            queue.push(neighborId);
        }
    }

    return PathResult{};
}

PathResult Pathfinder::DFS(Graph& graph, int startId, int goalId) {
    graph.ResetPathfindingState();

    PathNode* startNode = graph.GetNode(startId);
    PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    std::stack<int> stack;
    stack.push(startId);

    while (!stack.empty()) {
        int currentId = stack.top();
        stack.pop();

        PathNode* current = graph.GetNode(currentId);
        if (!current || current->visited) continue;

        current->visited = true;

        if (currentId == goalId) {
            return ReconstructPath(graph, startId, goalId);
        }

        for (int neighborId : current->neighbors) {
            PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || neighbor->visited) continue;

            neighbor->parent = currentId;
            stack.push(neighborId);
        }
    }

    return PathResult{};
}

PathResult Pathfinder::ReconstructPath(Graph& graph, int startId, int goalId) {
    PathResult result;
    result.found = true;

    int currentId = goalId;
    while (currentId != -1) {
        PathNode* node = graph.GetNode(currentId);
        if (!node) break;

        result.nodeIds.push_back(currentId);
        result.positions.push_back(node->position);

        if (currentId == startId) break;
        currentId = node->parent;
    }

    // Reverse to get start -> goal order
    std::reverse(result.nodeIds.begin(), result.nodeIds.end());
    std::reverse(result.positions.begin(), result.positions.end());

    // Calculate total cost
    for (size_t i = 1; i < result.nodeIds.size(); ++i) {
        result.totalCost += graph.GetEdgeWeight(result.nodeIds[i - 1], result.nodeIds[i]);
    }

    return result;
}

float Pathfinder::EuclideanHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return glm::distance(a, b);
}

float Pathfinder::ManhattanHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
}

float Pathfinder::ChebyshevHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
}

} // namespace Nova
