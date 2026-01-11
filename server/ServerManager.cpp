#include "ServerManager.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace Nova {

static uint64_t GetTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

ServerManager::ServerManager() = default;

ServerManager::~ServerManager() {
    Shutdown();
}

bool ServerManager::Initialize(const std::string& configPath, const std::string& worldPath) {
    if (m_initialized) {
        LogError("Server already initialized");
        return false;
    }

    m_configPath = configPath;
    m_worldPath = worldPath;

    // Load configuration
    if (!m_config.LoadFromFile(configPath)) {
        LogMessage("Config file not found, using defaults");
        m_config.SaveToFile(configPath);
    }

    std::string errorMessage;
    if (!m_config.Validate(errorMessage)) {
        LogError("Invalid configuration: " + errorMessage);
        return false;
    }

    // Initialize world database
    m_worldDatabase = std::make_unique<WorldDatabase>();
    if (!m_worldDatabase->Initialize(worldPath)) {
        LogError("Failed to initialize world database");
        return false;
    }

    // Initialize player database
    m_playerDatabase = std::make_unique<PlayerDatabase>(m_worldDatabase.get());

    // Initialize chunk streamer
    m_chunkStreamer = std::make_unique<ChunkStreamer>();
    if (!m_chunkStreamer->Initialize(m_worldDatabase.get(), m_config.chunkLoadThreads)) {
        LogError("Failed to initialize chunk streamer");
        return false;
    }

    // Configure chunk streamer
    m_chunkStreamer->SetAutoSaveEnabled(m_config.autoSaveEnabled);
    m_chunkStreamer->SetAutoSaveInterval(static_cast<float>(m_config.autoSaveInterval));

    m_initialized = true;
    m_status = ServerStatus::Stopped;

    LogMessage("Server initialized successfully");
    return true;
}

void ServerManager::Shutdown() {
    if (!m_initialized) return;

    Stop();

    // Save everything
    if (m_worldDatabase && m_worldDatabase->GetCurrentWorldId() >= 0) {
        LogMessage("Saving world...");
        SaveWorld();
    }

    // Shutdown subsystems
    if (m_chunkStreamer) {
        m_chunkStreamer->Shutdown();
    }

    if (m_worldDatabase) {
        m_worldDatabase->Shutdown();
    }

    m_initialized = false;
    LogMessage("Server shut down");
}

bool ServerManager::Start() {
    if (!m_initialized) {
        LogError("Server not initialized");
        return false;
    }

    if (m_status == ServerStatus::Running) {
        LogMessage("Server already running");
        return true;
    }

    m_status = ServerStatus::Starting;
    LogMessage("Starting server: " + m_config.serverName);

    // Load world if not loaded
    if (m_worldDatabase->GetCurrentWorldId() < 0) {
        if (!LoadWorld(m_config.serverName + "_world")) {
            // Create new world if it doesn't exist
            int seed = static_cast<int>(GetTimestamp());
            if (!CreateWorld(m_config.serverName + "_world", seed)) {
                LogError("Failed to create/load world");
                m_status = ServerStatus::Error;
                return false;
            }
        }
    }

    m_running = true;
    m_startTime = GetTimestamp();
    m_status = ServerStatus::Running;

    LogMessage("Server started successfully");
    LogMessage("Players: 0/" + std::to_string(m_config.maxPlayers));
    LogMessage("Listening on " + m_config.serverIP + ":" + std::to_string(m_config.serverPort));

    return true;
}

void ServerManager::Stop() {
    if (m_status != ServerStatus::Running) return;

    m_status = ServerStatus::Stopping;
    LogMessage("Stopping server...");

    m_running = false;

    // Wait for server thread if exists
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
    }

    // Kick all players
    auto onlinePlayers = GetOnlinePlayers();
    for (const auto& player : onlinePlayers) {
        PlayerLeave(player.playerId);
    }

    m_status = ServerStatus::Stopped;
    LogMessage("Server stopped");
}

bool ServerManager::Restart() {
    LogMessage("Restarting server...");
    Stop();
    return Start();
}

void ServerManager::Update(float deltaTime) {
    if (m_status != ServerStatus::Running) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Update chunk streamer
    if (m_chunkStreamer) {
        m_chunkStreamer->Update(deltaTime);
    }

    // Process auto-save
    if (m_config.autoSaveEnabled) {
        ProcessAutoSave(deltaTime);
    }

    // Process backup
    if (m_config.backupEnabled) {
        ProcessBackup(deltaTime);
    }

    // Update statistics
    UpdatePerformanceStats(deltaTime);

    auto endTime = std::chrono::high_resolution_clock::now();
    float tickTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_totalTickTime += tickTime;
    m_totalTicks++;
    m_stats.avgTickTime = m_totalTickTime / static_cast<float>(m_totalTicks);
}

int ServerManager::GetTickRate() const {
    return m_tickRate;
}

void ServerManager::SetTickRate(int ticksPerSecond) {
    m_tickRate = std::clamp(ticksPerSecond, 1, 120);
    m_tickInterval = 1.0f / static_cast<float>(m_tickRate);
}

int ServerManager::PlayerJoin(const std::string& username, const std::string& password) {
    if (!m_initialized || m_status != ServerStatus::Running) {
        return -1;
    }

    // Check blacklist
    if (m_config.IsBlacklisted(username)) {
        LogMessage("Blocked blacklisted user: " + username);
        return -1;
    }

    // Check whitelist
    if (m_config.whitelistEnabled && !m_config.IsWhitelisted(username)) {
        LogMessage("User not whitelisted: " + username);
        return -1;
    }

    // Check max players
    if (GetOnlinePlayerCount() >= static_cast<size_t>(m_config.maxPlayers)) {
        LogMessage("Server full, rejected: " + username);
        return -1;
    }

    // Authenticate or create player
    int playerId = m_playerDatabase->AuthenticatePlayer(username, password);
    if (playerId < 0) {
        // Try to create new player
        playerId = m_playerDatabase->CreatePlayer(username, password);
        if (playerId < 0) {
            LogError("Failed to authenticate/create player: " + username);
            return -1;
        }
    }

    // Set player online
    m_playerDatabase->SetPlayerOnline(playerId, true);

    // Add view position to chunk streamer
    Player player = m_playerDatabase->GetPlayerById(playerId);
    if (player.entityId >= 0) {
        Entity entity = m_worldDatabase->LoadEntity(player.entityId);
        m_chunkStreamer->AddViewPosition(playerId, entity.position);
    }

    LogMessage("Player joined: " + username + " (" + std::to_string(playerId) + ")");

    if (OnPlayerJoin) {
        OnPlayerJoin(playerId, username);
    }

    return playerId;
}

void ServerManager::PlayerLeave(int playerId) {
    if (!m_initialized) return;

    Player player = m_playerDatabase->GetPlayerById(playerId);
    if (player.playerId < 0) return;

    // Save player state
    m_playerDatabase->UpdatePlayer(player);

    // Set player offline
    m_playerDatabase->SetPlayerOnline(playerId, false);

    // Remove view position
    m_chunkStreamer->RemoveViewPosition(playerId);

    LogMessage("Player left: " + player.username + " (" + std::to_string(playerId) + ")");

    if (OnPlayerLeave) {
        OnPlayerLeave(playerId, player.username);
    }
}

size_t ServerManager::GetOnlinePlayerCount() const {
    if (!m_playerDatabase) return 0;
    return m_playerDatabase->GetOnlinePlayers().size();
}

std::vector<Player> ServerManager::GetOnlinePlayers() const {
    if (!m_playerDatabase) return {};
    return m_playerDatabase->GetOnlinePlayers();
}

bool ServerManager::KickPlayer(int playerId, const std::string& reason) {
    LogMessage("Kicking player " + std::to_string(playerId) + ": " + reason);
    PlayerLeave(playerId);
    return true;
}

bool ServerManager::BanPlayer(const std::string& username, const std::string& reason) {
    m_config.AddToBlacklist(username);
    SaveConfig();

    Player player = m_playerDatabase->GetPlayer(username);
    if (player.playerId >= 0) {
        player.isBanned = true;
        player.banReason = reason;
        m_playerDatabase->UpdatePlayer(player);

        if (player.isOnline) {
            PlayerLeave(player.playerId);
        }
    }

    LogMessage("Banned player: " + username + " (" + reason + ")");
    return true;
}

bool ServerManager::UnbanPlayer(const std::string& username) {
    m_config.RemoveFromBlacklist(username);
    SaveConfig();

    Player player = m_playerDatabase->GetPlayer(username);
    if (player.playerId >= 0) {
        player.isBanned = false;
        player.banReason.clear();
        m_playerDatabase->UpdatePlayer(player);
    }

    LogMessage("Unbanned player: " + username);
    return true;
}

bool ServerManager::CreateWorld(const std::string& worldName, int seed) {
    if (!m_worldDatabase) return false;

    int worldId = m_worldDatabase->CreateWorld(worldName, seed);
    if (worldId < 0) {
        LogError("Failed to create world: " + worldName);
        return false;
    }

    if (!m_worldDatabase->LoadWorld(worldId)) {
        LogError("Failed to load created world");
        return false;
    }

    m_worldName = worldName;
    LogMessage("Created and loaded world: " + worldName + " (seed: " + std::to_string(seed) + ")");
    return true;
}

bool ServerManager::LoadWorld(const std::string& worldName) {
    if (!m_worldDatabase) return false;

    if (!m_worldDatabase->LoadWorldByName(worldName)) {
        LogError("Failed to load world: " + worldName);
        return false;
    }

    m_worldName = worldName;
    LogMessage("Loaded world: " + worldName);
    return true;
}

void ServerManager::SaveWorld() {
    if (!m_worldDatabase || m_worldDatabase->GetCurrentWorldId() < 0) return;

    LogMessage("Saving world...");

    // Save all players
    auto players = m_playerDatabase->GetOnlinePlayers();
    for (const auto& player : players) {
        m_playerDatabase->UpdatePlayer(player);
    }

    // Save all dirty chunks
    if (m_chunkStreamer) {
        m_chunkStreamer->SaveAllDirtyChunks(true);
    }

    // Update world metadata
    m_worldDatabase->SaveWorld();

    LogMessage("World saved");
}

void ServerManager::TriggerSave() {
    SaveWorld();
    m_autoSaveTimer = 0.0f;
}

bool ServerManager::CreateBackup() {
    if (!m_worldDatabase) return false;

    std::string backupPath = GenerateBackupPath();
    LogMessage("Creating backup: " + backupPath);

    bool success = m_worldDatabase->CreateBackup(backupPath);
    if (success) {
        LogMessage("Backup created successfully");
    } else {
        LogError("Failed to create backup");
    }

    return success;
}

bool ServerManager::RestoreBackup(const std::string& backupPath) {
    if (!m_worldDatabase) return false;

    LogMessage("Restoring from backup: " + backupPath);

    bool success = m_worldDatabase->RestoreFromBackup(backupPath);
    if (success) {
        LogMessage("Backup restored successfully");
    } else {
        LogError("Failed to restore backup");
    }

    return success;
}

bool ServerManager::ReloadConfig() {
    if (m_config.LoadFromFile(m_configPath)) {
        LogMessage("Configuration reloaded");
        return true;
    }

    LogError("Failed to reload configuration");
    return false;
}

bool ServerManager::SaveConfig() {
    if (m_config.SaveToFile(m_configPath)) {
        return true;
    }
    return false;
}

ServerStats ServerManager::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    ServerStats stats = m_stats;

    stats.uptime = GetTimestamp() - m_startTime;
    stats.onlinePlayers = GetOnlinePlayerCount();

    if (m_playerDatabase) {
        stats.totalPlayers = m_playerDatabase->GetOnlinePlayers().size();
    }

    if (m_chunkStreamer) {
        stats.loadedChunks = m_chunkStreamer->GetLoadedChunkCount();
    }

    if (m_worldDatabase) {
        auto dbStats = m_worldDatabase->GetStatistics();
        stats.activeEntities = dbStats.activeEntities;
        stats.databaseSize = dbStats.databaseSizeBytes;
    }

    return stats;
}

DatabaseStats ServerManager::GetDatabaseStats() const {
    if (!m_worldDatabase) return {};
    return m_worldDatabase->GetStatistics();
}

ChunkStreamStats ServerManager::GetChunkStreamStats() const {
    if (!m_chunkStreamer) return {};
    return m_chunkStreamer->GetStatistics();
}

void ServerManager::ServerLoop() {
    float accumulator = 0.0f;
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        accumulator += deltaTime;

        while (accumulator >= m_tickInterval) {
            Update(m_tickInterval);
            accumulator -= m_tickInterval;
        }

        // Sleep to avoid burning CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ServerManager::ProcessAutoSave(float deltaTime) {
    m_autoSaveTimer += deltaTime;

    if (m_autoSaveTimer >= static_cast<float>(m_config.autoSaveInterval)) {
        SaveWorld();
        m_autoSaveTimer = 0.0f;
    }
}

void ServerManager::ProcessBackup(float deltaTime) {
    m_backupTimer += deltaTime;

    if (m_backupTimer >= static_cast<float>(m_config.backupInterval)) {
        CreateBackup();
        m_backupTimer = 0.0f;
    }
}

void ServerManager::UpdatePerformanceStats(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.avgFrameTime = deltaTime * 1000.0f; // Convert to milliseconds
}

void ServerManager::LogMessage(const std::string& message) {
    if (OnServerMessage) {
        OnServerMessage(message);
    }
}

void ServerManager::LogError(const std::string& error) {
    if (OnError) {
        OnError(error);
    }
}

std::string ServerManager::GenerateBackupPath() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "backups/" << m_worldName << "_";
    ss << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    ss << ".db";

    return ss.str();
}

} // namespace Nova
