#include "Territory.hpp"
#include "PersistentWorld.hpp"
#include "../network/FirebaseManager.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <queue>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define TERR_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define TERR_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#else
#include <iostream>
#define TERR_LOG_INFO(...) std::cout << "[Territory] " << __VA_ARGS__ << std::endl
#define TERR_LOG_WARN(...) std::cerr << "[Territory WARN] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// TerritoryTile Implementation
// ============================================================================

nlohmann::json TerritoryTile::ToJson() const {
    nlohmann::json j;
    j["position"] = {position.x, position.y};
    j["ownerId"] = ownerId;
    j["controlStrength"] = controlStrength;
    j["status"] = static_cast<int>(status);
    j["contestingPlayers"] = contestingPlayers;
    j["contestStrengths"] = contestStrengths;
    j["claimedTimestamp"] = claimedTimestamp;
    j["lastUpdated"] = lastUpdated;
    return j;
}

TerritoryTile TerritoryTile::FromJson(const nlohmann::json& j) {
    TerritoryTile tile;
    if (j.contains("position") && j["position"].is_array()) {
        tile.position.x = j["position"][0].get<int>();
        tile.position.y = j["position"][1].get<int>();
    }
    tile.ownerId = j.value("ownerId", "");
    tile.controlStrength = j.value("controlStrength", 0.0f);
    tile.status = static_cast<TerritoryStatus>(j.value("status", 0));
    if (j.contains("contestingPlayers")) {
        tile.contestingPlayers = j["contestingPlayers"].get<std::vector<std::string>>();
    }
    if (j.contains("contestStrengths")) {
        tile.contestStrengths = j["contestStrengths"].get<std::vector<float>>();
    }
    tile.claimedTimestamp = j.value("claimedTimestamp", 0LL);
    tile.lastUpdated = j.value("lastUpdated", 0LL);
    return tile;
}

// ============================================================================
// Territory Implementation
// ============================================================================

nlohmann::json Territory::ToJson() const {
    nlohmann::json j;
    j["ownerId"] = ownerId;
    j["totalControlStrength"] = totalControlStrength;
    j["totalTiles"] = totalTiles;
    j["buildingsInTerritory"] = buildingsInTerritory;
    j["resourceNodesInTerritory"] = resourceNodesInTerritory;

    j["tiles"] = nlohmann::json::array();
    for (const auto& t : tiles) {
        j["tiles"].push_back({t.x, t.y});
    }

    j["coreTiles"] = nlohmann::json::array();
    for (const auto& t : coreTiles) {
        j["coreTiles"].push_back({t.x, t.y});
    }

    j["borderTiles"] = nlohmann::json::array();
    for (const auto& t : borderTiles) {
        j["borderTiles"].push_back({t.x, t.y});
    }

    j["contestedTiles"] = nlohmann::json::array();
    for (const auto& t : contestedTiles) {
        j["contestedTiles"].push_back({t.x, t.y});
    }

    return j;
}

Territory Territory::FromJson(const nlohmann::json& j) {
    Territory t;
    t.ownerId = j.value("ownerId", "");
    t.totalControlStrength = j.value("totalControlStrength", 0.0f);
    t.totalTiles = j.value("totalTiles", 0);
    t.buildingsInTerritory = j.value("buildingsInTerritory", 0);
    t.resourceNodesInTerritory = j.value("resourceNodesInTerritory", 0);

    auto loadTiles = [](const nlohmann::json& arr) {
        std::vector<glm::ivec2> result;
        if (arr.is_array()) {
            for (const auto& item : arr) {
                if (item.is_array() && item.size() >= 2) {
                    result.push_back({item[0].get<int>(), item[1].get<int>()});
                }
            }
        }
        return result;
    };

    t.tiles = loadTiles(j.value("tiles", nlohmann::json::array()));
    t.coreTiles = loadTiles(j.value("coreTiles", nlohmann::json::array()));
    t.borderTiles = loadTiles(j.value("borderTiles", nlohmann::json::array()));
    t.contestedTiles = loadTiles(j.value("contestedTiles", nlohmann::json::array()));

    return t;
}

bool Territory::Contains(const glm::ivec2& pos) const {
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

float Territory::GetStrengthAt(const glm::ivec2& pos) const {
    // Could be optimized with a map, but vector search is fine for typical territory sizes
    if (Contains(pos)) {
        return totalControlStrength / std::max(1, totalTiles);
    }
    return 0.0f;
}

// ============================================================================
// TerritoryContest Implementation
// ============================================================================

nlohmann::json TerritoryContest::ToJson() const {
    return {
        {"position", {position.x, position.y}},
        {"defenderId", defenderId},
        {"attackerId", attackerId},
        {"defenderStrength", defenderStrength},
        {"attackerStrength", attackerStrength},
        {"startTimestamp", startTimestamp},
        {"resolveTimestamp", resolveTimestamp},
        {"resolved", resolved},
        {"winnerId", winnerId}
    };
}

TerritoryContest TerritoryContest::FromJson(const nlohmann::json& j) {
    TerritoryContest tc;
    if (j.contains("position") && j["position"].is_array()) {
        tc.position.x = j["position"][0].get<int>();
        tc.position.y = j["position"][1].get<int>();
    }
    tc.defenderId = j.value("defenderId", "");
    tc.attackerId = j.value("attackerId", "");
    tc.defenderStrength = j.value("defenderStrength", 0.0f);
    tc.attackerStrength = j.value("attackerStrength", 0.0f);
    tc.startTimestamp = j.value("startTimestamp", 0LL);
    tc.resolveTimestamp = j.value("resolveTimestamp", 0LL);
    tc.resolved = j.value("resolved", false);
    tc.winnerId = j.value("winnerId", "");
    return tc;
}

// ============================================================================
// TerritoryManager Implementation
// ============================================================================

TerritoryManager& TerritoryManager::Instance() {
    static TerritoryManager instance;
    return instance;
}

bool TerritoryManager::Initialize(const TerritoryConfig& config) {
    if (m_initialized) {
        TERR_LOG_WARN("TerritoryManager already initialized");
        return true;
    }

    m_config = config;
    m_initialized = true;

    TERR_LOG_INFO("TerritoryManager initialized");
    return true;
}

void TerritoryManager::Shutdown() {
    if (!m_initialized) return;

    StopListening();

    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);
        m_tiles.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_territoriesMutex);
        m_playerTerritories.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_contestsMutex);
        m_contests.clear();
    }

    m_initialized = false;
    TERR_LOG_INFO("TerritoryManager shutdown complete");
}

void TerritoryManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Update control strength periodically
    m_strengthUpdateTimer += deltaTime;
    if (m_strengthUpdateTimer >= STRENGTH_UPDATE_INTERVAL) {
        m_strengthUpdateTimer = 0.0f;
        UpdateControlStrength(STRENGTH_UPDATE_INTERVAL);
    }

    // Update contests
    m_contestUpdateTimer += deltaTime;
    if (m_contestUpdateTimer >= CONTEST_UPDATE_INTERVAL) {
        m_contestUpdateTimer = 0.0f;
        UpdateContests(CONTEST_UPDATE_INTERVAL);
    }
}

// ==================== Territory Queries ====================

TerritoryTile TerritoryManager::GetTileAt(const glm::ivec2& pos) const {
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    auto it = m_tiles.find(pos);
    if (it != m_tiles.end()) {
        return it->second;
    }

    // Return unclaimed tile
    TerritoryTile tile;
    tile.position = pos;
    return tile;
}

Territory TerritoryManager::GetPlayerTerritory(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_territoriesMutex);

    auto it = m_playerTerritories.find(playerId);
    if (it != m_playerTerritories.end()) {
        return it->second;
    }

    // Return empty territory
    Territory t;
    t.ownerId = playerId;
    return t;
}

Territory TerritoryManager::GetLocalTerritory() const {
    return GetPlayerTerritory(m_localPlayerId);
}

bool TerritoryManager::IsInTerritory(const glm::ivec2& pos, const std::string& playerId) const {
    TerritoryTile tile = GetTileAt(pos);
    return tile.ownerId == playerId && tile.status == TerritoryStatus::Owned;
}

bool TerritoryManager::IsInOwnTerritory(const glm::ivec2& pos) const {
    return IsInTerritory(pos, m_localPlayerId);
}

bool TerritoryManager::IsContested(const glm::ivec2& pos) const {
    TerritoryTile tile = GetTileAt(pos);
    return tile.status == TerritoryStatus::Contested;
}

TerritoryStatus TerritoryManager::GetStatusAt(const glm::ivec2& pos) const {
    return GetTileAt(pos).status;
}

std::vector<std::string> TerritoryManager::GetPlayersInRegion(
    const glm::ivec2& min, const glm::ivec2& max) const {

    std::unordered_set<std::string> players;

    std::lock_guard<std::mutex> lock(m_tilesMutex);

    for (const auto& [pos, tile] : m_tiles) {
        if (pos.x >= min.x && pos.x <= max.x &&
            pos.y >= min.y && pos.y <= max.y) {
            if (!tile.ownerId.empty()) {
                players.insert(tile.ownerId);
            }
            for (const auto& contestant : tile.contestingPlayers) {
                players.insert(contestant);
            }
        }
    }

    return std::vector<std::string>(players.begin(), players.end());
}

// ==================== Territory Modification ====================

void TerritoryManager::RecalculateTerritory(const std::string& playerId,
                                            const std::vector<Building>& buildings) {
    if (buildings.empty()) {
        ReleaseAllTerritory(playerId);
        return;
    }

    // Clear existing territory for this player
    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);
        for (auto& [pos, tile] : m_tiles) {
            if (tile.ownerId == playerId) {
                tile.ownerId.clear();
                tile.controlStrength = 0.0f;
                tile.status = TerritoryStatus::Unclaimed;
            }
        }
    }

    // Calculate territory from buildings
    std::unordered_set<glm::ivec2, IVec2Hash> claimedTiles;

    for (const auto& building : buildings) {
        if (!building.IsConstructed() || building.IsDestroyed()) {
            continue;
        }

        float radius = GetBuildingTerritoryRadius(building.type);
        float baseStrength = GetBuildingTerritoryStrength(building.type);

        // Expand territory in a circle around the building
        glm::ivec2 center = building.GetCenter();
        int radiusInt = static_cast<int>(std::ceil(radius));

        for (int dx = -radiusInt; dx <= radiusInt; ++dx) {
            for (int dy = -radiusInt; dy <= radiusInt; ++dy) {
                glm::ivec2 pos = center + glm::ivec2(dx, dy);
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

                if (dist <= radius) {
                    // Calculate strength at this distance
                    float strength = baseStrength - (dist * m_config.controlDecayPerTile);

                    if (strength >= m_config.minControlStrength) {
                        claimedTiles.insert(pos);
                        ClaimTile(pos, playerId, strength);
                    }
                }
            }
        }
    }

    // Rebuild player territory structure
    {
        std::lock_guard<std::mutex> lock(m_territoriesMutex);

        Territory& territory = m_playerTerritories[playerId];
        territory.ownerId = playerId;
        territory.tiles.clear();
        territory.coreTiles.clear();
        territory.borderTiles.clear();
        territory.contestedTiles.clear();
        territory.totalControlStrength = 0.0f;

        std::lock_guard<std::mutex> tilesLock(m_tilesMutex);

        for (const auto& pos : claimedTiles) {
            auto it = m_tiles.find(pos);
            if (it != m_tiles.end() && it->second.ownerId == playerId) {
                territory.tiles.push_back(pos);
                territory.totalControlStrength += it->second.controlStrength;

                if (it->second.status == TerritoryStatus::Contested) {
                    territory.contestedTiles.push_back(pos);
                } else {
                    // Check if border tile
                    bool isBorder = false;
                    for (const auto& dir : std::vector<glm::ivec2>{{1,0},{-1,0},{0,1},{0,-1}}) {
                        auto neighborIt = m_tiles.find(pos + dir);
                        if (neighborIt == m_tiles.end() || neighborIt->second.ownerId != playerId) {
                            isBorder = true;
                            break;
                        }
                    }

                    if (isBorder) {
                        territory.borderTiles.push_back(pos);
                    } else {
                        territory.coreTiles.push_back(pos);
                    }
                }
            }
        }

        territory.totalTiles = static_cast<int>(territory.tiles.size());
        territory.buildingsInTerritory = static_cast<int>(buildings.size());
    }

    // Notify callbacks
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        Territory territory = GetPlayerTerritory(playerId);
        for (auto& callback : m_territoryCallbacks) {
            if (callback) {
                callback(territory);
            }
        }
    }

    TERR_LOG_INFO("Recalculated territory for " + playerId + ": " +
                  std::to_string(claimedTiles.size()) + " tiles");
}

bool TerritoryManager::ClaimTile(const glm::ivec2& pos, const std::string& playerId, float strength) {
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    TerritoryTile& tile = m_tiles[pos];
    tile.position = pos;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    tile.lastUpdated = now;

    if (tile.status == TerritoryStatus::Protected) {
        return false;  // Can't claim protected tiles
    }

    if (tile.ownerId.empty() || tile.status == TerritoryStatus::Unclaimed) {
        // Unclaimed - just take it
        tile.ownerId = playerId;
        tile.controlStrength = strength;
        tile.status = TerritoryStatus::Owned;
        tile.claimedTimestamp = now;
        return true;
    }

    if (tile.ownerId == playerId) {
        // Already owned - update strength
        tile.controlStrength = std::max(tile.controlStrength, strength);
        return true;
    }

    // Owned by someone else - check for contest
    float contestThreshold = tile.controlStrength * m_config.contestThreshold;

    if (strength >= contestThreshold) {
        // Start contest
        StartContest(pos, playerId, tile.ownerId, strength);
        return true;
    }

    return false;  // Not strong enough to contest
}

void TerritoryManager::ReleaseTile(const glm::ivec2& pos, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    auto it = m_tiles.find(pos);
    if (it != m_tiles.end() && it->second.ownerId == playerId) {
        it->second.ownerId.clear();
        it->second.controlStrength = 0.0f;
        it->second.status = TerritoryStatus::Unclaimed;
        it->second.contestingPlayers.clear();
        it->second.contestStrengths.clear();
    }
}

void TerritoryManager::ReleaseAllTerritory(const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    for (auto& [pos, tile] : m_tiles) {
        if (tile.ownerId == playerId) {
            tile.ownerId.clear();
            tile.controlStrength = 0.0f;
            tile.status = TerritoryStatus::Unclaimed;
        }

        // Remove from contesting players
        auto it = std::find(tile.contestingPlayers.begin(),
                           tile.contestingPlayers.end(), playerId);
        if (it != tile.contestingPlayers.end()) {
            size_t idx = std::distance(tile.contestingPlayers.begin(), it);
            tile.contestingPlayers.erase(it);
            if (idx < tile.contestStrengths.size()) {
                tile.contestStrengths.erase(tile.contestStrengths.begin() + idx);
            }
        }
    }

    // Clear player territory
    {
        std::lock_guard<std::mutex> terrLock(m_territoriesMutex);
        m_playerTerritories.erase(playerId);
    }

    TERR_LOG_INFO("Released all territory for " + playerId);
}

// ==================== Contest Management ====================

std::vector<TerritoryContest> TerritoryManager::GetActiveContests(const std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_contestsMutex);

    std::vector<TerritoryContest> result;
    for (const auto& [pos, contest] : m_contests) {
        if (!contest.resolved &&
            (contest.defenderId == playerId || contest.attackerId == playerId)) {
            result.push_back(contest);
        }
    }
    return result;
}

const TerritoryContest* TerritoryManager::GetContestAt(const glm::ivec2& pos) const {
    std::lock_guard<std::mutex> lock(m_contestsMutex);

    auto it = m_contests.find(pos);
    if (it != m_contests.end() && !it->second.resolved) {
        return &it->second;
    }
    return nullptr;
}

void TerritoryManager::ResolveContest(const glm::ivec2& pos, const std::string& winnerId) {
    {
        std::lock_guard<std::mutex> lock(m_contestsMutex);

        auto it = m_contests.find(pos);
        if (it == m_contests.end()) return;

        it->second.resolved = true;
        it->second.winnerId = winnerId;
    }

    // Update tile ownership
    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);

        auto tileIt = m_tiles.find(pos);
        if (tileIt != m_tiles.end()) {
            tileIt->second.ownerId = winnerId;
            tileIt->second.status = TerritoryStatus::Owned;
            tileIt->second.contestingPlayers.clear();
            tileIt->second.contestStrengths.clear();
            tileIt->second.claimedTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    TERR_LOG_INFO("Contest resolved at (" + std::to_string(pos.x) + "," +
                  std::to_string(pos.y) + ") - winner: " + winnerId);
}

// ==================== Bonuses ====================

float TerritoryManager::GetDefenseBonus(const glm::ivec2& pos, const std::string& playerId) const {
    TerritoryTile tile = GetTileAt(pos);

    if (tile.ownerId == playerId) {
        if (tile.status == TerritoryStatus::Contested) {
            return 1.0f;  // No bonus in contested territory
        }
        return m_config.ownTerritoryDefenseBonus;
    }

    if (tile.status == TerritoryStatus::Contested) {
        return m_config.contestedPenalty;
    }

    return 1.0f;
}

float TerritoryManager::GetProductionBonus(const glm::ivec2& pos, const std::string& playerId) const {
    TerritoryTile tile = GetTileAt(pos);

    if (tile.ownerId == playerId) {
        if (tile.status == TerritoryStatus::Contested) {
            return m_config.contestedPenalty;
        }
        return m_config.ownTerritoryProductionBonus;
    }

    return 1.0f;
}

bool TerritoryManager::HasVision(const glm::ivec2& pos, const std::string& playerId) const {
    TerritoryTile tile = GetTileAt(pos);

    // Vision in own territory
    if (tile.ownerId == playerId) {
        return true;
    }

    // Vision in contested territory
    if (tile.status == TerritoryStatus::Contested) {
        for (const auto& contestant : tile.contestingPlayers) {
            if (contestant == playerId) {
                return true;
            }
        }
    }

    return false;
}

// ==================== Synchronization ====================

void TerritoryManager::SyncToServer() {
    if (m_localPlayerId.empty()) return;

    Territory territory = GetLocalTerritory();
    nlohmann::json data = territory.ToJson();

    FirebaseManager::Instance().SetValue(GetTerritoryPath(m_localPlayerId), data);
}

void TerritoryManager::LoadFromServer(const std::string& playerId) {
    FirebaseManager::Instance().GetValue(GetTerritoryPath(playerId),
        [this, playerId](const nlohmann::json& data) {
            if (data.is_null() || data.empty()) return;

            Territory territory = Territory::FromJson(data);

            std::lock_guard<std::mutex> lock(m_territoriesMutex);
            m_playerTerritories[playerId] = territory;

            TERR_LOG_INFO("Loaded territory for " + playerId + ": " +
                         std::to_string(territory.totalTiles) + " tiles");
        });
}

void TerritoryManager::ListenForChanges() {
    // Listen for territory changes in the region
    // This would listen for all nearby players' territory updates
}

void TerritoryManager::StopListening() {
    if (!m_territoryListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_territoryListenerId);
        m_territoryListenerId.clear();
    }
    if (!m_contestsListenerId.empty()) {
        FirebaseManager::Instance().StopListeningById(m_contestsListenerId);
        m_contestsListenerId.clear();
    }
}

// ==================== Callbacks ====================

void TerritoryManager::OnTerritoryChanged(TerritoryChangedCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_territoryCallbacks.push_back(std::move(callback));
}

void TerritoryManager::OnContest(ContestCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_contestCallbacks.push_back(std::move(callback));
}

// ==================== Private Methods ====================

void TerritoryManager::UpdateControlStrength(float deltaTime) {
    float hoursElapsed = deltaTime / 3600.0f;

    std::lock_guard<std::mutex> lock(m_tilesMutex);

    for (auto& [pos, tile] : m_tiles) {
        if (tile.status == TerritoryStatus::Owned && !tile.ownerId.empty()) {
            // Grow strength over time
            tile.controlStrength = std::min(100.0f,
                tile.controlStrength + m_config.controlGrowthPerHour * hoursElapsed);
        } else if (tile.controlStrength > 0.0f && tile.ownerId.empty()) {
            // Decay unclaimed tiles with residual strength
            tile.controlStrength = std::max(0.0f,
                tile.controlStrength - m_config.controlDecayPerHour * hoursElapsed);
        }
    }
}

void TerritoryManager::UpdateContests(float deltaTime) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(m_contestsMutex);

    for (auto& [pos, contest] : m_contests) {
        if (contest.resolved) continue;

        // Check if contest should be resolved
        if (now >= contest.resolveTimestamp) {
            // Winner is whoever has more strength
            std::string winner = contest.defenderStrength >= contest.attackerStrength
                ? contest.defenderId : contest.attackerId;

            ResolveContest(pos, winner);

            // Notify callbacks
            std::lock_guard<std::mutex> callbackLock(m_callbackMutex);
            for (auto& callback : m_contestCallbacks) {
                if (callback) {
                    callback(contest);
                }
            }
        }
    }
}

void TerritoryManager::StartContest(const glm::ivec2& pos, const std::string& attackerId,
                                    const std::string& defenderId, float attackStrength) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    TerritoryContest contest;
    contest.position = pos;
    contest.defenderId = defenderId;
    contest.attackerId = attackerId;
    contest.attackerStrength = attackStrength;
    contest.startTimestamp = now;
    contest.resolveTimestamp = now + static_cast<int64_t>(m_config.contestDurationHours * 3600.0f);

    // Get defender strength from tile
    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);
        auto it = m_tiles.find(pos);
        if (it != m_tiles.end()) {
            contest.defenderStrength = it->second.controlStrength;
            it->second.status = TerritoryStatus::Contested;
            it->second.contestingPlayers.push_back(attackerId);
            it->second.contestStrengths.push_back(attackStrength);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_contestsMutex);
        m_contests[pos] = contest;
    }

    TERR_LOG_INFO("Contest started at (" + std::to_string(pos.x) + "," +
                  std::to_string(pos.y) + ") - " + attackerId + " vs " + defenderId);

    // Notify callbacks
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        for (auto& callback : m_contestCallbacks) {
            if (callback) {
                callback(contest);
            }
        }
    }
}

float TerritoryManager::CalculateStrengthFromBuildings(const glm::ivec2& pos,
                                                       const std::vector<Building>& buildings) const {
    float maxStrength = 0.0f;

    for (const auto& building : buildings) {
        if (!building.IsConstructed() || building.IsDestroyed()) continue;

        glm::ivec2 center = building.GetCenter();
        glm::ivec2 diff = pos - center;
        float dist = std::sqrt(static_cast<float>(diff.x * diff.x + diff.y * diff.y));

        float radius = GetBuildingTerritoryRadius(building.type);
        if (dist <= radius) {
            float baseStrength = GetBuildingTerritoryStrength(building.type);
            float strength = baseStrength - (dist * m_config.controlDecayPerTile);
            maxStrength = std::max(maxStrength, strength);
        }
    }

    return maxStrength;
}

float TerritoryManager::GetBuildingTerritoryRadius(BuildingType type) const {
    switch (type) {
        case BuildingType::CommandCenter:
            return m_config.commandCenterRadius;
        case BuildingType::Beacon:
            return m_config.baseExpansionRadius + m_config.beaconExpansionBonus;
        case BuildingType::Tower:
            return m_config.baseExpansionRadius + 2.0f;
        case BuildingType::Wall:
        case BuildingType::Gate:
            return m_config.baseExpansionRadius - 2.0f;
        default:
            return m_config.baseExpansionRadius;
    }
}

float TerritoryManager::GetBuildingTerritoryStrength(BuildingType type) const {
    switch (type) {
        case BuildingType::CommandCenter:
            return m_config.baseControlPerBuilding * 2.5f;
        case BuildingType::Beacon:
            return m_config.baseControlPerBuilding * 1.5f;
        case BuildingType::Tower:
        case BuildingType::Bunker:
            return m_config.baseControlPerBuilding * 1.5f;
        case BuildingType::Wall:
        case BuildingType::Gate:
            return m_config.baseControlPerBuilding * 0.5f;
        default:
            return m_config.baseControlPerBuilding;
    }
}

std::string TerritoryManager::GetTerritoryPath(const std::string& playerId) const {
    return "rts/territory/" + playerId;
}

std::string TerritoryManager::GetContestsPath() const {
    return "rts/contests";
}

// ============================================================================
// TerritoryVisualizer Implementation
// ============================================================================

glm::vec4 TerritoryVisualizer::GetStatusColor(TerritoryStatus status) {
    switch (status) {
        case TerritoryStatus::Unclaimed:
            return glm::vec4(0.5f, 0.5f, 0.5f, 0.3f);  // Gray
        case TerritoryStatus::Owned:
            return glm::vec4(0.2f, 0.8f, 0.2f, 0.4f);  // Green
        case TerritoryStatus::Contested:
            return glm::vec4(1.0f, 0.5f, 0.0f, 0.5f);  // Orange
        case TerritoryStatus::Protected:
            return glm::vec4(0.0f, 0.5f, 1.0f, 0.4f);  // Blue
        default:
            return glm::vec4(1.0f, 1.0f, 1.0f, 0.2f);
    }
}

glm::vec4 TerritoryVisualizer::GetPlayerColor(const std::string& playerId, bool isOwn) {
    if (isOwn) {
        return glm::vec4(0.2f, 0.8f, 0.2f, 0.4f);  // Green for own
    }

    // Generate consistent color from player ID hash
    std::hash<std::string> hasher;
    size_t hash = hasher(playerId);

    float hue = static_cast<float>(hash % 360) / 360.0f;

    // HSV to RGB (simple version)
    float r, g, b;
    float h = hue * 6.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    float q = 1.0f - f;

    switch (i % 6) {
        case 0: r = 1.0f; g = f; b = 0.0f; break;
        case 1: r = q; g = 1.0f; b = 0.0f; break;
        case 2: r = 0.0f; g = 1.0f; b = f; break;
        case 3: r = 0.0f; g = q; b = 1.0f; break;
        case 4: r = f; g = 0.0f; b = 1.0f; break;
        default: r = 1.0f; g = 0.0f; b = q; break;
    }

    return glm::vec4(r * 0.8f, g * 0.8f, b * 0.8f, 0.4f);
}

glm::vec4 TerritoryVisualizer::GetStrengthColor(float strength) {
    // Interpolate from red (weak) to green (strong)
    float t = std::clamp(strength / 100.0f, 0.0f, 1.0f);
    return glm::vec4(1.0f - t, t, 0.0f, 0.5f);
}

} // namespace Vehement
