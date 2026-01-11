#pragma once

#include "EventCondition.hpp"
#include <string>
#include <functional>
#include <optional>
#include <chrono>
#include <atomic>

namespace Nova {
namespace Events {

/**
 * @brief Callback type for event bindings
 */
enum class CallbackType {
    Python,     // Python script or function
    Native,     // C++ function callback
    Event,      // Emit another event
    Command,    // Execute a game command
    Script      // Execute a script file
};

/**
 * @brief Convert callback type to string
 */
[[nodiscard]] const char* CallbackTypeToString(CallbackType type);
[[nodiscard]] std::optional<CallbackType> CallbackTypeFromString(const std::string& str);

/**
 * @brief Binding between an event condition and a callback action
 *
 * Represents a complete event binding that:
 * - Watches for events matching a condition
 * - Executes a callback (Python, native, or event emission)
 * - Supports priorities and enable/disable
 */
struct EventBinding {
    // Unique identifier
    std::string id;
    std::string name;
    std::string description;
    std::string category;

    // Condition that triggers this binding
    EventCondition condition;

    // Callback configuration
    CallbackType callbackType = CallbackType::Python;

    // Python callback settings
    std::string pythonScript;       // Inline Python code
    std::string pythonFile;         // Python file path
    std::string pythonModule;       // Module name
    std::string pythonFunction;     // Function name to call

    // Event emission settings (for CallbackType::Event)
    std::string emitEventType;      // Event type to emit
    std::unordered_map<std::string, std::any> emitEventData;

    // Command execution (for CallbackType::Command)
    std::string command;
    std::vector<std::string> commandArgs;

    // Native callback (set programmatically)
    std::function<void(const EventCondition&, const std::unordered_map<std::string, std::any>&)> nativeCallback;

    // Additional parameters passed to callback
    json parameters;

    // Execution settings
    bool enabled = true;
    int priority = 0;               // Higher priority executes first
    bool async = false;             // Execute asynchronously
    float delay = 0.0f;             // Delay before execution (seconds)
    float cooldown = 0.0f;          // Minimum time between executions
    int maxExecutions = -1;         // Max executions (-1 = unlimited)
    bool oneShot = false;           // Disable after first execution

    // Debugging
    bool logExecution = false;      // Log when this binding executes
    bool breakOnExecute = false;    // Break into debugger (editor only)

    // Metadata
    std::string author;
    std::string version;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;

    // Runtime state
    std::atomic<int> executionCount{0};
    std::chrono::system_clock::time_point lastExecutionTime;
    std::string lastError;
    bool hasError = false;

    // =========================================================================
    // Constructors
    // =========================================================================

    EventBinding() : createdAt(std::chrono::system_clock::now()),
                     modifiedAt(std::chrono::system_clock::now()) {}

    EventBinding(const std::string& bindingId, const EventCondition& cond)
        : id(bindingId), condition(cond),
          createdAt(std::chrono::system_clock::now()),
          modifiedAt(std::chrono::system_clock::now()) {}

    // =========================================================================
    // Builder Pattern
    // =========================================================================

    EventBinding& WithId(const std::string& bindingId) {
        id = bindingId;
        return *this;
    }

    EventBinding& WithName(const std::string& bindingName) {
        name = bindingName;
        return *this;
    }

    EventBinding& WithDescription(const std::string& desc) {
        description = desc;
        return *this;
    }

    EventBinding& WithCategory(const std::string& cat) {
        category = cat;
        return *this;
    }

    EventBinding& WithCondition(EventCondition cond) {
        condition = std::move(cond);
        return *this;
    }

    EventBinding& WithPythonScript(const std::string& script) {
        callbackType = CallbackType::Python;
        pythonScript = script;
        return *this;
    }

    EventBinding& WithPythonFile(const std::string& file) {
        callbackType = CallbackType::Python;
        pythonFile = file;
        return *this;
    }

    EventBinding& WithPythonFunction(const std::string& module, const std::string& func) {
        callbackType = CallbackType::Python;
        pythonModule = module;
        pythonFunction = func;
        return *this;
    }

    EventBinding& WithEventEmission(const std::string& eventType,
                                    const std::unordered_map<std::string, std::any>& data = {}) {
        callbackType = CallbackType::Event;
        emitEventType = eventType;
        emitEventData = data;
        return *this;
    }

    EventBinding& WithCommand(const std::string& cmd, const std::vector<std::string>& args = {}) {
        callbackType = CallbackType::Command;
        command = cmd;
        commandArgs = args;
        return *this;
    }

    EventBinding& WithNativeCallback(
        std::function<void(const EventCondition&, const std::unordered_map<std::string, std::any>&)> callback) {
        callbackType = CallbackType::Native;
        nativeCallback = std::move(callback);
        return *this;
    }

    EventBinding& WithParameters(json params) {
        parameters = std::move(params);
        return *this;
    }

    EventBinding& WithPriority(int prio) {
        priority = prio;
        return *this;
    }

    EventBinding& AsAsync() {
        async = true;
        return *this;
    }

    EventBinding& WithDelay(float delaySeconds) {
        delay = delaySeconds;
        return *this;
    }

    EventBinding& WithCooldown(float cooldownSeconds) {
        cooldown = cooldownSeconds;
        return *this;
    }

    EventBinding& WithMaxExecutions(int max) {
        maxExecutions = max;
        return *this;
    }

    EventBinding& AsOneShot() {
        oneShot = true;
        maxExecutions = 1;
        return *this;
    }

    EventBinding& WithLogging() {
        logExecution = true;
        return *this;
    }

    EventBinding& WithTag(const std::string& tag) {
        tags.push_back(tag);
        return *this;
    }

    EventBinding& Disabled() {
        enabled = false;
        return *this;
    }

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Check if binding can execute (enabled, not on cooldown, etc.)
     */
    [[nodiscard]] bool CanExecute() const {
        if (!enabled) return false;
        if (maxExecutions > 0 && executionCount >= maxExecutions) return false;

        if (cooldown > 0.0f) {
            auto now = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration<float>(now - lastExecutionTime).count();
            if (elapsed < cooldown) return false;
        }

        return true;
    }

    /**
     * @brief Record an execution
     */
    void RecordExecution() {
        executionCount++;
        lastExecutionTime = std::chrono::system_clock::now();
        hasError = false;
        lastError.clear();

        if (oneShot) {
            enabled = false;
        }
    }

    /**
     * @brief Record an error
     */
    void RecordError(const std::string& error) {
        hasError = true;
        lastError = error;
    }

    /**
     * @brief Reset execution state
     */
    void Reset() {
        executionCount = 0;
        lastExecutionTime = std::chrono::system_clock::time_point{};
        hasError = false;
        lastError.clear();
        if (oneShot) {
            enabled = true;
        }
    }

    /**
     * @brief Check if this binding uses Python
     */
    [[nodiscard]] bool UsesPython() const {
        return callbackType == CallbackType::Python &&
               (!pythonScript.empty() || !pythonFile.empty() || !pythonFunction.empty());
    }

    /**
     * @brief Get a display string for the binding
     */
    [[nodiscard]] std::string GetDisplayName() const {
        if (!name.empty()) return name;
        if (!id.empty()) return id;
        return condition.eventName + " binding";
    }

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static EventBinding FromJson(const json& j);

    /**
     * @brief Validate the binding configuration
     * @return Error message if invalid, empty if valid
     */
    [[nodiscard]] std::string Validate() const;
};

/**
 * @brief Collection of related event bindings
 */
struct BindingGroup {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    bool enabled = true;
    std::vector<EventBinding> bindings;

    /**
     * @brief Serialize to JSON
     */
    [[nodiscard]] json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static BindingGroup FromJson(const json& j);
};

} // namespace Events
} // namespace Nova
