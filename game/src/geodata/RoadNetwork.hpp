#pragma once

#include "GeoTypes.hpp"
#include <memory>
#include <unordered_map>
#include <set>

namespace Vehement {
namespace Geo {

/**
 * @brief Road segment in game coordinates
 */
struct RoadSegment {
    int64_t id = 0;                      // Original road ID
    int segmentIndex = 0;                // Index within the road

    glm::vec2 start{0.0f};               // Start point in game coords
    glm::vec2 end{0.0f};                 // End point in game coords

    RoadType type = RoadType::Unknown;
    float width = 4.0f;                  // Road width in game units
    int lanes = 1;

    bool oneway = false;
    bool bridge = false;
    bool tunnel = false;
    int layer = 0;

    /**
     * @brief Get segment length
     */
    float GetLength() const {
        return glm::length(end - start);
    }

    /**
     * @brief Get segment direction (normalized)
     */
    glm::vec2 GetDirection() const {
        return glm::normalize(end - start);
    }

    /**
     * @brief Get perpendicular direction (for road width)
     */
    glm::vec2 GetPerpendicular() const {
        glm::vec2 dir = GetDirection();
        return glm::vec2(-dir.y, dir.x);
    }

    /**
     * @brief Get corners of road segment (for rendering)
     */
    void GetCorners(glm::vec2& bl, glm::vec2& br, glm::vec2& tl, glm::vec2& tr) const {
        glm::vec2 perp = GetPerpendicular() * (width * 0.5f);
        bl = start - perp;
        br = start + perp;
        tl = end - perp;
        tr = end + perp;
    }
};

/**
 * @brief Road intersection/junction
 */
struct RoadIntersection {
    int64_t id = 0;                      // Node ID
    glm::vec2 position{0.0f};            // Position in game coords

    std::vector<int64_t> connectedRoads; // Connected road IDs
    std::vector<int> segmentIndices;     // Which segment of each road

    // Intersection properties
    bool isTrafficLight = false;
    bool isStopSign = false;
    bool isRoundabout = false;
    int priority = 0;                    // Higher = higher priority roads

    /**
     * @brief Get number of connecting roads
     */
    int GetDegree() const { return static_cast<int>(connectedRoads.size()); }
};

/**
 * @brief Processed road polyline in game coordinates
 */
struct ProcessedRoad {
    int64_t id = 0;
    std::string name;
    std::string ref;
    RoadType type = RoadType::Unknown;
    RoadSurface surface = RoadSurface::Unknown;

    std::vector<glm::vec2> points;       // Road centerline in game coords
    float width = 4.0f;
    int lanes = 1;
    bool oneway = false;
    bool bridge = false;
    bool tunnel = false;
    int layer = 0;

    std::vector<RoadSegment> segments;   // Pre-computed segments

    /**
     * @brief Get total road length
     */
    float GetLength() const {
        float len = 0.0f;
        for (size_t i = 1; i < points.size(); ++i) {
            len += glm::length(points[i] - points[i - 1]);
        }
        return len;
    }

    /**
     * @brief Get bounding box
     */
    std::pair<glm::vec2, glm::vec2> GetBounds() const {
        if (points.empty()) return {{0, 0}, {0, 0}};

        glm::vec2 min = points[0];
        glm::vec2 max = points[0];

        for (const auto& p : points) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        return {min, max};
    }
};

/**
 * @brief Road network graph for pathfinding
 */
class RoadGraph {
public:
    /**
     * @brief Graph node (intersection or road endpoint)
     */
    struct Node {
        int64_t id = 0;
        glm::vec2 position{0.0f};
        std::vector<std::pair<int64_t, float>> neighbors;  // (node ID, distance)
    };

    /**
     * @brief Graph edge (road segment)
     */
    struct Edge {
        int64_t fromNode = 0;
        int64_t toNode = 0;
        int64_t roadId = 0;
        float distance = 0.0f;
        float speedLimit = 50.0f;        // km/h
        bool oneway = false;
        RoadType type = RoadType::Unknown;
    };

    /**
     * @brief Add node to graph
     */
    void AddNode(const Node& node);

    /**
     * @brief Add edge to graph
     */
    void AddEdge(const Edge& edge);

    /**
     * @brief Get node by ID
     */
    const Node* GetNode(int64_t id) const;

    /**
     * @brief Find nearest node to position
     */
    int64_t FindNearestNode(const glm::vec2& position) const;

    /**
     * @brief Find shortest path between nodes (Dijkstra)
     * @return List of node IDs in path
     */
    std::vector<int64_t> FindPath(int64_t startNode, int64_t endNode) const;

    /**
     * @brief Find fastest path (considering speed limits)
     */
    std::vector<int64_t> FindFastestPath(int64_t startNode, int64_t endNode) const;

    /**
     * @brief Get all nodes
     */
    const std::unordered_map<int64_t, Node>& GetNodes() const { return m_nodes; }

    /**
     * @brief Get all edges
     */
    const std::vector<Edge>& GetEdges() const { return m_edges; }

    /**
     * @brief Clear graph
     */
    void Clear();

    /**
     * @brief Get statistics
     */
    size_t GetNodeCount() const { return m_nodes.size(); }
    size_t GetEdgeCount() const { return m_edges.size(); }

private:
    std::unordered_map<int64_t, Node> m_nodes;
    std::vector<Edge> m_edges;
};

/**
 * @brief Road network processor
 *
 * Converts geographic road data to game-ready format:
 * - Coordinate transformation
 * - Road segmentation
 * - Intersection detection
 * - Connectivity graph building
 * - Road tile generation
 */
class RoadNetwork {
public:
    /**
     * @brief Coordinate transform callback
     */
    using CoordTransform = std::function<glm::vec2(const GeoCoordinate&)>;

    RoadNetwork();
    ~RoadNetwork();

    /**
     * @brief Set coordinate transformation
     */
    void SetCoordinateTransform(CoordTransform transform) { m_transform = transform; }

    /**
     * @brief Set default coordinate transform (meters from origin)
     */
    void SetDefaultTransform(const GeoCoordinate& origin, float scale = 1.0f);

    // ==========================================================================
    // Processing
    // ==========================================================================

    /**
     * @brief Process roads from geographic data
     * @param roads Geographic roads
     * @return Number of roads processed
     */
    int ProcessRoads(const std::vector<GeoRoad>& roads);

    /**
     * @brief Process a single road
     */
    void ProcessRoad(const GeoRoad& road);

    /**
     * @brief Build road segments from processed roads
     */
    void BuildSegments();

    /**
     * @brief Detect and build intersections
     */
    void BuildIntersections();

    /**
     * @brief Build connectivity graph
     */
    void BuildGraph();

    /**
     * @brief Full processing pipeline
     */
    void ProcessAll(const std::vector<GeoRoad>& roads);

    /**
     * @brief Clear all data
     */
    void Clear();

    // ==========================================================================
    // Access
    // ==========================================================================

    /**
     * @brief Get processed roads
     */
    const std::vector<ProcessedRoad>& GetRoads() const { return m_roads; }

    /**
     * @brief Get road by ID
     */
    const ProcessedRoad* GetRoad(int64_t id) const;

    /**
     * @brief Get all segments
     */
    const std::vector<RoadSegment>& GetSegments() const { return m_segments; }

    /**
     * @brief Get intersections
     */
    const std::vector<RoadIntersection>& GetIntersections() const { return m_intersections; }

    /**
     * @brief Get road graph
     */
    const RoadGraph& GetGraph() const { return m_graph; }
    RoadGraph& GetGraph() { return m_graph; }

    // ==========================================================================
    // Queries
    // ==========================================================================

    /**
     * @brief Find roads within radius of point
     */
    std::vector<int64_t> FindRoadsNear(const glm::vec2& point, float radius) const;

    /**
     * @brief Find nearest road to point
     * @return Road ID and distance
     */
    std::pair<int64_t, float> FindNearestRoad(const glm::vec2& point) const;

    /**
     * @brief Find nearest point on road network
     */
    glm::vec2 FindNearestPointOnRoad(const glm::vec2& point) const;

    /**
     * @brief Check if point is on a road
     */
    bool IsOnRoad(const glm::vec2& point, float tolerance = 1.0f) const;

    /**
     * @brief Get roads in bounding box
     */
    std::vector<int64_t> GetRoadsInBounds(const glm::vec2& min, const glm::vec2& max) const;

    // ==========================================================================
    // Rendering Data
    // ==========================================================================

    /**
     * @brief Road mesh vertex
     */
    struct RoadVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec3 normal;
        float roadType;  // For shader to select texture
    };

    /**
     * @brief Generate road mesh for rendering
     * @param minBounds Minimum bounds
     * @param maxBounds Maximum bounds
     * @return Vertices and indices
     */
    std::pair<std::vector<RoadVertex>, std::vector<uint32_t>> GenerateRoadMesh(
        const glm::vec2& minBounds,
        const glm::vec2& maxBounds) const;

    /**
     * @brief Generate road mesh for specific types
     */
    std::pair<std::vector<RoadVertex>, std::vector<uint32_t>> GenerateRoadMeshByType(
        RoadType type,
        const glm::vec2& minBounds,
        const glm::vec2& maxBounds) const;

    // ==========================================================================
    // Utilities
    // ==========================================================================

    /**
     * @brief Simplify road polyline (Douglas-Peucker)
     */
    static std::vector<glm::vec2> SimplifyPolyline(
        const std::vector<glm::vec2>& points,
        float tolerance);

    /**
     * @brief Get road priority (for intersection handling)
     */
    static int GetRoadPriority(RoadType type);

    /**
     * @brief Get speed limit for road type (km/h)
     */
    static float GetSpeedLimit(RoadType type);

private:
    /**
     * @brief Transform geographic coordinates to game coordinates
     */
    glm::vec2 TransformCoord(const GeoCoordinate& coord) const;

    /**
     * @brief Find intersection point between two segments
     */
    bool FindIntersection(
        const glm::vec2& a1, const glm::vec2& a2,
        const glm::vec2& b1, const glm::vec2& b2,
        glm::vec2& intersection) const;

    /**
     * @brief Distance from point to line segment
     */
    float PointToSegmentDistance(
        const glm::vec2& point,
        const glm::vec2& segStart,
        const glm::vec2& segEnd) const;

    CoordTransform m_transform;
    GeoCoordinate m_origin;
    float m_scale = 1.0f;

    std::vector<ProcessedRoad> m_roads;
    std::unordered_map<int64_t, size_t> m_roadIndex;  // ID -> index
    std::vector<RoadSegment> m_segments;
    std::vector<RoadIntersection> m_intersections;
    RoadGraph m_graph;
};

} // namespace Geo
} // namespace Vehement
