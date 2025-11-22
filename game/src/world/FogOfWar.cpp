#include "FogOfWar.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace Vehement2 {

// ============================================================================
// Embedded Shaders
// ============================================================================

static const char* FOG_UPDATE_SHADER_SOURCE = R"(
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
uniform float u_UnexploredBrightness;
uniform float u_ExploredBrightness;
uniform float u_VisibleBrightness;
uniform float u_VisibilityThreshold;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= int(u_ScreenSize.x) || pixelCoord.y >= int(u_ScreenSize.y)) {
        return;
    }

    // Convert screen position to tile position
    vec2 screenPos = vec2(pixelCoord) + 0.5;
    ivec2 tileCoord = ivec2(screenPos / u_TileSize);

    // Clamp to valid tile range
    tileCoord = clamp(tileCoord, ivec2(0), ivec2(u_MapSize) - 1);

    // Sample explored and visibility states
    float explored = imageLoad(u_ExploredState, tileCoord).r;
    float visible = imageLoad(u_VisibilityState, tileCoord).r;

    // Sample radiance visibility (alpha channel)
    vec4 radiance = imageLoad(u_RadianceTexture, pixelCoord);
    float radianceVisibility = radiance.a;

    // Determine target brightness
    float targetBrightness;
    if (radianceVisibility > u_VisibilityThreshold || visible > 0.5) {
        targetBrightness = u_VisibleBrightness;
    } else if (explored > 0.5) {
        targetBrightness = u_ExploredBrightness;
    } else {
        targetBrightness = u_UnexploredBrightness;
    }

    // Get current brightness
    float currentBrightness = imageLoad(u_FogOutput, pixelCoord).r;

    // Smooth transition
    float newBrightness = mix(currentBrightness, targetBrightness,
                              1.0 - exp(-u_TransitionSpeed * u_DeltaTime));

    imageStore(u_FogOutput, pixelCoord, vec4(newBrightness, 0.0, 0.0, 1.0));
}
)";

static const char* FOG_COMBINE_SHADER_SOURCE = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output combined texture
layout(rgba16f, binding = 0) uniform writeonly image2D u_CombinedOutput;

// Input textures
layout(r16f, binding = 1) uniform readonly image2D u_FogTexture;
layout(rgba16f, binding = 2) uniform readonly image2D u_RadianceTexture;
layout(r8, binding = 3) uniform readonly image2D u_ExploredState;

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

    // Convert screen position to tile position
    vec2 screenPos = vec2(pixelCoord) + 0.5;
    ivec2 tileCoord = ivec2(screenPos / u_TileSize);
    tileCoord = clamp(tileCoord, ivec2(0), ivec2(u_MapSize) - 1);

    float explored = imageLoad(u_ExploredState, tileCoord).r;

    // Combine fog with lighting
    vec3 finalColor;

    if (fogFactor < 0.01) {
        // Unexplored - completely black
        finalColor = u_FogColor;
    } else if (fogFactor < u_ExploredBrightness + 0.1 && explored > 0.5) {
        // Explored but not visible - tinted and dimmed
        finalColor = radiance.rgb * fogFactor * u_ExploredTint;
    } else {
        // Visible - full radiance
        finalColor = radiance.rgb * fogFactor;
    }

    // Output with visibility in alpha
    imageStore(u_CombinedOutput, pixelCoord, vec4(finalColor, fogFactor));
}
)";

static const char* FOG_APPLY_VERTEX_SOURCE = R"(
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

static const char* FOG_APPLY_FRAGMENT_SOURCE = R"(
#version 460 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_FogTexture;
uniform sampler2D u_RadianceTexture;
uniform vec3 u_FogColor;
uniform float u_ExploredBrightness;
uniform bool u_UseRadiance;

void main() {
    vec4 sceneColor = texture(u_SceneTexture, v_TexCoord);
    float fogFactor = texture(u_FogTexture, v_TexCoord).r;

    vec3 finalColor;

    if (u_UseRadiance) {
        vec4 radiance = texture(u_RadianceTexture, v_TexCoord);

        // Apply radiance-based lighting
        vec3 lit = sceneColor.rgb * radiance.rgb;

        // Blend with fog
        finalColor = mix(u_FogColor, lit, fogFactor);
    } else {
        // Simple fog - just multiply by fog factor
        finalColor = mix(u_FogColor, sceneColor.rgb, fogFactor);
    }

    FragColor = vec4(finalColor, sceneColor.a);
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
        spdlog::error("Compute shader compilation failed:\n{}", infoLog);
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
        spdlog::error("Compute shader linking failed:\n{}", infoLog);
        glDeleteProgram(program);
        glDeleteShader(shader);
        return 0;
    }

    glDeleteShader(shader);
    return program;
}

static uint32_t CompileShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
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
// FogOfWar Implementation
// ============================================================================

FogOfWar::FogOfWar() = default;

FogOfWar::~FogOfWar() {
    Shutdown();
}

FogOfWar::FogOfWar(FogOfWar&& other) noexcept
    : m_mapWidth(other.m_mapWidth)
    , m_mapHeight(other.m_mapHeight)
    , m_tileSize(other.m_tileSize)
    , m_screenWidth(other.m_screenWidth)
    , m_screenHeight(other.m_screenHeight)
    , m_config(other.m_config)
    , m_initialized(other.m_initialized)
    , m_exploredState(std::move(other.m_exploredState))
    , m_visibilityState(std::move(other.m_visibilityState))
    , m_fogBrightness(std::move(other.m_fogBrightness))
    , m_fogTexture(other.m_fogTexture)
    , m_combinedTexture(other.m_combinedTexture)
    , m_exploredTexture(other.m_exploredTexture)
    , m_fogUpdateShader(other.m_fogUpdateShader)
    , m_fogCombineShader(other.m_fogCombineShader)
    , m_radianceCascades(other.m_radianceCascades)
    , m_lastPlayerPos(other.m_lastPlayerPos)
{
    other.m_initialized = false;
    other.m_fogTexture = 0;
    other.m_combinedTexture = 0;
    other.m_exploredTexture = 0;
    other.m_fogUpdateShader = 0;
    other.m_fogCombineShader = 0;
}

FogOfWar& FogOfWar::operator=(FogOfWar&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_mapWidth = other.m_mapWidth;
        m_mapHeight = other.m_mapHeight;
        m_tileSize = other.m_tileSize;
        m_screenWidth = other.m_screenWidth;
        m_screenHeight = other.m_screenHeight;
        m_config = other.m_config;
        m_initialized = other.m_initialized;
        m_exploredState = std::move(other.m_exploredState);
        m_visibilityState = std::move(other.m_visibilityState);
        m_fogBrightness = std::move(other.m_fogBrightness);
        m_fogTexture = other.m_fogTexture;
        m_combinedTexture = other.m_combinedTexture;
        m_exploredTexture = other.m_exploredTexture;
        m_fogUpdateShader = other.m_fogUpdateShader;
        m_fogCombineShader = other.m_fogCombineShader;
        m_radianceCascades = other.m_radianceCascades;
        m_lastPlayerPos = other.m_lastPlayerPos;

        other.m_initialized = false;
        other.m_fogTexture = 0;
        other.m_combinedTexture = 0;
        other.m_exploredTexture = 0;
        other.m_fogUpdateShader = 0;
        other.m_fogCombineShader = 0;
    }
    return *this;
}

bool FogOfWar::Initialize(int width, int height, float tileSize, int screenWidth, int screenHeight) {
    if (m_initialized) {
        spdlog::warn("FogOfWar already initialized, shutting down first");
        Shutdown();
    }

    m_mapWidth = width;
    m_mapHeight = height;
    m_tileSize = tileSize;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    spdlog::info("Initializing FogOfWar: {}x{} tiles, {}x{} screen", width, height, screenWidth, screenHeight);

    // Initialize state arrays
    int tileCount = width * height;
    m_exploredState.resize(tileCount, 0);
    m_visibilityState.resize(tileCount, 0);
    m_fogBrightness.resize(tileCount, 0.0f);

    if (!CreateShaders()) {
        spdlog::error("Failed to create fog of war shaders");
        return false;
    }

    if (!CreateTextures()) {
        spdlog::error("Failed to create fog of war textures");
        DestroyResources();
        return false;
    }

    m_initialized = true;
    spdlog::info("FogOfWar initialized successfully");
    return true;
}

void FogOfWar::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down FogOfWar");
    DestroyResources();
    m_initialized = false;
}

void FogOfWar::Resize(int screenWidth, int screenHeight) {
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

bool FogOfWar::CreateShaders() {
    m_fogUpdateShader = CompileComputeShader(FOG_UPDATE_SHADER_SOURCE);
    if (m_fogUpdateShader == 0) {
        spdlog::error("Failed to compile fog update shader");
        return false;
    }

    m_fogCombineShader = CompileComputeShader(FOG_COMBINE_SHADER_SOURCE);
    if (m_fogCombineShader == 0) {
        spdlog::error("Failed to compile fog combine shader");
        return false;
    }

    return true;
}

bool FogOfWar::CreateTextures() {
    // Fog texture (single channel, screen resolution)
    glGenTextures(1, &m_fogTexture);
    glBindTexture(GL_TEXTURE_2D, m_fogTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_screenWidth, m_screenHeight, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Combined texture (RGBA, screen resolution)
    glGenTextures(1, &m_combinedTexture);
    glBindTexture(GL_TEXTURE_2D, m_combinedTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Explored state texture (single channel, map resolution)
    glGenTextures(1, &m_exploredTexture);
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_mapWidth, m_mapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void FogOfWar::DestroyResources() {
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

    if (m_fogUpdateShader) {
        glDeleteProgram(m_fogUpdateShader);
        m_fogUpdateShader = 0;
    }

    if (m_fogCombineShader) {
        glDeleteProgram(m_fogCombineShader);
        m_fogCombineShader = 0;
    }

    m_exploredState.clear();
    m_visibilityState.clear();
    m_fogBrightness.clear();
}

void FogOfWar::SetRadianceCascades(RadianceCascades* cascades) {
    m_radianceCascades = cascades;
}

void FogOfWar::UpdateVisibility(glm::vec2 playerPos, float deltaTime) {
    if (!m_initialized) return;

    m_lastPlayerPos = playerPos;

    // Update explored state based on current visibility
    UpdateExploredState(playerPos);

    // Update fog texture
    UpdateFogTexture(deltaTime);

    // Update combined texture
    UpdateCombinedTexture();
}

void FogOfWar::ForceUpdate() {
    if (!m_initialized) return;

    // Force update all tiles
    for (int y = 0; y < m_mapHeight; y++) {
        for (int x = 0; x < m_mapWidth; x++) {
            int idx = TileIndex(x, y);
            glm::vec2 tileCenter = TileToWorld(x, y);

            bool visible = false;
            if (m_radianceCascades) {
                float vis = m_radianceCascades->GetVisibility(tileCenter);
                visible = vis > m_config.visibilityThreshold;
            }

            m_visibilityState[idx] = visible ? 255 : 0;

            if (visible && m_config.revealOnExplore) {
                m_exploredState[idx] = 1;
            }
        }
    }

    // Upload explored state
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar::UpdateExploredState(glm::vec2 playerPos) {
    if (!m_radianceCascades) return;

    // Only check tiles near the player for performance
    glm::ivec2 playerTile = WorldToTile(playerPos);
    int checkRadius = static_cast<int>(m_radianceCascades->GetConfig().maxRayDistance / m_tileSize) + 1;

    bool stateChanged = false;

    for (int dy = -checkRadius; dy <= checkRadius; dy++) {
        for (int dx = -checkRadius; dx <= checkRadius; dx++) {
            int tx = playerTile.x + dx;
            int ty = playerTile.y + dy;

            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            int idx = TileIndex(tx, ty);
            glm::vec2 tileCenter = TileToWorld(tx, ty);

            float visibility = m_radianceCascades->GetVisibility(tileCenter);
            bool visible = visibility > m_config.visibilityThreshold;

            m_visibilityState[idx] = visible ? 255 : 0;

            if (visible && m_config.revealOnExplore && m_exploredState[idx] == 0) {
                m_exploredState[idx] = 1;
                stateChanged = true;
            }
        }
    }

    // Upload updated explored state
    if (stateChanged) {
        glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void FogOfWar::UpdateFogTexture(float deltaTime) {
    if (!m_radianceCascades) return;

    glUseProgram(m_fogUpdateShader);

    // Bind output
    glBindImageTexture(0, m_fogTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16F);

    // Bind inputs
    glBindImageTexture(1, m_exploredTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Create visibility texture on the fly
    uint32_t visTexture;
    glGenTextures(1, &visTexture);
    glBindTexture(GL_TEXTURE_2D, visTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_mapWidth, m_mapHeight, 0, GL_RED, GL_UNSIGNED_BYTE, m_visibilityState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindImageTexture(2, visTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Bind radiance texture
    glBindImageTexture(3, m_radianceCascades->GetRadianceTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Set uniforms
    glUniform2f(glGetUniformLocation(m_fogUpdateShader, "u_ScreenSize"),
                static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
    glUniform2f(glGetUniformLocation(m_fogUpdateShader, "u_MapSize"),
                static_cast<float>(m_mapWidth), static_cast<float>(m_mapHeight));
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_TileSize"), m_tileSize);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_DeltaTime"), deltaTime);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_TransitionSpeed"), m_config.transitionSpeed);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_UnexploredBrightness"), m_config.unexploredBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_ExploredBrightness"), m_config.exploredBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_VisibleBrightness"), m_config.visibleBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdateShader, "u_VisibilityThreshold"), m_config.visibilityThreshold);

    // Dispatch
    int groupsX = (m_screenWidth + 7) / 8;
    int groupsY = (m_screenHeight + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Cleanup temp texture
    glDeleteTextures(1, &visTexture);

    glUseProgram(0);
}

void FogOfWar::UpdateCombinedTexture() {
    if (!m_radianceCascades) return;

    glUseProgram(m_fogCombineShader);

    // Bind output
    glBindImageTexture(0, m_combinedTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Bind inputs
    glBindImageTexture(1, m_fogTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R16F);
    glBindImageTexture(2, m_radianceCascades->GetRadianceTexture(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(3, m_exploredTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Set uniforms
    glUniform2f(glGetUniformLocation(m_fogCombineShader, "u_ScreenSize"),
                static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
    glUniform2f(glGetUniformLocation(m_fogCombineShader, "u_MapSize"),
                static_cast<float>(m_mapWidth), static_cast<float>(m_mapHeight));
    glUniform1f(glGetUniformLocation(m_fogCombineShader, "u_TileSize"), m_tileSize);
    glUniform3fv(glGetUniformLocation(m_fogCombineShader, "u_FogColor"), 1, glm::value_ptr(m_config.fogColor));
    glUniform3fv(glGetUniformLocation(m_fogCombineShader, "u_ExploredTint"), 1, glm::value_ptr(m_config.exploredTint));
    glUniform1f(glGetUniformLocation(m_fogCombineShader, "u_ExploredBrightness"), m_config.exploredBrightness);

    // Dispatch
    int groupsX = (m_screenWidth + 7) / 8;
    int groupsY = (m_screenHeight + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUseProgram(0);
}

FogState FogOfWar::GetFogState(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= m_mapWidth || tileY < 0 || tileY >= m_mapHeight) {
        return FogState::Unexplored;
    }

    int idx = TileIndex(tileX, tileY);

    if (m_visibilityState[idx] > 0) {
        return FogState::Visible;
    } else if (m_exploredState[idx] > 0) {
        return FogState::Explored;
    }
    return FogState::Unexplored;
}

FogState FogOfWar::GetFogStateAtPosition(glm::vec2 worldPos) const {
    glm::ivec2 tile = WorldToTile(worldPos);
    return GetFogState(tile.x, tile.y);
}

float FogOfWar::GetFogBrightness(glm::vec2 worldPos) const {
    glm::ivec2 tile = WorldToTile(worldPos);

    if (tile.x < 0 || tile.x >= m_mapWidth || tile.y < 0 || tile.y >= m_mapHeight) {
        return m_config.unexploredBrightness;
    }

    int idx = TileIndex(tile.x, tile.y);
    return m_fogBrightness[idx];
}

bool FogOfWar::IsExplored(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= m_mapWidth || tileY < 0 || tileY >= m_mapHeight) {
        return false;
    }
    return m_exploredState[TileIndex(tileX, tileY)] > 0;
}

bool FogOfWar::IsVisible(int tileX, int tileY) const {
    if (tileX < 0 || tileX >= m_mapWidth || tileY < 0 || tileY >= m_mapHeight) {
        return false;
    }
    return m_visibilityState[TileIndex(tileX, tileY)] > 0;
}

void FogOfWar::RevealTile(int tileX, int tileY) {
    if (tileX < 0 || tileX >= m_mapWidth || tileY < 0 || tileY >= m_mapHeight) {
        return;
    }

    m_exploredState[TileIndex(tileX, tileY)] = 1;

    // Update texture
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    uint8_t data = 1;
    glTexSubImage2D(GL_TEXTURE_2D, 0, tileX, tileY, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar::RevealArea(glm::vec2 center, float radius) {
    glm::ivec2 centerTile = WorldToTile(center);
    int tileRadius = static_cast<int>(std::ceil(radius / m_tileSize));

    for (int dy = -tileRadius; dy <= tileRadius; dy++) {
        for (int dx = -tileRadius; dx <= tileRadius; dx++) {
            int tx = centerTile.x + dx;
            int ty = centerTile.y + dy;

            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            float dist = glm::length(glm::vec2(dx, dy) * m_tileSize);
            if (dist <= radius) {
                m_exploredState[TileIndex(tx, ty)] = 1;
            }
        }
    }

    // Upload entire state
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar::RevealAll() {
    std::fill(m_exploredState.begin(), m_exploredState.end(), 1);

    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar::ResetFog() {
    std::fill(m_exploredState.begin(), m_exploredState.end(), 0);
    std::fill(m_visibilityState.begin(), m_visibilityState.end(), 0);
    std::fill(m_fogBrightness.begin(), m_fogBrightness.end(), 0.0f);

    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar::HideTile(int tileX, int tileY) {
    if (tileX < 0 || tileX >= m_mapWidth || tileY < 0 || tileY >= m_mapHeight) {
        return;
    }

    m_exploredState[TileIndex(tileX, tileY)] = 0;

    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    uint8_t data = 0;
    glTexSubImage2D(GL_TEXTURE_2D, 0, tileX, tileY, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool FogOfWar::SaveExploredState(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for saving fog state: {}", filepath);
        return false;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&m_mapWidth), sizeof(m_mapWidth));
    file.write(reinterpret_cast<const char*>(&m_mapHeight), sizeof(m_mapHeight));

    // Write explored state
    file.write(reinterpret_cast<const char*>(m_exploredState.data()), m_exploredState.size());

    spdlog::info("Saved fog state to: {}", filepath);
    return true;
}

bool FogOfWar::LoadExploredState(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for loading fog state: {}", filepath);
        return false;
    }

    // Read header
    int width, height;
    file.read(reinterpret_cast<char*>(&width), sizeof(width));
    file.read(reinterpret_cast<char*>(&height), sizeof(height));

    // Validate dimensions
    if (width != m_mapWidth || height != m_mapHeight) {
        spdlog::error("Fog state dimensions mismatch: expected {}x{}, got {}x{}",
                      m_mapWidth, m_mapHeight, width, height);
        return false;
    }

    // Read explored state
    file.read(reinterpret_cast<char*>(m_exploredState.data()), m_exploredState.size());

    // Upload to GPU
    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::info("Loaded fog state from: {}", filepath);
    return true;
}

void FogOfWar::SetExploredData(const std::vector<uint8_t>& data) {
    if (data.size() != m_exploredState.size()) {
        spdlog::error("Explored data size mismatch");
        return;
    }

    m_exploredState = data;

    glBindTexture(GL_TEXTURE_2D, m_exploredTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_mapWidth, m_mapHeight, GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

float FogOfWar::GetExplorationProgress() const {
    if (m_exploredState.empty()) return 0.0f;

    int explored = 0;
    for (uint8_t state : m_exploredState) {
        if (state > 0) explored++;
    }

    return (static_cast<float>(explored) / static_cast<float>(m_exploredState.size())) * 100.0f;
}

void FogOfWar::SetConfig(const Config& config) {
    m_config = config;
}

int FogOfWar::TileIndex(int x, int y) const {
    return y * m_mapWidth + x;
}

glm::ivec2 FogOfWar::WorldToTile(glm::vec2 worldPos) const {
    return glm::ivec2(
        static_cast<int>(worldPos.x / m_tileSize),
        static_cast<int>(worldPos.y / m_tileSize)
    );
}

glm::vec2 FogOfWar::TileToWorld(int x, int y) const {
    return glm::vec2(
        (x + 0.5f) * m_tileSize,
        (y + 0.5f) * m_tileSize
    );
}

// ============================================================================
// FogOfWarRenderer Implementation
// ============================================================================

FogOfWarRenderer::FogOfWarRenderer() = default;

FogOfWarRenderer::~FogOfWarRenderer() {
    Shutdown();
}

bool FogOfWarRenderer::Initialize() {
    if (m_initialized) return true;

    if (!CreateShaders()) {
        return false;
    }

    // Create fullscreen quad
    float quadVertices[] = {
        // Position    // TexCoord
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
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

    m_initialized = true;
    return true;
}

void FogOfWarRenderer::Shutdown() {
    DestroyResources();
    m_initialized = false;
}

bool FogOfWarRenderer::CreateShaders() {
    m_fogShader = CompileShaderProgram(FOG_APPLY_VERTEX_SOURCE, FOG_APPLY_FRAGMENT_SOURCE);
    return m_fogShader != 0;
}

void FogOfWarRenderer::DestroyResources() {
    if (m_fogShader) {
        glDeleteProgram(m_fogShader);
        m_fogShader = 0;
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

void FogOfWarRenderer::ApplyFog(const FogOfWar& fogOfWar, uint32_t sceneTexture, uint32_t outputFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);

    glUseProgram(m_fogShader);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(glGetUniformLocation(m_fogShader, "u_SceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fogOfWar.GetFogTexture());
    glUniform1i(glGetUniformLocation(m_fogShader, "u_FogTexture"), 1);

    // Set uniforms
    glUniform3fv(glGetUniformLocation(m_fogShader, "u_FogColor"), 1,
                 glm::value_ptr(fogOfWar.GetConfig().fogColor));
    glUniform1f(glGetUniformLocation(m_fogShader, "u_ExploredBrightness"),
                fogOfWar.GetConfig().exploredBrightness);
    glUniform1i(glGetUniformLocation(m_fogShader, "u_UseRadiance"), 0);

    // Draw fullscreen quad
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}

void FogOfWarRenderer::ApplyFogWithLighting(const FogOfWar& fogOfWar, const RadianceCascades& cascades,
                                             uint32_t sceneTexture, uint32_t outputFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);

    glUseProgram(m_fogShader);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(glGetUniformLocation(m_fogShader, "u_SceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fogOfWar.GetFogTexture());
    glUniform1i(glGetUniformLocation(m_fogShader, "u_FogTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, cascades.GetRadianceTexture());
    glUniform1i(glGetUniformLocation(m_fogShader, "u_RadianceTexture"), 2);

    // Set uniforms
    glUniform3fv(glGetUniformLocation(m_fogShader, "u_FogColor"), 1,
                 glm::value_ptr(fogOfWar.GetConfig().fogColor));
    glUniform1f(glGetUniformLocation(m_fogShader, "u_ExploredBrightness"),
                fogOfWar.GetConfig().exploredBrightness);
    glUniform1i(glGetUniformLocation(m_fogShader, "u_UseRadiance"), 1);

    // Draw fullscreen quad
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}

} // namespace Vehement2
