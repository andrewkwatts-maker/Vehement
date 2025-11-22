#include "Shop.hpp"
#include <algorithm>

namespace Vehement {

// ============================================================================
// Shop Implementation
// ============================================================================

Shop::Shop() {
}

void Shop::Initialize() {
    m_items.clear();
    m_nextItemId = 1;

    AddWeaponItems();
    AddAmmoItems();
    AddGrenadeItems();
}

ShopItem& Shop::AddItem(
    const std::string& name,
    const std::string& desc,
    ShopCategory category,
    int price
) {
    ShopItem item;
    item.itemId = GenerateItemId();
    item.name = name;
    item.description = desc;
    item.category = category;
    item.price = price;
    item.available = true;

    m_items.push_back(item);
    return m_items.back();
}

void Shop::AddWeaponItems() {
    using namespace WeaponTextures;

    // Glock (starting weapon - already owned)
    {
        auto& item = AddItem("Glock 17", "Standard sidearm. Reliable and accurate.",
                            ShopCategory::Weapons, 0);
        item.weaponType = WeaponType::Glock;
        item.iconPath = kGlockSide;
        item.owned = true;
        item.unlockWave = 0;
    }

    // AK-47
    {
        auto& item = AddItem("AK-47", "Fully automatic assault rifle. High damage, moderate recoil.",
                            ShopCategory::Weapons, 500);
        item.weaponType = WeaponType::AK47;
        item.iconPath = kAK47Side;
        item.unlockWave = 2;
    }

    // Sniper
    {
        auto& item = AddItem("AWP Sniper", "High-powered sniper rifle. Penetrates multiple targets.",
                            ShopCategory::Weapons, 1500);
        item.weaponType = WeaponType::Sniper;
        item.iconPath = kSniperSide;
        item.unlockWave = 5;
    }
}

void Shop::AddAmmoItems() {
    // Glock ammo
    {
        auto& item = AddItem("9mm Ammo", "Magazine of 12 rounds for Glock.",
                            ShopCategory::Ammo, 15);
        item.ammoForWeapon = WeaponType::Glock;
        item.ammoAmount = 12;
        item.iconPath = "Vehement2/images/UI/ammo_9mm.png";
        item.unlockWave = 0;
    }

    // AK-47 ammo
    {
        auto& item = AddItem("7.62mm Ammo", "Magazine of 30 rounds for AK-47.",
                            ShopCategory::Ammo, 50);
        item.ammoForWeapon = WeaponType::AK47;
        item.ammoAmount = 30;
        item.iconPath = "Vehement2/images/UI/ammo_762.png";
        item.unlockWave = 2;
    }

    // Sniper ammo
    {
        auto& item = AddItem(".308 Ammo", "Magazine of 5 rounds for AWP.",
                            ShopCategory::Ammo, 75);
        item.ammoForWeapon = WeaponType::Sniper;
        item.ammoAmount = 5;
        item.iconPath = "Vehement2/images/UI/ammo_308.png";
        item.unlockWave = 5;
    }
}

void Shop::AddGrenadeItems() {
    using namespace WeaponTextures;

    // Frag Grenade
    {
        auto& item = AddItem("Frag Grenade", "High explosive. Deals heavy damage in radius.",
                            ShopCategory::Grenades, 100);
        item.grenadeVariant = GrenadeVariant::Green;
        item.iconPath = kGrenadeGreen;
        item.unlockWave = 1;
    }

    // Flashbang
    {
        auto& item = AddItem("Flashbang", "Blinds zombies for 5 seconds.",
                            ShopCategory::Grenades, 75);
        item.grenadeVariant = GrenadeVariant::Flash;
        item.iconPath = kFlashNade;
        item.unlockWave = 2;
    }

    // Stun Grenade
    {
        auto& item = AddItem("Stun Grenade", "Slows zombies by 50% for 4 seconds.",
                            ShopCategory::Grenades, 75);
        item.grenadeVariant = GrenadeVariant::Stun;
        item.iconPath = kStunNade;
        item.unlockWave = 2;
    }

    // Smoke Grenade (Grey)
    {
        auto& item = AddItem("Smoke Grenade", "Creates smoke cover. Zombies lose sight.",
                            ShopCategory::Grenades, 50);
        item.grenadeVariant = GrenadeVariant::Grey;
        item.iconPath = kGrenadeGrey;
        item.unlockWave = 3;
    }

    // Incendiary (Red)
    {
        auto& item = AddItem("Incendiary Grenade", "Creates fire zone. Deals damage over time.",
                            ShopCategory::Grenades, 125);
        item.grenadeVariant = GrenadeVariant::Red;
        item.iconPath = kGrenadeRed;
        item.unlockWave = 4;
    }

    // Claymore
    {
        auto& item = AddItem("Claymore Mine", "Proximity mine. Triggers when zombie approaches.",
                            ShopCategory::Grenades, 200);
        item.grenadeVariant = GrenadeVariant::Claymore;
        item.iconPath = kClaymore;
        item.unlockWave = 3;
    }
}

void Shop::UpdateAvailability(int currentWave) {
    for (auto& item : m_items) {
        // Item is available if wave requirement is met
        item.available = (currentWave >= item.unlockWave);
    }
}

std::vector<const ShopItem*> Shop::GetItemsByCategory(ShopCategory category) const {
    std::vector<const ShopItem*> result;

    for (const auto& item : m_items) {
        if (item.category == category) {
            result.push_back(&item);
        }
    }

    return result;
}

const ShopItem* Shop::GetItem(int itemId) const {
    for (const auto& item : m_items) {
        if (item.itemId == itemId) {
            return &item;
        }
    }
    return nullptr;
}

const ShopItem* Shop::GetWeaponItem(WeaponType type) const {
    for (const auto& item : m_items) {
        if (item.category == ShopCategory::Weapons && item.weaponType == type) {
            return &item;
        }
    }
    return nullptr;
}

const ShopItem* Shop::GetAmmoItem(WeaponType type) const {
    for (const auto& item : m_items) {
        if (item.category == ShopCategory::Ammo && item.ammoForWeapon == type) {
            return &item;
        }
    }
    return nullptr;
}

const ShopItem* Shop::GetGrenadeItem(GrenadeVariant variant) const {
    for (const auto& item : m_items) {
        if (item.category == ShopCategory::Grenades && item.grenadeVariant == variant) {
            return &item;
        }
    }
    return nullptr;
}

// -------------------------------------------------------------------------
// Transactions
// -------------------------------------------------------------------------

TransactionResult Shop::BuyItem(int itemId, Wallet& wallet, WeaponInventory& inventory) {
    const ShopItem* item = GetItem(itemId);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    TransactionResult result;

    switch (item->category) {
        case ShopCategory::Weapons:
            result = BuyWeapon(item->weaponType, wallet, inventory);
            break;

        case ShopCategory::Ammo:
            result = BuyAmmo(item->ammoForWeapon, wallet, inventory);
            break;

        case ShopCategory::Grenades:
            result = BuyGrenade(item->grenadeVariant, wallet, inventory);
            break;

        default:
            result = TransactionResult::InvalidItem;
            break;
    }

    // Fire callback
    if (m_onPurchase) {
        m_onPurchase(*item, result);
    }

    return result;
}

TransactionResult Shop::BuyWeapon(WeaponType type, Wallet& wallet, WeaponInventory& inventory) {
    const ShopItem* item = GetWeaponItem(type);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    if (!item->available) {
        return TransactionResult::ItemNotAvailable;
    }

    // Check if already owned
    for (int i = 0; i < WeaponInventory::kMaxWeapons; ++i) {
        const Weapon* weapon = inventory.GetWeaponAt(i);
        if (weapon && weapon->GetType() == type) {
            // Already have weapon, buy ammo instead
            return BuyAmmo(type, wallet, inventory);
        }
    }

    // Check price
    int price = GetDiscountedPrice(item->price);
    if (!wallet.CanAfford(price)) {
        return TransactionResult::InsufficientFunds;
    }

    // Check inventory space
    if (inventory.GetWeaponCount() >= WeaponInventory::kMaxWeapons) {
        return TransactionResult::InventoryFull;
    }

    // Make purchase
    wallet.SpendCoins(price);
    inventory.AddWeapon(type);

    return TransactionResult::Success;
}

TransactionResult Shop::BuyAmmo(WeaponType type, Wallet& wallet, WeaponInventory& inventory) {
    const ShopItem* item = GetAmmoItem(type);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    if (!item->available) {
        return TransactionResult::ItemNotAvailable;
    }

    // Find the weapon in inventory
    Weapon* weapon = nullptr;
    for (int i = 0; i < WeaponInventory::kMaxWeapons; ++i) {
        Weapon* w = inventory.GetWeaponAt(i);
        if (w && w->GetType() == type) {
            weapon = w;
            break;
        }
    }

    if (!weapon) {
        // Don't have the weapon yet
        return TransactionResult::ItemNotAvailable;
    }

    // Check price
    int price = GetDiscountedPrice(item->price);
    if (!wallet.CanAfford(price)) {
        return TransactionResult::InsufficientFunds;
    }

    // Make purchase
    wallet.SpendCoins(price);
    weapon->AddAmmo(1);  // Add one magazine

    return TransactionResult::Success;
}

TransactionResult Shop::BuyGrenade(GrenadeVariant variant, Wallet& wallet, WeaponInventory& inventory) {
    const ShopItem* item = GetGrenadeItem(variant);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    if (!item->available) {
        return TransactionResult::ItemNotAvailable;
    }

    // Check if at max grenades
    if (variant == GrenadeVariant::Claymore) {
        if (inventory.GetClaymoreCount() >= WeaponInventory::kMaxClaymores) {
            return TransactionResult::InventoryFull;
        }
    } else {
        if (inventory.GetGrenadeCount(variant) >= WeaponInventory::kMaxGrenades) {
            return TransactionResult::InventoryFull;
        }
    }

    // Check price
    int price = GetDiscountedPrice(item->price);
    if (!wallet.CanAfford(price)) {
        return TransactionResult::InsufficientFunds;
    }

    // Make purchase
    wallet.SpendCoins(price);

    if (variant == GrenadeVariant::Claymore) {
        inventory.AddClaymores(1);
    } else {
        inventory.AddGrenade(variant, 1);
    }

    return TransactionResult::Success;
}

TransactionResult Shop::SellWeapon(WeaponType type, Wallet& wallet, WeaponInventory& inventory) {
    // Can't sell starting weapon
    if (type == WeaponType::Glock) {
        return TransactionResult::ItemNotAvailable;
    }

    // Find weapon in inventory
    bool found = false;
    for (int i = 0; i < WeaponInventory::kMaxWeapons; ++i) {
        const Weapon* weapon = inventory.GetWeaponAt(i);
        if (weapon && weapon->GetType() == type) {
            found = true;

            // Switch if selling current weapon
            if (i == inventory.GetCurrentSlot()) {
                inventory.NextWeapon();
            }

            break;
        }
    }

    if (!found) {
        return TransactionResult::InvalidItem;
    }

    // Calculate sell price
    int sellPrice = GetSellPrice(type);

    // Give coins
    wallet.AddCoins(sellPrice);

    // Remove weapon
    // Note: This would need implementation in WeaponInventory
    // For now we just remove from current slot if it matches

    // Fire callback
    if (m_onSell) {
        m_onSell(type, sellPrice);
    }

    return TransactionResult::Success;
}

int Shop::GetSellPrice(WeaponType type) const {
    const ShopItem* item = GetWeaponItem(type);
    if (!item) {
        return 0;
    }

    return static_cast<int>(item->price * kSellPriceMultiplier);
}

} // namespace Vehement
