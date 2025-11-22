#include "PropertyWatcher.hpp"
#include <sstream>

namespace Nova {
namespace Events {

// ============================================================================
// Watch Management
// ============================================================================

std::string PropertyWatcher::Watch(void* object,
                                   const Reflect::TypeInfo* typeInfo,
                                   const std::string& propertyPath,
                                   const PropertyWatchConfig& config,
                                   PropertyChangeCallback callback) {
    std::lock_guard lock(m_mutex);

    PropertyWatch watch;
    watch.config = config;
    watch.config.propertyPath = propertyPath;
    watch.callback = std::move(callback);
    watch.watchedObject = object;
    watch.typeInfo = typeInfo;
    watch.enabled = true;
    watch.lastChangeTime = std::chrono::system_clock::now();
    watch.lastNotificationTime = std::chrono::system_clock::now();

    // Generate ID if not provided
    std::string watchId = config.id.empty() ? GenerateWatchId() : config.id;
    watch.config.id = watchId;

    // Get initial value
    watch.lastValue = GetPropertyValue(object, typeInfo, propertyPath);

    // Initialize threshold tracking
    if (config.mode == NotificationMode::Threshold) {
        CheckThreshold(watch.lastValue, config.thresholdHigh, watch.wasAboveThreshold);
    }

    // Store watch
    m_watches[watchId] = std::move(watch);
    m_watchesByObject[object].push_back(watchId);

    return watchId;
}

std::string PropertyWatcher::Watch(void* object,
                                   const Reflect::TypeInfo* typeInfo,
                                   const std::string& propertyPath,
                                   PropertyChangeCallback callback) {
    return Watch(object, typeInfo, propertyPath, PropertyWatchConfig().WithPath(propertyPath), std::move(callback));
}

bool PropertyWatcher::Unwatch(const std::string& watchId) {
    std::lock_guard lock(m_mutex);

    auto it = m_watches.find(watchId);
    if (it == m_watches.end()) {
        return false;
    }

    // Remove from object index
    void* obj = it->second.watchedObject;
    auto objIt = m_watchesByObject.find(obj);
    if (objIt != m_watchesByObject.end()) {
        auto& vec = objIt->second;
        vec.erase(std::remove(vec.begin(), vec.end(), watchId), vec.end());
        if (vec.empty()) {
            m_watchesByObject.erase(objIt);
        }
    }

    m_watches.erase(it);
    return true;
}

void PropertyWatcher::UnwatchObject(void* object) {
    std::lock_guard lock(m_mutex);

    auto it = m_watchesByObject.find(object);
    if (it == m_watchesByObject.end()) {
        return;
    }

    for (const auto& watchId : it->second) {
        m_watches.erase(watchId);
    }

    m_watchesByObject.erase(it);
}

void PropertyWatcher::UnwatchAll() {
    std::lock_guard lock(m_mutex);
    m_watches.clear();
    m_watchesByObject.clear();
}

void PropertyWatcher::SetWatchEnabled(const std::string& watchId, bool enabled) {
    std::lock_guard lock(m_mutex);
    auto it = m_watches.find(watchId);
    if (it != m_watches.end()) {
        it->second.enabled = enabled;
    }
}

bool PropertyWatcher::HasWatch(const std::string& watchId) const {
    std::lock_guard lock(m_mutex);
    return m_watches.contains(watchId);
}

size_t PropertyWatcher::GetWatchCount() const {
    std::lock_guard lock(m_mutex);
    return m_watches.size();
}

std::vector<std::string> PropertyWatcher::GetWatchIds() const {
    std::lock_guard lock(m_mutex);
    std::vector<std::string> ids;
    ids.reserve(m_watches.size());
    for (const auto& [id, _] : m_watches) {
        ids.push_back(id);
    }
    return ids;
}

// ============================================================================
// Update and Polling
// ============================================================================

void PropertyWatcher::Update(float deltaTime) {
    std::lock_guard lock(m_mutex);

    float deltaMs = deltaTime * 1000.0f;

    for (auto& [watchId, watch] : m_watches) {
        if (!watch.enabled) continue;

        // Update debounce timers
        if (watch.hasPendingChange) {
            watch.debounceTimer -= deltaMs;
            if (watch.debounceTimer <= 0.0f) {
                // Debounce period elapsed, notify
                std::any currentValue = GetPropertyValue(watch.watchedObject, watch.typeInfo, watch.config.propertyPath);
                ProcessChange(watch, currentValue);
                watch.hasPendingChange = false;
            }
        }

        // Poll for changes based on mode
        if (watch.config.mode != NotificationMode::Debounced || !watch.hasPendingChange) {
            std::any currentValue = GetPropertyValue(watch.watchedObject, watch.typeInfo, watch.config.propertyPath);
            if (!CompareValues(watch.lastValue, currentValue)) {
                ProcessChange(watch, currentValue);
            }
        }
    }

    // Process global batch timer
    m_globalBatchTimer -= deltaMs;
    if (m_globalBatchTimer <= 0.0f && !m_globalBatch.empty()) {
        FlushBatched();
    }
}

void PropertyWatcher::PollAll() {
    std::lock_guard lock(m_mutex);

    for (auto& [watchId, watch] : m_watches) {
        if (!watch.enabled) continue;

        std::any currentValue = GetPropertyValue(watch.watchedObject, watch.typeInfo, watch.config.propertyPath);
        if (!CompareValues(watch.lastValue, currentValue)) {
            ProcessChange(watch, currentValue);
        }
    }
}

void PropertyWatcher::Poll(const std::string& watchId) {
    std::lock_guard lock(m_mutex);

    auto it = m_watches.find(watchId);
    if (it == m_watches.end() || !it->second.enabled) return;

    auto& watch = it->second;
    std::any currentValue = GetPropertyValue(watch.watchedObject, watch.typeInfo, watch.config.propertyPath);
    if (!CompareValues(watch.lastValue, currentValue)) {
        ProcessChange(watch, currentValue);
    }
}

void PropertyWatcher::NotifyChange(const std::string& watchId, const std::any& oldValue, const std::any& newValue) {
    std::lock_guard lock(m_mutex);

    auto it = m_watches.find(watchId);
    if (it == m_watches.end() || !it->second.enabled) return;

    auto& watch = it->second;
    watch.lastValue = oldValue;
    ProcessChange(watch, newValue);
}

// ============================================================================
// Batch Processing
// ============================================================================

void PropertyWatcher::FlushBatched() {
    std::lock_guard lock(m_mutex);

    if (m_globalBatch.empty()) return;

    // Notify batch callbacks
    for (const auto& [_, callback] : m_batchCallbacks) {
        if (callback) {
            callback(m_globalBatch);
        }
    }

    // Flush per-watch batches
    for (auto& [watchId, watch] : m_watches) {
        if (!watch.batchedChanges.empty() && watch.callback) {
            for (const auto& change : watch.batchedChanges) {
                watch.callback(change);
            }
            watch.batchedChanges.clear();
        }
    }

    m_globalBatch.clear();
}

size_t PropertyWatcher::OnBatch(BatchCallback callback) {
    std::lock_guard lock(m_mutex);
    size_t id = m_nextBatchCallbackId++;
    m_batchCallbacks[id] = std::move(callback);
    return id;
}

void PropertyWatcher::RemoveBatchCallback(size_t callbackId) {
    std::lock_guard lock(m_mutex);
    m_batchCallbacks.erase(callbackId);
}

// ============================================================================
// Utilities
// ============================================================================

std::optional<std::any> PropertyWatcher::GetCurrentValue(const std::string& watchId) const {
    std::lock_guard lock(m_mutex);

    auto it = m_watches.find(watchId);
    if (it == m_watches.end()) {
        return std::nullopt;
    }

    return GetPropertyValue(it->second.watchedObject, it->second.typeInfo, it->second.config.propertyPath);
}

std::optional<std::any> PropertyWatcher::GetLastValue(const std::string& watchId) const {
    std::lock_guard lock(m_mutex);

    auto it = m_watches.find(watchId);
    if (it == m_watches.end()) {
        return std::nullopt;
    }

    return it->second.lastValue;
}

// ============================================================================
// Internal Helpers
// ============================================================================

std::any PropertyWatcher::GetPropertyValue(void* object, const Reflect::TypeInfo* typeInfo,
                                           const std::string& propertyPath) const {
    if (!object || !typeInfo) {
        return std::any{};
    }

    // Split property path
    std::vector<std::string> parts;
    std::istringstream ss(propertyPath);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    if (parts.empty()) {
        return std::any{};
    }

    // Navigate to property (simplified - only handles first level for now)
    const Reflect::PropertyInfo* prop = typeInfo->FindProperty(parts[0]);
    if (!prop || !prop->getterAny) {
        return std::any{};
    }

    return prop->getterAny(object);
}

bool PropertyWatcher::CompareValues(const std::any& a, const std::any& b) const {
    // Compare by type and value
    if (a.type() != b.type()) {
        return false;
    }

    // Handle common types
    if (a.type() == typeid(int)) {
        return std::any_cast<int>(a) == std::any_cast<int>(b);
    }
    if (a.type() == typeid(float)) {
        return std::abs(std::any_cast<float>(a) - std::any_cast<float>(b)) < 1e-6f;
    }
    if (a.type() == typeid(double)) {
        return std::abs(std::any_cast<double>(a) - std::any_cast<double>(b)) < 1e-9;
    }
    if (a.type() == typeid(bool)) {
        return std::any_cast<bool>(a) == std::any_cast<bool>(b);
    }
    if (a.type() == typeid(std::string)) {
        return std::any_cast<std::string>(a) == std::any_cast<std::string>(b);
    }

    // For other types, assume different
    return false;
}

bool PropertyWatcher::IsIncrease(const std::any& oldVal, const std::any& newVal) const {
    auto toDouble = [](const std::any& v) -> std::optional<double> {
        if (v.type() == typeid(int)) return static_cast<double>(std::any_cast<int>(v));
        if (v.type() == typeid(float)) return static_cast<double>(std::any_cast<float>(v));
        if (v.type() == typeid(double)) return std::any_cast<double>(v);
        return std::nullopt;
    };

    auto oldNum = toDouble(oldVal);
    auto newNum = toDouble(newVal);

    if (oldNum && newNum) {
        return *newNum > *oldNum;
    }

    return false;
}

bool PropertyWatcher::CheckThreshold(const std::any& value, float threshold, bool& wasAbove) const {
    auto toDouble = [](const std::any& v) -> std::optional<double> {
        if (v.type() == typeid(int)) return static_cast<double>(std::any_cast<int>(v));
        if (v.type() == typeid(float)) return static_cast<double>(std::any_cast<float>(v));
        if (v.type() == typeid(double)) return std::any_cast<double>(v);
        return std::nullopt;
    };

    auto num = toDouble(value);
    if (!num) return false;

    bool isAbove = *num > threshold;
    bool crossed = (wasAbove != isAbove);
    wasAbove = isAbove;
    return crossed;
}

void PropertyWatcher::ProcessChange(PropertyWatch& watch, const std::any& newValue) {
    auto& config = watch.config;
    auto now = std::chrono::system_clock::now();

    // Check direction filter
    bool isIncrease = IsIncrease(watch.lastValue, newValue);
    if (isIncrease && !config.notifyOnIncrease) return;
    if (!isIncrease && !config.notifyOnDecrease) return;

    // Build change data
    PropertyChangeData data;
    data.watchId = config.id;
    data.propertyPath = config.propertyPath;
    data.object = watch.watchedObject;
    data.typeInfo = watch.typeInfo;
    data.oldValue = watch.lastValue;
    data.newValue = newValue;
    data.changeTime = now;
    data.wasIncrease = isIncrease;

    // Check threshold
    if (config.mode == NotificationMode::Threshold) {
        bool crossed = CheckThreshold(newValue, config.thresholdHigh, watch.wasAboveThreshold);
        if (!crossed && config.notifyOnCross) {
            // Also check low threshold
            bool wasBelow;
            CheckThreshold(watch.lastValue, config.thresholdLow, wasBelow);
            crossed = CheckThreshold(newValue, config.thresholdLow, wasBelow);
        }
        data.crossedThreshold = crossed;
        data.thresholdCrossed = watch.wasAboveThreshold ? config.thresholdHigh : config.thresholdLow;

        if (!crossed && config.notifyOnCross) {
            watch.lastValue = newValue;
            return;  // Don't notify if threshold wasn't crossed
        }
    }

    // Handle based on mode
    switch (config.mode) {
        case NotificationMode::Immediate:
            NotifyCallback(watch, data);
            break;

        case NotificationMode::Debounced:
            watch.pendingOldValue = watch.hasPendingChange ? watch.pendingOldValue : watch.lastValue;
            watch.hasPendingChange = true;
            watch.debounceTimer = config.debounceTimeMs;
            break;

        case NotificationMode::Throttled: {
            auto elapsed = std::chrono::duration<float, std::milli>(now - watch.lastNotificationTime).count();
            if (elapsed >= config.throttleIntervalMs) {
                NotifyCallback(watch, data);
            }
            break;
        }

        case NotificationMode::Batched:
            watch.batchedChanges.push_back(data);
            m_globalBatch.push_back(data);
            if (m_globalBatchTimer <= 0.0f) {
                m_globalBatchTimer = config.batchIntervalMs;
            }
            break;

        case NotificationMode::Threshold:
            NotifyCallback(watch, data);
            break;
    }

    watch.lastValue = newValue;
    watch.lastChangeTime = now;
}

void PropertyWatcher::NotifyCallback(PropertyWatch& watch, const PropertyChangeData& data) {
    if (watch.callback) {
        watch.callback(data);
    }
    watch.lastNotificationTime = std::chrono::system_clock::now();
}

std::string PropertyWatcher::GenerateWatchId() {
    return "watch_" + std::to_string(m_nextWatchId++);
}

} // namespace Events
} // namespace Nova
