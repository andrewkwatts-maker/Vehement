#include "TypeRegistry.hpp"
#include <algorithm>

namespace Nova {
namespace Reflect {

// ============================================================================
// TypeRegistry Singleton
// ============================================================================

TypeRegistry& TypeRegistry::Instance() {
    static TypeRegistry instance;
    return instance;
}

// ============================================================================
// Type Registration
// ============================================================================

bool TypeRegistry::IsRegistered(const std::string& name) const {
    std::shared_lock lock(m_mutex);
    return m_types.contains(name);
}

// ============================================================================
// Type Queries
// ============================================================================

const TypeInfo* TypeRegistry::GetType(const std::string& name) const {
    std::shared_lock lock(m_mutex);
    auto it = m_types.find(name);
    return it != m_types.end() ? it->second.get() : nullptr;
}

const TypeInfo* TypeRegistry::GetTypeByHash(size_t hash) const {
    std::shared_lock lock(m_mutex);
    auto it = m_typesByHash.find(hash);
    return it != m_typesByHash.end() ? it->second : nullptr;
}

TypeInfo* TypeRegistry::GetMutableType(const std::string& name) {
    std::shared_lock lock(m_mutex);
    auto it = m_types.find(name);
    return it != m_types.end() ? it->second.get() : nullptr;
}

std::vector<const TypeInfo*> TypeRegistry::GetAllTypes() const {
    std::shared_lock lock(m_mutex);
    std::vector<const TypeInfo*> result;
    result.reserve(m_types.size());
    for (const auto& [name, info] : m_types) {
        result.push_back(info.get());
    }
    return result;
}

std::vector<const TypeInfo*> TypeRegistry::GetTypesByCategory(const std::string& category) const {
    std::shared_lock lock(m_mutex);
    std::vector<const TypeInfo*> result;
    for (const auto& [name, info] : m_types) {
        if (info->category == category) {
            result.push_back(info.get());
        }
    }
    return result;
}

std::vector<const TypeInfo*> TypeRegistry::GetDerivedTypes(const std::string& baseTypeName) const {
    std::shared_lock lock(m_mutex);
    std::vector<const TypeInfo*> result;

    const TypeInfo* baseType = GetType(baseTypeName);
    if (!baseType) return result;

    for (const auto& [name, info] : m_types) {
        if (info->IsA(baseType) && info.get() != baseType) {
            result.push_back(info.get());
        }
    }
    return result;
}

std::vector<const TypeInfo*> TypeRegistry::GetComponentTypes() const {
    std::shared_lock lock(m_mutex);
    std::vector<const TypeInfo*> result;
    for (const auto& [name, info] : m_types) {
        if (info->isComponent) {
            result.push_back(info.get());
        }
    }
    return result;
}

std::vector<const TypeInfo*> TypeRegistry::GetEntityTypes() const {
    std::shared_lock lock(m_mutex);
    std::vector<const TypeInfo*> result;
    for (const auto& [name, info] : m_types) {
        if (info->isEntity) {
            result.push_back(info.get());
        }
    }
    return result;
}

size_t TypeRegistry::GetTypeCount() const {
    std::shared_lock lock(m_mutex);
    return m_types.size();
}

// ============================================================================
// Type Iteration
// ============================================================================

void TypeRegistry::ForEachType(const std::function<void(const TypeInfo&)>& callback) const {
    std::shared_lock lock(m_mutex);
    for (const auto& [name, info] : m_types) {
        callback(*info);
    }
}

void TypeRegistry::ForEachTypeWhere(const std::function<bool(const TypeInfo&)>& predicate,
                                    const std::function<void(const TypeInfo&)>& callback) const {
    std::shared_lock lock(m_mutex);
    for (const auto& [name, info] : m_types) {
        if (predicate(*info)) {
            callback(*info);
        }
    }
}

// ============================================================================
// Instance Creation
// ============================================================================

void* TypeRegistry::CreateInstance(const std::string& name) const {
    const TypeInfo* info = GetType(name);
    return info ? info->CreateInstance() : nullptr;
}

// ============================================================================
// Property Change Notifications
// ============================================================================

size_t TypeRegistry::RegisterPropertyChangeListener(PropertyChangeCallback callback) {
    std::lock_guard lock(m_listenerMutex);
    size_t id = m_nextListenerId++;
    m_propertyChangeListeners[id] = std::move(callback);
    return id;
}

void TypeRegistry::UnregisterPropertyChangeListener(size_t listenerId) {
    std::lock_guard lock(m_listenerMutex);
    m_propertyChangeListeners.erase(listenerId);
}

void TypeRegistry::NotifyPropertyChange(const PropertyChangeEvent& event) {
    std::lock_guard lock(m_listenerMutex);
    for (const auto& [id, callback] : m_propertyChangeListeners) {
        callback(event);
    }
}

} // namespace Reflect
} // namespace Nova
