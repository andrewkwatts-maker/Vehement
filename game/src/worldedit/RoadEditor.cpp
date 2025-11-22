#include "RoadEditor.hpp"
#include "../world/TileMap.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

RoadEditor::RoadEditor() = default;

// =============================================================================
// Drawing
// =============================================================================

void RoadEditor::BeginPath(const glm::ivec2& startPos) {
    m_isDrawing = true;
    m_currentPath.clear();
    m_currentPath.push_back(startPos);
}

void RoadEditor::AddPoint(const glm::ivec2& pos) {
    if (!m_isDrawing) {
        BeginPath(pos);
        return;
    }

    // Don't add duplicate points
    if (!m_currentPath.empty() && m_currentPath.back() == pos) {
        return;
    }

    m_currentPath.push_back(pos);
}

void RoadEditor::EndPath() {
    if (!m_isDrawing || m_currentPath.size() < 2) {
        CancelPath();
        return;
    }

    CreateSegmentFromPath();
    m_isDrawing = false;
    m_currentPath.clear();
}

void RoadEditor::CancelPath() {
    m_isDrawing = false;
    m_currentPath.clear();
}

void RoadEditor::CreateSegmentFromPath() {
    for (size_t i = 0; i < m_currentPath.size() - 1; ++i) {
        RoadSegment segment;
        segment.start = glm::vec2(m_currentPath[i]);
        segment.end = glm::vec2(m_currentPath[i + 1]);
        segment.type = m_roadType;
        segment.width = m_width;
        segment.isBridge = m_bridgeMode;
        segment.isTunnel = m_tunnelMode;
        segment.elevation = (m_bridgeMode || m_tunnelMode) ? m_elevation : 0.0f;

        m_network.segments.push_back(segment);

        if (m_onRoadCreated) {
            m_onRoadCreated(segment);
        }
    }
}

// =============================================================================
// Road Network
// =============================================================================

void RoadEditor::ClearNetwork() {
    m_network.segments.clear();
    m_network.intersections.clear();
}

int RoadEditor::AutoConnectIntersections(float maxDistance) {
    int connections = 0;
    float maxDistSq = maxDistance * maxDistance;

    // Find endpoints
    std::vector<std::pair<glm::vec2, int>> endpoints;  // position, segment index

    for (size_t i = 0; i < m_network.segments.size(); ++i) {
        endpoints.emplace_back(m_network.segments[i].start, static_cast<int>(i));
        endpoints.emplace_back(m_network.segments[i].end, static_cast<int>(i));
    }

    // Find nearby endpoints from different segments
    for (size_t i = 0; i < endpoints.size(); ++i) {
        for (size_t j = i + 1; j < endpoints.size(); ++j) {
            if (endpoints[i].second == endpoints[j].second) continue;

            glm::vec2 diff = endpoints[i].first - endpoints[j].first;
            float distSq = glm::dot(diff, diff);

            if (distSq > 0.0f && distSq <= maxDistSq) {
                // Create connecting segment
                RoadSegment connector;
                connector.start = endpoints[i].first;
                connector.end = endpoints[j].first;
                connector.type = m_network.segments[endpoints[i].second].type;
                connector.width = std::min(
                    m_network.segments[endpoints[i].second].width,
                    m_network.segments[endpoints[j].second].width
                );

                m_network.segments.push_back(connector);
                ++connections;
            }
        }
    }

    return connections;
}

std::vector<glm::ivec2> RoadEditor::FindIntersections() const {
    std::vector<glm::ivec2> intersections;

    // Simple grid-based intersection detection
    std::unordered_map<int64_t, int> tileCounts;

    auto hashPos = [](int x, int y) -> int64_t {
        return (static_cast<int64_t>(x) << 32) | static_cast<uint32_t>(y);
    };

    for (const auto& segment : m_network.segments) {
        auto tiles = GetSegmentTiles(segment);
        for (const auto& tile : tiles) {
            ++tileCounts[hashPos(tile.x, tile.y)];
        }
    }

    // Tiles covered by multiple segments are intersections
    for (const auto& [hash, count] : tileCounts) {
        if (count > 1) {
            int x = static_cast<int>(hash >> 32);
            int y = static_cast<int>(hash & 0xFFFFFFFF);
            intersections.emplace_back(x, y);
        }
    }

    return intersections;
}

// =============================================================================
// Apply to Map
// =============================================================================

std::vector<std::pair<glm::ivec2, TileType>> RoadEditor::ApplyToMap(TileMap& map) {
    std::vector<std::pair<glm::ivec2, TileType>> changes;

    for (const auto& segment : m_network.segments) {
        TileType roadTile = GetRoadTileType(segment.type);
        auto tiles = GetSegmentTiles(segment);

        for (const auto& pos : tiles) {
            if (map.IsValidPosition(pos.x, pos.y)) {
                Tile& tile = map.GetTile(pos.x, pos.y);

                TileType oldType = tile.type;
                tile.type = roadTile;
                tile.isWall = false;
                tile.isWalkable = true;
                tile.movementCost = 0.5f;  // Roads are faster

                if (segment.isBridge) {
                    tile.isWall = true;
                    tile.wallHeight = segment.elevation;
                } else if (segment.isTunnel) {
                    // Tunnels are at ground level but marked special
                    tile.textureVariant = 1;  // Use variant for tunnel
                }

                changes.emplace_back(pos, oldType);
            }
        }
    }

    return changes;
}

std::vector<glm::ivec2> RoadEditor::GetAffectedTiles() const {
    std::vector<glm::ivec2> tiles;

    // Get tiles from current path being drawn
    if (m_isDrawing && m_currentPath.size() >= 2) {
        for (size_t i = 0; i < m_currentPath.size() - 1; ++i) {
            auto lineTiles = GetLineTiles(m_currentPath[i], m_currentPath[i + 1], m_width);
            tiles.insert(tiles.end(), lineTiles.begin(), lineTiles.end());
        }
    }

    // Get tiles from existing segments
    for (const auto& segment : m_network.segments) {
        auto segTiles = GetSegmentTiles(segment);
        tiles.insert(tiles.end(), segTiles.begin(), segTiles.end());
    }

    // Remove duplicates
    std::sort(tiles.begin(), tiles.end(), [](const glm::ivec2& a, const glm::ivec2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    return tiles;
}

std::vector<glm::ivec2> RoadEditor::GetSegmentTiles(const RoadSegment& segment) const {
    glm::ivec2 start(static_cast<int>(segment.start.x), static_cast<int>(segment.start.y));
    glm::ivec2 end(static_cast<int>(segment.end.x), static_cast<int>(segment.end.y));
    return GetLineTiles(start, end, segment.width);
}

std::vector<glm::ivec2> RoadEditor::GetLineTiles(
    const glm::ivec2& start, const glm::ivec2& end, int width) const {

    std::vector<glm::ivec2> tiles;

    // Bresenham's line with width
    int dx = std::abs(end.x - start.x);
    int dy = std::abs(end.y - start.y);
    int sx = (start.x < end.x) ? 1 : -1;
    int sy = (start.y < end.y) ? 1 : -1;
    int err = dx - dy;

    int x = start.x;
    int y = start.y;

    int halfWidth = width / 2;

    while (true) {
        // Add tiles for road width
        for (int wy = -halfWidth; wy <= halfWidth; ++wy) {
            for (int wx = -halfWidth; wx <= halfWidth; ++wx) {
                tiles.emplace_back(x + wx, y + wy);
            }
        }

        if (x == end.x && y == end.y) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }

    return tiles;
}

// =============================================================================
// Erase
// =============================================================================

bool RoadEditor::EraseRoadAt(const glm::ivec2& pos) {
    for (size_t i = 0; i < m_network.segments.size(); ++i) {
        auto tiles = GetSegmentTiles(m_network.segments[i]);

        for (const auto& tile : tiles) {
            if (tile == pos) {
                if (m_onRoadErased) {
                    m_onRoadErased(m_network.segments[i]);
                }
                m_network.segments.erase(m_network.segments.begin() + i);
                return true;
            }
        }
    }

    return false;
}

bool RoadEditor::EraseSegment(int index) {
    if (index < 0 || index >= static_cast<int>(m_network.segments.size())) {
        return false;
    }

    if (m_onRoadErased) {
        m_onRoadErased(m_network.segments[index]);
    }

    m_network.segments.erase(m_network.segments.begin() + index);
    return true;
}

// =============================================================================
// Serialization
// =============================================================================

std::string RoadEditor::ToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"name\": \"" << m_network.name << "\",\n";
    ss << "  \"segments\": [\n";

    for (size_t i = 0; i < m_network.segments.size(); ++i) {
        const auto& seg = m_network.segments[i];
        if (i > 0) ss << ",\n";

        ss << "    {\n";
        ss << "      \"start\": {\"x\": " << seg.start.x << ", \"y\": " << seg.start.y << "},\n";
        ss << "      \"end\": {\"x\": " << seg.end.x << ", \"y\": " << seg.end.y << "},\n";
        ss << "      \"type\": " << static_cast<int>(seg.type) << ",\n";
        ss << "      \"width\": " << seg.width << ",\n";
        ss << "      \"isBridge\": " << (seg.isBridge ? "true" : "false") << ",\n";
        ss << "      \"isTunnel\": " << (seg.isTunnel ? "true" : "false") << ",\n";
        ss << "      \"elevation\": " << seg.elevation << "\n";
        ss << "    }";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool RoadEditor::FromJson(const std::string& /*json*/) {
    // Simplified - would parse JSON in real implementation
    return true;
}

bool RoadEditor::IsNearRoad(const glm::ivec2& pos, float distance) const {
    float distSq = distance * distance;

    for (const auto& segment : m_network.segments) {
        glm::vec2 p(pos);
        glm::vec2 a = segment.start;
        glm::vec2 b = segment.end;

        // Point-to-line-segment distance
        glm::vec2 ab = b - a;
        glm::vec2 ap = p - a;

        float t = glm::clamp(glm::dot(ap, ab) / glm::dot(ab, ab), 0.0f, 1.0f);
        glm::vec2 closest = a + t * ab;

        if (glm::dot(p - closest, p - closest) <= distSq) {
            return true;
        }
    }

    return false;
}

} // namespace Vehement
