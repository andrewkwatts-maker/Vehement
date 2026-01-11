#include "LightProbeSystem.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "RadianceCascade.hpp"
#include "Camera.hpp"
#include "debug/DebugDraw.hpp"

#include <glad/gl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>

namespace Nova {

// ============================================================================
// SH Constants
// ============================================================================

// L0 band constant: Y_0^0 = 0.5 * sqrt(1/pi)
constexpr float SH_Y0 = 0.282095f;

// L1 band constants: Y_1^{-1,0,1} = 0.5 * sqrt(3/pi) * {y, z, x}
constexpr float SH_Y1 = 0.488603f;

// L2 band constants
constexpr float SH_Y2_0 = 1.092548f;   // Y_2^{-2} = 0.5 * sqrt(15/pi) * xy
constexpr float SH_Y2_1 = 1.092548f;   // Y_2^{-1} = 0.5 * sqrt(15/pi) * yz
constexpr float SH_Y2_2 = 0.315392f;   // Y_2^0 = 0.25 * sqrt(5/pi) * (3z^2 - 1)
constexpr float SH_Y2_3 = 1.092548f;   // Y_2^1 = 0.5 * sqrt(15/pi) * xz
constexpr float SH_Y2_4 = 0.546274f;   // Y_2^2 = 0.25 * sqrt(15/pi) * (x^2 - y^2)

// Cosine lobe convolution factors (A_l coefficients)
constexpr float A0 = glm::pi<float>();
constexpr float A1 = 2.0f * glm::pi<float>() / 3.0f;
constexpr float A2 = glm::pi<float>() / 4.0f;

// ============================================================================
// LightProbeSystem Implementation
// ============================================================================

LightProbeSystem::LightProbeSystem() = default;

LightProbeSystem::~LightProbeSystem() {
    Shutdown();
}

LightProbeSystem::LightProbeSystem(LightProbeSystem&& other) noexcept
    : m_initialized(other.m_initialized)
    , m_config(std::move(other.m_config))
    , m_probes(std::move(other.m_probes))
    , m_gridDimensions(other.m_gridDimensions)
    , m_gridOrigin(other.m_gridOrigin)
    , m_invSpacing(other.m_invSpacing)
    , m_probeSSBO(other.m_probeSSBO)
    , m_gridInfoTexture(other.m_gridInfoTexture)
    , m_updateComputeSSBO(other.m_updateComputeSSBO)
    , m_gpuDataDirty(other.m_gpuDataDirty)
    , m_probeUpdateShader(std::move(other.m_probeUpdateShader))
    , m_probeSampleShader(std::move(other.m_probeSampleShader))
    , m_debugVisualizationShader(std::move(other.m_debugVisualizationShader))
    , m_radianceCascade(std::move(other.m_radianceCascade))
    , m_raycastFunc(std::move(other.m_raycastFunc))
    , m_debugView(other.m_debugView)
    , m_stats(other.m_stats)
    , m_frameCount(other.m_frameCount) {
    other.m_initialized = false;
    other.m_probeSSBO = 0;
    other.m_gridInfoTexture = 0;
    other.m_updateComputeSSBO = 0;
}

LightProbeSystem& LightProbeSystem::operator=(LightProbeSystem&& other) noexcept {
    if (this != &other) {
        Shutdown();
        m_initialized = other.m_initialized;
        m_config = std::move(other.m_config);
        m_probes = std::move(other.m_probes);
        m_gridDimensions = other.m_gridDimensions;
        m_gridOrigin = other.m_gridOrigin;
        m_invSpacing = other.m_invSpacing;
        m_probeSSBO = other.m_probeSSBO;
        m_gridInfoTexture = other.m_gridInfoTexture;
        m_updateComputeSSBO = other.m_updateComputeSSBO;
        m_gpuDataDirty = other.m_gpuDataDirty;
        m_probeUpdateShader = std::move(other.m_probeUpdateShader);
        m_probeSampleShader = std::move(other.m_probeSampleShader);
        m_debugVisualizationShader = std::move(other.m_debugVisualizationShader);
        m_radianceCascade = std::move(other.m_radianceCascade);
        m_raycastFunc = std::move(other.m_raycastFunc);
        m_debugView = other.m_debugView;
        m_stats = other.m_stats;
        m_frameCount = other.m_frameCount;

        other.m_initialized = false;
        other.m_probeSSBO = 0;
        other.m_gridInfoTexture = 0;
        other.m_updateComputeSSBO = 0;
    }
    return *this;
}

// ============================================================================
// Initialization
// ============================================================================

bool LightProbeSystem::Initialize(const ProbeGridConfig& config) {
    if (m_initialized) {
        spdlog::warn("LightProbeSystem already initialized");
        return true;
    }

    spdlog::info("Initializing LightProbeSystem");
    m_config = config;

    // Initialize GPU resources
    if (!InitializeBuffers()) {
        spdlog::error("Failed to initialize LightProbeSystem buffers");
        return false;
    }

    if (!InitializeShaders()) {
        spdlog::warn("Failed to load LightProbeSystem shaders (continuing without GPU acceleration)");
    }

    m_initialized = true;
    spdlog::info("LightProbeSystem initialized successfully");
    return true;
}

void LightProbeSystem::Shutdown() {
    if (!m_initialized) {
        return;
    }

    spdlog::info("Shutting down LightProbeSystem");
    CleanupResources();
    m_probes.clear();
    m_initialized = false;
}

bool LightProbeSystem::Reinitialize(const ProbeGridConfig& config) {
    Shutdown();
    return Initialize(config);
}

bool LightProbeSystem::InitializeBuffers() {
    // Create probe SSBO
    glGenBuffers(1, &m_probeSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_probeSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPULightProbe) * 8192, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create grid info texture (3D texture storing probe indices)
    glGenTextures(1, &m_gridInfoTexture);
    glBindTexture(GL_TEXTURE_3D, m_gridInfoTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_3D, 0);

    // Create compute update SSBO
    glGenBuffers(1, &m_updateComputeSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_updateComputeSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int) * 1024, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return glGetError() == GL_NO_ERROR;
}

bool LightProbeSystem::InitializeShaders() {
    // Compute shader for probe updates
    const char* probeUpdateSource = R"(
#version 450 core
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct GPULightProbe {
    vec4 positionAndValidity;
    vec4 sh0;
    vec4 sh1_r;
    vec4 sh1_g;
    vec4 sh1_b;
    vec4 sh2_rg;
    vec4 sh2_b_occlusion;
};

layout(std430, binding = 0) buffer ProbeBuffer {
    GPULightProbe probes[];
};

layout(std430, binding = 1) buffer UpdateIndices {
    int indicesToUpdate[];
};

uniform int u_numProbes;
uniform int u_raysPerProbe;
uniform float u_maxDistance;
uniform float u_temporalBlend;
uniform uint u_frameIndex;

// Pseudo-random number generation
uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

vec2 randomVec2(uint seed) {
    uint x = pcg(seed);
    uint y = pcg(x);
    return vec2(float(x) / 4294967295.0, float(y) / 4294967295.0);
}

vec3 fibonacciSphere(int i, int n) {
    float phi = float(i) * 2.399963229728653; // Golden angle
    float y = 1.0 - float(i) / float(n - 1) * 2.0;
    float radius = sqrt(1.0 - y * y);
    return vec3(cos(phi) * radius, y, sin(phi) * radius);
}

// SH basis evaluation for L2
void evaluateSHBasis(vec3 dir, out float basis[9]) {
    // L0
    basis[0] = 0.282095;

    // L1
    basis[1] = 0.488603 * dir.y;
    basis[2] = 0.488603 * dir.z;
    basis[3] = 0.488603 * dir.x;

    // L2
    basis[4] = 1.092548 * dir.x * dir.y;
    basis[5] = 1.092548 * dir.y * dir.z;
    basis[6] = 0.315392 * (3.0 * dir.z * dir.z - 1.0);
    basis[7] = 1.092548 * dir.x * dir.z;
    basis[8] = 0.546274 * (dir.x * dir.x - dir.y * dir.y);
}

void main() {
    uint probeIdx = gl_GlobalInvocationID.x;
    if (probeIdx >= u_numProbes) return;

    int actualIndex = indicesToUpdate[probeIdx];
    if (actualIndex < 0) return;

    GPULightProbe probe = probes[actualIndex];
    vec3 probePos = probe.positionAndValidity.xyz;

    // Accumulate radiance samples into SH
    vec3 shAccum[9];
    for (int i = 0; i < 9; i++) {
        shAccum[i] = vec3(0.0);
    }

    float weight = 4.0 * 3.14159265 / float(u_raysPerProbe);

    for (int ray = 0; ray < u_raysPerProbe; ray++) {
        // Generate sample direction
        uint seed = uint(actualIndex) * 65537u + uint(ray) * 32768u + u_frameIndex;
        vec2 jitter = randomVec2(seed) * 0.5;
        vec3 dir = fibonacciSphere(ray, u_raysPerProbe);

        // TODO: Trace ray and get radiance
        // For now, use placeholder sky color
        vec3 radiance = mix(vec3(0.8, 0.9, 1.0), vec3(0.2, 0.4, 0.8), dir.y * 0.5 + 0.5);

        // Project to SH
        float basis[9];
        evaluateSHBasis(dir, basis);

        for (int b = 0; b < 9; b++) {
            shAccum[b] += radiance * basis[b] * weight;
        }
    }

    // Temporal blend with previous values
    float blend = u_temporalBlend;

    // Pack back to GPU format
    probe.sh0.rgb = mix(shAccum[0], probe.sh0.rgb, blend);
    probe.sh1_r = vec4(mix(vec3(shAccum[1].r, shAccum[2].r, shAccum[3].r), probe.sh1_r.xyz, blend),
                       mix(shAccum[4].r, probe.sh1_r.w, blend));
    probe.sh1_g = vec4(mix(vec3(shAccum[1].g, shAccum[2].g, shAccum[3].g), probe.sh1_g.xyz, blend),
                       mix(shAccum[4].g, probe.sh1_g.w, blend));
    probe.sh1_b = vec4(mix(vec3(shAccum[1].b, shAccum[2].b, shAccum[3].b), probe.sh1_b.xyz, blend),
                       mix(shAccum[4].b, probe.sh1_b.w, blend));
    probe.sh2_rg = vec4(mix(vec2(shAccum[5].r, shAccum[6].r), probe.sh2_rg.xy, blend),
                        mix(vec2(shAccum[5].g, shAccum[6].g), probe.sh2_rg.zw, blend));
    probe.sh2_b_occlusion = vec4(mix(vec2(shAccum[5].b, shAccum[6].b), probe.sh2_b_occlusion.xy, blend),
                                  mix(vec2(shAccum[7].r, shAccum[8].r), probe.sh2_b_occlusion.zw, blend));

    probe.positionAndValidity.w = 1.0; // Mark as valid

    probes[actualIndex] = probe;
}
)";

    m_probeUpdateShader = std::make_unique<Shader>();
    // Note: Compute shader loading would need a specialized path
    // For now, we'll use CPU update fallback

    return true;
}

void LightProbeSystem::CleanupResources() {
    if (m_probeSSBO) {
        glDeleteBuffers(1, &m_probeSSBO);
        m_probeSSBO = 0;
    }

    if (m_gridInfoTexture) {
        glDeleteTextures(1, &m_gridInfoTexture);
        m_gridInfoTexture = 0;
    }

    if (m_updateComputeSSBO) {
        glDeleteBuffers(1, &m_updateComputeSSBO);
        m_updateComputeSSBO = 0;
    }

    m_probeUpdateShader.reset();
    m_probeSampleShader.reset();
    m_debugVisualizationShader.reset();
}

// ============================================================================
// Probe Placement
// ============================================================================

int LightProbeSystem::PlaceProbes(const AABB& bounds, const glm::vec3& spacing) {
    if (!m_initialized) {
        spdlog::error("LightProbeSystem not initialized");
        return 0;
    }

    m_config.bounds = bounds;
    m_config.spacing = spacing;

    BuildGrid();

    spdlog::info("Placed {} light probes in grid {}x{}x{}",
                 m_probes.size(), m_gridDimensions.x, m_gridDimensions.y, m_gridDimensions.z);

    m_gpuDataDirty = true;
    return static_cast<int>(m_probes.size());
}

int LightProbeSystem::PlaceProbes(const AABB& bounds, float uniformSpacing) {
    return PlaceProbes(bounds, glm::vec3(uniformSpacing));
}

int LightProbeSystem::PlaceProbeManual(const glm::vec3& position) {
    if (!m_initialized) {
        return -1;
    }

    LightProbe probe;
    probe.position = position;
    probe.needsUpdate = true;

    m_probes.push_back(probe);
    m_gpuDataDirty = true;

    return static_cast<int>(m_probes.size()) - 1;
}

void LightProbeSystem::RemoveProbe(int index) {
    if (index >= 0 && index < static_cast<int>(m_probes.size())) {
        m_probes.erase(m_probes.begin() + index);
        m_gpuDataDirty = true;
    }
}

void LightProbeSystem::ClearProbes() {
    m_probes.clear();
    m_gridDimensions = glm::ivec3(0);
    m_gpuDataDirty = true;
}

void LightProbeSystem::OptimizeProbes(RaycastFunc raycastFunc) {
    if (!raycastFunc) {
        return;
    }

    m_raycastFunc = raycastFunc;

    int removedCount = 0;
    for (auto& probe : m_probes) {
        // Test if probe is inside geometry by casting rays in 6 directions
        int hitCount = 0;
        const glm::vec3 directions[] = {
            {1, 0, 0}, {-1, 0, 0},
            {0, 1, 0}, {0, -1, 0},
            {0, 0, 1}, {0, 0, -1}
        };

        for (const auto& dir : directions) {
            glm::vec3 hitPos, hitNormal;
            if (raycastFunc(probe.position, dir, m_config.spacing * 0.5f, hitPos, hitNormal)) {
                hitCount++;
            }
        }

        // If most rays hit something very close, probe is likely inside geometry
        probe.isOccluded = (hitCount >= 4);
        if (probe.isOccluded) {
            probe.validity = 0.0f;
            removedCount++;
        }
    }

    spdlog::info("Optimized probes: {} marked as occluded", removedCount);
    m_gpuDataDirty = true;
}

void LightProbeSystem::BuildGrid() {
    m_probes.clear();

    const glm::vec3 size = m_config.bounds.Size();
    m_gridDimensions = glm::ivec3(
        std::max(1, static_cast<int>(std::ceil(size.x / m_config.spacing.x))),
        std::max(1, static_cast<int>(std::ceil(size.y / m_config.spacing.y))),
        std::max(1, static_cast<int>(std::ceil(size.z / m_config.spacing.z)))
    );

    m_gridOrigin = m_config.bounds.min;
    m_invSpacing = 1.0f / m_config.spacing;

    // Create probes
    int totalProbes = m_gridDimensions.x * m_gridDimensions.y * m_gridDimensions.z;
    m_probes.resize(totalProbes);

    for (int z = 0; z < m_gridDimensions.z; ++z) {
        for (int y = 0; y < m_gridDimensions.y; ++y) {
            for (int x = 0; x < m_gridDimensions.x; ++x) {
                int idx = GridToIndex(glm::ivec3(x, y, z));
                m_probes[idx].position = GridToWorld(glm::ivec3(x, y, z));
                m_probes[idx].needsUpdate = true;
            }
        }
    }

    // Update stats
    m_stats.totalProbes = totalProbes;
    m_stats.probesPendingUpdate = totalProbes;

    // Update grid info texture
    if (m_gridInfoTexture && m_gridDimensions.x > 0) {
        glBindTexture(GL_TEXTURE_3D, m_gridInfoTexture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R32I,
                    m_gridDimensions.x, m_gridDimensions.y, m_gridDimensions.z,
                    0, GL_RED_INTEGER, GL_INT, nullptr);

        // Fill with probe indices
        std::vector<int> indices(totalProbes);
        for (int i = 0; i < totalProbes; ++i) {
            indices[i] = i;
        }
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
                       m_gridDimensions.x, m_gridDimensions.y, m_gridDimensions.z,
                       GL_RED_INTEGER, GL_INT, indices.data());
        glBindTexture(GL_TEXTURE_3D, 0);
    }
}

// ============================================================================
// Grid Operations
// ============================================================================

glm::ivec3 LightProbeSystem::WorldToGrid(const glm::vec3& worldPos) const {
    glm::vec3 local = (worldPos - m_gridOrigin) * m_invSpacing;
    return glm::ivec3(glm::floor(local));
}

glm::vec3 LightProbeSystem::GridToWorld(const glm::ivec3& gridCoord) const {
    return m_gridOrigin + glm::vec3(gridCoord) * m_config.spacing +
           m_config.spacing * 0.5f; // Center of cell
}

int LightProbeSystem::GridToIndex(const glm::ivec3& gridCoord) const {
    if (!IsValidGridCoord(gridCoord)) {
        return -1;
    }
    return gridCoord.x +
           gridCoord.y * m_gridDimensions.x +
           gridCoord.z * m_gridDimensions.x * m_gridDimensions.y;
}

glm::ivec3 LightProbeSystem::IndexToGrid(int index) const {
    if (index < 0 || index >= static_cast<int>(m_probes.size())) {
        return glm::ivec3(-1);
    }
    int z = index / (m_gridDimensions.x * m_gridDimensions.y);
    int remainder = index % (m_gridDimensions.x * m_gridDimensions.y);
    int y = remainder / m_gridDimensions.x;
    int x = remainder % m_gridDimensions.x;
    return glm::ivec3(x, y, z);
}

bool LightProbeSystem::IsValidGridCoord(const glm::ivec3& gridCoord) const {
    return gridCoord.x >= 0 && gridCoord.x < m_gridDimensions.x &&
           gridCoord.y >= 0 && gridCoord.y < m_gridDimensions.y &&
           gridCoord.z >= 0 && gridCoord.z < m_gridDimensions.z;
}

// ============================================================================
// Probe Updates
// ============================================================================

void LightProbeSystem::UpdateProbes(const Camera& camera, float deltaTime) {
    if (!m_initialized || m_probes.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    glm::vec3 cameraPos = camera.GetPosition();
    CalculateUpdatePriorities(cameraPos);

    // Collect probes that need update
    std::vector<int> probesNeedingUpdate;
    probesNeedingUpdate.reserve(m_config.maxProbesPerFrame * 2);

    for (int i = 0; i < static_cast<int>(m_probes.size()); ++i) {
        if (m_probes[i].needsUpdate && !m_probes[i].isOccluded) {
            probesNeedingUpdate.push_back(i);
        }
    }

    // Sort by priority (highest first)
    SortProbesByPriority(probesNeedingUpdate);

    // Update top priority probes
    int numToUpdate = std::min(static_cast<int>(probesNeedingUpdate.size()),
                               m_config.maxProbesPerFrame);

    for (int i = 0; i < numToUpdate; ++i) {
        UpdateSingleProbe(probesNeedingUpdate[i]);
    }

    // Update age of all probes
    for (auto& probe : m_probes) {
        probe.framesSinceUpdate++;
        probe.updatePriority *= (1.0f - m_config.priorityDecay);
    }

    m_stats.probesUpdatedThisFrame = numToUpdate;
    m_stats.probesPendingUpdate = static_cast<int>(probesNeedingUpdate.size()) - numToUpdate;
    m_gpuDataDirty = true;
    m_frameCount++;

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.updateTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void LightProbeSystem::UpdateProbesGPU(const Camera& camera, float deltaTime) {
    // GPU compute path - requires compute shader support
    // For now, fall back to CPU
    UpdateProbes(camera, deltaTime);
}

void LightProbeSystem::BakeAllProbes(std::function<void(float progress)> progressCallback) {
    if (!m_initialized || m_probes.empty()) {
        return;
    }

    spdlog::info("Baking {} light probes...", m_probes.size());

    for (int i = 0; i < static_cast<int>(m_probes.size()); ++i) {
        if (!m_probes[i].isOccluded) {
            UpdateSingleProbe(i);
        }

        if (progressCallback && (i % 100 == 0 || i == static_cast<int>(m_probes.size()) - 1)) {
            progressCallback(static_cast<float>(i + 1) / m_probes.size());
        }
    }

    m_gpuDataDirty = true;
    spdlog::info("Light probe baking complete");
}

void LightProbeSystem::InvalidateRegion(const AABB& bounds) {
    for (auto& probe : m_probes) {
        if (bounds.Contains(probe.position)) {
            probe.needsUpdate = true;
            probe.updatePriority = 1.0f;
        }
    }
}

void LightProbeSystem::InvalidateProbe(int index) {
    if (index >= 0 && index < static_cast<int>(m_probes.size())) {
        m_probes[index].needsUpdate = true;
        m_probes[index].updatePriority = 1.0f;
    }
}

void LightProbeSystem::CalculateUpdatePriorities(const glm::vec3& cameraPos) {
    for (auto& probe : m_probes) {
        if (probe.isOccluded) {
            probe.updatePriority = 0.0f;
            continue;
        }

        float distance = glm::length(probe.position - cameraPos);
        float distancePriority = 1.0f - glm::clamp(distance / m_config.updateRadius, 0.0f, 1.0f);

        float agePriority = static_cast<float>(probe.framesSinceUpdate) / 60.0f;
        agePriority = glm::clamp(agePriority, 0.0f, 1.0f);

        float validityPriority = 1.0f - probe.validity;

        probe.updatePriority = distancePriority * 0.5f + agePriority * 0.3f + validityPriority * 0.2f;

        if (probe.needsUpdate) {
            probe.updatePriority += 0.5f;
        }
    }
}

void LightProbeSystem::SortProbesByPriority(std::vector<int>& probeIndices) {
    std::sort(probeIndices.begin(), probeIndices.end(),
              [this](int a, int b) {
                  return m_probes[a].updatePriority > m_probes[b].updatePriority;
              });
}

void LightProbeSystem::UpdateSingleProbe(int probeIndex) {
    if (probeIndex < 0 || probeIndex >= static_cast<int>(m_probes.size())) {
        return;
    }

    LightProbe& probe = m_probes[probeIndex];

    // Store previous for temporal blending
    probe.previousIrradiance = probe.irradiance;

    // Generate sample directions
    std::vector<glm::vec3> directions;
    GenerateSampleDirections(directions, m_config.raysPerProbe);

    // Accumulate radiance samples
    std::vector<glm::vec3> samples(m_config.raysPerProbe);

    for (int i = 0; i < m_config.raysPerProbe; ++i) {
        const glm::vec3& dir = directions[i];

        if (m_raycastFunc) {
            // Trace ray through scene
            glm::vec3 hitPos, hitNormal;
            if (m_raycastFunc(probe.position + dir * m_config.visibilityBias,
                             dir, 100.0f, hitPos, hitNormal)) {
                // Get radiance from hit point
                // For now, use simple sky gradient
                float t = glm::dot(hitNormal, glm::vec3(0, 1, 0)) * 0.5f + 0.5f;
                samples[i] = glm::mix(glm::vec3(0.2f, 0.3f, 0.4f),
                                      glm::vec3(0.8f, 0.85f, 1.0f), t);
            } else {
                // Sky color
                float t = dir.y * 0.5f + 0.5f;
                samples[i] = glm::mix(glm::vec3(0.4f, 0.5f, 0.6f),
                                      glm::vec3(0.6f, 0.8f, 1.0f), t);
            }
        } else {
            // No raycast function, use procedural sky
            float t = dir.y * 0.5f + 0.5f;
            samples[i] = glm::mix(glm::vec3(0.4f, 0.5f, 0.6f),
                                  glm::vec3(0.6f, 0.8f, 1.0f), t);
        }
    }

    // Project samples to SH
    SHCoefficients newSH;
    ProjectToSH(samples.data(), directions.data(), m_config.raysPerProbe, newSH);

    // Temporal blend
    if (probe.validity > 0.0f && m_config.temporalBlend > 0.0f) {
        for (int i = 0; i < newSH.order; ++i) {
            probe.irradiance.coeffs[i] = glm::mix(newSH.coeffs[i],
                                                   probe.previousIrradiance.coeffs[i],
                                                   m_config.temporalBlend);
        }
    } else {
        probe.irradiance = newSH;
    }

    probe.validity = 1.0f;
    probe.needsUpdate = false;
    probe.framesSinceUpdate = 0;
}

void LightProbeSystem::GenerateSampleDirections(std::vector<glm::vec3>& directions, int numSamples) const {
    directions.resize(numSamples);
    GenerateFibonacciSphereDirections(directions, numSamples);
}

// ============================================================================
// SH Operations
// ============================================================================

std::array<float, 16> LightProbeSystem::EvaluateSHBasis(const glm::vec3& direction) const {
    std::array<float, 16> basis{};
    const glm::vec3& d = direction;

    // L0
    basis[0] = SH_Y0;

    // L1
    basis[1] = SH_Y1 * d.y;
    basis[2] = SH_Y1 * d.z;
    basis[3] = SH_Y1 * d.x;

    // L2
    basis[4] = SH_Y2_0 * d.x * d.y;
    basis[5] = SH_Y2_1 * d.y * d.z;
    basis[6] = SH_Y2_2 * (3.0f * d.z * d.z - 1.0f);
    basis[7] = SH_Y2_3 * d.x * d.z;
    basis[8] = SH_Y2_4 * (d.x * d.x - d.y * d.y);

    return basis;
}

glm::vec3 LightProbeSystem::EvaluateSH(const SHCoefficients& sh, const glm::vec3& normal) {
    glm::vec3 result(0.0f);
    const glm::vec3& n = normal;

    // L0
    result += sh.coeffs[0] * SH_Y0 * A0;

    if (sh.order >= 4) {
        // L1
        result += sh.coeffs[1] * SH_Y1 * n.y * A1;
        result += sh.coeffs[2] * SH_Y1 * n.z * A1;
        result += sh.coeffs[3] * SH_Y1 * n.x * A1;
    }

    if (sh.order >= 9) {
        // L2
        result += sh.coeffs[4] * SH_Y2_0 * n.x * n.y * A2;
        result += sh.coeffs[5] * SH_Y2_1 * n.y * n.z * A2;
        result += sh.coeffs[6] * SH_Y2_2 * (3.0f * n.z * n.z - 1.0f) * A2;
        result += sh.coeffs[7] * SH_Y2_3 * n.x * n.z * A2;
        result += sh.coeffs[8] * SH_Y2_4 * (n.x * n.x - n.y * n.y) * A2;
    }

    return glm::max(result, glm::vec3(0.0f));
}

void LightProbeSystem::ProjectToSH(const glm::vec3* samples, const glm::vec3* directions,
                                   int numSamples, SHCoefficients& outSH) {
    outSH.Clear();
    outSH.order = 9; // L2

    float weight = 4.0f * glm::pi<float>() / static_cast<float>(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        const glm::vec3& dir = directions[i];
        const glm::vec3& radiance = samples[i];

        // Evaluate SH basis
        float basis[9];

        // L0
        basis[0] = SH_Y0;

        // L1
        basis[1] = SH_Y1 * dir.y;
        basis[2] = SH_Y1 * dir.z;
        basis[3] = SH_Y1 * dir.x;

        // L2
        basis[4] = SH_Y2_0 * dir.x * dir.y;
        basis[5] = SH_Y2_1 * dir.y * dir.z;
        basis[6] = SH_Y2_2 * (3.0f * dir.z * dir.z - 1.0f);
        basis[7] = SH_Y2_3 * dir.x * dir.z;
        basis[8] = SH_Y2_4 * (dir.x * dir.x - dir.y * dir.y);

        // Accumulate
        for (int b = 0; b < 9; ++b) {
            outSH.coeffs[b] += radiance * basis[b] * weight;
        }
    }
}

// ============================================================================
// GI Sampling
// ============================================================================

glm::vec3 LightProbeSystem::SampleGI(const glm::vec3& position, const glm::vec3& normal) const {
    if (m_probes.empty()) {
        return glm::vec3(0.0f);
    }

    InterpolationData interpData = GetInterpolationData(position);

    // Accumulate weighted SH
    SHCoefficients blendedSH;
    float totalWeight = 0.0f;

    for (int i = 0; i < 8; ++i) {
        int probeIdx = interpData.probeIndices[i];
        float weight = interpData.weights[i];

        if (probeIdx < 0 || weight <= 0.0f) {
            continue;
        }

        const LightProbe& probe = m_probes[probeIdx];
        if (probe.validity <= 0.0f || probe.isOccluded) {
            continue;
        }

        // Apply visibility weighting
        float visWeight = CalculateVisibilityWeight(probeIdx, position);
        weight *= visWeight * probe.validity;

        if (weight > 0.0f) {
            for (int c = 0; c < blendedSH.order; ++c) {
                blendedSH.coeffs[c] += probe.irradiance.coeffs[c] * weight;
            }
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0f) {
        for (int c = 0; c < blendedSH.order; ++c) {
            blendedSH.coeffs[c] /= totalWeight;
        }
    }

    return EvaluateSH(blendedSH, normal);
}

glm::vec3 LightProbeSystem::SampleGIDetailed(const glm::vec3& position, const glm::vec3& normal,
                                              std::array<int, 8>& outProbeIndices,
                                              std::array<float, 8>& outWeights) const {
    InterpolationData interpData = GetInterpolationData(position);
    outProbeIndices = interpData.probeIndices;
    outWeights = interpData.weights;

    return SampleGI(position, normal);
}

LightProbeSystem::InterpolationData LightProbeSystem::GetInterpolationData(const glm::vec3& position) const {
    InterpolationData data;
    data.probeIndices.fill(-1);
    data.weights.fill(0.0f);

    if (m_probes.empty() || m_gridDimensions.x <= 0) {
        return data;
    }

    // Get grid cell containing position
    glm::vec3 localPos = (position - m_gridOrigin) * m_invSpacing;
    glm::ivec3 baseCell = glm::ivec3(glm::floor(localPos));

    // Clamp to valid range
    baseCell = glm::clamp(baseCell, glm::ivec3(0), m_gridDimensions - 2);

    // Calculate cell bounds
    data.cellMin = GridToWorld(baseCell) - m_config.spacing * 0.5f;
    data.cellMax = data.cellMin + m_config.spacing;

    // Trilinear interpolation weights
    glm::vec3 t = (position - data.cellMin) / m_config.spacing;
    t = glm::clamp(t, glm::vec3(0.0f), glm::vec3(1.0f));

    // 8 corner probes
    int idx = 0;
    for (int dz = 0; dz <= 1; ++dz) {
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                glm::ivec3 gridCoord = baseCell + glm::ivec3(dx, dy, dz);
                data.probeIndices[idx] = GridToIndex(gridCoord);

                // Trilinear weight
                float wx = (dx == 0) ? (1.0f - t.x) : t.x;
                float wy = (dy == 0) ? (1.0f - t.y) : t.y;
                float wz = (dz == 0) ? (1.0f - t.z) : t.z;
                data.weights[idx] = wx * wy * wz;

                idx++;
            }
        }
    }

    return data;
}

float LightProbeSystem::CalculateVisibilityWeight(int probeIndex, const glm::vec3& samplePos) const {
    if (probeIndex < 0 || probeIndex >= static_cast<int>(m_probes.size())) {
        return 0.0f;
    }

    const LightProbe& probe = m_probes[probeIndex];

    // Simple distance-based falloff for now
    // Full implementation would use visibility rays or precomputed visibility
    float dist = glm::length(samplePos - probe.position);
    float maxDist = glm::length(m_config.spacing) * 1.5f;

    return glm::clamp(1.0f - dist / maxDist, 0.0f, 1.0f);
}

// ============================================================================
// RadianceCascade Integration
// ============================================================================

void LightProbeSystem::SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade) {
    m_radianceCascade = cascade;
}

glm::vec3 LightProbeSystem::SampleHybridGI(const glm::vec3& position, const glm::vec3& normal,
                                           float distanceFromCamera) const {
    glm::vec3 probeGI = SampleGI(position, normal);

    if (!m_radianceCascade || !m_config.enableRadianceCascadeBlend) {
        return probeGI;
    }

    // Blend based on distance from camera
    float blendStart = m_config.cascadeBlendDistance;
    float blendEnd = blendStart + m_config.cascadeBlendFalloff;

    float blendFactor = glm::clamp((distanceFromCamera - blendStart) /
                                   (blendEnd - blendStart), 0.0f, 1.0f);

    if (blendFactor <= 0.0f) {
        return probeGI;
    }

    glm::vec3 cascadeGI = m_radianceCascade->SampleRadiance(position, normal);
    return glm::mix(probeGI, cascadeGI, blendFactor);
}

// ============================================================================
// GPU Integration
// ============================================================================

void LightProbeSystem::UploadToGPU() {
    if (!m_gpuDataDirty || m_probes.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<GPULightProbe> gpuProbes;
    ConvertToGPUFormat(gpuProbes);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_probeSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuProbes.size() * sizeof(GPULightProbe),
                 gpuProbes.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_gpuDataDirty = false;

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.uploadTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    m_stats.gpuMemoryBytes = gpuProbes.size() * sizeof(GPULightProbe);
}

void LightProbeSystem::BindForRendering(uint32_t binding) {
    if (m_gpuDataDirty) {
        UploadToGPU();
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_probeSSBO);
}

void LightProbeSystem::UnbindFromRendering() {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, 0);
}

void LightProbeSystem::SetShaderUniforms(Shader& shader) {
    shader.SetIVec3("u_probeGridDim", m_gridDimensions);
    shader.SetVec3("u_probeGridOrigin", m_gridOrigin);
    shader.SetVec3("u_probeGridSpacing", m_config.spacing);
    shader.SetInt("u_probeCount", static_cast<int>(m_probes.size()));
}

void LightProbeSystem::ConvertToGPUFormat(std::vector<GPULightProbe>& gpuProbes) const {
    gpuProbes.resize(m_probes.size());

    for (size_t i = 0; i < m_probes.size(); ++i) {
        const LightProbe& probe = m_probes[i];
        GPULightProbe& gpu = gpuProbes[i];

        gpu.positionAndValidity = glm::vec4(probe.position, probe.validity);

        // Pack SH coefficients
        const auto& sh = probe.irradiance.coeffs;

        gpu.sh0 = glm::vec4(sh[0], 0.0f);

        // L1 + first L2 coefficient per channel
        gpu.sh1_r = glm::vec4(sh[1].r, sh[2].r, sh[3].r, sh[4].r);
        gpu.sh1_g = glm::vec4(sh[1].g, sh[2].g, sh[3].g, sh[4].g);
        gpu.sh1_b = glm::vec4(sh[1].b, sh[2].b, sh[3].b, sh[4].b);

        // Remaining L2 coefficients
        gpu.sh2_rg = glm::vec4(sh[5].r, sh[6].r, sh[5].g, sh[6].g);
        gpu.sh2_b_occlusion = glm::vec4(sh[5].b, sh[6].b, sh[7].r + sh[7].g + sh[7].b,
                                        probe.isOccluded ? 0.0f : 1.0f);
    }
}

// ============================================================================
// Accessors
// ============================================================================

LightProbe* LightProbeSystem::GetProbe(int index) {
    if (index >= 0 && index < static_cast<int>(m_probes.size())) {
        return &m_probes[index];
    }
    return nullptr;
}

const LightProbe* LightProbeSystem::GetProbe(int index) const {
    if (index >= 0 && index < static_cast<int>(m_probes.size())) {
        return &m_probes[index];
    }
    return nullptr;
}

LightProbe* LightProbeSystem::GetProbeAtGrid(const glm::ivec3& gridCoord) {
    int idx = GridToIndex(gridCoord);
    return GetProbe(idx);
}

const LightProbe* LightProbeSystem::GetProbeAtGrid(const glm::ivec3& gridCoord) const {
    int idx = GridToIndex(gridCoord);
    return GetProbe(idx);
}

int LightProbeSystem::GetNearestProbeIndex(const glm::vec3& position) const {
    if (m_probes.empty()) {
        return -1;
    }

    glm::ivec3 gridCoord = WorldToGrid(position);
    gridCoord = glm::clamp(gridCoord, glm::ivec3(0), m_gridDimensions - 1);
    return GridToIndex(gridCoord);
}

void LightProbeSystem::SetConfig(const ProbeGridConfig& config) {
    m_config = config;
    // Note: Some config changes require rebuilding the grid
}

// ============================================================================
// Debug Visualization
// ============================================================================

void LightProbeSystem::RenderDebugVisualization(Renderer* renderer) {
    if (!renderer || m_debugView == DebugView::None || m_probes.empty()) {
        return;
    }

    auto& debugDraw = renderer->GetDebugDraw();

    for (size_t i = 0; i < m_probes.size(); ++i) {
        const LightProbe& probe = m_probes[i];
        glm::vec4 color;
        float radius = m_config.spacing.x * 0.15f;

        switch (m_debugView) {
            case DebugView::ProbePositions:
                color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                break;

            case DebugView::ProbeValidity:
                color = glm::vec4(probe.validity, probe.validity, probe.validity, 1.0f);
                break;

            case DebugView::SHBands: {
                // Color based on dominant SH direction
                glm::vec3 dominant = glm::normalize(
                    glm::vec3(probe.irradiance.coeffs[3].r,
                             probe.irradiance.coeffs[1].g,
                             probe.irradiance.coeffs[2].b));
                color = glm::vec4(dominant * 0.5f + 0.5f, 1.0f);
                break;
            }

            case DebugView::Interpolation:
                color = glm::vec4(0.0f, 1.0f, 1.0f, 0.5f);
                break;

            case DebugView::OccludedProbes:
                color = probe.isOccluded ?
                        glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) :
                        glm::vec4(0.0f, 1.0f, 0.0f, 0.5f);
                break;

            case DebugView::UpdatePriority:
                color = glm::vec4(probe.updatePriority, 1.0f - probe.updatePriority, 0.0f, 1.0f);
                break;

            default:
                continue;
        }

        if (!probe.isOccluded || m_debugView == DebugView::OccludedProbes) {
            debugDraw.AddSphere(probe.position, radius, color);
        }
    }
}

void LightProbeSystem::RenderDebugSHSpheres(Renderer* renderer, int probeIndex) {
    if (!renderer || m_probes.empty()) {
        return;
    }

    auto& debugDraw = renderer->GetDebugDraw();

    auto renderSHSphere = [&](const LightProbe& probe) {
        const int segments = 16;
        float radius = m_config.spacing.x * 0.4f;

        for (int lat = 0; lat < segments; ++lat) {
            for (int lon = 0; lon < segments * 2; ++lon) {
                float theta0 = glm::pi<float>() * lat / segments;
                float theta1 = glm::pi<float>() * (lat + 1) / segments;
                float phi0 = 2.0f * glm::pi<float>() * lon / (segments * 2);
                float phi1 = 2.0f * glm::pi<float>() * (lon + 1) / (segments * 2);

                glm::vec3 dir(
                    std::sin(theta0) * std::cos(phi0),
                    std::cos(theta0),
                    std::sin(theta0) * std::sin(phi0)
                );

                glm::vec3 irr = EvaluateSH(probe.irradiance, dir);
                float intensity = glm::length(irr);
                glm::vec3 point = probe.position + dir * radius * (0.5f + intensity * 0.5f);

                glm::vec4 color(glm::clamp(irr, glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f);
                debugDraw.AddPoint(point, color, 2.0f);
            }
        }
    };

    if (probeIndex >= 0 && probeIndex < static_cast<int>(m_probes.size())) {
        renderSHSphere(m_probes[probeIndex]);
    } else {
        // Render subset of probes
        int step = std::max(1, static_cast<int>(m_probes.size()) / 64);
        for (size_t i = 0; i < m_probes.size(); i += step) {
            if (!m_probes[i].isOccluded && m_probes[i].validity > 0.5f) {
                renderSHSphere(m_probes[i]);
            }
        }
    }
}

void LightProbeSystem::ResetStats() {
    m_stats = Stats{};
    m_stats.totalProbes = static_cast<int>(m_probes.size());

    int valid = 0, occluded = 0;
    for (const auto& probe : m_probes) {
        if (probe.isOccluded) occluded++;
        else if (probe.validity > 0.5f) valid++;
    }
    m_stats.validProbes = valid;
    m_stats.occludedProbes = occluded;
}

// ============================================================================
// Utility Functions
// ============================================================================

void GenerateFibonacciSphereDirections(std::vector<glm::vec3>& directions, int numSamples) {
    directions.resize(numSamples);

    const float goldenAngle = glm::pi<float>() * (3.0f - std::sqrt(5.0f));

    for (int i = 0; i < numSamples; ++i) {
        float y = 1.0f - (static_cast<float>(i) / (numSamples - 1)) * 2.0f;
        float radiusAtY = std::sqrt(1.0f - y * y);
        float theta = goldenAngle * i;

        directions[i] = glm::vec3(
            std::cos(theta) * radiusAtY,
            y,
            std::sin(theta) * radiusAtY
        );
    }
}

void GenerateCosineWeightedDirections(std::vector<glm::vec3>& directions, int numSamples,
                                      const glm::vec3& normal) {
    directions.resize(numSamples);

    // Build tangent space
    glm::vec3 up = std::abs(normal.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
    glm::vec3 bitangent = glm::cross(normal, tangent);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < numSamples; ++i) {
        // Cosine-weighted hemisphere sampling
        float r1 = dist(rng);
        float r2 = dist(rng);

        float phi = 2.0f * glm::pi<float>() * r1;
        float cosTheta = std::sqrt(r2);
        float sinTheta = std::sqrt(1.0f - r2);

        glm::vec3 localDir(
            std::cos(phi) * sinTheta,
            cosTheta,
            std::sin(phi) * sinTheta
        );

        // Transform to world space
        directions[i] = tangent * localDir.x + normal * localDir.y + bitangent * localDir.z;
    }
}

void RotateSH(const SHCoefficients& input, const glm::mat3& rotation, SHCoefficients& output) {
    output = input;

    // L0 is rotationally invariant
    output.coeffs[0] = input.coeffs[0];

    if (input.order >= 4) {
        // L1 rotates as a 3D vector
        for (int c = 0; c < 3; ++c) {
            glm::vec3 l1(input.coeffs[1][c], input.coeffs[2][c], input.coeffs[3][c]);
            glm::vec3 rotated = rotation * l1;
            output.coeffs[1][c] = rotated.y;
            output.coeffs[2][c] = rotated.z;
            output.coeffs[3][c] = rotated.x;
        }
    }

    // L2 rotation is more complex - simplified version
    // Full implementation would use Wigner D-matrices
    if (input.order >= 9) {
        // Keep L2 unchanged for now (proper rotation requires D-matrices)
        for (int i = 4; i < 9; ++i) {
            output.coeffs[i] = input.coeffs[i];
        }
    }
}

void ConvolveSHCosine(SHCoefficients& sh) {
    // Apply cosine lobe convolution (A_l factors)
    sh.coeffs[0] *= A0;

    if (sh.order >= 4) {
        sh.coeffs[1] *= A1;
        sh.coeffs[2] *= A1;
        sh.coeffs[3] *= A1;
    }

    if (sh.order >= 9) {
        sh.coeffs[4] *= A2;
        sh.coeffs[5] *= A2;
        sh.coeffs[6] *= A2;
        sh.coeffs[7] *= A2;
        sh.coeffs[8] *= A2;
    }
}

} // namespace Nova
