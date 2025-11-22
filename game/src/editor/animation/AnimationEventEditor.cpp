#include "AnimationEventEditor.hpp"
#include <fstream>
#include <algorithm>
#include <cmath>

namespace Vehement {

// ============================================================================
// AnimationEventEditor Implementation
// ============================================================================

AnimationEventEditor::AnimationEventEditor() = default;
AnimationEventEditor::~AnimationEventEditor() = default;

void AnimationEventEditor::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
    m_eventIdCounter = 0;
    m_trackIdCounter = 0;
    m_events.clear();
    m_tracks.clear();
    m_undoStack.clear();
    m_redoStack.clear();

    // Register default event templates
    RegisterEventTemplate("play_sound", {
        {"sound", ""},
        {"volume", 1.0f},
        {"pitch", 1.0f}
    });

    RegisterEventTemplate("spawn_vfx", {
        {"vfx", ""},
        {"bone", ""},
        {"offset", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}},
        {"attach", false}
    });

    RegisterEventTemplate("attack_hit", {
        {"attackId", ""},
        {"damageMultiplier", 1.0f},
        {"hitboxOffset", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}},
        {"hitboxSize", {{"x", 1.0f}, {"y", 1.0f}, {"z", 1.0f}}}
    });

    RegisterEventTemplate("footstep", {
        {"foot", "left"},
        {"surface", "default"}
    });

    RegisterEventTemplate("spawn_projectile", {
        {"type", ""},
        {"bone", ""},
        {"offset", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}}
    });

    RegisterEventTemplate("notify", {
        {"message", ""}
    });

    // Create default tracks
    AddTrack("Sound", "sound");
    AddTrack("VFX", "vfx");
    AddTrack("Gameplay", "gameplay");
}

bool AnimationEventEditor::LoadEvents(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        json data;
        file >> data;
        file.close();

        if (!ImportFromJson(data)) {
            return false;
        }

        m_filePath = filepath;
        m_dirty = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool AnimationEventEditor::LoadEvents(const json& eventData) {
    return ImportFromJson(eventData);
}

bool AnimationEventEditor::SaveEvents(const std::string& filepath) {
    json data = ExportToJson();

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    file << data.dump(2);
    file.close();

    m_filePath = filepath;
    m_dirty = false;
    return true;
}

bool AnimationEventEditor::SaveEvents() {
    if (m_filePath.empty()) {
        return false;
    }
    return SaveEvents(m_filePath);
}

void AnimationEventEditor::ClearEvents() {
    json before = ExportToJson();

    m_events.clear();
    m_selectedEventIds.clear();
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::RemoveEvent, "all", before, after);

    if (OnModified) {
        OnModified();
    }
}

json AnimationEventEditor::ExportToJson() const {
    json data;

    data["duration"] = m_config.timeline.duration;
    data["frameRate"] = m_config.timeline.frameRate;

    // Export events
    json eventsArray = json::array();
    for (const auto& event : m_events) {
        json eventData;
        eventData["time"] = event.time;
        eventData["name"] = event.eventType;
        eventData["data"] = event.data;
        eventsArray.push_back(eventData);
    }
    data["events"] = eventsArray;

    // Export tracks
    json tracksArray = json::array();
    for (const auto& track : m_tracks) {
        json trackData;
        trackData["id"] = track.id;
        trackData["name"] = track.name;
        trackData["category"] = track.category;
        trackData["visible"] = track.visible;
        trackData["locked"] = track.locked;
        trackData["eventIds"] = track.eventIds;
        tracksArray.push_back(trackData);
    }
    data["tracks"] = tracksArray;

    // Export editor metadata
    json editorData;
    editorData["viewOffset"] = m_viewOffset;
    editorData["zoom"] = m_zoom;
    editorData["playheadTime"] = m_playheadTime;
    data["_editor"] = editorData;

    return data;
}

bool AnimationEventEditor::ImportFromJson(const json& data) {
    try {
        m_events.clear();
        m_tracks.clear();
        m_eventIdCounter = 0;
        m_trackIdCounter = 0;

        // Load timeline properties
        if (data.contains("duration")) {
            m_config.timeline.duration = data["duration"];
        }
        if (data.contains("frameRate")) {
            m_config.timeline.frameRate = data["frameRate"];
        }

        // Load tracks first
        if (data.contains("tracks")) {
            for (const auto& trackData : data["tracks"]) {
                EventTrack track;
                track.id = trackData.value("id", GenerateTrackId());
                track.name = trackData.value("name", "Track");
                track.category = trackData.value("category", "custom");
                track.visible = trackData.value("visible", true);
                track.locked = trackData.value("locked", false);

                if (trackData.contains("eventIds")) {
                    for (const auto& eventId : trackData["eventIds"]) {
                        track.eventIds.push_back(eventId);
                    }
                }

                m_tracks.push_back(track);
            }
        } else {
            // Create default tracks if none exist
            AddTrack("Sound", "sound");
            AddTrack("VFX", "vfx");
            AddTrack("Gameplay", "gameplay");
        }

        // Load events
        if (data.contains("events")) {
            for (const auto& eventData : data["events"]) {
                EventMarker event;
                event.id = GenerateEventId();
                event.time = eventData.value("time", 0.0f);
                event.absoluteTime = event.time * m_config.timeline.duration;
                event.eventType = eventData.value("name", "notify");
                event.name = event.eventType;

                if (eventData.contains("data")) {
                    event.data = eventData["data"];
                }

                event.color = GetEventTypeColor(event.eventType);
                m_events.push_back(event);

                // Auto-assign to track based on type
                std::string trackCategory = "gameplay";
                if (event.eventType == "play_sound" || event.eventType == "footstep") {
                    trackCategory = "sound";
                } else if (event.eventType == "spawn_vfx") {
                    trackCategory = "vfx";
                }

                for (auto& track : m_tracks) {
                    if (track.category == trackCategory) {
                        track.eventIds.push_back(event.id);
                        break;
                    }
                }
            }
        }

        // Load editor metadata
        if (data.contains("_editor")) {
            const auto& editorData = data["_editor"];
            m_viewOffset = editorData.value("viewOffset", 0.0f);
            m_zoom = editorData.value("zoom", 1.0f);
            m_playheadTime = editorData.value("playheadTime", 0.0f);
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// -------------------------------------------------------------------------
// Animation Properties
// -------------------------------------------------------------------------

void AnimationEventEditor::SetAnimationDuration(float duration) {
    m_config.timeline.duration = duration;

    // Update absolute times for all events
    for (auto& event : m_events) {
        event.absoluteTime = event.time * duration;
    }
}

void AnimationEventEditor::SetFrameRate(float fps) {
    m_config.timeline.frameRate = fps;
}

int AnimationEventEditor::GetTotalFrames() const {
    return static_cast<int>(m_config.timeline.duration * m_config.timeline.frameRate);
}

// -------------------------------------------------------------------------
// Event Operations
// -------------------------------------------------------------------------

EventMarker* AnimationEventEditor::AddEvent(const std::string& eventType, float time,
                                             const std::string& trackId) {
    json before = ExportToJson();

    EventMarker event;
    event.id = GenerateEventId();
    event.eventType = eventType;
    event.name = eventType;
    event.time = m_config.timeline.snapToFrames ? SnapToFrame(time) : time;
    event.absoluteTime = event.time * m_config.timeline.duration;
    event.color = GetEventTypeColor(eventType);

    // Initialize with template data
    if (m_eventTemplates.count(eventType)) {
        event.data = m_eventTemplates.at(eventType);
    }

    m_events.push_back(event);

    // Add to track
    if (!trackId.empty()) {
        EventTrack* track = GetTrack(trackId);
        if (track) {
            track->eventIds.push_back(event.id);
        }
    } else {
        // Auto-assign to appropriate track
        std::string trackCategory = "gameplay";
        if (eventType == "play_sound" || eventType == "footstep") {
            trackCategory = "sound";
        } else if (eventType == "spawn_vfx") {
            trackCategory = "vfx";
        }

        for (auto& track : m_tracks) {
            if (track.category == trackCategory) {
                track.eventIds.push_back(event.id);
                break;
            }
        }
    }

    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::AddEvent, event.id, before, after);

    if (OnModified) {
        OnModified();
    }

    return &m_events.back();
}

bool AnimationEventEditor::RemoveEvent(const std::string& id) {
    auto it = std::find_if(m_events.begin(), m_events.end(),
                           [&id](const EventMarker& e) { return e.id == id; });

    if (it == m_events.end()) {
        return false;
    }

    json before = ExportToJson();

    // Remove from tracks
    for (auto& track : m_tracks) {
        track.eventIds.erase(
            std::remove(track.eventIds.begin(), track.eventIds.end(), id),
            track.eventIds.end()
        );
    }

    m_events.erase(it);
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::RemoveEvent, id, before, after);

    // Remove from selection
    m_selectedEventIds.erase(
        std::remove(m_selectedEventIds.begin(), m_selectedEventIds.end(), id),
        m_selectedEventIds.end()
    );

    if (OnModified) {
        OnModified();
    }

    return true;
}

EventMarker* AnimationEventEditor::GetEvent(const std::string& id) {
    auto it = std::find_if(m_events.begin(), m_events.end(),
                           [&id](const EventMarker& e) { return e.id == id; });
    return it != m_events.end() ? &(*it) : nullptr;
}

std::vector<EventMarker*> AnimationEventEditor::GetEventsAtTime(float time, float tolerance) {
    std::vector<EventMarker*> result;
    for (auto& event : m_events) {
        if (std::abs(event.time - time) <= tolerance) {
            result.push_back(&event);
        }
    }
    return result;
}

std::vector<EventMarker*> AnimationEventEditor::GetEventsInRange(float startTime, float endTime) {
    std::vector<EventMarker*> result;
    for (auto& event : m_events) {
        if (event.time >= startTime && event.time <= endTime) {
            result.push_back(&event);
        }
    }
    return result;
}

void AnimationEventEditor::MoveEvent(const std::string& id, float newTime) {
    EventMarker* event = GetEvent(id);
    if (!event) return;

    json before = ExportToJson();

    event->time = m_config.timeline.snapToFrames ? SnapToFrame(newTime) : newTime;
    event->time = std::clamp(event->time, 0.0f, 1.0f);
    event->absoluteTime = event->time * m_config.timeline.duration;

    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::MoveEvent, id, before, after);

    if (OnModified) {
        OnModified();
    }
}

void AnimationEventEditor::SetEventData(const std::string& id, const json& data) {
    EventMarker* event = GetEvent(id);
    if (!event) return;

    json before = ExportToJson();

    event->data = data;
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::ModifyEvent, id, before, after);

    if (OnModified) {
        OnModified();
    }
}

EventMarker* AnimationEventEditor::DuplicateEvent(const std::string& id, float newTime) {
    EventMarker* source = GetEvent(id);
    if (!source) return nullptr;

    EventMarker* newEvent = AddEvent(source->eventType, newTime);
    if (newEvent) {
        newEvent->data = source->data;
    }
    return newEvent;
}

// -------------------------------------------------------------------------
// Track Operations
// -------------------------------------------------------------------------

EventTrack* AnimationEventEditor::AddTrack(const std::string& name, const std::string& category) {
    json before = ExportToJson();

    EventTrack track;
    track.id = GenerateTrackId();
    track.name = name;
    track.category = category;

    // Set color based on category
    if (category == "sound") {
        track.color = 0x44AA44FF;
    } else if (category == "vfx") {
        track.color = 0xAA44AAFF;
    } else if (category == "gameplay") {
        track.color = 0xAAAA44FF;
    } else {
        track.color = 0x4488FFFF;
    }

    m_tracks.push_back(track);
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::AddTrack, track.id, before, after);

    if (OnModified) {
        OnModified();
    }

    return &m_tracks.back();
}

bool AnimationEventEditor::RemoveTrack(const std::string& id) {
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                           [&id](const EventTrack& t) { return t.id == id; });

    if (it == m_tracks.end()) {
        return false;
    }

    json before = ExportToJson();

    // Remove all events in this track
    for (const auto& eventId : it->eventIds) {
        auto eventIt = std::find_if(m_events.begin(), m_events.end(),
                                     [&eventId](const EventMarker& e) { return e.id == eventId; });
        if (eventIt != m_events.end()) {
            m_events.erase(eventIt);
        }
    }

    m_tracks.erase(it);
    m_dirty = true;

    json after = ExportToJson();
    RecordAction(EventEditorAction::Type::RemoveTrack, id, before, after);

    if (OnModified) {
        OnModified();
    }

    return true;
}

EventTrack* AnimationEventEditor::GetTrack(const std::string& id) {
    auto it = std::find_if(m_tracks.begin(), m_tracks.end(),
                           [&id](const EventTrack& t) { return t.id == id; });
    return it != m_tracks.end() ? &(*it) : nullptr;
}

bool AnimationEventEditor::MoveEventToTrack(const std::string& eventId, const std::string& trackId) {
    EventMarker* event = GetEvent(eventId);
    EventTrack* targetTrack = GetTrack(trackId);

    if (!event || !targetTrack) return false;

    // Remove from current track
    for (auto& track : m_tracks) {
        track.eventIds.erase(
            std::remove(track.eventIds.begin(), track.eventIds.end(), eventId),
            track.eventIds.end()
        );
    }

    // Add to target track
    targetTrack->eventIds.push_back(eventId);
    m_dirty = true;

    if (OnModified) {
        OnModified();
    }

    return true;
}

void AnimationEventEditor::SetTrackVisible(const std::string& id, bool visible) {
    EventTrack* track = GetTrack(id);
    if (track) {
        track->visible = visible;
    }
}

void AnimationEventEditor::SetTrackLocked(const std::string& id, bool locked) {
    EventTrack* track = GetTrack(id);
    if (track) {
        track->locked = locked;
    }
}

// -------------------------------------------------------------------------
// Selection
// -------------------------------------------------------------------------

void AnimationEventEditor::SelectEvent(const std::string& id) {
    ClearSelection();
    AddToSelection(id);
}

void AnimationEventEditor::SelectEvents(const std::vector<std::string>& ids) {
    ClearSelection();
    for (const auto& id : ids) {
        AddToSelection(id);
    }
}

void AnimationEventEditor::AddToSelection(const std::string& id) {
    if (std::find(m_selectedEventIds.begin(), m_selectedEventIds.end(), id) ==
        m_selectedEventIds.end()) {
        m_selectedEventIds.push_back(id);

        EventMarker* event = GetEvent(id);
        if (event) {
            event->selected = true;
        }
    }

    if (OnSelectionChanged) {
        OnSelectionChanged(m_selectedEventIds);
    }

    if (m_selectedEventIds.size() == 1 && OnEventSelected) {
        OnEventSelected(GetEvent(m_selectedEventIds[0]));
    }
}

void AnimationEventEditor::ClearSelection() {
    for (const auto& id : m_selectedEventIds) {
        EventMarker* event = GetEvent(id);
        if (event) {
            event->selected = false;
        }
    }
    m_selectedEventIds.clear();

    if (OnSelectionChanged) {
        OnSelectionChanged(m_selectedEventIds);
    }
}

void AnimationEventEditor::SelectEventsInRange(float startTime, float endTime,
                                                const std::string& trackId) {
    ClearSelection();

    for (auto& event : m_events) {
        if (event.time >= startTime && event.time <= endTime) {
            if (trackId.empty()) {
                AddToSelection(event.id);
            } else {
                // Check if event is in specified track
                EventTrack* track = GetTrack(trackId);
                if (track) {
                    auto it = std::find(track->eventIds.begin(), track->eventIds.end(), event.id);
                    if (it != track->eventIds.end()) {
                        AddToSelection(event.id);
                    }
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
// Timeline Navigation
// -------------------------------------------------------------------------

void AnimationEventEditor::SetPlayheadTime(float time) {
    m_lastPlayheadTime = m_playheadTime;
    m_playheadTime = std::clamp(time, 0.0f, 1.0f);

    if (OnPlayheadChanged) {
        OnPlayheadChanged(m_playheadTime);
    }
}

void AnimationEventEditor::SetViewOffset(float offset) {
    m_viewOffset = std::max(0.0f, offset);
}

void AnimationEventEditor::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, m_config.zoomMin, m_config.zoomMax);
}

void AnimationEventEditor::ZoomToFit() {
    float visibleWidth = m_config.viewSize.x - m_config.timeline.headerWidth;
    m_zoom = visibleWidth / (m_config.timeline.duration * m_config.timeline.pixelsPerSecond);
    m_zoom = std::clamp(m_zoom, m_config.zoomMin, m_config.zoomMax);
    m_viewOffset = 0.0f;
}

void AnimationEventEditor::CenterOnTime(float time) {
    float visibleWidth = m_config.viewSize.x - m_config.timeline.headerWidth;
    float screenPos = TimeToScreen(time);
    m_viewOffset = time * m_config.timeline.duration * m_config.timeline.pixelsPerSecond * m_zoom
                   - visibleWidth / 2.0f;
    m_viewOffset = std::max(0.0f, m_viewOffset);
}

void AnimationEventEditor::GoToFrame(int frame) {
    float time = FrameToTime(frame);
    SetPlayheadTime(time);
}

int AnimationEventEditor::TimeToFrame(float time) const {
    return static_cast<int>(time * m_config.timeline.duration * m_config.timeline.frameRate);
}

float AnimationEventEditor::FrameToTime(int frame) const {
    return static_cast<float>(frame) / (m_config.timeline.duration * m_config.timeline.frameRate);
}

// -------------------------------------------------------------------------
// Input Handling
// -------------------------------------------------------------------------

void AnimationEventEditor::OnMouseDown(const glm::vec2& position, int button,
                                        bool shift, bool ctrl) {
    float time = ScreenToTime(position.x);

    if (button == 0) {  // Left click
        // Check for event click
        EventMarker* event = FindEventAt(position);

        if (event) {
            if (ctrl) {
                // Toggle selection
                if (event->selected) {
                    m_selectedEventIds.erase(
                        std::remove(m_selectedEventIds.begin(), m_selectedEventIds.end(), event->id),
                        m_selectedEventIds.end()
                    );
                    event->selected = false;
                } else {
                    AddToSelection(event->id);
                }
            } else if (shift) {
                AddToSelection(event->id);
            } else if (!event->selected) {
                SelectEvent(event->id);
            }

            // Start dragging
            if (!m_selectedEventIds.empty()) {
                m_dragging = true;
                m_dragStartTime = event->time;
                m_dragStart = position;
            }
        } else {
            // Click on timeline ruler - scrub
            if (position.y < m_config.timeline.trackHeight) {
                m_scrubbing = true;
                SetPlayheadTime(time);
            } else if (shift) {
                // Start marquee selection
                m_marqueeSelecting = true;
                m_marqueeStart = position;
                m_marqueeEnd = position;
            } else {
                ClearSelection();
            }
        }
    } else if (button == 1) {  // Right click
        // Context menu would be shown here
    }
}

void AnimationEventEditor::OnMouseUp(const glm::vec2& position, int button) {
    if (button == 0) {
        if (m_marqueeSelecting) {
            // Complete marquee selection
            float startTime = ScreenToTime(std::min(m_marqueeStart.x, m_marqueeEnd.x));
            float endTime = ScreenToTime(std::max(m_marqueeStart.x, m_marqueeEnd.x));
            SelectEventsInRange(startTime, endTime);
        }

        m_dragging = false;
        m_scrubbing = false;
        m_marqueeSelecting = false;
    }
}

void AnimationEventEditor::OnMouseMove(const glm::vec2& position) {
    float time = ScreenToTime(position.x);

    if (m_dragging && !m_selectedEventIds.empty()) {
        float timeDelta = time - ScreenToTime(m_dragStart.x);

        json before = ExportToJson();

        for (const auto& id : m_selectedEventIds) {
            EventMarker* event = GetEvent(id);
            if (event) {
                float newTime = event->time + timeDelta;
                newTime = std::clamp(newTime, 0.0f, 1.0f);
                if (m_config.timeline.snapToFrames) {
                    newTime = SnapToFrame(newTime);
                }
                event->time = newTime;
                event->absoluteTime = newTime * m_config.timeline.duration;
            }
        }

        m_dragStart = position;
        m_dirty = true;
    } else if (m_scrubbing) {
        SetPlayheadTime(time);
    } else if (m_marqueeSelecting) {
        m_marqueeEnd = position;
    }
}

void AnimationEventEditor::OnMouseDoubleClick(const glm::vec2& position, int button) {
    if (button == 0) {
        float time = ScreenToTime(position.x);
        EventMarker* event = FindEventAt(position);

        if (event) {
            // Open event properties panel
            if (OnEventSelected) {
                OnEventSelected(event);
            }
        } else {
            // Add new event at position
            AddEvent("notify", time);
        }
    }
}

void AnimationEventEditor::OnKeyDown(int key, bool shift, bool ctrl) {
    // Delete
    if (key == 127 || key == 8) {
        std::vector<std::string> toDelete = m_selectedEventIds;
        for (const auto& id : toDelete) {
            RemoveEvent(id);
        }
    }
    // Undo
    else if (ctrl && key == 'z') {
        Undo();
    }
    // Redo
    else if (ctrl && key == 'y') {
        Redo();
    }
    // Copy
    else if (ctrl && key == 'c') {
        CopySelected();
    }
    // Cut
    else if (ctrl && key == 'x') {
        CutSelected();
    }
    // Paste
    else if (ctrl && key == 'v') {
        Paste();
    }
    // Select all
    else if (ctrl && key == 'a') {
        std::vector<std::string> allIds;
        for (const auto& event : m_events) {
            allIds.push_back(event.id);
        }
        SelectEvents(allIds);
    }
    // Space - toggle playback
    else if (key == ' ') {
        if (m_playing) {
            Pause();
        } else {
            Play();
        }
    }
    // Left arrow - step backward
    else if (key == 37) {
        StepBackward();
    }
    // Right arrow - step forward
    else if (key == 39) {
        StepForward();
    }
    // Home - go to start
    else if (key == 36) {
        SetPlayheadTime(0.0f);
    }
    // End - go to end
    else if (key == 35) {
        SetPlayheadTime(1.0f);
    }
}

void AnimationEventEditor::OnScroll(float delta, bool horizontal) {
    if (horizontal) {
        m_viewOffset += delta * 50.0f;
        m_viewOffset = std::max(0.0f, m_viewOffset);
    } else {
        float newZoom = m_zoom * (1.0f + delta * 0.1f);
        SetZoom(newZoom);
    }
}

// -------------------------------------------------------------------------
// Clipboard
// -------------------------------------------------------------------------

void AnimationEventEditor::CopySelected() {
    m_clipboard.clear();
    m_clipboardBaseTime = std::numeric_limits<float>::max();

    for (const auto& id : m_selectedEventIds) {
        EventMarker* event = GetEvent(id);
        if (event) {
            json eventData;
            eventData["eventType"] = event->eventType;
            eventData["time"] = event->time;
            eventData["data"] = event->data;
            m_clipboard.push_back(eventData);

            m_clipboardBaseTime = std::min(m_clipboardBaseTime, event->time);
        }
    }
}

void AnimationEventEditor::CutSelected() {
    CopySelected();
    std::vector<std::string> toDelete = m_selectedEventIds;
    for (const auto& id : toDelete) {
        RemoveEvent(id);
    }
}

void AnimationEventEditor::Paste() {
    if (m_clipboard.empty()) return;

    ClearSelection();

    float timeOffset = m_playheadTime - m_clipboardBaseTime;

    for (const auto& eventData : m_clipboard) {
        float time = eventData["time"].get<float>() + timeOffset;
        EventMarker* newEvent = AddEvent(eventData["eventType"], time);
        if (newEvent && eventData.contains("data")) {
            newEvent->data = eventData["data"];
        }
        if (newEvent) {
            AddToSelection(newEvent->id);
        }
    }
}

// -------------------------------------------------------------------------
// Undo/Redo
// -------------------------------------------------------------------------

void AnimationEventEditor::Undo() {
    if (m_undoStack.empty()) return;

    EventEditorAction action = m_undoStack.back();
    m_undoStack.pop_back();

    ImportFromJson(action.beforeData);

    m_redoStack.push_back(action);

    if (OnModified) {
        OnModified();
    }
}

void AnimationEventEditor::Redo() {
    if (m_redoStack.empty()) return;

    EventEditorAction action = m_redoStack.back();
    m_redoStack.pop_back();

    ImportFromJson(action.afterData);

    m_undoStack.push_back(action);

    if (OnModified) {
        OnModified();
    }
}

// -------------------------------------------------------------------------
// Preview
// -------------------------------------------------------------------------

void AnimationEventEditor::Play() {
    m_playing = true;
    m_lastPlayheadTime = m_playheadTime;
    m_triggeredEvents.clear();
}

void AnimationEventEditor::Pause() {
    m_playing = false;
}

void AnimationEventEditor::Stop() {
    m_playing = false;
    SetPlayheadTime(0.0f);
    m_triggeredEvents.clear();
}

void AnimationEventEditor::UpdatePreview(float deltaTime) {
    if (!m_playing) return;

    float previousTime = m_playheadTime;
    float newTime = m_playheadTime + (deltaTime / m_config.timeline.duration) * m_playbackSpeed;

    if (newTime > 1.0f) {
        if (m_config.timeline.loopPreview) {
            newTime = std::fmod(newTime, 1.0f);
        } else {
            newTime = 1.0f;
            m_playing = false;
        }
    }

    SetPlayheadTime(newTime);
    UpdateTriggeredEvents(previousTime, newTime);
}

void AnimationEventEditor::SetPlaybackSpeed(float speed) {
    m_playbackSpeed = std::clamp(speed, 0.1f, 10.0f);
}

void AnimationEventEditor::StepForward() {
    int currentFrame = TimeToFrame(m_playheadTime);
    GoToFrame(currentFrame + 1);
}

void AnimationEventEditor::StepBackward() {
    int currentFrame = TimeToFrame(m_playheadTime);
    GoToFrame(std::max(0, currentFrame - 1));
}

std::vector<EventMarker*> AnimationEventEditor::GetTriggeredEvents() {
    std::vector<EventMarker*> result = m_triggeredEvents;
    m_triggeredEvents.clear();
    return result;
}

// -------------------------------------------------------------------------
// Event Templates
// -------------------------------------------------------------------------

std::vector<std::string> AnimationEventEditor::GetEventTypes() const {
    std::vector<std::string> types;
    for (const auto& [type, _] : m_eventTemplates) {
        types.push_back(type);
    }
    return types;
}

json AnimationEventEditor::GetEventTemplate(const std::string& type) const {
    auto it = m_eventTemplates.find(type);
    return it != m_eventTemplates.end() ? it->second : json::object();
}

void AnimationEventEditor::RegisterEventTemplate(const std::string& type, const json& templateData) {
    m_eventTemplates[type] = templateData;
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void AnimationEventEditor::RecordAction(EventEditorAction::Type type, const std::string& target,
                                         const json& before, const json& after) {
    EventEditorAction action;
    action.type = type;
    action.targetId = target;
    action.beforeData = before;
    action.afterData = after;

    m_undoStack.push_back(action);
    m_redoStack.clear();

    while (m_undoStack.size() > MAX_UNDO_SIZE) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

EventMarker* AnimationEventEditor::FindEventAt(const glm::vec2& position) {
    float time = ScreenToTime(position.x);
    float tolerance = 0.01f / m_zoom;  // Scale tolerance with zoom

    // Determine which track
    int trackIndex = static_cast<int>((position.y - m_config.timeline.trackHeight) /
                                       m_config.timeline.trackHeight);

    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_tracks.size())) {
        const EventTrack& track = m_tracks[trackIndex];

        for (const auto& eventId : track.eventIds) {
            EventMarker* event = GetEvent(eventId);
            if (event && std::abs(event->time - time) <= tolerance) {
                return event;
            }
        }
    }

    // Fall back to searching all events
    for (auto& event : m_events) {
        if (std::abs(event.time - time) <= tolerance) {
            return &event;
        }
    }

    return nullptr;
}

EventTrack* AnimationEventEditor::FindTrackAt(const glm::vec2& position) {
    int trackIndex = static_cast<int>((position.y - m_config.timeline.trackHeight) /
                                       m_config.timeline.trackHeight);

    if (trackIndex >= 0 && trackIndex < static_cast<int>(m_tracks.size())) {
        return &m_tracks[trackIndex];
    }

    return nullptr;
}

std::string AnimationEventEditor::GenerateEventId() {
    return "event_" + std::to_string(m_eventIdCounter++);
}

std::string AnimationEventEditor::GenerateTrackId() {
    return "track_" + std::to_string(m_trackIdCounter++);
}

float AnimationEventEditor::ScreenToTime(float screenX) const {
    float x = screenX - m_config.timeline.headerWidth + m_viewOffset;
    return x / (m_config.timeline.duration * m_config.timeline.pixelsPerSecond * m_zoom);
}

float AnimationEventEditor::TimeToScreen(float time) const {
    return time * m_config.timeline.duration * m_config.timeline.pixelsPerSecond * m_zoom
           - m_viewOffset + m_config.timeline.headerWidth;
}

float AnimationEventEditor::SnapToFrame(float time) const {
    float frameTime = 1.0f / (m_config.timeline.duration * m_config.timeline.frameRate);
    return std::round(time / frameTime) * frameTime;
}

void AnimationEventEditor::UpdateTriggeredEvents(float previousTime, float currentTime) {
    m_triggeredEvents.clear();

    for (auto& event : m_events) {
        bool triggered = false;

        if (currentTime >= previousTime) {
            // Normal forward playback
            triggered = event.time > previousTime && event.time <= currentTime;
        } else {
            // Wrapped around (looped)
            triggered = event.time > previousTime || event.time <= currentTime;
        }

        if (triggered) {
            m_triggeredEvents.push_back(&event);

            if (OnEventTriggered) {
                OnEventTriggered(event);
            }
        }
    }
}

uint32_t AnimationEventEditor::GetEventTypeColor(const std::string& type) const {
    if (type == "play_sound" || type == "footstep") {
        return 0x44AA44FF;  // Green
    } else if (type == "spawn_vfx") {
        return 0xAA44AAFF;  // Purple
    } else if (type == "attack_hit" || type == "spawn_projectile") {
        return 0xAA4444FF;  // Red
    } else if (type == "notify") {
        return 0x4488FFFF;  // Blue
    }
    return 0xAAAA44FF;  // Yellow default
}

// ============================================================================
// EventPropertiesPanel Implementation
// ============================================================================

void EventPropertiesPanel::SetEvent(EventMarker* event) {
    m_event = event;
    if (event) {
        m_editEvent = *event;
    }
}

bool EventPropertiesPanel::Render() {
    if (!m_event) return false;

    bool modified = false;

    // Would render UI controls for editing event properties
    // - Event type dropdown
    // - Time input field
    // - Data fields based on event type

    if (modified && OnEventModified) {
        OnEventModified(m_editEvent);
    }

    return modified;
}

EventMarker EventPropertiesPanel::GetModifiedEvent() const {
    return m_editEvent;
}

// ============================================================================
// TrackHeaderPanel Implementation
// ============================================================================

void TrackHeaderPanel::SetTracks(std::vector<EventTrack>* tracks) {
    m_tracks = tracks;
}

void TrackHeaderPanel::Render() {
    if (!m_tracks) return;

    // Would render track headers with:
    // - Track name
    // - Visibility toggle
    // - Lock toggle
    // - Color indicator
}

void TrackHeaderPanel::OnDragTrack(const std::string& trackId, int newIndex) {
    if (!m_tracks) return;

    // Find and move track to new index
    auto it = std::find_if(m_tracks->begin(), m_tracks->end(),
                           [&trackId](const EventTrack& t) { return t.id == trackId; });

    if (it != m_tracks->end()) {
        EventTrack track = *it;
        m_tracks->erase(it);

        newIndex = std::clamp(newIndex, 0, static_cast<int>(m_tracks->size()));
        m_tracks->insert(m_tracks->begin() + newIndex, track);
    }
}

// ============================================================================
// TimelineRuler Implementation
// ============================================================================

void TimelineRuler::Initialize(float width, float height) {
    m_width = width;
    m_height = height;
}

void TimelineRuler::SetTimelineProperties(float duration, float frameRate, float zoom, float offset) {
    m_duration = duration;
    m_frameRate = frameRate;
    m_zoom = zoom;
    m_offset = offset;
}

void TimelineRuler::Render() {
    // Would render:
    // - Time markers (major and minor)
    // - Frame numbers
    // - Second markers
}

float TimelineRuler::GetTimeAtPosition(float x) const {
    return (x + m_offset) / (m_duration * 200.0f * m_zoom);
}

float TimelineRuler::GetPositionAtTime(float time) const {
    return time * m_duration * 200.0f * m_zoom - m_offset;
}

// ============================================================================
// EventTypePalette Implementation
// ============================================================================

void EventTypePalette::SetEventTypes(const std::vector<std::string>& types) {
    m_eventTypes = types;
}

void EventTypePalette::Render() {
    // Would render event type buttons that can be dragged onto timeline
}

} // namespace Vehement
