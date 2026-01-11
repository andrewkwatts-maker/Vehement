#include "PlayerDatabase.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

// Simple hash function (use proper crypto library in production)
#include <functional>

namespace Nova {

PlayerDatabase::PlayerDatabase(WorldDatabase* database)
    : m_database(database) {}

int PlayerDatabase::CreatePlayer(const std::string& username, const std::string& password) {
    if (!m_database || username.empty()) return -1;

    if (PlayerExists(username)) {
        return -1; // Player already exists
    }

    int playerId = m_database->CreatePlayer(username);
    if (playerId < 0) return -1;

    // Set password
    Player player = m_database->LoadPlayerById(playerId);
    player.passwordHash = HashPassword(password);
    m_database->SavePlayer(player);

    return playerId;
}

int PlayerDatabase::AuthenticatePlayer(const std::string& username, const std::string& password) {
    if (!m_database) return -1;

    Player player = m_database->LoadPlayer(username);
    if (player.playerId < 0) return -1;

    if (VerifyPassword(password, player.passwordHash)) {
        return player.playerId;
    }

    return -1;
}

Player PlayerDatabase::GetPlayer(const std::string& username) {
    return m_database ? m_database->LoadPlayer(username) : Player{};
}

Player PlayerDatabase::GetPlayerById(int playerId) {
    return m_database ? m_database->LoadPlayerById(playerId) : Player{};
}

bool PlayerDatabase::UpdatePlayer(const Player& player) {
    return m_database ? m_database->SavePlayer(player) : false;
}

bool PlayerDatabase::DeletePlayer(int playerId) {
    return m_database ? m_database->DeletePlayer(playerId) : false;
}

bool PlayerDatabase::PlayerExists(const std::string& username) {
    return m_database ? m_database->PlayerExists(username) : false;
}

bool PlayerDatabase::SetPlayerOnline(int playerId, bool online) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.isOnline = online;
    if (online) {
        player.lastLogin = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    } else {
        player.lastLogout = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    return m_database->SavePlayer(player);
}

std::vector<Player> PlayerDatabase::GetOnlinePlayers() {
    return m_database ? m_database->GetOnlinePlayers() : std::vector<Player>{};
}

bool PlayerDatabase::UpdatePlayerPosition(int playerId, glm::vec3 position) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0 || player.entityId < 0) return false;

    Entity entity = m_database->LoadEntity(player.entityId);
    entity.position = position;
    m_database->SaveEntity(entity);

    return true;
}

bool PlayerDatabase::UpdatePlayerHealth(int playerId, float health) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.health = std::clamp(health, 0.0f, player.maxHealth);
    return m_database->SavePlayer(player);
}

bool PlayerDatabase::UpdatePlayerStats(int playerId, const std::vector<uint8_t>& stats) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.stats = stats;
    return m_database->SavePlayer(player);
}

std::vector<InventorySlot> PlayerDatabase::GetInventory(int playerId) {
    return m_database ? m_database->LoadInventory(playerId) : std::vector<InventorySlot>{};
}

bool PlayerDatabase::UpdateInventory(int playerId, const std::vector<InventorySlot>& inventory) {
    return m_database ? m_database->SaveInventory(playerId, inventory) : false;
}

bool PlayerDatabase::AddItem(int playerId, const std::string& itemId, int quantity) {
    if (!m_database || quantity <= 0) return false;

    auto inventory = GetInventory(playerId);

    // Try to stack with existing item
    for (auto& slot : inventory) {
        if (slot.itemId == itemId && !slot.isLocked) {
            slot.quantity += quantity;
            return UpdateInventory(playerId, inventory);
        }
    }

    // Find empty slot
    int nextSlot = 0;
    for (const auto& slot : inventory) {
        nextSlot = std::max(nextSlot, slot.slotIndex + 1);
    }

    InventorySlot newSlot;
    newSlot.slotIndex = nextSlot;
    newSlot.itemId = itemId;
    newSlot.quantity = quantity;
    newSlot.acquiredAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    inventory.push_back(newSlot);
    return UpdateInventory(playerId, inventory);
}

bool PlayerDatabase::RemoveItem(int playerId, int slotIndex, int quantity) {
    if (!m_database || quantity <= 0) return false;

    auto inventory = GetInventory(playerId);
    auto it = std::find_if(inventory.begin(), inventory.end(),
        [slotIndex](const InventorySlot& slot) { return slot.slotIndex == slotIndex; });

    if (it == inventory.end()) return false;

    it->quantity -= quantity;
    if (it->quantity <= 0) {
        inventory.erase(it);
    }

    return UpdateInventory(playerId, inventory);
}

int PlayerDatabase::GetItemCount(int playerId, const std::string& itemId) {
    auto inventory = GetInventory(playerId);
    int count = 0;
    for (const auto& slot : inventory) {
        if (slot.itemId == itemId) {
            count += slot.quantity;
        }
    }
    return count;
}

std::map<std::string, EquipmentSlot> PlayerDatabase::GetEquipment(int playerId) {
    return m_database ? m_database->LoadEquipment(playerId) : std::map<std::string, EquipmentSlot>{};
}

bool PlayerDatabase::UpdateEquipment(int playerId, const std::map<std::string, EquipmentSlot>& equipment) {
    return m_database ? m_database->SaveEquipment(playerId, equipment) : false;
}

bool PlayerDatabase::EquipItem(int playerId, const std::string& slotName, const std::string& itemId) {
    if (!m_database) return false;

    auto equipment = GetEquipment(playerId);
    EquipmentSlot slot;
    slot.slotName = slotName;
    slot.itemId = itemId;
    slot.equippedAt = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    equipment[slotName] = slot;
    return UpdateEquipment(playerId, equipment);
}

bool PlayerDatabase::UnequipItem(int playerId, const std::string& slotName) {
    if (!m_database) return false;

    auto equipment = GetEquipment(playerId);
    equipment.erase(slotName);
    return UpdateEquipment(playerId, equipment);
}

bool PlayerDatabase::AddExperience(int playerId, int amount) {
    if (!m_database || amount <= 0) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.experience += amount;

    // Check for level up (simple formula)
    int xpForNextLevel = player.level * 1000;
    if (player.experience >= xpForNextLevel) {
        player.experience -= xpForNextLevel;
        LevelUp(playerId);
    }

    return m_database->SavePlayer(player);
}

bool PlayerDatabase::LevelUp(int playerId) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.level++;
    player.maxHealth += 10.0f;
    player.maxMana += 5.0f;
    player.health = player.maxHealth;
    player.mana = player.maxMana;

    return m_database->SavePlayer(player);
}

bool PlayerDatabase::UpdateSkills(int playerId, const std::vector<uint8_t>& skills) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.skills = skills;
    return m_database->SavePlayer(player);
}

bool PlayerDatabase::UnlockAchievement(int playerId, const std::string& achievementId) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    // Achievements stored in player.achievements blob
    // Simple implementation: append achievement ID
    std::string achievements(player.achievements.begin(), player.achievements.end());
    achievements += achievementId + ";";
    player.achievements.assign(achievements.begin(), achievements.end());

    return m_database->SavePlayer(player);
}

bool PlayerDatabase::AddGold(int playerId, int amount) {
    if (!m_database || amount <= 0) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.currencyGold += amount;
    return m_database->SavePlayer(player);
}

bool PlayerDatabase::RemoveGold(int playerId, int amount) {
    if (!m_database || amount <= 0) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    if (player.currencyGold < amount) return false;

    player.currencyGold -= amount;
    return m_database->SavePlayer(player);
}

int PlayerDatabase::GetGold(int playerId) {
    Player player = GetPlayerById(playerId);
    return player.currencyGold;
}

bool PlayerDatabase::RecordDeath(int playerId) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.deaths++;
    return m_database->SavePlayer(player);
}

bool PlayerDatabase::RecordKill(int playerId) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.kills++;
    return m_database->SavePlayer(player);
}

bool PlayerDatabase::UpdatePlayTime(int playerId, uint64_t seconds) {
    if (!m_database) return false;

    Player player = m_database->LoadPlayerById(playerId);
    if (player.playerId < 0) return false;

    player.playTimeSeconds = seconds;
    return m_database->SavePlayer(player);
}

PlayerDatabase::PlayerStats PlayerDatabase::GetPlayerStats(int playerId) {
    PlayerStats stats;
    Player player = GetPlayerById(playerId);

    stats.level = player.level;
    stats.experience = player.experience;
    stats.deaths = player.deaths;
    stats.kills = player.kills;
    stats.playTime = player.playTimeSeconds;
    stats.goldEarned = player.currencyGold;

    return stats;
}

std::vector<Player> PlayerDatabase::GetTopPlayersByLevel(int limit) {
    if (!m_database) return {};

    auto players = m_database->GetAllPlayers();
    std::sort(players.begin(), players.end(),
        [](const Player& a, const Player& b) { return a.level > b.level; });

    if (players.size() > static_cast<size_t>(limit)) {
        players.resize(limit);
    }

    return players;
}

std::vector<Player> PlayerDatabase::GetTopPlayersByKills(int limit) {
    if (!m_database) return {};

    auto players = m_database->GetAllPlayers();
    std::sort(players.begin(), players.end(),
        [](const Player& a, const Player& b) { return a.kills > b.kills; });

    if (players.size() > static_cast<size_t>(limit)) {
        players.resize(limit);
    }

    return players;
}

std::vector<Player> PlayerDatabase::GetTopPlayersByPlayTime(int limit) {
    if (!m_database) return {};

    auto players = m_database->GetAllPlayers();
    std::sort(players.begin(), players.end(),
        [](const Player& a, const Player& b) { return a.playTimeSeconds > b.playTimeSeconds; });

    if (players.size() > static_cast<size_t>(limit)) {
        players.resize(limit);
    }

    return players;
}

std::string PlayerDatabase::HashPassword(const std::string& password) {
    // Simple hash for demo (use bcrypt/argon2 in production)
    std::hash<std::string> hasher;
    size_t hash = hasher(password + "salt");
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

bool PlayerDatabase::VerifyPassword(const std::string& password, const std::string& hash) {
    return HashPassword(password) == hash;
}

} // namespace Nova
