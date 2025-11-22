#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class BlendSpace1D;
class Skeleton;
class Animation;
struct AnimationPose;
}

namespace Vehement {

class Editor;

/**
 * @brief Editor for 1D blend spaces
 *
 * Provides visual editing of 1D blend spaces with:
 * - Horizontal slider with clip markers
 * - Drag clips to positions
 * - Preview at any position
 * - Motion sync indicators
 */
class BlendSpace1DEditor {
public:
    /**
     * @brief Sample marker for display
     */
    struct SampleMarker {
        size_t index = 0;
        std::string clipName;
        float position = 0.0f;
        float weight = 0.0f;     // Current blend weight (0-1)
        bool selected = false;
        bool dragging = false;

        // Display info
        glm::vec2 screenPos{0.0f};
        float thumbnailTime = 0.0f;
    };

    /**
     * @brief Sync marker for display
     */
    struct SyncMarkerDisplay {
        std::string name;
        float normalizedTime = 0.0f;
        glm::vec4 color{1.0f};
    };

    explicit BlendSpace1DEditor(Editor* editor);
    ~BlendSpace1DEditor();

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize();
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    // =========================================================================
    // Data
    // =========================================================================

    /**
     * @brief Set the blend space to edit
     */
    void SetBlendSpace(Nova::BlendSpace1D* blendSpace);
    [[nodiscard]] Nova::BlendSpace1D* GetBlendSpace() const { return m_blendSpace; }

    /**
     * @brief Set skeleton for preview
     */
    void SetSkeleton(Nova::Skeleton* skeleton);

    // =========================================================================
    // Editing
    // =========================================================================

    /**
     * @brief Add a sample at position
     */
    void AddSample(const Nova::Animation* clip, float position);

    /**
     * @brief Remove selected sample
     */
    void RemoveSelectedSample();

    /**
     * @brief Move sample to new position
     */
    void MoveSample(size_t index, float newPosition);

    /**
     * @brief Select sample
     */
    void SelectSample(size_t index);

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    /**
     * @brief Get selected sample index
     */
    [[nodiscard]] int GetSelectedSampleIndex() const { return m_selectedSampleIndex; }

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Set preview parameter value
     */
    void SetPreviewValue(float value);
    [[nodiscard]] float GetPreviewValue() const { return m_previewValue; }

    /**
     * @brief Set preview time (normalized)
     */
    void SetPreviewTime(float normalizedTime);
    [[nodiscard]] float GetPreviewTime() const { return m_previewTime; }

    /**
     * @brief Enable/disable live preview
     */
    void SetPreviewEnabled(bool enabled) { m_previewEnabled = enabled; }
    [[nodiscard]] bool IsPreviewEnabled() const { return m_previewEnabled; }

    /**
     * @brief Play/pause preview animation
     */
    void PlayPreview();
    void PausePreview();
    void ResetPreview();

    /**
     * @brief Get current preview pose
     */
    [[nodiscard]] const Nova::AnimationPose* GetPreviewPose() const;

    // =========================================================================
    // Display
    // =========================================================================

    /**
     * @brief Get sample markers for rendering
     */
    [[nodiscard]] std::vector<SampleMarker> GetSampleMarkers() const;

    /**
     * @brief Get sync markers for rendering
     */
    [[nodiscard]] std::vector<SyncMarkerDisplay> GetSyncMarkers() const;

    /**
     * @brief Get parameter range
     */
    [[nodiscard]] float GetMinValue() const;
    [[nodiscard]] float GetMaxValue() const;

    // =========================================================================
    // Input
    // =========================================================================

    /**
     * @brief Handle mouse down
     */
    bool OnMouseDown(const glm::vec2& pos, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& pos);

    /**
     * @brief Handle mouse up
     */
    void OnMouseUp(const glm::vec2& pos, int button);

    /**
     * @brief Handle key press
     */
    bool OnKeyDown(int key);

    // =========================================================================
    // Visibility
    // =========================================================================

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // =========================================================================
    // Layout
    // =========================================================================

    void SetTrackBounds(const glm::vec2& pos, const glm::vec2& size);
    [[nodiscard]] glm::vec2 GetTrackPosition() const { return m_trackPos; }
    [[nodiscard]] glm::vec2 GetTrackSize() const { return m_trackSize; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnBlendSpaceChanged;
    std::function<void(size_t)> OnSampleSelected;
    std::function<void(float)> OnPreviewValueChanged;

private:
    float ValueToScreenX(float value) const;
    float ScreenXToValue(float screenX) const;
    int FindSampleAtPosition(const glm::vec2& pos) const;
    void UpdateSampleWeights();

    Editor* m_editor = nullptr;
    Nova::BlendSpace1D* m_blendSpace = nullptr;
    Nova::Skeleton* m_skeleton = nullptr;

    bool m_visible = true;
    bool m_initialized = false;

    // Track layout
    glm::vec2 m_trackPos{50.0f, 100.0f};
    glm::vec2 m_trackSize{600.0f, 80.0f};
    float m_markerRadius = 20.0f;

    // Selection
    int m_selectedSampleIndex = -1;
    int m_draggingSampleIndex = -1;

    // Preview
    float m_previewValue = 0.5f;
    float m_previewTime = 0.0f;
    bool m_previewEnabled = true;
    bool m_previewPlaying = false;
    std::unique_ptr<Nova::AnimationPose> m_previewPose;
};

} // namespace Vehement
