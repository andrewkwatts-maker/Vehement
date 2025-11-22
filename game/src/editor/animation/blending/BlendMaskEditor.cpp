#include "BlendMaskEditor.hpp"
#include <engine/animation/blending/BlendMask.hpp>
#include <engine/animation/Skeleton.hpp>
#include <algorithm>
#include <cmath>

namespace Vehement {

BlendMaskEditor::BlendMaskEditor(Editor* editor)
    : m_editor(editor) {
}

BlendMaskEditor::~BlendMaskEditor() {
    Shutdown();
}

bool BlendMaskEditor::Initialize() {
    m_initialized = true;
    return true;
}

void BlendMaskEditor::Shutdown() {
    m_initialized = false;
}

void BlendMaskEditor::Update(float deltaTime) {
    if (!m_initialized || !m_visible) return;
    UpdateBonePositions();
}

void BlendMaskEditor::Render() {
    if (!m_initialized || !m_visible) return;
}

void BlendMaskEditor::SetMask(Nova::BlendMask* mask) {
    m_mask = mask;
    m_selectedBoneIndex = -1;

    if (mask && mask->GetSkeleton()) {
        m_skeleton = mask->GetSkeleton();
    }
}

void BlendMaskEditor::SetSkeleton(Nova::Skeleton* skeleton) {
    m_skeleton = skeleton;
    if (m_mask) {
        m_mask->SetSkeleton(skeleton);
    }
    UpdateBonePositions();
}

void BlendMaskEditor::SetBoneWeight(const std::string& boneName, float weight) {
    if (!m_mask) return;

    m_mask->SetBoneWeight(boneName, weight, false);

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::SetBoneWeight(int boneIndex, float weight) {
    if (!m_mask || !m_skeleton || boneIndex < 0) return;

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(boneIndex));
    if (bone) {
        SetBoneWeight(bone->name, weight);
    }
}

void BlendMaskEditor::SetBranchWeight(const std::string& boneName, float weight) {
    if (!m_mask) return;

    m_mask->SetBranchWeight(boneName, weight);

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::ToggleBone(const std::string& boneName) {
    if (!m_mask) return;

    float currentWeight = m_mask->GetBoneWeight(boneName);
    float newWeight = currentWeight > 0.5f ? 0.0f : 1.0f;
    SetBoneWeight(boneName, newWeight);
}

void BlendMaskEditor::ToggleBone(int boneIndex) {
    if (!m_skeleton || boneIndex < 0) return;

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(boneIndex));
    if (bone) {
        ToggleBone(bone->name);
    }
}

void BlendMaskEditor::SelectBone(int boneIndex) {
    m_selectedBoneIndex = boneIndex;

    if (OnBoneSelected) {
        OnBoneSelected(boneIndex);
    }
}

void BlendMaskEditor::ClearSelection() {
    m_selectedBoneIndex = -1;
}

void BlendMaskEditor::ApplyPreset(Nova::BlendMask::Preset preset) {
    if (!m_mask) return;

    m_mask->ApplyPreset(preset);

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

std::vector<BlendMaskEditor::PresetInfo> BlendMaskEditor::GetPresets() const {
    std::vector<PresetInfo> presets;

    auto available = Nova::BlendMask::GetAvailablePresets();
    for (auto preset : available) {
        PresetInfo info;
        info.name = Nova::BlendMask::GetPresetName(preset);
        info.type = preset;
        presets.push_back(info);
    }

    return presets;
}

void BlendMaskEditor::ClearAllWeights() {
    if (!m_mask) return;

    m_mask->ClearWeights();

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::SetAllWeights() {
    if (!m_mask) return;

    m_mask->SetAllWeights(1.0f);

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::InvertMask() {
    if (!m_mask) return;

    m_mask->Invert();

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::MirrorMask() {
    if (!m_mask) return;

    m_mask->Mirror();

    if (OnMaskChanged) {
        OnMaskChanged();
    }
}

void BlendMaskEditor::AddFeathering(int levels, float startWeight, float endWeight) {
    if (!m_mask || !m_skeleton || m_selectedBoneIndex < 0) return;

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(m_selectedBoneIndex));
    if (bone) {
        m_mask->AddFeathering(bone->name, levels, startWeight, endWeight);

        if (OnMaskChanged) {
            OnMaskChanged();
        }
    }
}

std::vector<BlendMaskEditor::BoneDisplay> BlendMaskEditor::GetBoneDisplays() const {
    std::vector<BoneDisplay> displays;

    if (!m_skeleton) return displays;

    const auto& bones = m_skeleton->GetBones();

    for (size_t i = 0; i < bones.size(); ++i) {
        BoneDisplay display;
        display.name = bones[i].name;
        display.index = static_cast<int>(i);
        display.parentIndex = bones[i].parentIndex;
        display.weight = m_mask ? m_mask->GetBoneWeight(i) : 0.0f;
        display.selected = (static_cast<int>(i) == m_selectedBoneIndex);
        display.hovered = (static_cast<int>(i) == m_hoveredBoneIndex);

        if (i < m_boneScreenPositions.size()) {
            display.screenPos = m_boneScreenPositions[i];
        }

        if (display.parentIndex >= 0 &&
            static_cast<size_t>(display.parentIndex) < m_boneScreenPositions.size()) {
            display.parentScreenPos = m_boneScreenPositions[display.parentIndex];
        }

        displays.push_back(display);
    }

    return displays;
}

void BlendMaskEditor::SetPoseTime(float normalizedTime) {
    // Would update bone positions based on animation pose
    UpdateBonePositions();
}

void BlendMaskEditor::SetViewRotation(float yaw, float pitch) {
    m_viewYaw = yaw;
    m_viewPitch = pitch;
    UpdateBonePositions();
}

void BlendMaskEditor::SetZoom(float zoom) {
    m_zoom = std::clamp(zoom, 0.1f, 5.0f);
    UpdateBonePositions();
}

bool BlendMaskEditor::OnMouseDown(const glm::vec2& pos, int button) {
    if (!m_visible || !m_skeleton) return false;

    if (button == 0) {
        int boneIndex = FindBoneAtPosition(pos);

        if (boneIndex >= 0) {
            SelectBone(boneIndex);
            ToggleBone(boneIndex);
            return true;
        }
    }

    return false;
}

void BlendMaskEditor::OnMouseMove(const glm::vec2& pos) {
    m_hoveredBoneIndex = FindBoneAtPosition(pos);
}

void BlendMaskEditor::OnMouseUp(const glm::vec2& pos, int button) {
    // Nothing to do
}

bool BlendMaskEditor::OnKeyDown(int key) {
    if (m_selectedBoneIndex >= 0) {
        // 'A' - select all children
        if (key == 'A' || key == 'a') {
            const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(m_selectedBoneIndex));
            if (bone) {
                SetBranchWeight(bone->name, 1.0f);
            }
            return true;
        }

        // 'C' - clear selection
        if (key == 'C' || key == 'c') {
            const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(m_selectedBoneIndex));
            if (bone) {
                SetBranchWeight(bone->name, 0.0f);
            }
            return true;
        }
    }

    return false;
}

void BlendMaskEditor::SetCanvasBounds(const glm::vec2& pos, const glm::vec2& size) {
    m_canvasPos = pos;
    m_canvasSize = size;
    UpdateBonePositions();
}

void BlendMaskEditor::SaveAsPreset(const std::string& name) {
    if (!m_mask) return;

    auto maskCopy = std::make_shared<Nova::BlendMask>(*m_mask);
    maskCopy->SetName(name);
    Nova::BlendMaskLibrary::Instance().RegisterMask(name, maskCopy);
}

void BlendMaskEditor::LoadPreset(const std::string& name) {
    auto mask = Nova::BlendMaskLibrary::Instance().GetMask(name);
    if (mask && m_mask) {
        // Copy weights from preset
        if (m_skeleton) {
            const auto& weights = mask->GetWeights();
            for (size_t i = 0; i < weights.size() && i < m_skeleton->GetBoneCount(); ++i) {
                m_mask->SetBoneWeight(i, weights[i]);
            }
        }

        if (OnMaskChanged) {
            OnMaskChanged();
        }
    }
}

std::vector<std::string> BlendMaskEditor::GetCustomPresetNames() const {
    return Nova::BlendMaskLibrary::Instance().GetMaskNames();
}

glm::vec2 BlendMaskEditor::BoneToScreen(int boneIndex) const {
    if (!m_skeleton || boneIndex < 0) {
        return m_canvasPos + m_canvasSize * 0.5f;
    }

    const auto* bone = m_skeleton->GetBoneByIndex(static_cast<size_t>(boneIndex));
    if (!bone) {
        return m_canvasPos + m_canvasSize * 0.5f;
    }

    // Simple 2D projection of bone position
    // In a real implementation, this would use the bone's world transform

    // Use bone index to create a simple hierarchy visualization
    float xOffset = 0.0f;
    float yOffset = static_cast<float>(boneIndex) * 20.0f * m_zoom;

    // Apply view rotation
    float cosYaw = std::cos(m_viewYaw);
    float sinYaw = std::sin(m_viewYaw);

    glm::vec2 center = m_canvasPos + m_canvasSize * 0.5f;

    return glm::vec2(
        center.x + xOffset * cosYaw * m_zoom,
        center.y - m_canvasSize.y * 0.4f + yOffset
    );
}

int BlendMaskEditor::FindBoneAtPosition(const glm::vec2& pos) const {
    for (size_t i = 0; i < m_boneScreenPositions.size(); ++i) {
        glm::vec2 bonePos = m_boneScreenPositions[i];
        float dx = pos.x - bonePos.x;
        float dy = pos.y - bonePos.y;

        if (dx * dx + dy * dy <= m_boneRadius * m_boneRadius) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void BlendMaskEditor::UpdateBonePositions() {
    if (!m_skeleton) {
        m_boneScreenPositions.clear();
        return;
    }

    m_boneScreenPositions.resize(m_skeleton->GetBoneCount());

    for (size_t i = 0; i < m_skeleton->GetBoneCount(); ++i) {
        m_boneScreenPositions[i] = BoneToScreen(static_cast<int>(i));
    }
}

} // namespace Vehement
