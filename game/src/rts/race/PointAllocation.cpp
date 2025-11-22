#include "PointAllocation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace Vehement {
namespace RTS {
namespace Race {

// ============================================================================
// MilitaryAllocation Implementation
// ============================================================================

void MilitaryAllocation::ComputeBonuses() {
    // Base calculation: points above 25 give bonuses, below give penalties
    // Range: -25% to +25% per sub-category

    // Infantry bonuses
    float infantryFactor = (infantry - 25) / 100.0f;
    infantryDamageBonus = infantryFactor * 0.5f;  // Up to +12.5% damage
    infantryArmorBonus = infantryFactor * 0.4f;   // Up to +10% armor

    // Ranged bonuses
    float rangedFactor = (ranged - 25) / 100.0f;
    rangedDamageBonus = rangedFactor * 0.5f;      // Up to +12.5% damage
    rangedRangeBonus = rangedFactor * 0.3f;       // Up to +7.5% range

    // Cavalry bonuses
    float cavalryFactor = (cavalry - 25) / 100.0f;
    cavalrySpeedBonus = cavalryFactor * 0.4f;     // Up to +10% speed
    cavalryChargeBonus = cavalryFactor * 0.6f;    // Up to +15% charge damage

    // Siege bonuses
    float siegeFactor = (siege - 25) / 100.0f;
    siegeDamageBonus = siegeFactor * 0.6f;        // Up to +15% damage
    siegeRangeBonus = siegeFactor * 0.3f;         // Up to +7.5% range
}

nlohmann::json MilitaryAllocation::ToJson() const {
    return {
        {"infantry", infantry},
        {"ranged", ranged},
        {"cavalry", cavalry},
        {"siege", siege},
        {"bonuses", {
            {"infantryDamageBonus", infantryDamageBonus},
            {"infantryArmorBonus", infantryArmorBonus},
            {"rangedDamageBonus", rangedDamageBonus},
            {"rangedRangeBonus", rangedRangeBonus},
            {"cavalrySpeedBonus", cavalrySpeedBonus},
            {"cavalryChargeBonus", cavalryChargeBonus},
            {"siegeDamageBonus", siegeDamageBonus},
            {"siegeRangeBonus", siegeRangeBonus}
        }}
    };
}

MilitaryAllocation MilitaryAllocation::FromJson(const nlohmann::json& j) {
    MilitaryAllocation alloc;
    if (j.contains("infantry")) alloc.infantry = j["infantry"].get<int>();
    if (j.contains("ranged")) alloc.ranged = j["ranged"].get<int>();
    if (j.contains("cavalry")) alloc.cavalry = j["cavalry"].get<int>();
    if (j.contains("siege")) alloc.siege = j["siege"].get<int>();
    alloc.ComputeBonuses();
    return alloc;
}

// ============================================================================
// EconomyAllocation Implementation
// ============================================================================

void EconomyAllocation::ComputeBonuses() {
    // Harvest speed: major impact on early game
    float harvestFactor = (harvestSpeed - 25) / 100.0f;
    harvestSpeedBonus = harvestFactor * 0.6f;     // Up to +15% gather rate

    // Build speed: affects expansion
    float buildFactor = (buildSpeed - 25) / 100.0f;
    buildSpeedBonus = buildFactor * 0.5f;         // Up to +12.5% build speed

    // Carry capacity: affects efficiency
    float carryFactor = (carryCapacity - 25) / 100.0f;
    carryCapacityBonus = carryFactor * 0.4f;      // Up to +10% carry

    // Trade profits: affects gold income
    float tradeFactor = (tradeProfits - 20) / 100.0f;
    tradeProfitBonus = tradeFactor * 0.8f;        // Up to +20% trade profits

    // Derived bonuses
    workerCostReduction = (harvestSpeedBonus + carryCapacityBonus) * 0.3f;
    storageBonus = carryCapacityBonus * 0.5f;
}

nlohmann::json EconomyAllocation::ToJson() const {
    return {
        {"harvestSpeed", harvestSpeed},
        {"buildSpeed", buildSpeed},
        {"carryCapacity", carryCapacity},
        {"tradeProfits", tradeProfits},
        {"bonuses", {
            {"harvestSpeedBonus", harvestSpeedBonus},
            {"buildSpeedBonus", buildSpeedBonus},
            {"carryCapacityBonus", carryCapacityBonus},
            {"tradeProfitBonus", tradeProfitBonus},
            {"workerCostReduction", workerCostReduction},
            {"storageBonus", storageBonus}
        }}
    };
}

EconomyAllocation EconomyAllocation::FromJson(const nlohmann::json& j) {
    EconomyAllocation alloc;
    if (j.contains("harvestSpeed")) alloc.harvestSpeed = j["harvestSpeed"].get<int>();
    if (j.contains("buildSpeed")) alloc.buildSpeed = j["buildSpeed"].get<int>();
    if (j.contains("carryCapacity")) alloc.carryCapacity = j["carryCapacity"].get<int>();
    if (j.contains("tradeProfits")) alloc.tradeProfits = j["tradeProfits"].get<int>();
    alloc.ComputeBonuses();
    return alloc;
}

// ============================================================================
// MagicAllocation Implementation
// ============================================================================

void MagicAllocation::ComputeBonuses() {
    // Spell damage: primary offensive stat
    float damageFactor = (spellDamage - 25) / 100.0f;
    spellDamageBonus = damageFactor * 0.6f;       // Up to +15% spell damage

    // Spell range: positioning advantage
    float rangeFactor = (spellRange - 25) / 100.0f;
    spellRangeBonus = rangeFactor * 0.4f;         // Up to +10% range

    // Mana cost: efficiency
    float manaCostFactor = (manaCost - 25) / 100.0f;
    manaCostReduction = manaCostFactor * 0.5f;    // Up to +12.5% efficiency

    // Cooldown reduction
    float cdFactor = (cooldownReduction - 20) / 100.0f;
    cooldownReductionBonus = cdFactor * 0.4f;     // Up to +10% CDR

    // Derived bonuses
    manaRegenBonus = manaCostReduction * 0.5f;
    maxManaBonus = (spellDamageBonus + manaCostReduction) * 0.3f;
}

nlohmann::json MagicAllocation::ToJson() const {
    return {
        {"spellDamage", spellDamage},
        {"spellRange", spellRange},
        {"manaCost", manaCost},
        {"cooldownReduction", cooldownReduction},
        {"bonuses", {
            {"spellDamageBonus", spellDamageBonus},
            {"spellRangeBonus", spellRangeBonus},
            {"manaCostReduction", manaCostReduction},
            {"cooldownReductionBonus", cooldownReductionBonus},
            {"manaRegenBonus", manaRegenBonus},
            {"maxManaBonus", maxManaBonus}
        }}
    };
}

MagicAllocation MagicAllocation::FromJson(const nlohmann::json& j) {
    MagicAllocation alloc;
    if (j.contains("spellDamage")) alloc.spellDamage = j["spellDamage"].get<int>();
    if (j.contains("spellRange")) alloc.spellRange = j["spellRange"].get<int>();
    if (j.contains("manaCost")) alloc.manaCost = j["manaCost"].get<int>();
    if (j.contains("cooldownReduction")) alloc.cooldownReduction = j["cooldownReduction"].get<int>();
    alloc.ComputeBonuses();
    return alloc;
}

// ============================================================================
// TechnologyAllocation Implementation
// ============================================================================

void TechnologyAllocation::ComputeBonuses() {
    // Research speed: faster tech
    float researchFactor = (researchSpeed - 33) / 100.0f;
    researchSpeedBonus = researchFactor * 0.6f;   // Up to +15% research speed

    // Age up cost: cheaper advancement
    float ageUpFactor = (ageUpCost - 33) / 100.0f;
    ageUpCostReduction = ageUpFactor * 0.5f;      // Up to +12.5% cost reduction

    // Unique techs: more powerful unique abilities
    float uniqueFactor = (uniqueTechs - 33) / 100.0f;
    uniqueTechBonus = uniqueFactor * 0.6f;        // Up to +15% power

    // Derived bonuses
    techProtectionBonus = (researchSpeedBonus + uniqueTechBonus) * 0.2f;
    bonusStartingTechs = static_cast<int>(uniqueTechBonus * 3);
}

nlohmann::json TechnologyAllocation::ToJson() const {
    return {
        {"researchSpeed", researchSpeed},
        {"ageUpCost", ageUpCost},
        {"uniqueTechs", uniqueTechs},
        {"bonuses", {
            {"researchSpeedBonus", researchSpeedBonus},
            {"ageUpCostReduction", ageUpCostReduction},
            {"uniqueTechBonus", uniqueTechBonus},
            {"techProtectionBonus", techProtectionBonus},
            {"bonusStartingTechs", bonusStartingTechs}
        }}
    };
}

TechnologyAllocation TechnologyAllocation::FromJson(const nlohmann::json& j) {
    TechnologyAllocation alloc;
    if (j.contains("researchSpeed")) alloc.researchSpeed = j["researchSpeed"].get<int>();
    if (j.contains("ageUpCost")) alloc.ageUpCost = j["ageUpCost"].get<int>();
    if (j.contains("uniqueTechs")) alloc.uniqueTechs = j["uniqueTechs"].get<int>();
    alloc.ComputeBonuses();
    return alloc;
}

// ============================================================================
// BalanceWarning Implementation
// ============================================================================

nlohmann::json BalanceWarning::ToJson() const {
    return {
        {"severity", static_cast<int>(severity)},
        {"category", category},
        {"message", message},
        {"deviation", deviation}
    };
}

BalanceWarning BalanceWarning::FromJson(const nlohmann::json& j) {
    BalanceWarning warning;
    if (j.contains("severity")) warning.severity = static_cast<BalanceWarningType>(j["severity"].get<int>());
    if (j.contains("category")) warning.category = j["category"].get<std::string>();
    if (j.contains("message")) warning.message = j["message"].get<std::string>();
    if (j.contains("deviation")) warning.deviation = j["deviation"].get<float>();
    return warning;
}

// ============================================================================
// BalanceScore Implementation
// ============================================================================

bool BalanceScore::HasCriticalWarnings() const {
    for (const auto& warning : warnings) {
        if (warning.severity == BalanceWarningType::Critical) {
            return true;
        }
    }
    return false;
}

nlohmann::json BalanceScore::ToJson() const {
    nlohmann::json warningsJson = nlohmann::json::array();
    for (const auto& warning : warnings) {
        warningsJson.push_back(warning.ToJson());
    }

    return {
        {"overallScore", overallScore},
        {"militaryBalance", militaryBalance},
        {"economyBalance", economyBalance},
        {"magicBalance", magicBalance},
        {"techBalance", techBalance},
        {"warnings", warningsJson}
    };
}

BalanceScore BalanceScore::FromJson(const nlohmann::json& j) {
    BalanceScore score;
    if (j.contains("overallScore")) score.overallScore = j["overallScore"].get<float>();
    if (j.contains("militaryBalance")) score.militaryBalance = j["militaryBalance"].get<float>();
    if (j.contains("economyBalance")) score.economyBalance = j["economyBalance"].get<float>();
    if (j.contains("magicBalance")) score.magicBalance = j["magicBalance"].get<float>();
    if (j.contains("techBalance")) score.techBalance = j["techBalance"].get<float>();

    if (j.contains("warnings") && j["warnings"].is_array()) {
        for (const auto& w : j["warnings"]) {
            score.warnings.push_back(BalanceWarning::FromJson(w));
        }
    }

    return score;
}

// ============================================================================
// PointAllocation Implementation
// ============================================================================

void PointAllocation::SetTotalPoints(int points) {
    totalPoints = std::max(50, std::min(200, points));
    remainingPoints = totalPoints - GetAllocatedPoints();
}

int PointAllocation::GetCategoryPoints(PointCategory category) const {
    switch (category) {
        case PointCategory::Military: return military;
        case PointCategory::Economy: return economy;
        case PointCategory::Magic: return magic;
        case PointCategory::Technology: return technology;
        default: return 0;
    }
}

bool PointAllocation::SetCategoryPoints(PointCategory category, int points) {
    // Clamp points to valid range
    points = std::max(0, std::min(100, points));

    switch (category) {
        case PointCategory::Military: military = points; break;
        case PointCategory::Economy: economy = points; break;
        case PointCategory::Magic: magic = points; break;
        case PointCategory::Technology: technology = points; break;
        default: return false;
    }

    remainingPoints = totalPoints - GetAllocatedPoints();
    return true;
}

float PointAllocation::GetCategoryPercentage(PointCategory category) const {
    if (totalPoints == 0) return 0.0f;
    return static_cast<float>(GetCategoryPoints(category)) / totalPoints * 100.0f;
}

void PointAllocation::ResetToDefault() {
    military = 25;
    economy = 25;
    magic = 25;
    technology = 25;
    remainingPoints = totalPoints - 100;

    militaryAlloc = MilitaryAllocation();
    economyAlloc = EconomyAllocation();
    magicAlloc = MagicAllocation();
    techAlloc = TechnologyAllocation();

    ComputeAllBonuses();
}

bool PointAllocation::Validate() const {
    // Check total allocation
    if (GetAllocatedPoints() != totalPoints) {
        return false;
    }

    // Check individual allocations
    if (military < 0 || economy < 0 || magic < 0 || technology < 0) {
        return false;
    }

    // Check sub-allocations
    if (!militaryAlloc.IsValid() || !economyAlloc.IsValid() ||
        !magicAlloc.IsValid() || !techAlloc.IsValid()) {
        return false;
    }

    return true;
}

std::string PointAllocation::GetValidationError() const {
    int allocated = GetAllocatedPoints();
    if (allocated != totalPoints) {
        std::ostringstream oss;
        oss << "Point mismatch: allocated " << allocated << " of " << totalPoints << " points";
        return oss.str();
    }

    if (military < 0) return "Military points cannot be negative";
    if (economy < 0) return "Economy points cannot be negative";
    if (magic < 0) return "Magic points cannot be negative";
    if (technology < 0) return "Technology points cannot be negative";

    if (!militaryAlloc.IsValid()) return "Military sub-allocation must sum to 100";
    if (!economyAlloc.IsValid()) return "Economy sub-allocation must sum to 100";
    if (!magicAlloc.IsValid()) return "Magic sub-allocation must sum to 100";
    if (!techAlloc.IsValid()) return "Technology sub-allocation must sum to 100";

    return "";
}

bool PointAllocation::ValidateCategory(PointCategory category) const {
    switch (category) {
        case PointCategory::Military: return militaryAlloc.IsValid();
        case PointCategory::Economy: return economyAlloc.IsValid();
        case PointCategory::Magic: return magicAlloc.IsValid();
        case PointCategory::Technology: return techAlloc.IsValid();
        default: return false;
    }
}

void PointAllocation::ComputeAllBonuses() {
    militaryAlloc.ComputeBonuses();
    economyAlloc.ComputeBonuses();
    magicAlloc.ComputeBonuses();
    techAlloc.ComputeBonuses();
}

float PointAllocation::GetBonus(const std::string& bonusName) const {
    auto allBonuses = GetAllBonuses();
    auto it = allBonuses.find(bonusName);
    return (it != allBonuses.end()) ? it->second : 0.0f;
}

std::map<std::string, float> PointAllocation::GetAllBonuses() const {
    std::map<std::string, float> bonuses;

    // Military bonuses
    bonuses["infantryDamageBonus"] = militaryAlloc.infantryDamageBonus;
    bonuses["infantryArmorBonus"] = militaryAlloc.infantryArmorBonus;
    bonuses["rangedDamageBonus"] = militaryAlloc.rangedDamageBonus;
    bonuses["rangedRangeBonus"] = militaryAlloc.rangedRangeBonus;
    bonuses["cavalrySpeedBonus"] = militaryAlloc.cavalrySpeedBonus;
    bonuses["cavalryChargeBonus"] = militaryAlloc.cavalryChargeBonus;
    bonuses["siegeDamageBonus"] = militaryAlloc.siegeDamageBonus;
    bonuses["siegeRangeBonus"] = militaryAlloc.siegeRangeBonus;

    // Economy bonuses
    bonuses["harvestSpeedBonus"] = economyAlloc.harvestSpeedBonus;
    bonuses["buildSpeedBonus"] = economyAlloc.buildSpeedBonus;
    bonuses["carryCapacityBonus"] = economyAlloc.carryCapacityBonus;
    bonuses["tradeProfitBonus"] = economyAlloc.tradeProfitBonus;
    bonuses["workerCostReduction"] = economyAlloc.workerCostReduction;
    bonuses["storageBonus"] = economyAlloc.storageBonus;

    // Magic bonuses
    bonuses["spellDamageBonus"] = magicAlloc.spellDamageBonus;
    bonuses["spellRangeBonus"] = magicAlloc.spellRangeBonus;
    bonuses["manaCostReduction"] = magicAlloc.manaCostReduction;
    bonuses["cooldownReductionBonus"] = magicAlloc.cooldownReductionBonus;
    bonuses["manaRegenBonus"] = magicAlloc.manaRegenBonus;
    bonuses["maxManaBonus"] = magicAlloc.maxManaBonus;

    // Technology bonuses
    bonuses["researchSpeedBonus"] = techAlloc.researchSpeedBonus;
    bonuses["ageUpCostReduction"] = techAlloc.ageUpCostReduction;
    bonuses["uniqueTechBonus"] = techAlloc.uniqueTechBonus;
    bonuses["techProtectionBonus"] = techAlloc.techProtectionBonus;

    return bonuses;
}

BalanceScore PointAllocation::CalculateBalanceScore() const {
    BalanceScore score;

    // Calculate relative balance (1.0 = average)
    float avgCategory = static_cast<float>(totalPoints) / 4.0f;

    score.militaryBalance = military / avgCategory;
    score.economyBalance = economy / avgCategory;
    score.magicBalance = magic / avgCategory;
    score.techBalance = technology / avgCategory;

    // Calculate deviation from balanced
    float maxDeviation = 0.0f;
    float totalDeviation = 0.0f;

    auto checkCategory = [&](float balance, const std::string& name) {
        float deviation = std::abs(balance - 1.0f);
        totalDeviation += deviation;
        maxDeviation = std::max(maxDeviation, deviation);

        if (deviation > 0.5f) {
            BalanceWarning warning;
            warning.severity = BalanceWarningType::Critical;
            warning.category = name;
            warning.message = name + " is severely " +
                (balance > 1.0f ? "over-allocated" : "under-allocated");
            warning.deviation = deviation;
            score.warnings.push_back(warning);
        } else if (deviation > 0.3f) {
            BalanceWarning warning;
            warning.severity = BalanceWarningType::MajorImbalance;
            warning.category = name;
            warning.message = name + " allocation is significantly imbalanced";
            warning.deviation = deviation;
            score.warnings.push_back(warning);
        } else if (deviation > 0.15f) {
            BalanceWarning warning;
            warning.severity = BalanceWarningType::MinorImbalance;
            warning.category = name;
            warning.message = name + " has minor imbalance";
            warning.deviation = deviation;
            score.warnings.push_back(warning);
        }
    };

    checkCategory(score.militaryBalance, "Military");
    checkCategory(score.economyBalance, "Economy");
    checkCategory(score.magicBalance, "Magic");
    checkCategory(score.techBalance, "Technology");

    // Overall score: 100 = perfectly balanced, 0 = maximally imbalanced
    score.overallScore = std::max(0.0f, 100.0f - (totalDeviation * 50.0f));

    return score;
}

std::map<PointCategory, int> PointAllocation::GetRecommendedAdjustments() const {
    std::map<PointCategory, int> adjustments;

    float avgCategory = static_cast<float>(totalPoints) / 4.0f;
    int avgInt = static_cast<int>(avgCategory);

    adjustments[PointCategory::Military] = avgInt - military;
    adjustments[PointCategory::Economy] = avgInt - economy;
    adjustments[PointCategory::Magic] = avgInt - magic;
    adjustments[PointCategory::Technology] = avgInt - technology;

    return adjustments;
}

void PointAllocation::AutoBalance(PointCategory preserveCategory) {
    int preserved = GetCategoryPoints(preserveCategory);
    int remaining = totalPoints - preserved;
    int perCategory = remaining / 3;
    int remainder = remaining % 3;

    std::vector<PointCategory> otherCategories;
    for (int i = 0; i < static_cast<int>(PointCategory::COUNT); ++i) {
        auto cat = static_cast<PointCategory>(i);
        if (cat != preserveCategory) {
            otherCategories.push_back(cat);
        }
    }

    for (size_t i = 0; i < otherCategories.size(); ++i) {
        int points = perCategory + (i < static_cast<size_t>(remainder) ? 1 : 0);
        SetCategoryPoints(otherCategories[i], points);
    }

    ComputeAllBonuses();
}

void PointAllocation::ApplyPreset(const std::string& presetName) {
    if (presetName == "balanced") {
        *this = CreateBalancedPreset();
    } else if (presetName == "military") {
        *this = CreateMilitaryPreset();
    } else if (presetName == "economy") {
        *this = CreateEconomyPreset();
    } else if (presetName == "magic") {
        *this = CreateMagicPreset();
    } else if (presetName == "technology") {
        *this = CreateTechPreset();
    } else if (presetName == "rush") {
        *this = CreateRushPreset();
    } else if (presetName == "turtle") {
        *this = CreateTurtlePreset();
    }
}

std::vector<std::string> PointAllocation::GetAvailablePresets() {
    return {
        "balanced",
        "military",
        "economy",
        "magic",
        "technology",
        "rush",
        "turtle"
    };
}

nlohmann::json PointAllocation::ToJson() const {
    return {
        {"totalPoints", totalPoints},
        {"remainingPoints", remainingPoints},
        {"military", military},
        {"economy", economy},
        {"magic", magic},
        {"technology", technology},
        {"militaryAlloc", militaryAlloc.ToJson()},
        {"economyAlloc", economyAlloc.ToJson()},
        {"magicAlloc", magicAlloc.ToJson()},
        {"techAlloc", techAlloc.ToJson()}
    };
}

PointAllocation PointAllocation::FromJson(const nlohmann::json& j) {
    PointAllocation alloc;

    if (j.contains("totalPoints")) alloc.totalPoints = j["totalPoints"].get<int>();
    if (j.contains("remainingPoints")) alloc.remainingPoints = j["remainingPoints"].get<int>();
    if (j.contains("military")) alloc.military = j["military"].get<int>();
    if (j.contains("economy")) alloc.economy = j["economy"].get<int>();
    if (j.contains("magic")) alloc.magic = j["magic"].get<int>();
    if (j.contains("technology")) alloc.technology = j["technology"].get<int>();

    if (j.contains("militaryAlloc")) alloc.militaryAlloc = MilitaryAllocation::FromJson(j["militaryAlloc"]);
    if (j.contains("economyAlloc")) alloc.economyAlloc = EconomyAllocation::FromJson(j["economyAlloc"]);
    if (j.contains("magicAlloc")) alloc.magicAlloc = MagicAllocation::FromJson(j["magicAlloc"]);
    if (j.contains("techAlloc")) alloc.techAlloc = TechnologyAllocation::FromJson(j["techAlloc"]);

    alloc.ComputeAllBonuses();
    return alloc;
}

bool PointAllocation::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJson().dump(2);
    return true;
}

bool PointAllocation::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;
        *this = FromJson(j);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Preset Implementations
// ============================================================================

PointAllocation CreateBalancedPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 25;
    alloc.economy = 25;
    alloc.magic = 25;
    alloc.technology = 25;
    alloc.remainingPoints = 0;
    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateMilitaryPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 40;
    alloc.economy = 25;
    alloc.magic = 15;
    alloc.technology = 20;
    alloc.remainingPoints = 0;

    // Adjust military sub-allocation for aggression
    alloc.militaryAlloc.infantry = 30;
    alloc.militaryAlloc.ranged = 25;
    alloc.militaryAlloc.cavalry = 30;
    alloc.militaryAlloc.siege = 15;

    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateEconomyPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 20;
    alloc.economy = 40;
    alloc.magic = 15;
    alloc.technology = 25;
    alloc.remainingPoints = 0;

    // Adjust economy sub-allocation for max gathering
    alloc.economyAlloc.harvestSpeed = 40;
    alloc.economyAlloc.buildSpeed = 25;
    alloc.economyAlloc.carryCapacity = 20;
    alloc.economyAlloc.tradeProfits = 15;

    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateMagicPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 15;
    alloc.economy = 20;
    alloc.magic = 45;
    alloc.technology = 20;
    alloc.remainingPoints = 0;

    // Adjust magic sub-allocation for damage
    alloc.magicAlloc.spellDamage = 40;
    alloc.magicAlloc.spellRange = 25;
    alloc.magicAlloc.manaCost = 20;
    alloc.magicAlloc.cooldownReduction = 15;

    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateTechPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 20;
    alloc.economy = 25;
    alloc.magic = 15;
    alloc.technology = 40;
    alloc.remainingPoints = 0;

    // Adjust tech sub-allocation for speed
    alloc.techAlloc.researchSpeed = 45;
    alloc.techAlloc.ageUpCost = 30;
    alloc.techAlloc.uniqueTechs = 25;

    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateRushPreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 45;
    alloc.economy = 30;
    alloc.magic = 10;
    alloc.technology = 15;
    alloc.remainingPoints = 0;

    // Rush: fast units, quick build
    alloc.militaryAlloc.infantry = 35;
    alloc.militaryAlloc.cavalry = 40;
    alloc.militaryAlloc.ranged = 15;
    alloc.militaryAlloc.siege = 10;

    alloc.economyAlloc.harvestSpeed = 40;
    alloc.economyAlloc.buildSpeed = 35;
    alloc.economyAlloc.carryCapacity = 15;
    alloc.economyAlloc.tradeProfits = 10;

    alloc.ComputeAllBonuses();
    return alloc;
}

PointAllocation CreateTurtlePreset() {
    PointAllocation alloc;
    alloc.totalPoints = 100;
    alloc.military = 25;
    alloc.economy = 35;
    alloc.magic = 20;
    alloc.technology = 20;
    alloc.remainingPoints = 0;

    // Turtle: defensive focus
    alloc.militaryAlloc.infantry = 35;  // Strong defense
    alloc.militaryAlloc.ranged = 35;    // Defensive ranged
    alloc.militaryAlloc.cavalry = 10;
    alloc.militaryAlloc.siege = 20;

    alloc.ComputeAllBonuses();
    return alloc;
}

// ============================================================================
// BalanceCalculator Implementation
// ============================================================================

BalanceCalculator& BalanceCalculator::Instance() {
    static BalanceCalculator instance;
    return instance;
}

float BalanceCalculator::CalculatePowerLevel(const PointAllocation& allocation) const {
    // Weighted power calculation
    float power = 0.0f;

    power += allocation.military * m_militaryWeight * 1.2f;  // Military is slightly more impactful
    power += allocation.economy * m_economyWeight * 1.1f;    // Economy scales well
    power += allocation.magic * m_magicWeight * 1.0f;        // Magic is balanced
    power += allocation.technology * m_techWeight * 0.9f;    // Tech is long-term

    // Normalize to 100 = balanced
    return power / (allocation.totalPoints * 1.05f) * 100.0f;
}

float BalanceCalculator::CompareAllocations(const PointAllocation& a,
                                             const PointAllocation& b) const {
    float powerA = CalculatePowerLevel(a);
    float powerB = CalculatePowerLevel(b);
    return powerA - powerB;
}

float BalanceCalculator::GetWinProbability(const PointAllocation& allocation) const {
    float power = CalculatePowerLevel(allocation);

    // Sigmoid function centered at 100 (balanced)
    float exponent = (power - 100.0f) / 20.0f;
    return 1.0f / (1.0f + std::exp(-exponent));
}

void BalanceCalculator::SetBalanceWeights(float militaryWeight, float economyWeight,
                                          float magicWeight, float techWeight) {
    m_militaryWeight = militaryWeight;
    m_economyWeight = economyWeight;
    m_magicWeight = magicWeight;
    m_techWeight = techWeight;
}

float BalanceCalculator::GetEffectiveBonus(const PointAllocation& allocation,
                                            const std::string& statType) const {
    auto bonuses = allocation.GetAllBonuses();

    // Map stat types to bonuses
    if (statType == "damage") {
        return (bonuses["infantryDamageBonus"] + bonuses["rangedDamageBonus"] +
                bonuses["spellDamageBonus"]) / 3.0f;
    } else if (statType == "defense") {
        return bonuses["infantryArmorBonus"];
    } else if (statType == "speed") {
        return (bonuses["cavalrySpeedBonus"] + bonuses["buildSpeedBonus"]) / 2.0f;
    } else if (statType == "economy") {
        return (bonuses["harvestSpeedBonus"] + bonuses["tradeProfitBonus"]) / 2.0f;
    }

    return allocation.GetBonus(statType);
}

} // namespace Race
} // namespace RTS
} // namespace Vehement
