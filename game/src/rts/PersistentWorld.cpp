#include "PersistentWorld.hpp"
#include "../network/FirebaseManager.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>

// Logging macros
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define RTS_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define RTS_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define RTS_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define RTS_LOG_INFO(...) std::cout << "[RTS] " << __VA_ARGS__ << std::endl
#define RTS_LOG_WARN(...) std::cerr << "[RTS WARN] " << __VA_ARGS__ << std::endl
#define RTS_LOG_ERROR(...) std::cerr << "[RTS ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ============================================================================
// Resource Type Helpers
// ============================================================================

const char* ResourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::Food: return "food";
        case ResourceType::Wood: return "wood";
        case ResourceType::Stone: return "stone";
        case ResourceType::Metal: return "metal";
        case ResourceType::Fuel: return "fuel";
        case ResourceType::Medicine: return "medicine";
        case ResourceType::Ammunition: return "ammunition";
        default: return "unknown";
    }
}

ResourceType StringToResourceType(const std::string& str) {
    if (str == "food") return ResourceType::Food;
    if (str == "wood") return ResourceType::Wood;
    if (str == "stone") return ResourceType::Stone;
    if (str == "metal") return ResourceType::Metal;
    if (str == "fuel") return ResourceType::Fuel;
    if (str == "medicine") return ResourceType::Medicine;
    if (str == "ammunition") return ResourceType::Ammunition;
    return ResourceType::Food;
}

// ============================================================================
// Building Type Helpers
// ============================================================================

const char* BuildingTypeToString(BuildingType type) {
    switch (type) {
        case BuildingType::Farm: return "farm";
        case BuildingType::Sawmill: return "sawmill";
        case BuildingType::Quarry: return "quarry";
        case BuildingType::Mine: return "mine";
        case BuildingType::Refinery: return "refinery";
        case BuildingType::Hospital: return "hospital";
        case BuildingType::Armory: return "armory";
        case BuildingType::Warehouse: return "warehouse";
        case BuildingType::Silo: return "silo";
        case BuildingType::Wall: return "wall";
        case BuildingType::Tower: return "tower";
        case BuildingType::Gate: return "gate";
        case BuildingType::Bunker: return "bunker";
        case BuildingType::House: return "house";
        case BuildingType::Barracks: return "barracks";
        case BuildingType::CommandCenter: return "command_center";
        case BuildingType::Workshop: return "workshop";
        case BuildingType::Laboratory: return "laboratory";
        case BuildingType::TradingPost: return "trading_post";
        case BuildingType::Beacon: return "beacon";
        default: return "unknown";
    }
}

BuildingType StringToBuildingType(const std::string& str) {
    if (str == "farm") return BuildingType::Farm;
    if (str == "sawmill") return BuildingType::Sawmill;
    if (str == "quarry") return BuildingType::Quarry;
    if (str == "mine") return BuildingType::Mine;
    if (str == "refinery") return BuildingType::Refinery;
    if (str == "hospital") return BuildingType::Hospital;
    if (str == "armory") return BuildingType::Armory;
    if (str == "warehouse") return BuildingType::Warehouse;
    if (str == "silo") return BuildingType::Silo;
    if (str == "wall") return BuildingType::Wall;
    if (str == "tower") return BuildingType::Tower;
    if (str == "gate") return BuildingType::Gate;
    if (str == "bunker") return BuildingType::Bunker;
    if (str == "house") return BuildingType::House;
    if (str == "barracks") return BuildingType::Barracks;
    if (str == "command_center") return BuildingType::CommandCenter;
    if (str == "workshop") return BuildingType::Workshop;
    if (str == "laboratory") return BuildingType::Laboratory;
    if (str == "trading_post") return BuildingType::TradingPost;
    if (str == "beacon") return BuildingType::Beacon;
    return BuildingType::Farm;
}

// ============================================================================
// ResourceStock Implementation
// ============================================================================

ResourceStock::ResourceStock() {
    // Initialize with default starting resources
    amounts[static_cast<size_t>(ResourceType::Food)] = 100;
    amounts[static_cast<size_t>(ResourceType::Wood)] = 50;
    amounts[static_cast<size_t>(ResourceType::Stone)] = 25;

    // Default capacities
    for (size_t i = 0; i < static_cast<size_t>(ResourceType::Count); ++i) {
        capacity[i] = 500;
    }
}

int ResourceStock::Get(ResourceType type) const {
    return amounts[static_cast<size_t>(type)];
}

void ResourceStock::Set(ResourceType type, int amount) {
    size_t idx = static_cast<size_t>(type);
    amounts[idx] = std::clamp(amount, 0, capacity[idx]);
}

void ResourceStock::Add(ResourceType type, int amount) {
    size_t idx = static_cast<size_t>(type);
    amounts[idx] = std::min(amounts[idx] + amount, capacity[idx]);
}

bool ResourceStock::Consume(ResourceType type, int amount) {
    size_t idx = static_cast<size_t>(type);
    if (amounts[idx] >= amount) {
        amounts[idx] -= amount;
        return true;
    }
    return false;
}

bool ResourceStock::CanAfford(ResourceType type, int amount) const {
    return amounts[static_cast<size_t>(type)] >= amount;
}

int ResourceStock::GetCapacity(ResourceType type) const {
    return capacity[static_cast<size_t>(type)];
}

void ResourceStock::SetCapacity(ResourceType type, int cap) {
    capacity[static_cast<size_t>(type)] = cap;
}

float ResourceStock::GetProductionRate(ResourceType type) const {
    return productionRate[static_cast<size_t>(type)];
}

void ResourceStock::SetProductionRate(ResourceType type, float rate) {
    productionRate[static_cast<size_t>(type)] = rate;
}

float ResourceStock::GetConsumptionRate(ResourceType type) const {
    return consumptionRate[static_cast<size_t>(type)];
}

void ResourceStock::SetConsumptionRate(ResourceType type, float rate) {
    consumptionRate[static_cast<size_t>(type)] = rate;
}

nlohmann::json ResourceStock::ToJson() const {
    nlohmann::json j;
    j["amounts"] = nlohmann::json::object();
    j["capacity"] = nlohmann::json::object();
    j["productionRate"] = nlohmann::json::object();
    j["consumptionRate"] = nlohmann::json::object();

    for (size_t i = 0; i < static_cast<size_t>(ResourceType::Count); ++i) {
        std::string name = ResourceTypeToString(static_cast<ResourceType>(i));
        j["amounts"][name] = amounts[i];
        j["capacity"][name] = capacity[i];
        j["productionRate"][name] = productionRate[i];
        j["consumptionRate"][name] = consumptionRate[i];
    }
    return j;
}

ResourceStock ResourceStock::FromJson(const nlohmann::json& j) {
    ResourceStock stock;
    if (j.contains("amounts")) {
        for (auto& [key, val] : j["amounts"].items()) {
            ResourceType type = StringToResourceType(key);
            stock.amounts[static_cast<size_t>(type)] = val.get<int>();
        }
    }
    if (j.contains("capacity")) {
        for (auto& [key, val] : j["capacity"].items()) {
            ResourceType type = StringToResourceType(key);
            stock.capacity[static_cast<size_t>(type)] = val.get<int>();
        }
    }
    if (j.contains("productionRate")) {
        for (auto& [key, val] : j["productionRate"].items()) {
            ResourceType type = StringToResourceType(key);
            stock.productionRate[static_cast<size_t>(type)] = val.get<float>();
        }
    }
    if (j.contains("consumptionRate")) {
        for (auto& [key, val] : j["consumptionRate"].items()) {
            ResourceType type = StringToResourceType(key);
            stock.consumptionRate[static_cast<size_t>(type)] = val.get<float>();
        }
    }
    return stock;
}

// ============================================================================
// Building Implementation
// ============================================================================

nlohmann::json Building::ToJson() const {
    return {
        {"id", id},
        {"type", BuildingTypeToString(type)},
        {"position", {position.x, position.y}},
        {"size", {size.x, size.y}},
        {"health", health},
        {"maxHealth", maxHealth},
        {"level", level},
        {"constructionProgress", constructionProgress},
        {"isActive", isActive},
        {"createdTimestamp", createdTimestamp},
        {"assignedWorkers", assignedWorkers},
        {"producesResource", ResourceTypeToString(producesResource)},
        {"productionPerHour", productionPerHour}
    };
}

Building Building::FromJson(const nlohmann::json& j) {
    Building b;
    b.id = j.value("id", -1);
    b.type = StringToBuildingType(j.value("type", "farm"));
    if (j.contains("position") && j["position"].is_array()) {
        b.position.x = j["position"][0].get<int>();
        b.position.y = j["position"][1].get<int>();
    }
    if (j.contains("size") && j["size"].is_array()) {
        b.size.x = j["size"][0].get<int>();
        b.size.y = j["size"][1].get<int>();
    }
    b.health = j.value("health", 100);
    b.maxHealth = j.value("maxHealth", 100);
    b.level = j.value("level", 1);
    b.constructionProgress = j.value("constructionProgress", 1.0f);
    b.isActive = j.value("isActive", true);
    b.createdTimestamp = j.value("createdTimestamp", 0LL);
    b.assignedWorkers = j.value("assignedWorkers", 0);
    b.producesResource = StringToResourceType(j.value("producesResource", "food"));
    b.productionPerHour = j.value("productionPerHour", 0.0f);
    return b;
}

// ============================================================================
// Worker Implementation
// ============================================================================

nlohmann::json Worker::ToJson() const {
    return {
        {"id", id},
        {"name", name},
        {"health", health},
        {"maxHealth", maxHealth},
        {"job", static_cast<int>(job)},
        {"assignedBuildingId", assignedBuildingId},
        {"position", {position.x, position.y}},
        {"efficiency", efficiency},
        {"morale", morale},
        {"hiredTimestamp", hiredTimestamp}
    };
}

Worker Worker::FromJson(const nlohmann::json& j) {
    Worker w;
    w.id = j.value("id", -1);
    w.name = j.value("name", "Worker");
    w.health = j.value("health", 100);
    w.maxHealth = j.value("maxHealth", 100);
    w.job = static_cast<WorkerJob>(j.value("job", 0));
    w.assignedBuildingId = j.value("assignedBuildingId", -1);
    if (j.contains("position") && j["position"].is_array()) {
        w.position.x = j["position"][0].get<int>();
        w.position.y = j["position"][1].get<int>();
    }
    w.efficiency = j.value("efficiency", 1.0f);
    w.morale = j.value("morale", 100.0f);
    w.hiredTimestamp = j.value("hiredTimestamp", 0LL);
    return w;
}

// ============================================================================
// HeroData Implementation
// ============================================================================

nlohmann::json HeroData::ToJson() const {
    return {
        {"playerId", playerId},
        {"name", name},
        {"level", level},
        {"experience", experience},
        {"health", health},
        {"maxHealth", maxHealth},
        {"position", {position.x, position.y}},
        {"rotation", rotation},
        {"zombiesKilled", zombiesKilled},
        {"deaths", deaths},
        {"survivalTime", survivalTime},
        {"inventory", inventory},
        {"equippedWeapon", equippedWeapon},
        {"isOnline", isOnline},
        {"lastOnlineTimestamp", lastOnlineTimestamp}
    };
}

HeroData HeroData::FromJson(const nlohmann::json& j) {
    HeroData h;
    h.playerId = j.value("playerId", "");
    h.name = j.value("name", "Hero");
    h.level = j.value("level", 1);
    h.experience = j.value("experience", 0);
    h.health = j.value("health", 100);
    h.maxHealth = j.value("maxHealth", 100);
    if (j.contains("position") && j["position"].is_array()) {
        h.position.x = j["position"][0].get<float>();
        h.position.y = j["position"][1].get<float>();
    }
    h.rotation = j.value("rotation", 0.0f);
    h.zombiesKilled = j.value("zombiesKilled", 0);
    h.deaths = j.value("deaths", 0);
    h.survivalTime = j.value("survivalTime", 0.0f);
    if (j.contains("inventory") && j["inventory"].is_array()) {
        h.inventory = j["inventory"].get<std::vector<int>>();
    }
    h.equippedWeapon = j.value("equippedWeapon", -1);
    h.isOnline = j.value("isOnline", false);
    h.lastOnlineTimestamp = j.value("lastOnlineTimestamp", 0LL);
    return h;
}

// ============================================================================
// TileChange Implementation
// ============================================================================

nlohmann::json TileChange::ToJson() const {
    return {
        {"position", {position.x, position.y}},
        {"previousTileType", previousTileType},
        {"newTileType", newTileType},
        {"changedBy", changedBy},
        {"timestamp", timestamp}
    };
}

TileChange TileChange::FromJson(const nlohmann::json& j) {
    TileChange tc;
    if (j.contains("position") && j["position"].is_array()) {
        tc.position.x = j["position"][0].get<int>();
        tc.position.y = j["position"][1].get<int>();
    }
    tc.previousTileType = j.value("previousTileType", 0);
    tc.newTileType = j.value("newTileType", 0);
    tc.changedBy = j.value("changedBy", "");
    tc.timestamp = j.value("timestamp", 0LL);
    return tc;
}

// ============================================================================
// ResourceNode Implementation
// ============================================================================

nlohmann::json ResourceNode::ToJson() const {
    return {
        {"id", id},
        {"type", ResourceTypeToString(type)},
        {"position", {position.x, position.y}},
        {"remaining", remaining},
        {"maxAmount", maxAmount},
        {"regenerationRate", regenerationRate},
        {"depleted", depleted},
        {"lastHarvestTimestamp", lastHarvestTimestamp}
    };
}

ResourceNode ResourceNode::FromJson(const nlohmann::json& j) {
    ResourceNode rn;
    rn.id = j.value("id", -1);
    rn.type = StringToResourceType(j.value("type", "wood"));
    if (j.contains("position") && j["position"].is_array()) {
        rn.position.x = j["position"][0].get<int>();
        rn.position.y = j["position"][1].get<int>();
    }
    rn.remaining = j.value("remaining", 100);
    rn.maxAmount = j.value("maxAmount", 100);
    rn.regenerationRate = j.value("regenerationRate", 0.0f);
    rn.depleted = j.value("depleted", false);
    rn.lastHarvestTimestamp = j.value("lastHarvestTimestamp", 0LL);
    return rn;
}

// ============================================================================
// WorldEvent Implementation
// ============================================================================

nlohmann::json WorldEvent::ToJson() const {
    return {
        {"type", static_cast<int>(type)},
        {"description", description},
        {"timestamp", timestamp},
        {"affectedPlayerId", affectedPlayerId},
        {"data", data}
    };
}

WorldEvent WorldEvent::FromJson(const nlohmann::json& j) {
    WorldEvent we;
    we.type = static_cast<WorldEvent::Type>(j.value("type", 0));
    we.description = j.value("description", "");
    we.timestamp = j.value("timestamp", 0LL);
    we.affectedPlayerId = j.value("affectedPlayerId", "");
    if (j.contains("data")) {
        we.data = j["data"];
    }
    return we;
}

// ============================================================================
// WorldState Implementation
// ============================================================================

nlohmann::json WorldState::ToJson() const {
    nlohmann::json j;
    j["playerId"] = playerId;
    j["baseRegion"] = baseRegion;

    // Buildings
    j["buildings"] = nlohmann::json::array();
    for (const auto& b : buildings) {
        j["buildings"].push_back(b.ToJson());
    }

    // Workers
    j["workers"] = nlohmann::json::array();
    for (const auto& w : workers) {
        j["workers"].push_back(w.ToJson());
    }

    // Resources
    j["resources"] = resources.ToJson();

    // Hero
    j["hero"] = hero.ToJson();

    // Map changes
    j["mapChanges"] = nlohmann::json::array();
    for (const auto& mc : mapChanges) {
        j["mapChanges"].push_back(mc.ToJson());
    }

    // Resource nodes
    j["resourceNodes"] = nlohmann::json::array();
    for (const auto& rn : resourceNodes) {
        j["resourceNodes"].push_back(rn.ToJson());
    }

    // Time tracking
    j["lastUpdateTimestamp"] = lastUpdateTimestamp;
    j["lastLoginTimestamp"] = lastLoginTimestamp;
    j["createdTimestamp"] = createdTimestamp;
    j["totalPlayTime"] = totalPlayTime;

    // Territory
    j["ownedTiles"] = nlohmann::json::array();
    for (const auto& tile : ownedTiles) {
        j["ownedTiles"].push_back({tile.x, tile.y});
    }
    j["territoryStrength"] = territoryStrength;

    // Stats
    j["totalZombiesKilled"] = totalZombiesKilled;
    j["totalBuildingsBuilt"] = totalBuildingsBuilt;
    j["totalWorkersHired"] = totalWorkersHired;
    j["attacksSurvived"] = attacksSurvived;

    return j;
}

WorldState WorldState::FromJson(const nlohmann::json& j) {
    WorldState ws;
    ws.playerId = j.value("playerId", "");
    ws.baseRegion = j.value("baseRegion", "");

    // Buildings
    if (j.contains("buildings") && j["buildings"].is_array()) {
        for (const auto& b : j["buildings"]) {
            ws.buildings.push_back(Building::FromJson(b));
        }
    }

    // Workers
    if (j.contains("workers") && j["workers"].is_array()) {
        for (const auto& w : j["workers"]) {
            ws.workers.push_back(Worker::FromJson(w));
        }
    }

    // Resources
    if (j.contains("resources")) {
        ws.resources = ResourceStock::FromJson(j["resources"]);
    }

    // Hero
    if (j.contains("hero")) {
        ws.hero = HeroData::FromJson(j["hero"]);
    }

    // Map changes
    if (j.contains("mapChanges") && j["mapChanges"].is_array()) {
        for (const auto& mc : j["mapChanges"]) {
            ws.mapChanges.push_back(TileChange::FromJson(mc));
        }
    }

    // Resource nodes
    if (j.contains("resourceNodes") && j["resourceNodes"].is_array()) {
        for (const auto& rn : j["resourceNodes"]) {
            ws.resourceNodes.push_back(ResourceNode::FromJson(rn));
        }
    }

    // Time tracking
    ws.lastUpdateTimestamp = j.value("lastUpdateTimestamp", 0LL);
    ws.lastLoginTimestamp = j.value("lastLoginTimestamp", 0LL);
    ws.createdTimestamp = j.value("createdTimestamp", 0LL);
    ws.totalPlayTime = j.value("totalPlayTime", 0.0f);

    // Territory
    if (j.contains("ownedTiles") && j["ownedTiles"].is_array()) {
        for (const auto& tile : j["ownedTiles"]) {
            if (tile.is_array() && tile.size() >= 2) {
                ws.ownedTiles.push_back({tile[0].get<int>(), tile[1].get<int>()});
            }
        }
    }
    ws.territoryStrength = j.value("territoryStrength", 0.0f);

    // Stats
    ws.totalZombiesKilled = j.value("totalZombiesKilled", 0);
    ws.totalBuildingsBuilt = j.value("totalBuildingsBuilt", 0);
    ws.totalWorkersHired = j.value("totalWorkersHired", 0);
    ws.attacksSurvived = j.value("attacksSurvived", 0);

    return ws;
}

Building* WorldState::GetBuilding(int id) {
    auto it = std::find_if(buildings.begin(), buildings.end(),
        [id](const Building& b) { return b.id == id; });
    return it != buildings.end() ? &(*it) : nullptr;
}

const Building* WorldState::GetBuilding(int id) const {
    auto it = std::find_if(buildings.begin(), buildings.end(),
        [id](const Building& b) { return b.id == id; });
    return it != buildings.end() ? &(*it) : nullptr;
}

Worker* WorldState::GetWorker(int id) {
    auto it = std::find_if(workers.begin(), workers.end(),
        [id](const Worker& w) { return w.id == id; });
    return it != workers.end() ? &(*it) : nullptr;
}

const Worker* WorldState::GetWorker(int id) const {
    auto it = std::find_if(workers.begin(), workers.end(),
        [id](const Worker& w) { return w.id == id; });
    return it != workers.end() ? &(*it) : nullptr;
}

int WorldState::GetTotalPopulation() const {
    return static_cast<int>(workers.size());
}

int WorldState::GetPopulationCapacity() const {
    int capacity = 5; // Base capacity
    for (const auto& b : buildings) {
        if (b.type == BuildingType::House && b.IsConstructed()) {
            capacity += 4 * b.level;
        } else if (b.type == BuildingType::Barracks && b.IsConstructed()) {
            capacity += 8 * b.level;
        }
    }
    return capacity;
}

int WorldState::GetIdleWorkers() const {
    int count = 0;
    for (const auto& w : workers) {
        if (w.IsIdle() && w.IsAlive()) {
            ++count;
        }
    }
    return count;
}

// ============================================================================
// PersistentWorld Implementation
// ============================================================================

PersistentWorld& PersistentWorld::Instance() {
    static PersistentWorld instance;
    return instance;
}

bool PersistentWorld::Initialize() {
    if (m_initialized) {
        RTS_LOG_WARN("PersistentWorld already initialized");
        return true;
    }

    RTS_LOG_INFO("Initializing PersistentWorld system");
    m_initialized = true;
    return true;
}

void PersistentWorld::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Save state before shutdown
    if (m_dirty && m_stateLoaded) {
        SaveState();
    }

    m_initialized = false;
    m_stateLoaded = false;
    RTS_LOG_INFO("PersistentWorld shutdown complete");
}

void PersistentWorld::Update(float deltaTime) {
    if (!m_initialized || !m_stateLoaded) {
        return;
    }

    // Update play time
    m_state.totalPlayTime += deltaTime / 3600.0f;

    // Process auto-save
    ProcessAutoSave(deltaTime);
}

void PersistentWorld::SaveState(StateSavedCallback callback) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state.playerId.empty()) {
        RTS_LOG_ERROR("Cannot save state: no player ID");
        if (callback) callback(false);
        return;
    }

    // Update timestamp
    m_state.lastUpdateTimestamp = GetCurrentTimestamp();

    // Convert to JSON and save
    nlohmann::json stateJson = m_state.ToJson();
    std::string path = GetPlayerStatePath(m_state.playerId);

    FirebaseManager::Instance().SetValue(path, stateJson,
        [this, callback](const FirebaseManager::Result& result) {
            if (result.success) {
                m_dirty = false;
                m_lastSaveTimestamp = GetCurrentTimestamp();
                RTS_LOG_INFO("World state saved successfully");
            } else {
                RTS_LOG_ERROR("Failed to save world state: " + result.errorMessage);
            }
            if (callback) callback(result.success);
        });
}

void PersistentWorld::LoadState(const std::string& playerId, StateLoadedCallback callback) {
    std::string path = GetPlayerStatePath(playerId);

    FirebaseManager::Instance().GetValue(path,
        [this, playerId, callback](const nlohmann::json& data) {
            std::lock_guard<std::mutex> lock(m_stateMutex);

            if (data.is_null() || data.empty()) {
                // New player - create initial state
                RTS_LOG_INFO("Creating new world state for player: " + playerId);
                m_state = WorldState();
                m_state.playerId = playerId;
                m_state.createdTimestamp = GetCurrentTimestamp();
                m_state.lastLoginTimestamp = GetCurrentTimestamp();
                m_state.lastUpdateTimestamp = GetCurrentTimestamp();

                // Initial hero
                m_state.hero.playerId = playerId;
                m_state.hero.name = "Hero";
                m_state.hero.isOnline = true;

                // Start with a command center
                Building cc;
                cc.id = GenerateBuildingId();
                cc.type = BuildingType::CommandCenter;
                cc.position = {0, 0};
                cc.size = {3, 3};
                cc.health = 500;
                cc.maxHealth = 500;
                cc.createdTimestamp = GetCurrentTimestamp();
                m_state.buildings.push_back(cc);

                // Start with 3 workers
                for (int i = 0; i < 3; ++i) {
                    Worker w;
                    w.id = GenerateWorkerId();
                    w.name = "Worker " + std::to_string(i + 1);
                    w.hiredTimestamp = GetCurrentTimestamp();
                    m_state.workers.push_back(w);
                }

                m_stateLoaded = true;
                m_dirty = true;

                if (callback) callback(true, m_state);
            } else {
                // Load existing state
                m_state = WorldState::FromJson(data);
                m_state.hero.isOnline = true;

                // Calculate offline time
                int64_t offlineSeconds = GetCurrentTimestamp() - m_state.lastUpdateTimestamp;
                m_state.lastLoginTimestamp = GetCurrentTimestamp();

                // Update max IDs
                for (const auto& b : m_state.buildings) {
                    m_nextBuildingId = std::max(m_nextBuildingId, b.id + 1);
                }
                for (const auto& w : m_state.workers) {
                    m_nextWorkerId = std::max(m_nextWorkerId, w.id + 1);
                }

                m_stateLoaded = true;
                RTS_LOG_INFO("Loaded world state for player: " + playerId);

                // Simulate offline time if significant
                if (offlineSeconds > 60) {
                    SimulateOfflineTime(offlineSeconds);
                }

                if (callback) callback(true, m_state);
            }
        });
}

void PersistentWorld::SimulateOfflineTime(int64_t secondsElapsed, OfflineReportCallback callback) {
    // Implemented in OfflineSimulation - just mark dirty here
    m_dirty = true;
    RTS_LOG_INFO("Simulating " + std::to_string(secondsElapsed / 3600) + " hours offline time");
}

int64_t PersistentWorld::GetSecondsOffline() const {
    return GetCurrentTimestamp() - m_state.lastUpdateTimestamp;
}

void PersistentWorld::SyncWithServer() {
    if (m_dirty) {
        SaveState();
    }
}

void PersistentWorld::ForceSyncNow() {
    SaveState();
}

// ==================== Building Management ====================

int PersistentWorld::PlaceBuilding(BuildingType type, const glm::ivec2& position) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (!CanAffordBuilding(type)) {
        RTS_LOG_WARN("Cannot afford building");
        return -1;
    }

    Building b;
    b.id = GenerateBuildingId();
    b.type = type;
    b.position = position;
    b.size = GetBuildingSize(type);
    b.maxHealth = GetBuildingMaxHealth(type);
    b.health = b.maxHealth;
    b.constructionProgress = 0.0f;  // Needs building time
    b.createdTimestamp = GetCurrentTimestamp();
    b.producesResource = GetBuildingResourceType(type);
    b.productionPerHour = GetBuildingProductionRate(type);

    PayBuildingCost(type);
    m_state.buildings.push_back(b);
    m_state.totalBuildingsBuilt++;
    m_dirty = true;

    RTS_LOG_INFO("Placed building: " + std::string(BuildingTypeToString(type)));
    return b.id;
}

bool PersistentWorld::RemoveBuilding(int buildingId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    auto it = std::find_if(m_state.buildings.begin(), m_state.buildings.end(),
        [buildingId](const Building& b) { return b.id == buildingId; });

    if (it != m_state.buildings.end()) {
        // Unassign workers
        for (auto& w : m_state.workers) {
            if (w.assignedBuildingId == buildingId) {
                w.assignedBuildingId = -1;
                w.job = WorkerJob::Idle;
            }
        }

        m_state.buildings.erase(it);
        m_dirty = true;
        return true;
    }
    return false;
}

bool PersistentWorld::UpgradeBuilding(int buildingId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    Building* b = m_state.GetBuilding(buildingId);
    if (!b || !b->IsConstructed()) {
        return false;
    }

    // Simple upgrade cost: level * base resources
    int woodCost = b->level * 50;
    int stoneCost = b->level * 30;

    if (!m_state.resources.CanAfford(ResourceType::Wood, woodCost) ||
        !m_state.resources.CanAfford(ResourceType::Stone, stoneCost)) {
        return false;
    }

    m_state.resources.Consume(ResourceType::Wood, woodCost);
    m_state.resources.Consume(ResourceType::Stone, stoneCost);

    b->level++;
    b->maxHealth += 50;
    b->health = b->maxHealth;
    b->productionPerHour *= 1.25f;  // 25% boost per level

    m_dirty = true;
    RecalculateProduction();

    RTS_LOG_INFO("Upgraded building " + std::to_string(buildingId) + " to level " + std::to_string(b->level));
    return true;
}

int PersistentWorld::RepairBuilding(int buildingId, int amount) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    Building* b = m_state.GetBuilding(buildingId);
    if (!b) return 0;

    int repairAmount = std::min(amount, b->maxHealth - b->health);
    b->health += repairAmount;
    m_dirty = true;
    return repairAmount;
}

// ==================== Worker Management ====================

int PersistentWorld::HireWorker() {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    // Check population capacity
    if (m_state.GetTotalPopulation() >= m_state.GetPopulationCapacity()) {
        RTS_LOG_WARN("Population at capacity");
        return -1;
    }

    // Cost to hire
    int foodCost = 20;
    if (!m_state.resources.CanAfford(ResourceType::Food, foodCost)) {
        RTS_LOG_WARN("Cannot afford worker");
        return -1;
    }

    m_state.resources.Consume(ResourceType::Food, foodCost);

    Worker w;
    w.id = GenerateWorkerId();
    w.name = "Worker " + std::to_string(m_state.totalWorkersHired + 1);
    w.hiredTimestamp = GetCurrentTimestamp();

    m_state.workers.push_back(w);
    m_state.totalWorkersHired++;
    m_dirty = true;

    RTS_LOG_INFO("Hired worker: " + w.name);
    return w.id;
}

bool PersistentWorld::FireWorker(int workerId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    auto it = std::find_if(m_state.workers.begin(), m_state.workers.end(),
        [workerId](const Worker& w) { return w.id == workerId; });

    if (it != m_state.workers.end()) {
        m_state.workers.erase(it);
        m_dirty = true;
        RecalculateProduction();
        return true;
    }
    return false;
}

bool PersistentWorld::AssignWorker(int workerId, WorkerJob job, int buildingId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    Worker* w = m_state.GetWorker(workerId);
    if (!w || !w->IsAlive()) {
        return false;
    }

    // Validate building if specified
    if (buildingId >= 0) {
        Building* b = m_state.GetBuilding(buildingId);
        if (!b || !b->IsConstructed()) {
            return false;
        }
    }

    // Remove from previous building
    if (w->assignedBuildingId >= 0) {
        Building* prevBuilding = m_state.GetBuilding(w->assignedBuildingId);
        if (prevBuilding) {
            prevBuilding->assignedWorkers = std::max(0, prevBuilding->assignedWorkers - 1);
        }
    }

    w->job = job;
    w->assignedBuildingId = buildingId;

    // Add to new building
    if (buildingId >= 0) {
        Building* newBuilding = m_state.GetBuilding(buildingId);
        if (newBuilding) {
            newBuilding->assignedWorkers++;
        }
    }

    m_dirty = true;
    RecalculateProduction();
    return true;
}

void PersistentWorld::AutoAssignWorkers() {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    // Get idle workers
    std::vector<Worker*> idleWorkers;
    for (auto& w : m_state.workers) {
        if (w.IsIdle() && w.IsAlive()) {
            idleWorkers.push_back(&w);
        }
    }

    if (idleWorkers.empty()) {
        return;
    }

    // Assign to production buildings that need workers
    for (auto& b : m_state.buildings) {
        if (!b.IsConstructed() || b.IsDestroyed()) {
            continue;
        }

        // Production buildings can have workers
        bool isProduction = (b.type == BuildingType::Farm ||
                            b.type == BuildingType::Sawmill ||
                            b.type == BuildingType::Quarry ||
                            b.type == BuildingType::Mine ||
                            b.type == BuildingType::Refinery ||
                            b.type == BuildingType::Hospital ||
                            b.type == BuildingType::Armory);

        if (isProduction && b.assignedWorkers < b.level * 2) {
            if (!idleWorkers.empty()) {
                Worker* w = idleWorkers.back();
                idleWorkers.pop_back();

                w->job = WorkerJob::Gathering;
                w->assignedBuildingId = b.id;
                b.assignedWorkers++;
            }
        }
    }

    m_dirty = true;
    RecalculateProduction();
}

// ==================== Resource Management ====================

int PersistentWorld::AddResources(ResourceType type, int amount) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    int current = m_state.resources.Get(type);
    int capacity = m_state.resources.GetCapacity(type);
    int actual = std::min(amount, capacity - current);

    m_state.resources.Add(type, actual);
    m_dirty = true;
    return actual;
}

bool PersistentWorld::SpendResources(ResourceType type, int amount) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    if (m_state.resources.Consume(type, amount)) {
        m_dirty = true;
        return true;
    }
    return false;
}

bool PersistentWorld::CanAfford(ResourceType type, int amount) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_state.resources.CanAfford(type, amount);
}

float PersistentWorld::CalculateProductionRate(ResourceType type) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    float rate = 0.0f;
    for (const auto& b : m_state.buildings) {
        if (b.IsConstructed() && b.isActive && b.producesResource == type) {
            float workerBonus = 1.0f + (b.assignedWorkers * 0.5f);
            rate += b.productionPerHour * workerBonus;
        }
    }
    return rate;
}

float PersistentWorld::CalculateConsumptionRate(ResourceType type) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    float rate = 0.0f;

    if (type == ResourceType::Food) {
        // Workers consume food
        rate += m_state.workers.size() * 2.0f;  // 2 food per worker per hour
    }

    return rate;
}

// ==================== Map Changes ====================

void PersistentWorld::RecordTileChange(const glm::ivec2& position, int newTileType) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    TileChange tc;
    tc.position = position;
    tc.newTileType = newTileType;
    tc.changedBy = m_state.playerId;
    tc.timestamp = GetCurrentTimestamp();

    m_state.mapChanges.push_back(tc);
    m_dirty = true;

    // Limit stored changes
    if (m_state.mapChanges.size() > 1000) {
        m_state.mapChanges.erase(m_state.mapChanges.begin());
    }
}

std::vector<TileChange> PersistentWorld::GetMapChangesSince(int64_t timestamp) const {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    std::vector<TileChange> changes;
    for (const auto& tc : m_state.mapChanges) {
        if (tc.timestamp >= timestamp) {
            changes.push_back(tc);
        }
    }
    return changes;
}

// ==================== Timestamps ====================

int64_t PersistentWorld::GetCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ==================== Private Helpers ====================

std::string PersistentWorld::GetPlayerStatePath(const std::string& playerId) const {
    return "rts/players/" + playerId + "/world";
}

std::string PersistentWorld::GetWorldEventsPath() const {
    return "rts/events";
}

void PersistentWorld::RecalculateProduction() {
    for (size_t i = 0; i < static_cast<size_t>(ResourceType::Count); ++i) {
        ResourceType type = static_cast<ResourceType>(i);
        m_state.resources.SetProductionRate(type, CalculateProductionRate(type));
        m_state.resources.SetConsumptionRate(type, CalculateConsumptionRate(type));
    }
}

void PersistentWorld::ProcessAutoSave(float deltaTime) {
    if (m_autoSaveInterval <= 0.0f) {
        return;
    }

    m_autoSaveTimer += deltaTime;
    if (m_autoSaveTimer >= m_autoSaveInterval && m_dirty) {
        m_autoSaveTimer = 0.0f;
        SaveState();
    }
}

int PersistentWorld::GenerateBuildingId() {
    return m_nextBuildingId++;
}

int PersistentWorld::GenerateWorkerId() {
    return m_nextWorkerId++;
}

bool PersistentWorld::CanAffordBuilding(BuildingType type) const {
    // Define building costs
    switch (type) {
        case BuildingType::Farm:
            return m_state.resources.CanAfford(ResourceType::Wood, 30);
        case BuildingType::Sawmill:
            return m_state.resources.CanAfford(ResourceType::Wood, 40) &&
                   m_state.resources.CanAfford(ResourceType::Stone, 20);
        case BuildingType::Quarry:
            return m_state.resources.CanAfford(ResourceType::Wood, 50);
        case BuildingType::Mine:
            return m_state.resources.CanAfford(ResourceType::Wood, 60) &&
                   m_state.resources.CanAfford(ResourceType::Stone, 40);
        case BuildingType::House:
            return m_state.resources.CanAfford(ResourceType::Wood, 40) &&
                   m_state.resources.CanAfford(ResourceType::Stone, 20);
        case BuildingType::Wall:
            return m_state.resources.CanAfford(ResourceType::Stone, 20);
        case BuildingType::Tower:
            return m_state.resources.CanAfford(ResourceType::Stone, 50) &&
                   m_state.resources.CanAfford(ResourceType::Metal, 20);
        case BuildingType::Warehouse:
            return m_state.resources.CanAfford(ResourceType::Wood, 60) &&
                   m_state.resources.CanAfford(ResourceType::Stone, 30);
        default:
            return m_state.resources.CanAfford(ResourceType::Wood, 50);
    }
}

void PersistentWorld::PayBuildingCost(BuildingType type) {
    switch (type) {
        case BuildingType::Farm:
            m_state.resources.Consume(ResourceType::Wood, 30);
            break;
        case BuildingType::Sawmill:
            m_state.resources.Consume(ResourceType::Wood, 40);
            m_state.resources.Consume(ResourceType::Stone, 20);
            break;
        case BuildingType::Quarry:
            m_state.resources.Consume(ResourceType::Wood, 50);
            break;
        case BuildingType::Mine:
            m_state.resources.Consume(ResourceType::Wood, 60);
            m_state.resources.Consume(ResourceType::Stone, 40);
            break;
        case BuildingType::House:
            m_state.resources.Consume(ResourceType::Wood, 40);
            m_state.resources.Consume(ResourceType::Stone, 20);
            break;
        case BuildingType::Wall:
            m_state.resources.Consume(ResourceType::Stone, 20);
            break;
        case BuildingType::Tower:
            m_state.resources.Consume(ResourceType::Stone, 50);
            m_state.resources.Consume(ResourceType::Metal, 20);
            break;
        case BuildingType::Warehouse:
            m_state.resources.Consume(ResourceType::Wood, 60);
            m_state.resources.Consume(ResourceType::Stone, 30);
            break;
        default:
            m_state.resources.Consume(ResourceType::Wood, 50);
            break;
    }
}

glm::ivec2 PersistentWorld::GetBuildingSize(BuildingType type) const {
    switch (type) {
        case BuildingType::Wall:
        case BuildingType::Gate:
            return {1, 1};
        case BuildingType::Tower:
        case BuildingType::Farm:
        case BuildingType::House:
            return {2, 2};
        case BuildingType::CommandCenter:
        case BuildingType::Warehouse:
        case BuildingType::Barracks:
            return {3, 3};
        default:
            return {2, 2};
    }
}

int PersistentWorld::GetBuildingMaxHealth(BuildingType type) const {
    switch (type) {
        case BuildingType::Wall:
            return 200;
        case BuildingType::Tower:
            return 150;
        case BuildingType::Bunker:
            return 400;
        case BuildingType::CommandCenter:
            return 500;
        case BuildingType::House:
            return 100;
        default:
            return 100;
    }
}

float PersistentWorld::GetBuildingProductionRate(BuildingType type) const {
    switch (type) {
        case BuildingType::Farm:
            return 10.0f;   // Food per hour
        case BuildingType::Sawmill:
            return 8.0f;    // Wood per hour
        case BuildingType::Quarry:
            return 6.0f;    // Stone per hour
        case BuildingType::Mine:
            return 4.0f;    // Metal per hour
        case BuildingType::Refinery:
            return 3.0f;    // Fuel per hour
        case BuildingType::Hospital:
            return 2.0f;    // Medicine per hour
        case BuildingType::Armory:
            return 3.0f;    // Ammunition per hour
        default:
            return 0.0f;
    }
}

ResourceType PersistentWorld::GetBuildingResourceType(BuildingType type) const {
    switch (type) {
        case BuildingType::Farm:
            return ResourceType::Food;
        case BuildingType::Sawmill:
            return ResourceType::Wood;
        case BuildingType::Quarry:
            return ResourceType::Stone;
        case BuildingType::Mine:
            return ResourceType::Metal;
        case BuildingType::Refinery:
            return ResourceType::Fuel;
        case BuildingType::Hospital:
            return ResourceType::Medicine;
        case BuildingType::Armory:
            return ResourceType::Ammunition;
        default:
            return ResourceType::Food;
    }
}

} // namespace Vehement
