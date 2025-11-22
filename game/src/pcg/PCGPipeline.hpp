#pragma once

#include "PCGContext.hpp"
#include "PCGScript.hpp"
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <chrono>

namespace Vehement {

// Forward declarations
class TerrainGenerator;
class RoadGenerator;
class BuildingGenerator;
class FoliageGenerator;
class EntitySpawner;
class TileMap;

/**
 * @brief PCG generation mode
 */
enum class PCGMode {
    Preview,    // Fast, low-detail for editor preview
    Final       // Full detail for final generation
};

/**
 * @brief PCG pipeline stage identifier
 */
enum class PCGStage {
    None = 0,
    Terrain,
    Roads,
    Buildings,
    Foliage,
    Entities,
    PostProcess,
    COUNT
};

/**
 * @brief Get stage name
 */
inline const char* GetPCGStageName(PCGStage stage) {
    switch (stage) {
        case PCGStage::Terrain: return "Terrain";
        case PCGStage::Roads: return "Roads";
        case PCGStage::Buildings: return "Buildings";
        case PCGStage::Foliage: return "Foliage";
        case PCGStage::Entities: return "Entities";
        case PCGStage::PostProcess: return "PostProcess";
        default: return "None";
    }
}

/**
 * @brief Stage execution result
 */
struct PCGStageResult {
    bool success = false;
    std::string errorMessage;
    float executionTime = 0.0f;
    int itemsGenerated = 0;
};

/**
 * @brief Pipeline execution result
 */
struct PCGPipelineResult {
    bool success = false;
    std::string errorMessage;
    float totalTime = 0.0f;
    std::vector<PCGStageResult> stageResults;

    // Generation statistics
    int tilesGenerated = 0;
    int entitiesSpawned = 0;
    int foliageSpawned = 0;
    int buildingsPlaced = 0;
    int roadsGenerated = 0;
};

/**
 * @brief Stage configuration
 */
struct PCGStageConfig {
    PCGStage stage = PCGStage::None;
    bool enabled = true;
    bool useScript = false;
    std::string scriptName;
    float weight = 1.0f;  // For progress calculation

    // Stage-specific parameters (JSON-like)
    std::unordered_map<std::string, std::string> params;
};

/**
 * @brief Pipeline configuration
 */
struct PCGPipelineConfig {
    // Generation parameters
    int width = 64;
    int height = 64;
    uint64_t seed = 0;  // 0 = random

    // Mode
    PCGMode mode = PCGMode::Final;

    // Stage configurations
    std::vector<PCGStageConfig> stages;

    // Geographic data
    std::shared_ptr<GeoTileData> geoData;

    // Default stage order
    static PCGPipelineConfig Default() {
        PCGPipelineConfig config;
        config.stages = {
            {PCGStage::Terrain, true, false, "", 1.0f},
            {PCGStage::Roads, true, false, "", 0.5f},
            {PCGStage::Buildings, true, false, "", 1.5f},
            {PCGStage::Foliage, true, false, "", 0.8f},
            {PCGStage::Entities, true, false, "", 0.5f},
            {PCGStage::PostProcess, false, false, "", 0.2f}
        };
        return config;
    }

    // Get stage config
    PCGStageConfig* GetStageConfig(PCGStage stage) {
        for (auto& s : stages) {
            if (s.stage == stage) return &s;
        }
        return nullptr;
    }

    const PCGStageConfig* GetStageConfig(PCGStage stage) const {
        for (const auto& s : stages) {
            if (s.stage == stage) return &s;
        }
        return nullptr;
    }
};

/**
 * @brief Main PCG orchestration pipeline
 *
 * Pipeline stages:
 * 1. TerrainGen - Base terrain from elevation/biome data
 * 2. RoadGen - Road placement from real data
 * 3. BuildingGen - Building placement
 * 4. FoliageGen - Vegetation placement
 * 5. EntityGen - NPC and resource spawning
 * 6. PostProcess - Final adjustments
 *
 * Each stage can be:
 * - C++ native generator
 * - Python script
 * - Disabled
 *
 * Data flows between stages via PCGContext.
 */
class PCGPipeline {
public:
    PCGPipeline();
    ~PCGPipeline();

    // Non-copyable
    PCGPipeline(const PCGPipeline&) = delete;
    PCGPipeline& operator=(const PCGPipeline&) = delete;

    /**
     * @brief Initialize pipeline with configuration
     */
    bool Initialize(const PCGPipelineConfig& config);

    /**
     * @brief Set configuration
     */
    void SetConfig(const PCGPipelineConfig& config);

    /**
     * @brief Get current configuration
     */
    const PCGPipelineConfig& GetConfig() const { return m_config; }

    /**
     * @brief Set generation seed
     */
    void SetSeed(uint64_t seed);

    /**
     * @brief Get current seed
     */
    uint64_t GetSeed() const { return m_config.seed; }

    /**
     * @brief Set geographic data
     */
    void SetGeoData(std::shared_ptr<GeoTileData> geoData);

    // ========== Stage Configuration ==========

    /**
     * @brief Enable/disable a stage
     */
    void SetStageEnabled(PCGStage stage, bool enabled);

    /**
     * @brief Check if stage is enabled
     */
    bool IsStageEnabled(PCGStage stage) const;

    /**
     * @brief Set Python script for a stage
     */
    void SetStageScript(PCGStage stage, const std::string& scriptName);

    /**
     * @brief Clear script for a stage (use native generator)
     */
    void ClearStageScript(PCGStage stage);

    /**
     * @brief Set stage parameter
     */
    void SetStageParam(PCGStage stage, const std::string& key, const std::string& value);

    // ========== Generator Access ==========

    /**
     * @brief Get terrain generator for configuration
     */
    TerrainGenerator& GetTerrainGenerator();

    /**
     * @brief Get road generator for configuration
     */
    RoadGenerator& GetRoadGenerator();

    /**
     * @brief Get building generator for configuration
     */
    BuildingGenerator& GetBuildingGenerator();

    /**
     * @brief Get foliage generator for configuration
     */
    FoliageGenerator& GetFoliageGenerator();

    /**
     * @brief Get entity spawner for configuration
     */
    EntitySpawner& GetEntitySpawner();

    // ========== Generation ==========

    /**
     * @brief Generate full content
     * @return Generation result
     */
    PCGPipelineResult Generate();

    /**
     * @brief Generate with custom context
     * @param context Pre-configured context
     * @return Generation result
     */
    PCGPipelineResult Generate(PCGContext& context);

    /**
     * @brief Generate preview (fast, low-detail)
     * @return Generation result
     */
    PCGPipelineResult GeneratePreview();

    /**
     * @brief Run single stage
     * @param stage Stage to run
     * @param context Context to use
     * @return Stage result
     */
    PCGStageResult RunStage(PCGStage stage, PCGContext& context);

    /**
     * @brief Apply generation to tile map
     * @param tileMap Target tile map
     * @param offsetX X offset in map
     * @param offsetY Y offset in map
     */
    void ApplyToTileMap(TileMap& tileMap, int offsetX = 0, int offsetY = 0);

    /**
     * @brief Get last generated context
     */
    PCGContext* GetLastContext() { return m_lastContext.get(); }
    const PCGContext* GetLastContext() const { return m_lastContext.get(); }

    // ========== Async Generation ==========

    /**
     * @brief Progress callback
     */
    using ProgressCallback = std::function<void(float progress, PCGStage stage, const std::string& message)>;

    /**
     * @brief Set progress callback
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief Start async generation
     * @return true if started
     */
    bool StartAsync();

    /**
     * @brief Check if async generation is running
     */
    bool IsRunning() const { return m_running; }

    /**
     * @brief Cancel async generation
     */
    void Cancel();

    /**
     * @brief Wait for async generation to complete
     * @param timeoutMs Maximum wait time (-1 for infinite)
     * @return true if completed, false if timeout
     */
    bool Wait(int timeoutMs = -1);

    /**
     * @brief Get async generation result
     */
    PCGPipelineResult GetAsyncResult();

    // ========== Serialization ==========

    /**
     * @brief Save configuration to JSON
     */
    std::string SaveConfigToJson() const;

    /**
     * @brief Load configuration from JSON
     */
    bool LoadConfigFromJson(const std::string& json);

    /**
     * @brief Save last generation to file
     */
    bool SaveGenerationToFile(const std::string& filepath) const;

private:
    PCGPipelineConfig m_config;
    std::unique_ptr<PCGContext> m_lastContext;

    // Native generators
    std::unique_ptr<TerrainGenerator> m_terrainGen;
    std::unique_ptr<RoadGenerator> m_roadGen;
    std::unique_ptr<BuildingGenerator> m_buildingGen;
    std::unique_ptr<FoliageGenerator> m_foliageGen;
    std::unique_ptr<EntitySpawner> m_entitySpawner;

    // Progress tracking
    ProgressCallback m_progressCallback;
    float m_currentProgress = 0.0f;
    bool m_running = false;
    bool m_cancelled = false;

    // Async result
    PCGPipelineResult m_asyncResult;

    // Internal helpers
    void ReportProgress(float progress, PCGStage stage, const std::string& message);
    float CalculateTotalWeight() const;
};

/**
 * @brief Base class for PCG stage generators
 */
class PCGStageGenerator {
public:
    virtual ~PCGStageGenerator() = default;

    /**
     * @brief Generate content for this stage
     * @param context PCG context
     * @param mode Generation mode
     * @return Stage result
     */
    virtual PCGStageResult Generate(PCGContext& context, PCGMode mode) = 0;

    /**
     * @brief Get stage type
     */
    virtual PCGStage GetStage() const = 0;

    /**
     * @brief Get generator name
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief Set parameter
     */
    virtual void SetParam(const std::string& key, const std::string& value) {
        m_params[key] = value;
    }

    /**
     * @brief Get parameter
     */
    virtual std::string GetParam(const std::string& key, const std::string& defaultValue = "") const {
        auto it = m_params.find(key);
        return (it != m_params.end()) ? it->second : defaultValue;
    }

    /**
     * @brief Get parameter as int
     */
    int GetParamInt(const std::string& key, int defaultValue = 0) const {
        auto it = m_params.find(key);
        if (it != m_params.end()) {
            try { return std::stoi(it->second); }
            catch (...) {}
        }
        return defaultValue;
    }

    /**
     * @brief Get parameter as float
     */
    float GetParamFloat(const std::string& key, float defaultValue = 0.0f) const {
        auto it = m_params.find(key);
        if (it != m_params.end()) {
            try { return std::stof(it->second); }
            catch (...) {}
        }
        return defaultValue;
    }

    /**
     * @brief Get parameter as bool
     */
    bool GetParamBool(const std::string& key, bool defaultValue = false) const {
        auto it = m_params.find(key);
        if (it != m_params.end()) {
            return it->second == "true" || it->second == "1";
        }
        return defaultValue;
    }

protected:
    std::unordered_map<std::string, std::string> m_params;
};

} // namespace Vehement
