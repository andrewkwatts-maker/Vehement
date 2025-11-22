#include "ConfigBindingSystem.hpp"
#include "../web/JSBridge.hpp"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <random>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#endif

namespace Vehement {

ConfigBindingSystem::ConfigBindingSystem() = default;

ConfigBindingSystem::~ConfigBindingSystem() {
    if (m_initialized) {
        Shutdown();
    }
}

bool ConfigBindingSystem::Initialize(WebEditor::JSBridge& bridge, const Config& config) {
    if (m_initialized) {
        return false;
    }

    m_bridge = &bridge;
    m_config = config;

    RegisterBridgeFunctions();

    if (m_config.enableHotReload) {
        StartFileWatching();
    }

    m_initialized = true;
    return true;
}

void ConfigBindingSystem::Shutdown() {
    if (!m_initialized) {
        return;
    }

    StopFileWatching();

    m_configContents.clear();
    m_configInfo.clear();
    m_bindings.clear();
    m_callbacks.clear();
    m_changeHistory.clear();

    m_initialized = false;
}

void ConfigBindingSystem::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // File watching
    if (m_config.enableHotReload) {
        m_fileWatchTimer += deltaTime;
        if (m_fileWatchTimer >= m_config.fileWatchInterval) {
            m_fileWatchTimer = 0.0f;
            ProcessFileChanges();
        }
    }

    // Process pending debounced changes
    ProcessPendingChanges();
}

// Config File Management
bool ConfigBindingSystem::LoadConfig(const std::string& path) {
    std::string fullPath = m_config.configBasePath + path;

    std::ifstream file(fullPath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string normalizedPath = NormalizePath(path);
    m_configContents[normalizedPath] = content;

    // Update file info
    ConfigFileInfo info;
    info.path = fullPath;
    info.relativePath = normalizedPath;
    info.lastModified = std::filesystem::last_write_time(fullPath);
    info.fileSize = std::filesystem::file_size(fullPath);
    info.isValid = true;

    // Parse dependencies
    ParseDependencies(normalizedPath, content);
    info.dependencies = GetDependencies(normalizedPath);

    m_configInfo[normalizedPath] = info;
    m_lastModifiedTimes[normalizedPath] = info.lastModified;

    if (OnConfigLoaded) {
        OnConfigLoaded(normalizedPath);
    }

    // Trigger bindings with triggerOnLoad
    ConfigChangeEvent event;
    event.filePath = normalizedPath;
    event.jsonPath = "$";
    event.changeType = "load";
    event.newValue = content;
    event.timestamp = std::chrono::steady_clock::now();

    for (const auto& [bindingId, binding] : m_bindings) {
        if (binding.configPath == normalizedPath && binding.triggerOnLoad) {
            auto it = m_callbacks.find(binding.callbackId);
            if (it != m_callbacks.end()) {
                it->second(event);
            }
        }
    }

    return true;
}

bool ConfigBindingSystem::ReloadConfig(const std::string& path) {
    std::string normalizedPath = NormalizePath(path);

    // Remember old content for diff
    std::string oldContent;
    auto it = m_configContents.find(normalizedPath);
    if (it != m_configContents.end()) {
        oldContent = it->second;
    }

    // Reload
    if (!LoadConfig(path)) {
        return false;
    }

    // Create change event
    ConfigChangeEvent event;
    event.filePath = normalizedPath;
    event.jsonPath = "$";
    event.oldValue = oldContent;
    event.newValue = m_configContents[normalizedPath];
    event.changeType = "reload";
    event.timestamp = std::chrono::steady_clock::now();

    TriggerBindings(event);

    // Record in history
    m_changeHistory.push_back(event);
    while (m_changeHistory.size() > static_cast<size_t>(m_config.maxHistorySize)) {
        m_changeHistory.pop_front();
    }

    if (OnConfigChanged) {
        OnConfigChanged(normalizedPath);
    }

    return true;
}

void ConfigBindingSystem::UnloadConfig(const std::string& path) {
    std::string normalizedPath = NormalizePath(path);

    m_configContents.erase(normalizedPath);
    m_configInfo.erase(normalizedPath);
    m_lastModifiedTimes.erase(normalizedPath);

    if (OnConfigUnloaded) {
        OnConfigUnloaded(normalizedPath);
    }
}

bool ConfigBindingSystem::SaveConfig(const std::string& path) {
    std::string normalizedPath = NormalizePath(path);

    auto it = m_configContents.find(normalizedPath);
    if (it == m_configContents.end()) {
        return false;
    }

    std::string fullPath = m_config.configBasePath + normalizedPath;
    std::ofstream file(fullPath);
    if (!file.is_open()) {
        return false;
    }

    file << it->second;
    file.close();

    // Update last modified time
    m_lastModifiedTimes[normalizedPath] = std::filesystem::last_write_time(fullPath);

    if (OnConfigSaved) {
        OnConfigSaved(normalizedPath);
    }

    return true;
}

std::string ConfigBindingSystem::GetConfigContent(const std::string& path) const {
    std::string normalizedPath = NormalizePath(path);
    auto it = m_configContents.find(normalizedPath);
    return it != m_configContents.end() ? it->second : "";
}

bool ConfigBindingSystem::SetConfigContent(const std::string& path, const std::string& content,
                                            bool save) {
    std::string normalizedPath = NormalizePath(path);

    std::string oldContent = GetConfigContent(normalizedPath);
    m_configContents[normalizedPath] = content;

    // Create change event
    ConfigChangeEvent event;
    event.filePath = normalizedPath;
    event.jsonPath = "$";
    event.oldValue = oldContent;
    event.newValue = content;
    event.changeType = "set";
    event.timestamp = std::chrono::steady_clock::now();

    TriggerBindings(event);

    // Record in history
    m_changeHistory.push_back(event);

    if (save) {
        return SaveConfig(path);
    }

    return true;
}

std::string ConfigBindingSystem::GetValue(const std::string& filePath,
                                           const std::string& jsonPath) const {
    std::string content = GetConfigContent(filePath);
    if (content.empty()) {
        return "";
    }

    // Simple JSON path navigation - in production use a proper JSON library
    // This is a placeholder
    return "";
}

bool ConfigBindingSystem::SetValue(const std::string& filePath,
                                    const std::string& jsonPath,
                                    const std::string& value) {
    std::string normalizedPath = NormalizePath(filePath);

    auto it = m_configContents.find(normalizedPath);
    if (it == m_configContents.end()) {
        return false;
    }

    std::string oldValue = GetValue(normalizedPath, jsonPath);

    // Would modify the JSON at the given path
    // Placeholder - in production use a proper JSON library

    // Create change event
    ConfigChangeEvent event;
    event.filePath = normalizedPath;
    event.jsonPath = jsonPath;
    event.oldValue = oldValue;
    event.newValue = value;
    event.changeType = "set";
    event.timestamp = std::chrono::steady_clock::now();

    TriggerBindings(event);
    m_changeHistory.push_back(event);

    return true;
}

std::vector<ConfigFileInfo> ConfigBindingSystem::GetLoadedConfigs() const {
    std::vector<ConfigFileInfo> result;
    result.reserve(m_configInfo.size());
    for (const auto& [path, info] : m_configInfo) {
        result.push_back(info);
    }
    return result;
}

std::optional<ConfigFileInfo> ConfigBindingSystem::GetConfigInfo(const std::string& path) const {
    std::string normalizedPath = NormalizePath(path);
    auto it = m_configInfo.find(normalizedPath);
    if (it != m_configInfo.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Event Binding
std::string ConfigBindingSystem::CreateBinding(const std::string& configPath,
                                                 const std::string& jsonPath,
                                                 ConfigChangeCallback callback) {
    std::string bindingId = GenerateBindingId();
    std::string callbackId = "cb_" + bindingId;

    ConfigBinding binding;
    binding.id = bindingId;
    binding.configPath = NormalizePath(configPath);
    binding.jsonPath = jsonPath;
    binding.callbackId = callbackId;
    binding.enabled = true;

    m_bindings[bindingId] = binding;
    m_callbacks[callbackId] = callback;

    return bindingId;
}

void ConfigBindingSystem::RemoveBinding(const std::string& bindingId) {
    auto it = m_bindings.find(bindingId);
    if (it != m_bindings.end()) {
        m_callbacks.erase(it->second.callbackId);
        m_bindings.erase(it);
    }
}

void ConfigBindingSystem::SetBindingEnabled(const std::string& bindingId, bool enabled) {
    auto it = m_bindings.find(bindingId);
    if (it != m_bindings.end()) {
        it->second.enabled = enabled;
    }
}

std::vector<ConfigBinding> ConfigBindingSystem::GetBindings() const {
    std::vector<ConfigBinding> result;
    result.reserve(m_bindings.size());
    for (const auto& [id, binding] : m_bindings) {
        result.push_back(binding);
    }
    return result;
}

std::vector<ConfigBinding> ConfigBindingSystem::GetBindingsForConfig(
    const std::string& configPath) const {

    std::string normalizedPath = NormalizePath(configPath);
    std::vector<ConfigBinding> result;

    for (const auto& [id, binding] : m_bindings) {
        if (binding.configPath == normalizedPath) {
            result.push_back(binding);
        }
    }

    return result;
}

// Hot Reload
void ConfigBindingSystem::SetHotReloadEnabled(bool enabled) {
    m_config.enableHotReload = enabled;
    if (enabled) {
        StartFileWatching();
    } else {
        StopFileWatching();
    }
}

void ConfigBindingSystem::CheckForChanges() {
    ProcessFileChanges();
}

std::vector<std::string> ConfigBindingSystem::GetChangedFiles() const {
    std::vector<std::string> changed;

    for (const auto& [path, lastTime] : m_lastModifiedTimes) {
        auto it = m_configInfo.find(path);
        if (it != m_configInfo.end()) {
            std::string fullPath = m_config.configBasePath + path;
            if (std::filesystem::exists(fullPath)) {
                auto currentTime = std::filesystem::last_write_time(fullPath);
                if (currentTime != lastTime) {
                    changed.push_back(path);
                }
            }
        }
    }

    return changed;
}

// Version Control
std::vector<VersionDiff> ConfigBindingSystem::GetVCSStatus() const {
    std::vector<VersionDiff> diffs;

    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return diffs;
    }

    std::string output = ExecuteGitCommand("git status --porcelain");
    ParseGitStatus(output, diffs);

    return diffs;
}

std::string ConfigBindingSystem::GetFileDiff(const std::string& path) const {
    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return "";
    }

    std::string fullPath = m_config.configBasePath + path;
    return ExecuteGitCommand("git diff \"" + fullPath + "\"");
}

std::vector<std::pair<std::string, std::string>> ConfigBindingSystem::GetFileHistory(
    const std::string& path, int maxEntries) const {

    std::vector<std::pair<std::string, std::string>> history;

    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return history;
    }

    std::string fullPath = m_config.configBasePath + path;
    std::string output = ExecuteGitCommand(
        "git log --oneline -" + std::to_string(maxEntries) + " \"" + fullPath + "\"");

    // Parse output
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line) && history.size() < static_cast<size_t>(maxEntries)) {
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            std::string hash = line.substr(0, spacePos);
            std::string message = line.substr(spacePos + 1);
            history.push_back({hash, message});
        }
    }

    return history;
}

bool ConfigBindingSystem::RevertFile(const std::string& path) {
    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return false;
    }

    std::string fullPath = m_config.configBasePath + path;
    std::string output = ExecuteGitCommand("git checkout -- \"" + fullPath + "\"");

    // Reload config
    ReloadConfig(path);

    return true;
}

bool ConfigBindingSystem::StageFile(const std::string& path) {
    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return false;
    }

    std::string fullPath = m_config.configBasePath + path;
    ExecuteGitCommand("git add \"" + fullPath + "\"");
    return true;
}

bool ConfigBindingSystem::Commit(const std::string& message) {
    if (!m_config.enableVersionControl || m_config.vcsType != "git") {
        return false;
    }

    ExecuteGitCommand("git commit -m \"" + message + "\"");
    return true;
}

// Merge Conflict Resolution
std::vector<MergeConflict> ConfigBindingSystem::GetMergeConflicts() const {
    return m_mergeConflicts;
}

bool ConfigBindingSystem::ResolveConflict(const std::string& filePath,
                                           const std::string& resolution,
                                           const std::string& mergedContent) {
    for (auto& conflict : m_mergeConflicts) {
        if (conflict.filePath == filePath) {
            conflict.resolved = true;
            conflict.resolution = resolution;

            if (resolution == "ours") {
                SetConfigContent(filePath, conflict.oursContent, true);
            } else if (resolution == "theirs") {
                SetConfigContent(filePath, conflict.theirsContent, true);
            } else if (resolution == "merged") {
                conflict.mergedContent = mergedContent;
                SetConfigContent(filePath, mergedContent, true);
            }

            if (OnConflictResolved) {
                OnConflictResolved(conflict);
            }

            return true;
        }
    }
    return false;
}

void ConfigBindingSystem::MarkAllResolved() {
    for (auto& conflict : m_mergeConflicts) {
        conflict.resolved = true;
    }
}

void ConfigBindingSystem::RenderConflictUI() {
    if (m_mergeConflicts.empty()) {
        ImGui::TextDisabled("No merge conflicts");
        return;
    }

    for (auto& conflict : m_mergeConflicts) {
        ImGui::PushID(conflict.filePath.c_str());

        bool open = ImGui::CollapsingHeader(conflict.filePath.c_str(),
                                             conflict.resolved ?
                                             ImGuiTreeNodeFlags_None :
                                             ImGuiTreeNodeFlags_DefaultOpen);

        if (conflict.resolved) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[Resolved: %s]",
                              conflict.resolution.c_str());
        }

        if (open) {
            // Three-way diff view
            float columnWidth = (ImGui::GetContentRegionAvail().x - 20) / 3;

            ImGui::Columns(3, "ConflictColumns", true);

            // Base (ancestor)
            ImGui::Text("Base");
            ImGui::BeginChild("Base", ImVec2(columnWidth, 200), true);
            ImGui::TextWrapped("%s", conflict.baseContent.c_str());
            ImGui::EndChild();

            ImGui::NextColumn();

            // Ours
            ImGui::Text("Ours");
            ImGui::BeginChild("Ours", ImVec2(columnWidth, 200), true);
            ImGui::TextWrapped("%s", conflict.oursContent.c_str());
            ImGui::EndChild();
            if (ImGui::Button("Accept Ours")) {
                ResolveConflict(conflict.filePath, "ours");
            }

            ImGui::NextColumn();

            // Theirs
            ImGui::Text("Theirs");
            ImGui::BeginChild("Theirs", ImVec2(columnWidth, 200), true);
            ImGui::TextWrapped("%s", conflict.theirsContent.c_str());
            ImGui::EndChild();
            if (ImGui::Button("Accept Theirs")) {
                ResolveConflict(conflict.filePath, "theirs");
            }

            ImGui::Columns(1);

            // Manual merge option
            if (ImGui::Button("Manual Merge...")) {
                // Would open merge editor
            }
        }

        ImGui::PopID();
        ImGui::Separator();
    }
}

// Change History
std::vector<ConfigChangeEvent> ConfigBindingSystem::GetChangeHistory(size_t maxEntries) const {
    std::vector<ConfigChangeEvent> history;
    size_t count = std::min(maxEntries, m_changeHistory.size());

    auto it = m_changeHistory.rbegin();
    for (size_t i = 0; i < count && it != m_changeHistory.rend(); ++i, ++it) {
        history.push_back(*it);
    }

    return history;
}

void ConfigBindingSystem::ClearHistory() {
    m_changeHistory.clear();
}

// Dependencies
std::vector<std::string> ConfigBindingSystem::GetDependencies(const std::string& path) const {
    std::string normalizedPath = NormalizePath(path);
    auto it = m_configInfo.find(normalizedPath);
    if (it != m_configInfo.end()) {
        return it->second.dependencies;
    }
    return {};
}

std::vector<std::string> ConfigBindingSystem::GetDependents(const std::string& path) const {
    std::string normalizedPath = NormalizePath(path);
    std::vector<std::string> dependents;

    for (const auto& [configPath, info] : m_configInfo) {
        for (const auto& dep : info.dependencies) {
            if (dep == normalizedPath) {
                dependents.push_back(configPath);
                break;
            }
        }
    }

    return dependents;
}

std::vector<std::string> ConfigBindingSystem::GetLoadOrder() const {
    std::vector<std::string> order;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inProgress;

    std::function<void(const std::string&)> visit = [&](const std::string& path) {
        if (visited.count(path)) return;
        if (inProgress.count(path)) {
            // Circular dependency - skip
            return;
        }

        inProgress.insert(path);

        auto deps = GetDependencies(path);
        for (const auto& dep : deps) {
            visit(dep);
        }

        inProgress.erase(path);
        visited.insert(path);
        order.push_back(path);
    };

    for (const auto& [path, info] : m_configInfo) {
        visit(path);
    }

    return order;
}

int ConfigBindingSystem::ReloadWithDependents(const std::string& path) {
    int count = 0;

    if (ReloadConfig(path)) {
        count++;
    }

    auto dependents = GetDependents(path);
    for (const auto& dependent : dependents) {
        if (ReloadConfig(dependent)) {
            count++;
        }
    }

    return count;
}

// Private helpers
void ConfigBindingSystem::StartFileWatching() {
    // File watching is done in Update()
}

void ConfigBindingSystem::StopFileWatching() {
    // Clear watch state
    m_lastModifiedTimes.clear();
}

void ConfigBindingSystem::ProcessFileChanges() {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto changedFiles = GetChangedFiles();
    for (const auto& path : changedFiles) {
        if (OnFileModified) {
            OnFileModified(path);
        }

        // Auto-reload if enabled
        if (m_config.enableHotReload) {
            ReloadConfig(path);
        }
    }
}

bool ConfigBindingSystem::MatchesJsonPath(const std::string& actualPath,
                                           const std::string& pattern) const {
    // Support wildcards
    if (pattern == "*" || pattern == "$" || pattern == "**") {
        return true;
    }

    // Convert pattern to regex
    std::string regexPattern = pattern;

    // Escape special regex characters
    std::string escaped;
    for (char c : regexPattern) {
        if (c == '.' || c == '[' || c == ']' || c == '(' || c == ')' ||
            c == '{' || c == '}' || c == '+' || c == '?' || c == '^' ||
            c == '|' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }

    // Convert * to .*
    size_t pos = 0;
    while ((pos = escaped.find('*', pos)) != std::string::npos) {
        escaped.replace(pos, 1, ".*");
        pos += 2;
    }

    try {
        std::regex re(escaped);
        return std::regex_match(actualPath, re);
    } catch (...) {
        return actualPath == pattern;
    }
}

void ConfigBindingSystem::TriggerBindings(const ConfigChangeEvent& event) {
    for (const auto& [bindingId, binding] : m_bindings) {
        if (!binding.enabled) continue;
        if (binding.configPath != event.filePath) continue;
        if (!MatchesJsonPath(event.jsonPath, binding.jsonPath)) continue;

        if (binding.debounce) {
            // Add to pending changes
            std::string key = binding.configPath + ":" + binding.jsonPath;
            m_pendingChanges[key] = {event, binding.debounceTime, {bindingId}};
        } else {
            // Trigger immediately
            auto it = m_callbacks.find(binding.callbackId);
            if (it != m_callbacks.end()) {
                it->second(event);
            }
        }
    }
}

void ConfigBindingSystem::ProcessPendingChanges() {
    // This would be called with deltaTime to handle debouncing
    // Simplified implementation
}

std::string ConfigBindingSystem::ExecuteGitCommand(const std::string& command) const {
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }

    return result;
}

void ConfigBindingSystem::ParseGitStatus(const std::string& output,
                                          std::vector<VersionDiff>& diffs) const {
    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.size() < 4) continue;

        VersionDiff diff;
        char status = line[0];

        switch (status) {
            case 'M': diff.status = "modified"; break;
            case 'A': diff.status = "added"; break;
            case 'D': diff.status = "deleted"; break;
            case 'R': diff.status = "renamed"; break;
            case '?': diff.status = "untracked"; break;
            default: diff.status = "unknown"; break;
        }

        diff.path = line.substr(3);
        diffs.push_back(diff);
    }
}

void ConfigBindingSystem::ParseMergeConflicts() {
    m_mergeConflicts.clear();

    // Would parse git merge conflicts from files
}

void ConfigBindingSystem::ParseDependencies(const std::string& path, const std::string& content) {
    // Look for $ref or imports in the JSON
    // This is a placeholder - would use proper JSON parsing

    auto it = m_configInfo.find(path);
    if (it == m_configInfo.end()) return;

    it->second.dependencies.clear();

    // Simple pattern matching for $ref
    std::regex refPattern(R"(\"\$ref\"\s*:\s*\"([^\"]+)\")");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());

    while (std::regex_search(searchStart, content.cend(), match, refPattern)) {
        std::string ref = match[1];
        // Convert to file path if external
        if (ref.find(".json") != std::string::npos) {
            it->second.dependencies.push_back(NormalizePath(ref));
        }
        searchStart = match.suffix().first;
    }
}

void ConfigBindingSystem::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    m_bridge->RegisterFunction("configBinding.load", [this](const auto& args) {
        if (args.empty()) {
            return WebEditor::JSResult::Error("Missing path");
        }
        bool success = LoadConfig(args[0].GetString());
        return WebEditor::JSResult::Success(WebEditor::JSValue(success));
    });

    m_bridge->RegisterFunction("configBinding.save", [this](const auto& args) {
        if (args.empty()) {
            return WebEditor::JSResult::Error("Missing path");
        }
        bool success = SaveConfig(args[0].GetString());
        return WebEditor::JSResult::Success(WebEditor::JSValue(success));
    });

    m_bridge->RegisterFunction("configBinding.getContent", [this](const auto& args) {
        if (args.empty()) {
            return WebEditor::JSResult::Error("Missing path");
        }
        std::string content = GetConfigContent(args[0].GetString());
        return WebEditor::JSResult::Success(WebEditor::JSValue(content));
    });

    m_bridge->RegisterFunction("configBinding.setContent", [this](const auto& args) {
        if (args.size() < 2) {
            return WebEditor::JSResult::Error("Missing path or content");
        }
        bool save = args.size() > 2 && args[2].GetBool();
        bool success = SetConfigContent(args[0].GetString(), args[1].GetString(), save);
        return WebEditor::JSResult::Success(WebEditor::JSValue(success));
    });

    m_bridge->RegisterFunction("configBinding.getVCSStatus", [this](const auto&) {
        auto status = GetVCSStatus();
        WebEditor::JSValue::Array result;
        for (const auto& diff : status) {
            WebEditor::JSValue::Object obj;
            obj["path"] = diff.path;
            obj["status"] = diff.status;
            obj["additions"] = diff.additions;
            obj["deletions"] = diff.deletions;
            result.push_back(WebEditor::JSValue(std::move(obj)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(result)));
    });

    m_bridge->RegisterFunction("configBinding.getDiff", [this](const auto& args) {
        if (args.empty()) {
            return WebEditor::JSResult::Error("Missing path");
        }
        std::string diff = GetFileDiff(args[0].GetString());
        return WebEditor::JSResult::Success(WebEditor::JSValue(diff));
    });
}

std::string ConfigBindingSystem::GenerateBindingId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "bind_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string ConfigBindingSystem::NormalizePath(const std::string& path) const {
    std::string normalized = path;

    // Replace backslashes with forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // Remove leading ./
    if (normalized.substr(0, 2) == "./") {
        normalized = normalized.substr(2);
    }

    // Remove leading /
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }

    return normalized;
}

} // namespace Vehement
