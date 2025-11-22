#include "AnimationConfig.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <sys/stat.h>

namespace Nova {

Platform AnimationConfig::s_currentPlatform = Platform::All;

// ============================================================================
// ValidationError
// ============================================================================

std::string ValidationError::ToString() const {
    std::string prefix = isWarning ? "[WARNING]" : "[ERROR]";
    return prefix + " " + path + ": " + message + " (rule: " + schemaRule + ")";
}

std::string ValidationResult::GetErrorSummary() const {
    std::stringstream ss;

    if (!valid) {
        ss << "Validation failed with " << errors.size() << " error(s):\n";
        for (const auto& error : errors) {
            ss << "  " << error.ToString() << "\n";
        }
    }

    if (!warnings.empty()) {
        ss << warnings.size() << " warning(s):\n";
        for (const auto& warning : warnings) {
            ss << "  " << warning.ToString() << "\n";
        }
    }

    return ss.str();
}

// ============================================================================
// AnimationSchemaValidator
// ============================================================================

bool AnimationSchemaValidator::LoadSchema(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        json schema;
        file >> schema;
        return LoadSchemaFromJson(schema);
    } catch (...) {
        return false;
    }
}

bool AnimationSchemaValidator::LoadSchemaFromJson(const json& schema) {
    m_schema = schema;
    return true;
}

ValidationResult AnimationSchemaValidator::Validate(const json& config) const {
    ValidationResult result;

    if (m_schema.is_null()) {
        result.valid = true;
        return result;
    }

    ValidateNode(config, m_schema, "", result);
    return result;
}

ValidationResult AnimationSchemaValidator::ValidateFile(const std::string& filepath) const {
    ValidationResult result;

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.valid = false;
            result.errors.push_back({filepath, "Could not open file", "file_exists"});
            return result;
        }

        json config;
        file >> config;
        return Validate(config);
    } catch (const std::exception& e) {
        result.valid = false;
        result.errors.push_back({filepath, std::string("JSON parse error: ") + e.what(), "valid_json"});
        return result;
    }
}

void AnimationSchemaValidator::ValidateNode(const json& config, const json& schema,
                                             const std::string& path, ValidationResult& result) const {
    // Check type
    if (schema.contains("type")) {
        std::string expectedType = schema["type"].get<std::string>();
        ValidateType(config, expectedType, path, result);
    }

    // Check enum
    if (schema.contains("enum")) {
        ValidateEnum(config, schema["enum"], path, result);
    }

    // Check pattern
    if (schema.contains("pattern") && config.is_string()) {
        ValidatePattern(config.get<std::string>(), schema["pattern"].get<std::string>(), path, result);
    }

    // Check required properties
    if (schema.contains("required") && schema["required"].is_array() && config.is_object()) {
        for (const auto& reqProp : schema["required"]) {
            std::string propName = reqProp.get<std::string>();
            if (!config.contains(propName)) {
                result.valid = false;
                result.errors.push_back({
                    path.empty() ? propName : path + "." + propName,
                    "Required property is missing",
                    "required"
                });
            }
        }
    }

    // Validate properties
    if (schema.contains("properties") && config.is_object()) {
        for (const auto& [propName, propSchema] : schema["properties"].items()) {
            if (config.contains(propName)) {
                std::string propPath = path.empty() ? propName : path + "." + propName;
                ValidateNode(config[propName], propSchema, propPath, result);
            }
        }
    }

    // Validate array items
    if (schema.contains("items") && config.is_array()) {
        for (size_t i = 0; i < config.size(); ++i) {
            std::string itemPath = path + "[" + std::to_string(i) + "]";
            ValidateNode(config[i], schema["items"], itemPath, result);
        }
    }

    // Check minimum/maximum for numbers
    if (config.is_number()) {
        if (schema.contains("minimum")) {
            float minVal = schema["minimum"].get<float>();
            if (config.get<float>() < minVal) {
                result.valid = false;
                result.errors.push_back({
                    path,
                    "Value " + std::to_string(config.get<float>()) + " is less than minimum " + std::to_string(minVal),
                    "minimum"
                });
            }
        }
        if (schema.contains("maximum")) {
            float maxVal = schema["maximum"].get<float>();
            if (config.get<float>() > maxVal) {
                result.valid = false;
                result.errors.push_back({
                    path,
                    "Value " + std::to_string(config.get<float>()) + " is greater than maximum " + std::to_string(maxVal),
                    "maximum"
                });
            }
        }
    }
}

void AnimationSchemaValidator::ValidateType(const json& value, const std::string& expectedType,
                                             const std::string& path, ValidationResult& result) const {
    bool valid = false;

    if (expectedType == "string") valid = value.is_string();
    else if (expectedType == "number") valid = value.is_number();
    else if (expectedType == "integer") valid = value.is_number_integer();
    else if (expectedType == "boolean") valid = value.is_boolean();
    else if (expectedType == "array") valid = value.is_array();
    else if (expectedType == "object") valid = value.is_object();
    else if (expectedType == "null") valid = value.is_null();

    if (!valid) {
        result.valid = false;
        result.errors.push_back({
            path,
            "Expected type '" + expectedType + "'",
            "type"
        });
    }
}

void AnimationSchemaValidator::ValidateEnum(const json& value, const json& enumValues,
                                             const std::string& path, ValidationResult& result) const {
    bool found = false;
    for (const auto& enumVal : enumValues) {
        if (value == enumVal) {
            found = true;
            break;
        }
    }

    if (!found) {
        result.valid = false;
        result.errors.push_back({
            path,
            "Value is not one of the allowed enum values",
            "enum"
        });
    }
}

void AnimationSchemaValidator::ValidatePattern(const std::string& value, const std::string& pattern,
                                                const std::string& path, ValidationResult& result) const {
    try {
        std::regex re(pattern);
        if (!std::regex_match(value, re)) {
            result.valid = false;
            result.errors.push_back({
                path,
                "Value does not match pattern '" + pattern + "'",
                "pattern"
            });
        }
    } catch (...) {
        // Invalid regex pattern - skip validation
    }
}

// ============================================================================
// AnimationConfig
// ============================================================================

AnimationConfig::AnimationConfig(const std::string& filepath) {
    LoadFromFile(filepath);
}

bool AnimationConfig::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        json config;
        file >> config;

        m_filepath = filepath;
        m_lastModified = GetFileModifiedTime(filepath);

        return LoadFromJson(config);
    } catch (...) {
        return false;
    }
}

bool AnimationConfig::LoadFromJson(const json& config) {
    try {
        m_data = config;
        m_id = config.value("id", "");
        m_name = config.value("name", "");
        m_basePath = config.value("extends", "");

        // Extract platform overrides
        if (config.contains("platformOverrides")) {
            m_platformOverrides = config["platformOverrides"];
        }

        m_loaded = true;
        return true;
    } catch (...) {
        m_loaded = false;
        return false;
    }
}

bool AnimationConfig::SaveToFile(const std::string& filepath) const {
    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << m_data.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool AnimationConfig::SaveToFile() const {
    if (m_filepath.empty()) {
        return false;
    }
    return SaveToFile(m_filepath);
}

json AnimationConfig::ToJson() const {
    return m_data;
}

bool AnimationConfig::Reload() {
    if (m_filepath.empty()) {
        return false;
    }

    if (!LoadFromFile(m_filepath)) {
        return false;
    }

    // Notify callbacks
    for (const auto& callback : m_reloadCallbacks) {
        callback(*this);
    }

    return true;
}

void AnimationConfig::SetHotReload(bool enable) {
    m_hotReloadEnabled = enable;
}

void AnimationConfig::CheckAndReload() {
    if (!m_hotReloadEnabled || m_filepath.empty()) {
        return;
    }

    std::time_t currentModified = GetFileModifiedTime(m_filepath);
    if (currentModified > m_lastModified) {
        Reload();
    }
}

void AnimationConfig::OnReload(ReloadCallback callback) {
    m_reloadCallbacks.push_back(std::move(callback));
}

json AnimationConfig::GetValue(const std::string& path) const {
    if (path.empty()) {
        return m_data;
    }

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    const json* current = &m_data;
    for (const auto& p : parts) {
        // Handle array index
        if (p.front() == '[' && p.back() == ']') {
            size_t index = std::stoul(p.substr(1, p.size() - 2));
            if (current->is_array() && index < current->size()) {
                current = &(*current)[index];
            } else {
                return json();
            }
        } else {
            if (current->is_object() && current->contains(p)) {
                current = &(*current)[p];
            } else {
                return json();
            }
        }
    }

    return *current;
}

void AnimationConfig::SetValue(const std::string& path, const json& value) {
    if (path.empty()) {
        m_data = value;
        return;
    }

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    json* current = &m_data;
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& p = parts[i];
        if (!current->contains(p)) {
            (*current)[p] = json::object();
        }
        current = &(*current)[p];
    }

    (*current)[parts.back()] = value;
}

void AnimationConfig::SetBase(const std::string& baseConfigPath) {
    m_basePath = baseConfigPath;
    m_data["extends"] = baseConfigPath;
}

bool AnimationConfig::ApplyInheritance(const std::unordered_map<std::string, std::shared_ptr<AnimationConfig>>& configs) {
    if (m_basePath.empty()) {
        return true;
    }

    // Find base config
    auto it = configs.find(m_basePath);
    if (it == configs.end()) {
        return false;
    }

    // Recursively apply base's inheritance first
    it->second->ApplyInheritance(configs);

    // Merge base into this config
    json merged = it->second->GetData();
    MergeJson(merged, m_data);
    m_data = merged;

    return true;
}

void AnimationConfig::SetCurrentPlatform(Platform platform) {
    s_currentPlatform = platform;
}

Platform AnimationConfig::GetCurrentPlatform() {
    return s_currentPlatform;
}

void AnimationConfig::ApplyPlatformOverrides() {
    if (m_platformOverrides.is_null()) {
        return;
    }

    std::string platformKey;
    switch (s_currentPlatform) {
        case Platform::Windows: platformKey = "windows"; break;
        case Platform::Mac: platformKey = "mac"; break;
        case Platform::Linux: platformKey = "linux"; break;
        case Platform::iOS: platformKey = "ios"; break;
        case Platform::Android: platformKey = "android"; break;
        case Platform::WebGL: platformKey = "webgl"; break;
        default: return;
    }

    if (m_platformOverrides.contains(platformKey)) {
        MergeJson(m_data, m_platformOverrides[platformKey]);
    }
}

json AnimationConfig::GetPlatformValue(const std::string& path) const {
    // First check platform override
    std::string platformKey;
    switch (s_currentPlatform) {
        case Platform::Windows: platformKey = "windows"; break;
        case Platform::Mac: platformKey = "mac"; break;
        case Platform::Linux: platformKey = "linux"; break;
        case Platform::iOS: platformKey = "ios"; break;
        case Platform::Android: platformKey = "android"; break;
        case Platform::WebGL: platformKey = "webgl"; break;
        default: return GetValue(path);
    }

    if (!m_platformOverrides.is_null() && m_platformOverrides.contains(platformKey)) {
        json platformData = m_platformOverrides[platformKey];
        // Navigate path in platform overrides
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }

        const json* current = &platformData;
        bool found = true;
        for (const auto& p : parts) {
            if (current->is_object() && current->contains(p)) {
                current = &(*current)[p];
            } else {
                found = false;
                break;
            }
        }

        if (found && !current->is_null()) {
            return *current;
        }
    }

    return GetValue(path);
}

ValidationResult AnimationConfig::Validate() const {
    if (m_schema) {
        return m_schema->Validate(m_data);
    }

    ValidationResult result;
    result.valid = true;
    return result;
}

void AnimationConfig::SetSchema(std::shared_ptr<AnimationSchemaValidator> schema) {
    m_schema = std::move(schema);
}

void AnimationConfig::MergeJson(json& target, const json& source) {
    if (!source.is_object()) {
        target = source;
        return;
    }

    for (const auto& [key, value] : source.items()) {
        if (target.contains(key) && target[key].is_object() && value.is_object()) {
            MergeJson(target[key], value);
        } else {
            target[key] = value;
        }
    }
}

std::time_t AnimationConfig::GetFileModifiedTime(const std::string& filepath) const {
    struct stat result;
    if (stat(filepath.c_str(), &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

// ============================================================================
// AnimationConfigManager
// ============================================================================

AnimationConfig* AnimationConfigManager::Load(const std::string& filepath) {
    auto config = std::make_shared<AnimationConfig>();
    if (!config->LoadFromFile(filepath)) {
        return nullptr;
    }

    std::string id = config->GetId();
    if (id.empty()) {
        id = filepath;
    }

    config->SetSchema(m_schema);

    // Register reload callback
    for (const auto& callback : m_reloadCallbacks) {
        config->OnReload([this, id, callback](const AnimationConfig& cfg) {
            callback(id, cfg);
        });
    }

    m_configs[id] = config;
    m_pathToId[filepath] = id;

    return config.get();
}

void AnimationConfigManager::LoadDirectory(const std::string& directory, bool recursive) {
    try {
        namespace fs = std::filesystem;

        auto processFile = [this](const fs::path& path) {
            if (path.extension() == ".json") {
                Load(path.string());
            }
        };

        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    processFile(entry.path());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    processFile(entry.path());
                }
            }
        }
    } catch (...) {
        // Directory doesn't exist or other error
    }
}

AnimationConfig* AnimationConfigManager::Get(const std::string& id) {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? it->second.get() : nullptr;
}

const AnimationConfig* AnimationConfigManager::Get(const std::string& id) const {
    auto it = m_configs.find(id);
    return it != m_configs.end() ? it->second.get() : nullptr;
}

AnimationConfig* AnimationConfigManager::GetByPath(const std::string& filepath) {
    auto pathIt = m_pathToId.find(filepath);
    if (pathIt != m_pathToId.end()) {
        return Get(pathIt->second);
    }
    return nullptr;
}

bool AnimationConfigManager::Remove(const std::string& id) {
    auto it = m_configs.find(id);
    if (it != m_configs.end()) {
        // Remove path mapping
        const auto& filepath = it->second->GetFilePath();
        m_pathToId.erase(filepath);
        m_configs.erase(it);
        return true;
    }
    return false;
}

void AnimationConfigManager::Clear() {
    m_configs.clear();
    m_pathToId.clear();
}

std::vector<std::string> AnimationConfigManager::GetAllIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_configs.size());
    for (const auto& [id, config] : m_configs) {
        ids.push_back(id);
    }
    return ids;
}

void AnimationConfigManager::ReloadAll() {
    for (auto& [id, config] : m_configs) {
        config->Reload();
    }
}

void AnimationConfigManager::CheckAndReloadAll() {
    for (auto& [id, config] : m_configs) {
        config->CheckAndReload();
    }
}

void AnimationConfigManager::ApplyAllInheritance() {
    for (auto& [id, config] : m_configs) {
        config->ApplyInheritance(m_configs);
    }
}

void AnimationConfigManager::SetSchema(const std::string& schemaPath) {
    m_schema = std::make_shared<AnimationSchemaValidator>();
    m_schema->LoadSchema(schemaPath);

    // Apply to existing configs
    for (auto& [id, config] : m_configs) {
        config->SetSchema(m_schema);
    }
}

std::unordered_map<std::string, ValidationResult> AnimationConfigManager::ValidateAll() const {
    std::unordered_map<std::string, ValidationResult> results;
    for (const auto& [id, config] : m_configs) {
        results[id] = config->Validate();
    }
    return results;
}

void AnimationConfigManager::OnAnyReload(std::function<void(const std::string&, const AnimationConfig&)> callback) {
    m_reloadCallbacks.push_back(callback);
}

// ============================================================================
// AnimationConfigBuilder
// ============================================================================

AnimationConfigBuilder& AnimationConfigBuilder::SetId(const std::string& id) {
    m_config["id"] = id;
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::SetName(const std::string& name) {
    m_config["name"] = name;
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::SetBase(const std::string& basePath) {
    m_config["extends"] = basePath;
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::AddState(const std::string& name, const json& stateConfig) {
    if (!m_config.contains("states")) {
        m_config["states"] = json::array();
    }

    json state = stateConfig;
    state["name"] = name;
    m_config["states"].push_back(state);
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::AddTransition(const std::string& from, const std::string& to,
                                                               const json& config) {
    if (!m_config.contains("transitions")) {
        m_config["transitions"] = json::array();
    }

    json transition = config;
    transition["from"] = from;
    transition["to"] = to;
    m_config["transitions"].push_back(transition);
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::AddParameter(const std::string& name, const std::string& type,
                                                              const json& defaultValue) {
    if (!m_config.contains("parameters")) {
        m_config["parameters"] = json::array();
    }

    m_config["parameters"].push_back({
        {"name", name},
        {"type", type},
        {"defaultValue", defaultValue}
    });
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::AddEvent(const std::string& stateName, float time,
                                                          const std::string& eventName, const json& data) {
    // Find state and add event
    if (m_config.contains("states")) {
        for (auto& state : m_config["states"]) {
            if (state["name"] == stateName) {
                if (!state.contains("events")) {
                    state["events"] = json::array();
                }
                state["events"].push_back({
                    {"time", time},
                    {"name", eventName},
                    {"data", data}
                });
                break;
            }
        }
    }
    return *this;
}

AnimationConfigBuilder& AnimationConfigBuilder::SetPlatformOverride(Platform platform, const std::string& path,
                                                                     const json& value) {
    std::string platformKey;
    switch (platform) {
        case Platform::Windows: platformKey = "windows"; break;
        case Platform::Mac: platformKey = "mac"; break;
        case Platform::Linux: platformKey = "linux"; break;
        case Platform::iOS: platformKey = "ios"; break;
        case Platform::Android: platformKey = "android"; break;
        case Platform::WebGL: platformKey = "webgl"; break;
        default: return *this;
    }

    if (!m_config.contains("platformOverrides")) {
        m_config["platformOverrides"] = json::object();
    }
    if (!m_config["platformOverrides"].contains(platformKey)) {
        m_config["platformOverrides"][platformKey] = json::object();
    }

    // Set nested path
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }

    json* current = &m_config["platformOverrides"][platformKey];
    for (size_t i = 0; i < parts.size() - 1; ++i) {
        if (!current->contains(parts[i])) {
            (*current)[parts[i]] = json::object();
        }
        current = &(*current)[parts[i]];
    }
    (*current)[parts.back()] = value;

    return *this;
}

AnimationConfig AnimationConfigBuilder::Build() {
    AnimationConfig config;
    config.LoadFromJson(m_config);
    return config;
}

json AnimationConfigBuilder::ToJson() const {
    return m_config;
}

} // namespace Nova
