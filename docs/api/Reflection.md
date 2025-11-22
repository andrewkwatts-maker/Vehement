# Reflection API

The reflection system provides runtime type information, property access, and a publish/subscribe event bus for decoupled communication.

## TypeRegistry Class

**Header**: `engine/reflection/TypeRegistry.hpp`

Global registry for runtime type information.

### Instance

```cpp
static TypeRegistry& Instance();
```

Get the singleton instance.

### Type Registration

#### RegisterType

```cpp
template<typename T>
TypeInfo& RegisterType(const std::string& name);
```

Register a type with the registry.

#### RegisterDerivedType

```cpp
template<typename T, typename Base>
TypeInfo& RegisterDerivedType(const std::string& name);
```

Register a type derived from a base type.

#### IsRegistered

```cpp
[[nodiscard]] bool IsRegistered(const std::string& name) const;

template<typename T>
[[nodiscard]] bool IsRegistered() const;
```

Check if a type is registered.

### Type Queries

#### GetType

```cpp
[[nodiscard]] const TypeInfo* GetType(const std::string& name) const;

template<typename T>
[[nodiscard]] const TypeInfo* GetType() const;

[[nodiscard]] const TypeInfo* GetTypeByHash(size_t hash) const;
```

Get type info by name, C++ type, or hash.

#### GetAllTypes

```cpp
[[nodiscard]] std::vector<const TypeInfo*> GetAllTypes() const;
[[nodiscard]] std::vector<const TypeInfo*> GetTypesByCategory(const std::string& category) const;
[[nodiscard]] std::vector<const TypeInfo*> GetDerivedTypes(const std::string& baseTypeName) const;
[[nodiscard]] std::vector<const TypeInfo*> GetComponentTypes() const;
[[nodiscard]] std::vector<const TypeInfo*> GetEntityTypes() const;
[[nodiscard]] size_t GetTypeCount() const;
```

Query registered types.

### Type Iteration

```cpp
void ForEachType(const std::function<void(const TypeInfo&)>& callback) const;

void ForEachTypeWhere(const std::function<bool(const TypeInfo&)>& predicate,
                      const std::function<void(const TypeInfo&)>& callback) const;
```

Iterate over types with optional filtering.

### Instance Creation

```cpp
[[nodiscard]] void* CreateInstance(const std::string& name) const;

template<typename T>
[[nodiscard]] std::unique_ptr<T> Create(const std::string& name) const;
```

Create instances by type name.

### Property Change Notifications

```cpp
size_t RegisterPropertyChangeListener(PropertyChangeCallback callback);
void UnregisterPropertyChangeListener(size_t listenerId);
void NotifyPropertyChange(const PropertyChangeEvent& event);
```

---

## TypeInfo Class

**Header**: `engine/reflection/TypeInfo.hpp`

Metadata for a registered type.

### Members

```cpp
struct TypeInfo {
    std::string name;
    std::type_index typeIndex;
    size_t size;
    size_t alignment;
    size_t typeHash;
    std::string baseTypeName;
    TypeInfo* baseType = nullptr;
    std::string category;
    std::string description;

    // Factory functions
    std::function<void*()> factory;
    std::function<void(void*)> destructor;
    std::function<void*(const void*)> copyConstructor;

    // Properties and events
    std::vector<PropertyInfo> properties;
    std::vector<EventInfo> events;
};
```

### Methods

```cpp
void AddProperty(PropertyInfo property);
void AddEvent(EventInfo event);
[[nodiscard]] const PropertyInfo* GetProperty(const std::string& name) const;
[[nodiscard]] bool HasProperty(const std::string& name) const;
[[nodiscard]] bool IsDerivedFrom(const std::string& baseTypeName) const;
```

---

## PropertyInfo Class

Property metadata.

```cpp
struct PropertyInfo {
    std::string name;
    std::string typeName;
    std::type_index typeIndex;
    size_t offset;
    size_t size;
    PropertyAttribute attributes;
    std::string category;
    std::string displayName;
    std::string description;
    float minValue, maxValue;

    // Accessors
    std::function<void(void*, const std::any&)> setterAny;
    std::function<std::any(const void*)> getterAny;

    // Builder methods
    PropertyInfo& WithCategory(const std::string& cat);
    PropertyInfo& WithDisplayName(const std::string& name);
    PropertyInfo& WithDescription(const std::string& desc);
    PropertyInfo& WithRange(float min, float max);
    PropertyInfo& WithAttributeString(const std::string& attr);
};
```

---

## PropertyAttribute Enum

```cpp
enum class PropertyAttribute : uint32_t {
    None = 0,
    Editable = 1 << 0,      // Can be edited in inspector
    Observable = 1 << 1,    // Fires change events
    Replicated = 1 << 2,    // Synced over network
    Serialized = 1 << 3,    // Saved to file
    Hidden = 1 << 4,        // Hidden from inspector
    ReadOnly = 1 << 5       // Cannot be modified
};
```

---

## Reflection Macros

### REFLECT_TYPE

```cpp
REFLECT_TYPE(MyClass)
```

Simple type registration.

### REFLECT_DERIVED_TYPE

```cpp
REFLECT_DERIVED_TYPE(DerivedClass, BaseClass)
```

Register derived type with base.

### Property Registration

```cpp
REFLECT_TYPE_BEGIN(Enemy)
    REFLECT_PROPERTY(health, REFLECT_ATTR_EDITABLE REFLECT_ATTR_RANGE(0, 100))
    REFLECT_PROPERTY(damage, REFLECT_ATTR_EDITABLE REFLECT_ATTR_SERIALIZED)
    REFLECT_PROPERTY(name, REFLECT_ATTR_EDITABLE REFLECT_ATTR_DISPLAY("Enemy Name"))
    REFLECT_PROPERTY_ACCESSORS(speed, GetSpeed, SetSpeed, REFLECT_ATTR_EDITABLE)
    REFLECT_EVENT(OnDamaged)
REFLECT_TYPE_END()
```

### Attribute Macros

```cpp
REFLECT_ATTR_EDITABLE     // PropertyAttribute::Editable
REFLECT_ATTR_OBSERVABLE   // PropertyAttribute::Observable
REFLECT_ATTR_REPLICATED   // PropertyAttribute::Replicated
REFLECT_ATTR_SERIALIZED   // PropertyAttribute::Serialized
REFLECT_ATTR_HIDDEN       // PropertyAttribute::Hidden
REFLECT_ATTR_READONLY     // PropertyAttribute::ReadOnly
REFLECT_ATTR_RANGE(Min, Max)
REFLECT_ATTR_CATEGORY(Cat)
REFLECT_ATTR_DISPLAY(Name)
REFLECT_ATTR_DESC(Desc)
```

---

## EventBus Class

**Header**: `engine/reflection/EventBus.hpp`

Publish/subscribe event system.

### Instance

```cpp
static EventBus& Instance();
```

### Subscription

#### Subscribe

```cpp
size_t Subscribe(const std::string& eventType,
                 std::function<void(BusEvent&)> handler,
                 EventPriority priority = EventPriority::Normal);

size_t Subscribe(const std::string& name,
                 const std::string& eventType,
                 std::function<void(BusEvent&)> handler,
                 EventPriority priority = EventPriority::Normal);
```

Subscribe to events. Returns subscription ID.

#### SubscribeFiltered

```cpp
size_t SubscribeFiltered(const std::string& eventType,
                         const std::string& sourceTypeFilter,
                         std::function<void(BusEvent&)> handler,
                         EventPriority priority = EventPriority::Normal);
```

Subscribe with source type filter.

#### Unsubscribe

```cpp
bool Unsubscribe(size_t subscriptionId);
bool Unsubscribe(const std::string& name);
void UnsubscribeAll(const std::string& eventType);
```

#### SetEnabled

```cpp
void SetEnabled(size_t subscriptionId, bool enabled);
```

### Publishing

#### Publish

```cpp
bool Publish(BusEvent& event);
bool Publish(const std::string& eventType,
             const std::unordered_map<std::string, std::any>& data = {});
```

Publish event immediately. Returns true if not cancelled.

#### QueueEvent

```cpp
void QueueEvent(const BusEvent& event);
void QueueDelayed(BusEvent event, float delaySeconds);
```

Queue event for deferred processing.

#### ProcessQueue

```cpp
void ProcessQueue(float deltaTime);
void ClearQueue();
[[nodiscard]] size_t GetQueueSize() const;
```

### History

```cpp
void SetHistoryEnabled(bool enabled, size_t maxEntries = 1000);
[[nodiscard]] std::vector<EventHistoryEntry> GetHistory() const;
[[nodiscard]] std::vector<EventHistoryEntry> GetHistory(const std::string& eventType) const;
void ClearHistory();
[[nodiscard]] bool IsHistoryEnabled() const;
```

### Metrics

```cpp
struct Metrics {
    std::atomic<uint64_t> totalEventsPublished{0};
    std::atomic<uint64_t> totalEventsCancelled{0};
    std::atomic<uint64_t> totalHandlersCalled{0};
    std::atomic<uint64_t> totalQueuedEvents{0};
    double totalProcessingTimeMs = 0.0;
    std::unordered_map<std::string, uint64_t> eventsPerType;
};

[[nodiscard]] const Metrics& GetMetrics() const;
void ResetMetrics();
```

---

## BusEvent Class

Event data container.

### Constructor

```cpp
BusEvent();
explicit BusEvent(const std::string& type);
BusEvent(const std::string& type, const std::string& srcType, uint64_t srcId = 0);
```

### Members

```cpp
std::string eventType;
std::string sourceType;
void* source = nullptr;
uint64_t sourceId = 0;
std::chrono::system_clock::time_point timestamp;
bool cancelled = false;
bool propagate = true;
std::unordered_map<std::string, std::any> data;
```

### Data Access

```cpp
template<typename T>
void SetData(const std::string& key, const T& value);

template<typename T>
[[nodiscard]] std::optional<T> GetData(const std::string& key) const;

template<typename T>
[[nodiscard]] T GetDataOr(const std::string& key, const T& defaultValue) const;

[[nodiscard]] bool HasData(const std::string& key) const;
```

### Control

```cpp
void Cancel();
void StopPropagation();
[[nodiscard]] bool IsCancelled() const;
[[nodiscard]] bool ShouldPropagate() const;
```

---

## EventPriority Enum

```cpp
enum class EventPriority : int {
    Lowest = 0,
    Low = 25,
    Normal = 50,
    High = 75,
    Highest = 100,
    Monitor = 200  // Cannot cancel, just observes
};
```

---

## EventSubscriptionGuard Class

RAII wrapper for automatic unsubscription.

```cpp
class EventSubscriptionGuard {
public:
    EventSubscriptionGuard() = default;
    explicit EventSubscriptionGuard(size_t id);
    ~EventSubscriptionGuard();

    void Release();
    [[nodiscard]] size_t GetId() const;
};
```

---

## Usage Examples

### Type Registration

```cpp
class Enemy {
public:
    float health = 100.0f;
    float damage = 10.0f;
    std::string name = "Enemy";

    float GetSpeed() const { return speed; }
    void SetSpeed(float s) { speed = s; }

private:
    float speed = 5.0f;
};

REFLECT_TYPE(Enemy)
REFLECT_TYPE_BEGIN(Enemy)
    REFLECT_PROPERTY(health, REFLECT_ATTR_EDITABLE REFLECT_ATTR_RANGE(0, 100))
    REFLECT_PROPERTY(damage, REFLECT_ATTR_EDITABLE)
    REFLECT_PROPERTY(name, REFLECT_ATTR_EDITABLE REFLECT_ATTR_CATEGORY("Info"))
    REFLECT_PROPERTY_ACCESSORS(speed, GetSpeed, SetSpeed, REFLECT_ATTR_EDITABLE)
REFLECT_TYPE_END()
```

### Using TypeRegistry

```cpp
auto& registry = TypeRegistry::Instance();

// Query type
const auto* info = registry.GetType("Enemy");
if (info) {
    // Create instance
    auto enemy = registry.Create<Enemy>("Enemy");

    // Access properties
    for (const auto& prop : info->properties) {
        std::cout << prop.name << ": " << prop.typeName << "\n";
    }
}

// List all entity types
auto entityTypes = registry.GetEntityTypes();
for (const auto* type : entityTypes) {
    std::cout << type->name << "\n";
}
```

### Event System

```cpp
auto& bus = EventBus::Instance();

// Subscribe to damage events
auto subId = bus.Subscribe("OnDamage", [](BusEvent& evt) {
    float damage = evt.GetDataOr<float>("damage", 0.0f);
    uint64_t target = evt.GetDataOr<uint64_t>("targetId", 0);

    // Handle damage
    ApplyDamage(target, damage);
});

// RAII subscription
{
    EventSubscriptionGuard guard(bus.Subscribe("OnSpawn", handleSpawn));
    // ... guard unsubscribes when out of scope
}

// Publish event
BusEvent damageEvent("OnDamage", "Weapon", weaponId);
damageEvent.SetData("damage", 50.0f);
damageEvent.SetData("targetId", enemyId);
damageEvent.SetData("damageType", std::string("fire"));

bus.Publish(damageEvent);

// Queue delayed event
BusEvent explosionEvent("OnExplosion");
explosionEvent.SetData("position", position);
bus.QueueDelayed(explosionEvent, 2.0f);

// Process queue each frame
bus.ProcessQueue(deltaTime);
```

### Priority Handling

```cpp
// High priority handler runs first
bus.Subscribe("OnDamage", [](BusEvent& evt) {
    // Shield check - can cancel event
    if (HasShield(evt.GetData<uint64_t>("targetId"))) {
        evt.Cancel();
    }
}, EventPriority::High);

// Normal priority handler
bus.Subscribe("OnDamage", [](BusEvent& evt) {
    // Apply damage (only runs if not cancelled)
    ApplyDamage(evt);
}, EventPriority::Normal);

// Monitor handler - always runs, cannot cancel
bus.Subscribe("OnDamage", [](BusEvent& evt) {
    // Log for analytics
    LogDamageEvent(evt);
}, EventPriority::Monitor);
```
