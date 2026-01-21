#pragma once

#include "EventBinding.hpp"
#include "../reflection/EventBus.hpp"
#ifdef NOVA_SCRIPTING_ENABLED
#include "../scripting/PythonEngine.hpp"
#endif
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <filesystem>

namespace Nova {
namespace Events {

/**
 * @brief Manages all event bindings, loading, saving, and execution
 *
 * Features:
 * - Load/save bindings from JSON files
 * - Runtime binding creation/removal
 * - Binding validation
 * - Python integration via PythonEngine
 * - Hot-reload support
 * - Binding execution and error handling
 */
class EventBindingManager {
public:
    /**
     * @brief Configuration for the binding manager
     */
    struct Config {
        std::string bindingsDirectory = "assets/configs/bindings";
        bool enableHotReload = true;
        float hotReloadInterval = 2.0f;  // Seconds between reload checks
        bool logBindingExecution = false;
        bool validateOnLoad = true;
        size_t maxQueuedCallbacks = 1000;
    };

    /**
     * @brief Get singleton instance
     */
    static EventBindingManager& Instance();

    // Non-copyable
    EventBindingManager(const EventBindingManager&) = delete;
    EventBindingManager& operator=(const EventBindingManager&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the binding manager
     * @param config Configuration options
     * @return true if initialization succeeded
     */
    bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Update the manager (call each frame)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Python Integration
    // =========================================================================

#ifdef NOVA_SCRIPTING_ENABLED
    /**
     * @brief Set the Python engine for executing callbacks
     */
    void SetPythonEngine(Scripting::PythonEngine* engine);

    /**
     * @brief Get the Python engine
     */
    [[nodiscard]] Scripting::PythonEngine* GetPythonEngine() const { return m_pythonEngine; }
#endif

    // =========================================================================
    // Binding Management
    // =========================================================================

    /**
     * @brief Add a binding
     * @param binding Binding to add
     * @return true if added successfully
     */
    bool AddBinding(EventBinding binding);

    /**
     * @brief Add a binding with automatic ID generation
     */
    std::string AddBindingAuto(EventBinding binding);

    /**
     * @brief Remove a binding by ID
     */
    bool RemoveBinding(const std::string& bindingId);

    /**
     * @brief Get a binding by ID
     */
    [[nodiscard]] EventBinding* GetBinding(const std::string& bindingId);
    [[nodiscard]] const EventBinding* GetBinding(const std::string& bindingId) const;

    /**
     * @brief Check if a binding exists
     */
    [[nodiscard]] bool HasBinding(const std::string& bindingId) const;

    /**
     * @brief Enable/disable a binding
     */
    void SetBindingEnabled(const std::string& bindingId, bool enabled);

    /**
     * @brief Get all binding IDs
     */
    [[nodiscard]] std::vector<std::string> GetBindingIds() const;

    /**
     * @brief Get bindings by category
     */
    [[nodiscard]] std::vector<const EventBinding*> GetBindingsByCategory(const std::string& category) const;

    /**
     * @brief Get bindings for a specific event type
     */
    [[nodiscard]] std::vector<const EventBinding*> GetBindingsForEvent(const std::string& eventType) const;

    /**
     * @brief Get all bindings
     */
    [[nodiscard]] std::vector<const EventBinding*> GetAllBindings() const;

    /**
     * @brief Get number of bindings
     */
    [[nodiscard]] size_t GetBindingCount() const;

    /**
     * @brief Clear all bindings
     */
    void ClearBindings();

    // =========================================================================
    // Loading and Saving
    // =========================================================================

    /**
     * @brief Load bindings from a JSON file
     * @param filePath Path to the JSON file
     * @return Number of bindings loaded, -1 on error
     */
    int LoadBindingsFromFile(const std::string& filePath);

    /**
     * @brief Load all bindings from the configured directory
     */
    int LoadAllBindings();

    /**
     * @brief Save bindings to a JSON file
     * @param filePath Path to save to
     * @param bindingIds Optional list of binding IDs (empty = all)
     * @return true if saved successfully
     */
    bool SaveBindingsToFile(const std::string& filePath,
                            const std::vector<std::string>& bindingIds = {});

    /**
     * @brief Export a binding group to JSON
     */
    [[nodiscard]] json ExportBindings(const std::vector<std::string>& bindingIds) const;

    /**
     * @brief Import bindings from JSON
     */
    int ImportBindings(const json& bindingsJson);

    /**
     * @brief Reload bindings from files
     */
    void ReloadBindings();

    // =========================================================================
    // Binding Validation
    // =========================================================================

    /**
     * @brief Validate a binding
     * @return Error message or empty if valid
     */
    [[nodiscard]] std::string ValidateBinding(const EventBinding& binding) const;

    /**
     * @brief Validate all bindings
     * @return Map of binding ID to error messages
     */
    [[nodiscard]] std::unordered_map<std::string, std::string> ValidateAllBindings() const;

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Execute a binding's callback
     * @param bindingId Binding ID to execute
     * @param eventData Event data to pass to callback
     * @return true if executed successfully
     */
    bool ExecuteBinding(const std::string& bindingId,
                        const std::unordered_map<std::string, std::any>& eventData = {});

    /**
     * @brief Test execute a binding (dry run)
     */
    bool TestBinding(const std::string& bindingId);

    /**
     * @brief Get execution statistics
     */
    struct ExecutionStats {
        size_t totalExecutions = 0;
        size_t successfulExecutions = 0;
        size_t failedExecutions = 0;
        double totalExecutionTimeMs = 0.0;
        std::unordered_map<std::string, size_t> executionsPerBinding;
    };

    [[nodiscard]] const ExecutionStats& GetExecutionStats() const { return m_stats; }
    void ResetExecutionStats() { m_stats = ExecutionStats{}; }

    // =========================================================================
    // Hot Reload
    // =========================================================================

    /**
     * @brief Enable/disable hot reload
     */
    void SetHotReloadEnabled(bool enabled);

    /**
     * @brief Force check for file changes
     */
    void CheckHotReload();

    /**
     * @brief Register callback for binding changes
     */
    using BindingChangedCallback = std::function<void(const std::string& bindingId, bool added)>;
    size_t OnBindingChanged(BindingChangedCallback callback);
    void RemoveBindingChangedCallback(size_t callbackId);

    // =========================================================================
    // Event Integration
    // =========================================================================

    /**
     * @brief Register all bindings with the event bus
     */
    void RegisterWithEventBus(Reflect::EventBus& eventBus);

    /**
     * @brief Unregister from event bus
     */
    void UnregisterFromEventBus(Reflect::EventBus& eventBus);

private:
    EventBindingManager() = default;
    ~EventBindingManager();

    // Internal methods
    void ExecutePythonCallback(const EventBinding& binding,
                               const std::unordered_map<std::string, std::any>& eventData);
    void ExecuteEventEmission(const EventBinding& binding,
                              const std::unordered_map<std::string, std::any>& eventData);
    void ExecuteCommand(const EventBinding& binding,
                        const std::unordered_map<std::string, std::any>& eventData);
    void ScheduleDelayedExecution(const std::string& bindingId,
                                  const std::unordered_map<std::string, std::any>& eventData,
                                  float delay);
    void ProcessDelayedExecutions(float deltaTime);
    void NotifyBindingChanged(const std::string& bindingId, bool added);

    // State
    bool m_initialized = false;
    Config m_config;

    // Bindings storage
    std::unordered_map<std::string, EventBinding> m_bindings;
    std::unordered_map<std::string, std::vector<std::string>> m_bindingsByEvent;
    std::unordered_map<std::string, std::vector<std::string>> m_bindingsByCategory;
    mutable std::mutex m_bindingsMutex;

    // Python engine
#ifdef NOVA_SCRIPTING_ENABLED
    Scripting::PythonEngine* m_pythonEngine = nullptr;
#endif

    // File tracking for hot reload
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileModTimes;
    float m_hotReloadTimer = 0.0f;

    // Delayed executions
    struct DelayedExecution {
        std::string bindingId;
        std::unordered_map<std::string, std::any> eventData;
        float delay;
    };
    std::vector<DelayedExecution> m_delayedExecutions;
    std::mutex m_delayedMutex;

    // Callbacks
    std::unordered_map<size_t, BindingChangedCallback> m_changeCallbacks;
    size_t m_nextCallbackId = 1;
    std::mutex m_callbackMutex;

    // Event bus subscriptions
    std::vector<size_t> m_eventBusSubscriptions;

    // Statistics
    ExecutionStats m_stats;

    // ID generation
    std::atomic<uint64_t> m_nextBindingId{1};
};

} // namespace Events
} // namespace Nova
