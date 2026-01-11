#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "ProcGenGraph.hpp"

namespace Nova {
namespace ProcGen {

/**
 * @brief Biome definition in world template
 */
struct BiomeDefinition {
    int id = 0;
    std::string name;
    std::string description;
    glm::vec3 color = glm::vec3(0.5f);

    // Climate constraints
    float minTemperature = -10.0f;
    float maxTemperature = 30.0f;
    float minPrecipitation = 0.0f;
    float maxPrecipitation = 2000.0f;
    float minElevation = 0.0f;
    float maxElevation = 3000.0f;

    // Vegetation
    std::vector<std::string> treeTypes;
    std::vector<std::string> plantTypes;
    float vegetationDensity = 0.5f;

    // Resources
    std::vector<std::string> oreTypes;
    std::vector<float> oreDensities;

    nlohmann::json ToJson() const;
    static BiomeDefinition FromJson(const nlohmann::json& json);
};

/**
 * @brief Resource distribution definition
 */
struct ResourceDefinition {
    std::string resourceType; // "iron_ore", "gold_ore", "diamond", etc.
    float density = 0.1f;
    float minHeight = 0.0f;
    float maxHeight = 100.0f;
    float minSlope = 0.0f;
    float maxSlope = 90.0f;
    float clusterSize = 5.0f;
    std::vector<int> allowedBiomes;

    nlohmann::json ToJson() const;
    static ResourceDefinition FromJson(const nlohmann::json& json);
};

/**
 * @brief Structure generation rules
 */
struct StructureDefinition {
    std::string structureType; // "ruins", "temple", "dungeon", etc.
    float density = 0.01f;
    float minDistance = 500.0f; // Minimum distance between structures
    float minSize = 10.0f;
    float maxSize = 50.0f;
    float maxSlope = 15.0f;
    std::vector<int> allowedBiomes;
    std::vector<std::string> variants;

    nlohmann::json ToJson() const;
    static StructureDefinition FromJson(const nlohmann::json& json);
};

/**
 * @brief Climate configuration
 */
struct ClimateConfig {
    float equatorTemperature = 30.0f;
    float poleTemperature = -20.0f;
    float temperatureVariation = 5.0f;
    float elevationTemperatureGradient = -6.5f; // degrees per 1000m

    float baseRainfall = 1000.0f;
    float oceanRainfallBonus = 500.0f;
    float mountainRainShadow = 0.5f;

    std::string windPattern = "westerlies"; // "westerlies", "trade", "monsoon"

    nlohmann::json ToJson() const;
    static ClimateConfig FromJson(const nlohmann::json& json);
};

/**
 * @brief World template - Complete procedural world definition
 */
class WorldTemplate {
public:
    WorldTemplate() = default;

    /**
     * @brief Load template from JSON file
     */
    static std::shared_ptr<WorldTemplate> LoadFromFile(const std::string& filePath);

    /**
     * @brief Load template from JSON
     */
    static std::shared_ptr<WorldTemplate> LoadFromJson(const nlohmann::json& json);

    /**
     * @brief Save template to JSON file
     */
    bool SaveToFile(const std::string& filePath) const;

    /**
     * @brief Save template to JSON
     */
    nlohmann::json SaveToJson() const;

    /**
     * @brief Validate template structure
     */
    bool Validate(std::vector<std::string>& errors) const;

    /**
     * @brief Create procedural generation graph from this template
     */
    std::shared_ptr<ProcGenGraph> CreateProcGenGraph() const;

    // Template properties
    std::string name;
    std::string description;
    std::string version = "1.0.0";
    int seed = 12345;

    // World size
    glm::ivec2 worldSize = glm::ivec2(10000, 10000);
    int maxHeight = 255;

    // Proc gen graph (serialized visual script)
    nlohmann::json procGenGraphJson;

    // Biomes
    std::vector<BiomeDefinition> biomes;

    // Resources
    std::vector<ResourceDefinition> ores;
    std::vector<ResourceDefinition> vegetation;
    std::vector<ResourceDefinition> water;

    // Structures
    std::vector<StructureDefinition> ruins;
    std::vector<StructureDefinition> ancients;
    std::vector<StructureDefinition> buildings;

    // Climate
    ClimateConfig climate;

    // Generation parameters
    float erosionStrength = 0.3f;
    int erosionIterations = 100;
    float terrainScale = 1.0f;
    float terrainAmplitude = 100.0f;

    // Metadata
    std::string author;
    std::string createdDate;
    std::string modifiedDate;
    std::vector<std::string> tags;
};

/**
 * @brief Template library - Manages available world templates
 */
class TemplateLibrary {
public:
    static TemplateLibrary& Instance();

    /**
     * @brief Load all templates from directory
     */
    void LoadTemplatesFromDirectory(const std::string& directory);

    /**
     * @brief Register a template
     */
    void RegisterTemplate(const std::string& id, std::shared_ptr<WorldTemplate> templ);

    /**
     * @brief Get template by ID
     */
    std::shared_ptr<WorldTemplate> GetTemplate(const std::string& id) const;

    /**
     * @brief Get all template IDs
     */
    std::vector<std::string> GetTemplateIds() const;

    /**
     * @brief Get templates by tag
     */
    std::vector<std::shared_ptr<WorldTemplate>> GetTemplatesByTag(const std::string& tag) const;

    /**
     * @brief Search templates
     */
    std::vector<std::shared_ptr<WorldTemplate>> SearchTemplates(const std::string& query) const;

    /**
     * @brief Load built-in templates
     */
    void LoadBuiltInTemplates();

private:
    TemplateLibrary() = default;
    std::unordered_map<std::string, std::shared_ptr<WorldTemplate>> m_templates;
};

/**
 * @brief Template builder - Fluent API for creating templates programmatically
 */
class TemplateBuilder {
public:
    TemplateBuilder(const std::string& name);

    TemplateBuilder& WithDescription(const std::string& desc);
    TemplateBuilder& WithSeed(int seed);
    TemplateBuilder& WithWorldSize(int width, int height);
    TemplateBuilder& WithBiome(const BiomeDefinition& biome);
    TemplateBuilder& WithResource(const ResourceDefinition& resource);
    TemplateBuilder& WithStructure(const StructureDefinition& structure);
    TemplateBuilder& WithClimate(const ClimateConfig& climate);
    TemplateBuilder& WithErosion(float strength, int iterations);
    TemplateBuilder& WithTag(const std::string& tag);

    std::shared_ptr<WorldTemplate> Build();

private:
    std::shared_ptr<WorldTemplate> m_template;
};

} // namespace ProcGen
} // namespace Nova
