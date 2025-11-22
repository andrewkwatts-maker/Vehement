#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

namespace Vehement {
namespace RTS {

// Forward declarations
class Hero;

/**
 * @brief Item slot types in the inventory
 */
enum class ItemSlot : uint8_t {
    Slot1 = 0,
    Slot2,
    Slot3,
    Slot4,
    Slot5,
    Slot6,

    COUNT
};

/**
 * @brief Item categories
 */
enum class ItemType : uint8_t {
    Weapon,         // Offensive items (swords, staffs)
    Armor,          // Defensive items (shields, plate)
    Accessory,      // Utility items (rings, amulets)
    Consumable,     // Single-use items (potions, scrolls)
    Material,       // Crafting materials
    Quest,          // Quest items (cannot be dropped)

    COUNT
};

/**
 * @brief Item rarity tiers
 */
enum class ItemRarity : uint8_t {
    Common,         // White - basic items
    Uncommon,       // Green - slightly enhanced
    Rare,           // Blue - notable bonuses
    Epic,           // Purple - powerful items
    Legendary,      // Orange - game-changing items

    COUNT
};

/**
 * @brief Get color for rarity display
 */
inline glm::vec4 GetRarityColor(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:    return {1.0f, 1.0f, 1.0f, 1.0f};     // White
        case ItemRarity::Uncommon:  return {0.0f, 1.0f, 0.0f, 1.0f};     // Green
        case ItemRarity::Rare:      return {0.0f, 0.5f, 1.0f, 1.0f};     // Blue
        case ItemRarity::Epic:      return {0.6f, 0.0f, 0.8f, 1.0f};     // Purple
        case ItemRarity::Legendary: return {1.0f, 0.5f, 0.0f, 1.0f};     // Orange
        default:                    return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

inline const char* GetRarityName(ItemRarity rarity) {
    switch (rarity) {
        case ItemRarity::Common:    return "Common";
        case ItemRarity::Uncommon:  return "Uncommon";
        case ItemRarity::Rare:      return "Rare";
        case ItemRarity::Epic:      return "Epic";
        case ItemRarity::Legendary: return "Legendary";
        default:                    return "Unknown";
    }
}

/**
 * @brief Stat bonuses provided by items
 */
struct ItemStats {
    // Primary stats
    float strength = 0.0f;
    float agility = 0.0f;
    float intelligence = 0.0f;

    // Combat stats
    float damage = 0.0f;
    float armor = 0.0f;
    float attackSpeed = 0.0f;

    // Resource stats
    float health = 0.0f;
    float mana = 0.0f;
    float healthRegen = 0.0f;
    float manaRegen = 0.0f;

    // Utility stats
    float moveSpeed = 0.0f;
    float cooldownReduction = 0.0f;
    float visionRange = 0.0f;
    float commandRadius = 0.0f;

    // Economic stats
    float goldBonus = 0.0f;         // % bonus gold
    float experienceBonus = 0.0f;   // % bonus XP

    ItemStats operator+(const ItemStats& other) const {
        ItemStats result;
        result.strength = strength + other.strength;
        result.agility = agility + other.agility;
        result.intelligence = intelligence + other.intelligence;
        result.damage = damage + other.damage;
        result.armor = armor + other.armor;
        result.attackSpeed = attackSpeed + other.attackSpeed;
        result.health = health + other.health;
        result.mana = mana + other.mana;
        result.healthRegen = healthRegen + other.healthRegen;
        result.manaRegen = manaRegen + other.manaRegen;
        result.moveSpeed = moveSpeed + other.moveSpeed;
        result.cooldownReduction = cooldownReduction + other.cooldownReduction;
        result.visionRange = visionRange + other.visionRange;
        result.commandRadius = commandRadius + other.commandRadius;
        result.goldBonus = goldBonus + other.goldBonus;
        result.experienceBonus = experienceBonus + other.experienceBonus;
        return result;
    }

    [[nodiscard]] bool HasAnyBonus() const {
        return strength != 0 || agility != 0 || intelligence != 0 ||
               damage != 0 || armor != 0 || attackSpeed != 0 ||
               health != 0 || mana != 0 || healthRegen != 0 || manaRegen != 0 ||
               moveSpeed != 0 || cooldownReduction != 0 ||
               visionRange != 0 || commandRadius != 0 ||
               goldBonus != 0 || experienceBonus != 0;
    }
};

/**
 * @brief Active ability on an item
 */
struct ItemActiveAbility {
    int abilityId = -1;             // Links to Ability system
    float cooldown = 0.0f;          // Item-specific cooldown
    float currentCooldown = 0.0f;   // Time remaining
    int charges = -1;               // -1 = unlimited, 0+ = remaining charges
    int maxCharges = -1;

    [[nodiscard]] bool IsReady() const { return currentCooldown <= 0.0f && (charges < 0 || charges > 0); }
    [[nodiscard]] bool HasCharges() const { return charges < 0 || charges > 0; }
};

/**
 * @brief Complete item definition
 */
struct ItemData {
    // Identification
    int id = -1;
    std::string name;
    std::string description;
    std::string lore;               // Flavor text
    std::string iconPath;

    // Classification
    ItemType type = ItemType::Accessory;
    ItemRarity rarity = ItemRarity::Common;

    // Stats
    ItemStats stats;

    // Active ability (if any)
    ItemActiveAbility activeAbility;
    bool hasActive = false;

    // Stacking
    bool stackable = false;
    int maxStack = 1;

    // Economy
    int buyPrice = 0;               // Gold to purchase
    int sellPrice = 0;              // Gold when selling

    // Requirements
    int requiredLevel = 1;
    int requiredStrength = 0;
    int requiredAgility = 0;
    int requiredIntelligence = 0;

    // Flags
    bool droppable = true;          // Can be dropped
    bool tradeable = true;          // Can be traded
    bool consumeOnUse = false;      // Destroyed when used

    // Crafting
    std::vector<int> craftComponents;   // Item IDs needed to craft
    int craftResult = -1;               // What this upgrades into
};

/**
 * @brief Instance of an item in inventory
 */
struct ItemInstance {
    int itemId = -1;                // Reference to ItemData
    int quantity = 1;               // Stack size
    float cooldownRemaining = 0.0f; // For active items
    int chargesRemaining = -1;      // For charged items

    [[nodiscard]] bool IsEmpty() const { return itemId < 0; }
    [[nodiscard]] bool IsReady() const { return cooldownRemaining <= 0.0f; }

    void Clear() {
        itemId = -1;
        quantity = 1;
        cooldownRemaining = 0.0f;
        chargesRemaining = -1;
    }
};

/**
 * @brief Hero inventory system
 *
 * Manages 6 item slots, item usage, stat aggregation,
 * and item-based active abilities.
 */
class HeroInventory {
public:
    static constexpr int SLOT_COUNT = 6;

    using ItemUseCallback = std::function<void(int itemId, int slot)>;
    using ItemChangeCallback = std::function<void(int slot, int oldItemId, int newItemId)>;

    HeroInventory();
    ~HeroInventory() = default;

    /**
     * @brief Set the owning hero (for stat requirements)
     */
    void SetOwner(Hero* owner) { m_owner = owner; }

    // =========================================================================
    // Item Management
    // =========================================================================

    /**
     * @brief Add an item to the inventory
     * @param itemId Item to add
     * @param preferredSlot Slot to try first (-1 for any)
     * @return Slot where item was placed, or -1 if failed
     */
    int AddItem(int itemId, int preferredSlot = -1);

    /**
     * @brief Remove item from a slot
     * @param slot Slot to clear
     * @return The item that was removed
     */
    ItemInstance RemoveItem(int slot);

    /**
     * @brief Swap items between two slots
     */
    bool SwapItems(int slotA, int slotB);

    /**
     * @brief Move item to different slot
     */
    bool MoveItem(int fromSlot, int toSlot);

    /**
     * @brief Drop item from inventory
     * @return true if item was droppable and removed
     */
    bool DropItem(int slot);

    /**
     * @brief Check if slot has an item
     */
    [[nodiscard]] bool HasItem(int slot) const;

    /**
     * @brief Check if inventory contains a specific item
     */
    [[nodiscard]] bool ContainsItem(int itemId) const;

    /**
     * @brief Find slot containing an item
     * @return Slot index or -1 if not found
     */
    [[nodiscard]] int FindItem(int itemId) const;

    /**
     * @brief Get item in a slot
     */
    [[nodiscard]] const ItemInstance& GetItem(int slot) const;

    /**
     * @brief Get number of empty slots
     */
    [[nodiscard]] int GetEmptySlotCount() const;

    /**
     * @brief Check if inventory is full
     */
    [[nodiscard]] bool IsFull() const { return GetEmptySlotCount() == 0; }

    // =========================================================================
    // Item Usage
    // =========================================================================

    /**
     * @brief Use item in slot (active ability or consumable)
     * @return true if item was used successfully
     */
    bool UseItem(int slot);

    /**
     * @brief Use item on a target
     */
    bool UseItemOnTarget(int slot, Entity* target);

    /**
     * @brief Use item at a location
     */
    bool UseItemAtPoint(int slot, const glm::vec3& point);

    /**
     * @brief Check if item can be used
     */
    [[nodiscard]] bool CanUseItem(int slot) const;

    /**
     * @brief Get cooldown remaining on item
     */
    [[nodiscard]] float GetItemCooldown(int slot) const;

    // =========================================================================
    // Stats
    // =========================================================================

    /**
     * @brief Get combined stats from all equipped items
     */
    [[nodiscard]] ItemStats GetTotalStats() const;

    /**
     * @brief Recalculate cached stats (call after item changes)
     */
    void RecalculateStats();

    /**
     * @brief Get cached total stats
     */
    [[nodiscard]] const ItemStats& GetCachedStats() const { return m_cachedStats; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update cooldowns
     */
    void Update(float deltaTime);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void SetOnItemUse(ItemUseCallback callback) { m_onItemUse = std::move(callback); }
    void SetOnItemChange(ItemChangeCallback callback) { m_onItemChange = std::move(callback); }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Clear all items
     */
    void Clear();

    /**
     * @brief Get all items for saving
     */
    [[nodiscard]] const std::array<ItemInstance, SLOT_COUNT>& GetAllItems() const { return m_items; }

private:
    bool MeetsRequirements(const ItemData& item) const;
    int FindEmptySlot() const;
    void NotifyItemChange(int slot, int oldId, int newId);

    Hero* m_owner = nullptr;
    std::array<ItemInstance, SLOT_COUNT> m_items;
    ItemStats m_cachedStats;

    ItemUseCallback m_onItemUse;
    ItemChangeCallback m_onItemChange;
};

/**
 * @brief Global item database
 */
class ItemDatabase {
public:
    static ItemDatabase& Instance() {
        static ItemDatabase instance;
        return instance;
    }

    /**
     * @brief Initialize item database
     */
    void Initialize();

    /**
     * @brief Get item data by ID
     */
    [[nodiscard]] const ItemData* GetItem(int id) const;

    /**
     * @brief Get all items of a type
     */
    [[nodiscard]] std::vector<const ItemData*> GetItemsByType(ItemType type) const;

    /**
     * @brief Get all items of a rarity
     */
    [[nodiscard]] std::vector<const ItemData*> GetItemsByRarity(ItemRarity rarity) const;

    /**
     * @brief Get items available at a shop level
     */
    [[nodiscard]] std::vector<const ItemData*> GetShopItems(int shopLevel) const;

    /**
     * @brief Get number of items in database
     */
    [[nodiscard]] size_t GetItemCount() const { return m_items.size(); }

private:
    ItemDatabase() = default;
    void RegisterDefaultItems();

    std::vector<ItemData> m_items;
};

// ============================================================================
// Item IDs
// ============================================================================

namespace ItemId {
    // Consumables
    constexpr int HEALTH_POTION_SMALL = 0;
    constexpr int HEALTH_POTION_LARGE = 1;
    constexpr int MANA_POTION_SMALL = 2;
    constexpr int MANA_POTION_LARGE = 3;
    constexpr int SCROLL_OF_TOWN_PORTAL = 4;

    // Weapons
    constexpr int IRON_SWORD = 10;
    constexpr int STEEL_BLADE = 11;
    constexpr int COMMANDER_BATON = 12;
    constexpr int ENGINEER_WRENCH = 13;
    constexpr int SCOUT_DAGGER = 14;

    // Armor
    constexpr int LEATHER_ARMOR = 20;
    constexpr int CHAIN_MAIL = 21;
    constexpr int PLATE_ARMOR = 22;
    constexpr int MAGE_ROBES = 23;

    // Accessories
    constexpr int RING_OF_STRENGTH = 30;
    constexpr int RING_OF_AGILITY = 31;
    constexpr int RING_OF_INTELLIGENCE = 32;
    constexpr int AMULET_OF_HEALTH = 33;
    constexpr int BOOTS_OF_SPEED = 34;
    constexpr int MERCHANT_COIN = 35;

    // Legendary
    constexpr int WARLORD_HELM = 100;
    constexpr int COMMANDER_BANNER = 101;
    constexpr int ENGINEER_GOGGLES = 102;
    constexpr int SCOUT_CLOAK = 103;
    constexpr int MERCHANT_LEDGER = 104;
}

} // namespace RTS
} // namespace Vehement
