#include "ReplicationManager.hpp"
#include "NetworkedEntity.hpp"
#include "NetworkTransport.hpp"
#include <algorithm>
#include <cstring>

namespace Network {
namespace Replication {

const std::vector<PropertyDefinition> ReplicationManager::s_emptyProperties;

ReplicationManager& ReplicationManager::getInstance() {
    static ReplicationManager instance;
    return instance;
}

ReplicationManager::ReplicationManager() {}

ReplicationManager::~ReplicationManager() {
    shutdown();
}

bool ReplicationManager::initialize(std::shared_ptr<NetworkTransport> transport) {
    if (m_initialized) return true;

    m_transport = transport;
    m_initialized = true;

    // Create default channels
    ReplicationChannel reliableChannel;
    reliableChannel.name = "reliable";
    reliableChannel.isReliable = true;
    reliableChannel.isOrdered = true;
    reliableChannel.maxBandwidth = 0;
    reliableChannel.minPriority = ReplicationPriority::Background;
    createChannel(reliableChannel);

    ReplicationChannel unreliableChannel;
    unreliableChannel.name = "unreliable";
    unreliableChannel.isReliable = false;
    unreliableChannel.isOrdered = false;
    unreliableChannel.maxBandwidth = 0;
    unreliableChannel.minPriority = ReplicationPriority::Low;
    createChannel(unreliableChannel);

    return true;
}

void ReplicationManager::shutdown() {
    if (!m_initialized) return;

    unregisterAllEntities();
    clearAllSnapshots();

    m_transport.reset();
    m_initialized = false;
}

void ReplicationManager::update(float deltaTime) {
    if (!m_initialized) return;

    // Update tick
    m_tickAccumulator += deltaTime;
    float tickInterval = 1.0f / m_networkTickRate;

    while (m_tickAccumulator >= tickInterval) {
        m_tickAccumulator -= tickInterval;
        m_currentTick++;

        // Process replication on each tick
        processOutgoingReplication();
        processIncomingReplication();
    }

    // Update stats
    m_stats.bandwidthUtilization = m_bandwidthLimit > 0 ?
        m_stats.bandwidthUsed / m_bandwidthLimit : 0.0f;
}

uint64_t ReplicationManager::registerEntity(std::shared_ptr<NetworkedEntity> entity,
                                             const std::string& type) {
    if (!entity) return 0;

    std::lock_guard<std::mutex> lock(m_entityMutex);

    uint64_t networkId = generateNetworkId();

    EntityRegistration reg;
    reg.networkId = networkId;
    reg.entityType = type;
    reg.ownerId = m_localPlayerId;
    reg.localRole = NetworkRole::Authority;
    reg.mode = ReplicationMode::Authoritative;
    reg.entity = entity;
    reg.registeredAt = std::chrono::steady_clock::now();

    m_entities[networkId] = reg;
    m_entitiesByType[type].push_back(networkId);

    entity->setNetworkId(networkId);
    entity->setOwner(m_localPlayerId);

    // Mark all properties dirty for initial replication
    markAllDirty(networkId);

    // Notify spawn callbacks
    for (auto& callback : m_spawnCallbacks) {
        callback(networkId, type);
    }

    m_stats.entitiesReplicated = static_cast<int>(m_entities.size());

    return networkId;
}

void ReplicationManager::unregisterEntity(uint64_t networkId) {
    std::lock_guard<std::mutex> lock(m_entityMutex);

    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    // Remove from type index
    auto& typeList = m_entitiesByType[it->second.entityType];
    typeList.erase(std::remove(typeList.begin(), typeList.end(), networkId), typeList.end());

    // Clean up related data
    m_dirtyProperties.erase(networkId);
    m_lastReplicationTime.erase(networkId);
    m_snapshots.erase(networkId);
    m_previousState.erase(networkId);
    m_entityChannels.erase(networkId);
    m_entityStats.erase(networkId);

    // Notify despawn callbacks
    for (auto& callback : m_despawnCallbacks) {
        callback(networkId);
    }

    m_entities.erase(it);
    m_stats.entitiesReplicated = static_cast<int>(m_entities.size());
}

void ReplicationManager::unregisterAllEntities() {
    std::lock_guard<std::mutex> lock(m_entityMutex);

    for (const auto& [networkId, reg] : m_entities) {
        for (auto& callback : m_despawnCallbacks) {
            callback(networkId);
        }
    }

    m_entities.clear();
    m_entitiesByType.clear();
    m_dirtyProperties.clear();
    m_lastReplicationTime.clear();
    m_entityChannels.clear();
    m_entityStats.clear();
    m_stats.entitiesReplicated = 0;
}

bool ReplicationManager::isEntityRegistered(uint64_t networkId) const {
    std::lock_guard<std::mutex> lock(m_entityMutex);
    return m_entities.find(networkId) != m_entities.end();
}

std::shared_ptr<NetworkedEntity> ReplicationManager::getEntity(uint64_t networkId) const {
    std::lock_guard<std::mutex> lock(m_entityMutex);

    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return nullptr;

    return it->second.entity.lock();
}

std::vector<uint64_t> ReplicationManager::getEntitiesByType(const std::string& type) const {
    std::lock_guard<std::mutex> lock(m_entityMutex);

    auto it = m_entitiesByType.find(type);
    if (it == m_entitiesByType.end()) return {};

    return it->second;
}

std::vector<uint64_t> ReplicationManager::getEntitiesByOwner(uint64_t ownerId) const {
    std::lock_guard<std::mutex> lock(m_entityMutex);

    std::vector<uint64_t> result;
    for (const auto& [networkId, reg] : m_entities) {
        if (reg.ownerId == ownerId) {
            result.push_back(networkId);
        }
    }
    return result;
}

void ReplicationManager::registerProperty(const std::string& entityType,
                                           const PropertyDefinition& property) {
    m_propertyDefinitions[entityType].push_back(property);
}

void ReplicationManager::registerProperties(const std::string& entityType,
                                             const std::vector<PropertyDefinition>& properties) {
    auto& defs = m_propertyDefinitions[entityType];
    defs.insert(defs.end(), properties.begin(), properties.end());
}

const std::vector<PropertyDefinition>& ReplicationManager::getPropertyDefinitions(
    const std::string& entityType) const {

    auto it = m_propertyDefinitions.find(entityType);
    if (it == m_propertyDefinitions.end()) return s_emptyProperties;
    return it->second;
}

void ReplicationManager::markDirty(uint64_t networkId, uint32_t propertyId) {
    m_dirtyProperties[networkId].insert(propertyId);
}

void ReplicationManager::markAllDirty(uint64_t networkId) {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    const auto& props = getPropertyDefinitions(it->second.entityType);
    for (const auto& prop : props) {
        m_dirtyProperties[networkId].insert(prop.id);
    }
}

void ReplicationManager::clearDirty(uint64_t networkId) {
    m_dirtyProperties.erase(networkId);
}

bool ReplicationManager::isDirty(uint64_t networkId) const {
    auto it = m_dirtyProperties.find(networkId);
    return it != m_dirtyProperties.end() && !it->second.empty();
}

bool ReplicationManager::isPropertyDirty(uint64_t networkId, uint32_t propertyId) const {
    auto it = m_dirtyProperties.find(networkId);
    if (it == m_dirtyProperties.end()) return false;
    return it->second.find(propertyId) != it->second.end();
}

std::vector<DirtyProperty> ReplicationManager::getDirtyProperties(uint64_t networkId) const {
    std::vector<DirtyProperty> result;

    auto it = m_dirtyProperties.find(networkId);
    if (it == m_dirtyProperties.end()) return result;

    auto entityIt = m_entities.find(networkId);
    if (entityIt == m_entities.end()) return result;

    const auto& propDefs = getPropertyDefinitions(entityIt->second.entityType);

    for (uint32_t propId : it->second) {
        for (const auto& def : propDefs) {
            if (def.id == propId) {
                DirtyProperty dp;
                dp.propertyId = propId;
                dp.propertyName = def.name;
                dp.priority = def.priority;
                dp.isReliable = def.isReliable;
                result.push_back(dp);
                break;
            }
        }
    }

    return result;
}

void ReplicationManager::replicateEntity(uint64_t networkId, bool reliable) {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    std::vector<uint8_t> buffer;
    serializeEntity(networkId, buffer);

    if (m_deltaCompressionEnabled) {
        auto prevIt = m_previousState.find(networkId);
        if (prevIt != m_previousState.end()) {
            std::vector<uint8_t> delta = computeDelta(buffer, prevIt->second);
            if (delta.size() < buffer.size()) {
                buffer = delta;
                m_stats.compressedBytes += static_cast<int>(prevIt->second.size() - delta.size());
            }
        }
        m_previousState[networkId] = buffer;
    }

    if (canSendUpdate(buffer.size())) {
        if (m_transport) {
            std::string channel = reliable ? "reliable" : "unreliable";
            m_transport->send(buffer, channel);
        }
        recordBandwidthUsage(buffer.size());
        clearDirty(networkId);
        m_lastReplicationTime[networkId] = std::chrono::steady_clock::now();

        auto& stats = m_entityStats[networkId];
        stats.bytesSent += static_cast<int>(buffer.size());
        stats.updatesSent++;
    } else {
        m_stats.droppedUpdates++;
    }
}

void ReplicationManager::replicateProperty(uint64_t networkId, uint32_t propertyId, bool reliable) {
    std::vector<uint8_t> buffer;
    serializeProperty(networkId, propertyId, buffer);

    if (canSendUpdate(buffer.size())) {
        if (m_transport) {
            std::string channel = reliable ? "reliable" : "unreliable";
            m_transport->send(buffer, channel);
        }
        recordBandwidthUsage(buffer.size());

        m_dirtyProperties[networkId].erase(propertyId);
    }
}

void ReplicationManager::replicateAll(bool reliable) {
    for (const auto& [networkId, reg] : m_entities) {
        if (reg.localRole == NetworkRole::Authority && isDirty(networkId)) {
            replicateEntity(networkId, reliable);
        }
    }
}

void ReplicationManager::forceReplication(uint64_t networkId) {
    markAllDirty(networkId);
    replicateEntity(networkId, true);
}

void ReplicationManager::setOwner(uint64_t networkId, uint64_t ownerId) {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    it->second.ownerId = ownerId;

    auto entity = it->second.entity.lock();
    if (entity) {
        entity->setOwner(ownerId);
    }
}

uint64_t ReplicationManager::getOwner(uint64_t networkId) const {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return 0;
    return it->second.ownerId;
}

bool ReplicationManager::isOwner(uint64_t networkId) const {
    return getOwner(networkId) == m_localPlayerId;
}

bool ReplicationManager::hasAuthority(uint64_t networkId) const {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return false;
    return it->second.localRole == NetworkRole::Authority;
}

void ReplicationManager::transferAuthority(uint64_t networkId, uint64_t newOwnerId) {
    setOwner(networkId, newOwnerId);

    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    if (newOwnerId == m_localPlayerId) {
        it->second.localRole = NetworkRole::Authority;
    } else {
        it->second.localRole = NetworkRole::SimulatedProxy;
    }

    // Force full replication after authority transfer
    forceReplication(networkId);
}

void ReplicationManager::setNetworkRole(uint64_t networkId, NetworkRole role) {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;
    it->second.localRole = role;
}

NetworkRole ReplicationManager::getNetworkRole(uint64_t networkId) const {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return NetworkRole::None;
    return it->second.localRole;
}

void ReplicationManager::setReplicationMode(uint64_t networkId, ReplicationMode mode) {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;
    it->second.mode = mode;
}

ReplicationMode ReplicationManager::getReplicationMode(uint64_t networkId) const {
    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return ReplicationMode::Authoritative;
    return it->second.mode;
}

void ReplicationManager::setBandwidthLimit(float bytesPerSecond) {
    m_bandwidthLimit = bytesPerSecond;
    m_stats.bandwidthLimit = bytesPerSecond;
}

void ReplicationManager::setPriorityThreshold(ReplicationPriority threshold) {
    m_priorityThreshold = threshold;
}

void ReplicationManager::storeSnapshot(uint64_t networkId) {
    auto entity = getEntity(networkId);
    if (!entity) return;

    EntitySnapshot snapshot;
    snapshot.networkId = networkId;
    snapshot.sequenceNumber = m_currentTick;
    snapshot.timestamp = std::chrono::steady_clock::now();
    serializeEntity(networkId, snapshot.data);

    auto& snapshots = m_snapshots[networkId];
    snapshots.push_back(snapshot);

    trimSnapshots(networkId);
}

EntitySnapshot ReplicationManager::getSnapshot(uint64_t networkId,
    std::chrono::steady_clock::time_point time) const {

    auto it = m_snapshots.find(networkId);
    if (it == m_snapshots.end() || it->second.empty()) {
        return EntitySnapshot();
    }

    const auto& snapshots = it->second;

    // Find two snapshots to interpolate between
    for (size_t i = 1; i < snapshots.size(); i++) {
        if (snapshots[i].timestamp >= time && snapshots[i-1].timestamp <= time) {
            // Found the right pair
            float t = std::chrono::duration<float>(time - snapshots[i-1].timestamp).count() /
                      std::chrono::duration<float>(snapshots[i].timestamp - snapshots[i-1].timestamp).count();
            return interpolateSnapshots(snapshots[i-1], snapshots[i], t);
        }
    }

    // Return nearest snapshot
    return snapshots.back();
}

void ReplicationManager::rewindTo(std::chrono::steady_clock::time_point time) {
    if (!m_lagCompensationEnabled) return;

    for (auto& [networkId, snapshots] : m_snapshots) {
        EntitySnapshot snapshot = getSnapshot(networkId, time);
        if (!snapshot.data.empty()) {
            deserializeEntity(snapshot.data);
        }
    }
}

void ReplicationManager::clearSnapshots(uint64_t networkId) {
    m_snapshots.erase(networkId);
}

void ReplicationManager::clearAllSnapshots() {
    m_snapshots.clear();
}

void ReplicationManager::createChannel(const ReplicationChannel& channel) {
    m_channels[channel.name] = channel;
}

void ReplicationManager::setEntityChannel(uint64_t networkId, const std::string& channelName) {
    m_entityChannels[networkId] = channelName;
}

EntityReplicationStats ReplicationManager::getEntityStats(uint64_t networkId) const {
    auto it = m_entityStats.find(networkId);
    if (it != m_entityStats.end()) return it->second;

    EntityReplicationStats empty;
    empty.networkId = networkId;
    return empty;
}

void ReplicationManager::resetStats() {
    m_stats = ReplicationStats();
    m_entityStats.clear();
}

void ReplicationManager::onEntitySpawn(EntitySpawnCallback callback) {
    m_spawnCallbacks.push_back(callback);
}

void ReplicationManager::onEntityDespawn(EntityDespawnCallback callback) {
    m_despawnCallbacks.push_back(callback);
}

void ReplicationManager::onPropertyUpdate(PropertyUpdateCallback callback) {
    m_propertyCallbacks.push_back(callback);
}

void ReplicationManager::setNetworkTickRate(int ticksPerSecond) {
    m_networkTickRate = std::max(1, ticksPerSecond);
}

std::string ReplicationManager::getDebugInfo() const {
    std::ostringstream info;
    info << "Replication Manager Debug Info\n";
    info << "==============================\n";
    info << "Entities: " << m_entities.size() << "\n";
    info << "Tick: " << m_currentTick << "\n";
    info << "Bandwidth: " << m_stats.bandwidthUsed << "/" << m_bandwidthLimit << " B/s\n";
    info << "Updates/sec: " << m_stats.updatesPerSecond << "\n";
    info << "Dropped: " << m_stats.droppedUpdates << "\n";
    info << "Compression ratio: " << m_stats.compressionRatio << "\n";
    return info.str();
}

// Private methods

void ReplicationManager::processOutgoingReplication() {
    // Process priority queue
    processUpdateQueue();

    // Replicate dirty entities
    for (const auto& [networkId, reg] : m_entities) {
        if (reg.localRole == NetworkRole::Authority && isDirty(networkId)) {
            auto dirtyProps = getDirtyProperties(networkId);

            for (const auto& prop : dirtyProps) {
                if (static_cast<int>(prop.priority) <= static_cast<int>(m_priorityThreshold)) {
                    queueUpdate(networkId, prop.propertyId, prop.priority);
                }
            }
        }
    }

    // Store snapshots for lag compensation
    if (m_lagCompensationEnabled) {
        for (const auto& [networkId, reg] : m_entities) {
            storeSnapshot(networkId);
        }
    }
}

void ReplicationManager::processIncomingReplication() {
    if (!m_transport) return;

    // Process received data
    std::vector<uint8_t> data;
    while (m_transport->receive(data)) {
        deserializeEntity(data);
    }
}

void ReplicationManager::serializeEntity(uint64_t networkId, std::vector<uint8_t>& buffer) {
    auto entity = getEntity(networkId);
    if (!entity) return;

    auto it = m_entities.find(networkId);
    if (it == m_entities.end()) return;

    // Header: network ID (8 bytes) + type length (2 bytes) + type
    buffer.clear();

    // Network ID
    for (int i = 0; i < 8; i++) {
        buffer.push_back(static_cast<uint8_t>((networkId >> (i * 8)) & 0xFF));
    }

    // Entity type
    const std::string& type = it->second.entityType;
    uint16_t typeLen = static_cast<uint16_t>(type.size());
    buffer.push_back(static_cast<uint8_t>(typeLen & 0xFF));
    buffer.push_back(static_cast<uint8_t>((typeLen >> 8) & 0xFF));
    buffer.insert(buffer.end(), type.begin(), type.end());

    // Serialize each property
    const auto& propDefs = getPropertyDefinitions(type);
    for (const auto& prop : propDefs) {
        if (isPropertyDirty(networkId, prop.id)) {
            // Property ID
            buffer.push_back(static_cast<uint8_t>(prop.id & 0xFF));
            buffer.push_back(static_cast<uint8_t>((prop.id >> 8) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((prop.id >> 16) & 0xFF));
            buffer.push_back(static_cast<uint8_t>((prop.id >> 24) & 0xFF));

            // Property data (using entity's serialize function)
            std::vector<uint8_t> propData = entity->serializeProperty(prop.id);
            uint16_t propLen = static_cast<uint16_t>(propData.size());
            buffer.push_back(static_cast<uint8_t>(propLen & 0xFF));
            buffer.push_back(static_cast<uint8_t>((propLen >> 8) & 0xFF));
            buffer.insert(buffer.end(), propData.begin(), propData.end());
        }
    }

    m_stats.totalBytesSent += static_cast<int>(buffer.size());
}

void ReplicationManager::deserializeEntity(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 10) return;

    size_t offset = 0;

    // Read network ID
    uint64_t networkId = 0;
    for (int i = 0; i < 8; i++) {
        networkId |= static_cast<uint64_t>(buffer[offset + i]) << (i * 8);
    }
    offset += 8;

    // Read entity type
    uint16_t typeLen = buffer[offset] | (buffer[offset + 1] << 8);
    offset += 2;

    if (offset + typeLen > buffer.size()) return;

    std::string type(buffer.begin() + offset, buffer.begin() + offset + typeLen);
    offset += typeLen;

    // Get or create entity
    auto entity = getEntity(networkId);
    if (!entity) {
        // Entity doesn't exist locally - would need to spawn it
        // This depends on the game's entity factory
        return;
    }

    // Deserialize properties
    while (offset + 6 <= buffer.size()) {
        uint32_t propId = buffer[offset] |
                          (buffer[offset + 1] << 8) |
                          (buffer[offset + 2] << 16) |
                          (buffer[offset + 3] << 24);
        offset += 4;

        uint16_t propLen = buffer[offset] | (buffer[offset + 1] << 8);
        offset += 2;

        if (offset + propLen > buffer.size()) break;

        std::vector<uint8_t> propData(buffer.begin() + offset, buffer.begin() + offset + propLen);
        offset += propLen;

        entity->deserializeProperty(propId, propData);

        // Notify callbacks
        for (auto& callback : m_propertyCallbacks) {
            callback(networkId, propId);
        }
    }

    m_stats.totalBytesReceived += static_cast<int>(buffer.size());

    auto& stats = m_entityStats[networkId];
    stats.bytesReceived += static_cast<int>(buffer.size());
    stats.updatesReceived++;
}

void ReplicationManager::serializeProperty(uint64_t networkId, uint32_t propertyId,
                                            std::vector<uint8_t>& buffer) {
    auto entity = getEntity(networkId);
    if (!entity) return;

    buffer.clear();

    // Network ID
    for (int i = 0; i < 8; i++) {
        buffer.push_back(static_cast<uint8_t>((networkId >> (i * 8)) & 0xFF));
    }

    // Property ID
    buffer.push_back(static_cast<uint8_t>(propertyId & 0xFF));
    buffer.push_back(static_cast<uint8_t>((propertyId >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((propertyId >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((propertyId >> 24) & 0xFF));

    // Property data
    std::vector<uint8_t> propData = entity->serializeProperty(propertyId);
    uint16_t propLen = static_cast<uint16_t>(propData.size());
    buffer.push_back(static_cast<uint8_t>(propLen & 0xFF));
    buffer.push_back(static_cast<uint8_t>((propLen >> 8) & 0xFF));
    buffer.insert(buffer.end(), propData.begin(), propData.end());
}

void ReplicationManager::deserializeProperty(uint64_t networkId, const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 14) return;

    size_t offset = 8;  // Skip network ID

    uint32_t propId = buffer[offset] |
                      (buffer[offset + 1] << 8) |
                      (buffer[offset + 2] << 16) |
                      (buffer[offset + 3] << 24);
    offset += 4;

    uint16_t propLen = buffer[offset] | (buffer[offset + 1] << 8);
    offset += 2;

    if (offset + propLen > buffer.size()) return;

    std::vector<uint8_t> propData(buffer.begin() + offset, buffer.begin() + offset + propLen);

    auto entity = getEntity(networkId);
    if (entity) {
        entity->deserializeProperty(propId, propData);
    }
}

std::vector<uint8_t> ReplicationManager::computeDelta(const std::vector<uint8_t>& current,
                                                       const std::vector<uint8_t>& previous) {
    std::vector<uint8_t> delta;

    // Simple XOR-based delta compression
    size_t maxLen = std::max(current.size(), previous.size());
    delta.reserve(maxLen + 4);

    // Store original size
    uint32_t originalSize = static_cast<uint32_t>(current.size());
    delta.push_back(static_cast<uint8_t>(originalSize & 0xFF));
    delta.push_back(static_cast<uint8_t>((originalSize >> 8) & 0xFF));
    delta.push_back(static_cast<uint8_t>((originalSize >> 16) & 0xFF));
    delta.push_back(static_cast<uint8_t>((originalSize >> 24) & 0xFF));

    // XOR with previous state
    for (size_t i = 0; i < current.size(); i++) {
        uint8_t prev = i < previous.size() ? previous[i] : 0;
        delta.push_back(current[i] ^ prev);
    }

    return delta;
}

std::vector<uint8_t> ReplicationManager::applyDelta(const std::vector<uint8_t>& previous,
                                                     const std::vector<uint8_t>& delta) {
    if (delta.size() < 4) return previous;

    uint32_t originalSize = delta[0] |
                            (delta[1] << 8) |
                            (delta[2] << 16) |
                            (delta[3] << 24);

    std::vector<uint8_t> result;
    result.reserve(originalSize);

    for (size_t i = 4; i < delta.size() && result.size() < originalSize; i++) {
        uint8_t prev = (i - 4) < previous.size() ? previous[i - 4] : 0;
        result.push_back(delta[i] ^ prev);
    }

    return result;
}

void ReplicationManager::queueUpdate(uint64_t networkId, uint32_t propertyId,
                                      ReplicationPriority priority) {
    PendingUpdate update;
    update.networkId = networkId;
    update.propertyId = propertyId;
    update.priority = priority;
    update.queueTime = std::chrono::steady_clock::now();
    update.isReliable = priority <= ReplicationPriority::High;

    m_updateQueue.push(update);
}

void ReplicationManager::processUpdateQueue() {
    while (!m_updateQueue.empty()) {
        PendingUpdate update = m_updateQueue.top();

        // Check if we have bandwidth
        // Estimate size (rough)
        size_t estimatedSize = 20;  // Header + property overhead
        if (!canSendUpdate(estimatedSize)) {
            break;  // Stop processing, bandwidth limit reached
        }

        m_updateQueue.pop();

        replicateProperty(update.networkId, update.propertyId, update.isReliable);
    }
}

bool ReplicationManager::canSendUpdate(size_t bytes) {
    return m_stats.bandwidthUsed + bytes <= m_bandwidthLimit || m_bandwidthLimit <= 0;
}

void ReplicationManager::recordBandwidthUsage(size_t bytes) {
    m_stats.bandwidthUsed += static_cast<float>(bytes);
    // Note: In a real implementation, bandwidth would decay over time
}

void ReplicationManager::trimSnapshots(uint64_t networkId) {
    auto& snapshots = m_snapshots[networkId];

    while (snapshots.size() > MAX_SNAPSHOTS_PER_ENTITY) {
        snapshots.erase(snapshots.begin());
    }

    // Also remove old snapshots beyond max lag compensation time
    auto cutoff = std::chrono::steady_clock::now() -
                  std::chrono::milliseconds(static_cast<int>(m_maxLagCompensation * 1000));

    snapshots.erase(
        std::remove_if(snapshots.begin(), snapshots.end(),
            [cutoff](const EntitySnapshot& s) { return s.timestamp < cutoff; }),
        snapshots.end());
}

EntitySnapshot ReplicationManager::interpolateSnapshots(const EntitySnapshot& a,
                                                         const EntitySnapshot& b, float t) const {
    EntitySnapshot result;
    result.networkId = a.networkId;
    result.timestamp = a.timestamp + std::chrono::milliseconds(
        static_cast<int>(t * std::chrono::duration<float>(b.timestamp - a.timestamp).count() * 1000));

    // For now, just use snapshot b if t > 0.5, otherwise a
    // Real interpolation would interpolate individual properties
    result.data = t > 0.5f ? b.data : a.data;

    return result;
}

uint64_t ReplicationManager::generateNetworkId() {
    return m_nextNetworkId++;
}

} // namespace Replication
} // namespace Network
