#pragma once

#include "../../../../engine/networking/ReplicationSystem.hpp"
#include "../../../../engine/networking/FirebasePersistence.hpp"
#include <string>
#include <vector>
#include <functional>

namespace Vehement {

class Editor;

/**
 * @brief Editor for configuring replication and persistence settings
 *
 * Features:
 * - View/edit event type replication modes
 * - Configure persistence settings
 * - View connection status and stats
 * - Monitor event flow
 * - Configure Firebase settings
 */
class ReplicationEditor {
public:
    ReplicationEditor();
    ~ReplicationEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(Editor* editor);

    /**
     * @brief Render the editor UI
     */
    void Render();

    /**
     * @brief Update the editor
     */
    void Update(float deltaTime);

private:
    // UI Panels
    void RenderEventTypePanel();
    void RenderConnectionPanel();
    void RenderStatsPanel();
    void RenderEventMonitor();
    void RenderFirebasePanel();
    void RenderPersistencePanel();
    void RenderEntityOwnershipPanel();
    void RenderOverridesPanel();

    // Event type helpers
    struct EventTypeDisplay {
        std::string name;
        Nova::ReplicationCategory category;
        Nova::ReplicationMode replicationMode;
        Nova::PersistenceMode persistenceMode;
        Nova::ReliabilityMode reliabilityMode;
        bool hasOverride = false;
    };

    std::vector<EventTypeDisplay> GetEventTypes() const;
    const char* GetCategoryName(Nova::ReplicationCategory cat) const;
    const char* GetReplicationModeName(Nova::ReplicationMode mode) const;
    const char* GetPersistenceModeName(Nova::PersistenceMode mode) const;
    const char* GetReliabilityModeName(Nova::ReliabilityMode mode) const;

    // Recent events for monitor
    struct EventLogEntry {
        uint64_t id;
        std::string type;
        Nova::ReplicationMode mode;
        bool sent;
        uint64_t timestamp;
    };

    void OnEventSent(const Nova::NetworkEvent& event);
    void OnEventReceived(const Nova::NetworkEvent& event);

    Editor* m_editor = nullptr;

    // Event log
    std::vector<EventLogEntry> m_eventLog;
    static constexpr size_t MAX_EVENT_LOG = 100;
    bool m_autoScroll = true;
    std::string m_eventFilter;

    // UI state
    bool m_showEventTypes = true;
    bool m_showConnections = true;
    bool m_showStats = true;
    bool m_showEventMonitor = true;
    bool m_showFirebase = true;
    bool m_showPersistence = true;
    bool m_showOwnership = false;
    bool m_showOverrides = true;

    // Selected items
    std::string m_selectedEventType;
    uint32_t m_selectedClientId = 0;

    // Firebase settings (for display)
    std::string m_firebaseProjectId;
    std::string m_firebaseApiKey;
    std::string m_firebaseDatabaseUrl;

    // Subscription IDs
    uint64_t m_sentSubscription = 0;
    uint64_t m_receivedSubscription = 0;

    bool m_initialized = false;
};

} // namespace Vehement
