#include "HexGrid.hpp"
#include <cmath>
#include <queue>
#include <unordered_set>

namespace Vehement {

// ============================================================================
// HexGrid Implementation
// ============================================================================

HexGrid::HexGrid() {
    UpdateDimensions();
}

HexGrid::HexGrid(float hexSize, HexOrientation orientation)
    : m_orientation(orientation)
    , m_hexSize(hexSize) {
    UpdateDimensions();
}

void HexGrid::UpdateDimensions() {
    // Inner radius (center to middle of edge) = outer * sqrt(3)/2
    m_innerRadius = m_hexSize * 0.866025403784f;

    if (m_orientation == HexOrientation::PointyTop) {
        // Pointy-top: width is 2 * inner radius, height is 2 * outer radius
        m_width = m_innerRadius * 2.0f;
        m_height = m_hexSize * 2.0f;
    } else {
        // Flat-top: width is 2 * outer radius, height is 2 * inner radius
        m_width = m_hexSize * 2.0f;
        m_height = m_innerRadius * 2.0f;
    }
}

glm::vec2 HexGrid::HexToWorld(const HexCoord& hex) const {
    float x, y;

    if (m_orientation == HexOrientation::PointyTop) {
        // Pointy-top orientation
        x = m_hexSize * (std::sqrt(3.0f) * static_cast<float>(hex.q) +
                         std::sqrt(3.0f) / 2.0f * static_cast<float>(hex.r));
        y = m_hexSize * (3.0f / 2.0f * static_cast<float>(hex.r));
    } else {
        // Flat-top orientation
        x = m_hexSize * (3.0f / 2.0f * static_cast<float>(hex.q));
        y = m_hexSize * (std::sqrt(3.0f) / 2.0f * static_cast<float>(hex.q) +
                         std::sqrt(3.0f) * static_cast<float>(hex.r));
    }

    return glm::vec2(x, y);
}

glm::vec3 HexGrid::HexToWorld3D(const HexCoord& hex, int zLevel, float tileSizeZ) const {
    glm::vec2 xy = HexToWorld(hex);
    return glm::vec3(xy.x, static_cast<float>(zLevel) * tileSizeZ, xy.y);
}

HexCoord HexGrid::WorldToHex(const glm::vec2& world) const {
    float q, r;

    if (m_orientation == HexOrientation::PointyTop) {
        // Pointy-top orientation
        q = (std::sqrt(3.0f) / 3.0f * world.x - 1.0f / 3.0f * world.y) / m_hexSize;
        r = (2.0f / 3.0f * world.y) / m_hexSize;
    } else {
        // Flat-top orientation
        q = (2.0f / 3.0f * world.x) / m_hexSize;
        r = (-1.0f / 3.0f * world.x + std::sqrt(3.0f) / 3.0f * world.y) / m_hexSize;
    }

    // Convert fractional axial to cube coordinates and round
    float s = -q - r;

    int qi = static_cast<int>(std::round(q));
    int ri = static_cast<int>(std::round(r));
    int si = static_cast<int>(std::round(s));

    float q_diff = std::abs(static_cast<float>(qi) - q);
    float r_diff = std::abs(static_cast<float>(ri) - r);
    float s_diff = std::abs(static_cast<float>(si) - s);

    // Reset the component with the largest rounding error
    if (q_diff > r_diff && q_diff > s_diff) {
        qi = -ri - si;
    } else if (r_diff > s_diff) {
        ri = -qi - si;
    } else {
        si = -qi - ri;
    }

    return HexCoord(qi, ri, si);
}

HexCoord HexGrid::WorldToHex(const glm::vec3& world) const {
    // Use X and Z as the horizontal plane
    return WorldToHex(glm::vec2(world.x, world.z));
}

int HexGrid::WorldYToZLevel(float worldY, float tileSizeZ) const {
    if (tileSizeZ <= 0.0f) return 0;
    return static_cast<int>(std::floor(worldY / tileSizeZ));
}

std::array<glm::vec2, 6> HexGrid::GetHexCorners(const HexCoord& hex) const {
    glm::vec2 center = HexToWorld(hex);
    std::array<glm::vec2, 6> corners;
    auto offsets = GetCornerOffsets();

    for (int i = 0; i < 6; ++i) {
        corners[i] = center + offsets[i];
    }

    return corners;
}

std::array<glm::vec2, 6> HexGrid::GetCornerOffsets() const {
    std::array<glm::vec2, 6> offsets;

    for (int i = 0; i < 6; ++i) {
        float angle = GetCornerAngle(i);
        offsets[i] = glm::vec2(m_hexSize * std::cos(angle),
                               m_hexSize * std::sin(angle));
    }

    return offsets;
}

float HexGrid::GetCornerAngle(int corner) const {
    // Angle offset depends on orientation
    float startAngle = (m_orientation == HexOrientation::PointyTop)
        ? 30.0f   // Pointy-top starts at 30 degrees
        : 0.0f;   // Flat-top starts at 0 degrees

    // Convert to radians and add 60 degrees per corner
    return glm::radians(startAngle + 60.0f * static_cast<float>(corner));
}

bool HexGrid::HasLineOfSight(const HexCoord& from, const HexCoord& to,
                             std::function<bool(const HexCoord&)> isBlocking) const {
    auto line = HexCoord::LineTo(from, to);

    // Check each hex along the line (except start and end)
    for (size_t i = 1; i < line.size() - 1; ++i) {
        if (isBlocking(line[i])) {
            return false;
        }
    }

    return true;
}

std::vector<HexCoord> HexGrid::GetVisibleHexes(const HexCoord& origin, int range,
                                                std::function<bool(const HexCoord&)> isBlocking) const {
    std::vector<HexCoord> visible;
    visible.reserve(static_cast<size_t>(3 * range * (range + 1) + 1));

    // Get all hexes in range
    auto candidates = HexCoord::Range(origin, range);

    for (const auto& hex : candidates) {
        if (HasLineOfSight(origin, hex, isBlocking)) {
            visible.push_back(hex);
        }
    }

    return visible;
}

std::vector<HexCoord> HexGrid::CalculateFOV(const HexCoord& origin, int range,
                                            std::function<bool(const HexCoord&)> isOpaque) const {
    // Simplified recursive shadowcasting for hex grid
    // Uses a BFS approach with visibility tracking

    std::unordered_set<HexCoord, HexCoord::Hash> visible;
    visible.insert(origin);

    // For each ring, check visibility from the previous ring
    std::vector<HexCoord> currentRing = {origin};

    for (int r = 1; r <= range; ++r) {
        std::vector<HexCoord> nextRing;
        auto ring = HexCoord::Ring(origin, r);

        for (const auto& hex : ring) {
            // Check if any adjacent hex in the previous ring can see this hex
            bool canSee = false;
            auto neighbors = hex.GetNeighbors();

            for (const auto& neighbor : neighbors) {
                if (neighbor.DistanceTo(origin) < r) {
                    // Neighbor is closer to origin
                    if (visible.count(neighbor) > 0 && !isOpaque(neighbor)) {
                        canSee = true;
                        break;
                    }
                }
            }

            // Also check direct line of sight for edge cases
            if (!canSee) {
                canSee = HasLineOfSight(origin, hex, isOpaque);
            }

            if (canSee) {
                visible.insert(hex);
                nextRing.push_back(hex);
            }
        }

        currentRing = std::move(nextRing);
    }

    return std::vector<HexCoord>(visible.begin(), visible.end());
}

bool HexGrid::IsInBounds(const HexCoord& hex, int width, int height) const {
    glm::ivec2 offset = hex.ToOffset(m_orientation);
    return offset.x >= 0 && offset.x < width && offset.y >= 0 && offset.y < height;
}

std::vector<HexCoord> HexGrid::GetHexesInRect(const glm::vec2& min, const glm::vec2& max) const {
    std::vector<HexCoord> hexes;

    // Convert corners to hex coordinates
    HexCoord minHex = WorldToHex(min);
    HexCoord maxHex = WorldToHex(max);

    // Get the range of q and r values
    int minQ = std::min(minHex.q, maxHex.q) - 1;
    int maxQ = std::max(minHex.q, maxHex.q) + 1;
    int minR = std::min(minHex.r, maxHex.r) - 1;
    int maxR = std::max(minHex.r, maxHex.r) + 1;

    // Iterate over potential hexes and check if center is in rect
    for (int q = minQ; q <= maxQ; ++q) {
        for (int r = minR; r <= maxR; ++r) {
            HexCoord hex(q, r);
            glm::vec2 center = HexToWorld(hex);

            // Check if hex center is within rectangle (with some margin for hex size)
            if (center.x >= min.x - m_hexSize && center.x <= max.x + m_hexSize &&
                center.y >= min.y - m_hexSize && center.y <= max.y + m_hexSize) {
                hexes.push_back(hex);
            }
        }
    }

    return hexes;
}

} // namespace Vehement
