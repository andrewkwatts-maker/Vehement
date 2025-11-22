#include "InventoryUI.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIDataBinding.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include "engine/ui/runtime/UIAnimation.hpp"

#include <algorithm>
#include <sstream>

namespace Game {
namespace UI {

InventoryUI::InventoryUI() = default;
InventoryUI::~InventoryUI() { Shutdown(); }

bool InventoryUI::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;
    m_uiManager = uiManager;

    m_window = m_uiManager->CreateWindow("inventory", "game/assets/ui/html/inventory.html",
                                         Engine::UI::UILayer::Windows);
    if (!m_window) return false;

    m_window->SetTitle("Inventory");
    m_window->SetResizable(true);
    m_window->SetMinSize(300, 400);
    m_window->Center();
    m_window->Hide();

    SetInventorySize(m_rows, m_cols);
    SetupDataBindings();
    SetupEventHandlers();

    return true;
}

void InventoryUI::Shutdown() {
    if (m_uiManager && m_window) {
        m_uiManager->CloseWindow("inventory");
        m_window = nullptr;
    }
}

void InventoryUI::Update(float deltaTime) {
    // Update drag visual if dragging
}

void InventoryUI::Show() {
    m_visible = true;
    if (m_window) {
        m_window->Show();
        m_uiManager->GetAnimation().Play("scaleIn", "inventory");
    }
    RefreshInventoryDisplay();
}

void InventoryUI::Hide() {
    m_visible = false;
    HideTooltip();
    if (m_window) {
        m_window->Hide();
    }
}

void InventoryUI::Toggle() {
    if (m_visible) Hide();
    else Show();
}

void InventoryUI::SetInventorySize(int rows, int cols) {
    m_rows = rows;
    m_cols = cols;
    m_slots.resize(rows * cols);

    for (int i = 0; i < rows * cols; ++i) {
        m_slots[i].index = i;
        m_slots[i].itemId = "";
        m_slots[i].stackCount = 0;
        m_slots[i].isLocked = false;
    }

    if (m_dataBinding) {
        m_dataBinding->SetValue("inventory.rows", rows);
        m_dataBinding->SetValue("inventory.cols", cols);
        m_dataBinding->SetValue("inventory.totalSlots", rows * cols);
    }

    RefreshInventoryDisplay();
}

void InventoryUI::SetSlot(int slotIndex, const ItemData& item) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return;

    m_slots[slotIndex].itemId = item.id;
    m_slots[slotIndex].stackCount = item.stackCount;

    if (!item.id.empty()) {
        m_items[item.id] = item;
    }

    RefreshInventoryDisplay();
}

void InventoryUI::ClearSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return;

    m_slots[slotIndex].itemId = "";
    m_slots[slotIndex].stackCount = 0;

    RefreshInventoryDisplay();
}

ItemData InventoryUI::GetSlotItem(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return ItemData();

    const auto& slot = m_slots[slotIndex];
    if (slot.itemId.empty()) return ItemData();

    auto it = m_items.find(slot.itemId);
    if (it != m_items.end()) {
        ItemData item = it->second;
        item.stackCount = slot.stackCount;
        return item;
    }

    return ItemData();
}

int InventoryUI::FindEmptySlot() const {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].itemId.empty() && !m_slots[i].isLocked) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryUI::FindItemSlot(const std::string& itemId) const {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].itemId == itemId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int InventoryUI::AddItem(const ItemData& item) {
    // First try to stack with existing item
    if (item.maxStack > 1) {
        for (size_t i = 0; i < m_slots.size(); ++i) {
            if (m_slots[i].itemId == item.id &&
                m_slots[i].stackCount < item.maxStack) {
                int canAdd = item.maxStack - m_slots[i].stackCount;
                int toAdd = std::min(canAdd, item.stackCount);
                m_slots[i].stackCount += toAdd;
                RefreshInventoryDisplay();
                return static_cast<int>(i);
            }
        }
    }

    // Find empty slot
    int slot = FindEmptySlot();
    if (slot >= 0) {
        SetSlot(slot, item);
    }
    return slot;
}

bool InventoryUI::RemoveItem(int slotIndex, int count) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return false;
    if (m_slots[slotIndex].isLocked) return false;

    auto& slot = m_slots[slotIndex];
    if (slot.itemId.empty()) return false;

    slot.stackCount -= count;
    if (slot.stackCount <= 0) {
        slot.itemId = "";
        slot.stackCount = 0;
    }

    RefreshInventoryDisplay();
    return true;
}

bool InventoryUI::MoveItem(int fromSlot, int toSlot) {
    if (fromSlot < 0 || fromSlot >= static_cast<int>(m_slots.size())) return false;
    if (toSlot < 0 || toSlot >= static_cast<int>(m_slots.size())) return false;
    if (m_slots[fromSlot].isLocked || m_slots[toSlot].isLocked) return false;

    if (m_itemMoveCallback && !m_itemMoveCallback(fromSlot, toSlot)) {
        return false;
    }

    if (m_slots[toSlot].itemId.empty()) {
        // Move to empty slot
        m_slots[toSlot] = m_slots[fromSlot];
        m_slots[toSlot].index = toSlot;
        m_slots[fromSlot].itemId = "";
        m_slots[fromSlot].stackCount = 0;
    } else if (m_slots[fromSlot].itemId == m_slots[toSlot].itemId) {
        // Stack items
        auto& fromItem = m_items[m_slots[fromSlot].itemId];
        int canAdd = fromItem.maxStack - m_slots[toSlot].stackCount;
        int toAdd = std::min(canAdd, m_slots[fromSlot].stackCount);
        m_slots[toSlot].stackCount += toAdd;
        m_slots[fromSlot].stackCount -= toAdd;
        if (m_slots[fromSlot].stackCount <= 0) {
            m_slots[fromSlot].itemId = "";
            m_slots[fromSlot].stackCount = 0;
        }
    } else {
        // Swap items
        SwapItems(fromSlot, toSlot);
    }

    RefreshInventoryDisplay();
    return true;
}

void InventoryUI::SwapItems(int slot1, int slot2) {
    std::swap(m_slots[slot1].itemId, m_slots[slot2].itemId);
    std::swap(m_slots[slot1].stackCount, m_slots[slot2].stackCount);
}

bool InventoryUI::SplitStack(int slotIndex, int count, int targetSlot) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_slots.size())) return false;
    if (m_slots[slotIndex].stackCount <= count) return false;

    if (targetSlot < 0) {
        targetSlot = FindEmptySlot();
    }
    if (targetSlot < 0) return false;

    ItemData splitItem = GetSlotItem(slotIndex);
    splitItem.stackCount = count;

    m_slots[slotIndex].stackCount -= count;
    SetSlot(targetSlot, splitItem);

    return true;
}

void InventoryUI::SetEquipmentSlots(const std::vector<EquipmentSlot>& slots) {
    m_equipmentSlots.clear();
    for (const auto& slot : slots) {
        m_equipmentSlots[slot.slotType] = slot;
    }
    RefreshEquipmentDisplay();
}

bool InventoryUI::EquipItem(int inventorySlot, const std::string& equipmentSlot) {
    ItemData item = GetSlotItem(inventorySlot);
    if (item.id.empty()) return false;

    auto it = m_equipmentSlots.find(equipmentSlot);
    if (it == m_equipmentSlots.end()) return false;
    if (it->second.isLocked) return false;

    if (m_equipCallback && !m_equipCallback(item, equipmentSlot)) {
        return false;
    }

    // If slot has item, swap to inventory
    if (!it->second.itemId.empty()) {
        ItemData equippedItem = m_items[it->second.itemId];
        equippedItem.isEquipped = false;
        SetSlot(inventorySlot, equippedItem);
    } else {
        ClearSlot(inventorySlot);
    }

    it->second.itemId = item.id;
    item.isEquipped = true;
    m_items[item.id] = item;

    RefreshEquipmentDisplay();
    return true;
}

bool InventoryUI::UnequipItem(const std::string& equipmentSlot) {
    auto it = m_equipmentSlots.find(equipmentSlot);
    if (it == m_equipmentSlots.end()) return false;
    if (it->second.itemId.empty()) return false;

    int slot = FindEmptySlot();
    if (slot < 0) return false;

    ItemData item = m_items[it->second.itemId];
    item.isEquipped = false;
    SetSlot(slot, item);

    it->second.itemId = "";
    RefreshEquipmentDisplay();
    return true;
}

ItemData InventoryUI::GetEquippedItem(const std::string& slotType) const {
    auto it = m_equipmentSlots.find(slotType);
    if (it == m_equipmentSlots.end() || it->second.itemId.empty()) {
        return ItemData();
    }

    auto itemIt = m_items.find(it->second.itemId);
    return itemIt != m_items.end() ? itemIt->second : ItemData();
}

void InventoryUI::ShowTooltip(const ItemData& item, int x, int y) {
    std::string tooltipHtml;
    if (m_tooltipFormatter) {
        tooltipHtml = m_tooltipFormatter(item);
    } else {
        std::stringstream ss;
        ss << "<div class='tooltip-title rarity-" << item.rarity << "'>" << item.name << "</div>";
        ss << "<div class='tooltip-type'>" << item.type << "</div>";
        ss << "<div class='tooltip-desc'>" << item.description << "</div>";
        if (item.stackCount > 1) {
            ss << "<div class='tooltip-stack'>Stack: " << item.stackCount << "/" << item.maxStack << "</div>";
        }
        for (const auto& [stat, value] : item.stats) {
            ss << "<div class='tooltip-stat'>" << stat << ": " << value << "</div>";
        }
        tooltipHtml = ss.str();
    }

    nlohmann::json args;
    args["html"] = tooltipHtml;
    args["x"] = x;
    args["y"] = y;
    m_uiManager->GetBinding().CallJS("UI.showTooltip", args);
}

void InventoryUI::HideTooltip() {
    m_uiManager->GetBinding().CallJS("UI.hideTooltip", {});
}

void InventoryUI::SetTooltipFormatter(std::function<std::string(const ItemData&)> formatter) {
    m_tooltipFormatter = formatter;
}

void InventoryUI::SetFilter(const std::string& type) {
    m_currentFilter = type;
    RefreshInventoryDisplay();
}

void InventoryUI::ClearFilter() {
    m_currentFilter = "";
    RefreshInventoryDisplay();
}

void InventoryUI::SortInventory(const std::string& sortBy, bool ascending) {
    std::vector<std::pair<int, ItemData>> itemsToSort;
    for (size_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].itemId.empty()) {
            itemsToSort.emplace_back(static_cast<int>(i), GetSlotItem(static_cast<int>(i)));
        }
    }

    std::sort(itemsToSort.begin(), itemsToSort.end(),
        [&sortBy, ascending](const auto& a, const auto& b) {
            int cmp = 0;
            if (sortBy == "name") {
                cmp = a.second.name.compare(b.second.name);
            } else if (sortBy == "type") {
                cmp = a.second.type.compare(b.second.type);
            } else if (sortBy == "rarity") {
                cmp = a.second.rarity.compare(b.second.rarity);
            } else if (sortBy == "stack") {
                cmp = a.second.stackCount - b.second.stackCount;
            }
            return ascending ? (cmp < 0) : (cmp > 0);
        });

    // Clear all slots
    for (auto& slot : m_slots) {
        slot.itemId = "";
        slot.stackCount = 0;
    }

    // Re-add sorted items
    for (size_t i = 0; i < itemsToSort.size(); ++i) {
        SetSlot(static_cast<int>(i), itemsToSort[i].second);
    }
}

void InventoryUI::SetItemUseCallback(std::function<void(const ItemData&)> callback) {
    m_itemUseCallback = callback;
}

void InventoryUI::SetItemDropCallback(std::function<bool(const ItemData&, int)> callback) {
    m_itemDropCallback = callback;
}

void InventoryUI::SetItemMoveCallback(std::function<bool(int, int)> callback) {
    m_itemMoveCallback = callback;
}

void InventoryUI::SetEquipCallback(std::function<bool(const ItemData&, const std::string&)> callback) {
    m_equipCallback = callback;
}

void InventoryUI::LockSlot(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < static_cast<int>(m_slots.size())) {
        m_slots[slotIndex].isLocked = true;
    }
}

void InventoryUI::UnlockSlot(int slotIndex) {
    if (slotIndex >= 0 && slotIndex < static_cast<int>(m_slots.size())) {
        m_slots[slotIndex].isLocked = false;
    }
}

void InventoryUI::LockAll() {
    for (auto& slot : m_slots) slot.isLocked = true;
}

void InventoryUI::UnlockAll() {
    for (auto& slot : m_slots) slot.isLocked = false;
}

void InventoryUI::HighlightSlot(int slotIndex, bool highlight) {
    if (slotIndex >= 0 && slotIndex < static_cast<int>(m_slots.size())) {
        m_slots[slotIndex].isHighlighted = highlight;
        RefreshInventoryDisplay();
    }
}

void InventoryUI::HighlightValidDropTargets(const ItemData& draggedItem) {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        bool valid = !m_slots[i].isLocked;
        if (!m_currentFilter.empty() && draggedItem.type != m_currentFilter) {
            valid = false;
        }
        m_slots[i].isHighlighted = valid;
    }
    RefreshInventoryDisplay();
}

void InventoryUI::ClearHighlights() {
    for (auto& slot : m_slots) slot.isHighlighted = false;
    RefreshInventoryDisplay();
}

void InventoryUI::SetCurrency(const std::string& type, int amount) {
    m_currencies[type] = amount;
    if (m_dataBinding) {
        m_dataBinding->SetValue("inventory.currency." + type, amount);
    }
}

void InventoryUI::SetWeight(float current, float max) {
    m_currentWeight = current;
    m_maxWeight = max;
    if (m_dataBinding) {
        m_dataBinding->SetValue("inventory.weight.current", current);
        m_dataBinding->SetValue("inventory.weight.max", max);
        m_dataBinding->SetValue("inventory.weight.percent", max > 0 ? (current / max * 100) : 0);
    }
}

void InventoryUI::SetupDataBindings() {
    m_dataBinding = &m_uiManager->GetBinding().GetDataBinding();
}

void InventoryUI::SetupEventHandlers() {
    auto& binding = m_uiManager->GetBinding();

    binding.ExposeFunction("Inventory.onSlotClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("slot")) {
            int slot = args["slot"].get<int>();
            HandleDoubleClick(slot);
        }
        return nullptr;
    });

    binding.ExposeFunction("Inventory.onDragStart", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("slot")) {
            HandleDragStart(args["slot"].get<int>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Inventory.onDrop", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("slot")) {
            HandleDragEnd(args["slot"].get<int>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Inventory.onRightClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("slot")) {
            HandleRightClick(args["slot"].get<int>());
        }
        return nullptr;
    });

    binding.ExposeFunction("Inventory.onHover", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("slot") && args.contains("x") && args.contains("y")) {
            int slot = args["slot"].get<int>();
            ItemData item = GetSlotItem(slot);
            if (!item.id.empty()) {
                ShowTooltip(item, args["x"].get<int>(), args["y"].get<int>());
            }
        }
        return nullptr;
    });

    binding.ExposeFunction("Inventory.onHoverEnd", [this](const nlohmann::json&) -> nlohmann::json {
        HideTooltip();
        return nullptr;
    });
}

void InventoryUI::RefreshInventoryDisplay() {
    if (!m_dataBinding) return;

    nlohmann::json slotsJson = nlohmann::json::array();
    for (const auto& slot : m_slots) {
        nlohmann::json s;
        s["index"] = slot.index;
        s["isEmpty"] = slot.itemId.empty();
        s["isLocked"] = slot.isLocked;
        s["isHighlighted"] = slot.isHighlighted;

        if (!slot.itemId.empty()) {
            auto it = m_items.find(slot.itemId);
            if (it != m_items.end()) {
                s["itemId"] = it->second.id;
                s["name"] = it->second.name;
                s["icon"] = it->second.iconPath;
                s["rarity"] = it->second.rarity;
                s["stackCount"] = slot.stackCount;
                s["maxStack"] = it->second.maxStack;
            }
        }

        // Apply filter
        if (!m_currentFilter.empty() && !slot.itemId.empty()) {
            auto it = m_items.find(slot.itemId);
            if (it != m_items.end() && it->second.type != m_currentFilter) {
                s["filtered"] = true;
            }
        }

        slotsJson.push_back(s);
    }

    m_dataBinding->SetValue("inventory.slots", slotsJson);
}

void InventoryUI::RefreshEquipmentDisplay() {
    if (!m_dataBinding) return;

    nlohmann::json equipJson = nlohmann::json::object();
    for (const auto& [slotType, slot] : m_equipmentSlots) {
        nlohmann::json s;
        s["slotType"] = slotType;
        s["isEmpty"] = slot.itemId.empty();
        s["isLocked"] = slot.isLocked;

        if (!slot.itemId.empty()) {
            auto it = m_items.find(slot.itemId);
            if (it != m_items.end()) {
                s["itemId"] = it->second.id;
                s["name"] = it->second.name;
                s["icon"] = it->second.iconPath;
                s["rarity"] = it->second.rarity;
            }
        }

        equipJson[slotType] = s;
    }

    m_dataBinding->SetValue("inventory.equipment", equipJson);
}

void InventoryUI::HandleDragStart(int slotIndex) {
    ItemData item = GetSlotItem(slotIndex);
    if (item.id.empty()) return;

    m_dragData.isActive = true;
    m_dragData.sourceType = "inventory";
    m_dragData.sourceSlot = slotIndex;
    m_dragData.itemId = item.id;

    HighlightValidDropTargets(item);
}

void InventoryUI::HandleDragEnd(int targetSlot) {
    if (!m_dragData.isActive) return;

    if (m_dragData.sourceType == "inventory") {
        MoveItem(m_dragData.sourceSlot, targetSlot);
    }

    m_dragData.isActive = false;
    ClearHighlights();
}

void InventoryUI::HandleRightClick(int slotIndex) {
    ItemData item = GetSlotItem(slotIndex);
    if (item.id.empty()) return;

    // Show context menu or use item
    if (item.type == "consumable" && m_itemUseCallback) {
        m_itemUseCallback(item);
    }
}

void InventoryUI::HandleDoubleClick(int slotIndex) {
    ItemData item = GetSlotItem(slotIndex);
    if (item.id.empty()) return;

    if (item.type == "weapon" || item.type == "armor" || item.type == "accessory") {
        // Try to equip
        std::string equipSlot = item.type == "weapon" ? "weapon" :
                               item.type == "armor" ? "chest" : "accessory1";
        EquipItem(slotIndex, equipSlot);
    } else if (m_itemUseCallback) {
        m_itemUseCallback(item);
    }
}

} // namespace UI
} // namespace Game
