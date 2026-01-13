/**
 * @file IEditorTool.cpp
 * @brief Implementation of EditorToolRegistry and EditorToolManager
 */

#include "IEditorTool.hpp"

#include <algorithm>
#include <stdexcept>

namespace Nova {

// =============================================================================
// ToolInputEvent Implementation
// =============================================================================

nlohmann::json ToolInputEvent::ToJson() const {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["mousePos"] = {mousePos.x, mousePos.y};
    j["mouseDelta"] = {mouseDelta.x, mouseDelta.y};
    j["button"] = static_cast<int>(button);
    j["key"] = key;
    j["scancode"] = scancode;
    j["character"] = std::string(1, character);
    j["modifiers"] = static_cast<uint8_t>(modifiers);
    j["scrollDelta"] = scrollDelta;
    j["scrollDeltaX"] = scrollDeltaX;
    j["pressure"] = pressure;
    j["tiltX"] = tiltX;
    j["tiltY"] = tiltY;
    j["viewportSize"] = {viewportSize.x, viewportSize.y};
    j["timestamp"] = timestamp;
    j["deltaTime"] = deltaTime;
    return j;
}

ToolInputEvent ToolInputEvent::FromJson(const nlohmann::json& json) {
    ToolInputEvent event;

    if (json.contains("type")) {
        event.type = static_cast<Type>(json["type"].get<int>());
    }
    if (json.contains("mousePos") && json["mousePos"].is_array()) {
        event.mousePos.x = json["mousePos"][0].get<float>();
        event.mousePos.y = json["mousePos"][1].get<float>();
    }
    if (json.contains("mouseDelta") && json["mouseDelta"].is_array()) {
        event.mouseDelta.x = json["mouseDelta"][0].get<float>();
        event.mouseDelta.y = json["mouseDelta"][1].get<float>();
    }
    if (json.contains("button")) {
        event.button = static_cast<MouseButton>(json["button"].get<int>());
    }
    if (json.contains("key")) {
        event.key = json["key"].get<int>();
    }
    if (json.contains("scancode")) {
        event.scancode = json["scancode"].get<int>();
    }
    if (json.contains("character")) {
        const auto& charStr = json["character"].get<std::string>();
        event.character = charStr.empty() ? '\0' : charStr[0];
    }
    if (json.contains("modifiers")) {
        event.modifiers = static_cast<ToolKeyModifiers>(json["modifiers"].get<uint8_t>());
    }
    if (json.contains("scrollDelta")) {
        event.scrollDelta = json["scrollDelta"].get<float>();
    }
    if (json.contains("scrollDeltaX")) {
        event.scrollDeltaX = json["scrollDeltaX"].get<float>();
    }
    if (json.contains("pressure")) {
        event.pressure = json["pressure"].get<float>();
    }
    if (json.contains("tiltX")) {
        event.tiltX = json["tiltX"].get<float>();
    }
    if (json.contains("tiltY")) {
        event.tiltY = json["tiltY"].get<float>();
    }
    if (json.contains("viewportSize") && json["viewportSize"].is_array()) {
        event.viewportSize.x = json["viewportSize"][0].get<float>();
        event.viewportSize.y = json["viewportSize"][1].get<float>();
    }
    if (json.contains("timestamp")) {
        event.timestamp = json["timestamp"].get<double>();
    }
    if (json.contains("deltaTime")) {
        event.deltaTime = json["deltaTime"].get<float>();
    }

    return event;
}

// =============================================================================
// EditorToolRegistry Implementation
// =============================================================================

EditorToolRegistry& EditorToolRegistry::Instance() {
    static EditorToolRegistry instance;
    return instance;
}

bool EditorToolRegistry::RegisterTool(ToolRegistration registration) {
    if (registration.id.empty()) {
        return false;
    }

    if (m_registrations.find(registration.id) != m_registrations.end()) {
        // Tool already registered
        return false;
    }

    m_registrations.emplace(registration.id, std::move(registration));
    return true;
}

bool EditorToolRegistry::Unregister(const std::string& id) {
    auto it = m_registrations.find(id);
    if (it == m_registrations.end()) {
        return false;
    }
    m_registrations.erase(it);
    return true;
}

bool EditorToolRegistry::IsRegistered(const std::string& id) const {
    return m_registrations.find(id) != m_registrations.end();
}

std::unique_ptr<IEditorTool> EditorToolRegistry::CreateTool(const std::string& id) const {
    auto it = m_registrations.find(id);
    if (it == m_registrations.end()) {
        return nullptr;
    }
    return it->second.Create();
}

std::vector<std::unique_ptr<IEditorTool>> EditorToolRegistry::CreateToolsInCategory(
    ToolCategory category) const {

    std::vector<std::unique_ptr<IEditorTool>> tools;
    auto ids = GetToolIdsByCategory(category);

    tools.reserve(ids.size());
    for (const auto& id : ids) {
        auto tool = CreateTool(id);
        if (tool) {
            tools.push_back(std::move(tool));
        }
    }

    return tools;
}

std::optional<ToolRegistration> EditorToolRegistry::GetRegistration(const std::string& id) const {
    auto it = m_registrations.find(id);
    if (it == m_registrations.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string> EditorToolRegistry::GetAllToolIds() const {
    std::vector<std::string> ids;
    ids.reserve(m_registrations.size());

    for (const auto& [id, reg] : m_registrations) {
        ids.push_back(id);
    }

    // Sort by category then priority
    std::sort(ids.begin(), ids.end(), [this](const std::string& a, const std::string& b) {
        const auto& regA = m_registrations.at(a);
        const auto& regB = m_registrations.at(b);

        if (regA.category != regB.category) {
            return static_cast<uint8_t>(regA.category) < static_cast<uint8_t>(regB.category);
        }
        return regA.priority > regB.priority;
    });

    return ids;
}

std::vector<std::string> EditorToolRegistry::GetToolIdsByCategory(ToolCategory category) const {
    std::vector<std::string> ids;

    for (const auto& [id, reg] : m_registrations) {
        if (reg.category == category) {
            ids.push_back(id);
        }
    }

    // Sort by priority (descending)
    std::sort(ids.begin(), ids.end(), [this](const std::string& a, const std::string& b) {
        return m_registrations.at(a).priority > m_registrations.at(b).priority;
    });

    return ids;
}

std::vector<ToolRegistration> EditorToolRegistry::GetAllRegistrations() const {
    std::vector<ToolRegistration> regs;
    regs.reserve(m_registrations.size());

    for (const auto& [id, reg] : m_registrations) {
        regs.push_back(reg);
    }

    // Sort by category then priority
    std::sort(regs.begin(), regs.end(), [](const ToolRegistration& a, const ToolRegistration& b) {
        if (a.category != b.category) {
            return static_cast<uint8_t>(a.category) < static_cast<uint8_t>(b.category);
        }
        return a.priority > b.priority;
    });

    return regs;
}

std::vector<ToolRegistration> EditorToolRegistry::GetRegistrationsByCategory(
    ToolCategory category) const {

    std::vector<ToolRegistration> regs;

    for (const auto& [id, reg] : m_registrations) {
        if (reg.category == category) {
            regs.push_back(reg);
        }
    }

    // Sort by priority (descending)
    std::sort(regs.begin(), regs.end(), [](const ToolRegistration& a, const ToolRegistration& b) {
        return a.priority > b.priority;
    });

    return regs;
}

std::string EditorToolRegistry::FindToolByShortcut(const std::string& shortcut) const {
    if (shortcut.empty()) {
        return "";
    }

    for (const auto& [id, reg] : m_registrations) {
        if (reg.shortcut == shortcut) {
            return id;
        }
    }

    return "";
}

size_t EditorToolRegistry::GetToolCount() const {
    return m_registrations.size();
}

size_t EditorToolRegistry::GetToolCountInCategory(ToolCategory category) const {
    size_t count = 0;
    for (const auto& [id, reg] : m_registrations) {
        if (reg.category == category) {
            ++count;
        }
    }
    return count;
}

nlohmann::json EditorToolRegistry::ToJson() const {
    nlohmann::json j;
    j["toolCount"] = m_registrations.size();

    nlohmann::json tools = nlohmann::json::array();
    for (const auto& [id, reg] : m_registrations) {
        nlohmann::json tool;
        tool["id"] = reg.id;
        tool["name"] = reg.name;
        tool["category"] = ToolCategoryToString(reg.category);
        tool["priority"] = reg.priority;
        tool["shortcut"] = reg.shortcut;
        tools.push_back(tool);
    }
    j["tools"] = tools;

    return j;
}

// =============================================================================
// EditorToolManager Implementation
// =============================================================================

EditorToolManager::EditorToolManager() = default;

EditorToolManager::~EditorToolManager() {
    Shutdown();
}

EditorToolManager::EditorToolManager(EditorToolManager&&) noexcept = default;
EditorToolManager& EditorToolManager::operator=(EditorToolManager&&) noexcept = default;

void EditorToolManager::Initialize() {
    if (m_initialized) {
        return;
    }

    // Try to activate the default tool
    if (!m_defaultToolId.empty()) {
        SetActiveTool(m_defaultToolId);
    }

    m_initialized = true;
}

void EditorToolManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Deactivate and clear all tools
    if (m_activeTool && m_activeTool->IsActive()) {
        m_activeTool->Deactivate();
    }
    m_activeTool.reset();
    m_activeToolId.clear();

    // Clear tool stack
    for (auto& entry : m_toolStack) {
        if (entry.tool && entry.tool->IsActive()) {
            entry.tool->Deactivate();
        }
    }
    m_toolStack.clear();

    m_callbacks.clear();
    m_initialized = false;
}

bool EditorToolManager::SetActiveTool(const std::string& toolId) {
    // Don't switch to same tool
    if (toolId == m_activeToolId && m_activeTool) {
        return true;
    }

    // Create new tool
    auto newTool = EditorToolRegistry::Instance().CreateTool(toolId);
    if (!newTool) {
        return false;
    }

    // Store old ID for callback
    std::string oldToolId = m_activeToolId;

    // Deactivate current tool
    if (m_activeTool && m_activeTool->IsActive()) {
        // Save settings before deactivating
        m_toolSettings[m_activeToolId] = m_activeTool->SaveSettings();
        m_activeTool->Deactivate();
    }

    // Switch to new tool
    m_activeTool = std::move(newTool);
    m_activeToolId = toolId;

    // Load saved settings
    auto settingsIt = m_toolSettings.find(toolId);
    if (settingsIt != m_toolSettings.end()) {
        m_activeTool->LoadSettings(settingsIt->second);
    }

    // Activate new tool
    m_activeTool->Activate();

    // Notify callbacks
    NotifyToolChanged(oldToolId, m_activeToolId);

    return true;
}

bool EditorToolManager::SetActiveToolByCategory(ToolCategory category) {
    auto ids = EditorToolRegistry::Instance().GetToolIdsByCategory(category);
    if (ids.empty()) {
        return false;
    }
    return SetActiveTool(ids.front());
}

bool EditorToolManager::PushTemporaryTool(const std::string& toolId) {
    if (toolId == m_activeToolId) {
        return true;  // Already active, nothing to do
    }

    auto newTool = EditorToolRegistry::Instance().CreateTool(toolId);
    if (!newTool) {
        return false;
    }

    // Save current tool to stack
    if (m_activeTool) {
        ToolStackEntry entry;
        entry.tool = std::move(m_activeTool);
        entry.id = m_activeToolId;

        // Don't fully deactivate, just pause
        m_toolStack.push_back(std::move(entry));
    }

    // Activate temporary tool
    std::string oldId = m_activeToolId;
    m_activeTool = std::move(newTool);
    m_activeToolId = toolId;
    m_activeTool->Activate();

    NotifyToolChanged(oldId, m_activeToolId);

    return true;
}

void EditorToolManager::PopTemporaryTool() {
    if (m_toolStack.empty()) {
        return;
    }

    // Deactivate current temporary tool
    std::string oldId = m_activeToolId;
    if (m_activeTool && m_activeTool->IsActive()) {
        m_activeTool->Deactivate();
    }

    // Restore previous tool
    auto& entry = m_toolStack.back();
    m_activeTool = std::move(entry.tool);
    m_activeToolId = entry.id;
    m_toolStack.pop_back();

    // Re-activate restored tool (it was paused, not fully deactivated)
    if (m_activeTool && !m_activeTool->IsActive()) {
        m_activeTool->Activate();
    }

    NotifyToolChanged(oldId, m_activeToolId);
}

void EditorToolManager::CycleToolInCategory(bool forward) {
    if (!m_activeTool) {
        return;
    }

    auto reg = EditorToolRegistry::Instance().GetRegistration(m_activeToolId);
    if (!reg) {
        return;
    }

    auto ids = EditorToolRegistry::Instance().GetToolIdsByCategory(reg->category);
    if (ids.size() <= 1) {
        return;
    }

    // Find current tool in list
    auto it = std::find(ids.begin(), ids.end(), m_activeToolId);
    if (it == ids.end()) {
        return;
    }

    // Move to next/previous
    if (forward) {
        ++it;
        if (it == ids.end()) {
            it = ids.begin();
        }
    } else {
        if (it == ids.begin()) {
            it = ids.end();
        }
        --it;
    }

    SetActiveTool(*it);
}

ToolInputResult EditorToolManager::ProcessInput(const ToolInputEvent& event,
                                                 const ToolContext& ctx) {
    if (!m_activeTool || !m_activeTool->IsActive()) {
        return ToolInputResult::NotHandled();
    }

    return m_activeTool->OnInput(event, ctx);
}

void EditorToolManager::Update(float deltaTime, const ToolContext& ctx) {
    if (m_activeTool && m_activeTool->IsActive()) {
        m_activeTool->Update(deltaTime, ctx);
    }
}

void EditorToolManager::Render(const ToolRenderContext& renderCtx) {
    if (m_activeTool && m_activeTool->IsActive()) {
        m_activeTool->Render(renderCtx);
    }
}

void EditorToolManager::RenderOverlay(ImDrawList* drawList, const ToolContext& ctx) {
    if (m_activeTool && m_activeTool->IsActive()) {
        m_activeTool->RenderOverlay(drawList, ctx);
    }
}

void EditorToolManager::LoadAllSettings(const nlohmann::json& settings) {
    if (!settings.is_object()) {
        return;
    }

    for (auto it = settings.begin(); it != settings.end(); ++it) {
        m_toolSettings[it.key()] = it.value();
    }

    // Apply to active tool if it exists
    if (m_activeTool && m_toolSettings.contains(m_activeToolId)) {
        m_activeTool->LoadSettings(m_toolSettings[m_activeToolId]);
    }
}

nlohmann::json EditorToolManager::SaveAllSettings() const {
    nlohmann::json settings;

    // Include cached settings
    for (const auto& [id, toolSettings] : m_toolSettings) {
        settings[id] = toolSettings;
    }

    // Include current tool's live settings
    if (m_activeTool) {
        settings[m_activeToolId] = m_activeTool->SaveSettings();
    }

    return settings;
}

uint64_t EditorToolManager::RegisterToolChangedCallback(ToolChangedCallback callback) {
    uint64_t id = m_nextCallbackId++;
    m_callbacks.push_back({id, std::move(callback)});
    return id;
}

void EditorToolManager::UnregisterCallback(uint64_t callbackId) {
    auto it = std::remove_if(m_callbacks.begin(), m_callbacks.end(),
                             [callbackId](const CallbackEntry& entry) {
                                 return entry.id == callbackId;
                             });
    m_callbacks.erase(it, m_callbacks.end());
}

void EditorToolManager::NotifyToolChanged(const std::string& oldId, const std::string& newId) {
    for (const auto& entry : m_callbacks) {
        if (entry.callback) {
            entry.callback(oldId, newId);
        }
    }
}

} // namespace Nova
