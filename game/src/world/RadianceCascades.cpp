#include "RadianceCascades.hpp"
#include "TileMap.hpp"  // For Vehement::TileMap integration
#include "Tile.hpp"     // For Vehement::Tile struct

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Vehement2 {

// Helper to compile compute shaders
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

// Helper to load shader from file
static std::string LoadShaderFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open shader file: {}", path);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// ============================================================================
// Embedded Compute Shaders
// ============================================================================

static const char* RAYMARCH_SHADER_SOURCE = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output cascade texture
layout(rgba16f, binding = 0) uniform image2D u_CascadeOutput;

// Input occlusion texture
layout(r8, binding = 1) uniform readonly image2D u_Occlusion;

// Light data SSBO
struct Light {
    vec2 position;
    vec3 color;
    float intensity;
    float radius;
    float _padding;
};

layout(std430, binding = 0) buffer LightBuffer {
    int numLights;
    int _pad1, _pad2, _pad3;
    Light lights[];
};

// Uniforms
uniform int u_CascadeLevel;
uniform int u_NumRays;
uniform float u_IntervalStart;
uniform float u_IntervalEnd;
uniform vec2 u_Resolution;
uniform vec2 u_OcclusionSize;
uniform float u_BiasDistance;

// Player visibility
uniform vec2 u_PlayerPosition;
uniform float u_PlayerRadius;
uniform bool u_HasPlayer;

const float PI = 3.14159265359;
const float TAU = 6.28318530718;

// Sample occlusion with bounds checking
float sampleOcclusion(vec2 pos) {
    ivec2 ipos = ivec2(pos);
    if (ipos.x < 0 || ipos.y < 0 ||
        ipos.x >= int(u_OcclusionSize.x) || ipos.y >= int(u_OcclusionSize.y)) {
        return 0.0; // Out of bounds = not blocked
    }
    return imageLoad(u_Occlusion, ipos).r;
}

// Ray march through occlusion texture
// Returns: x = visibility (0-1), y = distance traveled
vec2 rayMarch(vec2 origin, vec2 direction, float startDist, float endDist) {
    float stepSize = 1.0;
    float dist = startDist + u_BiasDistance;
    float visibility = 1.0;

    while (dist < endDist) {
        vec2 pos = origin + direction * dist;
        float occlusion = sampleOcclusion(pos);

        if (occlusion > 0.5) {
            // Hit occluder
            visibility = 0.0;
            break;
        }

        dist += stepSize;
    }

    return vec2(visibility, dist);
}

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 cascadeSize = vec2(imageSize(u_CascadeOutput));

    if (pixelCoord.x >= int(cascadeSize.x) || pixelCoord.y >= int(cascadeSize.y)) {
        return;
    }

    // Scale factor from cascade space to screen space
    float scaleFactor = pow(2.0, float(u_CascadeLevel));
    vec2 screenPos = (vec2(pixelCoord) + 0.5) * scaleFactor;

    // Accumulate radiance from all directions
    vec4 totalRadiance = vec4(0.0);
    float totalWeight = 0.0;

    // Shoot rays in multiple directions
    for (int i = 0; i < u_NumRays; i++) {
        float angle = (float(i) + 0.5) / float(u_NumRays) * TAU;
        vec2 direction = vec2(cos(angle), sin(angle));

        // Ray march in this direction
        vec2 result = rayMarch(screenPos, direction, u_IntervalStart, u_IntervalEnd);
        float visibility = result.x;

        // Gather light from this direction
        vec3 radiance = vec3(0.0);

        if (visibility > 0.0) {
            // Sample lights
            for (int li = 0; li < numLights && li < 256; li++) {
                Light light = lights[li];

                vec2 toLight = light.position - screenPos;
                float distToLight = length(toLight);

                if (distToLight < light.radius && distToLight > 0.001) {
                    vec2 dirToLight = toLight / distToLight;

                    // Check if this ray direction points toward the light
                    float alignment = dot(direction, dirToLight);
                    if (alignment > 0.0) {
                        // Attenuation based on distance
                        float attenuation = 1.0 - (distToLight / light.radius);
                        attenuation = attenuation * attenuation;

                        // Angular contribution (rays directly toward light contribute more)
                        float angular = pow(max(alignment, 0.0), 2.0);

                        radiance += light.color * light.intensity * attenuation * angular * visibility;
                    }
                }
            }

            // Player visibility contribution (white light for visibility)
            if (u_HasPlayer) {
                vec2 toPlayer = u_PlayerPosition - screenPos;
                float distToPlayer = length(toPlayer);

                if (distToPlayer < u_PlayerRadius && distToPlayer > 0.001) {
                    vec2 dirToPlayer = toPlayer / distToPlayer;
                    float alignment = dot(direction, dirToPlayer);

                    if (alignment > 0.0) {
                        float attenuation = 1.0 - (distToPlayer / u_PlayerRadius);
                        attenuation = attenuation * attenuation;
                        float angular = pow(max(alignment, 0.0), 2.0);

                        // Add to alpha channel for visibility
                        radiance += vec3(0.1) * attenuation * angular * visibility;
                    }
                }
            }
        }

        totalRadiance.rgb += radiance;
        totalRadiance.a += visibility;  // Store average visibility in alpha
        totalWeight += 1.0;
    }

    // Normalize
    if (totalWeight > 0.0) {
        totalRadiance /= totalWeight;
    }

    imageStore(u_CascadeOutput, pixelCoord, totalRadiance);
}
)";

static const char* MERGE_SHADER_SOURCE = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Current cascade level (being merged into)
layout(rgba16f, binding = 0) uniform image2D u_CascadeCurrent;

// Coarser cascade level (being merged from)
layout(rgba16f, binding = 1) uniform readonly image2D u_CascadeCoarse;

uniform int u_CascadeLevel;
uniform vec2 u_CurrentSize;
uniform vec2 u_CoarseSize;
uniform float u_MergeWeight;

// Bilinear interpolation helper
vec4 sampleBilinear(ivec2 pos, vec2 frac) {
    vec4 tl = imageLoad(u_CascadeCoarse, pos);
    vec4 tr = imageLoad(u_CascadeCoarse, pos + ivec2(1, 0));
    vec4 bl = imageLoad(u_CascadeCoarse, pos + ivec2(0, 1));
    vec4 br = imageLoad(u_CascadeCoarse, pos + ivec2(1, 1));

    vec4 top = mix(tl, tr, frac.x);
    vec4 bottom = mix(bl, br, frac.x);

    return mix(top, bottom, frac.y);
}

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= int(u_CurrentSize.x) || pixelCoord.y >= int(u_CurrentSize.y)) {
        return;
    }

    // Load current cascade value
    vec4 currentValue = imageLoad(u_CascadeCurrent, pixelCoord);

    // Sample from coarser cascade with bilinear interpolation
    vec2 coarseUV = (vec2(pixelCoord) + 0.5) / u_CurrentSize * u_CoarseSize;
    ivec2 coarsePixel = ivec2(floor(coarseUV - 0.5));
    vec2 frac = fract(coarseUV - 0.5);

    // Clamp to valid range
    coarsePixel = clamp(coarsePixel, ivec2(0), ivec2(u_CoarseSize) - 2);

    vec4 coarseValue = sampleBilinear(coarsePixel, frac);

    // Merge: current cascade provides local detail, coarse provides distant light
    // The merge weight controls how much of the coarse cascade propagates to finer levels
    vec4 merged;
    merged.rgb = currentValue.rgb + coarseValue.rgb * u_MergeWeight;
    merged.a = max(currentValue.a, coarseValue.a * u_MergeWeight);  // Visibility: take max

    imageStore(u_CascadeCurrent, pixelCoord, merged);
}
)";

static const char* RADIANCE_SHADER_SOURCE = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output radiance texture
layout(rgba16f, binding = 0) uniform writeonly image2D u_RadianceOutput;

// Finest cascade level
layout(rgba16f, binding = 1) uniform readonly image2D u_CascadeFinest;

// Occlusion texture
layout(r8, binding = 2) uniform readonly image2D u_Occlusion;

// Light data
struct Light {
    vec2 position;
    vec3 color;
    float intensity;
    float radius;
    float _padding;
};

layout(std430, binding = 0) buffer LightBuffer {
    int numLights;
    int _pad1, _pad2, _pad3;
    Light lights[];
};

uniform vec2 u_Resolution;
uniform vec2 u_OcclusionSize;
uniform vec2 u_PlayerPosition;
uniform float u_PlayerRadius;
uniform bool u_HasPlayer;
uniform float u_AmbientLight;

float sampleOcclusion(vec2 pos) {
    ivec2 ipos = ivec2(pos * u_OcclusionSize / u_Resolution);
    if (ipos.x < 0 || ipos.y < 0 ||
        ipos.x >= int(u_OcclusionSize.x) || ipos.y >= int(u_OcclusionSize.y)) {
        return 0.0;
    }
    return imageLoad(u_Occlusion, ipos).r;
}

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= int(u_Resolution.x) || pixelCoord.y >= int(u_Resolution.y)) {
        return;
    }

    vec2 screenPos = vec2(pixelCoord) + 0.5;

    // Check if this pixel is inside a wall
    float occlusion = sampleOcclusion(screenPos);
    if (occlusion > 0.5) {
        // Inside wall - no light
        imageStore(u_RadianceOutput, pixelCoord, vec4(0.0, 0.0, 0.0, 0.0));
        return;
    }

    // Sample cascade radiance
    vec4 cascadeValue = imageLoad(u_CascadeFinest, pixelCoord);
    vec3 radiance = cascadeValue.rgb;
    float visibility = cascadeValue.a;

    // Add direct light contributions (lights that are very close)
    for (int i = 0; i < numLights && i < 256; i++) {
        Light light = lights[i];

        float dist = length(light.position - screenPos);
        if (dist < light.radius * 0.1) {
            // Very close to light source - add direct contribution
            float attenuation = 1.0 - (dist / (light.radius * 0.1));
            attenuation = attenuation * attenuation;
            radiance += light.color * light.intensity * attenuation * 2.0;
        }
    }

    // Calculate player visibility
    float playerVisibility = 0.0;
    if (u_HasPlayer) {
        float distToPlayer = length(u_PlayerPosition - screenPos);
        if (distToPlayer < u_PlayerRadius) {
            playerVisibility = 1.0 - (distToPlayer / u_PlayerRadius);
            playerVisibility = playerVisibility * playerVisibility;
        }
    }

    // Combine visibility with cascade visibility
    visibility = max(visibility, playerVisibility);

    // Add ambient light
    radiance += vec3(u_AmbientLight);

    // Tone mapping (simple reinhard)
    radiance = radiance / (radiance + vec3(1.0));

    // Output: RGB = radiance, A = visibility
    imageStore(u_RadianceOutput, pixelCoord, vec4(radiance, visibility));
}
)";

// ============================================================================
// RadianceCascades Implementation
// ============================================================================

RadianceCascades::RadianceCascades() = default;

RadianceCascades::~RadianceCascades() {
    Shutdown();
}

RadianceCascades::RadianceCascades(RadianceCascades&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_cascadeLevels(other.m_cascadeLevels)
    , m_config(other.m_config)
    , m_initialized(other.m_initialized)
    , m_occlusionDirty(other.m_occlusionDirty)
    , m_lightsDirty(other.m_lightsDirty)
    , m_cascadeTextures(std::move(other.m_cascadeTextures))
    , m_cascadeTempTextures(std::move(other.m_cascadeTempTextures))
    , m_finalRadianceTexture(other.m_finalRadianceTexture)
    , m_occlusionTexture(other.m_occlusionTexture)
    , m_occlusionData(std::move(other.m_occlusionData))
    , m_occlusionWidth(other.m_occlusionWidth)
    , m_occlusionHeight(other.m_occlusionHeight)
    , m_lightTexture(other.m_lightTexture)
    , m_lights(std::move(other.m_lights))
    , m_playerPosition(other.m_playerPosition)
    , m_playerVisibilityRadius(other.m_playerVisibilityRadius)
    , m_hasPlayer(other.m_hasPlayer)
    , m_worldToScreen(other.m_worldToScreen)
    , m_screenToWorld(other.m_screenToWorld)
    , m_rayMarchShader(other.m_rayMarchShader)
    , m_mergeShader(other.m_mergeShader)
    , m_radianceShader(other.m_radianceShader)
    , m_lightSSBO(other.m_lightSSBO)
    , m_paramsUBO(other.m_paramsUBO)
{
    other.m_initialized = false;
    other.m_finalRadianceTexture = 0;
    other.m_occlusionTexture = 0;
    other.m_lightTexture = 0;
    other.m_rayMarchShader = 0;
    other.m_mergeShader = 0;
    other.m_radianceShader = 0;
    other.m_lightSSBO = 0;
    other.m_paramsUBO = 0;
}

RadianceCascades& RadianceCascades::operator=(RadianceCascades&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_width = other.m_width;
        m_height = other.m_height;
        m_cascadeLevels = other.m_cascadeLevels;
        m_config = other.m_config;
        m_initialized = other.m_initialized;
        m_occlusionDirty = other.m_occlusionDirty;
        m_lightsDirty = other.m_lightsDirty;
        m_cascadeTextures = std::move(other.m_cascadeTextures);
        m_cascadeTempTextures = std::move(other.m_cascadeTempTextures);
        m_finalRadianceTexture = other.m_finalRadianceTexture;
        m_occlusionTexture = other.m_occlusionTexture;
        m_occlusionData = std::move(other.m_occlusionData);
        m_occlusionWidth = other.m_occlusionWidth;
        m_occlusionHeight = other.m_occlusionHeight;
        m_lightTexture = other.m_lightTexture;
        m_lights = std::move(other.m_lights);
        m_playerPosition = other.m_playerPosition;
        m_playerVisibilityRadius = other.m_playerVisibilityRadius;
        m_hasPlayer = other.m_hasPlayer;
        m_worldToScreen = other.m_worldToScreen;
        m_screenToWorld = other.m_screenToWorld;
        m_rayMarchShader = other.m_rayMarchShader;
        m_mergeShader = other.m_mergeShader;
        m_radianceShader = other.m_radianceShader;
        m_lightSSBO = other.m_lightSSBO;
        m_paramsUBO = other.m_paramsUBO;

        other.m_initialized = false;
        other.m_finalRadianceTexture = 0;
        other.m_occlusionTexture = 0;
        other.m_lightTexture = 0;
        other.m_rayMarchShader = 0;
        other.m_mergeShader = 0;
        other.m_radianceShader = 0;
        other.m_lightSSBO = 0;
        other.m_paramsUBO = 0;
    }
    return *this;
}

bool RadianceCascades::Initialize(int width, int height, int cascadeLevels) {
    if (m_initialized) {
        spdlog::warn("RadianceCascades already initialized, shutting down first");
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_cascadeLevels = std::max(1, std::min(cascadeLevels, 8));

    spdlog::info("Initializing RadianceCascades: {}x{}, {} levels", width, height, m_cascadeLevels);

    if (!CreateShaders()) {
        spdlog::error("Failed to create radiance cascade shaders");
        return false;
    }

    if (!CreateTextures()) {
        spdlog::error("Failed to create radiance cascade textures");
        DestroyResources();
        return false;
    }

    if (!CreateBuffers()) {
        spdlog::error("Failed to create radiance cascade buffers");
        DestroyResources();
        return false;
    }

    m_initialized = true;
    spdlog::info("RadianceCascades initialized successfully");
    return true;
}

void RadianceCascades::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down RadianceCascades");
    DestroyResources();
    m_initialized = false;
}

void RadianceCascades::Resize(int width, int height) {
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    // Recreate textures with new size
    // Delete old cascade textures
    if (!m_cascadeTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTextures.size()), m_cascadeTextures.data());
        m_cascadeTextures.clear();
    }
    if (!m_cascadeTempTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTempTextures.size()), m_cascadeTempTextures.data());
        m_cascadeTempTextures.clear();
    }
    if (m_finalRadianceTexture) {
        glDeleteTextures(1, &m_finalRadianceTexture);
        m_finalRadianceTexture = 0;
    }

    CreateTextures();
}

bool RadianceCascades::CreateShaders() {
    m_rayMarchShader = CompileComputeShader(RAYMARCH_SHADER_SOURCE);
    if (m_rayMarchShader == 0) {
        spdlog::error("Failed to compile ray march shader");
        return false;
    }

    m_mergeShader = CompileComputeShader(MERGE_SHADER_SOURCE);
    if (m_mergeShader == 0) {
        spdlog::error("Failed to compile merge shader");
        return false;
    }

    m_radianceShader = CompileComputeShader(RADIANCE_SHADER_SOURCE);
    if (m_radianceShader == 0) {
        spdlog::error("Failed to compile radiance shader");
        return false;
    }

    return true;
}

bool RadianceCascades::CreateTextures() {
    // Create cascade textures at decreasing resolutions
    m_cascadeTextures.resize(m_cascadeLevels);
    m_cascadeTempTextures.resize(m_cascadeLevels);

    glGenTextures(m_cascadeLevels, m_cascadeTextures.data());
    glGenTextures(m_cascadeLevels, m_cascadeTempTextures.data());

    for (int level = 0; level < m_cascadeLevels; level++) {
        int levelWidth = m_width >> level;
        int levelHeight = m_height >> level;
        levelWidth = std::max(1, levelWidth);
        levelHeight = std::max(1, levelHeight);

        // Main cascade texture
        glBindTexture(GL_TEXTURE_2D, m_cascadeTextures[level]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, levelWidth, levelHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Temp texture for ping-pong
        glBindTexture(GL_TEXTURE_2D, m_cascadeTempTextures[level]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, levelWidth, levelHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Create final radiance texture at full resolution
    glGenTextures(1, &m_finalRadianceTexture);
    glBindTexture(GL_TEXTURE_2D, m_finalRadianceTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create occlusion texture
    glGenTextures(1, &m_occlusionTexture);
    glBindTexture(GL_TEXTURE_2D, m_occlusionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_width, m_height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool RadianceCascades::CreateBuffers() {
    // Create SSBO for light data
    glGenBuffers(1, &m_lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);

    // Allocate space for header + max lights
    struct LightSSBOHeader {
        int numLights;
        int pad1, pad2, pad3;
    };

    struct LightSSBOData {
        glm::vec2 position;
        glm::vec3 color;
        float intensity;
        float radius;
        float padding;
    };

    size_t bufferSize = sizeof(LightSSBOHeader) + sizeof(LightSSBOData) * MAX_LIGHTS;
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return true;
}

void RadianceCascades::DestroyResources() {
    if (!m_cascadeTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTextures.size()), m_cascadeTextures.data());
        m_cascadeTextures.clear();
    }

    if (!m_cascadeTempTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTempTextures.size()), m_cascadeTempTextures.data());
        m_cascadeTempTextures.clear();
    }

    if (m_finalRadianceTexture) {
        glDeleteTextures(1, &m_finalRadianceTexture);
        m_finalRadianceTexture = 0;
    }

    if (m_occlusionTexture) {
        glDeleteTextures(1, &m_occlusionTexture);
        m_occlusionTexture = 0;
    }

    if (m_lightTexture) {
        glDeleteTextures(1, &m_lightTexture);
        m_lightTexture = 0;
    }

    if (m_rayMarchShader) {
        glDeleteProgram(m_rayMarchShader);
        m_rayMarchShader = 0;
    }

    if (m_mergeShader) {
        glDeleteProgram(m_mergeShader);
        m_mergeShader = 0;
    }

    if (m_radianceShader) {
        glDeleteProgram(m_radianceShader);
        m_radianceShader = 0;
    }

    if (m_lightSSBO) {
        glDeleteBuffers(1, &m_lightSSBO);
        m_lightSSBO = 0;
    }

    if (m_paramsUBO) {
        glDeleteBuffers(1, &m_paramsUBO);
        m_paramsUBO = 0;
    }

    m_lights.clear();
    m_occlusionData.clear();
}

void RadianceCascades::SetOcclusionMap(const IOcclusionProvider& provider) {
    int mapWidth = provider.GetWidth();
    int mapHeight = provider.GetHeight();
    float tileSize = provider.GetTileSize();

    // Calculate occlusion texture size to match screen resolution
    m_occlusionWidth = static_cast<int>(mapWidth * tileSize);
    m_occlusionHeight = static_cast<int>(mapHeight * tileSize);

    m_occlusionData.resize(m_occlusionWidth * m_occlusionHeight);

    // Fill occlusion data from provider
    for (int y = 0; y < m_occlusionHeight; y++) {
        for (int x = 0; x < m_occlusionWidth; x++) {
            int tileX = static_cast<int>(x / tileSize);
            int tileY = static_cast<int>(y / tileSize);

            bool blocked = provider.IsBlocked(tileX, tileY);
            m_occlusionData[y * m_occlusionWidth + x] = blocked ? 255 : 0;
        }
    }

    // Upload to GPU
    glBindTexture(GL_TEXTURE_2D, m_occlusionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_occlusionWidth, m_occlusionHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_occlusionData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_occlusionDirty = false;
}

void RadianceCascades::SetOcclusionFromVehementMap(const Vehement::TileMap& map) {
    int mapWidth = map.GetWidth();
    int mapHeight = map.GetHeight();
    float tileSize = map.GetTileSize();

    // Calculate occlusion texture size to match screen resolution
    m_occlusionWidth = static_cast<int>(mapWidth * tileSize);
    m_occlusionHeight = static_cast<int>(mapHeight * tileSize);

    m_occlusionData.resize(m_occlusionWidth * m_occlusionHeight);

    // Fill occlusion data from Vehement TileMap using Tile::blocksSight
    for (int y = 0; y < m_occlusionHeight; y++) {
        for (int x = 0; x < m_occlusionWidth; x++) {
            int tileX = static_cast<int>(x / tileSize);
            int tileY = static_cast<int>(y / tileSize);

            bool blocked = false;
            const Vehement::Tile* tile = map.GetTile(tileX, tileY);
            if (tile) {
                // Use blocksSight for visibility occlusion
                blocked = tile->blocksSight;
            }
            m_occlusionData[y * m_occlusionWidth + x] = blocked ? 255 : 0;
        }
    }

    // Upload to GPU
    glBindTexture(GL_TEXTURE_2D, m_occlusionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_occlusionWidth, m_occlusionHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, m_occlusionData.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_occlusionDirty = false;
    spdlog::debug("Occlusion map updated from Vehement::TileMap: {}x{} tiles, {}x{} pixels",
                  mapWidth, mapHeight, m_occlusionWidth, m_occlusionHeight);
}

void RadianceCascades::SetOcclusionData(const uint8_t* data, int width, int height) {
    m_occlusionWidth = width;
    m_occlusionHeight = height;
    m_occlusionData.assign(data, data + width * height);

    glBindTexture(GL_TEXTURE_2D, m_occlusionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_occlusionDirty = false;
}

void RadianceCascades::AddLight(glm::vec2 position, glm::vec3 color, float intensity, float radius) {
    if (m_lights.size() >= MAX_LIGHTS) {
        spdlog::warn("Maximum light count reached ({})", MAX_LIGHTS);
        return;
    }

    m_lights.emplace_back(position, color, intensity, radius);
    m_lightsDirty = true;
}

void RadianceCascades::AddLight(const RadianceLight& light) {
    AddLight(light.position, light.color, light.intensity, light.radius);
}

void RadianceCascades::SetPlayerPosition(glm::vec2 pos) {
    m_playerPosition = pos;
    m_hasPlayer = true;
}

void RadianceCascades::SetPlayerVisibilityRadius(float radius) {
    m_playerVisibilityRadius = radius;
}

void RadianceCascades::ClearLights() {
    m_lights.clear();
    m_lightsDirty = true;
}

void RadianceCascades::UploadLightData() {
    if (!m_lightsDirty) return;

    struct LightSSBOHeader {
        int numLights;
        int pad1, pad2, pad3;
    };

    struct alignas(16) LightSSBOData {
        glm::vec2 position;
        float pad0, pad1;
        glm::vec3 color;
        float intensity;
        float radius;
        float padding[3];
    };

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(LightSSBOHeader) + sizeof(LightSSBOData) * m_lights.size());

    LightSSBOHeader* header = reinterpret_cast<LightSSBOHeader*>(buffer.data());
    header->numLights = static_cast<int>(m_lights.size());
    header->pad1 = 0;
    header->pad2 = 0;
    header->pad3 = 0;

    LightSSBOData* lightData = reinterpret_cast<LightSSBOData*>(buffer.data() + sizeof(LightSSBOHeader));
    for (size_t i = 0; i < m_lights.size(); i++) {
        lightData[i].position = m_lights[i].position;
        lightData[i].color = m_lights[i].color;
        lightData[i].intensity = m_lights[i].intensity;
        lightData[i].radius = m_lights[i].radius;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, buffer.size(), buffer.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_lightsDirty = false;
}

void RadianceCascades::Update() {
    if (!m_initialized) return;

    UploadLightData();

    // Bind light SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);

    // Pass 1: Ray march for each cascade level
    DispatchRayMarch();

    // Pass 2: Merge cascades from coarse to fine
    DispatchMerge();

    // Pass 3: Compute final radiance
    DispatchFinal();

    // Unbind SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

    // Memory barrier to ensure compute shader writes are visible
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void RadianceCascades::DispatchRayMarch() {
    glUseProgram(m_rayMarchShader);

    // Bind occlusion texture
    glBindImageTexture(1, m_occlusionTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Set common uniforms
    glUniform2f(glGetUniformLocation(m_rayMarchShader, "u_Resolution"),
                static_cast<float>(m_width), static_cast<float>(m_height));
    glUniform2f(glGetUniformLocation(m_rayMarchShader, "u_OcclusionSize"),
                static_cast<float>(m_occlusionWidth), static_cast<float>(m_occlusionHeight));
    glUniform1f(glGetUniformLocation(m_rayMarchShader, "u_BiasDistance"), m_config.biasDistance);
    glUniform2f(glGetUniformLocation(m_rayMarchShader, "u_PlayerPosition"),
                m_playerPosition.x, m_playerPosition.y);
    glUniform1f(glGetUniformLocation(m_rayMarchShader, "u_PlayerRadius"), m_playerVisibilityRadius);
    glUniform1i(glGetUniformLocation(m_rayMarchShader, "u_HasPlayer"), m_hasPlayer ? 1 : 0);

    // Process each cascade level
    for (int level = m_cascadeLevels - 1; level >= 0; level--) {
        int levelWidth = std::max(1, m_width >> level);
        int levelHeight = std::max(1, m_height >> level);

        // Bind output texture
        glBindImageTexture(0, m_cascadeTextures[level], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        // Calculate interval for this cascade level
        float scaleFactor = std::pow(2.0f, static_cast<float>(level));
        float intervalLength = m_config.intervalLength * scaleFactor;
        float intervalStart = (level == m_cascadeLevels - 1) ? 0.0f : intervalLength / 2.0f;
        float intervalEnd = intervalLength;

        // Number of rays decreases with cascade level (coarser levels need fewer rays)
        int numRays = std::max(4, m_config.raysPerPixel >> level);

        glUniform1i(glGetUniformLocation(m_rayMarchShader, "u_CascadeLevel"), level);
        glUniform1i(glGetUniformLocation(m_rayMarchShader, "u_NumRays"), numRays);
        glUniform1f(glGetUniformLocation(m_rayMarchShader, "u_IntervalStart"), intervalStart);
        glUniform1f(glGetUniformLocation(m_rayMarchShader, "u_IntervalEnd"), intervalEnd);

        // Dispatch compute shader
        int groupsX = (levelWidth + 7) / 8;
        int groupsY = (levelHeight + 7) / 8;
        glDispatchCompute(groupsX, groupsY, 1);

        // Memory barrier between cascade levels
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
}

void RadianceCascades::DispatchMerge() {
    if (m_cascadeLevels <= 1) return;

    glUseProgram(m_mergeShader);

    // Merge from coarse to fine
    for (int level = m_cascadeLevels - 2; level >= 0; level--) {
        int currentWidth = std::max(1, m_width >> level);
        int currentHeight = std::max(1, m_height >> level);
        int coarseWidth = std::max(1, m_width >> (level + 1));
        int coarseHeight = std::max(1, m_height >> (level + 1));

        // Bind current cascade (read/write)
        glBindImageTexture(0, m_cascadeTextures[level], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16F);

        // Bind coarser cascade (read only)
        glBindImageTexture(1, m_cascadeTextures[level + 1], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

        glUniform1i(glGetUniformLocation(m_mergeShader, "u_CascadeLevel"), level);
        glUniform2f(glGetUniformLocation(m_mergeShader, "u_CurrentSize"),
                    static_cast<float>(currentWidth), static_cast<float>(currentHeight));
        glUniform2f(glGetUniformLocation(m_mergeShader, "u_CoarseSize"),
                    static_cast<float>(coarseWidth), static_cast<float>(coarseHeight));

        // Merge weight - controls how much coarse light propagates to fine levels
        float mergeWeight = 0.8f;
        glUniform1f(glGetUniformLocation(m_mergeShader, "u_MergeWeight"), mergeWeight);

        int groupsX = (currentWidth + 7) / 8;
        int groupsY = (currentHeight + 7) / 8;
        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
}

void RadianceCascades::DispatchFinal() {
    glUseProgram(m_radianceShader);

    // Bind output texture
    glBindImageTexture(0, m_finalRadianceTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Bind finest cascade
    glBindImageTexture(1, m_cascadeTextures[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Bind occlusion texture
    glBindImageTexture(2, m_occlusionTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);

    // Set uniforms
    glUniform2f(glGetUniformLocation(m_radianceShader, "u_Resolution"),
                static_cast<float>(m_width), static_cast<float>(m_height));
    glUniform2f(glGetUniformLocation(m_radianceShader, "u_OcclusionSize"),
                static_cast<float>(m_occlusionWidth), static_cast<float>(m_occlusionHeight));
    glUniform2f(glGetUniformLocation(m_radianceShader, "u_PlayerPosition"),
                m_playerPosition.x, m_playerPosition.y);
    glUniform1f(glGetUniformLocation(m_radianceShader, "u_PlayerRadius"), m_playerVisibilityRadius);
    glUniform1i(glGetUniformLocation(m_radianceShader, "u_HasPlayer"), m_hasPlayer ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_radianceShader, "u_AmbientLight"), 0.02f);

    int groupsX = (m_width + 7) / 8;
    int groupsY = (m_height + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glUseProgram(0);
}

uint32_t RadianceCascades::GetCascadeTexture(int level) const {
    if (level < 0 || level >= static_cast<int>(m_cascadeTextures.size())) {
        return 0;
    }
    return m_cascadeTextures[level];
}

bool RadianceCascades::IsVisible(glm::vec2 from, glm::vec2 to) const {
    return RayMarchOcclusion(from, to);
}

float RadianceCascades::GetVisibility(glm::vec2 position) const {
    if (!m_hasPlayer) return 1.0f;

    // Check if line of sight exists from player to position
    if (!RayMarchOcclusion(m_playerPosition, position)) {
        return 0.0f;
    }

    // Calculate distance-based visibility
    float dist = glm::length(position - m_playerPosition);
    if (dist >= m_playerVisibilityRadius) {
        return 0.0f;
    }

    float visibility = 1.0f - (dist / m_playerVisibilityRadius);
    return visibility * visibility;  // Quadratic falloff
}

glm::vec3 RadianceCascades::GetRadiance(glm::vec2 position) const {
    // Sample from final radiance texture (would need to read back from GPU)
    // For performance, this is typically done on GPU during rendering
    // This CPU fallback approximates radiance from light positions

    glm::vec3 radiance(0.0f);

    for (const auto& light : m_lights) {
        float dist = glm::length(light.position - position);
        if (dist < light.radius && IsVisible(light.position, position)) {
            float attenuation = 1.0f - (dist / light.radius);
            attenuation = attenuation * attenuation;
            radiance += light.color * light.intensity * attenuation;
        }
    }

    return radiance;
}

bool RadianceCascades::RayMarchOcclusion(glm::vec2 from, glm::vec2 to) const {
    if (m_occlusionData.empty()) return true;

    glm::vec2 direction = to - from;
    float distance = glm::length(direction);

    if (distance < 0.001f) return true;

    direction /= distance;

    float stepSize = 1.0f;
    float traveled = m_config.biasDistance;

    while (traveled < distance) {
        glm::vec2 pos = from + direction * traveled;

        int x = static_cast<int>(pos.x * m_occlusionWidth / m_width);
        int y = static_cast<int>(pos.y * m_occlusionHeight / m_height);

        if (SampleOcclusion(x, y) > 0.5f) {
            return false;  // Hit occluder
        }

        traveled += stepSize;
    }

    return true;  // Clear line of sight
}

float RadianceCascades::SampleOcclusion(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_occlusionWidth || y >= m_occlusionHeight) {
        return 0.0f;
    }

    return m_occlusionData[y * m_occlusionWidth + x] / 255.0f;
}

void RadianceCascades::SetConfig(const Config& config) {
    m_config = config;
}

void RadianceCascades::SetWorldToScreenTransform(const glm::mat4& worldToScreen) {
    m_worldToScreen = worldToScreen;
    m_screenToWorld = glm::inverse(worldToScreen);
}

} // namespace Vehement2
