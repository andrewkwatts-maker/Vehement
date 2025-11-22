#include "PCGPipeline.hpp"
#include "TerrainGenerator.hpp"
#include "RoadGenerator.hpp"
#include "BuildingGenerator.hpp"
#include "FoliageGenerator.hpp"
#include "EntitySpawner.hpp"
#include "../world/TileMap.hpp"
#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>

namespace Vehement {

// ============================================================================
// PCGPipeline Implementation
// ============================================================================

PCGPipeline::PCGPipeline()
    : m_config(PCGPipelineConfig::Default())
    , m_terrainGen(std::make_unique<TerrainGenerator>())
    , m_roadGen(std::make_unique<RoadGenerator>())
    , m_buildingGen(std::make_unique<BuildingGenerator>())
    , m_foliageGen(std::make_unique<FoliageGenerator>())
    , m_entitySpawner(std::make_unique<EntitySpawner>())
{
}

PCGPipeline::~PCGPipeline() {
    Cancel();
}

bool PCGPipeline::Initialize(const PCGPipelineConfig& config) {
    m_config = config;

    // Initialize script manager if using scripts
    for (const auto& stage : config.stages) {
        if (stage.useScript && !stage.scriptName.empty()) {
            PCGScriptManager::Instance().Initialize();
            break;
        }
    }

    return true;
}

void PCGPipeline::SetConfig(const PCGPipelineConfig& config) {
    m_config = config;
}

void PCGPipeline::SetSeed(uint64_t seed) {
    m_config.seed = seed;
}

void PCGPipeline::SetGeoData(std::shared_ptr<GeoTileData> geoData) {
    m_config.geoData = std::move(geoData);
}

// ========== Stage Configuration ==========

void PCGPipeline::SetStageEnabled(PCGStage stage, bool enabled) {
    if (auto* config = m_config.GetStageConfig(stage)) {
        config->enabled = enabled;
    }
}

bool PCGPipeline::IsStageEnabled(PCGStage stage) const {
    if (const auto* config = m_config.GetStageConfig(stage)) {
        return config->enabled;
    }
    return false;
}

void PCGPipeline::SetStageScript(PCGStage stage, const std::string& scriptName) {
    if (auto* config = m_config.GetStageConfig(stage)) {
        config->useScript = true;
        config->scriptName = scriptName;
    }
}

void PCGPipeline::ClearStageScript(PCGStage stage) {
    if (auto* config = m_config.GetStageConfig(stage)) {
        config->useScript = false;
        config->scriptName.clear();
    }
}

void PCGPipeline::SetStageParam(PCGStage stage, const std::string& key, const std::string& value) {
    if (auto* config = m_config.GetStageConfig(stage)) {
        config->params[key] = value;
    }
}

// ========== Generator Access ==========

TerrainGenerator& PCGPipeline::GetTerrainGenerator() {
    return *m_terrainGen;
}

RoadGenerator& PCGPipeline::GetRoadGenerator() {
    return *m_roadGen;
}

BuildingGenerator& PCGPipeline::GetBuildingGenerator() {
    return *m_buildingGen;
}

FoliageGenerator& PCGPipeline::GetFoliageGenerator() {
    return *m_foliageGen;
}

EntitySpawner& PCGPipeline::GetEntitySpawner() {
    return *m_entitySpawner;
}

// ========== Generation ==========

PCGPipelineResult PCGPipeline::Generate() {
    // Create new context
    uint64_t seed = m_config.seed;
    if (seed == 0) {
        seed = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }

    m_lastContext = std::make_unique<PCGContext>(m_config.width, m_config.height, seed);

    if (m_config.geoData) {
        m_lastContext->SetGeoData(m_config.geoData);
    }

    return Generate(*m_lastContext);
}

PCGPipelineResult PCGPipeline::Generate(PCGContext& context) {
    PCGPipelineResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    m_running = true;
    m_cancelled = false;
    m_currentProgress = 0.0f;

    float totalWeight = CalculateTotalWeight();
    float progressAccum = 0.0f;

    // Run each enabled stage
    for (const auto& stageConfig : m_config.stages) {
        if (m_cancelled) {
            result.errorMessage = "Generation cancelled";
            break;
        }

        if (!stageConfig.enabled) {
            continue;
        }

        ReportProgress(progressAccum / totalWeight, stageConfig.stage,
                       std::string("Starting ") + GetPCGStageName(stageConfig.stage));

        PCGStageResult stageResult = RunStage(stageConfig.stage, context);
        result.stageResults.push_back(stageResult);

        if (!stageResult.success) {
            result.errorMessage = "Stage " + std::string(GetPCGStageName(stageConfig.stage)) +
                                  " failed: " + stageResult.errorMessage;
            break;
        }

        progressAccum += stageConfig.weight;
        ReportProgress(progressAccum / totalWeight, stageConfig.stage,
                       std::string("Completed ") + GetPCGStageName(stageConfig.stage));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.totalTime = std::chrono::duration<float>(endTime - startTime).count();

    // Gather statistics
    result.tilesGenerated = context.GetWidth() * context.GetHeight();
    result.entitiesSpawned = static_cast<int>(context.GetEntitySpawns().size());
    result.foliageSpawned = static_cast<int>(context.GetFoliageSpawns().size());

    // Count buildings and roads from stage results
    for (const auto& sr : result.stageResults) {
        // Could track these individually in stage results
    }

    result.success = result.errorMessage.empty();
    m_running = false;

    ReportProgress(1.0f, PCGStage::None, "Generation complete");

    return result;
}

PCGPipelineResult PCGPipeline::GeneratePreview() {
    PCGMode originalMode = m_config.mode;
    m_config.mode = PCGMode::Preview;

    // Use smaller dimensions for preview
    int originalWidth = m_config.width;
    int originalHeight = m_config.height;

    // Preview at half resolution
    m_config.width = std::max(16, m_config.width / 2);
    m_config.height = std::max(16, m_config.height / 2);

    PCGPipelineResult result = Generate();

    // Restore original settings
    m_config.mode = originalMode;
    m_config.width = originalWidth;
    m_config.height = originalHeight;

    return result;
}

PCGStageResult PCGPipeline::RunStage(PCGStage stage, PCGContext& context) {
    PCGStageResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    const auto* stageConfig = m_config.GetStageConfig(stage);
    if (!stageConfig) {
        result.errorMessage = "Stage not configured";
        return result;
    }

    // Apply stage parameters to generator
    auto applyParams = [&](PCGStageGenerator* gen) {
        for (const auto& [key, value] : stageConfig->params) {
            gen->SetParam(key, value);
        }
    };

    try {
        // Use script if configured
        if (stageConfig->useScript && !stageConfig->scriptName.empty()) {
            PCGScript* script = PCGScriptManager::Instance().GetScript(stageConfig->scriptName);
            if (script) {
                PCGScriptResult scriptResult;
                if (m_config.mode == PCGMode::Preview) {
                    scriptResult = script->Preview(context);
                } else {
                    scriptResult = script->Generate(context);
                }

                result.success = scriptResult.success;
                result.errorMessage = scriptResult.errorMessage;
                result.itemsGenerated = scriptResult.tilesModified;
            } else {
                result.errorMessage = "Script not found: " + stageConfig->scriptName;
            }
        }
        // Use native generator
        else {
            switch (stage) {
                case PCGStage::Terrain:
                    applyParams(m_terrainGen.get());
                    result = m_terrainGen->Generate(context, m_config.mode);
                    break;

                case PCGStage::Roads:
                    applyParams(m_roadGen.get());
                    result = m_roadGen->Generate(context, m_config.mode);
                    break;

                case PCGStage::Buildings:
                    applyParams(m_buildingGen.get());
                    result = m_buildingGen->Generate(context, m_config.mode);
                    break;

                case PCGStage::Foliage:
                    applyParams(m_foliageGen.get());
                    result = m_foliageGen->Generate(context, m_config.mode);
                    break;

                case PCGStage::Entities:
                    applyParams(m_entitySpawner.get());
                    result = m_entitySpawner->Generate(context, m_config.mode);
                    break;

                case PCGStage::PostProcess:
                    // Post-process pass - could apply additional filters
                    result.success = true;
                    break;

                default:
                    result.errorMessage = "Unknown stage";
                    break;
            }
        }
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTime = std::chrono::duration<float>(endTime - startTime).count();

    return result;
}

void PCGPipeline::ApplyToTileMap(TileMap& tileMap, int offsetX, int offsetY) {
    if (!m_lastContext) return;

    const auto& tiles = m_lastContext->GetTiles();
    int width = m_lastContext->GetWidth();
    int height = m_lastContext->GetHeight();

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int targetX = offsetX + x;
            int targetY = offsetY + y;

            if (tileMap.IsInBounds(targetX, targetY)) {
                tileMap.SetTile(targetX, targetY, tiles[y * width + x]);
            }
        }
    }

    tileMap.MarkDirty(offsetX, offsetY, width, height);
}

// ========== Async Generation ==========

void PCGPipeline::SetProgressCallback(ProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

bool PCGPipeline::StartAsync() {
    if (m_running) return false;

    m_running = true;
    m_cancelled = false;

    std::thread([this]() {
        m_asyncResult = Generate();
    }).detach();

    return true;
}

void PCGPipeline::Cancel() {
    m_cancelled = true;
}

bool PCGPipeline::Wait(int timeoutMs) {
    auto startTime = std::chrono::steady_clock::now();

    while (m_running) {
        if (timeoutMs >= 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return true;
}

PCGPipelineResult PCGPipeline::GetAsyncResult() {
    return m_asyncResult;
}

// ========== Serialization ==========

std::string PCGPipeline::SaveConfigToJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"width\": " << m_config.width << ",\n";
    json << "  \"height\": " << m_config.height << ",\n";
    json << "  \"seed\": " << m_config.seed << ",\n";
    json << "  \"mode\": \"" << (m_config.mode == PCGMode::Preview ? "preview" : "final") << "\",\n";
    json << "  \"stages\": [\n";

    bool first = true;
    for (const auto& stage : m_config.stages) {
        if (!first) json << ",\n";
        first = false;

        json << "    {\n";
        json << "      \"stage\": \"" << GetPCGStageName(stage.stage) << "\",\n";
        json << "      \"enabled\": " << (stage.enabled ? "true" : "false") << ",\n";
        json << "      \"useScript\": " << (stage.useScript ? "true" : "false") << ",\n";
        json << "      \"scriptName\": \"" << stage.scriptName << "\",\n";
        json << "      \"weight\": " << stage.weight << ",\n";
        json << "      \"params\": {";

        bool firstParam = true;
        for (const auto& [key, value] : stage.params) {
            if (!firstParam) json << ", ";
            firstParam = false;
            json << "\"" << key << "\": \"" << value << "\"";
        }

        json << "}\n";
        json << "    }";
    }

    json << "\n  ]\n";
    json << "}\n";

    return json.str();
}

bool PCGPipeline::LoadConfigFromJson(const std::string& /*json*/) {
    // JSON parsing would go here
    // For now, just return true
    return true;
}

bool PCGPipeline::SaveGenerationToFile(const std::string& /*filepath*/) const {
    // Would save the generated tile data
    return true;
}

// ========== Internal Helpers ==========

void PCGPipeline::ReportProgress(float progress, PCGStage stage, const std::string& message) {
    m_currentProgress = progress;
    if (m_progressCallback) {
        m_progressCallback(progress, stage, message);
    }
}

float PCGPipeline::CalculateTotalWeight() const {
    float total = 0.0f;
    for (const auto& stage : m_config.stages) {
        if (stage.enabled) {
            total += stage.weight;
        }
    }
    return total > 0.0f ? total : 1.0f;
}

} // namespace Vehement
