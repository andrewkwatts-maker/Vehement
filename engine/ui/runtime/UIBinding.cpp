#include "UIBinding.hpp"

#include <algorithm>
#include <sstream>

namespace Engine {
namespace UI {

// BindingContext Implementation
BindingContext::BindingContext(const std::string& name) : m_name(name) {}
BindingContext::~BindingContext() = default;

void BindingContext::ExposeFunction(const std::string& name,
                                   std::function<nlohmann::json(const nlohmann::json&)> fn) {
    FunctionBinding binding;
    binding.name = name;
    binding.handler = fn;
    m_functions[name] = binding;
}

void BindingContext::ExposeFunction(const std::string& name,
                                   std::function<nlohmann::json(const nlohmann::json&)> fn,
                                   const std::string& description,
                                   const std::vector<std::string>& paramNames) {
    FunctionBinding binding;
    binding.name = name;
    binding.handler = fn;
    binding.description = description;
    binding.parameterNames = paramNames;
    m_functions[name] = binding;
}

void BindingContext::ExposeProperty(const std::string& name,
                                   std::function<nlohmann::json()> getter,
                                   std::function<void(const nlohmann::json&)> setter) {
    PropertyBinding binding;
    binding.name = name;
    binding.getter = getter;
    binding.setter = setter;
    binding.readOnly = (setter == nullptr);
    m_properties[name] = binding;
}

void BindingContext::ExposeConstant(const std::string& name, const nlohmann::json& value) {
    m_constants[name] = value;
}

// UIBinding Implementation
UIBinding::UIBinding() = default;
UIBinding::~UIBinding() { Shutdown(); }

void UIBinding::Initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) return;

    // Create default binding contexts
    CreateContext("Engine");
    CreateContext("Game");
    CreateContext("UI");

    // Expose default Engine bindings
    auto* engineCtx = GetContext("Engine");
    if (engineCtx) {
        engineCtx->ExposeFunction("log", [](const nlohmann::json& args) -> nlohmann::json {
            if (args.contains("message")) {
                // Log to console
                printf("[UI] %s\n", args["message"].get<std::string>().c_str());
            }
            return nullptr;
        }, "Log a message to the console", {"message"});

        engineCtx->ExposeFunction("getTime", [](const nlohmann::json&) -> nlohmann::json {
            // Return current time
            return static_cast<double>(clock()) / CLOCKS_PER_SEC;
        }, "Get current engine time in seconds");
    }

    m_initialized = true;
}

void UIBinding::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_functions.clear();
    m_properties.clear();
    m_contexts.clear();
    m_eventHandlers.clear();
    m_pendingCalls.clear();
    m_batchedEvents.clear();

    m_initialized = false;
}

void UIBinding::ExposeFunction(const std::string& name,
                              std::function<nlohmann::json(const nlohmann::json&)> fn) {
    ExposeFunction(name, fn, "", {});
}

void UIBinding::ExposeFunction(const std::string& name,
                              std::function<nlohmann::json(const nlohmann::json&)> fn,
                              const std::string& description,
                              const std::vector<std::string>& paramNames) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string funcName = name.substr(dotPos + 1);

        auto* context = GetContext(contextName);
        if (!context) {
            context = CreateContext(contextName);
        }
        context->ExposeFunction(funcName, fn, description, paramNames);
        return;
    }

    FunctionBinding binding;
    binding.name = name;
    binding.handler = fn;
    binding.description = description;
    binding.parameterNames = paramNames;
    m_functions[name] = binding;
}

void UIBinding::RemoveFunction(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_functions.erase(name);
}

bool UIBinding::HasFunction(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check global functions
    if (m_functions.find(name) != m_functions.end()) {
        return true;
    }

    // Check namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string funcName = name.substr(dotPos + 1);

        auto it = m_contexts.find(contextName);
        if (it != m_contexts.end()) {
            return it->second->GetFunctions().find(funcName) != it->second->GetFunctions().end();
        }
    }

    return false;
}

nlohmann::json UIBinding::CallFunction(const std::string& name, const nlohmann::json& args) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check global functions
    auto it = m_functions.find(name);
    if (it != m_functions.end()) {
        return it->second.handler(args);
    }

    // Check namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string funcName = name.substr(dotPos + 1);

        auto ctxIt = m_contexts.find(contextName);
        if (ctxIt != m_contexts.end()) {
            auto& funcs = ctxIt->second->GetFunctions();
            auto funcIt = funcs.find(funcName);
            if (funcIt != funcs.end()) {
                return funcIt->second.handler(args);
            }
        }
    }

    return nullptr;
}

void UIBinding::ExposeProperty(const std::string& name,
                              std::function<nlohmann::json()> getter,
                              std::function<void(const nlohmann::json&)> setter) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string propName = name.substr(dotPos + 1);

        auto* context = GetContext(contextName);
        if (!context) {
            context = CreateContext(contextName);
        }
        context->ExposeProperty(propName, getter, setter);
        return;
    }

    PropertyBinding binding;
    binding.name = name;
    binding.getter = getter;
    binding.setter = setter;
    binding.readOnly = (setter == nullptr);
    m_properties[name] = binding;
}

void UIBinding::RemoveProperty(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_properties.erase(name);
}

nlohmann::json UIBinding::GetProperty(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check global properties
    auto it = m_properties.find(name);
    if (it != m_properties.end() && it->second.getter) {
        return it->second.getter();
    }

    // Check namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string propName = name.substr(dotPos + 1);

        auto ctxIt = m_contexts.find(contextName);
        if (ctxIt != m_contexts.end()) {
            auto& props = ctxIt->second->GetProperties();
            auto propIt = props.find(propName);
            if (propIt != props.end() && propIt->second.getter) {
                return propIt->second.getter();
            }
        }
    }

    return nullptr;
}

bool UIBinding::SetProperty(const std::string& name, const nlohmann::json& value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check global properties
    auto it = m_properties.find(name);
    if (it != m_properties.end()) {
        if (it->second.readOnly || !it->second.setter) {
            return false;
        }
        it->second.setter(value);
        return true;
    }

    // Check namespaced
    size_t dotPos = name.find('.');
    if (dotPos != std::string::npos) {
        std::string contextName = name.substr(0, dotPos);
        std::string propName = name.substr(dotPos + 1);

        auto ctxIt = m_contexts.find(contextName);
        if (ctxIt != m_contexts.end()) {
            auto& props = ctxIt->second->GetProperties();
            auto propIt = props.find(propName);
            if (propIt != props.end()) {
                if (propIt->second.readOnly || !propIt->second.setter) {
                    return false;
                }
                propIt->second.setter(value);
                return true;
            }
        }
    }

    return false;
}

nlohmann::json UIBinding::CallJS(const std::string& function, const nlohmann::json& args) {
    // In a real implementation, this would call into a JavaScript engine
    // For now, we return a placeholder

    // Build call string for logging
    std::stringstream ss;
    ss << function << "(";
    if (!args.is_null()) {
        ss << args.dump();
    }
    ss << ")";

    // Return null as placeholder
    return nullptr;
}

void UIBinding::CallJSAsync(const std::string& function, const nlohmann::json& args,
                           std::function<void(const JSResult&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    PendingJSCall call;
    call.function = function;
    call.args = args;
    call.callback = callback;
    call.callId = m_nextCallId++;

    m_pendingCalls.push_back(call);

    // In a real implementation, this would queue the call for the JS thread
    // For now, we immediately process
    ProcessPendingCalls();
}

JSResult UIBinding::ExecuteJS(const std::string& code) {
    JSResult result;

    // In a real implementation, this would execute JS code
    // For now, return success
    result.success = true;
    result.value = nullptr;

    return result;
}

void UIBinding::ExecuteJSAsync(const std::string& code,
                              std::function<void(const JSResult&)> callback) {
    // Queue for async execution
    JSResult result = ExecuteJS(code);
    if (callback) {
        callback(result);
    }
}

int UIBinding::OnUIEvent(const std::string& event,
                        std::function<void(const nlohmann::json&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    EventSubscription sub;
    sub.event = event;
    sub.handler = handler;
    sub.once = false;

    int id = m_nextSubscriptionId++;
    m_eventHandlers[event].push_back(sub);

    return id;
}

int UIBinding::OnceUIEvent(const std::string& event,
                          std::function<void(const nlohmann::json&)> handler) {
    std::lock_guard<std::mutex> lock(m_mutex);

    EventSubscription sub;
    sub.event = event;
    sub.handler = handler;
    sub.once = true;

    int id = m_nextSubscriptionId++;
    m_eventHandlers[event].push_back(sub);

    return id;
}

void UIBinding::OffUIEvent(int subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove subscription by ID
    // Note: In a real implementation, we'd track subscription IDs properly
}

void UIBinding::OffUIEvent(const std::string& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHandlers.erase(event);
}

void UIBinding::EmitEvent(const std::string& event, const nlohmann::json& data) {
    if (m_batchDepth > 0) {
        m_batchedEvents.emplace_back(event, data);
        return;
    }

    // Call JavaScript event handlers
    CallJS("Engine.emit", {{"event", event}, {"data", data}});
}

void UIBinding::HandleJSEvent(const std::string& event, const nlohmann::json& data) {
    std::vector<EventSubscription> handlers;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_eventHandlers.find(event);
        if (it != m_eventHandlers.end()) {
            handlers = it->second;
        }
    }

    // Call handlers
    for (auto& handler : handlers) {
        handler.handler(data);
    }

    // Remove one-time handlers
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_eventHandlers.find(event);
        if (it != m_eventHandlers.end()) {
            it->second.erase(
                std::remove_if(it->second.begin(), it->second.end(),
                    [](const EventSubscription& sub) { return sub.once; }),
                it->second.end());
        }
    }
}

BindingContext* UIBinding::CreateContext(const std::string& name) {
    auto context = std::make_unique<BindingContext>(name);
    BindingContext* ptr = context.get();
    m_contexts[name] = std::move(context);
    return ptr;
}

BindingContext* UIBinding::GetContext(const std::string& name) {
    auto it = m_contexts.find(name);
    if (it != m_contexts.end()) {
        return it->second.get();
    }
    return nullptr;
}

void UIBinding::RemoveContext(const std::string& name) {
    m_contexts.erase(name);
}

void UIBinding::BeginBatch() {
    m_batchDepth++;
}

void UIBinding::EndBatch() {
    if (m_batchDepth > 0) {
        m_batchDepth--;

        if (m_batchDepth == 0) {
            FlushBatchedEvents();
        }
    }
}

std::vector<std::string> UIBinding::GetExposedFunctions() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;

    for (const auto& [name, _] : m_functions) {
        names.push_back(name);
    }

    for (const auto& [ctxName, context] : m_contexts) {
        for (const auto& [funcName, _] : context->GetFunctions()) {
            names.push_back(ctxName + "." + funcName);
        }
    }

    return names;
}

std::vector<std::string> UIBinding::GetExposedProperties() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> names;

    for (const auto& [name, _] : m_properties) {
        names.push_back(name);
    }

    for (const auto& [ctxName, context] : m_contexts) {
        for (const auto& [propName, _] : context->GetProperties()) {
            names.push_back(ctxName + "." + propName);
        }
    }

    return names;
}

nlohmann::json UIBinding::GetDocumentation() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    nlohmann::json doc;

    // Document functions
    nlohmann::json functions = nlohmann::json::array();
    for (const auto& [name, binding] : m_functions) {
        nlohmann::json func;
        func["name"] = name;
        func["description"] = binding.description;
        func["parameters"] = binding.parameterNames;
        functions.push_back(func);
    }
    doc["functions"] = functions;

    // Document properties
    nlohmann::json properties = nlohmann::json::array();
    for (const auto& [name, binding] : m_properties) {
        nlohmann::json prop;
        prop["name"] = name;
        prop["readOnly"] = binding.readOnly;
        properties.push_back(prop);
    }
    doc["properties"] = properties;

    // Document contexts
    nlohmann::json contexts = nlohmann::json::object();
    for (const auto& [ctxName, context] : m_contexts) {
        nlohmann::json ctx;

        nlohmann::json ctxFuncs = nlohmann::json::array();
        for (const auto& [funcName, binding] : context->GetFunctions()) {
            nlohmann::json func;
            func["name"] = funcName;
            func["description"] = binding.description;
            func["parameters"] = binding.parameterNames;
            ctxFuncs.push_back(func);
        }
        ctx["functions"] = ctxFuncs;

        nlohmann::json ctxProps = nlohmann::json::array();
        for (const auto& [propName, binding] : context->GetProperties()) {
            nlohmann::json prop;
            prop["name"] = propName;
            prop["readOnly"] = binding.readOnly;
            ctxProps.push_back(prop);
        }
        ctx["properties"] = ctxProps;

        ctx["constants"] = context->GetConstants();

        contexts[ctxName] = ctx;
    }
    doc["contexts"] = contexts;

    return doc;
}

void UIBinding::ProcessPendingCalls() {
    std::vector<PendingJSCall> calls;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        calls = std::move(m_pendingCalls);
        m_pendingCalls.clear();
    }

    for (auto& call : calls) {
        JSResult result;
        result.success = true;
        result.value = CallJS(call.function, call.args);

        if (call.callback) {
            call.callback(result);
        }
    }
}

void UIBinding::FlushBatchedEvents() {
    std::vector<std::pair<std::string, nlohmann::json>> events;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        events = std::move(m_batchedEvents);
        m_batchedEvents.clear();
    }

    for (const auto& [event, data] : events) {
        EmitEvent(event, data);
    }
}

} // namespace UI
} // namespace Engine
