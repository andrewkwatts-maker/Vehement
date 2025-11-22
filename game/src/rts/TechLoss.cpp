#include "TechLoss.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace Vehement {
namespace RTS {

// ============================================================================
// TechLossResult Implementation
// ============================================================================

nlohmann::json TechLossResult::ToJson() const {
    return {
        {"lostTechs", lostTechs},
        {"protectedTechs", protectedTechs},
        {"gainedTechs", gainedTechs},
        {"previousAge", static_cast<int>(previousAge)},
        {"newAge", static_cast<int>(newAge)},
        {"ageRegressed", ageRegressed},
        {"totalTechsAtRisk", totalTechsAtRisk},
        {"effectiveSeverity", effectiveSeverity},
        {"protectionUsed", protectionUsed},
        {"message", message},
        {"deathType", static_cast<int>(deathType)}
    };
}

TechLossResult TechLossResult::FromJson(const nlohmann::json& j) {
    TechLossResult result;
    result.lostTechs = j.value("lostTechs", std::vector<std::string>{});
    result.protectedTechs = j.value("protectedTechs", std::vector<std::string>{});
    result.gainedTechs = j.value("gainedTechs", std::vector<std::string>{});
    result.previousAge = static_cast<Age>(j.value("previousAge", 0));
    result.newAge = static_cast<Age>(j.value("newAge", 0));
    result.ageRegressed = j.value("ageRegressed", false);
    result.totalTechsAtRisk = j.value("totalTechsAtRisk", 0);
    result.effectiveSeverity = j.value("effectiveSeverity", 0.0f);
    result.protectionUsed = j.value("protectionUsed", 0.0f);
    result.message = j.value("message", "");
    result.deathType = static_cast<DeathType>(j.value("deathType", 0));
    return result;
}

// ============================================================================
// TechLossConfig Implementation
// ============================================================================

nlohmann::json TechLossConfig::ToJson() const {
    return {
        {"heroDeathBaseLoss", heroDeathBaseLoss},
        {"baseDestroyedBaseLoss", baseDestroyedBaseLoss},
        {"totalDefeatBaseLoss", totalDefeatBaseLoss},
        {"surrenderBaseLoss", surrenderBaseLoss},
        {"assassinationBaseLoss", assassinationBaseLoss},
        {"keyTechProtectionBonus", keyTechProtectionBonus},
        {"lowAgeTechProtection", lowAgeTechProtection},
        {"consecutiveDeathPenalty", consecutiveDeathPenalty},
        {"cooldownReduction", cooldownReduction},
        {"maxTechsLostPerDeath", maxTechsLostPerDeath},
        {"minTechsRequired", minTechsRequired},
        {"maxAgeLossPerDeath", maxAgeLossPerDeath},
        {"conquestTechGainChance", conquestTechGainChance},
        {"conquestTechGainBonus", conquestTechGainBonus},
        {"maxTechsGainedPerConquest", maxTechsGainedPerConquest},
        {"deathCooldownHours", deathCooldownHours},
        {"protectionAfterDeathHours", protectionAfterDeathHours}
    };
}

TechLossConfig TechLossConfig::FromJson(const nlohmann::json& j) {
    TechLossConfig config;
    config.heroDeathBaseLoss = j.value("heroDeathBaseLoss", 0.15f);
    config.baseDestroyedBaseLoss = j.value("baseDestroyedBaseLoss", 0.35f);
    config.totalDefeatBaseLoss = j.value("totalDefeatBaseLoss", 0.60f);
    config.surrenderBaseLoss = j.value("surrenderBaseLoss", 0.08f);
    config.assassinationBaseLoss = j.value("assassinationBaseLoss", 0.30f);
    config.keyTechProtectionBonus = j.value("keyTechProtectionBonus", 0.3f);
    config.lowAgeTechProtection = j.value("lowAgeTechProtection", 0.5f);
    config.consecutiveDeathPenalty = j.value("consecutiveDeathPenalty", 0.1f);
    config.cooldownReduction = j.value("cooldownReduction", 0.05f);
    config.maxTechsLostPerDeath = j.value("maxTechsLostPerDeath", 5);
    config.minTechsRequired = j.value("minTechsRequired", 2);
    config.maxAgeLossPerDeath = j.value("maxAgeLossPerDeath", 1.0f);
    config.conquestTechGainChance = j.value("conquestTechGainChance", 0.5f);
    config.conquestTechGainBonus = j.value("conquestTechGainBonus", 0.2f);
    config.maxTechsGainedPerConquest = j.value("maxTechsGainedPerConquest", 3);
    config.deathCooldownHours = j.value("deathCooldownHours", 1.0f);
    config.protectionAfterDeathHours = j.value("protectionAfterDeathHours", 0.5f);
    return config;
}

// ============================================================================
// TechLoss Implementation
// ============================================================================

TechLoss::TechLoss()
    : m_rng(std::random_device{}()) {
}

TechLoss::~TechLoss() {
    Shutdown();
}

bool TechLoss::Initialize(const TechLossConfig& config) {
    m_config = config;
    m_initialized = true;
    return true;
}

void TechLoss::Shutdown() {
    m_deathHistory.clear();
    m_temporaryProtection.clear();
    m_totalLossesByPlayer.clear();
    m_totalGainsByPlayer.clear();
    m_lossCountByTech.clear();
    m_initialized = false;
}

float TechLoss::GetBaseSeverityForDeathType(DeathType type) const {
    switch (type) {
        case DeathType::HeroDeath:      return m_config.heroDeathBaseLoss;
        case DeathType::BaseDestroyed:  return m_config.baseDestroyedBaseLoss;
        case DeathType::TotalDefeat:    return m_config.totalDefeatBaseLoss;
        case DeathType::Surrender:      return m_config.surrenderBaseLoss;
        case DeathType::Timeout:        return 0.0f;
        case DeathType::Assassination:  return m_config.assassinationBaseLoss;
        default:                        return 0.0f;
    }
}

TechLossResult TechLoss::OnPlayerDeath(TechTree& playerTech, DeathType type,
                                        const std::string& playerId) {
    TechLossResult result;
    result.deathType = type;
    result.previousAge = playerTech.GetCurrentAge();

    // Calculate effective severity
    float baseSeverity = GetBaseSeverityForDeathType(type);
    result.effectiveSeverity = CalculateEffectiveSeverity(baseSeverity, playerTech, playerId);

    // Get protection level
    result.protectionUsed = GetTechProtectionLevel(playerTech);

    // Check for temporary protection
    if (HasTemporaryProtection(playerId)) {
        result.protectionUsed = std::min(1.0f, result.protectionUsed + 0.5f);
    }

    // Calculate techs at risk
    const auto& researchedTechs = playerTech.GetResearchedTechs();
    result.totalTechsAtRisk = static_cast<int>(researchedTechs.size());

    // Calculate which techs are lost
    auto lostTechs = CalculateLostTechs(playerTech, result.effectiveSeverity);

    // Track protected techs
    for (const auto& techId : researchedTechs) {
        if (IsTechProtected(techId, playerTech)) {
            result.protectedTechs.push_back(techId);
        }
    }

    // Apply the loss
    if (!lostTechs.empty()) {
        ApplyTechLoss(playerTech, lostTechs);
        result.lostTechs = std::move(lostTechs);

        // Update statistics
        if (!playerId.empty()) {
            m_totalLossesByPlayer[playerId] += static_cast<int>(result.lostTechs.size());
            for (const auto& techId : result.lostTechs) {
                m_lossCountByTech[techId]++;
            }
        }
    }

    // Check for age regression
    result.newAge = playerTech.GetCurrentAge();
    result.ageRegressed = (result.newAge < result.previousAge);

    // Record death
    if (!playerId.empty()) {
        RecordDeath(playerId, type);
        ApplyTemporaryProtection(playerId, m_config.protectionAfterDeathHours);
    }

    // Generate message
    result.message = GenerateLossMessage(result);

    // Trigger callback
    if (m_onTechLost && !playerId.empty()) {
        m_onTechLost(playerId, result);
    }

    return result;
}

TechLossResult TechLoss::OnBaseConquered(TechTree& defenderTech, TechTree& attackerTech,
                                          const std::string& defenderId,
                                          const std::string& attackerId) {
    // First, process defender's loss (base destroyed severity)
    TechLossResult result = OnPlayerDeath(defenderTech, DeathType::BaseDestroyed, defenderId);

    // Calculate what attacker gains from conquest
    if (!result.lostTechs.empty()) {
        auto gainedTechs = CalculateGainedTechs(defenderTech, attackerTech, result.lostTechs);

        if (!gainedTechs.empty()) {
            ApplyTechGain(attackerTech, gainedTechs);
            result.gainedTechs = std::move(gainedTechs);

            // Update statistics
            if (!attackerId.empty()) {
                m_totalGainsByPlayer[attackerId] += static_cast<int>(result.gainedTechs.size());
            }

            // Trigger callback
            if (m_onTechGained && !attackerId.empty()) {
                m_onTechGained(attackerId, result.gainedTechs);
            }
        }
    }

    return result;
}

TechLossResult TechLoss::OnSurrender(TechTree& playerTech, const std::string& playerId) {
    return OnPlayerDeath(playerTech, DeathType::Surrender, playerId);
}

std::vector<std::string> TechLoss::CalculateLostTechs(
    const TechTree& tech, float severity) const {

    std::vector<std::string> candidates;
    std::vector<std::string> lostTechs;

    float protection = GetTechProtectionLevel(tech);

    // Gather all techs that could be lost
    for (const auto& techId : tech.GetResearchedTechs()) {
        const TechNode* node = tech.GetTech(techId);
        if (!node) continue;

        // Skip permanent techs
        if (!node->canBeLost) continue;

        // Skip protected techs
        if (IsTechProtected(techId, tech)) continue;

        candidates.push_back(techId);
    }

    // Ensure we keep minimum required techs
    int maxLoss = std::max(0, static_cast<int>(candidates.size()) - m_config.minTechsRequired);
    maxLoss = std::min(maxLoss, m_config.maxTechsLostPerDeath);

    if (maxLoss == 0) return lostTechs;

    // Sort candidates by loss priority (higher age techs more likely to be lost)
    std::sort(candidates.begin(), candidates.end(), [&tech](const std::string& a, const std::string& b) {
        const TechNode* nodeA = tech.GetTech(a);
        const TechNode* nodeB = tech.GetTech(b);
        if (!nodeA || !nodeB) return false;

        // Higher tier techs are lost first
        int tierA = static_cast<int>(nodeA->requiredAge) * 10 + nodeA->tier;
        int tierB = static_cast<int>(nodeB->requiredAge) * 10 + nodeB->tier;
        return tierA > tierB;
    });

    // Roll for each candidate
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (const auto& techId : candidates) {
        if (static_cast<int>(lostTechs.size()) >= maxLoss) break;

        const TechNode* node = tech.GetTech(techId);
        if (!node) continue;

        if (ShouldLoseTech(*node, severity, protection)) {
            lostTechs.push_back(techId);
        }
    }

    return lostTechs;
}

std::vector<std::string> TechLoss::CalculateGainedTechs(
    const TechTree& loserTech,
    const TechTree& winnerTech,
    const std::vector<std::string>& lostTechs) const {

    std::vector<std::string> gainedTechs;

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (const auto& techId : lostTechs) {
        if (static_cast<int>(gainedTechs.size()) >= m_config.maxTechsGainedPerConquest) {
            break;
        }

        // Winner already has this tech?
        if (winnerTech.HasTech(techId)) continue;

        // Check if tech is available to winner's culture
        const TechNode* node = loserTech.GetTech(techId);
        if (!node) continue;
        if (!node->IsAvailableTo(winnerTech.GetCulture())) continue;

        // Roll for gain
        float gainChance = m_config.conquestTechGainChance + m_config.conquestTechGainBonus;
        if (dist(m_rng) < gainChance) {
            gainedTechs.push_back(techId);
        }
    }

    return gainedTechs;
}

float TechLoss::CalculateEffectiveSeverity(
    float baseSeverity,
    const TechTree& tech,
    const std::string& playerId) const {

    float severity = baseSeverity;

    // Consecutive death penalty
    if (!playerId.empty()) {
        int recentDeaths = GetRecentDeathCount(playerId, 2.0f);  // Last 2 hours
        severity += recentDeaths * m_config.consecutiveDeathPenalty;
    }

    // Cooldown reduction
    if (!playerId.empty()) {
        float hoursSinceLastDeath = GetHoursSinceLastDeath(playerId);
        float cooldownBonus = std::min(1.0f, hoursSinceLastDeath / m_config.deathCooldownHours);
        severity *= (1.0f - (cooldownBonus * m_config.cooldownReduction));
    }

    // Cap severity
    severity = std::min(1.0f, std::max(0.0f, severity));

    return severity;
}

bool TechLoss::IsTechProtected(const std::string& techId, const TechTree& tech) const {
    const TechNode* node = tech.GetTech(techId);
    if (!node) return true;  // Unknown tech is protected

    // Permanent techs are always protected
    if (!node->canBeLost) return true;

    // Check minimum age protection
    if (static_cast<int>(node->requiredAge) < static_cast<int>(node->minimumAgeLoss)) {
        return true;
    }

    // Stone Age techs get extra protection
    if (node->requiredAge == Age::Stone) {
        return true;  // Never lose Stone Age techs
    }

    return false;
}

float TechLoss::GetTechProtectionLevel(const TechTree& tech) const {
    float protection = tech.GetTechProtectionLevel();

    // Age-based protection bonus
    if (tech.GetCurrentAge() <= Age::Bronze) {
        protection += m_config.lowAgeTechProtection * 0.5f;
    }

    return std::min(1.0f, protection);
}

void TechLoss::ApplyTemporaryProtection(const std::string& playerId, float durationHours) {
    if (playerId.empty()) return;

    ProtectionEntry& entry = m_temporaryProtection[playerId];
    entry.remainingTime = durationHours * 3600.0f;  // Convert to seconds
    entry.initialDuration = entry.remainingTime;
}

bool TechLoss::HasTemporaryProtection(const std::string& playerId) const {
    auto it = m_temporaryProtection.find(playerId);
    return (it != m_temporaryProtection.end() && it->second.remainingTime > 0.0f);
}

float TechLoss::GetProtectionTimeRemaining(const std::string& playerId) const {
    auto it = m_temporaryProtection.find(playerId);
    if (it != m_temporaryProtection.end()) {
        return it->second.remainingTime / 3600.0f;  // Return hours
    }
    return 0.0f;
}

void TechLoss::RecordDeath(const std::string& playerId, DeathType type) {
    if (playerId.empty()) return;

    DeathRecord record;
    record.type = type;
    record.timestamp = GetCurrentTimestamp();
    record.techsLost = 0;  // Will be updated separately

    m_deathHistory[playerId].push_back(record);

    // Limit history size
    auto& history = m_deathHistory[playerId];
    if (history.size() > 100) {
        history.erase(history.begin(), history.begin() + 50);
    }
}

int TechLoss::GetRecentDeathCount(const std::string& playerId, float hoursBack) const {
    auto it = m_deathHistory.find(playerId);
    if (it == m_deathHistory.end()) return 0;

    int64_t cutoffTime = GetCurrentTimestamp() - static_cast<int64_t>(hoursBack * 3600);
    int count = 0;

    for (const auto& record : it->second) {
        if (record.timestamp >= cutoffTime) {
            count++;
        }
    }

    return count;
}

float TechLoss::GetHoursSinceLastDeath(const std::string& playerId) const {
    auto it = m_deathHistory.find(playerId);
    if (it == m_deathHistory.end() || it->second.empty()) {
        return 24.0f;  // Return large value if no death history
    }

    int64_t lastDeathTime = it->second.back().timestamp;
    int64_t currentTime = GetCurrentTimestamp();
    int64_t secondsElapsed = currentTime - lastDeathTime;

    return static_cast<float>(secondsElapsed) / 3600.0f;
}

void TechLoss::ClearDeathHistory(const std::string& playerId) {
    m_deathHistory.erase(playerId);
}

Age TechLoss::CalculateNewAge(const TechTree& tech,
                               const std::vector<std::string>& lostTechs) const {
    Age currentAge = tech.GetCurrentAge();

    // Check if we can still maintain current age without the lost techs
    std::set<std::string> remainingTechs(tech.GetResearchedTechs().begin(),
                                          tech.GetResearchedTechs().end());
    for (const auto& techId : lostTechs) {
        remainingTechs.erase(techId);
    }

    // Check from current age down
    for (int ageIdx = static_cast<int>(currentAge); ageIdx >= 0; ageIdx--) {
        Age checkAge = static_cast<Age>(ageIdx);
        if (CanMaintainAge(tech, checkAge,
            std::set<std::string>(lostTechs.begin(), lostTechs.end()))) {
            return checkAge;
        }
    }

    return Age::Stone;
}

bool TechLoss::CanMaintainAge(const TechTree& tech, Age age,
                               const std::set<std::string>& excludeTechs) const {
    if (age == Age::Stone) return true;  // Can always be Stone Age

    const auto& requirements = tech.GetAgeRequirements(age);

    // Check if all required techs are still available
    for (const auto& reqTech : requirements.requiredTechs) {
        if (excludeTechs.count(reqTech) > 0) {
            return false;  // Required tech is being lost
        }
        if (!tech.HasTech(reqTech)) {
            return false;  // Don't have required tech
        }
    }

    return true;
}

int TechLoss::GetTotalTechLosses(const std::string& playerId) const {
    auto it = m_totalLossesByPlayer.find(playerId);
    return (it != m_totalLossesByPlayer.end()) ? it->second : 0;
}

int TechLoss::GetTotalTechGains(const std::string& playerId) const {
    auto it = m_totalGainsByPlayer.find(playerId);
    return (it != m_totalGainsByPlayer.end()) ? it->second : 0;
}

std::string TechLoss::GetMostLostTech() const {
    if (m_lossCountByTech.empty()) return "";

    auto maxIt = std::max_element(m_lossCountByTech.begin(), m_lossCountByTech.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return maxIt->first;
}

void TechLoss::Update(float deltaTime) {
    // Update temporary protections
    for (auto it = m_temporaryProtection.begin(); it != m_temporaryProtection.end(); ) {
        it->second.remainingTime -= deltaTime;
        if (it->second.remainingTime <= 0.0f) {
            it = m_temporaryProtection.erase(it);
        } else {
            ++it;
        }
    }
}

nlohmann::json TechLoss::ToJson() const {
    nlohmann::json j;
    j["config"] = m_config.ToJson();

    // Death history
    nlohmann::json historyJson;
    for (const auto& [playerId, records] : m_deathHistory) {
        nlohmann::json recordsJson = nlohmann::json::array();
        for (const auto& record : records) {
            recordsJson.push_back({
                {"type", static_cast<int>(record.type)},
                {"timestamp", record.timestamp},
                {"techsLost", record.techsLost}
            });
        }
        historyJson[playerId] = recordsJson;
    }
    j["deathHistory"] = historyJson;

    // Statistics
    j["totalLossesByPlayer"] = m_totalLossesByPlayer;
    j["totalGainsByPlayer"] = m_totalGainsByPlayer;
    j["lossCountByTech"] = m_lossCountByTech;

    return j;
}

void TechLoss::FromJson(const nlohmann::json& j) {
    if (j.contains("config")) {
        m_config = TechLossConfig::FromJson(j["config"]);
    }

    // Death history
    m_deathHistory.clear();
    if (j.contains("deathHistory")) {
        for (const auto& [playerId, recordsJson] : j["deathHistory"].items()) {
            std::vector<DeathRecord> records;
            for (const auto& recordJson : recordsJson) {
                DeathRecord record;
                record.type = static_cast<DeathType>(recordJson.value("type", 0));
                record.timestamp = recordJson.value("timestamp", 0LL);
                record.techsLost = recordJson.value("techsLost", 0);
                records.push_back(record);
            }
            m_deathHistory[playerId] = std::move(records);
        }
    }

    // Statistics
    if (j.contains("totalLossesByPlayer")) {
        m_totalLossesByPlayer = j["totalLossesByPlayer"].get<std::unordered_map<std::string, int>>();
    }
    if (j.contains("totalGainsByPlayer")) {
        m_totalGainsByPlayer = j["totalGainsByPlayer"].get<std::unordered_map<std::string, int>>();
    }
    if (j.contains("lossCountByTech")) {
        m_lossCountByTech = j["lossCountByTech"].get<std::unordered_map<std::string, int>>();
    }
}

std::string TechLoss::GenerateLossMessage(const TechLossResult& result) const {
    std::stringstream ss;

    if (result.lostTechs.empty() && !result.ageRegressed) {
        ss << "You survived with your knowledge intact!";
        return ss.str();
    }

    ss << DeathTypeToString(result.deathType) << "! ";

    if (!result.lostTechs.empty()) {
        ss << "Lost " << result.lostTechs.size() << " technologies";
    }

    if (result.ageRegressed) {
        ss << " and regressed from " << AgeToString(result.previousAge)
           << " to " << AgeToString(result.newAge);
    }

    ss << ".";

    if (!result.protectedTechs.empty()) {
        ss << " " << result.protectedTechs.size() << " techs were protected.";
    }

    return ss.str();
}

void TechLoss::ApplyTechLoss(TechTree& tech, const std::vector<std::string>& lostTechs) {
    for (const auto& techId : lostTechs) {
        tech.LoseTech(techId);
    }

    // Check for age regression
    Age newAge = CalculateNewAge(tech, lostTechs);
    if (newAge < tech.GetCurrentAge()) {
        tech.RegressToAge(newAge);
    }
}

void TechLoss::ApplyTechGain(TechTree& tech, const std::vector<std::string>& gainedTechs) {
    for (const auto& techId : gainedTechs) {
        tech.GrantTech(techId);
    }
}

bool TechLoss::ShouldLoseTech(const TechNode& node, float severity, float protection) const {
    // Calculate effective loss chance
    float lossChance = node.lossChanceOnDeath * severity;

    // Apply key tech protection
    if (node.isKeyTech) {
        lossChance *= (1.0f - m_config.keyTechProtectionBonus);
    }

    // Apply low age protection
    if (node.requiredAge <= Age::Bronze) {
        lossChance *= (1.0f - m_config.lowAgeTechProtection);
    }

    // Apply player protection
    lossChance *= (1.0f - protection);

    // Roll
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(m_rng) < lossChance;
}

int64_t TechLoss::GetCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string GenerateTechLossMessage(
    const TechLossResult& result,
    const std::string& playerName) {

    std::stringstream ss;
    ss << playerName << " ";

    switch (result.deathType) {
        case DeathType::HeroDeath:
            ss << "was slain";
            break;
        case DeathType::BaseDestroyed:
            ss << "lost their base";
            break;
        case DeathType::TotalDefeat:
            ss << "suffered total defeat";
            break;
        case DeathType::Surrender:
            ss << "surrendered";
            break;
        case DeathType::Assassination:
            ss << "was assassinated";
            break;
        default:
            ss << "was defeated";
    }

    if (!result.lostTechs.empty()) {
        ss << " and lost " << result.lostTechs.size() << " technologies";
    }

    if (result.ageRegressed) {
        ss << ", falling from the " << AgeToShortString(result.previousAge)
           << " to the " << AgeToShortString(result.newAge);
    }

    ss << ".";

    return ss.str();
}

float CalculateRecommendedProtection(
    const TechTree& tech,
    int recentDeaths,
    float hoursSinceLastDeath) {

    float baseNeed = 0.2f;

    // More protection needed at higher ages
    baseNeed += static_cast<float>(static_cast<int>(tech.GetCurrentAge())) * 0.05f;

    // More protection if recent deaths
    baseNeed += recentDeaths * 0.1f;

    // Less protection needed if long time since last death
    if (hoursSinceLastDeath > 24.0f) {
        baseNeed *= 0.5f;
    } else if (hoursSinceLastDeath > 4.0f) {
        baseNeed *= 0.75f;
    }

    return std::min(1.0f, baseNeed);
}

} // namespace RTS
} // namespace Vehement
