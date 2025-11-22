#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <queue>
#include <mutex>

namespace Network {
namespace Replication {

// Forward declarations
class NetworkedEntity;
class NetworkTransport;

// Replication priority levels
enum class ReplicationPriority {
    Critical = 0,    // Must be sent immediately (death, spawn)
    High = 1,        // High priority (combat actions, important state)
    Normal = 2,      // Normal priority (movement, rotation)
    Low = 3,         // Low priority (cosmetic, animations)
    Background = 4   // Can be delayed (non-essential updates)
};

// Replication modes
enum class ReplicationMode {
    Authoritative,   // Server is authoritative
    Predicted,       // Client-side prediction with reconciliation
    Interpolated,    // Interpolate between snapshots
    Cosmetic         // No prediction, just visual
};

// Replication frequency
enum class ReplicationFrequency {
    EveryTick,       // Every network tick
    HighFrequency,   // Multiple times per second
    MediumFrequency, // Once per second
    LowFrequency,    // Few times per second
    OnChange         // Only when changed
};

// Network role
enum class NetworkRole {
    None,
    Authority,       // Has authority over this entity
    SimulatedProxy,  // Simulated on this client
    AutonomousProxy  // Autonomous on this client (player-controlled)
};

// Property replication condition
enum class ReplicationCondition {
    Always,          // Always replicate
    OwnerOnly,       // Only to owner
    SkipOwner,       // Everyone except owner
    InitialOnly,     // Only when first replicated
    Custom           // Custom condition function
};

// Dirty property for tracking changes
struct DirtyProperty {
    uint32_t propertyId;
    std::string propertyName;
    ReplicationPriority priority;
    std::chrono::steady_clock::time_point dirtyTime;
    bool isReliable;
};

// Replication stats for a single entity
struct EntityReplicationStats {
    uint64_t networkId;
    int bytesSent = 0;
    int bytesReceived = 0;
    int updatesSent = 0;
    int updatesReceived = 0;
    float averageBandwidth = 0.0f;
    std::chrono::steady_clock::time_point lastUpdate;
};

// Global replication stats
struct ReplicationStats {
    int totalBytesSent = 0;
    int totalBytesReceived = 0;
    int entitiesReplicated = 0;
    int updatesPerSecond = 0;
    float bandwidthUsed = 0.0f;      // Bytes per second
    float bandwidthLimit = 0.0f;     // Maximum bytes per second
    float bandwidthUtilization = 0.0f;
    int droppedUpdates = 0;
    int compressedBytes = 0;
    float compressionRatio = 0.0f;
};

// Snapshot for interpolation
struct EntitySnapshot {
    uint64_t networkId;
    uint32_t sequenceNumber;
    std::chrono::steady_clock::time_point timestamp;
    std::vector<uint8_t> data;
};

// Replication channel
struct ReplicationChannel {
    std::string name;
    bool isReliable;
    bool isOrdered;
    int maxBandwidth;  // Bytes per second, 0 = unlimited
    ReplicationPriority minPriority;
};

// Entity registration info
struct EntityRegistration {
    uint64_t networkId;
    std::string entityType;
    uint64_t ownerId;
    NetworkRole localRole;
    ReplicationMode mode;
    std::weak_ptr<NetworkedEntity> entity;
    std::chrono::steady_clock::time_point registeredAt;
};

// Property definition
struct PropertyDefinition {
    uint32_t id;
    std::string name;
    std::string type;
    size_t offset;
    size_t size;
    ReplicationCondition condition;
    ReplicationPriority priority;
    bool isReliable;
    std::function<bool(const void*, const void*)> hasChanged;
    std::function<void(void*, const uint8_t*, size_t)> deserialize;
    std::function<size_t(const void*, uint8_t*, size_t)> serialize;
};

// Callbacks
using EntitySpawnCallback = std::function<void(uint64_t networkId, const std::string& type)>;
using EntityDespawnCallback = std::function<void(uint64_t networkId)>;
using PropertyUpdateCallback = std::function<void(uint64_t networkId, uint32_t propertyId)>;

/**
 * ReplicationManager - Entity/state replication system
 *
 * Features:
 * - Entity registration for replication
 * - Dirty flag tracking
 * - Delta compression
 * - Priority-based updates
 * - Bandwidth management
 * - Interpolation/extrapolation
 * - Lag compensation
 */
class ReplicationManager {
public:
    static ReplicationManager& getInstance();

    // Initialization
    bool initialize(std::shared_ptr<NetworkTransport> transport);
    void shutdown();
    void update(float deltaTime);

    // Entity registration
    uint64_t registerEntity(std::shared_ptr<NetworkedEntity> entity, const std::string& type);
    void unregisterEntity(uint64_t networkId);
    void unregisterAllEntities();
    bool isEntityRegistered(uint64_t networkId) const;
    std::shared_ptr<NetworkedEntity> getEntity(uint64_t networkId) const;
    std::vector<uint64_t> getEntitiesByType(const std::string& type) const;
    std::vector<uint64_t> getEntitiesByOwner(uint64_t ownerId) const;

    // Property registration
    void registerProperty(const std::string& entityType, const PropertyDefinition& property);
    void registerProperties(const std::string& entityType, const std::vector<PropertyDefinition>& properties);
    const std::vector<PropertyDefinition>& getPropertyDefinitions(const std::string& entityType) const;

    // Dirty tracking
    void markDirty(uint64_t networkId, uint32_t propertyId);
    void markAllDirty(uint64_t networkId);
    void clearDirty(uint64_t networkId);
    bool isDirty(uint64_t networkId) const;
    bool isPropertyDirty(uint64_t networkId, uint32_t propertyId) const;
    std::vector<DirtyProperty> getDirtyProperties(uint64_t networkId) const;

    // Replication
    void replicateEntity(uint64_t networkId, bool reliable = false);
    void replicateProperty(uint64_t networkId, uint32_t propertyId, bool reliable = false);
    void replicateAll(bool reliable = false);
    void forceReplication(uint64_t networkId);

    // Ownership
    void setOwner(uint64_t networkId, uint64_t ownerId);
    uint64_t getOwner(uint64_t networkId) const;
    bool isOwner(uint64_t networkId) const;
    bool hasAuthority(uint64_t networkId) const;
    void transferAuthority(uint64_t networkId, uint64_t newOwnerId);

    // Role management
    void setNetworkRole(uint64_t networkId, NetworkRole role);
    NetworkRole getNetworkRole(uint64_t networkId) const;
    void setReplicationMode(uint64_t networkId, ReplicationMode mode);
    ReplicationMode getReplicationMode(uint64_t networkId) const;

    // Bandwidth management
    void setBandwidthLimit(float bytesPerSecond);
    float getBandwidthLimit() const { return m_bandwidthLimit; }
    float getCurrentBandwidth() const { return m_stats.bandwidthUsed; }
    void setPriorityThreshold(ReplicationPriority threshold);

    // Interpolation/Extrapolation
    void setInterpolationDelay(float seconds) { m_interpolationDelay = seconds; }
    float getInterpolationDelay() const { return m_interpolationDelay; }
    void setExtrapolationLimit(float seconds) { m_extrapolationLimit = seconds; }
    float getExtrapolationLimit() const { return m_extrapolationLimit; }

    // Lag compensation
    void enableLagCompensation(bool enabled) { m_lagCompensationEnabled = enabled; }
    bool isLagCompensationEnabled() const { return m_lagCompensationEnabled; }
    void setMaxLagCompensation(float seconds) { m_maxLagCompensation = seconds; }

    // Snapshot management
    void storeSnapshot(uint64_t networkId);
    EntitySnapshot getSnapshot(uint64_t networkId, std::chrono::steady_clock::time_point time) const;
    void rewindTo(std::chrono::steady_clock::time_point time);
    void clearSnapshots(uint64_t networkId);
    void clearAllSnapshots();

    // Delta compression
    void enableDeltaCompression(bool enabled) { m_deltaCompressionEnabled = enabled; }
    bool isDeltaCompressionEnabled() const { return m_deltaCompressionEnabled; }

    // Channels
    void createChannel(const ReplicationChannel& channel);
    void setEntityChannel(uint64_t networkId, const std::string& channelName);

    // Stats
    const ReplicationStats& getStats() const { return m_stats; }
    EntityReplicationStats getEntityStats(uint64_t networkId) const;
    void resetStats();

    // Callbacks
    void onEntitySpawn(EntitySpawnCallback callback);
    void onEntityDespawn(EntityDespawnCallback callback);
    void onPropertyUpdate(PropertyUpdateCallback callback);

    // Network tick
    void setNetworkTickRate(int ticksPerSecond);
    int getNetworkTickRate() const { return m_networkTickRate; }
    uint32_t getCurrentTick() const { return m_currentTick; }

    // Debug
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    std::string getDebugInfo() const;

private:
    ReplicationManager();
    ~ReplicationManager();
    ReplicationManager(const ReplicationManager&) = delete;
    ReplicationManager& operator=(const ReplicationManager&) = delete;

    // Internal replication
    void processOutgoingReplication();
    void processIncomingReplication();
    void serializeEntity(uint64_t networkId, std::vector<uint8_t>& buffer);
    void deserializeEntity(const std::vector<uint8_t>& buffer);
    void serializeProperty(uint64_t networkId, uint32_t propertyId, std::vector<uint8_t>& buffer);
    void deserializeProperty(uint64_t networkId, const std::vector<uint8_t>& buffer);

    // Delta compression
    std::vector<uint8_t> computeDelta(const std::vector<uint8_t>& current,
                                       const std::vector<uint8_t>& previous);
    std::vector<uint8_t> applyDelta(const std::vector<uint8_t>& previous,
                                     const std::vector<uint8_t>& delta);

    // Priority queue management
    void queueUpdate(uint64_t networkId, uint32_t propertyId, ReplicationPriority priority);
    void processUpdateQueue();

    // Bandwidth management
    bool canSendUpdate(size_t bytes);
    void recordBandwidthUsage(size_t bytes);

    // Snapshot management
    void trimSnapshots(uint64_t networkId);
    EntitySnapshot interpolateSnapshots(const EntitySnapshot& a, const EntitySnapshot& b, float t) const;

    // ID generation
    uint64_t generateNetworkId();

private:
    bool m_initialized = false;
    std::shared_ptr<NetworkTransport> m_transport;

    // Entity storage
    std::unordered_map<uint64_t, EntityRegistration> m_entities;
    std::unordered_map<std::string, std::vector<uint64_t>> m_entitiesByType;
    mutable std::mutex m_entityMutex;

    // Property definitions
    std::unordered_map<std::string, std::vector<PropertyDefinition>> m_propertyDefinitions;
    static const std::vector<PropertyDefinition> s_emptyProperties;

    // Dirty tracking
    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> m_dirtyProperties;
    std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> m_lastReplicationTime;

    // Update queue (priority-based)
    struct PendingUpdate {
        uint64_t networkId;
        uint32_t propertyId;
        ReplicationPriority priority;
        std::chrono::steady_clock::time_point queueTime;
        bool isReliable;

        bool operator<(const PendingUpdate& other) const {
            return priority > other.priority;  // Lower enum value = higher priority
        }
    };
    std::priority_queue<PendingUpdate> m_updateQueue;

    // Snapshots for interpolation/lag compensation
    std::unordered_map<uint64_t, std::vector<EntitySnapshot>> m_snapshots;
    static constexpr size_t MAX_SNAPSHOTS_PER_ENTITY = 64;

    // Previous state for delta compression
    std::unordered_map<uint64_t, std::vector<uint8_t>> m_previousState;

    // Channels
    std::unordered_map<std::string, ReplicationChannel> m_channels;
    std::unordered_map<uint64_t, std::string> m_entityChannels;

    // Callbacks
    std::vector<EntitySpawnCallback> m_spawnCallbacks;
    std::vector<EntityDespawnCallback> m_despawnCallbacks;
    std::vector<PropertyUpdateCallback> m_propertyCallbacks;

    // Settings
    float m_bandwidthLimit = 100000.0f;  // 100 KB/s default
    float m_interpolationDelay = 0.1f;   // 100ms
    float m_extrapolationLimit = 0.25f;  // 250ms max extrapolation
    float m_maxLagCompensation = 0.5f;   // 500ms max lag compensation
    bool m_lagCompensationEnabled = true;
    bool m_deltaCompressionEnabled = true;
    ReplicationPriority m_priorityThreshold = ReplicationPriority::Background;

    // Network tick
    int m_networkTickRate = 20;  // 20 ticks per second
    uint32_t m_currentTick = 0;
    float m_tickAccumulator = 0.0f;

    // Stats
    ReplicationStats m_stats;
    std::unordered_map<uint64_t, EntityReplicationStats> m_entityStats;

    // ID generation
    uint64_t m_nextNetworkId = 1;

    // Local player ID
    uint64_t m_localPlayerId = 0;

    // Debug
    bool m_debugMode = false;
};

// Macros for property replication
#define REPLICATE(name, type) \
    type name; \
    static constexpr uint32_t name##_PropertyId = __COUNTER__; \
    void set_##name(const type& value) { \
        if (name != value) { \
            name = value; \
            markDirty(name##_PropertyId); \
        } \
    }

#define REPLICATE_CONDITION(name, type, condition) \
    type name; \
    static constexpr uint32_t name##_PropertyId = __COUNTER__; \
    static constexpr ReplicationCondition name##_Condition = condition; \
    void set_##name(const type& value) { \
        if (name != value) { \
            name = value; \
            markDirty(name##_PropertyId); \
        } \
    }

} // namespace Replication
} // namespace Network
