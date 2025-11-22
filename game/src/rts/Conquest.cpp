#include "Conquest.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <ctime>
#include <random>

namespace Vehement {
namespace RTS {

// ============================================================================
// ConquestReward Implementation
// ============================================================================

int ConquestReward::GetTotalValue() const {
    int value = 0;

    // Resource values (rough estimates)
    for (const auto& [type, amount] : resources) {
        int multiplier = 1;
        if (type == ResourceType::Metal) multiplier = 3;
        else if (type == ResourceType::Coins) multiplier = 5;
        else if (type == ResourceType::Fuel) multiplier = 2;
        value += amount * multiplier;
    }

    // Tech value
    value += static_cast<int>(techs.size()) * 100;

    // Workers
    value += workers * 50;

    // Territory
    value += static_cast<int>(territoryGained * 200);

    // Experience
    value += experienceGained;

    // Fame
    value += fameGained * 10;

    return value;
}

bool ConquestReward::IsEmpty() const {
    return resources.empty() && techs.empty() && workers == 0 &&
           territoryGained == 0.0f && buildingsDestroyed == 0 &&
           buildingsCaptured == 0 && experienceGained == 0;
}

std::string ConquestReward::GetSummaryMessage() const {
    std::stringstream ss;
    ss << "Conquest Rewards:\n";

    if (!resources.empty()) {
        ss << "Resources: ";
        bool first = true;
        for (const auto& [type, amount] : resources) {
            if (!first) ss << ", ";
            ss << amount << " " << GetResourceName(type);
            first = false;
        }
        ss << "\n";
    }

    if (!techs.empty()) {
        ss << "Technologies stolen: " << techs.size() << "\n";
    }

    if (workers > 0) {
        ss << "Workers captured: " << workers << "\n";
    }

    if (territoryGained > 0.0f) {
        ss << "Territory gained: " << static_cast<int>(territoryGained * 100) << "%\n";
    }

    if (buildingsDestroyed > 0) {
        ss << "Buildings destroyed: " << buildingsDestroyed << "\n";
    }

    if (buildingsCaptured > 0) {
        ss << "Buildings captured: " << buildingsCaptured << "\n";
    }

    if (experienceGained > 0) {
        ss << "Experience: +" << experienceGained << "\n";
    }

    if (fameGained > 0) {
        ss << "Fame: +" << fameGained << "\n";
    }

    return ss.str();
}

nlohmann::json ConquestReward::ToJson() const {
    nlohmann::json j;

    nlohmann::json resourcesJson;
    for (const auto& [type, amount] : resources) {
        resourcesJson[std::to_string(static_cast<int>(type))] = amount;
    }
    j["resources"] = resourcesJson;
    j["techs"] = techs;
    j["workers"] = workers;
    j["territoryGained"] = territoryGained;

    nlohmann::json tilesJson = nlohmann::json::array();
    for (const auto& tile : capturedTiles) {
        tilesJson.push_back({tile.x, tile.y});
    }
    j["capturedTiles"] = tilesJson;

    j["buildingsDestroyed"] = buildingsDestroyed;
    j["buildingsCaptured"] = buildingsCaptured;
    j["capturedBuildingIds"] = capturedBuildingIds;
    j["experienceGained"] = experienceGained;
    j["fameGained"] = fameGained;
    j["specialItems"] = specialItems;

    return j;
}

ConquestReward ConquestReward::FromJson(const nlohmann::json& j) {
    ConquestReward r;

    if (j.contains("resources")) {
        for (auto& [key, value] : j["resources"].items()) {
            ResourceType type = static_cast<ResourceType>(std::stoi(key));
            r.resources[type] = value.get<int>();
        }
    }

    r.techs = j.value("techs", std::vector<std::string>{});
    r.workers = j.value("workers", 0);
    r.territoryGained = j.value("territoryGained", 0.0f);

    if (j.contains("capturedTiles")) {
        for (const auto& tileJson : j["capturedTiles"]) {
            r.capturedTiles.emplace_back(tileJson[0].get<int>(), tileJson[1].get<int>());
        }
    }

    r.buildingsDestroyed = j.value("buildingsDestroyed", 0);
    r.buildingsCaptured = j.value("buildingsCaptured", 0);
    r.capturedBuildingIds = j.value("capturedBuildingIds", std::vector<std::string>{});
    r.experienceGained = j.value("experienceGained", 0);
    r.fameGained = j.value("fameGained", 0);
    r.specialItems = j.value("specialItems", std::vector<std::string>{});

    return r;
}

// ============================================================================
// ConquestLoss Implementation
// ============================================================================

int ConquestLoss::GetTotalLossValue() const {
    int value = 0;

    for (const auto& [type, amount] : resourcesLost) {
        value += amount;
    }

    value += static_cast<int>(techsLost.size()) * 100;
    value += workersLost * 50;
    value += static_cast<int>(territoryLost * 200);
    value += buildingsDestroyed * 75;
    value += unitsLost * 25;

    if (heroKilled) value += 200;

    return value;
}

std::string ConquestLoss::GetSummaryMessage() const {
    std::stringstream ss;
    ss << "Conquest Losses:\n";

    if (!resourcesLost.empty()) {
        ss << "Resources lost: ";
        bool first = true;
        for (const auto& [type, amount] : resourcesLost) {
            if (!first) ss << ", ";
            ss << amount << " " << GetResourceName(type);
            first = false;
        }
        ss << "\n";
    }

    if (!techsLost.empty()) {
        ss << "Technologies lost: " << techsLost.size() << "\n";
    }

    if (workersLost > 0) {
        ss << "Workers lost: " << workersLost << "\n";
    }

    if (territoryLost > 0.0f) {
        ss << "Territory lost: " << static_cast<int>(territoryLost * 100) << "%\n";
    }

    if (buildingsDestroyed > 0) {
        ss << "Buildings destroyed: " << buildingsDestroyed << "\n";
    }

    if (previousAge != newAge) {
        ss << "Age regressed: " << AgeToString(previousAge) << " -> " << AgeToString(newAge) << "\n";
    }

    if (heroKilled) {
        ss << "Hero was killed!\n";
    }

    return ss.str();
}

nlohmann::json ConquestLoss::ToJson() const {
    nlohmann::json j;

    nlohmann::json resourcesJson;
    for (const auto& [type, amount] : resourcesLost) {
        resourcesJson[std::to_string(static_cast<int>(type))] = amount;
    }
    j["resourcesLost"] = resourcesJson;
    j["techsLost"] = techsLost;
    j["workersLost"] = workersLost;
    j["territoryLost"] = territoryLost;
    j["buildingsDestroyed"] = buildingsDestroyed;
    j["unitsLost"] = unitsLost;
    j["previousAge"] = static_cast<int>(previousAge);
    j["newAge"] = static_cast<int>(newAge);
    j["heroKilled"] = heroKilled;

    return j;
}

ConquestLoss ConquestLoss::FromJson(const nlohmann::json& j) {
    ConquestLoss l;

    if (j.contains("resourcesLost")) {
        for (auto& [key, value] : j["resourcesLost"].items()) {
            ResourceType type = static_cast<ResourceType>(std::stoi(key));
            l.resourcesLost[type] = value.get<int>();
        }
    }

    l.techsLost = j.value("techsLost", std::vector<std::string>{});
    l.workersLost = j.value("workersLost", 0);
    l.territoryLost = j.value("territoryLost", 0.0f);
    l.buildingsDestroyed = j.value("buildingsDestroyed", 0);
    l.unitsLost = j.value("unitsLost", 0);
    l.previousAge = static_cast<Age>(j.value("previousAge", 0));
    l.newAge = static_cast<Age>(j.value("newAge", 0));
    l.heroKilled = j.value("heroKilled", false);

    return l;
}

// ============================================================================
// CombatStats Implementation
// ============================================================================

nlohmann::json CombatStats::ToJson() const {
    return {
        {"attackerUnits", attackerUnits},
        {"attackerUnitsLost", attackerUnitsLost},
        {"attackerDamageDealt", attackerDamageDealt},
        {"attackerDamageTaken", attackerDamageTaken},
        {"defenderUnits", defenderUnits},
        {"defenderUnitsLost", defenderUnitsLost},
        {"defenderDamageDealt", defenderDamageDealt},
        {"defenderDamageTaken", defenderDamageTaken},
        {"buildingDamageDealt", buildingDamageDealt},
        {"combatDurationSeconds", combatDurationSeconds}
    };
}

CombatStats CombatStats::FromJson(const nlohmann::json& j) {
    CombatStats s;
    s.attackerUnits = j.value("attackerUnits", 0);
    s.attackerUnitsLost = j.value("attackerUnitsLost", 0);
    s.attackerDamageDealt = j.value("attackerDamageDealt", 0.0f);
    s.attackerDamageTaken = j.value("attackerDamageTaken", 0.0f);
    s.defenderUnits = j.value("defenderUnits", 0);
    s.defenderUnitsLost = j.value("defenderUnitsLost", 0);
    s.defenderDamageDealt = j.value("defenderDamageDealt", 0.0f);
    s.defenderDamageTaken = j.value("defenderDamageTaken", 0.0f);
    s.buildingDamageDealt = j.value("buildingDamageDealt", 0.0f);
    s.combatDurationSeconds = j.value("combatDurationSeconds", 0.0f);
    return s;
}

// ============================================================================
// ConquestInstance Implementation
// ============================================================================

bool ConquestInstance::IsActive() const {
    return state == ConquestState::Preparing || state == ConquestState::InProgress;
}

bool ConquestInstance::IsCompleted() const {
    return state == ConquestState::Successful ||
           state == ConquestState::Failed ||
           state == ConquestState::Retreated ||
           state == ConquestState::Cancelled ||
           state == ConquestState::Timeout;
}

float ConquestInstance::GetTimeUntilStart() const {
    if (state != ConquestState::Preparing) return 0.0f;

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t currentTime = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    int64_t startTime = initiatedTimestamp + static_cast<int64_t>(preparationTimeSeconds);

    return std::max(0.0f, static_cast<float>(startTime - currentTime));
}

float ConquestInstance::GetRemainingTime() const {
    if (state != ConquestState::InProgress) return 0.0f;

    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t currentTime = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    int64_t endTime = startedTimestamp + static_cast<int64_t>(maxDurationSeconds);

    return std::max(0.0f, static_cast<float>(endTime - currentTime));
}

nlohmann::json ConquestInstance::ToJson() const {
    return {
        {"id", id},
        {"attackerId", attackerId},
        {"defenderId", defenderId},
        {"type", static_cast<int>(type)},
        {"state", static_cast<int>(state)},
        {"initiatedTimestamp", initiatedTimestamp},
        {"startedTimestamp", startedTimestamp},
        {"completedTimestamp", completedTimestamp},
        {"preparationTimeSeconds", preparationTimeSeconds},
        {"maxDurationSeconds", maxDurationSeconds},
        {"conquestProgress", conquestProgress},
        {"defenseStrength", defenseStrength},
        {"attackerUnitIds", attackerUnitIds},
        {"defenderUnitIds", defenderUnitIds},
        {"targetPosition", {targetPosition.x, targetPosition.y}},
        {"attackRadius", attackRadius},
        {"attackerReward", attackerReward.ToJson()},
        {"defenderLoss", defenderLoss.ToJson()},
        {"combatStats", combatStats.ToJson()},
        {"defenderOnline", defenderOnline},
        {"wasContested", wasContested}
    };
}

ConquestInstance ConquestInstance::FromJson(const nlohmann::json& j) {
    ConquestInstance c;
    c.id = j.value("id", "");
    c.attackerId = j.value("attackerId", "");
    c.defenderId = j.value("defenderId", "");
    c.type = static_cast<ConquestType>(j.value("type", 0));
    c.state = static_cast<ConquestState>(j.value("state", 0));
    c.initiatedTimestamp = j.value("initiatedTimestamp", 0LL);
    c.startedTimestamp = j.value("startedTimestamp", 0LL);
    c.completedTimestamp = j.value("completedTimestamp", 0LL);
    c.preparationTimeSeconds = j.value("preparationTimeSeconds", 300.0f);
    c.maxDurationSeconds = j.value("maxDurationSeconds", 600.0f);
    c.conquestProgress = j.value("conquestProgress", 0.0f);
    c.defenseStrength = j.value("defenseStrength", 100.0f);
    c.attackerUnitIds = j.value("attackerUnitIds", std::vector<std::string>{});
    c.defenderUnitIds = j.value("defenderUnitIds", std::vector<std::string>{});

    if (j.contains("targetPosition") && j["targetPosition"].is_array()) {
        c.targetPosition.x = j["targetPosition"][0].get<int>();
        c.targetPosition.y = j["targetPosition"][1].get<int>();
    }

    c.attackRadius = j.value("attackRadius", 10.0f);

    if (j.contains("attackerReward")) {
        c.attackerReward = ConquestReward::FromJson(j["attackerReward"]);
    }
    if (j.contains("defenderLoss")) {
        c.defenderLoss = ConquestLoss::FromJson(j["defenderLoss"]);
    }
    if (j.contains("combatStats")) {
        c.combatStats = CombatStats::FromJson(j["combatStats"]);
    }

    c.defenderOnline = j.value("defenderOnline", false);
    c.wasContested = j.value("wasContested", false);

    return c;
}

// ============================================================================
// ConquestConfig Implementation
// ============================================================================

nlohmann::json ConquestConfig::ToJson() const {
    return {
        {"raidPreparationTime", raidPreparationTime},
        {"siegePreparationTime", siegePreparationTime},
        {"conquestPreparationTime", conquestPreparationTime},
        {"maxRaidDuration", maxRaidDuration},
        {"maxSiegeDuration", maxSiegeDuration},
        {"maxConquestDuration", maxConquestDuration},
        {"attackCooldownHours", attackCooldownHours},
        {"defenseCooldownHours", defenseCooldownHours},
        {"globalAttackCooldownMinutes", globalAttackCooldownMinutes},
        {"resourceLootPercent", resourceLootPercent},
        {"techStealChance", techStealChance},
        {"workerCapturePercent", workerCapturePercent},
        {"territoryGainPercent", territoryGainPercent},
        {"maxActiveConquests", maxActiveConquests},
        {"maxDailyAttacks", maxDailyAttacks},
        {"minAgeDifference", minAgeDifference},
        {"maxAgeDifference", maxAgeDifference},
        {"offlineDefenseBonus", offlineDefenseBonus},
        {"fortificationBonus", fortificationBonus},
        {"homeTerritoryBonus", homeTerritoryBonus},
        {"surpriseAttackBonus", surpriseAttackBonus},
        {"newPlayerProtectionHours", newPlayerProtectionHours},
        {"afterDefeatProtectionHours", afterDefeatProtectionHours}
    };
}

ConquestConfig ConquestConfig::FromJson(const nlohmann::json& j) {
    ConquestConfig c;
    c.raidPreparationTime = j.value("raidPreparationTime", 60.0f);
    c.siegePreparationTime = j.value("siegePreparationTime", 300.0f);
    c.conquestPreparationTime = j.value("conquestPreparationTime", 600.0f);
    c.maxRaidDuration = j.value("maxRaidDuration", 180.0f);
    c.maxSiegeDuration = j.value("maxSiegeDuration", 600.0f);
    c.maxConquestDuration = j.value("maxConquestDuration", 1200.0f);
    c.attackCooldownHours = j.value("attackCooldownHours", 1.0f);
    c.defenseCooldownHours = j.value("defenseCooldownHours", 2.0f);
    c.globalAttackCooldownMinutes = j.value("globalAttackCooldownMinutes", 10.0f);
    c.resourceLootPercent = j.value("resourceLootPercent", 0.3f);
    c.techStealChance = j.value("techStealChance", 0.25f);
    c.workerCapturePercent = j.value("workerCapturePercent", 0.1f);
    c.territoryGainPercent = j.value("territoryGainPercent", 0.15f);
    c.maxActiveConquests = j.value("maxActiveConquests", 1);
    c.maxDailyAttacks = j.value("maxDailyAttacks", 5);
    c.minAgeDifference = j.value("minAgeDifference", -2);
    c.maxAgeDifference = j.value("maxAgeDifference", 2);
    c.offlineDefenseBonus = j.value("offlineDefenseBonus", 0.5f);
    c.fortificationBonus = j.value("fortificationBonus", 1.5f);
    c.homeTerritoryBonus = j.value("homeTerritoryBonus", 1.3f);
    c.surpriseAttackBonus = j.value("surpriseAttackBonus", 1.2f);
    c.newPlayerProtectionHours = j.value("newPlayerProtectionHours", 24.0f);
    c.afterDefeatProtectionHours = j.value("afterDefeatProtectionHours", 4.0f);
    return c;
}

// ============================================================================
// ConquestManager Implementation
// ============================================================================

ConquestManager& ConquestManager::Instance() {
    static ConquestManager instance;
    return instance;
}

bool ConquestManager::Initialize(const std::string& localPlayerId,
                                  const ConquestConfig& config) {
    if (m_initialized) {
        Shutdown();
    }

    m_localPlayerId = localPlayerId;
    m_config = config;

    // Reset daily attacks if new day
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    int today = now_tm->tm_yday;

    if (today != m_dailyAttacksDay) {
        m_dailyAttacksUsed = 0;
        m_dailyAttacksDay = today;
    }

    m_initialized = true;
    return true;
}

void ConquestManager::Shutdown() {
    if (m_firebaseSyncEnabled) {
        DisableFirebaseSync();
    }

    m_activeConquests.clear();
    m_protectionExpiry.clear();
    m_targetCooldowns.clear();
    m_conquestHistory.clear();

    m_initialized = false;
}

void ConquestManager::Update(float deltaTime) {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_conquestMutex);

    std::vector<std::string> completedConquests;

    for (auto& [id, conquest] : m_activeConquests) {
        if (conquest.state == ConquestState::Preparing) {
            // Check if preparation time is over
            if (conquest.GetTimeUntilStart() <= 0.0f) {
                conquest.state = ConquestState::InProgress;
                conquest.startedTimestamp = GetCurrentTimestamp();

                if (m_onConquestStart) {
                    m_onConquestStart(conquest);
                }
            }
        }
        else if (conquest.state == ConquestState::InProgress) {
            // Simulate battle
            SimulateBattle(conquest, deltaTime);

            // Check for completion
            if (conquest.conquestProgress >= 100.0f) {
                conquest.state = ConquestState::Successful;
                conquest.completedTimestamp = GetCurrentTimestamp();
                completedConquests.push_back(id);
            }
            else if (conquest.defenseStrength <= 0.0f) {
                conquest.state = ConquestState::Successful;
                conquest.completedTimestamp = GetCurrentTimestamp();
                completedConquests.push_back(id);
            }
            else if (conquest.GetRemainingTime() <= 0.0f) {
                // Time's up - defender wins
                conquest.state = ConquestState::Failed;
                conquest.completedTimestamp = GetCurrentTimestamp();
                completedConquests.push_back(id);
            }

            if (m_onConquestUpdate) {
                m_onConquestUpdate(conquest);
            }
        }
    }

    // Finalize completed conquests
    for (const auto& id : completedConquests) {
        FinalizeConquest(m_activeConquests[id]);
    }
}

bool ConquestManager::CanAttack(const std::string& targetId) const {
    return GetAttackBlockedReason(targetId).empty();
}

std::string ConquestManager::GetAttackBlockedReason(const std::string& targetId) const {
    if (!m_initialized) {
        return "Conquest system not initialized";
    }

    if (targetId == m_localPlayerId) {
        return "Cannot attack yourself";
    }

    // Check global cooldown
    if (!IsOffAttackCooldown()) {
        return "Attack on cooldown";
    }

    // Check daily limit
    if (GetRemainingDailyAttacks() <= 0) {
        return "Daily attack limit reached";
    }

    // Check target-specific cooldown
    if (!CanAttackTarget(targetId)) {
        return "Cannot attack this target yet";
    }

    // Check if target has protection
    if (HasProtection(targetId)) {
        return "Target is under protection";
    }

    // Check max active conquests
    {
        std::lock_guard<std::mutex> lock(m_conquestMutex);
        int activeCount = 0;
        for (const auto& [id, conquest] : m_activeConquests) {
            if (conquest.attackerId == m_localPlayerId && conquest.IsActive()) {
                activeCount++;
            }
        }
        if (activeCount >= m_config.maxActiveConquests) {
            return "Maximum active conquests reached";
        }
    }

    return "";  // No blocking reason
}

std::string ConquestManager::InitiateConquest(const std::string& attackerId,
                                               const std::string& defenderId,
                                               ConquestType type) {
    if (!CanAttack(defenderId)) {
        return "";
    }

    ConquestInstance conquest;
    conquest.id = GenerateConquestId();
    conquest.attackerId = attackerId;
    conquest.defenderId = defenderId;
    conquest.type = type;
    conquest.state = ConquestState::Preparing;
    conquest.initiatedTimestamp = GetCurrentTimestamp();
    conquest.preparationTimeSeconds = CalculatePrepTime(type);
    conquest.maxDurationSeconds = CalculateMaxDuration(type);
    conquest.defenseStrength = CalculateDefenseStrength(defenderId);

    // Initialize combat stats
    conquest.combatStats.attackerUnits = static_cast<int>(conquest.attackerUnitIds.size());

    {
        std::lock_guard<std::mutex> lock(m_conquestMutex);
        m_activeConquests[conquest.id] = conquest;
    }

    // Update cooldowns
    m_lastAttackTimestamp = GetCurrentTimestamp();
    m_dailyAttacksUsed++;

    // Notify defender
    if (m_onUnderAttack && defenderId == m_localPlayerId) {
        m_onUnderAttack(conquest);
    }

    // Sync to Firebase
    if (m_firebaseSyncEnabled) {
        SaveToFirebase();
    }

    return conquest.id;
}

bool ConquestManager::CancelConquest(const std::string& conquestId) {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    auto it = m_activeConquests.find(conquestId);
    if (it == m_activeConquests.end()) return false;

    if (it->second.state != ConquestState::Preparing) {
        return false;  // Can only cancel during preparation
    }

    it->second.state = ConquestState::Cancelled;
    it->second.completedTimestamp = GetCurrentTimestamp();

    // Move to history
    m_conquestHistory.push_back(it->second);
    m_activeConquests.erase(it);

    return true;
}

bool ConquestManager::Retreat(const std::string& conquestId) {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    auto it = m_activeConquests.find(conquestId);
    if (it == m_activeConquests.end()) return false;

    if (it->second.state != ConquestState::InProgress) {
        return false;  // Can only retreat during battle
    }

    if (it->second.attackerId != m_localPlayerId) {
        return false;  // Can only retreat from own attacks
    }

    it->second.state = ConquestState::Retreated;
    it->second.completedTimestamp = GetCurrentTimestamp();

    // Partial consequences for retreating
    // (attacker loses some units but defender doesn't get full victory bonus)

    FinalizeConquest(it->second);

    return true;
}

const ConquestInstance* ConquestManager::GetConquest(const std::string& conquestId) const {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    auto it = m_activeConquests.find(conquestId);
    return (it != m_activeConquests.end()) ? &it->second : nullptr;
}

std::vector<const ConquestInstance*> ConquestManager::GetActiveConquests() const {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    std::vector<const ConquestInstance*> result;
    for (const auto& [id, conquest] : m_activeConquests) {
        if (conquest.IsActive()) {
            result.push_back(&conquest);
        }
    }
    return result;
}

std::vector<const ConquestInstance*> ConquestManager::GetMyAttacks() const {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    std::vector<const ConquestInstance*> result;
    for (const auto& [id, conquest] : m_activeConquests) {
        if (conquest.attackerId == m_localPlayerId && conquest.IsActive()) {
            result.push_back(&conquest);
        }
    }
    return result;
}

std::vector<const ConquestInstance*> ConquestManager::GetMyDefenses() const {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    std::vector<const ConquestInstance*> result;
    for (const auto& [id, conquest] : m_activeConquests) {
        if (conquest.defenderId == m_localPlayerId && conquest.IsActive()) {
            result.push_back(&conquest);
        }
    }
    return result;
}

ConquestInstance ConquestManager::CompleteConquest(const std::string& conquestId) {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    auto it = m_activeConquests.find(conquestId);
    if (it == m_activeConquests.end()) {
        return ConquestInstance{};
    }

    FinalizeConquest(it->second);
    return it->second;
}

ConquestReward ConquestManager::CalculateReward(const ConquestInstance& conquest,
                                                 const TechTree& defenderTech) const {
    ConquestReward reward;

    // Calculate based on conquest type
    float lootMultiplier = 1.0f;
    switch (conquest.type) {
        case ConquestType::Raid:
            lootMultiplier = 0.5f;
            break;
        case ConquestType::Siege:
            lootMultiplier = 0.8f;
            break;
        case ConquestType::Conquest:
            lootMultiplier = 1.0f;
            break;
        case ConquestType::Assassination:
            lootMultiplier = 0.2f;
            break;
        case ConquestType::Sabotage:
            lootMultiplier = 0.3f;
            break;
    }

    // Resource loot (placeholder - would get from defender's actual resources)
    reward.resources[ResourceType::Food] = static_cast<int>(100 * m_config.resourceLootPercent * lootMultiplier);
    reward.resources[ResourceType::Wood] = static_cast<int>(80 * m_config.resourceLootPercent * lootMultiplier);
    reward.resources[ResourceType::Stone] = static_cast<int>(60 * m_config.resourceLootPercent * lootMultiplier);
    reward.resources[ResourceType::Metal] = static_cast<int>(40 * m_config.resourceLootPercent * lootMultiplier);

    // Tech stealing chance
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (const auto& techId : defenderTech.GetResearchedTechs()) {
        const TechNode* node = defenderTech.GetTech(techId);
        if (node && node->canBeLost) {
            if (dist(rng) < m_config.techStealChance * lootMultiplier) {
                reward.techs.push_back(techId);
            }
        }
    }

    // Workers captured
    reward.workers = static_cast<int>(5 * m_config.workerCapturePercent * lootMultiplier);

    // Territory (only for conquest type)
    if (conquest.type == ConquestType::Conquest) {
        reward.territoryGained = m_config.territoryGainPercent;
    }

    // Experience and fame
    reward.experienceGained = static_cast<int>(50 * lootMultiplier);
    reward.fameGained = static_cast<int>(10 * lootMultiplier);

    return reward;
}

ConquestLoss ConquestManager::CalculateLoss(const ConquestInstance& conquest,
                                             const TechTree& defenderTech) const {
    ConquestLoss loss;

    loss.resourcesLost = conquest.attackerReward.resources;
    loss.techsLost = conquest.attackerReward.techs;
    loss.workersLost = conquest.attackerReward.workers;
    loss.territoryLost = conquest.attackerReward.territoryGained;
    loss.buildingsDestroyed = conquest.attackerReward.buildingsDestroyed;
    loss.previousAge = defenderTech.GetCurrentAge();
    loss.newAge = loss.previousAge;  // Will be calculated after tech loss

    if (conquest.type == ConquestType::Assassination ||
        conquest.type == ConquestType::Conquest) {
        loss.heroKilled = true;
    }

    return loss;
}

void ConquestManager::ApplyConquestResults(TechTree& attackerTech, TechTree& defenderTech,
                                            const ConquestInstance& conquest,
                                            TechLoss& techLoss) {
    if (conquest.state != ConquestState::Successful) return;

    // Determine death type based on conquest
    DeathType deathType = DeathType::HeroDeath;
    switch (conquest.type) {
        case ConquestType::Raid:
            deathType = DeathType::HeroDeath;
            break;
        case ConquestType::Siege:
            deathType = DeathType::BaseDestroyed;
            break;
        case ConquestType::Conquest:
            deathType = DeathType::TotalDefeat;
            break;
        case ConquestType::Assassination:
            deathType = DeathType::Assassination;
            break;
        default:
            deathType = DeathType::HeroDeath;
    }

    // Apply tech loss to defender
    TechLossResult lossResult = techLoss.OnBaseConquered(
        defenderTech, attackerTech,
        conquest.defenderId, conquest.attackerId);

    // Grant protection to defender
    GrantProtection(conquest.defenderId, m_config.afterDefeatProtectionHours);
}

bool ConquestManager::HasProtection(const std::string& playerId) const {
    auto it = m_protectionExpiry.find(playerId);
    if (it == m_protectionExpiry.end()) return false;

    return GetCurrentTimestamp() < it->second;
}

float ConquestManager::GetProtectionTimeRemaining(const std::string& playerId) const {
    auto it = m_protectionExpiry.find(playerId);
    if (it == m_protectionExpiry.end()) return 0.0f;

    int64_t remaining = it->second - GetCurrentTimestamp();
    return std::max(0.0f, static_cast<float>(remaining) / 3600.0f);
}

void ConquestManager::GrantProtection(const std::string& playerId, float durationHours) {
    int64_t expiryTime = GetCurrentTimestamp() + static_cast<int64_t>(durationHours * 3600);
    m_protectionExpiry[playerId] = expiryTime;
}

float ConquestManager::CalculateDefenseStrength(const std::string& playerId) const {
    float strength = 100.0f;  // Base defense

    // Add fortification bonus (placeholder - would query actual buildings)
    strength += 20.0f * m_config.fortificationBonus;

    // Offline bonus
    // (would check if player is actually online)
    bool isOffline = false;
    if (isOffline) {
        strength *= (1.0f + m_config.offlineDefenseBonus);
    }

    // Home territory bonus
    strength *= m_config.homeTerritoryBonus;

    return strength;
}

void ConquestManager::CommitDefense(const std::string& conquestId,
                                     const std::vector<std::string>& defenseUnits) {
    std::lock_guard<std::mutex> lock(m_conquestMutex);

    auto it = m_activeConquests.find(conquestId);
    if (it == m_activeConquests.end()) return;

    it->second.defenderUnitIds = defenseUnits;
    it->second.wasContested = true;
    it->second.combatStats.defenderUnits = static_cast<int>(defenseUnits.size());
}

bool ConquestManager::IsOffAttackCooldown() const {
    int64_t cooldownSeconds = static_cast<int64_t>(m_config.globalAttackCooldownMinutes * 60);
    return (GetCurrentTimestamp() - m_lastAttackTimestamp) >= cooldownSeconds;
}

float ConquestManager::GetAttackCooldownRemaining() const {
    int64_t cooldownSeconds = static_cast<int64_t>(m_config.globalAttackCooldownMinutes * 60);
    int64_t elapsed = GetCurrentTimestamp() - m_lastAttackTimestamp;
    int64_t remaining = cooldownSeconds - elapsed;
    return std::max(0.0f, static_cast<float>(remaining) / 60.0f);  // Return minutes
}

int ConquestManager::GetRemainingDailyAttacks() const {
    return std::max(0, m_config.maxDailyAttacks - m_dailyAttacksUsed);
}

bool ConquestManager::CanAttackTarget(const std::string& targetId) const {
    auto it = m_targetCooldowns.find(targetId);
    if (it == m_targetCooldowns.end()) return true;

    int64_t cooldownSeconds = static_cast<int64_t>(m_config.attackCooldownHours * 3600);
    return (GetCurrentTimestamp() - it->second) >= cooldownSeconds;
}

std::vector<ConquestInstance> ConquestManager::GetConquestHistory(
    const std::string& playerId, int limit) const {

    std::vector<ConquestInstance> result;

    for (auto it = m_conquestHistory.rbegin();
         it != m_conquestHistory.rend() && static_cast<int>(result.size()) < limit;
         ++it) {
        if (it->attackerId == playerId || it->defenderId == playerId) {
            result.push_back(*it);
        }
    }

    return result;
}

ConquestManager::AttackStats ConquestManager::GetAttackStats(const std::string& playerId) const {
    auto it = m_attackStats.find(playerId);
    return (it != m_attackStats.end()) ? it->second : AttackStats{};
}

ConquestManager::DefenseStats ConquestManager::GetDefenseStats(const std::string& playerId) const {
    auto it = m_defenseStats.find(playerId);
    return (it != m_defenseStats.end()) ? it->second : DefenseStats{};
}

void ConquestManager::SaveToFirebase() {
    if (m_localPlayerId.empty()) return;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    nlohmann::json j;
    j["activeConquests"] = nlohmann::json::object();
    for (const auto& [id, conquest] : m_activeConquests) {
        j["activeConquests"][id] = conquest.ToJson();
    }

    j["protectionExpiry"] = m_protectionExpiry;
    j["lastAttackTimestamp"] = m_lastAttackTimestamp;
    j["dailyAttacksUsed"] = m_dailyAttacksUsed;
    j["dailyAttacksDay"] = m_dailyAttacksDay;

    firebase.SetValue(GetFirebasePath(), j);
}

void ConquestManager::LoadFromFirebase(std::function<void(bool success)> callback) {
    if (m_localPlayerId.empty()) {
        if (callback) callback(false);
        return;
    }

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) {
        if (callback) callback(false);
        return;
    }

    firebase.GetValue(GetFirebasePath(), [this, callback](const nlohmann::json& data) {
        if (!data.is_null()) {
            std::lock_guard<std::mutex> lock(m_conquestMutex);

            m_activeConquests.clear();
            if (data.contains("activeConquests")) {
                for (const auto& [id, conquestJson] : data["activeConquests"].items()) {
                    m_activeConquests[id] = ConquestInstance::FromJson(conquestJson);
                }
            }

            if (data.contains("protectionExpiry")) {
                m_protectionExpiry = data["protectionExpiry"].get<std::unordered_map<std::string, int64_t>>();
            }

            m_lastAttackTimestamp = data.value("lastAttackTimestamp", 0LL);
            m_dailyAttacksUsed = data.value("dailyAttacksUsed", 0);
            m_dailyAttacksDay = data.value("dailyAttacksDay", 0);

            if (callback) callback(true);
        } else {
            if (callback) callback(false);
        }
    });
}

void ConquestManager::EnableFirebaseSync() {
    if (m_localPlayerId.empty() || m_firebaseSyncEnabled) return;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) return;

    m_firebaseListenerId = firebase.ListenToPath(GetFirebasePath() + "/activeConquests",
        [this](const nlohmann::json& data) {
            if (!data.is_null()) {
                std::lock_guard<std::mutex> lock(m_conquestMutex);
                m_activeConquests.clear();
                for (const auto& [id, conquestJson] : data.items()) {
                    m_activeConquests[id] = ConquestInstance::FromJson(conquestJson);
                }
            }
        });

    m_firebaseSyncEnabled = true;
}

void ConquestManager::DisableFirebaseSync() {
    if (!m_firebaseSyncEnabled) return;

    auto& firebase = FirebaseManager::Instance();
    if (firebase.IsInitialized() && !m_firebaseListenerId.empty()) {
        firebase.StopListeningById(m_firebaseListenerId);
    }

    m_firebaseListenerId.clear();
    m_firebaseSyncEnabled = false;
}

std::string ConquestManager::GenerateConquestId() {
    std::stringstream ss;
    ss << "conquest_" << m_localPlayerId << "_" << GetCurrentTimestamp() << "_" << m_nextConquestId++;
    return ss.str();
}

float ConquestManager::CalculatePrepTime(ConquestType type) const {
    switch (type) {
        case ConquestType::Raid:            return m_config.raidPreparationTime;
        case ConquestType::Siege:           return m_config.siegePreparationTime;
        case ConquestType::Conquest:        return m_config.conquestPreparationTime;
        case ConquestType::Assassination:   return m_config.raidPreparationTime * 0.5f;
        case ConquestType::Sabotage:        return m_config.raidPreparationTime * 0.75f;
        default:                            return m_config.raidPreparationTime;
    }
}

float ConquestManager::CalculateMaxDuration(ConquestType type) const {
    switch (type) {
        case ConquestType::Raid:            return m_config.maxRaidDuration;
        case ConquestType::Siege:           return m_config.maxSiegeDuration;
        case ConquestType::Conquest:        return m_config.maxConquestDuration;
        case ConquestType::Assassination:   return m_config.maxRaidDuration * 0.5f;
        case ConquestType::Sabotage:        return m_config.maxRaidDuration * 0.75f;
        default:                            return m_config.maxRaidDuration;
    }
}

void ConquestManager::SimulateBattle(ConquestInstance& conquest, float deltaTime) {
    // Simple battle simulation
    float attackPower = static_cast<float>(conquest.attackerUnitIds.size()) * 5.0f;
    float defensePower = conquest.defenseStrength * 0.5f;

    if (conquest.wasContested) {
        defensePower += static_cast<float>(conquest.defenderUnitIds.size()) * 4.0f;
    }

    // Apply bonuses
    if (!conquest.defenderOnline) {
        defensePower *= (1.0f + m_config.offlineDefenseBonus);
    }

    // Calculate damage
    float attackDamage = attackPower * deltaTime * 0.1f;
    float defenseDamage = defensePower * deltaTime * 0.08f;

    // Apply damage
    conquest.defenseStrength = std::max(0.0f, conquest.defenseStrength - attackDamage);
    conquest.conquestProgress = std::min(100.0f, conquest.conquestProgress + attackDamage * 0.5f);

    // Update combat stats
    conquest.combatStats.attackerDamageDealt += attackDamage;
    conquest.combatStats.defenderDamageDealt += defenseDamage;
    conquest.combatStats.attackerDamageTaken += defenseDamage;
    conquest.combatStats.defenderDamageTaken += attackDamage;
    conquest.combatStats.buildingDamageDealt += attackDamage * 0.2f;
    conquest.combatStats.combatDurationSeconds += deltaTime;

    // Calculate unit losses (simplified)
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    if (dist(rng) < defenseDamage * 0.01f && !conquest.attackerUnitIds.empty()) {
        conquest.attackerUnitIds.pop_back();
        conquest.combatStats.attackerUnitsLost++;
    }

    if (dist(rng) < attackDamage * 0.01f && !conquest.defenderUnitIds.empty()) {
        conquest.defenderUnitIds.pop_back();
        conquest.combatStats.defenderUnitsLost++;
    }
}

void ConquestManager::FinalizeConquest(ConquestInstance& conquest) {
    // Move to history
    m_conquestHistory.push_back(conquest);

    // Limit history size
    if (m_conquestHistory.size() > 100) {
        m_conquestHistory.erase(m_conquestHistory.begin(),
                                 m_conquestHistory.begin() + 50);
    }

    // Update statistics
    if (conquest.state == ConquestState::Successful) {
        auto& attackerStats = m_attackStats[conquest.attackerId];
        attackerStats.totalAttacks++;
        attackerStats.successfulAttacks++;
        attackerStats.totalResourcesLooted += conquest.attackerReward.GetTotalValue();
        attackerStats.totalTechsStolen += static_cast<int>(conquest.attackerReward.techs.size());
        attackerStats.totalTerritoryGained += conquest.attackerReward.territoryGained;

        auto& defenderStats = m_defenseStats[conquest.defenderId];
        defenderStats.totalDefenses++;
        defenderStats.failedDefenses++;
        defenderStats.totalResourcesLost += conquest.defenderLoss.GetTotalLossValue();
        defenderStats.totalTechsLost += static_cast<int>(conquest.defenderLoss.techsLost.size());
        defenderStats.totalTerritoryLost += conquest.defenderLoss.territoryLost;
    }
    else if (conquest.state == ConquestState::Failed) {
        auto& attackerStats = m_attackStats[conquest.attackerId];
        attackerStats.totalAttacks++;
        attackerStats.failedAttacks++;

        auto& defenderStats = m_defenseStats[conquest.defenderId];
        defenderStats.totalDefenses++;
        defenderStats.successfulDefenses++;
    }

    // Update cooldowns
    m_targetCooldowns[conquest.defenderId] = GetCurrentTimestamp();

    // Trigger callback
    if (m_onConquestComplete) {
        m_onConquestComplete(conquest);
    }

    // Remove from active
    m_activeConquests.erase(conquest.id);
}

int64_t ConquestManager::GetCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string ConquestManager::GetFirebasePath() const {
    return "players/" + m_localPlayerId + "/conquest";
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string GenerateConquestNotification(
    const ConquestInstance& conquest,
    bool forAttacker) {

    std::stringstream ss;

    if (forAttacker) {
        switch (conquest.state) {
            case ConquestState::Preparing:
                ss << "Preparing " << ConquestTypeToString(conquest.type)
                   << " against enemy base. Battle starts in "
                   << static_cast<int>(conquest.GetTimeUntilStart()) << " seconds.";
                break;
            case ConquestState::InProgress:
                ss << ConquestTypeToString(conquest.type) << " in progress! "
                   << static_cast<int>(conquest.conquestProgress) << "% complete.";
                break;
            case ConquestState::Successful:
                ss << "Victory! " << ConquestTypeToString(conquest.type) << " successful!\n"
                   << conquest.attackerReward.GetSummaryMessage();
                break;
            case ConquestState::Failed:
                ss << ConquestTypeToString(conquest.type) << " failed! "
                   << "The enemy defenses held.";
                break;
            case ConquestState::Retreated:
                ss << "Retreat! Our forces have withdrawn.";
                break;
            default:
                ss << ConquestTypeToString(conquest.type) << " ended.";
        }
    }
    else {
        switch (conquest.state) {
            case ConquestState::Preparing:
                ss << "WARNING: Enemy " << ConquestTypeToString(conquest.type)
                   << " incoming! Attack begins in "
                   << static_cast<int>(conquest.GetTimeUntilStart()) << " seconds!";
                break;
            case ConquestState::InProgress:
                ss << "Under attack! Defenses at "
                   << static_cast<int>(conquest.defenseStrength) << "%.";
                break;
            case ConquestState::Successful:
                ss << "Defeat! Base has been conquered.\n"
                   << conquest.defenderLoss.GetSummaryMessage();
                break;
            case ConquestState::Failed:
                ss << "Victory! Enemy " << ConquestTypeToString(conquest.type)
                   << " repelled!";
                break;
            default:
                ss << "Enemy attack ended.";
        }
    }

    return ss.str();
}

int CalculateRecommendedAttackForce(
    float targetDefenseStrength,
    ConquestType type) {

    float baseUnits = targetDefenseStrength / 10.0f;

    switch (type) {
        case ConquestType::Raid:
            baseUnits *= 0.5f;
            break;
        case ConquestType::Siege:
            baseUnits *= 1.5f;
            break;
        case ConquestType::Conquest:
            baseUnits *= 2.0f;
            break;
        case ConquestType::Assassination:
            baseUnits *= 0.3f;
            break;
        case ConquestType::Sabotage:
            baseUnits *= 0.4f;
            break;
    }

    return std::max(5, static_cast<int>(baseUnits));
}

} // namespace RTS
} // namespace Vehement
