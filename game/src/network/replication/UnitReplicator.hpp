#pragma once

#include "NetworkedEntity.hpp"
#include "NetworkPrediction.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace Network {
namespace Replication {

// Unit states
enum class UnitState {
    Idle,
    Moving,
    Attacking,
    Casting,
    Stunned,
    Dead,
    Spawning,
    Despawning
};

// Combat action types
enum class CombatAction {
    None,
    BasicAttack,
    Ability1,
    Ability2,
    Ability3,
    Ultimate,
    Item,
    Dodge,
    Block
};

// Replicated unit data
struct UnitReplicationData {
    uint64_t networkId;
    uint64_t ownerId;

    // Transform
    NetVec3 position;
    NetQuat rotation;
    NetVec3 velocity;

    // Combat stats
    float health;
    float maxHealth;
    float mana;
    float maxMana;
    float shield;

    // State
    UnitState state;
    uint64_t targetId;

    // Movement
    NetVec3 destination;
    float moveSpeed;
    bool isMoving;

    // Combat
    CombatAction currentAction;
    float actionProgress;
    float attackCooldown;
    std::vector<uint32_t> abilityCooldowns;

    // Formation
    int formationIndex;
    uint64_t formationLeaderId;

    // Buffs/Debuffs (serialized as IDs)
    std::vector<uint32_t> activeBuffs;
    std::vector<uint32_t> activeDebuffs;
};

// Movement smoothing data
struct MovementSmoothing {
    NetVec3 lastPosition;
    NetVec3 targetPosition;
    NetVec3 velocity;
    float interpolationTime;
    float interpolationDuration;
    bool isSmoothing;
};

// Combat action replication
struct CombatActionData {
    uint32_t actionId;
    CombatAction type;
    uint64_t sourceId;
    uint64_t targetId;
    NetVec3 targetPosition;
    float damage;
    uint32_t abilityId;
    std::vector<uint8_t> customData;
};

// Death/spawn data
struct SpawnData {
    uint64_t networkId;
    std::string unitType;
    uint64_t ownerId;
    NetVec3 position;
    NetQuat rotation;
    int team;
    std::unordered_map<std::string, std::string> properties;
};

struct DeathData {
    uint64_t networkId;
    uint64_t killerId;
    NetVec3 deathPosition;
    std::string deathCause;
    float respawnTime;
};

// Formation data
struct FormationData {
    uint64_t formationId;
    uint64_t leaderId;
    std::vector<uint64_t> memberIds;
    std::vector<NetVec3> offsets;
    int formationType;
    float spacing;
};

// Callbacks
using UnitSpawnCallback = std::function<void(const SpawnData&)>;
using UnitDeathCallback = std::function<void(const DeathData&)>;
using CombatActionCallback = std::function<void(const CombatActionData&)>;
using FormationCallback = std::function<void(const FormationData&)>;

/**
 * UnitReplicator - Unit-specific replication
 *
 * Features:
 * - Movement replication with smoothing
 * - Combat action replication
 * - Ability usage replication
 * - Death/spawn synchronization
 * - Formation replication
 */
class UnitReplicator {
public:
    static UnitReplicator& getInstance();

    // Initialization
    bool initialize();
    void shutdown();
    void update(float deltaTime);

    // Unit registration
    void registerUnit(uint64_t networkId, const std::string& unitType);
    void unregisterUnit(uint64_t networkId);
    bool isUnitRegistered(uint64_t networkId) const;

    // Movement replication
    void replicateMovement(uint64_t networkId, const NetVec3& position,
                           const NetQuat& rotation, const NetVec3& velocity);
    void replicateDestination(uint64_t networkId, const NetVec3& destination);
    void setMovementSmoothing(float duration) { m_smoothingDuration = duration; }
    void enableMovementPrediction(bool enabled) { m_movementPrediction = enabled; }

    // Apply smoothed position to entity
    NetVec3 getSmoothedPosition(uint64_t networkId) const;
    NetQuat getSmoothedRotation(uint64_t networkId) const;

    // Combat replication
    void replicateCombatAction(const CombatActionData& action);
    void replicateAbilityUse(uint64_t networkId, uint32_t abilityId,
                              uint64_t targetId, const NetVec3& targetPos);
    void replicateDamage(uint64_t sourceId, uint64_t targetId, float damage,
                          const std::string& damageType);
    void replicateHeal(uint64_t sourceId, uint64_t targetId, float amount);
    void replicateBuff(uint64_t targetId, uint32_t buffId, float duration);
    void replicateDebuff(uint64_t targetId, uint32_t debuffId, float duration);

    // State replication
    void replicateState(uint64_t networkId, UnitState state);
    void replicateTarget(uint64_t networkId, uint64_t targetId);
    void replicateStats(uint64_t networkId, float health, float mana, float shield);

    // Spawn/Death
    void replicateSpawn(const SpawnData& spawn);
    void replicateDeath(const DeathData& death);
    void replicateRespawn(uint64_t networkId, const NetVec3& position);

    // Formation replication
    void createFormation(const FormationData& formation);
    void updateFormation(uint64_t formationId, const std::vector<NetVec3>& offsets);
    void disbandFormation(uint64_t formationId);
    void joinFormation(uint64_t unitId, uint64_t formationId);
    void leaveFormation(uint64_t unitId);
    const FormationData* getFormation(uint64_t formationId) const;
    NetVec3 getFormationPosition(uint64_t unitId) const;

    // Get unit data
    const UnitReplicationData* getUnitData(uint64_t networkId) const;
    std::vector<uint64_t> getUnitsInRange(const NetVec3& center, float radius) const;
    std::vector<uint64_t> getUnitsByTeam(int team) const;

    // Callbacks
    void onUnitSpawn(UnitSpawnCallback callback);
    void onUnitDeath(UnitDeathCallback callback);
    void onCombatAction(CombatActionCallback callback);
    void onFormationUpdate(FormationCallback callback);

    // Settings
    void setReplicationRate(float rate) { m_replicationRate = rate; }
    void setPositionThreshold(float threshold) { m_positionThreshold = threshold; }
    void setRotationThreshold(float threshold) { m_rotationThreshold = threshold; }

private:
    UnitReplicator();
    ~UnitReplicator();
    UnitReplicator(const UnitReplicator&) = delete;
    UnitReplicator& operator=(const UnitReplicator&) = delete;

    // Internal updates
    void updateMovementSmoothing(float deltaTime);
    void updateFormations(float deltaTime);
    void processReplicationQueue();

    // Smoothing helpers
    void startSmoothing(uint64_t networkId, const NetVec3& targetPos);
    MovementSmoothing& getSmoothing(uint64_t networkId);

    // Serialization
    std::vector<uint8_t> serializeUnitData(const UnitReplicationData& data);
    UnitReplicationData deserializeUnitData(const std::vector<uint8_t>& buffer);
    std::vector<uint8_t> serializeCombatAction(const CombatActionData& action);
    CombatActionData deserializeCombatAction(const std::vector<uint8_t>& buffer);

private:
    bool m_initialized = false;

    // Unit data
    std::unordered_map<uint64_t, UnitReplicationData> m_unitData;
    std::unordered_map<uint64_t, std::string> m_unitTypes;

    // Movement smoothing
    std::unordered_map<uint64_t, MovementSmoothing> m_smoothing;
    float m_smoothingDuration = 0.1f;
    bool m_movementPrediction = true;

    // Formations
    std::unordered_map<uint64_t, FormationData> m_formations;
    std::unordered_map<uint64_t, uint64_t> m_unitToFormation;

    // Callbacks
    std::vector<UnitSpawnCallback> m_spawnCallbacks;
    std::vector<UnitDeathCallback> m_deathCallbacks;
    std::vector<CombatActionCallback> m_combatCallbacks;
    std::vector<FormationCallback> m_formationCallbacks;

    // Settings
    float m_replicationRate = 20.0f;  // Updates per second
    float m_positionThreshold = 0.01f;  // Min position change to replicate
    float m_rotationThreshold = 0.01f;  // Min rotation change to replicate

    // Replication timing
    float m_replicationTimer = 0.0f;
    std::unordered_map<uint64_t, float> m_lastReplicationTime;
};

} // namespace Replication
} // namespace Network
