#pragma once

/**
 * @file TechLoss.hpp
 * @brief Tech loss mechanics when players die or are defeated
 *
 * Implements Age of Empires style tech loss where:
 * - Hero death causes minor tech loss
 * - Base destruction causes moderate tech loss
 * - Total defeat causes major tech loss
 * - Conquerors can steal techs from defeated players
 * - Some techs are protected or permanent
 */

#include "TechTree.hpp"
#include <string>
#include <vector>
#include <random>
#include <functional>
#include <nlohmann/json.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class TechTree;

// ============================================================================
// Death Types
// ============================================================================

/**
 * @brief Types of defeat that trigger tech loss
 */
enum class DeathType : uint8_t {
    HeroDeath,          ///< Hero killed - minor loss (10-20% of techs at risk)
    BaseDestroyed,      ///< Main base destroyed - moderate loss (30-40%)
    TotalDefeat,        ///< All buildings destroyed - major loss (50-70%)
    Surrender,          ///< Player surrendered - minimal loss (5-10%)
    Timeout,            ///< AFK/disconnect timeout - no loss but vulnerable
    Assassination       ///< Hero killed in enemy territory - moderate loss (25-35%)
};

/**
 * @brief Get display name for death type
 */
[[nodiscard]] inline const char* DeathTypeToString(DeathType type) {
    switch (type) {
        case DeathType::HeroDeath:      return "Hero Death";
        case DeathType::BaseDestroyed:  return "Base Destroyed";
        case DeathType::TotalDefeat:    return "Total Defeat";
        case DeathType::Surrender:      return "Surrender";
        case DeathType::Timeout:        return "Timeout";
        case DeathType::Assassination:  return "Assassination";
        default:                        return "Unknown";
    }
}

/**
 * @brief Get severity multiplier for death type (affects tech loss chance)
 */
[[nodiscard]] inline float GetDeathTypeSeverity(DeathType type) {
    switch (type) {
        case DeathType::HeroDeath:      return 0.15f;   // 15% base severity
        case DeathType::BaseDestroyed:  return 0.35f;   // 35% base severity
        case DeathType::TotalDefeat:    return 0.60f;   // 60% base severity
        case DeathType::Surrender:      return 0.08f;   // 8% base severity
        case DeathType::Timeout:        return 0.0f;    // No tech loss
        case DeathType::Assassination:  return 0.30f;   // 30% base severity
        default:                        return 0.0f;
    }
}

// ============================================================================
// Tech Loss Result
// ============================================================================

/**
 * @brief Result of a tech loss calculation
 */
struct TechLossResult {
    // Lost techs
    std::vector<std::string> lostTechs;         ///< IDs of techs lost
    std::vector<std::string> protectedTechs;    ///< IDs of techs that were protected

    // Gained techs (for conqueror)
    std::vector<std::string> gainedTechs;       ///< IDs of techs gained from conquest

    // Age changes
    Age previousAge = Age::Stone;               ///< Age before loss
    Age newAge = Age::Stone;                    ///< Age after loss (may regress)
    bool ageRegressed = false;                  ///< Did age go down?

    // Statistics
    int totalTechsAtRisk = 0;                   ///< How many could have been lost
    float effectiveSeverity = 0.0f;             ///< Actual severity after modifiers
    float protectionUsed = 0.0f;                ///< Protection level that was active

    // Message for UI
    std::string message;                        ///< Descriptive message for player
    DeathType deathType = DeathType::HeroDeath; ///< What caused the loss

    /**
     * @brief Check if any techs were lost
     */
    [[nodiscard]] bool HasLoss() const { return !lostTechs.empty() || ageRegressed; }

    /**
     * @brief Check if any techs were gained (conquest)
     */
    [[nodiscard]] bool HasGain() const { return !gainedTechs.empty(); }

    /**
     * @brief Get total impact score (for statistics)
     */
    [[nodiscard]] int GetImpactScore() const {
        int score = static_cast<int>(lostTechs.size()) * 10;
        if (ageRegressed) {
            score += (static_cast<int>(previousAge) - static_cast<int>(newAge)) * 50;
        }
        return score;
    }

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static TechLossResult FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Loss Configuration
// ============================================================================

/**
 * @brief Configuration for tech loss system
 */
struct TechLossConfig {
    // Base loss chances per death type (can be modified)
    float heroDeathBaseLoss = 0.15f;
    float baseDestroyedBaseLoss = 0.35f;
    float totalDefeatBaseLoss = 0.60f;
    float surrenderBaseLoss = 0.08f;
    float assassinationBaseLoss = 0.30f;

    // Modifiers
    float keyTechProtectionBonus = 0.3f;        ///< Key techs get +30% protection
    float lowAgeTechProtection = 0.5f;          ///< Stone/Bronze age techs get +50% protection
    float consecutiveDeathPenalty = 0.1f;       ///< +10% loss per consecutive death
    float cooldownReduction = 0.05f;            ///< -5% loss per hour since last death

    // Limits
    int maxTechsLostPerDeath = 5;               ///< Maximum techs lost in single death
    int minTechsRequired = 2;                   ///< Always keep at least N techs
    float maxAgeLossPerDeath = 1.0f;            ///< Max 1 age level loss per death

    // Conquest settings
    float conquestTechGainChance = 0.5f;        ///< 50% chance to gain each lost tech
    float conquestTechGainBonus = 0.2f;         ///< +20% if tech was "stolen" vs lost
    int maxTechsGainedPerConquest = 3;          ///< Max techs gained from conquest

    // Cooldowns
    float deathCooldownHours = 1.0f;            ///< Hours before full loss potential
    float protectionAfterDeathHours = 0.5f;     ///< Temp protection after death

    /**
     * @brief Get default configuration
     */
    static TechLossConfig Default() { return {}; }

    /**
     * @brief Get forgiving configuration (casual mode)
     */
    static TechLossConfig Casual() {
        TechLossConfig config;
        config.heroDeathBaseLoss = 0.05f;
        config.baseDestroyedBaseLoss = 0.15f;
        config.totalDefeatBaseLoss = 0.30f;
        config.maxTechsLostPerDeath = 2;
        config.keyTechProtectionBonus = 0.5f;
        return config;
    }

    /**
     * @brief Get harsh configuration (hardcore mode)
     */
    static TechLossConfig Hardcore() {
        TechLossConfig config;
        config.heroDeathBaseLoss = 0.25f;
        config.baseDestroyedBaseLoss = 0.50f;
        config.totalDefeatBaseLoss = 0.80f;
        config.maxTechsLostPerDeath = 10;
        config.keyTechProtectionBonus = 0.1f;
        config.maxAgeLossPerDeath = 2.0f;
        return config;
    }

    [[nodiscard]] nlohmann::json ToJson() const;
    static TechLossConfig FromJson(const nlohmann::json& j);
};

// ============================================================================
// Tech Loss Manager
// ============================================================================

/**
 * @brief Manages tech loss when players die or are defeated
 *
 * Features:
 * - Calculates which techs are lost based on death type
 * - Applies protection from various sources
 * - Handles conquest tech stealing
 * - Tracks death history for consecutive penalties
 * - Manages age regression
 *
 * Example usage:
 * @code
 * TechLoss techLoss;
 * techLoss.Initialize(TechLossConfig::Default());
 *
 * // When hero dies
 * TechLossResult result = techLoss.OnPlayerDeath(playerTech, DeathType::HeroDeath);
 * if (result.HasLoss()) {
 *     ShowTechLossUI(result);
 * }
 *
 * // When base is conquered
 * TechLossResult result = techLoss.OnBaseConquered(defenderTech, attackerTech);
 * // Defender loses techs, attacker may gain some
 * @endcode
 */
class TechLoss {
public:
    // Callback types
    using TechLostCallback = std::function<void(const std::string& playerId, const TechLossResult& result)>;
    using TechGainedCallback = std::function<void(const std::string& playerId, const std::vector<std::string>& techs)>;

    TechLoss();
    ~TechLoss();

    // Non-copyable
    TechLoss(const TechLoss&) = delete;
    TechLoss& operator=(const TechLoss&) = delete;

    /**
     * @brief Initialize with configuration
     * @param config Tech loss configuration
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const TechLossConfig& config = TechLossConfig::Default());

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Set configuration
     */
    void SetConfig(const TechLossConfig& config) { m_config = config; }

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const TechLossConfig& GetConfig() const { return m_config; }

    // =========================================================================
    // Death Handling
    // =========================================================================

    /**
     * @brief Handle player death (hero killed or disconnected)
     * @param playerTech Player's tech tree (will be modified)
     * @param type Type of death
     * @param playerId Player's ID for tracking
     * @return Result of tech loss calculation
     */
    TechLossResult OnPlayerDeath(TechTree& playerTech, DeathType type,
                                  const std::string& playerId = "");

    /**
     * @brief Handle base being conquered by another player
     * @param defenderTech Defender's tech tree (will lose techs)
     * @param attackerTech Attacker's tech tree (may gain techs)
     * @param defenderId Defender's player ID
     * @param attackerId Attacker's player ID
     * @return Result including both losses and gains
     */
    TechLossResult OnBaseConquered(TechTree& defenderTech, TechTree& attackerTech,
                                    const std::string& defenderId = "",
                                    const std::string& attackerId = "");

    /**
     * @brief Handle surrender (minimal tech loss)
     * @param playerTech Player's tech tree
     * @param playerId Player's ID
     * @return Result of tech loss
     */
    TechLossResult OnSurrender(TechTree& playerTech, const std::string& playerId = "");

    // =========================================================================
    // Tech Loss Calculation
    // =========================================================================

    /**
     * @brief Calculate which techs would be lost (doesn't apply)
     * @param tech Tech tree to analyze
     * @param severity Loss severity (0.0-1.0)
     * @return List of tech IDs that would be lost
     */
    [[nodiscard]] std::vector<std::string> CalculateLostTechs(
        const TechTree& tech, float severity) const;

    /**
     * @brief Calculate which techs conqueror would gain
     * @param loserTech Loser's tech tree
     * @param winnerTech Winner's tech tree
     * @param lostTechs Techs that were lost by loser
     * @return List of tech IDs winner gains
     */
    [[nodiscard]] std::vector<std::string> CalculateGainedTechs(
        const TechTree& loserTech,
        const TechTree& winnerTech,
        const std::vector<std::string>& lostTechs) const;

    /**
     * @brief Get effective loss severity after modifiers
     * @param baseSeverity Base severity from death type
     * @param tech Player's tech tree
     * @param playerId Player ID for history lookup
     * @return Modified severity value
     */
    [[nodiscard]] float CalculateEffectiveSeverity(
        float baseSeverity,
        const TechTree& tech,
        const std::string& playerId) const;

    // =========================================================================
    // Protection Mechanics
    // =========================================================================

    /**
     * @brief Check if a specific tech is protected from loss
     * @param techId Tech to check
     * @param tech Player's tech tree
     * @return true if tech cannot be lost
     */
    [[nodiscard]] bool IsTechProtected(const std::string& techId, const TechTree& tech) const;

    /**
     * @brief Get overall protection level for a player
     * @param tech Player's tech tree
     * @return Protection level (0.0-1.0, higher = more protected)
     */
    [[nodiscard]] float GetTechProtectionLevel(const TechTree& tech) const;

    /**
     * @brief Apply temporary protection after death
     * @param playerId Player to protect
     * @param durationHours Protection duration
     */
    void ApplyTemporaryProtection(const std::string& playerId, float durationHours);

    /**
     * @brief Check if player has temporary protection
     */
    [[nodiscard]] bool HasTemporaryProtection(const std::string& playerId) const;

    /**
     * @brief Get remaining protection time
     */
    [[nodiscard]] float GetProtectionTimeRemaining(const std::string& playerId) const;

    // =========================================================================
    // Death History
    // =========================================================================

    /**
     * @brief Record a death for tracking purposes
     */
    void RecordDeath(const std::string& playerId, DeathType type);

    /**
     * @brief Get number of deaths in recent period
     */
    [[nodiscard]] int GetRecentDeathCount(const std::string& playerId,
                                           float hoursBack = 24.0f) const;

    /**
     * @brief Get time since last death
     */
    [[nodiscard]] float GetHoursSinceLastDeath(const std::string& playerId) const;

    /**
     * @brief Clear death history for player
     */
    void ClearDeathHistory(const std::string& playerId);

    // =========================================================================
    // Age Regression
    // =========================================================================

    /**
     * @brief Check if tech loss would cause age regression
     * @param tech Tech tree to analyze
     * @param lostTechs Techs that would be lost
     * @return New age after losing techs
     */
    [[nodiscard]] Age CalculateNewAge(const TechTree& tech,
                                       const std::vector<std::string>& lostTechs) const;

    /**
     * @brief Check if a specific age can be maintained
     * @param tech Tech tree
     * @param age Age to check
     * @param excludeTechs Techs to exclude (as if lost)
     * @return true if age can be maintained
     */
    [[nodiscard]] bool CanMaintainAge(const TechTree& tech, Age age,
                                       const std::set<std::string>& excludeTechs) const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnTechLost(TechLostCallback callback) {
        m_onTechLost = std::move(callback);
    }

    void SetOnTechGained(TechGainedCallback callback) {
        m_onTechGained = std::move(callback);
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total tech losses for a player
     */
    [[nodiscard]] int GetTotalTechLosses(const std::string& playerId) const;

    /**
     * @brief Get total tech gains for a player (from conquest)
     */
    [[nodiscard]] int GetTotalTechGains(const std::string& playerId) const;

    /**
     * @brief Get most commonly lost tech
     */
    [[nodiscard]] std::string GetMostLostTech() const;

    // =========================================================================
    // Update & Persistence
    // =========================================================================

    /**
     * @brief Update (for time-based protection expiry)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Serialize state to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Load state from JSON
     */
    void FromJson(const nlohmann::json& j);

private:
    // Helper functions
    float GetBaseSeverityForDeathType(DeathType type) const;
    std::string GenerateLossMessage(const TechLossResult& result) const;
    void ApplyTechLoss(TechTree& tech, const std::vector<std::string>& lostTechs);
    void ApplyTechGain(TechTree& tech, const std::vector<std::string>& gainedTechs);
    bool ShouldLoseTech(const TechNode& tech, float severity, float protection) const;
    int64_t GetCurrentTimestamp() const;

    // State
    bool m_initialized = false;
    TechLossConfig m_config;

    // Death history per player
    struct DeathRecord {
        DeathType type;
        int64_t timestamp;
        int techsLost;
    };
    std::unordered_map<std::string, std::vector<DeathRecord>> m_deathHistory;

    // Temporary protection
    struct ProtectionEntry {
        float remainingTime;
        float initialDuration;
    };
    std::unordered_map<std::string, ProtectionEntry> m_temporaryProtection;

    // Statistics
    std::unordered_map<std::string, int> m_totalLossesByPlayer;
    std::unordered_map<std::string, int> m_totalGainsByPlayer;
    std::unordered_map<std::string, int> m_lossCountByTech;

    // Random number generator
    mutable std::mt19937 m_rng;

    // Callbacks
    TechLostCallback m_onTechLost;
    TechGainedCallback m_onTechGained;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate a descriptive message for tech loss event
 */
[[nodiscard]] std::string GenerateTechLossMessage(
    const TechLossResult& result,
    const std::string& playerName);

/**
 * @brief Calculate recommended protection level for current situation
 */
[[nodiscard]] float CalculateRecommendedProtection(
    const TechTree& tech,
    int recentDeaths,
    float hoursSinceLastDeath);

} // namespace RTS
} // namespace Vehement
