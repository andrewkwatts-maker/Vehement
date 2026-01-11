#include "pathfinding/Pathfinder.hpp"
#include "pathfinding/Graph.hpp"
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Nova {

/**
 * @brief Advanced A* variants and utilities
 *
 * This file contains specialized A* implementations and related algorithms
 * for different pathfinding scenarios.
 */

namespace AStarUtils {

/**
 * @brief Bidirectional A* search
 *
 * Runs A* from both start and goal simultaneously, meeting in the middle.
 * Can be significantly faster than standard A* for large graphs.
 */
[[nodiscard]] PathResult BidirectionalAStar(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic = nullptr) {

    if (!heuristic) {
        heuristic = Pathfinder::EuclideanHeuristic;
    }

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode || !startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    // Forward search context (start -> goal)
    PathfindingContext forwardCtx(graph);
    using PQEntry = std::tuple<float, float, int>;
    std::set<PQEntry> forwardOpen;

    forwardCtx.SetGCost(startId, 0.0f);
    float startH = heuristic(startNode->position, goalNode->position);
    forwardCtx.SetHCost(startId, startH);
    forwardOpen.insert({startH, 0.0f, startId});

    // Backward search context (goal -> start)
    PathfindingContext backwardCtx(graph);
    std::set<PQEntry> backwardOpen;

    backwardCtx.SetGCost(goalId, 0.0f);
    float goalH = heuristic(goalNode->position, startNode->position);
    backwardCtx.SetHCost(goalId, goalH);
    backwardOpen.insert({goalH, 0.0f, goalId});

    int meetingNode = -1;
    float bestPathCost = std::numeric_limits<float>::infinity();
    int nodesExplored = 0;

    while (!forwardOpen.empty() && !backwardOpen.empty()) {
        // Expand forward search
        if (!forwardOpen.empty()) {
            auto fIt = forwardOpen.begin();
            auto [fF, fGNeg, fCurrent] = *fIt;
            forwardOpen.erase(fIt);

            if (!forwardCtx.IsVisited(fCurrent)) {
                forwardCtx.SetVisited(fCurrent, true);
                ++nodesExplored;

                // Check if backward search has visited this node
                if (backwardCtx.IsVisited(fCurrent)) {
                    float pathCost = forwardCtx.GetGCost(fCurrent) + backwardCtx.GetGCost(fCurrent);
                    if (pathCost < bestPathCost) {
                        bestPathCost = pathCost;
                        meetingNode = fCurrent;
                    }
                }

                const PathNode* current = graph.GetNode(fCurrent);
                if (current) {
                    float currentG = forwardCtx.GetGCost(fCurrent);

                    for (int neighborId : current->neighbors) {
                        const PathNode* neighbor = graph.GetNode(neighborId);
                        if (!neighbor || !neighbor->walkable || forwardCtx.IsVisited(neighborId)) continue;

                        float tentativeG = currentG + graph.GetEdgeWeight(fCurrent, neighborId);
                        if (tentativeG < forwardCtx.GetGCost(neighborId)) {
                            forwardCtx.SetParent(neighborId, fCurrent);
                            forwardCtx.SetGCost(neighborId, tentativeG);
                            float h = heuristic(neighbor->position, goalNode->position);
                            forwardCtx.SetHCost(neighborId, h);
                            forwardOpen.insert({tentativeG + h, -tentativeG, neighborId});
                        }
                    }
                }
            }
        }

        // Expand backward search
        if (!backwardOpen.empty()) {
            auto bIt = backwardOpen.begin();
            auto [bF, bGNeg, bCurrent] = *bIt;
            backwardOpen.erase(bIt);

            if (!backwardCtx.IsVisited(bCurrent)) {
                backwardCtx.SetVisited(bCurrent, true);
                ++nodesExplored;

                // Check if forward search has visited this node
                if (forwardCtx.IsVisited(bCurrent)) {
                    float pathCost = forwardCtx.GetGCost(bCurrent) + backwardCtx.GetGCost(bCurrent);
                    if (pathCost < bestPathCost) {
                        bestPathCost = pathCost;
                        meetingNode = bCurrent;
                    }
                }

                const PathNode* current = graph.GetNode(bCurrent);
                if (current) {
                    float currentG = backwardCtx.GetGCost(bCurrent);

                    for (int neighborId : current->neighbors) {
                        const PathNode* neighbor = graph.GetNode(neighborId);
                        if (!neighbor || !neighbor->walkable || backwardCtx.IsVisited(neighborId)) continue;

                        float tentativeG = currentG + graph.GetEdgeWeight(bCurrent, neighborId);
                        if (tentativeG < backwardCtx.GetGCost(neighborId)) {
                            backwardCtx.SetParent(neighborId, bCurrent);
                            backwardCtx.SetGCost(neighborId, tentativeG);
                            float h = heuristic(neighbor->position, startNode->position);
                            backwardCtx.SetHCost(neighborId, h);
                            backwardOpen.insert({tentativeG + h, -tentativeG, neighborId});
                        }
                    }
                }
            }
        }

        // Termination check
        if (meetingNode >= 0) {
            float forwardMin = forwardOpen.empty() ? std::numeric_limits<float>::infinity()
                                                   : std::get<0>(*forwardOpen.begin());
            float backwardMin = backwardOpen.empty() ? std::numeric_limits<float>::infinity()
                                                     : std::get<0>(*backwardOpen.begin());
            if (forwardMin + backwardMin >= bestPathCost) {
                break;
            }
        }
    }

    if (meetingNode < 0) {
        PathResult result;
        result.nodesExplored = nodesExplored;
        return result;
    }

    // Reconstruct path
    PathResult result;
    result.found = true;
    result.totalCost = bestPathCost;
    result.nodesExplored = nodesExplored;

    // Forward path (start -> meeting)
    std::vector<int> forwardPath;
    int current = meetingNode;
    while (current != -1) {
        forwardPath.push_back(current);
        if (current == startId) break;
        current = forwardCtx.GetParent(current);
    }
    std::reverse(forwardPath.begin(), forwardPath.end());

    // Backward path (meeting -> goal)
    std::vector<int> backwardPath;
    current = backwardCtx.GetParent(meetingNode);
    while (current != -1) {
        backwardPath.push_back(current);
        if (current == goalId) break;
        current = backwardCtx.GetParent(current);
    }

    // Combine paths
    result.nodeIds = forwardPath;
    for (int id : backwardPath) {
        result.nodeIds.push_back(id);
    }

    // Get positions
    for (int id : result.nodeIds) {
        const PathNode* node = graph.GetNode(id);
        if (node) {
            result.positions.push_back(node->position);
        }
    }

    return result;
}

/**
 * @brief Iterative Deepening A* (IDA*)
 *
 * Memory-efficient variant that uses iterative deepening.
 * Uses O(d) memory where d is the solution depth, compared to O(b^d) for standard A*.
 */
[[nodiscard]] PathResult IDAstar(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic = nullptr) {

    if (!heuristic) {
        heuristic = Pathfinder::EuclideanHeuristic;
    }

    const PathNode* startNode = graph.GetNode(startId);
    const PathNode* goalNode = graph.GetNode(goalId);

    if (!startNode || !goalNode || !startNode->walkable || !goalNode->walkable) {
        return PathResult{};
    }

    // Current path for DFS
    std::vector<int> currentPath;
    currentPath.push_back(startId);

    float threshold = heuristic(startNode->position, goalNode->position);
    int totalNodesExplored = 0;

    // Recursive search function
    std::function<float(int, float, std::unordered_set<int>&)> search;
    search = [&](int nodeId, float g, std::unordered_set<int>& visited) -> float {
        const PathNode* node = graph.GetNode(nodeId);
        if (!node) return std::numeric_limits<float>::infinity();

        float f = g + heuristic(node->position, goalNode->position);
        if (f > threshold) return f;

        ++totalNodesExplored;

        if (nodeId == goalId) return -1.0f;  // Found!

        float minThreshold = std::numeric_limits<float>::infinity();
        visited.insert(nodeId);

        for (int neighborId : node->neighbors) {
            if (visited.count(neighborId)) continue;

            const PathNode* neighbor = graph.GetNode(neighborId);
            if (!neighbor || !neighbor->walkable) continue;

            currentPath.push_back(neighborId);
            float edgeCost = graph.GetEdgeWeight(nodeId, neighborId);
            float result = search(neighborId, g + edgeCost, visited);

            if (result < 0) return -1.0f;  // Found in recursive call
            if (result < minThreshold) minThreshold = result;

            currentPath.pop_back();
        }

        visited.erase(nodeId);
        return minThreshold;
    };

    // Iterative deepening loop
    while (threshold < std::numeric_limits<float>::infinity()) {
        std::unordered_set<int> visited;
        float result = search(startId, 0.0f, visited);

        if (result < 0) {
            // Found path
            PathResult pathResult;
            pathResult.found = true;
            pathResult.nodeIds = currentPath;
            pathResult.nodesExplored = totalNodesExplored;

            for (int id : currentPath) {
                const PathNode* node = graph.GetNode(id);
                if (node) {
                    pathResult.positions.push_back(node->position);
                }
            }

            for (size_t i = 1; i < currentPath.size(); ++i) {
                pathResult.totalCost += graph.GetEdgeWeight(currentPath[i - 1], currentPath[i]);
            }

            return pathResult;
        }

        threshold = result;
    }

    PathResult result;
    result.nodesExplored = totalNodesExplored;
    return result;
}

/**
 * @brief Find multiple shortest paths (K shortest paths)
 *
 * Uses Yen's algorithm to find the K shortest paths between two nodes.
 */
[[nodiscard]] std::vector<PathResult> KShortestPaths(
    const Graph& graph,
    int startId,
    int goalId,
    int k,
    HeuristicFunc heuristic = nullptr) {

    std::vector<PathResult> result;
    if (k <= 0) return result;

    // Find first shortest path
    PathResult firstPath = Pathfinder::AStar(graph, startId, goalId, heuristic);
    if (!firstPath.found) return result;

    result.push_back(firstPath);
    if (k == 1) return result;

    // Candidate paths (sorted by cost)
    auto comparePaths = [](const PathResult& a, const PathResult& b) {
        return a.totalCost < b.totalCost;
    };
    std::multiset<PathResult, decltype(comparePaths)> candidates(comparePaths);

    for (int i = 1; i < k; ++i) {
        const PathResult& prevPath = result[i - 1];

        // For each node in the previous path (except the last)
        for (size_t j = 0; j < prevPath.nodeIds.size() - 1; ++j) {
            int spurNode = prevPath.nodeIds[j];

            // Root path from start to spur node
            std::vector<int> rootPath(prevPath.nodeIds.begin(), prevPath.nodeIds.begin() + j + 1);

            // Temporarily remove edges that would lead to paths already in result
            std::vector<std::pair<int, int>> removedEdges;
            for (const auto& existingPath : result) {
                if (existingPath.nodeIds.size() > j + 1) {
                    bool sameRoot = true;
                    for (size_t m = 0; m <= j && m < existingPath.nodeIds.size(); ++m) {
                        if (existingPath.nodeIds[m] != rootPath[m]) {
                            sameRoot = false;
                            break;
                        }
                    }
                    if (sameRoot) {
                        removedEdges.push_back({existingPath.nodeIds[j], existingPath.nodeIds[j + 1]});
                    }
                }
            }

            // Find spur path (simplified - without actually modifying graph)
            PathfindingConfig config;
            config.heuristic = heuristic;
            config.isTraversable = [&removedEdges, &rootPath](int nodeId, const Graph&) {
                // Block nodes in root path (except spur node)
                for (size_t m = 0; m < rootPath.size() - 1; ++m) {
                    if (nodeId == rootPath[m]) return false;
                }
                return true;
            };

            PathResult spurPath = Pathfinder::FindPath(graph, spurNode, goalId, config);

            if (spurPath.found) {
                // Combine root and spur paths
                PathResult totalPath;
                totalPath.found = true;
                totalPath.nodeIds = rootPath;
                for (size_t m = 1; m < spurPath.nodeIds.size(); ++m) {
                    totalPath.nodeIds.push_back(spurPath.nodeIds[m]);
                }

                // Calculate positions and cost
                for (int id : totalPath.nodeIds) {
                    const PathNode* node = graph.GetNode(id);
                    if (node) totalPath.positions.push_back(node->position);
                }

                for (size_t m = 1; m < totalPath.nodeIds.size(); ++m) {
                    totalPath.totalCost += graph.GetEdgeWeight(
                        totalPath.nodeIds[m - 1], totalPath.nodeIds[m]);
                }

                // Check if this path is unique
                bool isDuplicate = false;
                for (const auto& existing : result) {
                    if (existing.nodeIds == totalPath.nodeIds) {
                        isDuplicate = true;
                        break;
                    }
                }
                for (const auto& candidate : candidates) {
                    if (candidate.nodeIds == totalPath.nodeIds) {
                        isDuplicate = true;
                        break;
                    }
                }

                if (!isDuplicate) {
                    candidates.insert(totalPath);
                }
            }
        }

        if (candidates.empty()) break;

        // Add best candidate to result
        result.push_back(*candidates.begin());
        candidates.erase(candidates.begin());
    }

    return result;
}

/**
 * @brief Check if a path is valid (all nodes connected and walkable)
 */
[[nodiscard]] bool ValidatePath(const Graph& graph, const PathResult& path) {
    if (!path.found || path.nodeIds.empty()) {
        return false;
    }

    for (size_t i = 0; i < path.nodeIds.size(); ++i) {
        const PathNode* node = graph.GetNode(path.nodeIds[i]);
        if (!node || !node->walkable) {
            return false;
        }

        if (i > 0 && !graph.HasEdge(path.nodeIds[i - 1], path.nodeIds[i])) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Estimate path cost without full pathfinding (for quick comparisons)
 */
[[nodiscard]] float EstimatePathCost(
    const Graph& graph,
    int startId,
    int goalId,
    HeuristicFunc heuristic = nullptr) {

    if (!heuristic) {
        heuristic = Pathfinder::EuclideanHeuristic;
    }

    const PathNode* start = graph.GetNode(startId);
    const PathNode* goal = graph.GetNode(goalId);

    if (!start || !goal) {
        return std::numeric_limits<float>::infinity();
    }

    return heuristic(start->position, goal->position);
}

} // namespace AStarUtils

} // namespace Nova
