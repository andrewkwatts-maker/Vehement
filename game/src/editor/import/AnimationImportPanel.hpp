#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace Nova {
class AnimationImportSettings;
struct ImportedAnimation;
struct ImportedClip;
}

namespace Vehement {

/**
 * @brief Animation import settings panel
 *
 * Features:
 * - Timeline preview with scrubbing
 * - Pose preview
 * - Clip splitting UI
 * - Root motion configuration
 * - Event marker editor
 */
class AnimationImportPanel {
public:
    AnimationImportPanel();
    ~AnimationImportPanel();

    void Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    /**
     * @brief Set the animation file to configure
     */
    void SetAnimationPath(const std::string& path);

    /**
     * @brief Get current settings
     */
    Nova::AnimationImportSettings* GetSettings() { return m_settings.get(); }

    /**
     * @brief Apply preset
     */
    void ApplyPreset(const std::string& preset);

    // Timeline controls
    void Play();
    void Pause();
    void Stop();
    void SeekTo(float time);
    bool IsPlaying() const { return m_isPlaying; }

    // Callbacks
    using SettingsChangedCallback = std::function<void()>;
    void SetSettingsChangedCallback(SettingsChangedCallback cb) { m_onSettingsChanged = cb; }

private:
    void RenderTimeline();
    void RenderPosePreview();
    void RenderClipList();
    void RenderClipSplitter();
    void RenderRootMotionSettings();
    void RenderCompressionSettings();
    void RenderRetargetingSettings();
    void RenderEventEditor();
    void RenderStatistics();

    void UpdatePreview();
    void LoadPreviewAnimation();

    std::string m_animationPath;
    std::unique_ptr<Nova::AnimationImportSettings> m_settings;

    // Preview data
    std::unique_ptr<Nova::ImportedAnimation> m_previewAnimation;

    // Playback state
    float m_currentTime = 0.0f;
    float m_playbackSpeed = 1.0f;
    bool m_isPlaying = false;
    bool m_loopPlayback = true;

    // Timeline UI
    float m_timelineZoom = 1.0f;
    float m_timelineOffset = 0.0f;
    bool m_isDraggingPlayhead = false;

    // Clip markers
    struct ClipMarker {
        std::string name;
        float startTime;
        float endTime;
        glm::vec4 color;
        bool selected;
    };
    std::vector<ClipMarker> m_clipMarkers;
    int m_selectedMarkerIndex = -1;
    bool m_isDraggingMarker = false;
    bool m_draggingMarkerStart = true;

    // Event markers
    struct EventMarker {
        std::string name;
        float time;
        glm::vec4 color;
    };
    std::vector<EventMarker> m_eventMarkers;
    int m_selectedEventIndex = -1;

    // Root motion graph
    std::vector<glm::vec2> m_rootMotionPath;
    float m_rootMotionGraphScale = 1.0f;

    // UI state
    bool m_previewDirty = true;
    bool m_showBoneNames = false;
    bool m_showRootMotion = true;

    SettingsChangedCallback m_onSettingsChanged;
};

} // namespace Vehement
