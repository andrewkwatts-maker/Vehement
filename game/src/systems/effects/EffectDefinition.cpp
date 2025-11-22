#include "EffectDefinition.hpp"
#include "EffectTrigger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <cstring>

// Simple JSON parsing helpers (in production, use nlohmann/json or similar)
#include <regex>

namespace Vehement {
namespace Effects {

// ============================================================================
// Type Conversion Tables
// ============================================================================

static const std::unordered_map<std::string, EffectType> s_effectTypeFromString = {
    {"buff", EffectType::Buff},
    {"debuff", EffectType::Debuff},
    {"aura", EffectType::Aura},
    {"passive", EffectType::Passive},
    {"triggered", EffectType::Triggered}
};

const char* EffectTypeToString(EffectType type) {
    switch (type) {
        case EffectType::Buff:      return "buff";
        case EffectType::Debuff:    return "debuff";
        case EffectType::Aura:      return "aura";
        case EffectType::Passive:   return "passive";
        case EffectType::Triggered: return "triggered";
    }
    return "buff";
}

std::optional<EffectType> EffectTypeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_effectTypeFromString.find(lower);
    if (it != s_effectTypeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, DurationType> s_durationTypeFromString = {
    {"permanent", DurationType::Permanent},
    {"timed", DurationType::Timed},
    {"charges", DurationType::Charges},
    {"hybrid", DurationType::Hybrid}
};

const char* DurationTypeToString(DurationType type) {
    switch (type) {
        case DurationType::Permanent: return "permanent";
        case DurationType::Timed:     return "timed";
        case DurationType::Charges:   return "charges";
        case DurationType::Hybrid:    return "hybrid";
    }
    return "timed";
}

std::optional<DurationType> DurationTypeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_durationTypeFromString.find(lower);
    if (it != s_durationTypeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, StackingMode> s_stackingModeFromString = {
    {"none", StackingMode::None},
    {"refresh", StackingMode::Refresh},
    {"duration", StackingMode::Duration},
    {"intensity", StackingMode::Intensity},
    {"separate", StackingMode::Separate}
};

const char* StackingModeToString(StackingMode mode) {
    switch (mode) {
        case StackingMode::None:      return "none";
        case StackingMode::Refresh:   return "refresh";
        case StackingMode::Duration:  return "duration";
        case StackingMode::Intensity: return "intensity";
        case StackingMode::Separate:  return "separate";
    }
    return "refresh";
}

std::optional<StackingMode> StackingModeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_stackingModeFromString.find(lower);
    if (it != s_stackingModeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, DamageType> s_damageTypeFromString = {
    {"physical", DamageType::Physical},
    {"fire", DamageType::Fire},
    {"ice", DamageType::Ice},
    {"frost", DamageType::Ice},
    {"cold", DamageType::Ice},
    {"lightning", DamageType::Lightning},
    {"electric", DamageType::Lightning},
    {"poison", DamageType::Poison},
    {"toxic", DamageType::Poison},
    {"holy", DamageType::Holy},
    {"light", DamageType::Holy},
    {"dark", DamageType::Dark},
    {"shadow", DamageType::Dark},
    {"arcane", DamageType::Arcane},
    {"magic", DamageType::Arcane},
    {"nature", DamageType::Nature},
    {"true", DamageType::True},
    {"pure", DamageType::True}
};

const char* DamageTypeToString(DamageType type) {
    switch (type) {
        case DamageType::Physical:  return "physical";
        case DamageType::Fire:      return "fire";
        case DamageType::Ice:       return "ice";
        case DamageType::Lightning: return "lightning";
        case DamageType::Poison:    return "poison";
        case DamageType::Holy:      return "holy";
        case DamageType::Dark:      return "dark";
        case DamageType::Arcane:    return "arcane";
        case DamageType::Nature:    return "nature";
        case DamageType::True:      return "true";
    }
    return "physical";
}

std::optional<DamageType> DamageTypeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_damageTypeFromString.find(lower);
    if (it != s_damageTypeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// JSON Helper Functions
// ============================================================================

namespace {

// Strip C-style and C++-style comments from JSON
std::string StripJsonComments(const std::string& json) {
    std::string result;
    result.reserve(json.size());

    bool inString = false;
    bool inLineComment = false;
    bool inBlockComment = false;

    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        char next = (i + 1 < json.size()) ? json[i + 1] : '\0';

        if (inLineComment) {
            if (c == '\n') {
                inLineComment = false;
                result += c;
            }
            continue;
        }

        if (inBlockComment) {
            if (c == '*' && next == '/') {
                inBlockComment = false;
                ++i;
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

        if (c == '"') {
            inString = true;
            result += c;
            continue;
        }

        if (c == '/' && next == '/') {
            inLineComment = true;
            ++i;
            continue;
        }

        if (c == '/' && next == '*') {
            inBlockComment = true;
            ++i;
            continue;
        }

        result += c;
    }

    return result;
}

// Extract string value from JSON
std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str();
    }
    return "";
}

// Extract number value from JSON
float ExtractJsonNumber(const std::string& json, const std::string& key, float defaultVal = 0.0f) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]*\\.?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        try {
            return std::stof(match[1].str());
        } catch (...) {}
    }
    return defaultVal;
}

// Extract integer value from JSON
int ExtractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {}
    }
    return defaultVal;
}

// Extract boolean value from JSON
bool ExtractJsonBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str() == "true";
    }
    return defaultVal;
}

// Extract array of strings from JSON
std::vector<std::string> ExtractJsonStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::regex arrayPattern("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch arrayMatch;
    if (std::regex_search(json, arrayMatch, arrayPattern)) {
        std::string arrayContent = arrayMatch[1].str();
        std::regex stringPattern("\"([^\"]*)\"");
        auto begin = std::sregex_iterator(arrayContent.begin(), arrayContent.end(), stringPattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            result.push_back((*it)[1].str());
        }
    }
    return result;
}

// Extract vec4 from JSON array
glm::vec4 ExtractJsonVec4(const std::string& json, const std::string& key, const glm::vec4& defaultVal = glm::vec4(1.0f)) {
    std::regex arrayPattern("\"" + key + "\"\\s*:\\s*\\[([^\\]]*)\\]");
    std::smatch arrayMatch;
    if (std::regex_search(json, arrayMatch, arrayPattern)) {
        std::string arrayContent = arrayMatch[1].str();
        std::regex numPattern("(-?[0-9]*\\.?[0-9]+)");
        auto begin = std::sregex_iterator(arrayContent.begin(), arrayContent.end(), numPattern);
        auto end = std::sregex_iterator();
        std::vector<float> values;
        for (auto it = begin; it != end; ++it) {
            try {
                values.push_back(std::stof((*it)[0].str()));
            } catch (...) {}
        }
        if (values.size() >= 4) {
            return glm::vec4(values[0], values[1], values[2], values[3]);
        } else if (values.size() >= 3) {
            return glm::vec4(values[0], values[1], values[2], 1.0f);
        }
    }
    return defaultVal;
}

// Extract JSON object as string
std::string ExtractJsonObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return "";

    size_t start = json.find('{', colonPos);
    if (start == std::string::npos) return "";

    int depth = 1;
    size_t end = start + 1;
    while (end < json.size() && depth > 0) {
        if (json[end] == '{') depth++;
        else if (json[end] == '}') depth--;
        end++;
    }

    return json.substr(start, end - start);
}

// Extract JSON array as string
std::string ExtractJsonArray(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";

    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) return "";

    size_t start = json.find('[', colonPos);
    if (start == std::string::npos) return "";

    int depth = 1;
    size_t end = start + 1;
    while (end < json.size() && depth > 0) {
        if (json[end] == '[') depth++;
        else if (json[end] == ']') depth--;
        end++;
    }

    return json.substr(start, end - start);
}

// Split JSON array into elements
std::vector<std::string> SplitJsonArray(const std::string& arrayStr) {
    std::vector<std::string> result;
    if (arrayStr.empty() || arrayStr[0] != '[') return result;

    int depth = 0;
    size_t elementStart = 1;
    bool inString = false;

    for (size_t i = 1; i < arrayStr.size() - 1; ++i) {
        char c = arrayStr[i];

        if (c == '"' && (i == 0 || arrayStr[i-1] != '\\')) {
            inString = !inString;
            continue;
        }

        if (inString) continue;

        if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') depth--;
        else if (c == ',' && depth == 0) {
            std::string element = arrayStr.substr(elementStart, i - elementStart);
            // Trim whitespace
            size_t first = element.find_first_not_of(" \t\n\r");
            size_t last = element.find_last_not_of(" \t\n\r");
            if (first != std::string::npos) {
                result.push_back(element.substr(first, last - first + 1));
            }
            elementStart = i + 1;
        }
    }

    // Last element
    if (elementStart < arrayStr.size() - 1) {
        std::string element = arrayStr.substr(elementStart, arrayStr.size() - 1 - elementStart);
        size_t first = element.find_first_not_of(" \t\n\r");
        size_t last = element.find_last_not_of(" \t\n\r");
        if (first != std::string::npos) {
            result.push_back(element.substr(first, last - first + 1));
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// Periodic Effect
// ============================================================================

bool PeriodicEffect::LoadFromJson(const std::string& jsonStr) {
    interval = ExtractJsonNumber(jsonStr, "interval", 1.0f);
    tickOnApply = ExtractJsonBool(jsonStr, "tick_on_apply", false);
    tickOnExpire = ExtractJsonBool(jsonStr, "tick_on_expire", false);
    amount = ExtractJsonNumber(jsonStr, "amount", 10.0f);
    scaling = ExtractJsonNumber(jsonStr, "scaling", 0.0f);

    std::string typeStr = ExtractJsonString(jsonStr, "type");
    if (typeStr == "damage") type = Type::Damage;
    else if (typeStr == "heal") type = Type::Heal;
    else if (typeStr == "mana") type = Type::Mana;
    else if (typeStr == "stamina") type = Type::Stamina;
    else if (typeStr == "custom") type = Type::Custom;

    std::string dmgTypeStr = ExtractJsonString(jsonStr, "damage_type");
    if (auto dt = DamageTypeFromString(dmgTypeStr)) {
        damageType = *dt;
    }

    std::string scalingStatStr = ExtractJsonString(jsonStr, "scaling_stat");
    if (auto st = StatTypeFromString(scalingStatStr)) {
        scalingStat = *st;
    }

    scriptPath = ExtractJsonString(jsonStr, "script");
    functionName = ExtractJsonString(jsonStr, "function");

    return true;
}

std::string PeriodicEffect::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"interval\":" << interval;

    const char* typeStr = "damage";
    switch (type) {
        case Type::Damage:  typeStr = "damage"; break;
        case Type::Heal:    typeStr = "heal"; break;
        case Type::Mana:    typeStr = "mana"; break;
        case Type::Stamina: typeStr = "stamina"; break;
        case Type::Custom:  typeStr = "custom"; break;
    }
    ss << ",\"type\":\"" << typeStr << "\"";
    ss << ",\"amount\":" << amount;

    if (type == Type::Damage) {
        ss << ",\"damage_type\":\"" << DamageTypeToString(damageType) << "\"";
    }

    if (scaling > 0.0f) {
        ss << ",\"scaling\":" << scaling;
        ss << ",\"scaling_stat\":\"" << StatTypeToString(scalingStat) << "\"";
    }

    if (tickOnApply) ss << ",\"tick_on_apply\":true";
    if (tickOnExpire) ss << ",\"tick_on_expire\":true";

    if (!scriptPath.empty()) {
        ss << ",\"script\":\"" << scriptPath << "\"";
        if (!functionName.empty()) {
            ss << ",\"function\":\"" << functionName << "\"";
        }
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Effect Visual
// ============================================================================

bool EffectVisual::LoadFromJson(const std::string& jsonStr) {
    iconPath = ExtractJsonString(jsonStr, "icon");
    particlePath = ExtractJsonString(jsonStr, "particle");
    shaderOverride = ExtractJsonString(jsonStr, "shader");
    tint = ExtractJsonVec4(jsonStr, "tint", glm::vec4(1.0f));
    glowIntensity = ExtractJsonNumber(jsonStr, "glow", 0.0f);
    attachPoint = ExtractJsonString(jsonStr, "attach_point");
    looping = ExtractJsonBool(jsonStr, "looping", true);
    scale = ExtractJsonNumber(jsonStr, "scale", 1.0f);

    return true;
}

std::string EffectVisual::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    bool first = true;

    if (!iconPath.empty()) {
        ss << "\"icon\":\"" << iconPath << "\"";
        first = false;
    }

    if (!particlePath.empty()) {
        if (!first) ss << ",";
        ss << "\"particle\":\"" << particlePath << "\"";
        first = false;
    }

    if (tint != glm::vec4(1.0f)) {
        if (!first) ss << ",";
        ss << "\"tint\":[" << tint.r << "," << tint.g << "," << tint.b << "," << tint.a << "]";
        first = false;
    }

    if (glowIntensity > 0.0f) {
        if (!first) ss << ",";
        ss << "\"glow\":" << glowIntensity;
        first = false;
    }

    if (!attachPoint.empty()) {
        if (!first) ss << ",";
        ss << "\"attach_point\":\"" << attachPoint << "\"";
        first = false;
    }

    if (!looping) {
        if (!first) ss << ",";
        ss << "\"looping\":false";
        first = false;
    }

    if (scale != 1.0f) {
        if (!first) ss << ",";
        ss << "\"scale\":" << scale;
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Effect Events
// ============================================================================

bool EffectEvents::LoadFromJson(const std::string& jsonStr) {
    onApply = ExtractJsonString(jsonStr, "on_apply");
    onRefresh = ExtractJsonString(jsonStr, "on_refresh");
    onStack = ExtractJsonString(jsonStr, "on_stack");
    onTick = ExtractJsonString(jsonStr, "on_tick");
    onExpire = ExtractJsonString(jsonStr, "on_expire");
    onRemove = ExtractJsonString(jsonStr, "on_remove");
    onDispel = ExtractJsonString(jsonStr, "on_dispel");

    return true;
}

std::string EffectEvents::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    bool first = true;

    auto addScript = [&](const char* name, const std::string& path) {
        if (!path.empty()) {
            if (!first) ss << ",";
            ss << "\"" << name << "\":\"" << path << "\"";
            first = false;
        }
    };

    addScript("on_apply", onApply);
    addScript("on_refresh", onRefresh);
    addScript("on_stack", onStack);
    addScript("on_tick", onTick);
    addScript("on_expire", onExpire);
    addScript("on_remove", onRemove);
    addScript("on_dispel", onDispel);

    ss << "}";
    return ss.str();
}

// ============================================================================
// Stacking Config
// ============================================================================

bool StackingConfig::LoadFromJson(const std::string& jsonStr) {
    std::string modeStr = ExtractJsonString(jsonStr, "mode");
    if (auto m = StackingModeFromString(modeStr)) {
        mode = *m;
    }

    maxStacks = ExtractJsonInt(jsonStr, "max_stacks", 1);
    stackDurationBonus = ExtractJsonNumber(jsonStr, "duration_bonus", 0.0f);
    intensityPerStack = ExtractJsonNumber(jsonStr, "intensity_per_stack", 1.0f);
    separatePerSource = ExtractJsonBool(jsonStr, "separate_per_source", false);

    return true;
}

std::string StackingConfig::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"mode\":\"" << StackingModeToString(mode) << "\"";
    ss << ",\"max_stacks\":" << maxStacks;

    if (stackDurationBonus > 0.0f) {
        ss << ",\"duration_bonus\":" << stackDurationBonus;
    }

    if (intensityPerStack != 1.0f) {
        ss << ",\"intensity_per_stack\":" << intensityPerStack;
    }

    if (separatePerSource) {
        ss << ",\"separate_per_source\":true";
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Effect Definition
// ============================================================================

EffectDefinition::EffectDefinition() = default;

bool EffectDefinition::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    m_sourcePath = filePath;

    // Get last modified time
    try {
        auto ftime = std::filesystem::last_write_time(filePath);
        m_lastModified = ftime.time_since_epoch().count();
    } catch (...) {
        m_lastModified = 0;
    }

    return LoadFromJson(content);
}

bool EffectDefinition::LoadFromJson(const std::string& jsonStr) {
    std::string json = StripJsonComments(jsonStr);

    // Identity
    m_id = ExtractJsonString(json, "id");
    m_name = ExtractJsonString(json, "name");
    m_description = ExtractJsonString(json, "description");

    std::string typeStr = ExtractJsonString(json, "type");
    if (auto t = EffectTypeFromString(typeStr)) {
        m_type = *t;
    }

    m_tags = ExtractJsonStringArray(json, "tags");
    m_categories = ExtractJsonStringArray(json, "categories");

    // Duration
    m_baseDuration = ExtractJsonNumber(json, "duration", 10.0f);
    m_maxCharges = ExtractJsonInt(json, "charges", 1);

    std::string durTypeStr = ExtractJsonString(json, "duration_type");
    if (auto dt = DurationTypeFromString(durTypeStr)) {
        m_durationType = *dt;
    } else if (m_baseDuration <= 0.0f) {
        m_durationType = DurationType::Permanent;
    }

    // Stacking
    std::string stackingObj = ExtractJsonObject(json, "stacking");
    if (!stackingObj.empty()) {
        m_stacking.LoadFromJson(stackingObj);
    }

    // Modifiers
    std::string modifiersArray = ExtractJsonArray(json, "modifiers");
    if (!modifiersArray.empty()) {
        auto modElements = SplitJsonArray(modifiersArray);
        for (const auto& modJson : modElements) {
            StatModifier mod;
            std::string statStr = ExtractJsonString(modJson, "stat");
            if (auto s = StatTypeFromString(statStr)) {
                mod.stat = *s;
            }

            std::string opStr = ExtractJsonString(modJson, "op");
            if (auto o = ModifierOpFromString(opStr)) {
                mod.operation = *o;
            }

            mod.value = ExtractJsonNumber(modJson, "value", 0.0f);
            mod.priority = ExtractJsonInt(modJson, "priority", 0);

            // Conditional modifier
            std::string condObj = ExtractJsonObject(modJson, "condition");
            if (!condObj.empty()) {
                ModifierCondition cond;
                std::string condTypeStr = ExtractJsonString(condObj, "type");
                if (auto ct = ConditionTypeFromString(condTypeStr)) {
                    cond.type = *ct;
                }
                cond.threshold = ExtractJsonNumber(condObj, "threshold", 0.0f);
                cond.parameter = ExtractJsonString(condObj, "parameter");
                cond.inverted = ExtractJsonBool(condObj, "inverted", false);
                mod.condition = cond;
            }

            m_modifiers.push_back(mod);
        }
    }

    // Periodic effects
    std::string periodicArray = ExtractJsonArray(json, "periodic");
    if (!periodicArray.empty()) {
        auto periodicElements = SplitJsonArray(periodicArray);
        for (const auto& perJson : periodicElements) {
            PeriodicEffect periodic;
            periodic.LoadFromJson(perJson);
            m_periodicEffects.push_back(periodic);
        }
    }

    // Single periodic object support
    std::string periodicObj = ExtractJsonObject(json, "periodic");
    if (!periodicObj.empty() && periodicObj[0] == '{') {
        PeriodicEffect periodic;
        periodic.LoadFromJson(periodicObj);
        m_periodicEffects.push_back(periodic);
    }

    // Visual
    std::string visualObj = ExtractJsonObject(json, "visual");
    if (!visualObj.empty()) {
        m_visual.LoadFromJson(visualObj);
    }

    // Icon shorthand
    std::string iconPath = ExtractJsonString(json, "icon");
    if (!iconPath.empty() && m_visual.iconPath.empty()) {
        m_visual.iconPath = iconPath;
    }

    // Events
    std::string eventsObj = ExtractJsonObject(json, "events");
    if (!eventsObj.empty()) {
        m_events.LoadFromJson(eventsObj);
    }

    // Flags
    m_dispellable = ExtractJsonBool(json, "dispellable", true);
    m_purgeable = ExtractJsonBool(json, "purgeable", true);
    m_hidden = ExtractJsonBool(json, "hidden", false);
    m_persistent = ExtractJsonBool(json, "persistent", false);
    m_priority = ExtractJsonInt(json, "priority", 0);

    // Immunity
    m_immunityTags = ExtractJsonStringArray(json, "immunity_tags");

    return !m_id.empty();
}

std::string EffectDefinition::ToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << m_id << "\",\n";
    ss << "  \"name\": \"" << m_name << "\",\n";

    if (!m_description.empty()) {
        ss << "  \"description\": \"" << m_description << "\",\n";
    }

    ss << "  \"type\": \"" << EffectTypeToString(m_type) << "\",\n";

    if (!m_tags.empty()) {
        ss << "  \"tags\": [";
        for (size_t i = 0; i < m_tags.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << m_tags[i] << "\"";
        }
        ss << "],\n";
    }

    ss << "  \"duration\": " << m_baseDuration << ",\n";

    if (m_durationType != DurationType::Timed) {
        ss << "  \"duration_type\": \"" << DurationTypeToString(m_durationType) << "\",\n";
    }

    if (m_maxCharges > 1) {
        ss << "  \"charges\": " << m_maxCharges << ",\n";
    }

    ss << "  \"stacking\": " << m_stacking.ToJson() << ",\n";

    if (!m_modifiers.empty()) {
        ss << "  \"modifiers\": [\n";
        for (size_t i = 0; i < m_modifiers.size(); ++i) {
            ss << "    " << m_modifiers[i].ToJson();
            if (i < m_modifiers.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ],\n";
    }

    if (!m_periodicEffects.empty()) {
        ss << "  \"periodic\": [\n";
        for (size_t i = 0; i < m_periodicEffects.size(); ++i) {
            ss << "    " << m_periodicEffects[i].ToJson();
            if (i < m_periodicEffects.size() - 1) ss << ",";
            ss << "\n";
        }
        ss << "  ],\n";
    }

    ss << "  \"visual\": " << m_visual.ToJson() << ",\n";
    ss << "  \"events\": " << m_events.ToJson() << ",\n";

    ss << "  \"dispellable\": " << (m_dispellable ? "true" : "false") << ",\n";
    ss << "  \"purgeable\": " << (m_purgeable ? "true" : "false") << ",\n";
    ss << "  \"hidden\": " << (m_hidden ? "true" : "false") << ",\n";
    ss << "  \"priority\": " << m_priority << "\n";

    ss << "}";
    return ss.str();
}

std::vector<std::string> EffectDefinition::Validate() const {
    std::vector<std::string> errors;

    if (m_id.empty()) {
        errors.push_back("Effect ID is required");
    }

    if (m_name.empty()) {
        errors.push_back("Effect name is required");
    }

    if (m_durationType == DurationType::Timed && m_baseDuration <= 0.0f) {
        errors.push_back("Timed effects must have positive duration");
    }

    if (m_durationType == DurationType::Charges && m_maxCharges <= 0) {
        errors.push_back("Charge-based effects must have positive max charges");
    }

    if (m_stacking.maxStacks < 1) {
        errors.push_back("Max stacks must be at least 1");
    }

    for (const auto& periodic : m_periodicEffects) {
        if (periodic.interval <= 0.0f) {
            errors.push_back("Periodic effect interval must be positive");
        }
    }

    return errors;
}

bool EffectDefinition::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

void EffectDefinition::AddTrigger(const EffectTrigger& trigger) {
    m_triggers.push_back(trigger);
}

// ============================================================================
// Effect Definition Builder
// ============================================================================

EffectDefinitionBuilder& EffectDefinitionBuilder::Id(const std::string& id) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetId(id);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Name(const std::string& name) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetName(name);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Description(const std::string& desc) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetDescription(desc);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Type(EffectType type) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetType(type);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Tag(const std::string& tag) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->AddTag(tag);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Duration(float seconds) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetBaseDuration(seconds);
    m_definition->SetDurationType(DurationType::Timed);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Permanent() {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetDurationType(DurationType::Permanent);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Charges(int count) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetMaxCharges(count);
    m_definition->SetDurationType(DurationType::Charges);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Stacking(StackingMode mode, int maxStacks) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    StackingConfig config;
    config.mode = mode;
    config.maxStacks = maxStacks;
    m_definition->SetStacking(config);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::AddModifier(StatType stat, ModifierOp op, float value) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    StatModifier mod;
    mod.stat = stat;
    mod.operation = op;
    mod.value = value;
    m_definition->AddModifier(mod);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::AddPeriodicDamage(float damage, float interval, DamageType type) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    PeriodicEffect periodic;
    periodic.type = PeriodicEffect::Type::Damage;
    periodic.amount = damage;
    periodic.interval = interval;
    periodic.damageType = type;
    m_definition->AddPeriodicEffect(periodic);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::AddPeriodicHeal(float amount, float interval) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    PeriodicEffect periodic;
    periodic.type = PeriodicEffect::Type::Heal;
    periodic.amount = amount;
    periodic.interval = interval;
    m_definition->AddPeriodicEffect(periodic);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Icon(const std::string& path) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    EffectVisual visual = m_definition->GetVisual();
    visual.iconPath = path;
    m_definition->SetVisual(visual);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Particle(const std::string& path) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    EffectVisual visual = m_definition->GetVisual();
    visual.particlePath = path;
    m_definition->SetVisual(visual);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Tint(const glm::vec4& color) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    EffectVisual visual = m_definition->GetVisual();
    visual.tint = color;
    m_definition->SetVisual(visual);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Dispellable(bool value) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetDispellable(value);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Hidden(bool value) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetHidden(value);
    return *this;
}

EffectDefinitionBuilder& EffectDefinitionBuilder::Priority(int value) {
    if (!m_definition) m_definition = std::make_unique<EffectDefinition>();
    m_definition->SetPriority(value);
    return *this;
}

std::unique_ptr<EffectDefinition> EffectDefinitionBuilder::Build() {
    return std::move(m_definition);
}

} // namespace Effects
} // namespace Vehement
