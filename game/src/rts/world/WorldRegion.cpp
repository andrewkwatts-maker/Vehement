#include "WorldRegion.hpp"
#include "../../network/FirebaseManager.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <random>

namespace Vehement {

// ============================================================================
// GPSBounds Implementation
// ============================================================================

double GPSBounds::GetAreaKm2() const {
    double latDiff = northeast.latitude - southwest.latitude;
    double lonDiff = northeast.longitude - southwest.longitude;

    // Approximate conversion (varies by latitude)
    double avgLat = (northeast.latitude + southwest.latitude) / 2.0;
    double latKm = latDiff * 111.0;  // ~111 km per degree latitude
    double lonKm = lonDiff * 111.0 * std::cos(avgLat * 3.14159265358979 / 180.0);

    return latKm * lonKm;
}

bool GPSBounds::Intersects(const GPSBounds& other) const {
    return !(northeast.latitude < other.southwest.latitude ||
             southwest.latitude > other.northeast.latitude ||
             northeast.longitude < other.southwest.longitude ||
             southwest.longitude > other.northeast.longitude);
}

nlohmann::json GPSBounds::ToJson() const {
    return {
        {"southwest", {{"lat", southwest.latitude}, {"lon", southwest.longitude}}},
        {"northeast", {{"lat", northeast.latitude}, {"lon", northeast.longitude}}}
    };
}

GPSBounds GPSBounds::FromJson(const nlohmann::json& j) {
    GPSBounds bounds;
    if (j.contains("southwest")) {
        bounds.southwest.latitude = j["southwest"].value("lat", 0.0);
        bounds.southwest.longitude = j["southwest"].value("lon", 0.0);
    }
    if (j.contains("northeast")) {
        bounds.northeast.latitude = j["northeast"].value("lat", 0.0);
        bounds.northeast.longitude = j["northeast"].value("lon", 0.0);
    }
    return bounds;
}

// ============================================================================
// RegionBiome Helpers
// ============================================================================

const char* RegionBiomeToString(RegionBiome biome) {
    switch (biome) {
        case RegionBiome::Temperate: return "temperate";
        case RegionBiome::Desert: return "desert";
        case RegionBiome::Arctic: return "arctic";
        case RegionBiome::Tropical: return "tropical";
        case RegionBiome::Mountain: return "mountain";
        case RegionBiome::Ocean: return "ocean";
        case RegionBiome::Forest: return "forest";
        case RegionBiome::Swamp: return "swamp";
        case RegionBiome::Plains: return "plains";
        case RegionBiome::Volcanic: return "volcanic";
        case RegionBiome::Mystical: return "mystical";
        case RegionBiome::Corrupted: return "corrupted";
        case RegionBiome::Ancient: return "ancient";
        case RegionBiome::Dimensional: return "dimensional";
        default: return "unknown";
    }
}

RegionBiome RegionBiomeFromString(const std::string& str) {
    if (str == "temperate") return RegionBiome::Temperate;
    if (str == "desert") return RegionBiome::Desert;
    if (str == "arctic") return RegionBiome::Arctic;
    if (str == "tropical") return RegionBiome::Tropical;
    if (str == "mountain") return RegionBiome::Mountain;
    if (str == "ocean") return RegionBiome::Ocean;
    if (str == "forest") return RegionBiome::Forest;
    if (str == "swamp") return RegionBiome::Swamp;
    if (str == "plains") return RegionBiome::Plains;
    if (str == "volcanic") return RegionBiome::Volcanic;
    if (str == "mystical") return RegionBiome::Mystical;
    if (str == "corrupted") return RegionBiome::Corrupted;
    if (str == "ancient") return RegionBiome::Ancient;
    if (str == "dimensional") return RegionBiome::Dimensional;
    return RegionBiome::Unknown;
}

// ============================================================================
// ResourceNode Implementation
// ============================================================================

nlohmann::json ResourceNode::ToJson() const {
    return {
        {"id", id},
        {"resourceType", resourceType},
        {"location", {{"lat", location.latitude}, {"lon", location.longitude}}},
        {"baseYield", baseYield},
        {"currentYield", currentYield},
        {"regenerationRate", regenerationRate},
        {"maxYield", maxYield},
        {"depleted", depleted},
        {"lastHarvestTime", lastHarvestTime},
        {"controllingPlayerId", controllingPlayerId}
    };
}

ResourceNode ResourceNode::FromJson(const nlohmann::json& j) {
    ResourceNode node;
    node.id = j.value("id", "");
    node.resourceType = j.value("resourceType", "");
    if (j.contains("location")) {
        node.location.latitude = j["location"].value("lat", 0.0);
        node.location.longitude = j["location"].value("lon", 0.0);
    }
    node.baseYield = j.value("baseYield", 100.0f);
    node.currentYield = j.value("currentYield", 100.0f);
    node.regenerationRate = j.value("regenerationRate", 1.0f);
    node.maxYield = j.value("maxYield", 1000.0f);
    node.depleted = j.value("depleted", false);
    node.lastHarvestTime = j.value("lastHarvestTime", 0);
    node.controllingPlayerId = j.value("controllingPlayerId", "");
    return node;
}

// ============================================================================
// PortalConnection Implementation
// ============================================================================

nlohmann::json PortalConnection::ToJson() const {
    nlohmann::json resourceCostJson;
    for (const auto& [resource, amount] : resourceCost) {
        resourceCostJson[resource] = amount;
    }

    return {
        {"portalId", portalId},
        {"targetRegionId", targetRegionId},
        {"targetPortalId", targetPortalId},
        {"location", {{"lat", location.latitude}, {"lon", location.longitude}}},
        {"bidirectional", bidirectional},
        {"active", active},
        {"minLevel", minLevel},
        {"requiredQuests", requiredQuests},
        {"resourceCost", resourceCostJson},
        {"travelTimeSeconds", travelTimeSeconds}
    };
}

PortalConnection PortalConnection::FromJson(const nlohmann::json& j) {
    PortalConnection conn;
    conn.portalId = j.value("portalId", "");
    conn.targetRegionId = j.value("targetRegionId", "");
    conn.targetPortalId = j.value("targetPortalId", "");
    if (j.contains("location")) {
        conn.location.latitude = j["location"].value("lat", 0.0);
        conn.location.longitude = j["location"].value("lon", 0.0);
    }
    conn.bidirectional = j.value("bidirectional", true);
    conn.active = j.value("active", true);
    conn.minLevel = j.value("minLevel", 1);
    if (j.contains("requiredQuests") && j["requiredQuests"].is_array()) {
        for (const auto& quest : j["requiredQuests"]) {
            conn.requiredQuests.push_back(quest.get<std::string>());
        }
    }
    if (j.contains("resourceCost") && j["resourceCost"].is_object()) {
        for (auto& [key, val] : j["resourceCost"].items()) {
            conn.resourceCost[key] = val.get<int>();
        }
    }
    conn.travelTimeSeconds = j.value("travelTimeSeconds", 30.0f);
    return conn;
}

// ============================================================================
// RegionalQuest Implementation
// ============================================================================

nlohmann::json RegionalQuest::ToJson() const {
    nlohmann::json rewardsJson;
    for (const auto& [item, amount] : rewards) {
        rewardsJson[item] = amount;
    }

    return {
        {"questId", questId},
        {"name", name},
        {"description", description},
        {"minLevel", minLevel},
        {"maxLevel", maxLevel},
        {"repeatable", repeatable},
        {"cooldownHours", cooldownHours},
        {"prerequisites", prerequisites},
        {"rewards", rewardsJson}
    };
}

RegionalQuest RegionalQuest::FromJson(const nlohmann::json& j) {
    RegionalQuest quest;
    quest.questId = j.value("questId", "");
    quest.name = j.value("name", "");
    quest.description = j.value("description", "");
    quest.minLevel = j.value("minLevel", 1);
    quest.maxLevel = j.value("maxLevel", 100);
    quest.repeatable = j.value("repeatable", false);
    quest.cooldownHours = j.value("cooldownHours", 24);
    if (j.contains("prerequisites") && j["prerequisites"].is_array()) {
        for (const auto& pre : j["prerequisites"]) {
            quest.prerequisites.push_back(pre.get<std::string>());
        }
    }
    if (j.contains("rewards") && j["rewards"].is_object()) {
        for (auto& [key, val] : j["rewards"].items()) {
            quest.rewards[key] = val.get<int>();
        }
    }
    return quest;
}

// ============================================================================
// RegionWeather Implementation
// ============================================================================

nlohmann::json RegionWeather::ToJson() const {
    return {
        {"type", type},
        {"intensity", intensity},
        {"temperature", temperature},
        {"windSpeed", windSpeed},
        {"windDirection", windDirection},
        {"visibility", visibility},
        {"startTime", startTime},
        {"duration", duration}
    };
}

RegionWeather RegionWeather::FromJson(const nlohmann::json& j) {
    RegionWeather weather;
    weather.type = j.value("type", "clear");
    weather.intensity = j.value("intensity", 0.5f);
    weather.temperature = j.value("temperature", 20.0f);
    weather.windSpeed = j.value("windSpeed", 0.0f);
    weather.windDirection = j.value("windDirection", 0.0f);
    weather.visibility = j.value("visibility", 1.0f);
    weather.startTime = j.value("startTime", 0);
    weather.duration = j.value("duration", 3600);
    return weather;
}

// ============================================================================
// WorldRegion Implementation
// ============================================================================

bool WorldRegion::ContainsCoordinate(const Geo::GeoCoordinate& coord) const {
    // First check bounding box
    if (!bounds.Contains(coord)) {
        return false;
    }

    // If we have a detailed polygon, check that
    if (!polygonBoundary.empty()) {
        return Geo::PointInPolygon(coord, polygonBoundary);
    }

    return true;
}

double WorldRegion::GetDistanceFromCenter(const Geo::GeoCoordinate& coord) const {
    return centerPoint.DistanceTo(coord);
}

const PortalConnection* WorldRegion::FindNearestPortal(const Geo::GeoCoordinate& coord) const {
    if (portals.empty()) return nullptr;

    const PortalConnection* nearest = nullptr;
    double minDistance = std::numeric_limits<double>::max();

    for (const auto& portal : portals) {
        if (!portal.active) continue;

        double distance = coord.DistanceTo(portal.location);
        if (distance < minDistance) {
            minDistance = distance;
            nearest = &portal;
        }
    }

    return nearest;
}

bool WorldRegion::CanPlayerEnter(int playerLevel, const std::vector<std::string>& completedQuests) const {
    if (!accessible) return false;
    if (playerLevel < recommendedLevel - 5) return false;  // Allow some leeway

    // Check if any portal requirements are met
    for (const auto& portal : portals) {
        bool questsComplete = true;
        for (const auto& reqQuest : portal.requiredQuests) {
            if (std::find(completedQuests.begin(), completedQuests.end(), reqQuest) == completedQuests.end()) {
                questsComplete = false;
                break;
            }
        }
        if (questsComplete && playerLevel >= portal.minLevel) {
            return true;
        }
    }

    // If no portals or this is a starting region
    return isStartingRegion || portals.empty();
}

int WorldRegion::GetEffectiveDangerLevel() const {
    int effective = dangerLevel;

    // Weather affects danger
    if (currentWeather.type == "storm" || currentWeather.type == "sandstorm") {
        effective += 1;
    }

    // Event regions are more dangerous
    if (isEventRegion) {
        effective += 2;
    }

    // Contested regions
    if (isContested) {
        effective += 1;
    }

    return std::clamp(effective, 1, 10);
}

nlohmann::json WorldRegion::ToJson() const {
    nlohmann::json j = {
        {"id", id},
        {"name", name},
        {"description", description},
        {"continent", continent},
        {"country", country},
        {"bounds", bounds.ToJson()},
        {"centerPoint", {{"lat", centerPoint.latitude}, {"lon", centerPoint.longitude}}},
        {"biome", RegionBiomeToString(biome)},
        {"controllingFaction", controllingFaction},
        {"controllingPlayerId", controllingPlayerId},
        {"controlStrength", controlStrength},
        {"dangerLevel", dangerLevel},
        {"recommendedLevel", recommendedLevel},
        {"discovered", discovered},
        {"accessible", accessible},
        {"pvpEnabled", pvpEnabled},
        {"currentWeather", currentWeather.ToJson()},
        {"timeZoneOffset", timeZoneOffset},
        {"usesRealTime", usesRealTime},
        {"gameTimeMultiplier", gameTimeMultiplier},
        {"resourceMultiplier", resourceMultiplier},
        {"experienceMultiplier", experienceMultiplier},
        {"combatDifficultyMultiplier", combatDifficultyMultiplier},
        {"movementSpeedMultiplier", movementSpeedMultiplier},
        {"createdTimestamp", createdTimestamp},
        {"lastUpdated", lastUpdated},
        {"playerCount", playerCount},
        {"totalVisits", totalVisits},
        {"isStartingRegion", isStartingRegion},
        {"isSafeZone", isSafeZone},
        {"isContested", isContested},
        {"isEventRegion", isEventRegion},
        {"isHidden", isHidden}
    };

    // Arrays
    nlohmann::json portalsJson = nlohmann::json::array();
    for (const auto& portal : portals) {
        portalsJson.push_back(portal.ToJson());
    }
    j["portals"] = portalsJson;

    nlohmann::json resourcesJson = nlohmann::json::array();
    for (const auto& res : resources) {
        resourcesJson.push_back(res.ToJson());
    }
    j["resources"] = resourcesJson;

    nlohmann::json questsJson = nlohmann::json::array();
    for (const auto& quest : quests) {
        questsJson.push_back(quest.ToJson());
    }
    j["quests"] = questsJson;

    j["npcSpawnIds"] = npcSpawnIds;
    j["bossSpawnIds"] = bossSpawnIds;

    nlohmann::json polygonJson = nlohmann::json::array();
    for (const auto& coord : polygonBoundary) {
        polygonJson.push_back({{"lat", coord.latitude}, {"lon", coord.longitude}});
    }
    j["polygonBoundary"] = polygonJson;

    return j;
}

WorldRegion WorldRegion::FromJson(const nlohmann::json& j) {
    WorldRegion region;
    region.id = j.value("id", "");
    region.name = j.value("name", "");
    region.description = j.value("description", "");
    region.continent = j.value("continent", "");
    region.country = j.value("country", "");

    if (j.contains("bounds")) {
        region.bounds = GPSBounds::FromJson(j["bounds"]);
    }
    if (j.contains("centerPoint")) {
        region.centerPoint.latitude = j["centerPoint"].value("lat", 0.0);
        region.centerPoint.longitude = j["centerPoint"].value("lon", 0.0);
    } else {
        region.centerPoint = region.bounds.GetCenter();
    }

    region.biome = RegionBiomeFromString(j.value("biome", "unknown"));
    region.controllingFaction = j.value("controllingFaction", -1);
    region.controllingPlayerId = j.value("controllingPlayerId", "");
    region.controlStrength = j.value("controlStrength", 0.0f);
    region.dangerLevel = j.value("dangerLevel", 1);
    region.recommendedLevel = j.value("recommendedLevel", 1);
    region.discovered = j.value("discovered", false);
    region.accessible = j.value("accessible", true);
    region.pvpEnabled = j.value("pvpEnabled", true);

    if (j.contains("currentWeather")) {
        region.currentWeather = RegionWeather::FromJson(j["currentWeather"]);
    }

    region.timeZoneOffset = j.value("timeZoneOffset", 0.0f);
    region.usesRealTime = j.value("usesRealTime", true);
    region.gameTimeMultiplier = j.value("gameTimeMultiplier", 1.0f);
    region.resourceMultiplier = j.value("resourceMultiplier", 1.0f);
    region.experienceMultiplier = j.value("experienceMultiplier", 1.0f);
    region.combatDifficultyMultiplier = j.value("combatDifficultyMultiplier", 1.0f);
    region.movementSpeedMultiplier = j.value("movementSpeedMultiplier", 1.0f);
    region.createdTimestamp = j.value("createdTimestamp", 0);
    region.lastUpdated = j.value("lastUpdated", 0);
    region.playerCount = j.value("playerCount", 0);
    region.totalVisits = j.value("totalVisits", 0);
    region.isStartingRegion = j.value("isStartingRegion", false);
    region.isSafeZone = j.value("isSafeZone", false);
    region.isContested = j.value("isContested", false);
    region.isEventRegion = j.value("isEventRegion", false);
    region.isHidden = j.value("isHidden", false);

    if (j.contains("portals") && j["portals"].is_array()) {
        for (const auto& p : j["portals"]) {
            region.portals.push_back(PortalConnection::FromJson(p));
        }
    }

    if (j.contains("resources") && j["resources"].is_array()) {
        for (const auto& r : j["resources"]) {
            region.resources.push_back(ResourceNode::FromJson(r));
        }
    }

    if (j.contains("quests") && j["quests"].is_array()) {
        for (const auto& q : j["quests"]) {
            region.quests.push_back(RegionalQuest::FromJson(q));
        }
    }

    if (j.contains("npcSpawnIds") && j["npcSpawnIds"].is_array()) {
        for (const auto& id : j["npcSpawnIds"]) {
            region.npcSpawnIds.push_back(id.get<std::string>());
        }
    }

    if (j.contains("bossSpawnIds") && j["bossSpawnIds"].is_array()) {
        for (const auto& id : j["bossSpawnIds"]) {
            region.bossSpawnIds.push_back(id.get<std::string>());
        }
    }

    if (j.contains("polygonBoundary") && j["polygonBoundary"].is_array()) {
        for (const auto& coord : j["polygonBoundary"]) {
            Geo::GeoCoordinate c;
            c.latitude = coord.value("lat", 0.0);
            c.longitude = coord.value("lon", 0.0);
            region.polygonBoundary.push_back(c);
        }
    }

    return region;
}

// ============================================================================
// RegionDiscovery Implementation
// ============================================================================

nlohmann::json RegionDiscovery::ToJson() const {
    return {
        {"regionId", regionId},
        {"playerId", playerId},
        {"discoveredTimestamp", discoveredTimestamp},
        {"lastVisitTimestamp", lastVisitTimestamp},
        {"visitCount", visitCount},
        {"fullyExplored", fullyExplored},
        {"explorationPercent", explorationPercent},
        {"discoveredPortals", discoveredPortals},
        {"discoveredResources", discoveredResources},
        {"completedQuests", completedQuests}
    };
}

RegionDiscovery RegionDiscovery::FromJson(const nlohmann::json& j) {
    RegionDiscovery disc;
    disc.regionId = j.value("regionId", "");
    disc.playerId = j.value("playerId", "");
    disc.discoveredTimestamp = j.value("discoveredTimestamp", 0);
    disc.lastVisitTimestamp = j.value("lastVisitTimestamp", 0);
    disc.visitCount = j.value("visitCount", 0);
    disc.fullyExplored = j.value("fullyExplored", false);
    disc.explorationPercent = j.value("explorationPercent", 0.0f);

    if (j.contains("discoveredPortals") && j["discoveredPortals"].is_array()) {
        for (const auto& p : j["discoveredPortals"]) {
            disc.discoveredPortals.push_back(p.get<std::string>());
        }
    }
    if (j.contains("discoveredResources") && j["discoveredResources"].is_array()) {
        for (const auto& r : j["discoveredResources"]) {
            disc.discoveredResources.push_back(r.get<std::string>());
        }
    }
    if (j.contains("completedQuests") && j["completedQuests"].is_array()) {
        for (const auto& q : j["completedQuests"]) {
            disc.completedQuests.push_back(q.get<std::string>());
        }
    }

    return disc;
}

// ============================================================================
// RegionManager Implementation
// ============================================================================

RegionManager& RegionManager::Instance() {
    static RegionManager instance;
    return instance;
}

bool RegionManager::Initialize(const RegionConfig& config) {
    if (m_initialized) return true;

    m_config = config;
    m_initialized = true;

    return true;
}

void RegionManager::Shutdown() {
    StopListening();

    std::lock_guard<std::mutex> lock(m_regionsMutex);
    m_regions.clear();
    m_initialized = false;
}

void RegionManager::Update(float deltaTime) {
    if (!m_initialized) return;

    m_weatherUpdateTimer += deltaTime;
    m_resourceUpdateTimer += deltaTime;
    m_controlUpdateTimer += deltaTime;

    if (m_weatherUpdateTimer >= WEATHER_UPDATE_INTERVAL) {
        UpdateWeather(m_weatherUpdateTimer);
        m_weatherUpdateTimer = 0.0f;
    }

    if (m_resourceUpdateTimer >= RESOURCE_UPDATE_INTERVAL) {
        UpdateResourceRegeneration(m_resourceUpdateTimer);
        m_resourceUpdateTimer = 0.0f;
    }

    if (m_controlUpdateTimer >= CONTROL_UPDATE_INTERVAL) {
        UpdateControlDecay(m_controlUpdateTimer);
        m_controlUpdateTimer = 0.0f;
    }
}

const WorldRegion* RegionManager::GetRegion(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    auto it = m_regions.find(regionId);
    if (it != m_regions.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<const WorldRegion*> RegionManager::GetAllRegions() const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    std::vector<const WorldRegion*> result;
    result.reserve(m_regions.size());
    for (const auto& [id, region] : m_regions) {
        result.push_back(&region);
    }
    return result;
}

std::vector<const WorldRegion*> RegionManager::GetRegionsByContinent(const std::string& continent) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    std::vector<const WorldRegion*> result;
    for (const auto& [id, region] : m_regions) {
        if (region.continent == continent) {
            result.push_back(&region);
        }
    }
    return result;
}

std::vector<const WorldRegion*> RegionManager::GetRegionsByBiome(RegionBiome biome) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    std::vector<const WorldRegion*> result;
    for (const auto& [id, region] : m_regions) {
        if (region.biome == biome) {
            result.push_back(&region);
        }
    }
    return result;
}

const WorldRegion* RegionManager::FindRegionAtCoordinate(const Geo::GeoCoordinate& coord) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    for (const auto& [id, region] : m_regions) {
        if (region.ContainsCoordinate(coord)) {
            return &region;
        }
    }
    return nullptr;
}

const WorldRegion* RegionManager::FindNearestRegion(const Geo::GeoCoordinate& coord) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    const WorldRegion* nearest = nullptr;
    double minDistance = std::numeric_limits<double>::max();

    for (const auto& [id, region] : m_regions) {
        double distance = region.GetDistanceFromCenter(coord);
        if (distance < minDistance) {
            minDistance = distance;
            nearest = &region;
        }
    }

    return nearest;
}

std::vector<const WorldRegion*> RegionManager::FindRegionsInRadius(
    const Geo::GeoCoordinate& center, double radiusKm) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    std::vector<const WorldRegion*> result;
    double radiusMeters = radiusKm * 1000.0;

    for (const auto& [id, region] : m_regions) {
        double distance = region.GetDistanceFromCenter(center);
        if (distance <= radiusMeters) {
            result.push_back(&region);
        }
    }

    return result;
}

std::vector<const WorldRegion*> RegionManager::GetDiscoveredRegions(const std::string& playerId) const {
    std::lock_guard<std::mutex> discLock(m_discoveriesMutex);
    std::lock_guard<std::mutex> regLock(m_regionsMutex);

    std::vector<const WorldRegion*> result;
    auto playerIt = m_discoveries.find(playerId);
    if (playerIt == m_discoveries.end()) return result;

    for (const auto& [regionId, discovery] : playerIt->second) {
        auto regIt = m_regions.find(regionId);
        if (regIt != m_regions.end()) {
            result.push_back(&regIt->second);
        }
    }

    return result;
}

std::vector<const WorldRegion*> RegionManager::GetAccessibleRegions(
    const std::string& playerId, int playerLevel) const {
    auto discovered = GetDiscoveredRegions(playerId);
    std::vector<const WorldRegion*> result;

    for (const auto* region : discovered) {
        if (region->accessible && playerLevel >= region->recommendedLevel - 5) {
            result.push_back(region);
        }
    }

    return result;
}

bool RegionManager::RegisterRegion(const WorldRegion& region) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    if (m_regions.find(region.id) != m_regions.end()) {
        return false;  // Already exists
    }
    m_regions[region.id] = region;
    return true;
}

bool RegionManager::UpdateRegion(const WorldRegion& region) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    auto it = m_regions.find(region.id);
    if (it == m_regions.end()) {
        return false;
    }
    it->second = region;

    // Notify callbacks
    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& callback : m_regionCallbacks) {
        callback(region);
    }

    return true;
}

bool RegionManager::RemoveRegion(const std::string& regionId) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    return m_regions.erase(regionId) > 0;
}

bool RegionManager::LoadRegionsFromConfig(const std::string& configPath) {
    namespace fs = std::filesystem;

    if (!fs::exists(configPath)) {
        return false;
    }

    for (const auto& entry : fs::recursive_directory_iterator(configPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            std::ifstream file(entry.path());
            if (file.is_open()) {
                try {
                    nlohmann::json j;
                    file >> j;
                    WorldRegion region = WorldRegion::FromJson(j);
                    RegisterRegion(region);
                } catch (const std::exception& e) {
                    // Log error but continue
                }
            }
        }
    }

    return true;
}

void RegionManager::SetRegionControl(const std::string& regionId, int factionId,
                                     const std::string& playerId, float strength) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    auto it = m_regions.find(regionId);
    if (it != m_regions.end()) {
        it->second.controllingFaction = factionId;
        it->second.controllingPlayerId = playerId;
        it->second.controlStrength = strength;
        it->second.lastUpdated = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
}

void RegionManager::SetRegionWeather(const std::string& regionId, const RegionWeather& weather) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    auto it = m_regions.find(regionId);
    if (it != m_regions.end()) {
        it->second.currentWeather = weather;

        // Notify weather callbacks
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& callback : m_weatherCallbacks) {
            callback(regionId, weather);
        }
    }
}

void RegionManager::DiscoverRegion(const std::string& regionId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);

    auto& playerDiscoveries = m_discoveries[playerId];
    if (playerDiscoveries.find(regionId) != playerDiscoveries.end()) {
        return;  // Already discovered
    }

    RegionDiscovery discovery;
    discovery.regionId = regionId;
    discovery.playerId = playerId;
    discovery.discoveredTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    discovery.lastVisitTimestamp = discovery.discoveredTimestamp;
    discovery.visitCount = 1;

    playerDiscoveries[regionId] = discovery;

    // Notify callbacks
    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& callback : m_discoveryCallbacks) {
        callback(discovery);
    }
}

const RegionDiscovery* RegionManager::GetDiscovery(
    const std::string& regionId, const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);

    auto playerIt = m_discoveries.find(playerId);
    if (playerIt == m_discoveries.end()) return nullptr;

    auto discIt = playerIt->second.find(regionId);
    if (discIt == playerIt->second.end()) return nullptr;

    return &discIt->second;
}

void RegionManager::UpdateDiscovery(const RegionDiscovery& discovery) {
    std::lock_guard<std::mutex> lock(m_discoveriesMutex);
    m_discoveries[discovery.playerId][discovery.regionId] = discovery;
}

bool RegionManager::IsRegionDiscovered(const std::string& regionId, const std::string& playerId) const {
    return GetDiscovery(regionId, playerId) != nullptr;
}

bool RegionManager::HarvestResource(const std::string& regionId, const std::string& resourceId,
                                    const std::string& playerId, float amount) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);

    auto regIt = m_regions.find(regionId);
    if (regIt == m_regions.end()) return false;

    for (auto& res : regIt->second.resources) {
        if (res.id == resourceId) {
            if (res.depleted || res.currentYield < amount) {
                return false;
            }

            res.currentYield -= amount;
            res.lastHarvestTime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (res.currentYield <= 0) {
                res.depleted = true;
                res.currentYield = 0;
            }

            return true;
        }
    }

    return false;
}

std::vector<ResourceNode> RegionManager::GetAvailableResources(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_regionsMutex);

    auto it = m_regions.find(regionId);
    if (it == m_regions.end()) return {};

    std::vector<ResourceNode> result;
    for (const auto& res : it->second.resources) {
        if (!res.depleted && res.currentYield > 0) {
            result.push_back(res);
        }
    }

    return result;
}

void RegionManager::SyncToServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    std::lock_guard<std::mutex> lock(m_regionsMutex);
    for (const auto& [id, region] : m_regions) {
        firebase.SetValue("world/regions/" + id, region.ToJson());
    }
}

void RegionManager::LoadFromServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.GetValue("world/regions", [this](const nlohmann::json& data) {
        if (data.is_null() || !data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_regionsMutex);
        for (auto& [key, value] : data.items()) {
            m_regions[key] = WorldRegion::FromJson(value);
        }
    });
}

void RegionManager::ListenForChanges() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.ListenToPath("world/regions", [this](const nlohmann::json& data) {
        if (data.is_null() || !data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_regionsMutex);
        for (auto& [key, value] : data.items()) {
            m_regions[key] = WorldRegion::FromJson(value);
        }
    });
}

void RegionManager::StopListening() {
    auto& firebase = FirebaseManager::Instance();
    if (firebase.IsInitialized()) {
        firebase.StopListening("world/regions");
    }
}

void RegionManager::OnRegionChanged(RegionChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_regionCallbacks.push_back(std::move(callback));
}

void RegionManager::OnRegionDiscovered(DiscoveryCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_discoveryCallbacks.push_back(std::move(callback));
}

void RegionManager::OnWeatherChanged(WeatherCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_weatherCallbacks.push_back(std::move(callback));
}

void RegionManager::UpdateWeather(float deltaTime) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::lock_guard<std::mutex> lock(m_regionsMutex);
    for (auto& [id, region] : m_regions) {
        // Check if weather should change
        float hoursElapsed = deltaTime / 3600.0f;
        if (dist(gen) < m_config.weatherChangeProbability * hoursElapsed) {
            GenerateRandomWeather(region);
        }
    }
}

void RegionManager::UpdateResourceRegeneration(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    float hoursElapsed = deltaTime / 3600.0f;

    for (auto& [id, region] : m_regions) {
        for (auto& res : region.resources) {
            if (res.currentYield < res.maxYield) {
                res.currentYield = std::min(
                    res.maxYield,
                    res.currentYield + res.regenerationRate * hoursElapsed * region.resourceMultiplier
                );
                if (res.currentYield > 0) {
                    res.depleted = false;
                }
            }
        }
    }
}

void RegionManager::UpdateControlDecay(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_regionsMutex);
    float hoursElapsed = deltaTime / 3600.0f;

    for (auto& [id, region] : m_regions) {
        if (region.controlStrength > 0 && !region.isSafeZone) {
            region.controlStrength = std::max(
                0.0f,
                region.controlStrength - m_config.controlDecayPerHour * hoursElapsed
            );
            if (region.controlStrength <= 0) {
                region.controllingFaction = -1;
                region.controllingPlayerId.clear();
            }
        }
    }
}

void RegionManager::GenerateRandomWeather(WorldRegion& region) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<std::string> weatherTypes;

    // Weather types depend on biome
    switch (region.biome) {
        case RegionBiome::Desert:
            weatherTypes = {"clear", "clear", "sandstorm", "hot"};
            break;
        case RegionBiome::Arctic:
            weatherTypes = {"snow", "blizzard", "clear", "fog"};
            break;
        case RegionBiome::Tropical:
            weatherTypes = {"rain", "storm", "humid", "clear"};
            break;
        case RegionBiome::Forest:
            weatherTypes = {"clear", "rain", "fog", "mist"};
            break;
        case RegionBiome::Ocean:
            weatherTypes = {"clear", "rain", "storm", "fog"};
            break;
        case RegionBiome::Mountain:
            weatherTypes = {"clear", "snow", "wind", "fog"};
            break;
        case RegionBiome::Mystical:
            weatherTypes = {"ethereal", "aurora", "mist", "magical"};
            break;
        case RegionBiome::Corrupted:
            weatherTypes = {"dark", "ash", "toxic", "blood_rain"};
            break;
        default:
            weatherTypes = {"clear", "rain", "cloudy", "wind"};
    }

    std::uniform_int_distribution<size_t> weatherDist(0, weatherTypes.size() - 1);

    RegionWeather weather;
    weather.type = weatherTypes[weatherDist(gen)];
    weather.intensity = dist(gen);
    weather.windSpeed = dist(gen) * 30.0f;
    weather.windDirection = dist(gen) * 360.0f;
    weather.visibility = 1.0f - (weather.intensity * 0.5f);
    weather.startTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    weather.duration = static_cast<int64_t>(1800 + dist(gen) * 7200);  // 30 min to 2 hours

    region.currentWeather = weather;
}

} // namespace Vehement
