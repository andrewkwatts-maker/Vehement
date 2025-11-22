#include "EventBusUI.hpp"
#include "../web/JSBridge.hpp"
#include "../web/WebViewManager.hpp"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <random>
#include <iomanip>

namespace Vehement {

EventBusUI::EventBusUI() = default;

EventBusUI::~EventBusUI() {
    if (m_initialized) {
        Shutdown();
    }
}

bool EventBusUI::Initialize(WebEditor::JSBridge& bridge, const Config& config) {
    if (m_initialized) {
        return false;
    }

    m_bridge = &bridge;
    m_config = config;

    // Register JSBridge functions
    RegisterBridgeFunctions();

    // Initialize performance metrics
    m_metrics.lastMetricUpdate = std::chrono::steady_clock::now();

    m_initialized = true;
    return true;
}

void EventBusUI::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clear all data
    m_events.clear();
    m_connections.clear();
    m_nodePositions.clear();
    m_eventHistory.clear();

    m_initialized = false;
}

void EventBusUI::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update refresh timer
    m_refreshTimer += deltaTime;
    if (m_refreshTimer >= m_config.autoRefreshInterval) {
        m_refreshTimer = 0.0f;
        UpdateGraphView();
    }

    // Update performance metrics
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(now - m_metrics.lastMetricUpdate).count();
    if (elapsed >= 1.0f) {
        m_metrics.eventsPerSecond = m_metrics.recentEventCount;
        m_metrics.recentEventCount = 0;
        m_metrics.lastMetricUpdate = now;
    }

    // Process paused events if stepping
    // (Handled in StepEvent())
}

void EventBusUI::Render() {
    if (!m_initialized) {
        return;
    }

    ImGui::Begin("Event Bus", nullptr, ImGuiWindowFlags_MenuBar);

    RenderMenuBar();
    RenderToolbar();

    // Main content area with splitter
    float contentHeight = ImGui::GetContentRegionAvail().y;
    float historyHeight = m_showHistory ? contentHeight * 0.3f : 0.0f;
    float mainHeight = contentHeight - historyHeight;

    // Left panel - Event list and filters
    ImGui::BeginChild("EventListPanel", ImVec2(250, mainHeight), true);
    if (m_showFilterPanel) {
        RenderFilterPanel();
        ImGui::Separator();
    }
    RenderEventList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Center panel - Connections
    ImGui::BeginChild("ConnectionPanel", ImVec2(300, mainHeight), true);
    RenderConnectionList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel - Event details
    ImGui::BeginChild("DetailsPanel", ImVec2(0, mainHeight), true);
    RenderEventDetails();
    ImGui::EndChild();

    // Bottom panel - History timeline
    if (m_showHistory) {
        ImGui::Separator();
        ImGui::BeginChild("HistoryPanel", ImVec2(0, historyHeight), true);
        RenderHistoryTimeline();
        ImGui::EndChild();
    }

    // Dialogs
    if (m_showCreateEventDialog) {
        RenderCreateEventDialog();
    }
    if (m_showExportDialog) {
        RenderExportDialog();
    }
    if (m_showImportDialog) {
        RenderImportDialog();
    }

    ImGui::End();

    // Performance metrics overlay
    if (m_config.showPerformanceMetrics) {
        RenderPerformanceMetrics();
    }
}

void EventBusUI::RenderNodeGraph(WebEditor::WebViewManager& webViewManager) {
    if (!m_initialized) {
        return;
    }

    // Create web view if not exists
    if (!webViewManager.HasWebView(m_graphWebViewId)) {
        WebEditor::WebViewConfig config;
        config.id = m_graphWebViewId;
        config.title = "Event Bus Graph";
        config.width = 800;
        config.height = 600;
        config.debug = true;

        auto* webView = webViewManager.CreateWebView(config);
        if (webView) {
            webView->LoadFile("editor/html/event_bus_graph.html");
            webView->SetMessageHandler([this](const std::string& type, const std::string& payload) {
                HandleGraphMessage(type, payload);
            });
        }
    }

    // Render the web view
    webViewManager.RenderImGuiWindow(m_graphWebViewId, "Event Flow Graph", nullptr);
}

void EventBusUI::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Export Configuration...", "Ctrl+E")) {
                m_showExportDialog = true;
            }
            if (ImGui::MenuItem("Import Configuration...", "Ctrl+I")) {
                m_showImportDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Layout", "Ctrl+S")) {
                SaveLayout(m_config.defaultLayoutPath);
            }
            if (ImGui::MenuItem("Load Layout", "Ctrl+L")) {
                LoadLayout(m_config.defaultLayoutPath);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Create Custom Event...", "Ctrl+N")) {
                m_showCreateEventDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Selected Event", "Delete", false, !m_selectedEventId.empty())) {
                DeleteCustomEvent(m_selectedEventId);
            }
            if (ImGui::MenuItem("Delete Selected Connection", nullptr, false, !m_selectedConnectionId.empty())) {
                DeleteConnection(m_selectedConnectionId);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Filter Panel", nullptr, &m_showFilterPanel);
            ImGui::MenuItem("Show History", nullptr, &m_showHistory);
            ImGui::MenuItem("Show Performance", nullptr, &m_config.showPerformanceMetrics);
            ImGui::Separator();
            if (ImGui::MenuItem("Auto Layout - Hierarchical")) {
                AutoLayout("hierarchical");
            }
            if (ImGui::MenuItem("Auto Layout - Force Directed")) {
                AutoLayout("force");
            }
            if (ImGui::MenuItem("Auto Layout - Grid")) {
                AutoLayout("grid");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            bool paused = IsEventFlowPaused();
            if (ImGui::MenuItem(paused ? "Resume" : "Pause", "Space")) {
                if (paused) {
                    ResumeEventFlow();
                } else {
                    PauseEventFlow();
                }
            }
            if (ImGui::MenuItem("Step", "F10", false, paused)) {
                StepEvent();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear History", "Ctrl+Shift+C")) {
                ClearHistory();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EventBusUI::RenderToolbar() {
    // Pause/Resume button
    if (m_eventFlowPaused) {
        if (ImGui::Button("Resume")) {
            ResumeEventFlow();
        }
        ImGui::SameLine();
        if (ImGui::Button("Step")) {
            StepEvent();
        }
    } else {
        if (ImGui::Button("Pause")) {
            PauseEventFlow();
        }
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::Button("+ New Event")) {
        m_showCreateEventDialog = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear History")) {
        ClearHistory();
    }

    ImGui::SameLine();
    ImGui::Text("| Events: %zu | Connections: %zu | History: %zu",
                m_events.size(), m_connections.size(), m_eventHistory.size());

    if (m_eventFlowPaused) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[PAUSED - %zu queued]",
                           m_pausedEvents.size());
    }
}

void EventBusUI::RenderFilterPanel() {
    ImGui::Text("Filter");

    // Search text
    static char searchBuffer[256] = "";
    if (ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer))) {
        m_filter.searchText = searchBuffer;
    }

    ImGui::Checkbox("Case Sensitive", &m_filter.caseSensitive);
    ImGui::SameLine();
    ImGui::Checkbox("Regex", &m_filter.useRegex);

    // Category filter (would be a combo/multiselect in real implementation)
    ImGui::Checkbox("Built-in", &m_filter.showBuiltIn);
    ImGui::SameLine();
    ImGui::Checkbox("Custom", &m_filter.showCustom);

    ImGui::Checkbox("Enabled", &m_filter.showEnabled);
    ImGui::SameLine();
    ImGui::Checkbox("Disabled", &m_filter.showDisabled);
}

void EventBusUI::RenderEventList() {
    ImGui::Text("Events (%zu)", m_events.size());
    ImGui::Separator();

    auto filteredEvents = GetFilteredEvents();

    for (const auto& event : filteredEvents) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (event.id == m_selectedEventId) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Color based on type
        ImVec4 color = event.isCustom ?
            ImVec4(0.4f, 0.8f, 0.4f, 1.0f) :
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

        if (!event.enabled) {
            color.w = 0.5f;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TreeNodeEx(event.name.c_str(), flags);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            m_selectedEventId = event.id;
            if (OnEventSelected) {
                OnEventSelected(event);
            }
        }

        // Drag source for connections
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("EVENT_ID", event.id.c_str(), event.id.size() + 1);
            ImGui::Text("Connect: %s", event.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target for connections
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EVENT_ID")) {
                std::string sourceId(static_cast<const char*>(payload->Data));
                if (sourceId != event.id) {
                    CreateConnection(sourceId, event.id);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("ID: %s", event.id.c_str());
            ImGui::Text("Category: %s", event.category.c_str());
            ImGui::Text("Type: %s", event.isCustom ? "Custom" : "Built-in");
            ImGui::EndTooltip();
        }
    }
}

void EventBusUI::RenderEventDetails() {
    if (m_selectedEventId.empty()) {
        ImGui::TextDisabled("Select an event to view details");
        return;
    }

    const auto* event = GetEvent(m_selectedEventId);
    if (!event) {
        ImGui::TextDisabled("Event not found");
        return;
    }

    ImGui::Text("Event Details");
    ImGui::Separator();

    ImGui::Text("Name: %s", event->name.c_str());
    ImGui::Text("ID: %s", event->id.c_str());
    ImGui::Text("Category: %s", event->category.c_str());
    ImGui::Text("Source Type: %s", event->sourceType.c_str());
    ImGui::Text("Priority: %d", event->priority);

    ImGui::Separator();
    ImGui::Text("Description:");
    ImGui::TextWrapped("%s", event->description.c_str());

    ImGui::Separator();
    ImGui::Text("Parameters:");
    for (const auto& param : event->parameters) {
        ImGui::BulletText("%s", param.c_str());
    }

    ImGui::Separator();
    ImGui::Text("Tags:");
    for (const auto& tag : event->tags) {
        ImGui::SameLine();
        ImGui::SmallButton(tag.c_str());
    }

    ImGui::Separator();

    // Connections to/from this event
    auto outConnections = GetConnectionsForEvent(m_selectedEventId, true);
    auto inConnections = GetConnectionsForEvent(m_selectedEventId, false);

    if (ImGui::CollapsingHeader("Outgoing Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& conn : outConnections) {
            const auto* target = GetEvent(conn.targetEventId);
            ImGui::BulletText("-> %s", target ? target->name.c_str() : conn.targetEventId.c_str());
        }
        if (outConnections.empty()) {
            ImGui::TextDisabled("No outgoing connections");
        }
    }

    if (ImGui::CollapsingHeader("Incoming Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& conn : inConnections) {
            const auto* source = GetEvent(conn.sourceEventId);
            ImGui::BulletText("<- %s", source ? source->name.c_str() : conn.sourceEventId.c_str());
        }
        if (inConnections.empty()) {
            ImGui::TextDisabled("No incoming connections");
        }
    }

    // Actions
    ImGui::Separator();
    if (event->isCustom) {
        if (ImGui::Button("Delete Event")) {
            DeleteCustomEvent(m_selectedEventId);
            m_selectedEventId.clear();
        }
    }
}

void EventBusUI::RenderConnectionList() {
    ImGui::Text("Connections (%zu)", m_connections.size());
    ImGui::Separator();

    for (const auto& [id, conn] : m_connections) {
        const auto* source = GetEvent(conn.sourceEventId);
        const auto* target = GetEvent(conn.targetEventId);

        std::string label = (source ? source->name : conn.sourceEventId) +
                           " -> " +
                           (target ? target->name : conn.targetEventId);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (id == m_selectedConnectionId) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImVec4 color = conn.enabled ?
            ImVec4(conn.color.r, conn.color.g, conn.color.b, conn.color.a) :
            ImVec4(0.5f, 0.5f, 0.5f, 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TreeNodeEx(label.c_str(), flags);
        ImGui::PopStyleColor();

        if (ImGui::IsItemClicked()) {
            m_selectedConnectionId = id;
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
                DeleteConnection(id);
            }
            bool enabled = conn.enabled;
            if (ImGui::MenuItem("Enabled", nullptr, &enabled)) {
                SetConnectionEnabled(id, enabled);
            }
            ImGui::EndPopup();
        }
    }
}

void EventBusUI::RenderHistoryTimeline() {
    ImGui::Text("Event History (%zu)", m_eventHistory.size());
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        ClearHistory();
    }
    ImGui::Separator();

    // Columns: Time, Event, Source, Duration, Status
    if (ImGui::BeginTable("HistoryTable", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        // Show most recent first
        for (auto it = m_eventHistory.rbegin(); it != m_eventHistory.rend(); ++it) {
            const auto& entry = *it;

            ImGui::TableNextRow();

            // Time
            ImGui::TableNextColumn();
            auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                entry.timestamp.time_since_epoch()).count() % 86400000;
            int hours = static_cast<int>(time / 3600000);
            int mins = static_cast<int>((time % 3600000) / 60000);
            int secs = static_cast<int>((time % 60000) / 1000);
            int ms = static_cast<int>(time % 1000);
            ImGui::Text("%02d:%02d:%02d.%03d", hours, mins, secs, ms);

            // Event name
            ImGui::TableNextColumn();
            ImGui::Text("%s", entry.eventName.c_str());

            // Source
            ImGui::TableNextColumn();
            ImGui::Text("%s:%s", entry.sourceType.c_str(), entry.sourceId.c_str());

            // Duration
            ImGui::TableNextColumn();
            ImGui::Text("%.2f ms", entry.duration);

            // Status
            ImGui::TableNextColumn();
            if (entry.success) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "OK");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ERR");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", entry.errorMessage.c_str());
                    ImGui::EndTooltip();
                }
            }
        }

        ImGui::EndTable();
    }
}

void EventBusUI::RenderPerformanceMetrics() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("Event Bus Metrics", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                 ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing |
                 ImGuiWindowFlags_NoNav);

    ImGui::Text("Events/sec: %zu", m_metrics.eventsPerSecond);
    ImGui::Text("Total processed: %zu", m_metrics.totalEventsProcessed);
    ImGui::Text("Avg processing: %.2f ms", m_metrics.averageProcessingTime);

    ImGui::End();
}

void EventBusUI::RenderCreateEventDialog() {
    ImGui::OpenPopup("Create Custom Event");
    if (ImGui::BeginPopupModal("Create Custom Event", &m_showCreateEventDialog)) {
        ImGui::InputText("Name", m_newEventName, sizeof(m_newEventName));
        ImGui::InputText("Category", m_newEventCategory, sizeof(m_newEventCategory));
        ImGui::InputTextMultiline("Description", m_newEventDescription, sizeof(m_newEventDescription));

        ImGui::Separator();
        ImGui::Text("Parameters:");

        static char newParam[128] = "";
        if (ImGui::InputText("##NewParam", newParam, sizeof(newParam), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (strlen(newParam) > 0) {
                m_newEventParameters.push_back(newParam);
                newParam[0] = '\0';
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Parameter")) {
            if (strlen(newParam) > 0) {
                m_newEventParameters.push_back(newParam);
                newParam[0] = '\0';
            }
        }

        for (size_t i = 0; i < m_newEventParameters.size(); ++i) {
            ImGui::BulletText("%s", m_newEventParameters[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("X")) {
                m_newEventParameters.erase(m_newEventParameters.begin() + static_cast<ptrdiff_t>(i));
            }
            ImGui::PopID();
        }

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            CreateCustomEvent(m_newEventName, m_newEventCategory,
                             m_newEventDescription, m_newEventParameters);

            // Reset form
            m_newEventName[0] = '\0';
            m_newEventCategory[0] = '\0';
            m_newEventDescription[0] = '\0';
            m_newEventParameters.clear();
            m_showCreateEventDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showCreateEventDialog = false;
        }

        ImGui::EndPopup();
    }
}

void EventBusUI::RenderExportDialog() {
    ImGui::OpenPopup("Export Configuration");
    if (ImGui::BeginPopupModal("Export Configuration", &m_showExportDialog)) {
        static char exportPath[512] = "config/event_bus_config.json";
        static bool includeLayout = true;

        ImGui::InputText("Path", exportPath, sizeof(exportPath));
        ImGui::Checkbox("Include Layout", &includeLayout);

        ImGui::Separator();

        if (ImGui::Button("Export", ImVec2(120, 0))) {
            ExportConfiguration(exportPath, includeLayout);
            m_showExportDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showExportDialog = false;
        }

        ImGui::EndPopup();
    }
}

void EventBusUI::RenderImportDialog() {
    ImGui::OpenPopup("Import Configuration");
    if (ImGui::BeginPopupModal("Import Configuration", &m_showImportDialog)) {
        static char importPath[512] = "";

        ImGui::InputText("Path", importPath, sizeof(importPath));

        ImGui::Separator();

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            ImportConfiguration(importPath);
            m_showImportDialog = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showImportDialog = false;
        }

        ImGui::EndPopup();
    }
}

// Event Management
void EventBusUI::RegisterEvent(const EventInfo& event) {
    m_events[event.id] = event;
}

void EventBusUI::UnregisterEvent(const std::string& eventId) {
    m_events.erase(eventId);

    // Remove connections involving this event
    std::vector<std::string> connectionsToRemove;
    for (const auto& [id, conn] : m_connections) {
        if (conn.sourceEventId == eventId || conn.targetEventId == eventId) {
            connectionsToRemove.push_back(id);
        }
    }
    for (const auto& id : connectionsToRemove) {
        m_connections.erase(id);
    }
}

std::vector<EventInfo> EventBusUI::GetRegisteredEvents() const {
    std::vector<EventInfo> result;
    result.reserve(m_events.size());
    for (const auto& [id, event] : m_events) {
        result.push_back(event);
    }
    return result;
}

const EventInfo* EventBusUI::GetEvent(const std::string& eventId) const {
    auto it = m_events.find(eventId);
    return it != m_events.end() ? &it->second : nullptr;
}

EventInfo EventBusUI::CreateCustomEvent(const std::string& name,
                                          const std::string& category,
                                          const std::string& description,
                                          const std::vector<std::string>& parameters) {
    EventInfo event;
    event.id = GenerateEventId();
    event.name = name;
    event.category = category;
    event.description = description;
    event.parameters = parameters;
    event.isCustom = true;
    event.enabled = true;

    m_events[event.id] = event;

    if (OnEventCreated) {
        OnEventCreated(event);
    }

    return event;
}

bool EventBusUI::DeleteCustomEvent(const std::string& eventId) {
    auto it = m_events.find(eventId);
    if (it == m_events.end() || !it->second.isCustom) {
        return false;
    }

    UnregisterEvent(eventId);
    return true;
}

// Connection Management
EventConnection EventBusUI::CreateConnection(const std::string& sourceEventId,
                                               const std::string& targetEventId,
                                               const std::string& conditionId) {
    EventConnection conn;
    conn.id = GenerateConnectionId();
    conn.sourceEventId = sourceEventId;
    conn.targetEventId = targetEventId;
    conn.conditionId = conditionId;
    conn.enabled = true;

    m_connections[conn.id] = conn;

    if (OnConnectionCreated) {
        OnConnectionCreated(conn);
    }

    return conn;
}

void EventBusUI::DeleteConnection(const std::string& connectionId) {
    if (m_connections.erase(connectionId) > 0) {
        if (OnConnectionDeleted) {
            OnConnectionDeleted(connectionId);
        }
    }
}

std::vector<EventConnection> EventBusUI::GetConnections() const {
    std::vector<EventConnection> result;
    result.reserve(m_connections.size());
    for (const auto& [id, conn] : m_connections) {
        result.push_back(conn);
    }
    return result;
}

std::vector<EventConnection> EventBusUI::GetConnectionsForEvent(
    const std::string& eventId, bool asSource) const {

    std::vector<EventConnection> result;
    for (const auto& [id, conn] : m_connections) {
        if (asSource && conn.sourceEventId == eventId) {
            result.push_back(conn);
        } else if (!asSource && conn.targetEventId == eventId) {
            result.push_back(conn);
        }
    }
    return result;
}

void EventBusUI::SetConnectionEnabled(const std::string& connectionId, bool enabled) {
    auto it = m_connections.find(connectionId);
    if (it != m_connections.end()) {
        it->second.enabled = enabled;
    }
}

// Node Graph Layout
void EventBusUI::SetNodePosition(const std::string& eventId, float x, float y) {
    m_nodePositions[eventId] = {eventId, x, y, false};
}

glm::vec2 EventBusUI::GetNodePosition(const std::string& eventId) const {
    auto it = m_nodePositions.find(eventId);
    if (it != m_nodePositions.end()) {
        return {it->second.x, it->second.y};
    }
    return {0.0f, 0.0f};
}

void EventBusUI::AutoLayout(const std::string& algorithm) {
    // Send message to web view to trigger auto-layout
    if (m_bridge) {
        WebEditor::JSValue data;
        data["algorithm"] = algorithm;
        m_bridge->EmitEvent("autoLayout", data);
    }
}

bool EventBusUI::SaveLayout(const std::string& path) {
    std::string json = ExportToJson();
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << json;
    return true;
}

bool EventBusUI::LoadLayout(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ImportFromJson(buffer.str());
}

// Event Monitoring
void EventBusUI::LogEvent(const EventHistoryEntry& entry) {
    m_eventHistory.push_back(entry);

    // Trim history if needed
    while (m_eventHistory.size() > static_cast<size_t>(m_config.maxHistorySize)) {
        m_eventHistory.pop_front();
    }

    // Update metrics
    m_metrics.totalEventsProcessed++;
    m_metrics.recentEventCount++;

    // Update average processing time
    float alpha = 0.1f;
    m_metrics.averageProcessingTime = alpha * entry.duration +
                                      (1.0f - alpha) * m_metrics.averageProcessingTime;

    if (OnEventLogged) {
        OnEventLogged(entry);
    }
}

std::vector<EventHistoryEntry> EventBusUI::GetEventHistory(size_t maxEntries) const {
    std::vector<EventHistoryEntry> result;
    size_t count = std::min(maxEntries, m_eventHistory.size());
    result.reserve(count);

    auto it = m_eventHistory.rbegin();
    for (size_t i = 0; i < count && it != m_eventHistory.rend(); ++i, ++it) {
        result.push_back(*it);
    }

    return result;
}

void EventBusUI::ClearHistory() {
    m_eventHistory.clear();
}

void EventBusUI::PauseEventFlow() {
    m_eventFlowPaused = true;
    if (OnFlowPaused) {
        OnFlowPaused();
    }
}

void EventBusUI::ResumeEventFlow() {
    m_eventFlowPaused = false;

    // Process queued events
    while (!m_pausedEvents.empty()) {
        LogEvent(m_pausedEvents.front());
        m_pausedEvents.pop_front();
    }

    if (OnFlowResumed) {
        OnFlowResumed();
    }
}

void EventBusUI::StepEvent() {
    if (!m_pausedEvents.empty()) {
        LogEvent(m_pausedEvents.front());
        m_pausedEvents.pop_front();
    }
}

// Filtering
void EventBusUI::SetFilter(const EventFilter& filter) {
    m_filter = filter;
}

std::vector<EventInfo> EventBusUI::GetFilteredEvents() const {
    std::vector<EventInfo> result;

    for (const auto& [id, event] : m_events) {
        if (MatchesFilter(event)) {
            result.push_back(event);
        }
    }

    // Sort by name
    std::sort(result.begin(), result.end(), [](const EventInfo& a, const EventInfo& b) {
        return a.name < b.name;
    });

    return result;
}

bool EventBusUI::MatchesFilter(const EventInfo& event) const {
    // Check type filters
    if (event.isCustom && !m_filter.showCustom) return false;
    if (!event.isCustom && !m_filter.showBuiltIn) return false;

    // Check enabled filter
    if (event.enabled && !m_filter.showEnabled) return false;
    if (!event.enabled && !m_filter.showDisabled) return false;

    // Check search text
    if (!m_filter.searchText.empty()) {
        std::string searchIn = event.name + " " + event.category + " " + event.description;

        if (m_filter.useRegex) {
            try {
                std::regex pattern(m_filter.searchText,
                                   m_filter.caseSensitive ?
                                       std::regex_constants::ECMAScript :
                                       std::regex_constants::icase);
                if (!std::regex_search(searchIn, pattern)) {
                    return false;
                }
            } catch (...) {
                return false;
            }
        } else {
            std::string needle = m_filter.searchText;
            std::string haystack = searchIn;

            if (!m_filter.caseSensitive) {
                std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
                std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            }

            if (haystack.find(needle) == std::string::npos) {
                return false;
            }
        }
    }

    // Check category filter
    if (!m_filter.categories.empty()) {
        bool found = false;
        for (const auto& cat : m_filter.categories) {
            if (event.category == cat) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

// Export/Import
bool EventBusUI::ExportConfiguration(const std::string& path, bool includeLayout) {
    std::string json = ExportToJson();
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << json;
    return true;
}

bool EventBusUI::ImportConfiguration(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ImportFromJson(buffer.str());
}

std::string EventBusUI::ExportToJson() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"1.0\",\n";

    // Events
    ss << "  \"events\": [\n";
    bool first = true;
    for (const auto& [id, event] : m_events) {
        if (event.isCustom) {
            if (!first) ss << ",\n";
            first = false;
            ss << "    {\n";
            ss << "      \"id\": \"" << event.id << "\",\n";
            ss << "      \"name\": \"" << event.name << "\",\n";
            ss << "      \"category\": \"" << event.category << "\",\n";
            ss << "      \"description\": \"" << event.description << "\",\n";
            ss << "      \"enabled\": " << (event.enabled ? "true" : "false") << ",\n";
            ss << "      \"priority\": " << event.priority << ",\n";
            ss << "      \"parameters\": [";
            for (size_t i = 0; i < event.parameters.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << "\"" << event.parameters[i] << "\"";
            }
            ss << "]\n";
            ss << "    }";
        }
    }
    ss << "\n  ],\n";

    // Connections
    ss << "  \"connections\": [\n";
    first = true;
    for (const auto& [id, conn] : m_connections) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\n";
        ss << "      \"id\": \"" << conn.id << "\",\n";
        ss << "      \"sourceEventId\": \"" << conn.sourceEventId << "\",\n";
        ss << "      \"targetEventId\": \"" << conn.targetEventId << "\",\n";
        ss << "      \"conditionId\": \"" << conn.conditionId << "\",\n";
        ss << "      \"enabled\": " << (conn.enabled ? "true" : "false") << "\n";
        ss << "    }";
    }
    ss << "\n  ],\n";

    // Layout
    ss << "  \"layout\": [\n";
    first = true;
    for (const auto& [id, pos] : m_nodePositions) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\n";
        ss << "      \"eventId\": \"" << pos.eventId << "\",\n";
        ss << "      \"x\": " << pos.x << ",\n";
        ss << "      \"y\": " << pos.y << "\n";
        ss << "    }";
    }
    ss << "\n  ]\n";

    ss << "}\n";
    return ss.str();
}

bool EventBusUI::ImportFromJson(const std::string& json) {
    // Simple JSON parsing - in production use a proper JSON library
    // This is a placeholder for the implementation
    return true;
}

// Private helpers
void EventBusUI::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    m_bridge->RegisterFunction("eventBus.getEvents", [this](const std::vector<WebEditor::JSValue>&) {
        WebEditor::JSValue::Array events;
        for (const auto& [id, event] : m_events) {
            WebEditor::JSValue::Object obj;
            obj["id"] = event.id;
            obj["name"] = event.name;
            obj["category"] = event.category;
            obj["isCustom"] = event.isCustom;
            obj["enabled"] = event.enabled;
            events.push_back(WebEditor::JSValue(std::move(obj)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(events)));
    });

    m_bridge->RegisterFunction("eventBus.getConnections", [this](const std::vector<WebEditor::JSValue>&) {
        WebEditor::JSValue::Array connections;
        for (const auto& [id, conn] : m_connections) {
            WebEditor::JSValue::Object obj;
            obj["id"] = conn.id;
            obj["sourceEventId"] = conn.sourceEventId;
            obj["targetEventId"] = conn.targetEventId;
            obj["enabled"] = conn.enabled;
            connections.push_back(WebEditor::JSValue(std::move(obj)));
        }
        return WebEditor::JSResult::Success(WebEditor::JSValue(std::move(connections)));
    });

    m_bridge->RegisterFunction("eventBus.createConnection", [this](const std::vector<WebEditor::JSValue>& args) {
        if (args.size() < 2) {
            return WebEditor::JSResult::Error("Missing arguments");
        }
        auto conn = CreateConnection(args[0].GetString(), args[1].GetString());
        return WebEditor::JSResult::Success(WebEditor::JSValue(conn.id));
    });

    m_bridge->RegisterFunction("eventBus.deleteConnection", [this](const std::vector<WebEditor::JSValue>& args) {
        if (args.empty()) {
            return WebEditor::JSResult::Error("Missing connection ID");
        }
        DeleteConnection(args[0].GetString());
        return WebEditor::JSResult::Success();
    });

    m_bridge->RegisterFunction("eventBus.setNodePosition", [this](const std::vector<WebEditor::JSValue>& args) {
        if (args.size() < 3) {
            return WebEditor::JSResult::Error("Missing arguments");
        }
        SetNodePosition(args[0].GetString(),
                       static_cast<float>(args[1].GetNumber()),
                       static_cast<float>(args[2].GetNumber()));
        return WebEditor::JSResult::Success();
    });
}

void EventBusUI::UpdateGraphView() {
    SyncGraphToWeb();
}

void EventBusUI::SyncGraphToWeb() {
    if (!m_bridge) return;

    // Send current state to web view
    m_bridge->EmitEvent("eventBus.sync", WebEditor::JSValue());
}

void EventBusUI::HandleGraphMessage(const std::string& type, const std::string& payload) {
    if (type == "nodeSelected") {
        m_selectedEventId = payload;
    } else if (type == "connectionCreated") {
        // Parse payload for source and target
    } else if (type == "connectionDeleted") {
        DeleteConnection(payload);
    } else if (type == "layoutChanged") {
        // Update node positions from payload
    }
}

std::string EventBusUI::GenerateEventId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "evt_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string EventBusUI::GenerateConnectionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << "conn_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

} // namespace Vehement
