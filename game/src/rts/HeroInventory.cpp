#include "HeroInventory.hpp"
#include "Hero.hpp"
#include "Ability.hpp"
#include <algorithm>

namespace Vehement {
namespace RTS {

// ============================================================================
// HeroInventory Implementation
// ============================================================================

HeroInventory::HeroInventory() {
    Clear();
}

int HeroInventory::AddItem(int itemId, int preferredSlot) {
    const ItemData* itemData = ItemDatabase::Instance().GetItem(itemId);
    if (!itemData) {
        return -1;
    }

    // Check requirements
    if (m_owner && !MeetsRequirements(*itemData)) {
        return -1;
    }

    // Try to stack if stackable and already have one
    if (itemData->stackable) {
        int existingSlot = FindItem(itemId);
        if (existingSlot >= 0) {
            ItemInstance& existing = m_items[existingSlot];
            if (existing.quantity < itemData->maxStack) {
                existing.quantity++;
                NotifyItemChange(existingSlot, itemId, itemId);
                RecalculateStats();
                return existingSlot;
            }
        }
    }

    // Try preferred slot
    if (preferredSlot >= 0 && preferredSlot < SLOT_COUNT) {
        if (m_items[preferredSlot].IsEmpty()) {
            m_items[preferredSlot].itemId = itemId;
            m_items[preferredSlot].quantity = 1;
            m_items[preferredSlot].chargesRemaining = itemData->activeAbility.maxCharges;
            NotifyItemChange(preferredSlot, -1, itemId);
            RecalculateStats();
            return preferredSlot;
        }
    }

    // Find any empty slot
    int slot = FindEmptySlot();
    if (slot >= 0) {
        m_items[slot].itemId = itemId;
        m_items[slot].quantity = 1;
        m_items[slot].chargesRemaining = itemData->activeAbility.maxCharges;
        NotifyItemChange(slot, -1, itemId);
        RecalculateStats();
        return slot;
    }

    return -1; // Inventory full
}

ItemInstance HeroInventory::RemoveItem(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) {
        return ItemInstance{};
    }

    ItemInstance removed = m_items[slot];
    int oldId = m_items[slot].itemId;
    m_items[slot].Clear();

    if (oldId >= 0) {
        NotifyItemChange(slot, oldId, -1);
        RecalculateStats();
    }

    return removed;
}

bool HeroInventory::SwapItems(int slotA, int slotB) {
    if (slotA < 0 || slotA >= SLOT_COUNT || slotB < 0 || slotB >= SLOT_COUNT) {
        return false;
    }

    if (slotA == slotB) {
        return true;
    }

    std::swap(m_items[slotA], m_items[slotB]);

    NotifyItemChange(slotA, m_items[slotB].itemId, m_items[slotA].itemId);
    NotifyItemChange(slotB, m_items[slotA].itemId, m_items[slotB].itemId);

    return true;
}

bool HeroInventory::MoveItem(int fromSlot, int toSlot) {
    if (fromSlot < 0 || fromSlot >= SLOT_COUNT || toSlot < 0 || toSlot >= SLOT_COUNT) {
        return false;
    }

    if (m_items[fromSlot].IsEmpty()) {
        return false;
    }

    if (m_items[toSlot].IsEmpty()) {
        // Simple move
        m_items[toSlot] = m_items[fromSlot];
        m_items[fromSlot].Clear();
        NotifyItemChange(fromSlot, m_items[toSlot].itemId, -1);
        NotifyItemChange(toSlot, -1, m_items[toSlot].itemId);
        return true;
    } else {
        // Swap
        return SwapItems(fromSlot, toSlot);
    }
}

bool HeroInventory::DropItem(int slot) {
    if (slot < 0 || slot >= SLOT_COUNT) {
        return false;
    }

    if (m_items[slot].IsEmpty()) {
        return false;
    }

    const ItemData* itemData = ItemDatabase::Instance().GetItem(m_items[slot].itemId);
    if (!itemData || !itemData->droppable) {
        return false;
    }

    RemoveItem(slot);
    return true;
}

bool HeroInventory::HasItem(int slot) const {
    return slot >= 0 && slot < SLOT_COUNT && !m_items[slot].IsEmpty();
}

bool HeroInventory::ContainsItem(int itemId) const {
    return FindItem(itemId) >= 0;
}

int HeroInventory::FindItem(int itemId) const {
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (m_items[i].itemId == itemId) {
            return i;
        }
    }
    return -1;
}

const ItemInstance& HeroInventory::GetItem(int slot) const {
    static ItemInstance empty;
    if (slot < 0 || slot >= SLOT_COUNT) {
        return empty;
    }
    return m_items[slot];
}

int HeroInventory::GetEmptySlotCount() const {
    int count = 0;
    for (const auto& item : m_items) {
        if (item.IsEmpty()) {
            count++;
        }
    }
    return count;
}

bool HeroInventory::UseItem(int slot) {
    if (!CanUseItem(slot)) {
        return false;
    }

    ItemInstance& item = m_items[slot];
    const ItemData* itemData = ItemDatabase::Instance().GetItem(item.itemId);
    if (!itemData) {
        return false;
    }

    // Execute item ability if it has one
    if (itemData->hasActive && itemData->activeAbility.abilityId >= 0) {
        AbilityBehavior* behavior = AbilityManager::Instance().GetBehavior(itemData->activeAbility.abilityId);
        if (behavior && m_owner) {
            AbilityCastContext context;
            context.caster = m_owner;
            context.abilityLevel = 1; // Items are level 1 abilities

            const AbilityData* abilityData = AbilityManager::Instance().GetAbility(itemData->activeAbility.abilityId);
            if (abilityData) {
                AbilityCastResult result = behavior->Execute(context, *abilityData);
                if (!result.success) {
                    return false;
                }
            }
        }
    }

    // Apply cooldown
    item.cooldownRemaining = itemData->activeAbility.cooldown;

    // Consume charges
    if (item.chargesRemaining > 0) {
        item.chargesRemaining--;
    }

    // Consume if single-use
    if (itemData->consumeOnUse) {
        if (item.quantity > 1) {
            item.quantity--;
        } else {
            RemoveItem(slot);
        }
    }

    if (m_onItemUse) {
        m_onItemUse(item.itemId, slot);
    }

    return true;
}

bool HeroInventory::UseItemOnTarget(int slot, Entity* target) {
    // For now, same as UseItem - could be extended for targeted items
    return UseItem(slot);
}

bool HeroInventory::UseItemAtPoint(int slot, const glm::vec3& point) {
    // For now, same as UseItem - could be extended for ground-targeted items
    return UseItem(slot);
}

bool HeroInventory::CanUseItem(int slot) const {
    if (slot < 0 || slot >= SLOT_COUNT) {
        return false;
    }

    const ItemInstance& item = m_items[slot];
    if (item.IsEmpty()) {
        return false;
    }

    // Check cooldown
    if (item.cooldownRemaining > 0.0f) {
        return false;
    }

    // Check charges
    const ItemData* itemData = ItemDatabase::Instance().GetItem(item.itemId);
    if (!itemData) {
        return false;
    }

    if (itemData->hasActive && item.chargesRemaining == 0) {
        return false;
    }

    // Check mana cost if applicable
    if (m_owner && itemData->hasActive) {
        const AbilityData* abilityData = AbilityManager::Instance().GetAbility(itemData->activeAbility.abilityId);
        if (abilityData) {
            float manaCost = abilityData->GetLevelData(1).manaCost;
            if (m_owner->GetMana() < manaCost) {
                return false;
            }
        }
    }

    return true;
}

float HeroInventory::GetItemCooldown(int slot) const {
    if (slot < 0 || slot >= SLOT_COUNT) {
        return 0.0f;
    }
    return m_items[slot].cooldownRemaining;
}

ItemStats HeroInventory::GetTotalStats() const {
    ItemStats total;

    for (const auto& item : m_items) {
        if (!item.IsEmpty()) {
            const ItemData* itemData = ItemDatabase::Instance().GetItem(item.itemId);
            if (itemData) {
                total = total + itemData->stats;
            }
        }
    }

    return total;
}

void HeroInventory::RecalculateStats() {
    m_cachedStats = GetTotalStats();
}

void HeroInventory::Update(float deltaTime) {
    for (auto& item : m_items) {
        if (!item.IsEmpty() && item.cooldownRemaining > 0.0f) {
            item.cooldownRemaining -= deltaTime;
            if (item.cooldownRemaining < 0.0f) {
                item.cooldownRemaining = 0.0f;
            }
        }
    }
}

void HeroInventory::Clear() {
    for (auto& item : m_items) {
        item.Clear();
    }
    m_cachedStats = ItemStats{};
}

bool HeroInventory::MeetsRequirements(const ItemData& item) const {
    if (!m_owner) {
        return true; // No owner to check against
    }

    if (m_owner->GetLevel() < item.requiredLevel) {
        return false;
    }

    const HeroStats& stats = m_owner->GetStats();
    if (stats.strength < item.requiredStrength) return false;
    if (stats.agility < item.requiredAgility) return false;
    if (stats.intelligence < item.requiredIntelligence) return false;

    return true;
}

int HeroInventory::FindEmptySlot() const {
    for (int i = 0; i < SLOT_COUNT; ++i) {
        if (m_items[i].IsEmpty()) {
            return i;
        }
    }
    return -1;
}

void HeroInventory::NotifyItemChange(int slot, int oldId, int newId) {
    if (m_onItemChange) {
        m_onItemChange(slot, oldId, newId);
    }
}

// ============================================================================
// ItemDatabase Implementation
// ============================================================================

void ItemDatabase::Initialize() {
    RegisterDefaultItems();
}

const ItemData* ItemDatabase::GetItem(int id) const {
    if (id >= 0 && id < static_cast<int>(m_items.size())) {
        return &m_items[id];
    }
    return nullptr;
}

std::vector<const ItemData*> ItemDatabase::GetItemsByType(ItemType type) const {
    std::vector<const ItemData*> result;
    for (const auto& item : m_items) {
        if (item.type == type) {
            result.push_back(&item);
        }
    }
    return result;
}

std::vector<const ItemData*> ItemDatabase::GetItemsByRarity(ItemRarity rarity) const {
    std::vector<const ItemData*> result;
    for (const auto& item : m_items) {
        if (item.rarity == rarity) {
            result.push_back(&item);
        }
    }
    return result;
}

std::vector<const ItemData*> ItemDatabase::GetShopItems(int shopLevel) const {
    std::vector<const ItemData*> result;
    for (const auto& item : m_items) {
        if (item.buyPrice > 0 && item.requiredLevel <= shopLevel) {
            result.push_back(&item);
        }
    }
    return result;
}

void ItemDatabase::RegisterDefaultItems() {
    m_items.clear();

    // We need to ensure items are at their correct indices
    m_items.resize(200); // Reserve space

    // =========================================================================
    // CONSUMABLES (0-9)
    // =========================================================================

    m_items[ItemId::HEALTH_POTION_SMALL] = {
        .id = ItemId::HEALTH_POTION_SMALL,
        .name = "Minor Health Potion",
        .description = "Restores 100 health instantly.",
        .iconPath = "rts/icons/items/health_potion_small.png",
        .type = ItemType::Consumable,
        .rarity = ItemRarity::Common,
        .stats = {},
        .activeAbility = {.abilityId = -1, .cooldown = 15.0f, .charges = -1, .maxCharges = -1},
        .hasActive = true,
        .stackable = true,
        .maxStack = 10,
        .buyPrice = 50,
        .sellPrice = 25,
        .requiredLevel = 1,
        .consumeOnUse = true
    };

    m_items[ItemId::HEALTH_POTION_LARGE] = {
        .id = ItemId::HEALTH_POTION_LARGE,
        .name = "Major Health Potion",
        .description = "Restores 250 health instantly.",
        .iconPath = "rts/icons/items/health_potion_large.png",
        .type = ItemType::Consumable,
        .rarity = ItemRarity::Uncommon,
        .stats = {},
        .activeAbility = {.abilityId = -1, .cooldown = 20.0f, .charges = -1, .maxCharges = -1},
        .hasActive = true,
        .stackable = true,
        .maxStack = 10,
        .buyPrice = 125,
        .sellPrice = 60,
        .requiredLevel = 5,
        .consumeOnUse = true
    };

    m_items[ItemId::MANA_POTION_SMALL] = {
        .id = ItemId::MANA_POTION_SMALL,
        .name = "Minor Mana Potion",
        .description = "Restores 75 mana instantly.",
        .iconPath = "rts/icons/items/mana_potion_small.png",
        .type = ItemType::Consumable,
        .rarity = ItemRarity::Common,
        .stats = {},
        .activeAbility = {.abilityId = -1, .cooldown = 20.0f, .charges = -1, .maxCharges = -1},
        .hasActive = true,
        .stackable = true,
        .maxStack = 10,
        .buyPrice = 60,
        .sellPrice = 30,
        .requiredLevel = 1,
        .consumeOnUse = true
    };

    m_items[ItemId::MANA_POTION_LARGE] = {
        .id = ItemId::MANA_POTION_LARGE,
        .name = "Major Mana Potion",
        .description = "Restores 200 mana instantly.",
        .iconPath = "rts/icons/items/mana_potion_large.png",
        .type = ItemType::Consumable,
        .rarity = ItemRarity::Uncommon,
        .stats = {},
        .activeAbility = {.abilityId = -1, .cooldown = 25.0f, .charges = -1, .maxCharges = -1},
        .hasActive = true,
        .stackable = true,
        .maxStack = 10,
        .buyPrice = 150,
        .sellPrice = 75,
        .requiredLevel = 5,
        .consumeOnUse = true
    };

    m_items[ItemId::SCROLL_OF_TOWN_PORTAL] = {
        .id = ItemId::SCROLL_OF_TOWN_PORTAL,
        .name = "Scroll of Town Portal",
        .description = "Teleport to your base after a short channel.",
        .iconPath = "rts/icons/items/scroll_tp.png",
        .type = ItemType::Consumable,
        .rarity = ItemRarity::Common,
        .stats = {},
        .activeAbility = {.abilityId = -1, .cooldown = 60.0f, .charges = -1, .maxCharges = -1},
        .hasActive = true,
        .stackable = true,
        .maxStack = 5,
        .buyPrice = 75,
        .sellPrice = 35,
        .requiredLevel = 1,
        .consumeOnUse = true
    };

    // =========================================================================
    // WEAPONS (10-19)
    // =========================================================================

    m_items[ItemId::IRON_SWORD] = {
        .id = ItemId::IRON_SWORD,
        .name = "Iron Sword",
        .description = "A basic but reliable sword.",
        .iconPath = "rts/icons/items/iron_sword.png",
        .type = ItemType::Weapon,
        .rarity = ItemRarity::Common,
        .stats = {.damage = 10.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 200,
        .sellPrice = 100,
        .requiredLevel = 1
    };

    m_items[ItemId::STEEL_BLADE] = {
        .id = ItemId::STEEL_BLADE,
        .name = "Steel Blade",
        .description = "A finely crafted steel blade with increased damage.",
        .iconPath = "rts/icons/items/steel_blade.png",
        .type = ItemType::Weapon,
        .rarity = ItemRarity::Uncommon,
        .stats = {.strength = 5.0f, .damage = 20.0f, .attackSpeed = 0.1f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 500,
        .sellPrice = 250,
        .requiredLevel = 5,
        .requiredStrength = 15
    };

    m_items[ItemId::COMMANDER_BATON] = {
        .id = ItemId::COMMANDER_BATON,
        .name = "Commander's Baton",
        .description = "Increases command radius and ally buff strength.",
        .iconPath = "rts/icons/items/commander_baton.png",
        .type = ItemType::Weapon,
        .rarity = ItemRarity::Rare,
        .stats = {.intelligence = 10.0f, .mana = 50.0f, .commandRadius = 3.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 800,
        .sellPrice = 400,
        .requiredLevel = 8,
        .requiredIntelligence = 20
    };

    m_items[ItemId::ENGINEER_WRENCH] = {
        .id = ItemId::ENGINEER_WRENCH,
        .name = "Engineer's Wrench",
        .description = "Increases building construction and repair speed.",
        .lore = "A well-used tool that has built many a fortress.",
        .iconPath = "rts/icons/items/engineer_wrench.png",
        .type = ItemType::Weapon,
        .rarity = ItemRarity::Rare,
        .stats = {.intelligence = 8.0f, .armor = 2.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 700,
        .sellPrice = 350,
        .requiredLevel = 6
    };

    m_items[ItemId::SCOUT_DAGGER] = {
        .id = ItemId::SCOUT_DAGGER,
        .name = "Scout's Dagger",
        .description = "A light blade perfect for quick strikes.",
        .iconPath = "rts/icons/items/scout_dagger.png",
        .type = ItemType::Weapon,
        .rarity = ItemRarity::Uncommon,
        .stats = {.agility = 8.0f, .damage = 12.0f, .attackSpeed = 0.25f, .moveSpeed = 0.05f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 450,
        .sellPrice = 225,
        .requiredLevel = 4,
        .requiredAgility = 15
    };

    // =========================================================================
    // ARMOR (20-29)
    // =========================================================================

    m_items[ItemId::LEATHER_ARMOR] = {
        .id = ItemId::LEATHER_ARMOR,
        .name = "Leather Armor",
        .description = "Light armor providing basic protection.",
        .iconPath = "rts/icons/items/leather_armor.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Common,
        .stats = {.armor = 3.0f, .health = 25.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 175,
        .sellPrice = 85,
        .requiredLevel = 1
    };

    m_items[ItemId::CHAIN_MAIL] = {
        .id = ItemId::CHAIN_MAIL,
        .name = "Chain Mail",
        .description = "Interlocking metal rings provide solid defense.",
        .iconPath = "rts/icons/items/chain_mail.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Uncommon,
        .stats = {.armor = 6.0f, .health = 50.0f, .healthRegen = 1.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 400,
        .sellPrice = 200,
        .requiredLevel = 4,
        .requiredStrength = 12
    };

    m_items[ItemId::PLATE_ARMOR] = {
        .id = ItemId::PLATE_ARMOR,
        .name = "Plate Armor",
        .description = "Heavy armor granting maximum protection.",
        .iconPath = "rts/icons/items/plate_armor.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Rare,
        .stats = {.strength = 5.0f, .armor = 10.0f, .health = 100.0f, .healthRegen = 2.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 900,
        .sellPrice = 450,
        .requiredLevel = 10,
        .requiredStrength = 20
    };

    m_items[ItemId::MAGE_ROBES] = {
        .id = ItemId::MAGE_ROBES,
        .name = "Mage Robes",
        .description = "Enchanted robes that enhance magical abilities.",
        .iconPath = "rts/icons/items/mage_robes.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Rare,
        .stats = {.intelligence = 12.0f, .armor = 2.0f, .mana = 75.0f, .manaRegen = 3.0f, .cooldownReduction = 0.10f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 850,
        .sellPrice = 425,
        .requiredLevel = 8,
        .requiredIntelligence = 18
    };

    // =========================================================================
    // ACCESSORIES (30-39)
    // =========================================================================

    m_items[ItemId::RING_OF_STRENGTH] = {
        .id = ItemId::RING_OF_STRENGTH,
        .name = "Ring of Strength",
        .description = "A simple ring that enhances physical power.",
        .iconPath = "rts/icons/items/ring_strength.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Common,
        .stats = {.strength = 5.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 150,
        .sellPrice = 75,
        .requiredLevel = 1
    };

    m_items[ItemId::RING_OF_AGILITY] = {
        .id = ItemId::RING_OF_AGILITY,
        .name = "Ring of Agility",
        .description = "A simple ring that enhances reflexes.",
        .iconPath = "rts/icons/items/ring_agility.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Common,
        .stats = {.agility = 5.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 150,
        .sellPrice = 75,
        .requiredLevel = 1
    };

    m_items[ItemId::RING_OF_INTELLIGENCE] = {
        .id = ItemId::RING_OF_INTELLIGENCE,
        .name = "Ring of Intelligence",
        .description = "A simple ring that enhances mental acuity.",
        .iconPath = "rts/icons/items/ring_intelligence.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Common,
        .stats = {.intelligence = 5.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 150,
        .sellPrice = 75,
        .requiredLevel = 1
    };

    m_items[ItemId::AMULET_OF_HEALTH] = {
        .id = ItemId::AMULET_OF_HEALTH,
        .name = "Amulet of Health",
        .description = "Increases maximum health and regeneration.",
        .iconPath = "rts/icons/items/amulet_health.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Uncommon,
        .stats = {.health = 75.0f, .healthRegen = 2.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 350,
        .sellPrice = 175,
        .requiredLevel = 3
    };

    m_items[ItemId::BOOTS_OF_SPEED] = {
        .id = ItemId::BOOTS_OF_SPEED,
        .name = "Boots of Speed",
        .description = "Increases movement speed significantly.",
        .iconPath = "rts/icons/items/boots_speed.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Uncommon,
        .stats = {.agility = 3.0f, .moveSpeed = 0.15f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 300,
        .sellPrice = 150,
        .requiredLevel = 2
    };

    m_items[ItemId::MERCHANT_COIN] = {
        .id = ItemId::MERCHANT_COIN,
        .name = "Merchant's Lucky Coin",
        .description = "Increases gold gained from all sources.",
        .iconPath = "rts/icons/items/merchant_coin.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Rare,
        .stats = {.goldBonus = 0.10f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 500,
        .sellPrice = 250,
        .requiredLevel = 5
    };

    // =========================================================================
    // LEGENDARY ITEMS (100+)
    // =========================================================================

    m_items[ItemId::WARLORD_HELM] = {
        .id = ItemId::WARLORD_HELM,
        .name = "Warlord's Helm",
        .description = "A legendary helm worn by the greatest warriors.",
        .lore = "They say this helm was forged in the heat of a thousand battles.",
        .iconPath = "rts/icons/items/warlord_helm.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Legendary,
        .stats = {.strength = 20.0f, .armor = 8.0f, .health = 150.0f, .damage = 15.0f, .healthRegen = 5.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 3000,
        .sellPrice = 1500,
        .requiredLevel = 15,
        .requiredStrength = 30
    };

    m_items[ItemId::COMMANDER_BANNER] = {
        .id = ItemId::COMMANDER_BANNER,
        .name = "Commander's War Banner",
        .description = "A legendary banner that inspires all nearby allies.",
        .lore = "Under this banner, armies have turned the tide of war.",
        .iconPath = "rts/icons/items/commander_banner.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Legendary,
        .stats = {.intelligence = 25.0f, .mana = 100.0f, .commandRadius = 8.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 3500,
        .sellPrice = 1750,
        .requiredLevel = 15,
        .requiredIntelligence = 30
    };

    m_items[ItemId::ENGINEER_GOGGLES] = {
        .id = ItemId::ENGINEER_GOGGLES,
        .name = "Master Engineer's Goggles",
        .description = "Legendary goggles that reveal structural weaknesses.",
        .lore = "Built by an engineer who could see the flaws in any design.",
        .iconPath = "rts/icons/items/engineer_goggles.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Legendary,
        .stats = {.intelligence = 20.0f, .visionRange = 5.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 2800,
        .sellPrice = 1400,
        .requiredLevel = 14
    };

    m_items[ItemId::SCOUT_CLOAK] = {
        .id = ItemId::SCOUT_CLOAK,
        .name = "Shadowstalker's Cloak",
        .description = "A legendary cloak that bends light around the wearer.",
        .lore = "Those who wear this cloak become one with the shadows.",
        .iconPath = "rts/icons/items/scout_cloak.png",
        .type = ItemType::Armor,
        .rarity = ItemRarity::Legendary,
        .stats = {.agility = 25.0f, .moveSpeed = 0.25f, .visionRange = 3.0f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 3200,
        .sellPrice = 1600,
        .requiredLevel = 15,
        .requiredAgility = 30
    };

    m_items[ItemId::MERCHANT_LEDGER] = {
        .id = ItemId::MERCHANT_LEDGER,
        .name = "Golden Ledger of Fortune",
        .description = "A legendary ledger that brings prosperity to its owner.",
        .lore = "Every transaction recorded here seems to turn to gold.",
        .iconPath = "rts/icons/items/merchant_ledger.png",
        .type = ItemType::Accessory,
        .rarity = ItemRarity::Legendary,
        .stats = {.intelligence = 15.0f, .goldBonus = 0.30f, .experienceBonus = 0.15f},
        .activeAbility = {},
        .hasActive = false,
        .stackable = false,
        .maxStack = 1,
        .buyPrice = 4000,
        .sellPrice = 2000,
        .requiredLevel = 15
    };
}

} // namespace RTS
} // namespace Vehement
