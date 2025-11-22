#include "BlendSpace2DEditor.hpp"
#include <engine/animation/blending/BlendSpace2D.hpp>
#include <engine/animation/Skeleton.hpp>
#include <algorithm>

namespace Vehement {

BlendSpace2DEditor::BlendSpace2DEditor(Editor* editor)
    : m_editor(editor) {
}

BlendSpace2DEditor::~BlendSpace2DEditor() {
    Shutdown();
}

bool BlendSpace2DEditor::Initialize() {
    m_initialized = true;
    return true;
}

void BlendSpace2DEditor::Shutdown() {
    m_initialized = false;
}

void BlendSpace2DEditor::Update(float deltaTime) {
    if (!m_initialized || !m_visible || !m_blendSpace) return;

    if (m_previewEnabled && m_previewPlaying) {
        m_previewTime += deltaTime;
        if (m_previewTime > 1.0f) {
            m_previewTime = 0.0f;
        }

        auto result = m_blendSpace->Evaluate(m_previewPosition.x, m_previewPosition.y, deltaTime);
        if (!m_previewPose) {
            m_previewPose = std::make_unique<Nova::AnimationPose>();
        }
        *m_previewPose = std::move(result.pose);
    }
}

void BlendSpace2DEditor::Render() {
    if (!m_initialized || !m_visible) return;
}

void BlendSpace2DEditor::SetBlendSpace(Nova::BlendSpace2D* blendSpace) {
    m_blendSpace = blendSpace;
    m_selectedSampleIndex = -1;
    m_draggingSampleIndex = -1;

    if (blendSpace) {
        m_skeleton = blendSpace->GetSkeleton();
        auto min = blendSpace->GetMinBounds();
        auto max = blendSpace->GetMaxBounds();
        m_previewPosition = (min + max) * 0.5f;
    }
}

void BlendSpace2DEditor::SetSkeleton(Nova::Skeleton* skeleton) {
    m_skeleton = skeleton;
    if (m_blendSpace) {
        m_blendSpace->SetSkeleton(skeleton);
    }
}

void BlendSpace2DEditor::AddSample(const Nova::Animation* clip, const glm::vec2& position) {
    if (!m_blendSpace) return;

    m_blendSpace->AddSample(clip, position);

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace2DEditor::RemoveSelectedSample() {
    if (!m_blendSpace || m_selectedSampleIndex < 0) return;

    m_blendSpace->RemoveSample(static_cast<size_t>(m_selectedSampleIndex));
    m_selectedSampleIndex = -1;

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace2DEditor::MoveSample(size_t index, const glm::vec2& newPosition) {
    if (!m_blendSpace || index >= m_blendSpace->GetSampleCount()) return;

    m_blendSpace->SetSamplePosition(index, newPosition);

    if (OnBlendSpaceChanged) {
        OnBlendSpaceChanged();
    }
}

void BlendSpace2DEditor::SelectSample(size_t index) {
    if (!m_blendSpace || index >= m_blendSpace->GetSampleCount()) return;

    m_selectedSampleIndex = static_cast<int>(index);

    if (OnSampleSelected) {
        OnSampleSelected(index);
    }
}

void BlendSpace2DEditor::ClearSelection() {
    m_selectedSampleIndex = -1;
}

void BlendSpace2DEditor::SetPreviewPosition(const glm::vec2& position) {
    auto min = GetMinBounds();
    auto max = GetMaxBounds();
    m_previewPosition = glm::clamp(position, min, max);

    if (OnPreviewPositionChanged) {
        OnPreviewPositionChanged(m_previewPosition);
    }
}

void BlendSpace2DEditor::SetPreviewTime(float normalizedTime) {
    m_previewTime = std::clamp(normalizedTime, 0.0f, 1.0f);
}

void BlendSpace2DEditor::PlayPreview() {
    m_previewPlaying = true;
}

void BlendSpace2DEditor::PausePreview() {
    m_previewPlaying = false;
}

void BlendSpace2DEditor::ResetPreview() {
    m_previewTime = 0.0f;
    if (m_blendSpace) {
        m_blendSpace->Reset();
    }
}

const Nova::AnimationPose* BlendSpace2DEditor::GetPreviewPose() const {
    return m_previewPose.get();
}

std::vector<BlendSpace2DEditor::SamplePoint> BlendSpace2DEditor::GetSamplePoints() const {
    std::vector<SamplePoint> points;

    if (!m_blendSpace) return points;

    auto weights = m_blendSpace->GetSampleWeights(m_previewPosition);

    for (size_t i = 0; i < m_blendSpace->GetSampleCount(); ++i) {
        const auto& sample = m_blendSpace->GetSample(i);

        SamplePoint point;
        point.index = i;
        point.clipName = sample.clipId;
        point.position = sample.position;
        point.weight = i < weights.size() ? weights[i] : 0.0f;
        point.selected = (static_cast<int>(i) == m_selectedSampleIndex);
        point.dragging = (static_cast<int>(i) == m_draggingSampleIndex);
        point.screenPos = ValueToScreen(sample.position);

        points.push_back(point);
    }

    return points;
}

std::vector<BlendSpace2DEditor::TriangleDisplay> BlendSpace2DEditor::GetTriangles() const {
    std::vector<TriangleDisplay> triangles;

    if (!m_blendSpace || !m_showTriangulation) return triangles;

    const auto& tris = m_blendSpace->GetTriangles();
    const auto& samples = m_blendSpace->GetSamples();
    int containingTri = m_blendSpace->FindContainingTriangle(m_previewPosition);

    for (size_t i = 0; i < tris.size(); ++i) {
        const auto& tri = tris[i];
        if (tri.indices[0] >= samples.size() ||
            tri.indices[1] >= samples.size() ||
            tri.indices[2] >= samples.size()) {
            continue;
        }

        TriangleDisplay display;
        display.v0 = ValueToScreen(samples[tri.indices[0]].position);
        display.v1 = ValueToScreen(samples[tri.indices[1]].position);
        display.v2 = ValueToScreen(samples[tri.indices[2]].position);
        display.containsCursor = (static_cast<int>(i) == containingTri);

        triangles.push_back(display);
    }

    return triangles;
}

glm::vec2 BlendSpace2DEditor::GetMinBounds() const {
    return m_blendSpace ? m_blendSpace->GetMinBounds() : glm::vec2(-1.0f);
}

glm::vec2 BlendSpace2DEditor::GetMaxBounds() const {
    return m_blendSpace ? m_blendSpace->GetMaxBounds() : glm::vec2(1.0f);
}

bool BlendSpace2DEditor::OnMouseDown(const glm::vec2& pos, int button) {
    if (!m_visible || !m_blendSpace) return false;

    if (button == 0) {
        int sampleIndex = FindSampleAtPosition(pos);

        if (sampleIndex >= 0) {
            SelectSample(static_cast<size_t>(sampleIndex));
            m_draggingSampleIndex = sampleIndex;
            return true;
        } else {
            // Check if on canvas - move preview cursor
            if (pos.x >= m_canvasPos.x && pos.x <= m_canvasPos.x + m_canvasSize.x &&
                pos.y >= m_canvasPos.y && pos.y <= m_canvasPos.y + m_canvasSize.y) {
                SetPreviewPosition(ScreenToValue(pos));
                return true;
            }
        }
    }

    return false;
}

void BlendSpace2DEditor::OnMouseMove(const glm::vec2& pos) {
    if (m_draggingSampleIndex >= 0 && m_blendSpace) {
        glm::vec2 newValue = ScreenToValue(pos);
        newValue = glm::clamp(newValue, GetMinBounds(), GetMaxBounds());
        MoveSample(static_cast<size_t>(m_draggingSampleIndex), newValue);
    }
}

void BlendSpace2DEditor::OnMouseUp(const glm::vec2& pos, int button) {
    if (button == 0) {
        m_draggingSampleIndex = -1;
    }
}

bool BlendSpace2DEditor::OnKeyDown(int key) {
    if (key == 127 || key == 46) {  // Delete key
        if (m_selectedSampleIndex >= 0) {
            RemoveSelectedSample();
            return true;
        }
    }

    return false;
}

void BlendSpace2DEditor::SetCanvasBounds(const glm::vec2& pos, const glm::vec2& size) {
    m_canvasPos = pos;
    m_canvasSize = size;
}

glm::vec2 BlendSpace2DEditor::ValueToScreen(const glm::vec2& value) const {
    glm::vec2 min = GetMinBounds();
    glm::vec2 max = GetMaxBounds();
    glm::vec2 range = max - min;

    glm::vec2 t;
    t.x = range.x > 0 ? (value.x - min.x) / range.x : 0.5f;
    t.y = range.y > 0 ? (value.y - min.y) / range.y : 0.5f;

    return glm::vec2(
        m_canvasPos.x + t.x * m_canvasSize.x,
        m_canvasPos.y + (1.0f - t.y) * m_canvasSize.y  // Flip Y
    );
}

glm::vec2 BlendSpace2DEditor::ScreenToValue(const glm::vec2& screen) const {
    glm::vec2 t;
    t.x = (screen.x - m_canvasPos.x) / m_canvasSize.x;
    t.y = 1.0f - (screen.y - m_canvasPos.y) / m_canvasSize.y;  // Flip Y

    t = glm::clamp(t, glm::vec2(0.0f), glm::vec2(1.0f));

    glm::vec2 min = GetMinBounds();
    glm::vec2 max = GetMaxBounds();

    return min + t * (max - min);
}

int BlendSpace2DEditor::FindSampleAtPosition(const glm::vec2& pos) const {
    if (!m_blendSpace) return -1;

    for (size_t i = 0; i < m_blendSpace->GetSampleCount(); ++i) {
        glm::vec2 screenPos = ValueToScreen(m_blendSpace->GetSample(i).position);

        float dx = pos.x - screenPos.x;
        float dy = pos.y - screenPos.y;

        if (dx * dx + dy * dy <= m_pointRadius * m_pointRadius) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

} // namespace Vehement
