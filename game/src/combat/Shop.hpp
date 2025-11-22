#pragma once

#include <vector>
#include <string>
#include <functional>
#include <optional>
#include "Weapon.hpp"

namespace Vehement {

// ============================================================================
// Shop Item Types
// ============================================================================

/**
 * @brief Category of shop item
 */
enum class ShopCategory {
    Weapons,
    Ammo,
    Grenades,
    Equipment,
    Upgrades
};

// ============================================================================
// Shop Item
// ============================================================================

/**
 * @brief Represents an item available in the shop
 */
struct ShopItem {
    std::string name;
    std::string description;
    ShopCategory category = ShopCategory::Weapons;
    int price = 100;
    bool available = true;              // Can be purchased
    bool owned = false;                 // Already owned (for weapons)

    // Type-specific data
    WeaponType weaponType = WeaponType::Glock;
    GrenadeVariant grenadeVariant = GrenadeVariant::Green;

    // For ammo
    int ammoAmount = 0;                 // Bullets per purchase
    WeaponType ammoForWeapon = WeaponType::Glock;

    // Texture path for UI
    std::string iconPath;

    // Unique identifier
    int itemId = 0;

    // Requirements (wave number to unlock)
    int unlockWave = 0;

    /**
     * @brief Get formatted price string
     */
    [[nodiscard]] std::string GetPriceString() const {
        return std::to_string(price) + " coins";
    }
};

// ============================================================================
// Transaction Result
// ============================================================================

/**
 * @brief Result of a shop transaction
 */
enum class TransactionResult {
    Success,
    InsufficientFunds,
    ItemNotAvailable,
    InventoryFull,
    AlreadyOwned,
    InvalidItem
};

/**
 * @brief Get human-readable message for transaction result
 */
inline const char* GetTransactionMessage(TransactionResult result) {
    switch (result) {
        case TransactionResult::Success:            return "Purchase successful!";
        case TransactionResult::InsufficientFunds:  return "Not enough coins!";
        case TransactionResult::ItemNotAvailable:   return "Item not available!";
        case TransactionResult::InventoryFull:      return "Inventory full!";
        case TransactionResult::AlreadyOwned:       return "Already owned!";
        case TransactionResult::InvalidItem:        return "Invalid item!";
        default:                                    return "Unknown error";
    }
}

// ============================================================================
// Player Wallet
// ============================================================================

/**
 * @brief Manages player's currency
 */
class Wallet {
public:
    Wallet() = default;
    explicit Wallet(int startingCoins) : m_coins(startingCoins) {}

    /**
     * @brief Get current coin count
     */
    [[nodiscard]] int GetCoins() const { return m_coins; }

    /**
     * @brief Add coins
     */
    void AddCoins(int amount) {
        m_coins += amount;
        m_totalEarned += amount;
    }

    /**
     * @brief Spend coins
     * @return true if successful (had enough)
     */
    bool SpendCoins(int amount) {
        if (m_coins >= amount) {
            m_coins -= amount;
            m_totalSpent += amount;
            return true;
        }
        return false;
    }

    /**
     * @brief Check if can afford amount
     */
    [[nodiscard]] bool CanAfford(int amount) const {
        return m_coins >= amount;
    }

    /**
     * @brief Get total coins earned
     */
    [[nodiscard]] int GetTotalEarned() const { return m_totalEarned; }

    /**
     * @brief Get total coins spent
     */
    [[nodiscard]] int GetTotalSpent() const { return m_totalSpent; }

    /**
     * @brief Reset wallet
     */
    void Reset(int startingCoins = 0) {
        m_coins = startingCoins;
        m_totalEarned = startingCoins;
        m_totalSpent = 0;
    }

private:
    int m_coins = 0;
    int m_totalEarned = 0;
    int m_totalSpent = 0;
};

// ============================================================================
// Shop System
// ============================================================================

/**
 * @brief Weapon and item shop system
 */
class Shop {
public:
    Shop();
    ~Shop() = default;

    /**
     * @brief Initialize shop with default items
     */
    void Initialize();

    /**
     * @brief Update item availability based on current wave
     */
    void UpdateAvailability(int currentWave);

    // -------------------------------------------------------------------------
    // Item Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Get all shop items
     */
    [[nodiscard]] const std::vector<ShopItem>& GetAllItems() const { return m_items; }

    /**
     * @brief Get items by category
     */
    [[nodiscard]] std::vector<const ShopItem*> GetItemsByCategory(ShopCategory category) const;

    /**
     * @brief Get item by ID
     */
    [[nodiscard]] const ShopItem* GetItem(int itemId) const;

    /**
     * @brief Get weapon item
     */
    [[nodiscard]] const ShopItem* GetWeaponItem(WeaponType type) const;

    /**
     * @brief Get ammo item for weapon
     */
    [[nodiscard]] const ShopItem* GetAmmoItem(WeaponType type) const;

    /**
     * @brief Get grenade item
     */
    [[nodiscard]] const ShopItem* GetGrenadeItem(GrenadeVariant variant) const;

    // -------------------------------------------------------------------------
    // Transactions
    // -------------------------------------------------------------------------

    /**
     * @brief Attempt to buy an item
     * @param itemId ID of item to buy
     * @param wallet Player's wallet
     * @param inventory Player's inventory (for weapons)
     * @return Transaction result
     */
    TransactionResult BuyItem(int itemId, Wallet& wallet, WeaponInventory& inventory);

    /**
     * @brief Buy a weapon
     */
    TransactionResult BuyWeapon(WeaponType type, Wallet& wallet, WeaponInventory& inventory);

    /**
     * @brief Buy ammo for a weapon
     */
    TransactionResult BuyAmmo(WeaponType type, Wallet& wallet, WeaponInventory& inventory);

    /**
     * @brief Buy grenades
     */
    TransactionResult BuyGrenade(GrenadeVariant variant, Wallet& wallet, WeaponInventory& inventory);

    /**
     * @brief Sell a weapon (returns 50% value)
     */
    TransactionResult SellWeapon(WeaponType type, Wallet& wallet, WeaponInventory& inventory);

    /**
     * @brief Get sell price for weapon (50% of buy price)
     */
    [[nodiscard]] int GetSellPrice(WeaponType type) const;

    // -------------------------------------------------------------------------
    // Shop State
    // -------------------------------------------------------------------------

    /**
     * @brief Check if shop is open
     */
    [[nodiscard]] bool IsOpen() const { return m_isOpen; }

    /**
     * @brief Open the shop
     */
    void Open() { m_isOpen = true; }

    /**
     * @brief Close the shop
     */
    void Close() { m_isOpen = false; }

    /**
     * @brief Toggle shop open/closed
     */
    void Toggle() { m_isOpen = !m_isOpen; }

    /**
     * @brief Set discount percentage (0-100)
     */
    void SetDiscount(int percent) { m_discountPercent = std::clamp(percent, 0, 100); }

    /**
     * @brief Get current discount
     */
    [[nodiscard]] int GetDiscount() const { return m_discountPercent; }

    /**
     * @brief Get discounted price
     */
    [[nodiscard]] int GetDiscountedPrice(int basePrice) const {
        return basePrice - (basePrice * m_discountPercent / 100);
    }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using PurchaseCallback = std::function<void(const ShopItem&, TransactionResult)>;
    void SetOnPurchase(PurchaseCallback callback) { m_onPurchase = std::move(callback); }

    using SellCallback = std::function<void(WeaponType, int coinsReceived)>;
    void SetOnSell(SellCallback callback) { m_onSell = std::move(callback); }

private:
    void AddWeaponItems();
    void AddAmmoItems();
    void AddGrenadeItems();

    ShopItem& AddItem(const std::string& name, const std::string& desc,
                      ShopCategory category, int price);

    int GenerateItemId() { return m_nextItemId++; }

    std::vector<ShopItem> m_items;
    int m_nextItemId = 1;

    bool m_isOpen = false;
    int m_discountPercent = 0;

    PurchaseCallback m_onPurchase;
    SellCallback m_onSell;

    static constexpr float kSellPriceMultiplier = 0.5f;
};

// ============================================================================
// Shop UI Helper
// ============================================================================

/**
 * @brief Helper for rendering shop UI
 */
struct ShopUIState {
    ShopCategory selectedCategory = ShopCategory::Weapons;
    int selectedItemIndex = 0;
    int hoveredItemIndex = -1;
    std::string statusMessage;
    float statusMessageTimer = 0.0f;

    void ShowMessage(const std::string& msg, float duration = 2.0f) {
        statusMessage = msg;
        statusMessageTimer = duration;
    }

    void Update(float deltaTime) {
        if (statusMessageTimer > 0.0f) {
            statusMessageTimer -= deltaTime;
            if (statusMessageTimer <= 0.0f) {
                statusMessage.clear();
            }
        }
    }
};

/**
 * @brief Get category name for UI
 */
inline const char* GetCategoryName(ShopCategory category) {
    switch (category) {
        case ShopCategory::Weapons:   return "Weapons";
        case ShopCategory::Ammo:      return "Ammunition";
        case ShopCategory::Grenades:  return "Grenades";
        case ShopCategory::Equipment: return "Equipment";
        case ShopCategory::Upgrades:  return "Upgrades";
        default:                      return "Unknown";
    }
}

/**
 * @brief Get icon path for category
 */
inline const char* GetCategoryIcon(ShopCategory category) {
    switch (category) {
        case ShopCategory::Weapons:   return "Vehement2/images/Weapons/AK47Side.png";
        case ShopCategory::Ammo:      return "Vehement2/images/UI/ammo_icon.png";
        case ShopCategory::Grenades:  return "Vehement2/images/Weapons/GrenadeGreen.png";
        case ShopCategory::Equipment: return "Vehement2/images/UI/equipment_icon.png";
        case ShopCategory::Upgrades:  return "Vehement2/images/UI/upgrade_icon.png";
        default:                      return "";
    }
}

} // namespace Vehement
