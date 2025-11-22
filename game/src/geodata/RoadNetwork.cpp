#include "RoadNetwork.hpp"
#include <algorithm>
#include <queue>
#include <cmath>

namespace Vehement {
namespace Geo {

// =============================================================================
// RoadGraph Implementation
// =============================================================================

void RoadGraph::AddNode(const Node& node) {
    m_nodes[node.id] = node;
}

void RoadGraph::AddEdge(const Edge& edge) {
    m_edges.push_back(edge);

    // Add to adjacency lists
    auto it = m_nodes.find(edge.fromNode);
    if (it != m_nodes.end()) {
        it->second.neighbors.emplace_back(edge.toNode, edge.distance);
    }

    // If not oneway, add reverse connection
    if (!edge.oneway) {
        auto it2 = m_nodes.find(edge.toNode);
        if (it2 != m_nodes.end()) {
            it2->second.neighbors.emplace_back(edge.fromNode, edge.distance);
        }
    }
}

const RoadGraph::Node* RoadGraph::GetNode(int64_t id) const {
    auto it = m_nodes.find(id);
    return it != m_nodes.end() ? &it->second : nullptr;
}

int64_t RoadGraph::FindNearestNode(const glm::vec2& position) const {
    int64_t nearestId = -1;
    float nearestDist = std::numeric_limits<float>::max();

    for (const auto& [id, node] : m_nodes) {
        float dist = glm::length(node.position - position);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestId = id;
        }
    }

    return nearestId;
}

std::vector<int64_t> RoadGraph::FindPath(int64_t startNode, int64_t endNode) const {
    if (m_nodes.find(startNode) == m_nodes.end() ||
        m_nodes.find(endNode) == m_nodes.end()) {
        return {};
    }

    // Dijkstra's algorithm
    std::unordered_map<int64_t, float> dist;
    std::unordered_map<int64_t, int64_t> prev;

    using PQEntry = std::pair<float, int64_t>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> pq;

    for (const auto& [id, _] : m_nodes) {
        dist[id] = std::numeric_limits<float>::max();
    }

    dist[startNode] = 0.0f;
    pq.push({0.0f, startNode});

    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();

        if (u == endNode) break;
        if (d > dist[u]) continue;

        const Node& node = m_nodes.at(u);
        for (const auto& [v, weight] : node.neighbors) {
            float alt = dist[u] + weight;
            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                pq.push({alt, v});
            }
        }
    }

    // Reconstruct path
    std::vector<int64_t> path;
    if (dist[endNode] < std::numeric_limits<float>::max()) {
        int64_t current = endNode;
        while (current != startNode) {
            path.push_back(current);
            current = prev[current];
        }
        path.push_back(startNode);
        std::reverse(path.begin(), path.end());
    }

    return path;
}

std::vector<int64_t> RoadGraph::FindFastestPath(int64_t startNode, int64_t endNode) const {
    // Similar to FindPath but using travel time instead of distance
    // For now, use distance-based path
    return FindPath(startNode, endNode);
}

void RoadGraph::Clear() {
    m_nodes.clear();
    m_edges.clear();
}

// =============================================================================
// RoadNetwork Implementation
// =============================================================================

RoadNetwork::RoadNetwork() {
    // Default transform: identity (lat/lon to game units 1:1)
    m_transform = [](const GeoCoordinate& coord) {
        return glm::vec2(static_cast<float>(coord.longitude),
                         static_cast<float>(coord.latitude));
    };
}

RoadNetwork::~RoadNetwork() = default;

void RoadNetwork::SetDefaultTransform(const GeoCoordinate& origin, float scale) {
    m_origin = origin;
    m_scale = scale;

    m_transform = [this](const GeoCoordinate& coord) {
        // Convert to meters from origin
        double dx = (coord.longitude - m_origin.longitude) *
                    std::cos(DegToRad(m_origin.latitude)) * 111320.0;
        double dy = (coord.latitude - m_origin.latitude) * 110540.0;

        return glm::vec2(static_cast<float>(dx * m_scale),
                         static_cast<float>(dy * m_scale));
    };
}

int RoadNetwork::ProcessRoads(const std::vector<GeoRoad>& roads) {
    int count = 0;
    for (const auto& road : roads) {
        ProcessRoad(road);
        ++count;
    }
    return count;
}

void RoadNetwork::ProcessRoad(const GeoRoad& road) {
    if (road.points.size() < 2) return;

    ProcessedRoad processed;
    processed.id = road.id;
    processed.name = road.name;
    processed.ref = road.ref;
    processed.type = road.type;
    processed.surface = road.surface;
    processed.width = road.GetEffectiveWidth() * m_scale;
    processed.lanes = road.GetEffectiveLanes();
    processed.oneway = road.oneway;
    processed.bridge = road.bridge;
    processed.tunnel = road.tunnel;
    processed.layer = road.layer;

    // Transform points
    processed.points.reserve(road.points.size());
    for (const auto& geoPoint : road.points) {
        processed.points.push_back(TransformCoord(geoPoint));
    }

    // Store road
    m_roadIndex[processed.id] = m_roads.size();
    m_roads.push_back(std::move(processed));
}

void RoadNetwork::BuildSegments() {
    m_segments.clear();

    for (auto& road : m_roads) {
        road.segments.clear();

        for (size_t i = 0; i + 1 < road.points.size(); ++i) {
            RoadSegment segment;
            segment.id = road.id;
            segment.segmentIndex = static_cast<int>(i);
            segment.start = road.points[i];
            segment.end = road.points[i + 1];
            segment.type = road.type;
            segment.width = road.width;
            segment.lanes = road.lanes;
            segment.oneway = road.oneway;
            segment.bridge = road.bridge;
            segment.tunnel = road.tunnel;
            segment.layer = road.layer;

            road.segments.push_back(segment);
            m_segments.push_back(segment);
        }
    }
}

void RoadNetwork::BuildIntersections() {
    m_intersections.clear();

    // Find points where roads meet
    std::unordered_map<std::string, RoadIntersection> intersectionMap;

    auto makeKey = [](const glm::vec2& p) {
        // Round to grid for matching
        int x = static_cast<int>(std::round(p.x * 10));
        int y = static_cast<int>(std::round(p.y * 10));
        return std::to_string(x) + "_" + std::to_string(y);
    };

    int64_t nextId = 1;

    for (const auto& road : m_roads) {
        if (road.points.empty()) continue;

        // Check start point
        std::string startKey = makeKey(road.points.front());
        auto& startIntersection = intersectionMap[startKey];
        if (startIntersection.id == 0) {
            startIntersection.id = nextId++;
            startIntersection.position = road.points.front();
        }
        startIntersection.connectedRoads.push_back(road.id);
        startIntersection.segmentIndices.push_back(0);

        // Check end point
        std::string endKey = makeKey(road.points.back());
        auto& endIntersection = intersectionMap[endKey];
        if (endIntersection.id == 0) {
            endIntersection.id = nextId++;
            endIntersection.position = road.points.back();
        }
        endIntersection.connectedRoads.push_back(road.id);
        endIntersection.segmentIndices.push_back(static_cast<int>(road.points.size()) - 1);
    }

    // Filter to only keep actual intersections (degree > 1)
    for (auto& [key, intersection] : intersectionMap) {
        if (intersection.GetDegree() > 1) {
            // Determine priority from connected roads
            int maxPriority = 0;
            for (int64_t roadId : intersection.connectedRoads) {
                const ProcessedRoad* road = GetRoad(roadId);
                if (road) {
                    maxPriority = std::max(maxPriority, GetRoadPriority(road->type));
                }
            }
            intersection.priority = maxPriority;

            m_intersections.push_back(std::move(intersection));
        }
    }
}

void RoadNetwork::BuildGraph() {
    m_graph.Clear();

    // Add intersection nodes
    for (const auto& intersection : m_intersections) {
        RoadGraph::Node node;
        node.id = intersection.id;
        node.position = intersection.position;
        m_graph.AddNode(node);
    }

    // Add road endpoints as nodes
    int64_t nextNodeId = m_intersections.empty() ? 1 :
        m_intersections.back().id + 1;

    std::unordered_map<std::string, int64_t> pointToNode;

    auto makeKey = [](const glm::vec2& p) {
        int x = static_cast<int>(std::round(p.x * 10));
        int y = static_cast<int>(std::round(p.y * 10));
        return std::to_string(x) + "_" + std::to_string(y);
    };

    // Map existing intersections
    for (const auto& intersection : m_intersections) {
        pointToNode[makeKey(intersection.position)] = intersection.id;
    }

    // Add road endpoints and edges
    for (const auto& road : m_roads) {
        if (road.points.size() < 2) continue;

        std::string startKey = makeKey(road.points.front());
        std::string endKey = makeKey(road.points.back());

        // Get or create start node
        int64_t startNodeId;
        auto it = pointToNode.find(startKey);
        if (it != pointToNode.end()) {
            startNodeId = it->second;
        } else {
            startNodeId = nextNodeId++;
            RoadGraph::Node node;
            node.id = startNodeId;
            node.position = road.points.front();
            m_graph.AddNode(node);
            pointToNode[startKey] = startNodeId;
        }

        // Get or create end node
        int64_t endNodeId;
        it = pointToNode.find(endKey);
        if (it != pointToNode.end()) {
            endNodeId = it->second;
        } else {
            endNodeId = nextNodeId++;
            RoadGraph::Node node;
            node.id = endNodeId;
            node.position = road.points.back();
            m_graph.AddNode(node);
            pointToNode[endKey] = endNodeId;
        }

        // Add edge
        RoadGraph::Edge edge;
        edge.fromNode = startNodeId;
        edge.toNode = endNodeId;
        edge.roadId = road.id;
        edge.distance = road.GetLength();
        edge.speedLimit = GetSpeedLimit(road.type);
        edge.oneway = road.oneway;
        edge.type = road.type;

        m_graph.AddEdge(edge);
    }
}

void RoadNetwork::ProcessAll(const std::vector<GeoRoad>& roads) {
    Clear();
    ProcessRoads(roads);
    BuildSegments();
    BuildIntersections();
    BuildGraph();
}

void RoadNetwork::Clear() {
    m_roads.clear();
    m_roadIndex.clear();
    m_segments.clear();
    m_intersections.clear();
    m_graph.Clear();
}

const ProcessedRoad* RoadNetwork::GetRoad(int64_t id) const {
    auto it = m_roadIndex.find(id);
    if (it != m_roadIndex.end() && it->second < m_roads.size()) {
        return &m_roads[it->second];
    }
    return nullptr;
}

std::vector<int64_t> RoadNetwork::FindRoadsNear(const glm::vec2& point, float radius) const {
    std::vector<int64_t> result;
    float radiusSq = radius * radius;

    for (const auto& road : m_roads) {
        for (const auto& p : road.points) {
            if (glm::dot(p - point, p - point) <= radiusSq) {
                result.push_back(road.id);
                break;
            }
        }
    }

    return result;
}

std::pair<int64_t, float> RoadNetwork::FindNearestRoad(const glm::vec2& point) const {
    int64_t nearestId = -1;
    float nearestDist = std::numeric_limits<float>::max();

    for (const auto& road : m_roads) {
        for (size_t i = 0; i + 1 < road.points.size(); ++i) {
            float dist = PointToSegmentDistance(point, road.points[i], road.points[i + 1]);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestId = road.id;
            }
        }
    }

    return {nearestId, nearestDist};
}

glm::vec2 RoadNetwork::FindNearestPointOnRoad(const glm::vec2& point) const {
    glm::vec2 nearestPoint = point;
    float nearestDist = std::numeric_limits<float>::max();

    for (const auto& road : m_roads) {
        for (size_t i = 0; i + 1 < road.points.size(); ++i) {
            const glm::vec2& a = road.points[i];
            const glm::vec2& b = road.points[i + 1];

            glm::vec2 ab = b - a;
            float t = glm::dot(point - a, ab) / glm::dot(ab, ab);
            t = std::clamp(t, 0.0f, 1.0f);

            glm::vec2 closest = a + t * ab;
            float dist = glm::length(point - closest);

            if (dist < nearestDist) {
                nearestDist = dist;
                nearestPoint = closest;
            }
        }
    }

    return nearestPoint;
}

bool RoadNetwork::IsOnRoad(const glm::vec2& point, float tolerance) const {
    auto [id, dist] = FindNearestRoad(point);
    return id >= 0 && dist <= tolerance;
}

std::vector<int64_t> RoadNetwork::GetRoadsInBounds(const glm::vec2& min, const glm::vec2& max) const {
    std::vector<int64_t> result;

    for (const auto& road : m_roads) {
        auto [roadMin, roadMax] = road.GetBounds();

        // Check AABB overlap
        if (roadMax.x >= min.x && roadMin.x <= max.x &&
            roadMax.y >= min.y && roadMin.y <= max.y) {
            result.push_back(road.id);
        }
    }

    return result;
}

std::pair<std::vector<RoadNetwork::RoadVertex>, std::vector<uint32_t>>
RoadNetwork::GenerateRoadMesh(const glm::vec2& minBounds, const glm::vec2& maxBounds) const {

    std::vector<RoadVertex> vertices;
    std::vector<uint32_t> indices;

    auto roadsInBounds = GetRoadsInBounds(minBounds, maxBounds);

    for (int64_t roadId : roadsInBounds) {
        const ProcessedRoad* road = GetRoad(roadId);
        if (!road || road->points.size() < 2) continue;

        float halfWidth = road->width * 0.5f;
        float typeValue = static_cast<float>(road->type) / static_cast<float>(RoadType::Count);

        // Generate vertices along the road
        for (size_t i = 0; i + 1 < road->points.size(); ++i) {
            const glm::vec2& p0 = road->points[i];
            const glm::vec2& p1 = road->points[i + 1];

            glm::vec2 dir = glm::normalize(p1 - p0);
            glm::vec2 perp(-dir.y, dir.x);

            float segmentLength = glm::length(p1 - p0);
            float u0 = 0.0f;  // Could accumulate for continuous UV
            float u1 = segmentLength / road->width;

            uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

            // Four vertices per segment
            RoadVertex v0, v1, v2, v3;

            v0.position = glm::vec3(p0 - perp * halfWidth, 0.0f);
            v0.texCoord = glm::vec2(0.0f, u0);
            v0.normal = glm::vec3(0, 0, 1);
            v0.roadType = typeValue;

            v1.position = glm::vec3(p0 + perp * halfWidth, 0.0f);
            v1.texCoord = glm::vec2(1.0f, u0);
            v1.normal = glm::vec3(0, 0, 1);
            v1.roadType = typeValue;

            v2.position = glm::vec3(p1 - perp * halfWidth, 0.0f);
            v2.texCoord = glm::vec2(0.0f, u1);
            v2.normal = glm::vec3(0, 0, 1);
            v2.roadType = typeValue;

            v3.position = glm::vec3(p1 + perp * halfWidth, 0.0f);
            v3.texCoord = glm::vec2(1.0f, u1);
            v3.normal = glm::vec3(0, 0, 1);
            v3.roadType = typeValue;

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);

            // Two triangles
            indices.push_back(baseIndex + 0);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 1);

            indices.push_back(baseIndex + 1);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 3);
        }
    }

    return {vertices, indices};
}

std::pair<std::vector<RoadNetwork::RoadVertex>, std::vector<uint32_t>>
RoadNetwork::GenerateRoadMeshByType(RoadType type,
                                      const glm::vec2& minBounds,
                                      const glm::vec2& maxBounds) const {
    std::vector<RoadVertex> vertices;
    std::vector<uint32_t> indices;

    for (const auto& road : m_roads) {
        if (road.type != type) continue;

        auto [roadMin, roadMax] = road.GetBounds();
        if (roadMax.x < minBounds.x || roadMin.x > maxBounds.x ||
            roadMax.y < minBounds.y || roadMin.y > maxBounds.y) {
            continue;
        }

        // Generate mesh (similar to above)
        // ... abbreviated for brevity
    }

    return {vertices, indices};
}

std::vector<glm::vec2> RoadNetwork::SimplifyPolyline(
    const std::vector<glm::vec2>& points,
    float tolerance) {

    if (points.size() < 3) return points;

    // Douglas-Peucker algorithm
    std::vector<bool> keep(points.size(), false);
    keep[0] = true;
    keep[points.size() - 1] = true;

    std::function<void(size_t, size_t)> simplify = [&](size_t start, size_t end) {
        if (end <= start + 1) return;

        const glm::vec2& p0 = points[start];
        const glm::vec2& p1 = points[end];
        glm::vec2 line = p1 - p0;
        float lineLen = glm::length(line);

        float maxDist = 0.0f;
        size_t maxIdx = start;

        for (size_t i = start + 1; i < end; ++i) {
            // Distance from point to line
            glm::vec2 v = points[i] - p0;
            float dist = lineLen > 0.001f ?
                std::abs(v.x * line.y - v.y * line.x) / lineLen :
                glm::length(v);

            if (dist > maxDist) {
                maxDist = dist;
                maxIdx = i;
            }
        }

        if (maxDist > tolerance) {
            keep[maxIdx] = true;
            simplify(start, maxIdx);
            simplify(maxIdx, end);
        }
    };

    simplify(0, points.size() - 1);

    std::vector<glm::vec2> result;
    for (size_t i = 0; i < points.size(); ++i) {
        if (keep[i]) result.push_back(points[i]);
    }

    return result;
}

int RoadNetwork::GetRoadPriority(RoadType type) {
    switch (type) {
        case RoadType::Motorway:
        case RoadType::MotorwayLink:
            return 10;
        case RoadType::Trunk:
        case RoadType::TrunkLink:
            return 9;
        case RoadType::Primary:
        case RoadType::PrimaryLink:
            return 8;
        case RoadType::Secondary:
        case RoadType::SecondaryLink:
            return 7;
        case RoadType::Tertiary:
        case RoadType::TertiaryLink:
            return 6;
        case RoadType::Residential:
            return 5;
        case RoadType::Unclassified:
            return 4;
        case RoadType::Service:
            return 3;
        case RoadType::LivingStreet:
            return 2;
        default:
            return 1;
    }
}

float RoadNetwork::GetSpeedLimit(RoadType type) {
    switch (type) {
        case RoadType::Motorway:
            return 120.0f;
        case RoadType::Trunk:
            return 100.0f;
        case RoadType::Primary:
            return 80.0f;
        case RoadType::Secondary:
            return 60.0f;
        case RoadType::Tertiary:
            return 50.0f;
        case RoadType::Residential:
        case RoadType::LivingStreet:
            return 30.0f;
        case RoadType::Service:
            return 20.0f;
        default:
            return 50.0f;
    }
}

glm::vec2 RoadNetwork::TransformCoord(const GeoCoordinate& coord) const {
    return m_transform(coord);
}

bool RoadNetwork::FindIntersection(
    const glm::vec2& a1, const glm::vec2& a2,
    const glm::vec2& b1, const glm::vec2& b2,
    glm::vec2& intersection) const {

    glm::vec2 r = a2 - a1;
    glm::vec2 s = b2 - b1;

    float rxs = r.x * s.y - r.y * s.x;
    if (std::abs(rxs) < 0.0001f) return false;  // Parallel

    glm::vec2 qp = b1 - a1;
    float t = (qp.x * s.y - qp.y * s.x) / rxs;
    float u = (qp.x * r.y - qp.y * r.x) / rxs;

    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        intersection = a1 + t * r;
        return true;
    }

    return false;
}

float RoadNetwork::PointToSegmentDistance(
    const glm::vec2& point,
    const glm::vec2& segStart,
    const glm::vec2& segEnd) const {

    glm::vec2 v = segEnd - segStart;
    glm::vec2 w = point - segStart;

    float c1 = glm::dot(w, v);
    if (c1 <= 0) return glm::length(point - segStart);

    float c2 = glm::dot(v, v);
    if (c2 <= c1) return glm::length(point - segEnd);

    float b = c1 / c2;
    glm::vec2 pb = segStart + b * v;
    return glm::length(point - pb);
}

} // namespace Geo
} // namespace Vehement
