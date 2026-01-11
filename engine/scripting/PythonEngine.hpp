#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <optional>
#include <any>
#include <variant>

// Forward declare pybind11 types to avoid header pollution
namespace pybind11 {
    class object;
    class module_;
    class dict;
    class gil_scoped_acquire;
    class gil_scoped_release;
}

namespace Nova {
namespace Scripting {

// Forward declarations
class ScriptContext;
class EventDispatcher;

/**
 * @brief Result of script execution
 */
struct ScriptResult {
    bool success = false;
    std::string errorMessage;
    std::variant<std::monostate, bool, int, float, double, std::string> returnValue;

    operator bool() const { return success; }

    template<typename T>
    std::optional<T> GetValue() const {
        if (auto* val = std::get_if<T>(&returnValue)) {
            return *val;
        }
        return std::nullopt;
    }
};

/**
 * @brief Cached script information
 */
struct CachedScript {
    std::string path;
    std::string source;
    std::chrono::system_clock::time_point loadTime;
    std::chrono::system_clock::time_point fileModTime;
    bool isValid = false;
    std::shared_ptr<pybind11::object> compiledCode;
};

/**
 * @brief Configuration for Python engine initialization
 */
struct PythonEngineConfig {
    std::vector<std::string> scriptPaths;          // Paths to search for scripts
    std::string mainModuleName = "nova_game";      // Main module name
    bool enableHotReload = true;                   // Enable script hot-reloading
    float hotReloadCheckInterval = 1.0f;           // Seconds between hot-reload checks
    bool enableSandbox = true;                     // Enable sandbox restrictions
    size_t maxExecutionTimeMs = 100;               // Max script execution time
    size_t maxMemoryMB = 256;                      // Max memory for scripts
    bool verboseErrors = true;                     // Detailed error messages
};

/**
 * @brief Performance metrics for script execution
 */
struct ScriptMetrics {
    size_t totalExecutions = 0;
    size_t failedExecutions = 0;
    double totalExecutionTimeMs = 0.0;
    double avgExecutionTimeMs = 0.0;
    double maxExecutionTimeMs = 0.0;
    size_t hotReloads = 0;
    std::chrono::system_clock::time_point lastExecution;

    void RecordExecution(double timeMs, bool success);
    void Reset();
};

/**
 * @brief Core Python interpreter wrapper for Nova3D/Vehement2
 *
 * Provides:
 * - Python interpreter initialization and shutdown
 * - Script execution from files or strings
 * - Function calling with arguments and return values
 * - Exception handling and error reporting
 * - Script caching and hot-reload support
 * - Thread safety for multi-threaded game loop
 *
 * Usage:
 * @code
 * auto& engine = PythonEngine::Instance();
 * engine.Initialize({.scriptPaths = {"scripts/"}});
 *
 * // Execute script file
 * auto result = engine.ExecuteFile("ai/zombie_ai.py");
 *
 * // Call Python function
 * auto result = engine.CallFunction("zombie_ai", "update", 0.016f, entityId);
 *
 * engine.Shutdown();
 * @endcode
 */
class PythonEngine {
public:
    // Singleton access
    [[nodiscard]] static PythonEngine& Instance();

    // Delete copy/move
    PythonEngine(const PythonEngine&) = delete;
    PythonEngine& operator=(const PythonEngine&) = delete;
    PythonEngine(PythonEngine&&) = delete;
    PythonEngine& operator=(PythonEngine&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the Python interpreter
     * @param config Configuration options
     * @return true if initialization succeeded
     */
    [[nodiscard]] bool Initialize(const PythonEngineConfig& config = {});

    /**
     * @brief Shutdown the Python interpreter and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if engine is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    /**
     * @brief Update engine (call each frame for hot-reload checks)
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    // =========================================================================
    // Script Execution
    // =========================================================================

    /**
     * @brief Execute a Python script from file
     * @param filePath Path to the script file (relative to script paths)
     * @return Result of execution
     */
    [[nodiscard]] ScriptResult ExecuteFile(const std::string& filePath);

    /**
     * @brief Execute Python code from a string
     * @param code Python source code
     * @param name Optional name for error reporting
     * @return Result of execution
     */
    [[nodiscard]] ScriptResult ExecuteString(const std::string& code,
                                             const std::string& name = "<string>");

    /**
     * @brief Import a Python module
     * @param moduleName Module name to import
     * @return true if import succeeded
     */
    [[nodiscard]] bool ImportModule(const std::string& moduleName);

    /**
     * @brief Reload a previously loaded module
     * @param moduleName Module name to reload
     * @return true if reload succeeded
     */
    [[nodiscard]] bool ReloadModule(const std::string& moduleName);

    // =========================================================================
    // Function Calling
    // =========================================================================

    /**
     * @brief Call a Python function with variadic arguments
     * @tparam Args Argument types (must be convertible to Python)
     * @param moduleName Module containing the function
     * @param functionName Name of the function to call
     * @param args Arguments to pass
     * @return Result of the function call
     */
    template<typename... Args>
    [[nodiscard]] ScriptResult CallFunction(const std::string& moduleName,
                                            const std::string& functionName,
                                            Args&&... args);

    /**
     * @brief Call a Python function with vector of arguments
     * @param moduleName Module containing the function
     * @param functionName Name of the function to call
     * @param args Arguments as vector of variants
     * @return Result of the function call
     */
    [[nodiscard]] ScriptResult CallFunctionV(
        const std::string& moduleName,
        const std::string& functionName,
        const std::vector<std::variant<bool, int, float, double, std::string>>& args);

    /**
     * @brief Call a method on a Python object
     * @param objectName Global object name
     * @param methodName Method to call
     * @param args Arguments as vector
     * @return Result of the method call
     */
    [[nodiscard]] ScriptResult CallMethod(
        const std::string& objectName,
        const std::string& methodName,
        const std::vector<std::variant<bool, int, float, double, std::string>>& args = {});

    // =========================================================================
    // Variable Access
    // =========================================================================

    /**
     * @brief Get a global variable from a module
     * @tparam T Expected type
     * @param moduleName Module name
     * @param varName Variable name
     * @return Value if found and convertible
     */
    template<typename T>
    [[nodiscard]] std::optional<T> GetGlobal(const std::string& moduleName,
                                              const std::string& varName);

    /**
     * @brief Set a global variable in a module
     * @tparam T Value type
     * @param moduleName Module name
     * @param varName Variable name
     * @param value Value to set
     * @return true if successful
     */
    template<typename T>
    bool SetGlobal(const std::string& moduleName,
                   const std::string& varName,
                   const T& value);

    // =========================================================================
    // Script Caching and Hot-Reload
    // =========================================================================

    /**
     * @brief Preload and cache a script file
     * @param filePath Script file path
     * @return true if preload succeeded
     */
    [[nodiscard]] bool PreloadScript(const std::string& filePath);

    /**
     * @brief Clear the script cache
     */
    void ClearCache();

    /**
     * @brief Force check for script changes and reload
     */
    void CheckHotReload();

    /**
     * @brief Get list of cached scripts
     */
    [[nodiscard]] std::vector<std::string> GetCachedScripts() const;

    /**
     * @brief Check if a script has been modified since loading
     * @param filePath Script file path
     * @return true if file has been modified
     */
    [[nodiscard]] bool IsScriptModified(const std::string& filePath) const;

    // =========================================================================
    // Error Handling
    // =========================================================================

    /**
     * @brief Get the last error message
     */
    [[nodiscard]] const std::string& GetLastError() const noexcept { return m_lastError; }

    /**
     * @brief Clear the last error
     */
    void ClearError() { m_lastError.clear(); }

    /**
     * @brief Set error callback for script errors
     */
    using ErrorCallback = std::function<void(const std::string& error, const std::string& traceback)>;
    void SetErrorCallback(ErrorCallback callback) { m_errorCallback = std::move(callback); }

    // =========================================================================
    // Thread Safety
    // =========================================================================

    /**
     * @brief Acquire the Global Interpreter Lock (GIL)
     * Must be called before any Python operations from non-main threads
     */
    void AcquireGIL();

    /**
     * @brief Release the Global Interpreter Lock (GIL)
     */
    void ReleaseGIL();

    /**
     * @brief RAII wrapper for GIL management
     */
    class GILGuard {
    public:
        GILGuard();
        ~GILGuard();
        GILGuard(const GILGuard&) = delete;
        GILGuard& operator=(const GILGuard&) = delete;
    private:
        std::unique_ptr<pybind11::gil_scoped_acquire> m_acquire;
    };

    // =========================================================================
    // Metrics and Diagnostics
    // =========================================================================

    /**
     * @brief Get script execution metrics
     */
    [[nodiscard]] const ScriptMetrics& GetMetrics() const noexcept { return m_metrics; }

    /**
     * @brief Reset execution metrics
     */
    void ResetMetrics() { m_metrics.Reset(); }

    /**
     * @brief Get Python version string
     */
    [[nodiscard]] std::string GetPythonVersion() const;

    /**
     * @brief Get list of loaded modules
     */
    [[nodiscard]] std::vector<std::string> GetLoadedModules() const;

    // =========================================================================
    // Context and Bindings Access
    // =========================================================================

    /**
     * @brief Get the script context for exposing game state
     */
    [[nodiscard]] ScriptContext* GetContext() { return m_context.get(); }

    /**
     * @brief Get the event dispatcher
     */
    [[nodiscard]] EventDispatcher* GetEventDispatcher() { return m_eventDispatcher.get(); }

private:
    PythonEngine() = default;
    ~PythonEngine();

    // Internal helpers
    std::string ResolveScriptPath(const std::string& filePath) const;
    void SetupSysPaths();
    void SetupSandbox();
    void HandleException(const std::string& context);
    ScriptResult CreateErrorResult(const std::string& error);
    std::chrono::system_clock::time_point GetFileModTime(const std::string& path) const;

    // State
    bool m_initialized = false;
    PythonEngineConfig m_config;
    std::string m_lastError;

    // Script caching
    std::unordered_map<std::string, CachedScript> m_scriptCache;
    mutable std::mutex m_cacheMutex;

    // Hot reload
    float m_hotReloadTimer = 0.0f;

    // Metrics
    ScriptMetrics m_metrics;

    // Callbacks
    ErrorCallback m_errorCallback;

    // Loaded modules
    std::unordered_map<std::string, std::shared_ptr<pybind11::module_>> m_modules;
    mutable std::mutex m_moduleMutex;

    // Main module globals
    std::shared_ptr<pybind11::dict> m_globals;

    // Context and systems
    std::unique_ptr<ScriptContext> m_context;
    std::unique_ptr<EventDispatcher> m_eventDispatcher;

    // Thread safety
    mutable std::recursive_mutex m_executionMutex;
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename... Args>
ScriptResult PythonEngine::CallFunction(const std::string& moduleName,
                                        const std::string& functionName,
                                        Args&&... args) {
    // Convert variadic args to vector of variants
    std::vector<std::variant<bool, int, float, double, std::string>> argVec;

    // Fold expression to pack arguments
    (argVec.emplace_back(std::forward<Args>(args)), ...);

    return CallFunctionV(moduleName, functionName, argVec);
}

} // namespace Scripting
} // namespace Nova
