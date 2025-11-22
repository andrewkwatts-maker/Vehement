#include "EventPanel.hpp"
#include <imgui.h>
#include <cstring>

namespace Vehement {

EventPanel::EventPanel() = default;
EventPanel::~EventPanel() = default;

void EventPanel::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void EventPanel::Render() {
    if (!m_initialized || !m_timeline) return;

    // Toolbar
    if (ImGui::Button("Add Event")) {
        m_showAddDialog = true;
        m_newEventTime = m_currentTime;
        std::strncpy(m_newEventName, "", sizeof(m_newEventName));
        std::strncpy(m_newEventFunction, "", sizeof(m_newEventFunction));
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete") && m_selectedEventIndex >= 0) {
        m_timeline->RemoveEventMarker(m_timeline->GetEventMarkers()[static_cast<size_t>(m_selectedEventIndex)].name);
        m_selectedEventIndex = -1;
        if (OnEventRemoved) {
            OnEventRemoved();
        }
    }

    ImGui::Separator();

    // Event list
    RenderEventList();

    ImGui::Separator();

    // Event details
    if (m_selectedEventIndex >= 0) {
        RenderEventDetails();
    } else {
        ImGui::TextDisabled("Select an event to edit");
    }

    // Add event dialog
    if (m_showAddDialog) {
        RenderAddEventDialog();
    }
}

void EventPanel::RenderEventList() {
    const auto& events = m_timeline->GetEventMarkers();

    if (events.empty()) {
        ImGui::TextDisabled("No events");
        return;
    }

    ImGui::BeginChild("EventList", ImVec2(0, 150), true);

    // Table header
    if (m_config.showTimeColumn) {
        ImGui::Columns(3, "events_columns");
        ImGui::SetColumnWidth(0, 60);
        ImGui::SetColumnWidth(1, 150);
        ImGui::Text("Time");
        ImGui::NextColumn();
        ImGui::Text("Name");
        ImGui::NextColumn();
        ImGui::Text("Function");
        ImGui::NextColumn();
        ImGui::Separator();
    }

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];
        bool isSelected = (static_cast<int>(i) == m_selectedEventIndex);

        if (m_config.showTimeColumn) {
            // Time column
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%.3f", event.time);
            if (ImGui::Selectable(timeStr, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedEventIndex = static_cast<int>(i);
                if (OnEventSelected) {
                    OnEventSelected(event.name);
                }
            }
            ImGui::NextColumn();

            // Name column
            ImGui::Text("%s", event.name.c_str());
            ImGui::NextColumn();

            // Function column
            ImGui::Text("%s", event.functionName.c_str());
            ImGui::NextColumn();
        } else {
            char label[128];
            snprintf(label, sizeof(label), "%.3f - %s##%zu", event.time, event.name.c_str(), i);
            if (ImGui::Selectable(label, isSelected)) {
                m_selectedEventIndex = static_cast<int>(i);
                if (OnEventSelected) {
                    OnEventSelected(event.name);
                }
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Go to time")) {
                if (OnTimeChanged) {
                    OnTimeChanged(event.time);
                }
            }
            if (ImGui::MenuItem("Duplicate")) {
                auto* newEvent = m_timeline->AddEventMarker(event.name + "_copy", event.time + 0.1f);
                if (newEvent) {
                    newEvent->functionName = event.functionName;
                    newEvent->parameter = event.parameter;
                }
            }
            if (ImGui::MenuItem("Delete")) {
                m_timeline->RemoveEventMarker(event.name);
                m_selectedEventIndex = -1;
                if (OnEventRemoved) {
                    OnEventRemoved();
                }
            }
            ImGui::EndPopup();
        }

        // Double-click to go to time
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnTimeChanged) {
                OnTimeChanged(event.time);
            }
        }
    }

    if (m_config.showTimeColumn) {
        ImGui::Columns(1);
    }

    ImGui::EndChild();
}

void EventPanel::RenderEventDetails() {
    const auto& events = m_timeline->GetEventMarkers();
    if (m_selectedEventIndex < 0 || static_cast<size_t>(m_selectedEventIndex) >= events.size()) {
        return;
    }

    // Need to get non-const access to edit
    EventMarker* event = m_timeline->GetEventMarker(events[static_cast<size_t>(m_selectedEventIndex)].name);
    if (!event) return;

    ImGui::Text("Event Details");

    // Time
    float time = event->time;
    if (ImGui::DragFloat("Time", &time, 0.01f, 0.0f, m_timeline->GetDuration())) {
        event->time = time;
        if (OnEventModified) {
            OnEventModified();
        }
    }

    // Name (read-only display, need to track separately for rename)
    ImGui::Text("Name: %s", event->name.c_str());

    // Function name
    char funcBuffer[256];
    std::strncpy(funcBuffer, event->functionName.c_str(), sizeof(funcBuffer) - 1);
    funcBuffer[sizeof(funcBuffer) - 1] = '\0';
    if (ImGui::InputText("Function", funcBuffer, sizeof(funcBuffer))) {
        event->functionName = funcBuffer;
        if (OnEventModified) {
            OnEventModified();
        }
    }

    // Parameter
    char paramBuffer[256];
    std::strncpy(paramBuffer, event->parameter.c_str(), sizeof(paramBuffer) - 1);
    paramBuffer[sizeof(paramBuffer) - 1] = '\0';
    if (ImGui::InputText("Parameter", paramBuffer, sizeof(paramBuffer))) {
        event->parameter = paramBuffer;
        if (OnEventModified) {
            OnEventModified();
        }
    }

    // Color
    float color[4] = {event->color.r, event->color.g, event->color.b, event->color.a};
    if (ImGui::ColorEdit4("Color", color)) {
        event->color = glm::vec4(color[0], color[1], color[2], color[3]);
        if (OnEventModified) {
            OnEventModified();
        }
    }
}

void EventPanel::RenderAddEventDialog() {
    ImGui::OpenPopup("Add Event");

    if (ImGui::BeginPopupModal("Add Event", &m_showAddDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", m_newEventName, sizeof(m_newEventName));
        ImGui::InputText("Function", m_newEventFunction, sizeof(m_newEventFunction));
        ImGui::DragFloat("Time", &m_newEventTime, 0.01f, 0.0f, m_timeline->GetDuration());

        ImGui::Separator();

        if (ImGui::Button("Add", ImVec2(120, 0))) {
            if (std::strlen(m_newEventName) > 0) {
                auto* event = m_timeline->AddEventMarker(m_newEventName, m_newEventTime);
                if (event) {
                    event->functionName = m_newEventFunction;
                }
                m_showAddDialog = false;
                if (OnEventAdded) {
                    OnEventAdded();
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showAddDialog = false;
        }

        ImGui::EndPopup();
    }
}

} // namespace Vehement
