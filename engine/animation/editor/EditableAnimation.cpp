#include "EditableAnimation.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

EditableAnimation::EditableAnimation() = default;
EditableAnimation::~EditableAnimation() = default;

// ============================================================================
// Basic Properties
// ============================================================================

void EditableAnimation::SetDuration(float duration) {
    if (std::abs(m_duration - duration) < 0.0001f) return;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::ModifyDuration;
    action.description = "Change duration";
    action.oldDuration = m_duration;
    action.newDuration = duration;
    RecordAction(action);

    m_duration = duration;
    MarkDirty();
}

// ============================================================================
// Track Management
// ============================================================================

EditableBoneTrack* EditableAnimation::AddTrack(const std::string& boneName) {
    // Check if exists
    for (auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }

    EditableBoneTrack track;
    track.boneName = boneName;
    m_tracks.push_back(track);
    MarkDirty();

    return &m_tracks.back();
}

void EditableAnimation::RemoveTrack(const std::string& boneName) {
    m_tracks.erase(
        std::remove_if(m_tracks.begin(), m_tracks.end(),
            [&boneName](const EditableBoneTrack& t) { return t.boneName == boneName; }),
        m_tracks.end()
    );
    MarkDirty();
}

EditableBoneTrack* EditableAnimation::GetTrack(const std::string& boneName) {
    for (auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }
    return nullptr;
}

const EditableBoneTrack* EditableAnimation::GetTrack(const std::string& boneName) const {
    for (const auto& track : m_tracks) {
        if (track.boneName == boneName) {
            return &track;
        }
    }
    return nullptr;
}

void EditableAnimation::ClearTracks() {
    m_tracks.clear();
    MarkDirty();
}

// ============================================================================
// Keyframe Operations
// ============================================================================

EditableKeyframe* EditableAnimation::AddKeyframe(const std::string& boneName, float time, const EditableBoneTransform& transform) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track) {
        track = AddTrack(boneName);
    }

    // Check if keyframe exists at this time
    for (auto& kf : track->keyframes) {
        if (std::abs(kf.time - time) < 0.001f) {
            AnimationEditAction action;
            action.type = AnimationEditAction::Type::ModifyKeyframe;
            action.description = "Modify keyframe";
            action.boneName = boneName;
            action.oldKeyframe = kf;
            action.newKeyframe.time = time;
            action.newKeyframe.transform = transform;
            RecordAction(action);

            kf.transform = transform;
            MarkDirty();

            if (OnKeyframeModified) {
                OnKeyframeModified(boneName, &kf - track->keyframes.data());
            }
            return &kf;
        }
    }

    // Add new keyframe
    EditableKeyframe kf;
    kf.time = time;
    kf.transform = transform;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::AddKeyframe;
    action.description = "Add keyframe";
    action.boneName = boneName;
    action.time = time;
    action.newKeyframe = kf;
    RecordAction(action);

    track->keyframes.push_back(kf);
    SortKeyframes(*track);
    MarkDirty();

    // Find index
    size_t index = 0;
    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        if (std::abs(track->keyframes[i].time - time) < 0.001f) {
            index = i;
            break;
        }
    }

    if (OnKeyframeAdded) {
        OnKeyframeAdded(boneName, index);
    }

    return &track->keyframes[index];
}

void EditableAnimation::RemoveKeyframe(const std::string& boneName, size_t index) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::RemoveKeyframe;
    action.description = "Remove keyframe";
    action.boneName = boneName;
    action.keyframeIndex = index;
    action.oldKeyframe = track->keyframes[index];
    RecordAction(action);

    track->keyframes.erase(track->keyframes.begin() + static_cast<ptrdiff_t>(index));
    MarkDirty();

    if (OnKeyframeRemoved) {
        OnKeyframeRemoved(boneName, index);
    }
}

EditableKeyframe* EditableAnimation::GetKeyframe(const std::string& boneName, size_t index) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return nullptr;
    return &track->keyframes[index];
}

EditableKeyframe* EditableAnimation::GetKeyframeAtTime(const std::string& boneName, float time, float tolerance) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track) return nullptr;

    for (auto& kf : track->keyframes) {
        if (std::abs(kf.time - time) <= tolerance) {
            return &kf;
        }
    }
    return nullptr;
}

void EditableAnimation::MoveKeyframe(const std::string& boneName, size_t index, float newTime) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::MoveKeyframe;
    action.description = "Move keyframe";
    action.boneName = boneName;
    action.keyframeIndex = index;
    action.oldKeyframe = track->keyframes[index];
    action.newKeyframe = track->keyframes[index];
    action.newKeyframe.time = newTime;
    RecordAction(action);

    track->keyframes[index].time = newTime;
    SortKeyframes(*track);
    MarkDirty();
}

void EditableAnimation::ModifyKeyframe(const std::string& boneName, size_t index, const EditableKeyframe& newKeyframe) {
    EditableBoneTrack* track = GetTrack(boneName);
    if (!track || index >= track->keyframes.size()) return;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::ModifyKeyframe;
    action.description = "Modify keyframe";
    action.boneName = boneName;
    action.keyframeIndex = index;
    action.oldKeyframe = track->keyframes[index];
    action.newKeyframe = newKeyframe;
    RecordAction(action);

    track->keyframes[index] = newKeyframe;
    SortKeyframes(*track);
    MarkDirty();

    if (OnKeyframeModified) {
        OnKeyframeModified(boneName, index);
    }
}

// ============================================================================
// Event Operations
// ============================================================================

EditableAnimationEvent* EditableAnimation::AddEvent(float time, const std::string& name) {
    EditableAnimationEvent event;
    event.time = time;
    event.name = name;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::AddEvent;
    action.description = "Add event";
    action.newEvent = event;
    RecordAction(action);

    m_events.push_back(event);
    SortEvents();
    MarkDirty();

    size_t index = 0;
    for (size_t i = 0; i < m_events.size(); ++i) {
        if (std::abs(m_events[i].time - time) < 0.001f && m_events[i].name == name) {
            index = i;
            break;
        }
    }

    if (OnEventAdded) {
        OnEventAdded(index);
    }

    return &m_events[index];
}

void EditableAnimation::RemoveEvent(size_t index) {
    if (index >= m_events.size()) return;

    AnimationEditAction action;
    action.type = AnimationEditAction::Type::RemoveEvent;
    action.description = "Remove event";
    action.oldEvent = m_events[index];
    action.keyframeIndex = index;  // Reuse for event index
    RecordAction(action);

    m_events.erase(m_events.begin() + static_cast<ptrdiff_t>(index));
    MarkDirty();

    if (OnEventRemoved) {
        OnEventRemoved(index);
    }
}

EditableAnimationEvent* EditableAnimation::GetEvent(size_t index) {
    return index < m_events.size() ? &m_events[index] : nullptr;
}

// ============================================================================
// Evaluation
// ============================================================================

EditableBoneTransform EditableAnimation::EvaluateTransform(const std::string& boneName, float time) const {
    const EditableBoneTrack* track = GetTrack(boneName);
    if (!track || track->keyframes.empty()) {
        return EditableBoneTransform();
    }

    const auto& keyframes = track->keyframes;

    // Before first keyframe
    if (time <= keyframes.front().time) {
        return keyframes.front().transform;
    }

    // After last keyframe
    if (time >= keyframes.back().time) {
        return keyframes.back().transform;
    }

    // Find surrounding keyframes
    size_t i = 0;
    for (; i < keyframes.size() - 1; ++i) {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time) {
            break;
        }
    }

    float t = (time - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
    return InterpolateKeyframes(keyframes[i], keyframes[i + 1], t);
}

std::unordered_map<std::string, EditableBoneTransform> EditableAnimation::EvaluateAllTransforms(float time) const {
    std::unordered_map<std::string, EditableBoneTransform> result;

    for (const auto& track : m_tracks) {
        if (track.enabled) {
            result[track.boneName] = EvaluateTransform(track.boneName, time);
        }
    }

    return result;
}

std::vector<const EditableAnimationEvent*> EditableAnimation::GetEventsInRange(float start, float end) const {
    std::vector<const EditableAnimationEvent*> result;

    for (const auto& event : m_events) {
        if (event.time >= start && event.time <= end) {
            result.push_back(&event);
        }
    }

    return result;
}

// ============================================================================
// Undo/Redo
// ============================================================================

void EditableAnimation::Undo() {
    if (m_undoStack.empty()) return;

    AnimationEditAction action = m_undoStack.top();
    m_undoStack.pop();

    UnapplyAction(action);
    m_redoStack.push(action);
    MarkDirty();
}

void EditableAnimation::Redo() {
    if (m_redoStack.empty()) return;

    AnimationEditAction action = m_redoStack.top();
    m_redoStack.pop();

    ApplyAction(action);
    m_undoStack.push(action);
    MarkDirty();
}

void EditableAnimation::ClearHistory() {
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();
}

void EditableAnimation::BeginActionGroup(const std::string& description) {
    m_inActionGroup = true;
    m_groupAction = AnimationEditAction();
    m_groupAction.type = AnimationEditAction::Type::BatchTransform;
    m_groupAction.description = description;

    // Capture current state
    for (const auto& track : m_tracks) {
        m_groupAction.oldTracks[track.boneName] = track.keyframes;
    }
}

void EditableAnimation::EndActionGroup() {
    if (!m_inActionGroup) return;

    m_inActionGroup = false;

    // Capture new state
    for (const auto& track : m_tracks) {
        m_groupAction.newTracks[track.boneName] = track.keyframes;
    }

    // Only record if there were changes
    bool hasChanges = false;
    for (const auto& [name, oldKfs] : m_groupAction.oldTracks) {
        auto it = m_groupAction.newTracks.find(name);
        if (it == m_groupAction.newTracks.end() || it->second.size() != oldKfs.size()) {
            hasChanges = true;
            break;
        }
    }

    if (hasChanges) {
        m_undoStack.push(m_groupAction);
        while (!m_redoStack.empty()) m_redoStack.pop();

        while (m_undoStack.size() > MAX_UNDO_SIZE) {
            // Would need to convert stack to deque for efficient front removal
        }
    }
}

// ============================================================================
// Private
// ============================================================================

void EditableAnimation::RecordAction(const AnimationEditAction& action) {
    if (m_inActionGroup) return;  // Actions will be batched

    m_undoStack.push(action);
    while (!m_redoStack.empty()) m_redoStack.pop();

    // Limit undo history
    // Note: std::stack doesn't support size limiting easily
}

void EditableAnimation::ApplyAction(const AnimationEditAction& action) {
    switch (action.type) {
        case AnimationEditAction::Type::AddKeyframe:
            if (auto* track = GetTrack(action.boneName)) {
                track->keyframes.push_back(action.newKeyframe);
                SortKeyframes(*track);
            }
            break;

        case AnimationEditAction::Type::RemoveKeyframe:
            if (auto* track = GetTrack(action.boneName)) {
                if (action.keyframeIndex < track->keyframes.size()) {
                    track->keyframes.erase(track->keyframes.begin() +
                        static_cast<ptrdiff_t>(action.keyframeIndex));
                }
            }
            break;

        case AnimationEditAction::Type::ModifyKeyframe:
        case AnimationEditAction::Type::MoveKeyframe:
            if (auto* track = GetTrack(action.boneName)) {
                if (action.keyframeIndex < track->keyframes.size()) {
                    track->keyframes[action.keyframeIndex] = action.newKeyframe;
                    SortKeyframes(*track);
                }
            }
            break;

        case AnimationEditAction::Type::AddEvent:
            m_events.push_back(action.newEvent);
            SortEvents();
            break;

        case AnimationEditAction::Type::RemoveEvent:
            if (action.keyframeIndex < m_events.size()) {
                m_events.erase(m_events.begin() + static_cast<ptrdiff_t>(action.keyframeIndex));
            }
            break;

        case AnimationEditAction::Type::ModifyDuration:
            m_duration = action.newDuration;
            break;

        case AnimationEditAction::Type::BatchTransform:
            for (auto& track : m_tracks) {
                auto it = action.newTracks.find(track.boneName);
                if (it != action.newTracks.end()) {
                    track.keyframes = it->second;
                }
            }
            break;

        default:
            break;
    }
}

void EditableAnimation::UnapplyAction(const AnimationEditAction& action) {
    switch (action.type) {
        case AnimationEditAction::Type::AddKeyframe:
            // Find and remove the added keyframe
            if (auto* track = GetTrack(action.boneName)) {
                for (size_t i = 0; i < track->keyframes.size(); ++i) {
                    if (std::abs(track->keyframes[i].time - action.newKeyframe.time) < 0.001f) {
                        track->keyframes.erase(track->keyframes.begin() + static_cast<ptrdiff_t>(i));
                        break;
                    }
                }
            }
            break;

        case AnimationEditAction::Type::RemoveKeyframe:
            if (auto* track = GetTrack(action.boneName)) {
                track->keyframes.insert(track->keyframes.begin() +
                    static_cast<ptrdiff_t>(action.keyframeIndex), action.oldKeyframe);
            }
            break;

        case AnimationEditAction::Type::ModifyKeyframe:
        case AnimationEditAction::Type::MoveKeyframe:
            if (auto* track = GetTrack(action.boneName)) {
                // Find by new time and restore old
                for (auto& kf : track->keyframes) {
                    if (std::abs(kf.time - action.newKeyframe.time) < 0.001f) {
                        kf = action.oldKeyframe;
                        break;
                    }
                }
                SortKeyframes(*track);
            }
            break;

        case AnimationEditAction::Type::AddEvent:
            // Remove added event
            for (size_t i = 0; i < m_events.size(); ++i) {
                if (std::abs(m_events[i].time - action.newEvent.time) < 0.001f &&
                    m_events[i].name == action.newEvent.name) {
                    m_events.erase(m_events.begin() + static_cast<ptrdiff_t>(i));
                    break;
                }
            }
            break;

        case AnimationEditAction::Type::RemoveEvent:
            m_events.insert(m_events.begin() + static_cast<ptrdiff_t>(action.keyframeIndex), action.oldEvent);
            break;

        case AnimationEditAction::Type::ModifyDuration:
            m_duration = action.oldDuration;
            break;

        case AnimationEditAction::Type::BatchTransform:
            for (auto& track : m_tracks) {
                auto it = action.oldTracks.find(track.boneName);
                if (it != action.oldTracks.end()) {
                    track.keyframes = it->second;
                }
            }
            break;

        default:
            break;
    }
}

void EditableAnimation::MarkDirty() {
    m_dirty = true;
    if (OnModified) {
        OnModified();
    }
}

void EditableAnimation::SortKeyframes(EditableBoneTrack& track) {
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const EditableKeyframe& a, const EditableKeyframe& b) {
            return a.time < b.time;
        });
}

void EditableAnimation::SortEvents() {
    std::sort(m_events.begin(), m_events.end(),
        [](const EditableAnimationEvent& a, const EditableAnimationEvent& b) {
            return a.time < b.time;
        });
}

EditableBoneTransform EditableAnimation::InterpolateKeyframes(const EditableKeyframe& a, const EditableKeyframe& b, float t) const {
    if (a.interpolation == "step") {
        return a.transform;
    }

    return EditableBoneTransform::Lerp(a.transform, b.transform, t);
}

} // namespace Nova
