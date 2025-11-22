#pragma once

#include "../reflection/TypeInfo.hpp"
#include "../reflection/Observable.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <any>
#include <optional>

namespace Nova {
namespace Events {

/**
 * @brief Notification mode for property watchers
 */
enum class NotificationMode {
    Immediate,      // Notify immediately on change
    Debounced,      // Wait for changes to settle
    Throttled,      // Notify at most once per interval
    Batched,        // Collect changes and notify in batch
    Threshold       // Only notify when crossing threshold
};

/**
 * @brief Configuration for a property watch
 */
struct PropertyWatchConfig {
    std::string id;
    std::string propertyPath;           // e.g., "health.current", "position.x"
    NotificationMode mode = NotificationMode::Immediate;
    float debounceTimeMs = 100.0f;      // For Debounced mode
    float throttleIntervalMs = 16.0f;   // For Throttled mode (~60fps)
    float batchIntervalMs = 100.0f;     // For Batched mode

    // Threshold settings
    float thresholdLow = 0.0f;
    float thresholdHigh = 100.0f;
    bool notifyOnCross = true;          // Notify when crossing threshold

    // Filter settings
    bool notifyOnIncrease = true;
    bool notifyOnDecrease = true;

    // Builder pattern
    PropertyWatchConfig& WithId(const std::string& watchId) { id = watchId; return *this; }
    PropertyWatchConfig& WithPath(const std::string& path) { propertyPath = path; return *this; }
    PropertyWatchConfig& WithMode(NotificationMode m) { mode = m; return *this; }
    PropertyWatchConfig& WithDebounce(float ms) { mode = NotificationMode::Debounced; debounceTimeMs = ms; return *this; }
    PropertyWatchConfig& WithThrottle(float ms) { mode = NotificationMode::Throttled; throttleIntervalMs = ms; return *this; }
    PropertyWatchConfig& WithBatching(float ms) { mode = NotificationMode::Batched; batchIntervalMs = ms; return *this; }
    PropertyWatchConfig& WithThreshold(float low, float high) {
        mode = NotificationMode::Threshold;
        thresholdLow = low;
        thresholdHigh = high;
        return *this;
    }
    PropertyWatchConfig& OnlyIncrease() { notifyOnIncrease = true; notifyOnDecrease = false; return *this; }
    PropertyWatchConfig& OnlyDecrease() { notifyOnIncrease = false; notifyOnDecrease = true; return *this; }
};

/**
 * @brief Data passed to property change callbacks
 */
struct PropertyChangeData {
    std::string watchId;
    std::string propertyPath;
    void* object = nullptr;
    const Reflect::TypeInfo* typeInfo = nullptr;
    std::any oldValue;
    std::any newValue;
    std::chrono::system_clock::time_point changeTime;
    bool wasIncrease = false;
    bool crossedThreshold = false;
    float thresholdCrossed = 0.0f;
};

/**
 * @brief Callback type for property changes
 */
using PropertyChangeCallback = std::function<void(const PropertyChangeData&)>;

/**
 * @brief Internal watch state
 */
struct PropertyWatch {
    PropertyWatchConfig config;
    PropertyChangeCallback callback;
    void* watchedObject = nullptr;
    const Reflect::TypeInfo* typeInfo = nullptr;
    std::any lastValue;
    std::any pendingOldValue;
    bool hasPendingChange = false;
    std::chrono::system_clock::time_point lastChangeTime;
    std::chrono::system_clock::time_point lastNotificationTime;
    float debounceTimer = 0.0f;
    bool enabled = true;
    bool wasAboveThreshold = false;

    // Batched changes
    std::vector<PropertyChangeData> batchedChanges;
};

/**
 * @brief Watches specific properties on objects and triggers callbacks
 *
 * Features:
 * - Subscribe to property changes on specific objects
 * - Threshold-based notifications
 * - Debounced notifications
 * - Batch notifications
 *
 * Usage:
 * @code
 * PropertyWatcher watcher;
 *
 * // Watch health property with debouncing
 * watcher.Watch(unit, unitTypeInfo, "health.current",
 *     PropertyWatchConfig()
 *         .WithDebounce(100.0f),
 *     [](const PropertyChangeData& data) {
 *         // Handle health change
 *     });
 *
 * // Watch with threshold
 * watcher.Watch(unit, unitTypeInfo, "health.percentage",
 *     PropertyWatchConfig()
 *         .WithThreshold(0.0f, 25.0f),
 *     [](const PropertyChangeData& data) {
 *         if (data.crossedThreshold) {
 *             // Low health warning!
 *         }
 *     });
 *
 * // Update each frame
 * watcher.Update(deltaTime);
 * @endcode
 */
class PropertyWatcher {
public:
    PropertyWatcher() = default;
    ~PropertyWatcher() = default;

    // Non-copyable
    PropertyWatcher(const PropertyWatcher&) = delete;
    PropertyWatcher& operator=(const PropertyWatcher&) = delete;

    // =========================================================================
    // Watch Management
    // =========================================================================

    /**
     * @brief Watch a property on an object
     * @param object Object to watch
     * @param typeInfo Type information for the object
     * @param propertyPath Path to the property
     * @param config Watch configuration
     * @param callback Callback when property changes
     * @return Watch ID
     */
    std::string Watch(void* object,
                      const Reflect::TypeInfo* typeInfo,
                      const std::string& propertyPath,
                      const PropertyWatchConfig& config,
                      PropertyChangeCallback callback);

    /**
     * @brief Watch with default configuration
     */
    std::string Watch(void* object,
                      const Reflect::TypeInfo* typeInfo,
                      const std::string& propertyPath,
                      PropertyChangeCallback callback);

    /**
     * @brief Stop watching a property
     */
    bool Unwatch(const std::string& watchId);

    /**
     * @brief Stop watching all properties on an object
     */
    void UnwatchObject(void* object);

    /**
     * @brief Stop all watches
     */
    void UnwatchAll();

    /**
     * @brief Enable/disable a watch
     */
    void SetWatchEnabled(const std::string& watchId, bool enabled);

    /**
     * @brief Check if a watch exists
     */
    [[nodiscard]] bool HasWatch(const std::string& watchId) const;

    /**
     * @brief Get number of active watches
     */
    [[nodiscard]] size_t GetWatchCount() const;

    /**
     * @brief Get all watch IDs
     */
    [[nodiscard]] std::vector<std::string> GetWatchIds() const;

    // =========================================================================
    // Update and Polling
    // =========================================================================

    /**
     * @brief Update watcher (call each frame)
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Manually poll all watches for changes
     */
    void PollAll();

    /**
     * @brief Manually poll a specific watch
     */
    void Poll(const std::string& watchId);

    /**
     * @brief Notify a change externally (for Observable integration)
     */
    void NotifyChange(const std::string& watchId, const std::any& oldValue, const std::any& newValue);

    // =========================================================================
    // Batch Processing
    // =========================================================================

    /**
     * @brief Flush all batched notifications
     */
    void FlushBatched();

    /**
     * @brief Register callback for batched changes
     */
    using BatchCallback = std::function<void(const std::vector<PropertyChangeData>&)>;
    size_t OnBatch(BatchCallback callback);
    void RemoveBatchCallback(size_t callbackId);

    // =========================================================================
    // Utilities
    // =========================================================================

    /**
     * @brief Get the current value of a watched property
     */
    [[nodiscard]] std::optional<std::any> GetCurrentValue(const std::string& watchId) const;

    /**
     * @brief Get the last notified value
     */
    [[nodiscard]] std::optional<std::any> GetLastValue(const std::string& watchId) const;

private:
    // Internal helpers
    std::any GetPropertyValue(void* object, const Reflect::TypeInfo* typeInfo,
                              const std::string& propertyPath) const;
    bool CompareValues(const std::any& a, const std::any& b) const;
    bool IsIncrease(const std::any& oldVal, const std::any& newVal) const;
    bool CheckThreshold(const std::any& value, float threshold, bool& wasAbove) const;
    void ProcessChange(PropertyWatch& watch, const std::any& newValue);
    void NotifyCallback(PropertyWatch& watch, const PropertyChangeData& data);
    std::string GenerateWatchId();

    // State
    std::unordered_map<std::string, PropertyWatch> m_watches;
    std::unordered_map<void*, std::vector<std::string>> m_watchesByObject;
    mutable std::mutex m_mutex;

    // Batch callbacks
    std::unordered_map<size_t, BatchCallback> m_batchCallbacks;
    size_t m_nextBatchCallbackId = 1;
    std::vector<PropertyChangeData> m_globalBatch;
    float m_globalBatchTimer = 0.0f;

    // ID generation
    std::atomic<uint64_t> m_nextWatchId{1};
};

/**
 * @brief RAII wrapper for property watches
 */
class ScopedPropertyWatch {
public:
    ScopedPropertyWatch() = default;
    ScopedPropertyWatch(PropertyWatcher& watcher, const std::string& watchId)
        : m_watcher(&watcher), m_watchId(watchId) {}

    ~ScopedPropertyWatch() {
        if (m_watcher && !m_watchId.empty()) {
            m_watcher->Unwatch(m_watchId);
        }
    }

    ScopedPropertyWatch(const ScopedPropertyWatch&) = delete;
    ScopedPropertyWatch& operator=(const ScopedPropertyWatch&) = delete;

    ScopedPropertyWatch(ScopedPropertyWatch&& other) noexcept
        : m_watcher(other.m_watcher), m_watchId(std::move(other.m_watchId)) {
        other.m_watcher = nullptr;
    }

    ScopedPropertyWatch& operator=(ScopedPropertyWatch&& other) noexcept {
        if (this != &other) {
            if (m_watcher && !m_watchId.empty()) {
                m_watcher->Unwatch(m_watchId);
            }
            m_watcher = other.m_watcher;
            m_watchId = std::move(other.m_watchId);
            other.m_watcher = nullptr;
        }
        return *this;
    }

    void Release() {
        m_watcher = nullptr;
        m_watchId.clear();
    }

    [[nodiscard]] const std::string& GetWatchId() const { return m_watchId; }

private:
    PropertyWatcher* m_watcher = nullptr;
    std::string m_watchId;
};

} // namespace Events
} // namespace Nova
