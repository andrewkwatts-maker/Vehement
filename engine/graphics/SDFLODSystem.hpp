#pragma once

/**
 * @file SDFLODSystem.hpp
 * @brief Level-of-Detail (LOD) system for SDF primitive models
 *
 * Manages automatic LOD transitions for SDF models based on distance from camera.
 * Supports per-mesh LOD settings and project-wide defaults.
 *
 * Features:
 * - Distance-based LOD switching
 * - Smooth transitions with temporal dithering
 * - Per-model LOD configuration
 * - Global LOD settings and overrides
 * - LOD culling and visibility management
 * - Memory-efficient primitive streaming
 *
 * Example usage:
 * @code
 * SDFLODSystem lodSystem;
 *
 * // Configure model LOD
 * SDFLODConfiguration config;
 * config.levels = {
 *     {0.0f, 40},   // LOD0: 0-10m, 40 primitives
 *     {10.0f, 12},  // LOD1: 10-25m, 12 primitives
 *     {25.0f, 6},   // LOD2: 25-50m, 6 primitives
 *     {50.0f, 3}    // LOD3: 50m+, 3 primitives
 * };
 * lodSystem.SetModelLODConfig(modelId, config);
 *
 * // Update LODs each frame
 * lodSystem.Update(camera);
 * @endcode
 */

#include "../sdf/SDFModel.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

namespace Nova {

// Forward declarations
class Camera;
class SDFModel;
class SDFPrimitive;

/**
 * @brief Single LOD level definition
 */
struct SDFLODLevel {
    float distance = 0.0f;           // Minimum distance for this LOD
    int primitiveCount = 1;          // Number of primitives to render
    std::vector<int> primitiveIndices; // Which primitives to use (sorted by importance)

    bool operator<(const SDFLODLevel& other) const {
        return distance < other.distance;
    }
};

/**
 * @brief LOD configuration for a specific model
 */
struct SDFLODConfiguration {
    std::vector<SDFLODLevel> levels;

    // Transition settings
    float transitionWidth = 5.0f;    // Distance over which to blend LODs
    bool useDithering = true;        // Use temporal dithering for transitions
    float ditheringScale = 16.0f;    // Dithering pattern scale

    // Culling
    float maxDistance = 500.0f;      // Distance beyond which model is culled
    bool enableCulling = true;

    // Debug
    bool showLODColors = false;      // Color-code by LOD level

    /**
     * @brief Get LOD level for given distance
     * @return LOD level index, or -1 if culled
     */
    int GetLODLevelForDistance(float distance) const;

    /**
     * @brief Get LOD blend factor between levels
     * @param distance Distance from camera
     * @param outLod0 Output LOD level 0
     * @param outLod1 Output LOD level 1
     * @return Blend factor (0-1) between LOD0 and LOD1
     */
    float GetLODBlend(float distance, int& outLod0, int& outLod1) const;

    /**
     * @brief Create default LOD configuration
     */
    static SDFLODConfiguration CreateDefault();

    /**
     * @brief Create LOD configuration for specific quality level
     */
    static SDFLODConfiguration CreateForQuality(const std::string& quality); // "low", "medium", "high", "ultra"
};

/**
 * @brief Per-model LOD state (runtime)
 */
struct SDFLODState {
    uint32_t modelId = 0;
    glm::vec3 position{0.0f};
    float distanceToCamera = 0.0f;
    int currentLOD = 0;
    int targetLOD = 0;
    float lodBlend = 0.0f;           // 0 = current, 1 = target
    float transitionProgress = 0.0f; // For smooth transitions
    bool visible = true;
    bool culled = false;

    // Timing
    float timeInLOD = 0.0f;          // Time since last LOD change
    float lastTransitionTime = 0.0f;
};

/**
 * @brief Global LOD settings
 */
struct SDFLODGlobalSettings {
    // LOD bias
    float lodBias = 0.0f;            // Positive = higher LOD, negative = lower LOD
    float lodScale = 1.0f;           // Scale all LOD distances

    // Transition
    float transitionSpeed = 2.0f;    // How fast LOD transitions occur (seconds)
    bool enableTransitions = true;

    // Quality presets
    std::string qualityPreset = "medium"; // "low", "medium", "high", "ultra"

    // Performance
    int maxPrimitivesPerFrame = 10000; // Maximum total primitives to render
    bool enableDynamicLOD = true;      // Adjust LOD based on frame time

    // Hysteresis (prevent LOD thrashing)
    float hysteresisDistance = 2.0f;  // Extra distance before switching back
    float hysteresisTime = 0.5f;      // Minimum time between LOD changes

    // Debug
    bool visualizeDistances = false;
    bool logLODChanges = false;
};

/**
 * @brief Statistics for LOD system
 */
struct SDFLODStatistics {
    int totalModels = 0;
    int visibleModels = 0;
    int culledModels = 0;

    std::vector<int> modelsPerLOD;   // Count of models at each LOD level
    int totalPrimitivesRendered = 0;
    int totalPrimitivesAvailable = 0;

    float avgDistance = 0.0f;
    float minDistance = FLT_MAX;
    float maxDistance = 0.0f;

    int lodTransitionsThisFrame = 0;
    float updateTimeMs = 0.0f;

    void Reset();
    std::string ToString() const;
};

/**
 * @brief SDF LOD System
 *
 * Manages level-of-detail for SDF models based on distance from camera.
 * Automatically switches between LOD levels and handles smooth transitions.
 */
class SDFLODSystem {
public:
    SDFLODSystem();
    ~SDFLODSystem();

    // Non-copyable
    SDFLODSystem(const SDFLODSystem&) = delete;
    SDFLODSystem& operator=(const SDFLODSystem&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set LOD configuration for specific model
     */
    void SetModelLODConfig(uint32_t modelId, const SDFLODConfiguration& config);

    /**
     * @brief Get LOD configuration for model
     */
    const SDFLODConfiguration* GetModelLODConfig(uint32_t modelId) const;

    /**
     * @brief Remove LOD configuration for model
     */
    void RemoveModelLODConfig(uint32_t modelId);

    /**
     * @brief Set default LOD configuration (used when model has no specific config)
     */
    void SetDefaultLODConfig(const SDFLODConfiguration& config);

    /**
     * @brief Get default LOD configuration
     */
    const SDFLODConfiguration& GetDefaultLODConfig() const { return m_defaultConfig; }

    // =========================================================================
    // Global Settings
    // =========================================================================

    /**
     * @brief Get global LOD settings
     */
    SDFLODGlobalSettings& GetGlobalSettings() { return m_globalSettings; }
    const SDFLODGlobalSettings& GetGlobalSettings() const { return m_globalSettings; }

    /**
     * @brief Set global LOD settings
     */
    void SetGlobalSettings(const SDFLODGlobalSettings& settings);

    /**
     * @brief Apply quality preset
     */
    void ApplyQualityPreset(const std::string& preset);

    // =========================================================================
    // Model Registration
    // =========================================================================

    /**
     * @brief Register model with LOD system
     */
    void RegisterModel(uint32_t modelId, SDFModel* model, const glm::vec3& position);

    /**
     * @brief Unregister model from LOD system
     */
    void UnregisterModel(uint32_t modelId);

    /**
     * @brief Update model position
     */
    void UpdateModelPosition(uint32_t modelId, const glm::vec3& position);

    /**
     * @brief Check if model is registered
     */
    bool IsModelRegistered(uint32_t modelId) const;

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update LOD system (call once per frame)
     * @param cameraPosition Camera position for distance calculations
     * @param deltaTime Time since last update (seconds)
     */
    void Update(const glm::vec3& cameraPosition, float deltaTime);

    /**
     * @brief Update LOD system with camera object
     */
    void Update(const Camera& camera, float deltaTime);

    // =========================================================================
    // Query
    // =========================================================================

    /**
     * @brief Get current LOD state for model
     */
    const SDFLODState* GetModelLODState(uint32_t modelId) const;

    /**
     * @brief Get current LOD level for model
     */
    int GetCurrentLOD(uint32_t modelId) const;

    /**
     * @brief Check if model is visible (not culled)
     */
    bool IsModelVisible(uint32_t modelId) const;

    /**
     * @brief Get primitives to render for model at current LOD
     */
    std::vector<const SDFPrimitive*> GetVisiblePrimitives(uint32_t modelId) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get LOD statistics
     */
    const SDFLODStatistics& GetStatistics() const { return m_statistics; }

    /**
     * @brief Reset statistics
     */
    void ResetStatistics() { m_statistics.Reset(); }

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Force LOD level for model (debug)
     */
    void ForceLOD(uint32_t modelId, int lodLevel);

    /**
     * @brief Clear forced LOD
     */
    void ClearForcedLOD(uint32_t modelId);

    /**
     * @brief Get LOD color for visualization
     */
    static glm::vec3 GetLODDebugColor(int lodLevel);

private:
    // Update single model
    void UpdateModelLOD(SDFLODState& state, const SDFLODConfiguration& config,
                       const glm::vec3& cameraPosition, float deltaTime);

    // Calculate distance with LOD bias
    float CalculateAdjustedDistance(float actualDistance) const;

    // Check hysteresis
    bool ShouldTransitionLOD(const SDFLODState& state, int newLOD) const;

    // Model states
    std::unordered_map<uint32_t, SDFLODState> m_modelStates;

    // LOD configurations
    std::unordered_map<uint32_t, SDFLODConfiguration> m_modelConfigs;
    SDFLODConfiguration m_defaultConfig;

    // Model references
    std::unordered_map<uint32_t, SDFModel*> m_models;

    // Global settings
    SDFLODGlobalSettings m_globalSettings;

    // Statistics
    SDFLODStatistics m_statistics;

    // Forced LODs (debug)
    std::unordered_map<uint32_t, int> m_forcedLODs;
};

} // namespace Nova
