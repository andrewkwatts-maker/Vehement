#include "FogOfWar3D.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cmath>
#include <algorithm>

namespace Vehement2 {

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
// Embedded Shaders
// ============================================================================

static const char* FOG_UPDATE_3D_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;

// Output fog volume
layout(r16f, binding = 0) uniform image3D u_FogVolume;

// Input volumes
layout(r8, binding = 1) uniform readonly image3D u_ExploredState;
layout(r8, binding = 2) uniform readonly image3D u_VisibilityState;
layout(rgba16f, binding = 3) uniform readonly image3D u_RadianceVolume;

uniform ivec3 u_VolumeSize;
uniform float u_DeltaTime;
uniform float u_TransitionSpeed;
uniform float u_UnexploredBrightness;
uniform float u_ExploredBrightness;
uniform float u_VisibleBrightness;
uniform float u_VisibilityThreshold;

uniform int u_CurrentFloor;
uniform int u_ViewMode;  // 0=CurrentFloor, 1=CutawayAbove, 2=XRay, 3=AllFloors
uniform float u_AboveFloorOpacity;
uniform float u_BelowFloorOpacity;

void main() {
    ivec3 voxelCoord = ivec3(gl_GlobalInvocationID.xyz);

    if (voxelCoord.x >= u_VolumeSize.x || voxelCoord.y >= u_VolumeSize.y ||
        voxelCoord.z >= u_VolumeSize.z) {
        return;
    }

    // Sample states
    float explored = imageLoad(u_ExploredState, voxelCoord).r;
    float visible = imageLoad(u_VisibilityState, voxelCoord).r;
    vec4 radiance = imageLoad(u_RadianceVolume, voxelCoord);
    float radianceVisibility = radiance.a;

    // Determine base brightness based on exploration/visibility
    float baseBrightness;
    if (radianceVisibility > u_VisibilityThreshold || visible > 0.5) {
        baseBrightness = u_VisibleBrightness;
    } else if (explored > 0.5) {
        baseBrightness = u_ExploredBrightness;
    } else {
        baseBrightness = u_UnexploredBrightness;
    }

    // Apply floor-based visibility
    float floorModifier = 1.0;
    int floorDiff = voxelCoord.z - u_CurrentFloor;

    if (u_ViewMode == 0) {
        // CurrentFloor only
        if (voxelCoord.z != u_CurrentFloor) {
            floorModifier = 0.0;
        }
    } else if (u_ViewMode == 1) {
        // CutawayAbove
        if (floorDiff > 0) {
            floorModifier = u_AboveFloorOpacity;
        } else if (floorDiff < 0) {
            floorModifier = u_BelowFloorOpacity;
        }
    } else if (u_ViewMode == 2) {
        // XRay - floors within range visible
        float falloff = 1.0 / (1.0 + abs(float(floorDiff)) * 0.5);
        floorModifier = falloff;
    }
    // ViewMode 3 (AllFloors) = no modification

    float targetBrightness = baseBrightness * floorModifier;

    // Get current brightness
    float currentBrightness = imageLoad(u_FogVolume, voxelCoord).r;

    // Smooth transition
    float newBrightness = mix(currentBrightness, targetBrightness,
                              1.0 - exp(-u_TransitionSpeed * u_DeltaTime));

    imageStore(u_FogVolume, voxelCoord, vec4(newBrightness, 0.0, 0.0, 1.0));
}
)";

static const char* FOG_COMBINE_3D_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 4) in;

layout(rgba16f, binding = 0) uniform writeonly image3D u_CombinedOutput;
layout(r16f, binding = 1) uniform readonly image3D u_FogVolume;
layout(rgba16f, binding = 2) uniform readonly image3D u_RadianceVolume;
layout(r8, binding = 3) uniform readonly image3D u_ExploredState;

uniform ivec3 u_VolumeSize;
uniform vec3 u_FogColor;
uniform vec3 u_ExploredTint;
uniform float u_ExploredBrightness;

void main() {
    ivec3 voxelCoord = ivec3(gl_GlobalInvocationID.xyz);

    if (voxelCoord.x >= u_VolumeSize.x || voxelCoord.y >= u_VolumeSize.y ||
        voxelCoord.z >= u_VolumeSize.z) {
        return;
    }

    float fogFactor = imageLoad(u_FogVolume, voxelCoord).r;
    vec4 radiance = imageLoad(u_RadianceVolume, voxelCoord);
    float explored = imageLoad(u_ExploredState, voxelCoord).r;

    vec3 finalColor;
    if (fogFactor < 0.01) {
        finalColor = u_FogColor;
    } else if (fogFactor < u_ExploredBrightness + 0.1 && explored > 0.5) {
        finalColor = radiance.rgb * fogFactor * u_ExploredTint;
    } else {
        finalColor = radiance.rgb * fogFactor;
    }

    imageStore(u_CombinedOutput, voxelCoord, vec4(finalColor, fogFactor));
}
)";

static const char* EXTRACT_FLOOR_FOG_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(r16f, binding = 0) uniform writeonly image2D u_FloorOutput;
layout(r16f, binding = 1) uniform readonly image3D u_FogVolume;

uniform int u_FloorLevel;
uniform ivec2 u_OutputSize;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= u_OutputSize.x || pixelCoord.y >= u_OutputSize.y) {
        return;
    }

    ivec3 volumeCoord = ivec3(pixelCoord, u_FloorLevel);
    float fogValue = imageLoad(u_FogVolume, volumeCoord).r;

    imageStore(u_FloorOutput, pixelCoord, vec4(fogValue, 0.0, 0.0, 1.0));
}
)";

static const char* FLOOR_FOG_VERTEX_SHADER = R"(
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord = a_TexCoord;
}
)";

static const char* FLOOR_FOG_FRAGMENT_SHADER = R"(
#version 460 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_FogTexture;
uniform sampler2D u_RadianceTexture;
uniform vec3 u_FogColor;
uniform float u_FloorOpacity;
uniform bool u_UseRadiance;

void main() {
    vec4 sceneColor = texture(u_SceneTexture, v_TexCoord);
    float fogFactor = texture(u_FogTexture, v_TexCoord).r;

    vec3 finalColor;
    if (u_UseRadiance) {
        vec4 radiance = texture(u_RadianceTexture, v_TexCoord);
        vec3 lit = sceneColor.rgb * radiance.rgb;
        finalColor = mix(u_FogColor, lit, fogFactor);
    } else {
        finalColor = mix(u_FogColor, sceneColor.rgb, fogFactor);
    }

    FragColor = vec4(finalColor, sceneColor.a * u_FloorOpacity);
}
)";

// ============================================================================
// FogOfWar3D Implementation
// ============================================================================

FogOfWar3D::FogOfWar3D() = default;

FogOfWar3D::~FogOfWar3D() {
    Shutdown();
}

FogOfWar3D::FogOfWar3D(FogOfWar3D&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_depth(other.m_depth)
    , m_tileSizeXY(other.m_tileSizeXY)
    , m_tileSizeZ(other.m_tileSizeZ)
    , m_config(other.m_config)
    , m_initialized(other.m_initialized)
    , m_currentFloor(other.m_currentFloor)
    , m_previousFloor(other.m_previousFloor)
    , m_floorTransition(other.m_floorTransition)
    , m_viewMode(other.m_viewMode)
    , m_exploredState(std::move(other.m_exploredState))
    , m_visibilityState(std::move(other.m_visibilityState))
    , m_fogBrightness(std::move(other.m_fogBrightness))
    , m_fogVolume(other.m_fogVolume)
    , m_floorFogTextures(std::move(other.m_floorFogTextures))
    , m_floorCombinedTextures(std::move(other.m_floorCombinedTextures))
    , m_floorExploredTextures(std::move(other.m_floorExploredTextures))
    , m_fogUpdate3DShader(other.m_fogUpdate3DShader)
    , m_fogCombine3DShader(other.m_fogCombine3DShader)
    , m_extractFloorFogShader(other.m_extractFloorFogShader)
    , m_radianceCascades(other.m_radianceCascades)
    , m_lastPlayerPos(other.m_lastPlayerPos)
{
    other.m_initialized = false;
    other.m_fogVolume = 0;
    other.m_fogUpdate3DShader = 0;
    other.m_fogCombine3DShader = 0;
    other.m_extractFloorFogShader = 0;
}

FogOfWar3D& FogOfWar3D::operator=(FogOfWar3D&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_tileSizeXY = other.m_tileSizeXY;
        m_tileSizeZ = other.m_tileSizeZ;
        m_config = other.m_config;
        m_initialized = other.m_initialized;
        m_currentFloor = other.m_currentFloor;
        m_previousFloor = other.m_previousFloor;
        m_floorTransition = other.m_floorTransition;
        m_viewMode = other.m_viewMode;
        m_exploredState = std::move(other.m_exploredState);
        m_visibilityState = std::move(other.m_visibilityState);
        m_fogBrightness = std::move(other.m_fogBrightness);
        m_fogVolume = other.m_fogVolume;
        m_floorFogTextures = std::move(other.m_floorFogTextures);
        m_floorCombinedTextures = std::move(other.m_floorCombinedTextures);
        m_floorExploredTextures = std::move(other.m_floorExploredTextures);
        m_fogUpdate3DShader = other.m_fogUpdate3DShader;
        m_fogCombine3DShader = other.m_fogCombine3DShader;
        m_extractFloorFogShader = other.m_extractFloorFogShader;
        m_radianceCascades = other.m_radianceCascades;
        m_lastPlayerPos = other.m_lastPlayerPos;

        other.m_initialized = false;
        other.m_fogVolume = 0;
        other.m_fogUpdate3DShader = 0;
        other.m_fogCombine3DShader = 0;
        other.m_extractFloorFogShader = 0;
    }
    return *this;
}

bool FogOfWar3D::Initialize(int width, int height, int depth, float tileSizeXY, float tileSizeZ) {
    if (m_initialized) {
        spdlog::warn("FogOfWar3D already initialized, shutting down first");
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_tileSizeXY = tileSizeXY;
    m_tileSizeZ = tileSizeZ;

    spdlog::info("Initializing FogOfWar3D: {}x{}x{} voxels", width, height, depth);

    // Initialize state arrays
    int voxelCount = width * height * depth;
    m_exploredState.resize(voxelCount, 0);
    m_visibilityState.resize(voxelCount, 0);
    m_fogBrightness.resize(voxelCount, 0.0f);

    if (!CreateShaders()) {
        spdlog::error("Failed to create 3D fog of war shaders");
        return false;
    }

    if (!CreateTextures()) {
        spdlog::error("Failed to create 3D fog of war textures");
        DestroyResources();
        return false;
    }

    m_initialized = true;
    spdlog::info("FogOfWar3D initialized successfully");
    return true;
}

void FogOfWar3D::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down FogOfWar3D");
    DestroyResources();
    m_initialized = false;
}

void FogOfWar3D::Resize(int width, int height, int depth) {
    if (width == m_width && height == m_height && depth == m_depth) return;

    m_width = width;
    m_height = height;
    m_depth = depth;

    int voxelCount = width * height * depth;
    m_exploredState.resize(voxelCount, 0);
    m_visibilityState.resize(voxelCount, 0);
    m_fogBrightness.resize(voxelCount, 0.0f);

    // Recreate textures
    DestroyResources();
    CreateTextures();
}

bool FogOfWar3D::CreateShaders() {
    m_fogUpdate3DShader = CompileComputeShader(FOG_UPDATE_3D_SHADER);
    if (m_fogUpdate3DShader == 0) {
        spdlog::error("Failed to compile 3D fog update shader");
        return false;
    }

    m_fogCombine3DShader = CompileComputeShader(FOG_COMBINE_3D_SHADER);
    if (m_fogCombine3DShader == 0) {
        spdlog::error("Failed to compile 3D fog combine shader");
        return false;
    }

    m_extractFloorFogShader = CompileComputeShader(EXTRACT_FLOOR_FOG_SHADER);
    if (m_extractFloorFogShader == 0) {
        spdlog::error("Failed to compile floor fog extraction shader");
        return false;
    }

    return true;
}

bool FogOfWar3D::CreateTextures() {
    // Create 3D fog volume
    glGenTextures(1, &m_fogVolume);
    glBindTexture(GL_TEXTURE_3D, m_fogVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R16F, m_width, m_height, m_depth, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Create per-floor textures
    m_floorFogTextures.resize(m_depth);
    m_floorCombinedTextures.resize(m_depth);
    m_floorExploredTextures.resize(m_depth);

    glGenTextures(m_depth, m_floorFogTextures.data());
    glGenTextures(m_depth, m_floorCombinedTextures.data());
    glGenTextures(m_depth, m_floorExploredTextures.data());

    for (int z = 0; z < m_depth; z++) {
        // Fog texture
        glBindTexture(GL_TEXTURE_2D, m_floorFogTextures[z]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Combined texture
        glBindTexture(GL_TEXTURE_2D, m_floorCombinedTextures[z]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Explored texture
        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);

    return true;
}

void FogOfWar3D::DestroyResources() {
    if (m_fogVolume) {
        glDeleteTextures(1, &m_fogVolume);
        m_fogVolume = 0;
    }

    if (!m_floorFogTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_floorFogTextures.size()), m_floorFogTextures.data());
        m_floorFogTextures.clear();
    }

    if (!m_floorCombinedTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_floorCombinedTextures.size()), m_floorCombinedTextures.data());
        m_floorCombinedTextures.clear();
    }

    if (!m_floorExploredTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_floorExploredTextures.size()), m_floorExploredTextures.data());
        m_floorExploredTextures.clear();
    }

    if (m_fogUpdate3DShader) {
        glDeleteProgram(m_fogUpdate3DShader);
        m_fogUpdate3DShader = 0;
    }

    if (m_fogCombine3DShader) {
        glDeleteProgram(m_fogCombine3DShader);
        m_fogCombine3DShader = 0;
    }

    if (m_extractFloorFogShader) {
        glDeleteProgram(m_extractFloorFogShader);
        m_extractFloorFogShader = 0;
    }

    m_exploredState.clear();
    m_visibilityState.clear();
    m_fogBrightness.clear();
}

void FogOfWar3D::SetRadianceCascades(RadianceCascades3D* cascades) {
    m_radianceCascades = cascades;
}

void FogOfWar3D::SetCurrentFloor(int floor) {
    if (floor == m_currentFloor) return;

    m_previousFloor = m_currentFloor;
    m_currentFloor = std::clamp(floor, 0, m_depth - 1);
    m_floorTransition = 0.0f;  // Start transition
}

void FogOfWar3D::SetViewMode(ViewMode3D mode) {
    m_viewMode = mode;
}

bool FogOfWar3D::ShouldRenderFloor(int floor) const {
    if (floor < 0 || floor >= m_depth) return false;

    switch (m_viewMode) {
        case ViewMode3D::CurrentFloor:
            return floor == m_currentFloor;

        case ViewMode3D::CutawayAbove:
            return floor <= m_currentFloor;

        case ViewMode3D::XRay:
            // Render floors within vertical vision range
            return std::abs(floor - m_currentFloor) <=
                   std::max(m_config.maxVerticalVisionUp, m_config.maxVerticalVisionDown);

        case ViewMode3D::AllFloors:
            return true;
    }

    return false;
}

float FogOfWar3D::GetFloorOpacity(int floor) const {
    if (floor < 0 || floor >= m_depth) return 0.0f;

    int floorDiff = floor - m_currentFloor;

    switch (m_viewMode) {
        case ViewMode3D::CurrentFloor:
            return floor == m_currentFloor ? 1.0f : 0.0f;

        case ViewMode3D::CutawayAbove:
            if (floorDiff > 0) {
                return m_config.aboveFloorOpacity;
            } else if (floorDiff < 0) {
                return m_config.belowFloorOpacity;
            }
            return 1.0f;

        case ViewMode3D::XRay: {
            float falloff = 1.0f / (1.0f + std::abs(static_cast<float>(floorDiff)) * 0.5f);
            return falloff;
        }

        case ViewMode3D::AllFloors:
            return 1.0f;
    }

    return 1.0f;
}

void FogOfWar3D::UpdateVisibility(glm::vec3 playerPos, float deltaTime) {
    if (!m_initialized) return;

    m_lastPlayerPos = playerPos;

    // Update floor based on player Z position
    int playerFloor = static_cast<int>(playerPos.z / m_tileSizeZ);
    if (playerFloor != m_currentFloor) {
        SetCurrentFloor(playerFloor);
    }

    // Update explored state
    UpdateExploredState(playerPos);

    // Update floor transition
    UpdateFloorTransition(deltaTime);

    // Update fog textures
    UpdateFogTextures(deltaTime);
}

void FogOfWar3D::ForceUpdate() {
    if (!m_initialized || !m_radianceCascades) return;

    // Update all voxels
    for (int z = 0; z < m_depth; z++) {
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                int idx = VoxelIndex(x, y, z);
                glm::vec3 voxelCenter = VoxelToWorld(x, y, z);

                float vis = m_radianceCascades->GetVisibility(voxelCenter);
                bool visible = vis > m_config.visibilityThreshold;

                m_visibilityState[idx] = visible ? 255 : 0;

                if (visible && m_config.revealOnExplore) {
                    m_exploredState[idx] = 1;
                }
            }
        }
    }
}

void FogOfWar3D::UpdateExploredState(glm::vec3 playerPos) {
    if (!m_radianceCascades) return;

    glm::ivec3 playerVoxel = WorldToVoxel(playerPos);
    int checkRadius = static_cast<int>(m_radianceCascades->GetConfig().maxRayDistance / m_tileSizeXY) + 1;
    int checkRadiusZ = static_cast<int>(m_radianceCascades->GetConfig().maxRayDistance / m_tileSizeZ) + 1;

    bool stateChanged = false;

    for (int dz = -checkRadiusZ; dz <= checkRadiusZ; dz++) {
        for (int dy = -checkRadius; dy <= checkRadius; dy++) {
            for (int dx = -checkRadius; dx <= checkRadius; dx++) {
                int vx = playerVoxel.x + dx;
                int vy = playerVoxel.y + dy;
                int vz = playerVoxel.z + dz;

                if (vx < 0 || vx >= m_width || vy < 0 || vy >= m_height || vz < 0 || vz >= m_depth) {
                    continue;
                }

                int idx = VoxelIndex(vx, vy, vz);
                glm::vec3 voxelCenter = VoxelToWorld(vx, vy, vz);

                float visibility = m_radianceCascades->GetVisibility(voxelCenter);
                bool visible = visibility > m_config.visibilityThreshold;

                m_visibilityState[idx] = visible ? 255 : 0;

                if (visible && m_config.revealOnExplore && m_exploredState[idx] == 0) {
                    m_exploredState[idx] = 1;
                    stateChanged = true;
                }
            }
        }
    }

    // Upload explored state per floor if changed
    if (stateChanged) {
        for (int z = 0; z < m_depth; z++) {
            std::vector<uint8_t> floorData(m_width * m_height);
            for (int y = 0; y < m_height; y++) {
                for (int x = 0; x < m_width; x++) {
                    floorData[y * m_width + x] = m_exploredState[VoxelIndex(x, y, z)];
                }
            }

            glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height,
                           GL_RED, GL_UNSIGNED_BYTE, floorData.data());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void FogOfWar3D::UpdateFloorTransition(float deltaTime) {
    if (m_floorTransition < 1.0f) {
        m_floorTransition += deltaTime * m_config.floorTransitionSpeed;
        m_floorTransition = std::min(1.0f, m_floorTransition);
    }
}

void FogOfWar3D::UpdateFogTextures(float deltaTime) {
    if (!m_radianceCascades) return;

    // Create temporary 3D textures for visibility and explored state
    uint32_t visVolume, exploredVolume;
    glGenTextures(1, &visVolume);
    glGenTextures(1, &exploredVolume);

    // Upload visibility state
    glBindTexture(GL_TEXTURE_3D, visVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, m_width, m_height, m_depth, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_visibilityState.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload explored state
    glBindTexture(GL_TEXTURE_3D, exploredVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, m_width, m_height, m_depth, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_exploredState.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_3D, 0);

    // Update fog volume
    glUseProgram(m_fogUpdate3DShader);

    glBindImageTexture(0, m_fogVolume, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R16F);
    glBindImageTexture(1, exploredVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8);
    glBindImageTexture(2, visVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8);
    glBindImageTexture(3, m_radianceCascades->GetRadianceVolume(), 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

    glUniform3i(glGetUniformLocation(m_fogUpdate3DShader, "u_VolumeSize"), m_width, m_height, m_depth);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_DeltaTime"), deltaTime);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_TransitionSpeed"), m_config.transitionSpeed);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_UnexploredBrightness"), m_config.unexploredBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_ExploredBrightness"), m_config.exploredBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_VisibleBrightness"), m_config.visibleBrightness);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_VisibilityThreshold"), m_config.visibilityThreshold);
    glUniform1i(glGetUniformLocation(m_fogUpdate3DShader, "u_CurrentFloor"), m_currentFloor);
    glUniform1i(glGetUniformLocation(m_fogUpdate3DShader, "u_ViewMode"), static_cast<int>(m_viewMode));
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_AboveFloorOpacity"), m_config.aboveFloorOpacity);
    glUniform1f(glGetUniformLocation(m_fogUpdate3DShader, "u_BelowFloorOpacity"), m_config.belowFloorOpacity);

    int groupsX = (m_width + 7) / 8;
    int groupsY = (m_height + 7) / 8;
    int groupsZ = (m_depth + 3) / 4;
    glDispatchCompute(groupsX, groupsY, groupsZ);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // Extract per-floor textures
    glUseProgram(m_extractFloorFogShader);

    glBindImageTexture(1, m_fogVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R16F);
    glUniform2i(glGetUniformLocation(m_extractFloorFogShader, "u_OutputSize"), m_width, m_height);

    for (int z = 0; z < m_depth; z++) {
        glBindImageTexture(0, m_floorFogTextures[z], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);
        glUniform1i(glGetUniformLocation(m_extractFloorFogShader, "u_FloorLevel"), z);

        int groupsX2D = (m_width + 7) / 8;
        int groupsY2D = (m_height + 7) / 8;
        glDispatchCompute(groupsX2D, groupsY2D, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);

    // Cleanup temporary textures
    glDeleteTextures(1, &visVolume);
    glDeleteTextures(1, &exploredVolume);
}

uint32_t FogOfWar3D::GetFogTextureForFloor(int floor) const {
    if (floor < 0 || floor >= static_cast<int>(m_floorFogTextures.size())) {
        return 0;
    }
    return m_floorFogTextures[floor];
}

uint32_t FogOfWar3D::GetCombinedTextureForFloor(int floor) const {
    if (floor < 0 || floor >= static_cast<int>(m_floorCombinedTextures.size())) {
        return 0;
    }
    return m_floorCombinedTextures[floor];
}

FogState3D FogOfWar3D::GetFogState(int x, int y, int z) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return FogState3D::Unknown;
    }

    int idx = VoxelIndex(x, y, z);

    if (m_visibilityState[idx] > 0) {
        return FogState3D::Visible;
    } else if (m_exploredState[idx] > 0) {
        return FogState3D::Explored;
    }
    return FogState3D::Unknown;
}

FogState3D FogOfWar3D::GetFogStateAtPosition(glm::vec3 worldPos) const {
    glm::ivec3 voxel = WorldToVoxel(worldPos);
    return GetFogState(voxel.x, voxel.y, voxel.z);
}

float FogOfWar3D::GetFogBrightness(glm::vec3 worldPos) const {
    glm::ivec3 voxel = WorldToVoxel(worldPos);

    if (voxel.x < 0 || voxel.x >= m_width || voxel.y < 0 || voxel.y >= m_height ||
        voxel.z < 0 || voxel.z >= m_depth) {
        return m_config.unexploredBrightness;
    }

    return m_fogBrightness[VoxelIndex(voxel.x, voxel.y, voxel.z)];
}

bool FogOfWar3D::IsExplored(int x, int y, int z) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return false;
    }
    return m_exploredState[VoxelIndex(x, y, z)] > 0;
}

bool FogOfWar3D::IsVisible(int x, int y, int z) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return false;
    }
    return m_visibilityState[VoxelIndex(x, y, z)] > 0;
}

bool FogOfWar3D::HasVerticalLineOfSight(glm::ivec3 from, glm::ivec3 to) const {
    if (!m_radianceCascades) return true;

    glm::vec3 fromWorld = VoxelToWorld(from.x, from.y, from.z);
    glm::vec3 toWorld = VoxelToWorld(to.x, to.y, to.z);

    return m_radianceCascades->HasVerticalLineOfSight(fromWorld, toWorld);
}

void FogOfWar3D::RevealVoxel(int x, int y, int z) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return;
    }

    m_exploredState[VoxelIndex(x, y, z)] = 1;

    // Update texture
    glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
    uint8_t data = 1;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar3D::RevealArea(glm::vec3 center, float radius) {
    glm::ivec3 centerVoxel = WorldToVoxel(center);
    int radiusXY = static_cast<int>(std::ceil(radius / m_tileSizeXY));
    int radiusZ = static_cast<int>(std::ceil(radius / m_tileSizeZ));

    for (int dz = -radiusZ; dz <= radiusZ; dz++) {
        for (int dy = -radiusXY; dy <= radiusXY; dy++) {
            for (int dx = -radiusXY; dx <= radiusXY; dx++) {
                int vx = centerVoxel.x + dx;
                int vy = centerVoxel.y + dy;
                int vz = centerVoxel.z + dz;

                if (vx < 0 || vx >= m_width || vy < 0 || vy >= m_height || vz < 0 || vz >= m_depth) {
                    continue;
                }

                glm::vec3 voxelPos = VoxelToWorld(vx, vy, vz);
                float dist = glm::length(voxelPos - center);
                if (dist <= radius) {
                    m_exploredState[VoxelIndex(vx, vy, vz)] = 1;
                }
            }
        }
    }

    // Update all floor textures
    for (int z = 0; z < m_depth; z++) {
        std::vector<uint8_t> floorData(m_width * m_height);
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                floorData[y * m_width + x] = m_exploredState[VoxelIndex(x, y, z)];
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar3D::RevealFloor(int floor) {
    if (floor < 0 || floor >= m_depth) return;

    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            m_exploredState[VoxelIndex(x, y, floor)] = 1;
        }
    }

    // Update floor texture
    std::vector<uint8_t> floorData(m_width * m_height, 1);
    glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[floor]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar3D::RevealAll() {
    std::fill(m_exploredState.begin(), m_exploredState.end(), 1);

    for (int z = 0; z < m_depth; z++) {
        std::vector<uint8_t> floorData(m_width * m_height, 1);
        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar3D::ResetFog() {
    std::fill(m_exploredState.begin(), m_exploredState.end(), 0);
    std::fill(m_visibilityState.begin(), m_visibilityState.end(), 0);
    std::fill(m_fogBrightness.begin(), m_fogBrightness.end(), 0.0f);

    for (int z = 0; z < m_depth; z++) {
        std::vector<uint8_t> floorData(m_width * m_height, 0);
        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void FogOfWar3D::HideVoxel(int x, int y, int z) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return;
    }

    m_exploredState[VoxelIndex(x, y, z)] = 0;

    glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
    uint8_t data = 0;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_RED, GL_UNSIGNED_BYTE, &data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool FogOfWar3D::SaveExploredState(const std::string& filepath) const {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for saving 3D fog state: {}", filepath);
        return false;
    }

    // Write header
    file.write(reinterpret_cast<const char*>(&m_width), sizeof(m_width));
    file.write(reinterpret_cast<const char*>(&m_height), sizeof(m_height));
    file.write(reinterpret_cast<const char*>(&m_depth), sizeof(m_depth));

    // Write explored state
    file.write(reinterpret_cast<const char*>(m_exploredState.data()), m_exploredState.size());

    spdlog::info("Saved 3D fog state to: {}", filepath);
    return true;
}

bool FogOfWar3D::LoadExploredState(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for loading 3D fog state: {}", filepath);
        return false;
    }

    int width, height, depth;
    file.read(reinterpret_cast<char*>(&width), sizeof(width));
    file.read(reinterpret_cast<char*>(&height), sizeof(height));
    file.read(reinterpret_cast<char*>(&depth), sizeof(depth));

    if (width != m_width || height != m_height || depth != m_depth) {
        spdlog::error("3D fog state dimensions mismatch: expected {}x{}x{}, got {}x{}x{}",
                      m_width, m_height, m_depth, width, height, depth);
        return false;
    }

    file.read(reinterpret_cast<char*>(m_exploredState.data()), m_exploredState.size());

    // Update GPU textures
    for (int z = 0; z < m_depth; z++) {
        std::vector<uint8_t> floorData(m_width * m_height);
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                floorData[y * m_width + x] = m_exploredState[VoxelIndex(x, y, z)];
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    spdlog::info("Loaded 3D fog state from: {}", filepath);
    return true;
}

void FogOfWar3D::SetExploredData(const std::vector<uint8_t>& data) {
    if (data.size() != m_exploredState.size()) {
        spdlog::error("3D explored data size mismatch");
        return;
    }

    m_exploredState = data;

    for (int z = 0; z < m_depth; z++) {
        std::vector<uint8_t> floorData(m_width * m_height);
        for (int y = 0; y < m_height; y++) {
            for (int x = 0; x < m_width; x++) {
                floorData[y * m_width + x] = m_exploredState[VoxelIndex(x, y, z)];
            }
        }

        glBindTexture(GL_TEXTURE_2D, m_floorExploredTextures[z]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RED, GL_UNSIGNED_BYTE, floorData.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

float FogOfWar3D::GetExplorationProgress() const {
    if (m_exploredState.empty()) return 0.0f;

    int explored = 0;
    for (uint8_t state : m_exploredState) {
        if (state > 0) explored++;
    }

    return (static_cast<float>(explored) / static_cast<float>(m_exploredState.size())) * 100.0f;
}

float FogOfWar3D::GetFloorExplorationProgress(int floor) const {
    if (floor < 0 || floor >= m_depth) return 0.0f;

    int explored = 0;
    int total = m_width * m_height;

    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            if (m_exploredState[VoxelIndex(x, y, floor)] > 0) {
                explored++;
            }
        }
    }

    return (static_cast<float>(explored) / static_cast<float>(total)) * 100.0f;
}

void FogOfWar3D::SetConfig(const Config& config) {
    m_config = config;
}

int FogOfWar3D::VoxelIndex(int x, int y, int z) const {
    return z * (m_width * m_height) + y * m_width + x;
}

glm::ivec3 FogOfWar3D::WorldToVoxel(glm::vec3 worldPos) const {
    return glm::ivec3(
        static_cast<int>(worldPos.x / m_tileSizeXY),
        static_cast<int>(worldPos.y / m_tileSizeXY),
        static_cast<int>(worldPos.z / m_tileSizeZ)
    );
}

glm::vec3 FogOfWar3D::VoxelToWorld(int x, int y, int z) const {
    return glm::vec3(
        (x + 0.5f) * m_tileSizeXY,
        (y + 0.5f) * m_tileSizeXY,
        (z + 0.5f) * m_tileSizeZ
    );
}

// ============================================================================
// FogOfWar3DRenderer Implementation
// ============================================================================

FogOfWar3DRenderer::FogOfWar3DRenderer() = default;

FogOfWar3DRenderer::~FogOfWar3DRenderer() {
    Shutdown();
}

bool FogOfWar3DRenderer::Initialize() {
    if (m_initialized) return true;

    if (!CreateShaders()) {
        return false;
    }

    // Create fullscreen quad
    float quadVertices[] = {
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

void FogOfWar3DRenderer::Shutdown() {
    DestroyResources();
    m_initialized = false;
}

bool FogOfWar3DRenderer::CreateShaders() {
    m_floorFogShader = CompileShaderProgram(FLOOR_FOG_VERTEX_SHADER, FLOOR_FOG_FRAGMENT_SHADER);
    return m_floorFogShader != 0;
}

void FogOfWar3DRenderer::DestroyResources() {
    if (m_floorFogShader) {
        glDeleteProgram(m_floorFogShader);
        m_floorFogShader = 0;
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

void FogOfWar3DRenderer::BeginFloor(const FogOfWar3D& fogOfWar, int floor, float opacity) {
    m_currentRenderFloor = floor;
    m_currentFloorOpacity = opacity;
}

void FogOfWar3DRenderer::EndFloor() {
    m_currentRenderFloor = -1;
    m_currentFloorOpacity = 1.0f;
}

void FogOfWar3DRenderer::ApplyFogToFloor(const FogOfWar3D& fogOfWar, const RadianceCascades3D& cascades,
                                          int floor, uint32_t sceneTexture, uint32_t outputFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);

    glUseProgram(m_floorFogShader);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);
    glUniform1i(glGetUniformLocation(m_floorFogShader, "u_SceneTexture"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fogOfWar.GetFogTextureForFloor(floor));
    glUniform1i(glGetUniformLocation(m_floorFogShader, "u_FogTexture"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, cascades.GetRadianceTextureForLevel(floor));
    glUniform1i(glGetUniformLocation(m_floorFogShader, "u_RadianceTexture"), 2);

    // Set uniforms
    glUniform3fv(glGetUniformLocation(m_floorFogShader, "u_FogColor"), 1,
                 glm::value_ptr(fogOfWar.GetConfig().fogColor));
    glUniform1f(glGetUniformLocation(m_floorFogShader, "u_FloorOpacity"), fogOfWar.GetFloorOpacity(floor));
    glUniform1i(glGetUniformLocation(m_floorFogShader, "u_UseRadiance"), 1);

    // Draw fullscreen quad
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}

void FogOfWar3DRenderer::GetRenderOrder(const FogOfWar3D& fogOfWar, std::vector<int>& outFloors) const {
    outFloors.clear();

    // Render from bottom to top
    for (int floor = 0; floor < fogOfWar.GetDepth(); floor++) {
        if (fogOfWar.ShouldRenderFloor(floor)) {
            outFloors.push_back(floor);
        }
    }
}

} // namespace Vehement2
