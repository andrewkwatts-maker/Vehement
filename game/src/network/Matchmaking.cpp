#include "Matchmaking.hpp"
#include <algorithm>
#include <ctime>

// Include engine logger if available
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define MATCH_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define MATCH_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define MATCH_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define MATCH_LOG_INFO(...) std::cout << "[Matchmaking] " << __VA_ARGS__ << std::endl
#define MATCH_LOG_WARN(...) std::cerr << "[Matchmaking WARN] " << __VA_ARGS__ << std::endl
#define MATCH_LOG_ERROR(...) std::cerr << "[Matchmaking ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ==================== PlayerInfo ====================

nlohmann::json PlayerInfo::ToJson() const {
    return {
        {"oderId", oderId},
        {"displayName", displayName},
        {"townId", townId},
        {"x", x},
        {"y", y},
        {"z", z},
        {"rotation", rotation},
        {"status", static_cast<int>(status)},
        {"lastSeen", lastSeen},
        {"level", level},
        {"zombiesKilled", zombiesKilled},
        {"isHost", isHost}
    };
}

PlayerInfo PlayerInfo::FromJson(const nlohmann::json& j) {
    PlayerInfo info;
    info.oderId = j.value("oderId", "");
    info.displayName = j.value("displayName", "Player");
    info.townId = j.value("townId", "");
    info.x = j.value("x", 0.0f);
    info.y = j.value("y", 0.0f);
    info.z = j.value("z", 0.0f);
    info.rotation = j.value("rotation", 0.0f);
    info.status = static_cast<Status>(j.value("status", 0));
    info.lastSeen = j.value("lastSeen", static_cast<int64_t>(0));
    info.level = j.value("level", 1);
    info.zombiesKilled = j.value("zombiesKilled", 0);
    info.isHost = j.value("isHost", false);
    return info;
}

// ==================== Matchmaking ====================

Matchmaking& Matchmaking::Instance() {
    static Matchmaking instance;
    return instance;
}

bool Matchmaking::Initialize(const Config& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) {
        MATCH_LOG_WARN("FirebaseManager not initialized, matchmaking may not work properly");
    }

    // Initialize local player with Firebase user ID
    m_localPlayer.oderId = firebase.GetUserId();
    if (m_localPlayer.oderId.empty()) {
        m_localPlayer.oderId = "local_player";
    }
    m_localPlayer.displayName = "Player";
    m_localPlayer.status = PlayerInfo::Status::Online;
    m_localPlayer.lastSeen = std::time(nullptr);

    m_initialized = true;
    MATCH_LOG_INFO("Matchmaking initialized");
    return true;
}

void Matchmaking::Shutdown() {
    if (!m_initialized) {
        return;
    }

    LeaveTown();
    m_initialized = false;
    MATCH_LOG_INFO("Matchmaking shutdown");
}

// ==================== Town Operations ====================

void Matchmaking::JoinTown(const TownInfo& town, std::function<void(bool)> callback) {
    if (!m_initialized) {
        if (callback) {
            callback(false);
        }
        return;
    }

    // Leave current town first
    if (!m_currentTownId.empty()) {
        LeaveTown();
    }

    m_currentTownId = town.townId;
    m_localPlayer.townId = town.townId;
    m_localPlayer.lastSeen = std::time(nullptr);

    // Check if town has space
    GetTownPlayerCount(town.townId, [this, callback](int count) {
        if (count >= m_config.maxPlayersPerTown) {
            MATCH_LOG_WARN("Town is full: " + m_currentTownId);
            m_currentTownId.clear();
            m_localPlayer.townId.clear();
            if (callback) {
                callback(false);
            }
            return;
        }

        // Register presence
        RegisterPresence();

        // Setup listeners for other players
        SetupListeners();

        // Determine if we should be host (first player or existing host left)
        m_localPlayer.isHost = (count == 0);

        MATCH_LOG_INFO("Joined town: " + m_currentTownId + " (players: " + std::to_string(count + 1) + ")");

        if (callback) {
            callback(true);
        }
    });
}

void Matchmaking::LeaveTown() {
    if (m_currentTownId.empty()) {
        return;
    }

    MATCH_LOG_INFO("Leaving town: " + m_currentTownId);

    RemoveListeners();
    UnregisterPresence();

    // Clear players list
    {
        std::lock_guard<std::mutex> lock(m_playersMutex);
        m_players.clear();
    }

    m_currentTownId.clear();
    m_localPlayer.townId.clear();
    m_localPlayer.isHost = false;
}

// ==================== Player Information ====================

std::vector<PlayerInfo> Matchmaking::GetPlayersInTown() const {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    std::vector<PlayerInfo> result;
    result.reserve(m_players.size() + 1);

    // Add local player
    result.push_back(m_localPlayer);

    // Add other players
    for (const auto& [id, player] : m_players) {
        if (id != m_localPlayer.oderId) {
            result.push_back(player);
        }
    }

    return result;
}

int Matchmaking::GetPlayerCount() const {
    std::lock_guard<std::mutex> lock(m_playersMutex);
    // Count includes local player
    return static_cast<int>(m_players.size()) + 1;
}

std::optional<PlayerInfo> Matchmaking::GetPlayer(const std::string& oderId) const {
    if (oderId == m_localPlayer.oderId) {
        return m_localPlayer;
    }

    std::lock_guard<std::mutex> lock(m_playersMutex);
    auto it = m_players.find(oderId);
    if (it != m_players.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Matchmaking::UpdateLocalPlayer(const PlayerInfo& info) {
    m_localPlayer = info;
    m_localPlayer.lastSeen = std::time(nullptr);

    // Update in Firebase
    if (!m_currentTownId.empty()) {
        auto& firebase = FirebaseManager::Instance();
        firebase.SetValue(GetPlayerPath(m_localPlayer.oderId), m_localPlayer.ToJson());
    }
}

void Matchmaking::SetDisplayName(const std::string& name) {
    m_localPlayer.displayName = name;
    UpdateLocalPlayer(m_localPlayer);
}

void Matchmaking::UpdatePosition(float x, float y, float z, float rotation) {
    m_localPlayer.x = x;
    m_localPlayer.y = y;
    m_localPlayer.z = z;
    m_localPlayer.rotation = rotation;
    // Position updates are batched with heartbeat to reduce Firebase writes
}

// ==================== Player Events ====================

void Matchmaking::OnPlayerJoined(PlayerCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_playerJoinedCallbacks.push_back(std::move(callback));
}

void Matchmaking::OnPlayerLeft(PlayerLeftCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_playerLeftCallbacks.push_back(std::move(callback));
}

void Matchmaking::OnPlayerUpdated(PlayerCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_playerUpdatedCallbacks.push_back(std::move(callback));
}

// ==================== Town Discovery ====================

void Matchmaking::FindNearbyTowns(GPSCoordinates location, float radiusKm,
                                   std::function<void(const std::vector<std::pair<TownInfo, int>>&)> callback) {
    if (!callback) {
        return;
    }

    auto& firebase = FirebaseManager::Instance();

    // Query towns collection
    firebase.GetValue("towns", [location, radiusKm, callback](const nlohmann::json& data) {
        std::vector<std::pair<TownInfo, int>> results;

        if (data.is_object()) {
            for (auto& [townId, townData] : data.items()) {
                if (townData.contains("metadata")) {
                    auto& meta = townData["metadata"];

                    TownInfo town;
                    town.townId = townId;
                    town.townName = meta.value("townName", "");
                    town.country = meta.value("country", "");
                    town.countryCode = meta.value("countryCode", "");
                    town.center.latitude = meta.value("latitude", 0.0);
                    town.center.longitude = meta.value("longitude", 0.0);
                    town.radiusKm = meta.value("radiusKm", 5.0f);

                    // Check distance
                    double distance = location.DistanceTo(town.center);
                    if (distance <= radiusKm) {
                        // Count players
                        int playerCount = 0;
                        if (townData.contains("players") && townData["players"].is_object()) {
                            playerCount = static_cast<int>(townData["players"].size());
                        }

                        results.emplace_back(town, playerCount);
                    }
                }
            }
        }

        // Sort by player count (descending)
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        callback(results);
    });
}

void Matchmaking::GetTownPlayerCount(const std::string& townId,
                                      std::function<void(int)> callback) {
    if (!callback) {
        return;
    }

    auto& firebase = FirebaseManager::Instance();
    std::string path = "towns/" + townId + "/players";

    firebase.GetValue(path, [callback](const nlohmann::json& data) {
        int count = 0;
        if (data.is_object()) {
            count = static_cast<int>(data.size());
        }
        callback(count);
    });
}

// ==================== Update ====================

void Matchmaking::Update(float deltaTime) {
    if (!m_initialized || m_currentTownId.empty()) {
        return;
    }

    // Heartbeat
    m_heartbeatTimer += deltaTime;
    float heartbeatInterval = static_cast<float>(m_config.heartbeatIntervalMs) / 1000.0f;
    if (m_heartbeatTimer >= heartbeatInterval) {
        m_heartbeatTimer = 0.0f;
        SendHeartbeat();
    }

    // Check for player timeouts
    m_timeoutCheckTimer += deltaTime;
    if (m_timeoutCheckTimer >= TIMEOUT_CHECK_INTERVAL) {
        m_timeoutCheckTimer = 0.0f;
        CheckPlayerTimeouts();
    }
}

// ==================== Private Helpers ====================

std::string Matchmaking::GetTownPlayersPath() const {
    return "towns/" + m_currentTownId + "/players";
}

std::string Matchmaking::GetPlayerPath(const std::string& oderId) const {
    return GetTownPlayersPath() + "/" + oderId;
}

std::string Matchmaking::GetPresencePath(const std::string& oderId) const {
    return "presence/" + oderId;
}

void Matchmaking::RegisterPresence() {
    auto& firebase = FirebaseManager::Instance();

    m_localPlayer.lastSeen = std::time(nullptr);
    m_localPlayer.status = PlayerInfo::Status::Online;

    // Register in town players list
    firebase.SetValue(GetPlayerPath(m_localPlayer.oderId), m_localPlayer.ToJson());

    // Set up presence tracking
    nlohmann::json presenceData = {
        {"online", true},
        {"townId", m_currentTownId},
        {"lastSeen", m_localPlayer.lastSeen}
    };
    firebase.SetValue(GetPresencePath(m_localPlayer.oderId), presenceData);

    MATCH_LOG_INFO("Registered presence for player: " + m_localPlayer.oderId);
}

void Matchmaking::UnregisterPresence() {
    auto& firebase = FirebaseManager::Instance();

    // Remove from town players list
    firebase.DeleteValue(GetPlayerPath(m_localPlayer.oderId));

    // Update presence to offline
    nlohmann::json presenceData = {
        {"online", false},
        {"townId", ""},
        {"lastSeen", std::time(nullptr)}
    };
    firebase.SetValue(GetPresencePath(m_localPlayer.oderId), presenceData);

    MATCH_LOG_INFO("Unregistered presence for player: " + m_localPlayer.oderId);
}

void Matchmaking::SendHeartbeat() {
    auto& firebase = FirebaseManager::Instance();

    m_localPlayer.lastSeen = std::time(nullptr);

    // Update player data with current position and timestamp
    firebase.UpdateValue(GetPlayerPath(m_localPlayer.oderId), {
        {"x", m_localPlayer.x},
        {"y", m_localPlayer.y},
        {"z", m_localPlayer.z},
        {"rotation", m_localPlayer.rotation},
        {"lastSeen", m_localPlayer.lastSeen},
        {"status", static_cast<int>(m_localPlayer.status)}
    });

    // Update presence
    firebase.UpdateValue(GetPresencePath(m_localPlayer.oderId), {
        {"online", true},
        {"lastSeen", m_localPlayer.lastSeen}
    });
}

void Matchmaking::SetupListeners() {
    auto& firebase = FirebaseManager::Instance();

    m_playersListenerId = firebase.ListenToPath(GetTownPlayersPath(),
        [this](const nlohmann::json& data) {
            HandlePlayersUpdate(data);
        });
}

void Matchmaking::RemoveListeners() {
    if (!m_playersListenerId.empty()) {
        auto& firebase = FirebaseManager::Instance();
        firebase.StopListeningById(m_playersListenerId);
        m_playersListenerId.clear();
    }
}

void Matchmaking::HandlePlayersUpdate(const nlohmann::json& data) {
    if (!data.is_object()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_playersMutex);

    // Track which players we've seen
    std::unordered_map<std::string, bool> seenPlayers;

    // Process player data
    for (auto& [oderId, playerData] : data.items()) {
        // Skip local player
        if (oderId == m_localPlayer.oderId) {
            continue;
        }

        seenPlayers[oderId] = true;

        PlayerInfo player = PlayerInfo::FromJson(playerData);
        bool isNew = m_players.find(oderId) == m_players.end();

        m_players[oderId] = player;

        // Notify callbacks
        if (isNew) {
            MATCH_LOG_INFO("Player joined: " + player.displayName + " (" + oderId + ")");
            for (const auto& cb : m_playerJoinedCallbacks) {
                if (cb) {
                    cb(player);
                }
            }
        } else {
            for (const auto& cb : m_playerUpdatedCallbacks) {
                if (cb) {
                    cb(player);
                }
            }
        }
    }

    // Find and remove players who left
    std::vector<std::string> leftPlayers;
    for (const auto& [oderId, player] : m_players) {
        if (seenPlayers.find(oderId) == seenPlayers.end()) {
            leftPlayers.push_back(oderId);
        }
    }

    for (const auto& oderId : leftPlayers) {
        MATCH_LOG_INFO("Player left: " + oderId);
        m_players.erase(oderId);

        for (const auto& cb : m_playerLeftCallbacks) {
            if (cb) {
                cb(oderId);
            }
        }
    }
}

void Matchmaking::CheckPlayerTimeouts() {
    int64_t now = std::time(nullptr);
    int64_t timeoutThreshold = now - (m_config.offlineTimeoutMs / 1000);

    std::lock_guard<std::mutex> lock(m_playersMutex);

    std::vector<std::string> timedOutPlayers;

    for (auto& [oderId, player] : m_players) {
        if (player.lastSeen < timeoutThreshold && player.status == PlayerInfo::Status::Online) {
            player.status = PlayerInfo::Status::Offline;
            timedOutPlayers.push_back(oderId);
        }
    }

    for (const auto& oderId : timedOutPlayers) {
        MATCH_LOG_INFO("Player timed out: " + oderId);
        for (const auto& cb : m_playerLeftCallbacks) {
            if (cb) {
                cb(oderId);
            }
        }
    }
}

// ==================== TownFinder ====================

void TownFinder::FindBestTown(const SearchCriteria& criteria,
                               std::function<void(std::optional<TownInfo>)> callback) {
    if (!callback) {
        return;
    }

    FindMatchingTowns(criteria, [criteria, callback](std::vector<TownInfo> towns) {
        if (towns.empty()) {
            callback(std::nullopt);
            return;
        }

        // Score and sort towns
        auto& matchmaking = Matchmaking::Instance();
        std::vector<std::pair<TownInfo, float>> scoredTowns;

        for (const auto& town : towns) {
            float score = 0.0f;

            // Distance score (closer is better)
            if (criteria.preferNearby) {
                double distance = criteria.location.DistanceTo(town.center);
                score += static_cast<float>((criteria.maxRadiusKm - distance) / criteria.maxRadiusKm * 50.0);
            }

            scoredTowns.emplace_back(town, score);
        }

        // Sort by score
        std::sort(scoredTowns.begin(), scoredTowns.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        callback(scoredTowns.front().first);
    });
}

void TownFinder::FindMatchingTowns(const SearchCriteria& criteria,
                                    std::function<void(std::vector<TownInfo>)> callback) {
    if (!callback) {
        return;
    }

    auto& matchmaking = Matchmaking::Instance();

    matchmaking.FindNearbyTowns(criteria.location, criteria.maxRadiusKm,
        [criteria, callback](const std::vector<std::pair<TownInfo, int>>& townPlayers) {
            std::vector<TownInfo> result;

            for (const auto& [town, playerCount] : townPlayers) {
                if (playerCount >= criteria.minPlayers && playerCount < criteria.maxPlayers) {
                    result.push_back(town);
                }
            }

            callback(result);
        });
}

void TownFinder::GetOrCreateTownForLocation(GPSCoordinates location,
                                             std::function<void(TownInfo)> callback) {
    if (!callback) {
        return;
    }

    auto& gps = GPSLocation::Instance();

    // First, get town info from coordinates
    gps.GetTownFromCoordinates(location, [callback](TownInfo town) {
        // Check if town already exists in Firebase
        auto& firebase = FirebaseManager::Instance();
        firebase.GetValue("towns/" + town.townId + "/metadata", [town, callback](const nlohmann::json& data) {
            if (!data.is_null()) {
                // Town exists, return it
                TownInfo existingTown = town;
                existingTown.townName = data.value("townName", town.townName);
                existingTown.country = data.value("country", town.country);
                existingTown.radiusKm = data.value("radiusKm", town.radiusKm);
                callback(existingTown);
            } else {
                // Create new town
                nlohmann::json metadata = {
                    {"townId", town.townId},
                    {"townName", town.townName},
                    {"country", town.country},
                    {"countryCode", town.countryCode},
                    {"latitude", town.center.latitude},
                    {"longitude", town.center.longitude},
                    {"radiusKm", town.radiusKm},
                    {"createdAt", std::time(nullptr)}
                };

                FirebaseManager::Instance().SetValue("towns/" + town.townId + "/metadata", metadata);

                MATCH_LOG_INFO("Created new town: " + town.townId);
                callback(town);
            }
        });
    });
}

} // namespace Vehement
