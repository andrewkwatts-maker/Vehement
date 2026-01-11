#include "ReplicationSystem.hpp"
#include "FirebaseClient.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// NetworkEvent Implementation
// ============================================================================

void NetworkEvent::SetProperty(const std::string& name, const EventValue& value) {
    for (auto& prop : properties) {
        if (prop.name == name) {
            prop.value = value;
            prop.dirty = true;
            return;
        }
    }
    properties.push_back({name, value, true});
}

EventValue NetworkEvent::GetProperty(const std::string& name) const {
    for (const auto& prop : properties) {
        if (prop.name == name) {
            return prop.value;
        }
    }
    return std::monostate{};
}

bool NetworkEvent::HasProperty(const std::string& name) const {
    for (const auto& prop : properties) {
        if (prop.name == name) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// EventTypeRegistry Implementation
// ============================================================================

EventTypeRegistry& EventTypeRegistry::Instance() {
    static EventTypeRegistry instance;
    return instance;
}

void EventTypeRegistry::RegisterType(const EventTypeConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_types[config.typeName] = config;
}

void EventTypeRegistry::UnregisterType(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_types.erase(typeName);
}

const EventTypeConfig* EventTypeRegistry::GetConfig(const std::string& typeName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_types.find(typeName);
    return it != m_types.end() ? &it->second : nullptr;
}

std::vector<std::string> EventTypeRegistry::GetTypesByCategory(ReplicationCategory category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    for (const auto& [name, config] : m_types) {
        if (config.defaultCategory == category) {
            result.push_back(name);
        }
    }
    return result;
}

void EventTypeRegistry::SetOverride(const std::string& typeName, const std::string& property, const EventValue& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_overrides[typeName][property] = value;
}

void EventTypeRegistry::ClearOverrides(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_overrides.erase(typeName);
}

// ============================================================================
// ReplicationSystem Implementation
// ============================================================================

ReplicationSystem& ReplicationSystem::Instance() {
    static ReplicationSystem instance;
    return instance;
}

void ReplicationSystem::Initialize(const Config& config) {
    m_config = config;

    // Register default event types
    RegisterDefaultEventTypes();

    m_initialized = true;
}

void ReplicationSystem::Shutdown() {
    Disconnect();
    m_initialized = false;
}

void ReplicationSystem::Update(float deltaTime) {
    if (!m_initialized) return;

    // Process incoming network packets
    ProcessIncomingPackets();

    // Process local events
    m_syncTimer += deltaTime;
    if (m_syncTimer >= m_config.syncInterval) {
        m_syncTimer = 0.0f;
        SendOutgoingPackets();
    }

    // Send heartbeats
    m_heartbeatTimer += deltaTime;
    if (m_heartbeatTimer >= m_config.heartbeatInterval) {
        m_heartbeatTimer = 0.0f;
        SendHeartbeats();
        CheckTimeouts();
    }

    // Process Firebase queue
    ProcessFirebaseQueue();

    // Reset per-second rate limiting counters
    static float rateLimitTimer = 0.0f;
    rateLimitTimer += deltaTime;
    if (rateLimitTimer >= 1.0f) {
        rateLimitTimer = 0.0f;
        m_eventCountPerSecond.clear();
    }
}

// ============================================================================
// Event Dispatch
// ============================================================================

uint64_t ReplicationSystem::DispatchEvent(NetworkEvent event) {
    if (!m_initialized) return 0;

    // Assign event ID and timestamp
    event.eventId = m_nextEventId++;
    event.timestamp = GetServerTime();
    event.sourceClientId = m_config.localClientId;

    // Get type config and apply defaults
    if (const auto* config = EventTypeRegistry::Instance().GetConfig(event.eventType)) {
        if (event.category == ReplicationCategory::Custom) {
            event.category = config->defaultCategory;
        }
        if (event.replicationMode == ReplicationMode::ToAll) {
            event.replicationMode = config->defaultReplicationMode;
        }
        if (event.persistenceMode == PersistenceMode::None) {
            event.persistenceMode = config->defaultPersistenceMode;
        }
        if (event.reliabilityMode == ReliabilityMode::Reliable) {
            event.reliabilityMode = config->defaultReliabilityMode;
        }
        if (event.priority == EventPriority::Normal) {
            event.priority = config->defaultPriority;
        }

        // Validate event
        if (config->validator && !config->validator(event)) {
            m_stats.eventsDropped++;
            return 0;
        }

        // Preprocess event
        if (config->preprocessor) {
            config->preprocessor(event);
        }
    }

    // Check rate limiting
    if (!CheckRateLimit(event.eventType, event.sourceClientId)) {
        m_stats.eventsDropped++;
        return 0;
    }

    // Validate event
    if (!ValidateEvent(event)) {
        m_stats.eventsDropped++;
        return 0;
    }

    // Process locally first
    ProcessLocalEvent(event);

    // Handle replication
    if (event.replicationMode != ReplicationMode::None) {
        SendEventToNetwork(event);
    }

    // Handle persistence
    if (event.persistenceMode != PersistenceMode::None) {
        PersistEvent(event);
    }

    if (OnEventSent) OnEventSent(event);
    m_stats.eventsSent++;
    m_stats.eventCountByType[event.eventType]++;

    return event.eventId;
}

uint64_t ReplicationSystem::DispatchWithSettings(
    const std::string& eventType,
    ReplicationMode replication,
    PersistenceMode persistence,
    const std::vector<EventProperty>& properties
) {
    NetworkEvent event;
    event.eventType = eventType;
    event.replicationMode = replication;
    event.persistenceMode = persistence;
    event.properties = properties;
    return DispatchEvent(std::move(event));
}

// ============================================================================
// Event Subscription
// ============================================================================

uint64_t ReplicationSystem::Subscribe(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.eventType = eventType;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::Subscribe(ReplicationCategory category, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.category = category;
    sub.categoryFilter = true;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::SubscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

uint64_t ReplicationSystem::SubscribeFiltered(EventFilter filter, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.filter = std::move(filter);
    sub.handler = std::move(handler);
    m_subscriptions.push_back(std::move(sub));

    return sub.id;
}

void ReplicationSystem::Unsubscribe(uint64_t subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    m_subscriptions.erase(
        std::remove_if(m_subscriptions.begin(), m_subscriptions.end(),
            [subscriptionId](const Subscription& s) { return s.id == subscriptionId; }),
        m_subscriptions.end()
    );
}

// ============================================================================
// Connection Management
// ============================================================================

bool ReplicationSystem::StartHost(uint16_t port) {
    m_config.isHost = true;
    m_config.localClientId = 1;  // Host is always client ID 1

    // Add self as connection
    ConnectionInfo self;
    self.clientId = m_config.localClientId;
    self.isHost = true;
    self.isLocal = true;
    self.lastHeartbeat = GetServerTime();

    {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        m_connections[self.clientId] = self;
    }

    m_connected = true;

    // TODO: Start actual network listening

    return true;
}

bool ReplicationSystem::Connect(const std::string& address, uint16_t port) {
    // TODO: Implement actual network connection

    m_connected = true;

    if (OnConnectedToHost) OnConnectedToHost();

    // Request full state sync
    RequestFullSync();

    return true;
}

void ReplicationSystem::Disconnect() {
    if (!m_connected) return;

    // Notify subscribers
    if (m_config.isHost) {
        // Notify all clients
        for (const auto& [clientId, conn] : m_connections) {
            if (!conn.isLocal && OnClientDisconnected) {
                OnClientDisconnected(clientId);
            }
        }
    } else {
        if (OnDisconnectedFromHost) OnDisconnectedFromHost();
    }

    m_connected = false;
    m_connections.clear();
}

const ConnectionInfo* ReplicationSystem::GetConnection(uint32_t clientId) const {
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    auto it = m_connections.find(clientId);
    return it != m_connections.end() ? &it->second : nullptr;
}

std::vector<ConnectionInfo> ReplicationSystem::GetAllConnections() const {
    std::lock_guard<std::mutex> lock(m_connectionMutex);
    std::vector<ConnectionInfo> result;
    result.reserve(m_connections.size());
    for (const auto& [id, conn] : m_connections) {
        result.push_back(conn);
    }
    return result;
}

// ============================================================================
// Entity Ownership
// ============================================================================

void ReplicationSystem::SetEntityOwner(uint64_t entityId, uint32_t clientId) {
    std::lock_guard<std::mutex> lock(m_ownershipMutex);
    m_entityOwnership[entityId] = clientId;

    // Update connection's owned entities
    std::lock_guard<std::mutex> connLock(m_connectionMutex);
    for (auto& [id, conn] : m_connections) {
        conn.ownedEntities.erase(entityId);
    }
    if (auto it = m_connections.find(clientId); it != m_connections.end()) {
        it->second.ownedEntities.insert(entityId);
    }
}

uint32_t ReplicationSystem::GetEntityOwner(uint64_t entityId) const {
    std::lock_guard<std::mutex> lock(m_ownershipMutex);
    auto it = m_entityOwnership.find(entityId);
    return it != m_entityOwnership.end() ? it->second : 0;
}

bool ReplicationSystem::IsLocallyOwned(uint64_t entityId) const {
    return GetEntityOwner(entityId) == m_config.localClientId;
}

void ReplicationSystem::RequestOwnership(uint64_t entityId) {
    NetworkEvent event;
    event.eventType = "system.ownership.request";
    event.targetEntityId = entityId;
    event.replicationMode = ReplicationMode::ToHost;
    DispatchEvent(std::move(event));
}

// ============================================================================
// Firebase Persistence
// ============================================================================

void ReplicationSystem::SetFirebaseClient(std::shared_ptr<FirebaseClient> client) {
    m_firebaseClient = std::move(client);
}

void ReplicationSystem::PersistToFirebase(const NetworkEvent& event) {
    if (!m_firebaseClient) return;

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_firebaseQueue.push(event);
}

void ReplicationSystem::LoadFromFirebase(const std::string& path, std::function<void(std::vector<NetworkEvent>)> callback) {
    if (!m_firebaseClient) {
        callback({});
        return;
    }

    // TODO: Implement Firebase loading
    callback({});
}

void ReplicationSystem::SyncTerrainToFirebase() {
    if (!m_firebaseClient) return;

    // This would be called periodically to sync terrain changes
    // TODO: Implement terrain sync
}

// ============================================================================
// State Synchronization
// ============================================================================

void ReplicationSystem::RequestFullSync() {
    NetworkEvent event;
    event.eventType = "system.sync.request";
    event.replicationMode = ReplicationMode::ToHost;
    DispatchEvent(std::move(event));
}

void ReplicationSystem::SendFullStateTo(uint32_t clientId) {
    // This would serialize all current state and send to client
    // TODO: Implement full state serialization
}

uint64_t ReplicationSystem::GetServerTime() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() + m_serverTimeOffset;
}

// ============================================================================
// Statistics
// ============================================================================

void ReplicationSystem::ResetStats() {
    m_stats = ReplicationStats{};
}

// ============================================================================
// Editor Support
// ============================================================================

std::vector<std::string> ReplicationSystem::GetRegisteredEventTypes() const {
    std::vector<std::string> types;
    // Would iterate through EventTypeRegistry
    return types;
}

void ReplicationSystem::SetEventTypeOverride(const std::string& eventType, ReplicationMode mode, PersistenceMode persistence) {
    EventTypeRegistry::Instance().SetOverride(eventType, "replicationMode", static_cast<int32_t>(mode));
    EventTypeRegistry::Instance().SetOverride(eventType, "persistenceMode", static_cast<int32_t>(persistence));
}

void ReplicationSystem::ClearEventTypeOverride(const std::string& eventType) {
    EventTypeRegistry::Instance().ClearOverrides(eventType);
}

// ============================================================================
// Private Methods
// ============================================================================

void ReplicationSystem::ProcessLocalEvent(const NetworkEvent& event) {
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);

    for (const auto& sub : m_subscriptions) {
        bool matches = false;

        if (sub.filter) {
            matches = sub.filter(event);
        } else if (sub.categoryFilter) {
            matches = event.category == sub.category;
        } else if (!sub.eventType.empty()) {
            matches = event.eventType == sub.eventType;
        } else {
            matches = true;  // SubscribeAll
        }

        if (matches && sub.handler) {
            sub.handler(event);
        }
    }
}

void ReplicationSystem::ProcessRemoteEvent(const NetworkEvent& event) {
    m_stats.eventsReceived++;

    if (OnEventReceived) OnEventReceived(event);

    // Process like local event
    ProcessLocalEvent(event);
}

void ReplicationSystem::SendEventToNetwork(const NetworkEvent& event) {
    if (!m_connected) return;

    switch (event.replicationMode) {
        case ReplicationMode::None:
            break;

        case ReplicationMode::ToHost:
            if (!m_config.isHost) {
                SendEventTo(event, 1);  // Host is always ID 1
            }
            break;

        case ReplicationMode::ToClients:
            if (m_config.isHost) {
                BroadcastEvent(event);
            }
            break;

        case ReplicationMode::ToAll:
            BroadcastEvent(event);
            break;

        case ReplicationMode::ToOwner:
            if (event.targetEntityId != 0) {
                uint32_t owner = GetEntityOwner(event.targetEntityId);
                if (owner != 0 && owner != m_config.localClientId) {
                    SendEventTo(event, owner);
                }
            }
            break;

        case ReplicationMode::ToServer:
            if (m_config.isDedicatedServer) {
                // Already on server
            } else {
                SendEventTo(event, 1);
            }
            break;

        case ReplicationMode::Multicast:
            for (uint32_t clientId : event.targetClients) {
                if (clientId != m_config.localClientId) {
                    SendEventTo(event, clientId);
                }
            }
            break;
    }
}

void ReplicationSystem::BroadcastEvent(const NetworkEvent& event) {
    std::lock_guard<std::mutex> lock(m_connectionMutex);

    for (const auto& [clientId, conn] : m_connections) {
        if (!conn.isLocal) {
            SendEventTo(event, clientId);
        }
    }
}

void ReplicationSystem::SendEventTo(const NetworkEvent& event, uint32_t clientId) {
    std::vector<uint8_t> data = SerializeEvent(event);
    m_stats.bytesOut += data.size();

    // TODO: Actually send over network
}

bool ReplicationSystem::ValidateEvent(const NetworkEvent& event) const {
    // Check ownership requirements
    if (const auto* config = EventTypeRegistry::Instance().GetConfig(event.eventType)) {
        if (config->requiresOwnership && event.sourceEntityId != 0) {
            if (!IsLocallyOwned(event.sourceEntityId)) {
                return false;
            }
        }

        if (config->requiresHost && !m_config.isHost) {
            return false;
        }

        if (!config->allowFromClient && !m_config.isHost) {
            return false;
        }
    }

    return true;
}

bool ReplicationSystem::CheckRateLimit(const std::string& eventType, uint32_t clientId) {
    const auto* config = EventTypeRegistry::Instance().GetConfig(eventType);
    if (!config) return true;

    auto now = std::chrono::steady_clock::now();

    // Check minimum interval
    if (config->minInterval > 0.0f) {
        auto& lastTime = m_lastEventTime[eventType][clientId];
        auto elapsed = std::chrono::duration<float>(now - lastTime).count();
        if (elapsed < config->minInterval) {
            return false;
        }
        lastTime = now;
    }

    // Check max per second
    if (config->maxPerSecond > 0) {
        auto& count = m_eventCountPerSecond[eventType][clientId];
        if (count >= config->maxPerSecond) {
            return false;
        }
        count++;
    }

    return true;
}

std::vector<uint8_t> ReplicationSystem::SerializeEvent(const NetworkEvent& event) const {
    std::vector<uint8_t> data;

    // Simple serialization - would use proper binary format in production
    auto writeU64 = [&data](uint64_t v) {
        for (int i = 0; i < 8; i++) {
            data.push_back(static_cast<uint8_t>(v >> (i * 8)));
        }
    };

    auto writeU32 = [&data](uint32_t v) {
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(v >> (i * 8)));
        }
    };

    auto writeString = [&data, &writeU32](const std::string& s) {
        writeU32(static_cast<uint32_t>(s.size()));
        for (char c : s) {
            data.push_back(static_cast<uint8_t>(c));
        }
    };

    writeU64(event.eventId);
    writeU64(event.sourceEntityId);
    writeU64(event.targetEntityId);
    writeU32(event.sourceClientId);
    writeString(event.eventType);
    data.push_back(static_cast<uint8_t>(event.category));
    data.push_back(static_cast<uint8_t>(event.replicationMode));
    data.push_back(static_cast<uint8_t>(event.persistenceMode));
    data.push_back(static_cast<uint8_t>(event.reliabilityMode));
    data.push_back(static_cast<uint8_t>(event.priority));
    writeU64(event.timestamp);

    // Properties
    writeU32(static_cast<uint32_t>(event.properties.size()));

    auto writeFloat = [&data](float v) {
        uint32_t bits;
        memcpy(&bits, &v, sizeof(bits));
        for (int i = 0; i < 4; i++) {
            data.push_back(static_cast<uint8_t>(bits >> (i * 8)));
        }
    };

    auto writeVec3 = [&writeFloat](const glm::vec3& v) {
        writeFloat(v.x);
        writeFloat(v.y);
        writeFloat(v.z);
    };

    auto writeVec4 = [&writeFloat](const glm::vec4& v) {
        writeFloat(v.x);
        writeFloat(v.y);
        writeFloat(v.z);
        writeFloat(v.w);
    };

    for (const auto& prop : event.properties) {
        writeString(prop.name);

        // Write type index and value
        size_t typeIndex = prop.value.index();
        data.push_back(static_cast<uint8_t>(typeIndex));

        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                // Nothing to write
            } else if constexpr (std::is_same_v<T, bool>) {
                data.push_back(arg ? 1 : 0);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                writeU32(static_cast<uint32_t>(arg));
            } else if constexpr (std::is_same_v<T, int64_t>) {
                writeU64(static_cast<uint64_t>(arg));
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                writeU32(arg);
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                writeU64(arg);
            } else if constexpr (std::is_same_v<T, float>) {
                writeFloat(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                writeFloat(static_cast<float>(arg));
                writeFloat(static_cast<float>(arg)); // Extra precision lost, stored twice for alignment
            } else if constexpr (std::is_same_v<T, std::string>) {
                writeString(arg);
            } else if constexpr (std::is_same_v<T, glm::vec2>) {
                writeFloat(arg.x);
                writeFloat(arg.y);
            } else if constexpr (std::is_same_v<T, glm::vec3>) {
                writeVec3(arg);
            } else if constexpr (std::is_same_v<T, glm::vec4>) {
                writeVec4(arg);
            } else if constexpr (std::is_same_v<T, glm::quat>) {
                writeVec4(glm::vec4(arg.x, arg.y, arg.z, arg.w));
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                writeU32(static_cast<uint32_t>(arg.size()));
                for (uint8_t b : arg) {
                    data.push_back(b);
                }
            }
        }, prop.value);
    }

    return data;
}

NetworkEvent ReplicationSystem::DeserializeEvent(const std::vector<uint8_t>& data) const {
    NetworkEvent event;
    if (data.size() < 40) return event;  // Minimum size check

    size_t offset = 0;

    auto readU64 = [&data, &offset]() -> uint64_t {
        if (offset + 8 > data.size()) return 0;
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v |= static_cast<uint64_t>(data[offset++]) << (i * 8);
        }
        return v;
    };

    auto readU32 = [&data, &offset]() -> uint32_t {
        if (offset + 4 > data.size()) return 0;
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) {
            v |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        return v;
    };

    auto readU8 = [&data, &offset]() -> uint8_t {
        if (offset >= data.size()) return 0;
        return data[offset++];
    };

    auto readFloat = [&data, &offset]() -> float {
        if (offset + 4 > data.size()) return 0.0f;
        uint32_t bits = 0;
        for (int i = 0; i < 4; i++) {
            bits |= static_cast<uint32_t>(data[offset++]) << (i * 8);
        }
        float v;
        memcpy(&v, &bits, sizeof(v));
        return v;
    };

    auto readString = [&data, &offset, &readU32]() -> std::string {
        uint32_t len = readU32();
        if (offset + len > data.size()) return "";
        std::string s(reinterpret_cast<const char*>(&data[offset]), len);
        offset += len;
        return s;
    };

    event.eventId = readU64();
    event.sourceEntityId = readU64();
    event.targetEntityId = readU64();
    event.sourceClientId = readU32();
    event.eventType = readString();
    event.category = static_cast<ReplicationCategory>(readU8());
    event.replicationMode = static_cast<ReplicationMode>(readU8());
    event.persistenceMode = static_cast<PersistenceMode>(readU8());
    event.reliabilityMode = static_cast<ReliabilityMode>(readU8());
    event.priority = static_cast<EventPriority>(readU8());
    event.timestamp = readU64();

    // Read properties
    uint32_t propCount = readU32();
    for (uint32_t i = 0; i < propCount && offset < data.size(); i++) {
        EventProperty prop;
        prop.name = readString();
        uint8_t typeIndex = readU8();

        switch (typeIndex) {
            case 0: prop.value = std::monostate{}; break;
            case 1: prop.value = readU8() != 0; break;
            case 2: prop.value = static_cast<int32_t>(readU32()); break;
            case 3: prop.value = static_cast<int64_t>(readU64()); break;
            case 4: prop.value = readU32(); break;
            case 5: prop.value = readU64(); break;
            case 6: prop.value = readFloat(); break;
            case 7: {
                float f1 = readFloat();
                readFloat(); // Skip second float
                prop.value = static_cast<double>(f1);
                break;
            }
            case 8: prop.value = readString(); break;
            case 9: prop.value = glm::vec2(readFloat(), readFloat()); break;
            case 10: prop.value = glm::vec3(readFloat(), readFloat(), readFloat()); break;
            case 11: prop.value = glm::vec4(readFloat(), readFloat(), readFloat(), readFloat()); break;
            case 12: {
                float x = readFloat(), y = readFloat(), z = readFloat(), w = readFloat();
                prop.value = glm::quat(w, x, y, z);
                break;
            }
            case 13: {
                uint32_t size = readU32();
                std::vector<uint8_t> bytes;
                bytes.reserve(size);
                for (uint32_t j = 0; j < size && offset < data.size(); j++) {
                    bytes.push_back(data[offset++]);
                }
                prop.value = std::move(bytes);
                break;
            }
            default: break;
        }

        prop.dirty = false;
        event.properties.push_back(std::move(prop));
    }

    return event;
}

void ReplicationSystem::ProcessIncomingPackets() {
    // TODO: Process actual network packets
    // For now, process queued incoming events

    std::lock_guard<std::mutex> lock(m_eventMutex);
    while (!m_incomingEvents.empty()) {
        NetworkEvent event = std::move(m_incomingEvents.front());
        m_incomingEvents.pop();
        ProcessRemoteEvent(event);
    }
}

void ReplicationSystem::SendOutgoingPackets() {
    std::lock_guard<std::mutex> lock(m_eventMutex);
    while (!m_outgoingEvents.empty()) {
        NetworkEvent event = std::move(m_outgoingEvents.front());
        m_outgoingEvents.pop();
        SendEventToNetwork(event);
    }
}

void ReplicationSystem::SendHeartbeats() {
    if (!m_connected) return;

    NetworkEvent heartbeat;
    heartbeat.eventType = "system.heartbeat";
    heartbeat.replicationMode = ReplicationMode::ToAll;
    heartbeat.reliabilityMode = ReliabilityMode::Unreliable;
    heartbeat.priority = EventPriority::Low;
    DispatchEvent(std::move(heartbeat));
}

void ReplicationSystem::CheckTimeouts() {
    if (!m_config.isHost) return;

    uint64_t now = GetServerTime();
    const uint64_t timeoutMs = 5000;  // 5 second timeout

    std::vector<uint32_t> timedOut;

    {
        std::lock_guard<std::mutex> lock(m_connectionMutex);
        for (const auto& [clientId, conn] : m_connections) {
            if (!conn.isLocal && (now - conn.lastHeartbeat) > timeoutMs) {
                timedOut.push_back(clientId);
            }
        }

        for (uint32_t clientId : timedOut) {
            m_connections.erase(clientId);
        }
    }

    for (uint32_t clientId : timedOut) {
        if (OnClientDisconnected) OnClientDisconnected(clientId);
    }
}

void ReplicationSystem::PersistEvent(const NetworkEvent& event) {
    switch (event.persistenceMode) {
        case PersistenceMode::None:
            break;

        case PersistenceMode::Firebase:
            PersistToFirebase(event);
            break;

        case PersistenceMode::LocalFile:
            // TODO: Implement local file persistence
            break;

        case PersistenceMode::Both:
            PersistToFirebase(event);
            // TODO: Also persist to local file
            break;
    }

    m_stats.eventsPersisted++;
    if (OnEventPersisted) OnEventPersisted(event);
}

void ReplicationSystem::ProcessFirebaseQueue() {
    if (!m_firebaseClient) return;

    // Process a few events per frame
    std::lock_guard<std::mutex> lock(m_eventMutex);
    int processed = 0;
    while (!m_firebaseQueue.empty() && processed < 5) {
        NetworkEvent event = std::move(m_firebaseQueue.front());
        m_firebaseQueue.pop();

        // Build Firebase path based on event type
        std::string path = "events/" + event.eventType + "/" + std::to_string(event.eventId);

        // Serialize event to JSON
        nlohmann::json eventJson;
        eventJson["eventId"] = event.eventId;
        eventJson["sourceEntityId"] = event.sourceEntityId;
        eventJson["targetEntityId"] = event.targetEntityId;
        eventJson["sourceClientId"] = event.sourceClientId;
        eventJson["eventType"] = event.eventType;
        eventJson["category"] = static_cast<int>(event.category);
        eventJson["timestamp"] = event.timestamp;

        // Serialize properties
        nlohmann::json propsJson = nlohmann::json::object();
        for (const auto& prop : event.properties) {
            std::visit([&propsJson, &prop](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    propsJson[prop.name] = nullptr;
                } else if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, int32_t> ||
                                     std::is_same_v<T, int64_t> || std::is_same_v<T, uint32_t> ||
                                     std::is_same_v<T, uint64_t> || std::is_same_v<T, float> ||
                                     std::is_same_v<T, double> || std::is_same_v<T, std::string>) {
                    propsJson[prop.name] = arg;
                } else if constexpr (std::is_same_v<T, glm::vec2>) {
                    propsJson[prop.name] = {arg.x, arg.y};
                } else if constexpr (std::is_same_v<T, glm::vec3>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z};
                } else if constexpr (std::is_same_v<T, glm::vec4>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                } else if constexpr (std::is_same_v<T, glm::quat>) {
                    propsJson[prop.name] = {arg.x, arg.y, arg.z, arg.w};
                } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                    propsJson[prop.name] = nlohmann::json::array();
                }
            }, prop.value);
        }
        eventJson["properties"] = propsJson;

        // Push to Firebase asynchronously
        m_firebaseClient->Set(path, eventJson, [](const FirebaseResult& result) {
            if (!result.success) {
                (void)result.errorMessage; // Logging would go here
            }
        });

        processed++;
    }
}

// ============================================================================
// Default Event Type Registration
// ============================================================================

void ReplicationSystem::RegisterDefaultEventTypes() {
    auto& registry = EventTypeRegistry::Instance();

    // Input events - high frequency, unreliable, no persistence
    registry.RegisterType({
        Events::INPUT_MOVE,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::High,
        0.0f, 60,  // Max 60 per second
        true, false, true
    });

    registry.RegisterType({
        Events::INPUT_LOOK,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::Normal,
        0.0f, 30,
        true, false, true
    });

    registry.RegisterType({
        Events::INPUT_ACTION,
        ReplicationCategory::Input,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 20,
        false, false, true
    });

    // Entity events - reliable
    registry.RegisterType({
        Events::ENTITY_SPAWN,
        ReplicationCategory::EntitySpawn,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::High,
        0.0f, 0,
        false, true, false  // Host only
    });

    registry.RegisterType({
        Events::ENTITY_DESTROY,
        ReplicationCategory::EntitySpawn,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::High,
        0.0f, 0,
        false, true, false
    });

    registry.RegisterType({
        Events::ENTITY_MOVE,
        ReplicationCategory::EntityMovement,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Unreliable,
        EventPriority::Normal,
        0.0f, 30,
        true, false, true
    });

    registry.RegisterType({
        Events::ENTITY_STATE,
        ReplicationCategory::EntityState,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.0f, 0,
        true, false, true
    });

    // Combat events
    registry.RegisterType({
        Events::COMBAT_ATTACK,
        ReplicationCategory::Combat,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.1f, 10,  // Rate limited
        true, false, true
    });

    registry.RegisterType({
        Events::COMBAT_DAMAGE,
        ReplicationCategory::Combat,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        false, true, false  // Host only
    });

    // Ability events
    registry.RegisterType({
        Events::ABILITY_USE,
        ReplicationCategory::Abilities,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        true, false, true
    });

    // Building events
    registry.RegisterType({
        Events::BUILDING_PLACE,
        ReplicationCategory::Building,
        ReplicationMode::ToHost,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.5f, 2,
        true, false, true
    });

    registry.RegisterType({
        Events::BUILDING_COMPLETE,
        ReplicationCategory::Building,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.0f, 0,
        false, true, false
    });

    // Terrain events - PERSIST TO FIREBASE
    registry.RegisterType({
        Events::TERRAIN_MODIFY,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,  // <-- Key: persist terrain changes
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.1f, 10,
        false, true, false  // Host only can modify
    });

    registry.RegisterType({
        Events::TERRAIN_SCULPT,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.05f, 20,
        false, true, false
    });

    registry.RegisterType({
        Events::TERRAIN_TUNNEL,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.5f, 2,
        false, true, false
    });

    registry.RegisterType({
        Events::TERRAIN_CAVE,
        ReplicationCategory::Terrain,
        ReplicationMode::ToClients,
        PersistenceMode::Firebase,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        1.0f, 1,
        false, true, false
    });

    // Progression events - reliable, no persistence (fetch from host)
    registry.RegisterType({
        Events::PROGRESSION_XP,
        ReplicationCategory::Progression,
        ReplicationMode::ToOwner,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::Normal,
        0.0f, 0,
        false, true, false
    });

    registry.RegisterType({
        Events::PROGRESSION_LEVEL,
        ReplicationCategory::Progression,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::Reliable,
        EventPriority::High,
        0.0f, 0,
        false, true, false
    });

    // Chat events
    registry.RegisterType({
        Events::CHAT_MESSAGE,
        ReplicationCategory::Chat,
        ReplicationMode::ToAll,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Normal,
        0.1f, 10,
        false, false, true
    });

    // Game state events
    registry.RegisterType({
        Events::GAME_START,
        ReplicationCategory::GameState,
        ReplicationMode::ToClients,
        PersistenceMode::None,
        ReliabilityMode::ReliableOrdered,
        EventPriority::Critical,
        0.0f, 0,
        false, true, false
    });
}

} // namespace Nova
