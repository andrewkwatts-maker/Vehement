#include "MapFile.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

namespace Vehement {

std::string MapVersion::ToString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

MapVersion MapVersion::FromString(const std::string& str) {
    MapVersion v;
    sscanf(str.c_str(), "%hhu.%hhu.%hu", &v.major, &v.minor, &v.patch);
    return v;
}

bool MapVersion::IsCompatible(const MapVersion& other) const {
    return major == other.major;  // Same major version = compatible
}

MapFile::MapFile() {
    m_metadata.version = {1, 0, 0};
    m_metadata.createdTime = static_cast<uint64_t>(std::time(nullptr));
    m_metadata.modifiedTime = m_metadata.createdTime;
    m_metadata.minPlayers = 2;
    m_metadata.maxPlayers = 8;
    m_metadata.size = glm::ivec2(256, 256);

    m_cameraBounds.min = glm::vec2(0.0f, 0.0f);
    m_cameraBounds.max = glm::vec2(256.0f, 256.0f);
    m_cameraBounds.minZoom = 0.5f;
    m_cameraBounds.maxZoom = 2.0f;
}

MapFile::~MapFile() = default;

bool MapFile::Save(const std::string& filepath) {
    if (filepath.ends_with(".json") || filepath.ends_with(".vmap.json")) {
        return SaveJson(filepath);
    }
    return SaveBinary(filepath);
}

bool MapFile::Load(const std::string& filepath) {
    if (filepath.ends_with(".json") || filepath.ends_with(".vmap.json")) {
        return LoadJson(filepath);
    }
    return LoadBinary(filepath);
}

bool MapFile::SaveBinary(const std::string& filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    WriteHeader(file);

    if (OnSaveProgress) OnSaveProgress(0.1f);

    WriteTerrain(file);

    if (OnSaveProgress) OnSaveProgress(0.4f);

    WriteObjects(file);

    if (OnSaveProgress) OnSaveProgress(0.7f);

    WriteRegions(file);

    if (OnSaveProgress) OnSaveProgress(1.0f);

    return true;
}

bool MapFile::LoadBinary(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    if (!ReadHeader(file)) {
        return false;
    }

    if (OnLoadProgress) OnLoadProgress(0.1f);

    if (!ReadTerrain(file)) {
        return false;
    }

    if (OnLoadProgress) OnLoadProgress(0.4f);

    if (!ReadObjects(file)) {
        return false;
    }

    if (OnLoadProgress) OnLoadProgress(0.7f);

    if (!ReadRegions(file)) {
        return false;
    }

    if (OnLoadProgress) OnLoadProgress(1.0f);

    return true;
}

bool MapFile::SaveJson(const std::string& filepath) {
    nlohmann::json j;

    // Metadata
    j["metadata"]["name"] = m_metadata.name;
    j["metadata"]["description"] = m_metadata.description;
    j["metadata"]["author"] = m_metadata.author;
    j["metadata"]["authorId"] = m_metadata.authorId;
    j["metadata"]["version"] = m_metadata.version.ToString();
    j["metadata"]["created"] = m_metadata.createdTime;
    j["metadata"]["modified"] = m_metadata.modifiedTime;
    j["metadata"]["thumbnail"] = m_metadata.thumbnailPath;
    j["metadata"]["tags"] = m_metadata.tags;
    j["metadata"]["minPlayers"] = m_metadata.minPlayers;
    j["metadata"]["maxPlayers"] = m_metadata.maxPlayers;
    j["metadata"]["suggestedPlayers"] = m_metadata.suggestedPlayers;
    j["metadata"]["tileset"] = m_metadata.tileset;
    j["metadata"]["size"] = {m_metadata.size.x, m_metadata.size.y};

    // Terrain (store as base64 for heightmap)
    j["terrain"]["waterLevel"] = m_terrain.waterLevel;
    // Note: Full terrain data would be stored as base64 encoded binary

    // Objects
    nlohmann::json objectsJson = nlohmann::json::array();
    for (const auto& obj : m_objects) {
        nlohmann::json objJson;
        objJson["id"] = obj.id;
        objJson["typeId"] = obj.typeId;
        objJson["category"] = obj.category;
        objJson["position"] = {obj.position.x, obj.position.y, obj.position.z};
        objJson["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
        objJson["scale"] = {obj.scale.x, obj.scale.y, obj.scale.z};
        objJson["playerId"] = obj.playerId;
        objJson["variation"] = obj.variation;
        objJson["properties"] = obj.properties;
        objectsJson.push_back(objJson);
    }
    j["objects"] = objectsJson;

    // Regions
    nlohmann::json regionsJson = nlohmann::json::array();
    for (const auto& region : m_regions) {
        nlohmann::json regionJson;
        regionJson["id"] = region.id;
        regionJson["name"] = region.name;
        regionJson["center"] = {region.center.x, region.center.y, region.center.z};
        regionJson["size"] = {region.size.x, region.size.y, region.size.z};
        regionJson["color"] = {region.color.r, region.color.g, region.color.b, region.color.a};
        regionJson["isRect"] = region.isRect;
        regionJson["radius"] = region.radius;
        regionsJson.push_back(regionJson);
    }
    j["regions"] = regionsJson;

    // Trigger zones
    nlohmann::json zonesJson = nlohmann::json::array();
    for (const auto& zone : m_triggerZones) {
        nlohmann::json zoneJson;
        zoneJson["id"] = zone.id;
        zoneJson["name"] = zone.name;
        zoneJson["position"] = {zone.position.x, zone.position.y, zone.position.z};
        zoneJson["radius"] = zone.radius;
        zoneJson["isCircle"] = zone.isCircle;
        zoneJson["rectSize"] = {zone.rectSize.x, zone.rectSize.y};
        zoneJson["linkedTriggers"] = zone.linkedTriggers;
        zonesJson.push_back(zoneJson);
    }
    j["triggerZones"] = zonesJson;

    // Start locations
    nlohmann::json startsJson = nlohmann::json::array();
    for (const auto& start : m_startLocations) {
        nlohmann::json startJson;
        startJson["playerId"] = start.playerId;
        startJson["position"] = {start.position.x, start.position.y, start.position.z};
        startJson["facing"] = start.facing;
        startJson["race"] = start.race;
        startJson["goldMine"] = start.goldMine;
        startsJson.push_back(startJson);
    }
    j["startLocations"] = startsJson;

    // Camera bounds
    j["camera"]["min"] = {m_cameraBounds.min.x, m_cameraBounds.min.y};
    j["camera"]["max"] = {m_cameraBounds.max.x, m_cameraBounds.max.y};
    j["camera"]["minZoom"] = m_cameraBounds.minZoom;
    j["camera"]["maxZoom"] = m_cameraBounds.maxZoom;

    // Trigger script
    j["triggerScript"] = m_triggerScript;

    // Write to file
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    return true;
}

bool MapFile::LoadJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Metadata
        if (j.contains("metadata")) {
            auto& meta = j["metadata"];
            m_metadata.name = meta.value("name", "");
            m_metadata.description = meta.value("description", "");
            m_metadata.author = meta.value("author", "");
            m_metadata.authorId = meta.value("authorId", "");
            m_metadata.version = MapVersion::FromString(meta.value("version", "1.0.0"));
            m_metadata.createdTime = meta.value("created", 0ULL);
            m_metadata.modifiedTime = meta.value("modified", 0ULL);
            m_metadata.thumbnailPath = meta.value("thumbnail", "");
            if (meta.contains("tags")) {
                m_metadata.tags = meta["tags"].get<std::vector<std::string>>();
            }
            m_metadata.minPlayers = meta.value("minPlayers", 2);
            m_metadata.maxPlayers = meta.value("maxPlayers", 8);
            m_metadata.suggestedPlayers = meta.value("suggestedPlayers", "");
            m_metadata.tileset = meta.value("tileset", "");
            if (meta.contains("size") && meta["size"].is_array()) {
                m_metadata.size = glm::ivec2(meta["size"][0], meta["size"][1]);
            }
        }

        // Terrain
        if (j.contains("terrain")) {
            m_terrain.waterLevel = j["terrain"].value("waterLevel", 0.0f);
        }

        // Objects
        m_objects.clear();
        if (j.contains("objects")) {
            for (const auto& objJson : j["objects"]) {
                PlacedObject obj;
                obj.id = objJson.value("id", GenerateObjectId());
                obj.typeId = objJson.value("typeId", "");
                obj.category = objJson.value("category", "");
                if (objJson.contains("position") && objJson["position"].is_array()) {
                    obj.position = glm::vec3(objJson["position"][0], objJson["position"][1], objJson["position"][2]);
                }
                if (objJson.contains("rotation") && objJson["rotation"].is_array()) {
                    obj.rotation = glm::vec3(objJson["rotation"][0], objJson["rotation"][1], objJson["rotation"][2]);
                }
                if (objJson.contains("scale") && objJson["scale"].is_array()) {
                    obj.scale = glm::vec3(objJson["scale"][0], objJson["scale"][1], objJson["scale"][2]);
                } else {
                    obj.scale = glm::vec3(1.0f);
                }
                obj.playerId = objJson.value("playerId", 0);
                obj.variation = objJson.value("variation", 0);
                if (objJson.contains("properties")) {
                    obj.properties = objJson["properties"].get<std::unordered_map<std::string, std::string>>();
                }
                m_objects.push_back(obj);
            }
        }

        // Regions
        m_regions.clear();
        if (j.contains("regions")) {
            for (const auto& regionJson : j["regions"]) {
                RegionData region;
                region.id = regionJson.value("id", GenerateRegionId());
                region.name = regionJson.value("name", "");
                if (regionJson.contains("center") && regionJson["center"].is_array()) {
                    region.center = glm::vec3(regionJson["center"][0], regionJson["center"][1], regionJson["center"][2]);
                }
                if (regionJson.contains("size") && regionJson["size"].is_array()) {
                    region.size = glm::vec3(regionJson["size"][0], regionJson["size"][1], regionJson["size"][2]);
                }
                if (regionJson.contains("color") && regionJson["color"].is_array()) {
                    region.color = glm::vec4(regionJson["color"][0], regionJson["color"][1], regionJson["color"][2], regionJson["color"][3]);
                }
                region.isRect = regionJson.value("isRect", true);
                region.radius = regionJson.value("radius", 10.0f);
                m_regions.push_back(region);
            }
        }

        // Trigger zones
        m_triggerZones.clear();
        if (j.contains("triggerZones")) {
            for (const auto& zoneJson : j["triggerZones"]) {
                TriggerZoneData zone;
                zone.id = zoneJson.value("id", 0U);
                zone.name = zoneJson.value("name", "");
                if (zoneJson.contains("position") && zoneJson["position"].is_array()) {
                    zone.position = glm::vec3(zoneJson["position"][0], zoneJson["position"][1], zoneJson["position"][2]);
                }
                zone.radius = zoneJson.value("radius", 10.0f);
                zone.isCircle = zoneJson.value("isCircle", true);
                if (zoneJson.contains("rectSize") && zoneJson["rectSize"].is_array()) {
                    zone.rectSize = glm::vec2(zoneJson["rectSize"][0], zoneJson["rectSize"][1]);
                }
                if (zoneJson.contains("linkedTriggers")) {
                    zone.linkedTriggers = zoneJson["linkedTriggers"].get<std::vector<uint32_t>>();
                }
                m_triggerZones.push_back(zone);
            }
        }

        // Start locations
        m_startLocations.clear();
        if (j.contains("startLocations")) {
            for (const auto& startJson : j["startLocations"]) {
                StartLocation start;
                start.playerId = startJson.value("playerId", 0);
                if (startJson.contains("position") && startJson["position"].is_array()) {
                    start.position = glm::vec3(startJson["position"][0], startJson["position"][1], startJson["position"][2]);
                }
                start.facing = startJson.value("facing", 0.0f);
                start.race = startJson.value("race", "");
                start.goldMine = startJson.value("goldMine", -1);
                m_startLocations.push_back(start);
            }
        }

        // Camera bounds
        if (j.contains("camera")) {
            if (j["camera"].contains("min") && j["camera"]["min"].is_array()) {
                m_cameraBounds.min = glm::vec2(j["camera"]["min"][0], j["camera"]["min"][1]);
            }
            if (j["camera"].contains("max") && j["camera"]["max"].is_array()) {
                m_cameraBounds.max = glm::vec2(j["camera"]["max"][0], j["camera"]["max"][1]);
            }
            m_cameraBounds.minZoom = j["camera"].value("minZoom", 0.5f);
            m_cameraBounds.maxZoom = j["camera"].value("maxZoom", 2.0f);
        }

        // Trigger script
        m_triggerScript = j.value("triggerScript", "");

        return true;
    } catch (...) {
        return false;
    }
}

void MapFile::AddObject(const PlacedObject& obj) {
    PlacedObject newObj = obj;
    if (newObj.id == 0) {
        newObj.id = GenerateObjectId();
    }
    m_objects.push_back(newObj);
}

void MapFile::RemoveObject(uint32_t id) {
    m_objects.erase(
        std::remove_if(m_objects.begin(), m_objects.end(),
            [id](const PlacedObject& obj) { return obj.id == id; }),
        m_objects.end());
}

PlacedObject* MapFile::GetObject(uint32_t id) {
    for (auto& obj : m_objects) {
        if (obj.id == id) {
            return &obj;
        }
    }
    return nullptr;
}

void MapFile::ClearObjects() {
    m_objects.clear();
}

void MapFile::AddRegion(const RegionData& region) {
    RegionData newRegion = region;
    if (newRegion.id == 0) {
        newRegion.id = GenerateRegionId();
    }
    m_regions.push_back(newRegion);
}

void MapFile::RemoveRegion(uint32_t id) {
    m_regions.erase(
        std::remove_if(m_regions.begin(), m_regions.end(),
            [id](const RegionData& r) { return r.id == id; }),
        m_regions.end());
}

RegionData* MapFile::GetRegion(uint32_t id) {
    for (auto& region : m_regions) {
        if (region.id == id) {
            return &region;
        }
    }
    return nullptr;
}

void MapFile::ClearRegions() {
    m_regions.clear();
}

void MapFile::AddTriggerZone(const TriggerZoneData& zone) {
    m_triggerZones.push_back(zone);
}

void MapFile::RemoveTriggerZone(uint32_t id) {
    m_triggerZones.erase(
        std::remove_if(m_triggerZones.begin(), m_triggerZones.end(),
            [id](const TriggerZoneData& z) { return z.id == id; }),
        m_triggerZones.end());
}

void MapFile::ClearTriggerZones() {
    m_triggerZones.clear();
}

void MapFile::SetStartLocations(const std::vector<StartLocation>& locations) {
    m_startLocations = locations;
}

void MapFile::AddStartLocation(const StartLocation& location) {
    m_startLocations.push_back(location);
}

bool MapFile::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    valid &= ValidateTerrain(errors);
    valid &= ValidateObjects(errors);
    valid &= ValidateStartLocations(errors);

    if (m_metadata.name.empty()) {
        errors.push_back("Map name is required");
        valid = false;
    }

    return valid;
}

bool MapFile::ValidateTerrain(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_metadata.size.x <= 0 || m_metadata.size.y <= 0) {
        errors.push_back("Invalid map size");
        valid = false;
    }

    return valid;
}

bool MapFile::ValidateObjects(std::vector<std::string>& errors) const {
    bool valid = true;

    for (const auto& obj : m_objects) {
        if (obj.typeId.empty()) {
            errors.push_back("Object " + std::to_string(obj.id) + " has no type");
            valid = false;
        }
    }

    return valid;
}

bool MapFile::ValidateStartLocations(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_startLocations.size() < static_cast<size_t>(m_metadata.minPlayers)) {
        errors.push_back("Not enough start locations for minimum players");
        valid = false;
    }

    return valid;
}

uint32_t MapFile::GenerateObjectId() {
    return m_nextObjectId++;
}

uint32_t MapFile::GenerateRegionId() {
    return m_nextRegionId++;
}

void MapFile::WriteHeader(std::ostream& stream) {
    stream.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(MAGIC_NUMBER));
    stream.write(reinterpret_cast<const char*>(&CURRENT_VERSION), sizeof(CURRENT_VERSION));
}

bool MapFile::ReadHeader(std::istream& stream) {
    uint32_t magic;
    stream.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != MAGIC_NUMBER) {
        return false;
    }

    uint32_t version;
    stream.read(reinterpret_cast<char*>(&version), sizeof(version));

    return true;
}

void MapFile::WriteTerrain(std::ostream& stream) {
    // Write terrain data
}

bool MapFile::ReadTerrain(std::istream& stream) {
    // Read terrain data
    return true;
}

void MapFile::WriteObjects(std::ostream& stream) {
    uint32_t count = static_cast<uint32_t>(m_objects.size());
    stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
    // Write each object
}

bool MapFile::ReadObjects(std::istream& stream) {
    uint32_t count;
    stream.read(reinterpret_cast<char*>(&count), sizeof(count));
    // Read each object
    return true;
}

void MapFile::WriteRegions(std::ostream& stream) {
    uint32_t count = static_cast<uint32_t>(m_regions.size());
    stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
    // Write each region
}

bool MapFile::ReadRegions(std::istream& stream) {
    uint32_t count;
    stream.read(reinterpret_cast<char*>(&count), sizeof(count));
    // Read each region
    return true;
}

// MapFileUtils

bool MapFileUtils::IsValidMapFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return magic == MapFile::MAGIC_NUMBER;
}

MapMetadata MapFileUtils::ReadMetadataOnly(const std::string& filepath) {
    MapFile mapFile;
    mapFile.Load(filepath);
    return mapFile.GetMetadata();
}

std::string MapFileUtils::GetMapExtension(bool binary) {
    return binary ? ".vmap" : ".vmap.json";
}

std::vector<std::string> MapFileUtils::GetSupportedExtensions() {
    return {".vmap", ".vmap.json", ".json"};
}

} // namespace Vehement
