#include "TerritoryControl.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// ControlPoint Implementation
// ============================================================================

nlohmann::json ControlPoint::ToJson() const {
    return {
        {"id", id},
        {"name", name},
        {"regionId", regionId},
        {"location", {{"lat", location.latitude}, {"lon", location.longitude}}},
        {"worldPosition", {worldPosition.x, worldPosition.y, worldPosition.z}},
        {"status", static_cast<int>(status)},
        {"controllingFaction", controllingFaction},
        {"controllingPlayerId", controllingPlayerId},
        {"captureProgress", captureProgress},
        {"influenceRadius", influenceRadius},
        {"pointValue", pointValue},
        {"capturingFaction", capturingFaction},
        {"capturingPlayerId", capturingPlayerId},
        {"captureRate", captureRate},
        {"defendBonus", defendBonus},
        {"minPlayersToCapture", minPlayersToCapture},
        {"minUnitsToCapture", minUnitsToCapture},
        {"requiresAdjacentControl", requiresAdjacentControl},
        {"resourceBonusPercent", resourceBonusPercent},
        {"experienceBonusPercent", experienceBonusPercent},
        {"defenseBonusPercent", defenseBonusPercent},
        {"unlocksBuildings", unlocksBuildings},
        {"unlocksUnits", unlocksUnits},
        {"lastCaptureTimestamp", lastCaptureTimestamp},
        {"controlDuration", controlDuration},
        {"captureTimeRequired", captureTimeRequired}
    };
}

ControlPoint ControlPoint::FromJson(const nlohmann::json& j) {
    ControlPoint p;
    p.id = j.value("id", "");
    p.name = j.value("name", "");
    p.regionId = j.value("regionId", "");

    if (j.contains("location")) {
        p.location.latitude = j["location"].value("lat", 0.0);
        p.location.longitude = j["location"].value("lon", 0.0);
    }
    if (j.contains("worldPosition") && j["worldPosition"].is_array() && j["worldPosition"].size() >= 3) {
        p.worldPosition = glm::vec3(j["worldPosition"][0], j["worldPosition"][1], j["worldPosition"][2]);
    }

    p.status = static_cast<ControlPointStatus>(j.value("status", 0));
    p.controllingFaction = j.value("controllingFaction", -1);
    p.controllingPlayerId = j.value("controllingPlayerId", "");
    p.captureProgress = j.value("captureProgress", 0.0f);
    p.influenceRadius = j.value("influenceRadius", 500.0f);
    p.pointValue = j.value("pointValue", 1);
    p.capturingFaction = j.value("capturingFaction", -1);
    p.capturingPlayerId = j.value("capturingPlayerId", "");
    p.captureRate = j.value("captureRate", 1.0f);
    p.defendBonus = j.value("defendBonus", 1.5f);
    p.minPlayersToCapture = j.value("minPlayersToCapture", 1);
    p.minUnitsToCapture = j.value("minUnitsToCapture", 5);
    p.requiresAdjacentControl = j.value("requiresAdjacentControl", false);
    p.resourceBonusPercent = j.value("resourceBonusPercent", 10.0f);
    p.experienceBonusPercent = j.value("experienceBonusPercent", 5.0f);
    p.defenseBonusPercent = j.value("defenseBonusPercent", 20.0f);
    p.lastCaptureTimestamp = j.value("lastCaptureTimestamp", 0);
    p.controlDuration = j.value("controlDuration", 0);
    p.captureTimeRequired = j.value("captureTimeRequired", 300.0f);

    auto loadStrArray = [&j](const std::string& key) {
        std::vector<std::string> result;
        if (j.contains(key) && j[key].is_array()) {
            for (const auto& item : j[key]) result.push_back(item.get<std::string>());
        }
        return result;
    };

    p.unlocksBuildings = loadStrArray("unlocksBuildings");
    p.unlocksUnits = loadStrArray("unlocksUnits");

    return p;
}

// ============================================================================
// InfluenceNode Implementation
// ============================================================================

nlohmann::json InfluenceNode::ToJson() const {
    return {
        {"sourcePointId", sourcePointId},
        {"factionId", factionId},
        {"strength", strength},
        {"decayRate", decayRate},
        {"spreadRate", spreadRate},
        {"maxRadius", maxRadius}
    };
}

InfluenceNode InfluenceNode::FromJson(const nlohmann::json& j) {
    InfluenceNode n;
    n.sourcePointId = j.value("sourcePointId", "");
    n.factionId = j.value("factionId", -1);
    n.strength = j.value("strength", 0.0f);
    n.decayRate = j.value("decayRate", 0.1f);
    n.spreadRate = j.value("spreadRate", 0.05f);
    n.maxRadius = j.value("maxRadius", 1000.0f);
    return n;
}

// ============================================================================
// VictoryCondition Implementation
// ============================================================================

nlohmann::json VictoryCondition::ToJson() const {
    nlohmann::json factionPointsJson, factionHoldTimeJson;
    for (const auto& [k, v] : factionPoints) factionPointsJson[std::to_string(k)] = v;
    for (const auto& [k, v] : factionHoldTime) factionHoldTimeJson[std::to_string(k)] = v;

    return {
        {"id", id},
        {"name", name},
        {"description", description},
        {"type", type},
        {"targetPoints", targetPoints},
        {"pointsPerControlled", pointsPerControlled},
        {"pointsPerSecond", pointsPerSecond},
        {"holdTimeSeconds", holdTimeSeconds},
        {"controlPercentRequired", controlPercentRequired},
        {"factionPoints", factionPointsJson},
        {"factionHoldTime", factionHoldTimeJson},
        {"achieved", achieved},
        {"winningFaction", winningFaction}
    };
}

VictoryCondition VictoryCondition::FromJson(const nlohmann::json& j) {
    VictoryCondition v;
    v.id = j.value("id", "");
    v.name = j.value("name", "");
    v.description = j.value("description", "");
    v.type = j.value("type", "points");
    v.targetPoints = j.value("targetPoints", 1000);
    v.pointsPerControlled = j.value("pointsPerControlled", 1.0f);
    v.pointsPerSecond = j.value("pointsPerSecond", 0.1f);
    v.holdTimeSeconds = j.value("holdTimeSeconds", 3600.0f);
    v.controlPercentRequired = j.value("controlPercentRequired", 75.0f);
    v.achieved = j.value("achieved", false);
    v.winningFaction = j.value("winningFaction", -1);

    if (j.contains("factionPoints") && j["factionPoints"].is_object()) {
        for (auto& [k, val] : j["factionPoints"].items()) {
            v.factionPoints[std::stoi(k)] = val.get<int>();
        }
    }
    if (j.contains("factionHoldTime") && j["factionHoldTime"].is_object()) {
        for (auto& [k, val] : j["factionHoldTime"].items()) {
            v.factionHoldTime[std::stoi(k)] = val.get<float>();
        }
    }

    return v;
}

// ============================================================================
// CaptureAttempt Implementation
// ============================================================================

nlohmann::json CaptureAttempt::ToJson() const {
    return {
        {"pointId", pointId},
        {"playerId", playerId},
        {"factionId", factionId},
        {"startTimestamp", startTimestamp},
        {"progressAtStart", progressAtStart},
        {"unitsInvolved", unitsInvolved},
        {"successful", successful},
        {"interrupted", interrupted},
        {"interruptReason", interruptReason}
    };
}

CaptureAttempt CaptureAttempt::FromJson(const nlohmann::json& j) {
    CaptureAttempt a;
    a.pointId = j.value("pointId", "");
    a.playerId = j.value("playerId", "");
    a.factionId = j.value("factionId", -1);
    a.startTimestamp = j.value("startTimestamp", 0);
    a.progressAtStart = j.value("progressAtStart", 0.0f);
    a.unitsInvolved = j.value("unitsInvolved", 0);
    a.successful = j.value("successful", false);
    a.interrupted = j.value("interrupted", false);
    a.interruptReason = j.value("interruptReason", "");
    return a;
}

// ============================================================================
// TerritoryControlManager Implementation
// ============================================================================

TerritoryControlManager& TerritoryControlManager::Instance() {
    static TerritoryControlManager instance;
    return instance;
}

bool TerritoryControlManager::Initialize(const TerritoryControlConfig& config) {
    if (m_initialized) return true;
    m_config = config;
    m_initialized = true;
    return true;
}

void TerritoryControlManager::Shutdown() {
    m_controlPoints.clear();
    m_influenceNodes.clear();
    m_captureHistory.clear();
    m_activeCaptures.clear();
    m_initialized = false;
}

void TerritoryControlManager::Update(float deltaTime) {
    if (!m_initialized) return;

    UpdateCaptureProgress(deltaTime);

    m_influenceUpdateTimer += deltaTime;
    m_victoryUpdateTimer += deltaTime;

    if (m_influenceUpdateTimer >= INFLUENCE_UPDATE_INTERVAL) {
        UpdateInfluenceSpread(m_influenceUpdateTimer);
        m_influenceUpdateTimer = 0.0f;
    }

    if (m_victoryUpdateTimer >= VICTORY_UPDATE_INTERVAL) {
        UpdateVictoryCondition(m_victoryUpdateTimer);
        m_victoryUpdateTimer = 0.0f;
    }
}

void TerritoryControlManager::RegisterControlPoint(const ControlPoint& point) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    m_controlPoints[point.id] = point;
}

const ControlPoint* TerritoryControlManager::GetControlPoint(const std::string& pointId) const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    auto it = m_controlPoints.find(pointId);
    return it != m_controlPoints.end() ? &it->second : nullptr;
}

std::vector<ControlPoint> TerritoryControlManager::GetRegionControlPoints(const std::string& regionId) const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    std::vector<ControlPoint> result;
    for (const auto& [id, point] : m_controlPoints) {
        if (point.regionId == regionId) {
            result.push_back(point);
        }
    }
    return result;
}

std::vector<ControlPoint> TerritoryControlManager::GetFactionControlPoints(int factionId) const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    std::vector<ControlPoint> result;
    for (const auto& [id, point] : m_controlPoints) {
        if (point.controllingFaction == factionId) {
            result.push_back(point);
        }
    }
    return result;
}

bool TerritoryControlManager::StartCapture(const std::string& pointId, const std::string& playerId,
                                            int factionId, int unitCount) {
    if (!CanCapture(pointId, factionId)) return false;

    std::lock_guard<std::mutex> lock(m_captureMutex);

    ActiveCapture capture;
    capture.playerId = playerId;
    capture.factionId = factionId;
    capture.unitCount = unitCount;
    capture.startTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_activeCaptures[pointId].push_back(capture);

    // Update point status
    {
        std::lock_guard<std::mutex> pointLock(m_pointsMutex);
        auto it = m_controlPoints.find(pointId);
        if (it != m_controlPoints.end()) {
            if (it->second.status == ControlPointStatus::Neutral ||
                it->second.status == ControlPointStatus::Controlled) {
                it->second.status = ControlPointStatus::Capturing;
                it->second.capturingFaction = factionId;
                it->second.capturingPlayerId = playerId;
            } else if (it->second.capturingFaction != factionId) {
                it->second.status = ControlPointStatus::Contested;

                std::lock_guard<std::mutex> cbLock(m_callbackMutex);
                for (const auto& cb : m_contestedCallbacks) {
                    cb(it->second);
                }
            }
        }
    }

    // Record attempt
    {
        std::lock_guard<std::mutex> histLock(m_historyMutex);
        CaptureAttempt attempt;
        attempt.pointId = pointId;
        attempt.playerId = playerId;
        attempt.factionId = factionId;
        attempt.startTimestamp = capture.startTime;
        attempt.unitsInvolved = unitCount;

        const auto* point = GetControlPoint(pointId);
        if (point) {
            attempt.progressAtStart = point->captureProgress;
        }

        m_captureHistory[pointId].push_back(attempt);
    }

    return true;
}

void TerritoryControlManager::StopCapture(const std::string& pointId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_captureMutex);

    auto it = m_activeCaptures.find(pointId);
    if (it == m_activeCaptures.end()) return;

    auto& captures = it->second;
    captures.erase(std::remove_if(captures.begin(), captures.end(),
        [&playerId](const ActiveCapture& c) { return c.playerId == playerId; }),
        captures.end());

    // Update point status if no more captures
    if (captures.empty()) {
        std::lock_guard<std::mutex> pointLock(m_pointsMutex);
        auto pointIt = m_controlPoints.find(pointId);
        if (pointIt != m_controlPoints.end()) {
            if (pointIt->second.captureProgress < 100.0f) {
                pointIt->second.status = pointIt->second.controllingFaction >= 0 ?
                    ControlPointStatus::Controlled : ControlPointStatus::Neutral;
                pointIt->second.capturingFaction = -1;
                pointIt->second.capturingPlayerId.clear();
            }
        }
    }
}

void TerritoryControlManager::ForceCapture(const std::string& pointId, int factionId,
                                            const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);

    auto it = m_controlPoints.find(pointId);
    if (it == m_controlPoints.end()) return;

    it->second.controllingFaction = factionId;
    it->second.controllingPlayerId = playerId;
    it->second.captureProgress = 100.0f;
    it->second.status = ControlPointStatus::Controlled;
    it->second.capturingFaction = -1;
    it->second.capturingPlayerId.clear();
    it->second.lastCaptureTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& cb : m_capturedCallbacks) {
        cb(it->second);
    }
}

void TerritoryControlManager::NeutralizePoint(const std::string& pointId) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);

    auto it = m_controlPoints.find(pointId);
    if (it == m_controlPoints.end()) return;

    it->second.controllingFaction = -1;
    it->second.controllingPlayerId.clear();
    it->second.captureProgress = 0.0f;
    it->second.status = ControlPointStatus::Neutral;
    it->second.capturingFaction = -1;
    it->second.capturingPlayerId.clear();
}

bool TerritoryControlManager::CanCapture(const std::string& pointId, int factionId) const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);

    auto it = m_controlPoints.find(pointId);
    if (it == m_controlPoints.end()) return false;

    const auto& point = it->second;

    if (point.status == ControlPointStatus::Locked) return false;
    if (point.controllingFaction == factionId) return false;

    return true;
}

float TerritoryControlManager::GetInfluenceAt(const Geo::GeoCoordinate& coord, int factionId) const {
    std::lock_guard<std::mutex> lock(m_influenceMutex);

    float totalInfluence = 0.0f;

    for (const auto& [id, node] : m_influenceNodes) {
        if (node.factionId != factionId) continue;

        std::lock_guard<std::mutex> pointLock(m_pointsMutex);
        auto pointIt = m_controlPoints.find(node.sourcePointId);
        if (pointIt == m_controlPoints.end()) continue;

        double distance = coord.DistanceTo(pointIt->second.location);
        if (distance < node.maxRadius) {
            float falloff = 1.0f - static_cast<float>(distance / node.maxRadius);
            totalInfluence += node.strength * falloff;
        }
    }

    return totalInfluence;
}

int TerritoryControlManager::GetDominantFactionAt(const Geo::GeoCoordinate& coord) const {
    std::unordered_map<int, float> influences;

    std::lock_guard<std::mutex> lock(m_influenceMutex);
    for (const auto& [id, node] : m_influenceNodes) {
        float influence = GetInfluenceAt(coord, node.factionId);
        influences[node.factionId] = std::max(influences[node.factionId], influence);
    }

    int dominant = -1;
    float maxInfluence = 0.0f;
    for (const auto& [faction, influence] : influences) {
        if (influence > maxInfluence) {
            maxInfluence = influence;
            dominant = faction;
        }
    }

    return dominant;
}

float TerritoryControlManager::GetRegionInfluence(const std::string& regionId, int factionId) const {
    auto points = GetRegionControlPoints(regionId);
    float total = 0.0f;

    for (const auto& point : points) {
        if (point.controllingFaction == factionId) {
            total += point.pointValue * (point.captureProgress / 100.0f);
        }
    }

    return total;
}

float TerritoryControlManager::GetRegionControlPercent(const std::string& regionId, int factionId) const {
    auto points = GetRegionControlPoints(regionId);
    if (points.empty()) return 0.0f;

    int totalValue = 0;
    int controlledValue = 0;

    for (const auto& point : points) {
        totalValue += point.pointValue;
        if (point.controllingFaction == factionId && point.captureProgress >= 100.0f) {
            controlledValue += point.pointValue;
        }
    }

    return totalValue > 0 ? (static_cast<float>(controlledValue) / totalValue * 100.0f) : 0.0f;
}

void TerritoryControlManager::SetVictoryCondition(const VictoryCondition& condition) {
    std::lock_guard<std::mutex> lock(m_victoryMutex);
    m_victoryCondition = condition;
}

const VictoryCondition* TerritoryControlManager::GetVictoryCondition() const {
    std::lock_guard<std::mutex> lock(m_victoryMutex);
    return &m_victoryCondition;
}

int TerritoryControlManager::GetFactionPoints(int factionId) const {
    std::lock_guard<std::mutex> lock(m_victoryMutex);
    auto it = m_victoryCondition.factionPoints.find(factionId);
    return it != m_victoryCondition.factionPoints.end() ? it->second : 0;
}

bool TerritoryControlManager::CheckVictory() const {
    std::lock_guard<std::mutex> lock(m_victoryMutex);
    return m_victoryCondition.achieved;
}

int TerritoryControlManager::GetWinningFaction() const {
    std::lock_guard<std::mutex> lock(m_victoryMutex);
    return m_victoryCondition.winningFaction;
}

std::vector<CaptureAttempt> TerritoryControlManager::GetCaptureHistory(const std::string& pointId) const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    auto it = m_captureHistory.find(pointId);
    return it != m_captureHistory.end() ? it->second : std::vector<CaptureAttempt>{};
}

std::unordered_map<int, int> TerritoryControlManager::GetControlledPointsCount() const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    std::unordered_map<int, int> counts;

    for (const auto& [id, point] : m_controlPoints) {
        if (point.controllingFaction >= 0) {
            counts[point.controllingFaction]++;
        }
    }

    return counts;
}

std::vector<ControlPoint> TerritoryControlManager::GetContestedPoints() const {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    std::vector<ControlPoint> result;

    for (const auto& [id, point] : m_controlPoints) {
        if (point.status == ControlPointStatus::Contested ||
            point.status == ControlPointStatus::Capturing) {
            result.push_back(point);
        }
    }

    return result;
}

void TerritoryControlManager::OnPointCaptured(PointCapturedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_capturedCallbacks.push_back(std::move(callback));
}

void TerritoryControlManager::OnPointContested(PointContestedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_contestedCallbacks.push_back(std::move(callback));
}

void TerritoryControlManager::OnVictory(VictoryCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_victoryCallbacks.push_back(std::move(callback));
}

void TerritoryControlManager::OnInfluenceChanged(InfluenceChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_influenceCallbacks.push_back(std::move(callback));
}

void TerritoryControlManager::UpdateCaptureProgress(float deltaTime) {
    std::lock_guard<std::mutex> capLock(m_captureMutex);
    std::lock_guard<std::mutex> pointLock(m_pointsMutex);

    for (auto& [pointId, captures] : m_activeCaptures) {
        if (captures.empty()) continue;

        auto pointIt = m_controlPoints.find(pointId);
        if (pointIt == m_controlPoints.end()) continue;

        auto& point = pointIt->second;

        // Calculate total units per faction
        std::unordered_map<int, int> factionUnits;
        for (const auto& cap : captures) {
            factionUnits[cap.factionId] += cap.unitCount;
        }

        // Find attacking and defending factions
        int attackingFaction = -1;
        int attackerUnits = 0;
        for (const auto& [faction, units] : factionUnits) {
            if (faction != point.controllingFaction && units > attackerUnits) {
                attackingFaction = faction;
                attackerUnits = units;
            }
        }

        int defenderUnits = 0;
        if (point.controllingFaction >= 0) {
            auto it = factionUnits.find(point.controllingFaction);
            if (it != factionUnits.end()) {
                defenderUnits = it->second;
            }
        }

        if (attackingFaction < 0) continue;

        float captureSpeed = CalculateCaptureSpeed(point, attackerUnits, defenderUnits);
        float progressDelta = captureSpeed * deltaTime;

        // Apply progress
        if (point.controllingFaction >= 0 && point.controllingFaction != attackingFaction) {
            // Decap first
            point.captureProgress -= progressDelta;
            if (point.captureProgress <= 0) {
                point.captureProgress = 0;
                point.controllingFaction = -1;
                point.controllingPlayerId.clear();
            }
        } else {
            // Cap
            point.captureProgress += progressDelta;
            point.capturingFaction = attackingFaction;

            if (point.captureProgress >= 100.0f) {
                ProcessCapture(point);
            }
        }
    }
}

void TerritoryControlManager::UpdateInfluenceSpread(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_influenceMutex);
    float hours = deltaTime / 3600.0f;

    // Update influence nodes from controlled points
    {
        std::lock_guard<std::mutex> pointLock(m_pointsMutex);
        for (const auto& [id, point] : m_controlPoints) {
            if (point.controllingFaction < 0) continue;

            InfluenceNode& node = m_influenceNodes[id];
            node.sourcePointId = id;
            node.factionId = point.controllingFaction;
            node.maxRadius = point.influenceRadius;

            // Grow influence over time
            float growth = node.spreadRate * hours * point.pointValue;
            node.strength = std::min(100.0f, node.strength + growth);
        }
    }

    // Decay uncontrolled influence
    for (auto& [id, node] : m_influenceNodes) {
        std::lock_guard<std::mutex> pointLock(m_pointsMutex);
        auto pointIt = m_controlPoints.find(node.sourcePointId);
        if (pointIt == m_controlPoints.end() ||
            pointIt->second.controllingFaction != node.factionId) {
            node.strength -= node.decayRate * hours;
            if (node.strength <= 0) {
                node.strength = 0;
            }
        }
    }
}

void TerritoryControlManager::UpdateVictoryCondition(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_victoryMutex);

    if (m_victoryCondition.achieved) return;

    // Update points based on controlled territories
    auto controlledCounts = GetControlledPointsCount();

    for (const auto& [faction, count] : controlledCounts) {
        float pointsGained = count * m_victoryCondition.pointsPerSecond * deltaTime;
        m_victoryCondition.factionPoints[faction] += static_cast<int>(pointsGained);

        // Check point victory
        if (m_victoryCondition.type == "points" &&
            m_victoryCondition.factionPoints[faction] >= m_victoryCondition.targetPoints) {
            m_victoryCondition.achieved = true;
            m_victoryCondition.winningFaction = faction;

            std::lock_guard<std::mutex> cbLock(m_callbackMutex);
            for (const auto& cb : m_victoryCallbacks) {
                cb(m_victoryCondition, faction);
            }
            return;
        }
    }
}

void TerritoryControlManager::ProcessCapture(ControlPoint& point) {
    point.captureProgress = 100.0f;
    point.controllingFaction = point.capturingFaction;
    point.controllingPlayerId = point.capturingPlayerId;
    point.status = ControlPointStatus::Controlled;
    point.lastCaptureTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    point.capturingFaction = -1;
    point.capturingPlayerId.clear();

    std::lock_guard<std::mutex> cbLock(m_callbackMutex);
    for (const auto& cb : m_capturedCallbacks) {
        cb(point);
    }
}

float TerritoryControlManager::CalculateCaptureSpeed(const ControlPoint& point,
                                                      int attackerUnits,
                                                      int defenderUnits) const {
    float baseSpeed = 100.0f / point.captureTimeRequired;
    float attackerSpeed = baseSpeed * (1.0f + attackerUnits * m_config.captureSpeedPerUnit);

    if (defenderUnits > 0) {
        float defenseRatio = static_cast<float>(defenderUnits) * point.defendBonus / attackerUnits;
        attackerSpeed *= (1.0f - std::min(0.9f, defenseRatio));
    }

    return std::clamp(attackerSpeed, 0.0f, baseSpeed * m_config.maxCaptureSpeed);
}

} // namespace Vehement
