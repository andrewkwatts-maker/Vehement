#pragma once

#include "engine/animation/AnimationEventSystem.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Vehement {

using Nova::json;
using Nova::AnimationEventData;

/**
 * @brief Event marker on the timeline
 */
struct EventMarker {
    std::string id;
    std::string name;
    std::string eventType;
    float time = 0.0f;          // Normalized time (0-1) within animation
    float absoluteTime = 0.0f;  // Absolute time in seconds
    bool selected = false;
    bool dragging = false;
    uint32_t color = 0xFF8844FF;
    json data;
};

/**
 * @brief Track for organizing events
 */
struct EventTrack {
    std::string id;
    std::string name;
    std::string category;  // "sound", "vfx", "gameplay", "custom"
    bool visible = true;
    bool locked = false;
    bool expanded = true;
    uint32_t color = 0x4488FFFF;
    std::vector<std::string> eventIds;
};

/**
 * @brief Keyframe for animation preview
 */
struct PreviewKeyframe {
    float time;
    bool active = false;
    std::vector<std::string> triggeredEvents;
};

/**
 * @brief Editor action for undo/redo
 */
struct EventEditorAction {
    enum class Type {
        AddEvent,
        RemoveEvent,
        MoveEvent,
        ModifyEvent,
        AddTrack,
        RemoveTrack,
        ModifyTrack
    };

    Type type;
    json beforeData;
    json afterData;
    std::string targetId;
};

/**
 * @brief Animation timeline configuration
 */
struct TimelineConfig {
    float duration = 1.0f;          // Animation duration in seconds
    float frameRate = 30.0f;        // Display frame rate
    float pixelsPerSecond = 200.0f; // Timeline scale
    float trackHeight = 30.0f;
    float headerWidth = 150.0f;
    bool showFrameNumbers = true;
    bool showTimeSeconds = true;
    bool snapToFrames = true;
    bool loopPreview = true;
};

/**
 * @brief Visual animation event editor
 *
 * Features:
 * - Frame-by-frame timeline
 * - Drag markers for events
 * - Event property editor
 * - Preview with event highlights
 * - Multiple tracks for organization
 * - Undo/redo support
 */
class AnimationEventEditor {
public:
    struct Config {
        TimelineConfig timeline;
        glm::vec2 viewSize{800.0f, 400.0f};
        float zoomMin = 0.1f;
        float zoomMax = 10.0f;
    };

    AnimationEventEditor();
    ~AnimationEventEditor();

    /**
     * @brief Initialize the editor
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Load events for an animation
     */
    bool LoadEvents(const std::string& filepath);
    bool LoadEvents(const json& eventData);

    /**
     * @brief Save events
     */
    bool SaveEvents(const std::string& filepath);
    bool SaveEvents();

    /**
     * @brief Clear all events
     */
    void ClearEvents();

    /**
     * @brief Export to JSON
     */
    [[nodiscard]] json ExportToJson() const;

    /**
     * @brief Import from JSON
     */
    bool ImportFromJson(const json& data);

    // -------------------------------------------------------------------------
    // Animation Properties
    // -------------------------------------------------------------------------

    /**
     * @brief Set animation duration
     */
    void SetAnimationDuration(float duration);

    /**
     * @brief Get animation duration
     */
    [[nodiscard]] float GetAnimationDuration() const { return m_config.timeline.duration; }

    /**
     * @brief Set frame rate
     */
    void SetFrameRate(float fps);

    /**
     * @brief Get frame rate
     */
    [[nodiscard]] float GetFrameRate() const { return m_config.timeline.frameRate; }

    /**
     * @brief Get total frame count
     */
    [[nodiscard]] int GetTotalFrames() const;

    // -------------------------------------------------------------------------
    // Event Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add event at time
     */
    EventMarker* AddEvent(const std::string& eventType, float time,
                          const std::string& trackId = "");

    /**
     * @brief Remove event by ID
     */
    bool RemoveEvent(const std::string& id);

    /**
     * @brief Get event by ID
     */
    [[nodiscard]] EventMarker* GetEvent(const std::string& id);

    /**
     * @brief Get all events
     */
    [[nodiscard]] const std::vector<EventMarker>& GetEvents() const { return m_events; }

    /**
     * @brief Get events at specific time
     */
    [[nodiscard]] std::vector<EventMarker*> GetEventsAtTime(float time, float tolerance = 0.01f);

    /**
     * @brief Get events in time range
     */
    [[nodiscard]] std::vector<EventMarker*> GetEventsInRange(float startTime, float endTime);

    /**
     * @brief Move event to new time
     */
    void MoveEvent(const std::string& id, float newTime);

    /**
     * @brief Set event data
     */
    void SetEventData(const std::string& id, const json& data);

    /**
     * @brief Duplicate event
     */
    EventMarker* DuplicateEvent(const std::string& id, float newTime);

    // -------------------------------------------------------------------------
    // Track Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add track
     */
    EventTrack* AddTrack(const std::string& name, const std::string& category = "custom");

    /**
     * @brief Remove track
     */
    bool RemoveTrack(const std::string& id);

    /**
     * @brief Get track by ID
     */
    [[nodiscard]] EventTrack* GetTrack(const std::string& id);

    /**
     * @brief Get all tracks
     */
    [[nodiscard]] const std::vector<EventTrack>& GetTracks() const { return m_tracks; }

    /**
     * @brief Move event to track
     */
    bool MoveEventToTrack(const std::string& eventId, const std::string& trackId);

    /**
     * @brief Set track visibility
     */
    void SetTrackVisible(const std::string& id, bool visible);

    /**
     * @brief Set track locked state
     */
    void SetTrackLocked(const std::string& id, bool locked);

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Select event
     */
    void SelectEvent(const std::string& id);

    /**
     * @brief Select multiple events
     */
    void SelectEvents(const std::vector<std::string>& ids);

    /**
     * @brief Add to selection
     */
    void AddToSelection(const std::string& id);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected event IDs
     */
    [[nodiscard]] const std::vector<std::string>& GetSelectedEventIds() const {
        return m_selectedEventIds;
    }

    /**
     * @brief Select events in time range (marquee selection)
     */
    void SelectEventsInRange(float startTime, float endTime,
                             const std::string& trackId = "");

    // -------------------------------------------------------------------------
    // Timeline Navigation
    // -------------------------------------------------------------------------

    /**
     * @brief Set current playhead time
     */
    void SetPlayheadTime(float time);

    /**
     * @brief Get current playhead time
     */
    [[nodiscard]] float GetPlayheadTime() const { return m_playheadTime; }

    /**
     * @brief Set view offset
     */
    void SetViewOffset(float offset);

    /**
     * @brief Get view offset
     */
    [[nodiscard]] float GetViewOffset() const { return m_viewOffset; }

    /**
     * @brief Set zoom level
     */
    void SetZoom(float zoom);

    /**
     * @brief Get zoom level
     */
    [[nodiscard]] float GetZoom() const { return m_zoom; }

    /**
     * @brief Zoom to fit all events
     */
    void ZoomToFit();

    /**
     * @brief Center view on time
     */
    void CenterOnTime(float time);

    /**
     * @brief Go to frame
     */
    void GoToFrame(int frame);

    /**
     * @brief Get frame at time
     */
    [[nodiscard]] int TimeToFrame(float time) const;

    /**
     * @brief Get time at frame
     */
    [[nodiscard]] float FrameToTime(int frame) const;

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle mouse click
     */
    void OnMouseDown(const glm::vec2& position, int button, bool shift, bool ctrl);

    /**
     * @brief Handle mouse release
     */
    void OnMouseUp(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& position);

    /**
     * @brief Handle mouse double click
     */
    void OnMouseDoubleClick(const glm::vec2& position, int button);

    /**
     * @brief Handle key press
     */
    void OnKeyDown(int key, bool shift, bool ctrl);

    /**
     * @brief Handle scroll
     */
    void OnScroll(float delta, bool horizontal);

    // -------------------------------------------------------------------------
    // Clipboard
    // -------------------------------------------------------------------------

    /**
     * @brief Copy selected events
     */
    void CopySelected();

    /**
     * @brief Cut selected events
     */
    void CutSelected();

    /**
     * @brief Paste events at playhead
     */
    void Paste();

    /**
     * @brief Check if clipboard has content
     */
    [[nodiscard]] bool HasClipboardContent() const { return !m_clipboard.empty(); }

    // -------------------------------------------------------------------------
    // Undo/Redo
    // -------------------------------------------------------------------------

    /**
     * @brief Undo last action
     */
    void Undo();

    /**
     * @brief Redo last undone action
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }

    // -------------------------------------------------------------------------
    // Preview
    // -------------------------------------------------------------------------

    /**
     * @brief Start playback preview
     */
    void Play();

    /**
     * @brief Pause playback
     */
    void Pause();

    /**
     * @brief Stop playback (reset to start)
     */
    void Stop();

    /**
     * @brief Check if playing
     */
    [[nodiscard]] bool IsPlaying() const { return m_playing; }

    /**
     * @brief Update preview
     */
    void UpdatePreview(float deltaTime);

    /**
     * @brief Set playback speed
     */
    void SetPlaybackSpeed(float speed);

    /**
     * @brief Get playback speed
     */
    [[nodiscard]] float GetPlaybackSpeed() const { return m_playbackSpeed; }

    /**
     * @brief Step forward one frame
     */
    void StepForward();

    /**
     * @brief Step backward one frame
     */
    void StepBackward();

    /**
     * @brief Get events triggered since last update
     */
    [[nodiscard]] std::vector<EventMarker*> GetTriggeredEvents();

    // -------------------------------------------------------------------------
    // Event Templates
    // -------------------------------------------------------------------------

    /**
     * @brief Get available event types
     */
    [[nodiscard]] std::vector<std::string> GetEventTypes() const;

    /**
     * @brief Get event template
     */
    [[nodiscard]] json GetEventTemplate(const std::string& type) const;

    /**
     * @brief Register event template
     */
    void RegisterEventTemplate(const std::string& type, const json& templateData);

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Callback when selection changes
     */
    std::function<void(const std::vector<std::string>&)> OnSelectionChanged;

    /**
     * @brief Callback when timeline is modified
     */
    std::function<void()> OnModified;

    /**
     * @brief Callback when playhead moves
     */
    std::function<void(float)> OnPlayheadChanged;

    /**
     * @brief Callback when event is triggered during preview
     */
    std::function<void(const EventMarker&)> OnEventTriggered;

    /**
     * @brief Callback to request event property editor
     */
    std::function<void(EventMarker*)> OnEventSelected;

    // -------------------------------------------------------------------------
    // Dirty State
    // -------------------------------------------------------------------------

    [[nodiscard]] bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }

private:
    void RecordAction(EventEditorAction::Type type, const std::string& target,
                      const json& before, const json& after);
    EventMarker* FindEventAt(const glm::vec2& position);
    EventTrack* FindTrackAt(const glm::vec2& position);
    std::string GenerateEventId();
    std::string GenerateTrackId();
    float ScreenToTime(float screenX) const;
    float TimeToScreen(float time) const;
    float SnapToFrame(float time) const;
    void UpdateTriggeredEvents(float previousTime, float currentTime);
    uint32_t GetEventTypeColor(const std::string& type) const;

    Config m_config;
    std::string m_filePath;

    // Events and tracks
    std::vector<EventMarker> m_events;
    std::vector<EventTrack> m_tracks;

    // Event templates
    std::map<std::string, json> m_eventTemplates;

    // Selection
    std::vector<std::string> m_selectedEventIds;

    // Timeline view
    float m_playheadTime = 0.0f;
    float m_viewOffset = 0.0f;
    float m_zoom = 1.0f;

    // Interaction state
    bool m_dragging = false;
    bool m_scrubbing = false;
    bool m_marqueeSelecting = false;
    glm::vec2 m_dragStart{0.0f};
    float m_dragStartTime = 0.0f;
    glm::vec2 m_marqueeStart{0.0f};
    glm::vec2 m_marqueeEnd{0.0f};

    // Playback
    bool m_playing = false;
    float m_playbackSpeed = 1.0f;
    float m_lastPlayheadTime = 0.0f;
    std::vector<EventMarker*> m_triggeredEvents;

    // Clipboard
    std::vector<json> m_clipboard;
    float m_clipboardBaseTime = 0.0f;

    // Undo/Redo
    std::vector<EventEditorAction> m_undoStack;
    std::vector<EventEditorAction> m_redoStack;
    static constexpr size_t MAX_UNDO_SIZE = 100;

    // ID counters
    uint32_t m_eventIdCounter = 0;
    uint32_t m_trackIdCounter = 0;

    // State
    bool m_dirty = false;
    bool m_initialized = false;
};

/**
 * @brief Event properties panel
 */
class EventPropertiesPanel {
public:
    EventPropertiesPanel() = default;

    /**
     * @brief Set event to edit
     */
    void SetEvent(EventMarker* event);

    /**
     * @brief Render panel (returns true if modified)
     */
    bool Render();

    /**
     * @brief Get modified event data
     */
    [[nodiscard]] EventMarker GetModifiedEvent() const;

    std::function<void(const EventMarker&)> OnEventModified;

private:
    EventMarker* m_event = nullptr;
    EventMarker m_editEvent;
};

/**
 * @brief Track header panel
 */
class TrackHeaderPanel {
public:
    TrackHeaderPanel() = default;

    /**
     * @brief Set tracks to display
     */
    void SetTracks(std::vector<EventTrack>* tracks);

    /**
     * @brief Render panel
     */
    void Render();

    /**
     * @brief Handle track reordering
     */
    void OnDragTrack(const std::string& trackId, int newIndex);

    std::function<void(const std::string&, bool)> OnTrackVisibilityChanged;
    std::function<void(const std::string&, bool)> OnTrackLockChanged;
    std::function<void(const std::string&)> OnTrackSelected;

private:
    std::vector<EventTrack>* m_tracks = nullptr;
    std::string m_draggingTrack;
    int m_dragTargetIndex = -1;
};

/**
 * @brief Timeline ruler display
 */
class TimelineRuler {
public:
    TimelineRuler() = default;

    /**
     * @brief Initialize ruler
     */
    void Initialize(float width, float height);

    /**
     * @brief Set timeline properties
     */
    void SetTimelineProperties(float duration, float frameRate, float zoom, float offset);

    /**
     * @brief Render ruler
     */
    void Render();

    /**
     * @brief Get time at position
     */
    [[nodiscard]] float GetTimeAtPosition(float x) const;

    /**
     * @brief Get position at time
     */
    [[nodiscard]] float GetPositionAtTime(float time) const;

    std::function<void(float)> OnTimeClicked;

private:
    float m_width = 800.0f;
    float m_height = 30.0f;
    float m_duration = 1.0f;
    float m_frameRate = 30.0f;
    float m_zoom = 1.0f;
    float m_offset = 0.0f;
};

/**
 * @brief Event type palette for adding new events
 */
class EventTypePalette {
public:
    EventTypePalette() = default;

    /**
     * @brief Set available event types
     */
    void SetEventTypes(const std::vector<std::string>& types);

    /**
     * @brief Render palette
     */
    void Render();

    /**
     * @brief Get selected type for dragging
     */
    [[nodiscard]] const std::string& GetDraggedType() const { return m_draggedType; }

    /**
     * @brief Check if dragging
     */
    [[nodiscard]] bool IsDragging() const { return m_isDragging; }

    std::function<void(const std::string&, float)> OnEventDropped;

private:
    std::vector<std::string> m_eventTypes;
    std::string m_draggedType;
    bool m_isDragging = false;
};

} // namespace Vehement
