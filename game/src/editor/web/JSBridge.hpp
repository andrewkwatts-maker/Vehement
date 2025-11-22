#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>
#include <optional>
#include <queue>
#include <mutex>
#include <atomic>
#include <any>

namespace Vehement {
namespace WebEditor {

/**
 * @brief JSON value wrapper for C++ <-> JS communication
 */
class JSValue {
public:
    using Null = std::monostate;
    using Bool = bool;
    using Number = double;
    using String = std::string;
    using Array = std::vector<JSValue>;
    using Object = std::unordered_map<std::string, JSValue>;

    using ValueType = std::variant<Null, Bool, Number, String, Array, Object>;

    JSValue() : m_value(Null{}) {}
    JSValue(std::nullptr_t) : m_value(Null{}) {}
    JSValue(bool b) : m_value(b) {}
    JSValue(int n) : m_value(static_cast<double>(n)) {}
    JSValue(double n) : m_value(n) {}
    JSValue(const char* s) : m_value(std::string(s)) {}
    JSValue(const std::string& s) : m_value(s) {}
    JSValue(std::string&& s) : m_value(std::move(s)) {}
    JSValue(const Array& arr) : m_value(arr) {}
    JSValue(Array&& arr) : m_value(std::move(arr)) {}
    JSValue(const Object& obj) : m_value(obj) {}
    JSValue(Object&& obj) : m_value(std::move(obj)) {}

    // Type checks
    [[nodiscard]] bool IsNull() const { return std::holds_alternative<Null>(m_value); }
    [[nodiscard]] bool IsBool() const { return std::holds_alternative<Bool>(m_value); }
    [[nodiscard]] bool IsNumber() const { return std::holds_alternative<Number>(m_value); }
    [[nodiscard]] bool IsString() const { return std::holds_alternative<String>(m_value); }
    [[nodiscard]] bool IsArray() const { return std::holds_alternative<Array>(m_value); }
    [[nodiscard]] bool IsObject() const { return std::holds_alternative<Object>(m_value); }

    // Getters
    [[nodiscard]] bool AsBool() const { return std::get<Bool>(m_value); }
    [[nodiscard]] double AsNumber() const { return std::get<Number>(m_value); }
    [[nodiscard]] int AsInt() const { return static_cast<int>(std::get<Number>(m_value)); }
    [[nodiscard]] const std::string& AsString() const { return std::get<String>(m_value); }
    [[nodiscard]] const Array& AsArray() const { return std::get<Array>(m_value); }
    [[nodiscard]] const Object& AsObject() const { return std::get<Object>(m_value); }
    [[nodiscard]] Array& AsArray() { return std::get<Array>(m_value); }
    [[nodiscard]] Object& AsObject() { return std::get<Object>(m_value); }

    // Safe getters with defaults
    [[nodiscard]] bool GetBool(bool defaultVal = false) const {
        return IsBool() ? AsBool() : defaultVal;
    }
    [[nodiscard]] double GetNumber(double defaultVal = 0.0) const {
        return IsNumber() ? AsNumber() : defaultVal;
    }
    [[nodiscard]] int GetInt(int defaultVal = 0) const {
        return IsNumber() ? AsInt() : defaultVal;
    }
    [[nodiscard]] std::string GetString(const std::string& defaultVal = "") const {
        return IsString() ? AsString() : defaultVal;
    }

    // Object property access
    [[nodiscard]] bool HasProperty(const std::string& key) const {
        if (!IsObject()) return false;
        const auto& obj = AsObject();
        return obj.find(key) != obj.end();
    }

    [[nodiscard]] const JSValue& operator[](const std::string& key) const {
        static JSValue null;
        if (!IsObject()) return null;
        const auto& obj = AsObject();
        auto it = obj.find(key);
        return it != obj.end() ? it->second : null;
    }

    [[nodiscard]] JSValue& operator[](const std::string& key) {
        if (!IsObject()) {
            m_value = Object{};
        }
        return std::get<Object>(m_value)[key];
    }

    // Array element access
    [[nodiscard]] const JSValue& operator[](size_t index) const {
        static JSValue null;
        if (!IsArray()) return null;
        const auto& arr = AsArray();
        return index < arr.size() ? arr[index] : null;
    }

    [[nodiscard]] size_t Size() const {
        if (IsArray()) return AsArray().size();
        if (IsObject()) return AsObject().size();
        return 0;
    }

    // JSON serialization
    [[nodiscard]] std::string ToJson() const;
    static JSValue FromJson(const std::string& json);

private:
    ValueType m_value;
};

/**
 * @brief Result of a JavaScript function call
 */
struct JSResult {
    bool success = true;
    JSValue value;
    std::string error;

    static JSResult Success(JSValue val = {}) {
        return {true, std::move(val), ""};
    }

    static JSResult Error(const std::string& err) {
        return {false, {}, err};
    }
};

/**
 * @brief Async callback for JavaScript calls
 */
using JSCallback = std::function<void(const JSResult&)>;

/**
 * @brief Native function callable from JavaScript
 */
using NativeFunction = std::function<JSResult(const std::vector<JSValue>&)>;

/**
 * @brief JavaScript Bridge
 *
 * Provides two-way communication between C++ and JavaScript:
 * - Expose C++ functions to JavaScript
 * - Call JavaScript functions from C++
 * - Async message passing with callbacks
 * - JSON serialization for data exchange
 *
 * Usage:
 * @code
 * // Register C++ function
 * bridge.RegisterFunction("getPlayerHealth", [](const auto& args) {
 *     return JSResult::Success(JSValue(player->health));
 * });
 *
 * // Call from JavaScript:
 * // WebEditor.invoke("getPlayerHealth", [], (err, result) => console.log(result));
 *
 * // Call JavaScript from C++
 * bridge.CallJS("updateUI", {JSValue("newData")}, [](const JSResult& result) {
 *     // Handle result
 * });
 * @endcode
 */
class JSBridge {
public:
    JSBridge();
    ~JSBridge();

    // Non-copyable
    JSBridge(const JSBridge&) = delete;
    JSBridge& operator=(const JSBridge&) = delete;

    // =========================================================================
    // Function Registration (C++ -> JS exposure)
    // =========================================================================

    /**
     * @brief Register a native function callable from JavaScript
     * @param name Function name
     * @param func Function implementation
     */
    void RegisterFunction(const std::string& name, NativeFunction func);

    /**
     * @brief Unregister a function
     * @param name Function name
     */
    void UnregisterFunction(const std::string& name);

    /**
     * @brief Check if a function is registered
     */
    [[nodiscard]] bool HasFunction(const std::string& name) const;

    /**
     * @brief Get all registered function names
     */
    [[nodiscard]] std::vector<std::string> GetRegisteredFunctions() const;

    /**
     * @brief Register multiple functions at once using a builder pattern
     */
    class FunctionBuilder {
    public:
        explicit FunctionBuilder(JSBridge& bridge) : m_bridge(bridge) {}

        FunctionBuilder& Add(const std::string& name, NativeFunction func) {
            m_bridge.RegisterFunction(name, std::move(func));
            return *this;
        }

        template<typename T>
        FunctionBuilder& AddGetter(const std::string& name, T* valuePtr) {
            m_bridge.RegisterFunction(name, [valuePtr](const auto&) {
                return JSResult::Success(JSValue(*valuePtr));
            });
            return *this;
        }

        template<typename T>
        FunctionBuilder& AddSetter(const std::string& name, T* valuePtr) {
            m_bridge.RegisterFunction(name, [valuePtr](const auto& args) {
                if (args.empty()) return JSResult::Error("Missing argument");
                // Type-specific assignment would go here
                return JSResult::Success();
            });
            return *this;
        }

    private:
        JSBridge& m_bridge;
    };

    FunctionBuilder Functions() { return FunctionBuilder(*this); }

    // =========================================================================
    // JavaScript Invocation (C++ -> JS calls)
    // =========================================================================

    /**
     * @brief Call a JavaScript function asynchronously
     * @param functionName Name of the function to call
     * @param args Arguments to pass
     * @param callback Callback for result (optional)
     */
    void CallJS(const std::string& functionName,
                const std::vector<JSValue>& args = {},
                JSCallback callback = nullptr);

    /**
     * @brief Execute raw JavaScript code
     * @param script JavaScript code
     * @param callback Callback for result (optional)
     */
    void ExecuteScript(const std::string& script, JSCallback callback = nullptr);

    /**
     * @brief Evaluate JavaScript expression and get result
     * @param expression JavaScript expression
     * @param callback Callback for result
     */
    void Evaluate(const std::string& expression, JSCallback callback);

    // =========================================================================
    // Message Passing
    // =========================================================================

    /**
     * @brief Send a message to JavaScript
     * @param type Message type/channel
     * @param data Message data
     */
    void SendMessage(const std::string& type, const JSValue& data);

    /**
     * @brief Subscribe to messages from JavaScript
     * @param type Message type/channel
     * @param handler Handler function
     */
    void OnMessage(const std::string& type,
                   std::function<void(const JSValue&)> handler);

    /**
     * @brief Unsubscribe from messages
     * @param type Message type/channel
     */
    void OffMessage(const std::string& type);

    // =========================================================================
    // Event System
    // =========================================================================

    /**
     * @brief Emit an event to JavaScript
     * @param eventName Event name
     * @param data Event data
     */
    void EmitEvent(const std::string& eventName, const JSValue& data = {});

    /**
     * @brief Subscribe to events from JavaScript
     */
    void OnEvent(const std::string& eventName,
                 std::function<void(const JSValue&)> handler);

    // =========================================================================
    // Processing (call from main loop)
    // =========================================================================

    /**
     * @brief Process pending callbacks and messages
     * Must be called from the main thread/loop
     */
    void ProcessPending();

    /**
     * @brief Handle incoming message from JavaScript
     * Called by WebView when receiving messages
     * @param jsonMessage JSON-encoded message
     */
    void HandleIncomingMessage(const std::string& jsonMessage);

    // =========================================================================
    // Binding Helpers
    // =========================================================================

    /**
     * @brief Bind a C++ object's methods to JavaScript
     * Uses reflection-like registration
     */
    template<typename T>
    class ObjectBinding {
    public:
        ObjectBinding(JSBridge& bridge, const std::string& prefix, T* obj)
            : m_bridge(bridge), m_prefix(prefix), m_object(obj) {}

        template<typename R, typename... Args>
        ObjectBinding& Method(const std::string& name, R(T::*method)(Args...)) {
            std::string fullName = m_prefix + "." + name;
            m_bridge.RegisterFunction(fullName, [this, method](const std::vector<JSValue>& args) {
                // Method binding implementation would go here
                return JSResult::Success();
            });
            return *this;
        }

        template<typename V>
        ObjectBinding& Property(const std::string& name, V T::*member) {
            std::string getName = m_prefix + ".get" + name;
            std::string setName = m_prefix + ".set" + name;

            m_bridge.RegisterFunction(getName, [this, member](const auto&) {
                return JSResult::Success(JSValue(m_object->*member));
            });

            return *this;
        }

    private:
        JSBridge& m_bridge;
        std::string m_prefix;
        T* m_object;
    };

    template<typename T>
    ObjectBinding<T> BindObject(const std::string& name, T* obj) {
        return ObjectBinding<T>(*this, name, obj);
    }

    // =========================================================================
    // WebView Integration
    // =========================================================================

    /**
     * @brief Set the script executor (connection to WebView)
     * @param executor Function that executes JavaScript in the webview
     */
    using ScriptExecutor = std::function<void(const std::string& script, JSCallback callback)>;
    void SetScriptExecutor(ScriptExecutor executor) { m_scriptExecutor = executor; }

    /**
     * @brief Generate JavaScript code to initialize the bridge on the JS side
     */
    [[nodiscard]] std::string GenerateBridgeScript() const;

private:
    // Registered native functions
    std::unordered_map<std::string, NativeFunction> m_functions;

    // Message handlers
    std::unordered_map<std::string, std::function<void(const JSValue&)>> m_messageHandlers;

    // Event handlers
    std::unordered_map<std::string, std::vector<std::function<void(const JSValue&)>>> m_eventHandlers;

    // Pending callbacks
    struct PendingCallback {
        uint64_t id;
        JSCallback callback;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<uint64_t, PendingCallback> m_pendingCallbacks;
    std::atomic<uint64_t> m_nextCallbackId{1};

    // Thread-safe message queue
    std::queue<std::pair<std::string, JSValue>> m_incomingMessages;
    std::mutex m_messageMutex;

    // Script executor
    ScriptExecutor m_scriptExecutor;

    // Internal helpers
    JSResult InvokeNativeFunction(const std::string& name, const std::vector<JSValue>& args);
    void DeliverResult(uint64_t callbackId, const JSResult& result);
};

/**
 * @brief JSON serialization helpers
 */
namespace JSON {

/**
 * @brief Parse JSON string to JSValue
 */
JSValue Parse(const std::string& json);

/**
 * @brief Stringify JSValue to JSON
 */
std::string Stringify(const JSValue& value, bool pretty = false);

/**
 * @brief Escape string for JSON
 */
std::string EscapeString(const std::string& str);

/**
 * @brief Unescape JSON string
 */
std::string UnescapeString(const std::string& str);

} // namespace JSON

/**
 * @brief Global bridge instance accessor
 */
JSBridge& GetGlobalBridge();

} // namespace WebEditor
} // namespace Vehement
