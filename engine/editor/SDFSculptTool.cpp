/**
 * @file SDFSculptTool.cpp
 * @brief Implementation of SDF sculpting tool for Nova3D/Vehement editor
 */

#include "SDFSculptTool.hpp"
#include "EditorCommand.hpp"
#include "CommandHistory.hpp"
#include "../sdf/SDFModel.hpp"
#include "../sdf/SDFPrimitive.hpp"
#include "../graphics/Shader.hpp"
#include "../graphics/Mesh.hpp"
#include "../scene/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace Nova {

// =============================================================================
// Falloff Functions
// =============================================================================

namespace {

/**
 * @brief Smooth interpolation function (3x^2 - 2x^3)
 */
float Smoothstep(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Smoother interpolation function (6x^5 - 15x^4 + 10x^3)
 */
float Smootherstep(float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/**
 * @brief Smooth minimum for CSG operations
 */
float SmoothMin(float a, float b, float k) {
    if (k <= 0.0f) return std::min(a, b);
    float h = glm::clamp(0.5f + 0.5f * (b - a) / k, 0.0f, 1.0f);
    return glm::mix(b, a, h) - k * h * (1.0f - h);
}

/**
 * @brief Smooth maximum for CSG operations
 */
float SmoothMax(float a, float b, float k) {
    return -SmoothMin(-a, -b, k);
}

} // anonymous namespace

float SDFSculptTool::CalculateFalloff(float distance, float radius, FalloffType falloffType) {
    if (radius <= 0.0f) return 0.0f;
    if (distance >= radius) return 0.0f;
    if (distance <= 0.0f) return 1.0f;

    float normalizedDist = distance / radius;

    switch (falloffType) {
        case FalloffType::Linear:
            return 1.0f - normalizedDist;

        case FalloffType::Smooth:
            return Smoothstep(1.0f - normalizedDist);

        case FalloffType::Sharp:
            return (1.0f - normalizedDist) * (1.0f - normalizedDist);

        case FalloffType::Constant:
            return 1.0f;

        default:
            return 1.0f - normalizedDist;
    }
}

// =============================================================================
// SDFGrid Implementation
// =============================================================================

SDFGrid::SDFGrid() = default;

SDFGrid::SDFGrid(const glm::ivec3& resolution, const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    Initialize(resolution, boundsMin, boundsMax);
}

void SDFGrid::Initialize(const glm::ivec3& resolution, const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    m_resolution = glm::max(resolution, glm::ivec3(2));
    m_boundsMin = boundsMin;
    m_boundsMax = boundsMax;

    glm::vec3 extent = m_boundsMax - m_boundsMin;
    m_voxelSize = extent / glm::vec3(m_resolution - glm::ivec3(1));

    size_t totalVoxels = static_cast<size_t>(m_resolution.x) *
                         static_cast<size_t>(m_resolution.y) *
                         static_cast<size_t>(m_resolution.z);
    m_data.resize(totalVoxels, 1.0f); // Default to "outside" (positive distance)
}

void SDFGrid::InitializeFromModel(const SDFModel& model, int resolution) {
    auto bounds = model.GetBounds();
    glm::vec3 boundsMin = bounds.first;
    glm::vec3 boundsMax = bounds.second;

    // Add some padding
    glm::vec3 padding = (boundsMax - boundsMin) * 0.1f;
    boundsMin -= padding;
    boundsMax += padding;

    Initialize(glm::ivec3(resolution), boundsMin, boundsMax);

    // Sample the model's SDF to populate grid
    for (int z = 0; z < m_resolution.z; ++z) {
        for (int y = 0; y < m_resolution.y; ++y) {
            for (int x = 0; x < m_resolution.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float sdfValue = model.EvaluateSDF(worldPos);
                SetAt(x, y, z, sdfValue);
            }
        }
    }
}

void SDFGrid::Clear(float value) {
    std::fill(m_data.begin(), m_data.end(), value);
}

float SDFGrid::Sample(const glm::vec3& worldPos) const {
    if (m_data.empty()) return 1.0f;

    // Convert to normalized grid coordinates
    glm::vec3 gridPos = (worldPos - m_boundsMin) / m_voxelSize;

    // Get integer indices and fractional parts
    glm::ivec3 i0 = glm::ivec3(glm::floor(gridPos));
    glm::ivec3 i1 = i0 + glm::ivec3(1);
    glm::vec3 t = gridPos - glm::vec3(i0);

    // Clamp indices to valid range
    i0 = glm::clamp(i0, glm::ivec3(0), m_resolution - glm::ivec3(1));
    i1 = glm::clamp(i1, glm::ivec3(0), m_resolution - glm::ivec3(1));

    // Trilinear interpolation
    float c000 = SampleAt(i0.x, i0.y, i0.z);
    float c100 = SampleAt(i1.x, i0.y, i0.z);
    float c010 = SampleAt(i0.x, i1.y, i0.z);
    float c110 = SampleAt(i1.x, i1.y, i0.z);
    float c001 = SampleAt(i0.x, i0.y, i1.z);
    float c101 = SampleAt(i1.x, i0.y, i1.z);
    float c011 = SampleAt(i0.x, i1.y, i1.z);
    float c111 = SampleAt(i1.x, i1.y, i1.z);

    float c00 = glm::mix(c000, c100, t.x);
    float c10 = glm::mix(c010, c110, t.x);
    float c01 = glm::mix(c001, c101, t.x);
    float c11 = glm::mix(c011, c111, t.x);

    float c0 = glm::mix(c00, c10, t.y);
    float c1 = glm::mix(c01, c11, t.y);

    return glm::mix(c0, c1, t.z);
}

float SDFGrid::SampleAt(int x, int y, int z) const {
    if (!IsValidIndex(x, y, z)) return 1.0f;
    return m_data[GetLinearIndex(x, y, z)];
}

float SDFGrid::SampleAt(const glm::ivec3& index) const {
    return SampleAt(index.x, index.y, index.z);
}

void SDFGrid::SetAt(int x, int y, int z, float value) {
    if (!IsValidIndex(x, y, z)) return;
    m_data[GetLinearIndex(x, y, z)] = value;
}

void SDFGrid::SetAt(const glm::ivec3& index, float value) {
    SetAt(index.x, index.y, index.z, value);
}

glm::vec3 SDFGrid::CalculateGradient(const glm::vec3& worldPos, float epsilon) const {
    if (epsilon <= 0.0f) {
        epsilon = glm::length(m_voxelSize) * 0.5f;
    }

    glm::vec3 gradient;
    gradient.x = Sample(worldPos + glm::vec3(epsilon, 0, 0)) - Sample(worldPos - glm::vec3(epsilon, 0, 0));
    gradient.y = Sample(worldPos + glm::vec3(0, epsilon, 0)) - Sample(worldPos - glm::vec3(0, epsilon, 0));
    gradient.z = Sample(worldPos + glm::vec3(0, 0, epsilon)) - Sample(worldPos - glm::vec3(0, 0, epsilon));

    float len = glm::length(gradient);
    if (len > 1e-6f) {
        gradient /= len;
    } else {
        gradient = glm::vec3(0, 1, 0);
    }
    return gradient;
}

glm::ivec3 SDFGrid::WorldToGrid(const glm::vec3& worldPos) const {
    glm::vec3 normalized = (worldPos - m_boundsMin) / m_voxelSize;
    return glm::ivec3(glm::round(normalized));
}

glm::vec3 SDFGrid::GridToWorld(const glm::ivec3& gridIndex) const {
    return GridToWorld(gridIndex.x, gridIndex.y, gridIndex.z);
}

glm::vec3 SDFGrid::GridToWorld(int x, int y, int z) const {
    return m_boundsMin + glm::vec3(x, y, z) * m_voxelSize;
}

bool SDFGrid::IsValidIndex(int x, int y, int z) const {
    return x >= 0 && x < m_resolution.x &&
           y >= 0 && y < m_resolution.y &&
           z >= 0 && z < m_resolution.z;
}

bool SDFGrid::IsValidIndex(const glm::ivec3& index) const {
    return IsValidIndex(index.x, index.y, index.z);
}

size_t SDFGrid::GetLinearIndex(int x, int y, int z) const {
    return static_cast<size_t>(x) +
           static_cast<size_t>(y) * static_cast<size_t>(m_resolution.x) +
           static_cast<size_t>(z) * static_cast<size_t>(m_resolution.x) * static_cast<size_t>(m_resolution.y);
}

void SDFGrid::UnionSphere(const glm::vec3& center, float radius, float smoothness) {
    // Calculate affected region
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius + smoothness));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius + smoothness));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float sphereSDF = glm::length(worldPos - center) - radius;
                float currentSDF = SampleAt(x, y, z);

                if (smoothness > 0.0f) {
                    SetAt(x, y, z, SmoothMin(currentSDF, sphereSDF, smoothness));
                } else {
                    SetAt(x, y, z, std::min(currentSDF, sphereSDF));
                }
            }
        }
    }
}

void SDFGrid::SubtractSphere(const glm::vec3& center, float radius, float smoothness) {
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius + smoothness));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius + smoothness));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float sphereSDF = glm::length(worldPos - center) - radius;
                float currentSDF = SampleAt(x, y, z);

                if (smoothness > 0.0f) {
                    SetAt(x, y, z, SmoothMax(currentSDF, -sphereSDF, smoothness));
                } else {
                    SetAt(x, y, z, std::max(currentSDF, -sphereSDF));
                }
            }
        }
    }
}

void SDFGrid::SmoothRegion(const glm::vec3& center, float radius, float strength) {
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius));

    minIdx = glm::max(minIdx, glm::ivec3(1));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(2));

    // Create temporary copy for reading while writing
    std::vector<float> smoothed = m_data;

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius) continue;

                float falloff = SDFSculptTool::CalculateFalloff(dist, radius, FalloffType::Smooth);
                float blendFactor = strength * falloff;

                // Sample 3x3x3 neighborhood and average
                float sum = 0.0f;
                int count = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            sum += SampleAt(x + dx, y + dy, z + dz);
                            ++count;
                        }
                    }
                }
                float avg = sum / static_cast<float>(count);
                float current = SampleAt(x, y, z);

                smoothed[GetLinearIndex(x, y, z)] = glm::mix(current, avg, blendFactor);
            }
        }
    }

    m_data = std::move(smoothed);
}

void SDFGrid::FlattenToPlane(const glm::vec3& center, float radius,
                              const glm::vec3& planeNormal, float planeDistance, float strength) {
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    glm::vec3 normal = glm::normalize(planeNormal);

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius) continue;

                float falloff = SDFSculptTool::CalculateFalloff(dist, radius, FalloffType::Smooth);
                float blendFactor = strength * falloff;

                // Calculate distance to plane
                float planeSDF = glm::dot(worldPos, normal) - planeDistance;
                float currentSDF = SampleAt(x, y, z);

                // Blend toward plane
                SetAt(x, y, z, glm::mix(currentSDF, planeSDF, blendFactor));
            }
        }
    }
}

void SDFGrid::PinchRegion(const glm::vec3& center, float radius, float strength) {
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    std::vector<float> result = m_data;

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius || dist < 1e-6f) continue;

                float falloff = SDFSculptTool::CalculateFalloff(dist, radius, FalloffType::Smooth);
                float blendFactor = strength * falloff;

                // Move point toward center
                glm::vec3 toCenter = glm::normalize(center - worldPos);
                glm::vec3 newPos = worldPos + toCenter * blendFactor * m_voxelSize.x;

                // Sample SDF at new position
                float newSDF = Sample(newPos);
                float currentSDF = SampleAt(x, y, z);

                // Blend
                result[GetLinearIndex(x, y, z)] = glm::mix(currentSDF, newSDF, blendFactor);
            }
        }
    }

    m_data = std::move(result);
}

void SDFGrid::InflateRegion(const glm::vec3& center, float radius, float strength) {
    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius) continue;

                float currentSDF = SampleAt(x, y, z);

                // Only affect near-surface voxels
                if (std::abs(currentSDF) > radius * 0.5f) continue;

                float falloff = SDFSculptTool::CalculateFalloff(dist, radius, FalloffType::Smooth);
                float delta = -strength * falloff * m_voxelSize.x;

                SetAt(x, y, z, currentSDF + delta);
            }
        }
    }
}

void SDFGrid::DisplaceRegion(const glm::vec3& center, float radius,
                              const glm::vec3& displacement, float strength) {
    if (glm::length(displacement) < 1e-6f) return;

    glm::ivec3 minIdx = WorldToGrid(center - glm::vec3(radius) - glm::abs(displacement));
    glm::ivec3 maxIdx = WorldToGrid(center + glm::vec3(radius) + glm::abs(displacement));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_resolution - glm::ivec3(1));

    std::vector<float> result = m_data;

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius) continue;

                float falloff = SDFSculptTool::CalculateFalloff(dist, radius, FalloffType::Smooth);
                float blendFactor = strength * falloff;

                // Sample from displaced position
                glm::vec3 samplePos = worldPos - displacement * blendFactor;
                float newSDF = Sample(samplePos);

                result[GetLinearIndex(x, y, z)] = newSDF;
            }
        }
    }

    m_data = std::move(result);
}

SDFGrid::RegionSnapshot SDFGrid::CaptureRegion(const glm::vec3& center, float radius) const {
    RegionSnapshot snapshot;

    snapshot.minIndex = WorldToGrid(center - glm::vec3(radius));
    snapshot.maxIndex = WorldToGrid(center + glm::vec3(radius));

    snapshot.minIndex = glm::max(snapshot.minIndex, glm::ivec3(0));
    snapshot.maxIndex = glm::min(snapshot.maxIndex, m_resolution - glm::ivec3(1));

    glm::ivec3 size = snapshot.maxIndex - snapshot.minIndex + glm::ivec3(1);
    size_t count = static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * static_cast<size_t>(size.z);
    snapshot.values.reserve(count);

    for (int z = snapshot.minIndex.z; z <= snapshot.maxIndex.z; ++z) {
        for (int y = snapshot.minIndex.y; y <= snapshot.maxIndex.y; ++y) {
            for (int x = snapshot.minIndex.x; x <= snapshot.maxIndex.x; ++x) {
                snapshot.values.push_back(SampleAt(x, y, z));
            }
        }
    }

    return snapshot;
}

void SDFGrid::RestoreRegion(const RegionSnapshot& snapshot) {
    if (snapshot.IsEmpty()) return;

    size_t idx = 0;
    for (int z = snapshot.minIndex.z; z <= snapshot.maxIndex.z; ++z) {
        for (int y = snapshot.minIndex.y; y <= snapshot.maxIndex.y; ++y) {
            for (int x = snapshot.minIndex.x; x <= snapshot.maxIndex.x; ++x) {
                if (idx < snapshot.values.size()) {
                    SetAt(x, y, z, snapshot.values[idx++]);
                }
            }
        }
    }
}

// =============================================================================
// SDFSculptCommand Implementation
// =============================================================================

SDFSculptCommand::SDFSculptCommand(SDFGrid* grid, SDFBrushStroke stroke)
    : m_grid(grid)
    , m_stroke(std::move(stroke)) {
}

bool SDFSculptCommand::Execute() {
    if (!m_grid || m_stroke.IsEmpty()) return false;

    // Capture before state if not already done
    if (m_stroke.beforeSnapshot.IsEmpty()) {
        float maxRadius = 0.0f;
        for (const auto& dab : m_stroke.dabs) {
            maxRadius = std::max(maxRadius, dab.effectiveRadius);
        }
        // Capture entire affected region
        glm::vec3 regionCenter = (m_stroke.boundsMin + m_stroke.boundsMax) * 0.5f;
        float regionRadius = glm::length(m_stroke.boundsMax - m_stroke.boundsMin) * 0.5f + maxRadius;
        m_stroke.beforeSnapshot = m_grid->CaptureRegion(regionCenter, regionRadius);
    }

    // Re-apply the stroke would require replay - for now we just store after state
    if (!m_executed) {
        // Capture after state
        glm::vec3 regionCenter = (m_stroke.boundsMin + m_stroke.boundsMax) * 0.5f;
        float regionRadius = glm::length(m_stroke.boundsMax - m_stroke.boundsMin) * 0.5f;
        for (const auto& dab : m_stroke.dabs) {
            regionRadius = std::max(regionRadius, dab.effectiveRadius);
        }
        m_afterSnapshot = m_grid->CaptureRegion(regionCenter, regionRadius);
        m_executed = true;
    } else {
        // Redo - restore after state
        m_grid->RestoreRegion(m_afterSnapshot);
    }

    return true;
}

bool SDFSculptCommand::Undo() {
    if (!m_grid || m_stroke.beforeSnapshot.IsEmpty()) return false;
    m_grid->RestoreRegion(m_stroke.beforeSnapshot);
    return true;
}

std::string SDFSculptCommand::GetName() const {
    return std::string("Sculpt (") + GetBrushTypeName(m_stroke.brushType) + ")";
}

CommandTypeId SDFSculptCommand::GetTypeId() const {
    return typeid(SDFSculptCommand).hash_code();
}

bool SDFSculptCommand::CanMergeWith(const ICommand& other) const {
    // Don't merge sculpt commands - each stroke is independent
    (void)other;
    return false;
}

bool SDFSculptCommand::MergeWith(const ICommand& other) {
    (void)other;
    return false;
}

// =============================================================================
// SDFSculptTool Implementation
// =============================================================================

SDFSculptTool::SDFSculptTool() = default;

SDFSculptTool::~SDFSculptTool() {
    Shutdown();
}

SDFSculptTool::SDFSculptTool(SDFSculptTool&&) noexcept = default;
SDFSculptTool& SDFSculptTool::operator=(SDFSculptTool&&) noexcept = default;

bool SDFSculptTool::Initialize() {
    if (m_initialized) return true;

    if (!CreateOverlayShader()) {
        return false;
    }

    CreateOverlayMesh();
    m_initialized = true;
    return true;
}

void SDFSculptTool::Shutdown() {
    if (!m_initialized) return;

    m_overlayShader.reset();
    m_brushCircleMesh.reset();
    m_brushSphereMesh.reset();

    // Note: VAO/VBO cleanup would be done here if using raw OpenGL
    // The Mesh class handles its own cleanup

    m_initialized = false;
}

void SDFSculptTool::SetTarget(SDFGrid* grid) {
    // Cancel any active stroke when changing target
    if (m_strokeActive) {
        CancelStroke();
    }
    m_targetGrid = grid;
}

void SDFSculptTool::SetCloneSource(const glm::vec3& position, const glm::vec3& normal) {
    m_cloneSource = position;
    m_cloneSourceNormal = glm::normalize(normal);
    m_hasCloneSource = true;
}

bool SDFSculptTool::BeginStroke(const glm::vec3& hitPos, const glm::vec3& normal) {
    if (!m_targetGrid) return false;
    if (m_strokeActive) {
        CancelStroke();
    }

    // Initialize stroke data
    m_currentStroke = SDFBrushStroke{};
    m_currentStroke.brushType = m_brushType;
    m_currentStroke.settings = m_settings;

    // Capture initial region for undo
    float captureRadius = m_settings.radius * 2.0f;
    m_currentStroke.beforeSnapshot = m_targetGrid->CaptureRegion(hitPos, captureRadius);
    m_currentStroke.boundsMin = hitPos - glm::vec3(m_settings.radius);
    m_currentStroke.boundsMax = hitPos + glm::vec3(m_settings.radius);

    m_strokeActive = true;
    m_strokeStartPosition = hitPos;
    m_lastDabPosition = hitPos;
    m_strokeDistance = 0.0f;

    // Special handling for grab brush
    if (m_brushType == BrushType::Grab) {
        m_grabStartPosition = hitPos;
        m_grabLastPosition = hitPos;
    }

    // Special handling for flatten brush - compute reference plane
    if (m_brushType == BrushType::Flatten && !m_settings.useCustomPlane) {
        m_flattenPlaneNormal = glm::normalize(normal);
        m_flattenPlaneDistance = glm::dot(hitPos, m_flattenPlaneNormal);
    }

    // Initialize lazy mouse
    m_lazyPosition = hitPos;

    // Apply first dab
    UpdateStroke(hitPos, normal, 1.0f);

    if (m_onStrokeBegin) {
        m_onStrokeBegin();
    }

    return true;
}

void SDFSculptTool::UpdateStroke(const glm::vec3& hitPos, const glm::vec3& normal, float pressure) {
    if (!m_strokeActive || !m_targetGrid) return;

    glm::vec3 targetPos = hitPos;

    // Apply lazy mouse if enabled
    if (m_settings.lazyMouse) {
        glm::vec3 toTarget = hitPos - m_lazyPosition;
        float dist = glm::length(toTarget);
        float lazyDist = m_settings.lazyRadius * m_settings.radius;

        if (dist > lazyDist) {
            m_lazyPosition += glm::normalize(toTarget) * (dist - lazyDist);
        }
        targetPos = m_lazyPosition;
    }

    // Calculate effective brush parameters
    float effectiveRadius, effectiveStrength;
    CalculateEffectiveParams(pressure, effectiveRadius, effectiveStrength);

    // Apply invert if enabled
    if (m_settings.invertBrush) {
        effectiveStrength = -effectiveStrength;
    }

    // Check spacing
    float spacing = m_settings.spacing * effectiveRadius;
    glm::vec3 delta = targetPos - m_lastDabPosition;
    float stepDist = glm::length(delta);

    if (stepDist < spacing && !m_currentStroke.dabs.empty()) {
        // For grab brush, we still need to update even without new dabs
        if (m_brushType == BrushType::Grab) {
            m_grabLastPosition = targetPos;
        }
        return;
    }

    // Interpolate dabs along the stroke path
    int numDabs = std::max(1, static_cast<int>(stepDist / spacing));
    glm::vec3 step = delta / static_cast<float>(numDabs);

    for (int i = 0; i < numDabs; ++i) {
        glm::vec3 dabPos = m_lastDabPosition + step * static_cast<float>(i + 1);

        BrushDab dab;
        dab.position = dabPos;
        dab.normal = normal;
        dab.pressure = pressure;
        dab.effectiveRadius = effectiveRadius;
        dab.effectiveStrength = effectiveStrength;

        // Expand stroke bounds
        m_currentStroke.ExpandBounds(dabPos, effectiveRadius);

        // Apply the dab with symmetry
        ApplyWithSymmetry(dab);

        m_currentStroke.dabs.push_back(dab);
    }

    m_lastDabPosition = targetPos;
    m_strokeDistance += stepDist;

    // Update grab state
    if (m_brushType == BrushType::Grab) {
        m_grabLastPosition = targetPos;
    }

    if (m_onGridModified) {
        m_onGridModified();
    }
}

void SDFSculptTool::EndStroke(CommandHistory* history) {
    if (!m_strokeActive) return;

    // Apply auto-smooth if enabled
    if (m_settings.autoSmooth && m_targetGrid) {
        glm::vec3 center = (m_currentStroke.boundsMin + m_currentStroke.boundsMax) * 0.5f;
        float radius = glm::length(m_currentStroke.boundsMax - m_currentStroke.boundsMin) * 0.5f;
        m_targetGrid->SmoothRegion(center, radius, m_settings.autoSmoothStrength);
    }

    // Record command for undo
    if (history && !m_currentStroke.dabs.empty()) {
        // Expand capture region to include all dabs
        float maxRadius = 0.0f;
        for (const auto& dab : m_currentStroke.dabs) {
            maxRadius = std::max(maxRadius, dab.effectiveRadius);
        }

        // Re-capture with expanded bounds
        glm::vec3 center = (m_currentStroke.boundsMin + m_currentStroke.boundsMax) * 0.5f;
        float captureRadius = glm::length(m_currentStroke.boundsMax - m_currentStroke.boundsMin) * 0.5f + maxRadius;

        // Update the beforeSnapshot to cover the full region
        if (m_targetGrid) {
            SDFGrid::RegionSnapshot expandedBefore = m_targetGrid->CaptureRegion(
                m_strokeStartPosition,
                glm::length(m_currentStroke.boundsMax - m_currentStroke.boundsMin) + maxRadius * 2.0f
            );

            // Create command with captured state
            auto command = std::make_unique<SDFSculptCommand>(m_targetGrid, std::move(m_currentStroke));
            history->ExecuteCommand(std::move(command));
        }
    }

    m_strokeActive = false;
    m_currentStroke = SDFBrushStroke{};

    if (m_onStrokeEnd) {
        m_onStrokeEnd();
    }
}

void SDFSculptTool::CancelStroke() {
    if (!m_strokeActive) return;

    // Restore original state
    if (m_targetGrid && !m_currentStroke.beforeSnapshot.IsEmpty()) {
        m_targetGrid->RestoreRegion(m_currentStroke.beforeSnapshot);
    }

    m_strokeActive = false;
    m_currentStroke = SDFBrushStroke{};

    if (m_onGridModified) {
        m_onGridModified();
    }
}

void SDFSculptTool::UpdatePreview(const glm::vec3& hitPos, const glm::vec3& normal) {
    m_previewPosition = hitPos;
    m_previewNormal = glm::normalize(normal);
    m_previewValid = true;
    UpdateOverlayMesh();
}

void SDFSculptTool::ClearPreview() {
    m_previewValid = false;
}

void SDFSculptTool::RenderOverlay(const Camera& camera) {
    if (!m_initialized || !m_showPreview || !m_previewValid) return;

    // Get view and projection matrices from camera
    // Note: Implementation depends on Camera class interface
    // RenderOverlay(camera.GetViewMatrix(), camera.GetProjectionMatrix());
}

void SDFSculptTool::RenderOverlay(const glm::mat4& view, const glm::mat4& projection) {
    if (!m_initialized || !m_showPreview || !m_previewValid) return;
    if (!m_overlayShader) return;

    // Render brush circle at preview position
    // This would use OpenGL calls to render the brush indicator
    // Implementation depends on the rendering backend

    // Build model matrix for brush circle
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_previewPosition);

    // Align circle to surface normal
    glm::vec3 up = glm::vec3(0, 1, 0);
    if (std::abs(glm::dot(m_previewNormal, up)) > 0.99f) {
        up = glm::vec3(1, 0, 0);
    }
    glm::vec3 right = glm::normalize(glm::cross(up, m_previewNormal));
    up = glm::normalize(glm::cross(m_previewNormal, right));

    glm::mat4 rotation = glm::mat4(
        glm::vec4(right, 0),
        glm::vec4(m_previewNormal, 0),
        glm::vec4(up, 0),
        glm::vec4(0, 0, 0, 1)
    );
    model = model * rotation;
    model = glm::scale(model, glm::vec3(m_settings.radius));

    // The actual rendering would be done here using the shader and mesh
    // This is a placeholder for the rendering code
    (void)view;
    (void)projection;
    (void)model;
}

void SDFSculptTool::ApplyDab(const BrushDab& dab) {
    if (!m_targetGrid) return;

    switch (m_brushType) {
        case BrushType::Add:
            ApplyAddBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;

        case BrushType::Subtract:
            ApplySubtractBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;

        case BrushType::Smooth:
            ApplySmoothBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;

        case BrushType::Flatten:
            ApplyFlattenBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength, dab.normal);
            break;

        case BrushType::Pinch:
            ApplyPinchBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;

        case BrushType::Inflate:
            ApplyInflateBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;

        case BrushType::Grab: {
            glm::vec3 delta = m_grabLastPosition - m_grabStartPosition;
            ApplyGrabBrush(m_grabStartPosition, dab.effectiveRadius, dab.effectiveStrength, delta);
            break;
        }

        case BrushType::Clone:
            ApplyCloneBrush(dab.position, dab.effectiveRadius, dab.effectiveStrength);
            break;
    }
}

void SDFSculptTool::ApplyWithSymmetry(const BrushDab& dab) {
    std::vector<glm::vec3> positions = GetSymmetryPositions(dab.position);

    for (const auto& pos : positions) {
        BrushDab symmetricDab = dab;
        symmetricDab.position = pos;

        // Flip normal for mirrored positions if needed
        glm::vec3 offset = pos - m_settings.symmetryOrigin;
        glm::vec3 origOffset = dab.position - m_settings.symmetryOrigin;

        if (HasSymmetry(m_settings.symmetry, SymmetryMode::X) && offset.x * origOffset.x < 0) {
            symmetricDab.normal.x = -symmetricDab.normal.x;
        }
        if (HasSymmetry(m_settings.symmetry, SymmetryMode::Y) && offset.y * origOffset.y < 0) {
            symmetricDab.normal.y = -symmetricDab.normal.y;
        }
        if (HasSymmetry(m_settings.symmetry, SymmetryMode::Z) && offset.z * origOffset.z < 0) {
            symmetricDab.normal.z = -symmetricDab.normal.z;
        }

        ApplyDab(symmetricDab);
    }
}

std::vector<glm::vec3> SDFSculptTool::GetSymmetryPositions(const glm::vec3& position) const {
    std::vector<glm::vec3> positions;
    positions.push_back(position);

    if (m_settings.symmetry == SymmetryMode::None) {
        return positions;
    }

    glm::vec3 offset = position - m_settings.symmetryOrigin;

    // Mirror across X axis (YZ plane)
    if (HasSymmetry(m_settings.symmetry, SymmetryMode::X)) {
        size_t count = positions.size();
        for (size_t i = 0; i < count; ++i) {
            glm::vec3 mirrored = positions[i];
            mirrored.x = m_settings.symmetryOrigin.x - (mirrored.x - m_settings.symmetryOrigin.x);
            positions.push_back(mirrored);
        }
    }

    // Mirror across Y axis (XZ plane)
    if (HasSymmetry(m_settings.symmetry, SymmetryMode::Y)) {
        size_t count = positions.size();
        for (size_t i = 0; i < count; ++i) {
            glm::vec3 mirrored = positions[i];
            mirrored.y = m_settings.symmetryOrigin.y - (mirrored.y - m_settings.symmetryOrigin.y);
            positions.push_back(mirrored);
        }
    }

    // Mirror across Z axis (XY plane)
    if (HasSymmetry(m_settings.symmetry, SymmetryMode::Z)) {
        size_t count = positions.size();
        for (size_t i = 0; i < count; ++i) {
            glm::vec3 mirrored = positions[i];
            mirrored.z = m_settings.symmetryOrigin.z - (mirrored.z - m_settings.symmetryOrigin.z);
            positions.push_back(mirrored);
        }
    }

    // Radial symmetry around Y axis
    if (HasSymmetry(m_settings.symmetry, SymmetryMode::Radial)) {
        std::vector<glm::vec3> radialPositions;
        float angleStep = 2.0f * 3.14159265359f / static_cast<float>(m_settings.radialCount);

        for (const auto& pos : positions) {
            glm::vec3 localPos = pos - m_settings.symmetryOrigin;

            for (int i = 1; i < m_settings.radialCount; ++i) {
                float angle = angleStep * static_cast<float>(i);
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                glm::vec3 rotated;
                rotated.x = localPos.x * cosA - localPos.z * sinA;
                rotated.y = localPos.y;
                rotated.z = localPos.x * sinA + localPos.z * cosA;

                radialPositions.push_back(rotated + m_settings.symmetryOrigin);
            }
        }

        positions.insert(positions.end(), radialPositions.begin(), radialPositions.end());
    }

    return positions;
}

void SDFSculptTool::CalculateEffectiveParams(float pressure, float& outRadius, float& outStrength) const {
    outRadius = m_settings.radius;
    outStrength = m_settings.strength;

    if (m_settings.pressureSensitivity) {
        float radiusScale = glm::mix(1.0f, pressure, m_settings.pressureRadiusScale);
        float strengthScale = glm::mix(1.0f, pressure, m_settings.pressureStrengthScale);

        outRadius *= radiusScale;
        outStrength *= strengthScale;
    }

    // Clamp to valid ranges
    outRadius = glm::clamp(outRadius, 0.01f, 10.0f);
    outStrength = glm::clamp(outStrength, 0.0f, 1.0f);
}

bool SDFSculptTool::ShouldApplyDab(const glm::vec3& position) const {
    float spacing = m_settings.spacing * m_settings.radius;
    return glm::length(position - m_lastDabPosition) >= spacing;
}

void SDFSculptTool::CreateOverlayMesh() {
    // Create a circle mesh for brush preview
    // This would create vertex data for a circle with CIRCLE_SEGMENTS vertices
    // Implementation depends on the Mesh class interface
}

void SDFSculptTool::UpdateOverlayMesh() {
    // Update the overlay mesh transformation based on current preview state
    // This would update uniforms or vertex data as needed
}

bool SDFSculptTool::CreateOverlayShader() {
    // Create shader for rendering brush overlay
    // This would compile vertex and fragment shaders

    // Placeholder - actual implementation would create real shaders
    // m_overlayShader = std::make_unique<Shader>("brush_overlay.vert", "brush_overlay.frag");
    return true;
}

// =============================================================================
// Brush Operation Implementations
// =============================================================================

void SDFSculptTool::ApplyAddBrush(const glm::vec3& center, float radius, float strength) {
    float smoothness = radius * 0.1f * strength;
    m_targetGrid->UnionSphere(center, radius * strength, smoothness);
}

void SDFSculptTool::ApplySubtractBrush(const glm::vec3& center, float radius, float strength) {
    float smoothness = radius * 0.1f * strength;
    m_targetGrid->SubtractSphere(center, radius * strength, smoothness);
}

void SDFSculptTool::ApplySmoothBrush(const glm::vec3& center, float radius, float strength) {
    m_targetGrid->SmoothRegion(center, radius, strength);
}

void SDFSculptTool::ApplyFlattenBrush(const glm::vec3& center, float radius, float strength,
                                       const glm::vec3& normal) {
    glm::vec3 planeNormal;
    float planeDist;

    if (m_settings.useCustomPlane) {
        planeNormal = m_settings.flattenPlaneNormal;
        planeDist = m_settings.flattenPlaneDistance;
    } else {
        planeNormal = m_flattenPlaneNormal;
        planeDist = m_flattenPlaneDistance;
    }

    m_targetGrid->FlattenToPlane(center, radius, planeNormal, planeDist, strength);
}

void SDFSculptTool::ApplyPinchBrush(const glm::vec3& center, float radius, float strength) {
    m_targetGrid->PinchRegion(center, radius, strength);
}

void SDFSculptTool::ApplyInflateBrush(const glm::vec3& center, float radius, float strength) {
    m_targetGrid->InflateRegion(center, radius, strength);
}

void SDFSculptTool::ApplyGrabBrush(const glm::vec3& center, float radius, float strength,
                                    const glm::vec3& delta) {
    m_targetGrid->DisplaceRegion(center, radius, delta * strength, 1.0f);
}

void SDFSculptTool::ApplyCloneBrush(const glm::vec3& center, float radius, float strength) {
    if (!m_hasCloneSource) return;

    // Calculate source position based on offset
    glm::vec3 sourcePos = center + m_settings.cloneSourceOffset;

    // Sample from source, write to destination
    glm::ivec3 minIdx = m_targetGrid->WorldToGrid(center - glm::vec3(radius));
    glm::ivec3 maxIdx = m_targetGrid->WorldToGrid(center + glm::vec3(radius));

    minIdx = glm::max(minIdx, glm::ivec3(0));
    maxIdx = glm::min(maxIdx, m_targetGrid->GetResolution() - glm::ivec3(1));

    for (int z = minIdx.z; z <= maxIdx.z; ++z) {
        for (int y = minIdx.y; y <= maxIdx.y; ++y) {
            for (int x = minIdx.x; x <= maxIdx.x; ++x) {
                glm::vec3 worldPos = m_targetGrid->GridToWorld(x, y, z);
                float dist = glm::length(worldPos - center);

                if (dist >= radius) continue;

                float falloff = CalculateFalloff(dist, radius, m_settings.falloff);
                float blendFactor = strength * falloff;

                // Sample from source location
                glm::vec3 samplePos = worldPos + m_settings.cloneSourceOffset;
                float sourceSDF = m_targetGrid->Sample(samplePos);
                float currentSDF = m_targetGrid->SampleAt(x, y, z);

                m_targetGrid->SetAt(x, y, z, glm::mix(currentSDF, sourceSDF, blendFactor));
            }
        }
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* GetBrushTypeName(BrushType type) {
    switch (type) {
        case BrushType::Add:      return "Add";
        case BrushType::Subtract: return "Subtract";
        case BrushType::Smooth:   return "Smooth";
        case BrushType::Flatten:  return "Flatten";
        case BrushType::Pinch:    return "Pinch";
        case BrushType::Inflate:  return "Inflate";
        case BrushType::Grab:     return "Grab";
        case BrushType::Clone:    return "Clone";
        default:                  return "Unknown";
    }
}

const char* GetFalloffTypeName(FalloffType type) {
    switch (type) {
        case FalloffType::Linear:   return "Linear";
        case FalloffType::Smooth:   return "Smooth";
        case FalloffType::Sharp:    return "Sharp";
        case FalloffType::Constant: return "Constant";
        default:                    return "Unknown";
    }
}

const char* GetSymmetryModeName(SymmetryMode mode) {
    switch (mode) {
        case SymmetryMode::None:   return "None";
        case SymmetryMode::X:      return "X Mirror";
        case SymmetryMode::Y:      return "Y Mirror";
        case SymmetryMode::Z:      return "Z Mirror";
        case SymmetryMode::Radial: return "Radial";
        default:                   return "Multiple";
    }
}

} // namespace Nova
