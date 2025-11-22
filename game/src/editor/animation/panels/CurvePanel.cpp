#include "CurvePanel.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Vehement {

CurvePanel::CurvePanel() {
    // Default visible channels
    m_visibleChannels = {CurveChannel::PositionY, CurveChannel::RotationY};
}

CurvePanel::~CurvePanel() = default;

void CurvePanel::Initialize(const Config& config) {
    m_config = config;
    m_initialized = true;
}

void CurvePanel::ToggleChannel(CurveChannel channel) {
    auto it = std::find(m_visibleChannels.begin(), m_visibleChannels.end(), channel);
    if (it != m_visibleChannels.end()) {
        m_visibleChannels.erase(it);
    } else {
        m_visibleChannels.push_back(channel);
    }
}

void CurvePanel::Render() {
    if (!m_initialized || !m_keyframeEditor) return;

    RenderToolbar();
    ImGui::Separator();
    RenderCurveView();
}

void CurvePanel::SetViewRange(float minTime, float maxTime, float minValue, float maxValue) {
    m_viewMinTime = minTime;
    m_viewMaxTime = maxTime;
    m_viewMinValue = minValue;
    m_viewMaxValue = maxValue;
}

void CurvePanel::ZoomToFit() {
    if (m_selectedBone.empty()) return;

    BoneTrack* track = m_keyframeEditor->GetTrack(m_selectedBone);
    if (!track || track->keyframes.empty()) return;

    m_viewMinTime = 0.0f;
    m_viewMaxTime = m_keyframeEditor->GetDuration();

    // Find value range
    float minVal = std::numeric_limits<float>::max();
    float maxVal = std::numeric_limits<float>::lowest();

    for (const auto& kf : track->keyframes) {
        for (CurveChannel channel : m_visibleChannels) {
            float value = GetChannelValue(kf.transform, channel);
            minVal = std::min(minVal, value);
            maxVal = std::max(maxVal, value);
        }
    }

    // Add padding
    float padding = (maxVal - minVal) * 0.1f;
    if (padding < 0.1f) padding = 0.5f;

    m_viewMinValue = minVal - padding;
    m_viewMaxValue = maxVal + padding;
}

void CurvePanel::RenderToolbar() {
    // Channel toggles
    ImGui::Text("Channels:");
    ImGui::SameLine();

    struct ChannelInfo { CurveChannel channel; const char* label; };
    ChannelInfo channels[] = {
        {CurveChannel::PositionX, "Pos X"},
        {CurveChannel::PositionY, "Pos Y"},
        {CurveChannel::PositionZ, "Pos Z"},
        {CurveChannel::RotationX, "Rot X"},
        {CurveChannel::RotationY, "Rot Y"},
        {CurveChannel::RotationZ, "Rot Z"},
        {CurveChannel::ScaleX, "Scl X"},
        {CurveChannel::ScaleY, "Scl Y"},
        {CurveChannel::ScaleZ, "Scl Z"}
    };

    for (const auto& info : channels) {
        bool enabled = std::find(m_visibleChannels.begin(), m_visibleChannels.end(), info.channel) != m_visibleChannels.end();
        glm::vec4 color = GetChannelColor(info.channel);

        if (enabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.r, color.g, color.b, 0.7f));
        }

        if (ImGui::SmallButton(info.label)) {
            ToggleChannel(info.channel);
        }

        if (enabled) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    }

    ImGui::NewLine();

    // View controls
    if (ImGui::Button("Fit")) {
        ZoomToFit();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_viewMinTime = 0.0f;
        m_viewMaxTime = m_keyframeEditor->GetDuration();
        m_viewMinValue = -1.0f;
        m_viewMaxValue = 1.0f;
    }

    ImGui::SameLine();
    ImGui::Text("Bone: %s", m_selectedBone.empty() ? "(none)" : m_selectedBone.c_str());
}

void CurvePanel::RenderCurveView() {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    if (canvasSize.x < 50.0f) canvasSize.x = 50.0f;
    if (canvasSize.y < 50.0f) canvasSize.y = 50.0f;

    m_curveAreaMin = glm::vec2(canvasPos.x, canvasPos.y);
    m_curveAreaMax = glm::vec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(
        ImVec2(m_curveAreaMin.x, m_curveAreaMin.y),
        ImVec2(m_curveAreaMax.x, m_curveAreaMax.y),
        IM_COL32(30, 30, 35, 255)
    );

    // Render grid
    if (m_config.showGrid) {
        RenderGrid();
    }

    // Render curves
    for (CurveChannel channel : m_visibleChannels) {
        RenderCurve(channel, GetChannelColor(channel));
    }

    // Render playhead
    RenderPlayhead();

    // Handle input
    ImGui::InvisibleButton("curve_canvas", canvasSize);

    if (ImGui::IsItemHovered()) {
        // Scroll to zoom
        float wheel = ImGui::GetIO().MouseWheel;
        if (std::abs(wheel) > 0.01f) {
            float zoom = 1.0f + wheel * 0.1f;
            float centerTime = (m_viewMinTime + m_viewMaxTime) * 0.5f;
            float range = m_viewMaxTime - m_viewMinTime;
            m_viewMinTime = centerTime - range * 0.5f / zoom;
            m_viewMaxTime = centerTime + range * 0.5f / zoom;
        }

        // Middle mouse drag to pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float timeRange = m_viewMaxTime - m_viewMinTime;
            float valueRange = m_viewMaxValue - m_viewMinValue;

            float timeDelta = -delta.x / canvasSize.x * timeRange;
            float valueDelta = delta.y / canvasSize.y * valueRange;

            m_viewMinTime += timeDelta;
            m_viewMaxTime += timeDelta;
            m_viewMinValue += valueDelta;
            m_viewMaxValue += valueDelta;
        }

        // Left click to set time
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mousePos = ImGui::GetMousePos();
            glm::vec2 pos(mousePos.x, mousePos.y);
            glm::vec2 value = ScreenToValue(pos);

            if (OnTimeChanged) {
                OnTimeChanged(std::clamp(value.x, 0.0f, m_keyframeEditor->GetDuration()));
            }
        }
    }

    // Border
    drawList->AddRect(
        ImVec2(m_curveAreaMin.x, m_curveAreaMin.y),
        ImVec2(m_curveAreaMax.x, m_curveAreaMax.y),
        IM_COL32(80, 80, 90, 255)
    );
}

void CurvePanel::RenderCurve(CurveChannel channel, const glm::vec4& color) {
    if (m_selectedBone.empty()) return;

    BoneTrack* track = m_keyframeEditor->GetTrack(m_selectedBone);
    if (!track || track->keyframes.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 lineColor = IM_COL32(
        static_cast<int>(color.r * 255),
        static_cast<int>(color.g * 255),
        static_cast<int>(color.b * 255),
        static_cast<int>(color.a * 255)
    );

    // Draw curve by sampling
    const int numSamples = 100;
    float timeStep = (m_viewMaxTime - m_viewMinTime) / static_cast<float>(numSamples);

    std::vector<ImVec2> points;
    points.reserve(numSamples + 1);

    for (int i = 0; i <= numSamples; ++i) {
        float time = m_viewMinTime + i * timeStep;
        BoneTransform transform = m_keyframeEditor->EvaluateTransform(m_selectedBone, time);
        float value = GetChannelValue(transform, channel);

        glm::vec2 screen = ValueToScreen(time, value);
        points.emplace_back(screen.x, screen.y);
    }

    if (points.size() >= 2) {
        drawList->AddPolyline(points.data(), static_cast<int>(points.size()), lineColor, 0, 1.5f);
    }

    // Draw keyframes
    for (size_t i = 0; i < track->keyframes.size(); ++i) {
        const auto& kf = track->keyframes[i];
        float value = GetChannelValue(kf.transform, channel);
        bool isSelected = (static_cast<int>(i) == m_selectedKeyframe && channel == m_selectedChannel);
        RenderKeyframe(kf.time, value, isSelected, color);
    }
}

void CurvePanel::RenderKeyframe(float time, float value, bool selected, const glm::vec4& color) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    glm::vec2 screen = ValueToScreen(time, value);

    float size = selected ? 6.0f : 4.0f;

    ImU32 fillColor = selected ?
        IM_COL32(255, 255, 255, 255) :
        IM_COL32(
            static_cast<int>(color.r * 255),
            static_cast<int>(color.g * 255),
            static_cast<int>(color.b * 255),
            255
        );

    // Diamond shape
    drawList->AddQuadFilled(
        ImVec2(screen.x, screen.y - size),
        ImVec2(screen.x + size, screen.y),
        ImVec2(screen.x, screen.y + size),
        ImVec2(screen.x - size, screen.y),
        fillColor
    );

    drawList->AddQuad(
        ImVec2(screen.x, screen.y - size),
        ImVec2(screen.x + size, screen.y),
        ImVec2(screen.x, screen.y + size),
        ImVec2(screen.x - size, screen.y),
        IM_COL32(255, 255, 255, 200),
        1.0f
    );
}

void CurvePanel::RenderTangentHandles(float /*time*/, float /*value*/, const TangentHandle& /*tangent*/) {
    // Would render bezier tangent handles
}

void CurvePanel::RenderPlayhead() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 top = ValueToScreen(m_currentTime, m_viewMaxValue);
    glm::vec2 bottom = ValueToScreen(m_currentTime, m_viewMinValue);

    drawList->AddLine(
        ImVec2(top.x, m_curveAreaMin.y),
        ImVec2(bottom.x, m_curveAreaMax.y),
        IM_COL32(255, 80, 80, 200),
        1.5f
    );
}

void CurvePanel::RenderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Vertical lines (time)
    float timeRange = m_viewMaxTime - m_viewMinTime;
    float timeStep = std::pow(10.0f, std::floor(std::log10(timeRange / 5.0f)));

    for (float t = std::ceil(m_viewMinTime / timeStep) * timeStep; t <= m_viewMaxTime; t += timeStep) {
        glm::vec2 screen = ValueToScreen(t, 0.0f);
        drawList->AddLine(
            ImVec2(screen.x, m_curveAreaMin.y),
            ImVec2(screen.x, m_curveAreaMax.y),
            IM_COL32(50, 50, 55, 255)
        );

        if (m_config.showValues) {
            char label[16];
            snprintf(label, sizeof(label), "%.2f", t);
            drawList->AddText(ImVec2(screen.x + 2, m_curveAreaMax.y - 14), IM_COL32(150, 150, 150, 255), label);
        }
    }

    // Horizontal lines (value)
    float valueRange = m_viewMaxValue - m_viewMinValue;
    float valueStep = std::pow(10.0f, std::floor(std::log10(valueRange / 5.0f)));

    for (float v = std::ceil(m_viewMinValue / valueStep) * valueStep; v <= m_viewMaxValue; v += valueStep) {
        glm::vec2 screen = ValueToScreen(0.0f, v);
        drawList->AddLine(
            ImVec2(m_curveAreaMin.x, screen.y),
            ImVec2(m_curveAreaMax.x, screen.y),
            std::abs(v) < 0.001f ? IM_COL32(80, 80, 90, 255) : IM_COL32(50, 50, 55, 255)
        );

        if (m_config.showValues) {
            char label[16];
            snprintf(label, sizeof(label), "%.2f", v);
            drawList->AddText(ImVec2(m_curveAreaMin.x + 2, screen.y - 6), IM_COL32(150, 150, 150, 255), label);
        }
    }
}

glm::vec2 CurvePanel::ValueToScreen(float time, float value) const {
    float x = (time - m_viewMinTime) / (m_viewMaxTime - m_viewMinTime);
    float y = 1.0f - (value - m_viewMinValue) / (m_viewMaxValue - m_viewMinValue);

    return glm::vec2(
        m_curveAreaMin.x + x * (m_curveAreaMax.x - m_curveAreaMin.x),
        m_curveAreaMin.y + y * (m_curveAreaMax.y - m_curveAreaMin.y)
    );
}

glm::vec2 CurvePanel::ScreenToValue(const glm::vec2& screen) const {
    float x = (screen.x - m_curveAreaMin.x) / (m_curveAreaMax.x - m_curveAreaMin.x);
    float y = 1.0f - (screen.y - m_curveAreaMin.y) / (m_curveAreaMax.y - m_curveAreaMin.y);

    return glm::vec2(
        m_viewMinTime + x * (m_viewMaxTime - m_viewMinTime),
        m_viewMinValue + y * (m_viewMaxValue - m_viewMinValue)
    );
}

float CurvePanel::GetChannelValue(const BoneTransform& transform, CurveChannel channel) const {
    switch (channel) {
        case CurveChannel::PositionX: return transform.position.x;
        case CurveChannel::PositionY: return transform.position.y;
        case CurveChannel::PositionZ: return transform.position.z;
        case CurveChannel::RotationX: return glm::degrees(glm::eulerAngles(transform.rotation).x);
        case CurveChannel::RotationY: return glm::degrees(glm::eulerAngles(transform.rotation).y);
        case CurveChannel::RotationZ: return glm::degrees(glm::eulerAngles(transform.rotation).z);
        case CurveChannel::ScaleX: return transform.scale.x;
        case CurveChannel::ScaleY: return transform.scale.y;
        case CurveChannel::ScaleZ: return transform.scale.z;
        default: return 0.0f;
    }
}

void CurvePanel::SetChannelValue(BoneTransform& transform, CurveChannel channel, float value) {
    switch (channel) {
        case CurveChannel::PositionX: transform.position.x = value; break;
        case CurveChannel::PositionY: transform.position.y = value; break;
        case CurveChannel::PositionZ: transform.position.z = value; break;
        case CurveChannel::ScaleX: transform.scale.x = value; break;
        case CurveChannel::ScaleY: transform.scale.y = value; break;
        case CurveChannel::ScaleZ: transform.scale.z = value; break;
        default: break;
    }
}

std::string CurvePanel::GetChannelName(CurveChannel channel) const {
    switch (channel) {
        case CurveChannel::PositionX: return "Position X";
        case CurveChannel::PositionY: return "Position Y";
        case CurveChannel::PositionZ: return "Position Z";
        case CurveChannel::RotationX: return "Rotation X";
        case CurveChannel::RotationY: return "Rotation Y";
        case CurveChannel::RotationZ: return "Rotation Z";
        case CurveChannel::ScaleX: return "Scale X";
        case CurveChannel::ScaleY: return "Scale Y";
        case CurveChannel::ScaleZ: return "Scale Z";
        default: return "Unknown";
    }
}

glm::vec4 CurvePanel::GetChannelColor(CurveChannel channel) const {
    switch (channel) {
        case CurveChannel::PositionX:
        case CurveChannel::RotationX:
        case CurveChannel::ScaleX:
            return glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
        case CurveChannel::PositionY:
        case CurveChannel::RotationY:
        case CurveChannel::ScaleY:
            return glm::vec4(0.2f, 1.0f, 0.2f, 1.0f);  // Green
        case CurveChannel::PositionZ:
        case CurveChannel::RotationZ:
        case CurveChannel::ScaleZ:
            return glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);  // Blue
        default:
            return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

} // namespace Vehement
