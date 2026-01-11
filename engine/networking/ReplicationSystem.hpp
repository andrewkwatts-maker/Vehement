#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <chrono>
#include <variant>
#include <optional>
#include <mutex>

namespace Nova {

// Forward declarations
class NetworkConnection;
class FirebaseClient;

// ============================================================================
// Event Categories for Replication
// ============================================================================

/**
 * @brief Category of replicable events
 */
enum class ReplicationCategory : uint8_t {
    Input,              // User input (movement, clicks, keys)
    EntityState,        // Entity property changes
    EntitySpawn,        // Entity creation/destruction
    EntityMovement,     // Entity position/rotation updates
    Combat,             // Attacks, damage, healing
    Abilities,          // Spell/ability usage
    Building,           // Building placement/construction
    Terrain,            // Landscape modifications
    Progression,        // XP, levels, unlocks
    Inventory,          // Item changes
    UI,                 // UI state changes
    Chat,               // Chat messages
    GameState,          // Match state, objectives
    Custom              // Custom events
};

/**
 * @brief Replication mode for events
 */
enum class ReplicationMode : uint8_t {
    None,               // No replication (local only)
    ToHost,             // Client -> Host only
    ToClients,          // Host -> All Clients
    ToAll,              // Broadcast to everyone
    ToOwner,            // Only to the owning client
    ToServer,           // Clients -> Server (dedicated)
    Multicast           // To specific set of connections
};

/**
 * @brief Persistence mode for events
 */
enum class PersistenceMode : uint8_t {
    None,               // Not persisted (temporary, fetch from host)
    Firebase,           // Persist to Firebase
    LocalFile,          // Persist to local file
    Both                // Firebase + Local backup
};

/**
 * @brief Reliability mode for network events
 */
enum class ReliabilityMode : uint8_t {
    Unreliable,         // UDP-style, may be lost (movement updates)
    Reliable,           // Guaranteed delivery (important state changes)
    ReliableOrdered     // Guaranteed + ordered (commands, chat)
};

/**
 * @brief Priority for event processing
 */
enum class EventPriority : uint8_t {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

// ============================================================================
// Replication Event Data Types
// ============================================================================

using EventValue = std::variant<
    std::monostate,     // Void/None
    bool,
    int32_t,
    int64_t,
    uint32_t,
    uint64_t,
    float,
    double,
    std::string,
    glm::vec2,
    glm::vec3,
    glm::vec4,
    glm::quat,
    std::vector<uint8_t>  // Binary data
>;

/**
 * @brief Single property in an event
 */
struct EventProperty {
    std::string name;
    EventValue value;
    bool dirty = true;      // Changed since last sync
};

/**
 * @brief Network event that can be replicated
 */
struct NetworkEvent {
    // Identification
    uint64_t eventId = 0;           // Unique event ID
    uint64_t sourceEntityId = 0;    // Entity that generated event (0 = system)
    uint64_t targetEntityId = 0;    // Target entity (0 = none/world)
    uint32_t sourceClientId = 0;    // Client that sent event

    // Event type
    std::string eventType;          // Event type identifier
    ReplicationCategory category = ReplicationCategory::Custom;

    // Replication settings
    ReplicationMode replicationMode = ReplicationMode::ToAll;
    PersistenceMode persistenceMode = PersistenceMode::None;
    ReliabilityMode reliabilityMode = ReliabilityMode::Reliable;
    EventPriority priority = EventPriority::Normal;

    // Timing
    uint64_t timestamp = 0;         // When event was created
    uint64_t serverTimestamp = 0;   // Server-assigned timestamp
    float delay = 0.0f;             // Delay before processing

    // Data
    std::vector<EventProperty> properties;

    // Routing
    std::unordered_set<uint32_t> targetClients;  // For Multicast mode
    bool processed = false;
    bool acknowledged = false;

    // Helper methods
    void SetProperty(const std::string& name, const EventValue& value);
    EventValue GetProperty(const std::string& name) const;
    bool HasProperty(const std::string& name) const;

    template<typename T>
    T GetPropertyAs(const std::string& name, const T& defaultValue = T{}) const {
        auto val = GetProperty(name);
        if (auto* ptr = std::get_if<T>(&val)) {
            return *ptr;
        }
        return defaultValue;
    }
};

// ============================================================================
// Event Type Registration
// ============================================================================

/**
 * @brief Configuration for an event type
 */
struct EventTypeConfig {
    std::string typeName;
    ReplicationCategory defaultCategory = ReplicationCategory::Custom;
    ReplicationMode defaultReplicationMode = ReplicationMode::ToAll;
    PersistenceMode defaultPersistenceMode = PersistenceMode::None;
    ReliabilityMode defaultReliabilityMode = ReliabilityMode::Reliable;
    EventPriority defaultPriority = EventPriority::Normal;

    // Rate limiting
    float minInterval = 0.0f;       // Minimum seconds between events of this type
    int maxPerSecond = 0;           // Max events per second (0 = unlimited)

    // Validation
    bool requiresOwnership = false; // Must own source entity
    bool requiresHost = false;      // Must be host to send
    bool allowFromClient = true;    // Clients can send this

    // Callbacks
    std::function<bool(const NetworkEvent&)> validator;
    std::function<void(NetworkEvent&)> preprocessor;
};

/**
 * @brief Registry of event types
 */
class EventTypeRegistry {
public:
    static EventTypeRegistry& Instance();

    void RegisterType(const EventTypeConfig& config);
    void UnregisterType(const std::string& typeName);

    const EventTypeConfig* GetConfig(const std::string& typeName) const;
    std::vector<std::string> GetTypesByCategory(ReplicationCategory category) const;

    // Editor support
    void SetOverride(const std::string& typeName, const std::string& property, const EventValue& value);
    void ClearOverrides(const std::string& typeName);

private:
    EventTypeRegistry() = default;
    std::unordered_map<std::string, EventTypeConfig> m_types;
    std::unordered_map<std::string, std::unordered_map<std::string, EventValue>> m_overrides;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Replication System
// ============================================================================

/**
 * @brief Connection info
 */
struct ConnectionInfo {
    uint32_t clientId = 0;
    std::string address;
    uint16_t port = 0;
    bool isHost = false;
    bool isLocal = false;
    float latency = 0.0f;
    uint64_t lastHeartbeat = 0;

    // Owned entities
    std::unordered_set<uint64_t> ownedEntities;
};

/**
 * @brief Replication statistics
 */
struct ReplicationStats {
    uint64_t eventsSent = 0;
    uint64_t eventsReceived = 0;
    uint64_t eventsDropped = 0;
    uint64_t eventsPersisted = 0;
    uint64_t bytesOut = 0;
    uint64_t bytesIn = 0;
    float avgLatency = 0.0f;

    std::unordered_map<std::string, uint64_t> eventCountByType;
};

/**
 * @brief Main replication system
 *
 * Handles:
 * - Event routing between clients and host
 * - Firebase persistence for permanent changes
 * - Local event processing
 * - Network synchronization
 */
class ReplicationSystem {
public:
    struct Config {
        bool isHost = false;
        bool isDedicatedServer = false;
        uint32_t localClientId = 0;
        std::string firebaseProjectId;
        std::string firebaseApiKey;
        std::string firebaseDatabaseUrl;
        float syncInterval = 0.05f;         // 20 Hz sync rate
        float heartbeatInterval = 1.0f;
        int maxEventsPerFrame = 100;
        bool enablePrediction = true;
        bool enableInterpolation = true;
        float interpolationDelay = 0.1f;    // 100ms interpolation buffer
    };

    static ReplicationSystem& Instance();

    void Initialize(const Config& config);
    void Shutdown();

    /**
     * @brief Update replication (call every frame)
     */
    void Update(float deltaTime);

    // =========================================================================
    // Event Dispatch
    // =========================================================================

    /**
     * @brief Dispatch an event through the replication system
     * @return Event ID
     */
    uint64_t DispatchEvent(NetworkEvent event);

    /**
     * @brief Create and dispatch a typed event
     */
    template<typename... Args>
    uint64_t Dispatch(const std::string& eventType, Args&&... args) {
        NetworkEvent event;
        event.eventType = eventType;
        SetEventProperties(event, std::forward<Args>(args)...);
        return DispatchEvent(std::move(event));
    }

    /**
     * @brief Dispatch event with explicit settings
     */
    uint64_t DispatchWithSettings(
        const std::string& eventType,
        ReplicationMode replication,
        PersistenceMode persistence,
        const std::vector<EventProperty>& properties
    );

    // =========================================================================
    // Event Subscription
    // =========================================================================

    using EventHandler = std::function<void(const NetworkEvent&)>;
    using EventFilter = std::function<bool(const NetworkEvent&)>;

    /**
     * @brief Subscribe to events
     * @return Subscription ID
     */
    uint64_t Subscribe(const std::string& eventType, EventHandler handler);
    uint64_t Subscribe(ReplicationCategory category, EventHandler handler);
    uint64_t SubscribeAll(EventHandler handler);
    uint64_t SubscribeFiltered(EventFilter filter, EventHandler handler);

    /**
     * @brief Unsubscribe from events
     */
    void Unsubscribe(uint64_t subscriptionId);

    // =========================================================================
    // Connection Management
    // =========================================================================

    /**
     * @brief Start hosting
     */
    bool StartHost(uint16_t port);

    /**
     * @brief Connect to host
     */
    bool Connect(const std::string& address, uint16_t port);

    /**
     * @brief Disconnect
     */
    void Disconnect();

    /**
     * @brief Get connection info
     */
    const ConnectionInfo* GetConnection(uint32_t clientId) const;
    std::vector<ConnectionInfo> GetAllConnections() const;

    /**
     * @brief Check if connected
     */
    bool IsConnected() const { return m_connected; }
    bool IsHost() const { return m_config.isHost; }
    uint32_t GetLocalClientId() const { return m_config.localClientId; }

    // =========================================================================
    // Entity Ownership
    // =========================================================================

    /**
     * @brief Set entity ownership
     */
    void SetEntityOwner(uint64_t entityId, uint32_t clientId);

    /**
     * @brief Get entity owner
     */
    uint32_t GetEntityOwner(uint64_t entityId) const;

    /**
     * @brief Check if local client owns entity
     */
    bool IsLocallyOwned(uint64_t entityId) const;

    /**
     * @brief Request ownership of entity
     */
    void RequestOwnership(uint64_t entityId);

    // =========================================================================
    // Firebase Persistence
    // =========================================================================

    /**
     * @brief Set Firebase client
     */
    void SetFirebaseClient(std::shared_ptr<FirebaseClient> client);

    /**
     * @brief Persist event to Firebase
     */
    void PersistToFirebase(const NetworkEvent& event);

    /**
     * @brief Load persisted events from Firebase
     */
    void LoadFromFirebase(const std::string& path, std::function<void(std::vector<NetworkEvent>)> callback);

    /**
     * @brief Sync terrain changes to Firebase
     */
    void SyncTerrainToFirebase();

    // =========================================================================
    // State Synchronization
    // =========================================================================

    /**
     * @brief Request full state sync from host
     */
    void RequestFullSync();

    /**
     * @brief Send full state to client
     */
    void SendFullStateTo(uint32_t clientId);

    /**
     * @brief Get server time (synchronized)
     */
    uint64_t GetServerTime() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    const ReplicationStats& GetStats() const { return m_stats; }
    void ResetStats();

    // =========================================================================
    // Editor Support
    // =========================================================================

    /**
     * @brief Get all registered event types
     */
    std::vector<std::string> GetRegisteredEventTypes() const;

    /**
     * @brief Set event type override (editor)
     */
    void SetEventTypeOverride(const std::string& eventType, ReplicationMode mode, PersistenceMode persistence);

    /**
     * @brief Clear event type override
     */
    void ClearEventTypeOverride(const std::string& eventType);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(uint32_t)> OnClientConnected;
    std::function<void(uint32_t)> OnClientDisconnected;
    std::function<void()> OnConnectedToHost;
    std::function<void()> OnDisconnectedFromHost;
    std::function<void(const NetworkEvent&)> OnEventReceived;
    std::function<void(const NetworkEvent&)> OnEventSent;
    std::function<void(const NetworkEvent&)> OnEventPersisted;

private:
    ReplicationSystem() = default;

    // Event processing
    void ProcessLocalEvent(const NetworkEvent& event);
    void ProcessRemoteEvent(const NetworkEvent& event);
    void SendEventToNetwork(const NetworkEvent& event);
    void BroadcastEvent(const NetworkEvent& event);
    void SendEventTo(const NetworkEvent& event, uint32_t clientId);

    // Validation
    bool ValidateEvent(const NetworkEvent& event) const;
    bool CheckRateLimit(const std::string& eventType, uint32_t clientId);

    // Serialization
    std::vector<uint8_t> SerializeEvent(const NetworkEvent& event) const;
    NetworkEvent DeserializeEvent(const std::vector<uint8_t>& data) const;

    // Network I/O
    void ProcessIncomingPackets();
    void SendOutgoingPackets();
    void SendHeartbeats();
    void CheckTimeouts();

    // Property helpers
    template<typename T, typename... Rest>
    void SetEventProperties(NetworkEvent& event, const std::string& name, const T& value, Rest&&... rest) {
        event.SetProperty(name, EventValue(value));
        if constexpr (sizeof...(rest) > 0) {
            SetEventProperties(event, std::forward<Rest>(rest)...);
        }
    }

    void SetEventProperties(NetworkEvent& /*event*/) {} // Base case

    Config m_config;
    bool m_initialized = false;
    bool m_connected = false;

    // Connections
    std::unordered_map<uint32_t, ConnectionInfo> m_connections;
    mutable std::mutex m_connectionMutex;

    // Event queues
    std::queue<NetworkEvent> m_outgoingEvents;
    std::queue<NetworkEvent> m_incomingEvents;
    std::queue<NetworkEvent> m_pendingEvents;  // Events waiting for ack
    mutable std::mutex m_eventMutex;

    // Subscriptions
    struct Subscription {
        uint64_t id;
        std::string eventType;      // Empty = all types
        ReplicationCategory category = ReplicationCategory::Custom;
        bool categoryFilter = false;
        EventFilter filter;
        EventHandler handler;
    };
    std::vector<Subscription> m_subscriptions;
    uint64_t m_nextSubscriptionId = 1;
    mutable std::mutex m_subscriptionMutex;

    // Entity ownership
    std::unordered_map<uint64_t, uint32_t> m_entityOwnership;
    mutable std::mutex m_ownershipMutex;

    // Rate limiting
    std::unordered_map<std::string, std::unordered_map<uint32_t, std::chrono::steady_clock::time_point>> m_lastEventTime;
    std::unordered_map<std::string, std::unordered_map<uint32_t, int>> m_eventCountPerSecond;

    // Firebase
    std::shared_ptr<FirebaseClient> m_firebaseClient;
    std::queue<NetworkEvent> m_firebaseQueue;

    // Time sync
    uint64_t m_serverTimeOffset = 0;

    // Statistics
    ReplicationStats m_stats;

    // Event ID generation
    uint64_t m_nextEventId = 1;

    // Timing
    float m_syncTimer = 0.0f;
    float m_heartbeatTimer = 0.0f;

    // Private helper methods
    void RegisterDefaultEventTypes();
    void ProcessFirebaseQueue();
    void PersistEvent(const NetworkEvent& event);
};

// ============================================================================
// Pre-defined Event Types
// ============================================================================

namespace Events {

// Input events
constexpr const char* INPUT_MOVE = "input.move";
constexpr const char* INPUT_LOOK = "input.look";
constexpr const char* INPUT_ACTION = "input.action";
constexpr const char* INPUT_KEY = "input.key";
constexpr const char* INPUT_MOUSE = "input.mouse";

// Entity events
constexpr const char* ENTITY_SPAWN = "entity.spawn";
constexpr const char* ENTITY_DESTROY = "entity.destroy";
constexpr const char* ENTITY_MOVE = "entity.move";
constexpr const char* ENTITY_ROTATE = "entity.rotate";
constexpr const char* ENTITY_SCALE = "entity.scale";
constexpr const char* ENTITY_STATE = "entity.state";
constexpr const char* ENTITY_PROPERTY = "entity.property";
constexpr const char* ENTITY_COMPONENT_ADD = "entity.component.add";
constexpr const char* ENTITY_COMPONENT_REMOVE = "entity.component.remove";

// Combat events
constexpr const char* COMBAT_ATTACK = "combat.attack";
constexpr const char* COMBAT_DAMAGE = "combat.damage";
constexpr const char* COMBAT_HEAL = "combat.heal";
constexpr const char* COMBAT_DEATH = "combat.death";
constexpr const char* COMBAT_RESPAWN = "combat.respawn";

// Ability events
constexpr const char* ABILITY_USE = "ability.use";
constexpr const char* ABILITY_CANCEL = "ability.cancel";
constexpr const char* ABILITY_EFFECT = "ability.effect";
constexpr const char* ABILITY_COOLDOWN = "ability.cooldown";

// Building events
constexpr const char* BUILDING_PLACE = "building.place";
constexpr const char* BUILDING_START = "building.start";
constexpr const char* BUILDING_PROGRESS = "building.progress";
constexpr const char* BUILDING_COMPLETE = "building.complete";
constexpr const char* BUILDING_DESTROY = "building.destroy";
constexpr const char* BUILDING_UPGRADE = "building.upgrade";

// Terrain events
constexpr const char* TERRAIN_MODIFY = "terrain.modify";
constexpr const char* TERRAIN_PAINT = "terrain.paint";
constexpr const char* TERRAIN_SCULPT = "terrain.sculpt";
constexpr const char* TERRAIN_TUNNEL = "terrain.tunnel";
constexpr const char* TERRAIN_CAVE = "terrain.cave";

// Progression events
constexpr const char* PROGRESSION_XP = "progression.xp";
constexpr const char* PROGRESSION_LEVEL = "progression.level";
constexpr const char* PROGRESSION_UNLOCK = "progression.unlock";
constexpr const char* PROGRESSION_ACHIEVEMENT = "progression.achievement";

// Inventory events
constexpr const char* INVENTORY_ADD = "inventory.add";
constexpr const char* INVENTORY_REMOVE = "inventory.remove";
constexpr const char* INVENTORY_MOVE = "inventory.move";
constexpr const char* INVENTORY_USE = "inventory.use";
constexpr const char* INVENTORY_DROP = "inventory.drop";

// Game state events
constexpr const char* GAME_START = "game.start";
constexpr const char* GAME_END = "game.end";
constexpr const char* GAME_PAUSE = "game.pause";
constexpr const char* GAME_RESUME = "game.resume";
constexpr const char* GAME_OBJECTIVE = "game.objective";

// Chat events
constexpr const char* CHAT_MESSAGE = "chat.message";
constexpr const char* CHAT_SYSTEM = "chat.system";

} // namespace Events

// ============================================================================
// Helper Macros
// ============================================================================

#define DISPATCH_EVENT(type, ...) \
    Nova::ReplicationSystem::Instance().Dispatch(type, __VA_ARGS__)

#define DISPATCH_REPLICATED(type, mode, ...) \
    Nova::ReplicationSystem::Instance().DispatchWithSettings( \
        type, mode, Nova::PersistenceMode::None, {__VA_ARGS__})

#define DISPATCH_PERSISTED(type, persistence, ...) \
    Nova::ReplicationSystem::Instance().DispatchWithSettings( \
        type, Nova::ReplicationMode::ToAll, persistence, {__VA_ARGS__})

} // namespace Nova
