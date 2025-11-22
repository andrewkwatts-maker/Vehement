#include "LifecycleManager.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>

// We'll use a simple JSON parsing approach
// In production, include nlohmann/json.hpp
namespace nlohmann {
    template<typename Key, typename Value, typename... Args>
    class basic_json {
    public:
        basic_json() = default;
        // Minimal stub for compilation
    };
    using json = basic_json<std::map, std::vector, std::string, bool, std::int64_t, std::uint64_t, double, std::allocator, void>;
}

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// LifecycleManager Implementation
// ============================================================================

LifecycleManager::LifecycleManager() {
    // Pre-allocate object storage
    m_objects.reserve(1024);

    // Default tick config
    m_defaultTickConfig.group = TickGroup::AI;
    m_defaultTickConfig.interval = 0.0f;
    m_defaultTickConfig.priority = 0;
    m_defaultTickConfig.enabled = true;
}

LifecycleManager::~LifecycleManager() {
    DestroyAll();
    ClearPools();
}

bool LifecycleManager::IsTypeRegistered(const std::string& typeName) const {
    return m_typeRegistry.find(typeName) != m_typeRegistry.end();
}

std::vector<std::string> LifecycleManager::GetRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(m_typeRegistry.size());
    for (const auto& pair : m_typeRegistry) {
        types.push_back(pair.first);
    }
    return types;
}

LifecycleHandle LifecycleManager::CreateFromConfig(const std::string& typeName,
                                                   const nlohmann::json& config) {
    auto it = m_typeRegistry.find(typeName);
    if (it == m_typeRegistry.end()) {
        return LifecycleHandle::Invalid;
    }

    // Create object
    std::unique_ptr<ILifecycle> object = it->second();
    if (!object) {
        return LifecycleHandle::Invalid;
    }

    // Allocate handle
    LifecycleHandle handle = AllocateHandle();

    // Setup entry
    ObjectEntry& entry = m_objects[handle.index];
    entry.object = std::move(object);
    entry.typeName = typeName;
    entry.generation = handle.generation;

    // Set handle on object
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetHandle(handle);
        base->SetLifecycleState(LifecycleState::Creating);
    }

    // Register for ticking
    entry.tickHandle = m_tickScheduler.Register(entry.object.get(), m_defaultTickConfig);

    // Call OnCreate
    entry.object->OnCreate(config);

    // Mark as active
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetLifecycleState(LifecycleState::Active);
    }

    // Fire spawned event
    GameEvent spawnEvent(EventType::Spawned, handle);
    m_eventDispatcher.Dispatch(spawnEvent);

    return handle;
}

LifecycleHandle LifecycleManager::CreateFromFile(const std::string& configPath) {
    // Read file
    std::ifstream file(configPath);
    if (!file.is_open()) {
        return LifecycleHandle::Invalid;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Parse JSON and extract type
    // In production, use nlohmann::json::parse()
    // For now, simple string search for "type" field
    size_t typePos = content.find("\"type\"");
    if (typePos == std::string::npos) {
        return LifecycleHandle::Invalid;
    }

    // Extract type value (simplified parsing)
    size_t colonPos = content.find(':', typePos);
    size_t quoteStart = content.find('"', colonPos);
    size_t quoteEnd = content.find('"', quoteStart + 1);

    if (quoteStart == std::string::npos || quoteEnd == std::string::npos) {
        return LifecycleHandle::Invalid;
    }

    std::string typeName = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

    // Create with full config
    nlohmann::json config; // Would parse here in production
    return CreateFromConfig(typeName, config);
}

LifecycleHandle LifecycleManager::Clone(LifecycleHandle source) {
    const ObjectEntry* sourceEntry = GetEntry(source);
    if (!sourceEntry) {
        return LifecycleHandle::Invalid;
    }

    // Create new object of same type
    auto it = m_typeRegistry.find(sourceEntry->typeName);
    if (it == m_typeRegistry.end()) {
        return LifecycleHandle::Invalid;
    }

    std::unique_ptr<ILifecycle> object = it->second();
    if (!object) {
        return LifecycleHandle::Invalid;
    }

    // Allocate handle
    LifecycleHandle handle = AllocateHandle();

    // Setup entry
    ObjectEntry& entry = m_objects[handle.index];
    entry.object = std::move(object);
    entry.typeName = sourceEntry->typeName;
    entry.generation = handle.generation;

    // Set handle on object
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetHandle(handle);
        base->SetLifecycleState(LifecycleState::Creating);
    }

    // Register for ticking
    entry.tickHandle = m_tickScheduler.Register(entry.object.get(), m_defaultTickConfig);

    // Call OnCreate with empty config (would ideally serialize/deserialize source)
    nlohmann::json config;
    entry.object->OnCreate(config);

    // Mark as active
    if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
        base->SetLifecycleState(LifecycleState::Active);
    }

    return handle;
}

void LifecycleManager::Destroy(LifecycleHandle handle, bool immediate) {
    ObjectEntry* entry = GetEntry(handle);
    if (!entry || entry->pendingDestruction) {
        return;
    }

    if (immediate || !m_defaultDeferredDestruction) {
        DestroyImmediate(handle);
    } else {
        entry->pendingDestruction = true;

        DeferredAction action;
        action.type = DeferredAction::Type::Destroy;
        action.handle = handle;
        m_deferredActions.push_back(action);
    }
}

void LifecycleManager::DestroyAllOfType(const std::string& typeName, bool immediate) {
    for (size_t i = 0; i < m_objects.size(); ++i) {
        auto& entry = m_objects[i];
        if (entry.object && entry.typeName == typeName && !entry.pendingDestruction) {
            LifecycleHandle handle;
            handle.index = static_cast<uint32_t>(i);
            handle.generation = entry.generation;
            Destroy(handle, immediate);
        }
    }
}

void LifecycleManager::DestroyAll() {
    for (size_t i = 0; i < m_objects.size(); ++i) {
        auto& entry = m_objects[i];
        if (entry.object && !entry.pendingDestruction) {
            LifecycleHandle handle;
            handle.index = static_cast<uint32_t>(i);
            handle.generation = entry.generation;
            DestroyImmediate(handle);
        }
    }

    m_objects.clear();
    m_freeSlots.clear();
    m_deferredActions.clear();
}

bool LifecycleManager::IsAlive(LifecycleHandle handle) const {
    const ObjectEntry* entry = GetEntry(handle);
    return entry && !entry->pendingDestruction;
}

ILifecycle* LifecycleManager::Get(LifecycleHandle handle) {
    ObjectEntry* entry = GetEntry(handle);
    return entry ? entry->object.get() : nullptr;
}

const ILifecycle* LifecycleManager::Get(LifecycleHandle handle) const {
    const ObjectEntry* entry = GetEntry(handle);
    return entry ? entry->object.get() : nullptr;
}

std::vector<ILifecycle*> LifecycleManager::GetAll(
    std::function<bool(const ILifecycle*)> predicate) {

    std::vector<ILifecycle*> result;
    for (auto& entry : m_objects) {
        if (entry.object && !entry.pendingDestruction) {
            if (!predicate || predicate(entry.object.get())) {
                result.push_back(entry.object.get());
            }
        }
    }
    return result;
}

size_t LifecycleManager::GetObjectCount() const {
    return m_objects.size() - m_freeSlots.size();
}

size_t LifecycleManager::GetActiveObjectCount() const {
    size_t count = 0;
    for (const auto& entry : m_objects) {
        if (entry.object && !entry.pendingDestruction) {
            count++;
        }
    }
    return count;
}

void LifecycleManager::SetParent(LifecycleHandle child, LifecycleHandle parent) {
    ObjectEntry* childEntry = GetEntry(child);
    if (!childEntry) return;

    // Remove from old parent
    if (childEntry->parentHandle.IsValid()) {
        DetachFromParent(child);
    }

    // Add to new parent
    ObjectEntry* parentEntry = GetEntry(parent);
    if (parentEntry) {
        childEntry->parentHandle = parent;
        parentEntry->children.push_back(child);
    }
}

LifecycleHandle LifecycleManager::GetParent(LifecycleHandle handle) const {
    const ObjectEntry* entry = GetEntry(handle);
    return entry ? entry->parentHandle : LifecycleHandle::Invalid;
}

std::vector<LifecycleHandle> LifecycleManager::GetChildren(LifecycleHandle handle) const {
    const ObjectEntry* entry = GetEntry(handle);
    return entry ? entry->children : std::vector<LifecycleHandle>{};
}

void LifecycleManager::DetachFromParent(LifecycleHandle handle) {
    ObjectEntry* entry = GetEntry(handle);
    if (!entry || !entry->parentHandle.IsValid()) return;

    ObjectEntry* parentEntry = GetEntry(entry->parentHandle);
    if (parentEntry) {
        auto& children = parentEntry->children;
        children.erase(std::remove(children.begin(), children.end(), handle), children.end());
    }

    entry->parentHandle = LifecycleHandle::Invalid;
}

void LifecycleManager::Update(float deltaTime) {
    auto startTime = std::chrono::high_resolution_clock::now();

    // Process tick scheduler
    m_tickScheduler.Tick(deltaTime);

    // Process event queue
    m_eventDispatcher.ProcessQueuedEvents(m_tickScheduler.GetTotalTime());

    // Process deferred actions
    ProcessDeferredActions();

    auto endTime = std::chrono::high_resolution_clock::now();
    m_stats.lastUpdateDuration = std::chrono::duration<double>(endTime - startTime).count();
}

void LifecycleManager::ProcessDeferredActions() {
    while (!m_deferredActions.empty()) {
        DeferredAction action = std::move(m_deferredActions.front());
        m_deferredActions.pop_front();

        switch (action.type) {
            case DeferredAction::Type::Destroy:
                DestroyImmediate(action.handle);
                break;
            case DeferredAction::Type::Create:
            case DeferredAction::Type::Event:
            case DeferredAction::Type::Custom:
                if (action.action) {
                    action.action();
                }
                break;
        }
    }
}

bool LifecycleManager::SendEvent(LifecycleHandle target, GameEvent& event) {
    ObjectEntry* entry = GetEntry(target);
    if (!entry || !entry->object) {
        return false;
    }

    event.target = target;

    // Check if object can receive events
    if (auto* base = dynamic_cast<LifecycleBase*>(entry->object.get())) {
        if (!base->CanReceiveEvents()) {
            return false;
        }
    }

    // Dispatch to object
    bool handled = entry->object->OnEvent(event);

    // Propagate to children if capture down
    if (!event.handled && HasPropagation(event.propagation, EventPropagation::CaptureDown)) {
        PropagateEventToChildren(target, event);
    }

    // Propagate to parent if bubble up
    if (!event.handled && HasPropagation(event.propagation, EventPropagation::BubbleUp)) {
        if (entry->parentHandle.IsValid()) {
            SendEvent(entry->parentHandle, event);
        }
    }

    m_stats.eventsProcessed++;
    return handled || event.handled;
}

void LifecycleManager::BroadcastEvent(GameEvent& event) {
    event.propagation = EventPropagation::Broadcast;

    for (auto& entry : m_objects) {
        if (entry.object && !entry.pendingDestruction) {
            if (auto* base = dynamic_cast<LifecycleBase*>(entry.object.get())) {
                if (base->CanReceiveEvents()) {
                    entry.object->OnEvent(event);
                }
            } else {
                entry.object->OnEvent(event);
            }
        }
    }

    m_stats.eventsProcessed++;
}

void LifecycleManager::QueueEvent(GameEvent event) {
    m_eventDispatcher.QueueEvent(std::move(event));
}

void LifecycleManager::PreWarmPools() {
    // Pre-allocate objects in pools
    for (auto& pair : m_pools) {
        // Pools self-manage their capacity
    }
}

void LifecycleManager::ClearPools() {
    for (auto& pair : m_pools) {
        pair.second->Clear();
    }
}

void LifecycleManager::SetDefaultTickConfig(const TickConfig& config) {
    m_defaultTickConfig = config;
}

LifecycleManager::Stats LifecycleManager::GetStats() const {
    m_stats.totalObjects = m_objects.size();
    m_stats.activeObjects = GetActiveObjectCount();

    m_stats.pooledObjects = 0;
    for (const auto& pair : m_pools) {
        m_stats.pooledObjects += pair.second->GetPooledCount();
    }

    m_stats.pendingDestructions = 0;
    for (const auto& entry : m_objects) {
        if (entry.pendingDestruction) {
            m_stats.pendingDestructions++;
        }
    }

    return m_stats;
}

LifecycleManager::ObjectEntry* LifecycleManager::GetEntry(LifecycleHandle handle) {
    if (!handle.IsValid() || handle.index >= m_objects.size()) {
        return nullptr;
    }

    ObjectEntry& entry = m_objects[handle.index];
    if (entry.generation != handle.generation) {
        return nullptr;
    }

    return &entry;
}

const LifecycleManager::ObjectEntry* LifecycleManager::GetEntry(LifecycleHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_objects.size()) {
        return nullptr;
    }

    const ObjectEntry& entry = m_objects[handle.index];
    if (entry.generation != handle.generation) {
        return nullptr;
    }

    return &entry;
}

LifecycleHandle LifecycleManager::AllocateHandle() {
    LifecycleHandle handle;

    if (!m_freeSlots.empty()) {
        handle.index = m_freeSlots.back();
        m_freeSlots.pop_back();
    } else {
        handle.index = static_cast<uint32_t>(m_objects.size());
        m_objects.emplace_back();
    }

    handle.generation = m_nextGeneration++;
    return handle;
}

void LifecycleManager::DestroyImmediate(LifecycleHandle handle) {
    ObjectEntry* entry = GetEntry(handle);
    if (!entry || !entry->object) {
        return;
    }

    // Fire destroyed event
    GameEvent destroyEvent(EventType::Destroyed, handle);
    m_eventDispatcher.Dispatch(destroyEvent);

    // Destroy children first
    for (const auto& childHandle : entry->children) {
        DestroyImmediate(childHandle);
    }
    entry->children.clear();

    // Detach from parent
    DetachFromParent(handle);

    // Mark as destroying
    if (auto* base = dynamic_cast<LifecycleBase*>(entry->object.get())) {
        base->SetLifecycleState(LifecycleState::Destroying);
    }

    // Unregister from tick scheduler
    m_tickScheduler.Unregister(entry->tickHandle);

    // Call OnDestroy
    entry->object->OnDestroy();

    // Check for pool
    auto poolIt = m_pools.find(std::type_index(typeid(*entry->object)));
    if (poolIt != m_pools.end()) {
        // Return to pool
        entry->object->OnDeactivate();
        if (auto* base = dynamic_cast<LifecycleBase*>(entry->object.get())) {
            base->SetLifecycleState(LifecycleState::Pooled);
        }
        poolIt->second->Release(entry->object.release());
    } else {
        // Delete
        entry->object.reset();
    }

    // Mark slot as free
    entry->generation = 0;
    entry->typeName.clear();
    entry->pendingDestruction = false;
    m_freeSlots.push_back(handle.index);
}

void LifecycleManager::PropagateEventToChildren(LifecycleHandle parent, GameEvent& event) {
    const ObjectEntry* entry = GetEntry(parent);
    if (!entry) return;

    for (const auto& childHandle : entry->children) {
        if (event.handled) break;

        ObjectEntry* childEntry = GetEntry(childHandle);
        if (childEntry && childEntry->object) {
            childEntry->object->OnEvent(event);

            // Recursively propagate
            if (!event.handled) {
                PropagateEventToChildren(childHandle, event);
            }
        }
    }
}

// ============================================================================
// Global Manager
// ============================================================================

namespace {
    std::unique_ptr<LifecycleManager> g_globalManager;
    std::once_flag g_managerInitFlag;
}

LifecycleManager& GetGlobalLifecycleManager() {
    std::call_once(g_managerInitFlag, []() {
        g_globalManager = std::make_unique<LifecycleManager>();
    });
    return *g_globalManager;
}

} // namespace Lifecycle
} // namespace Vehement
