#include "TravelManager.hpp"
#include "WorldMap.hpp"
#include <chrono>
#include <random>
#include <algorithm>

namespace Vehement {

// ============================================================================
// TravelEncounter Implementation
// ============================================================================

nlohmann::json TravelEncounter::ToJson() const {
    nlohmann::json rewardsJson, costsJson;
    for (const auto& [k, v] : rewards) rewardsJson[k] = v;
    for (const auto& [k, v] : costs) costsJson[k] = v;

    return {
        {"encounterId", encounterId},
        {"encounterType", encounterType},
        {"name", name},
        {"description", description},
        {"travelProgressTrigger", travelProgressTrigger},
        {"mandatory", mandatory},
        {"combat", combat},
        {"enemySpawns", enemySpawns},
        {"rewards", rewardsJson},
        {"costs", costsJson},
        {"delaySeconds", delaySeconds}
    };
}

TravelEncounter TravelEncounter::FromJson(const nlohmann::json& j) {
    TravelEncounter e;
    e.encounterId = j.value("encounterId", "");
    e.encounterType = j.value("encounterType", "");
    e.name = j.value("name", "");
    e.description = j.value("description", "");
    e.travelProgressTrigger = j.value("travelProgressTrigger", 0.5f);
    e.mandatory = j.value("mandatory", false);
    e.combat = j.value("combat", false);
    e.delaySeconds = j.value("delaySeconds", 0.0f);

    if (j.contains("enemySpawns") && j["enemySpawns"].is_array()) {
        for (const auto& s : j["enemySpawns"]) e.enemySpawns.push_back(s.get<std::string>());
    }
    if (j.contains("rewards") && j["rewards"].is_object()) {
        for (auto& [k, v] : j["rewards"].items()) e.rewards[k] = v.get<int>();
    }
    if (j.contains("costs") && j["costs"].is_object()) {
        for (auto& [k, v] : j["costs"].items()) e.costs[k] = v.get<int>();
    }

    return e;
}

// ============================================================================
// TransferLimits Implementation
// ============================================================================

nlohmann::json TransferLimits::ToJson() const {
    nlohmann::json maxAmountsJson;
    for (const auto& [k, v] : maxResourceAmounts) maxAmountsJson[k] = v;

    return {
        {"maxUnitsPerTrip", maxUnitsPerTrip},
        {"maxResourceTypesPerTrip", maxResourceTypesPerTrip},
        {"maxResourceAmounts", maxAmountsJson},
        {"carryCapacityPerUnit", carryCapacityPerUnit},
        {"allowHeroes", allowHeroes},
        {"allowSiegeUnits", allowSiegeUnits},
        {"allowBuildings", allowBuildings}
    };
}

TransferLimits TransferLimits::FromJson(const nlohmann::json& j) {
    TransferLimits l;
    l.maxUnitsPerTrip = j.value("maxUnitsPerTrip", 100);
    l.maxResourceTypesPerTrip = j.value("maxResourceTypesPerTrip", 10);
    l.carryCapacityPerUnit = j.value("carryCapacityPerUnit", 100.0f);
    l.allowHeroes = j.value("allowHeroes", true);
    l.allowSiegeUnits = j.value("allowSiegeUnits", false);
    l.allowBuildings = j.value("allowBuildings", false);

    if (j.contains("maxResourceAmounts") && j["maxResourceAmounts"].is_object()) {
        for (auto& [k, v] : j["maxResourceAmounts"].items()) {
            l.maxResourceAmounts[k] = v.get<int>();
        }
    }

    return l;
}

// ============================================================================
// TravelSession Implementation
// ============================================================================

nlohmann::json TravelSession::ToJson() const {
    nlohmann::json resourcesJson, resourcesLostJson;
    for (const auto& [k, v] : resources) resourcesJson[k] = v;
    for (const auto& [k, v] : resourcesLost) resourcesLostJson[k] = v;

    nlohmann::json encountersJson = nlohmann::json::array();
    for (const auto& e : pendingEncounters) encountersJson.push_back(e.ToJson());

    return {
        {"sessionId", sessionId},
        {"playerId", playerId},
        {"state", static_cast<int>(state)},
        {"sourceRegionId", sourceRegionId},
        {"destinationRegionId", destinationRegionId},
        {"portalPath", portalPath},
        {"currentPortalIndex", currentPortalIndex},
        {"unitIds", unitIds},
        {"heroIds", heroIds},
        {"resources", resourcesJson},
        {"startTimestamp", startTimestamp},
        {"estimatedArrival", estimatedArrival},
        {"progress", progress},
        {"totalTravelTime", totalTravelTime},
        {"elapsedTime", elapsedTime},
        {"pendingEncounters", encountersJson},
        {"encounterResolved", encounterResolved},
        {"cancelled", cancelled},
        {"failureReason", failureReason},
        {"unitsLost", unitsLost},
        {"resourcesLost", resourcesLostJson}
    };
}

TravelSession TravelSession::FromJson(const nlohmann::json& j) {
    TravelSession s;
    s.sessionId = j.value("sessionId", "");
    s.playerId = j.value("playerId", "");
    s.state = static_cast<TravelState>(j.value("state", 0));
    s.sourceRegionId = j.value("sourceRegionId", "");
    s.destinationRegionId = j.value("destinationRegionId", "");
    s.currentPortalIndex = j.value("currentPortalIndex", 0);
    s.startTimestamp = j.value("startTimestamp", 0);
    s.estimatedArrival = j.value("estimatedArrival", 0);
    s.progress = j.value("progress", 0.0f);
    s.totalTravelTime = j.value("totalTravelTime", 0.0f);
    s.elapsedTime = j.value("elapsedTime", 0.0f);
    s.encounterResolved = j.value("encounterResolved", false);
    s.cancelled = j.value("cancelled", false);
    s.failureReason = j.value("failureReason", "");
    s.unitsLost = j.value("unitsLost", 0);

    auto loadStrArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) result.push_back(item.get<std::string>());
        }
        return result;
    };

    s.portalPath = loadStrArray("portalPath");
    s.unitIds = loadStrArray("unitIds");
    s.heroIds = loadStrArray("heroIds");

    if (j.contains("resources") && j["resources"].is_object()) {
        for (auto& [k, v] : j["resources"].items()) s.resources[k] = v.get<int>();
    }
    if (j.contains("resourcesLost") && j["resourcesLost"].is_object()) {
        for (auto& [k, v] : j["resourcesLost"].items()) s.resourcesLost[k] = v.get<int>();
    }
    if (j.contains("pendingEncounters") && j["pendingEncounters"].is_array()) {
        for (const auto& e : j["pendingEncounters"]) {
            s.pendingEncounters.push_back(TravelEncounter::FromJson(e));
        }
    }

    return s;
}

// ============================================================================
// TravelRequest Implementation
// ============================================================================

nlohmann::json TravelRequest::ToJson() const {
    nlohmann::json resourcesJson;
    for (const auto& [k, v] : resources) resourcesJson[k] = v;

    return {
        {"playerId", playerId},
        {"sourceRegionId", sourceRegionId},
        {"destinationRegionId", destinationRegionId},
        {"unitIds", unitIds},
        {"heroIds", heroIds},
        {"resources", resourcesJson},
        {"useShortestPath", useShortestPath},
        {"allowDangerousRegions", allowDangerousRegions},
        {"maxDangerLevel", maxDangerLevel}
    };
}

TravelRequest TravelRequest::FromJson(const nlohmann::json& j) {
    TravelRequest r;
    r.playerId = j.value("playerId", "");
    r.sourceRegionId = j.value("sourceRegionId", "");
    r.destinationRegionId = j.value("destinationRegionId", "");
    r.useShortestPath = j.value("useShortestPath", true);
    r.allowDangerousRegions = j.value("allowDangerousRegions", false);
    r.maxDangerLevel = j.value("maxDangerLevel", 5);

    auto loadStrArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) result.push_back(item.get<std::string>());
        }
        return result;
    };

    r.unitIds = loadStrArray("unitIds");
    r.heroIds = loadStrArray("heroIds");

    if (j.contains("resources") && j["resources"].is_object()) {
        for (auto& [k, v] : j["resources"].items()) r.resources[k] = v.get<int>();
    }

    return r;
}

// ============================================================================
// TravelManager Implementation
// ============================================================================

TravelManager& TravelManager::Instance() {
    static TravelManager instance;
    return instance;
}

bool TravelManager::Initialize(const TravelManagerConfig& config) {
    if (m_initialized) return true;
    m_config = config;
    m_initialized = true;
    return true;
}

void TravelManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    m_sessions.clear();
    m_initialized = false;
}

void TravelManager::Update(float deltaTime) {
    if (!m_initialized) return;
    UpdateSessions(deltaTime);
}

TravelResult TravelManager::RequestTravel(const TravelRequest& request) {
    TravelResult result = ValidateTravel(request);
    if (!result.success) return result;

    // Find path
    auto path = WorldMap::Instance().FindShortestPath(request.sourceRegionId, request.destinationRegionId);
    if (!path.valid) {
        result.success = false;
        result.errorMessage = "No valid path to destination";
        return result;
    }

    // Calculate cost
    auto cost = CalculateTravelCost(request.sourceRegionId, request.destinationRegionId,
                                    static_cast<int>(request.unitIds.size()));

    if (!CanAffordTravel(request.playerId, cost)) {
        result.success = false;
        result.errorMessage = "Insufficient resources for travel";
        return result;
    }

    // Create session
    TravelSession session;
    session.sessionId = GenerateSessionId();
    session.playerId = request.playerId;
    session.state = TravelState::Preparing;
    session.sourceRegionId = request.sourceRegionId;
    session.destinationRegionId = request.destinationRegionId;
    session.portalPath = path.portalIds;
    session.unitIds = request.unitIds;
    session.heroIds = request.heroIds;
    session.resources = request.resources;
    session.startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    session.totalTravelTime = path.totalTravelTime;
    session.estimatedArrival = session.startTimestamp + static_cast<int64_t>(session.totalTravelTime);
    session.pendingEncounters = GenerateEncounters(session);

    // Deduct cost
    DeductTravelCost(request.playerId, cost);

    // Start travel
    session.state = TravelState::InTransit;

    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_sessions[session.sessionId] = session;
    }

    // Preload destination
    PreloadRegion(request.destinationRegionId);

    // Notify callbacks
    {
        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& cb : m_startCallbacks) {
            cb(session);
        }
    }

    result.sessionId = session.sessionId;
    result.estimatedTime = session.totalTravelTime;
    result.totalCost = cost;

    return result;
}

bool TravelManager::CancelTravel(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return false;

    if (!m_config.allowCancelDuringTravel && it->second.state == TravelState::InTransit) {
        return false;
    }

    it->second.state = TravelState::Failed;
    it->second.cancelled = true;
    it->second.failureReason = "Cancelled by player";

    // Apply penalty
    // Resources lost based on progress
    float lossPercent = it->second.progress * m_config.cancelPenaltyPercent / 100.0f;
    for (auto& [res, amount] : it->second.resources) {
        int lost = static_cast<int>(amount * lossPercent);
        it->second.resourcesLost[res] = lost;
    }

    return true;
}

const TravelSession* TravelManager::GetSession(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    return it != m_sessions.end() ? &it->second : nullptr;
}

std::vector<TravelSession> TravelManager::GetPlayerSessions(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    std::vector<TravelSession> result;
    for (const auto& [id, session] : m_sessions) {
        if (session.playerId == playerId) {
            result.push_back(session);
        }
    }
    return result;
}

int TravelManager::GetActiveSessionCount() const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    int count = 0;
    for (const auto& [id, session] : m_sessions) {
        if (session.state == TravelState::InTransit || session.state == TravelState::Preparing) {
            count++;
        }
    }
    return count;
}

TravelResult TravelManager::ValidateTravel(const TravelRequest& request) const {
    TravelResult result;
    result.success = true;

    if (request.playerId.empty()) {
        result.success = false;
        result.errorMessage = "Invalid player ID";
        return result;
    }

    if (request.sourceRegionId == request.destinationRegionId) {
        result.success = false;
        result.errorMessage = "Already at destination";
        return result;
    }

    auto& regionMgr = RegionManager::Instance();
    const auto* srcRegion = regionMgr.GetRegion(request.sourceRegionId);
    const auto* dstRegion = regionMgr.GetRegion(request.destinationRegionId);

    if (!srcRegion || !dstRegion) {
        result.success = false;
        result.errorMessage = "Invalid region";
        return result;
    }

    if (!dstRegion->accessible) {
        result.success = false;
        result.errorMessage = "Destination region is not accessible";
        return result;
    }

    if (!request.allowDangerousRegions && dstRegion->dangerLevel > request.maxDangerLevel) {
        result.success = false;
        result.errorMessage = "Destination region is too dangerous";
        result.warnings.push_back("Danger level: " + std::to_string(dstRegion->dangerLevel));
        return result;
    }

    // Check unit limits
    if (static_cast<int>(request.unitIds.size()) > m_config.defaultLimits.maxUnitsPerTrip) {
        result.success = false;
        result.errorMessage = "Too many units for travel";
        return result;
    }

    return result;
}

std::unordered_map<std::string, int> TravelManager::CalculateTravelCost(
    const std::string& sourceRegion,
    const std::string& destRegion,
    int unitCount) const {

    std::unordered_map<std::string, int> cost;

    double distance = WorldMap::Instance().GetRegionDistance(sourceRegion, destRegion);
    if (distance < 0) distance = 100.0;  // Default

    // Base cost scales with distance and units
    cost["gold"] = static_cast<int>(distance * 0.5 + unitCount * 2);
    cost["supplies"] = static_cast<int>(unitCount * 5);

    return cost;
}

float TravelManager::EstimateTravelTime(
    const std::string& sourceRegion,
    const std::string& destRegion) const {

    auto path = WorldMap::Instance().FindShortestPath(sourceRegion, destRegion);
    return path.valid ? path.totalTravelTime : -1.0f;
}

bool TravelManager::CanAffordTravel(
    const std::string& playerId,
    const std::unordered_map<std::string, int>& cost) const {
    // This would check player's actual resources
    // For now, assume they can afford it
    return true;
}

TransferLimits TravelManager::GetTransferLimits(const std::string& portalId) const {
    std::lock_guard<std::mutex> lock(m_limitsMutex);
    auto it = m_portalLimits.find(portalId);
    if (it != m_portalLimits.end()) {
        return it->second;
    }
    return m_config.defaultLimits;
}

void TravelManager::SetTransferLimits(const std::string& portalId, const TransferLimits& limits) {
    std::lock_guard<std::mutex> lock(m_limitsMutex);
    m_portalLimits[portalId] = limits;
}

bool TravelManager::ValidateCargo(
    const std::vector<std::string>& units,
    const std::unordered_map<std::string, int>& resources,
    const TransferLimits& limits) const {

    if (static_cast<int>(units.size()) > limits.maxUnitsPerTrip) {
        return false;
    }

    if (static_cast<int>(resources.size()) > limits.maxResourceTypesPerTrip) {
        return false;
    }

    int totalResources = 0;
    for (const auto& [type, amount] : resources) {
        totalResources += amount;
        auto it = limits.maxResourceAmounts.find(type);
        if (it != limits.maxResourceAmounts.end() && amount > it->second) {
            return false;
        }
    }

    float maxCapacity = static_cast<float>(units.size()) * limits.carryCapacityPerUnit;
    if (static_cast<float>(totalResources) > maxCapacity) {
        return false;
    }

    return true;
}

void TravelManager::ResolveEncounter(const std::string& sessionId, const std::string& choice) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || !it->second.activeEncounter) return;

    it->second.encounterResolved = true;

    // Apply rewards/costs based on choice
    if (choice == "fight" && it->second.activeEncounter->combat) {
        // Combat resolution would happen here
        for (const auto& [res, amount] : it->second.activeEncounter->rewards) {
            it->second.resources[res] += amount;
        }
    } else if (choice == "pay") {
        for (const auto& [res, amount] : it->second.activeEncounter->costs) {
            it->second.resources[res] = std::max(0, it->second.resources[res] - amount);
        }
    }

    it->second.activeEncounter = nullptr;
}

bool TravelManager::SkipEncounter(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || !it->second.activeEncounter) return false;

    if (it->second.activeEncounter->mandatory) return false;

    it->second.encounterResolved = true;
    it->second.activeEncounter = nullptr;
    return true;
}

std::vector<std::string> TravelManager::GetEncounterChoices(const std::string& sessionId) const {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end() || !it->second.activeEncounter) return {};

    std::vector<std::string> choices;
    if (it->second.activeEncounter->combat) {
        choices.push_back("fight");
    }
    if (!it->second.activeEncounter->costs.empty()) {
        choices.push_back("pay");
    }
    if (!it->second.activeEncounter->mandatory) {
        choices.push_back("flee");
    }

    return choices;
}

void TravelManager::PreloadRegion(const std::string& regionId) {
    std::lock_guard<std::mutex> lock(m_loadingMutex);
    if (m_loadedRegions.find(regionId) != m_loadedRegions.end()) return;

    m_loadingProgress[regionId] = 0.0f;
    // Actual loading would happen asynchronously
    m_loadingProgress[regionId] = 1.0f;
    m_loadedRegions.insert(regionId);
}

void TravelManager::UnloadRegion(const std::string& regionId) {
    std::lock_guard<std::mutex> lock(m_loadingMutex);
    m_loadedRegions.erase(regionId);
    m_loadingProgress.erase(regionId);
}

bool TravelManager::IsRegionLoaded(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_loadingMutex);
    return m_loadedRegions.find(regionId) != m_loadedRegions.end();
}

float TravelManager::GetRegionLoadProgress(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_loadingMutex);
    auto it = m_loadingProgress.find(regionId);
    return it != m_loadingProgress.end() ? it->second : 0.0f;
}

void TravelManager::OnTravelStarted(TravelStartCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_startCallbacks.push_back(std::move(callback));
}

void TravelManager::OnTravelProgress(TravelProgressCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_progressCallbacks.push_back(std::move(callback));
}

void TravelManager::OnTravelCompleted(TravelCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_completeCallbacks.push_back(std::move(callback));
}

void TravelManager::OnEncounter(EncounterCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_encounterCallbacks.push_back(std::move(callback));
}

void TravelManager::UpdateSessions(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    std::vector<std::string> completedSessions;

    for (auto& [id, session] : m_sessions) {
        if (session.state != TravelState::InTransit) continue;

        // Wait for encounter resolution
        if (session.activeEncounter && !session.encounterResolved) continue;

        session.elapsedTime += deltaTime;
        session.progress = session.totalTravelTime > 0 ?
            session.elapsedTime / session.totalTravelTime : 1.0f;

        // Check for encounters
        for (auto& encounter : session.pendingEncounters) {
            if (session.progress >= encounter.travelProgressTrigger) {
                session.activeEncounter = &encounter;
                session.encounterResolved = false;

                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                for (auto& cb : m_encounterCallbacks) {
                    cb(session, encounter);
                }
                break;
            }
        }

        // Notify progress
        {
            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            for (const auto& cb : m_progressCallbacks) {
                cb(session);
            }
        }

        // Check completion
        if (session.progress >= 1.0f) {
            ProcessArrival(session);
            completedSessions.push_back(id);
        }
    }

    // Notify completions and cleanup
    for (const auto& id : completedSessions) {
        auto& session = m_sessions[id];

        std::lock_guard<std::mutex> cbLock(m_callbackMutex);
        for (const auto& cb : m_completeCallbacks) {
            cb(session);
        }
    }
}

void TravelManager::ProcessArrival(TravelSession& session) {
    session.state = TravelState::Arriving;
    session.progress = 1.0f;

    // Transfer units and resources to destination
    // This would integrate with the actual game systems

    session.state = TravelState::Idle;
}

void TravelManager::TriggerEncounter(TravelSession& session) {
    // Already handled in UpdateSessions
}

void TravelManager::DeductTravelCost(const std::string& playerId,
                                      const std::unordered_map<std::string, int>& cost) {
    // This would integrate with the resource system
}

std::string TravelManager::GenerateSessionId() {
    return "travel_" + std::to_string(m_nextSessionId++);
}

std::vector<TravelEncounter> TravelManager::GenerateEncounters(const TravelSession& session) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::vector<TravelEncounter> encounters;

    // Base encounter chance
    float baseChance = m_config.encounterCheckInterval * 0.1f;

    // Generate 0-3 encounters based on travel time
    int maxEncounters = static_cast<int>(session.totalTravelTime / 60.0f);
    maxEncounters = std::clamp(maxEncounters, 0, 3);

    for (int i = 0; i < maxEncounters; ++i) {
        if (dist(gen) < baseChance) {
            TravelEncounter encounter;
            encounter.encounterId = "enc_" + std::to_string(i);
            encounter.travelProgressTrigger = (i + 1) * 0.3f;

            float roll = dist(gen);
            if (roll < 0.4f) {
                encounter.encounterType = "merchant";
                encounter.name = "Traveling Merchant";
                encounter.description = "A merchant offers their wares.";
                encounter.combat = false;
                encounter.rewards["gold"] = -50;  // Cost to buy
                encounter.rewards["supplies"] = 20;
            } else if (roll < 0.7f) {
                encounter.encounterType = "ambush";
                encounter.name = "Bandit Ambush";
                encounter.description = "Bandits attack your caravan!";
                encounter.combat = true;
                encounter.enemySpawns.push_back("bandit_basic");
                encounter.rewards["gold"] = 30;
            } else {
                encounter.encounterType = "event";
                encounter.name = "Strange Portal";
                encounter.description = "A mysterious portal flickers nearby.";
                encounter.mandatory = false;
            }

            encounters.push_back(encounter);
        }
    }

    return encounters;
}

} // namespace Vehement
