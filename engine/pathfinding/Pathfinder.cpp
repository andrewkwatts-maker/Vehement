#include "pathfinding/Pathfinder.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "pathfinding/Graph.hpp"
#include <glm/gtx/norm.hpp>
#include <queue>
#include <stack>
#include <set>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Nova {

// PathResult implementation
float PathResult::GetLength() const {
    float length = 0.0f;
    for (size_t i = 1; i < positions.size(); ++i) {
        length += glm::distance(positions[i - 1], positions[i]);
    }
    return length;
}

// Pathfinder implementation
PathResult Pathfinder::FindPath(
    const Graph& graph,
    int startId,
    int goalId,
    const PathfindingConfig& config) {

    switch (config.algorithm) {
        case PathfindingAlgorithm::AStar:
            return AStarInternal(graph, startId, goalId, config.heuristic, config.heuristicWeight, config);

        case PathfindingAlgorithm::Dijkstra:
            return Dijkstra(graph, startId, goalId);

        case PathfindingAlgorithm::BFS:
            return BFS(graph, startId, goalId);

        case PathfindingAlgorithm::DFS:
            return DFS(graph, startId, goalId);

        case PathfindingAlgorithm::Greedy:
            return GreedyBestFirst(graph, startId, goalId, config.heuristic);

        default:
            return PathResult{};
    }
}

PathResult Pathfinder::AStar(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic) {

    return AStarInternal(graph, startId, goalId, heuristic, 1.0f, {});
}

PathResult Pathfinder::WeightedAStar(
    const Graph& graph,
    int startId,
    int goalId,
    float weight,
    HeuristicFunc heuristic) {

    return AStarInternal(graph, startId, goalId, heuristic, weight, {});
}

PathResult Pathfinder::AStarInternal(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic,
    float heuristicWeight,
    const PathfindingConfig& config) {

    PathfindingContext context(graph);

    if (!heuristic) {
        heuristic = EuclideanHeuristic;
    }

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    if (!startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    // Use a set of (f-cost, g-cost, nodeId) for efficient priority queue with updates
    // The g-cost is used as a tiebreaker to prefer nodes closer to the goal
    using PQEntry = std::tuple<float, float, int>;
    std::set<PQEntry> openSet;

    context.SetGCost(startId, 0.0f);
    float startH = heuristic(startNode->position, goalNode->position) * heuristicWeight;
    context.SetHCost(startId, startH);
    context.SetInOpenSet(startId, true);
    openSet.insert({startH, 0.0f, startId});

    int nodesExplored = 0;

    while (!openSet.empty()) {
        // Get node with lowest f-cost
        auto it = openSet.begin();
        auto [fCost, gCostNeg, currentId] = *it;
        openSet.erase(it);

        // Skip if already visited (stale entry)
        if (context.IsVisited(currentId)) {
            continue;
        }

        context.SetInOpenSet(currentId, false);
        context.SetVisited(currentId, true);
        ++nodesExplored;

        // Check search limits
        if (config.maxNodesExplored > 0 && nodesExplored >= config.maxNodesExplored) {
            break;
        }

        // Found goal
        if (currentId == goalId) {
            PathResult result = ReconstructPath(graph, context, startId, goalId);
            result.nodesExplored = nodesExplored;
            return result;
        }

        const PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        float currentG = context.GetGCost(currentId);

        // Check max search distance
        if (config.maxSearchDistance > 0.0f && currentG > config.maxSearchDistance) {
            continue;
        }

        // Explore neighbors
        for (int neighborId : current->neighbors) {
            const PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || context.IsVisited(neighborId)) continue;

            // Check walkability
            if (!neighbor->walkable) continue;

            // Check custom traversability predicate
            if (config.isTraversable && !config.isTraversable(neighborId, graph)) {
                continue;
            }

            float edgeWeight = graph.GetEdgeWeight(currentId, neighborId);
            float tentativeG = currentG + edgeWeight;

            if (tentativeG < context.GetGCost(neighborId)) {
                // This path is better - update the node

                // Remove old entry from open set if present
                if (context.IsInOpenSet(neighborId)) {
                    float oldF = context.GetFCost(neighborId);
                    float oldG = context.GetGCost(neighborId);
                    openSet.erase({oldF, -oldG, neighborId});
                }

                context.SetParent(neighborId, currentId);
                context.SetGCost(neighborId, tentativeG);

                float h = heuristic(neighbor->position, goalNode->position) * heuristicWeight;
                context.SetHCost(neighborId, h);

                float f = tentativeG + h;
                context.SetInOpenSet(neighborId, true);
                openSet.insert({f, -tentativeG, neighborId});  // Negative g for tiebreaking (prefer higher g)
            }
        }
    }

    // No path found
    PathResult result;
    result.nodesExplored = nodesExplored;
    return result;
}

PathResult Pathfinder::Dijkstra(const Graph& graph, int startId, int goalId) {
    return AStarInternal(graph, startId, goalId, ZeroHeuristic, 1.0f, {});
}

PathResult Pathfinder::BFS(const Graph& graph, int startId, int goalId) {
    PathfindingContext context(graph);

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    if (!startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    std::queue<int> queue;
    queue.push(startId);
    context.SetVisited(startId, true);
    context.SetGCost(startId, 0.0f);

    int nodesExplored = 0;

    while (!queue.empty()) {
        int currentId = queue.front();
        queue.pop();
        ++nodesExplored;

        if (currentId == goalId) {
            PathResult result = ReconstructPath(graph, context, startId, goalId);
            result.nodesExplored = nodesExplored;
            return result;
        }

        const PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        for (int neighborId : current->neighbors) {
            if (context.IsVisited(neighborId)) continue;

            const PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || !neighbor->walkable) continue;

            context.SetVisited(neighborId, true);
            context.SetParent(neighborId, currentId);
            context.SetGCost(neighborId, context.GetGCost(currentId) + graph.GetEdgeWeight(currentId, neighborId));
            queue.push(neighborId);
        }
    }

    PathResult result;
    result.nodesExplored = nodesExplored;
    return result;
}

PathResult Pathfinder::DFS(const Graph& graph, int startId, int goalId) {
    PathfindingContext context(graph);

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    if (!startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    std::stack<int> stack;
    stack.push(startId);
    context.SetGCost(startId, 0.0f);

    int nodesExplored = 0;

    while (!stack.empty()) {
        int currentId = stack.top();
        stack.pop();

        if (context.IsVisited(currentId)) continue;

        context.SetVisited(currentId, true);
        ++nodesExplored;

        if (currentId == goalId) {
            PathResult result = ReconstructPath(graph, context, startId, goalId);
            result.nodesExplored = nodesExplored;
            return result;
        }

        const PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        for (int neighborId : current->neighbors) {
            if (context.IsVisited(neighborId)) continue;

            const PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || !neighbor->walkable) continue;

            context.SetParent(neighborId, currentId);
            context.SetGCost(neighborId, context.GetGCost(currentId) + graph.GetEdgeWeight(currentId, neighborId));
            stack.push(neighborId);
        }
    }

    PathResult result;
    result.nodesExplored = nodesExplored;
    return result;
}

PathResult Pathfinder::GreedyBestFirst(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic) {

    PathfindingContext context(graph);

    if (!heuristic) {
        heuristic = EuclideanHeuristic;
    }

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode) {
        return PathResult{};
    }

    if (!startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    // Priority queue ordered by heuristic only
    using PQEntry = std::pair<float, int>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> openSet;

    float startH = heuristic(startNode->position, goalNode->position);
    context.SetHCost(startId, startH);
    context.SetGCost(startId, 0.0f);
    context.SetInOpenSet(startId, true);
    openSet.push({startH, startId});

    int nodesExplored = 0;

    while (!openSet.empty()) {
        auto [_, currentId] = openSet.top();
        openSet.pop();

        if (context.IsVisited(currentId)) continue;

        context.SetVisited(currentId, true);
        ++nodesExplored;

        if (currentId == goalId) {
            PathResult result = ReconstructPath(graph, context, startId, goalId);
            result.nodesExplored = nodesExplored;
            return result;
        }

        const PathNode* current = graph.GetNode(currentId);
        if (!current) continue;

        for (int neighborId : current->neighbors) {
            if (context.IsVisited(neighborId) || context.IsInOpenSet(neighborId)) continue;

            const PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || !neighbor->walkable) continue;

            context.SetParent(neighborId, currentId);
            context.SetGCost(neighborId, context.GetGCost(currentId) + graph.GetEdgeWeight(currentId, neighborId));

            float h = heuristic(neighbor->position, goalNode->position);
            context.SetHCost(neighborId, h);
            context.SetInOpenSet(neighborId, true);
            openSet.push({h, neighborId});
        }
    }

    PathResult result;
    result.nodesExplored = nodesExplored;
    return result;
}

PathResult Pathfinder::ReconstructPath(
    const Graph& graph,
    const PathfindingContext& context,
    int startId,
    int goalId) {

    PathResult result;
    result.found = true;

    int currentId = goalId;
    while (currentId != -1) {
        const PathNode* node = graph.GetNode(currentId);
        if (!node) break;

        result.nodeIds.push_back(currentId);
        result.positions.push_back(node->position);

        if (currentId == startId) break;
        currentId = context.GetParent(currentId);
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

// Heuristic implementations
float Pathfinder::EuclideanHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return glm::distance(a, b);
}

float Pathfinder::ManhattanHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z);
}

float Pathfinder::ChebyshevHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return std::max({std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z)});
}

float Pathfinder::OctileHeuristic(const glm::vec3& a, const glm::vec3& b) {
    // For 8-directional movement where diagonal cost is sqrt(2)
    float dx = std::abs(a.x - b.x);
    float dy = std::abs(a.y - b.y);
    float dz = std::abs(a.z - b.z);

    // In 2D (ignoring y for ground-based movement)
    float dMin = std::min(dx, dz);
    float dMax = std::max(dx, dz);

    // Diagonal moves cost sqrt(2), cardinal moves cost 1
    constexpr float SQRT2_MINUS_1 = 0.41421356f;
    return dMax + SQRT2_MINUS_1 * dMin + dy;
}

float Pathfinder::SquaredEuclideanHeuristic(const glm::vec3& a, const glm::vec3& b) {
    return glm::distance2(a, b);
}

float Pathfinder::ZeroHeuristic(const glm::vec3&, const glm::vec3&) {
    return 0.0f;
}

// Path utilities
PathResult Pathfinder::SmoothPath(const Graph& graph, const PathResult& path) {
    if (path.positions.size() <= 2) {
        return path;
    }

    PathResult smoothed;
    smoothed.found = path.found;
    smoothed.totalCost = path.totalCost;

    // Simple string-pulling: keep only waypoints where direction changes significantly
    smoothed.positions.push_back(path.positions.front());
    smoothed.nodeIds.push_back(path.nodeIds.front());

    for (size_t i = 1; i < path.positions.size() - 1; ++i) {
        glm::vec3 prev = smoothed.positions.back();
        glm::vec3 curr = path.positions[i];
        glm::vec3 next = path.positions[i + 1];

        glm::vec3 dir1 = glm::normalize(curr - prev);
        glm::vec3 dir2 = glm::normalize(next - curr);

        float dot = glm::dot(dir1, dir2);

        // Keep point if direction changes significantly (not nearly collinear)
        if (dot < 0.99f) {
            smoothed.positions.push_back(curr);
            smoothed.nodeIds.push_back(path.nodeIds[i]);
        }
    }

    smoothed.positions.push_back(path.positions.back());
    smoothed.nodeIds.push_back(path.nodeIds.back());

    smoothed.nodesExplored = path.nodesExplored;
    return smoothed;
}

PathResult Pathfinder::SimplifyPath(const PathResult& path, float tolerance) {
    if (path.positions.size() <= 2) {
        return path;
    }

    PathResult simplified;
    simplified.found = path.found;
    simplified.totalCost = path.totalCost;
    simplified.nodesExplored = path.nodesExplored;

    simplified.positions.push_back(path.positions.front());
    simplified.nodeIds.push_back(path.nodeIds.front());

    for (size_t i = 1; i < path.positions.size() - 1; ++i) {
        glm::vec3 prev = simplified.positions.back();
        glm::vec3 curr = path.positions[i];
        glm::vec3 next = path.positions[i + 1];

        glm::vec3 dir1 = curr - prev;
        glm::vec3 dir2 = next - curr;

        float len1 = glm::length(dir1);
        float len2 = glm::length(dir2);

        if (len1 > 0.0001f && len2 > 0.0001f) {
            dir1 /= len1;
            dir2 /= len2;

            float dot = glm::dot(dir1, dir2);
            float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));

            // Keep point if angle exceeds tolerance
            if (angle > tolerance) {
                simplified.positions.push_back(curr);
                simplified.nodeIds.push_back(path.nodeIds[i]);
            }
        } else {
            // Keep zero-length segments' endpoints
            simplified.positions.push_back(curr);
            simplified.nodeIds.push_back(path.nodeIds[i]);
        }
    }

    simplified.positions.push_back(path.positions.back());
    simplified.nodeIds.push_back(path.nodeIds.back());

    return simplified;
}

PathResult Pathfinder::InterpolatePath(const PathResult& path, float maxSegmentLength) {
    if (path.positions.empty() || maxSegmentLength <= 0.0f) {
        return path;
    }

    PathResult interpolated;
    interpolated.found = path.found;
    interpolated.totalCost = path.totalCost;
    interpolated.nodesExplored = path.nodesExplored;

    interpolated.positions.push_back(path.positions.front());
    interpolated.nodeIds.push_back(path.nodeIds.front());

    for (size_t i = 1; i < path.positions.size(); ++i) {
        glm::vec3 start = path.positions[i - 1];
        glm::vec3 end = path.positions[i];
        float segmentLength = glm::distance(start, end);

        if (segmentLength > maxSegmentLength) {
            int numSegments = static_cast<int>(std::ceil(segmentLength / maxSegmentLength));
            for (int j = 1; j < numSegments; ++j) {
                float t = static_cast<float>(j) / static_cast<float>(numSegments);
                glm::vec3 interpolatedPos = glm::mix(start, end, t);
                interpolated.positions.push_back(interpolatedPos);
                // Intermediate points don't have node IDs, use -1
                interpolated.nodeIds.push_back(-1);
            }
        }

        interpolated.positions.push_back(end);
        interpolated.nodeIds.push_back(path.nodeIds[i]);
    }

    return interpolated;
}

} // namespace Nova
