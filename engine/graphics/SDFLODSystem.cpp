#include "SDFLODSystem.hpp"
#include "../core/Camera.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <chrono>
#include <cmath>

namespace Nova {

// ============================================================================
// SDFLODConfiguration Implementation
// ============================================================================

int SDFLODConfiguration::GetLODLevelForDistance(float distance) const {
    if (enableCulling && distance >= maxDistance) {
        return -1; // Culled
    }

    if (levels.empty()) {
        return 0;
    }

    // Find appropriate LOD level
    int lodLevel = 0;
    for (size_t i = 0; i < levels.size(); ++i) {
        if (distance >= levels[i].distance) {
            lodLevel = static_cast<int>(i);
        } else {
            break;
        }
    }

    return lodLevel;
}

float SDFLODConfiguration::GetLODBlend(float distance, int& outLod0, int& outLod1) const {
    if (levels.empty()) {
        outLod0 = 0;
        outLod1 = 0;
        return 0.0f;
    }

    outLod0 = GetLODLevelForDistance(distance);
    if (outLod0 < 0) {
        outLod1 = outLod0;
        return 0.0f;
    }

    // Check if we're in transition zone
    if (outLod0 < static_cast<int>(levels.size()) - 1) {
        float currentDist = levels[outLod0].distance;
        float nextDist = levels[outLod0 + 1].distance;
        float transitionStart = nextDist - transitionWidth;

        if (distance >= transitionStart && distance < nextDist) {
            outLod1 = outLod0 + 1;
            return (distance - transitionStart) / transitionWidth;
        }
    }

    outLod1 = outLod0;
    return 0.0f;
}

SDFLODConfiguration SDFLODConfiguration::CreateDefault() {
    SDFLODConfiguration config;

    // Default 4 LOD levels
    SDFLODLevel lod0, lod1, lod2, lod3;

    lod0.distance = 0.0f;
    lod0.primitiveCount = 40;

    lod1.distance = 10.0f;
    lod1.primitiveCount = 12;

    lod2.distance = 25.0f;
    lod2.primitiveCount = 6;

    lod3.distance = 50.0f;
    lod3.primitiveCount = 3;

    config.levels = {lod0, lod1, lod2, lod3};
    config.transitionWidth = 5.0f;
    config.maxDistance = 200.0f;

    return config;
}

SDFLODConfiguration SDFLODConfiguration::CreateForQuality(const std::string& quality) {
    SDFLODConfiguration config;

    if (quality == "low") {
        config.levels = {
            {0.0f, 20},
            {15.0f, 8},
            {40.0f, 3},
            {80.0f, 1}
        };
        config.maxDistance = 150.0f;
        config.transitionWidth = 3.0f;
    } else if (quality == "medium") {
        config.levels = {
            {0.0f, 40},
            {10.0f, 12},
            {25.0f, 6},
            {50.0f, 3}
        };
        config.maxDistance = 200.0f;
        config.transitionWidth = 5.0f;
    } else if (quality == "high") {
        config.levels = {
            {0.0f, 80},
            {8.0f, 40},
            {20.0f, 12},
            {45.0f, 6}
        };
        config.maxDistance = 300.0f;
        config.transitionWidth = 4.0f;
    } else if (quality == "ultra") {
        config.levels = {
            {0.0f, 120},
            {5.0f, 80},
            {15.0f, 40},
            {35.0f, 12}
        };
        config.maxDistance = 500.0f;
        config.transitionWidth = 3.0f;
    } else {
        return CreateDefault();
    }

    return config;
}

// ============================================================================
// SDFLODStatistics Implementation
// ============================================================================

void SDFLODStatistics::Reset() {
    totalModels = 0;
    visibleModels = 0;
    culledModels = 0;
    modelsPerLOD.clear();
    totalPrimitivesRendered = 0;
    totalPrimitivesAvailable = 0;
    avgDistance = 0.0f;
    minDistance = FLT_MAX;
    maxDistance = 0.0f;
    lodTransitionsThisFrame = 0;
    updateTimeMs = 0.0f;
}

std::string SDFLODStatistics::ToString() const {
    std::stringstream ss;
    ss << "LOD Statistics:\n";
    ss << "  Total Models: " << totalModels << "\n";
    ss << "  Visible: " << visibleModels << ", Culled: " << culledModels << "\n";
    ss << "  Primitives: " << totalPrimitivesRendered << " / " << totalPrimitivesAvailable << "\n";
    ss << "  Distance Range: " << minDistance << " - " << maxDistance << " (avg: " << avgDistance << ")\n";
    ss << "  LOD Transitions: " << lodTransitionsThisFrame << "\n";
    ss << "  Update Time: " << updateTimeMs << " ms\n";
    ss << "  Models per LOD:";
    for (size_t i = 0; i < modelsPerLOD.size(); ++i) {
        ss << " LOD" << i << "=" << modelsPerLOD[i];
    }
    ss << "\n";
    return ss.str();
}

// ============================================================================
// SDFLODSystem Implementation
// ============================================================================

SDFLODSystem::SDFLODSystem() {
    m_defaultConfig = SDFLODConfiguration::CreateDefault();
}

SDFLODSystem::~SDFLODSystem() = default;

// ============================================================================
// Configuration
// ============================================================================

void SDFLODSystem::SetModelLODConfig(uint32_t modelId, const SDFLODConfiguration& config) {
    m_modelConfigs[modelId] = config;

    // Sort LOD levels by distance
    std::sort(m_modelConfigs[modelId].levels.begin(), m_modelConfigs[modelId].levels.end());
}

const SDFLODConfiguration* SDFLODSystem::GetModelLODConfig(uint32_t modelId) const {
    auto it = m_modelConfigs.find(modelId);
    if (it != m_modelConfigs.end()) {
        return &it->second;
    }
    return nullptr;
}

void SDFLODSystem::RemoveModelLODConfig(uint32_t modelId) {
    m_modelConfigs.erase(modelId);
}

void SDFLODSystem::SetDefaultLODConfig(const SDFLODConfiguration& config) {
    m_defaultConfig = config;
    std::sort(m_defaultConfig.levels.begin(), m_defaultConfig.levels.end());
}

// ============================================================================
// Global Settings
// ============================================================================

void SDFLODSystem::SetGlobalSettings(const SDFLODGlobalSettings& settings) {
    m_globalSettings = settings;
}

void SDFLODSystem::ApplyQualityPreset(const std::string& preset) {
    m_globalSettings.qualityPreset = preset;
    m_defaultConfig = SDFLODConfiguration::CreateForQuality(preset);

    if (preset == "low") {
        m_globalSettings.lodBias = 1.0f;
        m_globalSettings.maxPrimitivesPerFrame = 5000;
    } else if (preset == "medium") {
        m_globalSettings.lodBias = 0.0f;
        m_globalSettings.maxPrimitivesPerFrame = 10000;
    } else if (preset == "high") {
        m_globalSettings.lodBias = -0.5f;
        m_globalSettings.maxPrimitivesPerFrame = 20000;
    } else if (preset == "ultra") {
        m_globalSettings.lodBias = -1.0f;
        m_globalSettings.maxPrimitivesPerFrame = 50000;
    }
}

// ============================================================================
// Model Registration
// ============================================================================

void SDFLODSystem::RegisterModel(uint32_t modelId, SDFModel* model, const glm::vec3& position) {
    SDFLODState state;
    state.modelId = modelId;
    state.position = position;
    state.visible = true;

    m_modelStates[modelId] = state;
    m_models[modelId] = model;
}

void SDFLODSystem::UnregisterModel(uint32_t modelId) {
    m_modelStates.erase(modelId);
    m_models.erase(modelId);
    m_forcedLODs.erase(modelId);
}

void SDFLODSystem::UpdateModelPosition(uint32_t modelId, const glm::vec3& position) {
    auto it = m_modelStates.find(modelId);
    if (it != m_modelStates.end()) {
        it->second.position = position;
    }
}

bool SDFLODSystem::IsModelRegistered(uint32_t modelId) const {
    return m_modelStates.find(modelId) != m_modelStates.end();
}

// ============================================================================
// Update
// ============================================================================

void SDFLODSystem::Update(const glm::vec3& cameraPosition, float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();

    m_statistics.Reset();
    m_statistics.totalModels = static_cast<int>(m_modelStates.size());

    // Ensure modelsPerLOD has enough space
    int maxLOD = 0;
    for (const auto& [id, config] : m_modelConfigs) {
        maxLOD = std::max(maxLOD, static_cast<int>(config.levels.size()));
    }
    maxLOD = std::max(maxLOD, static_cast<int>(m_defaultConfig.levels.size()));
    m_statistics.modelsPerLOD.resize(maxLOD, 0);

    // Update each model
    for (auto& [modelId, state] : m_modelStates) {
        // Get LOD config
        const SDFLODConfiguration* config = GetModelLODConfig(modelId);
        if (!config) {
            config = &m_defaultConfig;
        }

        // Update model LOD
        UpdateModelLOD(state, *config, cameraPosition, deltaTime);

        // Update statistics
        if (state.culled) {
            m_statistics.culledModels++;
        } else {
            m_statistics.visibleModels++;
            if (state.currentLOD >= 0 && state.currentLOD < static_cast<int>(m_statistics.modelsPerLOD.size())) {
                m_statistics.modelsPerLOD[state.currentLOD]++;
            }

            m_statistics.avgDistance += state.distanceToCamera;
            m_statistics.minDistance = std::min(m_statistics.minDistance, state.distanceToCamera);
            m_statistics.maxDistance = std::max(m_statistics.maxDistance, state.distanceToCamera);

            // Count primitives
            if (state.currentLOD >= 0 && state.currentLOD < static_cast<int>(config->levels.size())) {
                m_statistics.totalPrimitivesRendered += config->levels[state.currentLOD].primitiveCount;
            }
        }
    }

    if (m_statistics.visibleModels > 0) {
        m_statistics.avgDistance /= m_statistics.visibleModels;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    m_statistics.updateTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
}

void SDFLODSystem::Update(const Camera& camera, float deltaTime) {
    // Extract camera position
    // Note: Assuming Camera has GetPosition() method
    // Update(camera.GetPosition(), deltaTime);

    // For now, use a default position
    Update(glm::vec3(0.0f), deltaTime);
}

void SDFLODSystem::UpdateModelLOD(SDFLODState& state, const SDFLODConfiguration& config,
                                 const glm::vec3& cameraPosition, float deltaTime) {
    // Calculate distance
    state.distanceToCamera = glm::length(state.position - cameraPosition);
    float adjustedDistance = CalculateAdjustedDistance(state.distanceToCamera);

    // Check if forced LOD
    auto forcedIt = m_forcedLODs.find(state.modelId);
    if (forcedIt != m_forcedLODs.end()) {
        state.currentLOD = forcedIt->second;
        state.targetLOD = state.currentLOD;
        state.lodBlend = 0.0f;
        state.culled = false;
        return;
    }

    // Get target LOD
    int targetLOD = config.GetLODLevelForDistance(adjustedDistance);

    if (targetLOD < 0) {
        // Model is culled
        state.culled = true;
        state.visible = false;
        return;
    }

    state.culled = false;
    state.visible = true;

    // Check if LOD should change
    if (targetLOD != state.currentLOD) {
        if (ShouldTransitionLOD(state, targetLOD)) {
            // Start transition
            if (m_globalSettings.logLODChanges) {
                std::cout << "Model " << state.modelId << ": LOD " << state.currentLOD
                         << " -> " << targetLOD << " (distance: " << state.distanceToCamera << ")\n";
            }

            state.targetLOD = targetLOD;
            state.transitionProgress = 0.0f;
            state.timeInLOD = 0.0f;
            m_statistics.lodTransitionsThisFrame++;
        }
    }

    // Update transition
    if (m_globalSettings.enableTransitions && state.currentLOD != state.targetLOD) {
        state.transitionProgress += deltaTime * m_globalSettings.transitionSpeed;

        if (state.transitionProgress >= 1.0f) {
            // Transition complete
            state.currentLOD = state.targetLOD;
            state.transitionProgress = 0.0f;
            state.lodBlend = 0.0f;
        } else {
            state.lodBlend = state.transitionProgress;
        }
    } else {
        state.currentLOD = targetLOD;
        state.lodBlend = 0.0f;
    }

    state.timeInLOD += deltaTime;
}

float SDFLODSystem::CalculateAdjustedDistance(float actualDistance) const {
    // Apply LOD bias and scale
    float adjusted = actualDistance * m_globalSettings.lodScale;

    // LOD bias: positive = use higher detail (subtract from distance)
    adjusted -= m_globalSettings.lodBias * 10.0f;

    return std::max(0.0f, adjusted);
}

bool SDFLODSystem::ShouldTransitionLOD(const SDFLODState& state, int newLOD) const {
    // Check minimum time between transitions
    if (state.timeInLOD < m_globalSettings.hysteresisTime) {
        return false;
    }

    // Check hysteresis: if switching back to higher detail, require extra distance
    if (newLOD < state.currentLOD) {
        // Moving to higher detail - apply hysteresis
        // (Already handled by distance calculation, so allow)
        return true;
    }

    return true;
}

// ============================================================================
// Query
// ============================================================================

const SDFLODState* SDFLODSystem::GetModelLODState(uint32_t modelId) const {
    auto it = m_modelStates.find(modelId);
    if (it != m_modelStates.end()) {
        return &it->second;
    }
    return nullptr;
}

int SDFLODSystem::GetCurrentLOD(uint32_t modelId) const {
    const SDFLODState* state = GetModelLODState(modelId);
    return state ? state->currentLOD : -1;
}

bool SDFLODSystem::IsModelVisible(uint32_t modelId) const {
    const SDFLODState* state = GetModelLODState(modelId);
    return state && state->visible && !state->culled;
}

std::vector<const SDFPrimitive*> SDFLODSystem::GetVisiblePrimitives(uint32_t modelId) const {
    std::vector<const SDFPrimitive*> result;

    const SDFLODState* state = GetModelLODState(modelId);
    if (!state || !state->visible || state->culled) {
        return result;
    }

    auto modelIt = m_models.find(modelId);
    if (modelIt == m_models.end() || !modelIt->second) {
        return result;
    }

    const SDFModel* model = modelIt->second;
    const SDFLODConfiguration* config = GetModelLODConfig(modelId);
    if (!config) {
        config = &m_defaultConfig;
    }

    // Get primitives for current LOD
    if (state->currentLOD >= 0 && state->currentLOD < static_cast<int>(config->levels.size())) {
        const SDFLODLevel& level = config->levels[state->currentLOD];

        // Get all primitives from model
        auto allPrimitives = model->GetAllPrimitives();

        // Return primitives according to LOD level
        for (int idx : level.primitiveIndices) {
            if (idx >= 0 && idx < static_cast<int>(allPrimitives.size())) {
                result.push_back(allPrimitives[idx]);
            }
        }
    }

    return result;
}

// ============================================================================
// Utilities
// ============================================================================

void SDFLODSystem::ForceLOD(uint32_t modelId, int lodLevel) {
    m_forcedLODs[modelId] = lodLevel;
}

void SDFLODSystem::ClearForcedLOD(uint32_t modelId) {
    m_forcedLODs.erase(modelId);
}

glm::vec3 SDFLODSystem::GetLODDebugColor(int lodLevel) {
    static const std::vector<glm::vec3> colors = {
        {0.0f, 1.0f, 0.0f},  // LOD0 - Green (highest detail)
        {1.0f, 1.0f, 0.0f},  // LOD1 - Yellow
        {1.0f, 0.5f, 0.0f},  // LOD2 - Orange
        {1.0f, 0.0f, 0.0f},  // LOD3 - Red (lowest detail)
        {0.5f, 0.0f, 0.5f},  // LOD4+ - Purple
    };

    if (lodLevel < 0) {
        return {0.2f, 0.2f, 0.2f}; // Culled - Dark gray
    }

    if (lodLevel < static_cast<int>(colors.size())) {
        return colors[lodLevel];
    }

    return colors.back();
}

} // namespace Nova
