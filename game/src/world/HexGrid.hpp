#pragma once

#include "WorldConfig.hpp"
#include <array>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace Vehement {

/**
 * @brief Cube coordinates for hexagonal grid
 *
 * Uses the cube coordinate system where q + r + s = 0.
 * This provides easy neighbor calculation, distance computation,
 * and line drawing algorithms.
 */
struct HexCoord {
    int q = 0;  // Column (x-axis in cube coords)
    int r = 0;  // Row (y-axis in cube coords)
    int s = 0;  // Derived (z-axis in cube coords, always -q-r)

    // ========== Constructors ==========

    HexCoord() = default;

    HexCoord(int q_, int r_) : q(q_), r(r_), s(-q_ - r_) {}

    HexCoord(int q_, int r_, int s_) : q(q_), r(r_), s(s_) {
        // Validate cube constraint
        // assert(q + r + s == 0);
    }

    // ========== Operators ==========

    bool operator==(const HexCoord& other) const {
        return q == other.q && r == other.r && s == other.s;
    }

    bool operator!=(const HexCoord& other) const {
        return !(*this == other);
    }

    HexCoord operator+(const HexCoord& other) const {
        return HexCoord(q + other.q, r + other.r, s + other.s);
    }

    HexCoord operator-(const HexCoord& other) const {
        return HexCoord(q - other.q, r - other.r, s - other.s);
    }

    HexCoord operator*(int scale) const {
        return HexCoord(q * scale, r * scale, s * scale);
    }

    // ========== Offset Conversion ==========

    /**
     * @brief Create HexCoord from offset (column, row) coordinates
     * @param col Column in offset grid
     * @param row Row in offset grid
     * @param orient Hex orientation
     * @return HexCoord in cube coordinates
     */
    static HexCoord FromOffset(int col, int row, HexOrientation orient) {
        if (orient == HexOrientation::PointyTop) {
            // Odd-r offset coordinates (pointy-top)
            int q = col - (row - (row & 1)) / 2;
            int r = row;
            return HexCoord(q, r);
        } else {
            // Odd-q offset coordinates (flat-top)
            int q = col;
            int r = row - (col - (col & 1)) / 2;
            return HexCoord(q, r);
        }
    }

    /**
     * @brief Convert to offset (column, row) coordinates
     * @param orient Hex orientation
     * @return Offset coordinates as (column, row)
     */
    glm::ivec2 ToOffset(HexOrientation orient) const {
        if (orient == HexOrientation::PointyTop) {
            // Odd-r offset coordinates (pointy-top)
            int col = q + (r - (r & 1)) / 2;
            int row = r;
            return glm::ivec2(col, row);
        } else {
            // Odd-q offset coordinates (flat-top)
            int col = q;
            int row = r + (q - (q & 1)) / 2;
            return glm::ivec2(col, row);
        }
    }

    // ========== Neighbors ==========

    /**
     * @brief Direction offsets for 6 hex neighbors (cube coordinates)
     *
     * Order: E, NE, NW, W, SW, SE (clockwise from east for pointy-top)
     */
    static constexpr std::array<HexCoord, 6> Directions() {
        return {{
            HexCoord(1, 0, -1),   // East
            HexCoord(1, -1, 0),   // Northeast
            HexCoord(0, -1, 1),   // Northwest
            HexCoord(-1, 0, 1),   // West
            HexCoord(-1, 1, 0),   // Southwest
            HexCoord(0, 1, -1)    // Southeast
        }};
    }

    /**
     * @brief Get all 6 neighboring hex coordinates
     * @return Array of 6 neighbor coordinates
     */
    std::array<HexCoord, 6> GetNeighbors() const {
        std::array<HexCoord, 6> neighbors;
        auto dirs = Directions();
        for (int i = 0; i < 6; ++i) {
            neighbors[i] = *this + dirs[i];
        }
        return neighbors;
    }

    /**
     * @brief Get neighbor in a specific direction (0-5)
     * @param direction Direction index (0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE)
     * @return Neighbor coordinate
     */
    HexCoord GetNeighbor(int direction) const {
        auto dirs = Directions();
        return *this + dirs[direction % 6];
    }

    /**
     * @brief Get diagonal neighbors (6 diagonals between the 6 main neighbors)
     */
    std::array<HexCoord, 6> GetDiagonalNeighbors() const {
        return {{
            HexCoord(q + 2, r - 1, s - 1),
            HexCoord(q + 1, r - 2, s + 1),
            HexCoord(q - 1, r - 1, s + 2),
            HexCoord(q - 2, r + 1, s + 1),
            HexCoord(q - 1, r + 2, s - 1),
            HexCoord(q + 1, r + 1, s - 2)
        }};
    }

    // ========== Distance ==========

    /**
     * @brief Calculate Manhattan distance to another hex
     * @param other Target hex coordinate
     * @return Distance in hex steps
     */
    int DistanceTo(const HexCoord& other) const {
        return (std::abs(q - other.q) + std::abs(r - other.r) + std::abs(s - other.s)) / 2;
    }

    /**
     * @brief Get length/distance from origin
     */
    int Length() const {
        return (std::abs(q) + std::abs(r) + std::abs(s)) / 2;
    }

    // ========== Line Drawing ==========

    /**
     * @brief Draw a line between two hex coordinates using linear interpolation
     * @param a Start hex
     * @param b End hex
     * @return Vector of hex coordinates along the line
     */
    static std::vector<HexCoord> LineTo(HexCoord a, HexCoord b) {
        int distance = a.DistanceTo(b);
        if (distance == 0) {
            return {a};
        }

        std::vector<HexCoord> results;
        results.reserve(distance + 1);

        float step = 1.0f / static_cast<float>(distance);
        for (int i = 0; i <= distance; ++i) {
            float t = step * i;
            results.push_back(HexRound(HexLerp(a, b, t)));
        }

        return results;
    }

    // ========== Ring and Spiral Patterns ==========

    /**
     * @brief Get all hexes in a ring at specified radius from center
     * @param center Center hex coordinate
     * @param radius Ring radius (0 = just center)
     * @return Vector of hex coordinates in the ring
     */
    static std::vector<HexCoord> Ring(HexCoord center, int radius) {
        if (radius == 0) {
            return {center};
        }

        std::vector<HexCoord> results;
        results.reserve(6 * radius);

        // Start at the hex directly to the SW of center, scaled by radius
        HexCoord hex = center + Directions()[4] * radius;

        // Walk around the ring
        auto dirs = Directions();
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < radius; ++j) {
                results.push_back(hex);
                hex = hex + dirs[i];
            }
        }

        return results;
    }

    /**
     * @brief Get all hexes in a spiral pattern from center outward
     * @param center Center hex coordinate
     * @param radius Maximum radius of spiral
     * @return Vector of hex coordinates in spiral order (center first)
     */
    static std::vector<HexCoord> Spiral(HexCoord center, int radius) {
        std::vector<HexCoord> results;
        results.push_back(center);

        for (int r = 1; r <= radius; ++r) {
            auto ring = Ring(center, r);
            results.insert(results.end(), ring.begin(), ring.end());
        }

        return results;
    }

    /**
     * @brief Get all hexes within a certain radius (filled circle)
     * @param center Center hex coordinate
     * @param radius Radius of the area
     * @return Vector of all hex coordinates within radius
     */
    static std::vector<HexCoord> Range(HexCoord center, int radius) {
        std::vector<HexCoord> results;

        for (int q = -radius; q <= radius; ++q) {
            int r1 = std::max(-radius, -q - radius);
            int r2 = std::min(radius, -q + radius);
            for (int r = r1; r <= r2; ++r) {
                results.push_back(center + HexCoord(q, r, -q - r));
            }
        }

        return results;
    }

    // ========== Rotation ==========

    /**
     * @brief Rotate hex 60 degrees clockwise around origin
     */
    HexCoord RotateRight() const {
        return HexCoord(-r, -s, -q);
    }

    /**
     * @brief Rotate hex 60 degrees counter-clockwise around origin
     */
    HexCoord RotateLeft() const {
        return HexCoord(-s, -q, -r);
    }

    // ========== Hashing (for use in unordered containers) ==========

    struct Hash {
        size_t operator()(const HexCoord& hex) const {
            size_t h1 = std::hash<int>()(hex.q);
            size_t h2 = std::hash<int>()(hex.r);
            return h1 ^ (h2 << 1);
        }
    };

private:
    // Helper for line drawing - fractional hex for interpolation
    struct FractionalHex {
        float q, r, s;
    };

    static FractionalHex HexLerp(const HexCoord& a, const HexCoord& b, float t) {
        return {
            static_cast<float>(a.q) + (static_cast<float>(b.q) - static_cast<float>(a.q)) * t,
            static_cast<float>(a.r) + (static_cast<float>(b.r) - static_cast<float>(a.r)) * t,
            static_cast<float>(a.s) + (static_cast<float>(b.s) - static_cast<float>(a.s)) * t
        };
    }

    static HexCoord HexRound(const FractionalHex& h) {
        int q = static_cast<int>(std::round(h.q));
        int r = static_cast<int>(std::round(h.r));
        int s = static_cast<int>(std::round(h.s));

        float q_diff = std::abs(static_cast<float>(q) - h.q);
        float r_diff = std::abs(static_cast<float>(r) - h.r);
        float s_diff = std::abs(static_cast<float>(s) - h.s);

        if (q_diff > r_diff && q_diff > s_diff) {
            q = -r - s;
        } else if (r_diff > s_diff) {
            r = -q - s;
        } else {
            s = -q - r;
        }

        return HexCoord(q, r, s);
    }
};

/**
 * @brief Hexagonal grid system with world coordinate conversion
 *
 * Provides:
 * - Conversion between hex coordinates and world positions
 * - Hex corner calculation for rendering
 * - Line of sight calculation
 * - Configurable orientation (pointy-top or flat-top)
 */
class HexGrid {
public:
    HexGrid();
    explicit HexGrid(float hexSize, HexOrientation orientation = HexOrientation::PointyTop);
    ~HexGrid() = default;

    // ========== Configuration ==========

    /**
     * @brief Set hex size (outer radius)
     */
    void SetHexSize(float size) { m_hexSize = size; UpdateDimensions(); }
    float GetHexSize() const { return m_hexSize; }

    /**
     * @brief Set hex orientation
     */
    void SetOrientation(HexOrientation orientation) { m_orientation = orientation; UpdateDimensions(); }
    HexOrientation GetOrientation() const { return m_orientation; }

    /**
     * @brief Get derived dimensions
     */
    float GetInnerRadius() const { return m_innerRadius; }
    float GetHexWidth() const { return m_width; }
    float GetHexHeight() const { return m_height; }

    // ========== Coordinate Conversion ==========

    /**
     * @brief Convert hex coordinate to world position (center of hex)
     * @param hex Hex coordinate
     * @return World position (x, y) where y is the horizontal plane
     */
    glm::vec2 HexToWorld(const HexCoord& hex) const;

    /**
     * @brief Convert hex coordinate to 3D world position with Z level
     * @param hex Hex coordinate
     * @param zLevel Vertical level
     * @param tileSizeZ Vertical tile size
     * @return World position (x, y, z)
     */
    glm::vec3 HexToWorld3D(const HexCoord& hex, int zLevel, float tileSizeZ) const;

    /**
     * @brief Convert world position to hex coordinate
     * @param world World position (x, y)
     * @return Nearest hex coordinate
     */
    HexCoord WorldToHex(const glm::vec2& world) const;

    /**
     * @brief Convert 3D world position to hex coordinate
     * @param world World position
     * @return Nearest hex coordinate
     */
    HexCoord WorldToHex(const glm::vec3& world) const;

    /**
     * @brief Get the Z level from a world Y coordinate
     * @param worldY World Y coordinate
     * @param tileSizeZ Vertical tile size
     * @return Z level index
     */
    int WorldYToZLevel(float worldY, float tileSizeZ) const;

    // ========== Hex Corners (for rendering) ==========

    /**
     * @brief Get the 6 corner positions of a hex in world space
     * @param hex Hex coordinate
     * @return Array of 6 corner positions (x, y)
     */
    std::array<glm::vec2, 6> GetHexCorners(const HexCoord& hex) const;

    /**
     * @brief Get corner positions relative to hex center (no world offset)
     * @return Array of 6 corner offsets
     */
    std::array<glm::vec2, 6> GetCornerOffsets() const;

    /**
     * @brief Get corner angle for a specific corner (0-5)
     * @param corner Corner index
     * @return Angle in radians
     */
    float GetCornerAngle(int corner) const;

    // ========== Line of Sight ==========

    /**
     * @brief Check if there's a clear line of sight between two hexes
     * @param from Starting hex
     * @param to Target hex
     * @param isBlocking Function that returns true if a hex blocks sight
     * @return true if line of sight is clear
     */
    bool HasLineOfSight(const HexCoord& from, const HexCoord& to,
                        std::function<bool(const HexCoord&)> isBlocking) const;

    /**
     * @brief Get all hexes visible from a position within a range
     * @param origin Origin hex
     * @param range Maximum visibility range
     * @param isBlocking Function that returns true if a hex blocks sight
     * @return Vector of visible hex coordinates
     */
    std::vector<HexCoord> GetVisibleHexes(const HexCoord& origin, int range,
                                           std::function<bool(const HexCoord&)> isBlocking) const;

    // ========== Field of View ==========

    /**
     * @brief Calculate field of view using shadowcasting
     * @param origin Origin hex
     * @param range Maximum visibility range
     * @param isOpaque Function that returns true if a hex blocks light
     * @return Vector of visible hex coordinates
     */
    std::vector<HexCoord> CalculateFOV(const HexCoord& origin, int range,
                                        std::function<bool(const HexCoord&)> isOpaque) const;

    // ========== Utility ==========

    /**
     * @brief Check if a hex is within grid bounds
     * @param hex Hex coordinate
     * @param width Grid width
     * @param height Grid height
     * @return true if within bounds
     */
    bool IsInBounds(const HexCoord& hex, int width, int height) const;

    /**
     * @brief Get hexes intersected by a rectangle in world space
     * @param min Min corner of rectangle
     * @param max Max corner of rectangle
     * @return Vector of hex coordinates
     */
    std::vector<HexCoord> GetHexesInRect(const glm::vec2& min, const glm::vec2& max) const;

private:
    void UpdateDimensions();

    HexOrientation m_orientation = HexOrientation::PointyTop;
    float m_hexSize = 1.0f;        // Outer radius (center to corner)
    float m_innerRadius = 0.866f;  // Inner radius (center to edge) = size * sqrt(3)/2
    float m_width = 1.732f;        // Width of hex
    float m_height = 2.0f;         // Height of hex
};

} // namespace Vehement
