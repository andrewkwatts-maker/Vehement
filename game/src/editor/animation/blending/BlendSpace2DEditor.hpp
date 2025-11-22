#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class BlendSpace2D;
class Skeleton;
class Animation;
struct AnimationPose;
}

namespace Vehement {

class Editor;

/**
 * @brief Editor for 2D blend spaces
 *
 * Provides visual editing of 2D blend spaces with:
 * - 2D grid with clip positions
 * - Drag clips in 2D space
 * - Triangulation visualization
 * - Cursor position preview
 */
class BlendSpace2DEditor {
public:
    /**
     * @brief Sample point for display
     */
    struct SamplePoint {
        size_t index = 0;
        std::string clipName;
        glm::vec2 position{0.0f};
        float weight = 0.0f;
        bool selected = false;
        bool dragging = false;
        glm::vec2 screenPos{0.0f};
    };

    /**
     * @brief Triangle for visualization
     */
    struct TriangleDisplay {
        glm::vec2 v0, v1, v2;
        bool containsCursor = false;
    };

    explicit BlendSpace2DEditor(Editor* editor);
    ~BlendSpace2DEditor();

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

    void SetBlendSpace(Nova::BlendSpace2D* blendSpace);
    [[nodiscard]] Nova::BlendSpace2D* GetBlendSpace() const { return m_blendSpace; }

    void SetSkeleton(Nova::Skeleton* skeleton);

    // =========================================================================
    // Editing
    // =========================================================================

    void AddSample(const Nova::Animation* clip, const glm::vec2& position);
    void RemoveSelectedSample();
    void MoveSample(size_t index, const glm::vec2& newPosition);
    void SelectSample(size_t index);
    void ClearSelection();

    [[nodiscard]] int GetSelectedSampleIndex() const { return m_selectedSampleIndex; }

    // =========================================================================
    // Preview
    // =========================================================================

    void SetPreviewPosition(const glm::vec2& position);
    [[nodiscard]] glm::vec2 GetPreviewPosition() const { return m_previewPosition; }

    void SetPreviewTime(float normalizedTime);
    [[nodiscard]] float GetPreviewTime() const { return m_previewTime; }

    void SetPreviewEnabled(bool enabled) { m_previewEnabled = enabled; }
    [[nodiscard]] bool IsPreviewEnabled() const { return m_previewEnabled; }

    void PlayPreview();
    void PausePreview();
    void ResetPreview();

    [[nodiscard]] const Nova::AnimationPose* GetPreviewPose() const;

    // =========================================================================
    // Display
    // =========================================================================

    [[nodiscard]] std::vector<SamplePoint> GetSamplePoints() const;
    [[nodiscard]] std::vector<TriangleDisplay> GetTriangles() const;

    [[nodiscard]] glm::vec2 GetMinBounds() const;
    [[nodiscard]] glm::vec2 GetMaxBounds() const;

    /**
     * @brief Enable triangulation visualization
     */
    void SetShowTriangulation(bool show) { m_showTriangulation = show; }
    [[nodiscard]] bool GetShowTriangulation() const { return m_showTriangulation; }

    /**
     * @brief Enable weight gradient visualization
     */
    void SetShowWeightGradient(bool show) { m_showWeightGradient = show; }
    [[nodiscard]] bool GetShowWeightGradient() const { return m_showWeightGradient; }

    // =========================================================================
    // Input
    // =========================================================================

    bool OnMouseDown(const glm::vec2& pos, int button);
    void OnMouseMove(const glm::vec2& pos);
    void OnMouseUp(const glm::vec2& pos, int button);
    bool OnKeyDown(int key);

    // =========================================================================
    // Visibility & Layout
    // =========================================================================

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetCanvasBounds(const glm::vec2& pos, const glm::vec2& size);
    [[nodiscard]] glm::vec2 GetCanvasPosition() const { return m_canvasPos; }
    [[nodiscard]] glm::vec2 GetCanvasSize() const { return m_canvasSize; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnBlendSpaceChanged;
    std::function<void(size_t)> OnSampleSelected;
    std::function<void(const glm::vec2&)> OnPreviewPositionChanged;

private:
    glm::vec2 ValueToScreen(const glm::vec2& value) const;
    glm::vec2 ScreenToValue(const glm::vec2& screen) const;
    int FindSampleAtPosition(const glm::vec2& pos) const;

    Editor* m_editor = nullptr;
    Nova::BlendSpace2D* m_blendSpace = nullptr;
    Nova::Skeleton* m_skeleton = nullptr;

    bool m_visible = true;
    bool m_initialized = false;

    // Canvas layout
    glm::vec2 m_canvasPos{50.0f, 50.0f};
    glm::vec2 m_canvasSize{400.0f, 400.0f};
    float m_pointRadius = 15.0f;

    // Selection
    int m_selectedSampleIndex = -1;
    int m_draggingSampleIndex = -1;

    // Preview
    glm::vec2 m_previewPosition{0.0f};
    float m_previewTime = 0.0f;
    bool m_previewEnabled = true;
    bool m_previewPlaying = false;
    std::unique_ptr<Nova::AnimationPose> m_previewPose;

    // Display options
    bool m_showTriangulation = true;
    bool m_showWeightGradient = false;
};

} // namespace Vehement
