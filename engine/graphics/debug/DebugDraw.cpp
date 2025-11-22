#include "graphics/debug/DebugDraw.hpp"
#include "graphics/Shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace Nova {

// Embedded shader source - no external files needed
static const char* DEBUG_LINE_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ProjectionView;

out vec4 v_Color;

void main() {
    v_Color = a_Color;
    gl_Position = u_ProjectionView * vec4(a_Position, 1.0);
}
)";

static const char* DEBUG_LINE_FRAGMENT_SHADER = R"(
#version 460 core

in vec4 v_Color;
out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

DebugDraw::DebugDraw() {
    m_lines.reserve(INITIAL_LINE_CAPACITY * 2);
}

DebugDraw::~DebugDraw() {
    Shutdown();
}

bool DebugDraw::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Create shader from embedded source
    m_lineShader = std::make_unique<Shader>();
    if (!m_lineShader->LoadFromSource(DEBUG_LINE_VERTEX_SHADER, DEBUG_LINE_FRAGMENT_SHADER)) {
        spdlog::error("DebugDraw: Failed to create line shader");
        return false;
    }

    // Create VAO and VBO with error checking
    glGenVertexArrays(1, &m_lineVAO);
    if (m_lineVAO == 0) {
        spdlog::error("DebugDraw: Failed to create VAO");
        return false;
    }

    glGenBuffers(1, &m_lineVBO);
    if (m_lineVBO == 0) {
        glDeleteVertexArrays(1, &m_lineVAO);
        m_lineVAO = 0;
        spdlog::error("DebugDraw: Failed to create VBO");
        return false;
    }

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    // Allocate initial buffer with GL_STREAM_DRAW for frequently updated data
    m_lineBufferCapacity = INITIAL_LINE_CAPACITY * 2;
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_lineBufferCapacity * sizeof(LineVertex)),
                 nullptr,
                 GL_STREAM_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                         reinterpret_cast<void*>(offsetof(LineVertex, position)));

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                         reinterpret_cast<void*>(offsetof(LineVertex, color)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_initialized = true;
    spdlog::info("DebugDraw: System initialized (capacity: {} lines)", INITIAL_LINE_CAPACITY);
    return true;
}

void DebugDraw::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Delete OpenGL resources in reverse order of creation
    if (m_lineVBO != 0) {
        glDeleteBuffers(1, &m_lineVBO);
        m_lineVBO = 0;
    }
    if (m_lineVAO != 0) {
        glDeleteVertexArrays(1, &m_lineVAO);
        m_lineVAO = 0;
    }

    m_lineShader.reset();
    m_lines.clear();
    m_lines.shrink_to_fit();
    m_lineBufferCapacity = 0;
    m_initialized = false;

    spdlog::info("DebugDraw: System shutdown");
}

void DebugDraw::Clear() {
    m_lines.clear();
}

void DebugDraw::Render(const glm::mat4& projectionView) {
    if (m_lines.empty() || !m_initialized) {
        return;
    }

    // Clamp to maximum batch size to prevent GPU stalls
    const size_t vertexCount = std::min(m_lines.size(), MAX_LINES_PER_BATCH * 2);
    if (m_lines.size() > MAX_LINES_PER_BATCH * 2) {
        spdlog::warn("DebugDraw: Line count ({}) exceeds max batch size ({}), truncating",
                     m_lines.size() / 2, MAX_LINES_PER_BATCH);
    }

    // Bind buffer and upload data using buffer orphaning for better performance
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    const size_t requiredSize = vertexCount * sizeof(LineVertex);

    if (vertexCount > m_lineBufferCapacity) {
        // Grow buffer capacity (double each time to amortize allocations)
        m_lineBufferCapacity = vertexCount * 2;
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_lineBufferCapacity * sizeof(LineVertex)),
                     nullptr,
                     GL_STREAM_DRAW);
    }

    // Buffer orphaning: invalidate old buffer and upload new data
    // This avoids implicit synchronization when the GPU is still using the buffer
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_lineBufferCapacity * sizeof(LineVertex)),
                 nullptr,
                 GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(requiredSize),
                    m_lines.data());

    // Set up render state (minimize state queries)
    GLboolean prevDepthTest = GL_TRUE;
    GLfloat prevLineWidth = 1.0f;
    glGetBooleanv(GL_DEPTH_TEST, &prevDepthTest);
    glGetFloatv(GL_LINE_WIDTH, &prevLineWidth);

    if (m_depthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glLineWidth(m_lineWidth);

    // Render
    m_lineShader->Bind();
    m_lineShader->SetMat4("u_ProjectionView", projectionView);

    glBindVertexArray(m_lineVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount));

    // Restore previous state
    if (prevDepthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glLineWidth(prevLineWidth);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ============================================================================
// Helper Functions
// ============================================================================

void DebugDraw::ComputeBasis(const glm::vec3& normal, glm::vec3& outRight, glm::vec3& outForward) {
    const glm::vec3 up = glm::normalize(normal);

    // Choose a reference vector that isn't parallel to the normal
    if (std::abs(up.y) < 0.999f) {
        outRight = glm::normalize(glm::cross(up, glm::vec3(0.0f, 1.0f, 0.0f)));
    } else {
        outRight = glm::normalize(glm::cross(up, glm::vec3(1.0f, 0.0f, 0.0f)));
    }
    outForward = glm::cross(outRight, up);
}

// ============================================================================
// Line Drawing
// ============================================================================

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) {
    m_lines.push_back({start, color});
    m_lines.push_back({end, color});
}

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end,
                        const glm::vec4& startColor, const glm::vec4& endColor) {
    m_lines.push_back({start, startColor});
    m_lines.push_back({end, endColor});
}

void DebugDraw::AddPolyline(const std::vector<glm::vec3>& points,
                            const glm::vec4& color, bool closed) {
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i < points.size() - 1; ++i) {
        AddLine(points[i], points[i + 1], color);
    }

    if (closed && points.size() > 2) {
        AddLine(points.back(), points.front(), color);
    }
}

// ============================================================================
// Shape Drawing
// ============================================================================

void DebugDraw::AddTransform(const glm::mat4& transform, float size) {
    const glm::vec3 origin = glm::vec3(transform[3]);
    const glm::vec3 right = glm::vec3(transform[0]) * size;
    const glm::vec3 up = glm::vec3(transform[1]) * size;
    const glm::vec3 forward = glm::vec3(transform[2]) * size;

    // RGB corresponds to XYZ axes (standard convention)
    AddLine(origin, origin + right, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));   // X - Red
    AddLine(origin, origin + up, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));      // Y - Green
    AddLine(origin, origin + forward, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)); // Z - Blue
}

void DebugDraw::AddGrid(int halfExtent, float spacing, const glm::vec4& color) {
    const glm::vec4 axisColor(1.0f, 1.0f, 1.0f, 1.0f);
    const float extent = static_cast<float>(halfExtent) * spacing;

    for (int i = -halfExtent; i <= halfExtent; ++i) {
        const float pos = static_cast<float>(i) * spacing;
        const bool isCenter = (i == 0);
        const glm::vec4& lineColor = isCenter ? axisColor : color;

        // Lines parallel to X axis
        AddLine(glm::vec3(-extent, 0.0f, pos), glm::vec3(extent, 0.0f, pos), lineColor);
        // Lines parallel to Z axis
        AddLine(glm::vec3(pos, 0.0f, -extent), glm::vec3(pos, 0.0f, extent), lineColor);
    }
}

void DebugDraw::AddAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color) {
    // Bottom face (y = min.y)
    AddLine({min.x, min.y, min.z}, {max.x, min.y, min.z}, color);
    AddLine({max.x, min.y, min.z}, {max.x, min.y, max.z}, color);
    AddLine({max.x, min.y, max.z}, {min.x, min.y, max.z}, color);
    AddLine({min.x, min.y, max.z}, {min.x, min.y, min.z}, color);

    // Top face (y = max.y)
    AddLine({min.x, max.y, min.z}, {max.x, max.y, min.z}, color);
    AddLine({max.x, max.y, min.z}, {max.x, max.y, max.z}, color);
    AddLine({max.x, max.y, max.z}, {min.x, max.y, max.z}, color);
    AddLine({min.x, max.y, max.z}, {min.x, max.y, min.z}, color);

    // Vertical edges
    AddLine({min.x, min.y, min.z}, {min.x, max.y, min.z}, color);
    AddLine({max.x, min.y, min.z}, {max.x, max.y, min.z}, color);
    AddLine({max.x, min.y, max.z}, {max.x, max.y, max.z}, color);
    AddLine({min.x, min.y, max.z}, {min.x, max.y, max.z}, color);
}

void DebugDraw::AddBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color) {
    // Generate and transform 8 corners
    const glm::vec3 localCorners[8] = {
        {-halfExtents.x, -halfExtents.y, -halfExtents.z},
        { halfExtents.x, -halfExtents.y, -halfExtents.z},
        { halfExtents.x, -halfExtents.y,  halfExtents.z},
        {-halfExtents.x, -halfExtents.y,  halfExtents.z},
        {-halfExtents.x,  halfExtents.y, -halfExtents.z},
        { halfExtents.x,  halfExtents.y, -halfExtents.z},
        { halfExtents.x,  halfExtents.y,  halfExtents.z},
        {-halfExtents.x,  halfExtents.y,  halfExtents.z}
    };

    glm::vec3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = glm::vec3(transform * glm::vec4(localCorners[i], 1.0f));
    }

    // Bottom face
    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    // Top face
    AddLine(corners[4], corners[5], color);
    AddLine(corners[5], corners[6], color);
    AddLine(corners[6], corners[7], color);
    AddLine(corners[7], corners[4], color);

    // Vertical edges
    AddLine(corners[0], corners[4], color);
    AddLine(corners[1], corners[5], color);
    AddLine(corners[2], corners[6], color);
    AddLine(corners[3], corners[7], color);
}

void DebugDraw::AddSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
    // Draw three orthogonal circles
    AddCircle(center, radius, glm::vec3(1.0f, 0.0f, 0.0f), color, segments); // YZ plane
    AddCircle(center, radius, glm::vec3(0.0f, 1.0f, 0.0f), color, segments); // XZ plane
    AddCircle(center, radius, glm::vec3(0.0f, 0.0f, 1.0f), color, segments); // XY plane
}

void DebugDraw::AddCircle(const glm::vec3& center, float radius, const glm::vec3& normal,
                          const glm::vec4& color, int segments) {
    if (segments < 3 || radius <= 0.0f) {
        return;
    }

    glm::vec3 right, forward;
    ComputeBasis(normal, right, forward);

    const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);
    glm::vec3 prevPoint = center + right * radius;

    for (int i = 1; i <= segments; ++i) {
        const float angle = angleStep * static_cast<float>(i);
        const glm::vec3 point = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(prevPoint, point, color);
        prevPoint = point;
    }
}

void DebugDraw::AddCylinder(const glm::vec3& base, float height, float radius,
                            const glm::vec4& color, int segments) {
    if (segments < 3) {
        return;
    }

    const glm::vec3 top = base + glm::vec3(0.0f, height, 0.0f);

    // Draw top and bottom circles
    AddCircle(base, radius, glm::vec3(0.0f, 1.0f, 0.0f), color, segments);
    AddCircle(top, radius, glm::vec3(0.0f, 1.0f, 0.0f), color, segments);

    // Draw 4 vertical lines at cardinal directions
    const int step = std::max(1, segments / 4);
    const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

    for (int i = 0; i < segments; i += step) {
        const float angle = angleStep * static_cast<float>(i);
        const glm::vec3 offset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        AddLine(base + offset, top + offset, color);
    }
}

void DebugDraw::AddCapsule(const glm::vec3& start, const glm::vec3& end, float radius,
                           const glm::vec4& color, int segments) {
    if (segments < 4) {
        return;
    }

    const glm::vec3 axis = end - start;
    const float height = glm::length(axis);

    if (height < 0.0001f) {
        // Degenerate case: just draw a sphere
        AddSphere(start, radius, color, segments);
        return;
    }

    const glm::vec3 up = axis / height;
    glm::vec3 right, forward;
    ComputeBasis(up, right, forward);

    // Draw the cylindrical body
    AddCircle(start, radius, up, color, segments);
    AddCircle(end, radius, up, color, segments);

    // Vertical lines
    const int step = std::max(1, segments / 4);
    const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

    for (int i = 0; i < segments; i += step) {
        const float angle = angleStep * static_cast<float>(i);
        const glm::vec3 offset = (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(start + offset, end + offset, color);
    }

    // Draw hemispherical caps using semi-circles
    const int halfSegments = segments / 2;
    const float semiAngleStep = glm::pi<float>() / static_cast<float>(halfSegments);

    // Draw arcs for each cap at cardinal directions
    for (int i = 0; i < segments; i += step) {
        const float baseAngle = angleStep * static_cast<float>(i);
        const glm::vec3 arcDir = right * std::cos(baseAngle) + forward * std::sin(baseAngle);

        // Bottom hemisphere (from start, going down)
        glm::vec3 prevBottom = start + arcDir * radius;
        for (int j = 1; j <= halfSegments; ++j) {
            const float phi = semiAngleStep * static_cast<float>(j);
            const glm::vec3 point = start - up * (radius * std::sin(phi)) + arcDir * (radius * std::cos(phi));
            AddLine(prevBottom, point, color);
            prevBottom = point;
        }

        // Top hemisphere (from end, going up)
        glm::vec3 prevTop = end + arcDir * radius;
        for (int j = 1; j <= halfSegments; ++j) {
            const float phi = semiAngleStep * static_cast<float>(j);
            const glm::vec3 point = end + up * (radius * std::sin(phi)) + arcDir * (radius * std::cos(phi));
            AddLine(prevTop, point, color);
            prevTop = point;
        }
    }
}

void DebugDraw::AddCone(const glm::vec3& apex, const glm::vec3& base, float radius,
                        const glm::vec4& color, int segments) {
    if (segments < 3) {
        return;
    }

    const glm::vec3 axis = base - apex;
    const glm::vec3 normal = glm::normalize(axis);

    // Draw base circle
    AddCircle(base, radius, normal, color, segments);

    // Draw lines from apex to base
    glm::vec3 right, forward;
    ComputeBasis(normal, right, forward);

    const int step = std::max(1, segments / 4);
    const float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

    for (int i = 0; i < segments; i += step) {
        const float angle = angleStep * static_cast<float>(i);
        const glm::vec3 point = base + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(apex, point, color);
    }
}

void DebugDraw::AddArrow(const glm::vec3& start, const glm::vec3& end,
                         const glm::vec4& color, float headSize) {
    const glm::vec3 dir = end - start;
    const float length = glm::length(dir);

    if (length < 0.0001f) {
        return;
    }

    AddLine(start, end, color);

    const glm::vec3 normalized = dir / length;

    glm::vec3 right, up;
    ComputeBasis(normalized, right, up);

    const float arrowLength = length * headSize;
    const float arrowWidth = arrowLength * 0.5f;
    const glm::vec3 arrowBase = end - normalized * arrowLength;

    // Four arrow head lines
    AddLine(end, arrowBase + right * arrowWidth, color);
    AddLine(end, arrowBase - right * arrowWidth, color);
    AddLine(end, arrowBase + up * arrowWidth, color);
    AddLine(end, arrowBase - up * arrowWidth, color);
}

void DebugDraw::AddRay(const glm::vec3& origin, const glm::vec3& direction,
                       float length, const glm::vec4& color) {
    const glm::vec3 dir = glm::normalize(direction);
    AddLine(origin, origin + dir * length, color);
}

void DebugDraw::AddPlane(const glm::vec3& center, const glm::vec3& normal,
                         float size, const glm::vec4& color) {
    glm::vec3 right, forward;
    ComputeBasis(normal, right, forward);

    const glm::vec3 corners[4] = {
        center + (-right - forward) * size,
        center + ( right - forward) * size,
        center + ( right + forward) * size,
        center + (-right + forward) * size
    };

    // Draw quad outline
    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    // Draw normal indicator
    AddArrow(center, center + glm::normalize(normal) * size * 0.5f, color, 0.2f);
}

void DebugDraw::AddFrustum(const glm::mat4& viewProjection, const glm::vec4& color) {
    const glm::mat4 invVP = glm::inverse(viewProjection);

    // NDC corners: near plane (z=-1), far plane (z=1)
    const glm::vec4 ndcCorners[8] = {
        {-1.0f, -1.0f, -1.0f, 1.0f}, { 1.0f, -1.0f, -1.0f, 1.0f},
        { 1.0f,  1.0f, -1.0f, 1.0f}, {-1.0f,  1.0f, -1.0f, 1.0f},
        {-1.0f, -1.0f,  1.0f, 1.0f}, { 1.0f, -1.0f,  1.0f, 1.0f},
        { 1.0f,  1.0f,  1.0f, 1.0f}, {-1.0f,  1.0f,  1.0f, 1.0f}
    };

    glm::vec3 worldCorners[8];
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 world = invVP * ndcCorners[i];
        worldCorners[i] = glm::vec3(world) / world.w;
    }

    // Near plane
    AddLine(worldCorners[0], worldCorners[1], color);
    AddLine(worldCorners[1], worldCorners[2], color);
    AddLine(worldCorners[2], worldCorners[3], color);
    AddLine(worldCorners[3], worldCorners[0], color);

    // Far plane
    AddLine(worldCorners[4], worldCorners[5], color);
    AddLine(worldCorners[5], worldCorners[6], color);
    AddLine(worldCorners[6], worldCorners[7], color);
    AddLine(worldCorners[7], worldCorners[4], color);

    // Connecting edges
    AddLine(worldCorners[0], worldCorners[4], color);
    AddLine(worldCorners[1], worldCorners[5], color);
    AddLine(worldCorners[2], worldCorners[6], color);
    AddLine(worldCorners[3], worldCorners[7], color);
}

void DebugDraw::AddPoint(const glm::vec3& position, float size, const glm::vec4& color) {
    AddLine(position - glm::vec3(size, 0.0f, 0.0f), position + glm::vec3(size, 0.0f, 0.0f), color);
    AddLine(position - glm::vec3(0.0f, size, 0.0f), position + glm::vec3(0.0f, size, 0.0f), color);
    AddLine(position - glm::vec3(0.0f, 0.0f, size), position + glm::vec3(0.0f, 0.0f, size), color);
}

void DebugDraw::AddTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                            const glm::vec4& color) {
    AddLine(a, b, color);
    AddLine(b, c, color);
    AddLine(c, a, color);
}

// ============================================================================
// Curve Drawing
// ============================================================================

void DebugDraw::AddBezierQuadratic(const glm::vec3& start, const glm::vec3& control,
                                    const glm::vec3& end,
                                    const glm::vec4& color, int segments) {
    if (segments < 1) {
        return;
    }

    glm::vec3 prev = start;
    const float step = 1.0f / static_cast<float>(segments);

    for (int i = 1; i <= segments; ++i) {
        const float t = step * static_cast<float>(i);
        const float oneMinusT = 1.0f - t;

        // Quadratic Bezier: B(t) = (1-t)^2 * P0 + 2(1-t)t * P1 + t^2 * P2
        const glm::vec3 point = oneMinusT * oneMinusT * start
                               + 2.0f * oneMinusT * t * control
                               + t * t * end;

        AddLine(prev, point, color);
        prev = point;
    }
}

void DebugDraw::AddBezierCubic(const glm::vec3& start, const glm::vec3& control1,
                                const glm::vec3& control2, const glm::vec3& end,
                                const glm::vec4& color, int segments) {
    if (segments < 1) {
        return;
    }

    glm::vec3 prev = start;
    const float step = 1.0f / static_cast<float>(segments);

    for (int i = 1; i <= segments; ++i) {
        const float t = step * static_cast<float>(i);
        const float oneMinusT = 1.0f - t;
        const float oneMinusT2 = oneMinusT * oneMinusT;
        const float oneMinusT3 = oneMinusT2 * oneMinusT;
        const float t2 = t * t;
        const float t3 = t2 * t;

        // Cubic Bezier: B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
        const glm::vec3 point = oneMinusT3 * start
                               + 3.0f * oneMinusT2 * t * control1
                               + 3.0f * oneMinusT * t2 * control2
                               + t3 * end;

        AddLine(prev, point, color);
        prev = point;
    }
}

void DebugDraw::AddArc(const glm::vec3& center, float radius,
                       const glm::vec3& normal, const glm::vec3& startDir,
                       float angleDegrees,
                       const glm::vec4& color, int segments) {
    if (segments < 1 || radius <= 0.0f) {
        return;
    }

    // Normalize inputs
    const glm::vec3 up = glm::normalize(normal);
    const glm::vec3 right = glm::normalize(startDir - up * glm::dot(startDir, up)); // Project startDir onto plane
    const glm::vec3 forward = glm::cross(up, right);

    const float angleRadians = glm::radians(angleDegrees);
    const float angleStep = angleRadians / static_cast<float>(segments);

    glm::vec3 prev = center + right * radius;

    for (int i = 1; i <= segments; ++i) {
        const float angle = angleStep * static_cast<float>(i);
        const glm::vec3 point = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(prev, point, color);
        prev = point;
    }
}

} // namespace Nova
