#include "MultiplayerSync.hpp"
#include <algorithm>
#include <ctime>
#include <cmath>

// Include engine logger if available
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define SYNC_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define SYNC_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define SYNC_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define SYNC_LOG_INFO(...) std::cout << "[MultiplayerSync] " << __VA_ARGS__ << std::endl
#define SYNC_LOG_WARN(...) std::cerr << "[MultiplayerSync WARN] " << __VA_ARGS__ << std::endl
#define SYNC_LOG_ERROR(...) std::cerr << "[MultiplayerSync ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ==================== NetworkTransform ====================

nlohmann::json NetworkTransform::ToJson() const {
    return {
        {"x", x}, {"y", y}, {"z", z},
        {"rotX", rotX}, {"rotY", rotY}, {"rotZ", rotZ},
        {"velX", velX}, {"velY", velY}, {"velZ", velZ},
        {"timestamp", timestamp}
    };
}

NetworkTransform NetworkTransform::FromJson(const nlohmann::json& j) {
    NetworkTransform t;
    t.x = j.value("x", 0.0f);
    t.y = j.value("y", 0.0f);
    t.z = j.value("z", 0.0f);
    t.rotX = j.value("rotX", 0.0f);
    t.rotY = j.value("rotY", 0.0f);
    t.rotZ = j.value("rotZ", 0.0f);
    t.velX = j.value("velX", 0.0f);
    t.velY = j.value("velY", 0.0f);
    t.velZ = j.value("velZ", 0.0f);
    t.timestamp = j.value("timestamp", static_cast<int64_t>(0));
    return t;
}

NetworkTransform NetworkTransform::Lerp(const NetworkTransform& a,
                                         const NetworkTransform& b,
                                         float t) {
    t = std::clamp(t, 0.0f, 1.0f);

    NetworkTransform result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    result.rotX = a.rotX + (b.rotX - a.rotX) * t;
    result.rotY = a.rotY + (b.rotY - a.rotY) * t;
    result.rotZ = a.rotZ + (b.rotZ - a.rotZ) * t;
    result.velX = a.velX + (b.velX - a.velX) * t;
    result.velY = a.velY + (b.velY - a.velY) * t;
    result.velZ = a.velZ + (b.velZ - a.velZ) * t;
    result.timestamp = static_cast<int64_t>(a.timestamp + (b.timestamp - a.timestamp) * t);
    return result;
}

// ==================== ZombieNetState ====================

nlohmann::json ZombieNetState::ToJson() const {
    return {
        {"id", id},
        {"transform", transform.ToJson()},
        {"health", health},
        {"isDead", isDead},
        {"targetPlayerId", targetPlayerId},
        {"lastDamagedBy", lastDamagedBy},
        {"state", static_cast<int>(state)}
    };
}

ZombieNetState ZombieNetState::FromJson(const nlohmann::json& j) {
    ZombieNetState z;
    z.id = j.value("id", "");
    if (j.contains("transform")) {
        z.transform = NetworkTransform::FromJson(j["transform"]);
    }
    z.health = j.value("health", 100);
    z.isDead = j.value("isDead", false);
    z.targetPlayerId = j.value("targetPlayerId", "");
    z.lastDamagedBy = j.value("lastDamagedBy", "");
    z.state = static_cast<State>(j.value("state", 0));
    return z;
}

// ==================== PlayerNetState ====================

nlohmann::json PlayerNetState::ToJson() const {
    return {
        {"oderId", oderId},
        {"transform", transform.ToJson()},
        {"health", health},
        {"isDead", isDead},
        {"score", score},
        {"currentWeapon", currentWeapon},
        {"isShooting", isShooting},
        {"isReloading", isReloading}
    };
}

PlayerNetState PlayerNetState::FromJson(const nlohmann::json& j) {
    PlayerNetState p;
    p.oderId = j.value("oderId", "");
    if (j.contains("transform")) {
        p.transform = NetworkTransform::FromJson(j["transform"]);
    }
    p.health = j.value("health", 100);
    p.isDead = j.value("isDead", false);
    p.score = j.value("score", 0);
    p.currentWeapon = j.value("currentWeapon", "");
    p.isShooting = j.value("isShooting", false);
    p.isReloading = j.value("isReloading", false);
    return p;
}

// ==================== MapEditEvent ====================

nlohmann::json MapEditEvent::ToJson() const {
    return {
        {"tileX", tileX},
        {"tileY", tileY},
        {"newTile", newTile.ToJson()},
        {"editedBy", editedBy},
        {"timestamp", timestamp}
    };
}

MapEditEvent MapEditEvent::FromJson(const nlohmann::json& j) {
    MapEditEvent e;
    e.tileX = j.value("tileX", 0);
    e.tileY = j.value("tileY", 0);
    if (j.contains("newTile")) {
        e.newTile = Tile::FromJson(j["newTile"]);
    }
    e.editedBy = j.value("editedBy", "");
    e.timestamp = j.value("timestamp", static_cast<int64_t>(0));
    return e;
}

// ==================== GameEvent ====================

nlohmann::json GameEvent::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"sourceId", sourceId},
        {"targetId", targetId},
        {"data", data},
        {"timestamp", timestamp}
    };
}

GameEvent GameEvent::FromJson(const nlohmann::json& j) {
    GameEvent e;
    e.type = static_cast<Type>(j.value("type", 0));
    e.sourceId = j.value("sourceId", "");
    e.targetId = j.value("targetId", "");
    if (j.contains("data")) {
        e.data = j["data"];
    }
    e.timestamp = j.value("timestamp", static_cast<int64_t>(0));
    return e;
}

// ==================== MultiplayerSync ====================

MultiplayerSync& MultiplayerSync::Instance() {
    static MultiplayerSync instance;
    return instance;
}

bool MultiplayerSync::Initialize(const Config& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    auto& firebase = FirebaseManager::Instance();
    if (!firebase.IsInitialized()) {
        SYNC_LOG_WARN("FirebaseManager not initialized");
    }

    // Initialize local player state
    m_localPlayerState.oderId = firebase.GetUserId();
    if (m_localPlayerState.oderId.empty()) {
        m_localPlayerState.oderId = "local_player";
    }

    m_initialized = true;
    SYNC_LOG_INFO("MultiplayerSync initialized");
    return true;
}

void MultiplayerSync::Shutdown() {
    if (!m_initialized) {
        return;
    }

    StopSync();
    m_initialized = false;
    SYNC_LOG_INFO("MultiplayerSync shutdown");
}

void MultiplayerSync::StartSync() {
    if (m_syncing) {
        return;
    }

    auto& matchmaking = Matchmaking::Instance();
    m_currentTownId = matchmaking.GetCurrentTownId();

    if (m_currentTownId.empty()) {
        SYNC_LOG_ERROR("Cannot start sync: not in a town");
        return;
    }

    SetupListeners();
    m_syncing = true;
    SYNC_LOG_INFO("Started multiplayer sync for town: " + m_currentTownId);
}

void MultiplayerSync::StopSync() {
    if (!m_syncing) {
        return;
    }

    RemoveListeners();

    // Clear states
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        m_playerStates.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_zombieMutex);
        m_zombieStates.clear();
    }

    m_syncing = false;
    m_currentTownId.clear();
    SYNC_LOG_INFO("Stopped multiplayer sync");
}

void MultiplayerSync::Update(float deltaTime) {
    if (!m_syncing) {
        return;
    }

    // Update timers
    m_playerSyncTimer += deltaTime;
    m_zombieSyncTimer += deltaTime;
    m_statsTimer += deltaTime;

    // Send player updates
    float playerInterval = 1.0f / m_config.playerSyncRate;
    if (m_playerSyncTimer >= playerInterval) {
        m_playerSyncTimer = 0.0f;
        SendPlayerUpdate();
    }

    // Send zombie updates (host only)
    if (IsHost()) {
        float zombieInterval = 1.0f / m_config.zombieSyncRate;
        if (m_zombieSyncTimer >= zombieInterval) {
            m_zombieSyncTimer = 0.0f;
            SendZombieUpdates();
        }
    }

    // Process remote states for interpolation
    ProcessRemoteStates();

    // Process events
    ProcessEvents();

    // Update stats every second
    if (m_statsTimer >= 1.0f) {
        m_stats.playerUpdatesPerSecond = m_playerUpdates;
        m_stats.zombieUpdatesPerSecond = m_zombieUpdates;
        m_stats.eventsPerSecond = m_eventsProcessed;
        m_stats.averageLatency = m_latency;

        m_playerUpdates = 0;
        m_zombieUpdates = 0;
        m_eventsProcessed = 0;
        m_statsTimer = 0.0f;
    }

    // Update latency estimate periodically
    UpdateLatencyEstimate();
}

// ==================== Player Sync ====================

void MultiplayerSync::SetLocalPlayerState(const PlayerNetState& state) {
    m_localPlayerState = state;
    m_localPlayerState.transform.timestamp = GetServerTime();
}

std::optional<PlayerNetState> MultiplayerSync::GetPlayerState(const std::string& oderId) const {
    std::lock_guard<std::mutex> lock(m_playerMutex);
    auto it = m_playerStates.find(oderId);
    if (it != m_playerStates.end()) {
        return it->second.interpolated;
    }
    return std::nullopt;
}

std::vector<PlayerNetState> MultiplayerSync::GetAllPlayerStates() const {
    std::lock_guard<std::mutex> lock(m_playerMutex);
    std::vector<PlayerNetState> result;
    result.reserve(m_playerStates.size());
    for (const auto& [id, history] : m_playerStates) {
        result.push_back(history.interpolated);
    }
    return result;
}

void MultiplayerSync::OnPlayerStateChanged(PlayerStateCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_playerStateCallbacks.push_back(std::move(callback));
}

// ==================== Zombie Sync ====================

void MultiplayerSync::SetZombieState(const ZombieNetState& state) {
    if (!IsHost()) {
        SYNC_LOG_WARN("Only host can set zombie state");
        return;
    }

    std::lock_guard<std::mutex> lock(m_zombieMutex);
    auto& history = m_zombieStates[state.id];

    ZombieNetState timestampedState = state;
    timestampedState.transform.timestamp = GetServerTime();

    history.states.push_back({timestampedState.transform.timestamp, timestampedState});

    // Trim history
    while (history.states.size() > static_cast<size_t>(m_config.maxInterpolationStates)) {
        history.states.pop_front();
    }

    history.interpolated = timestampedState;
}

std::optional<ZombieNetState> MultiplayerSync::GetZombieState(const std::string& zombieId) const {
    std::lock_guard<std::mutex> lock(m_zombieMutex);
    auto it = m_zombieStates.find(zombieId);
    if (it != m_zombieStates.end()) {
        return it->second.interpolated;
    }
    return std::nullopt;
}

std::vector<ZombieNetState> MultiplayerSync::GetAllZombieStates() const {
    std::lock_guard<std::mutex> lock(m_zombieMutex);
    std::vector<ZombieNetState> result;
    result.reserve(m_zombieStates.size());
    for (const auto& [id, history] : m_zombieStates) {
        result.push_back(history.interpolated);
    }
    return result;
}

std::string MultiplayerSync::SpawnZombie(const ZombieNetState& zombie) {
    if (!IsHost()) {
        SYNC_LOG_WARN("Only host can spawn zombies");
        return "";
    }

    ZombieNetState newZombie = zombie;
    if (newZombie.id.empty()) {
        auto& firebase = FirebaseManager::Instance();
        newZombie.id = "zombie_" + firebase.PushValue(GetZombiesPath(), nlohmann::json::object());
    }

    newZombie.transform.timestamp = GetServerTime();

    // Store locally
    {
        std::lock_guard<std::mutex> lock(m_zombieMutex);
        m_zombieStates[newZombie.id].interpolated = newZombie;
        m_zombieStates[newZombie.id].states.push_back({newZombie.transform.timestamp, newZombie});
    }

    // Send to Firebase
    auto& firebase = FirebaseManager::Instance();
    firebase.SetValue(GetZombiesPath() + "/" + newZombie.id, newZombie.ToJson());

    // Send spawn event
    GameEvent event;
    event.type = GameEvent::Type::ZombieSpawned;
    event.sourceId = newZombie.id;
    event.timestamp = GetServerTime();
    event.data = newZombie.ToJson();
    SendEvent(event);

    SYNC_LOG_INFO("Spawned zombie: " + newZombie.id);
    return newZombie.id;
}

void MultiplayerSync::KillZombie(const std::string& zombieId, const std::string& killedBy) {
    std::lock_guard<std::mutex> lock(m_zombieMutex);
    auto it = m_zombieStates.find(zombieId);
    if (it == m_zombieStates.end()) {
        return;
    }

    it->second.interpolated.isDead = true;
    it->second.interpolated.health = 0;
    it->second.interpolated.state = ZombieNetState::State::Dead;

    // Send death event
    GameEvent event;
    event.type = GameEvent::Type::ZombieDied;
    event.sourceId = zombieId;
    event.targetId = killedBy;
    event.timestamp = GetServerTime();
    SendEvent(event);

    // Update Firebase
    auto& firebase = FirebaseManager::Instance();
    firebase.UpdateValue(GetZombiesPath() + "/" + zombieId, {
        {"isDead", true},
        {"health", 0},
        {"state", static_cast<int>(ZombieNetState::State::Dead)}
    });

    SYNC_LOG_INFO("Zombie killed: " + zombieId + " by " + killedBy);
}

void MultiplayerSync::DamageZombie(const std::string& zombieId, int damage, const std::string& damagedBy) {
    std::lock_guard<std::mutex> lock(m_zombieMutex);
    auto it = m_zombieStates.find(zombieId);
    if (it == m_zombieStates.end() || it->second.interpolated.isDead) {
        return;
    }

    it->second.interpolated.health = std::max(0, it->second.interpolated.health - damage);
    it->second.interpolated.lastDamagedBy = damagedBy;

    // Send damage event
    GameEvent event;
    event.type = GameEvent::Type::DamageDealt;
    event.sourceId = damagedBy;
    event.targetId = zombieId;
    event.data = {{"damage", damage}};
    event.timestamp = GetServerTime();
    SendEvent(event);

    // Check for death
    if (it->second.interpolated.health <= 0) {
        // Release lock before calling KillZombie
        lock.~lock_guard();
        KillZombie(zombieId, damagedBy);
        return;
    }

    // Update Firebase
    auto& firebase = FirebaseManager::Instance();
    firebase.UpdateValue(GetZombiesPath() + "/" + zombieId, {
        {"health", it->second.interpolated.health},
        {"lastDamagedBy", damagedBy}
    });
}

void MultiplayerSync::OnZombieStateChanged(ZombieStateCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_zombieStateCallbacks.push_back(std::move(callback));
}

bool MultiplayerSync::IsHost() const {
    return Matchmaking::Instance().GetLocalPlayer().isHost;
}

// ==================== Map Sync ====================

void MultiplayerSync::SendMapEdit(int x, int y, const Tile& newTile) {
    MapEditEvent edit;
    edit.tileX = x;
    edit.tileY = y;
    edit.newTile = newTile;
    edit.editedBy = m_localPlayerState.oderId;
    edit.timestamp = GetServerTime();

    // Queue edit
    {
        std::lock_guard<std::mutex> lock(m_mapEditMutex);
        m_pendingMapEdits.push_back(edit);
    }

    // Send to Firebase
    auto& firebase = FirebaseManager::Instance();
    std::string editKey = std::to_string(x) + "_" + std::to_string(y);
    firebase.SetValue(GetMapEditsPath() + "/" + editKey, edit.ToJson());

    // Also update TownServer
    TownServer::Instance().SaveTileChange(x, y, newTile);
}

void MultiplayerSync::OnMapEdited(MapEditCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_mapEditCallbacks.push_back(std::move(callback));
}

// ==================== Game Events ====================

void MultiplayerSync::SendEvent(const GameEvent& event) {
    // Queue locally
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_pendingEvents.push_back(event);
    }

    // Send to Firebase
    auto& firebase = FirebaseManager::Instance();
    firebase.PushValue(GetEventsPath(), event.ToJson());
}

void MultiplayerSync::OnGameEvent(GameEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_gameEventCallbacks.push_back(std::move(callback));
}

// ==================== Latency & Stats ====================

int64_t MultiplayerSync::GetServerTime() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return timestamp + m_serverTimeOffset;
}

MultiplayerSync::SyncStats MultiplayerSync::GetStats() const {
    return m_stats;
}

// ==================== Private Helpers ====================

std::string MultiplayerSync::GetPlayersPath() const {
    return "towns/" + m_currentTownId + "/sync/players";
}

std::string MultiplayerSync::GetZombiesPath() const {
    return "towns/" + m_currentTownId + "/sync/zombies";
}

std::string MultiplayerSync::GetEventsPath() const {
    return "towns/" + m_currentTownId + "/sync/events";
}

std::string MultiplayerSync::GetMapEditsPath() const {
    return "towns/" + m_currentTownId + "/sync/mapEdits";
}

void MultiplayerSync::SetupListeners() {
    auto& firebase = FirebaseManager::Instance();

    // Listen for player updates
    m_playersListenerId = firebase.ListenToPath(GetPlayersPath(),
        [this](const nlohmann::json& data) {
            if (data.is_object()) {
                for (auto& [id, playerData] : data.items()) {
                    if (id != m_localPlayerState.oderId) {
                        HandlePlayerUpdate(id, playerData);
                    }
                }
            }
        });

    // Listen for zombie updates (if not host)
    if (!IsHost()) {
        m_zombiesListenerId = firebase.ListenToPath(GetZombiesPath(),
            [this](const nlohmann::json& data) {
                if (data.is_object()) {
                    for (auto& [id, zombieData] : data.items()) {
                        HandleZombieUpdate(id, zombieData);
                    }
                }
            });
    }

    // Listen for map edits
    m_mapEditsListenerId = firebase.ListenToPath(GetMapEditsPath(),
        [this](const nlohmann::json& data) {
            if (data.is_object()) {
                for (auto& [key, editData] : data.items()) {
                    HandleMapEdit(editData);
                }
            }
        });

    // Listen for game events
    m_eventsListenerId = firebase.ListenToPath(GetEventsPath(),
        [this](const nlohmann::json& data) {
            if (data.is_object()) {
                for (auto& [key, eventData] : data.items()) {
                    HandleGameEvent(eventData);
                }
            }
        });
}

void MultiplayerSync::RemoveListeners() {
    auto& firebase = FirebaseManager::Instance();

    if (!m_playersListenerId.empty()) {
        firebase.StopListeningById(m_playersListenerId);
        m_playersListenerId.clear();
    }
    if (!m_zombiesListenerId.empty()) {
        firebase.StopListeningById(m_zombiesListenerId);
        m_zombiesListenerId.clear();
    }
    if (!m_mapEditsListenerId.empty()) {
        firebase.StopListeningById(m_mapEditsListenerId);
        m_mapEditsListenerId.clear();
    }
    if (!m_eventsListenerId.empty()) {
        firebase.StopListeningById(m_eventsListenerId);
        m_eventsListenerId.clear();
    }
}

void MultiplayerSync::SendPlayerUpdate() {
    auto& firebase = FirebaseManager::Instance();

    m_localPlayerState.transform.timestamp = GetServerTime();
    firebase.SetValue(GetPlayersPath() + "/" + m_localPlayerState.oderId,
                      m_localPlayerState.ToJson());

    m_playerUpdates++;
}

void MultiplayerSync::SendZombieUpdates() {
    if (!IsHost()) {
        return;
    }

    auto& firebase = FirebaseManager::Instance();

    std::lock_guard<std::mutex> lock(m_zombieMutex);
    for (auto& [id, history] : m_zombieStates) {
        if (!history.interpolated.isDead) {
            history.interpolated.transform.timestamp = GetServerTime();
            firebase.SetValue(GetZombiesPath() + "/" + id, history.interpolated.ToJson());
            m_zombieUpdates++;
        }
    }
}

void MultiplayerSync::ProcessRemoteStates() {
    int64_t interpolationTime = GetServerTime() -
        static_cast<int64_t>(m_config.interpolationDelay * 1000);

    // Interpolate player states
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        for (auto& [id, history] : m_playerStates) {
            if (history.states.size() >= 2) {
                // Find surrounding states
                auto it = std::lower_bound(history.states.begin(), history.states.end(),
                    interpolationTime,
                    [](const auto& a, int64_t b) { return a.first < b; });

                if (it != history.states.begin() && it != history.states.end()) {
                    auto prev = std::prev(it);
                    float t = static_cast<float>(interpolationTime - prev->first) /
                              static_cast<float>(it->first - prev->first);

                    // Lerp transform
                    history.interpolated.transform = NetworkTransform::Lerp(
                        prev->second.transform, it->second.transform, t);

                    // Use latest for non-interpolatable values
                    history.interpolated.health = it->second.health;
                    history.interpolated.isDead = it->second.isDead;
                    history.interpolated.score = it->second.score;
                    history.interpolated.currentWeapon = it->second.currentWeapon;
                    history.interpolated.isShooting = it->second.isShooting;
                    history.interpolated.isReloading = it->second.isReloading;
                }
            }
        }
    }

    // Interpolate zombie states (non-host only)
    if (!IsHost()) {
        std::lock_guard<std::mutex> lock(m_zombieMutex);
        for (auto& [id, history] : m_zombieStates) {
            if (history.states.size() >= 2) {
                auto it = std::lower_bound(history.states.begin(), history.states.end(),
                    interpolationTime,
                    [](const auto& a, int64_t b) { return a.first < b; });

                if (it != history.states.begin() && it != history.states.end()) {
                    auto prev = std::prev(it);
                    float t = static_cast<float>(interpolationTime - prev->first) /
                              static_cast<float>(it->first - prev->first);

                    history.interpolated.transform = NetworkTransform::Lerp(
                        prev->second.transform, it->second.transform, t);
                    history.interpolated.health = it->second.health;
                    history.interpolated.isDead = it->second.isDead;
                    history.interpolated.state = it->second.state;
                    history.interpolated.targetPlayerId = it->second.targetPlayerId;
                }
            }
        }
    }
}

void MultiplayerSync::ProcessEvents() {
    std::deque<GameEvent> eventsToProcess;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        std::swap(eventsToProcess, m_pendingEvents);
    }

    for (const auto& event : eventsToProcess) {
        m_eventsProcessed++;

        // Notify callbacks
        for (const auto& cb : m_gameEventCallbacks) {
            if (cb) {
                cb(event);
            }
        }
    }

    // Process map edits
    std::deque<MapEditEvent> editsToProcess;
    {
        std::lock_guard<std::mutex> lock(m_mapEditMutex);
        std::swap(editsToProcess, m_pendingMapEdits);
    }

    for (const auto& edit : editsToProcess) {
        // Notify callbacks
        for (const auto& cb : m_mapEditCallbacks) {
            if (cb) {
                cb(edit);
            }
        }
    }
}

void MultiplayerSync::HandlePlayerUpdate(const std::string& oderId, const nlohmann::json& data) {
    PlayerNetState state = PlayerNetState::FromJson(data);

    std::lock_guard<std::mutex> lock(m_playerMutex);
    auto& history = m_playerStates[oderId];

    history.states.push_back({state.transform.timestamp, state});

    // Trim history
    while (history.states.size() > static_cast<size_t>(m_config.maxInterpolationStates)) {
        history.states.pop_front();
    }

    // Update interpolated immediately if this is first state
    if (history.states.size() == 1) {
        history.interpolated = state;
    }

    // Notify callbacks
    for (const auto& cb : m_playerStateCallbacks) {
        if (cb) {
            cb(state);
        }
    }
}

void MultiplayerSync::HandleZombieUpdate(const std::string& zombieId, const nlohmann::json& data) {
    ZombieNetState state = ZombieNetState::FromJson(data);

    std::lock_guard<std::mutex> lock(m_zombieMutex);
    auto& history = m_zombieStates[zombieId];

    history.states.push_back({state.transform.timestamp, state});

    while (history.states.size() > static_cast<size_t>(m_config.maxInterpolationStates)) {
        history.states.pop_front();
    }

    if (history.states.size() == 1) {
        history.interpolated = state;
    }

    for (const auto& cb : m_zombieStateCallbacks) {
        if (cb) {
            cb(state);
        }
    }
}

void MultiplayerSync::HandleMapEdit(const nlohmann::json& data) {
    MapEditEvent edit = MapEditEvent::FromJson(data);

    // Don't process our own edits
    if (edit.editedBy == m_localPlayerState.oderId) {
        return;
    }

    // Queue for processing
    {
        std::lock_guard<std::mutex> lock(m_mapEditMutex);
        m_pendingMapEdits.push_back(edit);
    }

    // Apply to TownServer map
    TownServer::Instance().GetTownMap().SetTile(edit.tileX, edit.tileY, edit.newTile);
}

void MultiplayerSync::HandleGameEvent(const nlohmann::json& data) {
    GameEvent event = GameEvent::FromJson(data);

    // Don't process events we sent
    if (event.sourceId == m_localPlayerState.oderId) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_pendingEvents.push_back(event);
}

void MultiplayerSync::UpdateLatencyEstimate() {
    // In a real implementation, this would ping the server
    // For the stub, we simulate low latency
    m_latency = 50.0f; // 50ms simulated latency
    m_serverTimeOffset = 0; // No offset in stub mode
}

// ==================== ConflictResolver ====================

ZombieNetState ConflictResolver::ResolveZombieConflict(const ZombieNetState& local,
                                                        const ZombieNetState& remote,
                                                        Strategy strategy) {
    switch (strategy) {
        case Strategy::FirstWins:
            return (local.transform.timestamp <= remote.transform.timestamp) ? local : remote;

        case Strategy::LastWins:
            return (local.transform.timestamp >= remote.transform.timestamp) ? local : remote;

        case Strategy::HostWins:
            // Assume remote is from host if we're not host
            return Matchmaking::Instance().GetLocalPlayer().isHost ? local : remote;

        case Strategy::HighestHealth:
            // Lower health wins (damage was applied)
            return (local.health <= remote.health) ? local : remote;

        case Strategy::Merge: {
            ZombieNetState merged = local;
            // Use most recent transform
            if (remote.transform.timestamp > local.transform.timestamp) {
                merged.transform = remote.transform;
            }
            // Use lowest health (accept damage from either)
            merged.health = std::min(local.health, remote.health);
            merged.isDead = local.isDead || remote.isDead;
            return merged;
        }

        default:
            return remote;
    }
}

MapEditEvent ConflictResolver::ResolveMapEditConflict(const MapEditEvent& local,
                                                       const MapEditEvent& remote,
                                                       Strategy strategy) {
    switch (strategy) {
        case Strategy::FirstWins:
            return (local.timestamp <= remote.timestamp) ? local : remote;

        case Strategy::LastWins:
            return (local.timestamp >= remote.timestamp) ? local : remote;

        case Strategy::HostWins:
            return Matchmaking::Instance().GetLocalPlayer().isHost ? local : remote;

        default:
            return (local.timestamp >= remote.timestamp) ? local : remote;
    }
}

} // namespace Vehement
