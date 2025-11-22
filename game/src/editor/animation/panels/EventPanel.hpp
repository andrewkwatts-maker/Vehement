#pragma once

#include "../AnimationTimeline.hpp"
#include <string>
#include <functional>
#include <vector>

namespace Vehement {

/**
 * @brief Animation events panel
 *
 * Features:
 * - List of animation events
 * - Add/edit/remove events
 * - Event parameters
 */
class EventPanel {
public:
    struct Config {
        bool showTimeColumn = true;
        bool showParametersInline = true;
    };

    EventPanel();
    ~EventPanel();

    /**
     * @brief Initialize panel
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Set timeline reference
     */
    void SetTimeline(AnimationTimeline* timeline) { m_timeline = timeline; }

    /**
     * @brief Set current time
     */
    void SetCurrentTime(float time) { m_currentTime = time; }

    /**
     * @brief Render the panel (ImGui)
     */
    void Render();

    // Callbacks
    std::function<void(const std::string&)> OnEventSelected;
    std::function<void(float)> OnTimeChanged;
    std::function<void()> OnEventAdded;
    std::function<void()> OnEventRemoved;
    std::function<void()> OnEventModified;

private:
    void RenderEventList();
    void RenderEventDetails();
    void RenderAddEventDialog();

    Config m_config;
    AnimationTimeline* m_timeline = nullptr;

    float m_currentTime = 0.0f;
    int m_selectedEventIndex = -1;

    // Add event dialog
    bool m_showAddDialog = false;
    char m_newEventName[256] = "";
    char m_newEventFunction[256] = "";
    float m_newEventTime = 0.0f;

    bool m_initialized = false;
};

} // namespace Vehement
