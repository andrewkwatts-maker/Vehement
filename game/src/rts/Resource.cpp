#include "Resource.hpp"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace Vehement {
namespace RTS {

// ============================================================================
// Resource Type Helpers
// ============================================================================

const char* GetResourceName(ResourceType type) {
    switch (type) {
        case ResourceType::Food:       return "Food";
        case ResourceType::Wood:       return "Wood";
        case ResourceType::Stone:      return "Stone";
        case ResourceType::Metal:      return "Metal";
        case ResourceType::Coins:      return "Coins";
        case ResourceType::Fuel:       return "Fuel";
        case ResourceType::Medicine:   return "Medicine";
        case ResourceType::Ammunition: return "Ammunition";
        default:                       return "Unknown";
    }
}

const char* GetResourceIcon(ResourceType type) {
    switch (type) {
        case ResourceType::Food:       return "Vehement2/images/UI/resource_food.png";
        case ResourceType::Wood:       return "Vehement2/images/UI/resource_wood.png";
        case ResourceType::Stone:      return "Vehement2/images/UI/resource_stone.png";
        case ResourceType::Metal:      return "Vehement2/images/UI/resource_metal.png";
        case ResourceType::Coins:      return "Vehement2/images/UI/resource_coins.png";
        case ResourceType::Fuel:       return "Vehement2/images/UI/resource_fuel.png";
        case ResourceType::Medicine:   return "Vehement2/images/UI/resource_medicine.png";
        case ResourceType::Ammunition: return "Vehement2/images/UI/resource_ammo.png";
        default:                       return "";
    }
}

uint32_t GetResourceColor(ResourceType type) {
    switch (type) {
        case ResourceType::Food:       return 0x8BC34AFF; // Light green
        case ResourceType::Wood:       return 0x795548FF; // Brown
        case ResourceType::Stone:      return 0x9E9E9EFF; // Gray
        case ResourceType::Metal:      return 0x607D8BFF; // Blue-gray
        case ResourceType::Coins:      return 0xFFC107FF; // Gold
        case ResourceType::Fuel:       return 0xFF9800FF; // Orange
        case ResourceType::Medicine:   return 0xE91E63FF; // Pink
        case ResourceType::Ammunition: return 0xF44336FF; // Red
        default:                       return 0xFFFFFFFF; // White
    }
}

// ============================================================================
// ResourceCost Implementation
// ============================================================================

std::string ResourceCost::ToString() const {
    if (costs.empty()) return "Free";

    std::stringstream ss;
    bool first = true;
    for (const auto& [type, amount] : costs) {
        if (!first) ss << ", ";
        ss << amount << " " << GetResourceName(type);
        first = false;
    }
    return ss.str();
}

// ============================================================================
// ResourceStock Implementation
// ============================================================================

ResourceStock::ResourceStock() {
    // Initialize all resource types to zero
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        amounts[type] = 0;
        capacity[type] = 1000; // Default capacity
        incomeRate[type] = 0.0f;
        expenseRate[type] = 0.0f;
        m_fractionalAccumulator[type] = 0.0f;
    }

    // Coins have unlimited capacity
    capacity[ResourceType::Coins] = 999999;
}

int ResourceStock::GetAmount(ResourceType type) const {
    auto it = amounts.find(type);
    return it != amounts.end() ? it->second : 0;
}

int ResourceStock::GetCapacity(ResourceType type) const {
    auto it = capacity.find(type);
    return it != capacity.end() ? it->second : 0;
}

int ResourceStock::GetFreeSpace(ResourceType type) const {
    return std::max(0, GetCapacity(type) - GetAmount(type));
}

bool ResourceStock::IsFull(ResourceType type) const {
    return GetAmount(type) >= GetCapacity(type);
}

float ResourceStock::GetFillPercentage(ResourceType type) const {
    int cap = GetCapacity(type);
    if (cap <= 0) return 1.0f;
    return static_cast<float>(GetAmount(type)) / cap;
}

float ResourceStock::GetNetRate(ResourceType type) const {
    float income = 0.0f, expense = 0.0f;

    auto incIt = incomeRate.find(type);
    if (incIt != incomeRate.end()) income = incIt->second;

    auto expIt = expenseRate.find(type);
    if (expIt != expenseRate.end()) expense = expIt->second;

    return income - expense;
}

bool ResourceStock::CanAfford(ResourceType type, int amount) const {
    return GetAmount(type) >= amount;
}

bool ResourceStock::CanAfford(const ResourceCost& cost) const {
    for (const auto& [type, amount] : cost.costs) {
        if (!CanAfford(type, amount)) return false;
    }
    return true;
}

ResourceCost ResourceStock::GetMissing(const ResourceCost& cost) const {
    ResourceCost missing;
    for (const auto& [type, amount] : cost.costs) {
        int have = GetAmount(type);
        if (have < amount) {
            missing.Add(type, amount - have);
        }
    }
    return missing;
}

int ResourceStock::Add(ResourceType type, int amount) {
    if (amount <= 0) return 0;

    int oldAmount = GetAmount(type);
    int cap = GetCapacity(type);
    int space = std::max(0, cap - oldAmount);
    int actualAdd = std::min(amount, space);

    if (actualAdd > 0) {
        amounts[type] = oldAmount + actualAdd;
        NotifyChange(type, oldAmount, amounts[type]);
    }

    return actualAdd;
}

bool ResourceStock::Remove(ResourceType type, int amount) {
    if (amount <= 0) return true;

    int oldAmount = GetAmount(type);
    if (oldAmount < amount) return false;

    amounts[type] = oldAmount - amount;
    NotifyChange(type, oldAmount, amounts[type]);
    CheckLowResource(type);

    return true;
}

bool ResourceStock::Spend(const ResourceCost& cost) {
    if (!CanAfford(cost)) return false;

    for (const auto& [type, amount] : cost.costs) {
        Remove(type, amount);
    }
    return true;
}

void ResourceStock::Set(ResourceType type, int amount) {
    int oldAmount = GetAmount(type);
    amounts[type] = std::max(0, amount);
    if (oldAmount != amounts[type]) {
        NotifyChange(type, oldAmount, amounts[type]);
        CheckLowResource(type);
    }
}

void ResourceStock::SetCapacity(ResourceType type, int cap) {
    capacity[type] = std::max(0, cap);
    // Clamp current amount if over new capacity
    if (GetAmount(type) > cap) {
        Set(type, cap);
    }
}

void ResourceStock::AddCapacity(ResourceType type, int additionalCap) {
    SetCapacity(type, GetCapacity(type) + additionalCap);
}

void ResourceStock::SetIncomeRate(ResourceType type, float rate) {
    incomeRate[type] = std::max(0.0f, rate);
}

void ResourceStock::AddIncomeRate(ResourceType type, float rate) {
    incomeRate[type] = std::max(0.0f, incomeRate[type] + rate);
}

void ResourceStock::SetExpenseRate(ResourceType type, float rate) {
    expenseRate[type] = std::max(0.0f, rate);
}

void ResourceStock::AddExpenseRate(ResourceType type, float rate) {
    expenseRate[type] = std::max(0.0f, expenseRate[type] + rate);
}

void ResourceStock::ApplyRates(float deltaTime) {
    for (int i = 0; i < static_cast<int>(ResourceType::COUNT); ++i) {
        auto type = static_cast<ResourceType>(i);
        float netRate = GetNetRate(type);

        if (std::abs(netRate) < 0.0001f) continue;

        // Accumulate fractional changes
        m_fractionalAccumulator[type] += netRate * deltaTime;

        // Apply whole units
        int wholeUnits = static_cast<int>(m_fractionalAccumulator[type]);
        if (wholeUnits != 0) {
            m_fractionalAccumulator[type] -= wholeUnits;

            if (wholeUnits > 0) {
                Add(type, wholeUnits);
            } else {
                // Don't remove more than we have
                int toRemove = std::min(-wholeUnits, GetAmount(type));
                if (toRemove > 0) {
                    Remove(type, toRemove);
                }
            }
        }
    }
}

void ResourceStock::SetLowThreshold(ResourceType type, int threshold) {
    m_lowThresholds[type] = threshold;
}

void ResourceStock::Clear() {
    for (auto& [type, amount] : amounts) {
        amount = 0;
    }
    for (auto& [type, acc] : m_fractionalAccumulator) {
        acc = 0.0f;
    }
}

void ResourceStock::InitializeDefaults() {
    // Starting resources for a new game
    Set(ResourceType::Food, 100);
    Set(ResourceType::Wood, 50);
    Set(ResourceType::Stone, 25);
    Set(ResourceType::Metal, 10);
    Set(ResourceType::Coins, 50);
    Set(ResourceType::Fuel, 20);
    Set(ResourceType::Medicine, 10);
    Set(ResourceType::Ammunition, 50);

    // Set default capacities
    SetCapacity(ResourceType::Food, 500);
    SetCapacity(ResourceType::Wood, 500);
    SetCapacity(ResourceType::Stone, 500);
    SetCapacity(ResourceType::Metal, 300);
    SetCapacity(ResourceType::Coins, 999999);
    SetCapacity(ResourceType::Fuel, 200);
    SetCapacity(ResourceType::Medicine, 100);
    SetCapacity(ResourceType::Ammunition, 500);

    // Set low thresholds for warnings
    SetLowThreshold(ResourceType::Food, 20);
    SetLowThreshold(ResourceType::Wood, 15);
    SetLowThreshold(ResourceType::Stone, 10);
    SetLowThreshold(ResourceType::Metal, 5);
    SetLowThreshold(ResourceType::Fuel, 10);
    SetLowThreshold(ResourceType::Medicine, 5);
    SetLowThreshold(ResourceType::Ammunition, 20);
}

int ResourceStock::GetTotalValue() const {
    const auto& values = GetResourceValues();
    int total = 0;

    for (const auto& [type, amount] : amounts) {
        total += static_cast<int>(amount * values.GetBaseValue(type));
    }

    return total;
}

void ResourceStock::NotifyChange(ResourceType type, int oldAmount, int newAmount) {
    if (m_onResourceChanged) {
        m_onResourceChanged(type, oldAmount, newAmount);
    }
}

void ResourceStock::CheckLowResource(ResourceType type) {
    if (!m_onLowResource) return;

    auto it = m_lowThresholds.find(type);
    if (it == m_lowThresholds.end()) return;

    int amount = GetAmount(type);
    if (amount <= it->second) {
        m_onLowResource(type, amount, it->second);
    }
}

// ============================================================================
// ResourceValueTable Implementation
// ============================================================================

ResourceValueTable::ResourceValueTable() {
    // Base values in coins per unit
    baseValues[ResourceType::Food] = 2.0f;
    baseValues[ResourceType::Wood] = 1.5f;
    baseValues[ResourceType::Stone] = 2.0f;
    baseValues[ResourceType::Metal] = 4.0f;
    baseValues[ResourceType::Coins] = 1.0f;
    baseValues[ResourceType::Fuel] = 3.0f;
    baseValues[ResourceType::Medicine] = 5.0f;
    baseValues[ResourceType::Ammunition] = 1.0f;
}

float ResourceValueTable::GetBaseValue(ResourceType type) const {
    auto it = baseValues.find(type);
    return it != baseValues.end() ? it->second : 1.0f;
}

int ResourceValueTable::CalculateValue(const ResourceCost& cost) const {
    float total = 0.0f;
    for (const auto& [type, amount] : cost.costs) {
        total += amount * GetBaseValue(type);
    }
    return static_cast<int>(total);
}

// Global resource value table
static ResourceValueTable s_resourceValues;

const ResourceValueTable& GetResourceValues() {
    return s_resourceValues;
}

} // namespace RTS
} // namespace Vehement
