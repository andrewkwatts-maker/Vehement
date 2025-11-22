#include "graphics/debug/DebugDraw.hpp"
#include "graphics/Shader.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

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
        spdlog::error("Failed to create debug line shader");
        return false;
    }

    // Create VAO and VBO
    glGenVertexArrays(1, &m_lineVAO);
    glGenBuffers(1, &m_lineVBO);

    glBindVertexArray(m_lineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    // Allocate initial buffer
    m_lineBufferCapacity = INITIAL_LINE_CAPACITY * 2;
    glBufferData(GL_ARRAY_BUFFER, m_lineBufferCapacity * sizeof(LineVertex), nullptr, GL_DYNAMIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                         (void*)offsetof(LineVertex, position));

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                         (void*)offsetof(LineVertex, color));

    glBindVertexArray(0);

    m_initialized = true;
    spdlog::info("Debug draw system initialized");
    return true;
}

void DebugDraw::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_lineVAO) {
        glDeleteVertexArrays(1, &m_lineVAO);
        m_lineVAO = 0;
    }
    if (m_lineVBO) {
        glDeleteBuffers(1, &m_lineVBO);
        m_lineVBO = 0;
    }

    m_lineShader.reset();
    m_lines.clear();
    m_initialized = false;
}

void DebugDraw::Clear() {
    m_lines.clear();
}

void DebugDraw::Render(const glm::mat4& projectionView) {
    if (m_lines.empty()) {
        return;
    }

    // Save state
    GLboolean depthTestEnabled;
    glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);

    // Set state
    if (m_depthTest) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glLineWidth(m_lineWidth);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVBO);

    // Resize buffer if needed
    if (m_lines.size() > m_lineBufferCapacity) {
        m_lineBufferCapacity = m_lines.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, m_lineBufferCapacity * sizeof(LineVertex), nullptr, GL_DYNAMIC_DRAW);
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, m_lines.size() * sizeof(LineVertex), m_lines.data());

    // Draw
    m_lineShader->Bind();
    m_lineShader->SetMat4("u_ProjectionView", projectionView);

    glBindVertexArray(m_lineVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_lines.size()));
    glBindVertexArray(0);

    // Restore state
    if (depthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }

    glLineWidth(1.0f);
}

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) {
    m_lines.push_back({start, color});
    m_lines.push_back({end, color});
}

void DebugDraw::AddLine(const glm::vec3& start, const glm::vec3& end,
                        const glm::vec4& startColor, const glm::vec4& endColor) {
    m_lines.push_back({start, startColor});
    m_lines.push_back({end, endColor});
}

void DebugDraw::AddTransform(const glm::mat4& transform, float size) {
    glm::vec3 origin = glm::vec3(transform[3]);
    glm::vec3 right = glm::vec3(transform[0]) * size;
    glm::vec3 up = glm::vec3(transform[1]) * size;
    glm::vec3 forward = glm::vec3(transform[2]) * size;

    AddLine(origin, origin + right, glm::vec4(1, 0, 0, 1));   // X - Red
    AddLine(origin, origin + up, glm::vec4(0, 1, 0, 1));      // Y - Green
    AddLine(origin, origin + forward, glm::vec4(0, 0, 1, 1)); // Z - Blue
}

void DebugDraw::AddGrid(int halfExtent, float spacing, const glm::vec4& color) {
    glm::vec4 axisColor = glm::vec4(1, 1, 1, 1);

    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float pos = static_cast<float>(i) * spacing;
        bool isCenter = (i == 0);

        // X-axis lines (along Z)
        AddLine(glm::vec3(-halfExtent * spacing, 0, pos),
                glm::vec3(halfExtent * spacing, 0, pos),
                isCenter ? axisColor : color);

        // Z-axis lines (along X)
        AddLine(glm::vec3(pos, 0, -halfExtent * spacing),
                glm::vec3(pos, 0, halfExtent * spacing),
                isCenter ? axisColor : color);
    }
}

void DebugDraw::AddAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color) {
    // Bottom face
    AddLine(glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z), color);
    AddLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, min.y, max.z), color);
    AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
    AddLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, min.y, min.z), color);

    // Top face
    AddLine(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), color);
    AddLine(glm::vec3(max.x, max.y, min.z), glm::vec3(max.x, max.y, max.z), color);
    AddLine(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
    AddLine(glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z), color);

    // Vertical edges
    AddLine(glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z), color);
    AddLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);
    AddLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
    AddLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
}

void DebugDraw::AddBox(const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color) {
    // Generate 8 corners
    glm::vec3 corners[8] = {
        glm::vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z),
        glm::vec3( halfExtents.x, -halfExtents.y, -halfExtents.z),
        glm::vec3( halfExtents.x, -halfExtents.y,  halfExtents.z),
        glm::vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z),
        glm::vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z),
        glm::vec3( halfExtents.x,  halfExtents.y, -halfExtents.z),
        glm::vec3( halfExtents.x,  halfExtents.y,  halfExtents.z),
        glm::vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z)
    };

    // Transform corners
    for (auto& corner : corners) {
        corner = glm::vec3(transform * glm::vec4(corner, 1.0f));
    }

    // Bottom
    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    // Top
    AddLine(corners[4], corners[5], color);
    AddLine(corners[5], corners[6], color);
    AddLine(corners[6], corners[7], color);
    AddLine(corners[7], corners[4], color);

    // Verticals
    AddLine(corners[0], corners[4], color);
    AddLine(corners[1], corners[5], color);
    AddLine(corners[2], corners[6], color);
    AddLine(corners[3], corners[7], color);
}

void DebugDraw::AddSphere(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
    // Draw three circles for each axis
    AddCircle(center, radius, glm::vec3(1, 0, 0), color, segments);
    AddCircle(center, radius, glm::vec3(0, 1, 0), color, segments);
    AddCircle(center, radius, glm::vec3(0, 0, 1), color, segments);
}

void DebugDraw::AddCircle(const glm::vec3& center, float radius, const glm::vec3& normal,
                          const glm::vec4& color, int segments) {
    // Calculate perpendicular vectors
    glm::vec3 up = glm::normalize(normal);
    glm::vec3 right;

    if (std::abs(up.y) < 0.999f) {
        right = glm::normalize(glm::cross(up, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(up, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::cross(right, up);

    float angleStep = glm::two_pi<float>() / static_cast<float>(segments);

    glm::vec3 prevPoint = center + right * radius;
    for (int i = 1; i <= segments; ++i) {
        float angle = angleStep * static_cast<float>(i);
        glm::vec3 point = center + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(prevPoint, point, color);
        prevPoint = point;
    }
}

void DebugDraw::AddCylinder(const glm::vec3& base, float height, float radius,
                            const glm::vec4& color, int segments) {
    glm::vec3 top = base + glm::vec3(0, height, 0);

    // Draw top and bottom circles
    AddCircle(base, radius, glm::vec3(0, 1, 0), color, segments);
    AddCircle(top, radius, glm::vec3(0, 1, 0), color, segments);

    // Draw vertical lines
    float angleStep = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; i += segments / 4) {
        float angle = angleStep * static_cast<float>(i);
        glm::vec3 offset(std::cos(angle) * radius, 0, std::sin(angle) * radius);
        AddLine(base + offset, top + offset, color);
    }
}

void DebugDraw::AddCone(const glm::vec3& apex, const glm::vec3& base, float radius,
                        const glm::vec4& color, int segments) {
    glm::vec3 axis = base - apex;
    glm::vec3 normal = glm::normalize(axis);

    // Draw base circle
    AddCircle(base, radius, normal, color, segments);

    // Draw lines from apex to base circle
    glm::vec3 right;
    if (std::abs(normal.y) < 0.999f) {
        right = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::cross(right, normal);

    float angleStep = glm::two_pi<float>() / static_cast<float>(segments);
    for (int i = 0; i < segments; i += segments / 4) {
        float angle = angleStep * static_cast<float>(i);
        glm::vec3 point = base + (right * std::cos(angle) + forward * std::sin(angle)) * radius;
        AddLine(apex, point, color);
    }
}

void DebugDraw::AddArrow(const glm::vec3& start, const glm::vec3& end,
                         const glm::vec4& color, float headSize) {
    AddLine(start, end, color);

    glm::vec3 dir = end - start;
    float length = glm::length(dir);
    if (length < 0.001f) return;

    dir = glm::normalize(dir);

    // Calculate perpendicular vectors
    glm::vec3 right;
    if (std::abs(dir.y) < 0.999f) {
        right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(dir, glm::vec3(1, 0, 0)));
    }
    glm::vec3 up = glm::cross(right, dir);

    float arrowSize = length * headSize;
    glm::vec3 arrowBase = end - dir * arrowSize;

    // Arrow head
    AddLine(end, arrowBase + right * arrowSize * 0.5f, color);
    AddLine(end, arrowBase - right * arrowSize * 0.5f, color);
    AddLine(end, arrowBase + up * arrowSize * 0.5f, color);
    AddLine(end, arrowBase - up * arrowSize * 0.5f, color);
}

void DebugDraw::AddRay(const glm::vec3& origin, const glm::vec3& direction,
                       float length, const glm::vec4& color) {
    AddLine(origin, origin + glm::normalize(direction) * length, color);
}

void DebugDraw::AddPlane(const glm::vec3& center, const glm::vec3& normal,
                         float size, const glm::vec4& color) {
    // Calculate perpendicular vectors
    glm::vec3 up = glm::normalize(normal);
    glm::vec3 right;
    if (std::abs(up.y) < 0.999f) {
        right = glm::normalize(glm::cross(up, glm::vec3(0, 1, 0)));
    } else {
        right = glm::normalize(glm::cross(up, glm::vec3(1, 0, 0)));
    }
    glm::vec3 forward = glm::cross(right, up);

    glm::vec3 corners[4] = {
        center + (-right - forward) * size,
        center + ( right - forward) * size,
        center + ( right + forward) * size,
        center + (-right + forward) * size
    };

    AddLine(corners[0], corners[1], color);
    AddLine(corners[1], corners[2], color);
    AddLine(corners[2], corners[3], color);
    AddLine(corners[3], corners[0], color);

    // Draw normal
    AddArrow(center, center + normal * size * 0.5f, color, 0.2f);
}

void DebugDraw::AddFrustum(const glm::mat4& viewProjection, const glm::vec4& color) {
    glm::mat4 invVP = glm::inverse(viewProjection);

    // NDC corners
    glm::vec4 ndcCorners[8] = {
        {-1, -1, -1, 1}, { 1, -1, -1, 1}, { 1,  1, -1, 1}, {-1,  1, -1, 1},  // Near
        {-1, -1,  1, 1}, { 1, -1,  1, 1}, { 1,  1,  1, 1}, {-1,  1,  1, 1}   // Far
    };

    glm::vec3 worldCorners[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 world = invVP * ndcCorners[i];
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
    AddLine(position - glm::vec3(size, 0, 0), position + glm::vec3(size, 0, 0), color);
    AddLine(position - glm::vec3(0, size, 0), position + glm::vec3(0, size, 0), color);
    AddLine(position - glm::vec3(0, 0, size), position + glm::vec3(0, 0, size), color);
}

void DebugDraw::AddText(const glm::vec3& position, const std::string& text, const glm::vec4& color) {
    // Text rendering would require a font system
    // For now, just draw a marker at the position
    AddPoint(position, 0.1f, color);
}

} // namespace Nova
