#include "SpellDefinition.hpp"
#include "SpellTargeting.hpp"
#include "SpellEffect.hpp"
#include "SpellVisuals.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <ctime>
#include <sys/stat.h>

namespace Vehement {
namespace Spells {

// ============================================================================
// Targeting Mode String Conversion
// ============================================================================

const char* TargetingModeToString(TargetingMode mode) {
    switch (mode) {
        case TargetingMode::Self: return "self";
        case TargetingMode::Single: return "single";
        case TargetingMode::PassiveRadius: return "passive_radius";
        case TargetingMode::AOE: return "aoe";
        case TargetingMode::Line: return "line";
        case TargetingMode::Cone: return "cone";
        case TargetingMode::Projectile: return "projectile";
        case TargetingMode::Chain: return "chain";
        default: return "unknown";
    }
}

TargetingMode StringToTargetingMode(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "self") return TargetingMode::Self;
    if (lower == "single") return TargetingMode::Single;
    if (lower == "passive_radius" || lower == "passiveradius") return TargetingMode::PassiveRadius;
    if (lower == "aoe" || lower == "area") return TargetingMode::AOE;
    if (lower == "line") return TargetingMode::Line;
    if (lower == "cone") return TargetingMode::Cone;
    if (lower == "projectile") return TargetingMode::Projectile;
    if (lower == "chain") return TargetingMode::Chain;

    return TargetingMode::Single; // Default
}

const char* TargetPriorityToString(TargetPriority priority) {
    switch (priority) {
        case TargetPriority::Nearest: return "nearest";
        case TargetPriority::Farthest: return "farthest";
        case TargetPriority::LowestHealth: return "lowest_health";
        case TargetPriority::HighestHealth: return "highest_health";
        case TargetPriority::HighestThreat: return "highest_threat";
        case TargetPriority::Random: return "random";
        default: return "nearest";
    }
}

TargetPriority StringToTargetPriority(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "nearest") return TargetPriority::Nearest;
    if (lower == "farthest") return TargetPriority::Farthest;
    if (lower == "lowest_health" || lower == "lowesthealth") return TargetPriority::LowestHealth;
    if (lower == "highest_health" || lower == "highesthealth") return TargetPriority::HighestHealth;
    if (lower == "highest_threat" || lower == "highestthreat") return TargetPriority::HighestThreat;
    if (lower == "random") return TargetPriority::Random;

    return TargetPriority::Nearest;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

namespace {

// Simple JSON value extraction (production code should use a proper JSON library)
std::string ExtractString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";

    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) return "";

    size_t valueEnd = json.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) return "";

    return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

float ExtractFloat(const std::string& json, const std::string& key, float defaultVal = 0.0f) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultVal;

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart])) valueStart++;

    size_t valueEnd = valueStart;
    while (valueEnd < json.size() && (std::isdigit(json[valueEnd]) ||
           json[valueEnd] == '.' || json[valueEnd] == '-')) valueEnd++;

    if (valueStart == valueEnd) return defaultVal;

    try {
        return std::stof(json.substr(valueStart, valueEnd - valueStart));
    } catch (...) {
        return defaultVal;
    }
}

int ExtractInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    return static_cast<int>(ExtractFloat(json, key, static_cast<float>(defaultVal)));
}

bool ExtractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultVal;

    size_t truePos = json.find("true", colonPos);
    size_t falsePos = json.find("false", colonPos);

    if (truePos != std::string::npos && (falsePos == std::string::npos || truePos < falsePos)) {
        return true;
    }
    return defaultVal;
}

std::string ExtractObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) return "";

    int braceCount = 1;
    size_t braceEnd = braceStart + 1;
    while (braceEnd < json.size() && braceCount > 0) {
        if (json[braceEnd] == '{') braceCount++;
        else if (json[braceEnd] == '}') braceCount--;
        braceEnd++;
    }

    return json.substr(braceStart, braceEnd - braceStart);
}

std::string ExtractArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) return "";

    int bracketCount = 1;
    size_t bracketEnd = bracketStart + 1;
    while (bracketEnd < json.size() && bracketCount > 0) {
        if (json[bracketEnd] == '[') bracketCount++;
        else if (json[bracketEnd] == ']') bracketCount--;
        bracketEnd++;
    }

    return json.substr(bracketStart, bracketEnd - bracketStart);
}

std::vector<std::string> ExtractStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::string arrayStr = ExtractArray(json, key);
    if (arrayStr.empty()) return result;

    size_t pos = 0;
    while ((pos = arrayStr.find('"', pos)) != std::string::npos) {
        size_t endPos = arrayStr.find('"', pos + 1);
        if (endPos != std::string::npos) {
            result.push_back(arrayStr.substr(pos + 1, endPos - pos - 1));
            pos = endPos + 1;
        } else {
            break;
        }
    }

    return result;
}

std::string StripJsonComments(const std::string& json) {
    std::string result;
    result.reserve(json.size());

    bool inString = false;
    bool inSingleLineComment = false;
    bool inMultiLineComment = false;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        char next = (i + 1 < json.size()) ? json[i + 1] : '\0';

        if (inSingleLineComment) {
            if (c == '\n') {
                inSingleLineComment = false;
                result += c;
            }
            continue;
        }

        if (inMultiLineComment) {
            if (c == '*' && next == '/') {
                inMultiLineComment = false;
                i++; // Skip the /
            }
            continue;
        }

        if (inString) {
            result += c;
            if (c == '"' && (i == 0 || json[i - 1] != '\\')) {
                inString = false;
            }
            continue;
        }

        // Not in string or comment
        if (c == '"') {
            inString = true;
            result += c;
        } else if (c == '/' && next == '/') {
            inSingleLineComment = true;
            i++; // Skip second /
        } else if (c == '/' && next == '*') {
            inMultiLineComment = true;
            i++; // Skip *
        } else {
            result += c;
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// SpellDefinition Implementation
// ============================================================================

bool SpellDefinition::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    m_sourcePath = filePath;

    // Get file modification time
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        m_lastModified = fileStat.st_mtime;
    }

    return LoadFromString(content);
}

bool SpellDefinition::LoadFromString(const std::string& jsonString) {
    // Strip comments for JSON5 support
    std::string json = StripJsonComments(jsonString);

    // Parse identity
    ParseIdentity(json);

    // Parse targeting
    ParseTargeting(json);

    // Parse timing
    ParseTiming(json);

    // Parse cost
    ParseCost(json);

    // Parse effects
    ParseEffects(json);

    // Parse flags
    ParseFlags(json);

    // Parse requirements
    ParseRequirements(json);

    // Parse scripts
    ParseScripts(json);

    // Parse visuals
    ParseVisuals(json);

    return !m_id.empty();
}

bool SpellDefinition::SaveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJsonString();
    file.close();
    return true;
}

std::string SpellDefinition::ToJsonString() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"id\": \"" << m_id << "\",\n";
    json << "  \"name\": \"" << m_name << "\",\n";
    json << "  \"description\": \"" << m_description << "\",\n";

    if (!m_iconPath.empty()) {
        json << "  \"icon\": \"" << m_iconPath << "\",\n";
    }

    if (!m_school.empty()) {
        json << "  \"school\": \"" << m_school << "\",\n";
    }

    // Tags
    if (!m_tags.empty()) {
        json << "  \"tags\": [";
        for (size_t i = 0; i < m_tags.size(); ++i) {
            json << "\"" << m_tags[i] << "\"";
            if (i < m_tags.size() - 1) json << ", ";
        }
        json << "],\n";
    }

    // Targeting
    json << "  \"targeting\": {\n";
    json << "    \"mode\": \"" << TargetingModeToString(m_targetingMode) << "\",\n";
    json << "    \"range\": " << m_range << ",\n";
    json << "    \"min_range\": " << m_minRange << "\n";
    json << "  },\n";

    // Timing
    json << "  \"timing\": {\n";
    json << "    \"cast_time\": " << m_timing.castTime << ",\n";
    json << "    \"channel_duration\": " << m_timing.channelDuration << ",\n";
    json << "    \"cooldown\": " << m_timing.cooldown << ",\n";
    json << "    \"charges\": " << m_timing.maxCharges << ",\n";
    json << "    \"charge_recharge_time\": " << m_timing.chargeRechargeTime << "\n";
    json << "  },\n";

    // Cost
    json << "  \"cost\": {\n";
    json << "    \"mana\": " << m_cost.mana << ",\n";
    json << "    \"health\": " << m_cost.health << ",\n";
    json << "    \"stamina\": " << m_cost.stamina << "\n";
    json << "  }\n";

    json << "}\n";

    return json.str();
}

bool SpellDefinition::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_id.empty()) {
        errors.push_back("Spell ID is required");
        valid = false;
    }

    if (m_name.empty()) {
        errors.push_back("Spell name is required");
        valid = false;
    }

    if (m_range < 0) {
        errors.push_back("Range cannot be negative");
        valid = false;
    }

    if (m_timing.cooldown < 0) {
        errors.push_back("Cooldown cannot be negative");
        valid = false;
    }

    if (m_timing.castTime < 0) {
        errors.push_back("Cast time cannot be negative");
        valid = false;
    }

    if (m_timing.maxCharges < 1) {
        errors.push_back("Max charges must be at least 1");
        valid = false;
    }

    // Validate targeting config
    if (m_targeting) {
        m_targeting->Validate(errors);
    }

    // Validate effects
    for (const auto& effect : m_effects) {
        if (effect) {
            effect->Validate(errors);
        }
    }

    return valid;
}

void SpellDefinition::OnCreate(SpellInstance& instance) const {
    if (m_onCreate) {
        m_onCreate(instance);
    }
}

void SpellDefinition::OnTick(SpellInstance& instance, float deltaTime) const {
    if (m_onTick) {
        m_onTick(instance);
    }
}

void SpellDefinition::OnDestroy(SpellInstance& instance) const {
    if (m_onDestroy) {
        m_onDestroy(instance);
    }
}

void SpellDefinition::ParseIdentity(const std::string& json) {
    m_id = ExtractString(json, "id");
    m_name = ExtractString(json, "name");
    m_description = ExtractString(json, "description");
    m_iconPath = ExtractString(json, "icon");
    m_school = ExtractString(json, "school");
    m_tags = ExtractStringArray(json, "tags");
}

void SpellDefinition::ParseTargeting(const std::string& json) {
    std::string targetingJson = ExtractObject(json, "targeting");
    if (targetingJson.empty()) return;

    m_targetingMode = StringToTargetingMode(ExtractString(targetingJson, "mode"));
    m_range = ExtractFloat(targetingJson, "range", 30.0f);
    m_minRange = ExtractFloat(targetingJson, "min_range", 0.0f);

    // Create targeting configuration
    m_targeting = std::make_shared<SpellTargeting>();
    m_targeting->LoadFromJson(targetingJson);
}

void SpellDefinition::ParseTiming(const std::string& json) {
    std::string timingJson = ExtractObject(json, "timing");
    if (timingJson.empty()) return;

    m_timing.castTime = ExtractFloat(timingJson, "cast_time", 0.0f);
    m_timing.channelDuration = ExtractFloat(timingJson, "channel_duration", 0.0f);
    m_timing.cooldown = ExtractFloat(timingJson, "cooldown", 0.0f);
    m_timing.maxCharges = ExtractInt(timingJson, "charges", 1);
    m_timing.chargeRechargeTime = ExtractFloat(timingJson, "charge_recharge_time", 0.0f);
    m_timing.triggersGCD = ExtractBool(timingJson, "triggers_gcd", true);
    m_timing.affectedByGCD = ExtractBool(timingJson, "affected_by_gcd", true);
    m_timing.gcdDuration = ExtractFloat(timingJson, "gcd_duration", 1.5f);
    m_timing.castTimeScalesWithHaste = ExtractBool(timingJson, "cast_scales_haste", true);
    m_timing.cooldownScalesWithHaste = ExtractBool(timingJson, "cooldown_scales_haste", false);
}

void SpellDefinition::ParseCost(const std::string& json) {
    std::string costJson = ExtractObject(json, "cost");
    if (costJson.empty()) return;

    m_cost.mana = ExtractFloat(costJson, "mana", 0.0f);
    m_cost.health = ExtractFloat(costJson, "health", 0.0f);
    m_cost.stamina = ExtractFloat(costJson, "stamina", 0.0f);
    m_cost.rage = ExtractFloat(costJson, "rage", 0.0f);
    m_cost.energy = ExtractFloat(costJson, "energy", 0.0f);
}

void SpellDefinition::ParseEffects(const std::string& json) {
    std::string effectsJson = ExtractArray(json, "effects");
    if (effectsJson.empty()) return;

    // Parse each effect object in the array
    size_t pos = 0;
    while ((pos = effectsJson.find('{', pos)) != std::string::npos) {
        int braceCount = 1;
        size_t endPos = pos + 1;
        while (endPos < effectsJson.size() && braceCount > 0) {
            if (effectsJson[endPos] == '{') braceCount++;
            else if (effectsJson[endPos] == '}') braceCount--;
            endPos++;
        }

        std::string effectJson = effectsJson.substr(pos, endPos - pos);
        auto effect = std::make_shared<SpellEffect>();
        if (effect->LoadFromJson(effectJson)) {
            m_effects.push_back(std::move(effect));
        }

        pos = endPos;
    }
}

void SpellDefinition::ParseFlags(const std::string& json) {
    std::string flagsJson = ExtractObject(json, "flags");
    if (flagsJson.empty()) {
        // Also check for individual flags at root level
        m_flags.canCrit = ExtractBool(json, "can_crit", true);
        m_flags.canMiss = ExtractBool(json, "can_miss", false);
        m_flags.canBeInterrupted = ExtractBool(json, "interruptable", true);
        m_flags.canBeSilenced = ExtractBool(json, "silenceable", true);
        m_flags.requiresLineOfSight = ExtractBool(json, "requires_los", true);
        m_flags.canCastWhileMoving = ExtractBool(json, "cast_while_moving", false);
        m_flags.isPassive = ExtractBool(json, "passive", false);
        return;
    }

    m_flags.canCrit = ExtractBool(flagsJson, "can_crit", true);
    m_flags.canMiss = ExtractBool(flagsJson, "can_miss", false);
    m_flags.canBeReflected = ExtractBool(flagsJson, "can_be_reflected", true);
    m_flags.canBeInterrupted = ExtractBool(flagsJson, "can_be_interrupted", true);
    m_flags.canBeSilenced = ExtractBool(flagsJson, "can_be_silenced", true);
    m_flags.requiresLineOfSight = ExtractBool(flagsJson, "requires_los", true);
    m_flags.requiresFacing = ExtractBool(flagsJson, "requires_facing", true);
    m_flags.canCastWhileMoving = ExtractBool(flagsJson, "cast_while_moving", false);
    m_flags.canCastWhileCasting = ExtractBool(flagsJson, "cast_while_casting", false);
    m_flags.isPassive = ExtractBool(flagsJson, "passive", false);
    m_flags.isToggle = ExtractBool(flagsJson, "toggle", false);
    m_flags.isAura = ExtractBool(flagsJson, "aura", false);
    m_flags.breaksOnDamage = ExtractBool(flagsJson, "breaks_on_damage", false);
    m_flags.breaksOnMovement = ExtractBool(flagsJson, "breaks_on_movement", false);
    m_flags.ignoresArmor = ExtractBool(flagsJson, "ignores_armor", false);
    m_flags.ignoresResistance = ExtractBool(flagsJson, "ignores_resistance", false);
}

void SpellDefinition::ParseRequirements(const std::string& json) {
    std::string reqsJson = ExtractObject(json, "requirements");
    if (reqsJson.empty()) return;

    m_requirements.minLevel = ExtractInt(reqsJson, "min_level", 1);
    m_requirements.requiredWeapon = ExtractString(reqsJson, "required_weapon");
    m_requirements.requiredBuffs = ExtractStringArray(reqsJson, "required_buffs");
    m_requirements.forbiddenBuffs = ExtractStringArray(reqsJson, "forbidden_buffs");
    m_requirements.requiredStance = ExtractString(reqsJson, "required_stance");
    m_requirements.requiresCombat = ExtractBool(reqsJson, "requires_combat", false);
    m_requirements.requiresNotCombat = ExtractBool(reqsJson, "requires_not_combat", false);
    m_requirements.requiresStealth = ExtractBool(reqsJson, "requires_stealth", false);
    m_requirements.minHealth = ExtractFloat(reqsJson, "min_health", 0.0f);
    m_requirements.maxHealth = ExtractFloat(reqsJson, "max_health", 100.0f);
}

void SpellDefinition::ParseScripts(const std::string& json) {
    std::string eventsJson = ExtractObject(json, "events");
    if (eventsJson.empty()) {
        // Check for scripts object instead
        eventsJson = ExtractObject(json, "scripts");
    }
    if (eventsJson.empty()) return;

    m_scripts.onCastStart = ExtractString(eventsJson, "on_cast_start");
    m_scripts.onCastComplete = ExtractString(eventsJson, "on_cast_complete");
    m_scripts.onCastInterrupt = ExtractString(eventsJson, "on_cast_interrupt");
    m_scripts.onChannelTick = ExtractString(eventsJson, "on_channel_tick");
    m_scripts.onHit = ExtractString(eventsJson, "on_hit");
    m_scripts.onCrit = ExtractString(eventsJson, "on_crit");
    m_scripts.onKill = ExtractString(eventsJson, "on_kill");
    m_scripts.onMiss = ExtractString(eventsJson, "on_miss");
    m_scripts.onReflect = ExtractString(eventsJson, "on_reflect");
    m_scripts.onAbsorb = ExtractString(eventsJson, "on_absorb");
}

void SpellDefinition::ParseVisuals(const std::string& json) {
    std::string visualsJson = ExtractObject(json, "visuals");
    if (visualsJson.empty()) return;

    m_visuals = std::make_shared<SpellVisuals>();
    m_visuals->LoadFromJson(visualsJson);
}

// ============================================================================
// SpellInstance Implementation
// ============================================================================

SpellInstance::SpellInstance(const SpellDefinition* definition, uint32_t casterId)
    : m_definition(definition)
    , m_casterId(casterId)
{
    if (definition) {
        const auto& timing = definition->GetTiming();
        m_remainingCastTime = timing.castTime;
        m_totalCastTime = timing.castTime;
        m_remainingChannelTime = timing.channelDuration;
        m_totalChannelTime = timing.channelDuration;
    }
}

float SpellInstance::GetCastProgress() const {
    if (m_totalCastTime <= 0.0f) return 1.0f;
    return 1.0f - (m_remainingCastTime / m_totalCastTime);
}

float SpellInstance::GetChannelProgress() const {
    if (m_totalChannelTime <= 0.0f) return 1.0f;
    return 1.0f - (m_remainingChannelTime / m_totalChannelTime);
}

} // namespace Spells
} // namespace Vehement
