#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>
#include <string>
#include <optional>

namespace Nova {
namespace Reflect {

/**
 * @brief Connection handle for observer subscriptions
 */
class ObserverConnection {
public:
    ObserverConnection() = default;
    explicit ObserverConnection(std::shared_ptr<bool> connected)
        : m_connected(std::move(connected)) {}

    void Disconnect() {
        if (m_connected) {
            *m_connected = false;
        }
    }

    [[nodiscard]] bool IsConnected() const {
        return m_connected && *m_connected;
    }

private:
    std::shared_ptr<bool> m_connected;
};

/**
 * @brief Scoped connection that auto-disconnects on destruction
 */
class ScopedConnection {
public:
    ScopedConnection() = default;
    explicit ScopedConnection(ObserverConnection conn) : m_connection(std::move(conn)) {}
    ~ScopedConnection() { m_connection.Disconnect(); }

    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ScopedConnection(ScopedConnection&& other) noexcept
        : m_connection(std::move(other.m_connection)) {}

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            m_connection.Disconnect();
            m_connection = std::move(other.m_connection);
        }
        return *this;
    }

    void Disconnect() { m_connection.Disconnect(); }
    [[nodiscard]] bool IsConnected() const { return m_connection.IsConnected(); }

private:
    ObserverConnection m_connection;
};

/**
 * @brief Observable property wrapper with change notification support
 *
 * @tparam T Value type
 *
 * Usage:
 * @code
 * Observable<int> health(100);
 *
 * // Subscribe to changes
 * auto conn = health.OnChanged([](int oldVal, int newVal) {
 *     std::cout << "Health changed from " << oldVal << " to " << newVal << std::endl;
 * });
 *
 * health.Set(80); // Triggers callback
 * health = 60;    // Also triggers callback via operator=
 * @endcode
 */
template<typename T>
class Observable {
public:
    using ChangeCallback = std::function<void(const T& oldValue, const T& newValue)>;

    // Constructors
    Observable() : m_value{} {}
    explicit Observable(const T& value) : m_value(value) {}
    explicit Observable(T&& value) : m_value(std::move(value)) {}

    // Copy/Move
    Observable(const Observable& other) : m_value(other.m_value) {}
    Observable(Observable&& other) noexcept : m_value(std::move(other.m_value)) {}

    Observable& operator=(const Observable& other) {
        if (this != &other) {
            Set(other.m_value);
        }
        return *this;
    }

    Observable& operator=(Observable&& other) noexcept {
        if (this != &other) {
            Set(std::move(other.m_value));
        }
        return *this;
    }

    // Value assignment
    Observable& operator=(const T& value) {
        Set(value);
        return *this;
    }

    Observable& operator=(T&& value) {
        Set(std::move(value));
        return *this;
    }

    // =========================================================================
    // Value Access
    // =========================================================================

    /**
     * @brief Set new value and notify observers if changed
     */
    void Set(const T& newValue) {
        std::lock_guard lock(m_mutex);
        if (m_value != newValue) {
            T oldValue = std::move(m_value);
            m_value = newValue;
            NotifyObservers(oldValue, m_value);
        }
    }

    void Set(T&& newValue) {
        std::lock_guard lock(m_mutex);
        if (m_value != newValue) {
            T oldValue = std::move(m_value);
            m_value = std::move(newValue);
            NotifyObservers(oldValue, m_value);
        }
    }

    /**
     * @brief Set value without notification
     */
    void SetSilent(const T& newValue) {
        std::lock_guard lock(m_mutex);
        m_value = newValue;
    }

    void SetSilent(T&& newValue) {
        std::lock_guard lock(m_mutex);
        m_value = std::move(newValue);
    }

    /**
     * @brief Force notify observers with current value
     */
    void ForceNotify() {
        std::lock_guard lock(m_mutex);
        NotifyObservers(m_value, m_value);
    }

    /**
     * @brief Get current value
     */
    [[nodiscard]] const T& Get() const {
        std::lock_guard lock(m_mutex);
        return m_value;
    }

    /**
     * @brief Get value (implicit conversion)
     */
    operator const T&() const { return Get(); }

    /**
     * @brief Arrow operator for member access
     */
    const T* operator->() const {
        std::lock_guard lock(m_mutex);
        return &m_value;
    }

    // =========================================================================
    // Observer Management
    // =========================================================================

    /**
     * @brief Subscribe to value changes
     * @param callback Function called with (oldValue, newValue)
     * @return Connection handle
     */
    [[nodiscard]] ObserverConnection OnChanged(ChangeCallback callback) {
        std::lock_guard lock(m_observerMutex);

        auto connected = std::make_shared<bool>(true);
        m_observers.push_back({std::move(callback), connected});
        return ObserverConnection(connected);
    }

    /**
     * @brief Subscribe and immediately call with current value
     */
    [[nodiscard]] ObserverConnection OnChangedAndNow(ChangeCallback callback) {
        auto conn = OnChanged(callback);
        std::lock_guard lock(m_mutex);
        callback(m_value, m_value);
        return conn;
    }

    /**
     * @brief Remove all observers
     */
    void ClearObservers() {
        std::lock_guard lock(m_observerMutex);
        m_observers.clear();
    }

    /**
     * @brief Get number of active observers
     */
    [[nodiscard]] size_t GetObserverCount() const {
        std::lock_guard lock(m_observerMutex);
        return std::count_if(m_observers.begin(), m_observers.end(),
            [](const Observer& obs) { return obs.connected && *obs.connected; });
    }

private:
    struct Observer {
        ChangeCallback callback;
        std::shared_ptr<bool> connected;
    };

    void NotifyObservers(const T& oldValue, const T& newValue) {
        std::vector<ChangeCallback> callbacks;

        {
            std::lock_guard lock(m_observerMutex);

            // Remove disconnected observers
            m_observers.erase(
                std::remove_if(m_observers.begin(), m_observers.end(),
                    [](const Observer& obs) { return !obs.connected || !*obs.connected; }),
                m_observers.end()
            );

            // Collect callbacks to invoke outside the lock
            callbacks.reserve(m_observers.size());
            for (const auto& obs : m_observers) {
                if (obs.callback) {
                    callbacks.push_back(obs.callback);
                }
            }
        }

        // Invoke callbacks outside the lock to prevent deadlocks
        for (const auto& callback : callbacks) {
            callback(oldValue, newValue);
        }
    }

    T m_value;
    std::vector<Observer> m_observers;
    mutable std::mutex m_mutex;
    mutable std::mutex m_observerMutex;
};

/**
 * @brief Observable property with name for debugging/logging
 */
template<typename T>
class NamedObservable : public Observable<T> {
public:
    NamedObservable(const std::string& name) : m_name(name) {}
    NamedObservable(const std::string& name, const T& value) : Observable<T>(value), m_name(name) {}

    [[nodiscard]] const std::string& GetName() const { return m_name; }

private:
    std::string m_name;
};

/**
 * @brief Observable with value clamping
 */
template<typename T>
class ClampedObservable : public Observable<T> {
public:
    ClampedObservable(const T& min, const T& max)
        : m_min(min), m_max(max) {}

    ClampedObservable(const T& value, const T& min, const T& max)
        : Observable<T>(std::clamp(value, min, max)), m_min(min), m_max(max) {}

    void Set(const T& newValue) {
        Observable<T>::Set(std::clamp(newValue, m_min, m_max));
    }

    void SetRange(const T& min, const T& max) {
        m_min = min;
        m_max = max;
        Set(Observable<T>::Get()); // Re-clamp current value
    }

    [[nodiscard]] T GetMin() const { return m_min; }
    [[nodiscard]] T GetMax() const { return m_max; }

private:
    T m_min;
    T m_max;
};

/**
 * @brief Collection of observable connections for batch management
 */
class ConnectionGroup {
public:
    void Add(ObserverConnection conn) {
        m_connections.push_back(std::move(conn));
    }

    void Add(ScopedConnection&& conn) {
        m_scopedConnections.push_back(std::move(conn));
    }

    void DisconnectAll() {
        for (auto& conn : m_connections) {
            conn.Disconnect();
        }
        m_connections.clear();
        m_scopedConnections.clear();
    }

    [[nodiscard]] size_t GetConnectionCount() const {
        return m_connections.size() + m_scopedConnections.size();
    }

private:
    std::vector<ObserverConnection> m_connections;
    std::vector<ScopedConnection> m_scopedConnections;
};

} // namespace Reflect
} // namespace Nova
