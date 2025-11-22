#include "HeroDefinition.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

// Simple JSON parsing (in production, use nlohmann/json or similar)
namespace {

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool WriteFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

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

std::string ExtractJsonObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos = json.find('{', pos);
    if (pos == std::string::npos) return "";

    int braceCount = 1;
    size_t start = pos;
    pos++;

    while (pos < json.size() && braceCount > 0) {
        if (json[pos] == '{') braceCount++;
        else if (json[pos] == '}') braceCount--;
        pos++;
    }

    return json.substr(start, pos - start);
}

std::string ExtractJsonArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos = json.find('[', pos);
    if (pos == std::string::npos) return "";

    int bracketCount = 1;
    size_t start = pos;
    pos++;

    while (pos < json.size() && bracketCount > 0) {
        if (json[pos] == '[') bracketCount++;
        else if (json[pos] == ']') bracketCount--;
        pos++;
    }

    return json.substr(start, pos - start);
}

std::vector<std::string> SplitJsonArrayElements(const std::string& arrayContent) {
    std::vector<std::string> elements;
    if (arrayContent.empty() || arrayContent[0] != '[') return elements;

    int braceCount = 0;
    int bracketCount = 0;
    size_t start = 1;

    for (size_t i = 1; i < arrayContent.size() - 1; i++) {
        char c = arrayContent[i];
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
        else if (c == '[') bracketCount++;
        else if (c == ']') bracketCount--;
        else if (c == ',' && braceCount == 0 && bracketCount == 0) {
            std::string element = arrayContent.substr(start, i - start);
            // Trim whitespace
            size_t first = element.find_first_not_of(" \t\n\r");
            size_t last = element.find_last_not_of(" \t\n\r");
            if (first != std::string::npos) {
                elements.push_back(element.substr(first, last - first + 1));
            }
            start = i + 1;
        }
    }

    // Last element
    std::string element = arrayContent.substr(start, arrayContent.size() - 1 - start);
    size_t first = element.find_first_not_of(" \t\n\r");
    size_t last = element.find_last_not_of(" \t\n\r");
    if (first != std::string::npos) {
        elements.push_back(element.substr(first, last - first + 1));
    }

    return elements;
}

} // anonymous namespace

namespace Vehement {
namespace Heroes {

// ============================================================================
// HeroDefinition Implementation
// ============================================================================

HeroDefinition::HeroDefinition() {
    // Initialize default talent tiers
    m_talentTiers[0] = {1, 10, {"", ""}};
    m_talentTiers[1] = {2, 15, {"", ""}};
    m_talentTiers[2] = {3, 20, {"", ""}};
    m_talentTiers[3] = {4, 25, {"", ""}};
}

HeroDefinition::~HeroDefinition() {
    if (m_isActive) {
        Destroy();
    }
}

bool HeroDefinition::LoadFromFile(const std::string& filePath) {
    std::string content = ReadFile(filePath);
    if (content.empty()) {
        return false;
    }

    m_sourcePath = filePath;

    // Get file modification time
    try {
        auto ftime = std::filesystem::last_write_time(filePath);
        m_lastModified = ftime.time_since_epoch().count();
    } catch (...) {
        m_lastModified = 0;
    }

    return LoadFromString(content);
}

bool HeroDefinition::LoadFromString(const std::string& jsonString) {
    try {
        ParseJson(jsonString);
        return true;
    } catch (...) {
        return false;
    }
}

void HeroDefinition::ParseJson(const std::string& json) {
    // Parse identity
    m_id = ExtractJsonString(json, "id");
    m_name = ExtractJsonString(json, "name");
    m_title = ExtractJsonString(json, "title");
    m_description = ExtractJsonString(json, "description");
    m_lore = ExtractJsonString(json, "lore");

    // Parse class type
    m_classTypeName = ExtractJsonString(json, "class");
    m_classType = StringToHeroClassType(m_classTypeName);

    // Parse primary attribute
    std::string attrStr = ExtractJsonString(json, "primary_attribute");
    m_primaryAttribute = StringToPrimaryAttribute(attrStr);

    // Parse base stats
    ParseBaseStats(json);

    // Parse stat growth
    ParseStatGrowth(json);

    // Parse abilities
    ParseAbilities(json);

    // Parse talents
    ParseTalents(json);

    // Parse visuals
    ParseVisuals(json);

    // Parse events
    ParseEvents(json);
}

void HeroDefinition::ParseBaseStats(const std::string& json) {
    std::string statsObj = ExtractJsonObject(json, "base_stats");
    if (statsObj.empty()) return;

    m_baseStats.health = ExtractJsonFloat(statsObj, "health", 200.0f);
    m_baseStats.mana = ExtractJsonFloat(statsObj, "mana", 75.0f);
    m_baseStats.damage = ExtractJsonFloat(statsObj, "damage", 25.0f);
    m_baseStats.armor = ExtractJsonFloat(statsObj, "armor", 3.0f);
    m_baseStats.magicResist = ExtractJsonFloat(statsObj, "magic_resist", 0.0f);
    m_baseStats.moveSpeed = ExtractJsonFloat(statsObj, "move_speed", 300.0f);
    m_baseStats.turnRate = ExtractJsonFloat(statsObj, "turn_rate", 0.6f);
    m_baseStats.attackRange = ExtractJsonFloat(statsObj, "attack_range", 1.5f);
    m_baseStats.attackSpeed = ExtractJsonFloat(statsObj, "attack_speed", 1.0f);
    m_baseStats.healthRegen = ExtractJsonFloat(statsObj, "health_regen", 1.0f);
    m_baseStats.manaRegen = ExtractJsonFloat(statsObj, "mana_regen", 0.5f);
    m_baseStats.strength = ExtractJsonFloat(statsObj, "strength", 20.0f);
    m_baseStats.agility = ExtractJsonFloat(statsObj, "agility", 15.0f);
    m_baseStats.intelligence = ExtractJsonFloat(statsObj, "intelligence", 15.0f);
    m_baseStats.dayVision = ExtractJsonFloat(statsObj, "day_vision", 18.0f);
    m_baseStats.nightVision = ExtractJsonFloat(statsObj, "night_vision", 8.0f);
}

void HeroDefinition::ParseStatGrowth(const std::string& json) {
    std::string growthObj = ExtractJsonObject(json, "stat_growth");
    if (growthObj.empty()) return;

    m_statGrowth.healthPerLevel = ExtractJsonFloat(growthObj, "health_per_level", 25.0f);
    m_statGrowth.manaPerLevel = ExtractJsonFloat(growthObj, "mana_per_level", 2.0f);
    m_statGrowth.damagePerLevel = ExtractJsonFloat(growthObj, "damage_per_level", 0.0f);
    m_statGrowth.armorPerLevel = ExtractJsonFloat(growthObj, "armor_per_level", 0.0f);
    m_statGrowth.strengthPerLevel = ExtractJsonFloat(growthObj, "strength_per_level", 2.5f);
    m_statGrowth.agilityPerLevel = ExtractJsonFloat(growthObj, "agility_per_level", 1.5f);
    m_statGrowth.intelligencePerLevel = ExtractJsonFloat(growthObj, "intelligence_per_level", 1.5f);
    m_statGrowth.healthRegenPerLevel = ExtractJsonFloat(growthObj, "health_regen_per_level", 0.1f);
    m_statGrowth.manaRegenPerLevel = ExtractJsonFloat(growthObj, "mana_regen_per_level", 0.05f);
}

void HeroDefinition::ParseAbilities(const std::string& json) {
    std::string abilitiesArr = ExtractJsonArray(json, "abilities");
    if (abilitiesArr.empty()) return;

    auto elements = SplitJsonArrayElements(abilitiesArr);
    m_abilities.clear();

    for (const auto& elem : elements) {
        AbilitySlotBinding binding;
        binding.slot = ExtractJsonInt(elem, "slot", 1);
        binding.abilityId = ExtractJsonString(elem, "id");
        binding.isUltimate = ExtractJsonBool(elem, "ultimate", false);
        binding.unlockLevel = ExtractJsonInt(elem, "unlock_level", 1);
        binding.maxLevel = ExtractJsonInt(elem, "max_level", 4);
        m_abilities.push_back(binding);
    }
}

void HeroDefinition::ParseTalents(const std::string& json) {
    std::string talentsArr = ExtractJsonArray(json, "talents");
    if (talentsArr.empty()) return;

    auto elements = SplitJsonArrayElements(talentsArr);

    for (size_t i = 0; i < elements.size() && i < TALENT_TIER_COUNT; i++) {
        const auto& elem = elements[i];
        m_talentTiers[i].tier = ExtractJsonInt(elem, "tier", static_cast<int>(i + 1));

        // Default unlock levels: 10, 15, 20, 25
        int defaultLevel = 10 + static_cast<int>(i) * 5;
        m_talentTiers[i].unlockLevel = ExtractJsonInt(elem, "unlock_level", defaultLevel);

        // Parse choices array
        std::string choicesArr = ExtractJsonArray(elem, "choices");
        auto choices = SplitJsonArrayElements(choicesArr);

        for (size_t j = 0; j < choices.size() && j < 2; j++) {
            // Remove quotes from string
            std::string choice = choices[j];
            if (choice.front() == '"') choice = choice.substr(1);
            if (choice.back() == '"') choice = choice.substr(0, choice.size() - 1);
            m_talentTiers[i].choices[j] = choice;
        }
    }
}

void HeroDefinition::ParseVisuals(const std::string& json) {
    std::string visualsObj = ExtractJsonObject(json, "visuals");
    if (visualsObj.empty()) return;

    m_visualOptions.modelPath = ExtractJsonString(visualsObj, "model");
    m_visualOptions.skeletonPath = ExtractJsonString(visualsObj, "skeleton");
    m_visualOptions.portraitPath = ExtractJsonString(visualsObj, "portrait");
    m_visualOptions.iconPath = ExtractJsonString(visualsObj, "icon");
    m_visualOptions.minimapIcon = ExtractJsonString(visualsObj, "minimap_icon");
    m_visualOptions.animationSet = ExtractJsonString(visualsObj, "animation_set");
    m_visualOptions.modelScale = ExtractJsonFloat(visualsObj, "scale", 1.0f);

    // Effects
    m_visualOptions.attackEffect = ExtractJsonString(visualsObj, "attack_effect");
    m_visualOptions.castEffect = ExtractJsonString(visualsObj, "cast_effect");
    m_visualOptions.deathEffect = ExtractJsonString(visualsObj, "death_effect");
    m_visualOptions.respawnEffect = ExtractJsonString(visualsObj, "respawn_effect");

    // Audio
    m_visualOptions.attackSound = ExtractJsonString(visualsObj, "attack_sound");
    m_visualOptions.hurtSound = ExtractJsonString(visualsObj, "hurt_sound");
    m_visualOptions.deathSound = ExtractJsonString(visualsObj, "death_sound");
}

void HeroDefinition::ParseEvents(const std::string& json) {
    std::string eventsObj = ExtractJsonObject(json, "events");
    if (eventsObj.empty()) return;

    m_eventBindings.onSpawn = ExtractJsonString(eventsObj, "on_spawn");
    m_eventBindings.onDeath = ExtractJsonString(eventsObj, "on_death");
    m_eventBindings.onRespawn = ExtractJsonString(eventsObj, "on_respawn");
    m_eventBindings.onLevelUp = ExtractJsonString(eventsObj, "on_level_up");
    m_eventBindings.onAbilityLearn = ExtractJsonString(eventsObj, "on_ability_learn");
    m_eventBindings.onKill = ExtractJsonString(eventsObj, "on_kill");
    m_eventBindings.onAssist = ExtractJsonString(eventsObj, "on_assist");
    m_eventBindings.onTalentSelect = ExtractJsonString(eventsObj, "on_talent_select");
    m_eventBindings.onItemEquip = ExtractJsonString(eventsObj, "on_item_equip");
    m_eventBindings.onCreate = ExtractJsonString(eventsObj, "on_create");
    m_eventBindings.onDestroy = ExtractJsonString(eventsObj, "on_destroy");
}

bool HeroDefinition::SaveToFile(const std::string& filePath) const {
    std::string json = ToJsonString();
    return WriteFile(filePath, json);
}

std::string HeroDefinition::ToJsonString() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << m_id << "\",\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"title\": \"" << m_title << "\",\n";
    ss << "  \"class\": \"" << m_classTypeName << "\",\n";
    ss << "  \"primary_attribute\": \"" << PrimaryAttributeToString(m_primaryAttribute) << "\",\n";

    // Base stats
    ss << "  \"base_stats\": {\n";
    ss << "    \"health\": " << m_baseStats.health << ",\n";
    ss << "    \"mana\": " << m_baseStats.mana << ",\n";
    ss << "    \"damage\": " << m_baseStats.damage << ",\n";
    ss << "    \"armor\": " << m_baseStats.armor << ",\n";
    ss << "    \"move_speed\": " << m_baseStats.moveSpeed << "\n";
    ss << "  },\n";

    // Stat growth
    ss << "  \"stat_growth\": {\n";
    ss << "    \"health_per_level\": " << m_statGrowth.healthPerLevel << ",\n";
    ss << "    \"mana_per_level\": " << m_statGrowth.manaPerLevel << ",\n";
    ss << "    \"strength_per_level\": " << m_statGrowth.strengthPerLevel << ",\n";
    ss << "    \"agility_per_level\": " << m_statGrowth.agilityPerLevel << ",\n";
    ss << "    \"intelligence_per_level\": " << m_statGrowth.intelligencePerLevel << "\n";
    ss << "  },\n";

    // Abilities
    ss << "  \"abilities\": [\n";
    for (size_t i = 0; i < m_abilities.size(); i++) {
        const auto& ability = m_abilities[i];
        ss << "    {\"slot\": " << ability.slot << ", \"id\": \"" << ability.abilityId << "\"";
        if (ability.isUltimate) {
            ss << ", \"ultimate\": true, \"unlock_level\": " << ability.unlockLevel;
        }
        ss << "}";
        if (i < m_abilities.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}\n";
    return ss.str();
}

bool HeroDefinition::Validate(std::vector<std::string>& outErrors) const {
    bool valid = true;

    if (m_id.empty()) {
        outErrors.push_back("Hero ID is empty");
        valid = false;
    }

    if (m_name.empty()) {
        outErrors.push_back("Hero name is empty");
        valid = false;
    }

    if (m_baseStats.health <= 0) {
        outErrors.push_back("Base health must be positive");
        valid = false;
    }

    if (m_abilities.empty()) {
        outErrors.push_back("Hero must have at least one ability");
        valid = false;
    }

    // Check ability slots
    for (const auto& ability : m_abilities) {
        if (ability.slot < 1 || ability.slot > ABILITY_SLOT_COUNT) {
            outErrors.push_back("Invalid ability slot: " + std::to_string(ability.slot));
            valid = false;
        }
        if (ability.abilityId.empty()) {
            outErrors.push_back("Ability in slot " + std::to_string(ability.slot) + " has no ID");
            valid = false;
        }
    }

    return valid;
}

void HeroDefinition::Create() {
    if (m_isActive) return;

    m_isActive = true;

    if (m_onCreate) {
        m_onCreate(*this);
    }
}

void HeroDefinition::Tick(float deltaTime) {
    if (!m_isActive) return;

    if (m_onTick) {
        m_onTick(*this, deltaTime);
    }
}

void HeroDefinition::Destroy() {
    if (!m_isActive) return;

    if (m_onDestroy) {
        m_onDestroy(*this);
    }

    m_isActive = false;
}

HeroBaseStats HeroDefinition::CalculateStatsAtLevel(int level) const {
    HeroBaseStats stats = m_baseStats;
    int levelsGained = std::max(0, level - 1);

    stats.health += m_statGrowth.healthPerLevel * levelsGained;
    stats.mana += m_statGrowth.manaPerLevel * levelsGained;
    stats.damage += m_statGrowth.damagePerLevel * levelsGained;
    stats.armor += m_statGrowth.armorPerLevel * levelsGained;

    stats.strength += m_statGrowth.strengthPerLevel * levelsGained;
    stats.agility += m_statGrowth.agilityPerLevel * levelsGained;
    stats.intelligence += m_statGrowth.intelligencePerLevel * levelsGained;

    stats.healthRegen += m_statGrowth.healthRegenPerLevel * levelsGained;
    stats.manaRegen += m_statGrowth.manaRegenPerLevel * levelsGained;

    // Apply attribute bonuses
    // Strength: +20 health per point
    stats.health += stats.strength * 20.0f;
    // Agility: +0.17 armor per point
    stats.armor += stats.agility * 0.17f;
    // Intelligence: +12 mana per point
    stats.mana += stats.intelligence * 12.0f;

    return stats;
}

const AbilitySlotBinding* HeroDefinition::GetAbilitySlot(int slot) const {
    for (const auto& ability : m_abilities) {
        if (ability.slot == slot) {
            return &ability;
        }
    }
    return nullptr;
}

bool HeroDefinition::HasUltimate() const {
    for (const auto& ability : m_abilities) {
        if (ability.isUltimate) return true;
    }
    return false;
}

int HeroDefinition::GetUltimateUnlockLevel() const {
    for (const auto& ability : m_abilities) {
        if (ability.isUltimate) return ability.unlockLevel;
    }
    return 6; // Default
}

const TalentTierConfig& HeroDefinition::GetTalentTier(int tier) const {
    if (tier >= 0 && tier < TALENT_TIER_COUNT) {
        return m_talentTiers[tier];
    }
    static TalentTierConfig empty;
    return empty;
}

int HeroDefinition::GetTalentUnlockLevel(int tier) const {
    if (tier >= 0 && tier < TALENT_TIER_COUNT) {
        return m_talentTiers[tier].unlockLevel;
    }
    return 99;
}

float HeroDefinition::GetPrimaryAttributeBonus(PrimaryAttribute attr) const {
    switch (attr) {
        case PrimaryAttribute::Strength:
            return 20.0f; // +20 health per point
        case PrimaryAttribute::Agility:
            return 0.17f; // +0.17 armor per point
        case PrimaryAttribute::Intelligence:
            return 12.0f; // +12 mana per point
    }
    return 0.0f;
}

bool HeroDefinition::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

std::optional<std::string> HeroDefinition::GetMetadataValue(const std::string& key) const {
    auto it = m_metadata.find(key);
    if (it != m_metadata.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// HeroDefinitionRegistry Implementation
// ============================================================================

HeroDefinitionRegistry& HeroDefinitionRegistry::Instance() {
    static HeroDefinitionRegistry instance;
    return instance;
}

int HeroDefinitionRegistry::LoadFromDirectory(const std::string& configPath) {
    m_configPath = configPath;
    int count = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(configPath)) {
            if (entry.path().extension() == ".json") {
                auto definition = std::make_shared<HeroDefinition>();
                if (definition->LoadFromFile(entry.path().string())) {
                    Register(definition);
                    count++;
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }

    return count;
}

void HeroDefinitionRegistry::Register(std::shared_ptr<HeroDefinition> definition) {
    if (definition) {
        definition->Create();
        m_definitions[definition->GetId()] = definition;
    }
}

std::shared_ptr<HeroDefinition> HeroDefinitionRegistry::Get(const std::string& id) const {
    auto it = m_definitions.find(id);
    if (it != m_definitions.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<HeroDefinition>> HeroDefinitionRegistry::GetAll() const {
    std::vector<std::shared_ptr<HeroDefinition>> result;
    result.reserve(m_definitions.size());
    for (const auto& [id, def] : m_definitions) {
        result.push_back(def);
    }
    return result;
}

std::vector<std::shared_ptr<HeroDefinition>> HeroDefinitionRegistry::GetByClassType(HeroClassType type) const {
    std::vector<std::shared_ptr<HeroDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->GetClassType() == type) {
            result.push_back(def);
        }
    }
    return result;
}

std::vector<std::shared_ptr<HeroDefinition>> HeroDefinitionRegistry::GetByPrimaryAttribute(PrimaryAttribute attr) const {
    std::vector<std::shared_ptr<HeroDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->GetPrimaryAttribute() == attr) {
            result.push_back(def);
        }
    }
    return result;
}

std::vector<std::shared_ptr<HeroDefinition>> HeroDefinitionRegistry::GetByTag(const std::string& tag) const {
    std::vector<std::shared_ptr<HeroDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->HasTag(tag)) {
            result.push_back(def);
        }
    }
    return result;
}

bool HeroDefinitionRegistry::Exists(const std::string& id) const {
    return m_definitions.find(id) != m_definitions.end();
}

void HeroDefinitionRegistry::Clear() {
    for (auto& [id, def] : m_definitions) {
        def->Destroy();
    }
    m_definitions.clear();
}

void HeroDefinitionRegistry::Reload() {
    Clear();
    if (!m_configPath.empty()) {
        LoadFromDirectory(m_configPath);
    }
}

void HeroDefinitionRegistry::Tick(float deltaTime) {
    for (auto& [id, def] : m_definitions) {
        def->Tick(deltaTime);
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string PrimaryAttributeToString(PrimaryAttribute attr) {
    switch (attr) {
        case PrimaryAttribute::Strength: return "strength";
        case PrimaryAttribute::Agility: return "agility";
        case PrimaryAttribute::Intelligence: return "intelligence";
    }
    return "strength";
}

PrimaryAttribute StringToPrimaryAttribute(const std::string& str) {
    if (str == "agility" || str == "agi") return PrimaryAttribute::Agility;
    if (str == "intelligence" || str == "int") return PrimaryAttribute::Intelligence;
    return PrimaryAttribute::Strength;
}

std::string HeroClassTypeToString(HeroClassType type) {
    switch (type) {
        case HeroClassType::Warrior: return "warrior";
        case HeroClassType::Mage: return "mage";
        case HeroClassType::Support: return "support";
        case HeroClassType::Assassin: return "assassin";
        case HeroClassType::Tank: return "tank";
        case HeroClassType::Marksman: return "marksman";
    }
    return "warrior";
}

HeroClassType StringToHeroClassType(const std::string& str) {
    if (str == "mage") return HeroClassType::Mage;
    if (str == "support") return HeroClassType::Support;
    if (str == "assassin") return HeroClassType::Assassin;
    if (str == "tank") return HeroClassType::Tank;
    if (str == "marksman") return HeroClassType::Marksman;
    return HeroClassType::Warrior;
}

} // namespace Heroes
} // namespace Vehement
