#include "KeyframePanel.hpp"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Vehement {

KeyframePanel::KeyframePanel() = default;
KeyframePanel::~KeyframePanel() = default;

void KeyframePanel::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void KeyframePanel::Render() {
    if (!m_initialized || !m_keyframeEditor) return;

    // Header
    ImGui::Text("Bone: %s", m_selectedBone.empty() ? "(none)" : m_selectedBone.c_str());
    ImGui::Separator();

    if (m_selectedBone.empty()) {
        ImGui::TextDisabled("Select a bone to view keyframes");
        return;
    }

    // Add keyframe button
    if (ImGui::Button("Add Keyframe")) {
        m_keyframeEditor->AddKeyframe(m_selectedBone, m_currentTime);
        if (OnKeyframeAdded) {
            OnKeyframeAdded();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Selected") && m_selectedKeyframeIndex >= 0) {
        m_keyframeEditor->RemoveKeyframe(m_selectedBone, static_cast<size_t>(m_selectedKeyframeIndex));
        m_selectedKeyframeIndex = -1;
        if (OnKeyframeRemoved) {
            OnKeyframeRemoved();
        }
    }

    ImGui::Separator();

    // Keyframe list
    RenderKeyframeList();

    ImGui::Separator();

    // Keyframe details
    if (m_selectedKeyframeIndex >= 0) {
        RenderKeyframeDetails();
    } else {
        ImGui::TextDisabled("Select a keyframe to edit");
    }
}

void KeyframePanel::RenderKeyframeList() {
    BoneTrack* track = m_keyframeEditor->GetTrack(m_selectedBone);
    if (!track) {
        ImGui::TextDisabled("No keyframes");
        return;
    }

    ImGui::BeginChild("KeyframeList", ImVec2(0, 150), true);

    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        const auto& kf = track->keyframes[i];

        bool isSelected = (static_cast<int>(i) == m_selectedKeyframeIndex);

        char label[64];
        snprintf(label, sizeof(label), "%.3fs##%zu", kf.time, i);

        if (ImGui::Selectable(label, isSelected)) {
            m_selectedKeyframeIndex = static_cast<int>(i);
            if (OnKeyframeSelected) {
                OnKeyframeSelected(i);
            }
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Go to time")) {
                if (OnTimeChanged) {
                    OnTimeChanged(kf.time);
                }
            }
            if (ImGui::MenuItem("Delete")) {
                m_keyframeEditor->RemoveKeyframe(m_selectedBone, i);
                m_selectedKeyframeIndex = -1;
                if (OnKeyframeRemoved) {
                    OnKeyframeRemoved();
                }
            }
            if (ImGui::MenuItem("Duplicate")) {
                m_keyframeEditor->AddKeyframe(m_selectedBone, kf.time + 0.1f, kf.transform);
            }
            ImGui::EndPopup();
        }

        // Double-click to go to time
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnTimeChanged) {
                OnTimeChanged(kf.time);
            }
        }
    }

    ImGui::EndChild();
}

void KeyframePanel::RenderKeyframeDetails() {
    BoneTrack* track = m_keyframeEditor->GetTrack(m_selectedBone);
    if (!track || m_selectedKeyframeIndex < 0 ||
        static_cast<size_t>(m_selectedKeyframeIndex) >= track->keyframes.size()) {
        return;
    }

    Keyframe& kf = track->keyframes[static_cast<size_t>(m_selectedKeyframeIndex)];

    ImGui::Text("Keyframe Details");

    // Time
    float time = kf.time;
    if (ImGui::DragFloat("Time", &time, 0.01f, 0.0f, m_keyframeEditor->GetDuration())) {
        m_keyframeEditor->MoveKeyframe(m_selectedBone, static_cast<size_t>(m_selectedKeyframeIndex), time);
    }

    if (m_config.showTransformDetails) {
        RenderTransformEditor(kf);
    }

    if (m_config.showInterpolationOptions) {
        RenderInterpolationEditor(kf);
    }
}

void KeyframePanel::RenderTransformEditor(Keyframe& kf) {
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Position
        glm::vec3 pos = kf.transform.position;
        if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.01f)) {
            kf.transform.position = pos;
        }

        // Rotation (as euler angles for display)
        glm::vec3 euler = glm::degrees(glm::eulerAngles(kf.transform.rotation));
        if (ImGui::DragFloat3("Rotation", glm::value_ptr(euler), 1.0f)) {
            kf.transform.rotation = glm::quat(glm::radians(euler));
        }

        // Scale
        glm::vec3 scale = kf.transform.scale;
        if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f, 0.001f, 10.0f)) {
            kf.transform.scale = scale;
        }
    }
}

void KeyframePanel::RenderInterpolationEditor(Keyframe& kf) {
    if (ImGui::CollapsingHeader("Interpolation")) {
        const char* items[] = {"Linear", "Step", "Bezier", "Hermite", "Catmull-Rom"};
        int currentItem = static_cast<int>(kf.interpolation);

        if (ImGui::Combo("Mode", &currentItem, items, IM_ARRAYSIZE(items))) {
            kf.interpolation = static_cast<InterpolationMode>(currentItem);
        }

        if (m_config.showTangentControls && kf.interpolation == InterpolationMode::Bezier) {
            ImGui::Text("Tangents");

            glm::vec2 inTangent = kf.tangent.inTangent;
            if (ImGui::DragFloat2("In Tangent", glm::value_ptr(inTangent), 0.01f)) {
                kf.tangent.inTangent = inTangent;
            }

            glm::vec2 outTangent = kf.tangent.outTangent;
            if (ImGui::DragFloat2("Out Tangent", glm::value_ptr(outTangent), 0.01f)) {
                kf.tangent.outTangent = outTangent;
            }

            const char* tangentModes[] = {"Free", "Aligned", "Mirrored", "Auto", "Flat", "Linear"};
            int tangentMode = static_cast<int>(kf.tangent.mode);
            if (ImGui::Combo("Tangent Mode", &tangentMode, tangentModes, IM_ARRAYSIZE(tangentModes))) {
                kf.tangent.mode = static_cast<TangentMode>(tangentMode);
            }
        }
    }
}

} // namespace Vehement
