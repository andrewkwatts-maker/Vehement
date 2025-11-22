#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../../rts/world/WorldRegion.hpp"

namespace Vehement {

/**
 * @brief Region info tab type
 */
enum class RegionInfoTab : uint8_t {
    Overview,
    Resources,
    Portals,
    Quests,
    Players,
    History,
    Leaderboard
};

/**
 * @brief Region leaderboard entry
 */
struct RegionLeaderboardEntry {
    std::string playerId;
    std::string playerName;
    int factionId = -1;
    int score = 0;
    int rank = 0;
    std::string achievement;
    int64_t timestamp = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionLeaderboardEntry FromJson(const nlohmann::json& j);
};

/**
 * @brief Region history event
 */
struct RegionHistoryEvent {
    std::string eventId;
    std::string eventType;
    std::string description;
    int64_t timestamp = 0;
    std::string playerId;
    int factionId = -1;
    nlohmann::json details;

    [[nodiscard]] nlohmann::json ToJson() const;
    static RegionHistoryEvent FromJson(const nlohmann::json& j);
};

/**
 * @brief Active player info in region
 */
struct RegionPlayerInfo {
    std::string playerId;
    std::string playerName;
    int level = 1;
    int factionId = -1;
    bool online = false;
    int64_t lastSeen = 0;
    std::string activity;
};

/**
 * @brief Region info UI configuration
 */
struct RegionInfoUIConfig {
    bool showWeather = true;
    bool showPlayerCount = true;
    bool showControlInfo = true;
    bool showResources = true;
    bool showPortals = true;
    bool showQuests = true;
    int maxHistoryEntries = 50;
    int maxLeaderboardEntries = 100;
    float refreshInterval = 30.0f;
};

/**
 * @brief Region information UI panel
 */
class RegionInfoUI {
public:
    using RegionActionCallback = std::function<void(const std::string& action, const std::string& regionId)>;
    using PortalSelectCallback = std::function<void(const std::string& portalId)>;
    using QuestSelectCallback = std::function<void(const std::string& questId)>;
    using PlayerSelectCallback = std::function<void(const std::string& playerId)>;

    [[nodiscard]] static RegionInfoUI& Instance();

    RegionInfoUI(const RegionInfoUI&) = delete;
    RegionInfoUI& operator=(const RegionInfoUI&) = delete;

    [[nodiscard]] bool Initialize(const RegionInfoUIConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);
    void Render();

    // ==================== Panel Control ====================

    void Show(const std::string& regionId);
    void Hide();
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetRegion(const std::string& regionId);
    [[nodiscard]] std::string GetCurrentRegionId() const { return m_currentRegionId; }

    void Refresh();

    // ==================== Tab Navigation ====================

    void SetActiveTab(RegionInfoTab tab);
    [[nodiscard]] RegionInfoTab GetActiveTab() const { return m_activeTab; }

    void NextTab();
    void PreviousTab();

    // ==================== Overview ====================

    [[nodiscard]] std::string GetRegionName() const;
    [[nodiscard]] std::string GetRegionDescription() const;
    [[nodiscard]] std::string GetBiomeType() const;
    [[nodiscard]] int GetDangerLevel() const;
    [[nodiscard]] int GetRecommendedLevel() const;
    [[nodiscard]] RegionWeather GetCurrentWeather() const;

    // ==================== Control Info ====================

    [[nodiscard]] int GetControllingFaction() const;
    [[nodiscard]] std::string GetControllingPlayer() const;
    [[nodiscard]] float GetControlStrength() const;
    [[nodiscard]] bool IsContested() const;

    // ==================== Resources ====================

    [[nodiscard]] std::vector<ResourceNode> GetAvailableResources() const;
    [[nodiscard]] float GetResourceMultiplier() const;

    // ==================== Portals ====================

    [[nodiscard]] std::vector<PortalConnection> GetPortals() const;

    // ==================== Quests ====================

    [[nodiscard]] std::vector<RegionalQuest> GetAvailableQuests() const;
    [[nodiscard]] std::vector<RegionalQuest> GetCompletedQuests() const;

    // ==================== Players ====================

    [[nodiscard]] std::vector<RegionPlayerInfo> GetActivePlayers() const;
    [[nodiscard]] int GetOnlinePlayerCount() const;
    [[nodiscard]] int GetTotalVisitors() const;

    // ==================== History ====================

    [[nodiscard]] std::vector<RegionHistoryEvent> GetHistory(int limit = 50) const;
    void AddHistoryEvent(const RegionHistoryEvent& event);

    // ==================== Leaderboard ====================

    [[nodiscard]] std::vector<RegionLeaderboardEntry> GetLeaderboard(const std::string& category) const;
    [[nodiscard]] std::vector<std::string> GetLeaderboardCategories() const;

    // ==================== Actions ====================

    void TravelToRegion();
    void SetAsWaypoint();
    void ViewOnMap();

    // ==================== Callbacks ====================

    void OnRegionAction(RegionActionCallback callback);
    void OnPortalSelected(PortalSelectCallback callback);
    void OnQuestSelected(QuestSelectCallback callback);
    void OnPlayerSelected(PlayerSelectCallback callback);

    // ==================== JS Bridge ====================

    [[nodiscard]] nlohmann::json GetRegionInfoForJS() const;
    void HandleJSCommand(const std::string& command, const nlohmann::json& params);

    // ==================== Configuration ====================

    [[nodiscard]] const RegionInfoUIConfig& GetConfig() const { return m_config; }

private:
    RegionInfoUI() = default;
    ~RegionInfoUI() = default;

    void RefreshData();
    void LoadHistory();
    void LoadLeaderboard();

    bool m_initialized = false;
    RegionInfoUIConfig m_config;
    bool m_visible = false;

    std::string m_currentRegionId;
    RegionInfoTab m_activeTab = RegionInfoTab::Overview;

    // Cached data
    std::vector<RegionHistoryEvent> m_history;
    std::unordered_map<std::string, std::vector<RegionLeaderboardEntry>> m_leaderboards;
    std::vector<RegionPlayerInfo> m_activePlayers;

    float m_refreshTimer = 0.0f;

    // Callbacks
    std::vector<RegionActionCallback> m_actionCallbacks;
    std::vector<PortalSelectCallback> m_portalCallbacks;
    std::vector<QuestSelectCallback> m_questCallbacks;
    std::vector<PlayerSelectCallback> m_playerCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
