#include "SpellVisuals.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace Spells {

// ============================================================================
// String Conversion Functions
// ============================================================================

const char* VisualEffectTypeToString(VisualEffectType type) {
    switch (type) {
        case VisualEffectType::Particle: return "particle";
        case VisualEffectType::Model: return "model";
        case VisualEffectType::Decal: return "decal";
        case VisualEffectType::Light: return "light";
        case VisualEffectType::ScreenEffect: return "screen_effect";
        case VisualEffectType::Trail: return "trail";
        case VisualEffectType::Beam: return "beam";
        case VisualEffectType::Sprite: return "sprite";
        case VisualEffectType::Animation: return "animation";
        default: return "unknown";
    }
}

VisualEffectType StringToVisualEffectType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "particle") return VisualEffectType::Particle;
    if (lower == "model") return VisualEffectType::Model;
    if (lower == "decal") return VisualEffectType::Decal;
    if (lower == "light") return VisualEffectType::Light;
    if (lower == "screen_effect" || lower == "screen") return VisualEffectType::ScreenEffect;
    if (lower == "trail") return VisualEffectType::Trail;
    if (lower == "beam") return VisualEffectType::Beam;
    if (lower == "sprite") return VisualEffectType::Sprite;
    if (lower == "animation") return VisualEffectType::Animation;

    return VisualEffectType::Particle;
}

const char* AttachPointToString(AttachPoint point) {
    switch (point) {
        case AttachPoint::Origin: return "origin";
        case AttachPoint::Caster: return "caster";
        case AttachPoint::CasterHand: return "caster_hand";
        case AttachPoint::CasterChest: return "caster_chest";
        case AttachPoint::CasterHead: return "caster_head";
        case AttachPoint::CasterFeet: return "caster_feet";
        case AttachPoint::Target: return "target";
        case AttachPoint::TargetCenter: return "target_center";
        case AttachPoint::TargetGround: return "target_ground";
        case AttachPoint::TargetPoint: return "target_point";
        case AttachPoint::Projectile: return "projectile";
        case AttachPoint::Impact: return "impact";
        default: return "origin";
    }
}

AttachPoint StringToAttachPoint(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "origin") return AttachPoint::Origin;
    if (lower == "caster") return AttachPoint::Caster;
    if (lower == "caster_hand" || lower == "hand") return AttachPoint::CasterHand;
    if (lower == "caster_chest" || lower == "chest") return AttachPoint::CasterChest;
    if (lower == "caster_head" || lower == "head") return AttachPoint::CasterHead;
    if (lower == "caster_feet" || lower == "feet") return AttachPoint::CasterFeet;
    if (lower == "target") return AttachPoint::Target;
    if (lower == "target_center") return AttachPoint::TargetCenter;
    if (lower == "target_ground") return AttachPoint::TargetGround;
    if (lower == "target_point") return AttachPoint::TargetPoint;
    if (lower == "projectile") return AttachPoint::Projectile;
    if (lower == "impact") return AttachPoint::Impact;

    return AttachPoint::Origin;
}

const char* TriggerToString(VisualEffectEntry::Trigger trigger) {
    switch (trigger) {
        case VisualEffectEntry::Trigger::OnCastStart: return "on_cast_start";
        case VisualEffectEntry::Trigger::OnCastComplete: return "on_cast_complete";
        case VisualEffectEntry::Trigger::OnChannelTick: return "on_channel_tick";
        case VisualEffectEntry::Trigger::OnProjectileLaunch: return "on_projectile_launch";
        case VisualEffectEntry::Trigger::OnProjectileTravel: return "on_projectile_travel";
        case VisualEffectEntry::Trigger::OnHit: return "on_hit";
        case VisualEffectEntry::Trigger::OnCrit: return "on_crit";
        case VisualEffectEntry::Trigger::OnKill: return "on_kill";
        case VisualEffectEntry::Trigger::OnMiss: return "on_miss";
        case VisualEffectEntry::Trigger::OnExpire: return "on_expire";
        case VisualEffectEntry::Trigger::Continuous: return "continuous";
        default: return "on_cast_start";
    }
}

VisualEffectEntry::Trigger StringToTrigger(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "on_cast_start" || lower == "cast_start") return VisualEffectEntry::Trigger::OnCastStart;
    if (lower == "on_cast_complete" || lower == "cast_complete") return VisualEffectEntry::Trigger::OnCastComplete;
    if (lower == "on_channel_tick" || lower == "channel_tick") return VisualEffectEntry::Trigger::OnChannelTick;
    if (lower == "on_projectile_launch" || lower == "projectile_launch") return VisualEffectEntry::Trigger::OnProjectileLaunch;
    if (lower == "on_projectile_travel" || lower == "projectile_travel") return VisualEffectEntry::Trigger::OnProjectileTravel;
    if (lower == "on_hit" || lower == "hit") return VisualEffectEntry::Trigger::OnHit;
    if (lower == "on_crit" || lower == "crit") return VisualEffectEntry::Trigger::OnCrit;
    if (lower == "on_kill" || lower == "kill") return VisualEffectEntry::Trigger::OnKill;
    if (lower == "on_miss" || lower == "miss") return VisualEffectEntry::Trigger::OnMiss;
    if (lower == "on_expire" || lower == "expire") return VisualEffectEntry::Trigger::OnExpire;
    if (lower == "continuous" || lower == "always") return VisualEffectEntry::Trigger::Continuous;

    return VisualEffectEntry::Trigger::OnCastStart;
}

// ============================================================================
// JSON Parsing Helpers
// ============================================================================

namespace {

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

bool ExtractBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultVal;

    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultVal;

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && std::isspace(json[valueStart])) valueStart++;

    if (json.compare(valueStart, 4, "true") == 0) return true;
    if (json.compare(valueStart, 5, "false") == 0) return false;

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

} // anonymous namespace

// ============================================================================
// SpellVisuals Implementation
// ============================================================================

bool SpellVisuals::LoadFromJson(const std::string& jsonString) {
    // Parse projectile visuals
    std::string projectileJson = ExtractObject(jsonString, "projectile");
    if (!projectileJson.empty()) {
        m_projectileModel = ExtractString(projectileJson, "model");
        m_projectileTrail = ExtractString(projectileJson, "trail");
        m_projectileScale = ExtractFloat(projectileJson, "scale", 1.0f);
    }

    // Parse icons
    m_iconPath = ExtractString(jsonString, "icon");
    m_cooldownIconPath = ExtractString(jsonString, "cooldown_icon");

    // Parse visual effects array
    std::string effectsJson = ExtractArray(jsonString, "effects");
    if (!effectsJson.empty()) {
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

            VisualEffectEntry entry;
            entry.id = ExtractString(effectJson, "id");
            entry.type = StringToVisualEffectType(ExtractString(effectJson, "type"));
            entry.attachPoint = StringToAttachPoint(ExtractString(effectJson, "attach"));
            entry.trigger = StringToTrigger(ExtractString(effectJson, "trigger"));
            entry.delay = ExtractFloat(effectJson, "delay", 0.0f);
            entry.duration = ExtractFloat(effectJson, "duration", 0.0f);

            // Parse type-specific config
            switch (entry.type) {
                case VisualEffectType::Particle: {
                    ParticleConfig cfg;
                    cfg.systemPath = ExtractString(effectJson, "path");
                    cfg.scale = ExtractFloat(effectJson, "scale", 1.0f);
                    cfg.loop = ExtractBool(effectJson, "loop", false);
                    entry.particle = cfg;
                    break;
                }

                case VisualEffectType::Model: {
                    ModelConfig cfg;
                    cfg.modelPath = ExtractString(effectJson, "path");
                    cfg.animationName = ExtractString(effectJson, "animation");
                    cfg.opacity = ExtractFloat(effectJson, "opacity", 1.0f);
                    cfg.castShadows = ExtractBool(effectJson, "shadows", false);
                    entry.model = cfg;
                    break;
                }

                case VisualEffectType::Light: {
                    LightConfig cfg;
                    cfg.intensity = ExtractFloat(effectJson, "intensity", 1.0f);
                    cfg.range = ExtractFloat(effectJson, "range", 10.0f);
                    cfg.flicker = ExtractBool(effectJson, "flicker", false);
                    entry.light = cfg;
                    break;
                }

                case VisualEffectType::Beam: {
                    BeamConfig cfg;
                    cfg.texturePath = ExtractString(effectJson, "texture");
                    cfg.width = ExtractFloat(effectJson, "width", 0.5f);
                    cfg.branching = ExtractBool(effectJson, "branching", false);
                    entry.beam = cfg;
                    break;
                }

                case VisualEffectType::Decal: {
                    DecalConfig cfg;
                    cfg.texturePath = ExtractString(effectJson, "texture");
                    cfg.size = ExtractFloat(effectJson, "size", 2.0f);
                    cfg.duration = ExtractFloat(effectJson, "duration", 5.0f);
                    entry.decal = cfg;
                    break;
                }

                case VisualEffectType::ScreenEffect: {
                    ScreenEffectConfig cfg;
                    cfg.duration = ExtractFloat(effectJson, "duration", 0.5f);
                    cfg.intensity = ExtractFloat(effectJson, "intensity", 1.0f);
                    cfg.selfOnly = ExtractBool(effectJson, "self_only", false);
                    entry.screenEffect = cfg;
                    break;
                }

                case VisualEffectType::Trail: {
                    TrailConfig cfg;
                    cfg.texturePath = ExtractString(effectJson, "texture");
                    cfg.width = ExtractFloat(effectJson, "width", 0.5f);
                    cfg.duration = ExtractFloat(effectJson, "duration", 1.0f);
                    entry.trail = cfg;
                    break;
                }

                default:
                    break;
            }

            m_visualEffects.push_back(std::move(entry));
            pos = endPos;
        }
    }

    // Parse sounds array
    std::string soundsJson = ExtractArray(jsonString, "sounds");
    if (!soundsJson.empty()) {
        size_t pos = 0;
        while ((pos = soundsJson.find('{', pos)) != std::string::npos) {
            int braceCount = 1;
            size_t endPos = pos + 1;
            while (endPos < soundsJson.size() && braceCount > 0) {
                if (soundsJson[endPos] == '{') braceCount++;
                else if (soundsJson[endPos] == '}') braceCount--;
                endPos++;
            }

            std::string soundJson = soundsJson.substr(pos, endPos - pos);

            SoundEffectEntry entry;
            entry.id = ExtractString(soundJson, "id");
            entry.config.soundPath = ExtractString(soundJson, "path");
            entry.config.volume = ExtractFloat(soundJson, "volume", 1.0f);
            entry.config.pitch = ExtractFloat(soundJson, "pitch", 1.0f);
            entry.config.positional = ExtractBool(soundJson, "positional", true);
            entry.config.loop = ExtractBool(soundJson, "loop", false);
            entry.trigger = StringToTrigger(ExtractString(soundJson, "trigger"));

            m_soundEffects.push_back(std::move(entry));
            pos = endPos;
        }
    }

    // Parse animations array
    std::string animsJson = ExtractArray(jsonString, "animations");
    if (!animsJson.empty()) {
        size_t pos = 0;
        while ((pos = animsJson.find('{', pos)) != std::string::npos) {
            int braceCount = 1;
            size_t endPos = pos + 1;
            while (endPos < animsJson.size() && braceCount > 0) {
                if (animsJson[endPos] == '{') braceCount++;
                else if (animsJson[endPos] == '}') braceCount--;
                endPos++;
            }

            std::string animJson = animsJson.substr(pos, endPos - pos);

            AnimationEntry entry;
            entry.id = ExtractString(animJson, "id");
            entry.animationName = ExtractString(animJson, "name");
            entry.speed = ExtractFloat(animJson, "speed", 1.0f);
            entry.blendTime = ExtractFloat(animJson, "blend_time", 0.2f);
            entry.loop = ExtractBool(animJson, "loop", false);
            entry.trigger = StringToTrigger(ExtractString(animJson, "trigger"));

            m_animations.push_back(std::move(entry));
            pos = endPos;
        }
    }

    return true;
}

std::string SpellVisuals::ToJsonString() const {
    std::ostringstream json;
    json << "{\n";

    if (!m_iconPath.empty()) {
        json << "  \"icon\": \"" << m_iconPath << "\",\n";
    }

    if (!m_projectileModel.empty()) {
        json << "  \"projectile\": {\n";
        json << "    \"model\": \"" << m_projectileModel << "\",\n";
        if (!m_projectileTrail.empty()) {
            json << "    \"trail\": \"" << m_projectileTrail << "\",\n";
        }
        json << "    \"scale\": " << m_projectileScale << "\n";
        json << "  },\n";
    }

    if (!m_visualEffects.empty()) {
        json << "  \"effects\": [\n";
        for (size_t i = 0; i < m_visualEffects.size(); ++i) {
            const auto& effect = m_visualEffects[i];
            json << "    {\n";
            json << "      \"type\": \"" << VisualEffectTypeToString(effect.type) << "\",\n";
            json << "      \"trigger\": \"" << TriggerToString(effect.trigger) << "\",\n";
            json << "      \"attach\": \"" << AttachPointToString(effect.attachPoint) << "\"\n";
            json << "    }";
            if (i < m_visualEffects.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ],\n";
    }

    if (!m_soundEffects.empty()) {
        json << "  \"sounds\": [\n";
        for (size_t i = 0; i < m_soundEffects.size(); ++i) {
            const auto& sound = m_soundEffects[i];
            json << "    {\n";
            json << "      \"path\": \"" << sound.config.soundPath << "\",\n";
            json << "      \"trigger\": \"" << TriggerToString(sound.trigger) << "\",\n";
            json << "      \"volume\": " << sound.config.volume << "\n";
            json << "    }";
            if (i < m_soundEffects.size() - 1) json << ",";
            json << "\n";
        }
        json << "  ]\n";
    }

    json << "}";

    return json.str();
}

bool SpellVisuals::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    for (const auto& effect : m_visualEffects) {
        if (effect.type == VisualEffectType::Particle && effect.particle) {
            if (effect.particle->systemPath.empty()) {
                errors.push_back("Particle effect missing system path");
                valid = false;
            }
        }

        if (effect.type == VisualEffectType::Model && effect.model) {
            if (effect.model->modelPath.empty()) {
                errors.push_back("Model effect missing model path");
                valid = false;
            }
        }
    }

    for (const auto& sound : m_soundEffects) {
        if (sound.config.soundPath.empty()) {
            errors.push_back("Sound effect missing sound path");
            valid = false;
        }
    }

    return valid;
}

std::vector<const VisualEffectEntry*> SpellVisuals::GetEffectsForTrigger(
    VisualEffectEntry::Trigger trigger
) const {
    std::vector<const VisualEffectEntry*> results;

    for (const auto& effect : m_visualEffects) {
        if (effect.trigger == trigger) {
            results.push_back(&effect);
        }
    }

    return results;
}

std::vector<const SoundEffectEntry*> SpellVisuals::GetSoundsForTrigger(
    VisualEffectEntry::Trigger trigger
) const {
    std::vector<const SoundEffectEntry*> results;

    for (const auto& sound : m_soundEffects) {
        if (sound.trigger == trigger) {
            results.push_back(&sound);
        }
    }

    return results;
}

std::vector<const AnimationEntry*> SpellVisuals::GetAnimationsForTrigger(
    VisualEffectEntry::Trigger trigger
) const {
    std::vector<const AnimationEntry*> results;

    for (const auto& anim : m_animations) {
        if (anim.trigger == trigger) {
            results.push_back(&anim);
        }
    }

    return results;
}

} // namespace Spells
} // namespace Vehement
