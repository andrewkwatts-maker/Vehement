#pragma once

#include "../KeyframeEditor.hpp"
#include <glm/glm.hpp>
#include <string>
#include <functional>
#include <vector>

namespace Vehement {

/**
 * @brief Animation curve editing panel
 *
 * Features:
 * - Bezier curve editor
 * - Handle manipulation
 * - Preset curves
 * - Multi-curve view
 */
class CurvePanel {
public:
    struct Config {
        float gridSize = 20.0f;
        bool showGrid = true;
        bool showValues = true;
        glm::vec4 curveColor{1.0f, 0.8f, 0.2f, 1.0f};
        glm::vec4 keyframeColor{1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 selectedColor{0.2f, 0.8f, 1.0f, 1.0f};
        glm::vec4 gridColor{0.3f, 0.3f, 0.35f, 1.0f};
    };

    /**
     * @brief Curve channel to display
     */
    enum class CurveChannel {
        PositionX,
        PositionY,
        PositionZ,
        RotationX,
        RotationY,
        RotationZ,
        ScaleX,
        ScaleY,
        ScaleZ
    };

    CurvePanel();
    ~CurvePanel();

    /**
     * @brief Initialize panel
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set keyframe editor reference
     */
    void SetKeyframeEditor(KeyframeEditor* editor) { m_keyframeEditor = editor; }

    /**
     * @brief Set selected bone
     */
    void SetSelectedBone(const std::string& boneName) { m_selectedBone = boneName; }

    /**
     * @brief Set visible channels
     */
    void SetVisibleChannels(const std::vector<CurveChannel>& channels) { m_visibleChannels = channels; }

    /**
     * @brief Toggle channel visibility
     */
    void ToggleChannel(CurveChannel channel);

    /**
     * @brief Render the panel (ImGui)
     */
    void Render();

    /**
     * @brief Set view range
     */
    void SetViewRange(float minTime, float maxTime, float minValue, float maxValue);

    /**
     * @brief Zoom to fit curves
     */
    void ZoomToFit();

    /**
     * @brief Set current time (playhead)
     */
    void SetCurrentTime(float time) { m_currentTime = time; }

    // Callbacks
    std::function<void(float)> OnTimeChanged;
    std::function<void(size_t, CurveChannel)> OnKeyframeSelected;
    std::function<void()> OnCurveModified;

private:
    void RenderToolbar();
    void RenderCurveView();
    void RenderCurve(CurveChannel channel, const glm::vec4& color);
    void RenderKeyframe(float time, float value, bool selected, const glm::vec4& color);
    void RenderTangentHandles(float time, float value, const TangentHandle& tangent);
    void RenderPlayhead();
    void RenderGrid();

    glm::vec2 ValueToScreen(float time, float value) const;
    glm::vec2 ScreenToValue(const glm::vec2& screen) const;
    float GetChannelValue(const BoneTransform& transform, CurveChannel channel) const;
    void SetChannelValue(BoneTransform& transform, CurveChannel channel, float value);
    std::string GetChannelName(CurveChannel channel) const;
    glm::vec4 GetChannelColor(CurveChannel channel) const;

    Config m_config;
    KeyframeEditor* m_keyframeEditor = nullptr;

    std::string m_selectedBone;
    std::vector<CurveChannel> m_visibleChannels;
    float m_currentTime = 0.0f;

    // View
    float m_viewMinTime = 0.0f;
    float m_viewMaxTime = 1.0f;
    float m_viewMinValue = -1.0f;
    float m_viewMaxValue = 1.0f;
    glm::vec2 m_viewOffset{0.0f};
    float m_viewZoom = 1.0f;

    // Interaction
    bool m_isDragging = false;
    bool m_isPanning = false;
    int m_selectedKeyframe = -1;
    CurveChannel m_selectedChannel = CurveChannel::PositionX;
    bool m_editingInTangent = false;
    bool m_editingOutTangent = false;

    // Panel geometry
    glm::vec2 m_curveAreaMin{0.0f};
    glm::vec2 m_curveAreaMax{0.0f};

    bool m_initialized = false;
};

} // namespace Vehement
