#include "JSBridge.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace Vehement {
namespace WebEditor {

// ============================================================================
// JSValue Implementation
// ============================================================================

std::string JSValue::ToJson() const {
    return JSON::Stringify(*this);
}

JSValue JSValue::FromJson(const std::string& json) {
    return JSON::Parse(json);
}

// ============================================================================
// JSON Namespace Implementation
// ============================================================================

namespace JSON {

std::string EscapeString(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b";  break;
            case '\f': oss << "\\f";  break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

std::string UnescapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"';  ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'b':  result += '\b'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'u':
                    if (i + 5 < str.size()) {
                        // Parse 4 hex digits
                        int codepoint = std::stoi(str.substr(i + 2, 4), nullptr, 16);
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else {
                            // UTF-8 encoding would go here
                            result += '?';
                        }
                        i += 5;
                    }
                    break;
                default:
                    result += str[i];
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

std::string Stringify(const JSValue& value, bool pretty) {
    std::string indent;
    std::function<std::string(const JSValue&, int)> stringify;

    stringify = [&](const JSValue& v, int depth) -> std::string {
        std::string ws = pretty ? std::string(depth * 2, ' ') : "";
        std::string nl = pretty ? "\n" : "";
        std::string sp = pretty ? " " : "";

        if (v.IsNull()) {
            return "null";
        }
        else if (v.IsBool()) {
            return v.AsBool() ? "true" : "false";
        }
        else if (v.IsNumber()) {
            double num = v.AsNumber();
            if (std::isnan(num) || std::isinf(num)) {
                return "null";
            }
            // Check if it's a whole number
            if (num == std::floor(num) && std::abs(num) < 1e15) {
                return std::to_string(static_cast<int64_t>(num));
            }
            std::ostringstream oss;
            oss << std::setprecision(15) << num;
            return oss.str();
        }
        else if (v.IsString()) {
            return "\"" + EscapeString(v.AsString()) + "\"";
        }
        else if (v.IsArray()) {
            const auto& arr = v.AsArray();
            if (arr.empty()) return "[]";

            std::ostringstream oss;
            oss << "[" << nl;
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) oss << "," << nl;
                oss << ws << "  " << stringify(arr[i], depth + 1);
            }
            oss << nl << ws << "]";
            return oss.str();
        }
        else if (v.IsObject()) {
            const auto& obj = v.AsObject();
            if (obj.empty()) return "{}";

            std::ostringstream oss;
            oss << "{" << nl;
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) oss << "," << nl;
                first = false;
                oss << ws << "  " << "\"" << EscapeString(key) << "\":" << sp
                    << stringify(val, depth + 1);
            }
            oss << nl << ws << "}";
            return oss.str();
        }

        return "null";
    };

    return stringify(value, 0);
}

JSValue Parse(const std::string& json) {
    size_t pos = 0;

    auto skipWhitespace = [&]() {
        while (pos < json.size() && std::isspace(json[pos])) ++pos;
    };

    std::function<JSValue()> parseValue;

    auto parseString = [&]() -> std::string {
        if (json[pos] != '"') return "";
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'u':
                        if (pos + 4 < json.size()) {
                            int cp = std::stoi(json.substr(pos + 1, 4), nullptr, 16);
                            result += static_cast<char>(cp);
                            pos += 4;
                        }
                        break;
                    default:
                        result += json[pos];
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        if (pos < json.size()) ++pos;  // Skip closing quote
        return result;
    };

    auto parseNumber = [&]() -> double {
        size_t start = pos;
        if (json[pos] == '-') ++pos;
        while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        if (pos < json.size() && json[pos] == '.') {
            ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) ++pos;
        }
        return std::stod(json.substr(start, pos - start));
    };

    parseValue = [&]() -> JSValue {
        skipWhitespace();
        if (pos >= json.size()) return {};

        char c = json[pos];

        if (c == 'n' && json.substr(pos, 4) == "null") {
            pos += 4;
            return {};
        }
        else if (c == 't' && json.substr(pos, 4) == "true") {
            pos += 4;
            return true;
        }
        else if (c == 'f' && json.substr(pos, 5) == "false") {
            pos += 5;
            return false;
        }
        else if (c == '"') {
            return parseString();
        }
        else if (c == '-' || std::isdigit(c)) {
            return parseNumber();
        }
        else if (c == '[') {
            ++pos;
            JSValue::Array arr;
            skipWhitespace();
            if (pos < json.size() && json[pos] == ']') {
                ++pos;
                return arr;
            }
            while (pos < json.size()) {
                arr.push_back(parseValue());
                skipWhitespace();
                if (pos >= json.size()) break;
                if (json[pos] == ']') { ++pos; break; }
                if (json[pos] == ',') ++pos;
            }
            return arr;
        }
        else if (c == '{') {
            ++pos;
            JSValue::Object obj;
            skipWhitespace();
            if (pos < json.size() && json[pos] == '}') {
                ++pos;
                return obj;
            }
            while (pos < json.size()) {
                skipWhitespace();
                if (json[pos] != '"') break;
                std::string key = parseString();
                skipWhitespace();
                if (pos < json.size() && json[pos] == ':') ++pos;
                obj[key] = parseValue();
                skipWhitespace();
                if (pos >= json.size()) break;
                if (json[pos] == '}') { ++pos; break; }
                if (json[pos] == ',') ++pos;
            }
            return obj;
        }

        return {};
    };

    return parseValue();
}

} // namespace JSON

// ============================================================================
// JSBridge Implementation
// ============================================================================

JSBridge::JSBridge() = default;
JSBridge::~JSBridge() = default;

void JSBridge::RegisterFunction(const std::string& name, NativeFunction func) {
    m_functions[name] = std::move(func);
}

void JSBridge::UnregisterFunction(const std::string& name) {
    m_functions.erase(name);
}

bool JSBridge::HasFunction(const std::string& name) const {
    return m_functions.find(name) != m_functions.end();
}

std::vector<std::string> JSBridge::GetRegisteredFunctions() const {
    std::vector<std::string> names;
    names.reserve(m_functions.size());
    for (const auto& [name, func] : m_functions) {
        names.push_back(name);
    }
    return names;
}

void JSBridge::CallJS(const std::string& functionName,
                      const std::vector<JSValue>& args,
                      JSCallback callback) {
    std::ostringstream script;
    script << functionName << "(";

    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) script << ",";
        script << args[i].ToJson();
    }
    script << ")";

    ExecuteScript(script.str(), std::move(callback));
}

void JSBridge::ExecuteScript(const std::string& script, JSCallback callback) {
    if (m_scriptExecutor) {
        m_scriptExecutor(script, std::move(callback));
    } else if (callback) {
        callback(JSResult::Error("No script executor set"));
    }
}

void JSBridge::Evaluate(const std::string& expression, JSCallback callback) {
    ExecuteScript(expression, std::move(callback));
}

void JSBridge::SendMessage(const std::string& type, const JSValue& data) {
    std::string script = "WebEditor._handleMessage('" + type + "'," + data.ToJson() + ");";
    ExecuteScript(script, nullptr);
}

void JSBridge::OnMessage(const std::string& type,
                         std::function<void(const JSValue&)> handler) {
    m_messageHandlers[type] = std::move(handler);
}

void JSBridge::OffMessage(const std::string& type) {
    m_messageHandlers.erase(type);
}

void JSBridge::EmitEvent(const std::string& eventName, const JSValue& data) {
    JSValue::Object event;
    event["type"] = eventName;
    event["data"] = data;
    event["timestamp"] = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );

    std::string script = "window.dispatchEvent(new CustomEvent('webeditor-" +
                         eventName + "',{detail:" + JSValue(event).ToJson() + "}));";
    ExecuteScript(script, nullptr);
}

void JSBridge::OnEvent(const std::string& eventName,
                       std::function<void(const JSValue&)> handler) {
    m_eventHandlers[eventName].push_back(std::move(handler));
}

void JSBridge::ProcessPending() {
    // Process incoming messages
    std::queue<std::pair<std::string, JSValue>> messages;
    {
        std::lock_guard<std::mutex> lock(m_messageMutex);
        std::swap(messages, m_incomingMessages);
    }

    while (!messages.empty()) {
        auto [type, data] = std::move(messages.front());
        messages.pop();

        auto handlerIt = m_messageHandlers.find(type);
        if (handlerIt != m_messageHandlers.end()) {
            handlerIt->second(data);
        }
    }
}

void JSBridge::HandleIncomingMessage(const std::string& jsonMessage) {
    JSValue msg = JSON::Parse(jsonMessage);
    if (!msg.IsObject()) return;

    std::string type = msg["type"].GetString();
    JSValue payload = msg["payload"];

    if (type == "invoke") {
        // Handle function invocation
        std::string funcName = payload["function"].GetString();
        uint64_t callbackId = static_cast<uint64_t>(payload["id"].GetNumber());

        std::vector<JSValue> args;
        if (payload["args"].IsArray()) {
            args = payload["args"].AsArray();
        }

        JSResult result = InvokeNativeFunction(funcName, args);
        DeliverResult(callbackId, result);
    }
    else if (type == "event") {
        // Handle event
        std::string eventName = payload["event"].GetString();
        JSValue eventData = payload["data"];

        auto handlersIt = m_eventHandlers.find(eventName);
        if (handlersIt != m_eventHandlers.end()) {
            for (const auto& handler : handlersIt->second) {
                handler(eventData);
            }
        }
    }
    else {
        // Queue for regular message handling
        std::lock_guard<std::mutex> lock(m_messageMutex);
        m_incomingMessages.emplace(type, payload);
    }
}

JSResult JSBridge::InvokeNativeFunction(const std::string& name,
                                         const std::vector<JSValue>& args) {
    auto it = m_functions.find(name);
    if (it == m_functions.end()) {
        return JSResult::Error("Function not found: " + name);
    }

    try {
        return it->second(args);
    } catch (const std::exception& e) {
        return JSResult::Error(std::string("Exception: ") + e.what());
    }
}

void JSBridge::DeliverResult(uint64_t callbackId, const JSResult& result) {
    std::ostringstream script;
    script << "WebEditor._handleResult(" << callbackId << ",";

    if (result.success) {
        script << result.value.ToJson() << ",null);";
    } else {
        script << "null,\"" << JSON::EscapeString(result.error) << "\");";
    }

    ExecuteScript(script.str(), nullptr);
}

std::string JSBridge::GenerateBridgeScript() const {
    std::ostringstream script;

    script << R"(
// Auto-generated bridge functions
(function() {
    var functions = [)";

    bool first = true;
    for (const auto& [name, func] : m_functions) {
        if (!first) script << ",";
        first = false;
        script << "'" << name << "'";
    }

    script << R"(];

    functions.forEach(function(name) {
        var parts = name.split('.');
        var obj = window;
        for (var i = 0; i < parts.length - 1; i++) {
            obj[parts[i]] = obj[parts[i]] || {};
            obj = obj[parts[i]];
        }
        var funcName = parts[parts.length - 1];
        obj[funcName] = function() {
            var args = Array.prototype.slice.call(arguments);
            var callback = typeof args[args.length - 1] === 'function' ? args.pop() : null;
            return new Promise(function(resolve, reject) {
                WebEditor.invoke(name, args, function(err, result) {
                    if (callback) callback(err, result);
                    if (err) reject(err);
                    else resolve(result);
                });
            });
        };
    });
})();
)";

    return script.str();
}

// ============================================================================
// Global Bridge
// ============================================================================

JSBridge& GetGlobalBridge() {
    static JSBridge instance;
    return instance;
}

} // namespace WebEditor
} // namespace Vehement
