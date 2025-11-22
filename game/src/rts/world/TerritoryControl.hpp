#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Control point status
 */
enum class ControlPointStatus : uint8_t {
    Neutral,        // No one controlling
    Capturing,      // Being captured
    Controlled,     // Fully controlled
    Contested,      // Multiple factions fighting
    Fortified,      // Extra defense bonus
    Locked          // Cannot be captured temporarily
};

/**
 * @brief Control point in a region
 */
struct ControlPoint {
    std::string id;
    std::string name;
    std::string regionId;
    Geo::GeoCoordinate location;
    glm::vec3 worldPosition{0.0f};

    ControlPointStatus status = ControlPointStatus::Neutral;
    int controllingFaction = -1;
    std::string controllingPlayerId;
    float captureProgress = 0.0f;  // 0-100
    float influenceRadius = 500.0f;  // meters
    int pointValue = 1;  // Strategic value

    // Capture state
    int capturingFaction = -1;
    std::string capturingPlayerId;
    float captureRate = 1.0f;
    float defendBonus = 1.5f;

    // Requirements
    int minPlayersToCapture = 1;
    int minUnitsToCapture = 5;
    bool requiresAdjacentControl = false;

    // Bonuses when controlled
    float resourceBonusPercent = 10.0f;
    float experienceBonusPercent = 5.0f;
    float defenseBonusPercent = 20.0f;
    std::vector<std::string> unlocksBuildings;
    std::vector<std::string> unlocksUnits;

    // Timing
    int64_t lastCaptureTimestamp = 0;
    int64_t controlDuration = 0;
    float captureTimeRequired = 300.0f;  // seconds

    [[nodiscard]] nlohmann::json ToJson() const;
    static ControlPoint FromJson(const nlohmann::json& j);
};

/**
 * @brief Influence spread from controlled points
 */
struct InfluenceNode {
    std::string sourcePointId;
    int factionId = -1;
    float strength = 0.0f;
    float decayRate = 0.1f;  // per hour
    float spreadRate = 0.05f;  // per hour
    float maxRadius = 1000.0f;

    [[nodiscard]] nlohmann::json ToJson() const;
    static InfluenceNode FromJson(const nlohmann::json& j);
};

/**
 * @brief Victory condition for territory control
 */
struct VictoryCondition {
    std::string id;
    std::string name;
    std::string description;
    std::string type;  // points, time, domination, elimination

    // Point-based
    int targetPoints = 1000;
    float pointsPerControlled = 1.0f;
    float pointsPerSecond = 0.1f;

    // Time-based
    float holdTimeSeconds = 3600.0f;

    // Domination
    float controlPercentRequired = 75.0f;

    // Current state
    std::unordered_map<int, int> factionPoints;
    std::unordered_map<int, float> factionHoldTime;
    bool achieved = false;
    int winningFaction = -1;

    [[nodiscard]] nlohmann::json ToJson() const;
    static VictoryCondition FromJson(const nlohmann::json& j);
};

/**
 * @brief Capture attempt record
 */
struct CaptureAttempt {
    std::string pointId;
    std::string playerId;
    int factionId = -1;
    int64_t startTimestamp = 0;
    float progressAtStart = 0.0f;
    int unitsInvolved = 0;
    bool successful = false;
    bool interrupted = false;
    std::string interruptReason;

    [[nodiscard]] nlohmann::json ToJson() const;
    static CaptureAttempt FromJson(const nlohmann::json& j);
};

/**
 * @brief Territory control configuration
 */
struct TerritoryControlConfig {
    float baseCaptureTime = 300.0f;
    float captureSpeedPerUnit = 0.1f;
    float maxCaptureSpeed = 5.0f;
    float defendBonusMultiplier = 1.5f;
    float influenceDecayPerHour = 10.0f;
    float influenceSpreadPerHour = 5.0f;
    float contestedDecayMultiplier = 2.0f;
    float pointsPerSecondControlled = 0.1f;
    bool requireLineOfSight = true;
    float captureInterruptRadius = 50.0f;
};

/**
 * @brief Manager for territory control mechanics
 */
class TerritoryControlManager {
public:
    using PointCapturedCallback = std::function<void(const ControlPoint& point)>;
    using PointContestedCallback = std::function<void(const ControlPoint& point)>;
    using VictoryCallback = std::function<void(const VictoryCondition& condition, int winningFaction)>;
    using InfluenceChangedCallback = std::function<void(const std::string& regionId, int factionId, float influence)>;

    [[nodiscard]] static TerritoryControlManager& Instance();

    TerritoryControlManager(const TerritoryControlManager&) = delete;
    TerritoryControlManager& operator=(const TerritoryControlManager&) = delete;

    [[nodiscard]] bool Initialize(const TerritoryControlConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);

    // ==================== Control Points ====================

    /**
     * @brief Register control point
     */
    void RegisterControlPoint(const ControlPoint& point);

    /**
     * @brief Get control point
     */
    [[nodiscard]] const ControlPoint* GetControlPoint(const std::string& pointId) const;

    /**
     * @brief Get all control points in region
     */
    [[nodiscard]] std::vector<ControlPoint> GetRegionControlPoints(const std::string& regionId) const;

    /**
     * @brief Get controlled points for faction
     */
    [[nodiscard]] std::vector<ControlPoint> GetFactionControlPoints(int factionId) const;

    /**
     * @brief Start capturing a point
     */
    bool StartCapture(const std::string& pointId, const std::string& playerId,
                      int factionId, int unitCount);

    /**
     * @brief Stop capturing
     */
    void StopCapture(const std::string& pointId, const std::string& playerId);

    /**
     * @brief Force capture (admin)
     */
    void ForceCapture(const std::string& pointId, int factionId, const std::string& playerId);

    /**
     * @brief Neutralize point
     */
    void NeutralizePoint(const std::string& pointId);

    /**
     * @brief Check if point can be captured
     */
    [[nodiscard]] bool CanCapture(const std::string& pointId, int factionId) const;

    // ==================== Influence ====================

    /**
     * @brief Get faction influence at coordinate
     */
    [[nodiscard]] float GetInfluenceAt(const Geo::GeoCoordinate& coord, int factionId) const;

    /**
     * @brief Get dominant faction at coordinate
     */
    [[nodiscard]] int GetDominantFactionAt(const Geo::GeoCoordinate& coord) const;

    /**
     * @brief Get total influence for faction in region
     */
    [[nodiscard]] float GetRegionInfluence(const std::string& regionId, int factionId) const;

    /**
     * @brief Get control percentage for faction in region
     */
    [[nodiscard]] float GetRegionControlPercent(const std::string& regionId, int factionId) const;

    // ==================== Victory Conditions ====================

    /**
     * @brief Set victory condition
     */
    void SetVictoryCondition(const VictoryCondition& condition);

    /**
     * @brief Get current victory condition
     */
    [[nodiscard]] const VictoryCondition* GetVictoryCondition() const;

    /**
     * @brief Get faction points
     */
    [[nodiscard]] int GetFactionPoints(int factionId) const;

    /**
     * @brief Check if any faction achieved victory
     */
    [[nodiscard]] bool CheckVictory() const;

    /**
     * @brief Get winning faction (-1 if none)
     */
    [[nodiscard]] int GetWinningFaction() const;

    // ==================== Statistics ====================

    /**
     * @brief Get capture history for point
     */
    [[nodiscard]] std::vector<CaptureAttempt> GetCaptureHistory(const std::string& pointId) const;

    /**
     * @brief Get total controlled points per faction
     */
    [[nodiscard]] std::unordered_map<int, int> GetControlledPointsCount() const;

    /**
     * @brief Get contested points
     */
    [[nodiscard]] std::vector<ControlPoint> GetContestedPoints() const;

    // ==================== Callbacks ====================

    void OnPointCaptured(PointCapturedCallback callback);
    void OnPointContested(PointContestedCallback callback);
    void OnVictory(VictoryCallback callback);
    void OnInfluenceChanged(InfluenceChangedCallback callback);

    // ==================== Configuration ====================

    [[nodiscard]] const TerritoryControlConfig& GetConfig() const { return m_config; }
    void SetConfig(const TerritoryControlConfig& config) { m_config = config; }

private:
    TerritoryControlManager() = default;
    ~TerritoryControlManager() = default;

    void UpdateCaptureProgress(float deltaTime);
    void UpdateInfluenceSpread(float deltaTime);
    void UpdateVictoryCondition(float deltaTime);
    void ProcessCapture(ControlPoint& point);
    float CalculateCaptureSpeed(const ControlPoint& point, int attackerUnits, int defenderUnits) const;

    bool m_initialized = false;
    TerritoryControlConfig m_config;

    std::unordered_map<std::string, ControlPoint> m_controlPoints;
    mutable std::mutex m_pointsMutex;

    std::unordered_map<std::string, InfluenceNode> m_influenceNodes;
    mutable std::mutex m_influenceMutex;

    VictoryCondition m_victoryCondition;
    mutable std::mutex m_victoryMutex;

    std::unordered_map<std::string, std::vector<CaptureAttempt>> m_captureHistory;
    std::mutex m_historyMutex;

    // Active captures
    struct ActiveCapture {
        std::string playerId;
        int factionId;
        int unitCount;
        int64_t startTime;
    };
    std::unordered_map<std::string, std::vector<ActiveCapture>> m_activeCaptures;
    std::mutex m_captureMutex;

    std::vector<PointCapturedCallback> m_capturedCallbacks;
    std::vector<PointContestedCallback> m_contestedCallbacks;
    std::vector<VictoryCallback> m_victoryCallbacks;
    std::vector<InfluenceChangedCallback> m_influenceCallbacks;
    std::mutex m_callbackMutex;

    float m_influenceUpdateTimer = 0.0f;
    float m_victoryUpdateTimer = 0.0f;
    static constexpr float INFLUENCE_UPDATE_INTERVAL = 10.0f;
    static constexpr float VICTORY_UPDATE_INTERVAL = 1.0f;
};

} // namespace Vehement
