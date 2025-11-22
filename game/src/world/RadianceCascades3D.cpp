#include "RadianceCascades3D.hpp"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Vehement2 {

// ============================================================================
// Voxel3DMap Implementation
// ============================================================================

Voxel3DMap::Voxel3DMap(int width, int height, int depth, float tileSizeXY, float tileSizeZ)
    : m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_tileSizeXY(tileSizeXY)
    , m_tileSizeZ(tileSizeZ)
{
    m_data.resize(width * height * depth, 0);
}

void Voxel3DMap::Resize(int width, int height, int depth) {
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_data.resize(width * height * depth, 0);
}

int Voxel3DMap::VoxelIndex(int x, int y, int z) const {
    return z * (m_width * m_height) + y * m_width + x;
}

bool Voxel3DMap::IsBlocked(int x, int y, int z) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return false;  // Out of bounds = not blocked
    }
    return m_data[VoxelIndex(x, y, z)] > 0;
}

void Voxel3DMap::SetBlocked(int x, int y, int z, bool blocked) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height || z < 0 || z >= m_depth) {
        return;
    }
    m_data[VoxelIndex(x, y, z)] = blocked ? 255 : 0;
}

glm::ivec3 Voxel3DMap::WorldToVoxel(glm::vec3 worldPos) const {
    return glm::ivec3(
        static_cast<int>(worldPos.x / m_tileSizeXY),
        static_cast<int>(worldPos.y / m_tileSizeXY),
        static_cast<int>(worldPos.z / m_tileSizeZ)
    );
}

glm::vec3 Voxel3DMap::VoxelToWorld(int x, int y, int z) const {
    return glm::vec3(
        (x + 0.5f) * m_tileSizeXY,
        (y + 0.5f) * m_tileSizeXY,
        (z + 0.5f) * m_tileSizeZ
    );
}

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
// RadianceCascades3D Implementation
// ============================================================================

RadianceCascades3D::RadianceCascades3D() = default;

RadianceCascades3D::~RadianceCascades3D() {
    Shutdown();
}

RadianceCascades3D::RadianceCascades3D(RadianceCascades3D&& other) noexcept
    : m_width(other.m_width)
    , m_height(other.m_height)
    , m_depth(other.m_depth)
    , m_cascadeLevels(other.m_cascadeLevels)
    , m_tileSizeXY(other.m_tileSizeXY)
    , m_tileSizeZ(other.m_tileSizeZ)
    , m_config(other.m_config)
    , m_initialized(other.m_initialized)
    , m_occlusionDirty(other.m_occlusionDirty)
    , m_lightsDirty(other.m_lightsDirty)
    , m_cascadeVolumes(std::move(other.m_cascadeVolumes))
    , m_cascadeTempVolumes(std::move(other.m_cascadeTempVolumes))
    , m_finalRadianceVolume(other.m_finalRadianceVolume)
    , m_occlusionVolume(other.m_occlusionVolume)
    , m_occlusionData(std::move(other.m_occlusionData))
    , m_occlusionWidth(other.m_occlusionWidth)
    , m_occlusionHeight(other.m_occlusionHeight)
    , m_occlusionDepth(other.m_occlusionDepth)
    , m_floorRadianceTextures(std::move(other.m_floorRadianceTextures))
    , m_lights(std::move(other.m_lights))
    , m_playerPosition(other.m_playerPosition)
    , m_playerFloor(other.m_playerFloor)
    , m_playerVisibilityRadius(other.m_playerVisibilityRadius)
    , m_hasPlayer(other.m_hasPlayer)
    , m_rayMarch3DShader(other.m_rayMarch3DShader)
    , m_merge3DShader(other.m_merge3DShader)
    , m_radiance3DShader(other.m_radiance3DShader)
    , m_extractFloorShader(other.m_extractFloorShader)
    , m_lightSSBO(other.m_lightSSBO)
    , m_paramsUBO(other.m_paramsUBO)
{
    other.m_initialized = false;
    other.m_finalRadianceVolume = 0;
    other.m_occlusionVolume = 0;
    other.m_rayMarch3DShader = 0;
    other.m_merge3DShader = 0;
    other.m_radiance3DShader = 0;
    other.m_extractFloorShader = 0;
    other.m_lightSSBO = 0;
    other.m_paramsUBO = 0;
}

RadianceCascades3D& RadianceCascades3D::operator=(RadianceCascades3D&& other) noexcept {
    if (this != &other) {
        Shutdown();

        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_cascadeLevels = other.m_cascadeLevels;
        m_tileSizeXY = other.m_tileSizeXY;
        m_tileSizeZ = other.m_tileSizeZ;
        m_config = other.m_config;
        m_initialized = other.m_initialized;
        m_occlusionDirty = other.m_occlusionDirty;
        m_lightsDirty = other.m_lightsDirty;
        m_cascadeVolumes = std::move(other.m_cascadeVolumes);
        m_cascadeTempVolumes = std::move(other.m_cascadeTempVolumes);
        m_finalRadianceVolume = other.m_finalRadianceVolume;
        m_occlusionVolume = other.m_occlusionVolume;
        m_occlusionData = std::move(other.m_occlusionData);
        m_occlusionWidth = other.m_occlusionWidth;
        m_occlusionHeight = other.m_occlusionHeight;
        m_occlusionDepth = other.m_occlusionDepth;
        m_floorRadianceTextures = std::move(other.m_floorRadianceTextures);
        m_lights = std::move(other.m_lights);
        m_playerPosition = other.m_playerPosition;
        m_playerFloor = other.m_playerFloor;
        m_playerVisibilityRadius = other.m_playerVisibilityRadius;
        m_hasPlayer = other.m_hasPlayer;
        m_rayMarch3DShader = other.m_rayMarch3DShader;
        m_merge3DShader = other.m_merge3DShader;
        m_radiance3DShader = other.m_radiance3DShader;
        m_extractFloorShader = other.m_extractFloorShader;
        m_lightSSBO = other.m_lightSSBO;
        m_paramsUBO = other.m_paramsUBO;

        other.m_initialized = false;
        other.m_finalRadianceVolume = 0;
        other.m_occlusionVolume = 0;
        other.m_rayMarch3DShader = 0;
        other.m_merge3DShader = 0;
        other.m_radiance3DShader = 0;
        other.m_extractFloorShader = 0;
        other.m_lightSSBO = 0;
        other.m_paramsUBO = 0;
    }
    return *this;
}

bool RadianceCascades3D::Initialize(int width, int height, int depth, int cascadeLevels) {
    if (m_initialized) {
        spdlog::warn("RadianceCascades3D already initialized, shutting down first");
        Shutdown();
    }

    m_width = width;
    m_height = height;
    m_depth = depth;
    m_cascadeLevels = std::max(1, std::min(cascadeLevels, 6));  // Max 6 levels for 3D

    spdlog::info("Initializing RadianceCascades3D: {}x{}x{}, {} levels", width, height, depth, m_cascadeLevels);

    if (!CreateShaders()) {
        spdlog::error("Failed to create 3D radiance cascade shaders");
        return false;
    }

    if (!CreateVolumes()) {
        spdlog::error("Failed to create 3D radiance cascade volumes");
        DestroyResources();
        return false;
    }

    if (!CreateBuffers()) {
        spdlog::error("Failed to create 3D radiance cascade buffers");
        DestroyResources();
        return false;
    }

    if (!CreateFloorTextures()) {
        spdlog::error("Failed to create floor textures");
        DestroyResources();
        return false;
    }

    m_initialized = true;
    spdlog::info("RadianceCascades3D initialized successfully");
    return true;
}

void RadianceCascades3D::Shutdown() {
    if (!m_initialized) return;

    spdlog::info("Shutting down RadianceCascades3D");
    DestroyResources();
    m_initialized = false;
}

void RadianceCascades3D::Resize(int width, int height, int depth) {
    if (width == m_width && height == m_height && depth == m_depth) return;

    m_width = width;
    m_height = height;
    m_depth = depth;

    // Recreate volumes with new size
    if (!m_cascadeVolumes.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeVolumes.size()), m_cascadeVolumes.data());
        m_cascadeVolumes.clear();
    }
    if (!m_cascadeTempVolumes.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTempVolumes.size()), m_cascadeTempVolumes.data());
        m_cascadeTempVolumes.clear();
    }
    if (m_finalRadianceVolume) {
        glDeleteTextures(1, &m_finalRadianceVolume);
        m_finalRadianceVolume = 0;
    }
    if (!m_floorRadianceTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_floorRadianceTextures.size()), m_floorRadianceTextures.data());
        m_floorRadianceTextures.clear();
    }

    CreateVolumes();
    CreateFloorTextures();
}

bool RadianceCascades3D::CreateShaders() {
    // Load shaders from files
    std::string rayMarchSource = LoadShaderFile("game/assets/shaders/radiance3d_raymarch.comp");
    std::string mergeSource = LoadShaderFile("game/assets/shaders/radiance3d_merge.comp");
    std::string finalSource = LoadShaderFile("game/assets/shaders/radiance3d_final.comp");

    if (rayMarchSource.empty() || mergeSource.empty() || finalSource.empty()) {
        spdlog::error("Failed to load 3D radiance cascade shader files");
        return false;
    }

    m_rayMarch3DShader = CompileComputeShader(rayMarchSource);
    if (m_rayMarch3DShader == 0) {
        spdlog::error("Failed to compile 3D ray march shader");
        return false;
    }

    m_merge3DShader = CompileComputeShader(mergeSource);
    if (m_merge3DShader == 0) {
        spdlog::error("Failed to compile 3D merge shader");
        return false;
    }

    m_radiance3DShader = CompileComputeShader(finalSource);
    if (m_radiance3DShader == 0) {
        spdlog::error("Failed to compile 3D radiance shader");
        return false;
    }

    // Create floor extraction shader inline
    static const char* EXTRACT_FLOOR_SHADER = R"(
#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba16f, binding = 0) uniform writeonly image2D u_FloorOutput;
layout(rgba16f, binding = 1) uniform readonly image3D u_RadianceVolume;

uniform int u_FloorLevel;
uniform ivec2 u_OutputSize;

void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);

    if (pixelCoord.x >= u_OutputSize.x || pixelCoord.y >= u_OutputSize.y) {
        return;
    }

    // Sample from 3D volume at the specified floor level
    ivec3 volumeCoord = ivec3(pixelCoord, u_FloorLevel);
    vec4 radiance = imageLoad(u_RadianceVolume, volumeCoord);

    imageStore(u_FloorOutput, pixelCoord, radiance);
}
)";

    m_extractFloorShader = CompileComputeShader(EXTRACT_FLOOR_SHADER);
    if (m_extractFloorShader == 0) {
        spdlog::error("Failed to compile floor extraction shader");
        return false;
    }

    return true;
}

bool RadianceCascades3D::CreateVolumes() {
    // Create cascade volumes at decreasing resolutions
    m_cascadeVolumes.resize(m_cascadeLevels);
    m_cascadeTempVolumes.resize(m_cascadeLevels);

    glGenTextures(m_cascadeLevels, m_cascadeVolumes.data());
    glGenTextures(m_cascadeLevels, m_cascadeTempVolumes.data());

    for (int level = 0; level < m_cascadeLevels; level++) {
        int levelWidth = std::max(1, m_width >> level);
        int levelHeight = std::max(1, m_height >> level);
        int levelDepth = std::max(1, m_depth >> level);

        // Main cascade volume
        glBindTexture(GL_TEXTURE_3D, m_cascadeVolumes[level]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, levelWidth, levelHeight, levelDepth,
                     0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Temp volume for ping-pong
        glBindTexture(GL_TEXTURE_3D, m_cascadeTempVolumes[level]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, levelWidth, levelHeight, levelDepth,
                     0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    // Create final radiance volume at full resolution
    glGenTextures(1, &m_finalRadianceVolume);
    glBindTexture(GL_TEXTURE_3D, m_finalRadianceVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, m_width, m_height, m_depth,
                 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Create occlusion volume
    glGenTextures(1, &m_occlusionVolume);
    glBindTexture(GL_TEXTURE_3D, m_occlusionVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, m_width, m_height, m_depth,
                 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_3D, 0);

    return true;
}

bool RadianceCascades3D::CreateBuffers() {
    // Create SSBO for light data
    glGenBuffers(1, &m_lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);

    // Allocate space for header + max lights
    struct LightSSBOHeader {
        int numLights;
        int playerFloor;
        int pad2, pad3;
    };

    struct alignas(16) LightSSBOData {
        glm::vec3 position;
        float intensity;
        glm::vec3 color;
        float radius;
        int floorLevel;
        int pad1, pad2, pad3;
    };

    size_t bufferSize = sizeof(LightSSBOHeader) + sizeof(LightSSBOData) * MAX_LIGHTS;
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return true;
}

bool RadianceCascades3D::CreateFloorTextures() {
    m_floorRadianceTextures.resize(m_depth);
    glGenTextures(m_depth, m_floorRadianceTextures.data());

    for (int z = 0; z < m_depth; z++) {
        glBindTexture(GL_TEXTURE_2D, m_floorRadianceTextures[z]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void RadianceCascades3D::DestroyResources() {
    if (!m_cascadeVolumes.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeVolumes.size()), m_cascadeVolumes.data());
        m_cascadeVolumes.clear();
    }

    if (!m_cascadeTempVolumes.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_cascadeTempVolumes.size()), m_cascadeTempVolumes.data());
        m_cascadeTempVolumes.clear();
    }

    if (m_finalRadianceVolume) {
        glDeleteTextures(1, &m_finalRadianceVolume);
        m_finalRadianceVolume = 0;
    }

    if (m_occlusionVolume) {
        glDeleteTextures(1, &m_occlusionVolume);
        m_occlusionVolume = 0;
    }

    if (!m_floorRadianceTextures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(m_floorRadianceTextures.size()), m_floorRadianceTextures.data());
        m_floorRadianceTextures.clear();
    }

    if (m_rayMarch3DShader) {
        glDeleteProgram(m_rayMarch3DShader);
        m_rayMarch3DShader = 0;
    }

    if (m_merge3DShader) {
        glDeleteProgram(m_merge3DShader);
        m_merge3DShader = 0;
    }

    if (m_radiance3DShader) {
        glDeleteProgram(m_radiance3DShader);
        m_radiance3DShader = 0;
    }

    if (m_extractFloorShader) {
        glDeleteProgram(m_extractFloorShader);
        m_extractFloorShader = 0;
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

void RadianceCascades3D::UpdateOcclusionVolume(const Voxel3DMap& map) {
    m_occlusionWidth = map.GetWidth();
    m_occlusionHeight = map.GetHeight();
    m_occlusionDepth = map.GetDepth();

    m_occlusionData = map.GetData();

    // Upload to GPU
    glBindTexture(GL_TEXTURE_3D, m_occlusionVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, m_occlusionWidth, m_occlusionHeight, m_occlusionDepth,
                 0, GL_RED, GL_UNSIGNED_BYTE, m_occlusionData.data());
    glBindTexture(GL_TEXTURE_3D, 0);

    m_occlusionDirty = false;
    spdlog::debug("Occlusion volume updated: {}x{}x{}", m_occlusionWidth, m_occlusionHeight, m_occlusionDepth);
}

void RadianceCascades3D::UpdateOcclusionVolume(const IVoxelOcclusionProvider& provider) {
    m_occlusionWidth = provider.GetWidth();
    m_occlusionHeight = provider.GetHeight();
    m_occlusionDepth = provider.GetDepth();

    m_occlusionData.resize(m_occlusionWidth * m_occlusionHeight * m_occlusionDepth);

    // Fill from provider
    for (int z = 0; z < m_occlusionDepth; z++) {
        for (int y = 0; y < m_occlusionHeight; y++) {
            for (int x = 0; x < m_occlusionWidth; x++) {
                int idx = z * (m_occlusionWidth * m_occlusionHeight) + y * m_occlusionWidth + x;
                m_occlusionData[idx] = provider.IsBlocked(x, y, z) ? 255 : 0;
            }
        }
    }

    // Upload to GPU
    glBindTexture(GL_TEXTURE_3D, m_occlusionVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, m_occlusionWidth, m_occlusionHeight, m_occlusionDepth,
                 0, GL_RED, GL_UNSIGNED_BYTE, m_occlusionData.data());
    glBindTexture(GL_TEXTURE_3D, 0);

    m_occlusionDirty = false;
}

void RadianceCascades3D::SetOcclusionData(const uint8_t* data, int width, int height, int depth) {
    m_occlusionWidth = width;
    m_occlusionHeight = height;
    m_occlusionDepth = depth;
    m_occlusionData.assign(data, data + width * height * depth);

    glBindTexture(GL_TEXTURE_3D, m_occlusionVolume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, width, height, depth,
                 0, GL_RED, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_3D, 0);

    m_occlusionDirty = false;
}

void RadianceCascades3D::AddLight(glm::vec3 position, glm::vec3 color, float intensity, float radius) {
    if (m_lights.size() >= MAX_LIGHTS) {
        spdlog::warn("Maximum light count reached ({})", MAX_LIGHTS);
        return;
    }

    RadianceLight3D light;
    light.position = position;
    light.color = color;
    light.intensity = intensity;
    light.radius = radius;
    light.floorLevel = static_cast<int>(position.z / m_tileSizeZ);

    m_lights.push_back(light);
    m_lightsDirty = true;
}

void RadianceCascades3D::AddLight(const RadianceLight3D& light) {
    if (m_lights.size() >= MAX_LIGHTS) {
        spdlog::warn("Maximum light count reached ({})", MAX_LIGHTS);
        return;
    }

    m_lights.push_back(light);
    if (m_lights.back().floorLevel < 0) {
        m_lights.back().floorLevel = static_cast<int>(light.position.z / m_tileSizeZ);
    }
    m_lightsDirty = true;
}

void RadianceCascades3D::ClearLights() {
    m_lights.clear();
    m_lightsDirty = true;
}

void RadianceCascades3D::SetPlayerPosition(glm::vec3 pos) {
    m_playerPosition = pos;
    m_playerFloor = static_cast<int>(pos.z / m_tileSizeZ);
    m_hasPlayer = true;
}

void RadianceCascades3D::SetPlayerFloor(int zLevel) {
    m_playerFloor = std::clamp(zLevel, 0, m_depth - 1);
}

void RadianceCascades3D::SetPlayerVisibilityRadius(float radius) {
    m_playerVisibilityRadius = radius;
}

void RadianceCascades3D::UploadLightData() {
    if (!m_lightsDirty) return;

    struct LightSSBOHeader {
        int numLights;
        int playerFloor;
        int pad2, pad3;
    };

    struct alignas(16) LightSSBOData {
        glm::vec3 position;
        float intensity;
        glm::vec3 color;
        float radius;
        int floorLevel;
        int pad1, pad2, pad3;
    };

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(LightSSBOHeader) + sizeof(LightSSBOData) * m_lights.size());

    LightSSBOHeader* header = reinterpret_cast<LightSSBOHeader*>(buffer.data());
    header->numLights = static_cast<int>(m_lights.size());
    header->playerFloor = m_playerFloor;
    header->pad2 = 0;
    header->pad3 = 0;

    LightSSBOData* lightData = reinterpret_cast<LightSSBOData*>(buffer.data() + sizeof(LightSSBOHeader));
    for (size_t i = 0; i < m_lights.size(); i++) {
        lightData[i].position = m_lights[i].position;
        lightData[i].intensity = m_lights[i].intensity;
        lightData[i].color = m_lights[i].color;
        lightData[i].radius = m_lights[i].radius;
        lightData[i].floorLevel = m_lights[i].floorLevel;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, buffer.size(), buffer.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_lightsDirty = false;
}

void RadianceCascades3D::Update() {
    if (!m_initialized) return;

    UploadLightData();

    // Bind light SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);

    // Pass 1: 3D Ray march for each cascade level
    DispatchRayMarch3D();

    // Pass 2: Merge 3D cascades from coarse to fine
    DispatchMerge3D();

    // Pass 3: Compute final 3D radiance
    DispatchFinal3D();

    // Pass 4: Extract per-floor 2D textures
    ExtractFloorTextures();

    // Unbind SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

    // Memory barrier
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void RadianceCascades3D::DispatchRayMarch3D() {
    glUseProgram(m_rayMarch3DShader);

    // Bind occlusion volume
    glBindImageTexture(1, m_occlusionVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8);

    // Set common uniforms
    glUniform3f(glGetUniformLocation(m_rayMarch3DShader, "u_VolumeSize"),
                static_cast<float>(m_width), static_cast<float>(m_height), static_cast<float>(m_depth));
    glUniform3f(glGetUniformLocation(m_rayMarch3DShader, "u_OcclusionSize"),
                static_cast<float>(m_occlusionWidth), static_cast<float>(m_occlusionHeight),
                static_cast<float>(m_occlusionDepth));
    glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_BiasDistance"), m_config.biasDistance);
    glUniform3f(glGetUniformLocation(m_rayMarch3DShader, "u_PlayerPosition"),
                m_playerPosition.x, m_playerPosition.y, m_playerPosition.z);
    glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_PlayerRadius"), m_playerVisibilityRadius);
    glUniform1i(glGetUniformLocation(m_rayMarch3DShader, "u_HasPlayer"), m_hasPlayer ? 1 : 0);
    glUniform1i(glGetUniformLocation(m_rayMarch3DShader, "u_PlayerFloor"), m_playerFloor);
    glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_ZScale"), m_config.zScale);
    glUniform1i(glGetUniformLocation(m_rayMarch3DShader, "u_EnableVerticalLight"), m_config.enableVerticalLight ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_VerticalFalloff"), m_config.verticalFalloff);

    // Process each cascade level
    for (int level = m_cascadeLevels - 1; level >= 0; level--) {
        int levelWidth = std::max(1, m_width >> level);
        int levelHeight = std::max(1, m_height >> level);
        int levelDepth = std::max(1, m_depth >> level);

        // Bind output volume
        glBindImageTexture(0, m_cascadeVolumes[level], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        // Calculate interval for this cascade level
        float scaleFactor = std::pow(2.0f, static_cast<float>(level));
        float intervalLength = m_config.intervalLength * scaleFactor;
        float intervalStart = (level == m_cascadeLevels - 1) ? 0.0f : intervalLength / 2.0f;
        float intervalEnd = intervalLength;

        int numRays = std::max(8, m_config.raysPerVoxel >> level);

        glUniform1i(glGetUniformLocation(m_rayMarch3DShader, "u_CascadeLevel"), level);
        glUniform1i(glGetUniformLocation(m_rayMarch3DShader, "u_NumRays"), numRays);
        glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_IntervalStart"), intervalStart);
        glUniform1f(glGetUniformLocation(m_rayMarch3DShader, "u_IntervalEnd"), intervalEnd);

        // Dispatch compute shader (8x8x4 workgroups)
        int groupsX = (levelWidth + 7) / 8;
        int groupsY = (levelHeight + 7) / 8;
        int groupsZ = (levelDepth + 3) / 4;
        glDispatchCompute(groupsX, groupsY, groupsZ);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
}

void RadianceCascades3D::DispatchMerge3D() {
    if (m_cascadeLevels <= 1) return;

    glUseProgram(m_merge3DShader);

    // Merge from coarse to fine
    for (int level = m_cascadeLevels - 2; level >= 0; level--) {
        int currentWidth = std::max(1, m_width >> level);
        int currentHeight = std::max(1, m_height >> level);
        int currentDepth = std::max(1, m_depth >> level);
        int coarseWidth = std::max(1, m_width >> (level + 1));
        int coarseHeight = std::max(1, m_height >> (level + 1));
        int coarseDepth = std::max(1, m_depth >> (level + 1));

        // Bind current cascade (read/write)
        glBindImageTexture(0, m_cascadeVolumes[level], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);

        // Bind coarser cascade (read only)
        glBindImageTexture(1, m_cascadeVolumes[level + 1], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

        glUniform1i(glGetUniformLocation(m_merge3DShader, "u_CascadeLevel"), level);
        glUniform3f(glGetUniformLocation(m_merge3DShader, "u_CurrentSize"),
                    static_cast<float>(currentWidth), static_cast<float>(currentHeight),
                    static_cast<float>(currentDepth));
        glUniform3f(glGetUniformLocation(m_merge3DShader, "u_CoarseSize"),
                    static_cast<float>(coarseWidth), static_cast<float>(coarseHeight),
                    static_cast<float>(coarseDepth));
        glUniform1f(glGetUniformLocation(m_merge3DShader, "u_MergeWeight"), 0.8f);
        glUniform1f(glGetUniformLocation(m_merge3DShader, "u_VerticalFalloff"), m_config.verticalFalloff);

        int groupsX = (currentWidth + 7) / 8;
        int groupsY = (currentHeight + 7) / 8;
        int groupsZ = (currentDepth + 3) / 4;
        glDispatchCompute(groupsX, groupsY, groupsZ);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
}

void RadianceCascades3D::DispatchFinal3D() {
    glUseProgram(m_radiance3DShader);

    // Bind output volume
    glBindImageTexture(0, m_finalRadianceVolume, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    // Bind finest cascade
    glBindImageTexture(1, m_cascadeVolumes[0], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

    // Bind occlusion volume
    glBindImageTexture(2, m_occlusionVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R8);

    // Set uniforms
    glUniform3f(glGetUniformLocation(m_radiance3DShader, "u_VolumeSize"),
                static_cast<float>(m_width), static_cast<float>(m_height), static_cast<float>(m_depth));
    glUniform3f(glGetUniformLocation(m_radiance3DShader, "u_OcclusionSize"),
                static_cast<float>(m_occlusionWidth), static_cast<float>(m_occlusionHeight),
                static_cast<float>(m_occlusionDepth));
    glUniform3f(glGetUniformLocation(m_radiance3DShader, "u_PlayerPosition"),
                m_playerPosition.x, m_playerPosition.y, m_playerPosition.z);
    glUniform1f(glGetUniformLocation(m_radiance3DShader, "u_PlayerRadius"), m_playerVisibilityRadius);
    glUniform1i(glGetUniformLocation(m_radiance3DShader, "u_HasPlayer"), m_hasPlayer ? 1 : 0);
    glUniform1i(glGetUniformLocation(m_radiance3DShader, "u_PlayerFloor"), m_playerFloor);
    glUniform1f(glGetUniformLocation(m_radiance3DShader, "u_AmbientLight"), 0.02f);

    int groupsX = (m_width + 7) / 8;
    int groupsY = (m_height + 7) / 8;
    int groupsZ = (m_depth + 3) / 4;
    glDispatchCompute(groupsX, groupsY, groupsZ);

    glUseProgram(0);
}

void RadianceCascades3D::ExtractFloorTextures() {
    glUseProgram(m_extractFloorShader);

    glBindImageTexture(1, m_finalRadianceVolume, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA16F);

    glUniform2i(glGetUniformLocation(m_extractFloorShader, "u_OutputSize"), m_width, m_height);

    for (int z = 0; z < m_depth && z < static_cast<int>(m_floorRadianceTextures.size()); z++) {
        glBindImageTexture(0, m_floorRadianceTextures[z], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        glUniform1i(glGetUniformLocation(m_extractFloorShader, "u_FloorLevel"), z);

        int groupsX = (m_width + 7) / 8;
        int groupsY = (m_height + 7) / 8;
        glDispatchCompute(groupsX, groupsY, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    glUseProgram(0);
}

uint32_t RadianceCascades3D::GetRadianceTextureForLevel(int zLevel) const {
    if (zLevel < 0 || zLevel >= static_cast<int>(m_floorRadianceTextures.size())) {
        return 0;
    }
    return m_floorRadianceTextures[zLevel];
}

uint32_t RadianceCascades3D::GetCascadeVolume(int level) const {
    if (level < 0 || level >= static_cast<int>(m_cascadeVolumes.size())) {
        return 0;
    }
    return m_cascadeVolumes[level];
}

glm::ivec3 RadianceCascades3D::WorldToVoxel(glm::vec3 worldPos) const {
    return glm::ivec3(
        static_cast<int>(worldPos.x / m_tileSizeXY),
        static_cast<int>(worldPos.y / m_tileSizeXY),
        static_cast<int>(worldPos.z / m_tileSizeZ)
    );
}

bool RadianceCascades3D::IsVisible(glm::vec3 from, glm::vec3 to) const {
    return RayMarch3DOcclusion(from, to);
}

float RadianceCascades3D::GetVisibility(glm::vec3 position) const {
    if (!m_hasPlayer) return 1.0f;

    // Check if line of sight exists from player to position
    if (!RayMarch3DOcclusion(m_playerPosition, position)) {
        return 0.0f;
    }

    // Calculate 3D distance-based visibility
    float dist = glm::length(position - m_playerPosition);
    if (dist >= m_playerVisibilityRadius) {
        return 0.0f;
    }

    float visibility = 1.0f - (dist / m_playerVisibilityRadius);
    return visibility * visibility;  // Quadratic falloff
}

bool RadianceCascades3D::HasVerticalLineOfSight(glm::vec3 from, glm::vec3 to) const {
    if (m_occlusionData.empty()) return true;

    // March vertically between the two points
    glm::ivec3 fromVoxel = WorldToVoxel(from);
    glm::ivec3 toVoxel = WorldToVoxel(to);

    int minZ = std::min(fromVoxel.z, toVoxel.z);
    int maxZ = std::max(fromVoxel.z, toVoxel.z);

    // Check same XY position on all floors between
    for (int z = minZ; z <= maxZ; z++) {
        if (SampleOcclusion3D(fromVoxel.x, fromVoxel.y, z) > 0.5f) {
            return false;
        }
    }

    return true;
}

glm::vec3 RadianceCascades3D::GetRadiance(glm::vec3 position) const {
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

bool RadianceCascades3D::RayMarch3DOcclusion(glm::vec3 from, glm::vec3 to) const {
    if (m_occlusionData.empty()) return true;

    glm::vec3 direction = to - from;
    float distance = glm::length(direction);

    if (distance < 0.001f) return true;

    direction /= distance;

    float stepSize = std::min(m_tileSizeXY, m_tileSizeZ) * 0.5f;
    float traveled = m_config.biasDistance;

    while (traveled < distance) {
        glm::vec3 pos = from + direction * traveled;
        glm::ivec3 voxel = WorldToVoxel(pos);

        if (SampleOcclusion3D(voxel.x, voxel.y, voxel.z) > 0.5f) {
            return false;  // Hit occluder
        }

        traveled += stepSize;
    }

    return true;  // Clear line of sight
}

float RadianceCascades3D::SampleOcclusion3D(int x, int y, int z) const {
    if (x < 0 || y < 0 || z < 0 ||
        x >= m_occlusionWidth || y >= m_occlusionHeight || z >= m_occlusionDepth) {
        return 0.0f;
    }

    int idx = z * (m_occlusionWidth * m_occlusionHeight) + y * m_occlusionWidth + x;
    return m_occlusionData[idx] / 255.0f;
}

void RadianceCascades3D::SetConfig(const Config& config) {
    m_config = config;
}

void RadianceCascades3D::SetTileSizes(float tileSizeXY, float tileSizeZ) {
    m_tileSizeXY = tileSizeXY;
    m_tileSizeZ = tileSizeZ;
}

} // namespace Vehement2
