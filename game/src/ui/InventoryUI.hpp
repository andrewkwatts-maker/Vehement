#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; class UIDataBinding; } }

namespace Game {
namespace UI {

/**
 * @brief Item data structure
 */
struct ItemData {
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;
    std::string rarity; // "common", "uncommon", "rare", "epic", "legendary"
    std::string type;   // "weapon", "armor", "consumable", "material", "quest"
    int stackCount = 1;
    int maxStack = 1;
    int slotIndex = -1;
    bool isEquipped = false;
    std::unordered_map<std::string, std::string> stats;
    std::unordered_map<std::string, std::string> attributes;
};

/**
 * @brief Equipment slot data
 */
struct EquipmentSlot {
    std::string slotType; // "head", "chest", "legs", "feet", "weapon", "offhand", "accessory1", etc.
    std::string itemId;   // Empty if no item equipped
    std::string iconPath;
    bool isLocked = false;
};

/**
 * @brief Inventory slot
 */
struct InventorySlot {
    int index;
    std::string itemId;
    int stackCount = 0;
    bool isLocked = false;
    bool isHighlighted = false;
};

/**
 * @brief Drag operation data
 */
struct DragData {
    std::string sourceType; // "inventory", "equipment", "external"
    int sourceSlot = -1;
    std::string itemId;
    bool isActive = false;
};

/**
 * @brief Inventory UI System
 *
 * Grid-based inventory with drag-drop items, tooltips,
 * equipment slots, and stack splitting.
 */
class InventoryUI {
public:
    InventoryUI();
    ~InventoryUI();

    /**
     * @brief Initialize the inventory UI
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown the inventory UI
     */
    void Shutdown();

    /**
     * @brief Update the UI
     */
    void Update(float deltaTime);

    /**
     * @brief Show the inventory window
     */
    void Show();

    /**
     * @brief Hide the inventory window
     */
    void Hide();

    /**
     * @brief Toggle inventory visibility
     */
    void Toggle();

    /**
     * @brief Check if visible
     */
    bool IsVisible() const { return m_visible; }

    // Inventory Management

    /**
     * @brief Set inventory size
     * @param rows Number of rows
     * @param cols Number of columns
     */
    void SetInventorySize(int rows, int cols);

    /**
     * @brief Set item in slot
     * @param slotIndex Slot index
     * @param item Item data (empty id to clear)
     */
    void SetSlot(int slotIndex, const ItemData& item);

    /**
     * @brief Clear slot
     */
    void ClearSlot(int slotIndex);

    /**
     * @brief Get item in slot
     */
    ItemData GetSlotItem(int slotIndex) const;

    /**
     * @brief Find first empty slot
     * @return Slot index or -1 if full
     */
    int FindEmptySlot() const;

    /**
     * @brief Find slot containing item
     * @param itemId Item ID
     * @return Slot index or -1 if not found
     */
    int FindItemSlot(const std::string& itemId) const;

    /**
     * @brief Add item to inventory
     * @param item Item to add
     * @return Slot index where added, -1 if failed
     */
    int AddItem(const ItemData& item);

    /**
     * @brief Remove item from inventory
     * @param slotIndex Slot to remove from
     * @param count Number to remove (for stacks)
     * @return true if removed successfully
     */
    bool RemoveItem(int slotIndex, int count = 1);

    /**
     * @brief Move item between slots
     */
    bool MoveItem(int fromSlot, int toSlot);

    /**
     * @brief Swap items between slots
     */
    void SwapItems(int slot1, int slot2);

    /**
     * @brief Split stack
     * @param slotIndex Source slot
     * @param count Number to split off
     * @param targetSlot Target slot (-1 to find empty)
     * @return true if split successful
     */
    bool SplitStack(int slotIndex, int count, int targetSlot = -1);

    // Equipment

    /**
     * @brief Set equipment slots
     */
    void SetEquipmentSlots(const std::vector<EquipmentSlot>& slots);

    /**
     * @brief Equip item
     * @param inventorySlot Source inventory slot
     * @param equipmentSlot Target equipment slot type
     * @return true if equipped successfully
     */
    bool EquipItem(int inventorySlot, const std::string& equipmentSlot);

    /**
     * @brief Unequip item
     * @param equipmentSlot Equipment slot type
     * @return true if unequipped successfully
     */
    bool UnequipItem(const std::string& equipmentSlot);

    /**
     * @brief Get equipped item
     */
    ItemData GetEquippedItem(const std::string& slotType) const;

    // Tooltips

    /**
     * @brief Show item tooltip
     */
    void ShowTooltip(const ItemData& item, int x, int y);

    /**
     * @brief Hide tooltip
     */
    void HideTooltip();

    /**
     * @brief Set tooltip format function
     */
    void SetTooltipFormatter(std::function<std::string(const ItemData&)> formatter);

    // Filtering and Sorting

    /**
     * @brief Set filter by item type
     */
    void SetFilter(const std::string& type);

    /**
     * @brief Clear filter
     */
    void ClearFilter();

    /**
     * @brief Sort inventory
     * @param sortBy "name", "type", "rarity", "stack"
     * @param ascending Sort direction
     */
    void SortInventory(const std::string& sortBy, bool ascending = true);

    // Callbacks

    /**
     * @brief Set callback for item use
     */
    void SetItemUseCallback(std::function<void(const ItemData&)> callback);

    /**
     * @brief Set callback for item drop
     */
    void SetItemDropCallback(std::function<bool(const ItemData&, int count)> callback);

    /**
     * @brief Set callback for item move
     */
    void SetItemMoveCallback(std::function<bool(int from, int to)> callback);

    /**
     * @brief Set callback for equip
     */
    void SetEquipCallback(std::function<bool(const ItemData&, const std::string&)> callback);

    // Locking

    /**
     * @brief Lock slot (prevent modifications)
     */
    void LockSlot(int slotIndex);

    /**
     * @brief Unlock slot
     */
    void UnlockSlot(int slotIndex);

    /**
     * @brief Lock all slots
     */
    void LockAll();

    /**
     * @brief Unlock all slots
     */
    void UnlockAll();

    // Visual

    /**
     * @brief Highlight slot
     */
    void HighlightSlot(int slotIndex, bool highlight);

    /**
     * @brief Highlight valid drop targets
     */
    void HighlightValidDropTargets(const ItemData& draggedItem);

    /**
     * @brief Clear all highlights
     */
    void ClearHighlights();

    /**
     * @brief Set gold/currency display
     */
    void SetCurrency(const std::string& type, int amount);

    /**
     * @brief Set weight display
     */
    void SetWeight(float current, float max);

private:
    void SetupDataBindings();
    void SetupEventHandlers();
    void RefreshInventoryDisplay();
    void RefreshEquipmentDisplay();
    void HandleDragStart(int slotIndex);
    void HandleDragEnd(int targetSlot);
    void HandleRightClick(int slotIndex);
    void HandleDoubleClick(int slotIndex);

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    Engine::UI::UIWindow* m_window = nullptr;
    Engine::UI::UIDataBinding* m_dataBinding = nullptr;

    bool m_visible = false;
    int m_rows = 4;
    int m_cols = 8;

    std::vector<InventorySlot> m_slots;
    std::unordered_map<std::string, EquipmentSlot> m_equipmentSlots;
    std::unordered_map<std::string, ItemData> m_items; // Item database cache

    DragData m_dragData;
    std::string m_currentFilter;

    std::function<void(const ItemData&)> m_itemUseCallback;
    std::function<bool(const ItemData&, int)> m_itemDropCallback;
    std::function<bool(int, int)> m_itemMoveCallback;
    std::function<bool(const ItemData&, const std::string&)> m_equipCallback;
    std::function<std::string(const ItemData&)> m_tooltipFormatter;

    std::unordered_map<std::string, int> m_currencies;
    float m_currentWeight = 0;
    float m_maxWeight = 100;
};

} // namespace UI
} // namespace Game
