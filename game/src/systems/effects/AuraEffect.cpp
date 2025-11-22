#include "AuraEffect.hpp"
#include "EffectManager.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <regex>

namespace Vehement {
namespace Effects {

// ============================================================================
// Type Conversion
// ============================================================================

static const std::unordered_map<std::string, AuraShape> s_auraShapeFromString = {
    {"circle", AuraShape::Circle},
    {"sphere", AuraShape::Circle},
    {"cone", AuraShape::Cone},
    {"rectangle", AuraShape::Rectangle},
    {"rect", AuraShape::Rectangle},
    {"ring", AuraShape::Ring},
    {"line", AuraShape::Line}
};

const char* AuraShapeToString(AuraShape shape) {
    switch (shape) {
        case AuraShape::Circle:    return "circle";
        case AuraShape::Cone:      return "cone";
        case AuraShape::Rectangle: return "rectangle";
        case AuraShape::Ring:      return "ring";
        case AuraShape::Line:      return "line";
    }
    return "circle";
}

std::optional<AuraShape> AuraShapeFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_auraShapeFromString.find(lower);
    if (it != s_auraShapeFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

static const std::unordered_map<std::string, AuraTargetFilter> s_auraTargetFromString = {
    {"allies", AuraTargetFilter::Allies},
    {"friendly", AuraTargetFilter::Allies},
    {"enemies", AuraTargetFilter::Enemies},
    {"hostile", AuraTargetFilter::Enemies},
    {"both", AuraTargetFilter::Both},
    {"all", AuraTargetFilter::Both},
    {"self_only", AuraTargetFilter::SelfOnly},
    {"self", AuraTargetFilter::SelfOnly},
    {"allies_no_self", AuraTargetFilter::AlliesNoSelf},
    {"enemies_no_self", AuraTargetFilter::EnemiesNoSelf}
};

const char* AuraTargetFilterToString(AuraTargetFilter filter) {
    switch (filter) {
        case AuraTargetFilter::Allies:       return "allies";
        case AuraTargetFilter::Enemies:      return "enemies";
        case AuraTargetFilter::Both:         return "both";
        case AuraTargetFilter::SelfOnly:     return "self_only";
        case AuraTargetFilter::AlliesNoSelf: return "allies_no_self";
        case AuraTargetFilter::EnemiesNoSelf: return "enemies_no_self";
    }
    return "allies";
}

std::optional<AuraTargetFilter> AuraTargetFilterFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = s_auraTargetFromString.find(lower);
    if (it != s_auraTargetFromString.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// JSON Helpers
// ============================================================================

namespace {

std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str();
    }
    return "";
}

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

bool ExtractJsonBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(json, match, pattern)) {
        return match[1].str() == "true";
    }
    return defaultVal;
}

} // anonymous namespace

// ============================================================================
// Aura Config
// ============================================================================

bool AuraConfig::LoadFromJson(const std::string& jsonStr) {
    std::string shapeStr = ExtractJsonString(jsonStr, "shape");
    if (auto s = AuraShapeFromString(shapeStr)) {
        shape = *s;
    }

    radius = ExtractJsonNumber(jsonStr, "radius", 10.0f);
    innerRadius = ExtractJsonNumber(jsonStr, "inner_radius", 0.0f);
    coneAngle = ExtractJsonNumber(jsonStr, "cone_angle", 60.0f);
    width = ExtractJsonNumber(jsonStr, "width", 5.0f);
    length = ExtractJsonNumber(jsonStr, "length", 10.0f);

    std::string targetStr = ExtractJsonString(jsonStr, "affects");
    if (targetStr.empty()) {
        targetStr = ExtractJsonString(jsonStr, "target_filter");
    }
    if (auto t = AuraTargetFilterFromString(targetStr)) {
        targetFilter = *t;
    }

    maxTargets = ExtractJsonInt(jsonStr, "max_targets", -1);
    pulseInterval = ExtractJsonNumber(jsonStr, "pulse_interval", 1.0f);
    pulseOnEnter = ExtractJsonBool(jsonStr, "pulse_on_enter", true);
    removeOnExit = ExtractJsonBool(jsonStr, "remove_on_exit", true);

    applyEffectId = ExtractJsonString(jsonStr, "apply_effect");
    followsSource = ExtractJsonBool(jsonStr, "follows_source", true);

    visualEffect = ExtractJsonString(jsonStr, "visual_effect");
    showRange = ExtractJsonBool(jsonStr, "show_range", false);

    return true;
}

std::string AuraConfig::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"shape\":\"" << AuraShapeToString(shape) << "\"";
    ss << ",\"radius\":" << radius;

    if (innerRadius > 0.0f) {
        ss << ",\"inner_radius\":" << innerRadius;
    }

    if (shape == AuraShape::Cone) {
        ss << ",\"cone_angle\":" << coneAngle;
    }

    if (shape == AuraShape::Rectangle || shape == AuraShape::Line) {
        ss << ",\"width\":" << width;
        ss << ",\"length\":" << length;
    }

    ss << ",\"affects\":\"" << AuraTargetFilterToString(targetFilter) << "\"";

    if (maxTargets >= 0) {
        ss << ",\"max_targets\":" << maxTargets;
    }

    ss << ",\"pulse_interval\":" << pulseInterval;
    ss << ",\"pulse_on_enter\":" << (pulseOnEnter ? "true" : "false");
    ss << ",\"remove_on_exit\":" << (removeOnExit ? "true" : "false");

    if (!applyEffectId.empty()) {
        ss << ",\"apply_effect\":\"" << applyEffectId << "\"";
    }

    ss << ",\"follows_source\":" << (followsSource ? "true" : "false");

    if (!visualEffect.empty()) {
        ss << ",\"visual_effect\":\"" << visualEffect << "\"";
    }

    if (showRange) {
        ss << ",\"show_range\":true";
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// Aura Instance
// ============================================================================

AuraInstance::AuraId AuraInstance::s_nextAuraId = 1;

AuraInstance::AuraInstance() = default;

AuraInstance::AuraInstance(const EffectDefinition* definition)
    : m_definition(definition) {
    m_auraId = s_nextAuraId++;
}

AuraInstance::~AuraInstance() = default;

void AuraInstance::Initialize(const EffectDefinition* definition, const AuraConfig& config) {
    m_definition = definition;
    m_config = config;
    m_auraId = s_nextAuraId++;
}

void AuraInstance::Activate(uint32_t sourceId, const glm::vec3& position) {
    m_sourceId = sourceId;
    m_position = position;
    m_active = true;
    m_pulseTimer = 0.0f;
    m_entitiesInRange.clear();
    m_enteredThisFrame.clear();
    m_exitedThisFrame.clear();
}

void AuraInstance::Deactivate() {
    m_active = false;

    // Notify all entities that they left
    for (uint32_t entityId : m_entitiesInRange) {
        if (m_onEntityExit) {
            m_onEntityExit(entityId, this);
        }
    }

    m_entitiesInRange.clear();
}

void AuraInstance::Update(float deltaTime, const glm::vec3& sourcePosition) {
    if (!m_active) return;

    // Update position if following source
    if (m_config.followsSource) {
        m_position = sourcePosition + m_config.offset;
    }

    // Update pulse timer
    m_pulseTimer += deltaTime;
}

bool AuraInstance::IsPulseReady() const {
    return m_pulseTimer >= m_config.pulseInterval;
}

void AuraInstance::ConsumePulse() {
    m_pulseTimer -= m_config.pulseInterval;
    if (m_pulseTimer < 0.0f) {
        m_pulseTimer = 0.0f;
    }
}

bool AuraInstance::IsInRange(const glm::vec3& entityPosition, float entityRadius) const {
    switch (m_config.shape) {
        case AuraShape::Circle:
            return CheckCircleIntersection(entityPosition, entityRadius);
        case AuraShape::Cone:
            return CheckConeIntersection(entityPosition, entityRadius);
        case AuraShape::Rectangle:
            return CheckRectangleIntersection(entityPosition, entityRadius);
        case AuraShape::Ring:
            return CheckRingIntersection(entityPosition, entityRadius);
        case AuraShape::Line:
            return CheckLineIntersection(entityPosition, entityRadius);
    }
    return false;
}

void AuraInstance::UpdateEntityPresence(uint32_t entityId, bool inRange) {
    bool wasInRange = m_entitiesInRange.find(entityId) != m_entitiesInRange.end();

    if (inRange && !wasInRange) {
        // Entity entered
        m_entitiesInRange.insert(entityId);
        m_enteredThisFrame.push_back(entityId);

        if (m_onEntityEnter) {
            m_onEntityEnter(entityId, this);
        }
    } else if (!inRange && wasInRange) {
        // Entity exited
        m_entitiesInRange.erase(entityId);
        m_exitedThisFrame.push_back(entityId);

        if (m_onEntityExit) {
            m_onEntityExit(entityId, this);
        }
    }
}

void AuraInstance::ClearFrameTracking() {
    m_enteredThisFrame.clear();
    m_exitedThisFrame.clear();
}

// -------------------------------------------------------------------------
// Shape Intersection Tests
// -------------------------------------------------------------------------

bool AuraInstance::CheckCircleIntersection(const glm::vec3& entityPos, float entityRadius) const {
    // 2D distance check (XZ plane for top-down)
    float dx = entityPos.x - m_position.x;
    float dz = entityPos.z - m_position.z;
    float distSq = dx * dx + dz * dz;
    float range = m_config.radius + entityRadius;
    return distSq <= range * range;
}

bool AuraInstance::CheckConeIntersection(const glm::vec3& entityPos, float entityRadius) const {
    // Vector to entity
    glm::vec3 toEntity = entityPos - m_position;
    toEntity.y = 0.0f;  // 2D

    float dist = glm::length(toEntity);
    if (dist > m_config.radius + entityRadius) {
        return false;
    }

    if (dist < 0.001f) {
        return true;  // At center
    }

    // Check angle
    glm::vec3 normalizedToEntity = toEntity / dist;
    glm::vec3 facingXZ = glm::vec3(m_facingDirection.x, 0.0f, m_facingDirection.z);
    if (glm::length(facingXZ) > 0.001f) {
        facingXZ = glm::normalize(facingXZ);
    } else {
        facingXZ = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    float dot = glm::dot(normalizedToEntity, facingXZ);
    float halfAngleRad = glm::radians(m_config.coneAngle * 0.5f);

    return dot >= std::cos(halfAngleRad);
}

bool AuraInstance::CheckRectangleIntersection(const glm::vec3& entityPos, float entityRadius) const {
    // Transform to local space (facing = forward)
    glm::vec3 toEntity = entityPos - m_position;
    toEntity.y = 0.0f;

    // Rotate to local space
    glm::vec3 facingXZ = glm::vec3(m_facingDirection.x, 0.0f, m_facingDirection.z);
    if (glm::length(facingXZ) < 0.001f) {
        facingXZ = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        facingXZ = glm::normalize(facingXZ);
    }

    glm::vec3 right = glm::vec3(facingXZ.z, 0.0f, -facingXZ.x);

    float localX = glm::dot(toEntity, right);
    float localZ = glm::dot(toEntity, facingXZ);

    float halfWidth = m_config.width * 0.5f + entityRadius;
    float halfLength = m_config.length * 0.5f + entityRadius;

    return std::abs(localX) <= halfWidth && localZ >= -entityRadius && localZ <= m_config.length + entityRadius;
}

bool AuraInstance::CheckRingIntersection(const glm::vec3& entityPos, float entityRadius) const {
    float dx = entityPos.x - m_position.x;
    float dz = entityPos.z - m_position.z;
    float dist = std::sqrt(dx * dx + dz * dz);

    float outerDist = m_config.radius + entityRadius;
    float innerDist = m_config.innerRadius - entityRadius;

    return dist <= outerDist && dist >= innerDist;
}

bool AuraInstance::CheckLineIntersection(const glm::vec3& entityPos, float entityRadius) const {
    // Line segment from position along facing direction
    glm::vec3 facingXZ = glm::vec3(m_facingDirection.x, 0.0f, m_facingDirection.z);
    if (glm::length(facingXZ) < 0.001f) {
        facingXZ = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        facingXZ = glm::normalize(facingXZ);
    }

    glm::vec3 lineEnd = m_position + facingXZ * m_config.length;

    // Point to line segment distance
    glm::vec3 toEntity = glm::vec3(entityPos.x - m_position.x, 0.0f, entityPos.z - m_position.z);
    glm::vec3 lineVec = glm::vec3(lineEnd.x - m_position.x, 0.0f, lineEnd.z - m_position.z);

    float lineLen = glm::length(lineVec);
    if (lineLen < 0.001f) {
        return glm::length(toEntity) <= entityRadius;
    }

    float t = glm::clamp(glm::dot(toEntity, lineVec) / (lineLen * lineLen), 0.0f, 1.0f);
    glm::vec3 closest = lineVec * t;
    glm::vec3 diff = toEntity - closest;

    float dist = glm::length(diff);
    return dist <= m_config.width * 0.5f + entityRadius;
}

// ============================================================================
// Aura Manager
// ============================================================================

AuraManager::AuraManager() = default;

AuraManager::~AuraManager() = default;

AuraInstance* AuraManager::CreateAura(
    const EffectDefinition* definition,
    uint32_t sourceId,
    const glm::vec3& position
) {
    if (!definition || definition->GetType() != EffectType::Aura) {
        return nullptr;
    }

    auto aura = std::make_unique<AuraInstance>(definition);

    // Extract aura config from definition
    // In a real implementation, this would parse the aura config from the definition
    AuraConfig config;
    // config would be loaded from definition

    aura->Initialize(definition, config);
    aura->Activate(sourceId, position);

    AuraInstance* auraPtr = aura.get();
    m_auras.push_back(std::move(aura));

    return auraPtr;
}

AuraInstance* AuraManager::CreateAura(
    const AuraConfig& config,
    uint32_t sourceId,
    const glm::vec3& position
) {
    auto aura = std::make_unique<AuraInstance>();
    aura->Initialize(nullptr, config);
    aura->Activate(sourceId, position);

    AuraInstance* auraPtr = aura.get();
    m_auras.push_back(std::move(aura));

    return auraPtr;
}

bool AuraManager::RemoveAura(AuraInstance::AuraId auraId) {
    auto it = std::find_if(m_auras.begin(), m_auras.end(),
        [auraId](const auto& aura) {
            return aura->GetId() == auraId;
        });

    if (it == m_auras.end()) {
        return false;
    }

    (*it)->Deactivate();
    m_auras.erase(it);
    return true;
}

int AuraManager::RemoveAurasBySource(uint32_t sourceId) {
    int removed = 0;
    auto it = m_auras.begin();
    while (it != m_auras.end()) {
        if ((*it)->GetSourceId() == sourceId) {
            (*it)->Deactivate();
            it = m_auras.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    return removed;
}

void AuraManager::Update(
    float deltaTime,
    const std::unordered_map<uint32_t, glm::vec3>& entityPositions,
    const std::unordered_map<uint32_t, int>& entityFactions
) {
    for (const auto& aura : m_auras) {
        if (!aura->IsActive()) continue;

        // Get source position for following
        glm::vec3 sourcePos = aura->GetPosition();
        auto sourceIt = entityPositions.find(aura->GetSourceId());
        if (sourceIt != entityPositions.end()) {
            sourcePos = sourceIt->second;
        }

        // Clear frame tracking
        aura->ClearFrameTracking();

        // Update aura
        aura->Update(deltaTime, sourcePos);

        // Process targets
        ProcessAuraTargets(aura.get(), entityPositions, entityFactions);

        // Handle pulses
        if (aura->IsPulseReady()) {
            aura->ConsumePulse();

            // Apply effects to all entities in range
            for (uint32_t entityId : aura->GetEntitiesInRange()) {
                if (m_onApplyEffect && !aura->GetConfig().applyEffectId.empty()) {
                    m_onApplyEffect(aura.get(), entityId, aura->GetConfig().applyEffectId);
                }
            }
        }

        // Handle new entries (apply effect immediately if configured)
        if (aura->GetConfig().pulseOnEnter) {
            for (uint32_t entityId : aura->GetNewEntities()) {
                if (m_onApplyEffect && !aura->GetConfig().applyEffectId.empty()) {
                    m_onApplyEffect(aura.get(), entityId, aura->GetConfig().applyEffectId);
                }
            }
        }

        // Handle exits (remove effect if configured)
        if (aura->GetConfig().removeOnExit) {
            for (uint32_t entityId : aura->GetExitedEntities()) {
                if (m_onRemoveEffect && !aura->GetConfig().applyEffectId.empty()) {
                    m_onRemoveEffect(aura.get(), entityId, aura->GetConfig().applyEffectId);
                }
            }
        }
    }
}

void AuraManager::ProcessAuraTargets(
    AuraInstance* aura,
    const std::unordered_map<uint32_t, glm::vec3>& entityPositions,
    const std::unordered_map<uint32_t, int>& entityFactions
) {
    int sourceFaction = 0;
    auto sourceFactIt = entityFactions.find(aura->GetSourceId());
    if (sourceFactIt != entityFactions.end()) {
        sourceFaction = sourceFactIt->second;
    }

    for (const auto& [entityId, position] : entityPositions) {
        // Check target filter
        int targetFaction = 0;
        auto targetFactIt = entityFactions.find(entityId);
        if (targetFactIt != entityFactions.end()) {
            targetFaction = targetFactIt->second;
        }

        if (!PassesTargetFilter(aura->GetConfig().targetFilter,
                                aura->GetSourceId(), entityId,
                                sourceFaction, targetFaction)) {
            aura->UpdateEntityPresence(entityId, false);
            continue;
        }

        // Check range
        bool inRange = aura->IsInRange(position, 0.5f);  // Default entity radius
        aura->UpdateEntityPresence(entityId, inRange);
    }
}

bool AuraManager::PassesTargetFilter(
    AuraTargetFilter filter,
    uint32_t sourceId,
    uint32_t targetId,
    int sourceFaction,
    int targetFaction
) const {
    bool isSelf = (sourceId == targetId);
    bool isAlly = (sourceFaction == targetFaction);

    switch (filter) {
        case AuraTargetFilter::Allies:
            return isAlly;
        case AuraTargetFilter::Enemies:
            return !isAlly;
        case AuraTargetFilter::Both:
            return true;
        case AuraTargetFilter::SelfOnly:
            return isSelf;
        case AuraTargetFilter::AlliesNoSelf:
            return isAlly && !isSelf;
        case AuraTargetFilter::EnemiesNoSelf:
            return !isAlly && !isSelf;
    }
    return false;
}

std::vector<AuraInstance*> AuraManager::GetAurasAffecting(uint32_t entityId) const {
    std::vector<AuraInstance*> result;
    for (const auto& aura : m_auras) {
        if (aura->GetEntitiesInRange().find(entityId) != aura->GetEntitiesInRange().end()) {
            result.push_back(aura.get());
        }
    }
    return result;
}

std::vector<AuraInstance*> AuraManager::GetAurasFromSource(uint32_t sourceId) const {
    std::vector<AuraInstance*> result;
    for (const auto& aura : m_auras) {
        if (aura->GetSourceId() == sourceId) {
            result.push_back(aura.get());
        }
    }
    return result;
}

bool AuraManager::IsInAnyAura(uint32_t entityId) const {
    for (const auto& aura : m_auras) {
        if (aura->GetEntitiesInRange().find(entityId) != aura->GetEntitiesInRange().end()) {
            return true;
        }
    }
    return false;
}

AuraInstance* AuraManager::GetAura(AuraInstance::AuraId auraId) const {
    for (const auto& aura : m_auras) {
        if (aura->GetId() == auraId) {
            return aura.get();
        }
    }
    return nullptr;
}

} // namespace Effects
} // namespace Vehement
