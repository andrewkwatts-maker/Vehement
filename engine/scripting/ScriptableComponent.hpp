#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

namespace Nova {
namespace Scripting {

// Forward declarations
class PythonEngine;
class ScriptContext;
class Blackboard;

/**
 * @brief Script state that can be attached to an entity
 *
 * Contains per-entity variables and state for scripts.
 */
class ScriptState {
public:
    ScriptState() = default;
    ~ScriptState() = default;

    // Variable storage
    void SetInt(const std::string& name, int value) { m_ints[name] = value; }
    void SetFloat(const std::string& name, float value) { m_floats[name] = value; }
    void SetBool(const std::string& name, bool value) { m_bools[name] = value; }
    void SetString(const std::string& name, const std::string& value) { m_strings[name] = value; }

    [[nodiscard]] int GetInt(const std::string& name, int defaultVal = 0) const {
        auto it = m_ints.find(name);
        return it != m_ints.end() ? it->second : defaultVal;
    }

    [[nodiscard]] float GetFloat(const std::string& name, float defaultVal = 0.0f) const {
        auto it = m_floats.find(name);
        return it != m_floats.end() ? it->second : defaultVal;
    }

    [[nodiscard]] bool GetBool(const std::string& name, bool defaultVal = false) const {
        auto it = m_bools.find(name);
        return it != m_bools.end() ? it->second : defaultVal;
    }

    [[nodiscard]] std::string GetString(const std::string& name, const std::string& defaultVal = "") const {
        auto it = m_strings.find(name);
        return it != m_strings.end() ? it->second : defaultVal;
    }

    [[nodiscard]] bool HasVariable(const std::string& name) const {
        return m_ints.contains(name) || m_floats.contains(name) ||
               m_bools.contains(name) || m_strings.contains(name);
    }

    void RemoveVariable(const std::string& name) {
        m_ints.erase(name);
        m_floats.erase(name);
        m_bools.erase(name);
        m_strings.erase(name);
    }

    void Clear() {
        m_ints.clear();
        m_floats.clear();
        m_bools.clear();
        m_strings.clear();
    }

    [[nodiscard]] std::vector<std::string> GetVariableNames() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : m_ints) names.push_back(name);
        for (const auto& [name, _] : m_floats) names.push_back(name);
        for (const auto& [name, _] : m_bools) names.push_back(name);
        for (const auto& [name, _] : m_strings) names.push_back(name);
        return names;
    }

private:
    std::unordered_map<std::string, int> m_ints;
    std::unordered_map<std::string, float> m_floats;
    std::unordered_map<std::string, bool> m_bools;
    std::unordered_map<std::string, std::string> m_strings;
};

/**
 * @brief Event callback registration for scripted entities
 */
struct ScriptEventCallback {
    std::string eventName;
    std::string pythonModule;
    std::string pythonFunction;
    bool enabled = true;
};

/**
 * @brief ECS component for attaching scripts to entities
 *
 * Features:
 * - Attach scripts to entities
 * - Per-entity script state
 * - Update tick callback
 * - Event callback registration
 *
 * Usage:
 * @code
 * // In C++
 * ScriptableComponent script;
 * script.SetScriptPath("ai/zombie_ai.py");
 * script.SetUpdateFunction("zombie_ai", "update");
 * script.AddEventCallback("on_damage", "zombie_ai", "handle_damage");
 *
 * // In Python (zombie_ai.py):
 * def update(entity_id, delta_time, state):
 *     # AI logic here
 *     pass
 *
 * def handle_damage(entity_id, event, state):
 *     # React to damage
 *     pass
 * @endcode
 */
class ScriptableComponent {
public:
    ScriptableComponent();
    explicit ScriptableComponent(const std::string& scriptPath);
    ~ScriptableComponent();

    ScriptableComponent(const ScriptableComponent&) = delete;
    ScriptableComponent& operator=(const ScriptableComponent&) = delete;
    ScriptableComponent(ScriptableComponent&&) noexcept = default;
    ScriptableComponent& operator=(ScriptableComponent&&) noexcept = default;

    // =========================================================================
    // Script Configuration
    // =========================================================================

    /**
     * @brief Set the script file path
     */
    void SetScriptPath(const std::string& path) { m_scriptPath = path; }

    /**
     * @brief Get the script file path
     */
    [[nodiscard]] const std::string& GetScriptPath() const { return m_scriptPath; }

    /**
     * @brief Set the Python module and function for update ticks
     */
    void SetUpdateFunction(const std::string& module, const std::string& function) {
        m_updateModule = module;
        m_updateFunction = function;
    }

    /**
     * @brief Set the init function (called once when script loads)
     */
    void SetInitFunction(const std::string& module, const std::string& function) {
        m_initModule = module;
        m_initFunction = function;
    }

    /**
     * @brief Set the cleanup function (called when component is destroyed)
     */
    void SetCleanupFunction(const std::string& module, const std::string& function) {
        m_cleanupModule = module;
        m_cleanupFunction = function;
    }

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    /**
     * @brief Register an event callback
     */
    void AddEventCallback(const std::string& eventName,
                          const std::string& module,
                          const std::string& function);

    /**
     * @brief Remove an event callback
     */
    void RemoveEventCallback(const std::string& eventName);

    /**
     * @brief Enable/disable an event callback
     */
    void SetEventCallbackEnabled(const std::string& eventName, bool enabled);

    /**
     * @brief Get all registered event callbacks
     */
    [[nodiscard]] const std::vector<ScriptEventCallback>& GetEventCallbacks() const {
        return m_eventCallbacks;
    }

    // =========================================================================
    // Script State
    // =========================================================================

    /**
     * @brief Get the script state for this entity
     */
    [[nodiscard]] ScriptState& GetState() { return m_state; }
    [[nodiscard]] const ScriptState& GetState() const { return m_state; }

    /**
     * @brief Get the entity ID this component is attached to
     */
    [[nodiscard]] uint32_t GetEntityId() const { return m_entityId; }

    /**
     * @brief Set the entity ID (called by entity manager)
     */
    void SetEntityId(uint32_t id) { m_entityId = id; }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the script (load and call init function)
     */
    bool Initialize(PythonEngine& engine);

    /**
     * @brief Update the script (call update function)
     */
    void Update(PythonEngine& engine, float deltaTime);

    /**
     * @brief Handle an event
     */
    void HandleEvent(PythonEngine& engine, const std::string& eventName,
                     const std::unordered_map<std::string, std::string>& eventData = {});

    /**
     * @brief Cleanup the script
     */
    void Cleanup(PythonEngine& engine);

    /**
     * @brief Check if script is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Check if script is enabled
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Enable/disable the script
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    // =========================================================================
    // Update Rate Control
    // =========================================================================

    /**
     * @brief Set update interval (0 = every frame)
     */
    void SetUpdateInterval(float interval) { m_updateInterval = interval; }

    /**
     * @brief Get update interval
     */
    [[nodiscard]] float GetUpdateInterval() const { return m_updateInterval; }

    // =========================================================================
    // Metrics
    // =========================================================================

    struct Metrics {
        size_t updateCalls = 0;
        size_t eventCalls = 0;
        double totalUpdateTimeMs = 0.0;
        double avgUpdateTimeMs = 0.0;
        double maxUpdateTimeMs = 0.0;
        std::chrono::system_clock::time_point lastUpdate;

        void RecordUpdate(double timeMs) {
            updateCalls++;
            totalUpdateTimeMs += timeMs;
            avgUpdateTimeMs = totalUpdateTimeMs / updateCalls;
            if (timeMs > maxUpdateTimeMs) maxUpdateTimeMs = timeMs;
            lastUpdate = std::chrono::system_clock::now();
        }
    };

    [[nodiscard]] const Metrics& GetMetrics() const { return m_metrics; }

private:
    // Script configuration
    std::string m_scriptPath;
    std::string m_updateModule;
    std::string m_updateFunction;
    std::string m_initModule;
    std::string m_initFunction;
    std::string m_cleanupModule;
    std::string m_cleanupFunction;

    // Event callbacks
    std::vector<ScriptEventCallback> m_eventCallbacks;

    // State
    ScriptState m_state;
    uint32_t m_entityId = 0;
    bool m_initialized = false;
    bool m_enabled = true;

    // Update rate control
    float m_updateInterval = 0.0f;
    float m_timeSinceUpdate = 0.0f;

    // Metrics
    Metrics m_metrics;
};

/**
 * @brief Manager for scriptable components
 */
class ScriptableComponentManager {
public:
    ScriptableComponentManager();
    ~ScriptableComponentManager();

    /**
     * @brief Attach a scriptable component to an entity
     */
    ScriptableComponent* AttachScript(uint32_t entityId, const std::string& scriptPath);

    /**
     * @brief Attach an existing component
     */
    void AttachComponent(uint32_t entityId, std::unique_ptr<ScriptableComponent> component);

    /**
     * @brief Detach script from entity
     */
    void DetachScript(uint32_t entityId);

    /**
     * @brief Get script component for entity
     */
    [[nodiscard]] ScriptableComponent* GetComponent(uint32_t entityId);

    /**
     * @brief Check if entity has script component
     */
    [[nodiscard]] bool HasComponent(uint32_t entityId) const;

    /**
     * @brief Update all script components
     */
    void Update(float deltaTime);

    /**
     * @brief Broadcast event to all script components
     */
    void BroadcastEvent(const std::string& eventName,
                        const std::unordered_map<std::string, std::string>& eventData = {});

    /**
     * @brief Send event to specific entity's script
     */
    void SendEvent(uint32_t entityId, const std::string& eventName,
                   const std::unordered_map<std::string, std::string>& eventData = {});

    /**
     * @brief Set the Python engine
     */
    void SetPythonEngine(PythonEngine* engine) { m_pythonEngine = engine; }

    /**
     * @brief Initialize all pending components
     */
    void InitializeAll();

    /**
     * @brief Cleanup all components
     */
    void CleanupAll();

    /**
     * @brief Get number of scripted entities
     */
    [[nodiscard]] size_t GetComponentCount() const { return m_components.size(); }

    /**
     * @brief Get all entity IDs with script components
     */
    [[nodiscard]] std::vector<uint32_t> GetScriptedEntities() const;

private:
    std::unordered_map<uint32_t, std::unique_ptr<ScriptableComponent>> m_components;
    PythonEngine* m_pythonEngine = nullptr;
};

} // namespace Scripting
} // namespace Nova
