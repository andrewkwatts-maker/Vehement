#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <deque>
#include <glm/glm.hpp>

namespace Vehement {
namespace WebEditor {
    class JSBridge;
    class WebViewManager;
}

/**
 * @brief Represents an event in the event bus system
 */
struct EventInfo {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::string sourceType;
    std::vector<std::string> parameters;
    std::vector<std::string> tags;
    bool isCustom = false;
    bool enabled = true;
    int priority = 0;
};

/**
 * @brief Represents a connection between event source and target
 */
struct EventConnection {
    std::string id;
    std::string sourceEventId;
    std::string targetEventId;
    std::string conditionId;  // Optional condition
    std::string transformExpression;  // Optional data transformation
    bool enabled = true;
    glm::vec4 color{0.5f, 0.8f, 1.0f, 1.0f};
};

/**
 * @brief Node position for visual graph editor
 */
struct EventNodePosition {
    std::string eventId;
    float x = 0.0f;
    float y = 0.0f;
    bool collapsed = false;
};

/**
 * @brief Event history entry for debugging
 */
struct EventHistoryEntry {
    uint64_t id;
    std::string eventName;
    std::string sourceId;
    std::string sourceType;
    std::string payload;  // JSON
    std::chrono::steady_clock::time_point timestamp;
    float duration = 0.0f;  // Execution time in ms
    bool success = true;
    std::string errorMessage;
    std::vector<std::string> triggeredCallbacks;
};

/**
 * @brief Filter settings for event monitoring
 */
struct EventFilter {
    std::string searchText;
    std::vector<std::string> categories;
    std::vector<std::string> sourceTypes;
    std::vector<std::string> tags;
    bool showEnabled = true;
    bool showDisabled = true;
    bool showBuiltIn = true;
    bool showCustom = true;
    bool caseSensitive = false;
    bool useRegex = false;
};

/**
 * @brief Event Bus UI
 *
 * Main event bus binding UI providing:
 * - Visual node graph for event connections
 * - Drag-drop event sources to targets
 * - Real-time event monitoring/debugging
 * - Create custom events
 * - Filter and search events
 * - Event history timeline
 * - Pause/resume event flow
 * - Export/import event configurations
 */
class EventBusUI {
public:
    /**
     * @brief Configuration for the event bus UI
     */
    struct Config {
        int maxHistorySize = 1000;
        float autoRefreshInterval = 0.1f;  // seconds
        bool enableRealTimeMonitoring = true;
        bool showPerformanceMetrics = true;
        std::string defaultLayoutPath = "config/event_bus_layout.json";
    };

    /**
     * @brief Callback types
     */
    using OnEventSelectedCallback = std::function<void(const EventInfo&)>;
    using OnConnectionCreatedCallback = std::function<void(const EventConnection&)>;
    using OnConnectionDeletedCallback = std::function<void(const std::string& connectionId)>;
    using OnEventCreatedCallback = std::function<void(const EventInfo&)>;

    EventBusUI();
    ~EventBusUI();

    // Non-copyable
    EventBusUI(const EventBusUI&) = delete;
    EventBusUI& operator=(const EventBusUI&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the event bus UI
     * @param bridge Reference to JSBridge for web view communication
     * @param config UI configuration
     * @return true if initialization succeeded
     */
    bool Initialize(WebEditor::JSBridge& bridge, const Config& config = {});

    /**
     * @brief Shutdown the event bus UI
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Rendering
    // =========================================================================

    /**
     * @brief Update UI state
     * @param deltaTime Time since last update
     */
    void Update(float deltaTime);

    /**
     * @brief Render the event bus UI (ImGui)
     */
    void Render();

    /**
     * @brief Render the visual node graph (WebView)
     * @param webViewManager Reference to WebViewManager
     */
    void RenderNodeGraph(WebEditor::WebViewManager& webViewManager);

    // =========================================================================
    // Event Management
    // =========================================================================

    /**
     * @brief Register an event
     * @param event Event information
     */
    void RegisterEvent(const EventInfo& event);

    /**
     * @brief Unregister an event
     * @param eventId Event ID
     */
    void UnregisterEvent(const std::string& eventId);

    /**
     * @brief Get all registered events
     */
    [[nodiscard]] std::vector<EventInfo> GetRegisteredEvents() const;

    /**
     * @brief Get event by ID
     * @param eventId Event ID
     * @return Event info or nullptr if not found
     */
    [[nodiscard]] const EventInfo* GetEvent(const std::string& eventId) const;

    /**
     * @brief Create a custom event
     * @param name Event name
     * @param category Event category
     * @param description Event description
     * @param parameters Parameter names
     * @return Created event info
     */
    EventInfo CreateCustomEvent(const std::string& name,
                                 const std::string& category,
                                 const std::string& description,
                                 const std::vector<std::string>& parameters);

    /**
     * @brief Delete a custom event
     * @param eventId Event ID
     * @return true if deleted
     */
    bool DeleteCustomEvent(const std::string& eventId);

    // =========================================================================
    // Connection Management
    // =========================================================================

    /**
     * @brief Create a connection between events
     * @param sourceEventId Source event ID
     * @param targetEventId Target event ID
     * @param conditionId Optional condition ID
     * @return Created connection
     */
    EventConnection CreateConnection(const std::string& sourceEventId,
                                      const std::string& targetEventId,
                                      const std::string& conditionId = "");

    /**
     * @brief Delete a connection
     * @param connectionId Connection ID
     */
    void DeleteConnection(const std::string& connectionId);

    /**
     * @brief Get all connections
     */
    [[nodiscard]] std::vector<EventConnection> GetConnections() const;

    /**
     * @brief Get connections for a specific event
     * @param eventId Event ID
     * @param asSource If true, get connections where event is source
     */
    [[nodiscard]] std::vector<EventConnection> GetConnectionsForEvent(
        const std::string& eventId, bool asSource) const;

    /**
     * @brief Enable/disable a connection
     * @param connectionId Connection ID
     * @param enabled Enable state
     */
    void SetConnectionEnabled(const std::string& connectionId, bool enabled);

    // =========================================================================
    // Node Graph Layout
    // =========================================================================

    /**
     * @brief Set node position in graph
     * @param eventId Event ID
     * @param x X position
     * @param y Y position
     */
    void SetNodePosition(const std::string& eventId, float x, float y);

    /**
     * @brief Get node position
     * @param eventId Event ID
     * @return Position or default (0,0)
     */
    [[nodiscard]] glm::vec2 GetNodePosition(const std::string& eventId) const;

    /**
     * @brief Auto-layout all nodes
     * @param algorithm Layout algorithm ("hierarchical", "force", "grid")
     */
    void AutoLayout(const std::string& algorithm = "hierarchical");

    /**
     * @brief Save layout to file
     * @param path File path
     * @return true if successful
     */
    bool SaveLayout(const std::string& path);

    /**
     * @brief Load layout from file
     * @param path File path
     * @return true if successful
     */
    bool LoadLayout(const std::string& path);

    // =========================================================================
    // Event Monitoring
    // =========================================================================

    /**
     * @brief Log an event occurrence (call from event bus)
     * @param entry History entry
     */
    void LogEvent(const EventHistoryEntry& entry);

    /**
     * @brief Get event history
     * @param maxEntries Maximum entries to return
     */
    [[nodiscard]] std::vector<EventHistoryEntry> GetEventHistory(size_t maxEntries = 100) const;

    /**
     * @brief Clear event history
     */
    void ClearHistory();

    /**
     * @brief Pause event flow (for debugging)
     */
    void PauseEventFlow();

    /**
     * @brief Resume event flow
     */
    void ResumeEventFlow();

    /**
     * @brief Check if event flow is paused
     */
    [[nodiscard]] bool IsEventFlowPaused() const { return m_eventFlowPaused; }

    /**
     * @brief Step through one event (when paused)
     */
    void StepEvent();

    // =========================================================================
    // Filtering
    // =========================================================================

    /**
     * @brief Set event filter
     * @param filter Filter settings
     */
    void SetFilter(const EventFilter& filter);

    /**
     * @brief Get current filter
     */
    [[nodiscard]] const EventFilter& GetFilter() const { return m_filter; }

    /**
     * @brief Get filtered events
     */
    [[nodiscard]] std::vector<EventInfo> GetFilteredEvents() const;

    // =========================================================================
    // Export/Import
    // =========================================================================

    /**
     * @brief Export event configuration to JSON
     * @param path File path
     * @param includeLayout Include node positions
     * @return true if successful
     */
    bool ExportConfiguration(const std::string& path, bool includeLayout = true);

    /**
     * @brief Import event configuration from JSON
     * @param path File path
     * @return true if successful
     */
    bool ImportConfiguration(const std::string& path);

    /**
     * @brief Export to JSON string
     */
    [[nodiscard]] std::string ExportToJson() const;

    /**
     * @brief Import from JSON string
     * @param json JSON string
     * @return true if successful
     */
    bool ImportFromJson(const std::string& json);

    // =========================================================================
    // Callbacks
    // =========================================================================

    OnEventSelectedCallback OnEventSelected;
    OnConnectionCreatedCallback OnConnectionCreated;
    OnConnectionDeletedCallback OnConnectionDeleted;
    OnEventCreatedCallback OnEventCreated;

    std::function<void()> OnFlowPaused;
    std::function<void()> OnFlowResumed;
    std::function<void(const EventHistoryEntry&)> OnEventLogged;

private:
    // ImGui rendering helpers
    void RenderMenuBar();
    void RenderToolbar();
    void RenderEventList();
    void RenderEventDetails();
    void RenderHistoryTimeline();
    void RenderConnectionList();
    void RenderFilterPanel();
    void RenderPerformanceMetrics();
    void RenderCreateEventDialog();
    void RenderExportDialog();
    void RenderImportDialog();

    // Graph update helpers
    void UpdateGraphView();
    void SyncGraphToWeb();
    void HandleGraphMessage(const std::string& type, const std::string& payload);

    // JSBridge registration
    void RegisterBridgeFunctions();

    // Utility functions
    std::string GenerateEventId();
    std::string GenerateConnectionId();
    bool MatchesFilter(const EventInfo& event) const;

    // State
    bool m_initialized = false;
    Config m_config;
    WebEditor::JSBridge* m_bridge = nullptr;

    // Events and connections
    std::unordered_map<std::string, EventInfo> m_events;
    std::unordered_map<std::string, EventConnection> m_connections;
    std::unordered_map<std::string, EventNodePosition> m_nodePositions;

    // Event history
    std::deque<EventHistoryEntry> m_eventHistory;
    uint64_t m_nextHistoryId = 1;
    bool m_eventFlowPaused = false;
    std::deque<EventHistoryEntry> m_pausedEvents;

    // Filter
    EventFilter m_filter;

    // Selection state
    std::string m_selectedEventId;
    std::string m_selectedConnectionId;

    // UI state
    bool m_showCreateEventDialog = false;
    bool m_showExportDialog = false;
    bool m_showImportDialog = false;
    bool m_showFilterPanel = true;
    bool m_showHistory = true;
    float m_refreshTimer = 0.0f;

    // Create event dialog state
    char m_newEventName[256] = "";
    char m_newEventCategory[128] = "";
    char m_newEventDescription[512] = "";
    std::vector<std::string> m_newEventParameters;

    // Performance metrics
    struct PerformanceMetrics {
        size_t totalEventsProcessed = 0;
        size_t eventsPerSecond = 0;
        float averageProcessingTime = 0.0f;
        std::chrono::steady_clock::time_point lastMetricUpdate;
        size_t recentEventCount = 0;
    };
    PerformanceMetrics m_metrics;

    // Web view ID for graph editor
    std::string m_graphWebViewId = "event_bus_graph";
};

} // namespace Vehement
