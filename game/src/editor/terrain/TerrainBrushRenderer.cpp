#include "TerrainBrushRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

namespace Vehement {

// Shader sources for brush preview rendering
static const char* OUTLINE_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main() {
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

static const char* OUTLINE_FRAGMENT_SHADER = R"(
#version 460 core

uniform vec4 u_Color;

out vec4 FragColor;

void main() {
    FragColor = u_Color;
}
)";

static const char* GRADIENT_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 v_Color;

void main() {
    v_Color = a_Color;
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}
)";

static const char* GRADIENT_FRAGMENT_SHADER = R"(
#version 460 core

in vec4 v_Color;

out vec4 FragColor;

void main() {
    FragColor = v_Color;
}
)";

// =============================================================================
// TerrainBrushRenderer
// =============================================================================

TerrainBrushRenderer::TerrainBrushRenderer() = default;
TerrainBrushRenderer::~TerrainBrushRenderer() = default;

bool TerrainBrushRenderer::Initialize(Nova::Renderer& renderer) {
    m_renderer = &renderer;

    // Create shaders
    m_outlineShader = std::make_shared<Nova::Shader>();
    if (!m_outlineShader->LoadFromSource(OUTLINE_VERTEX_SHADER, OUTLINE_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create outline shader for brush preview");
        return false;
    }

    m_gradientShader = std::make_shared<Nova::Shader>();
    if (!m_gradientShader->LoadFromSource(GRADIENT_VERTEX_SHADER, GRADIENT_FRAGMENT_SHADER)) {
        spdlog::error("Failed to create gradient shader for brush preview");
        return false;
    }

    m_previewShader = m_gradientShader;  // Reuse gradient shader

    m_initialized = true;
    return true;
}

void TerrainBrushRenderer::Shutdown() {
    m_outlineShader.reset();
    m_gradientShader.reset();
    m_previewShader.reset();
    m_circleMesh.reset();
    m_squareMesh.reset();
    m_diamondMesh.reset();
    m_gradientMesh.reset();
    m_previewMesh.reset();
    m_initialized = false;
}

void TerrainBrushRenderer::RenderBrushPreview(const TerrainEditor& editor, const glm::mat4& viewProjection) {
    if (!m_initialized || !editor.HasValidPreview()) {
        return;
    }

    const TerrainBrush& brush = editor.GetBrush();
    glm::vec3 position = editor.GetPreviewPosition();

    // Render outline
    if (m_config.showBrushOutline) {
        std::vector<glm::vec3> outlineVertices;

        switch (brush.shape) {
            case TerrainBrushShape::Sphere:
                GenerateCircleOutline(brush.radius, outlineVertices);
                break;
            case TerrainBrushShape::Cube:
                GenerateSquareOutline(brush.radius, outlineVertices);
                break;
            case TerrainBrushShape::Cylinder:
                GenerateCircleOutline(brush.radius, outlineVertices);
                break;
            default:
                GenerateCircleOutline(brush.radius, outlineVertices);
                break;
        }

        RenderOutline(outlineVertices, position, m_config.brushColor, viewProjection);
    }

    // Render strength gradient
    if (m_config.showStrengthGradient) {
        std::vector<glm::vec3> gradientVertices;
        std::vector<glm::vec4> gradientColors;
        GenerateStrengthGradient(brush, gradientVertices, gradientColors);
        RenderGradient(gradientVertices, gradientColors, position, viewProjection);
    }

    // Render height preview
    if (m_config.showHeightPreview) {
        std::vector<glm::vec3> previewVertices;
        std::vector<glm::vec3> previewNormals;
        GenerateHeightPreview(editor, previewVertices, previewNormals);
        RenderHeightPreview(previewVertices, previewNormals, position, viewProjection);
    }
}

void TerrainBrushRenderer::RenderOtherPlayerBrushes(const std::unordered_map<uint32_t, glm::vec3>& otherEditors,
                                                     const TerrainBrush& brush,
                                                     const glm::mat4& viewProjection) {
    if (!m_initialized || !m_config.showOtherPlayers) {
        return;
    }

    std::vector<glm::vec3> outlineVertices;
    GenerateCircleOutline(brush.radius, outlineVertices);

    for (const auto& [playerId, position] : otherEditors) {
        RenderOutline(outlineVertices, position, m_config.otherPlayerColor, viewProjection);
    }
}

// =============================================================================
// Preview Mesh Generation
// =============================================================================

void TerrainBrushRenderer::GenerateCircleOutline(float radius, std::vector<glm::vec3>& vertices) {
    vertices.clear();
    int segments = m_config.previewResolution;

    for (int i = 0; i <= segments; ++i) {
        float angle = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * glm::pi<float>();
        float x = radius * std::cos(angle);
        float z = radius * std::sin(angle);
        vertices.emplace_back(x, 0.0f, z);
    }
}

void TerrainBrushRenderer::GenerateSquareOutline(float size, std::vector<glm::vec3>& vertices) {
    vertices.clear();

    // Four corners
    vertices.emplace_back(-size, 0.0f, -size);
    vertices.emplace_back(size, 0.0f, -size);
    vertices.emplace_back(size, 0.0f, size);
    vertices.emplace_back(-size, 0.0f, size);
    vertices.emplace_back(-size, 0.0f, -size);  // Close the loop
}

void TerrainBrushRenderer::GenerateDiamondOutline(float size, std::vector<glm::vec3>& vertices) {
    vertices.clear();

    vertices.emplace_back(0.0f, 0.0f, -size);
    vertices.emplace_back(size, 0.0f, 0.0f);
    vertices.emplace_back(0.0f, 0.0f, size);
    vertices.emplace_back(-size, 0.0f, 0.0f);
    vertices.emplace_back(0.0f, 0.0f, -size);  // Close the loop
}

void TerrainBrushRenderer::GenerateStrengthGradient(const TerrainBrush& brush, std::vector<glm::vec3>& vertices, std::vector<glm::vec4>& colors) {
    vertices.clear();
    colors.clear();

    int rings = 10;
    int segments = m_config.previewResolution;

    for (int r = 0; r < rings; ++r) {
        float innerRadius = (static_cast<float>(r) / static_cast<float>(rings)) * brush.radius;
        float outerRadius = (static_cast<float>(r + 1) / static_cast<float>(rings)) * brush.radius;

        float innerStrength = 1.0f - (innerRadius / brush.radius);
        float outerStrength = 1.0f - (outerRadius / brush.radius);

        // Apply falloff
        if (innerStrength > 1.0f - brush.falloff) {
            innerStrength = (innerStrength - (1.0f - brush.falloff)) / brush.falloff;
        } else {
            innerStrength = 1.0f;
        }

        if (outerStrength > 1.0f - brush.falloff) {
            outerStrength = (outerStrength - (1.0f - brush.falloff)) / brush.falloff;
        } else {
            outerStrength = 1.0f;
        }

        glm::vec4 innerColor = m_config.brushColor * glm::vec4(1, 1, 1, innerStrength * brush.strength);
        glm::vec4 outerColor = m_config.brushColor * glm::vec4(1, 1, 1, outerStrength * brush.strength);

        for (int s = 0; s < segments; ++s) {
            float angle1 = (static_cast<float>(s) / static_cast<float>(segments)) * 2.0f * glm::pi<float>();
            float angle2 = (static_cast<float>(s + 1) / static_cast<float>(segments)) * 2.0f * glm::pi<float>();

            // Triangle 1
            vertices.emplace_back(innerRadius * std::cos(angle1), 0.01f, innerRadius * std::sin(angle1));
            colors.push_back(innerColor);

            vertices.emplace_back(outerRadius * std::cos(angle1), 0.01f, outerRadius * std::sin(angle1));
            colors.push_back(outerColor);

            vertices.emplace_back(outerRadius * std::cos(angle2), 0.01f, outerRadius * std::sin(angle2));
            colors.push_back(outerColor);

            // Triangle 2
            vertices.emplace_back(innerRadius * std::cos(angle1), 0.01f, innerRadius * std::sin(angle1));
            colors.push_back(innerColor);

            vertices.emplace_back(outerRadius * std::cos(angle2), 0.01f, outerRadius * std::sin(angle2));
            colors.push_back(outerColor);

            vertices.emplace_back(innerRadius * std::cos(angle2), 0.01f, innerRadius * std::sin(angle2));
            colors.push_back(innerColor);
        }
    }
}

void TerrainBrushRenderer::GenerateHeightPreview(const TerrainEditor& editor, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals) {
    vertices.clear();
    normals.clear();

    // Get preview mesh from editor
    std::vector<uint32_t> indices;
    editor.GetPreviewMesh(vertices, indices);

    // Generate simple normals (all pointing up for now)
    normals.resize(vertices.size(), glm::vec3(0, 1, 0));
}

// =============================================================================
// Render Helpers
// =============================================================================

void TerrainBrushRenderer::RenderOutline(const std::vector<glm::vec3>& vertices, const glm::vec3& position, const glm::vec4& color, const glm::mat4& viewProjection) {
    if (vertices.empty() || !m_outlineShader) {
        return;
    }

    // Create model matrix
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

    // Set shader uniforms
    m_outlineShader->Bind();
    m_outlineShader->SetMat4("u_ViewProjection", viewProjection);
    m_outlineShader->SetMat4("u_Model", model);
    m_outlineShader->SetVec4("u_Color", color);

    // Create dynamic VAO/VBO for the outline vertices
    uint32_t vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);

    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    // Set line width
    glLineWidth(m_config.outlineThickness);

    // Draw as line strip
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(vertices.size()));

    // Reset line width
    glLineWidth(1.0f);

    // Cleanup
    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void TerrainBrushRenderer::RenderGradient(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec4>& colors, const glm::vec3& position, const glm::mat4& viewProjection) {
    if (vertices.empty() || colors.empty() || vertices.size() != colors.size() || !m_gradientShader) {
        return;
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

    m_gradientShader->Bind();
    m_gradientShader->SetMat4("u_ViewProjection", viewProjection);
    m_gradientShader->SetMat4("u_Model", model);

    // Create dynamic VAO/VBOs for position and color data
    uint32_t vao = 0, vboPos = 0, vboCol = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboPos);
    glGenBuffers(1, &vboCol);

    glBindVertexArray(vao);

    // Position buffer (location = 0)
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    // Color buffer (location = 1)
    glBindBuffer(GL_ARRAY_BUFFER, vboCol);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), colors.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Disable depth writing but keep depth testing for proper layering
    glDepthMask(GL_FALSE);

    // Draw as triangles (gradient mesh is generated as triangles)
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    // Restore state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // Cleanup
    glBindVertexArray(0);
    glDeleteBuffers(1, &vboPos);
    glDeleteBuffers(1, &vboCol);
    glDeleteVertexArrays(1, &vao);
}

void TerrainBrushRenderer::RenderHeightPreview(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals, const glm::vec3& position, const glm::mat4& viewProjection) {
    if (vertices.empty() || !m_previewShader) {
        return;
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);

    m_previewShader->Bind();
    m_previewShader->SetMat4("u_ViewProjection", viewProjection);
    m_previewShader->SetMat4("u_Model", model);

    // Generate vertex colors based on height for visualization
    std::vector<glm::vec4> colors;
    colors.reserve(vertices.size());

    // Find height range for normalization
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& v : vertices) {
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }
    float heightRange = std::max(maxY - minY, 0.001f);

    // Generate colors based on height (green at bottom, blue at top)
    for (const auto& v : vertices) {
        float t = (v.y - minY) / heightRange;
        glm::vec4 color(
            0.2f + t * 0.3f,      // R: slightly increase with height
            0.7f - t * 0.3f,      // G: decrease with height
            0.3f + t * 0.5f,      // B: increase with height
            0.5f                   // Alpha: semi-transparent
        );
        colors.push_back(color);
    }

    // Create dynamic VAO/VBOs
    uint32_t vao = 0, vboPos = 0, vboCol = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vboPos);
    glGenBuffers(1, &vboCol);

    glBindVertexArray(vao);

    // Position buffer (location = 0)
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    // Color buffer (location = 1)
    glBindBuffer(GL_ARRAY_BUFFER, vboCol);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec4), colors.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), nullptr);

    // Enable blending and polygon offset for wireframe-like preview
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Draw preview mesh as wireframe
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    // Restore polygon mode and disable blending
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_BLEND);

    // Cleanup
    glBindVertexArray(0);
    glDeleteBuffers(1, &vboPos);
    glDeleteBuffers(1, &vboCol);
    glDeleteVertexArrays(1, &vao);
}

// =============================================================================
// MultiUserEditVisualization
// =============================================================================

void MultiUserEditVisualization::UpdatePlayerCursor(uint32_t playerId, const std::string& name, const glm::vec3& position, const TerrainBrush& brush) {
    // Find existing cursor
    auto it = std::find_if(m_activeCursors.begin(), m_activeCursors.end(),
        [playerId](const PlayerCursor& cursor) { return cursor.playerId == playerId; });

    if (it != m_activeCursors.end()) {
        // Update existing
        it->position = position;
        it->brush = brush;
        it->lastUpdateTime = m_currentTime;
    } else {
        // Add new cursor
        PlayerCursor cursor;
        cursor.playerId = playerId;
        cursor.playerName = name;
        cursor.position = position;
        cursor.brush = brush;
        cursor.lastUpdateTime = m_currentTime;

        // Assign color based on player ID
        float hue = (playerId * 137.5f) / 360.0f;  // Golden angle for good distribution
        cursor.color = glm::vec4(
            std::sin(hue * 6.28f) * 0.5f + 0.5f,
            std::sin((hue + 0.33f) * 6.28f) * 0.5f + 0.5f,
            std::sin((hue + 0.66f) * 6.28f) * 0.5f + 0.5f,
            0.6f
        );

        m_activeCursors.push_back(cursor);
    }
}

void MultiUserEditVisualization::RemovePlayerCursor(uint32_t playerId) {
    m_activeCursors.erase(
        std::remove_if(m_activeCursors.begin(), m_activeCursors.end(),
            [playerId](const PlayerCursor& cursor) { return cursor.playerId == playerId; }),
        m_activeCursors.end()
    );
}

void MultiUserEditVisualization::Update(float deltaTime, float timeout) {
    m_currentTime += deltaTime;

    // Remove stale cursors
    m_activeCursors.erase(
        std::remove_if(m_activeCursors.begin(), m_activeCursors.end(),
            [this, timeout](const PlayerCursor& cursor) {
                return (m_currentTime - cursor.lastUpdateTime) > timeout;
            }),
        m_activeCursors.end()
    );
}

void MultiUserEditVisualization::Render(TerrainBrushRenderer& renderer, const glm::mat4& viewProjection) {
    for (const auto& cursor : m_activeCursors) {
        std::unordered_map<uint32_t, glm::vec3> singleCursor;
        singleCursor[cursor.playerId] = cursor.position;

        // Temporarily set color for this player
        auto oldColor = renderer.GetConfig().otherPlayerColor;
        renderer.SetBrushColor(cursor.color);
        renderer.RenderOtherPlayerBrushes(singleCursor, cursor.brush, viewProjection);
        renderer.SetBrushColor(oldColor);
    }
}

} // namespace Vehement
