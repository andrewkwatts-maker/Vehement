#pragma once

#include "GPSLocation.hpp"
#include "FirebaseManager.hpp"
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>

namespace Vehement {

/**
 * @brief Player information for matchmaking
 */
struct PlayerInfo {
    std::string oderId;          ///< Unique player ID (Firebase user ID)
    std::string displayName;     ///< Player's display name
    std::string townId;          ///< Current town the player is in
    float x = 0, y = 0, z = 0;   ///< Last known position
    float rotation = 0;          ///< Y-axis rotation

    /**
     * @brief Player status
     */
    enum class Status {
        Online,
        Away,
        Busy,
        Offline
    } status = Status::Online;

    int64_t lastSeen = 0;        ///< Unix timestamp of last activity
    int level = 1;               ///< Player level/rank
    int zombiesKilled = 0;       ///< Total zombies killed
    bool isHost = false;         ///< Is this player the town host

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static PlayerInfo FromJson(const nlohmann::json& j);

    /**
     * @brief Check if player data is valid
     */
    [[nodiscard]] bool IsValid() const { return !oderId.empty(); }

    /**
     * @brief Check if player is currently online
     */
    [[nodiscard]] bool IsOnline() const { return status == Status::Online; }
};

/**
 * @brief Player matchmaking and presence management
 *
 * Handles:
 * - Registering player presence in a town
 * - Listing players in the current town
 * - Join/leave town notifications
 * - Player disconnect detection
 * - Finding nearby towns with active players
 *
 * Firebase paths:
 * - /towns/{townId}/players/{oderId} - player presence data
 * - /players/{oderId} - global player profile
 * - /presence/{oderId} - connection state
 */
class Matchmaking {
public:
    /**
     * @brief Matchmaking configuration
     */
    struct Config {
        int maxPlayersPerTown = 32;        ///< Maximum players in one town
        int heartbeatIntervalMs = 5000;    ///< Presence heartbeat interval
        int offlineTimeoutMs = 15000;      ///< Time before marking player offline
        bool autoReconnect = true;         ///< Automatically reconnect on disconnect
    };

    // Callback types
    using PlayerCallback = std::function<void(const PlayerInfo& player)>;
    using PlayerLeftCallback = std::function<void(const std::string& oderId)>;
    using PlayersListCallback = std::function<void(const std::vector<PlayerInfo>& players)>;
    using TownPlayersCallback = std::function<void(const std::string& townId, int playerCount)>;

    /**
     * @brief Get singleton instance
     */
    [[nodiscard]] static Matchmaking& Instance();

    // Delete copy/move
    Matchmaking(const Matchmaking&) = delete;
    Matchmaking& operator=(const Matchmaking&) = delete;

    /**
     * @brief Initialize matchmaking system
     * @param config Configuration options
     * @return true if successful
     */
    [[nodiscard]] bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown matchmaking
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // ==================== Town Operations ====================

    /**
     * @brief Join a town and register presence
     * @param town Town to join
     * @param callback Called when join completes
     */
    void JoinTown(const TownInfo& town, std::function<void(bool success)> callback = nullptr);

    /**
     * @brief Leave the current town
     */
    void LeaveTown();

    /**
     * @brief Get the current town ID
     */
    [[nodiscard]] std::string GetCurrentTownId() const { return m_currentTownId; }

    /**
     * @brief Check if currently in a town
     */
    [[nodiscard]] bool IsInTown() const { return !m_currentTownId.empty(); }

    // ==================== Player Information ====================

    /**
     * @brief Get all players in the current town
     * @return Vector of player info
     */
    [[nodiscard]] std::vector<PlayerInfo> GetPlayersInTown() const;

    /**
     * @brief Get player count in current town
     */
    [[nodiscard]] int GetPlayerCount() const;

    /**
     * @brief Get specific player by ID
     * @param oderId Player ID to find
     * @return Player info or nullopt if not found
     */
    [[nodiscard]] std::optional<PlayerInfo> GetPlayer(const std::string& oderId) const;

    /**
     * @brief Get the local player's info
     */
    [[nodiscard]] const PlayerInfo& GetLocalPlayer() const { return m_localPlayer; }

    /**
     * @brief Update local player information
     * @param info Updated player info
     */
    void UpdateLocalPlayer(const PlayerInfo& info);

    /**
     * @brief Set local player's display name
     * @param name Display name
     */
    void SetDisplayName(const std::string& name);

    /**
     * @brief Update local player's position
     */
    void UpdatePosition(float x, float y, float z, float rotation);

    // ==================== Player Events ====================

    /**
     * @brief Register callback for when a player joins
     * @param callback Function to call with player info
     */
    void OnPlayerJoined(PlayerCallback callback);

    /**
     * @brief Register callback for when a player leaves
     * @param callback Function to call with player ID
     */
    void OnPlayerLeft(PlayerLeftCallback callback);

    /**
     * @brief Register callback for player updates
     * @param callback Function to call when any player data changes
     */
    void OnPlayerUpdated(PlayerCallback callback);

    // ==================== Town Discovery ====================

    /**
     * @brief Find nearby towns with active players
     * @param location Current GPS location
     * @param radiusKm Search radius in kilometers
     * @param callback Called with list of towns and player counts
     */
    void FindNearbyTowns(GPSCoordinates location, float radiusKm,
                         std::function<void(const std::vector<std::pair<TownInfo, int>>&)> callback);

    /**
     * @brief Get player count for a specific town
     * @param townId Town identifier
     * @param callback Called with player count
     */
    void GetTownPlayerCount(const std::string& townId,
                            std::function<void(int count)> callback);

    // ==================== Update ====================

    /**
     * @brief Process matchmaking updates (call from game loop)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

private:
    Matchmaking() = default;
    ~Matchmaking() = default;

    // Firebase paths
    [[nodiscard]] std::string GetTownPlayersPath() const;
    [[nodiscard]] std::string GetPlayerPath(const std::string& oderId) const;
    [[nodiscard]] std::string GetPresencePath(const std::string& oderId) const;

    // Internal operations
    void RegisterPresence();
    void UnregisterPresence();
    void SendHeartbeat();
    void SetupListeners();
    void RemoveListeners();
    void HandlePlayersUpdate(const nlohmann::json& data);
    void CheckPlayerTimeouts();

    // Configuration
    Config m_config;
    bool m_initialized = false;

    // State
    std::string m_currentTownId;
    PlayerInfo m_localPlayer;
    std::unordered_map<std::string, PlayerInfo> m_players;
    mutable std::mutex m_playersMutex;

    // Listeners
    std::string m_playersListenerId;

    // Callbacks
    std::vector<PlayerCallback> m_playerJoinedCallbacks;
    std::vector<PlayerLeftCallback> m_playerLeftCallbacks;
    std::vector<PlayerCallback> m_playerUpdatedCallbacks;
    std::mutex m_callbackMutex;

    // Heartbeat timing
    float m_heartbeatTimer = 0.0f;
    float m_timeoutCheckTimer = 0.0f;
    static constexpr float TIMEOUT_CHECK_INTERVAL = 2.0f;
};

/**
 * @brief Find the best town to join based on various criteria
 */
class TownFinder {
public:
    /**
     * @brief Search criteria for finding towns
     */
    struct SearchCriteria {
        GPSCoordinates location;         ///< Search center
        float maxRadiusKm = 50.0f;       ///< Maximum search radius
        int minPlayers = 0;              ///< Minimum players required
        int maxPlayers = 32;             ///< Maximum players allowed
        bool preferNearby = true;        ///< Prefer closer towns
        bool preferPopulated = true;     ///< Prefer towns with more players
    };

    /**
     * @brief Find best matching town
     * @param criteria Search criteria
     * @param callback Called with best matching town (or empty if none found)
     */
    static void FindBestTown(const SearchCriteria& criteria,
                             std::function<void(std::optional<TownInfo>)> callback);

    /**
     * @brief Find all matching towns
     * @param criteria Search criteria
     * @param callback Called with list of matching towns
     */
    static void FindMatchingTowns(const SearchCriteria& criteria,
                                  std::function<void(std::vector<TownInfo>)> callback);

    /**
     * @brief Create or find town for location
     *
     * If a suitable town exists nearby, returns it.
     * Otherwise, creates a new town for the location.
     *
     * @param location GPS coordinates
     * @param callback Called with town info
     */
    static void GetOrCreateTownForLocation(GPSCoordinates location,
                                           std::function<void(TownInfo)> callback);
};

} // namespace Vehement
