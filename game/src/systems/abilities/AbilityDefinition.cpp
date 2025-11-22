#include "AbilityDefinition.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

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
            size_t first = element.find_first_not_of(" \t\n\r");
            size_t last = element.find_last_not_of(" \t\n\r");
            if (first != std::string::npos) {
                elements.push_back(element.substr(first, last - first + 1));
            }
            start = i + 1;
        }
    }

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
namespace Abilities {

// ============================================================================
// AbilityDefinition Implementation
// ============================================================================

AbilityDefinition::AbilityDefinition() {
    // Initialize with one default level
    m_levelData.resize(1);
}

AbilityDefinition::~AbilityDefinition() {
    if (m_isActive) {
        Destroy();
    }
}

bool AbilityDefinition::LoadFromFile(const std::string& filePath) {
    std::string content = ReadFile(filePath);
    if (content.empty()) return false;

    m_sourcePath = filePath;

    try {
        auto ftime = std::filesystem::last_write_time(filePath);
        m_lastModified = ftime.time_since_epoch().count();
    } catch (...) {
        m_lastModified = 0;
    }

    return LoadFromString(content);
}

bool AbilityDefinition::LoadFromString(const std::string& jsonString) {
    try {
        ParseJson(jsonString);
        return true;
    } catch (...) {
        return false;
    }
}

void AbilityDefinition::ParseJson(const std::string& json) {
    // Parse identity
    m_id = ExtractJsonString(json, "id");
    m_name = ExtractJsonString(json, "name");
    m_description = ExtractJsonString(json, "description");
    m_lore = ExtractJsonString(json, "lore");

    // Parse type
    std::string typeStr = ExtractJsonString(json, "type");
    m_type = StringToAbilityType(typeStr);

    // Parse max level
    m_maxLevel = ExtractJsonInt(json, "max_level", 4);

    // Parse targeting
    std::string targetStr = ExtractJsonString(json, "targeting");
    m_targetingType = StringToTargetingType(targetStr);

    // Parse level data
    ParseLevels(json);

    // Parse triggers
    ParseTriggers(json);

    // Parse effects
    ParseEffects(json);

    // Parse scripts
    ParseScripts(json);
}

void AbilityDefinition::ParseLevels(const std::string& json) {
    std::string levelsArr = ExtractJsonArray(json, "levels");
    if (levelsArr.empty()) return;

    auto elements = SplitJsonArrayElements(levelsArr);
    m_levelData.clear();

    for (const auto& elem : elements) {
        AbilityLevelData level;

        level.damage = ExtractJsonFloat(elem, "damage", 0.0f);
        level.healing = ExtractJsonFloat(elem, "healing", 0.0f);
        level.manaCost = ExtractJsonFloat(elem, "mana_cost", 0.0f);
        level.healthCost = ExtractJsonFloat(elem, "health_cost", 0.0f);
        level.cooldown = ExtractJsonFloat(elem, "cooldown", 10.0f);
        level.duration = ExtractJsonFloat(elem, "duration", 0.0f);
        level.castTime = ExtractJsonFloat(elem, "cast_time", 0.0f);
        level.channelTime = ExtractJsonFloat(elem, "channel_time", 0.0f);
        level.castRange = ExtractJsonFloat(elem, "range", 0.0f);
        level.effectRadius = ExtractJsonFloat(elem, "radius", 0.0f);

        // Alternative names for compatibility
        if (level.effectRadius == 0.0f) {
            level.effectRadius = ExtractJsonFloat(elem, "cleave_radius", 0.0f);
        }

        level.width = ExtractJsonFloat(elem, "width", 0.0f);
        level.travelDistance = ExtractJsonFloat(elem, "travel_distance", 0.0f);

        level.slowPercent = ExtractJsonFloat(elem, "slow_percent", 0.0f);
        level.stunDuration = ExtractJsonFloat(elem, "stun_duration", 0.0f);
        level.silenceDuration = ExtractJsonFloat(elem, "silence_duration", 0.0f);
        level.disarmDuration = ExtractJsonFloat(elem, "disarm_duration", 0.0f);

        level.bonusDamage = ExtractJsonFloat(elem, "bonus_damage", 0.0f);
        level.bonusArmor = ExtractJsonFloat(elem, "bonus_armor", 0.0f);
        level.bonusMoveSpeed = ExtractJsonFloat(elem, "bonus_move_speed", 0.0f);
        level.bonusAttackSpeed = ExtractJsonFloat(elem, "bonus_attack_speed", 0.0f);
        level.bonusHealthRegen = ExtractJsonFloat(elem, "bonus_health_regen", 0.0f);
        level.bonusManaRegen = ExtractJsonFloat(elem, "bonus_mana_regen", 0.0f);

        level.specialValue1 = ExtractJsonFloat(elem, "special_value_1", 0.0f);
        level.specialValue2 = ExtractJsonFloat(elem, "special_value_2", 0.0f);
        level.specialValue3 = ExtractJsonFloat(elem, "special_value_3", 0.0f);
        level.specialValue4 = ExtractJsonFloat(elem, "special_value_4", 0.0f);

        // Cleave percent for compatibility
        float cleavePercent = ExtractJsonFloat(elem, "cleave_percent", 0.0f);
        if (cleavePercent > 0.0f && level.specialValue1 == 0.0f) {
            level.specialValue1 = cleavePercent;
        }

        level.charges = ExtractJsonInt(elem, "charges", 0);
        level.chargeRestoreTime = ExtractJsonFloat(elem, "charge_restore_time", 0.0f);
        level.maxTargets = ExtractJsonInt(elem, "max_targets", 1);
        level.bounces = ExtractJsonInt(elem, "bounces", 0);

        m_levelData.push_back(level);
    }

    // Ensure we have data for max level
    while (static_cast<int>(m_levelData.size()) < m_maxLevel) {
        m_levelData.push_back(m_levelData.empty() ? AbilityLevelData{} : m_levelData.back());
    }
}

void AbilityDefinition::ParseTriggers(const std::string& json) {
    std::string triggersArr = ExtractJsonArray(json, "triggers");
    if (triggersArr.empty()) return;

    auto elements = SplitJsonArrayElements(triggersArr);
    m_triggers.clear();

    for (const auto& elem : elements) {
        AbilityTrigger trigger;
        std::string eventStr = ExtractJsonString(elem, "on");
        trigger.event = StringToTriggerEvent(eventStr);
        trigger.effectId = ExtractJsonString(elem, "effect");
        trigger.chance = ExtractJsonFloat(elem, "chance", 1.0f);
        trigger.cooldown = ExtractJsonFloat(elem, "cooldown", 0.0f);
        trigger.threshold = ExtractJsonFloat(elem, "threshold", 0.0f);
        trigger.condition = ExtractJsonString(elem, "condition");
        m_triggers.push_back(trigger);
    }
}

void AbilityDefinition::ParseEffects(const std::string& json) {
    std::string effectsObj = ExtractJsonObject(json, "effects");
    if (effectsObj.empty()) return;

    m_effects.castAnimation = ExtractJsonString(effectsObj, "cast_animation");
    m_effects.castSound = ExtractJsonString(effectsObj, "cast_sound");
    m_effects.castParticle = ExtractJsonString(effectsObj, "cast_particle");
    m_effects.impactSound = ExtractJsonString(effectsObj, "impact_sound");
    m_effects.impactParticle = ExtractJsonString(effectsObj, "impact_particle");
    m_effects.projectileModel = ExtractJsonString(effectsObj, "projectile_model");
    m_effects.projectileTrail = ExtractJsonString(effectsObj, "projectile_trail");
    m_effects.projectileSpeed = ExtractJsonFloat(effectsObj, "projectile_speed", 0.0f);
    m_effects.buffParticle = ExtractJsonString(effectsObj, "buff_particle");
    m_effects.debuffParticle = ExtractJsonString(effectsObj, "debuff_particle");
    m_effects.channelAnimation = ExtractJsonString(effectsObj, "channel_animation");
    m_effects.channelParticle = ExtractJsonString(effectsObj, "channel_particle");
    m_effects.iconPath = ExtractJsonString(effectsObj, "icon");
}

void AbilityDefinition::ParseScripts(const std::string& json) {
    std::string scriptsObj = ExtractJsonObject(json, "scripts");
    if (scriptsObj.empty()) return;

    m_scripts.onLearn = ExtractJsonString(scriptsObj, "on_learn");
    m_scripts.onUpgrade = ExtractJsonString(scriptsObj, "on_upgrade");
    m_scripts.onCastStart = ExtractJsonString(scriptsObj, "on_cast_start");
    m_scripts.onCastComplete = ExtractJsonString(scriptsObj, "on_cast_complete");
    m_scripts.onChannelTick = ExtractJsonString(scriptsObj, "on_channel_tick");
    m_scripts.onChannelEnd = ExtractJsonString(scriptsObj, "on_channel_end");
    m_scripts.onHit = ExtractJsonString(scriptsObj, "on_hit");
    m_scripts.onKill = ExtractJsonString(scriptsObj, "on_kill");
    m_scripts.onToggleOn = ExtractJsonString(scriptsObj, "on_toggle_on");
    m_scripts.onToggleOff = ExtractJsonString(scriptsObj, "on_toggle_off");
    m_scripts.onCreate = ExtractJsonString(scriptsObj, "on_create");
    m_scripts.onDestroy = ExtractJsonString(scriptsObj, "on_destroy");
}

bool AbilityDefinition::SaveToFile(const std::string& filePath) const {
    std::string json = ToJsonString();
    return WriteFile(filePath, json);
}

std::string AbilityDefinition::ToJsonString() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << m_id << "\",\n";
    ss << "  \"name\": \"" << m_name << "\",\n";
    ss << "  \"type\": \"" << AbilityTypeToString(m_type) << "\",\n";
    ss << "  \"max_level\": " << m_maxLevel << ",\n";

    // Level data
    ss << "  \"levels\": [\n";
    for (size_t i = 0; i < m_levelData.size(); i++) {
        const auto& level = m_levelData[i];
        ss << "    {\n";
        ss << "      \"damage\": " << level.damage << ",\n";
        ss << "      \"mana_cost\": " << level.manaCost << ",\n";
        ss << "      \"cooldown\": " << level.cooldown << ",\n";
        ss << "      \"range\": " << level.castRange << ",\n";
        ss << "      \"radius\": " << level.effectRadius << "\n";
        ss << "    }";
        if (i < m_levelData.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";

    ss << "}\n";
    return ss.str();
}

bool AbilityDefinition::Validate(std::vector<std::string>& outErrors) const {
    bool valid = true;

    if (m_id.empty()) {
        outErrors.push_back("Ability ID is empty");
        valid = false;
    }

    if (m_name.empty()) {
        outErrors.push_back("Ability name is empty");
        valid = false;
    }

    if (m_levelData.empty()) {
        outErrors.push_back("Ability has no level data");
        valid = false;
    }

    if (m_maxLevel <= 0 || m_maxLevel > MAX_ABILITY_LEVEL) {
        outErrors.push_back("Invalid max level: " + std::to_string(m_maxLevel));
        valid = false;
    }

    return valid;
}

void AbilityDefinition::Create() {
    if (m_isActive) return;
    m_isActive = true;
    if (m_onCreate) m_onCreate(*this);
}

void AbilityDefinition::Tick(float deltaTime) {
    if (!m_isActive) return;
    if (m_onTick) m_onTick(*this, deltaTime);
}

void AbilityDefinition::Destroy() {
    if (!m_isActive) return;
    if (m_onDestroy) m_onDestroy(*this);
    m_isActive = false;
}

const AbilityLevelData& AbilityDefinition::GetDataForLevel(int level) const {
    static AbilityLevelData empty;
    if (m_levelData.empty()) return empty;

    int idx = std::clamp(level - 1, 0, static_cast<int>(m_levelData.size()) - 1);
    return m_levelData[idx];
}

float AbilityDefinition::GetInterpolatedValue(int level, float AbilityLevelData::*member) const {
    if (m_levelData.empty()) return 0.0f;

    int idx = std::clamp(level - 1, 0, static_cast<int>(m_levelData.size()) - 1);
    return m_levelData[idx].*member;
}

bool AbilityDefinition::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

// ============================================================================
// AbilityDefinitionRegistry Implementation
// ============================================================================

AbilityDefinitionRegistry& AbilityDefinitionRegistry::Instance() {
    static AbilityDefinitionRegistry instance;
    return instance;
}

int AbilityDefinitionRegistry::LoadFromDirectory(const std::string& configPath) {
    m_configPath = configPath;
    int count = 0;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(configPath)) {
            if (entry.path().extension() == ".json") {
                auto definition = std::make_shared<AbilityDefinition>();
                if (definition->LoadFromFile(entry.path().string())) {
                    Register(definition);
                    count++;
                }
            }
        }
    } catch (...) {}

    return count;
}

void AbilityDefinitionRegistry::Register(std::shared_ptr<AbilityDefinition> definition) {
    if (definition) {
        definition->Create();
        m_definitions[definition->GetId()] = definition;
    }
}

std::shared_ptr<AbilityDefinition> AbilityDefinitionRegistry::Get(const std::string& id) const {
    auto it = m_definitions.find(id);
    return (it != m_definitions.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<AbilityDefinition>> AbilityDefinitionRegistry::GetAll() const {
    std::vector<std::shared_ptr<AbilityDefinition>> result;
    result.reserve(m_definitions.size());
    for (const auto& [id, def] : m_definitions) {
        result.push_back(def);
    }
    return result;
}

std::vector<std::shared_ptr<AbilityDefinition>> AbilityDefinitionRegistry::GetByType(AbilityType type) const {
    std::vector<std::shared_ptr<AbilityDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->GetType() == type) result.push_back(def);
    }
    return result;
}

std::vector<std::shared_ptr<AbilityDefinition>> AbilityDefinitionRegistry::GetByTag(const std::string& tag) const {
    std::vector<std::shared_ptr<AbilityDefinition>> result;
    for (const auto& [id, def] : m_definitions) {
        if (def->HasTag(tag)) result.push_back(def);
    }
    return result;
}

bool AbilityDefinitionRegistry::Exists(const std::string& id) const {
    return m_definitions.find(id) != m_definitions.end();
}

void AbilityDefinitionRegistry::Clear() {
    for (auto& [id, def] : m_definitions) def->Destroy();
    m_definitions.clear();
}

void AbilityDefinitionRegistry::Reload() {
    Clear();
    if (!m_configPath.empty()) LoadFromDirectory(m_configPath);
}

void AbilityDefinitionRegistry::Tick(float deltaTime) {
    for (auto& [id, def] : m_definitions) def->Tick(deltaTime);
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string AbilityTypeToString(AbilityType type) {
    switch (type) {
        case AbilityType::Active: return "active";
        case AbilityType::Passive: return "passive";
        case AbilityType::Toggle: return "toggle";
        case AbilityType::Autocast: return "autocast";
    }
    return "active";
}

AbilityType StringToAbilityType(const std::string& str) {
    if (str == "passive") return AbilityType::Passive;
    if (str == "toggle") return AbilityType::Toggle;
    if (str == "autocast") return AbilityType::Autocast;
    return AbilityType::Active;
}

std::string TargetingTypeToString(TargetingType type) {
    switch (type) {
        case TargetingType::None: return "none";
        case TargetingType::Point: return "point";
        case TargetingType::Unit: return "unit";
        case TargetingType::UnitOrPoint: return "unit_or_point";
        case TargetingType::Area: return "area";
        case TargetingType::Direction: return "direction";
        case TargetingType::Cone: return "cone";
        case TargetingType::Line: return "line";
        case TargetingType::Vector: return "vector";
    }
    return "none";
}

TargetingType StringToTargetingType(const std::string& str) {
    if (str == "point") return TargetingType::Point;
    if (str == "unit") return TargetingType::Unit;
    if (str == "unit_or_point") return TargetingType::UnitOrPoint;
    if (str == "area") return TargetingType::Area;
    if (str == "direction") return TargetingType::Direction;
    if (str == "cone") return TargetingType::Cone;
    if (str == "line") return TargetingType::Line;
    if (str == "vector") return TargetingType::Vector;
    return TargetingType::None;
}

std::string TriggerEventToString(TriggerEvent event) {
    switch (event) {
        case TriggerEvent::None: return "none";
        case TriggerEvent::OnAttackHit: return "on_attack_hit";
        case TriggerEvent::OnAttackStart: return "on_attack_start";
        case TriggerEvent::OnDamageTaken: return "on_damage_taken";
        case TriggerEvent::OnDamageDealt: return "on_damage_dealt";
        case TriggerEvent::OnKill: return "on_kill";
        case TriggerEvent::OnDeath: return "on_death";
        case TriggerEvent::OnCast: return "on_cast";
        case TriggerEvent::OnAbilityHit: return "on_ability_hit";
        case TriggerEvent::OnHealthLow: return "on_health_low";
        case TriggerEvent::OnManaLow: return "on_mana_low";
        case TriggerEvent::OnInterval: return "on_interval";
        case TriggerEvent::OnProximity: return "on_proximity";
        case TriggerEvent::OnMove: return "on_move";
        case TriggerEvent::OnStop: return "on_stop";
        case TriggerEvent::OnTakeMagicDamage: return "on_take_magic_damage";
        case TriggerEvent::OnTakePhysicalDamage: return "on_take_physical_damage";
        case TriggerEvent::OnCriticalHit: return "on_critical_hit";
        case TriggerEvent::OnEvasion: return "on_evasion";
    }
    return "none";
}

TriggerEvent StringToTriggerEvent(const std::string& str) {
    if (str == "on_attack_hit") return TriggerEvent::OnAttackHit;
    if (str == "on_attack_start") return TriggerEvent::OnAttackStart;
    if (str == "on_damage_taken") return TriggerEvent::OnDamageTaken;
    if (str == "on_damage_dealt") return TriggerEvent::OnDamageDealt;
    if (str == "on_kill") return TriggerEvent::OnKill;
    if (str == "on_death") return TriggerEvent::OnDeath;
    if (str == "on_cast") return TriggerEvent::OnCast;
    if (str == "on_ability_hit") return TriggerEvent::OnAbilityHit;
    if (str == "on_health_low") return TriggerEvent::OnHealthLow;
    if (str == "on_mana_low") return TriggerEvent::OnManaLow;
    if (str == "on_interval") return TriggerEvent::OnInterval;
    if (str == "on_proximity") return TriggerEvent::OnProximity;
    if (str == "on_move") return TriggerEvent::OnMove;
    if (str == "on_stop") return TriggerEvent::OnStop;
    if (str == "on_take_magic_damage") return TriggerEvent::OnTakeMagicDamage;
    if (str == "on_take_physical_damage") return TriggerEvent::OnTakePhysicalDamage;
    if (str == "on_critical_hit") return TriggerEvent::OnCriticalHit;
    if (str == "on_evasion") return TriggerEvent::OnEvasion;
    return TriggerEvent::None;
}

std::string DamageTypeToString(DamageType type) {
    switch (type) {
        case DamageType::Physical: return "physical";
        case DamageType::Magical: return "magical";
        case DamageType::Pure: return "pure";
        case DamageType::HP_Removal: return "hp_removal";
    }
    return "physical";
}

DamageType StringToDamageType(const std::string& str) {
    if (str == "magical" || str == "magic") return DamageType::Magical;
    if (str == "pure") return DamageType::Pure;
    if (str == "hp_removal") return DamageType::HP_Removal;
    return DamageType::Physical;
}

} // namespace Abilities
} // namespace Vehement
