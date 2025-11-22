#include "BlendSpace1DEditor.hpp"
#include <engine/animation/blending/BlendSpace1D.hpp>
#include <engine/animation/Skeleton.hpp>
#include <algorithm>

namespace Vehement {

BlendSpace1DEditor::BlendSpace1DEditor(Editor* editor)
    : m_editor(editor) {
}

BlendSpace1DEditor::~BlendSpace1DEditor() {
    Shutdown();
}

bool BlendSpace1DEditor::Initialize() {
    m_initialized = true;
    return true;
}

void BlendSpace1DEditor::Shutdown() {
    m_initialized = false;
}

void BlendSpace1DEditor::Update(float deltaTime) {
    if (!m_initialized || !m_visible || !m_blendSpace) return;

    if (m_previewEnabled && m_previewPlaying) {
        m_previewTime += deltaTime;
        if (m_previewTime > 1.0f) {
            m_previewTime = 0.0f;
        }

        // Evaluate blend space
        auto result = m_blendSpace->Evaluate(m_previewValue, deltaTime);
        if (!m_previewPose) {
            m_previewPose = std::make_unique<Nova::AnimationPose>();
        }
        *m_previewPose = std::move(result.pose);
    }

    UpdateSampleWeights();
}

void BlendSpace1DEditor::Render() {
    if (!m_initialized || !m_visible) return;

    // Rendering would be done through ImGui or web view
    // This is called by the editor's rendering loop
}

void BlendSpace1DEditor::SetBlendSpace(Nova::BlendSpace1D* blendSpace) {
    m_blendSpace = blendSpace;
    m_selectedSampleIndex = -1;
    m_draggingSampleIndex = -1;

    if (blendSpace) {
        m_skeleton = blendSpace->GetSkeleton();
        m_previewValue = (blendSpace->GetMinParameter() + blendSpace->GetMaxParameter()) * 0.5f;
    }
}

void BlendSpace1DEditor::SetSkeleton(Nova::Skeleton* skeleton) {
    m_skeleton = skeleton;
    if (m_blendSpace) {
        m_blendSpace->SetSkeleton(skeleton);
    }
}

void BlendSpace1DEditor::AddSample(const Nova::Animation* clip, float position) {
    if (!m_blendSpace) return;

    m_blendSpace->AddSample(clip, position);

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace1DEditor::RemoveSelectedSample() {
    if (!m_blendSpace || m_selectedSampleIndex < 0) return;

    m_blendSpace->RemoveSample(static_cast<size_t>(m_selectedSampleIndex));
    m_selectedSampleIndex = -1;

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace1DEditor::MoveSample(size_t index, float newPosition) {
    if (!m_blendSpace || index >= m_blendSpace->GetSampleCount()) return;

    m_blendSpace->SetSamplePosition(index, newPosition);

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace1DEditor::SelectSample(size_t index) {
    if (!m_blendSpace || index >= m_blendSpace->GetSampleCount()) return;

    m_selectedSampleIndex = static_cast<int>(index);

    if (OnSampleSelected) {
        OnSampleSelected(index);
    }
}

void BlendSpace1DEditor::ClearSelection() {
    m_selectedSampleIndex = -1;
}

void BlendSpace1DEditor::SetPreviewValue(float value) {
    m_previewValue = std::clamp(value, GetMinValue(), GetMaxValue());

    if (OnPreviewValueChanged) {
        OnPreviewValueChanged(m_previewValue);
    }
}

void BlendSpace1DEditor::SetPreviewTime(float normalizedTime) {
    m_previewTime = std::clamp(normalizedTime, 0.0f, 1.0f);
}

void BlendSpace1DEditor::PlayPreview() {
    m_previewPlaying = true;
}

void BlendSpace1DEditor::PausePreview() {
    m_previewPlaying = false;
}

void BlendSpace1DEditor::ResetPreview() {
    m_previewTime = 0.0f;
    if (m_blendSpace) {
        m_blendSpace->Reset();
    }
}

const Nova::AnimationPose* BlendSpace1DEditor::GetPreviewPose() const {
    return m_previewPose.get();
}

std::vector<BlendSpace1DEditor::SampleMarker> BlendSpace1DEditor::GetSampleMarkers() const {
    std::vector<SampleMarker> markers;

    if (!m_blendSpace) return markers;

    auto weights = m_blendSpace->GetSampleWeights(m_previewValue);

    for (size_t i = 0; i < m_blendSpace->GetSampleCount(); ++i) {
        const auto& sample = m_blendSpace->GetSample(i);

        SampleMarker marker;
        marker.index = i;
        marker.clipName = sample.clipId;
        marker.position = sample.position;
        marker.weight = i < weights.size() ? weights[i] : 0.0f;
        marker.selected = (static_cast<int>(i) == m_selectedSampleIndex);
        marker.dragging = (static_cast<int>(i) == m_draggingSampleIndex);
        marker.screenPos.x = ValueToScreenX(sample.position);
        marker.screenPos.y = m_trackPos.y + m_trackSize.y * 0.5f;

        markers.push_back(marker);
    }

    return markers;
}

std::vector<BlendSpace1DEditor::SyncMarkerDisplay> BlendSpace1DEditor::GetSyncMarkers() const {
    std::vector<SyncMarkerDisplay> markers;

    if (!m_blendSpace) return markers;

    for (const auto& sync : m_blendSpace->GetSyncMarkers()) {
        SyncMarkerDisplay display;
        display.name = sync.name;
        display.normalizedTime = sync.normalizedTime;
        display.color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
        markers.push_back(display);
    }

    return markers;
}

float BlendSpace1DEditor::GetMinValue() const {
    return m_blendSpace ? m_blendSpace->GetMinParameter() : 0.0f;
}

float BlendSpace1DEditor::GetMaxValue() const {
    return m_blendSpace ? m_blendSpace->GetMaxParameter() : 1.0f;
}

bool BlendSpace1DEditor::OnMouseDown(const glm::vec2& pos, int button) {
    if (!m_visible || !m_blendSpace) return false;

    if (button == 0) {  // Left click
        int sampleIndex = FindSampleAtPosition(pos);

        if (sampleIndex >= 0) {
            SelectSample(static_cast<size_t>(sampleIndex));
            m_draggingSampleIndex = sampleIndex;
            return true;
        } else {
            // Check if on track - move preview cursor
            if (pos.x >= m_trackPos.x && pos.x <= m_trackPos.x + m_trackSize.x &&
                pos.y >= m_trackPos.y && pos.y <= m_trackPos.y + m_trackSize.y) {
                SetPreviewValue(ScreenXToValue(pos.x));
                return true;
            }
        }
    }

    return false;
}

void BlendSpace1DEditor::OnMouseMove(const glm::vec2& pos) {
    if (m_draggingSampleIndex >= 0 && m_blendSpace) {
        float newValue = ScreenXToValue(pos.x);
        newValue = std::clamp(newValue, GetMinValue(), GetMaxValue());
        MoveSample(static_cast<size_t>(m_draggingSampleIndex), newValue);
    }
}

void BlendSpace1DEditor::OnMouseUp(const glm::vec2& pos, int button) {
    if (button == 0) {
        m_draggingSampleIndex = -1;
    }
}

bool BlendSpace1DEditor::OnKeyDown(int key) {
    // Handle Delete key to remove selected sample
    if (key == 127 || key == 46) {  // Delete key
        if (m_selectedSampleIndex >= 0) {
            RemoveSelectedSample();
            return true;
        }
    }

    return false;
}

void BlendSpace1DEditor::SetTrackBounds(const glm::vec2& pos, const glm::vec2& size) {
    m_trackPos = pos;
    m_trackSize = size;
}

float BlendSpace1DEditor::ValueToScreenX(float value) const {
    float min = GetMinValue();
    float max = GetMaxValue();
    float t = (max > min) ? (value - min) / (max - min) : 0.5f;
    return m_trackPos.x + t * m_trackSize.x;
}

float BlendSpace1DEditor::ScreenXToValue(float screenX) const {
    float t = (screenX - m_trackPos.x) / m_trackSize.x;
    t = std::clamp(t, 0.0f, 1.0f);
    return GetMinValue() + t * (GetMaxValue() - GetMinValue());
}

int BlendSpace1DEditor::FindSampleAtPosition(const glm::vec2& pos) const {
    if (!m_blendSpace) return -1;

    for (size_t i = 0; i < m_blendSpace->GetSampleCount(); ++i) {
        float sampleX = ValueToScreenX(m_blendSpace->GetSample(i).position);
        float sampleY = m_trackPos.y + m_trackSize.y * 0.5f;

        float dx = pos.x - sampleX;
        float dy = pos.y - sampleY;

        if (dx * dx + dy * dy <= m_markerRadius * m_markerRadius) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void BlendSpace1DEditor::UpdateSampleWeights() {
    // Weights are computed on demand in GetSampleMarkers()
}

} // namespace Vehement
