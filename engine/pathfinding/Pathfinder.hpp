#pragma once

#include <vector>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace Nova {

class Graph;
class PathfindingContext;

/**
 * @brief Result of a pathfinding operation
 */
struct PathResult {
    std::vector<int> nodeIds;           // Node IDs along the path
    std::vector<glm::vec3> positions;   // World positions along the path
    float totalCost = 0.0f;             // Total path cost
    bool found = false;                 // Whether a valid path was found
    int nodesExplored = 0;              // Number of nodes explored (for debugging)

    /**
     * @brief Check if the path is valid
     */
    [[nodiscard]] explicit operator bool() const noexcept { return found; }

    /**
     * @brief Get path length in world units
     */
    [[nodiscard]] float GetLength() const;

    /**
     * @brief Get number of waypoints
     */
    [[nodiscard]] size_t GetWaypointCount() const noexcept { return positions.size(); }

    /**
     * @brief Check if path is empty
     */
    [[nodiscard]] bool IsEmpty() const noexcept { return positions.empty(); }
};

/**
 * @brief Heuristic function type for A*
 */
using HeuristicFunc = std::function<float(const glm::vec3&, const glm::vec3&)>;

/**
 * @brief Predicate for node traversability
 */
using TraversablePredicate = std::function<bool(int nodeId, const Graph& graph)>;

/**
 * @brief Algorithm selection enum
 */
enum class PathfindingAlgorithm {
    AStar,      // A* with heuristic (default)
    Dijkstra,   // Shortest path without heuristic
    BFS,        // Breadth-first search (unweighted)
    DFS,        // Depth-first search (not optimal, but fast)
    Greedy      // Greedy best-first search
};

/**
 * @brief Configuration for pathfinding operations
 */
struct PathfindingConfig {
    PathfindingAlgorithm algorithm = PathfindingAlgorithm::AStar;
    HeuristicFunc heuristic = nullptr;          // Custom heuristic (nullptr = Euclidean)
    TraversablePredicate isTraversable = nullptr; // Custom traversability check
    float maxSearchDistance = -1.0f;            // Max distance to search (-1 = unlimited)
    int maxNodesExplored = -1;                  // Max nodes to explore (-1 = unlimited)
    bool allowDiagonals = true;                 // For grid-based pathfinding
    float heuristicWeight = 1.0f;               // Weight for heuristic (>1 = faster but less optimal)
};

/**
 * @brief Pathfinding algorithms for navigation graphs
 *
 * Provides static methods for various pathfinding algorithms with support
 * for custom heuristics, traversability predicates, and search limits.
 */
class Pathfinder {
public:
    /**
     * @brief Find a path using the specified configuration
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @param config Pathfinding configuration
     * @return Path result containing the found path or empty result if no path exists
     */
    [[nodiscard]] static PathResult FindPath(
        const Graph& graph,
        int startId,
        int goalId,
        const PathfindingConfig& config = {});

    /**
     * @brief A* pathfinding algorithm with custom heuristic
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @param heuristic Heuristic function (nullptr = Euclidean distance)
     * @return Path result
     */
    [[nodiscard]] static PathResult AStar(
        const Graph& graph,
        int startId,
        int goalId,
        HeuristicFunc heuristic = nullptr);

    /**
     * @brief Weighted A* for faster but potentially suboptimal paths
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @param weight Heuristic weight (>1 for faster search)
     * @param heuristic Heuristic function (nullptr = Euclidean distance)
     * @return Path result
     */
    [[nodiscard]] static PathResult WeightedAStar(
        const Graph& graph,
        int startId,
        int goalId,
        float weight,
        HeuristicFunc heuristic = nullptr);

    /**
     * @brief Dijkstra's algorithm (guaranteed shortest path)
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @return Path result
     */
    [[nodiscard]] static PathResult Dijkstra(
        const Graph& graph,
        int startId,
        int goalId);

    /**
     * @brief Breadth-first search (unweighted shortest path)
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @return Path result
     */
    [[nodiscard]] static PathResult BFS(
        const Graph& graph,
        int startId,
        int goalId);

    /**
     * @brief Depth-first search (finds a path, not necessarily shortest)
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @return Path result
     */
    [[nodiscard]] static PathResult DFS(
        const Graph& graph,
        int startId,
        int goalId);

    /**
     * @brief Greedy best-first search (fast but not optimal)
     * @param graph The navigation graph
     * @param startId Starting node ID
     * @param goalId Target node ID
     * @param heuristic Heuristic function (nullptr = Euclidean distance)
     * @return Path result
     */
    [[nodiscard]] static PathResult GreedyBestFirst(
        const Graph& graph,
        int startId,
        int goalId,
        HeuristicFunc heuristic = nullptr);

    // ========== Heuristic Functions ==========

    /**
     * @brief Euclidean distance heuristic (straight-line distance)
     * Admissible for any graph, optimal for Euclidean space
     */
    [[nodiscard]] static float EuclideanHeuristic(const glm::vec3& a, const glm::vec3& b);

    /**
     * @brief Manhattan distance heuristic (grid-aligned movement)
     * Admissible for 4-directional grid movement
     */
    [[nodiscard]] static float ManhattanHeuristic(const glm::vec3& a, const glm::vec3& b);

    /**
     * @brief Chebyshev distance heuristic (8-directional grid movement)
     * Admissible for 8-directional grid movement with uniform costs
     */
    [[nodiscard]] static float ChebyshevHeuristic(const glm::vec3& a, const glm::vec3& b);

    /**
     * @brief Octile distance heuristic (8-directional grid with diagonal cost)
     * Admissible for 8-directional grid with diagonal cost = sqrt(2)
     */
    [[nodiscard]] static float OctileHeuristic(const glm::vec3& a, const glm::vec3& b);

    /**
     * @brief Squared Euclidean distance (faster but not admissible)
     * Use only when consistency with A* is not required
     */
    [[nodiscard]] static float SquaredEuclideanHeuristic(const glm::vec3& a, const glm::vec3& b);

    /**
     * @brief Zero heuristic (equivalent to Dijkstra)
     */
    [[nodiscard]] static float ZeroHeuristic(const glm::vec3& a, const glm::vec3& b);

    // ========== Path Utilities ==========

    /**
     * @brief Smooth a path using string-pulling (funnel algorithm)
     * @param graph The navigation graph
     * @param path The path to smooth
     * @return Smoothed path with fewer waypoints
     */
    [[nodiscard]] static PathResult SmoothPath(
        const Graph& graph,
        const PathResult& path);

    /**
     * @brief Simplify a path by removing collinear points
     * @param path The path to simplify
     * @param tolerance Angle tolerance in radians for considering points collinear
     * @return Simplified path
     */
    [[nodiscard]] static PathResult SimplifyPath(
        const PathResult& path,
        float tolerance = 0.01f);

    /**
     * @brief Interpolate additional points along a path
     * @param path The path to interpolate
     * @param maxSegmentLength Maximum distance between adjacent points
     * @return Path with interpolated points
     */
    [[nodiscard]] static PathResult InterpolatePath(
        const PathResult& path,
        float maxSegmentLength);

private:
    /**
     * @brief Internal A* implementation using PathfindingContext
     */
    [[nodiscard]] static PathResult AStarInternal(
        const Graph& graph,
        int startId,
        int goalId,
        HeuristicFunc heuristic,
        float heuristicWeight,
        const PathfindingConfig& config);

    /**
     * @brief Reconstruct path from pathfinding context
     */
    [[nodiscard]] static PathResult ReconstructPath(
        const Graph& graph,
        const PathfindingContext& context,
        int startId,
        int goalId);
};

} // namespace Nova
