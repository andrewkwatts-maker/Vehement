#include "LayerStackEditor.hpp"
#include <engine/animation/blending/AnimationLayer.hpp>
#include <engine/animation/blending/BlendMask.hpp>
#include <algorithm>

namespace Vehement {

LayerStackEditor::LayerStackEditor(Editor* editor)
    : m_editor(editor) {
}

LayerStackEditor::~LayerStackEditor() {
    Shutdown();
}

bool LayerStackEditor::Initialize() {
    m_initialized = true;
    return true;
}

void LayerStackEditor::Shutdown() {
    m_initialized = false;
}

void LayerStackEditor::Update(float deltaTime) {
    if (!m_initialized || !m_visible) return;
}

void LayerStackEditor::Render() {
    if (!m_initialized || !m_visible) return;
}

void LayerStackEditor::SetLayerStack(Nova::AnimationLayerStack* stack) {
    m_layerStack = stack;
    m_selectedLayerIndex = -1;
    m_isDragging = false;
}

void LayerStackEditor::AddLayer(const std::string& name) {
    if (!m_layerStack) return;

    auto layer = std::make_unique<Nova::AnimationLayer>(name);
    m_layerStack->AddLayer(std::move(layer));

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::RemoveLayer(size_t index) {
    if (!m_layerStack || index >= m_layerStack->GetLayerCount()) return;

    m_layerStack->RemoveLayer(index);

    if (m_selectedLayerIndex >= static_cast<int>(index)) {
        m_selectedLayerIndex = std::max(-1, m_selectedLayerIndex - 1);
    }

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::DuplicateLayer(size_t index) {
    if (!m_layerStack || index >= m_layerStack->GetLayerCount()) return;

    auto* source = m_layerStack->GetLayer(index);
    if (!source) return;

    auto layer = std::make_unique<Nova::AnimationLayer>(source->GetName() + " Copy");
    layer->SetWeight(source->GetWeight());
    layer->SetBlendMode(source->GetBlendMode());
    layer->SetEnabled(source->IsEnabled());

    m_layerStack->AddLayer(std::move(layer));

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::MoveLayer(size_t fromIndex, size_t toIndex) {
    if (!m_layerStack) return;

    m_layerStack->MoveLayer(fromIndex, toIndex);

    // Update selection
    if (m_selectedLayerIndex == static_cast<int>(fromIndex)) {
        m_selectedLayerIndex = static_cast<int>(toIndex);
    } else if (fromIndex < toIndex) {
        if (m_selectedLayerIndex > static_cast<int>(fromIndex) &&
            m_selectedLayerIndex <= static_cast<int>(toIndex)) {
            m_selectedLayerIndex--;
        }
    } else {
        if (m_selectedLayerIndex >= static_cast<int>(toIndex) &&
            m_selectedLayerIndex < static_cast<int>(fromIndex)) {
            m_selectedLayerIndex++;
        }
    }

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::SelectLayer(size_t index) {
    m_selectedLayerIndex = static_cast<int>(index);

    if (OnLayerSelected) {
        OnLayerSelected(index);
    }
}

void LayerStackEditor::ClearSelection() {
    m_selectedLayerIndex = -1;
}

void LayerStackEditor::SetLayerWeight(size_t index, float weight) {
    if (!m_layerStack) return;

    auto* layer = m_layerStack->GetLayer(index);
    if (layer) {
        layer->SetWeight(weight);

        if (OnLayerStackChanged) {
            OnLayerStackChanged();
        }
    }
}

void LayerStackEditor::SetLayerEnabled(size_t index, bool enabled) {
    if (!m_layerStack) return;

    auto* layer = m_layerStack->GetLayer(index);
    if (layer) {
        layer->SetEnabled(enabled);

        if (OnLayerStackChanged) {
            OnLayerStackChanged();
        }
    }
}

void LayerStackEditor::SetLayerBlendMode(size_t index, int blendMode) {
    if (!m_layerStack) return;

    auto* layer = m_layerStack->GetLayer(index);
    if (layer) {
        layer->SetBlendMode(static_cast<Nova::AnimationLayer::BlendMode>(blendMode));

        if (OnLayerStackChanged) {
            OnLayerStackChanged();
        }
    }
}

void LayerStackEditor::SetLayerMask(size_t index, const std::string& maskName) {
    if (!m_layerStack) return;

    auto* layer = m_layerStack->GetLayer(index);
    if (layer) {
        auto mask = Nova::BlendMaskLibrary::Instance().GetMask(maskName);
        layer->SetMask(mask);

        if (OnLayerStackChanged) {
            OnLayerStackChanged();
        }
    }
}

void LayerStackEditor::SoloLayer(size_t index) {
    if (!m_layerStack) return;

    if (m_layerStack->IsInSoloMode()) {
        m_layerStack->ClearSolo();
    } else {
        m_layerStack->SoloLayer(index);
    }

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::MuteLayer(size_t index, bool muted) {
    if (!m_layerStack) return;

    m_layerStack->MuteLayer(index, muted);

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

void LayerStackEditor::ClearSolo() {
    if (!m_layerStack) return;

    m_layerStack->ClearSolo();

    if (OnLayerStackChanged) {
        OnLayerStackChanged();
    }
}

std::vector<LayerStackEditor::LayerItem> LayerStackEditor::GetLayerItems() const {
    std::vector<LayerItem> items;

    if (!m_layerStack) return items;

    for (size_t i = 0; i < m_layerStack->GetLayerCount(); ++i) {
        auto* layer = m_layerStack->GetLayer(i);
        if (!layer) continue;

        LayerItem item;
        item.index = i;
        item.name = layer->GetName();
        item.weight = layer->GetWeight();
        item.enabled = layer->IsEnabled();
        item.solo = m_layerStack->IsInSoloMode() &&
                   m_layerStack->GetLayerIndex(layer->GetName()) == static_cast<int>(i);
        item.muted = false;  // Would need to track this

        switch (layer->GetBlendMode()) {
            case Nova::AnimationLayer::BlendMode::Additive:
                item.blendMode = "Additive";
                break;
            case Nova::AnimationLayer::BlendMode::Multiply:
                item.blendMode = "Multiply";
                break;
            default:
                item.blendMode = "Override";
                break;
        }

        if (layer->GetMask()) {
            item.maskName = layer->GetMask()->GetName();
        }

        item.selected = (static_cast<int>(i) == m_selectedLayerIndex);
        item.dragging = (static_cast<int>(i) == m_draggingLayerIndex);

        items.push_back(item);
    }

    return items;
}

std::vector<LayerStackEditor::BlendModeOption> LayerStackEditor::GetBlendModeOptions() const {
    return {
        {"Override", 0},
        {"Additive", 1},
        {"Multiply", 2}
    };
}

std::vector<std::string> LayerStackEditor::GetAvailableMasks() const {
    std::vector<std::string> masks;
    masks.push_back("");  // No mask option

    auto names = Nova::BlendMaskLibrary::Instance().GetMaskNames();
    masks.insert(masks.end(), names.begin(), names.end());

    return masks;
}

bool LayerStackEditor::OnMouseDown(const glm::vec2& pos, int button) {
    if (!m_visible || !m_layerStack) return false;

    if (button == 0) {
        int layerIndex = FindLayerAtPosition(pos);

        if (layerIndex >= 0) {
            SelectLayer(static_cast<size_t>(layerIndex));
            BeginDragLayer(static_cast<size_t>(layerIndex));
            return true;
        }
    }

    return false;
}

void LayerStackEditor::OnMouseMove(const glm::vec2& pos) {
    if (m_isDragging) {
        UpdateDrag(pos);
    }
}

void LayerStackEditor::OnMouseUp(const glm::vec2& pos, int button) {
    if (button == 0 && m_isDragging) {
        EndDrag();
    }
}

bool LayerStackEditor::OnKeyDown(int key) {
    if (m_selectedLayerIndex >= 0) {
        // Delete key
        if (key == 127 || key == 46) {
            RemoveLayer(static_cast<size_t>(m_selectedLayerIndex));
            return true;
        }

        // Ctrl+D - duplicate
        if (key == 'd' || key == 'D') {
            DuplicateLayer(static_cast<size_t>(m_selectedLayerIndex));
            return true;
        }
    }

    return false;
}

void LayerStackEditor::BeginDragLayer(size_t index) {
    if (!m_layerStack || index >= m_layerStack->GetLayerCount()) return;

    m_isDragging = true;
    m_draggingLayerIndex = static_cast<int>(index);
    m_dropTargetIndex = -1;
}

void LayerStackEditor::UpdateDrag(const glm::vec2& pos) {
    if (!m_isDragging) return;

    m_dropTargetIndex = FindDropPosition(pos);
}

void LayerStackEditor::EndDrag() {
    if (!m_isDragging) return;

    if (m_dropTargetIndex >= 0 && m_dropTargetIndex != m_draggingLayerIndex) {
        MoveLayer(static_cast<size_t>(m_draggingLayerIndex),
                  static_cast<size_t>(m_dropTargetIndex));
    }

    m_isDragging = false;
    m_draggingLayerIndex = -1;
    m_dropTargetIndex = -1;
}

void LayerStackEditor::SetPanelBounds(const glm::vec2& pos, const glm::vec2& size) {
    m_panelPos = pos;
    m_panelSize = size;
}

int LayerStackEditor::FindLayerAtPosition(const glm::vec2& pos) const {
    if (!m_layerStack) return -1;

    for (size_t i = 0; i < m_layerStack->GetLayerCount(); ++i) {
        glm::vec2 layerPos = GetLayerPosition(i);

        if (pos.x >= layerPos.x && pos.x <= layerPos.x + m_panelSize.x &&
            pos.y >= layerPos.y && pos.y <= layerPos.y + m_layerHeight) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int LayerStackEditor::FindDropPosition(const glm::vec2& pos) const {
    if (!m_layerStack) return -1;

    for (size_t i = 0; i <= m_layerStack->GetLayerCount(); ++i) {
        float y = m_panelPos.y + static_cast<float>(i) * m_layerHeight;

        if (pos.y < y + m_layerHeight * 0.5f) {
            return static_cast<int>(i);
        }
    }

    return static_cast<int>(m_layerStack->GetLayerCount());
}

glm::vec2 LayerStackEditor::GetLayerPosition(size_t index) const {
    return glm::vec2(
        m_panelPos.x,
        m_panelPos.y + static_cast<float>(index) * m_layerHeight
    );
}

} // namespace Vehement
