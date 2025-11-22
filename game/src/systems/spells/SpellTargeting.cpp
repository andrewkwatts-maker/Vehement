#include "SpellTargeting.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <random>

namespace Vehement {
namespace Spells {

// ============================================================================
// String Conversion Helpers
// ============================================================================

const char* UnitTypeFilterToString(UnitTypeFilter filter) {
    switch (filter) {
        case UnitTypeFilter::Any: return "any";
        case UnitTypeFilter::Player: return "player";
        case UnitTypeFilter::NPC: return "npc";
        case UnitTypeFilter::Monster: return "monster";
        case UnitTypeFilter::Summon: return "summon";
        case UnitTypeFilter::Building: return "building";
        case UnitTypeFilter::Destructible: return "destructible";
        default: return "any";
    }
}

UnitTypeFilter StringToUnitTypeFilter(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "player") return UnitTypeFilter::Player;
    if (lower == "npc") return UnitTypeFilter::NPC;
    if (lower == "monster" || lower == "enemy") return UnitTypeFilter::Monster;
    if (lower == "summon" || lower == "pet") return UnitTypeFilter::Summon;
    if (lower == "building" || lower == "structure") return UnitTypeFilter::Building;
    if (lower == "destructible") return UnitTypeFilter::Destructible;

    return UnitTypeFilter::Any;
}

// ============================================================================
// Utility Functions
// ============================================================================

float GetHorizontalDistance(const glm::vec3& a, const glm::vec3& b) {
    float dx = b.x - a.x;
    float dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

float GetAngleBetween(const glm::vec3& dir1, const glm::vec3& dir2) {
    glm::vec3 d1 = glm::normalize(dir1);
    glm::vec3 d2 = glm::normalize(dir2);
    float dot = glm::dot(d1, d2);
    dot = glm::clamp(dot, -1.0f, 1.0f);
    return glm::degrees(std::acos(dot));
}

bool IsInRange(const glm::vec3& origin, const glm::vec3& target, float minRange, float maxRange) {
    float dist = GetHorizontalDistance(origin, target);
    return dist >= minRange && dist <= maxRange;
}

glm::vec3 GetDirection(const glm::vec3& origin, const glm::vec3& target) {
    glm::vec3 dir = target - origin;
    if (glm::length(dir) < 0.001f) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }
    return glm::normalize(dir);
}

// ============================================================================
// JSON Parsing Helpers (defined in SpellDefinition.cpp, declared here for use)
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

std::vector<std::string> ExtractStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return result;

    size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) return result;

    size_t bracketEnd = json.find(']', bracketStart);
    if (bracketEnd == std::string::npos) return result;

    std::string arrayStr = json.substr(bracketStart, bracketEnd - bracketStart + 1);

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

} // anonymous namespace

// ============================================================================
// SpellTargeting Implementation
// ============================================================================

bool SpellTargeting::LoadFromJson(const std::string& jsonString) {
    m_mode = StringToTargetingMode(ExtractString(jsonString, "mode"));
    m_range = ExtractFloat(jsonString, "range", 30.0f);
    m_minRange = ExtractFloat(jsonString, "min_range", 0.0f);
    m_radius = ExtractFloat(jsonString, "radius", 0.0f);
    m_angle = ExtractFloat(jsonString, "angle", 90.0f);
    m_width = ExtractFloat(jsonString, "width", 2.0f);
    m_maxTargets = ExtractInt(jsonString, "max_targets", 1);
    m_priority = StringToTargetPriority(ExtractString(jsonString, "priority"));

    // Parse filter
    std::string filterJson = ExtractObject(jsonString, "filter");
    if (!filterJson.empty()) {
        std::string factionStr = ExtractString(filterJson, "faction");
        if (factionStr == "enemy") m_filter.factionFilter = FactionFilter::Enemy;
        else if (factionStr == "friendly") m_filter.factionFilter = FactionFilter::Friendly;
        else if (factionStr == "self") m_filter.factionFilter = FactionFilter::Self;
        else if (factionStr == "neutral") m_filter.factionFilter = FactionFilter::Neutral;
        else m_filter.factionFilter = FactionFilter::All;

        m_filter.canTargetSelf = ExtractBool(filterJson, "can_target_self", true);
        m_filter.mustBeAlive = ExtractBool(filterJson, "must_be_alive", true);
        m_filter.canTargetDead = ExtractBool(filterJson, "can_target_dead", false);
        m_filter.canTargetInvisible = ExtractBool(filterJson, "can_target_invisible", false);
        m_filter.canTargetInvulnerable = ExtractBool(filterJson, "can_target_invulnerable", false);
        m_filter.mustBeInCombat = ExtractBool(filterJson, "in_combat", false);
        m_filter.mustBeOutOfCombat = ExtractBool(filterJson, "out_of_combat", false);
        m_filter.requiredBuffs = ExtractStringArray(filterJson, "has_buff");
        m_filter.excludedBuffs = ExtractStringArray(filterJson, "missing_buff");
        m_filter.minHealthPercent = ExtractFloat(filterJson, "min_health_percent", 0.0f);
        m_filter.maxHealthPercent = ExtractFloat(filterJson, "max_health_percent", 100.0f);
        m_filter.customFilterScript = ExtractString(filterJson, "custom_script");

        // Parse unit types
        auto allowedTypes = ExtractStringArray(filterJson, "unit_type");
        for (const auto& type : allowedTypes) {
            m_filter.allowedTypes.push_back(StringToUnitTypeFilter(type));
        }
    }

    // Parse projectile config
    std::string projectileJson = ExtractObject(jsonString, "projectile");
    if (!projectileJson.empty()) {
        m_projectile.speed = ExtractFloat(projectileJson, "speed", 20.0f);
        m_projectile.acceleration = ExtractFloat(projectileJson, "acceleration", 0.0f);
        m_projectile.maxSpeed = ExtractFloat(projectileJson, "max_speed", 100.0f);
        m_projectile.turnRate = ExtractFloat(projectileJson, "turn_rate", 0.0f);
        m_projectile.gravity = ExtractFloat(projectileJson, "gravity", 0.0f);
        m_projectile.radius = ExtractFloat(projectileJson, "radius", 0.5f);
        m_projectile.piercing = ExtractBool(projectileJson, "piercing", false);
        m_projectile.maxPierceCount = ExtractInt(projectileJson, "max_pierce", 1);
        m_projectile.pierceDamageFalloff = ExtractFloat(projectileJson, "pierce_falloff", 0.2f);
        m_projectile.homingEnabled = ExtractBool(projectileJson, "homing", false);
        m_projectile.homingAcquireRange = ExtractFloat(projectileJson, "homing_range", 5.0f);
        m_projectile.explodeOnImpact = ExtractBool(projectileJson, "explode", false);
        m_projectile.explosionRadius = ExtractFloat(projectileJson, "explosion_radius", 0.0f);
        m_projectile.maxLifetime = ExtractFloat(projectileJson, "lifetime", 10.0f);
        m_projectile.maxRange = ExtractFloat(projectileJson, "max_range", 100.0f);
        m_projectile.modelPath = ExtractString(projectileJson, "model");
        m_projectile.trailEffect = ExtractString(projectileJson, "trail");
    }

    // Parse chain config
    std::string chainJson = ExtractObject(jsonString, "chain");
    if (!chainJson.empty()) {
        m_chain.maxBounces = ExtractInt(chainJson, "max_bounces", 3);
        m_chain.bounceRange = ExtractFloat(chainJson, "bounce_range", 10.0f);
        m_chain.damagePerBounce = ExtractFloat(chainJson, "damage_per_bounce", 0.0f);
        m_chain.damageMultiplierPerBounce = ExtractFloat(chainJson, "damage_multiplier", 0.9f);
        m_chain.bounceDelay = ExtractFloat(chainJson, "bounce_delay", 0.1f);
        m_chain.canHitSameTarget = ExtractBool(chainJson, "can_hit_same", false);
        m_chain.requiresLoS = ExtractBool(chainJson, "requires_los", true);
        m_chain.bouncePriority = StringToTargetPriority(ExtractString(chainJson, "priority"));
    }

    // Parse ground target config
    std::string groundJson = ExtractObject(jsonString, "ground_target");
    if (!groundJson.empty()) {
        m_groundTarget.enabled = ExtractBool(groundJson, "enabled", true);
        m_groundTarget.snapToTerrain = ExtractBool(groundJson, "snap_to_terrain", true);
        m_groundTarget.requiresWalkable = ExtractBool(groundJson, "requires_walkable", false);
        m_groundTarget.showGroundIndicator = ExtractBool(groundJson, "show_indicator", true);
        m_groundTarget.indicatorRadius = ExtractFloat(groundJson, "indicator_radius", 1.0f);
        m_groundTarget.maxHeightDifference = ExtractFloat(groundJson, "max_height_diff", 10.0f);
    }

    // Parse preview config
    std::string previewJson = ExtractObject(jsonString, "preview");
    if (!previewJson.empty()) {
        std::string shapeStr = ExtractString(previewJson, "shape");
        if (shapeStr == "circle") m_preview.shape = TargetingPreview::Shape::Circle;
        else if (shapeStr == "rectangle") m_preview.shape = TargetingPreview::Shape::Rectangle;
        else if (shapeStr == "cone") m_preview.shape = TargetingPreview::Shape::Cone;
        else if (shapeStr == "ring") m_preview.shape = TargetingPreview::Shape::Ring;
        else if (shapeStr == "arrow") m_preview.shape = TargetingPreview::Shape::Arrow;

        m_preview.showRange = ExtractBool(previewJson, "show_range", true);
        m_preview.showAOE = ExtractBool(previewJson, "show_aoe", true);
        m_preview.showTargets = ExtractBool(previewJson, "show_targets", true);
        m_preview.pulseAnimation = ExtractBool(previewJson, "pulse", true);
        m_preview.pulseSpeed = ExtractFloat(previewJson, "pulse_speed", 2.0f);
    }

    return true;
}

std::string SpellTargeting::ToJsonString() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"mode\": \"" << TargetingModeToString(m_mode) << "\",\n";
    json << "  \"range\": " << m_range << ",\n";
    json << "  \"min_range\": " << m_minRange << ",\n";
    json << "  \"radius\": " << m_radius << ",\n";
    json << "  \"angle\": " << m_angle << ",\n";
    json << "  \"width\": " << m_width << ",\n";
    json << "  \"max_targets\": " << m_maxTargets << ",\n";
    json << "  \"priority\": \"" << TargetPriorityToString(m_priority) << "\"\n";
    json << "}";
    return json.str();
}

bool SpellTargeting::Validate(std::vector<std::string>& errors) const {
    bool valid = true;

    if (m_range < 0) {
        errors.push_back("Targeting range cannot be negative");
        valid = false;
    }

    if (m_minRange > m_range) {
        errors.push_back("Minimum range cannot exceed maximum range");
        valid = false;
    }

    if (m_radius < 0) {
        errors.push_back("Targeting radius cannot be negative");
        valid = false;
    }

    if (m_angle <= 0 || m_angle > 360) {
        errors.push_back("Cone angle must be between 0 and 360 degrees");
        valid = false;
    }

    if (m_maxTargets < 1) {
        errors.push_back("Max targets must be at least 1");
        valid = false;
    }

    if (m_mode == TargetingMode::Projectile && m_projectile.speed <= 0) {
        errors.push_back("Projectile speed must be positive");
        valid = false;
    }

    if (m_mode == TargetingMode::Chain && m_chain.maxBounces < 1) {
        errors.push_back("Chain bounces must be at least 1");
        valid = false;
    }

    return valid;
}

std::vector<uint32_t> SpellTargeting::FindTargets(
    const SpellInstance& instance,
    const glm::vec3& casterPosition,
    const glm::vec3& targetPosition,
    const glm::vec3& targetDirection,
    EntityQueryFunc entityQuery
) const {
    std::vector<uint32_t> targets;

    switch (m_mode) {
        case TargetingMode::Self:
            targets.push_back(instance.GetCasterId());
            break;

        case TargetingMode::Single:
            if (instance.GetTargetId() != 0) {
                targets.push_back(instance.GetTargetId());
            }
            break;

        case TargetingMode::PassiveRadius:
        case TargetingMode::AOE:
            {
                glm::vec3 center = (m_mode == TargetingMode::PassiveRadius) ?
                    casterPosition : targetPosition;

                auto candidates = entityQuery(center, m_radius);
                for (uint32_t id : candidates) {
                    targets.push_back(id);
                    if (static_cast<int>(targets.size()) >= m_maxTargets) break;
                }
            }
            break;

        case TargetingMode::Line:
            {
                // Get all entities in a box along the line
                glm::vec3 lineEnd = casterPosition + targetDirection * m_range;
                float searchRadius = m_range / 2.0f;
                glm::vec3 center = (casterPosition + lineEnd) * 0.5f;

                auto candidates = entityQuery(center, searchRadius);
                for (uint32_t id : candidates) {
                    targets.push_back(id);
                    if (static_cast<int>(targets.size()) >= m_maxTargets) break;
                }
            }
            break;

        case TargetingMode::Cone:
            {
                auto candidates = entityQuery(casterPosition, m_range);
                for (uint32_t id : candidates) {
                    targets.push_back(id);
                    if (static_cast<int>(targets.size()) >= m_maxTargets) break;
                }
            }
            break;

        case TargetingMode::Projectile:
            // Projectile targeting is handled differently - initial target
            if (instance.GetTargetId() != 0) {
                targets.push_back(instance.GetTargetId());
            }
            break;

        case TargetingMode::Chain:
            // Initial target for chain
            if (instance.GetTargetId() != 0) {
                targets.push_back(instance.GetTargetId());
            }
            break;
    }

    return targets;
}

bool SpellTargeting::IsValidTarget(
    uint32_t entityId,
    uint32_t casterId,
    EntityValidationFunc validateFunc
) const {
    if (!m_filter.canTargetSelf && entityId == casterId) {
        return false;
    }

    return validateFunc(entityId, m_filter);
}

TargetingPreview SpellTargeting::GetPreviewData(
    const glm::vec3& casterPosition,
    const glm::vec3& targetPosition,
    const glm::vec3& targetDirection
) const {
    TargetingPreview preview = m_preview;

    // Auto-determine shape if not set
    if (preview.shape == TargetingPreview::Shape::None) {
        switch (m_mode) {
            case TargetingMode::AOE:
            case TargetingMode::PassiveRadius:
                preview.shape = TargetingPreview::Shape::Circle;
                break;
            case TargetingMode::Line:
                preview.shape = TargetingPreview::Shape::Rectangle;
                break;
            case TargetingMode::Cone:
                preview.shape = TargetingPreview::Shape::Cone;
                break;
            case TargetingMode::Projectile:
            case TargetingMode::Single:
                preview.shape = TargetingPreview::Shape::Arrow;
                break;
            default:
                break;
        }
    }

    return preview;
}

std::vector<uint32_t> SpellTargeting::GetAOETargets(
    const glm::vec3& center,
    float radius,
    EntityQueryFunc entityQuery,
    EntityValidationFunc validateFunc
) const {
    std::vector<uint32_t> targets;
    auto candidates = entityQuery(center, radius);

    for (uint32_t id : candidates) {
        if (validateFunc(id, m_filter)) {
            targets.push_back(id);
            if (static_cast<int>(targets.size()) >= m_maxTargets) break;
        }
    }

    return targets;
}

std::vector<uint32_t> SpellTargeting::GetLineTargets(
    const glm::vec3& start,
    const glm::vec3& end,
    float width,
    EntityQueryFunc entityQuery,
    EntityValidationFunc validateFunc
) const {
    std::vector<uint32_t> targets;

    // Query in cylinder along line
    float length = glm::length(end - start);
    glm::vec3 center = (start + end) * 0.5f;
    float searchRadius = length / 2.0f + width;

    auto candidates = entityQuery(center, searchRadius);

    for (uint32_t id : candidates) {
        if (validateFunc(id, m_filter)) {
            targets.push_back(id);
            if (static_cast<int>(targets.size()) >= m_maxTargets) break;
        }
    }

    return targets;
}

std::vector<uint32_t> SpellTargeting::GetConeTargets(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float range,
    float angle,
    EntityQueryFunc entityQuery,
    EntityValidationFunc validateFunc
) const {
    std::vector<uint32_t> targets;
    auto candidates = entityQuery(origin, range);

    for (uint32_t id : candidates) {
        if (validateFunc(id, m_filter)) {
            targets.push_back(id);
            if (static_cast<int>(targets.size()) >= m_maxTargets) break;
        }
    }

    return targets;
}

std::optional<uint32_t> SpellTargeting::GetNextChainTarget(
    const glm::vec3& currentPosition,
    const std::vector<uint32_t>& alreadyHit,
    EntityQueryFunc entityQuery,
    EntityValidationFunc validateFunc
) const {
    auto candidates = entityQuery(currentPosition, m_chain.bounceRange);

    for (uint32_t id : candidates) {
        // Skip already hit targets (unless allowed)
        if (!m_chain.canHitSameTarget) {
            if (std::find(alreadyHit.begin(), alreadyHit.end(), id) != alreadyHit.end()) {
                continue;
            }
        }

        if (validateFunc(id, m_filter)) {
            return id;
        }
    }

    return std::nullopt;
}

void SpellTargeting::SortTargetsByPriority(
    std::vector<uint32_t>& targets,
    const glm::vec3& referencePoint,
    std::function<glm::vec3(uint32_t)> getPosition,
    std::function<float(uint32_t)> getHealth,
    std::function<float(uint32_t)> getThreat
) const {
    switch (m_priority) {
        case TargetPriority::Nearest:
            std::sort(targets.begin(), targets.end(), [&](uint32_t a, uint32_t b) {
                float distA = glm::length(getPosition(a) - referencePoint);
                float distB = glm::length(getPosition(b) - referencePoint);
                return distA < distB;
            });
            break;

        case TargetPriority::Farthest:
            std::sort(targets.begin(), targets.end(), [&](uint32_t a, uint32_t b) {
                float distA = glm::length(getPosition(a) - referencePoint);
                float distB = glm::length(getPosition(b) - referencePoint);
                return distA > distB;
            });
            break;

        case TargetPriority::LowestHealth:
            std::sort(targets.begin(), targets.end(), [&](uint32_t a, uint32_t b) {
                return getHealth(a) < getHealth(b);
            });
            break;

        case TargetPriority::HighestHealth:
            std::sort(targets.begin(), targets.end(), [&](uint32_t a, uint32_t b) {
                return getHealth(a) > getHealth(b);
            });
            break;

        case TargetPriority::HighestThreat:
            std::sort(targets.begin(), targets.end(), [&](uint32_t a, uint32_t b) {
                return getThreat(a) > getThreat(b);
            });
            break;

        case TargetPriority::Random:
            {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::shuffle(targets.begin(), targets.end(), gen);
            }
            break;
    }
}

bool SpellTargeting::IsInCone(
    const glm::vec3& origin,
    const glm::vec3& direction,
    const glm::vec3& point,
    float range,
    float angle
) const {
    glm::vec3 toPoint = point - origin;
    float dist = glm::length(toPoint);

    if (dist > range) return false;
    if (dist < 0.001f) return true;

    glm::vec3 normalizedToPoint = toPoint / dist;
    glm::vec3 normalizedDir = glm::normalize(direction);

    float dot = glm::dot(normalizedDir, normalizedToPoint);
    float halfAngleRad = glm::radians(angle / 2.0f);

    return dot >= std::cos(halfAngleRad);
}

bool SpellTargeting::IsInLine(
    const glm::vec3& start,
    const glm::vec3& end,
    const glm::vec3& point,
    float width
) const {
    glm::vec3 line = end - start;
    float lineLength = glm::length(line);
    if (lineLength < 0.001f) return false;

    glm::vec3 lineDir = line / lineLength;
    glm::vec3 toPoint = point - start;

    float projection = glm::dot(toPoint, lineDir);
    if (projection < 0 || projection > lineLength) return false;

    glm::vec3 closestPoint = start + lineDir * projection;
    float distance = glm::length(point - closestPoint);

    return distance <= width / 2.0f;
}

} // namespace Spells
} // namespace Vehement
