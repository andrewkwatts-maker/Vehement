#include "PythonEngine.hpp"
#include "ScriptContext.hpp"
#include "EventDispatcher.hpp"
#include "ScriptBindings.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>

namespace py = pybind11;
namespace fs = std::filesystem;

namespace Nova {
namespace Scripting {

// ============================================================================
// ScriptMetrics Implementation
// ============================================================================

void ScriptMetrics::RecordExecution(double timeMs, bool success) {
    totalExecutions++;
    if (!success) {
        failedExecutions++;
    }
    totalExecutionTimeMs += timeMs;
    avgExecutionTimeMs = totalExecutionTimeMs / totalExecutions;
    if (timeMs > maxExecutionTimeMs) {
        maxExecutionTimeMs = timeMs;
    }
    lastExecution = std::chrono::system_clock::now();
}

void ScriptMetrics::Reset() {
    totalExecutions = 0;
    failedExecutions = 0;
    totalExecutionTimeMs = 0.0;
    avgExecutionTimeMs = 0.0;
    maxExecutionTimeMs = 0.0;
    hotReloads = 0;
}

// ============================================================================
// GILGuard Implementation
// ============================================================================

PythonEngine::GILGuard::GILGuard() {
    m_acquire = std::make_unique<py::gil_scoped_acquire>();
}

PythonEngine::GILGuard::~GILGuard() = default;

// ============================================================================
// PythonEngine Implementation
// ============================================================================

PythonEngine& PythonEngine::Instance() {
    static PythonEngine instance;
    return instance;
}

PythonEngine::~PythonEngine() {
    if (m_initialized) {
        Shutdown();
    }
}

bool PythonEngine::Initialize(const PythonEngineConfig& config) {
    if (m_initialized) {
        m_lastError = "Python engine already initialized";
        return false;
    }

    m_config = config;

    try {
        // Initialize the Python interpreter
        py::initialize_interpreter();

        // Setup system paths for script discovery
        SetupSysPaths();

        // Create main module globals dict
        m_globals = std::make_shared<py::dict>();

        // Import builtins for the global namespace
        auto builtins = py::module_::import("builtins");
        (*m_globals)["__builtins__"] = builtins;

        // Setup sandbox if enabled
        if (m_config.enableSandbox) {
            SetupSandbox();
        }

        // Create script context for exposing game state
        m_context = std::make_unique<ScriptContext>();

        // Create event dispatcher
        m_eventDispatcher = std::make_unique<EventDispatcher>();

        // Register C++ bindings
        ScriptBindings::RegisterAll();

        m_initialized = true;
        return true;

    } catch (const py::error_already_set& e) {
        m_lastError = std::string("Python initialization failed: ") + e.what();
        if (m_errorCallback) {
            m_errorCallback(m_lastError, "");
        }
        return false;
    } catch (const std::exception& e) {
        m_lastError = std::string("Initialization exception: ") + e.what();
        return false;
    }
}

void PythonEngine::Shutdown() {
    if (!m_initialized) {
        return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    // Clear cached scripts
    {
        std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
        m_scriptCache.clear();
    }

    // Clear loaded modules
    {
        std::lock_guard<std::mutex> moduleLock(m_moduleMutex);
        m_modules.clear();
    }

    // Release context and dispatcher
    m_eventDispatcher.reset();
    m_context.reset();

    // Release globals
    m_globals.reset();

    // Finalize interpreter
    try {
        py::finalize_interpreter();
    } catch (...) {
        // Ignore shutdown errors
    }

    m_initialized = false;
}

void PythonEngine::Update(float deltaTime) {
    if (!m_initialized || !m_config.enableHotReload) {
        return;
    }

    m_hotReloadTimer += deltaTime;
    if (m_hotReloadTimer >= m_config.hotReloadCheckInterval) {
        m_hotReloadTimer = 0.0f;
        CheckHotReload();
    }
}

// ============================================================================
// Script Execution
// ============================================================================

ScriptResult PythonEngine::ExecuteFile(const std::string& filePath) {
    if (!m_initialized) {
        return CreateErrorResult("Python engine not initialized");
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    try {
        GILGuard gil;

        // Resolve full path
        std::string fullPath = ResolveScriptPath(filePath);
        if (fullPath.empty()) {
            return CreateErrorResult("Script file not found: " + filePath);
        }

        // Check cache
        {
            std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
            auto it = m_scriptCache.find(fullPath);
            if (it != m_scriptCache.end() && it->second.isValid) {
                // Use cached script if not modified
                if (!m_config.enableHotReload || !IsScriptModified(fullPath)) {
                    // Execute cached code
                    py::exec(*(it->second.compiledCode), *m_globals);
                    result.success = true;

                    auto endTime = std::chrono::high_resolution_clock::now();
                    double execTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
                    m_metrics.RecordExecution(execTime, true);
                    return result;
                }
            }
        }

        // Read script file
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            return CreateErrorResult("Failed to open script file: " + fullPath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();

        // Compile the code
        auto code = py::eval<py::eval_statements>(source, *m_globals);

        // Cache the compiled code
        {
            std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
            CachedScript cached;
            cached.path = fullPath;
            cached.source = source;
            cached.loadTime = std::chrono::system_clock::now();
            cached.fileModTime = GetFileModTime(fullPath);
            cached.isValid = true;
            cached.compiledCode = std::make_shared<py::object>(code);
            m_scriptCache[fullPath] = std::move(cached);
        }

        result.success = true;

    } catch (const py::error_already_set& e) {
        HandleException("ExecuteFile(" + filePath + ")");
        result = CreateErrorResult(m_lastError);
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception in ExecuteFile: ") + e.what();
        result = CreateErrorResult(m_lastError);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double execTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordExecution(execTime, result.success);

    return result;
}

ScriptResult PythonEngine::ExecuteString(const std::string& code, const std::string& name) {
    if (!m_initialized) {
        return CreateErrorResult("Python engine not initialized");
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    try {
        GILGuard gil;

        // Create a local namespace for this execution
        py::dict localNs;
        localNs["__name__"] = name;

        py::exec(code, *m_globals, localNs);
        result.success = true;

    } catch (const py::error_already_set& e) {
        HandleException("ExecuteString(" + name + ")");
        result = CreateErrorResult(m_lastError);
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception in ExecuteString: ") + e.what();
        result = CreateErrorResult(m_lastError);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double execTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordExecution(execTime, result.success);

    return result;
}

bool PythonEngine::ImportModule(const std::string& moduleName) {
    if (!m_initialized) {
        m_lastError = "Python engine not initialized";
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    try {
        GILGuard gil;

        auto module = py::module_::import(moduleName.c_str());

        std::lock_guard<std::mutex> moduleLock(m_moduleMutex);
        m_modules[moduleName] = std::make_shared<py::module_>(module);

        // Add to globals
        (*m_globals)[moduleName.c_str()] = module;

        return true;

    } catch (const py::error_already_set& e) {
        HandleException("ImportModule(" + moduleName + ")");
        return false;
    }
}

bool PythonEngine::ReloadModule(const std::string& moduleName) {
    if (!m_initialized) {
        m_lastError = "Python engine not initialized";
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    try {
        GILGuard gil;

        // Get importlib for reload
        auto importlib = py::module_::import("importlib");

        std::lock_guard<std::mutex> moduleLock(m_moduleMutex);
        auto it = m_modules.find(moduleName);
        if (it == m_modules.end()) {
            m_lastError = "Module not loaded: " + moduleName;
            return false;
        }

        // Reload the module
        auto reloaded = importlib.attr("reload")(*(it->second));
        it->second = std::make_shared<py::module_>(reloaded.cast<py::module_>());

        // Update in globals
        (*m_globals)[moduleName.c_str()] = *(it->second);

        m_metrics.hotReloads++;
        return true;

    } catch (const py::error_already_set& e) {
        HandleException("ReloadModule(" + moduleName + ")");
        return false;
    }
}

// ============================================================================
// Function Calling
// ============================================================================

ScriptResult PythonEngine::CallFunctionV(
    const std::string& moduleName,
    const std::string& functionName,
    const std::vector<std::variant<bool, int, float, double, std::string>>& args) {

    if (!m_initialized) {
        return CreateErrorResult("Python engine not initialized");
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    try {
        GILGuard gil;

        // Get the module
        std::lock_guard<std::mutex> moduleLock(m_moduleMutex);
        auto it = m_modules.find(moduleName);
        if (it == m_modules.end()) {
            // Try to import it first
            m_moduleMutex.unlock();
            if (!ImportModule(moduleName)) {
                return CreateErrorResult("Module not found: " + moduleName);
            }
            m_moduleMutex.lock();
            it = m_modules.find(moduleName);
        }

        // Get the function
        if (!py::hasattr(*(it->second), functionName.c_str())) {
            return CreateErrorResult("Function not found: " + functionName + " in module " + moduleName);
        }

        auto func = it->second->attr(functionName.c_str());

        // Convert arguments to Python tuple
        py::tuple pyArgs(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            std::visit([&pyArgs, i](auto&& arg) {
                pyArgs[i] = py::cast(arg);
            }, args[i]);
        }

        // Call the function
        py::object pyResult = func(*pyArgs);

        // Convert result back to C++
        result.success = true;
        if (!pyResult.is_none()) {
            if (py::isinstance<py::bool_>(pyResult)) {
                result.returnValue = pyResult.cast<bool>();
            } else if (py::isinstance<py::int_>(pyResult)) {
                result.returnValue = pyResult.cast<int>();
            } else if (py::isinstance<py::float_>(pyResult)) {
                result.returnValue = pyResult.cast<double>();
            } else if (py::isinstance<py::str>(pyResult)) {
                result.returnValue = pyResult.cast<std::string>();
            }
        }

    } catch (const py::error_already_set& e) {
        HandleException("CallFunction(" + moduleName + "." + functionName + ")");
        result = CreateErrorResult(m_lastError);
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception in CallFunction: ") + e.what();
        result = CreateErrorResult(m_lastError);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double execTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordExecution(execTime, result.success);

    return result;
}

ScriptResult PythonEngine::CallMethod(
    const std::string& objectName,
    const std::string& methodName,
    const std::vector<std::variant<bool, int, float, double, std::string>>& args) {

    if (!m_initialized) {
        return CreateErrorResult("Python engine not initialized");
    }

    std::lock_guard<std::recursive_mutex> lock(m_executionMutex);

    auto startTime = std::chrono::high_resolution_clock::now();
    ScriptResult result;

    try {
        GILGuard gil;

        // Get the object from globals
        if (!m_globals->contains(objectName.c_str())) {
            return CreateErrorResult("Object not found in globals: " + objectName);
        }

        py::object obj = (*m_globals)[objectName.c_str()];

        // Get the method
        if (!py::hasattr(obj, methodName.c_str())) {
            return CreateErrorResult("Method not found: " + methodName + " on object " + objectName);
        }

        auto method = obj.attr(methodName.c_str());

        // Convert arguments
        py::tuple pyArgs(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            std::visit([&pyArgs, i](auto&& arg) {
                pyArgs[i] = py::cast(arg);
            }, args[i]);
        }

        // Call method
        py::object pyResult = method(*pyArgs);

        result.success = true;
        // Convert result (same as CallFunctionV)
        if (!pyResult.is_none()) {
            if (py::isinstance<py::bool_>(pyResult)) {
                result.returnValue = pyResult.cast<bool>();
            } else if (py::isinstance<py::int_>(pyResult)) {
                result.returnValue = pyResult.cast<int>();
            } else if (py::isinstance<py::float_>(pyResult)) {
                result.returnValue = pyResult.cast<double>();
            } else if (py::isinstance<py::str>(pyResult)) {
                result.returnValue = pyResult.cast<std::string>();
            }
        }

    } catch (const py::error_already_set& e) {
        HandleException("CallMethod(" + objectName + "." + methodName + ")");
        result = CreateErrorResult(m_lastError);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double execTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordExecution(execTime, result.success);

    return result;
}

// ============================================================================
// Script Caching and Hot-Reload
// ============================================================================

bool PythonEngine::PreloadScript(const std::string& filePath) {
    // Simply execute the file, which will cache it
    auto result = ExecuteFile(filePath);
    return result.success;
}

void PythonEngine::ClearCache() {
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
    m_scriptCache.clear();
}

void PythonEngine::CheckHotReload() {
    if (!m_initialized || !m_config.enableHotReload) {
        return;
    }

    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);

    for (auto& [path, cached] : m_scriptCache) {
        if (IsScriptModified(path)) {
            // Invalidate cache entry
            cached.isValid = false;

            // Re-execute to reload
            ExecuteFile(path);
        }
    }
}

std::vector<std::string> PythonEngine::GetCachedScripts() const {
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);

    std::vector<std::string> scripts;
    scripts.reserve(m_scriptCache.size());
    for (const auto& [path, _] : m_scriptCache) {
        scripts.push_back(path);
    }
    return scripts;
}

bool PythonEngine::IsScriptModified(const std::string& filePath) const {
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);

    auto it = m_scriptCache.find(filePath);
    if (it == m_scriptCache.end()) {
        return false;
    }

    auto currentModTime = GetFileModTime(filePath);
    return currentModTime > it->second.fileModTime;
}

// ============================================================================
// Thread Safety
// ============================================================================

void PythonEngine::AcquireGIL() {
    PyGILState_Ensure();
}

void PythonEngine::ReleaseGIL() {
    PyGILState_Release(PyGILState_UNLOCKED);
}

// ============================================================================
// Diagnostics
// ============================================================================

std::string PythonEngine::GetPythonVersion() const {
    if (!m_initialized) {
        return "Not initialized";
    }

    try {
        GILGuard gil;
        auto sys = py::module_::import("sys");
        return sys.attr("version").cast<std::string>();
    } catch (...) {
        return "Unknown";
    }
}

std::vector<std::string> PythonEngine::GetLoadedModules() const {
    std::lock_guard<std::mutex> moduleLock(m_moduleMutex);

    std::vector<std::string> modules;
    modules.reserve(m_modules.size());
    for (const auto& [name, _] : m_modules) {
        modules.push_back(name);
    }
    return modules;
}

// ============================================================================
// Internal Helpers
// ============================================================================

std::string PythonEngine::ResolveScriptPath(const std::string& filePath) const {
    // First check if it's an absolute path that exists
    if (fs::exists(filePath)) {
        return filePath;
    }

    // Search in configured script paths
    for (const auto& basePath : m_config.scriptPaths) {
        std::string fullPath = basePath + "/" + filePath;
        if (fs::exists(fullPath)) {
            return fullPath;
        }
    }

    // Check current directory
    if (fs::exists(filePath)) {
        return fs::absolute(filePath).string();
    }

    return "";
}

void PythonEngine::SetupSysPaths() {
    try {
        GILGuard gil;

        auto sys = py::module_::import("sys");
        py::list paths = sys.attr("path");

        for (const auto& scriptPath : m_config.scriptPaths) {
            paths.append(scriptPath);
        }

    } catch (const py::error_already_set& e) {
        HandleException("SetupSysPaths");
    }
}

void PythonEngine::SetupSandbox() {
    try {
        GILGuard gil;

        // Create restricted builtins
        std::string sandboxCode = R"(
import builtins

# Functions to restrict in sandbox mode
_restricted_builtins = ['open', 'exec', 'eval', 'compile', '__import__']

class RestrictedImporter:
    """Custom importer that restricts certain modules"""
    _allowed_modules = {
        'math', 'random', 'time', 'collections', 'itertools',
        'functools', 'json', 'typing', 're', 'copy', 'heapq',
        'nova_game', 'nova_engine', 'nova_ai'  # Our game modules
    }

    def find_module(self, name, path=None):
        # Allow all nova_* modules
        if name.startswith('nova_'):
            return None
        # Allow whitelisted modules
        if name.split('.')[0] in self._allowed_modules:
            return None
        # Block everything else
        raise ImportError(f"Import of '{name}' is restricted in sandbox mode")

# Install restricted importer (if sandbox is enabled)
import sys
sys.meta_path.insert(0, RestrictedImporter())
)";

        py::exec(sandboxCode, *m_globals);

    } catch (const py::error_already_set& e) {
        HandleException("SetupSandbox");
    }
}

void PythonEngine::HandleException(const std::string& context) {
    try {
        GILGuard gil;

        // Get exception info
        PyObject *type, *value, *traceback;
        PyErr_Fetch(&type, &value, &traceback);
        PyErr_NormalizeException(&type, &value, &traceback);

        std::string errorMsg = "Python error";
        std::string tracebackStr;

        if (value) {
            py::object valueObj = py::reinterpret_borrow<py::object>(value);
            errorMsg = py::str(valueObj).cast<std::string>();
        }

        if (traceback) {
            auto tbModule = py::module_::import("traceback");
            py::object tbObj = py::reinterpret_borrow<py::object>(traceback);
            py::list tbLines = tbModule.attr("format_tb")(tbObj);

            for (auto& line : tbLines) {
                tracebackStr += line.cast<std::string>();
            }
        }

        m_lastError = context + ": " + errorMsg;
        if (m_config.verboseErrors && !tracebackStr.empty()) {
            m_lastError += "\nTraceback:\n" + tracebackStr;
        }

        if (m_errorCallback) {
            m_errorCallback(errorMsg, tracebackStr);
        }

        // Clear the error
        PyErr_Clear();

        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);

    } catch (...) {
        m_lastError = context + ": Unknown Python exception";
    }
}

ScriptResult PythonEngine::CreateErrorResult(const std::string& error) {
    ScriptResult result;
    result.success = false;
    result.errorMessage = error;
    m_lastError = error;
    return result;
}

std::chrono::system_clock::time_point PythonEngine::GetFileModTime(const std::string& path) const {
    try {
        auto ftime = fs::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        return sctp;
    } catch (...) {
        return std::chrono::system_clock::time_point{};
    }
}

} // namespace Scripting
} // namespace Nova
