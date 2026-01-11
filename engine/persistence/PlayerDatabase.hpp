#pragma once

#include "WorldDatabase.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Nova {

/**
 * @brief Player database wrapper for common player operations
 *
 * Provides high-level player management operations built on WorldDatabase.
 * Includes inventory, equipment, stats, skills, quests, and achievements.
 */
class PlayerDatabase {
public:
    explicit PlayerDatabase(WorldDatabase* database);
    ~PlayerDatabase() = default;

    // =========================================================================
    // PLAYER MANAGEMENT
    // =========================================================================

    /**
     * @brief Create new player
     * @param username Unique username
     * @param password Password (will be hashed)
     * @return Player ID, or -1 on failure
     */
    int CreatePlayer(const std::string& username, const std::string& password);

    /**
     * @brief Authenticate player
     * @param username Username
     * @param password Password
     * @return Player ID if authenticated, -1 otherwise
     */
    int AuthenticatePlayer(const std::string& username, const std::string& password);

    /**
     * @brief Get player by username
     */
    Player GetPlayer(const std::string& username);

    /**
     * @brief Get player by ID
     */
    Player GetPlayerById(int playerId);

    /**
     * @brief Update player
     */
    bool UpdatePlayer(const Player& player);

    /**
     * @brief Delete player
     */
    bool DeletePlayer(int playerId);

    /**
     * @brief Check if player exists
     */
    bool PlayerExists(const std::string& username);

    // =========================================================================
    // PLAYER STATE
    // =========================================================================

    /**
     * @brief Set player online status
     */
    bool SetPlayerOnline(int playerId, bool online);

    /**
     * @brief Get online players
     */
    std::vector<Player> GetOnlinePlayers();

    /**
     * @brief Update player position
     */
    bool UpdatePlayerPosition(int playerId, glm::vec3 position);

    /**
     * @brief Update player health
     */
    bool UpdatePlayerHealth(int playerId, float health);

    /**
     * @brief Update player stats
     */
    bool UpdatePlayerStats(int playerId, const std::vector<uint8_t>& stats);

    // =========================================================================
    // INVENTORY MANAGEMENT
    // =========================================================================

    /**
     * @brief Get player inventory
     */
    std::vector<InventorySlot> GetInventory(int playerId);

    /**
     * @brief Update inventory
     */
    bool UpdateInventory(int playerId, const std::vector<InventorySlot>& inventory);

    /**
     * @brief Add item to inventory
     */
    bool AddItem(int playerId, const std::string& itemId, int quantity = 1);

    /**
     * @brief Remove item from inventory
     */
    bool RemoveItem(int playerId, int slotIndex, int quantity = 1);

    /**
     * @brief Get item count
     */
    int GetItemCount(int playerId, const std::string& itemId);

    // =========================================================================
    // EQUIPMENT MANAGEMENT
    // =========================================================================

    /**
     * @brief Get player equipment
     */
    std::map<std::string, EquipmentSlot> GetEquipment(int playerId);

    /**
     * @brief Update equipment
     */
    bool UpdateEquipment(int playerId, const std::map<std::string, EquipmentSlot>& equipment);

    /**
     * @brief Equip item
     */
    bool EquipItem(int playerId, const std::string& slotName, const std::string& itemId);

    /**
     * @brief Unequip item
     */
    bool UnequipItem(int playerId, const std::string& slotName);

    // =========================================================================
    // PROGRESSION
    // =========================================================================

    /**
     * @brief Add experience
     */
    bool AddExperience(int playerId, int amount);

    /**
     * @brief Level up player
     */
    bool LevelUp(int playerId);

    /**
     * @brief Update skills
     */
    bool UpdateSkills(int playerId, const std::vector<uint8_t>& skills);

    /**
     * @brief Unlock achievement
     */
    bool UnlockAchievement(int playerId, const std::string& achievementId);

    // =========================================================================
    // CURRENCY
    // =========================================================================

    /**
     * @brief Add gold
     */
    bool AddGold(int playerId, int amount);

    /**
     * @brief Remove gold
     */
    bool RemoveGold(int playerId, int amount);

    /**
     * @brief Get gold
     */
    int GetGold(int playerId);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Record player death
     */
    bool RecordDeath(int playerId);

    /**
     * @brief Record player kill
     */
    bool RecordKill(int playerId);

    /**
     * @brief Update play time
     */
    bool UpdatePlayTime(int playerId, uint64_t seconds);

    /**
     * @brief Get player statistics
     */
    struct PlayerStats {
        int level = 1;
        int experience = 0;
        int deaths = 0;
        int kills = 0;
        uint64_t playTime = 0;
        int goldEarned = 0;
    };
    PlayerStats GetPlayerStats(int playerId);

    // =========================================================================
    // LEADERBOARD
    // =========================================================================

    /**
     * @brief Get top players by level
     */
    std::vector<Player> GetTopPlayersByLevel(int limit = 10);

    /**
     * @brief Get top players by kills
     */
    std::vector<Player> GetTopPlayersByKills(int limit = 10);

    /**
     * @brief Get top players by play time
     */
    std::vector<Player> GetTopPlayersByPlayTime(int limit = 10);

private:
    WorldDatabase* m_database = nullptr;

    // Helper functions
    std::string HashPassword(const std::string& password);
    bool VerifyPassword(const std::string& password, const std::string& hash);
};

} // namespace Nova
