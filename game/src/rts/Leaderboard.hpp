#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Leaderboard categories
 */
enum class LeaderboardCategory {
    TerritoryControlled = 0,    // Most tiles owned
    ZombiesKilled,              // Total zombies killed
    Population,                 // Current population (workers)
    Wealth,                     // Total resource value
    SurvivalTime,               // Hours survived
    BuildingsConstructed,       // Total buildings built
    AttacksSurvived,            // Attacks successfully defended
    DailyZombieKills,           // Zombies killed in last 24 hours
    WeeklyTerritory,            // Territory gained in last 7 days
    Count
};

/**
 * @brief Get category name
 */
[[nodiscard]] const char* LeaderboardCategoryToString(LeaderboardCategory category);

/**
 * @brief Leaderboard entry
 */
struct LeaderboardEntry {
    int rank = 0;
    std::string playerId;
    std::string playerName;
    int64_t score = 0;
    int64_t previousScore = 0;      // Score from last update
    int previousRank = 0;           // Rank from last update
    int64_t lastUpdated = 0;        // Timestamp
    std::string region;             // Player's region

    // Additional context based on category
    nlohmann::json metadata;        // Category-specific data

    [[nodiscard]] nlohmann::json ToJson() const;
    static LeaderboardEntry FromJson(const nlohmann::json& j);

    /**
     * @brief Get rank change (+positive = improved, -negative = dropped)
     */
    [[nodiscard]] int GetRankChange() const { return previousRank - rank; }

    /**
     * @brief Get score change
     */
    [[nodiscard]] int64_t GetScoreChange() const { return score - previousScore; }
};

/**
 * @brief Leaderboard data for a category
 */
struct Leaderboard {
    LeaderboardCategory category;
    std::vector<LeaderboardEntry> entries;
    int64_t lastUpdated = 0;
    int totalPlayers = 0;           // Total players on this leaderboard
    int64_t highestScore = 0;       // Top score
    float averageScore = 0.0f;      // Average score

    [[nodiscard]] nlohmann::json ToJson() const;
    static Leaderboard FromJson(const nlohmann::json& j);

    /**
     * @brief Get entry by player ID
     */
    [[nodiscard]] const LeaderboardEntry* GetEntry(const std::string& playerId) const;

    /**
     * @brief Get entries in rank range
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetRange(int startRank, int count) const;

    /**
     * @brief Get top N entries
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetTop(int count) const;

    /**
     * @brief Get entries around a player
     */
    [[nodiscard]] std::vector<LeaderboardEntry> GetAroundPlayer(
        const std::string& playerId, int count) const;
};

/**
 * @brief Player stats for leaderboard submission
 */
struct PlayerLeaderboardStats {
    std::string playerId;
    std::string playerName;
    std::string region;

    // Stats
    int territoryTiles = 0;
    int zombiesKilled = 0;
    int population = 0;
    int64_t wealth = 0;             // Total resource value
    float survivalHours = 0.0f;
    int buildingsBuilt = 0;
    int attacksSurvived = 0;

    // Time-windowed stats
    int zombiesKilled24h = 0;       // Last 24 hours
    int territoryGained7d = 0;      // Last 7 days

    [[nodiscard]] nlohmann::json ToJson() const;
    static PlayerLeaderboardStats FromJson(const nlohmann::json& j);

    /**
     * @brief Calculate wealth from resources
     */
    static int64_t CalculateWealth(int food, int wood, int stone, int metal,
                                   int fuel, int medicine, int ammo);
};

/**
 * @brief Leaderboard configuration
 */
struct LeaderboardConfig {
    // Update settings
    float updateIntervalSeconds = 300.0f;   // 5 minutes between updates
    float scoreSubmitInterval = 60.0f;      // Submit local scores every minute

    // Display settings
    int defaultTopCount = 100;              // Default entries to fetch
    int aroundPlayerCount = 5;              // Entries around player (each side)

    // Cache settings
    float cacheLifetimeSeconds = 120.0f;    // How long to cache results
    bool enableLocalCache = true;

    // Filtering
    bool showOnlyOnlinePlayers = false;
    bool showOnlyRegionalPlayers = false;
};

/**
 * @brief Achievement unlocked from leaderboard position
 */
struct LeaderboardAchievement {
    std::string id;
    std::string name;
    std::string description;
    LeaderboardCategory category;
    int requiredRank = 0;           // 0 = any rank, 1 = top 1, etc.
    int64_t requiredScore = 0;      // Alternative: required score
    bool earned = false;
    int64_t earnedTimestamp = 0;

    [[nodiscard]] nlohmann::json ToJson() const;
    static LeaderboardAchievement FromJson(const nlohmann::json& j);
};

/**
 * @brief Global leaderboard manager
 *
 * Features:
 * - Multiple leaderboard categories
 * - Real-time score updates via Firebase
 * - Local caching for performance
 * - Regional and global leaderboards
 * - Time-windowed leaderboards (daily, weekly)
 * - Achievement tracking based on ranks
 */
class LeaderboardManager {
public:
    /**
     * @brief Callback types
     */
    using LeaderboardCallback = std::function<void(const Leaderboard& leaderboard)>;
    using RankCallback = std::function<void(LeaderboardCategory category, int rank)>;
    using AchievementCallback = std::function<void(const LeaderboardAchievement& achievement)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static LeaderboardManager& Instance();

    // Non-copyable
    LeaderboardManager(const LeaderboardManager&) = delete;
    LeaderboardManager& operator=(const LeaderboardManager&) = delete;

    /**
     * @brief Initialize leaderboard system
     * @param config Configuration
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const LeaderboardConfig& config = {});

    /**
     * @brief Shutdown leaderboard system
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update leaderboard system (call from game loop)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // ==================== Score Submission ====================

    /**
     * @brief Submit current player stats to all leaderboards
     * @param stats Player stats to submit
     */
    void SubmitStats(const PlayerLeaderboardStats& stats);

    /**
     * @brief Submit score to a specific leaderboard
     * @param category Leaderboard category
     * @param score Score to submit
     * @param metadata Optional additional data
     */
    void SubmitScore(LeaderboardCategory category, int64_t score,
                     const nlohmann::json& metadata = {});

    /**
     * @brief Set local player info
     */
    void SetLocalPlayer(const std::string& playerId, const std::string& playerName,
                       const std::string& region);

    // ==================== Leaderboard Queries ====================

    /**
     * @brief Get leaderboard (from cache or server)
     * @param category Leaderboard category
     * @param callback Callback with leaderboard data
     * @param forceRefresh Force server fetch even if cached
     */
    void GetLeaderboard(LeaderboardCategory category, LeaderboardCallback callback,
                       bool forceRefresh = false);

    /**
     * @brief Get top N entries for a category
     * @param category Leaderboard category
     * @param count Number of entries
     * @param callback Callback with entries
     */
    void GetTopEntries(LeaderboardCategory category, int count, LeaderboardCallback callback);

    /**
     * @brief Get entries around local player
     * @param category Leaderboard category
     * @param callback Callback with entries
     */
    void GetAroundMe(LeaderboardCategory category, LeaderboardCallback callback);

    /**
     * @brief Get local player's rank in a category
     * @param category Leaderboard category
     * @param callback Callback with rank
     */
    void GetMyRank(LeaderboardCategory category, RankCallback callback);

    /**
     * @brief Get local player's entry
     */
    [[nodiscard]] const LeaderboardEntry* GetMyEntry(LeaderboardCategory category) const;

    /**
     * @brief Get cached leaderboard (may be stale)
     */
    [[nodiscard]] const Leaderboard* GetCachedLeaderboard(LeaderboardCategory category) const;

    // ==================== Regional Leaderboards ====================

    /**
     * @brief Get regional leaderboard
     * @param category Leaderboard category
     * @param region Region code
     * @param callback Callback with leaderboard
     */
    void GetRegionalLeaderboard(LeaderboardCategory category, const std::string& region,
                               LeaderboardCallback callback);

    /**
     * @brief Get my region's leaderboard
     */
    void GetMyRegionLeaderboard(LeaderboardCategory category, LeaderboardCallback callback);

    // ==================== Achievements ====================

    /**
     * @brief Get all leaderboard achievements
     */
    [[nodiscard]] std::vector<LeaderboardAchievement> GetAchievements() const;

    /**
     * @brief Get earned achievements
     */
    [[nodiscard]] std::vector<LeaderboardAchievement> GetEarnedAchievements() const;

    /**
     * @brief Check and award achievements based on current rank
     */
    void CheckAchievements();

    /**
     * @brief Register callback for achievement unlocks
     */
    void OnAchievementUnlocked(AchievementCallback callback);

    // ==================== Statistics ====================

    /**
     * @brief Get stats about a leaderboard category
     */
    struct LeaderboardStats {
        int totalPlayers;
        int64_t highestScore;
        int64_t lowestScore;
        float averageScore;
        float medianScore;
        int myRank;
        int64_t myScore;
        float percentile;           // Your percentile (top X%)
    };
    [[nodiscard]] LeaderboardStats GetStats(LeaderboardCategory category) const;

    // ==================== Configuration ====================

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const LeaderboardConfig& GetConfig() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void SetConfig(const LeaderboardConfig& config) { m_config = config; }

    /**
     * @brief Force refresh all leaderboards
     */
    void RefreshAll();

    // ==================== Callbacks ====================

    /**
     * @brief Register callback for rank changes
     */
    void OnRankChanged(RankCallback callback);

private:
    LeaderboardManager() = default;
    ~LeaderboardManager() = default;

    // Firebase paths
    [[nodiscard]] std::string GetGlobalLeaderboardPath(LeaderboardCategory category) const;
    [[nodiscard]] std::string GetRegionalLeaderboardPath(LeaderboardCategory category,
                                                         const std::string& region) const;
    [[nodiscard]] std::string GetPlayerScorePath(const std::string& playerId) const;

    // Internal helpers
    void FetchLeaderboard(LeaderboardCategory category, const std::string& path,
                         LeaderboardCallback callback);
    void ProcessScoreSubmission();
    void UpdateLocalRanks();
    void InitializeAchievements();
    int64_t GetScoreForCategory(const PlayerLeaderboardStats& stats,
                                LeaderboardCategory category) const;

    bool m_initialized = false;
    LeaderboardConfig m_config;

    // Local player info
    std::string m_localPlayerId;
    std::string m_localPlayerName;
    std::string m_localRegion;

    // Cached leaderboards
    std::unordered_map<LeaderboardCategory, Leaderboard> m_cachedLeaderboards;
    std::unordered_map<LeaderboardCategory, int64_t> m_cacheTimestamps;
    mutable std::mutex m_cacheMutex;

    // Local player entries
    std::unordered_map<LeaderboardCategory, LeaderboardEntry> m_myEntries;
    mutable std::mutex m_myEntriesMutex;

    // Pending score submissions
    PlayerLeaderboardStats m_pendingStats;
    bool m_hasPendingStats = false;
    std::mutex m_pendingMutex;

    // Timers
    float m_updateTimer = 0.0f;
    float m_submitTimer = 0.0f;

    // Achievements
    std::vector<LeaderboardAchievement> m_achievements;
    std::mutex m_achievementsMutex;

    // Callbacks
    std::vector<RankCallback> m_rankCallbacks;
    std::vector<AchievementCallback> m_achievementCallbacks;
    std::mutex m_callbackMutex;
};

} // namespace Vehement
