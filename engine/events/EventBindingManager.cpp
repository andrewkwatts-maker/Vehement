#include "EventBindingManager.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

// Include nlohmann/json for JSON operations
#include <nlohmann/json.hpp>

namespace Nova {
namespace Events {

// ============================================================================
// Singleton
// ============================================================================

EventBindingManager& EventBindingManager::Instance() {
    static EventBindingManager instance;
    return instance;
}

EventBindingManager::~EventBindingManager() {
    if (m_initialized) {
        Shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

bool EventBindingManager::Initialize(const Config& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;

    // Set up condition evaluator with Python engine
    if (m_pythonEngine) {
        EventConditionEvaluator::SetPythonEngine(m_pythonEngine);
    }

    // Load all bindings from the configured directory
    if (!m_config.bindingsDirectory.empty()) {
        LoadAllBindings();
    }

    m_initialized = true;
    return true;
}

void EventBindingManager::Shutdown() {
    if (!m_initialized) return;

    ClearBindings();
    m_delayedExecutions.clear();
    m_changeCallbacks.clear();
    m_fileModTimes.clear();

    m_initialized = false;
}

void EventBindingManager::Update(float deltaTime) {
    if (!m_initialized) return;

    // Check hot reload
    if (m_config.enableHotReload) {
        m_hotReloadTimer += deltaTime;
        if (m_hotReloadTimer >= m_config.hotReloadInterval) {
            m_hotReloadTimer = 0.0f;
            CheckHotReload();
        }
    }

    // Process delayed executions
    ProcessDelayedExecutions(deltaTime);
}

// ============================================================================
// Python Integration
// ============================================================================

void EventBindingManager::SetPythonEngine(Scripting::PythonEngine* engine) {
    m_pythonEngine = engine;
    EventConditionEvaluator::SetPythonEngine(engine);
}

// ============================================================================
// Binding Management
// ============================================================================

bool EventBindingManager::AddBinding(EventBinding binding) {
    std::lock_guard lock(m_bindingsMutex);

    if (binding.id.empty()) {
        return false;
    }

    if (m_bindings.contains(binding.id)) {
        return false;
    }

    // Validate if configured
    if (m_config.validateOnLoad) {
        std::string error = ValidateBinding(binding);
        if (!error.empty()) {
            return false;
        }
    }

    const std::string id = binding.id;
    const std::string eventName = binding.condition.eventName;
    const std::string category = binding.category;

    m_bindings[id] = std::move(binding);

    // Index by event name
    if (!eventName.empty()) {
        m_bindingsByEvent[eventName].push_back(id);
    }

    // Index by category
    if (!category.empty()) {
        m_bindingsByCategory[category].push_back(id);
    }

    NotifyBindingChanged(id, true);
    return true;
}

std::string EventBindingManager::AddBindingAuto(EventBinding binding) {
    if (binding.id.empty()) {
        binding.id = "binding_" + std::to_string(m_nextBindingId++);
    }
    std::string id = binding.id;
    if (AddBinding(std::move(binding))) {
        return id;
    }
    return "";
}

bool EventBindingManager::RemoveBinding(const std::string& bindingId) {
    std::lock_guard lock(m_bindingsMutex);

    auto it = m_bindings.find(bindingId);
    if (it == m_bindings.end()) {
        return false;
    }

    const auto& binding = it->second;

    // Remove from event index
    if (!binding.condition.eventName.empty()) {
        auto& vec = m_bindingsByEvent[binding.condition.eventName];
        vec.erase(std::remove(vec.begin(), vec.end(), bindingId), vec.end());
    }

    // Remove from category index
    if (!binding.category.empty()) {
        auto& vec = m_bindingsByCategory[binding.category];
        vec.erase(std::remove(vec.begin(), vec.end(), bindingId), vec.end());
    }

    m_bindings.erase(it);
    NotifyBindingChanged(bindingId, false);
    return true;
}

EventBinding* EventBindingManager::GetBinding(const std::string& bindingId) {
    std::lock_guard lock(m_bindingsMutex);
    auto it = m_bindings.find(bindingId);
    return it != m_bindings.end() ? &it->second : nullptr;
}

const EventBinding* EventBindingManager::GetBinding(const std::string& bindingId) const {
    std::lock_guard lock(m_bindingsMutex);
    auto it = m_bindings.find(bindingId);
    return it != m_bindings.end() ? &it->second : nullptr;
}

bool EventBindingManager::HasBinding(const std::string& bindingId) const {
    std::lock_guard lock(m_bindingsMutex);
    return m_bindings.contains(bindingId);
}

void EventBindingManager::SetBindingEnabled(const std::string& bindingId, bool enabled) {
    std::lock_guard lock(m_bindingsMutex);
    auto it = m_bindings.find(bindingId);
    if (it != m_bindings.end()) {
        it->second.enabled = enabled;
    }
}

std::vector<std::string> EventBindingManager::GetBindingIds() const {
    std::lock_guard lock(m_bindingsMutex);
    std::vector<std::string> ids;
    ids.reserve(m_bindings.size());
    for (const auto& [id, _] : m_bindings) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<const EventBinding*> EventBindingManager::GetBindingsByCategory(const std::string& category) const {
    std::lock_guard lock(m_bindingsMutex);
    std::vector<const EventBinding*> result;

    auto it = m_bindingsByCategory.find(category);
    if (it != m_bindingsByCategory.end()) {
        for (const auto& id : it->second) {
            auto bindingIt = m_bindings.find(id);
            if (bindingIt != m_bindings.end()) {
                result.push_back(&bindingIt->second);
            }
        }
    }
    return result;
}

std::vector<const EventBinding*> EventBindingManager::GetBindingsForEvent(const std::string& eventType) const {
    std::lock_guard lock(m_bindingsMutex);
    std::vector<const EventBinding*> result;

    auto it = m_bindingsByEvent.find(eventType);
    if (it != m_bindingsByEvent.end()) {
        for (const auto& id : it->second) {
            auto bindingIt = m_bindings.find(id);
            if (bindingIt != m_bindings.end()) {
                result.push_back(&bindingIt->second);
            }
        }
    }
    return result;
}

std::vector<const EventBinding*> EventBindingManager::GetAllBindings() const {
    std::lock_guard lock(m_bindingsMutex);
    std::vector<const EventBinding*> result;
    result.reserve(m_bindings.size());
    for (const auto& [_, binding] : m_bindings) {
        result.push_back(&binding);
    }
    return result;
}

size_t EventBindingManager::GetBindingCount() const {
    std::lock_guard lock(m_bindingsMutex);
    return m_bindings.size();
}

void EventBindingManager::ClearBindings() {
    std::lock_guard lock(m_bindingsMutex);
    m_bindings.clear();
    m_bindingsByEvent.clear();
    m_bindingsByCategory.clear();
}

// ============================================================================
// Loading and Saving
// ============================================================================

int EventBindingManager::LoadBindingsFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return -1;
    }

    try {
        json j;
        file >> j;

        int count = 0;

        // Handle both single binding and array of bindings
        if (j.is_array()) {
            for (const auto& bindingJson : j) {
                EventBinding binding = EventBinding::FromJson(bindingJson);
                if (AddBinding(std::move(binding))) {
                    count++;
                }
            }
        } else if (j.contains("bindings")) {
            // Handle binding group format
            for (const auto& bindingJson : j["bindings"]) {
                EventBinding binding = EventBinding::FromJson(bindingJson);
                if (AddBinding(std::move(binding))) {
                    count++;
                }
            }
        } else {
            // Single binding
            EventBinding binding = EventBinding::FromJson(j);
            if (AddBinding(std::move(binding))) {
                count++;
            }
        }

        // Track file modification time for hot reload
        m_fileModTimes[filePath] = std::filesystem::last_write_time(filePath);

        return count;

    } catch (const std::exception& e) {
        return -1;
    }
}

int EventBindingManager::LoadAllBindings() {
    if (m_config.bindingsDirectory.empty()) {
        return 0;
    }

    if (!std::filesystem::exists(m_config.bindingsDirectory)) {
        return 0;
    }

    int totalLoaded = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(m_config.bindingsDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            int loaded = LoadBindingsFromFile(entry.path().string());
            if (loaded > 0) {
                totalLoaded += loaded;
            }
        }
    }

    return totalLoaded;
}

bool EventBindingManager::SaveBindingsToFile(const std::string& filePath,
                                             const std::vector<std::string>& bindingIds) {
    std::lock_guard lock(m_bindingsMutex);

    json bindingsArray = json::array();

    if (bindingIds.empty()) {
        // Export all bindings
        for (const auto& [_, binding] : m_bindings) {
            bindingsArray.push_back(binding.ToJson());
        }
    } else {
        // Export specific bindings
        for (const auto& id : bindingIds) {
            auto it = m_bindings.find(id);
            if (it != m_bindings.end()) {
                bindingsArray.push_back(it->second.ToJson());
            }
        }
    }

    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    json output;
    output["bindings"] = bindingsArray;
    output["version"] = "1.0";
    output["exportedAt"] = std::chrono::system_clock::now().time_since_epoch().count();

    file << output.dump(2);
    return true;
}

json EventBindingManager::ExportBindings(const std::vector<std::string>& bindingIds) const {
    std::lock_guard lock(m_bindingsMutex);

    json bindingsArray = json::array();
    for (const auto& id : bindingIds) {
        auto it = m_bindings.find(id);
        if (it != m_bindings.end()) {
            bindingsArray.push_back(it->second.ToJson());
        }
    }

    return bindingsArray;
}

int EventBindingManager::ImportBindings(const json& bindingsJson) {
    int count = 0;

    if (bindingsJson.is_array()) {
        for (const auto& bindingJson : bindingsJson) {
            EventBinding binding = EventBinding::FromJson(bindingJson);
            if (AddBinding(std::move(binding))) {
                count++;
            }
        }
    }

    return count;
}

void EventBindingManager::ReloadBindings() {
    ClearBindings();
    LoadAllBindings();
}

// ============================================================================
// Validation
// ============================================================================

std::string EventBindingManager::ValidateBinding(const EventBinding& binding) const {
    return binding.Validate();
}

std::unordered_map<std::string, std::string> EventBindingManager::ValidateAllBindings() const {
    std::lock_guard lock(m_bindingsMutex);

    std::unordered_map<std::string, std::string> errors;
    for (const auto& [id, binding] : m_bindings) {
        std::string error = binding.Validate();
        if (!error.empty()) {
            errors[id] = error;
        }
    }
    return errors;
}

// ============================================================================
// Execution
// ============================================================================

bool EventBindingManager::ExecuteBinding(const std::string& bindingId,
                                         const std::unordered_map<std::string, std::any>& eventData) {
    auto startTime = std::chrono::high_resolution_clock::now();

    EventBinding* binding = GetBinding(bindingId);
    if (!binding) {
        return false;
    }

    if (!binding->CanExecute()) {
        return false;
    }

    // Handle delay
    if (binding->delay > 0.0f) {
        ScheduleDelayedExecution(bindingId, eventData, binding->delay);
        return true;
    }

    bool success = true;

    try {
        switch (binding->callbackType) {
            case CallbackType::Python:
                ExecutePythonCallback(*binding, eventData);
                break;

            case CallbackType::Native:
                if (binding->nativeCallback) {
                    binding->nativeCallback(binding->condition, eventData);
                }
                break;

            case CallbackType::Event:
                ExecuteEventEmission(*binding, eventData);
                break;

            case CallbackType::Command:
                ExecuteCommand(*binding, eventData);
                break;

            case CallbackType::Script:
                if (m_pythonEngine && !binding->pythonFile.empty()) {
                    m_pythonEngine->ExecuteFile(binding->pythonFile);
                }
                break;
        }

        binding->RecordExecution();
        m_stats.successfulExecutions++;

    } catch (const std::exception& e) {
        binding->RecordError(e.what());
        m_stats.failedExecutions++;
        success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double timeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.totalExecutionTimeMs += timeMs;
    m_stats.totalExecutions++;
    m_stats.executionsPerBinding[bindingId]++;

    return success;
}

bool EventBindingManager::TestBinding(const std::string& bindingId) {
    const EventBinding* binding = GetBinding(bindingId);
    if (!binding) {
        return false;
    }

    // Validate the binding
    std::string error = ValidateBinding(*binding);
    return error.empty();
}

// ============================================================================
// Hot Reload
// ============================================================================

void EventBindingManager::SetHotReloadEnabled(bool enabled) {
    m_config.enableHotReload = enabled;
}

void EventBindingManager::CheckHotReload() {
    bool needsReload = false;

    for (auto& [path, modTime] : m_fileModTimes) {
        if (std::filesystem::exists(path)) {
            auto currentModTime = std::filesystem::last_write_time(path);
            if (currentModTime > modTime) {
                needsReload = true;
                break;
            }
        }
    }

    if (needsReload) {
        ReloadBindings();
    }
}

size_t EventBindingManager::OnBindingChanged(BindingChangedCallback callback) {
    std::lock_guard lock(m_callbackMutex);
    size_t id = m_nextCallbackId++;
    m_changeCallbacks[id] = std::move(callback);
    return id;
}

void EventBindingManager::RemoveBindingChangedCallback(size_t callbackId) {
    std::lock_guard lock(m_callbackMutex);
    m_changeCallbacks.erase(callbackId);
}

// ============================================================================
// Event Integration
// ============================================================================

void EventBindingManager::RegisterWithEventBus(Reflect::EventBus& eventBus) {
    std::lock_guard lock(m_bindingsMutex);

    for (const auto& [id, binding] : m_bindings) {
        if (!binding.enabled) continue;

        auto subId = eventBus.Subscribe(
            binding.id,
            binding.condition.eventName.empty() ? "*" : binding.condition.eventName,
            [this, bindingId = id](Reflect::BusEvent& event) {
                auto* binding = GetBinding(bindingId);
                if (!binding || !binding->enabled) return;

                // Evaluate condition
                bool matches = EventConditionEvaluator::Evaluate(
                    binding->condition,
                    event.eventType,
                    event.sourceType,
                    std::to_string(event.sourceId),
                    event.source
                );

                if (matches) {
                    std::unordered_map<std::string, std::any> eventData = event.data;
                    eventData["eventType"] = event.eventType;
                    eventData["sourceType"] = event.sourceType;
                    eventData["sourceId"] = event.sourceId;
                    ExecuteBinding(bindingId, eventData);
                }
            },
            static_cast<Reflect::EventPriority>(binding.priority)
        );

        m_eventBusSubscriptions.push_back(subId);
    }
}

void EventBindingManager::UnregisterFromEventBus(Reflect::EventBus& eventBus) {
    for (size_t subId : m_eventBusSubscriptions) {
        eventBus.Unsubscribe(subId);
    }
    m_eventBusSubscriptions.clear();
}

// ============================================================================
// Internal Methods
// ============================================================================

void EventBindingManager::ExecutePythonCallback(const EventBinding& binding,
                                                const std::unordered_map<std::string, std::any>& eventData) {
    if (!m_pythonEngine) return;

    if (!binding.pythonScript.empty()) {
        // Execute inline script
        m_pythonEngine->ExecuteString(binding.pythonScript, binding.id);
    } else if (!binding.pythonFunction.empty() && !binding.pythonModule.empty()) {
        // Call Python function
        m_pythonEngine->CallFunction<>(binding.pythonModule, binding.pythonFunction);
    } else if (!binding.pythonFile.empty()) {
        // Execute script file
        m_pythonEngine->ExecuteFile(binding.pythonFile);
    }
}

void EventBindingManager::ExecuteEventEmission(const EventBinding& binding,
                                               const std::unordered_map<std::string, std::any>& eventData) {
    Reflect::BusEvent event(binding.emitEventType);
    event.data = binding.emitEventData;

    // Merge in original event data
    for (const auto& [key, value] : eventData) {
        if (!event.data.contains(key)) {
            event.data[key] = value;
        }
    }

    Reflect::EventBus::Instance().Publish(event);
}

void EventBindingManager::ExecuteCommand(const EventBinding& binding,
                                         const std::unordered_map<std::string, std::any>& eventData) {
    // Command execution would be implemented based on game's command system
    // For now, this is a placeholder
}

void EventBindingManager::ScheduleDelayedExecution(const std::string& bindingId,
                                                   const std::unordered_map<std::string, std::any>& eventData,
                                                   float delay) {
    std::lock_guard lock(m_delayedMutex);
    m_delayedExecutions.push_back({bindingId, eventData, delay});
}

void EventBindingManager::ProcessDelayedExecutions(float deltaTime) {
    std::lock_guard lock(m_delayedMutex);

    auto it = m_delayedExecutions.begin();
    while (it != m_delayedExecutions.end()) {
        it->delay -= deltaTime;
        if (it->delay <= 0.0f) {
            // Get binding and execute without delay this time
            EventBinding* binding = GetBinding(it->bindingId);
            if (binding) {
                float savedDelay = binding->delay;
                binding->delay = 0.0f;
                ExecuteBinding(it->bindingId, it->eventData);
                binding->delay = savedDelay;
            }
            it = m_delayedExecutions.erase(it);
        } else {
            ++it;
        }
    }
}

void EventBindingManager::NotifyBindingChanged(const std::string& bindingId, bool added) {
    std::lock_guard lock(m_callbackMutex);
    for (const auto& [_, callback] : m_changeCallbacks) {
        if (callback) {
            callback(bindingId, added);
        }
    }
}

} // namespace Events
} // namespace Nova
