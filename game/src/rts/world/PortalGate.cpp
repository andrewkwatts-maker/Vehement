#include "PortalGate.hpp"
#include "../../network/FirebaseManager.hpp"
#include <fstream>
#include <algorithm>
#include <random>
#include <queue>
#include <chrono>

namespace Vehement {

// ============================================================================
// Status/Type Helpers
// ============================================================================

const char* PortalStatusToString(PortalStatus status) {
    switch (status) {
        case PortalStatus::Inactive: return "inactive";
        case PortalStatus::Activating: return "activating";
        case PortalStatus::Active: return "active";
        case PortalStatus::Cooldown: return "cooldown";
        case PortalStatus::Disabled: return "disabled";
        case PortalStatus::Locked: return "locked";
        case PortalStatus::Unstable: return "unstable";
        default: return "unknown";
    }
}

PortalStatus PortalStatusFromString(const std::string& str) {
    if (str == "inactive") return PortalStatus::Inactive;
    if (str == "activating") return PortalStatus::Activating;
    if (str == "active") return PortalStatus::Active;
    if (str == "cooldown") return PortalStatus::Cooldown;
    if (str == "disabled") return PortalStatus::Disabled;
    if (str == "locked") return PortalStatus::Locked;
    if (str == "unstable") return PortalStatus::Unstable;
    return PortalStatus::Inactive;
}

const char* PortalVisualTypeToString(PortalVisualType type) {
    switch (type) {
        case PortalVisualType::Standard: return "standard";
        case PortalVisualType::Fire: return "fire";
        case PortalVisualType::Ice: return "ice";
        case PortalVisualType::Shadow: return "shadow";
        case PortalVisualType::Nature: return "nature";
        case PortalVisualType::Tech: return "tech";
        case PortalVisualType::Celestial: return "celestial";
        case PortalVisualType::Infernal: return "infernal";
        case PortalVisualType::Ancient: return "ancient";
        case PortalVisualType::Dimensional: return "dimensional";
        default: return "standard";
    }
}

PortalVisualType PortalVisualTypeFromString(const std::string& str) {
    if (str == "fire") return PortalVisualType::Fire;
    if (str == "ice") return PortalVisualType::Ice;
    if (str == "shadow") return PortalVisualType::Shadow;
    if (str == "nature") return PortalVisualType::Nature;
    if (str == "tech") return PortalVisualType::Tech;
    if (str == "celestial") return PortalVisualType::Celestial;
    if (str == "infernal") return PortalVisualType::Infernal;
    if (str == "ancient") return PortalVisualType::Ancient;
    if (str == "dimensional") return PortalVisualType::Dimensional;
    return PortalVisualType::Standard;
}

// ============================================================================
// PortalRequirements Implementation
// ============================================================================

nlohmann::json PortalRequirements::ToJson() const {
    nlohmann::json resourceCostJson, maintenanceCostJson;
    for (const auto& [k, v] : resourceCost) resourceCostJson[k] = v;
    for (const auto& [k, v] : maintenanceCost) maintenanceCostJson[k] = v;

    return {
        {"minLevel", minLevel},
        {"maxLevel", maxLevel},
        {"requiredQuests", requiredQuests},
        {"requiredItems", requiredItems},
        {"resourceCost", resourceCostJson},
        {"maintenanceCost", maintenanceCostJson},
        {"requiredFactions", requiredFactions},
        {"bannedFactions", bannedFactions},
        {"requiresGroupLeader", requiresGroupLeader},
        {"minGroupSize", minGroupSize},
        {"maxGroupSize", maxGroupSize},
        {"cooldownSeconds", cooldownSeconds},
        {"availableAfterTimestamp", availableAfterTimestamp},
        {"availableUntilTimestamp", availableUntilTimestamp},
        {"requiredAchievements", requiredAchievements},
        {"requiredRegionControl", requiredRegionControl}
    };
}

PortalRequirements PortalRequirements::FromJson(const nlohmann::json& j) {
    PortalRequirements req;
    req.minLevel = j.value("minLevel", 1);
    req.maxLevel = j.value("maxLevel", 100);

    auto loadStringArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) {
                result.push_back(item.get<std::string>());
            }
        }
        return result;
    };

    req.requiredQuests = loadStringArray("requiredQuests");
    req.requiredItems = loadStringArray("requiredItems");
    req.requiredFactions = loadStringArray("requiredFactions");
    req.bannedFactions = loadStringArray("bannedFactions");
    req.requiredAchievements = loadStringArray("requiredAchievements");

    if (j.contains("resourceCost") && j["resourceCost"].is_object()) {
        for (auto& [k, v] : j["resourceCost"].items()) {
            req.resourceCost[k] = v.get<int>();
        }
    }
    if (j.contains("maintenanceCost") && j["maintenanceCost"].is_object()) {
        for (auto& [k, v] : j["maintenanceCost"].items()) {
            req.maintenanceCost[k] = v.get<int>();
        }
    }

    req.requiresGroupLeader = j.value("requiresGroupLeader", false);
    req.minGroupSize = j.value("minGroupSize", 1);
    req.maxGroupSize = j.value("maxGroupSize", 100);
    req.cooldownSeconds = j.value("cooldownSeconds", 0.0f);
    req.availableAfterTimestamp = j.value("availableAfterTimestamp", 0);
    req.availableUntilTimestamp = j.value("availableUntilTimestamp", 0);
    req.requiredRegionControl = j.value("requiredRegionControl", "");

    return req;
}

// ============================================================================
// PortalVisuals Implementation
// ============================================================================

nlohmann::json PortalVisuals::ToJson() const {
    return {
        {"type", PortalVisualTypeToString(type)},
        {"primaryColor", {primaryColor.r, primaryColor.g, primaryColor.b}},
        {"secondaryColor", {secondaryColor.r, secondaryColor.g, secondaryColor.b}},
        {"scale", scale},
        {"rotationSpeed", rotationSpeed},
        {"pulseFrequency", pulseFrequency},
        {"particleDensity", particleDensity},
        {"customModel", customModel},
        {"customTexture", customTexture},
        {"idleAnimation", idleAnimation},
        {"activateAnimation", activateAnimation},
        {"deactivateAnimation", deactivateAnimation},
        {"travelAnimation", travelAnimation},
        {"ambientSound", ambientSound},
        {"activateSound", activateSound},
        {"travelSound", travelSound},
        {"arrivalSound", arrivalSound},
        {"soundRadius", soundRadius},
        {"emitsLight", emitsLight},
        {"lightRadius", lightRadius},
        {"lightColor", {lightColor.r, lightColor.g, lightColor.b}}
    };
}

PortalVisuals PortalVisuals::FromJson(const nlohmann::json& j) {
    PortalVisuals vis;
    vis.type = PortalVisualTypeFromString(j.value("type", "standard"));

    auto loadVec3 = [&j](const std::string& key, glm::vec3 def) {
        if (j.contains(key) && j[key].is_array() && j[key].size() >= 3) {
            return glm::vec3(j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>());
        }
        return def;
    };

    vis.primaryColor = loadVec3("primaryColor", glm::vec3(0.5f, 0.5f, 1.0f));
    vis.secondaryColor = loadVec3("secondaryColor", glm::vec3(0.3f, 0.3f, 0.8f));
    vis.lightColor = loadVec3("lightColor", glm::vec3(0.6f, 0.6f, 1.0f));

    vis.scale = j.value("scale", 1.0f);
    vis.rotationSpeed = j.value("rotationSpeed", 1.0f);
    vis.pulseFrequency = j.value("pulseFrequency", 1.0f);
    vis.particleDensity = j.value("particleDensity", 1.0f);
    vis.customModel = j.value("customModel", "");
    vis.customTexture = j.value("customTexture", "");
    vis.idleAnimation = j.value("idleAnimation", "");
    vis.activateAnimation = j.value("activateAnimation", "");
    vis.deactivateAnimation = j.value("deactivateAnimation", "");
    vis.travelAnimation = j.value("travelAnimation", "");
    vis.ambientSound = j.value("ambientSound", "");
    vis.activateSound = j.value("activateSound", "");
    vis.travelSound = j.value("travelSound", "");
    vis.arrivalSound = j.value("arrivalSound", "");
    vis.soundRadius = j.value("soundRadius", 50.0f);
    vis.emitsLight = j.value("emitsLight", true);
    vis.lightRadius = j.value("lightRadius", 20.0f);

    return vis;
}

// ============================================================================
// TravelConfig Implementation
// ============================================================================

nlohmann::json TravelConfig::ToJson() const {
    return {
        {"baseTravelTime", baseTravelTime},
        {"distanceTimeMultiplier", distanceTimeMultiplier},
        {"maxUnitsPerTrip", maxUnitsPerTrip},
        {"maxResourcesPerTrip", maxResourcesPerTrip},
        {"unitCapacityMultiplier", unitCapacityMultiplier},
        {"allowCombatUnits", allowCombatUnits},
        {"allowCivilianUnits", allowCivilianUnits},
        {"allowHeroes", allowHeroes},
        {"preserveFormation", preserveFormation},
        {"encounterChance", encounterChance},
        {"possibleEncounters", possibleEncounters},
        {"canBeInterrupted", canBeInterrupted},
        {"interruptionChance", interruptionChance}
    };
}

TravelConfig TravelConfig::FromJson(const nlohmann::json& j) {
    TravelConfig cfg;
    cfg.baseTravelTime = j.value("baseTravelTime", 10.0f);
    cfg.distanceTimeMultiplier = j.value("distanceTimeMultiplier", 0.001f);
    cfg.maxUnitsPerTrip = j.value("maxUnitsPerTrip", 50);
    cfg.maxResourcesPerTrip = j.value("maxResourcesPerTrip", 10000);
    cfg.unitCapacityMultiplier = j.value("unitCapacityMultiplier", 1.0f);
    cfg.allowCombatUnits = j.value("allowCombatUnits", true);
    cfg.allowCivilianUnits = j.value("allowCivilianUnits", true);
    cfg.allowHeroes = j.value("allowHeroes", true);
    cfg.preserveFormation = j.value("preserveFormation", true);
    cfg.encounterChance = j.value("encounterChance", 0.0f);
    cfg.canBeInterrupted = j.value("canBeInterrupted", false);
    cfg.interruptionChance = j.value("interruptionChance", 0.0f);

    if (j.contains("possibleEncounters") && j["possibleEncounters"].is_array()) {
        for (const auto& enc : j["possibleEncounters"]) {
            cfg.possibleEncounters.push_back(enc.get<std::string>());
        }
    }

    return cfg;
}

// ============================================================================
// PortalTraveler Implementation
// ============================================================================

nlohmann::json PortalTraveler::ToJson() const {
    nlohmann::json resourcesJson;
    for (const auto& [k, v] : resources) resourcesJson[k] = v;

    return {
        {"travelerId", travelerId},
        {"playerId", playerId},
        {"sourceRegionId", sourceRegionId},
        {"destinationRegionId", destinationRegionId},
        {"sourcePortalId", sourcePortalId},
        {"destinationPortalId", destinationPortalId},
        {"departureTime", departureTime},
        {"arrivalTime", arrivalTime},
        {"progress", progress},
        {"unitIds", unitIds},
        {"resources", resourcesJson},
        {"interrupted", interrupted},
        {"encounterId", encounterId}
    };
}

PortalTraveler PortalTraveler::FromJson(const nlohmann::json& j) {
    PortalTraveler t;
    t.travelerId = j.value("travelerId", "");
    t.playerId = j.value("playerId", "");
    t.sourceRegionId = j.value("sourceRegionId", "");
    t.destinationRegionId = j.value("destinationRegionId", "");
    t.sourcePortalId = j.value("sourcePortalId", "");
    t.destinationPortalId = j.value("destinationPortalId", "");
    t.departureTime = j.value("departureTime", 0);
    t.arrivalTime = j.value("arrivalTime", 0);
    t.progress = j.value("progress", 0.0f);
    t.interrupted = j.value("interrupted", false);
    t.encounterId = j.value("encounterId", "");

    if (j.contains("unitIds") && j["unitIds"].is_array()) {
        for (const auto& id : j["unitIds"]) {
            t.unitIds.push_back(id.get<std::string>());
        }
    }
    if (j.contains("resources") && j["resources"].is_object()) {
        for (auto& [k, v] : j["resources"].items()) {
            t.resources[k] = v.get<int>();
        }
    }

    return t;
}

// ============================================================================
// PortalGate Implementation
// ============================================================================

bool PortalGate::CanPlayerUse(int playerLevel,
                               const std::vector<std::string>& completedQuests,
                               const std::vector<std::string>& inventory,
                               const std::string& factionId) const {
    if (status != PortalStatus::Active && status != PortalStatus::Unstable) {
        return false;
    }

    // Level check
    if (playerLevel < requirements.minLevel || playerLevel > requirements.maxLevel) {
        return false;
    }

    // Quest check
    for (const auto& quest : requirements.requiredQuests) {
        if (std::find(completedQuests.begin(), completedQuests.end(), quest) == completedQuests.end()) {
            return false;
        }
    }

    // Item check
    for (const auto& item : requirements.requiredItems) {
        if (std::find(inventory.begin(), inventory.end(), item) == inventory.end()) {
            return false;
        }
    }

    // Faction check
    if (!requirements.requiredFactions.empty()) {
        bool found = std::find(requirements.requiredFactions.begin(),
                               requirements.requiredFactions.end(),
                               factionId) != requirements.requiredFactions.end();
        if (!found) return false;
    }

    // Banned faction check
    if (std::find(requirements.bannedFactions.begin(),
                  requirements.bannedFactions.end(),
                  factionId) != requirements.bannedFactions.end()) {
        return false;
    }

    // Time check
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (requirements.availableAfterTimestamp > 0 && now < requirements.availableAfterTimestamp) {
        return false;
    }
    if (requirements.availableUntilTimestamp > 0 && now > requirements.availableUntilTimestamp) {
        return false;
    }

    // Key check
    if (requiresKey && !keyItemId.empty()) {
        if (std::find(inventory.begin(), inventory.end(), keyItemId) == inventory.end()) {
            return false;
        }
    }

    return true;
}

float PortalGate::CalculateTravelTime(double distanceKm) const {
    float time = travelConfig.baseTravelTime;
    time += static_cast<float>(distanceKm * travelConfig.distanceTimeMultiplier * 1000.0);

    // Congestion penalty
    if (congestionLevel > 0.5f) {
        time *= 1.0f + (congestionLevel - 0.5f);
    }

    return time;
}

bool PortalGate::HasCapacity(int unitCount) const {
    return currentCapacity + unitCount <= maxCapacity;
}

float PortalGate::GetEffectiveCooldown() const {
    float cd = requirements.cooldownSeconds;
    if (congestionLevel > 0.5f) {
        cd *= 1.0f + congestionLevel;
    }
    return cd;
}

nlohmann::json PortalGate::ToJson() const {
    nlohmann::json inTransitJson = nlohmann::json::array();
    for (const auto& t : inTransit) {
        inTransitJson.push_back(t.ToJson());
    }

    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"regionId", regionId},
        {"gpsLocation", {{"lat", gpsLocation.latitude}, {"lon", gpsLocation.longitude}}},
        {"worldPosition", {worldPosition.x, worldPosition.y, worldPosition.z}},
        {"rotation", rotation},
        {"destinationRegionId", destinationRegionId},
        {"destinationPortalId", destinationPortalId},
        {"bidirectional", bidirectional},
        {"status", PortalStatusToString(status)},
        {"activationProgress", activationProgress},
        {"cooldownRemaining", cooldownRemaining},
        {"lastUsedTimestamp", lastUsedTimestamp},
        {"requirements", requirements.ToJson()},
        {"visuals", visuals.ToJson()},
        {"travelConfig", travelConfig.ToJson()},
        {"currentCapacity", currentCapacity},
        {"maxCapacity", maxCapacity},
        {"congestionLevel", congestionLevel},
        {"totalUses", totalUses},
        {"uniqueUsers", uniqueUsers},
        {"createdTimestamp", createdTimestamp},
        {"inTransit", inTransitJson},
        {"isOneWay", isOneWay},
        {"isHidden", isHidden},
        {"isTemporary", isTemporary},
        {"expirationTimestamp", expirationTimestamp},
        {"isBossPortal", isBossPortal},
        {"isEventPortal", isEventPortal},
        {"requiresKey", requiresKey},
        {"keyItemId", keyItemId}
    };
}

PortalGate PortalGate::FromJson(const nlohmann::json& j) {
    PortalGate gate;
    gate.id = j.value("id", "");
    gate.name = j.value("name", "");
    gate.description = j.value("description", "");
    gate.regionId = j.value("regionId", "");

    if (j.contains("gpsLocation")) {
        gate.gpsLocation.latitude = j["gpsLocation"].value("lat", 0.0);
        gate.gpsLocation.longitude = j["gpsLocation"].value("lon", 0.0);
    }

    if (j.contains("worldPosition") && j["worldPosition"].is_array() && j["worldPosition"].size() >= 3) {
        gate.worldPosition = glm::vec3(
            j["worldPosition"][0].get<float>(),
            j["worldPosition"][1].get<float>(),
            j["worldPosition"][2].get<float>()
        );
    }

    gate.rotation = j.value("rotation", 0.0f);
    gate.destinationRegionId = j.value("destinationRegionId", "");
    gate.destinationPortalId = j.value("destinationPortalId", "");
    gate.bidirectional = j.value("bidirectional", true);
    gate.status = PortalStatusFromString(j.value("status", "active"));
    gate.activationProgress = j.value("activationProgress", 0.0f);
    gate.cooldownRemaining = j.value("cooldownRemaining", 0.0f);
    gate.lastUsedTimestamp = j.value("lastUsedTimestamp", 0);

    if (j.contains("requirements")) {
        gate.requirements = PortalRequirements::FromJson(j["requirements"]);
    }
    if (j.contains("visuals")) {
        gate.visuals = PortalVisuals::FromJson(j["visuals"]);
    }
    if (j.contains("travelConfig")) {
        gate.travelConfig = TravelConfig::FromJson(j["travelConfig"]);
    }

    gate.currentCapacity = j.value("currentCapacity", 0);
    gate.maxCapacity = j.value("maxCapacity", 10);
    gate.congestionLevel = j.value("congestionLevel", 0.0f);
    gate.totalUses = j.value("totalUses", 0);
    gate.uniqueUsers = j.value("uniqueUsers", 0);
    gate.createdTimestamp = j.value("createdTimestamp", 0);

    if (j.contains("inTransit") && j["inTransit"].is_array()) {
        for (const auto& t : j["inTransit"]) {
            gate.inTransit.push_back(PortalTraveler::FromJson(t));
        }
    }

    gate.isOneWay = j.value("isOneWay", false);
    gate.isHidden = j.value("isHidden", false);
    gate.isTemporary = j.value("isTemporary", false);
    gate.expirationTimestamp = j.value("expirationTimestamp", 0);
    gate.isBossPortal = j.value("isBossPortal", false);
    gate.isEventPortal = j.value("isEventPortal", false);
    gate.requiresKey = j.value("requiresKey", false);
    gate.keyItemId = j.value("keyItemId", "");

    return gate;
}

// ============================================================================
// PortalNetworkEdge Implementation
// ============================================================================

nlohmann::json PortalNetworkEdge::ToJson() const {
    return {
        {"sourcePortalId", sourcePortalId},
        {"targetPortalId", targetPortalId},
        {"sourceRegionId", sourceRegionId},
        {"targetRegionId", targetRegionId},
        {"baseTravelTime", baseTravelTime},
        {"currentTravelTime", currentTravelTime},
        {"bidirectional", bidirectional},
        {"active", active},
        {"minLevel", minLevel}
    };
}

PortalNetworkEdge PortalNetworkEdge::FromJson(const nlohmann::json& j) {
    PortalNetworkEdge edge;
    edge.sourcePortalId = j.value("sourcePortalId", "");
    edge.targetPortalId = j.value("targetPortalId", "");
    edge.sourceRegionId = j.value("sourceRegionId", "");
    edge.targetRegionId = j.value("targetRegionId", "");
    edge.baseTravelTime = j.value("baseTravelTime", 0.0f);
    edge.currentTravelTime = j.value("currentTravelTime", 0.0f);
    edge.bidirectional = j.value("bidirectional", true);
    edge.active = j.value("active", true);
    edge.minLevel = j.value("minLevel", 1);
    return edge;
}

// ============================================================================
// PortalManager Implementation
// ============================================================================

PortalManager& PortalManager::Instance() {
    static PortalManager instance;
    return instance;
}

bool PortalManager::Initialize(const PortalConfig& config) {
    if (m_initialized) return true;

    m_config = config;
    m_initialized = true;
    m_networkDirty = true;

    return true;
}

void PortalManager::Shutdown() {
    StopListening();

    std::lock_guard<std::mutex> lock(m_portalsMutex);
    m_portals.clear();
    m_travelers.clear();
    m_networkEdges.clear();
    m_initialized = false;
}

void PortalManager::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateTravelers(deltaTime);
    UpdateCooldowns(deltaTime);
    UpdateActivations(deltaTime);

    if (m_networkDirty) {
        RebuildNetworkGraph();
        m_networkDirty = false;
    }
}

const PortalGate* PortalManager::GetPortal(const std::string& portalId) const {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    auto it = m_portals.find(portalId);
    return it != m_portals.end() ? &it->second : nullptr;
}

std::vector<const PortalGate*> PortalManager::GetAllPortals() const {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    std::vector<const PortalGate*> result;
    result.reserve(m_portals.size());
    for (const auto& [id, portal] : m_portals) {
        result.push_back(&portal);
    }
    return result;
}

std::vector<const PortalGate*> PortalManager::GetPortalsInRegion(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    std::vector<const PortalGate*> result;
    for (const auto& [id, portal] : m_portals) {
        if (portal.regionId == regionId) {
            result.push_back(&portal);
        }
    }
    return result;
}

std::vector<const PortalGate*> PortalManager::GetAccessiblePortals(
    const std::string& playerId, int playerLevel,
    const std::vector<std::string>& completedQuests) const {

    std::lock_guard<std::mutex> lock(m_portalsMutex);
    std::vector<const PortalGate*> result;

    for (const auto& [id, portal] : m_portals) {
        if (portal.status == PortalStatus::Active &&
            !portal.isHidden &&
            portal.CanPlayerUse(playerLevel, completedQuests, {}, "")) {
            result.push_back(&portal);
        }
    }

    return result;
}

const PortalGate* PortalManager::FindNearestPortal(const Geo::GeoCoordinate& coord) const {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    const PortalGate* nearest = nullptr;
    double minDist = std::numeric_limits<double>::max();

    for (const auto& [id, portal] : m_portals) {
        if (portal.status != PortalStatus::Active) continue;

        double dist = coord.DistanceTo(portal.gpsLocation);
        if (dist < minDist) {
            minDist = dist;
            nearest = &portal;
        }
    }

    return nearest;
}

std::vector<const PortalGate*> PortalManager::FindPortalsToRegion(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    std::vector<const PortalGate*> result;

    for (const auto& [id, portal] : m_portals) {
        if (portal.destinationRegionId == regionId && portal.status == PortalStatus::Active) {
            result.push_back(&portal);
        }
    }

    return result;
}

bool PortalManager::RegisterPortal(const PortalGate& portal) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    if (m_portals.find(portal.id) != m_portals.end()) {
        return false;
    }
    m_portals[portal.id] = portal;
    m_networkDirty = true;
    return true;
}

bool PortalManager::UpdatePortal(const PortalGate& portal) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    auto it = m_portals.find(portal.id);
    if (it == m_portals.end()) return false;

    it->second = portal;
    m_networkDirty = true;

    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& cb : m_portalCallbacks) {
        cb(portal);
    }

    return true;
}

bool PortalManager::RemovePortal(const std::string& portalId) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    bool removed = m_portals.erase(portalId) > 0;
    if (removed) m_networkDirty = true;
    return removed;
}

bool PortalManager::LoadPortalsFromConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("portals") && j["portals"].is_array()) {
            for (const auto& p : j["portals"]) {
                PortalGate gate = PortalGate::FromJson(p);
                RegisterPortal(gate);
            }
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void PortalManager::SetPortalStatus(const std::string& portalId, PortalStatus status) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    auto it = m_portals.find(portalId);
    if (it != m_portals.end()) {
        it->second.status = status;
        m_networkDirty = true;
    }
}

bool PortalManager::ActivatePortal(const std::string& portalId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    auto it = m_portals.find(portalId);
    if (it == m_portals.end()) return false;

    if (it->second.status == PortalStatus::Inactive ||
        it->second.status == PortalStatus::Locked) {
        it->second.status = PortalStatus::Activating;
        it->second.activationProgress = 0.0f;
        return true;
    }

    return false;
}

void PortalManager::DeactivatePortal(const std::string& portalId) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);
    auto it = m_portals.find(portalId);
    if (it != m_portals.end()) {
        it->second.status = PortalStatus::Inactive;
        it->second.activationProgress = 0.0f;
        m_networkDirty = true;
    }
}

std::string PortalManager::StartTravel(const std::string& portalId,
                                        const std::string& playerId,
                                        const std::vector<std::string>& unitIds,
                                        const std::unordered_map<std::string, int>& resources) {
    std::lock_guard<std::mutex> portalLock(m_portalsMutex);
    auto it = m_portals.find(portalId);
    if (it == m_portals.end()) return "";

    PortalGate& portal = it->second;
    if (portal.status != PortalStatus::Active) return "";
    if (!portal.HasCapacity(static_cast<int>(unitIds.size()))) return "";

    // Create traveler
    PortalTraveler traveler;
    traveler.travelerId = GenerateTravelId();
    traveler.playerId = playerId;
    traveler.sourceRegionId = portal.regionId;
    traveler.destinationRegionId = portal.destinationRegionId;
    traveler.sourcePortalId = portalId;
    traveler.destinationPortalId = portal.destinationPortalId;
    traveler.departureTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    traveler.arrivalTime = traveler.departureTime +
        static_cast<int64_t>(portal.travelConfig.baseTravelTime);
    traveler.unitIds = unitIds;
    traveler.resources = resources;

    // Update portal state
    portal.currentCapacity += static_cast<int>(unitIds.size());
    portal.totalUses++;
    portal.lastUsedTimestamp = traveler.departureTime;
    portal.congestionLevel = static_cast<float>(portal.currentCapacity) / portal.maxCapacity;

    // Add to travelers
    {
        std::lock_guard<std::mutex> travelerLock(m_travelersMutex);
        m_travelers[traveler.travelerId] = traveler;
    }

    // Notify callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& cb : m_travelStartCallbacks) {
            cb(traveler);
        }
    }

    return traveler.travelerId;
}

bool PortalManager::CancelTravel(const std::string& travelId) {
    std::lock_guard<std::mutex> lock(m_travelersMutex);
    auto it = m_travelers.find(travelId);
    if (it == m_travelers.end()) return false;

    // Return capacity to portal
    {
        std::lock_guard<std::mutex> portalLock(m_portalsMutex);
        auto portalIt = m_portals.find(it->second.sourcePortalId);
        if (portalIt != m_portals.end()) {
            portalIt->second.currentCapacity -= static_cast<int>(it->second.unitIds.size());
            portalIt->second.congestionLevel = static_cast<float>(portalIt->second.currentCapacity) /
                                               portalIt->second.maxCapacity;
        }
    }

    m_travelers.erase(it);
    return true;
}

const PortalTraveler* PortalManager::GetTravelStatus(const std::string& travelId) const {
    std::lock_guard<std::mutex> lock(m_travelersMutex);
    auto it = m_travelers.find(travelId);
    return it != m_travelers.end() ? &it->second : nullptr;
}

std::vector<PortalTraveler> PortalManager::GetPlayerTravelers(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_travelersMutex);
    std::vector<PortalTraveler> result;
    for (const auto& [id, traveler] : m_travelers) {
        if (traveler.playerId == playerId) {
            result.push_back(traveler);
        }
    }
    return result;
}

TravelPath PortalManager::FindPath(const std::string& sourceRegionId,
                                    const std::string& destRegionId,
                                    int playerLevel,
                                    const std::vector<std::string>& completedQuests) const {
    TravelPath path;
    if (sourceRegionId == destRegionId) {
        path.valid = true;
        path.regionIds.push_back(sourceRegionId);
        return path;
    }

    std::lock_guard<std::mutex> lock(m_networkMutex);

    // Dijkstra's algorithm
    std::unordered_map<std::string, float> distances;
    std::unordered_map<std::string, std::string> previous;
    std::unordered_map<std::string, std::string> previousPortal;
    std::priority_queue<std::pair<float, std::string>,
                        std::vector<std::pair<float, std::string>>,
                        std::greater<>> pq;

    distances[sourceRegionId] = 0;
    pq.push({0, sourceRegionId});

    while (!pq.empty()) {
        auto [dist, current] = pq.top();
        pq.pop();

        if (current == destRegionId) break;
        if (dist > distances[current]) continue;

        auto edgeIt = m_regionToEdges.find(current);
        if (edgeIt == m_regionToEdges.end()) continue;

        for (size_t edgeIdx : edgeIt->second) {
            const auto& edge = m_networkEdges[edgeIdx];
            if (!edge.active) continue;
            if (edge.minLevel > playerLevel) continue;

            std::string neighbor = (edge.sourceRegionId == current) ?
                                   edge.targetRegionId : edge.sourceRegionId;

            float newDist = dist + edge.currentTravelTime;
            if (distances.find(neighbor) == distances.end() || newDist < distances[neighbor]) {
                distances[neighbor] = newDist;
                previous[neighbor] = current;
                previousPortal[neighbor] = (edge.sourceRegionId == current) ?
                                           edge.sourcePortalId : edge.targetPortalId;
                pq.push({newDist, neighbor});
            }
        }
    }

    if (distances.find(destRegionId) == distances.end()) {
        path.valid = false;
        path.invalidReason = "No path found";
        return path;
    }

    // Reconstruct path
    std::string current = destRegionId;
    while (current != sourceRegionId) {
        path.regionIds.insert(path.regionIds.begin(), current);
        if (previousPortal.find(current) != previousPortal.end()) {
            path.portalIds.insert(path.portalIds.begin(), previousPortal[current]);
        }
        current = previous[current];
    }
    path.regionIds.insert(path.regionIds.begin(), sourceRegionId);

    path.totalTravelTime = distances[destRegionId];
    path.valid = true;

    return path;
}

std::vector<TravelPath> PortalManager::FindAllPaths(const std::string& sourceRegionId,
                                                     const std::string& destRegionId,
                                                     int maxPaths) const {
    std::vector<TravelPath> paths;
    // Simple BFS to find multiple paths
    // For production, implement Yen's k-shortest paths algorithm

    auto mainPath = FindPath(sourceRegionId, destRegionId, 100, {});
    if (mainPath.valid) {
        paths.push_back(mainPath);
    }

    return paths;
}

bool PortalManager::AreRegionsConnected(const std::string& regionA, const std::string& regionB) const {
    auto path = FindPath(regionA, regionB, 100, {});
    return path.valid;
}

std::vector<PortalNetworkEdge> PortalManager::GetNetworkEdges() const {
    std::lock_guard<std::mutex> lock(m_networkMutex);
    return m_networkEdges;
}

void PortalManager::SyncToServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    std::lock_guard<std::mutex> lock(m_portalsMutex);
    for (const auto& [id, portal] : m_portals) {
        firebase.SetValue("world/portals/" + id, portal.ToJson());
    }
}

void PortalManager::LoadFromServer() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.GetValue("world/portals", [this](const nlohmann::json& data) {
        if (!data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_portalsMutex);
        for (auto& [key, val] : data.items()) {
            m_portals[key] = PortalGate::FromJson(val);
        }
        m_networkDirty = true;
    });
}

void PortalManager::ListenForChanges() {
    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    firebase.ListenToPath("world/portals", [this](const nlohmann::json& data) {
        if (!data.is_object()) return;

        std::lock_guard<std::mutex> lock(m_portalsMutex);
        for (auto& [key, val] : data.items()) {
            m_portals[key] = PortalGate::FromJson(val);
        }
        m_networkDirty = true;
    });
}

void PortalManager::StopListening() {
    auto& firebase = FirebaseManager::Instance();
    if (firebase.IsInitialized()) {
        firebase.StopListening("world/portals");
    }
}

void PortalManager::OnPortalChanged(PortalChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_portalCallbacks.push_back(std::move(callback));
}

void PortalManager::OnTravelStarted(TravelStartCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_travelStartCallbacks.push_back(std::move(callback));
}

void PortalManager::OnTravelCompleted(TravelCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_travelCompleteCallbacks.push_back(std::move(callback));
}

void PortalManager::OnEncounter(EncounterCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_encounterCallbacks.push_back(std::move(callback));
}

void PortalManager::UpdateTravelers(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_travelersMutex);

    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::string> completedIds;

    for (auto& [id, traveler] : m_travelers) {
        if (traveler.interrupted) continue;

        float totalTime = static_cast<float>(traveler.arrivalTime - traveler.departureTime);
        float elapsed = static_cast<float>(now - traveler.departureTime);
        traveler.progress = std::clamp(elapsed / totalTime, 0.0f, 1.0f);

        if (now >= traveler.arrivalTime) {
            completedIds.push_back(id);
        }
    }

    for (const auto& id : completedIds) {
        ProcessTravelCompletion(m_travelers[id]);
    }
}

void PortalManager::UpdateCooldowns(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);

    for (auto& [id, portal] : m_portals) {
        if (portal.status == PortalStatus::Cooldown && portal.cooldownRemaining > 0) {
            portal.cooldownRemaining -= deltaTime;
            if (portal.cooldownRemaining <= 0) {
                portal.cooldownRemaining = 0;
                portal.status = PortalStatus::Active;
            }
        }
    }
}

void PortalManager::UpdateActivations(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_portalsMutex);

    for (auto& [id, portal] : m_portals) {
        if (portal.status == PortalStatus::Activating) {
            portal.activationProgress += deltaTime / m_config.activationTime;
            if (portal.activationProgress >= 1.0f) {
                portal.activationProgress = 1.0f;
                portal.status = PortalStatus::Active;
                m_networkDirty = true;
            }
        }
    }
}

void PortalManager::ProcessTravelCompletion(PortalTraveler& traveler) {
    // Return capacity
    {
        std::lock_guard<std::mutex> portalLock(m_portalsMutex);
        auto it = m_portals.find(traveler.sourcePortalId);
        if (it != m_portals.end()) {
            it->second.currentCapacity -= static_cast<int>(traveler.unitIds.size());
            it->second.congestionLevel = static_cast<float>(it->second.currentCapacity) /
                                         it->second.maxCapacity;
        }
    }

    traveler.progress = 1.0f;

    // Notify callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& cb : m_travelCompleteCallbacks) {
            cb(traveler);
        }
    }

    m_travelers.erase(traveler.travelerId);
}

void PortalManager::ProcessEncounter(PortalTraveler& traveler) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::lock_guard<std::mutex> portalLock(m_portalsMutex);
    auto it = m_portals.find(traveler.sourcePortalId);
    if (it == m_portals.end()) return;

    const auto& config = it->second.travelConfig;
    if (dist(gen) > config.encounterChance) return;

    if (!config.possibleEncounters.empty()) {
        std::uniform_int_distribution<size_t> encDist(0, config.possibleEncounters.size() - 1);
        traveler.encounterId = config.possibleEncounters[encDist(gen)];
        traveler.interrupted = config.canBeInterrupted;

        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& cb : m_encounterCallbacks) {
            cb(traveler, traveler.encounterId);
        }
    }
}

void PortalManager::RebuildNetworkGraph() {
    std::lock_guard<std::mutex> lock(m_networkMutex);
    m_networkEdges.clear();
    m_regionToEdges.clear();

    std::lock_guard<std::mutex> portalLock(m_portalsMutex);

    for (const auto& [id, portal] : m_portals) {
        if (portal.status != PortalStatus::Active &&
            portal.status != PortalStatus::Unstable) continue;
        if (portal.destinationRegionId.empty()) continue;

        PortalNetworkEdge edge;
        edge.sourcePortalId = portal.id;
        edge.targetPortalId = portal.destinationPortalId;
        edge.sourceRegionId = portal.regionId;
        edge.targetRegionId = portal.destinationRegionId;
        edge.baseTravelTime = portal.travelConfig.baseTravelTime;
        edge.currentTravelTime = edge.baseTravelTime * (1.0f + portal.congestionLevel);
        edge.bidirectional = portal.bidirectional;
        edge.active = true;
        edge.minLevel = portal.requirements.minLevel;

        size_t edgeIdx = m_networkEdges.size();
        m_networkEdges.push_back(edge);
        m_regionToEdges[portal.regionId].push_back(edgeIdx);

        if (portal.bidirectional) {
            m_regionToEdges[portal.destinationRegionId].push_back(edgeIdx);
        }
    }
}

std::string PortalManager::GenerateTravelId() {
    return "travel_" + std::to_string(m_nextTravelId++);
}

} // namespace Vehement
