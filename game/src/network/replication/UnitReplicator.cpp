#include "UnitReplicator.hpp"
#include "ReplicationManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Network {
namespace Replication {

UnitReplicator& UnitReplicator::getInstance() {
    static UnitReplicator instance;
    return instance;
}

UnitReplicator::UnitReplicator() {}

UnitReplicator::~UnitReplicator() {
    shutdown();
}

bool UnitReplicator::initialize() {
    if (m_initialized) return true;
    m_initialized = true;
    return true;
}

void UnitReplicator::shutdown() {
    if (!m_initialized) return;

    m_unitData.clear();
    m_smoothing.clear();
    m_formations.clear();

    m_initialized = false;
}

void UnitReplicator::update(float deltaTime) {
    if (!m_initialized) return;

    updateMovementSmoothing(deltaTime);
    updateFormations(deltaTime);

    // Process replication at fixed rate
    m_replicationTimer += deltaTime;
    float replicationInterval = 1.0f / m_replicationRate;

    if (m_replicationTimer >= replicationInterval) {
        m_replicationTimer -= replicationInterval;
        processReplicationQueue();
    }
}

void UnitReplicator::registerUnit(uint64_t networkId, const std::string& unitType) {
    UnitReplicationData data;
    data.networkId = networkId;
    data.health = 100.0f;
    data.maxHealth = 100.0f;
    data.mana = 100.0f;
    data.maxMana = 100.0f;
    data.state = UnitState::Idle;
    data.moveSpeed = 5.0f;

    m_unitData[networkId] = data;
    m_unitTypes[networkId] = unitType;

    // Initialize smoothing
    m_smoothing[networkId] = MovementSmoothing();
}

void UnitReplicator::unregisterUnit(uint64_t networkId) {
    m_unitData.erase(networkId);
    m_unitTypes.erase(networkId);
    m_smoothing.erase(networkId);

    // Remove from any formation
    auto formIt = m_unitToFormation.find(networkId);
    if (formIt != m_unitToFormation.end()) {
        leaveFormation(networkId);
    }
}

bool UnitReplicator::isUnitRegistered(uint64_t networkId) const {
    return m_unitData.find(networkId) != m_unitData.end();
}

void UnitReplicator::replicateMovement(uint64_t networkId, const NetVec3& position,
                                        const NetQuat& rotation, const NetVec3& velocity) {
    auto it = m_unitData.find(networkId);
    if (it == m_unitData.end()) return;

    auto& data = it->second;

    // Check if change is significant enough to replicate
    float posDiff = std::sqrt(
        std::pow(data.position.x - position.x, 2) +
        std::pow(data.position.y - position.y, 2) +
        std::pow(data.position.z - position.z, 2));

    if (posDiff < m_positionThreshold) return;

    // Start smoothing for remote entities
    if (!ReplicationManager::getInstance().hasAuthority(networkId)) {
        startSmoothing(networkId, position);
    }

    data.position = position;
    data.rotation = rotation;
    data.velocity = velocity;

    // Mark dirty for network replication
    ReplicationManager::getInstance().markDirty(networkId, NetworkedEntity::PROP_POSITION);
    ReplicationManager::getInstance().markDirty(networkId, NetworkedEntity::PROP_ROTATION);
}

void UnitReplicator::replicateDestination(uint64_t networkId, const NetVec3& destination) {
    auto it = m_unitData.find(networkId);
    if (it == m_unitData.end()) return;

    it->second.destination = destination;
    it->second.isMoving = true;

    // Replicate via RPC or state update
}

NetVec3 UnitReplicator::getSmoothedPosition(uint64_t networkId) const {
    auto smoothIt = m_smoothing.find(networkId);
    if (smoothIt == m_smoothing.end() || !smoothIt->second.isSmoothing) {
        auto dataIt = m_unitData.find(networkId);
        if (dataIt != m_unitData.end()) {
            return dataIt->second.position;
        }
        return NetVec3();
    }

    const auto& smooth = smoothIt->second;
    float t = smooth.interpolationTime / smooth.interpolationDuration;
    t = std::min(1.0f, std::max(0.0f, t));

    NetVec3 result;
    result.x = smooth.lastPosition.x + (smooth.targetPosition.x - smooth.lastPosition.x) * t;
    result.y = smooth.lastPosition.y + (smooth.targetPosition.y - smooth.lastPosition.y) * t;
    result.z = smooth.lastPosition.z + (smooth.targetPosition.z - smooth.lastPosition.z) * t;
    return result;
}

NetQuat UnitReplicator::getSmoothedRotation(uint64_t networkId) const {
    auto dataIt = m_unitData.find(networkId);
    if (dataIt != m_unitData.end()) {
        return dataIt->second.rotation;
    }
    return NetQuat();
}

void UnitReplicator::replicateCombatAction(const CombatActionData& action) {
    // Serialize and send via reliable channel
    std::vector<uint8_t> buffer = serializeCombatAction(action);

    // Notify local callbacks
    for (auto& callback : m_combatCallbacks) {
        callback(action);
    }
}

void UnitReplicator::replicateAbilityUse(uint64_t networkId, uint32_t abilityId,
                                          uint64_t targetId, const NetVec3& targetPos) {
    CombatActionData action;
    action.actionId = static_cast<uint32_t>(std::rand());
    action.type = CombatAction::Ability1;  // Would map from abilityId
    action.sourceId = networkId;
    action.targetId = targetId;
    action.targetPosition = targetPos;
    action.abilityId = abilityId;

    replicateCombatAction(action);
}

void UnitReplicator::replicateDamage(uint64_t sourceId, uint64_t targetId, float damage,
                                      const std::string& damageType) {
    auto it = m_unitData.find(targetId);
    if (it != m_unitData.end()) {
        it->second.health = std::max(0.0f, it->second.health - damage);

        if (it->second.health <= 0.0f) {
            it->second.state = UnitState::Dead;
        }
    }

    CombatActionData action;
    action.sourceId = sourceId;
    action.targetId = targetId;
    action.damage = damage;
    action.type = CombatAction::BasicAttack;

    replicateCombatAction(action);
}

void UnitReplicator::replicateHeal(uint64_t sourceId, uint64_t targetId, float amount) {
    auto it = m_unitData.find(targetId);
    if (it != m_unitData.end()) {
        it->second.health = std::min(it->second.maxHealth, it->second.health + amount);
    }
}

void UnitReplicator::replicateBuff(uint64_t targetId, uint32_t buffId, float duration) {
    auto it = m_unitData.find(targetId);
    if (it != m_unitData.end()) {
        it->second.activeBuffs.push_back(buffId);
    }
}

void UnitReplicator::replicateDebuff(uint64_t targetId, uint32_t debuffId, float duration) {
    auto it = m_unitData.find(targetId);
    if (it != m_unitData.end()) {
        it->second.activeDebuffs.push_back(debuffId);
    }
}

void UnitReplicator::replicateState(uint64_t networkId, UnitState state) {
    auto it = m_unitData.find(networkId);
    if (it != m_unitData.end()) {
        it->second.state = state;
    }
}

void UnitReplicator::replicateTarget(uint64_t networkId, uint64_t targetId) {
    auto it = m_unitData.find(networkId);
    if (it != m_unitData.end()) {
        it->second.targetId = targetId;
    }
}

void UnitReplicator::replicateStats(uint64_t networkId, float health, float mana, float shield) {
    auto it = m_unitData.find(networkId);
    if (it != m_unitData.end()) {
        it->second.health = health;
        it->second.mana = mana;
        it->second.shield = shield;
    }
}

void UnitReplicator::replicateSpawn(const SpawnData& spawn) {
    // Create unit data
    UnitReplicationData data;
    data.networkId = spawn.networkId;
    data.ownerId = spawn.ownerId;
    data.position = spawn.position;
    data.rotation = spawn.rotation;
    data.state = UnitState::Spawning;
    data.health = 100.0f;
    data.maxHealth = 100.0f;

    m_unitData[spawn.networkId] = data;
    m_unitTypes[spawn.networkId] = spawn.unitType;

    // Notify callbacks
    for (auto& callback : m_spawnCallbacks) {
        callback(spawn);
    }
}

void UnitReplicator::replicateDeath(const DeathData& death) {
    auto it = m_unitData.find(death.networkId);
    if (it != m_unitData.end()) {
        it->second.state = UnitState::Dead;
        it->second.health = 0.0f;
    }

    // Notify callbacks
    for (auto& callback : m_deathCallbacks) {
        callback(death);
    }
}

void UnitReplicator::replicateRespawn(uint64_t networkId, const NetVec3& position) {
    auto it = m_unitData.find(networkId);
    if (it != m_unitData.end()) {
        it->second.position = position;
        it->second.state = UnitState::Spawning;
        it->second.health = it->second.maxHealth;
        it->second.mana = it->second.maxMana;
    }
}

void UnitReplicator::createFormation(const FormationData& formation) {
    m_formations[formation.formationId] = formation;

    for (uint64_t memberId : formation.memberIds) {
        m_unitToFormation[memberId] = formation.formationId;
    }

    // Notify callbacks
    for (auto& callback : m_formationCallbacks) {
        callback(formation);
    }
}

void UnitReplicator::updateFormation(uint64_t formationId, const std::vector<NetVec3>& offsets) {
    auto it = m_formations.find(formationId);
    if (it != m_formations.end()) {
        it->second.offsets = offsets;

        for (auto& callback : m_formationCallbacks) {
            callback(it->second);
        }
    }
}

void UnitReplicator::disbandFormation(uint64_t formationId) {
    auto it = m_formations.find(formationId);
    if (it == m_formations.end()) return;

    // Clear member references
    for (uint64_t memberId : it->second.memberIds) {
        m_unitToFormation.erase(memberId);

        auto unitIt = m_unitData.find(memberId);
        if (unitIt != m_unitData.end()) {
            unitIt->second.formationIndex = -1;
            unitIt->second.formationLeaderId = 0;
        }
    }

    m_formations.erase(it);
}

void UnitReplicator::joinFormation(uint64_t unitId, uint64_t formationId) {
    auto formIt = m_formations.find(formationId);
    if (formIt == m_formations.end()) return;

    // Add to formation
    formIt->second.memberIds.push_back(unitId);
    m_unitToFormation[unitId] = formationId;

    auto unitIt = m_unitData.find(unitId);
    if (unitIt != m_unitData.end()) {
        unitIt->second.formationIndex = static_cast<int>(formIt->second.memberIds.size() - 1);
        unitIt->second.formationLeaderId = formIt->second.leaderId;
    }

    for (auto& callback : m_formationCallbacks) {
        callback(formIt->second);
    }
}

void UnitReplicator::leaveFormation(uint64_t unitId) {
    auto formMapIt = m_unitToFormation.find(unitId);
    if (formMapIt == m_unitToFormation.end()) return;

    uint64_t formationId = formMapIt->second;
    auto formIt = m_formations.find(formationId);

    if (formIt != m_formations.end()) {
        auto& members = formIt->second.memberIds;
        members.erase(std::remove(members.begin(), members.end(), unitId), members.end());

        // Update indices
        for (size_t i = 0; i < members.size(); i++) {
            auto memberIt = m_unitData.find(members[i]);
            if (memberIt != m_unitData.end()) {
                memberIt->second.formationIndex = static_cast<int>(i);
            }
        }

        for (auto& callback : m_formationCallbacks) {
            callback(formIt->second);
        }
    }

    m_unitToFormation.erase(formMapIt);

    auto unitIt = m_unitData.find(unitId);
    if (unitIt != m_unitData.end()) {
        unitIt->second.formationIndex = -1;
        unitIt->second.formationLeaderId = 0;
    }
}

const FormationData* UnitReplicator::getFormation(uint64_t formationId) const {
    auto it = m_formations.find(formationId);
    if (it != m_formations.end()) {
        return &it->second;
    }
    return nullptr;
}

NetVec3 UnitReplicator::getFormationPosition(uint64_t unitId) const {
    auto formMapIt = m_unitToFormation.find(unitId);
    if (formMapIt == m_unitToFormation.end()) {
        auto dataIt = m_unitData.find(unitId);
        if (dataIt != m_unitData.end()) {
            return dataIt->second.position;
        }
        return NetVec3();
    }

    auto formIt = m_formations.find(formMapIt->second);
    if (formIt == m_formations.end()) return NetVec3();

    auto unitIt = m_unitData.find(unitId);
    if (unitIt == m_unitData.end()) return NetVec3();

    int index = unitIt->second.formationIndex;
    if (index < 0 || index >= static_cast<int>(formIt->second.offsets.size())) {
        return unitIt->second.position;
    }

    // Get leader position
    auto leaderIt = m_unitData.find(formIt->second.leaderId);
    if (leaderIt == m_unitData.end()) return unitIt->second.position;

    const NetVec3& leaderPos = leaderIt->second.position;
    const NetVec3& offset = formIt->second.offsets[index];

    NetVec3 result;
    result.x = leaderPos.x + offset.x;
    result.y = leaderPos.y + offset.y;
    result.z = leaderPos.z + offset.z;
    return result;
}

const UnitReplicationData* UnitReplicator::getUnitData(uint64_t networkId) const {
    auto it = m_unitData.find(networkId);
    if (it != m_unitData.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint64_t> UnitReplicator::getUnitsInRange(const NetVec3& center, float radius) const {
    std::vector<uint64_t> result;
    float radiusSq = radius * radius;

    for (const auto& [id, data] : m_unitData) {
        float dx = data.position.x - center.x;
        float dy = data.position.y - center.y;
        float dz = data.position.z - center.z;
        float distSq = dx*dx + dy*dy + dz*dz;

        if (distSq <= radiusSq) {
            result.push_back(id);
        }
    }

    return result;
}

std::vector<uint64_t> UnitReplicator::getUnitsByTeam(int team) const {
    std::vector<uint64_t> result;
    // Would filter by team if we tracked team in UnitReplicationData
    return result;
}

void UnitReplicator::onUnitSpawn(UnitSpawnCallback callback) {
    m_spawnCallbacks.push_back(callback);
}

void UnitReplicator::onUnitDeath(UnitDeathCallback callback) {
    m_deathCallbacks.push_back(callback);
}

void UnitReplicator::onCombatAction(CombatActionCallback callback) {
    m_combatCallbacks.push_back(callback);
}

void UnitReplicator::onFormationUpdate(FormationCallback callback) {
    m_formationCallbacks.push_back(callback);
}

// Private methods

void UnitReplicator::updateMovementSmoothing(float deltaTime) {
    for (auto& [id, smooth] : m_smoothing) {
        if (!smooth.isSmoothing) continue;

        smooth.interpolationTime += deltaTime;

        if (smooth.interpolationTime >= smooth.interpolationDuration) {
            smooth.isSmoothing = false;
            smooth.lastPosition = smooth.targetPosition;
        }
    }
}

void UnitReplicator::updateFormations(float deltaTime) {
    // Update formation member positions to follow leader
    for (auto& [id, formation] : m_formations) {
        auto leaderIt = m_unitData.find(formation.leaderId);
        if (leaderIt == m_unitData.end()) continue;

        for (size_t i = 0; i < formation.memberIds.size(); i++) {
            uint64_t memberId = formation.memberIds[i];
            if (memberId == formation.leaderId) continue;

            NetVec3 targetPos = getFormationPosition(memberId);

            auto memberIt = m_unitData.find(memberId);
            if (memberIt != m_unitData.end()) {
                memberIt->second.destination = targetPos;
            }
        }
    }
}

void UnitReplicator::processReplicationQueue() {
    // Replicate all dirty units
    for (auto& [id, data] : m_unitData) {
        if (ReplicationManager::getInstance().hasAuthority(id)) {
            if (ReplicationManager::getInstance().isDirty(id)) {
                // Unit will be replicated by ReplicationManager
            }
        }
    }
}

void UnitReplicator::startSmoothing(uint64_t networkId, const NetVec3& targetPos) {
    auto& smooth = m_smoothing[networkId];

    if (smooth.isSmoothing) {
        // Continue from current interpolated position
        smooth.lastPosition = getSmoothedPosition(networkId);
    } else {
        auto dataIt = m_unitData.find(networkId);
        if (dataIt != m_unitData.end()) {
            smooth.lastPosition = dataIt->second.position;
        }
    }

    smooth.targetPosition = targetPos;
    smooth.interpolationTime = 0.0f;
    smooth.interpolationDuration = m_smoothingDuration;
    smooth.isSmoothing = true;
}

MovementSmoothing& UnitReplicator::getSmoothing(uint64_t networkId) {
    return m_smoothing[networkId];
}

std::vector<uint8_t> UnitReplicator::serializeUnitData(const UnitReplicationData& data) {
    std::vector<uint8_t> buffer;
    // Serialize all fields
    buffer.resize(sizeof(UnitReplicationData));
    std::memcpy(buffer.data(), &data, sizeof(UnitReplicationData));
    return buffer;
}

UnitReplicationData UnitReplicator::deserializeUnitData(const std::vector<uint8_t>& buffer) {
    UnitReplicationData data;
    if (buffer.size() >= sizeof(UnitReplicationData)) {
        std::memcpy(&data, buffer.data(), sizeof(UnitReplicationData));
    }
    return data;
}

std::vector<uint8_t> UnitReplicator::serializeCombatAction(const CombatActionData& action) {
    std::vector<uint8_t> buffer;
    size_t offset = 0;

    buffer.resize(64);  // Base size

    // Action ID
    std::memcpy(buffer.data() + offset, &action.actionId, 4);
    offset += 4;

    // Type
    uint8_t type = static_cast<uint8_t>(action.type);
    buffer[offset++] = type;

    // Source ID
    std::memcpy(buffer.data() + offset, &action.sourceId, 8);
    offset += 8;

    // Target ID
    std::memcpy(buffer.data() + offset, &action.targetId, 8);
    offset += 8;

    // Target position
    std::memcpy(buffer.data() + offset, &action.targetPosition, sizeof(NetVec3));
    offset += sizeof(NetVec3);

    // Damage
    std::memcpy(buffer.data() + offset, &action.damage, 4);
    offset += 4;

    // Ability ID
    std::memcpy(buffer.data() + offset, &action.abilityId, 4);
    offset += 4;

    buffer.resize(offset);
    return buffer;
}

CombatActionData UnitReplicator::deserializeCombatAction(const std::vector<uint8_t>& buffer) {
    CombatActionData action;
    if (buffer.size() < 32) return action;

    size_t offset = 0;

    std::memcpy(&action.actionId, buffer.data() + offset, 4);
    offset += 4;

    action.type = static_cast<CombatAction>(buffer[offset++]);

    std::memcpy(&action.sourceId, buffer.data() + offset, 8);
    offset += 8;

    std::memcpy(&action.targetId, buffer.data() + offset, 8);
    offset += 8;

    std::memcpy(&action.targetPosition, buffer.data() + offset, sizeof(NetVec3));
    offset += sizeof(NetVec3);

    std::memcpy(&action.damage, buffer.data() + offset, 4);
    offset += 4;

    std::memcpy(&action.abilityId, buffer.data() + offset, 4);

    return action;
}

} // namespace Replication
} // namespace Network
