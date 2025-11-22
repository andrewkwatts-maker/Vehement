#include "MinimapReveal.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace Vehement2 {
namespace RTS {

// ============================================================================
// Embedded Shaders
// ============================================================================

static const char* MINIMAP_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

uniform mat4 u_Projection;
uniform vec2 u_Position;
uniform vec2 u_Size;

void main() {
    vec2 pos = u_Position + a_Position * u_Size;
    gl_Position = u_Projection * vec4(pos, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

static const char* MINIMAP_FRAGMENT_SHADER = R"(
#version 460 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_TerrainTexture;
uniform sampler2D u_FogTexture;
uniform vec4 u_BackgroundColor;
uniform vec4 u_UnknownColor;
uniform vec4 u_ExploredColor;
uniform vec4 u_VisibleColor;
uniform float u_BorderWidth;
uniform vec4 u_BorderColor;
uniform vec2 u_Size;

void main() {
    vec2 uv = v_TexCoord;

    // Border check
    vec2 borderUV = uv * u_Size;
    if (borderUV.x < u_BorderWidth || borderUV.x > u_Size.x - u_BorderWidth ||
        borderUV.y < u_BorderWidth || borderUV.y > u_Size.y - u_BorderWidth) {
        FragColor = u_BorderColor;
        return;
    }

    // Sample textures
    vec4 terrain = texture(u_TerrainTexture, uv);
    float fogState = texture(u_FogTexture, uv).r;

    // Determine color based on fog state
    vec4 color;
    if (fogState < 0.1) {
        // Unknown - completely black
        color = u_UnknownColor;
    } else if (fogState < 0.5) {
        // Explored - show terrain dimmed
        color = mix(u_ExploredColor, terrain, 0.3);
    } else {
        // Visible - show terrain fully
        color = mix(u_VisibleColor, terrain, 0.7);
    }

    FragColor = color;
}
)";

static const char* MARKER_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec2 a_Position;

uniform mat4 u_Projection;
uniform vec2 u_MarkerPosition;
uniform float u_MarkerSize;
uniform float u_Rotation;

void main() {
    // Rotate
    float c = cos(u_Rotation);
    float s = sin(u_Rotation);
    vec2 rotated = vec2(
        a_Position.x * c - a_Position.y * s,
        a_Position.x * s + a_Position.y * c
    );

    // Scale and position
    vec2 pos = u_MarkerPosition + rotated * u_MarkerSize;
    gl_Position = u_Projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* MARKER_FRAGMENT_SHADER = R"(
#version 460 core

out vec4 FragColor;

uniform vec4 u_MarkerColor;
uniform float u_PulseAlpha;

void main() {
    FragColor = u_MarkerColor;
    FragColor.a *= u_PulseAlpha;
}
)";

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t CompileShader(const std::string& vertexSource, const std::string& fragmentSource) {
    // Compile vertex shader
    uint32_t vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vSrc = vertexSource.c_str();
    glShaderSource(vertexShader, 1, &vSrc, nullptr);
    glCompileShader(vertexShader);

    int success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("Vertex shader compilation failed:\n{}", infoLog);
        glDeleteShader(vertexShader);
        return 0;
    }

    // Compile fragment shader
    uint32_t fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fSrc = fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &fSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("Fragment shader compilation failed:\n{}", infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    // Link program
    uint32_t program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("Shader program linking failed:\n{}", infoLog);
        glDeleteProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

// ============================================================================
// MinimapReveal Implementation
// ============================================================================

MinimapReveal::MinimapReveal() = default;

MinimapReveal::~MinimapReveal() {
    Shutdown();
}

bool MinimapReveal::Initialize(SessionFogOfWar* fogOfWar, int mapWidth, int mapHeight, float tileSize) {
    if (m_initialized) {
        spdlog::warn("MinimapReveal already initialized");
        return true;
    }

    m_fogOfWar = fogOfWar;
    m_mapWidth = mapWidth;
    m_mapHeight = mapHeight;
    m_tileSize = tileSize;

    // Calculate scale factor
    m_worldToMinimapScale = static_cast<float>(m_config.width) / (mapWidth * tileSize);

    if (!CreateResources()) {
        spdlog::error("Failed to create minimap resources");
        return false;
    }

    m_initialized = true;
    spdlog::info("MinimapReveal initialized for {}x{} map", mapWidth, mapHeight);
    return true;
}

void MinimapReveal::Shutdown() {
    if (!m_initialized) return;

    DestroyResources();
    m_markers.clear();
    m_terrainData.clear();

    m_initialized = false;
    spdlog::info("MinimapReveal shutdown");
}

bool MinimapReveal::CreateResources() {
    // Create shaders
    m_minimapShader = CompileShader(MINIMAP_VERTEX_SHADER, MINIMAP_FRAGMENT_SHADER);
    if (m_minimapShader == 0) return false;

    m_markerShader = CompileShader(MARKER_VERTEX_SHADER, MARKER_FRAGMENT_SHADER);
    if (m_markerShader == 0) return false;

    // Create quad geometry
    float quadVertices[] = {
        // Position    // TexCoord
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 0.0f,    1.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        0.0f, 0.0f,    0.0f, 0.0f,
        1.0f, 1.0f,    1.0f, 1.0f,
        0.0f, 1.0f,    0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // Create minimap texture
    glGenTextures(1, &m_minimapTexture);
    glBindTexture(GL_TEXTURE_2D, m_minimapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_config.width, m_config.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create fog texture (minimap resolution)
    glGenTextures(1, &m_fogTexture);
    glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_mapWidth, m_mapHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create terrain texture
    glGenTextures(1, &m_terrainTexture);
    glBindTexture(GL_TEXTURE_2D, m_terrainTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_mapWidth, m_mapHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create framebuffer
    glGenFramebuffers(1, &m_minimapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_minimapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_minimapTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        spdlog::error("Minimap framebuffer incomplete");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void MinimapReveal::DestroyResources() {
    if (m_minimapShader) {
        glDeleteProgram(m_minimapShader);
        m_minimapShader = 0;
    }
    if (m_markerShader) {
        glDeleteProgram(m_markerShader);
        m_markerShader = 0;
    }
    if (m_minimapTexture) {
        glDeleteTextures(1, &m_minimapTexture);
        m_minimapTexture = 0;
    }
    if (m_fogTexture) {
        glDeleteTextures(1, &m_fogTexture);
        m_fogTexture = 0;
    }
    if (m_terrainTexture) {
        glDeleteTextures(1, &m_terrainTexture);
        m_terrainTexture = 0;
    }
    if (m_minimapFBO) {
        glDeleteFramebuffers(1, &m_minimapFBO);
        m_minimapFBO = 0;
    }
    if (m_quadVAO) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
}

void MinimapReveal::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateMarkers(deltaTime);
    UpdateMinimapTexture();
}

void MinimapReveal::Render() {
    if (!m_initialized) return;

    // Render to minimap framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_minimapFBO);
    glViewport(0, 0, m_config.width, m_config.height);

    // Clear with background color
    glClearColor(m_config.backgroundColor.r, m_config.backgroundColor.g,
                 m_config.backgroundColor.b, m_config.backgroundColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render layers
    RenderTerrainLayer();
    RenderFogLayer();
    RenderMarkers();
    RenderViewport();

    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MinimapReveal::UpdateMinimapTexture() {
    if (!m_fogOfWar) return;

    // Update fog texture from fog of war system
    std::vector<uint8_t> fogData(m_mapWidth * m_mapHeight);

    for (int y = 0; y < m_mapHeight; y++) {
        for (int x = 0; x < m_mapWidth; x++) {
            FogState state = m_fogOfWar->GetFogState(glm::ivec2(x, y));
            uint8_t value = 0;
            switch (state) {
                case FogState::Unknown: value = 0; break;
                case FogState::Explored: value = 128; break;
                case FogState::Visible: value = 255; break;
            }
            fogData[y * m_mapWidth + x] = value;
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RED, GL_UNSIGNED_BYTE, fogData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MinimapReveal::RenderTerrainLayer() {
    // Simple terrain rendering - actual implementation would use terrain texture
    // For now, just fill with default ground color
}

void MinimapReveal::RenderFogLayer() {
    // Fog is applied in the composite shader
}

void MinimapReveal::RenderMarkers() {
    if (m_markers.empty()) return;

    glUseProgram(m_markerShader);

    // Set up projection (orthographic, minimap space)
    // This is simplified - actual implementation needs proper projection

    for (const auto& marker : m_markers) {
        if (!marker.visible) continue;

        // Check visibility based on fog of war
        bool showMarker = false;

        switch (marker.type) {
            case MinimapMarkerType::Player:
            case MinimapMarkerType::AllyUnit:
            case MinimapMarkerType::AllyBuilding:
            case MinimapMarkerType::Ping:
            case MinimapMarkerType::Alert:
            case MinimapMarkerType::Objective:
                // Always show these
                showMarker = true;
                break;

            case MinimapMarkerType::Discovery:
            case MinimapMarkerType::ResourceNode:
                // Show if explored
                showMarker = ShouldShowOnMinimap(marker.worldPosition, false);
                break;

            case MinimapMarkerType::EnemyUnit:
            case MinimapMarkerType::EnemyBuilding:
                // Only show if visible
                showMarker = ShouldShowOnMinimap(marker.worldPosition, true);
                break;

            default:
                break;
        }

        if (!showMarker) continue;

        glm::vec2 minimapPos = WorldToMinimap(marker.worldPosition);
        glm::vec4 color = GetMarkerColor(marker.type);
        float size = GetMarkerSize(marker.type);

        // Apply pulse effect
        float pulseAlpha = 1.0f;
        if (marker.pulsing) {
            pulseAlpha = 0.5f + 0.5f * std::sin(marker.pulseTimer * 4.0f);
        }

        // Set uniforms and draw
        glUniform2fv(glGetUniformLocation(m_markerShader, "u_MarkerPosition"), 1,
                     glm::value_ptr(minimapPos));
        glUniform4fv(glGetUniformLocation(m_markerShader, "u_MarkerColor"), 1,
                     glm::value_ptr(color));
        glUniform1f(glGetUniformLocation(m_markerShader, "u_MarkerSize"), size);
        glUniform1f(glGetUniformLocation(m_markerShader, "u_Rotation"), marker.rotation);
        glUniform1f(glGetUniformLocation(m_markerShader, "u_PulseAlpha"), pulseAlpha);

        // Draw marker (simplified - actual implementation would use proper geometry)
        glBindVertexArray(m_quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

void MinimapReveal::RenderViewport() {
    if (!m_config.showCameraViewport) return;

    // Draw camera viewport rectangle
    // Implementation depends on rendering system
}

void MinimapReveal::UpdateMarkers(float deltaTime) {
    // Update pulse timers and lifetimes
    for (auto it = m_markers.begin(); it != m_markers.end();) {
        it->pulseTimer += deltaTime;

        if (it->lifetime > 0.0f) {
            it->lifetime -= deltaTime;
            if (it->lifetime <= 0.0f) {
                it = m_markers.erase(it);
                continue;
            }
        }

        // Update minimap position
        it->minimapPosition = WorldToMinimap(it->worldPosition);

        ++it;
    }
}

void MinimapReveal::SetTerrainData(const uint8_t* terrainTypes, int width, int height) {
    if (width != m_mapWidth || height != m_mapHeight) {
        spdlog::warn("Terrain data dimensions don't match map size");
        return;
    }

    m_terrainData.assign(terrainTypes, terrainTypes + (width * height));

    // Create RGBA terrain texture
    std::vector<uint8_t> terrainRGBA(m_mapWidth * m_mapHeight * 4);

    for (int i = 0; i < m_mapWidth * m_mapHeight; i++) {
        glm::vec4 color;
        switch (terrainTypes[i]) {
            case 0: color = m_config.terrainGround; break;
            case 1: color = m_config.terrainWater; break;
            case 2: color = m_config.terrainWall; break;
            case 3: color = m_config.terrainRoad; break;
            default: color = m_config.terrainGround; break;
        }

        terrainRGBA[i * 4 + 0] = static_cast<uint8_t>(color.r * 255);
        terrainRGBA[i * 4 + 1] = static_cast<uint8_t>(color.g * 255);
        terrainRGBA[i * 4 + 2] = static_cast<uint8_t>(color.b * 255);
        terrainRGBA[i * 4 + 3] = static_cast<uint8_t>(color.a * 255);
    }

    glBindTexture(GL_TEXTURE_2D, m_terrainTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RGBA, GL_UNSIGNED_BYTE, terrainRGBA.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MinimapReveal::SetCameraPosition(const glm::vec2& position, const glm::vec2& viewSize) {
    m_cameraPosition = position;
    m_cameraViewSize = viewSize;
}

size_t MinimapReveal::AddMarker(const MinimapMarker& marker) {
    m_markers.push_back(marker);
    m_markers.back().minimapPosition = WorldToMinimap(marker.worldPosition);
    return m_markers.size() - 1;
}

void MinimapReveal::UpdateMarkerPosition(uint32_t entityId, const glm::vec2& worldPosition) {
    for (auto& marker : m_markers) {
        if (marker.entityId == entityId) {
            marker.worldPosition = worldPosition;
            marker.minimapPosition = WorldToMinimap(worldPosition);
            return;
        }
    }
}

void MinimapReveal::RemoveMarker(uint32_t entityId) {
    m_markers.erase(
        std::remove_if(m_markers.begin(), m_markers.end(),
            [entityId](const MinimapMarker& m) { return m.entityId == entityId; }),
        m_markers.end()
    );
}

void MinimapReveal::RemoveMarkersOfType(MinimapMarkerType type) {
    m_markers.erase(
        std::remove_if(m_markers.begin(), m_markers.end(),
            [type](const MinimapMarker& m) { return m.type == type; }),
        m_markers.end()
    );
}

void MinimapReveal::ClearMarkers() {
    m_markers.clear();
}

void MinimapReveal::AddPing(const glm::vec2& worldPosition) {
    if (!m_config.enablePinging) return;

    MinimapMarker ping;
    ping.type = MinimapMarkerType::Ping;
    ping.worldPosition = worldPosition;
    ping.color = m_config.pingColor;
    ping.size = 8.0f;
    ping.pulsing = true;
    ping.lifetime = m_config.pingDuration;

    AddMarker(ping);
}

void MinimapReveal::AddAlert(const glm::vec2& worldPosition, float duration) {
    MinimapMarker alert;
    alert.type = MinimapMarkerType::Alert;
    alert.worldPosition = worldPosition;
    alert.color = m_config.alertColor;
    alert.size = 10.0f;
    alert.pulsing = true;
    alert.lifetime = duration;

    AddMarker(alert);
}

bool MinimapReveal::ShouldShowOnMinimap(const glm::vec2& worldPosition, bool requireVisible) const {
    if (!m_fogOfWar) return true;

    glm::ivec2 tile = glm::ivec2(
        static_cast<int>(worldPosition.x / m_tileSize),
        static_cast<int>(worldPosition.y / m_tileSize)
    );

    if (requireVisible) {
        return m_fogOfWar->IsVisible(tile);
    } else {
        return m_fogOfWar->IsExplored(tile);
    }
}

glm::vec2 MinimapReveal::WorldToMinimap(const glm::vec2& worldPosition) const {
    return glm::vec2(
        worldPosition.x * m_worldToMinimapScale + m_config.screenPosition.x,
        worldPosition.y * m_worldToMinimapScale + m_config.screenPosition.y
    );
}

glm::vec2 MinimapReveal::MinimapToWorld(const glm::vec2& minimapPosition) const {
    return glm::vec2(
        (minimapPosition.x - m_config.screenPosition.x) / m_worldToMinimapScale,
        (minimapPosition.y - m_config.screenPosition.y) / m_worldToMinimapScale
    );
}

bool MinimapReveal::IsPointOverMinimap(const glm::vec2& screenPosition) const {
    return screenPosition.x >= m_config.screenPosition.x &&
           screenPosition.x <= m_config.screenPosition.x + m_config.width &&
           screenPosition.y >= m_config.screenPosition.y &&
           screenPosition.y <= m_config.screenPosition.y + m_config.height;
}

void MinimapReveal::SetConfig(const MinimapConfig& config) {
    bool sizeChanged = (config.width != m_config.width || config.height != m_config.height);
    m_config = config;

    if (sizeChanged && m_initialized) {
        // Recreate textures with new size
        // Implementation would resize textures here
        m_worldToMinimapScale = static_cast<float>(m_config.width) / (m_mapWidth * m_tileSize);
    }
}

void MinimapReveal::SetSize(int width, int height) {
    m_config.width = width;
    m_config.height = height;
    m_worldToMinimapScale = static_cast<float>(width) / (m_mapWidth * m_tileSize);

    // Recreate minimap texture
    if (m_minimapTexture) {
        glBindTexture(GL_TEXTURE_2D, m_minimapTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

glm::vec2 MinimapReveal::HandleClick(const glm::vec2& screenPosition) const {
    if (!IsPointOverMinimap(screenPosition)) {
        return glm::vec2(-1.0f, -1.0f);
    }

    return MinimapToWorld(screenPosition);
}

bool MinimapReveal::WasClickOnMinimap(const glm::vec2& screenPosition) const {
    return IsPointOverMinimap(screenPosition);
}

glm::vec4 MinimapReveal::GetMarkerColor(MinimapMarkerType type) const {
    switch (type) {
        case MinimapMarkerType::Player: return m_config.playerColor;
        case MinimapMarkerType::AllyUnit: return m_config.allyUnitColor;
        case MinimapMarkerType::AllyBuilding: return m_config.allyBuildingColor;
        case MinimapMarkerType::EnemyUnit: return m_config.enemyUnitColor;
        case MinimapMarkerType::EnemyBuilding: return m_config.enemyBuildingColor;
        case MinimapMarkerType::ResourceNode: return m_config.resourceColor;
        case MinimapMarkerType::Discovery: return m_config.discoveryColor;
        case MinimapMarkerType::Objective: return m_config.objectiveColor;
        case MinimapMarkerType::Ping: return m_config.pingColor;
        case MinimapMarkerType::Alert: return m_config.alertColor;
        default: return glm::vec4(1.0f);
    }
}

float MinimapReveal::GetMarkerSize(MinimapMarkerType type) const {
    switch (type) {
        case MinimapMarkerType::Player: return m_config.playerSize;
        case MinimapMarkerType::AllyUnit:
        case MinimapMarkerType::EnemyUnit: return m_config.unitSize;
        case MinimapMarkerType::AllyBuilding:
        case MinimapMarkerType::EnemyBuilding: return m_config.buildingSize;
        case MinimapMarkerType::ResourceNode: return m_config.resourceSize;
        case MinimapMarkerType::Discovery: return m_config.discoverySize;
        default: return 4.0f;
    }
}

} // namespace RTS
} // namespace Vehement2
