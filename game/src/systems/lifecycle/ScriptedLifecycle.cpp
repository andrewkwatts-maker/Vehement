#include "ScriptedLifecycle.hpp"
#include <sstream>
#include <sys/stat.h>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// Static Members
// ============================================================================

std::shared_ptr<IPythonBridge> ScriptedLifecycle::s_pythonBridge;
std::vector<std::string> ScriptedLifecycle::s_defaultSearchPaths;

// ============================================================================
// ScriptError Implementation
// ============================================================================

std::string ScriptError::ToString() const {
    std::ostringstream oss;
    oss << "Script Error in " << scriptPath;
    if (!functionName.empty()) {
        oss << "::" << functionName;
    }
    if (lineNumber >= 0) {
        oss << " (line " << lineNumber << ")";
    }
    oss << ": " << errorMessage;
    return oss.str();
}

// ============================================================================
// ScriptedLifecycle Implementation
// ============================================================================

ScriptedLifecycle::ScriptedLifecycle() {
    AddLifecycleFlags(LifecycleFlags::HasScript);
}

ScriptedLifecycle::~ScriptedLifecycle() = default;

void ScriptedLifecycle::OnCreate(const nlohmann::json& config) {
    LifecycleBase::OnCreate(config);

    // Extract script config from JSON
    // In production, parse the config properly
    // For now, use already-set script bindings

    m_contextDirty = true;

    // Call on_create script
    if (!m_scriptConfig.onCreate.empty()) {
        CallScript(m_scriptConfig.onCreate, "on_create");
    }
}

void ScriptedLifecycle::OnTick(float deltaTime) {
    LifecycleBase::OnTick(deltaTime);

    // Update context timing
    m_cachedContext.deltaTime = deltaTime;
    m_contextDirty = true;

    // Call on_tick script
    if (!m_scriptConfig.onTick.empty()) {
        CallScript(m_scriptConfig.onTick, "on_tick");
    }
}

bool ScriptedLifecycle::OnEvent(const GameEvent& event) {
    if (LifecycleBase::OnEvent(event)) {
        return true;
    }

    // Find event handler
    std::string eventName = EventTypeToString(event.type);
    auto it = m_scriptConfig.eventHandlers.find(eventName);

    if (it == m_scriptConfig.eventHandlers.end()) {
        // Try lowercase
        std::string lowerName = eventName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        it = m_scriptConfig.eventHandlers.find(lowerName);
    }

    if (it != m_scriptConfig.eventHandlers.end() && !it->second.empty()) {
        // Update context with event data
        m_cachedContext.event.type = eventName;

        if (const auto* dmgData = event.GetData<DamageEventData>()) {
            m_cachedContext.event.damage = dmgData->amount;
        }

        m_contextDirty = true;
        return CallScript(it->second, "on_event");
    }

    return false;
}

void ScriptedLifecycle::OnDestroy() {
    // Call on_destroy script
    if (!m_scriptConfig.onDestroy.empty()) {
        CallScript(m_scriptConfig.onDestroy, "on_destroy");
    }

    LifecycleBase::OnDestroy();
}

void ScriptedLifecycle::SetScriptBindings(const ScriptBindingConfig& config) {
    m_scriptConfig = config;

    // Pre-load scripts
    auto& bridge = GetPythonBridge();
    if (bridge.IsAvailable()) {
        if (!config.onCreate.empty()) bridge.LoadScript(config.onCreate);
        if (!config.onTick.empty()) bridge.LoadScript(config.onTick);
        if (!config.onDestroy.empty()) bridge.LoadScript(config.onDestroy);

        for (const auto& pair : config.eventHandlers) {
            if (!pair.second.empty()) {
                bridge.LoadScript(pair.second);
            }
        }
    }
}

void ScriptedLifecycle::SetOnCreateScript(const std::string& path) {
    m_scriptConfig.onCreate = path;
    if (!path.empty() && GetPythonBridge().IsAvailable()) {
        GetPythonBridge().LoadScript(path);
    }
}

void ScriptedLifecycle::SetOnTickScript(const std::string& path) {
    m_scriptConfig.onTick = path;
    if (!path.empty() && GetPythonBridge().IsAvailable()) {
        GetPythonBridge().LoadScript(path);
    }
}

void ScriptedLifecycle::SetOnDestroyScript(const std::string& path) {
    m_scriptConfig.onDestroy = path;
    if (!path.empty() && GetPythonBridge().IsAvailable()) {
        GetPythonBridge().LoadScript(path);
    }
}

void ScriptedLifecycle::SetEventScript(const std::string& eventType, const std::string& path) {
    m_scriptConfig.eventHandlers[eventType] = path;
    if (!path.empty() && GetPythonBridge().IsAvailable()) {
        GetPythonBridge().LoadScript(path);
    }
}

void ScriptedLifecycle::RemoveEventScript(const std::string& eventType) {
    auto it = m_scriptConfig.eventHandlers.find(eventType);
    if (it != m_scriptConfig.eventHandlers.end()) {
        m_scriptConfig.eventHandlers.erase(it);
    }
}

ScriptContext ScriptedLifecycle::BuildContext() const {
    if (!m_contextDirty) {
        return m_cachedContext;
    }

    ScriptContext ctx;
    ctx.handle = m_handle;

    // Properties as JSON
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& pair : m_properties) {
        if (!first) oss << ",";
        oss << "\"" << pair.first << "\":\"" << pair.second << "\"";
        first = false;
    }
    oss << "}";
    ctx.properties = oss.str();

    m_cachedContext = ctx;
    m_contextDirty = false;

    return ctx;
}

void ScriptedLifecycle::ApplyContext(const ScriptContext& context) {
    // Apply transform changes
    // In a full implementation, this would update the entity's actual transform

    // Apply health changes
    // ...

    m_contextDirty = true;
}

void ScriptedLifecycle::SetProperty(const std::string& key, const std::string& value) {
    m_properties[key] = value;
    m_contextDirty = true;
}

std::string ScriptedLifecycle::GetProperty(const std::string& key) const {
    auto it = m_properties.find(key);
    return it != m_properties.end() ? it->second : "";
}

void ScriptedLifecycle::SetPythonBridge(std::shared_ptr<IPythonBridge> bridge) {
    s_pythonBridge = std::move(bridge);
    if (s_pythonBridge && !s_defaultSearchPaths.empty()) {
        s_pythonBridge->SetSearchPaths(s_defaultSearchPaths);
    }
}

IPythonBridge& ScriptedLifecycle::GetPythonBridge() {
    if (!s_pythonBridge) {
        // Create stub bridge if none set
        s_pythonBridge = std::make_shared<StubPythonBridge>();
    }
    return *s_pythonBridge;
}

void ScriptedLifecycle::SetDefaultSearchPaths(const std::vector<std::string>& paths) {
    s_defaultSearchPaths = paths;
    if (s_pythonBridge) {
        s_pythonBridge->SetSearchPaths(paths);
    }
}

bool ScriptedLifecycle::CallScript(const std::string& scriptPath, const std::string& function) {
    auto& bridge = GetPythonBridge();

    if (!bridge.IsAvailable()) {
        return false;
    }

    ScriptContext ctx = BuildContext();
    ScriptError error;

    bool success = bridge.CallFunction(scriptPath, function, ctx, &error);

    if (!success) {
        LogError(error);
        return false;
    }

    // Apply any context changes from script
    ApplyContext(ctx);

    return true;
}

void ScriptedLifecycle::LogError(const ScriptError& error) {
    m_errors.push_back(error);

    // Keep error log bounded
    if (m_errors.size() > 100) {
        m_errors.erase(m_errors.begin());
    }

    if (m_onError) {
        m_onError(error);
    }
}

// ============================================================================
// ScriptManager Implementation
// ============================================================================

ScriptManager& ScriptManager::Instance() {
    static ScriptManager instance;
    return instance;
}

void ScriptManager::PreloadScripts(const std::vector<std::string>& scriptPaths) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& bridge = ScriptedLifecycle::GetPythonBridge();
    if (!bridge.IsAvailable()) return;

    for (const auto& path : scriptPaths) {
        if (m_loadedScripts.find(path) == m_loadedScripts.end()) {
            if (bridge.LoadScript(path)) {
                m_loadedScripts.insert(path);
                m_stats.scriptsLoaded++;

                // Store mod time for hot reload
                struct stat statBuf;
                if (stat(path.c_str(), &statBuf) == 0) {
                    m_scriptModTimes[path] = statBuf.st_mtime;
                }
            }
        }
    }
}

void ScriptManager::UnloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& bridge = ScriptedLifecycle::GetPythonBridge();
    for (const auto& path : m_loadedScripts) {
        bridge.UnloadScript(path);
    }

    m_loadedScripts.clear();
    m_scriptModTimes.clear();
    m_stats.scriptsLoaded = 0;
}

bool ScriptManager::ReloadScript(const std::string& scriptPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& bridge = ScriptedLifecycle::GetPythonBridge();
    if (!bridge.IsAvailable()) return false;

    bridge.UnloadScript(scriptPath);

    if (bridge.LoadScript(scriptPath)) {
        m_loadedScripts.insert(scriptPath);

        struct stat statBuf;
        if (stat(scriptPath.c_str(), &statBuf) == 0) {
            m_scriptModTimes[scriptPath] = statBuf.st_mtime;
        }
        return true;
    }

    m_loadedScripts.erase(scriptPath);
    return false;
}

bool ScriptManager::IsLoaded(const std::string& scriptPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loadedScripts.find(scriptPath) != m_loadedScripts.end();
}

void ScriptManager::SetHotReloadEnabled(bool enabled) {
    m_hotReloadEnabled = enabled;
}

size_t ScriptManager::CheckForReloads() {
    if (!m_hotReloadEnabled) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    size_t reloaded = 0;
    std::vector<std::string> toReload;

    for (const auto& pair : m_scriptModTimes) {
        struct stat statBuf;
        if (stat(pair.first.c_str(), &statBuf) == 0) {
            if (statBuf.st_mtime > pair.second) {
                toReload.push_back(pair.first);
            }
        }
    }

    // Unlock before reloading
    for (const auto& path : toReload) {
        // Note: ReloadScript acquires lock, so we'd need to refactor
        // For now, direct reload without lock
        auto& bridge = ScriptedLifecycle::GetPythonBridge();
        bridge.UnloadScript(path);
        if (bridge.LoadScript(path)) {
            struct stat statBuf;
            if (stat(path.c_str(), &statBuf) == 0) {
                m_scriptModTimes[path] = statBuf.st_mtime;
            }
            reloaded++;
        }
    }

    return reloaded;
}

ScriptManager::Stats ScriptManager::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

} // namespace Lifecycle
} // namespace Vehement
