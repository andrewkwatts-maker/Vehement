#include "RoadGenerator.hpp"
#include <algorithm>
#include <queue>
#include <cmath>

namespace Vehement {

RoadGenerator::RoadGenerator() {
}

PCGStageResult RoadGenerator::Generate(PCGContext& context, PCGMode mode) {
    PCGStageResult result;
    result.success = true;

    // Update params from string parameters
    m_params.useRealData = GetParamBool("useRealData", m_params.useRealData);
    m_params.generateProcedural = GetParamBool("generateProcedural", m_params.generateProcedural);
    m_params.addSidewalks = GetParamBool("addSidewalks", m_params.addSidewalks);

    try {
        std::vector<RoadSegment> segments;
        std::vector<RoadIntersection> intersections;

        // Convert real-world road data
        if (m_params.useRealData) {
            ConvertGeoRoads(context, segments);
        }

        // Generate procedural roads if needed
        if (m_params.generateProcedural || segments.empty()) {
            GenerateProceduralRoads(context, segments);
        }

        // Connect disconnected segments
        if (m_params.connectDisconnected && mode == PCGMode::Final) {
            ConnectRoads(context, segments);
        }

        // Find intersections
        FindIntersections(segments, intersections);

        // Rasterize roads to tiles
        RasterizeRoads(context, segments);

        // Smooth intersections
        if (m_params.smoothIntersections) {
            SmoothIntersections(context, intersections);
        }

        // Add sidewalks
        if (m_params.addSidewalks && mode == PCGMode::Final) {
            AddSidewalks(context);
        }

        result.itemsGenerated = static_cast<int>(segments.size());

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }

    return result;
}

void RoadGenerator::ConvertGeoRoads(PCGContext& context, std::vector<RoadSegment>& segments) {
    // Get roads from geo data via context
    auto nearbyRoads = context.GetNearbyRoads(context.GetWidth() / 2, context.GetHeight() / 2,
                                               static_cast<float>(std::max(context.GetWidth(), context.GetHeight())));

    for (const auto* road : nearbyRoads) {
        if (road->points.size() < 2) continue;

        // Convert each segment of the road
        for (size_t i = 1; i < road->points.size(); ++i) {
            RoadSegment segment;
            segment.start = glm::ivec2(static_cast<int>(road->points[i - 1].x),
                                        static_cast<int>(road->points[i - 1].y));
            segment.end = glm::ivec2(static_cast<int>(road->points[i].x),
                                      static_cast<int>(road->points[i].y));
            segment.type = road->type;
            segment.width = GetWidthForRoadType(road->type);

            // Check if segment is within bounds
            if (context.InBounds(segment.start.x, segment.start.y) ||
                context.InBounds(segment.end.x, segment.end.y)) {
                segments.push_back(segment);
            }
        }
    }
}

void RoadGenerator::GenerateProceduralRoads(PCGContext& context, std::vector<RoadSegment>& segments) {
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Generate simple grid pattern
    int gridSpacing = 15 + context.RandomInt(0, 10);

    // Horizontal roads
    for (int y = gridSpacing; y < height - gridSpacing; y += gridSpacing) {
        // Add some variation
        y += context.RandomInt(-3, 3);
        if (y < 0 || y >= height) continue;

        RoadSegment segment;
        segment.start = glm::ivec2(0, y);
        segment.end = glm::ivec2(width - 1, y);
        segment.type = (y == height / 2) ? RoadType::MainRoad : RoadType::SecondaryRoad;
        segment.width = GetWidthForRoadType(segment.type);
        segments.push_back(segment);
    }

    // Vertical roads
    for (int x = gridSpacing; x < width - gridSpacing; x += gridSpacing) {
        x += context.RandomInt(-3, 3);
        if (x < 0 || x >= width) continue;

        RoadSegment segment;
        segment.start = glm::ivec2(x, 0);
        segment.end = glm::ivec2(x, height - 1);
        segment.type = (x == width / 2) ? RoadType::MainRoad : RoadType::SecondaryRoad;
        segment.width = GetWidthForRoadType(segment.type);
        segments.push_back(segment);
    }
}

void RoadGenerator::FindIntersections(const std::vector<RoadSegment>& segments,
                                        std::vector<RoadIntersection>& intersections) {
    // Simple intersection detection at segment endpoints
    std::unordered_map<int64_t, RoadIntersection> intersectionMap;

    auto posKey = [](int x, int y) -> int64_t {
        return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
    };

    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];

        // Check start point
        int64_t startKey = posKey(seg.start.x, seg.start.y);
        auto& startIntersection = intersectionMap[startKey];
        startIntersection.position = seg.start;
        startIntersection.connectedSegments.push_back(static_cast<int>(i));
        startIntersection.degree++;

        // Check end point
        int64_t endKey = posKey(seg.end.x, seg.end.y);
        auto& endIntersection = intersectionMap[endKey];
        endIntersection.position = seg.end;
        endIntersection.connectedSegments.push_back(static_cast<int>(i));
        endIntersection.degree++;
    }

    // Extract intersections with degree > 2 (actual intersections)
    for (auto& [key, intersection] : intersectionMap) {
        if (intersection.degree >= 2) {
            intersections.push_back(intersection);
        }
    }
}

void RoadGenerator::RasterizeRoads(PCGContext& context, const std::vector<RoadSegment>& segments) {
    for (const auto& segment : segments) {
        TileType tile = GetTileForRoadType(segment.type);
        RasterizeLine(context, segment.start, segment.end, tile, segment.width);
    }
}

void RoadGenerator::RasterizeLine(PCGContext& context, glm::ivec2 start, glm::ivec2 end,
                                   TileType tile, int width) {
    // Bresenham's line with width
    int dx = std::abs(end.x - start.x);
    int dy = std::abs(end.y - start.y);
    int sx = start.x < end.x ? 1 : -1;
    int sy = start.y < end.y ? 1 : -1;
    int err = dx - dy;

    int x = start.x, y = start.y;
    int halfWidth = width / 2;

    while (true) {
        // Draw thick line
        for (int oy = -halfWidth; oy <= halfWidth; ++oy) {
            for (int ox = -halfWidth; ox <= halfWidth; ++ox) {
                int px = x + ox;
                int py = y + oy;
                if (context.InBounds(px, py)) {
                    context.SetTile(px, py, tile);
                    context.MarkOccupied(px, py);
                }
            }
        }

        if (x == end.x && y == end.y) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
}

void RoadGenerator::ConnectRoads(PCGContext& context, std::vector<RoadSegment>& segments) {
    // Find disconnected road endpoints and connect them
    if (segments.size() < 2) return;

    // Build list of endpoints
    std::vector<std::pair<glm::ivec2, int>> endpoints;  // position, segment index
    for (size_t i = 0; i < segments.size(); ++i) {
        endpoints.push_back({segments[i].start, static_cast<int>(i)});
        endpoints.push_back({segments[i].end, static_cast<int>(i)});
    }

    // Find isolated endpoints and connect to nearest
    for (size_t i = 0; i < endpoints.size(); ++i) {
        bool connected = false;
        const auto& ep = endpoints[i];

        // Check if this endpoint connects to any other
        for (size_t j = 0; j < endpoints.size(); ++j) {
            if (i == j) continue;
            if (endpoints[i].second == endpoints[j].second) continue;  // Same segment

            float dist = context.Distance(ep.first.x, ep.first.y,
                                           endpoints[j].first.x, endpoints[j].first.y);
            if (dist < 3.0f) {
                connected = true;
                break;
            }
        }

        // If not connected, find nearest and create connection
        if (!connected) {
            float nearestDist = 1000000.0f;
            int nearestIdx = -1;

            for (size_t j = 0; j < endpoints.size(); ++j) {
                if (endpoints[i].second == endpoints[j].second) continue;

                float dist = context.Distance(ep.first.x, ep.first.y,
                                               endpoints[j].first.x, endpoints[j].first.y);
                if (dist < nearestDist && dist > 3.0f && dist < 30.0f) {
                    nearestDist = dist;
                    nearestIdx = static_cast<int>(j);
                }
            }

            if (nearestIdx >= 0) {
                RoadSegment connection;
                connection.start = ep.first;
                connection.end = endpoints[nearestIdx].first;
                connection.type = RoadType::ResidentialStreet;
                connection.width = m_params.residentialWidth;
                segments.push_back(connection);
            }
        }
    }
}

void RoadGenerator::AddSidewalks(PCGContext& context) {
    int width = context.GetWidth();
    int height = context.GetHeight();

    // Find road tiles and add sidewalks adjacent to them
    std::vector<glm::ivec2> sidewalkPositions;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            TileType tile = context.GetTile(x, y);

            // Check if this is a road tile
            bool isRoad = (tile == m_params.highwayTile ||
                           tile == m_params.mainRoadTile ||
                           tile == m_params.secondaryRoadTile ||
                           tile == m_params.residentialTile);

            if (isRoad) {
                // Check neighbors for potential sidewalk placement
                int dx[] = {-1, 1, 0, 0};
                int dy[] = {0, 0, -1, 1};

                for (int i = 0; i < 4; ++i) {
                    int nx = x + dx[i];
                    int ny = y + dy[i];

                    if (context.InBounds(nx, ny) && !context.IsOccupied(nx, ny)) {
                        sidewalkPositions.push_back({nx, ny});
                    }
                }
            }
        }
    }

    // Place sidewalks
    for (const auto& pos : sidewalkPositions) {
        context.SetTile(pos.x, pos.y, m_params.sidewalkTile);
        context.MarkOccupied(pos.x, pos.y);
    }
}

void RoadGenerator::SmoothIntersections(PCGContext& context,
                                         const std::vector<RoadIntersection>& intersections) {
    // Ensure intersection tiles are consistent
    for (const auto& intersection : intersections) {
        int x = intersection.position.x;
        int y = intersection.position.y;

        // Fill a small area around intersection
        int radius = 2;
        TileType roadTile = m_params.mainRoadTile;

        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                int px = x + dx;
                int py = y + dy;

                if (context.InBounds(px, py)) {
                    // Only fill if it's already a road
                    TileType existing = context.GetTile(px, py);
                    if (existing == m_params.highwayTile ||
                        existing == m_params.mainRoadTile ||
                        existing == m_params.secondaryRoadTile ||
                        existing == m_params.residentialTile) {
                        context.SetTile(px, py, roadTile);
                    }
                }
            }
        }
    }
}

TileType RoadGenerator::GetTileForRoadType(RoadType type) const {
    switch (type) {
        case RoadType::Highway: return m_params.highwayTile;
        case RoadType::MainRoad: return m_params.mainRoadTile;
        case RoadType::SecondaryRoad: return m_params.secondaryRoadTile;
        case RoadType::ResidentialStreet: return m_params.residentialTile;
        case RoadType::Path: return m_params.pathTile;
        default: return m_params.residentialTile;
    }
}

int RoadGenerator::GetWidthForRoadType(RoadType type) const {
    switch (type) {
        case RoadType::Highway: return m_params.highwayWidth;
        case RoadType::MainRoad: return m_params.mainRoadWidth;
        case RoadType::SecondaryRoad: return m_params.secondaryRoadWidth;
        case RoadType::ResidentialStreet: return m_params.residentialWidth;
        case RoadType::Path: return m_params.pathWidth;
        default: return m_params.residentialWidth;
    }
}

std::vector<glm::ivec2> RoadGenerator::FindPath(PCGContext& context, glm::ivec2 start, glm::ivec2 end) {
    // Use A* pathfinding from context
    return context.FindPath(start.x, start.y, end.x, end.y);
}

} // namespace Vehement
