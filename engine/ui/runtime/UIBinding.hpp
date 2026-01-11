#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <any>

// Using nlohmann/json for JSON handling
#include <nlohmann/json.hpp>

namespace Engine {
namespace UI {

/**
 * @brief Property binding with getter/setter
 */
struct PropertyBinding {
    std::string name;
    std::function<nlohmann::json()> getter;
    std::function<void(const nlohmann::json&)> setter;
    bool readOnly = false;
};

/**
 * @brief Function binding
 */
struct FunctionBinding {
    std::string name;
    std::function<nlohmann::json(const nlohmann::json&)> handler;
    std::vector<std::string> parameterNames;
    std::string description;
};

/**
 * @brief Event subscription
 */
struct EventSubscription {
    std::string event;
    std::function<void(const nlohmann::json&)> handler;
    int priority = 0;
    bool once = false;
};

/**
 * @brief Binding context for namespaced bindings
 */
class BindingContext {
public:
    BindingContext(const std::string& name);
    ~BindingContext();

    /**
     * @brief Expose a C++ function to JavaScript
     * @param name Function name
     * @param fn Function handler
     */
    void ExposeFunction(const std::string& name,
                       std::function<nlohmann::json(const nlohmann::json&)> fn);

    /**
     * @brief Expose a C++ function with description
     */
    void ExposeFunction(const std::string& name,
                       std::function<nlohmann::json(const nlohmann::json&)> fn,
                       const std::string& description,
                       const std::vector<std::string>& paramNames = {});

    /**
     * @brief Expose a C++ property with getter and setter
     * @param name Property name
     * @param getter Getter function
     * @param setter Setter function (nullptr for read-only)
     */
    void ExposeProperty(const std::string& name,
                       std::function<nlohmann::json()> getter,
                       std::function<void(const nlohmann::json&)> setter = nullptr);

    /**
     * @brief Expose a constant value
     */
    void ExposeConstant(const std::string& name, const nlohmann::json& value);

    /**
     * @brief Get context name
     */
    const std::string& GetName() const { return m_name; }

    /**
     * @brief Get all functions
     */
    const std::unordered_map<std::string, FunctionBinding>& GetFunctions() const { return m_functions; }

    /**
     * @brief Get all properties
     */
    const std::unordered_map<std::string, PropertyBinding>& GetProperties() const { return m_properties; }

    /**
     * @brief Get all constants
     */
    const std::unordered_map<std::string, nlohmann::json>& GetConstants() const { return m_constants; }

private:
    std::string m_name;
    std::unordered_map<std::string, FunctionBinding> m_functions;
    std::unordered_map<std::string, PropertyBinding> m_properties;
    std::unordered_map<std::string, nlohmann::json> m_constants;
};

/**
 * @brief JavaScript execution result
 */
struct JSResult {
    bool success = true;
    nlohmann::json value;
    std::string error;
};

/**
 * @brief Pending JavaScript call
 */
struct PendingJSCall {
    std::string function;
    nlohmann::json args;
    std::function<void(const JSResult&)> callback;
    int callId = 0;
};

/**
 * @brief Game-to-UI Bindings System
 *
 * Provides two-way communication between C++ game code and JavaScript UI code.
 * Supports exposing functions, properties, events, and calling JS from C++.
 */
class UIBinding {
public:
    UIBinding();
    ~UIBinding();

    /**
     * @brief Initialize the binding system
     */
    void Initialize();

    /**
     * @brief Shutdown the binding system
     */
    void Shutdown();

    // Function Exposure

    /**
     * @brief Expose a C++ function to JavaScript
     * @param name Function name (can be namespaced like "Game.getPlayer")
     * @param fn Function handler that takes JSON args and returns JSON
     */
    void ExposeFunction(const std::string& name,
                       std::function<nlohmann::json(const nlohmann::json&)> fn);

    /**
     * @brief Expose a C++ function with metadata
     */
    void ExposeFunction(const std::string& name,
                       std::function<nlohmann::json(const nlohmann::json&)> fn,
                       const std::string& description,
                       const std::vector<std::string>& paramNames = {});

    /**
     * @brief Remove an exposed function
     */
    void RemoveFunction(const std::string& name);

    /**
     * @brief Check if function exists
     */
    bool HasFunction(const std::string& name) const;

    /**
     * @brief Call an exposed function
     */
    nlohmann::json CallFunction(const std::string& name, const nlohmann::json& args);

    // Property Exposure

    /**
     * @brief Expose a C++ property with getter and optional setter
     * @param name Property name
     * @param getter Function to get property value
     * @param setter Function to set property value (nullptr for read-only)
     */
    void ExposeProperty(const std::string& name,
                       std::function<nlohmann::json()> getter,
                       std::function<void(const nlohmann::json&)> setter = nullptr);

    /**
     * @brief Remove an exposed property
     */
    void RemoveProperty(const std::string& name);

    /**
     * @brief Get property value
     */
    nlohmann::json GetProperty(const std::string& name);

    /**
     * @brief Set property value
     */
    bool SetProperty(const std::string& name, const nlohmann::json& value);

    // JavaScript Calls

    /**
     * @brief Call a JavaScript function from C++
     * @param function Function name (can include path like "UI.updateHealth")
     * @param args Arguments as JSON
     * @return Result as JSON
     */
    nlohmann::json CallJS(const std::string& function, const nlohmann::json& args = {});

    /**
     * @brief Call JavaScript function asynchronously
     * @param function Function name
     * @param args Arguments
     * @param callback Callback when complete
     */
    void CallJSAsync(const std::string& function, const nlohmann::json& args,
                    std::function<void(const JSResult&)> callback);

    /**
     * @brief Execute raw JavaScript code
     * @param code JavaScript code
     * @return Execution result
     */
    JSResult ExecuteJS(const std::string& code);

    /**
     * @brief Execute JavaScript code asynchronously
     */
    void ExecuteJSAsync(const std::string& code,
                       std::function<void(const JSResult&)> callback);

    // Event System

    /**
     * @brief Subscribe to a UI event
     * @param event Event name
     * @param handler Event handler
     * @return Subscription ID for unsubscribing
     */
    int OnUIEvent(const std::string& event,
                 std::function<void(const nlohmann::json&)> handler);

    /**
     * @brief Subscribe to UI event (one-time)
     */
    int OnceUIEvent(const std::string& event,
                   std::function<void(const nlohmann::json&)> handler);

    /**
     * @brief Unsubscribe from UI event
     */
    void OffUIEvent(int subscriptionId);

    /**
     * @brief Unsubscribe all handlers for an event
     */
    void OffUIEvent(const std::string& event);

    /**
     * @brief Emit an event to JavaScript
     * @param event Event name
     * @param data Event data
     */
    void EmitEvent(const std::string& event, const nlohmann::json& data = {});

    /**
     * @brief Handle event from JavaScript (called by JS bridge)
     */
    void HandleJSEvent(const std::string& event, const nlohmann::json& data);

    // Binding Contexts

    /**
     * @brief Create a binding context for namespaced bindings
     * @param name Context name (e.g., "Game", "Player")
     * @return Binding context
     */
    BindingContext* CreateContext(const std::string& name);

    /**
     * @brief Get existing binding context
     */
    BindingContext* GetContext(const std::string& name);

    /**
     * @brief Remove binding context
     */
    void RemoveContext(const std::string& name);

    // Type Conversion Helpers

    /**
     * @brief Convert C++ value to JSON
     */
    template<typename T>
    static nlohmann::json ToJSON(const T& value) {
        return nlohmann::json(value);
    }

    /**
     * @brief Convert JSON to C++ value
     */
    template<typename T>
    static T FromJSON(const nlohmann::json& json) {
        return json.get<T>();
    }

    // Batch Operations

    /**
     * @brief Begin batch update (delays event propagation)
     */
    void BeginBatch();

    /**
     * @brief End batch update and propagate events
     */
    void EndBatch();

    /**
     * @brief Check if in batch mode
     */
    bool IsInBatch() const { return m_batchDepth > 0; }

    // Debug

    /**
     * @brief Get all exposed function names
     */
    std::vector<std::string> GetExposedFunctions() const;

    /**
     * @brief Get all exposed property names
     */
    std::vector<std::string> GetExposedProperties() const;

    /**
     * @brief Get binding documentation as JSON
     */
    nlohmann::json GetDocumentation() const;

private:
    void ProcessPendingCalls();
    void FlushBatchedEvents();

    std::unordered_map<std::string, FunctionBinding> m_functions;
    std::unordered_map<std::string, PropertyBinding> m_properties;
    std::unordered_map<std::string, std::unique_ptr<BindingContext>> m_contexts;

    std::unordered_map<std::string, std::vector<EventSubscription>> m_eventHandlers;
    int m_nextSubscriptionId = 1;

    std::vector<PendingJSCall> m_pendingCalls;
    int m_nextCallId = 1;

    int m_batchDepth = 0;
    std::vector<std::pair<std::string, nlohmann::json>> m_batchedEvents;

    mutable std::mutex m_mutex;
    bool m_initialized = false;
};

// Macro helpers for easy binding
#define BIND_FUNCTION(binding, name, func) \
    binding.ExposeFunction(name, [this](const nlohmann::json& args) -> nlohmann::json { \
        return func(args); \
    })

#define BIND_METHOD(binding, name, obj, method) \
    binding.ExposeFunction(name, [obj](const nlohmann::json& args) -> nlohmann::json { \
        return obj->method(args); \
    })

#define BIND_PROPERTY(binding, name, getter, setter) \
    binding.ExposeProperty(name, \
        [this]() -> nlohmann::json { return getter(); }, \
        [this](const nlohmann::json& v) { setter(v); })

#define BIND_READONLY_PROPERTY(binding, name, getter) \
    binding.ExposeProperty(name, \
        [this]() -> nlohmann::json { return getter(); }, \
        nullptr)

} // namespace UI
} // namespace Engine
