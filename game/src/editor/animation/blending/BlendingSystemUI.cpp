#include "BlendingSystemUI.hpp"
#include "../../../editor/Editor.hpp"
#include "../../../editor/web/WebViewManager.hpp"
#include "../../../editor/web/JSBridge.hpp"
#include <engine/animation/blending/BlendTreeRuntime.hpp>
#include <engine/animation/blending/AnimationLayer.hpp>
#include <engine/animation/blending/BlendMask.hpp>
#include <engine/animation/Skeleton.hpp>
#include <fstream>
#include <sstream>

namespace Vehement {

BlendingSystemUI::BlendingSystemUI(Editor* editor)
    : m_editor(editor) {
}

BlendingSystemUI::~BlendingSystemUI() {
    Shutdown();
}

bool BlendingSystemUI::Initialize(const Config& config) {
    m_config = config;

    // Create web view for HTML-based UI
    // m_webView = std::make_unique<WebEditor::WebView>();
    // m_bridge = std::make_unique<WebEditor::JSBridge>();

    SetupJSBridge();

    m_initialized = true;
    return true;
}

void BlendingSystemUI::Shutdown() {
    m_webView.reset();
    m_bridge.reset();
    m_initialized = false;
}

void BlendingSystemUI::Update(float deltaTime) {
    if (!m_initialized || !m_visible) return;

    if (m_previewEnabled && m_previewPlaying) {
        UpdatePreview(deltaTime * m_previewSpeed);
    }
}

void BlendingSystemUI::Render() {
    if (!m_initialized || !m_visible) return;

    // Render through ImGui or web view
    // This would integrate with the editor's rendering system
}

void BlendingSystemUI::SetRuntime(Nova::BlendTreeRuntime* runtime) {
    m_runtime = runtime;
    if (runtime) {
        m_skeleton = runtime->GetSkeleton();
        m_layerStack = runtime->GetLayerStack();
    }
    SyncUIState();
}

void BlendingSystemUI::SetLayerStack(Nova::AnimationLayerStack* stack) {
    m_layerStack = stack;
    SyncUIState();
}

void BlendingSystemUI::SetSkeleton(Nova::Skeleton* skeleton) {
    m_skeleton = skeleton;
    if (m_previewPose && skeleton) {
        m_previewPose->Resize(skeleton->GetBoneCount());
    }
}

void BlendingSystemUI::SetPreviewEnabled(bool enabled) {
    m_previewEnabled = enabled;
    if (!enabled) {
        m_previewPlaying = false;
    }
}

void BlendingSystemUI::PlayPreview() {
    m_previewPlaying = true;
}

void BlendingSystemUI::PausePreview() {
    m_previewPlaying = false;
}

void BlendingSystemUI::ResetPreview() {
    m_previewTime = 0.0f;
    if (m_runtime) {
        m_runtime->Reset();
    }
}

const Nova::AnimationPose* BlendingSystemUI::GetPreviewPose() const {
    return m_previewPose.get();
}

std::vector<BlendingSystemUI::ParameterInfo> BlendingSystemUI::GetParameters() const {
    std::vector<ParameterInfo> params;

    if (m_runtime) {
        for (const auto& [name, param] : m_runtime->GetParameters()) {
            ParameterInfo info;
            info.name = name;
            info.value = param.value;
            info.minValue = param.minValue;
            info.maxValue = param.maxValue;
            info.isSmooth = param.smooth;
            info.smoothSpeed = param.smoothSpeed;
            params.push_back(info);
        }
    }

    return params;
}

void BlendingSystemUI::SetParameter(const std::string& name, float value) {
    if (m_runtime) {
        m_runtime->SetParameter(name, value);

        if (OnParameterChanged) {
            OnParameterChanged(name, value);
        }
    }
}

void BlendingSystemUI::AddParameter(const std::string& name, float defaultValue) {
    if (m_runtime) {
        m_runtime->RegisterParameter(name, defaultValue);
        SyncUIState();
    }
}

void BlendingSystemUI::RemoveParameter(const std::string& name) {
    // Parameters cannot be removed from runtime directly
    // Would need to rebuild the runtime
}

std::vector<BlendingSystemUI::LayerInfo> BlendingSystemUI::GetLayers() const {
    std::vector<LayerInfo> layers;

    if (m_layerStack) {
        for (size_t i = 0; i < m_layerStack->GetLayerCount(); ++i) {
            auto* layer = m_layerStack->GetLayer(i);
            if (layer) {
                LayerInfo info;
                info.name = layer->GetName();
                info.weight = layer->GetWeight();
                info.enabled = layer->IsEnabled();
                info.muted = false;  // Would need to track this
                info.solo = m_layerStack->IsInSoloMode() &&
                           m_layerStack->GetLayerIndex(layer->GetName()) == static_cast<int>(i);

                switch (layer->GetBlendMode()) {
                    case Nova::AnimationLayer::BlendMode::Additive:
                        info.blendMode = "Additive";
                        break;
                    case Nova::AnimationLayer::BlendMode::Multiply:
                        info.blendMode = "Multiply";
                        break;
                    default:
                        info.blendMode = "Override";
                        break;
                }

                if (layer->GetMask()) {
                    info.maskName = layer->GetMask()->GetName();
                }

                layers.push_back(info);
            }
        }
    }

    return layers;
}

void BlendingSystemUI::SetLayerWeight(size_t index, float weight) {
    if (m_layerStack) {
        auto* layer = m_layerStack->GetLayer(index);
        if (layer) {
            layer->SetWeight(weight);
            NotifyBlendTreeChanged();
        }
    }
}

void BlendingSystemUI::SetLayerEnabled(size_t index, bool enabled) {
    if (m_layerStack) {
        auto* layer = m_layerStack->GetLayer(index);
        if (layer) {
            layer->SetEnabled(enabled);
            NotifyBlendTreeChanged();
        }
    }
}

void BlendingSystemUI::SetLayerMuted(size_t index, bool muted) {
    if (m_layerStack) {
        m_layerStack->MuteLayer(index, muted);
        NotifyBlendTreeChanged();
    }
}

void BlendingSystemUI::SoloLayer(size_t index) {
    if (m_layerStack) {
        m_layerStack->SoloLayer(index);
        SyncUIState();
    }
}

void BlendingSystemUI::ClearSolo() {
    if (m_layerStack) {
        m_layerStack->ClearSolo();
        SyncUIState();
    }
}

void BlendingSystemUI::MoveLayer(size_t fromIndex, size_t toIndex) {
    if (m_layerStack) {
        m_layerStack->MoveLayer(fromIndex, toIndex);
        NotifyBlendTreeChanged();
    }
}

void BlendingSystemUI::EditLayerTree(size_t layerIndex) {
    m_selectedLayerIndex = static_cast<int>(layerIndex);

    if (OnLayerSelected) {
        OnLayerSelected(layerIndex);
    }
}

void BlendingSystemUI::EditLayerMask(size_t layerIndex) {
    if (!m_layerStack) return;

    auto* layer = m_layerStack->GetLayer(layerIndex);
    if (layer && layer->GetMask()) {
        OpenMaskEditor(layer->GetMask()->GetName());
    }
}

void BlendingSystemUI::OpenBlendSpace1DEditor(const std::string& blendSpaceId) {
    // Would open the blend space 1D editor
    // This would typically be handled by the editor's panel system
}

void BlendingSystemUI::OpenBlendSpace2DEditor(const std::string& blendSpaceId) {
    // Would open the blend space 2D editor
}

void BlendingSystemUI::OpenMaskEditor(const std::string& maskId) {
    // Would open the mask editor
}

bool BlendingSystemUI::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    // Parse JSON and load blend tree
    // Would use proper JSON library

    m_isDirty = false;
    SyncUIState();
    return true;
}

bool BlendingSystemUI::SaveToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    // Serialize blend tree to JSON
    if (m_layerStack) {
        file << m_layerStack->ToJson();
    }

    m_isDirty = false;
    return true;
}

void BlendingSystemUI::NewBlendTree() {
    // Reset to empty state
    m_runtime = nullptr;
    m_layerStack = nullptr;
    m_previewTime = 0.0f;
    m_selectedLayerIndex = -1;
    m_isDirty = false;
    SyncUIState();
}

void BlendingSystemUI::SetPreviewPanelVisible(bool visible) {
    m_config.showPreview = visible;
}

void BlendingSystemUI::SetLayerPanelVisible(bool visible) {
    m_config.showLayers = visible;
}

void BlendingSystemUI::SetParameterPanelVisible(bool visible) {
    m_config.showParameters = visible;
}

void BlendingSystemUI::SetupJSBridge() {
    if (!m_bridge) return;

    // Register JS callbacks for web-based UI
    // This would expose C++ functions to JavaScript

    // Example:
    // m_bridge->RegisterFunction("setParameter", [this](const JSValue& args) {
    //     std::string name = args[0].AsString();
    //     float value = args[1].AsFloat();
    //     SetParameter(name, value);
    // });
}

void BlendingSystemUI::UpdatePreview(float deltaTime) {
    m_previewTime += deltaTime;

    if (m_runtime) {
        m_runtime->Update(deltaTime);
        auto pose = m_runtime->Evaluate(deltaTime);

        if (!m_previewPose) {
            m_previewPose = std::make_unique<Nova::AnimationPose>();
        }
        *m_previewPose = std::move(pose);
    }
}

void BlendingSystemUI::SyncUIState() {
    // Sync C++ state to UI (web view or ImGui)
    // This would send updates to the JavaScript side

    if (m_bridge) {
        // m_bridge->CallJS("updateLayers", GetLayers());
        // m_bridge->CallJS("updateParameters", GetParameters());
    }
}

void BlendingSystemUI::NotifyBlendTreeChanged() {
    m_isDirty = true;

    if (OnBlendTreeChanged) {
        OnBlendTreeChanged();
    }
}

} // namespace Vehement
