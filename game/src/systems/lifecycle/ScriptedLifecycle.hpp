#pragma once

#include "ILifecycle.hpp"
#include "GameEvent.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Script Context
// ============================================================================

/**
 * @brief Context data passed to Python scripts
 *
 * Contains entity state and helper functions for scripts to access.
 */
struct ScriptContext {
    // Entity information
    LifecycleHandle handle;
    std::string entityType;
    std::string entityId;

    // Transform
    struct {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
        float scaleX = 1.0f, scaleY = 1.0f, scaleZ = 1.0f;
    } transform;

    // Health
    struct {
        float current = 100.0f;
        float max = 100.0f;
        float armor = 0.0f;
    } health;

    // Event data (when handling events)
    struct {
        std::string type;
        float damage = 0.0f;
        std::string source;
        std::string target;
        std::string customData; // JSON string
    } event;

    // Timing
    float deltaTime = 0.0f;
    double totalTime = 0.0;
    uint64_t frameCount = 0;

    // Custom properties (JSON string for flexibility)
    std::string properties;
};

// ============================================================================
// Script Error
// ============================================================================

/**
 * @brief Script execution error information
 */
struct ScriptError {
    std::string scriptPath;
    std::string functionName;
    std::string errorMessage;
    int lineNumber = -1;
    double timestamp = 0.0;

    [[nodiscard]] std::string ToString() const;
};

// ============================================================================
// Script Binding Configuration
// ============================================================================

/**
 * @brief Configuration for script bindings
 */
struct ScriptBindingConfig {
    std::string onCreate;       // Script path for on_create
    std::string onTick;         // Script path for on_tick
    std::string onDestroy;      // Script path for on_destroy

    // Event handlers: event name -> script path
    std::unordered_map<std::string, std::string> eventHandlers;

    // Whether to continue on script errors
    bool continueOnError = true;

    // Maximum execution time per script call (ms)
    uint32_t timeoutMs = 100;
};

// ============================================================================
// IPythonBridge Interface
// ============================================================================

/**
 * @brief Interface for Python interpreter bridge
 *
 * Implement this to provide actual Python integration.
 * Default implementation is a no-op stub.
 */
class IPythonBridge {
public:
    virtual ~IPythonBridge() = default;

    /**
     * @brief Initialize the Python interpreter
     */
    virtual bool Initialize() = 0;

    /**
     * @brief Shutdown the Python interpreter
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Check if Python is available
     */
    [[nodiscard]] virtual bool IsAvailable() const = 0;

    /**
     * @brief Load a Python script file
     *
     * @param scriptPath Path to the .py file
     * @return true if loaded successfully
     */
    virtual bool LoadScript(const std::string& scriptPath) = 0;

    /**
     * @brief Unload a Python script
     */
    virtual void UnloadScript(const std::string& scriptPath) = 0;

    /**
     * @brief Call a function in a script
     *
     * @param scriptPath Path to the script
     * @param functionName Function to call
     * @param context Context data to pass
     * @param outError Error information if call fails
     * @return true if call succeeded
     */
    virtual bool CallFunction(const std::string& scriptPath,
                             const std::string& functionName,
                             const ScriptContext& context,
                             ScriptError* outError = nullptr) = 0;

    /**
     * @brief Check if a function exists in a script
     */
    [[nodiscard]] virtual bool HasFunction(const std::string& scriptPath,
                                           const std::string& functionName) const = 0;

    /**
     * @brief Get last error
     */
    [[nodiscard]] virtual ScriptError GetLastError() const = 0;

    /**
     * @brief Set script search paths
     */
    virtual void SetSearchPaths(const std::vector<std::string>& paths) = 0;
};

// ============================================================================
// StubPythonBridge - No-op implementation
// ============================================================================

/**
 * @brief Stub Python bridge when Python is not available
 */
class StubPythonBridge : public IPythonBridge {
public:
    bool Initialize() override { return true; }
    void Shutdown() override {}
    [[nodiscard]] bool IsAvailable() const override { return false; }
    bool LoadScript(const std::string&) override { return false; }
    void UnloadScript(const std::string&) override {}
    bool CallFunction(const std::string&, const std::string&,
                     const ScriptContext&, ScriptError* outError) override {
        if (outError) {
            outError->errorMessage = "Python not available";
        }
        return false;
    }
    [[nodiscard]] bool HasFunction(const std::string&, const std::string&) const override {
        return false;
    }
    [[nodiscard]] ScriptError GetLastError() const override { return {}; }
    void SetSearchPaths(const std::vector<std::string>&) override {}
};

// ============================================================================
// ScriptedLifecycle
// ============================================================================

/**
 * @brief Lifecycle object with Python script integration
 *
 * Calls Python scripts for lifecycle events:
 * - on_create(context): Called when object is created
 * - on_tick(context): Called each frame
 * - on_event(context): Called for game events
 * - on_destroy(context): Called when object is destroyed
 *
 * Scripts receive a context dictionary with entity state and can
 * modify it to affect the game object.
 */
class ScriptedLifecycle : public LifecycleBase {
public:
    ScriptedLifecycle();
    ~ScriptedLifecycle() override;

    // Disable copy, allow move
    ScriptedLifecycle(const ScriptedLifecycle&) = delete;
    ScriptedLifecycle& operator=(const ScriptedLifecycle&) = delete;
    ScriptedLifecycle(ScriptedLifecycle&&) noexcept = default;
    ScriptedLifecycle& operator=(ScriptedLifecycle&&) noexcept = default;

    // =========================================================================
    // ILifecycle Implementation
    // =========================================================================

    void OnCreate(const nlohmann::json& config) override;
    void OnTick(float deltaTime) override;
    bool OnEvent(const GameEvent& event) override;
    void OnDestroy() override;

    [[nodiscard]] const char* GetTypeName() const override { return "ScriptedLifecycle"; }

    // =========================================================================
    // Script Configuration
    // =========================================================================

    /**
     * @brief Set script bindings
     */
    void SetScriptBindings(const ScriptBindingConfig& config);

    /**
     * @brief Get current script bindings
     */
    [[nodiscard]] const ScriptBindingConfig& GetScriptBindings() const {
        return m_scriptConfig;
    }

    /**
     * @brief Set individual script path
     */
    void SetOnCreateScript(const std::string& path);
    void SetOnTickScript(const std::string& path);
    void SetOnDestroyScript(const std::string& path);
    void SetEventScript(const std::string& eventType, const std::string& path);

    /**
     * @brief Remove script binding
     */
    void RemoveEventScript(const std::string& eventType);

    // =========================================================================
    // Context Access
    // =========================================================================

    /**
     * @brief Get current script context
     *
     * Derived classes can override to add custom data.
     */
    [[nodiscard]] virtual ScriptContext BuildContext() const;

    /**
     * @brief Apply context changes back to object
     */
    virtual void ApplyContext(const ScriptContext& context);

    /**
     * @brief Set custom property (accessible from scripts)
     */
    void SetProperty(const std::string& key, const std::string& value);

    /**
     * @brief Get custom property
     */
    [[nodiscard]] std::string GetProperty(const std::string& key) const;

    // =========================================================================
    // Error Handling
    // =========================================================================

    /**
     * @brief Get recent script errors
     */
    [[nodiscard]] const std::vector<ScriptError>& GetErrors() const { return m_errors; }

    /**
     * @brief Clear error log
     */
    void ClearErrors() { m_errors.clear(); }

    /**
     * @brief Set error callback
     */
    using ErrorCallback = std::function<void(const ScriptError&)>;
    void SetOnError(ErrorCallback callback) { m_onError = std::move(callback); }

    // =========================================================================
    // Static Configuration
    // =========================================================================

    /**
     * @brief Set the Python bridge implementation
     */
    static void SetPythonBridge(std::shared_ptr<IPythonBridge> bridge);

    /**
     * @brief Get the Python bridge
     */
    static IPythonBridge& GetPythonBridge();

    /**
     * @brief Set default script search paths
     */
    static void SetDefaultSearchPaths(const std::vector<std::string>& paths);

protected:
    /**
     * @brief Call a script function
     */
    bool CallScript(const std::string& scriptPath, const std::string& function);

    /**
     * @brief Log an error
     */
    void LogError(const ScriptError& error);

    ScriptBindingConfig m_scriptConfig;
    std::unordered_map<std::string, std::string> m_properties;
    std::vector<ScriptError> m_errors;
    ErrorCallback m_onError;

    // Cached context for performance
    mutable ScriptContext m_cachedContext;
    mutable bool m_contextDirty = true;

    // Static Python bridge
    static std::shared_ptr<IPythonBridge> s_pythonBridge;
    static std::vector<std::string> s_defaultSearchPaths;
};

// ============================================================================
// Script Manager
// ============================================================================

/**
 * @brief Manages script loading and caching
 */
class ScriptManager {
public:
    static ScriptManager& Instance();

    /**
     * @brief Pre-load scripts for better runtime performance
     */
    void PreloadScripts(const std::vector<std::string>& scriptPaths);

    /**
     * @brief Unload all cached scripts
     */
    void UnloadAll();

    /**
     * @brief Reload a specific script
     */
    bool ReloadScript(const std::string& scriptPath);

    /**
     * @brief Check if script is loaded
     */
    [[nodiscard]] bool IsLoaded(const std::string& scriptPath) const;

    /**
     * @brief Enable hot reload monitoring
     */
    void SetHotReloadEnabled(bool enabled);

    /**
     * @brief Check for script file changes and reload
     */
    size_t CheckForReloads();

    /**
     * @brief Get statistics
     */
    struct Stats {
        size_t scriptsLoaded = 0;
        size_t totalCalls = 0;
        size_t failedCalls = 0;
        double totalExecutionTime = 0.0;
    };
    [[nodiscard]] Stats GetStats() const;

private:
    ScriptManager() = default;

    std::unordered_set<std::string> m_loadedScripts;
    std::unordered_map<std::string, int64_t> m_scriptModTimes;
    bool m_hotReloadEnabled = false;
    mutable Stats m_stats;
    mutable std::mutex m_mutex;
};

} // namespace Lifecycle
} // namespace Vehement
