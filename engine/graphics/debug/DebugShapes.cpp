/**
 * @file DebugShapes.cpp
 * @brief Additional debug shape utilities and geometry helpers
 *
 * This file contains standalone helper functions for generating debug geometry
 * that can be used independently or in conjunction with DebugDraw.
 */

#include "graphics/debug/DebugDraw.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace Nova {
namespace DebugShapes {

// ============================================================================
// Color Utilities
// ============================================================================

/**
 * @brief Standard debug colors for consistent visualization
 */
namespace Colors {
    constexpr glm::vec4 Red     = {1.0f, 0.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Green   = {0.0f, 1.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Blue    = {0.0f, 0.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Yellow  = {1.0f, 1.0f, 0.0f, 1.0f};
    constexpr glm::vec4 Cyan    = {0.0f, 1.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Magenta = {1.0f, 0.0f, 1.0f, 1.0f};
    constexpr glm::vec4 White   = {1.0f, 1.0f, 1.0f, 1.0f};
    constexpr glm::vec4 Gray    = {0.5f, 0.5f, 0.5f, 1.0f};
    constexpr glm::vec4 Orange  = {1.0f, 0.5f, 0.0f, 1.0f};
    constexpr glm::vec4 Purple  = {0.5f, 0.0f, 1.0f, 1.0f};

    // Physics debug colors
    constexpr glm::vec4 ColliderActive   = {0.0f, 1.0f, 0.0f, 0.8f};
    constexpr glm::vec4 ColliderInactive = {0.5f, 0.5f, 0.5f, 0.5f};
    constexpr glm::vec4 ColliderTrigger  = {1.0f, 1.0f, 0.0f, 0.6f};
    constexpr glm::vec4 Velocity         = {0.0f, 0.5f, 1.0f, 1.0f};
    constexpr glm::vec4 Force            = {1.0f, 0.3f, 0.0f, 1.0f};
    constexpr glm::vec4 ContactPoint     = {1.0f, 0.0f, 0.0f, 1.0f};
    constexpr glm::vec4 ContactNormal    = {0.0f, 1.0f, 0.0f, 1.0f};
}

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Create a color that interpolates based on a value
 * @param t Interpolation factor (0.0 to 1.0)
 * @param from Start color
 * @param to End color
 * @return Interpolated color
 */
inline glm::vec4 LerpColor(float t, const glm::vec4& from, const glm::vec4& to) {
    t = glm::clamp(t, 0.0f, 1.0f);
    return from + t * (to - from);
}

/**
 * @brief Create a heat map color (blue -> cyan -> green -> yellow -> red)
 * @param t Value from 0.0 (cold/blue) to 1.0 (hot/red)
 * @return Heat map color
 */
inline glm::vec4 HeatMapColor(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);

    if (t < 0.25f) {
        // Blue to Cyan
        float local = t / 0.25f;
        return glm::vec4(0.0f, local, 1.0f, 1.0f);
    } else if (t < 0.5f) {
        // Cyan to Green
        float local = (t - 0.25f) / 0.25f;
        return glm::vec4(0.0f, 1.0f, 1.0f - local, 1.0f);
    } else if (t < 0.75f) {
        // Green to Yellow
        float local = (t - 0.5f) / 0.25f;
        return glm::vec4(local, 1.0f, 0.0f, 1.0f);
    } else {
        // Yellow to Red
        float local = (t - 0.75f) / 0.25f;
        return glm::vec4(1.0f, 1.0f - local, 0.0f, 1.0f);
    }
}

/**
 * @brief Generate a random but consistent color from an ID
 * @param id Unique identifier
 * @return Deterministic color for this ID
 */
inline glm::vec4 ColorFromId(uint32_t id) {
    // Use a simple hash to spread colors across the spectrum
    uint32_t hash = id * 2654435761u; // Knuth's multiplicative hash
    float hue = static_cast<float>(hash & 0xFFFF) / 65535.0f;

    // HSV to RGB (saturation = 0.8, value = 0.9)
    const float s = 0.8f;
    const float v = 0.9f;
    const float h = hue * 6.0f;

    const int i = static_cast<int>(h);
    const float f = h - static_cast<float>(i);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));

    glm::vec3 rgb;
    switch (i % 6) {
        case 0: rgb = {v, t, p}; break;
        case 1: rgb = {q, v, p}; break;
        case 2: rgb = {p, v, t}; break;
        case 3: rgb = {p, q, v}; break;
        case 4: rgb = {t, p, v}; break;
        default: rgb = {v, p, q}; break;
    }

    return glm::vec4(rgb, 1.0f);
}

// ============================================================================
// Physics Debug Helpers
// ============================================================================

/**
 * @brief Draw a velocity vector with magnitude-based coloring
 */
inline void DrawVelocity(DebugDraw& debug, const glm::vec3& position,
                         const glm::vec3& velocity, float maxSpeed = 10.0f) {
    const float speed = glm::length(velocity);
    if (speed < 0.001f) {
        return;
    }

    const float t = glm::clamp(speed / maxSpeed, 0.0f, 1.0f);
    const glm::vec4 color = LerpColor(t, Colors::Velocity, Colors::Force);

    debug.AddArrow(position, position + velocity, color, 0.15f);
}

/**
 * @brief Draw a contact point with normal
 */
inline void DrawContactPoint(DebugDraw& debug, const glm::vec3& point,
                             const glm::vec3& normal, float penetration = 0.0f) {
    debug.AddPoint(point, 0.05f, Colors::ContactPoint);
    debug.AddArrow(point, point + normal * 0.5f, Colors::ContactNormal, 0.2f);

    // Show penetration depth if significant
    if (penetration > 0.001f) {
        debug.AddLine(point, point - normal * penetration, Colors::Red);
    }
}

/**
 * @brief Draw a physics body's state (position, velocity, angular velocity)
 */
inline void DrawRigidBodyState(DebugDraw& debug, const glm::vec3& position,
                               const glm::vec3& velocity, const glm::vec3& angularVelocity,
                               float scale = 1.0f) {
    // Position marker
    debug.AddPoint(position, 0.1f * scale, Colors::White);

    // Linear velocity
    if (glm::length(velocity) > 0.01f) {
        debug.AddArrow(position, position + velocity * scale, Colors::Velocity, 0.1f);
    }

    // Angular velocity (as rotation axis)
    if (glm::length(angularVelocity) > 0.01f) {
        const glm::vec3 axis = glm::normalize(angularVelocity);
        const float magnitude = glm::length(angularVelocity);
        debug.AddArrow(position, position + axis * magnitude * scale * 0.5f, Colors::Magenta, 0.1f);
    }
}

// ============================================================================
// Spline Debug Helpers
// ============================================================================

/**
 * @brief Draw a Catmull-Rom spline through control points
 */
inline void DrawCatmullRomSpline(DebugDraw& debug, const std::vector<glm::vec3>& points,
                                  const glm::vec4& color = Colors::Cyan,
                                  int segmentsPerSpan = 16, bool drawControlPoints = true) {
    if (points.size() < 4) {
        // Need at least 4 points for Catmull-Rom
        if (points.size() >= 2) {
            debug.AddPolyline(points, color, false);
        }
        return;
    }

    // Draw control points
    if (drawControlPoints) {
        for (const auto& p : points) {
            debug.AddPoint(p, 0.1f, Colors::Yellow);
        }
    }

    // Draw spline segments
    for (size_t i = 0; i < points.size() - 3; ++i) {
        const glm::vec3& p0 = points[i];
        const glm::vec3& p1 = points[i + 1];
        const glm::vec3& p2 = points[i + 2];
        const glm::vec3& p3 = points[i + 3];

        glm::vec3 prev = p1;

        for (int j = 1; j <= segmentsPerSpan; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(segmentsPerSpan);
            const float t2 = t * t;
            const float t3 = t2 * t;

            // Catmull-Rom basis functions
            const glm::vec3 point = 0.5f * (
                (2.0f * p1) +
                (-p0 + p2) * t +
                (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
            );

            debug.AddLine(prev, point, color);
            prev = point;
        }
    }
}

/**
 * @brief Draw a B-spline curve (degree 3)
 */
inline void DrawBSpline(DebugDraw& debug, const std::vector<glm::vec3>& controlPoints,
                        const glm::vec4& color = Colors::Purple,
                        int segmentsPerSpan = 16, bool drawControlPolygon = true) {
    if (controlPoints.size() < 4) {
        return;
    }

    // Draw control polygon
    if (drawControlPolygon) {
        for (size_t i = 0; i < controlPoints.size() - 1; ++i) {
            debug.AddLine(controlPoints[i], controlPoints[i + 1],
                         glm::vec4(Colors::Gray.r, Colors::Gray.g, Colors::Gray.b, 0.5f));
        }
        for (const auto& p : controlPoints) {
            debug.AddPoint(p, 0.08f, Colors::Orange);
        }
    }

    // Draw B-spline using Cox-de Boor (simplified uniform cubic B-spline)
    for (size_t i = 0; i < controlPoints.size() - 3; ++i) {
        const glm::vec3& p0 = controlPoints[i];
        const glm::vec3& p1 = controlPoints[i + 1];
        const glm::vec3& p2 = controlPoints[i + 2];
        const glm::vec3& p3 = controlPoints[i + 3];

        glm::vec3 prev = (p0 + 4.0f * p1 + p2) / 6.0f; // B-spline doesn't pass through control points

        for (int j = 1; j <= segmentsPerSpan; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(segmentsPerSpan);
            const float t2 = t * t;
            const float t3 = t2 * t;

            // Uniform cubic B-spline basis
            const float b0 = (1.0f - t) * (1.0f - t) * (1.0f - t) / 6.0f;
            const float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
            const float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
            const float b3 = t3 / 6.0f;

            const glm::vec3 point = b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3;

            debug.AddLine(prev, point, color);
            prev = point;
        }
    }
}

// ============================================================================
// Navigation / AI Debug Helpers
// ============================================================================

/**
 * @brief Draw a navigation path with optional waypoint markers
 */
inline void DrawPath(DebugDraw& debug, const std::vector<glm::vec3>& waypoints,
                     const glm::vec4& color = Colors::Green,
                     bool drawWaypoints = true, bool drawArrows = false) {
    if (waypoints.size() < 2) {
        return;
    }

    for (size_t i = 0; i < waypoints.size() - 1; ++i) {
        if (drawArrows) {
            debug.AddArrow(waypoints[i], waypoints[i + 1], color, 0.1f);
        } else {
            debug.AddLine(waypoints[i], waypoints[i + 1], color);
        }

        if (drawWaypoints) {
            debug.AddPoint(waypoints[i], 0.15f, Colors::Yellow);
        }
    }

    if (drawWaypoints) {
        // Mark the last waypoint (destination) differently
        debug.AddPoint(waypoints.back(), 0.2f, Colors::Red);
    }
}

/**
 * @brief Draw a vision cone for AI debugging
 */
inline void DrawVisionCone(DebugDraw& debug, const glm::vec3& origin,
                           const glm::vec3& direction, float range, float halfAngleDegrees,
                           const glm::vec4& color = Colors::Yellow, int segments = 16) {
    const glm::vec3 dir = glm::normalize(direction);

    // Calculate the base of the cone
    const glm::vec3 coneBase = origin + dir * range;
    const float baseRadius = range * std::tan(glm::radians(halfAngleDegrees));

    // Draw the cone shape
    debug.AddCone(origin, coneBase, baseRadius, color, segments);

    // Draw a few range indicator circles
    for (int i = 1; i <= 3; ++i) {
        const float dist = range * static_cast<float>(i) / 3.0f;
        const float r = dist * std::tan(glm::radians(halfAngleDegrees));
        debug.AddCircle(origin + dir * dist, r, dir,
                       glm::vec4(color.r, color.g, color.b, color.a * 0.5f), segments);
    }
}

// ============================================================================
// Grid and Coordinate System Helpers
// ============================================================================

/**
 * @brief Draw a 3D coordinate system with labeled axes
 */
inline void DrawCoordinateSystem(DebugDraw& debug, const glm::vec3& origin,
                                 float size = 1.0f, bool drawLabels = true) {
    // Main axes
    debug.AddArrow(origin, origin + glm::vec3(size, 0.0f, 0.0f), Colors::Red, 0.1f);
    debug.AddArrow(origin, origin + glm::vec3(0.0f, size, 0.0f), Colors::Green, 0.1f);
    debug.AddArrow(origin, origin + glm::vec3(0.0f, 0.0f, size), Colors::Blue, 0.1f);

    // Negative axes (fainter)
    const float negSize = size * 0.5f;
    debug.AddLine(origin, origin - glm::vec3(negSize, 0.0f, 0.0f),
                 glm::vec4(0.5f, 0.0f, 0.0f, 0.5f));
    debug.AddLine(origin, origin - glm::vec3(0.0f, negSize, 0.0f),
                 glm::vec4(0.0f, 0.5f, 0.0f, 0.5f));
    debug.AddLine(origin, origin - glm::vec3(0.0f, 0.0f, negSize),
                 glm::vec4(0.0f, 0.0f, 0.5f, 0.5f));

    // Unit squares on each plane (optional visual aid)
    if (size >= 1.0f) {
        const float planeAlpha = 0.2f;
        // XY plane
        debug.AddLine(origin + glm::vec3(1, 0, 0), origin + glm::vec3(1, 1, 0),
                     glm::vec4(1, 1, 0, planeAlpha));
        debug.AddLine(origin + glm::vec3(0, 1, 0), origin + glm::vec3(1, 1, 0),
                     glm::vec4(1, 1, 0, planeAlpha));
        // XZ plane
        debug.AddLine(origin + glm::vec3(1, 0, 0), origin + glm::vec3(1, 0, 1),
                     glm::vec4(1, 0, 1, planeAlpha));
        debug.AddLine(origin + glm::vec3(0, 0, 1), origin + glm::vec3(1, 0, 1),
                     glm::vec4(1, 0, 1, planeAlpha));
        // YZ plane
        debug.AddLine(origin + glm::vec3(0, 1, 0), origin + glm::vec3(0, 1, 1),
                     glm::vec4(0, 1, 1, planeAlpha));
        debug.AddLine(origin + glm::vec3(0, 0, 1), origin + glm::vec3(0, 1, 1),
                     glm::vec4(0, 1, 1, planeAlpha));
    }
}

/**
 * @brief Draw a height-colored terrain grid for visualization
 */
inline void DrawHeightGrid(DebugDraw& debug, const std::vector<std::vector<float>>& heights,
                           const glm::vec3& origin, float cellSize,
                           float minHeight, float maxHeight) {
    if (heights.empty() || heights[0].empty()) {
        return;
    }

    const size_t rows = heights.size();
    const size_t cols = heights[0].size();
    const float heightRange = maxHeight - minHeight;

    for (size_t z = 0; z < rows; ++z) {
        for (size_t x = 0; x < cols; ++x) {
            const float h = heights[z][x];
            const glm::vec3 pos = origin + glm::vec3(
                static_cast<float>(x) * cellSize,
                h,
                static_cast<float>(z) * cellSize
            );

            // Color based on height
            const float t = (heightRange > 0.001f) ?
                           glm::clamp((h - minHeight) / heightRange, 0.0f, 1.0f) : 0.5f;
            const glm::vec4 color = HeatMapColor(t);

            // Draw connections to neighbors
            if (x < cols - 1) {
                const float hRight = heights[z][x + 1];
                const glm::vec3 posRight = origin + glm::vec3(
                    static_cast<float>(x + 1) * cellSize,
                    hRight,
                    static_cast<float>(z) * cellSize
                );
                const float tRight = (heightRange > 0.001f) ?
                                    glm::clamp((hRight - minHeight) / heightRange, 0.0f, 1.0f) : 0.5f;
                debug.AddLine(pos, posRight, color, HeatMapColor(tRight));
            }

            if (z < rows - 1) {
                const float hDown = heights[z + 1][x];
                const glm::vec3 posDown = origin + glm::vec3(
                    static_cast<float>(x) * cellSize,
                    hDown,
                    static_cast<float>(z + 1) * cellSize
                );
                const float tDown = (heightRange > 0.001f) ?
                                   glm::clamp((hDown - minHeight) / heightRange, 0.0f, 1.0f) : 0.5f;
                debug.AddLine(pos, posDown, color, HeatMapColor(tDown));
            }
        }
    }
}

} // namespace DebugShapes
} // namespace Nova
