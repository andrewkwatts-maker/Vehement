#include "SessionFogOfWar.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace Vehement2 {
namespace RTS {

// ============================================================================
// Embedded Shaders
// ============================================================================

static const char* SESSION_FOG_UPDATE_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output fog texture
layout(r16f, binding = 0) uniform image2D u_FogOutput;

// Input textures
layout(r8, binding = 1) uniform readonly image2D u_ExploredState;
layout(r8, binding = 2) uniform readonly image2D u_VisibilityState;
layout(rgba16f, binding = 3) uniform readonly image2D u_RadianceTexture;

uniform vec2 u_ScreenSize;
uniform vec2 u_MapSize;
uniform float u_TileSize;
uniform float u_DeltaTime;
uniform float u_TransitionSpeed;
uniform float u_UnknownBrightness;
uniform float u_ExploredBrightness;
uniform float u_VisibleBrightness;
uniform bool u_SmoothEdges;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= int(u_ScreenSize.x) || pixelCoord.y >= int(u_ScreenSize.y)) {
        return;
    }

    // Convert screen position to tile position
    vec2 screenPos = vec2(pixelCoord) + 0.5;
    ivec2 tileCoord = ivec2(screenPos / u_TileSize);
    tileCoord = clamp(tileCoord, ivec2(0), ivec2(u_MapSize) - 1);

    // Sample states
    float explored = imageLoad(u_ExploredState, tileCoord).r;
    float visible = imageLoad(u_VisibilityState, tileCoord).r;

    // Determine target brightness based on fog state
    float targetBrightness;
    if (visible > 0.5) {
        targetBrightness = u_VisibleBrightness;
    } else if (explored > 0.5) {
        targetBrightness = u_ExploredBrightness;
    } else {
        targetBrightness = u_UnknownBrightness;
    }

    // Optional smooth edge sampling
    if (u_SmoothEdges) {
        // Sample neighboring tiles for smooth transitions
        float neighborSum = 0.0;
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                ivec2 neighborTile = tileCoord + ivec2(dx, dy);
                if (neighborTile.x >= 0 && neighborTile.x < int(u_MapSize.x) &&
                    neighborTile.y >= 0 && neighborTile.y < int(u_MapSize.y)) {
                    float nExplored = imageLoad(u_ExploredState, neighborTile).r;
                    float nVisible = imageLoad(u_VisibilityState, neighborTile).r;

                    float nBrightness;
                    if (nVisible > 0.5) nBrightness = u_VisibleBrightness;
                    else if (nExplored > 0.5) nBrightness = u_ExploredBrightness;
                    else nBrightness = u_UnknownBrightness;

                    neighborSum += nBrightness;
                    count++;
                }
            }
        }
        targetBrightness = mix(targetBrightness, neighborSum / float(count), 0.3);
    }

    // Get current brightness and smoothly transition
    float currentBrightness = imageLoad(u_FogOutput, pixelCoord).r;
    float newBrightness = mix(currentBrightness, targetBrightness,
                              1.0 - exp(-u_TransitionSpeed * u_DeltaTime));

    imageStore(u_FogOutput, pixelCoord, vec4(newBrightness, 0.0, 0.0, 1.0));
}
)";

static const char* SESSION_FOG_COMBINE_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output
layout(rgba16f, binding = 0) uniform writeonly image2D u_CombinedOutput;

// Inputs
layout(r16f, binding = 1) uniform readonly image2D u_FogTexture;
layout(rgba16f, binding = 2) uniform readonly image2D u_RadianceTexture;
layout(r8, binding = 3) uniform readonly image2D u_ExploredState;
layout(r8, binding = 4) uniform readonly image2D u_VisibilityState;

uniform vec2 u_ScreenSize;
uniform vec2 u_MapSize;
uniform float u_TileSize;
uniform vec3 u_FogColor;
uniform vec3 u_ExploredTint;
uniform float u_ExploredBrightness;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= int(u_ScreenSize.x) || pixelCoord.y >= int(u_ScreenSize.y)) {
        return;
    }

    // Sample inputs
    float fogFactor = imageLoad(u_FogTexture, pixelCoord).r;
    vec4 radiance = imageLoad(u_RadianceTexture, pixelCoord);

    // Get tile state
    vec2 screenPos = vec2(pixelCoord) + 0.5;
    ivec2 tileCoord = ivec2(screenPos / u_TileSize);
    tileCoord = clamp(tileCoord, ivec2(0), ivec2(u_MapSize) - 1);

    float explored = imageLoad(u_ExploredState, tileCoord).r;
    float visible = imageLoad(u_VisibilityState, tileCoord).r;

    // Combine fog with lighting
    vec3 finalColor;

    if (fogFactor < 0.01) {
        // Unknown - completely dark
        finalColor = u_FogColor;
    } else if (visible < 0.5 && explored > 0.5) {
        // Explored but not visible - show terrain with tint, dim lighting
        finalColor = radiance.rgb * fogFactor * u_ExploredTint;
    } else {
        // Visible - full radiance
        finalColor = radiance.rgb * fogFactor;
    }

    // Output with fog factor in alpha for potential masking
    imageStore(u_CombinedOutput, pixelCoord, vec4(finalColor, fogFactor));
}
)";

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t CompileComputeShader(const std::string& source) {
    uint32_t shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("SessionFogOfWar compute shader compilation failed:\n{}", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    uint32_t program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        spdlog::error("SessionFogOfWar compute shader linking failed:\n{}", infoLog);
        glDeleteProgram(program);
        glDeleteShader(shader);
        return 0;
    }

    glDeleteShader(shader);
    return program;
}

// ============================================================================
// SessionFogOfWar Implementation
// ============================================================================

SessionFogOfWar::SessionFogOfWar() = default;

SessionFogOfWar::~SessionFogOfWar() {
    Shutdown();
}

SessionFogOfWar::SessionFogOfWar(SessionFogOfWar&& other) noexcept
    : m_mapWidth(other.m_mapWidth)
    , m_mapHeight(other.m_mapHeight)
    , m_tileSize(other.m_tileSize)
    , m_screenWidth(other.m_screenWidth)
    , m_screenHeight(other.m_screenHeight)
    , m_config(other.m_config)
    , m_initialized(other.m_initialized)
    , m_sessionStartTime(other.m_sessionStartTime)
    , m_lastActivityTime(other.m_lastActivityTime)
    , m_sessionActive(other.m_sessionActive)
    , m_fogState(std::move(other.m_fogState))
    , m_visibilityState(std::move(other.m_visibilityState))
    , m_fogBrightness(std::move(other.m_fogBrightness))
    , m_occlusionData(std::move(other.m_occlusionData))
    , m_occlusionWidth(other.m_occlusionWidth)
    , m_occlusionHeight(other.m_occlusionHeight)
    , m_tilesExploredCount(other.m_tilesExploredCount)
    , m_tilesVisibleCount(other.m_tilesVisibleCount)
    , m_lastExplorationPercent(other.m_lastExplorationPercent)
    , m_fogTexture(other.m_fogTexture)
    , m_combinedTexture(other.m_combinedTexture)
    , m_exploredTexture(other.m_exploredTexture)
    , m_visibilityTexture(other.m_visibilityTexture)
    , m_fogUpdateShader(other.m_fogUpdateShader)
    , m_fogCombineShader(other.m_fogCombineShader)
    , m_radianceCascades(other.m_radianceCascades)
{
    other.m_initialized = false;
    other.m_fogTexture = 0;
    other.m_combinedTexture = 0;
    other.m_exploredTexture = 0;
    other.m_visibilityTexture = 0;
    other.m_fogUpdateShader = 0;
    other.m_fogCombineShader = 0;
}

SessionFogOfWar& SessionFogOfWar::operator=(SessionFogOfWar&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_mapWidth = other.m_mapWidth;
        m_mapHeight = other.m_mapHeight;
        m_tileSize = other.m_tileSize;
        m_screenWidth = other.m_screenWidth;
        m_screenHeight = other.m_screenHeight;
        m_config = other.m_config;
        m_initialized = other.m_initialized;
        m_sessionStartTime = other.m_sessionStartTime;
        m_lastActivityTime = other.m_lastActivityTime;
        m_sessionActive = other.m_sessionActive;
        m_fogState = std::move(other.m_fogState);
        m_visibilityState = std::move(other.m_visibilityState);
        m_fogBrightness = std::move(other.m_fogBrightness);
        m_occlusionData = std::move(other.m_occlusionData);
        m_occlusionWidth = other.m_occlusionWidth;
        m_occlusionHeight = other.m_occlusionHeight;
        m_tilesExploredCount = other.m_tilesExploredCount;
        m_tilesVisibleCount = other.m_tilesVisibleCount;
        m_lastExplorationPercent = other.m_lastExplorationPercent;
        m_fogTexture = other.m_fogTexture;
        m_combinedTexture = other.m_combinedTexture;
        m_exploredTexture = other.m_exploredTexture;
        m_visibilityTexture = other.m_visibilityTexture;
        m_fogUpdateShader = other.m_fogUpdateShader;
        m_fogCombineShader = other.m_fogCombineShader;
        m_radianceCascades = other.m_radianceCascades;

        other.m_initialized = false;
        other.m_fogTexture = 0;
        other.m_combinedTexture = 0;
        other.m_exploredTexture = 0;
        other.m_visibilityTexture = 0;
        other.m_fogUpdateShader = 0;
        other.m_fogCombineShader = 0;
    }
    return *this;
}

bool SessionFogOfWar::Initialize(int mapWidth, int mapHeight, float tileSize,
                                  int screenWidth, int screenHeight) {
    if (m_initialized) {
        spdlog::warn("SessionFogOfWar already initialized, shutting down first");
        Shutdown();
    }

    m_mapWidth = mapWidth;
    m_mapHeight = mapHeight;
    m_tileSize = tileSize;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    spdlog::info("Initializing SessionFogOfWar: {}x{} tiles, {}x{} screen",
                 mapWidth, mapHeight, screenWidth, screenHeight);

    // Initialize state arrays
    int tileCount = mapWidth * mapHeight;
    m_fogState.resize(tileCount, FogState::Unknown);
    m_visibilityState.resize(tileCount, 0);
    m_fogBrightness.resize(tileCount, 0.0f);

    if (!CreateShaders()) {
        spdlog::error("Failed to create SessionFogOfWar shaders");
        return false;
    }

    if (!CreateTextures()) {
        spdlog::error("Failed to create SessionFogOfWar textures");
        DestroyResources();
        return false;
    }

    // Start session
    m_sessionStartTime = std::chrono::steady_clock::now();
    m_lastActivityTime = m_sessionStartTime;
    m_sessionActive = true;

    m_initialized = true;
    spdlog::info("SessionFogOfWar initialized successfully");
    return true;
}

void SessionFogOfWar::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down SessionFogOfWar");
    DestroyResources();
    m_initialized = false;
}

void SessionFogOfWar::Resize(int screenWidth, int screenHeight) {
    if (screenWidth == m_screenWidth && screenHeight == m_screenHeight) return;

    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    // Recreate screen-sized textures
    if (m_fogTexture) {
        glDeleteTextures(1, &m_fogTexture);
        m_fogTexture = 0;
    }
    if (m_combinedTexture) {
        glDeleteTextures(1, &m_combinedTexture);
        m_combinedTexture = 0;
    }

    CreateTextures();
}

bool SessionFogOfWar::CreateShaders() {
    m_fogUpdateShader = CompileComputeShader(SESSION_FOG_UPDATE_SHADER);
    if (m_fogUpdateShader == 0) return false;

    m_fogCombineShader = CompileComputeShader(SESSION_FOG_COMBINE_SHADER);
    if (m_fogCombineShader == 0) return false;

    return true;
}

bool SessionFogOfWar::CreateTextures() {
    // Fog texture (screen resolution)
    glGenTextures(1, &m_fogTexture);
    glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_screenWidth, m_screenHeight,
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Combined texture (screen resolution)
    glGenTextures(1, &m_combinedTexture);
    glBindTexture(GL_TEXTURE_2D, m_combinedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_screenWidth, m_screenHeight,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Explored state texture (map resolution)
    std::vector<uint8_t> exploredData(m_mapWidth * m_mapHeight, 0);
    glGenTextures(1, &m_exploredTexture);
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_mapWidth, m_mapHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, exploredData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Visibility state texture (map resolution)
    std::vector<uint8_t> visibilityData(m_mapWidth * m_mapHeight, 0);
    glGenTextures(1, &m_visibilityTexture);
    glBindTexture(GL_TEXTURE_2D, m_visibilityTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_mapWidth, m_mapHeight,
                 0, GL_RED, GL_UNSIGNED_BYTE, visibilityData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void SessionFogOfWar::DestroyResources() {
    if (m_fogTexture) {
        glDeleteTextures(1, &m_fogTexture);
        m_fogTexture = 0;
    }
    if (m_combinedTexture) {
        glDeleteTextures(1, &m_combinedTexture);
        m_combinedTexture = 0;
    }
    if (m_exploredTexture) {
        glDeleteTextures(1, &m_exploredTexture);
        m_exploredTexture = 0;
    }
    if (m_visibilityTexture) {
        glDeleteTextures(1, &m_visibilityTexture);
        m_visibilityTexture = 0;
    }
    if (m_fogUpdateShader) {
        glDeleteProgram(m_fogUpdateShader);
        m_fogUpdateShader = 0;
    }
    if (m_fogCombineShader) {
        glDeleteProgram(m_fogCombineShader);
        m_fogCombineShader = 0;
    }

    m_fogState.clear();
    m_visibilityState.clear();
    m_fogBrightness.clear();
    m_occlusionData.clear();
}

// ============================================================================
// Session Management
// ============================================================================

void SessionFogOfWar::ResetFogOfWar() {
    spdlog::info("Resetting session fog of war");

    // Reset all tiles to unknown
    std::fill(m_fogState.begin(), m_fogState.end(), FogState::Unknown);
    std::fill(m_visibilityState.begin(), m_visibilityState.end(), 0);
    std::fill(m_fogBrightness.begin(), m_fogBrightness.end(), 0.0f);

    // Reset statistics
    m_tilesExploredCount = 0;
    m_tilesVisibleCount = 0;
    m_lastExplorationPercent = 0.0f;

    // Reset session timing
    m_sessionStartTime = std::chrono::steady_clock::now();
    m_lastActivityTime = m_sessionStartTime;
    m_sessionActive = true;

    // Update textures
    std::vector<uint8_t> emptyData(m_mapWidth * m_mapHeight, 0);

    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RED, GL_UNSIGNED_BYTE, emptyData.data());

    glBindTexture(GL_TEXTURE_2D, m_visibilityTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RED, GL_UNSIGNED_BYTE, emptyData.data());

    glBindTexture(GL_TEXTURE_2D, 0);

    // Notify callback
    if (m_onSessionReset) {
        m_onSessionReset();
    }
}

void SessionFogOfWar::RecordActivity() {
    m_lastActivityTime = std::chrono::steady_clock::now();
}

bool SessionFogOfWar::IsSessionExpired() const {
    if (!m_sessionActive) return true;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - m_lastActivityTime).count();
    return elapsed >= m_config.inactivityTimeout;
}

float SessionFogOfWar::GetTimeUntilExpiry() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - m_lastActivityTime).count();
    return std::max(0.0f, m_config.inactivityTimeout - elapsed);
}

void SessionFogOfWar::OnPlayerDisconnect() {
    spdlog::info("Player disconnected");
    m_sessionActive = false;

    if (m_config.resetOnDisconnect) {
        ResetFogOfWar();
    }
}

void SessionFogOfWar::OnPlayerReconnect() {
    spdlog::info("Player reconnected");
    m_sessionActive = true;
    m_lastActivityTime = std::chrono::steady_clock::now();
}

// ============================================================================
// Vision Updates
// ============================================================================

void SessionFogOfWar::UpdateVision(const std::vector<VisionSource>& sources,
                                    const VisionEnvironment& environment,
                                    float deltaTime) {
    if (!m_initialized) return;

    // Check for session timeout
    if (IsSessionExpired()) {
        spdlog::info("Session expired due to inactivity, resetting fog");
        ResetFogOfWar();
    }

    // Update visibility state
    UpdateVisibilityState(sources, environment);

    // Update explored state
    UpdateExploredState();
}

void SessionFogOfWar::UpdateVisionFromSource(const VisionSource& source,
                                              const VisionEnvironment& environment) {
    std::vector<VisionSource> sources = { source };
    UpdateVisibilityState(sources, environment);
    UpdateExploredState();
}

void SessionFogOfWar::UpdateVisibilityState(const std::vector<VisionSource>& sources,
                                             const VisionEnvironment& environment) {
    // Clear current visibility
    std::fill(m_visibilityState.begin(), m_visibilityState.end(), 0);
    m_tilesVisibleCount = 0;

    // Calculate vision for each source
    for (const auto& source : sources) {
        if (!source.active) continue;

        VisionConfig config;
        switch (source.type) {
            case VisionSourceType::Hero: config = VisionConfig::ForHero(); break;
            case VisionSourceType::Worker: config = VisionConfig::ForWorker(); break;
            case VisionSourceType::Building: config = VisionConfig::ForBuilding(); break;
            case VisionSourceType::Scout: config = VisionConfig::ForScout(); break;
            case VisionSourceType::WatchTower: config = VisionConfig::ForWatchTower(); break;
            case VisionSourceType::Flare: config = VisionConfig::ForFlare(); break;
            default: break;
        }

        CalculateVisionForSource(source, environment, m_visibilityState);
    }

    // Count visible tiles
    for (uint8_t v : m_visibilityState) {
        if (v > 0) m_tilesVisibleCount++;
    }

    // Upload visibility state
    glBindTexture(GL_TEXTURE_2D, m_visibilityTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RED, GL_UNSIGNED_BYTE, m_visibilityState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SessionFogOfWar::CalculateVisionForSource(const VisionSource& source,
                                                const VisionEnvironment& environment,
                                                std::vector<uint8_t>& visibilityBuffer) {
    VisionConfig config;
    switch (source.type) {
        case VisionSourceType::Hero: config = VisionConfig::ForHero(); break;
        case VisionSourceType::Worker: config = VisionConfig::ForWorker(); break;
        case VisionSourceType::Building: config = VisionConfig::ForBuilding(); break;
        case VisionSourceType::Scout: config = VisionConfig::ForScout(); break;
        case VisionSourceType::WatchTower: config = VisionConfig::ForWatchTower(); break;
        case VisionSourceType::Flare: config = VisionConfig::ForFlare(); break;
        default: break;
    }

    float effectiveRadius = source.GetEffectiveRadius(
        environment.isDaytime, environment.weatherVisibility, config);

    // Ensure minimum vision
    effectiveRadius = std::max(effectiveRadius, m_config.minimumVisionRadius);

    // Convert to tile coordinates
    glm::ivec2 centerTile = WorldToTile(source.position);
    int tileRadius = static_cast<int>(std::ceil(effectiveRadius / m_tileSize)) + 1;

    // Iterate over tiles in range
    for (int dy = -tileRadius; dy <= tileRadius; dy++) {
        for (int dx = -tileRadius; dx <= tileRadius; dx++) {
            int tx = centerTile.x + dx;
            int ty = centerTile.y + dy;

            // Bounds check
            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            // Distance check
            glm::vec2 tileCenter = TileToWorld(tx, ty);
            float distance = glm::length(tileCenter - source.position);

            if (distance <= effectiveRadius) {
                // Line of sight check
                bool canSee = true;
                if (m_config.enableLineOfSight && source.blockedByTerrain) {
                    canSee = !RaycastOcclusion(source.position, tileCenter);
                }

                if (canSee) {
                    int idx = TileIndex(tx, ty);
                    visibilityBuffer[idx] = 255;
                }
            }
        }
    }
}

void SessionFogOfWar::UpdateExploredState() {
    bool exploredChanged = false;
    int previousExplored = m_tilesExploredCount;

    // Mark visible tiles as explored
    for (int i = 0; i < static_cast<int>(m_visibilityState.size()); i++) {
        if (m_visibilityState[i] > 0) {
            FogState previousState = m_fogState[i];

            if (previousState == FogState::Unknown) {
                m_fogState[i] = FogState::Explored;
                m_tilesExploredCount++;
                exploredChanged = true;

                // Notify callback
                if (m_onTileRevealed) {
                    int x = i % m_mapWidth;
                    int y = i / m_mapWidth;
                    m_onTileRevealed(glm::ivec2(x, y), previousState);
                }
            }

            // Currently visible tiles are marked as Visible
            m_fogState[i] = FogState::Visible;
        } else {
            // Not visible - revert to Explored if was visible
            if (m_fogState[i] == FogState::Visible) {
                m_fogState[i] = FogState::Explored;
            }
        }
    }

    // Upload explored state
    if (exploredChanged) {
        std::vector<uint8_t> exploredData(m_mapWidth * m_mapHeight);
        for (int i = 0; i < static_cast<int>(m_fogState.size()); i++) {
            exploredData[i] = (m_fogState[i] != FogState::Unknown) ? 255 : 0;
        }

        glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                        GL_RED, GL_UNSIGNED_BYTE, exploredData.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        // Notify exploration progress callback
        float newPercent = GetExplorationPercent();
        if (std::abs(newPercent - m_lastExplorationPercent) > 0.1f) {
            m_lastExplorationPercent = newPercent;
            if (m_onAreaExplored) {
                m_onAreaExplored(newPercent);
            }
        }
    }
}

void SessionFogOfWar::SetRadianceCascades(RadianceCascades* cascades) {
    m_radianceCascades = cascades;
}

void SessionFogOfWar::SetOcclusionData(const uint8_t* occlusionData, int width, int height) {
    m_occlusionWidth = width;
    m_occlusionHeight = height;
    m_occlusionData.assign(occlusionData, occlusionData + (width * height));
}

// ============================================================================
// Visibility Queries
// ============================================================================

bool SessionFogOfWar::IsVisible(const glm::ivec2& tile) const {
    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return false;
    }
    return m_fogState[TileIndex(tile.x, tile.y)] == FogState::Visible;
}

bool SessionFogOfWar::IsExplored(const glm::ivec2& tile) const {
    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return false;
    }
    return m_fogState[TileIndex(tile.x, tile.y)] != FogState::Unknown;
}

FogState SessionFogOfWar::GetFogState(const glm::ivec2& tile) const {
    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return FogState::Unknown;
    }
    return m_fogState[TileIndex(tile.x, tile.y)];
}

FogState SessionFogOfWar::GetFogStateAtPosition(const glm::vec2& worldPos) const {
    glm::ivec2 tile = WorldToTile(worldPos);
    return GetFogState(tile);
}

bool SessionFogOfWar::HasLineOfSight(const glm::vec2& from, const glm::vec2& to) const {
    return !RaycastOcclusion(from, to);
}

bool SessionFogOfWar::CanSeeUnit(const glm::vec2& position, bool isHidden,
                                  const std::vector<VisionSource>& checkingSources) const {
    // Check if position is visible by any source
    for (const auto& source : checkingSources) {
        if (!source.active) continue;

        // Hidden units require special detection
        if (isHidden && !source.detectsHidden) continue;

        VisionConfig config;
        switch (source.type) {
            case VisionSourceType::Hero: config = VisionConfig::ForHero(); break;
            case VisionSourceType::Worker: config = VisionConfig::ForWorker(); break;
            case VisionSourceType::Building: config = VisionConfig::ForBuilding(); break;
            case VisionSourceType::Scout: config = VisionConfig::ForScout(); break;
            case VisionSourceType::WatchTower: config = VisionConfig::ForWatchTower(); break;
            case VisionSourceType::Flare: config = VisionConfig::ForFlare(); break;
            default: break;
        }

        // Note: Using default environment for query
        float radius = source.GetEffectiveRadius(true, 1.0f, config);
        float distance = glm::length(position - source.position);

        if (distance <= radius) {
            // Check line of sight
            if (!source.blockedByTerrain || !RaycastOcclusion(source.position, position)) {
                return true;
            }
        }
    }

    return false;
}

bool SessionFogOfWar::RaycastOcclusion(const glm::vec2& from, const glm::vec2& to) const {
    if (m_occlusionData.empty()) return false;

    // DDA line algorithm
    glm::vec2 delta = to - from;
    float distance = glm::length(delta);
    if (distance < 0.001f) return false;

    glm::vec2 dir = delta / distance;
    float stepSize = m_tileSize * 0.5f;  // Half-tile steps
    int numSteps = static_cast<int>(distance / stepSize) + 1;

    for (int i = 1; i < numSteps; i++) {
        glm::vec2 pos = from + dir * (stepSize * static_cast<float>(i));
        glm::ivec2 tile = WorldToTile(pos);

        if (tile.x >= 0 && tile.x < m_occlusionWidth &&
            tile.y >= 0 && tile.y < m_occlusionHeight) {
            int idx = tile.y * m_occlusionWidth + tile.x;
            if (m_occlusionData[idx] > 0) {
                return true;  // Hit obstacle
            }
        }
    }

    return false;
}

// ============================================================================
// Manual Reveal
// ============================================================================

void SessionFogOfWar::RevealTile(const glm::ivec2& tile) {
    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return;
    }

    int idx = TileIndex(tile.x, tile.y);
    FogState previousState = m_fogState[idx];

    if (previousState == FogState::Unknown) {
        m_fogState[idx] = FogState::Explored;
        m_tilesExploredCount++;

        // Update texture
        uint8_t data = 255;
        glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, tile.x, tile.y, 1, 1,
                        GL_RED, GL_UNSIGNED_BYTE, &data);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (m_onTileRevealed) {
            m_onTileRevealed(tile, previousState);
        }
    }
}

void SessionFogOfWar::RevealArea(const glm::vec2& center, float radius) {
    glm::ivec2 centerTile = WorldToTile(center);
    int tileRadius = static_cast<int>(std::ceil(radius / m_tileSize));

    for (int dy = -tileRadius; dy <= tileRadius; dy++) {
        for (int dx = -tileRadius; dx <= tileRadius; dx++) {
            int tx = centerTile.x + dx;
            int ty = centerTile.y + dy;

            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            float dist = glm::length(glm::vec2(dx, dy) * m_tileSize);
            if (dist <= radius) {
                RevealTile(glm::ivec2(tx, ty));
            }
        }
    }
}

void SessionFogOfWar::RevealAll() {
    for (int i = 0; i < static_cast<int>(m_fogState.size()); i++) {
        if (m_fogState[i] == FogState::Unknown) {
            m_fogState[i] = FogState::Explored;
            m_tilesExploredCount++;
        }
    }

    std::vector<uint8_t> allExplored(m_mapWidth * m_mapHeight, 255);
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight,
                    GL_RED, GL_UNSIGNED_BYTE, allExplored.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SessionFogOfWar::HideTile(const glm::ivec2& tile) {
    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return;
    }

    int idx = TileIndex(tile.x, tile.y);
    if (m_fogState[idx] != FogState::Unknown) {
        m_fogState[idx] = FogState::Unknown;
        m_tilesExploredCount--;

        uint8_t data = 0;
        glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, tile.x, tile.y, 1, 1,
                        GL_RED, GL_UNSIGNED_BYTE, &data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

// ============================================================================
// Rendering
// ============================================================================

void SessionFogOfWar::UpdateRendering(float deltaTime) {
    if (!m_initialized) return;

    UpdateFogTexture(deltaTime);
    UpdateCombinedTexture();
}

void SessionFogOfWar::UpdateFogTexture(float deltaTime) {
    glUseProgram(m_fogUpdateShader);

    // Bind textures
    glBindImageTexture(0, m_fogTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16F);
    glBindImageTexture(1, m_exploredTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
    glBindImageTexture(2, m_visibilityTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    if (m_radianceCascades) {
        glBindImageTexture(3, m_radianceCascades->GetRadianceTexture(), 0,
                           GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    }

    // Set uniforms
    glUniform2f(glGetUniformLocation(m_fogUpdateShader, "u_ScreenSize"),
                static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
    glUniform2f(glGetUniformLocation(m_fogUpdateShader, "u_MapSize"),
                static_cast<float>(m_mapWidth), static_cast<float>(m_mapHeight));
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_TileSize"), m_tileSize);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_DeltaTime"), deltaTime);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_TransitionSpeed"), m_config.transitionSpeed);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_UnknownBrightness"), m_config.unknownBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_ExploredBrightness"), m_config.exploredBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_VisibleBrightness"), m_config.visibleBrightness);
    glUniform1i(glGetUniformLocation(m_fogUpdateShader, "u_SmoothEdges"), m_config.smoothEdges);

    // Dispatch
    int groupsX = (m_screenWidth + 7) / 8;
    int groupsY = (m_screenHeight + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

void SessionFogOfWar::UpdateCombinedTexture() {
    if (!m_radianceCascades) return;

    glUseProgram(m_fogCombineShader);

    // Bind textures
    glBindImageTexture(0, m_combinedTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glBindImageTexture(1, m_fogTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(2, m_radianceCascades->GetRadianceTexture(), 0,
                       GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(3, m_exploredTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
    glBindImageTexture(4, m_visibilityTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Set uniforms
    glUniform2f(glGetUniformLocation(m_fogCombineShader, "u_ScreenSize"),
                static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
    glUniform2f(glGetUniformLocation(m_fogCombineShader, "u_MapSize"),
                static_cast<float>(m_mapWidth), static_cast<float>(m_mapHeight));
    glUniform1f(glGetUniformLocation(m_fogCombineShader, "u_TileSize"), m_tileSize);
    glUniform3fv(glGetUniformLocation(m_fogCombineShader, "u_FogColor"), 1,
                 glm::value_ptr(m_config.fogColor));
    glUniform3fv(glGetUniformLocation(m_fogCombineShader, "u_ExploredTint"), 1,
                 glm::value_ptr(m_config.exploredTint));
    glUniform1f(glGetUniformLocation(m_fogCombineShader, "u_ExploredBrightness"),
                m_config.exploredBrightness);

    // Dispatch
    int groupsX = (m_screenWidth + 7) / 8;
    int groupsY = (m_screenHeight + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(0);
}

// ============================================================================
// Statistics
// ============================================================================

float SessionFogOfWar::GetExplorationPercent() const {
    if (m_fogState.empty()) return 0.0f;
    return (static_cast<float>(m_tilesExploredCount) / static_cast<float>(m_fogState.size())) * 100.0f;
}

int SessionFogOfWar::GetTilesExplored() const {
    return m_tilesExploredCount;
}

int SessionFogOfWar::GetTilesVisible() const {
    return m_tilesVisibleCount;
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

int SessionFogOfWar::TileIndex(int x, int y) const {
    return y * m_mapWidth + x;
}

glm::ivec2 SessionFogOfWar::WorldToTile(const glm::vec2& worldPos) const {
    return glm::ivec2(
        static_cast<int>(worldPos.x / m_tileSize),
        static_cast<int>(worldPos.y / m_tileSize)
    );
}

glm::vec2 SessionFogOfWar::TileToWorld(int x, int y) const {
    return glm::vec2(
        (x + 0.5f) * m_tileSize,
        (y + 0.5f) * m_tileSize
    );
}

} // namespace RTS
} // namespace Vehement2
