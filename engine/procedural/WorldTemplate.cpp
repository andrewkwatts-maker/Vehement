#include "WorldTemplate.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace Nova {
namespace ProcGen {

// =============================================================================
// BiomeDefinition Implementation
// =============================================================================

nlohmann::json BiomeDefinition::ToJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["name"] = name;
    j["description"] = description;
    j["color"] = {color.r, color.g, color.b};
    j["minTemperature"] = minTemperature;
    j["maxTemperature"] = maxTemperature;
    j["minPrecipitation"] = minPrecipitation;
    j["maxPrecipitation"] = maxPrecipitation;
    j["minElevation"] = minElevation;
    j["maxElevation"] = maxElevation;
    j["treeTypes"] = treeTypes;
    j["plantTypes"] = plantTypes;
    j["vegetationDensity"] = vegetationDensity;
    j["oreTypes"] = oreTypes;
    j["oreDensities"] = oreDensities;
    return j;
}

BiomeDefinition BiomeDefinition::FromJson(const nlohmann::json& j) {
    BiomeDefinition biome;
    biome.id = j.value("id", 0);
    biome.name = j.value("name", "");
    biome.description = j.value("description", "");

    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
        biome.color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
    }

    biome.minTemperature = j.value("minTemperature", -10.0f);
    biome.maxTemperature = j.value("maxTemperature", 30.0f);
    biome.minPrecipitation = j.value("minPrecipitation", 0.0f);
    biome.maxPrecipitation = j.value("maxPrecipitation", 2000.0f);
    biome.minElevation = j.value("minElevation", 0.0f);
    biome.maxElevation = j.value("maxElevation", 3000.0f);

    if (j.contains("treeTypes")) biome.treeTypes = j["treeTypes"].get<std::vector<std::string>>();
    if (j.contains("plantTypes")) biome.plantTypes = j["plantTypes"].get<std::vector<std::string>>();
    biome.vegetationDensity = j.value("vegetationDensity", 0.5f);
    if (j.contains("oreTypes")) biome.oreTypes = j["oreTypes"].get<std::vector<std::string>>();
    if (j.contains("oreDensities")) biome.oreDensities = j["oreDensities"].get<std::vector<float>>();

    return biome;
}

// =============================================================================
// ResourceDefinition Implementation
// =============================================================================

nlohmann::json ResourceDefinition::ToJson() const {
    nlohmann::json j;
    j["resourceType"] = resourceType;
    j["density"] = density;
    j["minHeight"] = minHeight;
    j["maxHeight"] = maxHeight;
    j["minSlope"] = minSlope;
    j["maxSlope"] = maxSlope;
    j["clusterSize"] = clusterSize;
    j["allowedBiomes"] = allowedBiomes;
    return j;
}

ResourceDefinition ResourceDefinition::FromJson(const nlohmann::json& j) {
    ResourceDefinition resource;
    resource.resourceType = j.value("resourceType", "");
    resource.density = j.value("density", 0.1f);
    resource.minHeight = j.value("minHeight", 0.0f);
    resource.maxHeight = j.value("maxHeight", 100.0f);
    resource.minSlope = j.value("minSlope", 0.0f);
    resource.maxSlope = j.value("maxSlope", 90.0f);
    resource.clusterSize = j.value("clusterSize", 5.0f);
    if (j.contains("allowedBiomes")) {
        resource.allowedBiomes = j["allowedBiomes"].get<std::vector<int>>();
    }
    return resource;
}

// =============================================================================
// StructureDefinition Implementation
// =============================================================================

nlohmann::json StructureDefinition::ToJson() const {
    nlohmann::json j;
    j["structureType"] = structureType;
    j["density"] = density;
    j["minDistance"] = minDistance;
    j["minSize"] = minSize;
    j["maxSize"] = maxSize;
    j["maxSlope"] = maxSlope;
    j["allowedBiomes"] = allowedBiomes;
    j["variants"] = variants;
    return j;
}

StructureDefinition StructureDefinition::FromJson(const nlohmann::json& j) {
    StructureDefinition structure;
    structure.structureType = j.value("structureType", "");
    structure.density = j.value("density", 0.01f);
    structure.minDistance = j.value("minDistance", 500.0f);
    structure.minSize = j.value("minSize", 10.0f);
    structure.maxSize = j.value("maxSize", 50.0f);
    structure.maxSlope = j.value("maxSlope", 15.0f);
    if (j.contains("allowedBiomes")) {
        structure.allowedBiomes = j["allowedBiomes"].get<std::vector<int>>();
    }
    if (j.contains("variants")) {
        structure.variants = j["variants"].get<std::vector<std::string>>();
    }
    return structure;
}

// =============================================================================
// ClimateConfig Implementation
// =============================================================================

nlohmann::json ClimateConfig::ToJson() const {
    nlohmann::json j;
    j["equatorTemperature"] = equatorTemperature;
    j["poleTemperature"] = poleTemperature;
    j["temperatureVariation"] = temperatureVariation;
    j["elevationTemperatureGradient"] = elevationTemperatureGradient;
    j["baseRainfall"] = baseRainfall;
    j["oceanRainfallBonus"] = oceanRainfallBonus;
    j["mountainRainShadow"] = mountainRainShadow;
    j["windPattern"] = windPattern;
    return j;
}

ClimateConfig ClimateConfig::FromJson(const nlohmann::json& j) {
    ClimateConfig climate;
    climate.equatorTemperature = j.value("equatorTemperature", 30.0f);
    climate.poleTemperature = j.value("poleTemperature", -20.0f);
    climate.temperatureVariation = j.value("temperatureVariation", 5.0f);
    climate.elevationTemperatureGradient = j.value("elevationTemperatureGradient", -6.5f);
    climate.baseRainfall = j.value("baseRainfall", 1000.0f);
    climate.oceanRainfallBonus = j.value("oceanRainfallBonus", 500.0f);
    climate.mountainRainShadow = j.value("mountainRainShadow", 0.5f);
    climate.windPattern = j.value("windPattern", "westerlies");
    return climate;
}

// =============================================================================
// WorldTemplate Implementation
// =============================================================================

std::shared_ptr<WorldTemplate> WorldTemplate::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return nullptr;
        }

        nlohmann::json j;
        file >> j;
        return LoadFromJson(j);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

std::shared_ptr<WorldTemplate> WorldTemplate::LoadFromJson(const nlohmann::json& j) {
    try {
        auto templ = std::make_shared<WorldTemplate>();

        templ->name = j.value("name", "Unnamed Template");
        templ->description = j.value("description", "");
        templ->version = j.value("version", "1.0.0");
        templ->seed = j.value("seed", 12345);

        if (j.contains("worldSize")) {
            const auto& ws = j["worldSize"];
            templ->worldSize = glm::ivec2(ws.value("width", 10000), ws.value("height", 10000));
        }

        templ->maxHeight = j.value("maxHeight", 255);

        if (j.contains("procGenGraph")) {
            templ->procGenGraphJson = j["procGenGraph"];
        }

        if (j.contains("biomes")) {
            for (const auto& biomeJson : j["biomes"]) {
                templ->biomes.push_back(BiomeDefinition::FromJson(biomeJson));
            }
        }

        if (j.contains("resources")) {
            const auto& resources = j["resources"];
            if (resources.contains("ores")) {
                for (const auto& oreJson : resources["ores"]) {
                    templ->ores.push_back(ResourceDefinition::FromJson(oreJson));
                }
            }
            if (resources.contains("vegetation")) {
                for (const auto& vegJson : resources["vegetation"]) {
                    templ->vegetation.push_back(ResourceDefinition::FromJson(vegJson));
                }
            }
            if (resources.contains("water")) {
                for (const auto& waterJson : resources["water"]) {
                    templ->water.push_back(ResourceDefinition::FromJson(waterJson));
                }
            }
        }

        if (j.contains("structures")) {
            const auto& structures = j["structures"];
            if (structures.contains("ruins")) {
                for (const auto& ruinJson : structures["ruins"]) {
                    templ->ruins.push_back(StructureDefinition::FromJson(ruinJson));
                }
            }
            if (structures.contains("ancients")) {
                for (const auto& ancientJson : structures["ancients"]) {
                    templ->ancients.push_back(StructureDefinition::FromJson(ancientJson));
                }
            }
            if (structures.contains("buildings")) {
                for (const auto& buildingJson : structures["buildings"]) {
                    templ->buildings.push_back(StructureDefinition::FromJson(buildingJson));
                }
            }
        }

        if (j.contains("climate")) {
            templ->climate = ClimateConfig::FromJson(j["climate"]);
        }

        templ->erosionStrength = j.value("erosionStrength", 0.3f);
        templ->erosionIterations = j.value("erosionIterations", 100);
        templ->terrainScale = j.value("terrainScale", 1.0f);
        templ->terrainAmplitude = j.value("terrainAmplitude", 100.0f);

        templ->author = j.value("author", "");
        templ->createdDate = j.value("createdDate", "");
        templ->modifiedDate = j.value("modifiedDate", "");
        if (j.contains("tags")) {
            templ->tags = j["tags"].get<std::vector<std::string>>();
        }

        return templ;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

bool WorldTemplate::SaveToFile(const std::string& filePath) const {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json j = SaveToJson();
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

nlohmann::json WorldTemplate::SaveToJson() const {
    nlohmann::json j;

    j["name"] = name;
    j["description"] = description;
    j["version"] = version;
    j["seed"] = seed;

    j["worldSize"] = {
        {"width", worldSize.x},
        {"height", worldSize.y}
    };
    j["maxHeight"] = maxHeight;

    if (!procGenGraphJson.is_null()) {
        j["procGenGraph"] = procGenGraphJson;
    }

    nlohmann::json biomesJson = nlohmann::json::array();
    for (const auto& biome : biomes) {
        biomesJson.push_back(biome.ToJson());
    }
    j["biomes"] = biomesJson;

    nlohmann::json resourcesJson;
    nlohmann::json oresJson = nlohmann::json::array();
    for (const auto& ore : ores) {
        oresJson.push_back(ore.ToJson());
    }
    resourcesJson["ores"] = oresJson;

    nlohmann::json vegJson = nlohmann::json::array();
    for (const auto& veg : vegetation) {
        vegJson.push_back(veg.ToJson());
    }
    resourcesJson["vegetation"] = vegJson;

    nlohmann::json waterJson = nlohmann::json::array();
    for (const auto& w : water) {
        waterJson.push_back(w.ToJson());
    }
    resourcesJson["water"] = waterJson;
    j["resources"] = resourcesJson;

    nlohmann::json structuresJson;
    nlohmann::json ruinsJson = nlohmann::json::array();
    for (const auto& ruin : ruins) {
        ruinsJson.push_back(ruin.ToJson());
    }
    structuresJson["ruins"] = ruinsJson;

    nlohmann::json ancientsJson = nlohmann::json::array();
    for (const auto& ancient : ancients) {
        ancientsJson.push_back(ancient.ToJson());
    }
    structuresJson["ancients"] = ancientsJson;

    nlohmann::json buildingsJson = nlohmann::json::array();
    for (const auto& building : buildings) {
        buildingsJson.push_back(building.ToJson());
    }
    structuresJson["buildings"] = buildingsJson;
    j["structures"] = structuresJson;

    j["climate"] = climate.ToJson();

    j["erosionStrength"] = erosionStrength;
    j["erosionIterations"] = erosionIterations;
    j["terrainScale"] = terrainScale;
    j["terrainAmplitude"] = terrainAmplitude;

    j["author"] = author;
    j["createdDate"] = createdDate;
    j["modifiedDate"] = modifiedDate;
    j["tags"] = tags;

    return j;
}

bool WorldTemplate::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    if (name.empty()) {
        errors.push_back("Template name is empty");
        valid = false;
    }

    if (worldSize.x <= 0 || worldSize.y <= 0) {
        errors.push_back("Invalid world size");
        valid = false;
    }

    if (biomes.empty()) {
        errors.push_back("No biomes defined");
        valid = false;
    }

    return valid;
}

std::shared_ptr<ProcGenGraph> WorldTemplate::CreateProcGenGraph() const {
    auto graph = std::make_shared<ProcGenGraph>();

    if (!procGenGraphJson.is_null()) {
        graph->LoadFromJson(procGenGraphJson);
    }

    ProcGenConfig config;
    config.seed = seed;
    config.worldScale = terrainScale;
    graph->SetConfig(config);

    return graph;
}

// =============================================================================
// TemplateLibrary Implementation
// =============================================================================

TemplateLibrary& TemplateLibrary::Instance() {
    static TemplateLibrary instance;
    return instance;
}

void TemplateLibrary::LoadTemplatesFromDirectory(const std::string& directory) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                auto templ = WorldTemplate::LoadFromFile(entry.path().string());
                if (templ) {
                    RegisterTemplate(entry.path().stem().string(), templ);
                }
            }
        }
    } catch (const std::exception& e) {
        // Handle error
    }
}

void TemplateLibrary::RegisterTemplate(const std::string& id, std::shared_ptr<WorldTemplate> templ) {
    m_templates[id] = templ;
}

std::shared_ptr<WorldTemplate> TemplateLibrary::GetTemplate(const std::string& id) const {
    auto it = m_templates.find(id);
    return (it != m_templates.end()) ? it->second : nullptr;
}

std::vector<std::string> TemplateLibrary::GetTemplateIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_templates.size());
    for (const auto& pair : m_templates) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<std::shared_ptr<WorldTemplate>> TemplateLibrary::GetTemplatesByTag(const std::string& tag) const {
    std::vector<std::shared_ptr<WorldTemplate>> result;
    for (const auto& pair : m_templates) {
        const auto& tags = pair.second->tags;
        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<WorldTemplate>> TemplateLibrary::SearchTemplates(const std::string& query) const {
    std::vector<std::shared_ptr<WorldTemplate>> result;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& pair : m_templates) {
        std::string lowerName = pair.second->name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName.find(lowerQuery) != std::string::npos ||
            pair.second->description.find(query) != std::string::npos) {
            result.push_back(pair.second);
        }
    }
    return result;
}

void TemplateLibrary::LoadBuiltInTemplates() {
    // Load templates from default locations
    LoadTemplatesFromDirectory("game/assets/procgen/templates/");
}

// =============================================================================
// TemplateBuilder Implementation
// =============================================================================

TemplateBuilder::TemplateBuilder(const std::string& name) {
    m_template = std::make_shared<WorldTemplate>();
    m_template->name = name;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    m_template->createdDate = std::ctime(&time);
}

TemplateBuilder& TemplateBuilder::WithDescription(const std::string& desc) {
    m_template->description = desc;
    return *this;
}

TemplateBuilder& TemplateBuilder::WithSeed(int seed) {
    m_template->seed = seed;
    return *this;
}

TemplateBuilder& TemplateBuilder::WithWorldSize(int width, int height) {
    m_template->worldSize = glm::ivec2(width, height);
    return *this;
}

TemplateBuilder& TemplateBuilder::WithBiome(const BiomeDefinition& biome) {
    m_template->biomes.push_back(biome);
    return *this;
}

TemplateBuilder& TemplateBuilder::WithResource(const ResourceDefinition& resource) {
    m_template->ores.push_back(resource);
    return *this;
}

TemplateBuilder& TemplateBuilder::WithStructure(const StructureDefinition& structure) {
    m_template->ruins.push_back(structure);
    return *this;
}

TemplateBuilder& TemplateBuilder::WithClimate(const ClimateConfig& climate) {
    m_template->climate = climate;
    return *this;
}

TemplateBuilder& TemplateBuilder::WithErosion(float strength, int iterations) {
    m_template->erosionStrength = strength;
    m_template->erosionIterations = iterations;
    return *this;
}

TemplateBuilder& TemplateBuilder::WithTag(const std::string& tag) {
    m_template->tags.push_back(tag);
    return *this;
}

std::shared_ptr<WorldTemplate> TemplateBuilder::Build() {
    auto result = m_template;
    m_template = std::make_shared<WorldTemplate>();
    return result;
}

} // namespace ProcGen
} // namespace Nova
