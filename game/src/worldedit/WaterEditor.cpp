#include "WaterEditor.hpp"
#include "../world/TileMap.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

WaterEditor::WaterEditor() = default;

// =============================================================================
// Polygon Drawing
// =============================================================================

void WaterEditor::BeginPolygon(const std::string& name) {
    m_currentPolygon = WaterPolygon{};
    m_currentPolygon.name = name;
    m_currentPolygon.type = m_waterType;
    m_currentPolygon.waterLevel = m_waterLevel;
    m_currentPolygon.flowDirection = m_flowDirection;
    m_currentPolygon.flowSpeed = m_flowSpeed;
    m_isDrawing = true;
}

void WaterEditor::AddVertex(const glm::vec2& position, float depth) {
    if (!m_isDrawing) {
        BeginPolygon("Water Body " + std::to_string(m_waterBodies.size() + 1));
    }

    WaterVertex vertex;
    vertex.position = position;
    vertex.depth = depth;
    m_currentPolygon.vertices.push_back(vertex);
}

void WaterEditor::RemoveLastVertex() {
    if (!m_currentPolygon.vertices.empty()) {
        m_currentPolygon.vertices.pop_back();
    }
}

bool WaterEditor::FinishPolygon() {
    if (m_currentPolygon.vertices.size() < 3) {
        CancelPolygon();
        return false;
    }

    m_waterBodies.push_back(m_currentPolygon);

    if (m_onWaterBodyCreated) {
        m_onWaterBodyCreated(m_currentPolygon);
    }

    m_isDrawing = false;
    m_currentPolygon = WaterPolygon{};
    return true;
}

void WaterEditor::CancelPolygon() {
    m_isDrawing = false;
    m_currentPolygon = WaterPolygon{};
}

// =============================================================================
// Water Level
// =============================================================================

void WaterEditor::SetPolygonWaterLevel(int index, float level) {
    if (index >= 0 && index < static_cast<int>(m_waterBodies.size())) {
        m_waterBodies[index].waterLevel = level;

        if (m_onWaterBodyModified) {
            m_onWaterBodyModified(m_waterBodies[index]);
        }
    }
}

// =============================================================================
// River Flow
// =============================================================================

void WaterEditor::SetFlowDirection(const glm::vec2& direction) {
    float length = glm::length(direction);
    if (length > 0.0f) {
        m_flowDirection = direction / length;
    }

    if (m_isDrawing) {
        m_currentPolygon.flowDirection = m_flowDirection;
    }
}

void WaterEditor::SetFlowFromPoints(const glm::vec2& from, const glm::vec2& to) {
    SetFlowDirection(to - from);
}

// =============================================================================
// Water Bodies Management
// =============================================================================

bool WaterEditor::DeleteWaterBody(int index) {
    if (index < 0 || index >= static_cast<int>(m_waterBodies.size())) {
        return false;
    }

    m_waterBodies.erase(m_waterBodies.begin() + index);

    if (m_selectedIndex == index) {
        m_selectedIndex = -1;
    } else if (m_selectedIndex > index) {
        --m_selectedIndex;
    }

    return true;
}

void WaterEditor::ClearAllWaterBodies() {
    m_waterBodies.clear();
    m_selectedIndex = -1;
}

void WaterEditor::SelectWaterBody(int index) {
    if (index >= -1 && index < static_cast<int>(m_waterBodies.size())) {
        m_selectedIndex = index;
    }
}

// =============================================================================
// Apply to Map
// =============================================================================

std::vector<std::pair<glm::ivec2, TileType>> WaterEditor::ApplyToMap(TileMap& map) {
    std::vector<std::pair<glm::ivec2, TileType>> changes;

    for (const auto& water : m_waterBodies) {
        // Get water tiles
        auto waterTiles = GetWaterTiles(water);

        for (const auto& pos : waterTiles) {
            if (map.IsValidPosition(pos.x, pos.y)) {
                Tile& tile = map.GetTile(pos.x, pos.y);
                changes.emplace_back(pos, tile.type);

                tile.type = TileType::Water1;
                tile.isWall = false;
                tile.isWalkable = true;
                tile.movementCost = 2.0f;  // Slow in water
                tile.animation = TileAnimation::Water;

                if (water.isDeep) {
                    tile.isDamaging = true;
                    tile.damagePerSecond = water.damagePerSecond;
                }
            }
        }

        // Apply shore blending
        auto shoreTiles = GetShoreTiles(water);

        for (const auto& pos : shoreTiles) {
            if (map.IsValidPosition(pos.x, pos.y)) {
                Tile& tile = map.GetTile(pos.x, pos.y);

                // Don't overwrite water tiles
                if (tile.type != TileType::Water1) {
                    changes.emplace_back(pos, tile.type);
                    tile.type = m_shoreSettings.shoreType;
                }
            }
        }
    }

    return changes;
}

std::vector<glm::ivec2> WaterEditor::GetWaterTiles(const WaterPolygon& polygon) const {
    return RasterizePolygon(polygon.vertices);
}

std::vector<glm::ivec2> WaterEditor::GetShoreTiles(const WaterPolygon& polygon) const {
    std::vector<glm::ivec2> shoreTiles;
    auto waterTiles = GetWaterTiles(polygon);

    // Create set for fast lookup
    std::set<std::pair<int, int>> waterSet;
    for (const auto& tile : waterTiles) {
        waterSet.insert({tile.x, tile.y});
    }

    // Find tiles adjacent to water but not in water
    for (const auto& tile : waterTiles) {
        for (int dy = -m_shoreSettings.blendWidth; dy <= m_shoreSettings.blendWidth; ++dy) {
            for (int dx = -m_shoreSettings.blendWidth; dx <= m_shoreSettings.blendWidth; ++dx) {
                if (dx == 0 && dy == 0) continue;

                int nx = tile.x + dx;
                int ny = tile.y + dy;

                if (waterSet.find({nx, ny}) == waterSet.end()) {
                    shoreTiles.emplace_back(nx, ny);
                }
            }
        }
    }

    // Remove duplicates
    std::sort(shoreTiles.begin(), shoreTiles.end(),
        [](const glm::ivec2& a, const glm::ivec2& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });
    shoreTiles.erase(std::unique(shoreTiles.begin(), shoreTiles.end()), shoreTiles.end());

    return shoreTiles;
}

bool WaterEditor::IsPointInWater(const glm::vec2& point) const {
    for (const auto& water : m_waterBodies) {
        if (IsPointInPolygon(point, water.vertices)) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Serialization
// =============================================================================

std::string WaterEditor::ToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"waterBodies\": [\n";

    for (size_t i = 0; i < m_waterBodies.size(); ++i) {
        const auto& water = m_waterBodies[i];
        if (i > 0) ss << ",\n";

        ss << "    {\n";
        ss << "      \"name\": \"" << water.name << "\",\n";
        ss << "      \"type\": " << static_cast<int>(water.type) << ",\n";
        ss << "      \"waterLevel\": " << water.waterLevel << ",\n";
        ss << "      \"flowDirection\": {\"x\": " << water.flowDirection.x
           << ", \"y\": " << water.flowDirection.y << "},\n";
        ss << "      \"flowSpeed\": " << water.flowSpeed << ",\n";
        ss << "      \"isDeep\": " << (water.isDeep ? "true" : "false") << ",\n";
        ss << "      \"vertices\": [\n";

        for (size_t j = 0; j < water.vertices.size(); ++j) {
            const auto& v = water.vertices[j];
            if (j > 0) ss << ",\n";
            ss << "        {\"x\": " << v.position.x
               << ", \"y\": " << v.position.y
               << ", \"depth\": " << v.depth << "}";
        }

        ss << "\n      ]\n    }";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool WaterEditor::FromJson(const std::string& /*json*/) {
    // Simplified - would parse JSON in real implementation
    return true;
}

// =============================================================================
// Private Helpers
// =============================================================================

bool WaterEditor::IsPointInPolygon(
    const glm::vec2& point,
    const std::vector<WaterVertex>& vertices) const {

    if (vertices.size() < 3) return false;

    // Ray casting algorithm
    bool inside = false;
    size_t j = vertices.size() - 1;

    for (size_t i = 0; i < vertices.size(); i++) {
        const glm::vec2& pi = vertices[i].position;
        const glm::vec2& pj = vertices[j].position;

        if ((pi.y > point.y) != (pj.y > point.y) &&
            point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x) {
            inside = !inside;
        }

        j = i;
    }

    return inside;
}

std::vector<glm::ivec2> WaterEditor::RasterizePolygon(
    const std::vector<WaterVertex>& vertices) const {

    std::vector<glm::ivec2> tiles;

    if (vertices.size() < 3) return tiles;

    // Find bounding box
    float minX = vertices[0].position.x;
    float maxX = vertices[0].position.x;
    float minY = vertices[0].position.y;
    float maxY = vertices[0].position.y;

    for (const auto& v : vertices) {
        minX = std::min(minX, v.position.x);
        maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
    }

    // Scan each tile in bounding box
    for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y) {
        for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); ++x) {
            glm::vec2 center(x + 0.5f, y + 0.5f);
            if (IsPointInPolygon(center, vertices)) {
                tiles.emplace_back(x, y);
            }
        }
    }

    return tiles;
}

} // namespace Vehement
