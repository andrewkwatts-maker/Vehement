#include "ScriptableComponent.hpp"
#include "PythonEngine.hpp"
#include <chrono>

namespace Nova {
namespace Scripting {

// ============================================================================
// ScriptableComponent Implementation
// ============================================================================

ScriptableComponent::ScriptableComponent() = default;

ScriptableComponent::ScriptableComponent(const std::string& scriptPath)
    : m_scriptPath(scriptPath) {}

ScriptableComponent::~ScriptableComponent() = default;

void ScriptableComponent::AddEventCallback(const std::string& eventName,
                                           const std::string& module,
                                           const std::string& function) {
    // Check if callback already exists
    for (auto& cb : m_eventCallbacks) {
        if (cb.eventName == eventName) {
            cb.pythonModule = module;
            cb.pythonFunction = function;
            cb.enabled = true;
            return;
        }
    }

    ScriptEventCallback callback;
    callback.eventName = eventName;
    callback.pythonModule = module;
    callback.pythonFunction = function;
    m_eventCallbacks.push_back(callback);
}

void ScriptableComponent::RemoveEventCallback(const std::string& eventName) {
    m_eventCallbacks.erase(
        std::remove_if(m_eventCallbacks.begin(), m_eventCallbacks.end(),
                       [&eventName](const ScriptEventCallback& cb) {
                           return cb.eventName == eventName;
                       }),
        m_eventCallbacks.end());
}

void ScriptableComponent::SetEventCallbackEnabled(const std::string& eventName, bool enabled) {
    for (auto& cb : m_eventCallbacks) {
        if (cb.eventName == eventName) {
            cb.enabled = enabled;
            return;
        }
    }
}

bool ScriptableComponent::Initialize(PythonEngine& engine) {
    if (m_initialized) {
        return true;
    }

    // Load the script file if specified
    if (!m_scriptPath.empty()) {
        auto result = engine.ExecuteFile(m_scriptPath);
        if (!result.success) {
            return false;
        }
    }

    // Call init function if specified
    if (!m_initModule.empty() && !m_initFunction.empty()) {
        auto result = engine.CallFunction(m_initModule, m_initFunction,
                                          static_cast<int>(m_entityId));
        if (!result.success) {
            // Init function failure is not fatal
        }
    }

    m_initialized = true;
    return true;
}

void ScriptableComponent::Update(PythonEngine& engine, float deltaTime) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Check update interval
    if (m_updateInterval > 0.0f) {
        m_timeSinceUpdate += deltaTime;
        if (m_timeSinceUpdate < m_updateInterval) {
            return;
        }
        m_timeSinceUpdate = 0.0f;
    }

    // Call update function if specified
    if (m_updateModule.empty() || m_updateFunction.empty()) {
        return;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    auto result = engine.CallFunction(m_updateModule, m_updateFunction,
                                      static_cast<int>(m_entityId), deltaTime);

    auto endTime = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_metrics.RecordUpdate(timeMs);

    if (!result.success) {
        // Log error but don't disable script
    }
}

void ScriptableComponent::HandleEvent(PythonEngine& engine, const std::string& eventName,
                                       const std::unordered_map<std::string, std::string>& eventData) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    for (const auto& callback : m_eventCallbacks) {
        if (callback.eventName == eventName && callback.enabled) {
            // Convert event data to string for Python
            std::string dataStr;
            for (const auto& [key, value] : eventData) {
                if (!dataStr.empty()) dataStr += ";";
                dataStr += key + "=" + value;
            }

            auto result = engine.CallFunction(callback.pythonModule, callback.pythonFunction,
                                              static_cast<int>(m_entityId), dataStr);

            m_metrics.eventCalls++;

            if (!result.success) {
                // Log error but continue
            }
        }
    }
}

void ScriptableComponent::Cleanup(PythonEngine& engine) {
    if (!m_initialized) {
        return;
    }

    // Call cleanup function if specified
    if (!m_cleanupModule.empty() && !m_cleanupFunction.empty()) {
        engine.CallFunction(m_cleanupModule, m_cleanupFunction,
                            static_cast<int>(m_entityId));
    }

    m_initialized = false;
}

// ============================================================================
// ScriptableComponentManager Implementation
// ============================================================================

ScriptableComponentManager::ScriptableComponentManager() = default;
ScriptableComponentManager::~ScriptableComponentManager() = default;

ScriptableComponent* ScriptableComponentManager::AttachScript(uint32_t entityId,
                                                               const std::string& scriptPath) {
    auto component = std::make_unique<ScriptableComponent>(scriptPath);
    component->SetEntityId(entityId);

    ScriptableComponent* ptr = component.get();
    m_components[entityId] = std::move(component);

    // Initialize if engine is set
    if (m_pythonEngine && m_pythonEngine->IsInitialized()) {
        ptr->Initialize(*m_pythonEngine);
    }

    return ptr;
}

void ScriptableComponentManager::AttachComponent(uint32_t entityId,
                                                  std::unique_ptr<ScriptableComponent> component) {
    component->SetEntityId(entityId);

    // Initialize if engine is set
    if (m_pythonEngine && m_pythonEngine->IsInitialized() && !component->IsInitialized()) {
        component->Initialize(*m_pythonEngine);
    }

    m_components[entityId] = std::move(component);
}

void ScriptableComponentManager::DetachScript(uint32_t entityId) {
    auto it = m_components.find(entityId);
    if (it != m_components.end()) {
        if (m_pythonEngine) {
            it->second->Cleanup(*m_pythonEngine);
        }
        m_components.erase(it);
    }
}

ScriptableComponent* ScriptableComponentManager::GetComponent(uint32_t entityId) {
    auto it = m_components.find(entityId);
    return it != m_components.end() ? it->second.get() : nullptr;
}

bool ScriptableComponentManager::HasComponent(uint32_t entityId) const {
    return m_components.contains(entityId);
}

void ScriptableComponentManager::Update(float deltaTime) {
    if (!m_pythonEngine || !m_pythonEngine->IsInitialized()) {
        return;
    }

    for (auto& [entityId, component] : m_components) {
        component->Update(*m_pythonEngine, deltaTime);
    }
}

void ScriptableComponentManager::BroadcastEvent(const std::string& eventName,
                                                 const std::unordered_map<std::string, std::string>& eventData) {
    if (!m_pythonEngine || !m_pythonEngine->IsInitialized()) {
        return;
    }

    for (auto& [entityId, component] : m_components) {
        component->HandleEvent(*m_pythonEngine, eventName, eventData);
    }
}

void ScriptableComponentManager::SendEvent(uint32_t entityId, const std::string& eventName,
                                            const std::unordered_map<std::string, std::string>& eventData) {
    if (!m_pythonEngine || !m_pythonEngine->IsInitialized()) {
        return;
    }

    auto it = m_components.find(entityId);
    if (it != m_components.end()) {
        it->second->HandleEvent(*m_pythonEngine, eventName, eventData);
    }
}

void ScriptableComponentManager::InitializeAll() {
    if (!m_pythonEngine || !m_pythonEngine->IsInitialized()) {
        return;
    }

    for (auto& [entityId, component] : m_components) {
        if (!component->IsInitialized()) {
            component->Initialize(*m_pythonEngine);
        }
    }
}

void ScriptableComponentManager::CleanupAll() {
    if (!m_pythonEngine) {
        return;
    }

    for (auto& [entityId, component] : m_components) {
        component->Cleanup(*m_pythonEngine);
    }

    m_components.clear();
}

std::vector<uint32_t> ScriptableComponentManager::GetScriptedEntities() const {
    std::vector<uint32_t> entities;
    entities.reserve(m_components.size());
    for (const auto& [entityId, _] : m_components) {
        entities.push_back(entityId);
    }
    return entities;
}

} // namespace Scripting
} // namespace Nova
