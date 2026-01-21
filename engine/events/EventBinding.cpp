#include "EventBinding.hpp"
#include <sstream>
#include "../core/json_config.hpp"

namespace Nova {
namespace Events {

// ============================================================================
// CallbackType Utilities
// ============================================================================

const char* CallbackTypeToString(CallbackType type) {
    switch (type) {
        case CallbackType::Python: return "python";
        case CallbackType::Native: return "native";
        case CallbackType::Event: return "event";
        case CallbackType::Command: return "command";
        case CallbackType::Script: return "script";
        default: return "unknown";
    }
}

std::optional<CallbackType> CallbackTypeFromString(const std::string& str) {
    if (str == "python") return CallbackType::Python;
    if (str == "native") return CallbackType::Native;
    if (str == "event") return CallbackType::Event;
    if (str == "command") return CallbackType::Command;
    if (str == "script") return CallbackType::Script;
    return std::nullopt;
}

// ============================================================================
// EventBinding Serialization
// ============================================================================

json EventBinding::ToJson() const {
    json j;

    // Basic info
    j["id"] = id;
    if (!name.empty()) j["name"] = name;
    if (!description.empty()) j["description"] = description;
    if (!category.empty()) j["category"] = category;

    // Condition
    j["condition"] = condition.ToJson();

    // Callback type and settings
    j["callbackType"] = CallbackTypeToString(callbackType);

    switch (callbackType) {
        case CallbackType::Python:
            if (!pythonScript.empty()) j["pythonScript"] = pythonScript;
            if (!pythonFile.empty()) j["pythonFile"] = pythonFile;
            if (!pythonModule.empty()) j["pythonModule"] = pythonModule;
            if (!pythonFunction.empty()) j["pythonFunction"] = pythonFunction;
            break;

        case CallbackType::Event:
            j["emitEventType"] = emitEventType;
            // Note: emitEventData would need special handling for std::any
            break;

        case CallbackType::Command:
            j["command"] = command;
            if (!commandArgs.empty()) j["commandArgs"] = commandArgs;
            break;

        case CallbackType::Script:
            if (!pythonFile.empty()) j["scriptFile"] = pythonFile;
            break;

        case CallbackType::Native:
            // Native callbacks can't be serialized
            break;
    }

    // Parameters
    if (!parameters.empty() && !parameters.is_null()) {
        j["parameters"] = parameters;
    }

    // Execution settings
    j["enabled"] = enabled;
    if (priority != 0) j["priority"] = priority;
    if (async) j["async"] = true;
    if (delay > 0.0f) j["delay"] = delay;
    if (cooldown > 0.0f) j["cooldown"] = cooldown;
    if (maxExecutions > 0) j["maxExecutions"] = maxExecutions;
    if (oneShot) j["oneShot"] = true;

    // Debugging
    if (logExecution) j["logExecution"] = true;
    if (breakOnExecute) j["breakOnExecute"] = true;

    // Metadata
    if (!author.empty()) j["author"] = author;
    if (!version.empty()) j["version"] = version;
    if (!tags.empty()) j["tags"] = tags;

    return j;
}

EventBinding EventBinding::FromJson(const json& j) {
    EventBinding binding;

    // Basic info
    if (j.contains("id")) binding.id = j["id"].get<std::string>();
    if (j.contains("name")) binding.name = j["name"].get<std::string>();
    if (j.contains("description")) binding.description = j["description"].get<std::string>();
    if (j.contains("category")) binding.category = j["category"].get<std::string>();

    // Condition
    if (j.contains("condition")) {
        binding.condition = EventCondition::FromJson(j["condition"]);
    }

    // Callback type and settings
    if (j.contains("callbackType")) {
        auto type = CallbackTypeFromString(j["callbackType"].get<std::string>());
        if (type) binding.callbackType = *type;
    }

    if (j.contains("pythonScript")) binding.pythonScript = j["pythonScript"].get<std::string>();
    if (j.contains("pythonFile")) binding.pythonFile = j["pythonFile"].get<std::string>();
    if (j.contains("pythonModule")) binding.pythonModule = j["pythonModule"].get<std::string>();
    if (j.contains("pythonFunction")) binding.pythonFunction = j["pythonFunction"].get<std::string>();
    if (j.contains("emitEventType")) binding.emitEventType = j["emitEventType"].get<std::string>();
    if (j.contains("command")) binding.command = j["command"].get<std::string>();
    if (j.contains("commandArgs")) binding.commandArgs = j["commandArgs"].get<std::vector<std::string>>();
    if (j.contains("scriptFile")) binding.pythonFile = j["scriptFile"].get<std::string>();

    // Parameters
    if (j.contains("parameters")) {
        binding.parameters = j["parameters"];
    }

    // Execution settings
    if (j.contains("enabled")) binding.enabled = j["enabled"].get<bool>();
    if (j.contains("priority")) binding.priority = j["priority"].get<int>();
    if (j.contains("async")) binding.async = j["async"].get<bool>();
    if (j.contains("delay")) binding.delay = j["delay"].get<float>();
    if (j.contains("cooldown")) binding.cooldown = j["cooldown"].get<float>();
    if (j.contains("maxExecutions")) binding.maxExecutions = j["maxExecutions"].get<int>();
    if (j.contains("oneShot")) binding.oneShot = j["oneShot"].get<bool>();

    // Debugging
    if (j.contains("logExecution")) binding.logExecution = j["logExecution"].get<bool>();
    if (j.contains("breakOnExecute")) binding.breakOnExecute = j["breakOnExecute"].get<bool>();

    // Metadata
    if (j.contains("author")) binding.author = j["author"].get<std::string>();
    if (j.contains("version")) binding.version = j["version"].get<std::string>();
    if (j.contains("tags")) binding.tags = j["tags"].get<std::vector<std::string>>();

    binding.modifiedAt = std::chrono::system_clock::now();

    return binding;
}

std::string EventBinding::Validate() const {
    std::ostringstream errors;

    if (id.empty()) {
        errors << "Binding must have an ID. ";
    }

    if (condition.eventName.empty() && condition.sourceType == "*") {
        errors << "Condition must specify either an event name or source type. ";
    }

    switch (callbackType) {
        case CallbackType::Python:
            if (pythonScript.empty() && pythonFile.empty() && pythonFunction.empty()) {
                errors << "Python binding must specify script, file, or function. ";
            }
            if (!pythonFunction.empty() && pythonModule.empty()) {
                errors << "Python function requires a module. ";
            }
            break;

        case CallbackType::Event:
            if (emitEventType.empty()) {
                errors << "Event binding must specify event type to emit. ";
            }
            break;

        case CallbackType::Command:
            if (command.empty()) {
                errors << "Command binding must specify a command. ";
            }
            break;

        case CallbackType::Native:
            if (!nativeCallback) {
                errors << "Native binding must have a callback function. ";
            }
            break;

        case CallbackType::Script:
            if (pythonFile.empty()) {
                errors << "Script binding must specify a script file. ";
            }
            break;
    }

    if (cooldown < 0.0f) {
        errors << "Cooldown cannot be negative. ";
    }

    if (delay < 0.0f) {
        errors << "Delay cannot be negative. ";
    }

    return errors.str();
}

// ============================================================================
// BindingGroup Serialization
// ============================================================================

json BindingGroup::ToJson() const {
    json j;

    j["id"] = id;
    if (!name.empty()) j["name"] = name;
    if (!description.empty()) j["description"] = description;
    if (!category.empty()) j["category"] = category;
    j["enabled"] = enabled;

    json bindingsArray = json::array();
    for (const auto& binding : bindings) {
        bindingsArray.push_back(binding.ToJson());
    }
    j["bindings"] = bindingsArray;

    return j;
}

BindingGroup BindingGroup::FromJson(const json& j) {
    BindingGroup group;

    if (j.contains("id")) group.id = j["id"].get<std::string>();
    if (j.contains("name")) group.name = j["name"].get<std::string>();
    if (j.contains("description")) group.description = j["description"].get<std::string>();
    if (j.contains("category")) group.category = j["category"].get<std::string>();
    if (j.contains("enabled")) group.enabled = j["enabled"].get<bool>();

    if (j.contains("bindings")) {
        for (const auto& bindingJson : j["bindings"]) {
            group.bindings.push_back(EventBinding::FromJson(bindingJson));
        }
    }

    return group;
}

} // namespace Events
} // namespace Nova
