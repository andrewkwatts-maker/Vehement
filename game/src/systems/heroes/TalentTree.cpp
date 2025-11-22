#include "TalentTree.hpp"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace {

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";

    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";

    size_t start = pos + 1;
    size_t end = json.find('"', start);
    if (end == std::string::npos) return "";

    return json.substr(start, end - start);
}

float ExtractJsonFloat(const std::string& json, const std::string& key, float defaultValue = 0.0f) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultValue;

    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    std::string numStr;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-')) {
        numStr += json[pos++];
    }

    try {
        return std::stof(numStr);
    } catch (...) {
        return defaultValue;
    }
}

int ExtractJsonInt(const std::string& json, const std::string& key, int defaultValue = 0) {
    return static_cast<int>(ExtractJsonFloat(json, key, static_cast<float>(defaultValue)));
}

bool ExtractJsonBool(const std::string& json, const std::string& key, bool defaultValue = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return defaultValue;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultValue;

    if (json.find("true", pos) < json.find(',', pos)) return true;
    if (json.find("false", pos) < json.find(',', pos)) return false;

    return defaultValue;
}

} // anonymous namespace

namespace Vehement {
namespace Heroes {

// ============================================================================
// TalentDefinition Implementation
// ============================================================================

TalentDefinition::TalentDefinition() = default;
TalentDefinition::~TalentDefinition() = default;

bool TalentDefinition::LoadFromJson(const std::string& json) {
    m_id = ExtractJsonString(json, "id");
    m_name = ExtractJsonString(json, "name");
    m_description = ExtractJsonString(json, "description");
    m_iconPath = ExtractJsonString(json, "icon");

    m_tier = ExtractJsonInt(json, "tier", 0);
    m_choice = ExtractJsonInt(json, "choice", 0);
    m_requiredLevel = ExtractJsonInt(json, "required_level", TalentTree::UNLOCK_LEVELS[m_tier]);

    m_modifiedAbilityId = ExtractJsonString(json, "modifies_ability");
    m_onSelectScript = ExtractJsonString(json, "on_select_script");

    // Parse bonuses would go here
    // For simplicity, using a single bonus
    TalentBonus bonus;
    std::string bonusType = ExtractJsonString(json, "bonus_type");
    bonus.type = StringToTalentBonusType(bonusType);
    bonus.value = ExtractJsonFloat(json, "bonus_value", 0.0f);
    bonus.isPercentage = ExtractJsonBool(json, "is_percentage", false);
    bonus.targetAbilityId = m_modifiedAbilityId;

    if (bonus.value != 0.0f) {
        m_bonuses.push_back(bonus);
    }

    return !m_id.empty();
}

std::string TalentDefinition::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << m_id << "\",\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"description\": \"" << m_description << "\",\n";
    ss << "  \"tier\": " << m_tier << ",\n";
    ss << "  \"choice\": " << m_choice << ",\n";
    ss << "  \"required_level\": " << m_requiredLevel << "\n";
    ss << "}";
    return ss.str();
}

std::string TalentDefinition::GetFormattedDescription() const {
    std::string desc = m_description;

    // Replace placeholders with actual values
    for (const auto& bonus : m_bonuses) {
        std::string placeholder = "{value}";
        size_t pos = desc.find(placeholder);
        if (pos != std::string::npos) {
            std::stringstream ss;
            ss << bonus.value;
            if (bonus.isPercentage) ss << "%";
            desc.replace(pos, placeholder.length(), ss.str());
        }
    }

    return desc;
}

// ============================================================================
// TalentTree Implementation
// ============================================================================

TalentTree::TalentTree() {
    m_selections.fill(-1);
}

TalentTree::TalentTree(const std::string& heroId)
    : m_heroId(heroId)
{
    m_selections.fill(-1);
}

TalentTree::~TalentTree() = default;

bool TalentTree::LoadFromJson(const std::string& json) {
    // Would parse talent tree from JSON
    return true;
}

bool TalentTree::LoadForHero(const std::string& heroId) {
    m_heroId = heroId;

    auto talents = TalentRegistry::Instance().GetForHero(heroId);
    for (const auto& talent : talents) {
        int tier = talent->GetTier();
        int choice = talent->GetChoice();
        if (tier >= 0 && tier < TIER_COUNT && choice >= 0 && choice < CHOICES_PER_TIER) {
            m_talents[tier][choice] = talent;
        }
    }

    return true;
}

std::string TalentTree::ToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"hero_id\": \"" << m_heroId << "\",\n";
    ss << "  \"selections\": [";
    for (int i = 0; i < TIER_COUNT; i++) {
        ss << m_selections[i];
        if (i < TIER_COUNT - 1) ss << ", ";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

const TalentDefinition* TalentTree::GetTalent(int tier, int choice) const {
    if (tier < 0 || tier >= TIER_COUNT) return nullptr;
    if (choice < 0 || choice >= CHOICES_PER_TIER) return nullptr;
    return m_talents[tier][choice].get();
}

TalentDefinition* TalentTree::GetTalent(int tier, int choice) {
    if (tier < 0 || tier >= TIER_COUNT) return nullptr;
    if (choice < 0 || choice >= CHOICES_PER_TIER) return nullptr;
    return m_talents[tier][choice].get();
}

void TalentTree::SetTalent(int tier, int choice, std::shared_ptr<TalentDefinition> talent) {
    if (tier < 0 || tier >= TIER_COUNT) return;
    if (choice < 0 || choice >= CHOICES_PER_TIER) return;
    m_talents[tier][choice] = std::move(talent);
}

std::array<std::shared_ptr<TalentDefinition>, TalentTree::CHOICES_PER_TIER> TalentTree::GetTierTalents(int tier) const {
    if (tier < 0 || tier >= TIER_COUNT) {
        return {};
    }
    return m_talents[tier];
}

int TalentTree::GetSelection(int tier) const {
    if (tier < 0 || tier >= TIER_COUNT) return -1;
    return m_selections[tier];
}

bool TalentTree::HasSelection(int tier) const {
    return GetSelection(tier) >= 0;
}

const TalentDefinition* TalentTree::GetSelectedTalent(int tier) const {
    int choice = GetSelection(tier);
    if (choice < 0) return nullptr;
    return GetTalent(tier, choice);
}

bool TalentTree::IsTierUnlocked(int tier, int heroLevel) const {
    if (tier < 0 || tier >= TIER_COUNT) return false;
    return heroLevel >= UNLOCK_LEVELS[tier];
}

bool TalentTree::CanSelect(int tier, int choice, int heroLevel) const {
    if (tier < 0 || tier >= TIER_COUNT) return false;
    if (choice < 0 || choice >= CHOICES_PER_TIER) return false;

    // Check if tier is unlocked
    if (!IsTierUnlocked(tier, heroLevel)) return false;

    // Check if already selected
    if (HasSelection(tier)) return false;

    // Check if talent exists
    if (!GetTalent(tier, choice)) return false;

    return true;
}

bool TalentTree::Select(int tier, int choice) {
    // Note: Caller should verify CanSelect first with proper hero level
    if (tier < 0 || tier >= TIER_COUNT) return false;
    if (choice < 0 || choice >= CHOICES_PER_TIER) return false;
    if (HasSelection(tier)) return false;

    auto* talent = GetTalent(tier, choice);
    if (!talent) return false;

    m_selections[tier] = choice;

    if (m_onSelect) {
        m_onSelect(tier, choice, *talent);
    }

    return true;
}

void TalentTree::ResetSelections() {
    m_selections.fill(-1);
}

float TalentTree::GetTotalBonus(TalentBonusType type) const {
    float total = 0.0f;

    for (int tier = 0; tier < TIER_COUNT; tier++) {
        auto* talent = GetSelectedTalent(tier);
        if (!talent) continue;

        for (const auto& bonus : talent->GetBonuses()) {
            if (bonus.type == type && bonus.targetAbilityId.empty()) {
                total += bonus.value;
            }
        }
    }

    return total;
}

float TalentTree::GetAbilityBonus(const std::string& abilityId, TalentBonusType type) const {
    float total = 0.0f;

    for (int tier = 0; tier < TIER_COUNT; tier++) {
        auto* talent = GetSelectedTalent(tier);
        if (!talent) continue;

        for (const auto& bonus : talent->GetBonuses()) {
            if (bonus.type == type && bonus.targetAbilityId == abilityId) {
                total += bonus.value;
            }
        }
    }

    return total;
}

std::vector<TalentBonus> TalentTree::GetAllSelectedBonuses() const {
    std::vector<TalentBonus> bonuses;

    for (int tier = 0; tier < TIER_COUNT; tier++) {
        auto* talent = GetSelectedTalent(tier);
        if (!talent) continue;

        for (const auto& bonus : talent->GetBonuses()) {
            bonuses.push_back(bonus);
        }
    }

    return bonuses;
}

bool TalentTree::ModifiesAbility(const std::string& abilityId) const {
    for (int tier = 0; tier < TIER_COUNT; tier++) {
        auto* talent = GetSelectedTalent(tier);
        if (talent && talent->GetModifiedAbilityId() == abilityId) {
            return true;
        }
    }
    return false;
}

int TalentTree::GetUnlockLevel(int tier) {
    if (tier < 0 || tier >= TIER_COUNT) return 99;
    return UNLOCK_LEVELS[tier];
}

int TalentTree::GetTierForLevel(int level) {
    for (int i = TIER_COUNT - 1; i >= 0; i--) {
        if (level >= UNLOCK_LEVELS[i]) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// TalentRegistry Implementation
// ============================================================================

TalentRegistry& TalentRegistry::Instance() {
    static TalentRegistry instance;
    return instance;
}

int TalentRegistry::LoadFromDirectory(const std::string& configPath) {
    int count = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(configPath)) {
            if (entry.path().extension() == ".json") {
                std::ifstream file(entry.path());
                if (!file.is_open()) continue;

                std::stringstream buffer;
                buffer << file.rdbuf();

                auto talent = std::make_shared<TalentDefinition>();
                if (talent->LoadFromJson(buffer.str())) {
                    Register(talent);
                    count++;
                }
            }
        }
    } catch (...) {}

    return count;
}

void TalentRegistry::Register(std::shared_ptr<TalentDefinition> talent) {
    if (!talent) return;

    m_talents[talent->GetId()] = talent;

    // Extract hero ID from talent ID (format: talent_heroId_name)
    std::string id = talent->GetId();
    size_t firstUnderscore = id.find('_');
    if (firstUnderscore != std::string::npos) {
        size_t secondUnderscore = id.find('_', firstUnderscore + 1);
        if (secondUnderscore != std::string::npos) {
            std::string heroId = id.substr(firstUnderscore + 1, secondUnderscore - firstUnderscore - 1);
            m_heroTalents[heroId].push_back(talent->GetId());
        }
    }
}

std::shared_ptr<TalentDefinition> TalentRegistry::Get(const std::string& id) const {
    auto it = m_talents.find(id);
    return (it != m_talents.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<TalentDefinition>> TalentRegistry::GetForHero(const std::string& heroId) const {
    std::vector<std::shared_ptr<TalentDefinition>> result;

    auto it = m_heroTalents.find(heroId);
    if (it != m_heroTalents.end()) {
        for (const auto& talentId : it->second) {
            auto talent = Get(talentId);
            if (talent) {
                result.push_back(talent);
            }
        }
    }

    return result;
}

bool TalentRegistry::Exists(const std::string& id) const {
    return m_talents.find(id) != m_talents.end();
}

void TalentRegistry::Clear() {
    m_talents.clear();
    m_heroTalents.clear();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string TalentBonusTypeToString(TalentBonusType type) {
    switch (type) {
        case TalentBonusType::BonusStrength: return "bonus_strength";
        case TalentBonusType::BonusAgility: return "bonus_agility";
        case TalentBonusType::BonusIntelligence: return "bonus_intelligence";
        case TalentBonusType::BonusHealth: return "bonus_health";
        case TalentBonusType::BonusMana: return "bonus_mana";
        case TalentBonusType::BonusDamage: return "bonus_damage";
        case TalentBonusType::BonusArmor: return "bonus_armor";
        case TalentBonusType::BonusMoveSpeed: return "bonus_move_speed";
        case TalentBonusType::BonusAttackSpeed: return "bonus_attack_speed";
        case TalentBonusType::BonusHealthRegen: return "bonus_health_regen";
        case TalentBonusType::BonusManaRegen: return "bonus_mana_regen";
        case TalentBonusType::BonusCastRange: return "bonus_cast_range";
        case TalentBonusType::BonusAttackRange: return "bonus_attack_range";
        case TalentBonusType::AbilityDamage: return "ability_damage";
        case TalentBonusType::AbilityCooldown: return "ability_cooldown";
        case TalentBonusType::AbilityManaCost: return "ability_mana_cost";
        case TalentBonusType::AbilityRange: return "ability_range";
        case TalentBonusType::AbilityDuration: return "ability_duration";
        case TalentBonusType::AbilityRadius: return "ability_radius";
        case TalentBonusType::AbilityCharges: return "ability_charges";
        case TalentBonusType::GoldIncome: return "gold_income";
        case TalentBonusType::XPGain: return "xp_gain";
        case TalentBonusType::CooldownReduction: return "cooldown_reduction";
        case TalentBonusType::SpellAmplification: return "spell_amplification";
        case TalentBonusType::StatusResistance: return "status_resistance";
        case TalentBonusType::Lifesteal: return "lifesteal";
        case TalentBonusType::SpellLifesteal: return "spell_lifesteal";
        case TalentBonusType::Evasion: return "evasion";
        case TalentBonusType::CriticalChance: return "critical_chance";
        case TalentBonusType::CriticalDamage: return "critical_damage";
        case TalentBonusType::AbilitySpecial: return "ability_special";
        case TalentBonusType::AbilityUpgrade: return "ability_upgrade";
        case TalentBonusType::Custom: return "custom";
    }
    return "custom";
}

TalentBonusType StringToTalentBonusType(const std::string& str) {
    if (str == "bonus_strength") return TalentBonusType::BonusStrength;
    if (str == "bonus_agility") return TalentBonusType::BonusAgility;
    if (str == "bonus_intelligence") return TalentBonusType::BonusIntelligence;
    if (str == "bonus_health") return TalentBonusType::BonusHealth;
    if (str == "bonus_mana") return TalentBonusType::BonusMana;
    if (str == "bonus_damage") return TalentBonusType::BonusDamage;
    if (str == "bonus_armor") return TalentBonusType::BonusArmor;
    if (str == "bonus_move_speed") return TalentBonusType::BonusMoveSpeed;
    if (str == "bonus_attack_speed") return TalentBonusType::BonusAttackSpeed;
    if (str == "bonus_health_regen") return TalentBonusType::BonusHealthRegen;
    if (str == "bonus_mana_regen") return TalentBonusType::BonusManaRegen;
    if (str == "bonus_cast_range") return TalentBonusType::BonusCastRange;
    if (str == "bonus_attack_range") return TalentBonusType::BonusAttackRange;
    if (str == "ability_damage") return TalentBonusType::AbilityDamage;
    if (str == "ability_cooldown") return TalentBonusType::AbilityCooldown;
    if (str == "ability_mana_cost") return TalentBonusType::AbilityManaCost;
    if (str == "ability_range") return TalentBonusType::AbilityRange;
    if (str == "ability_duration") return TalentBonusType::AbilityDuration;
    if (str == "ability_radius") return TalentBonusType::AbilityRadius;
    if (str == "ability_charges") return TalentBonusType::AbilityCharges;
    if (str == "gold_income") return TalentBonusType::GoldIncome;
    if (str == "xp_gain") return TalentBonusType::XPGain;
    if (str == "cooldown_reduction") return TalentBonusType::CooldownReduction;
    if (str == "spell_amplification") return TalentBonusType::SpellAmplification;
    if (str == "status_resistance") return TalentBonusType::StatusResistance;
    if (str == "lifesteal") return TalentBonusType::Lifesteal;
    if (str == "spell_lifesteal") return TalentBonusType::SpellLifesteal;
    if (str == "evasion") return TalentBonusType::Evasion;
    if (str == "critical_chance") return TalentBonusType::CriticalChance;
    if (str == "critical_damage") return TalentBonusType::CriticalDamage;
    if (str == "ability_special") return TalentBonusType::AbilitySpecial;
    if (str == "ability_upgrade") return TalentBonusType::AbilityUpgrade;
    return TalentBonusType::Custom;
}

} // namespace Heroes
} // namespace Vehement
