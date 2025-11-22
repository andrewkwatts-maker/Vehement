#pragma once

#include "../KeyframeEditor.hpp"
#include <string>
#include <functional>
#include <vector>

namespace Vehement {

/**
 * @brief Keyframe list panel for keyframe management
 *
 * Features:
 * - List of keyframes for selected bone
 * - Add/remove keyframes
 * - Edit keyframe properties
 * - Copy/paste keyframes
 */
class KeyframePanel {
public:
    struct Config {
        bool showTransformDetails = true;
        bool showInterpolationOptions = true;
        bool showTangentControls = true;
    };

    KeyframePanel();
    ~KeyframePanel();

    /**
     * @brief Initialize panel
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set keyframe editor reference
     */
    void SetKeyframeEditor(KeyframeEditor* editor) { m_keyframeEditor = editor; }

    /**
     * @brief Set selected bone name
     */
    void SetSelectedBone(const std::string& boneName) { m_selectedBone = boneName; }

    /**
     * @brief Set current time
     */
    void SetCurrentTime(float time) { m_currentTime = time; }

    /**
     * @brief Render the panel (ImGui)
     */
    void Render();

    // Callbacks
    std::function<void(size_t)> OnKeyframeSelected;
    std::function<void(float)> OnTimeChanged;
    std::function<void()> OnKeyframeAdded;
    std::function<void()> OnKeyframeRemoved;

private:
    void RenderKeyframeList();
    void RenderKeyframeDetails();
    void RenderTransformEditor(Keyframe& kf);
    void RenderInterpolationEditor(Keyframe& kf);

    Config m_config;
    KeyframeEditor* m_keyframeEditor = nullptr;

    std::string m_selectedBone;
    float m_currentTime = 0.0f;
    int m_selectedKeyframeIndex = -1;

    bool m_initialized = false;
};

} // namespace Vehement
