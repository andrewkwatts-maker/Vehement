#include "BlendTreeBuilder.hpp"
#include <algorithm>

namespace Nova {

// =============================================================================
// BlendTreeBuilder
// =============================================================================

BlendTreeBuilder& BlendTreeBuilder::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::SetName(const std::string& name) {
    m_name = name;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::AddClip(const Animation* clip, float speed) {
    auto node = std::make_unique<ClipNode>(clip);
    node->SetSpeed(speed);
    if (m_skeleton) node->SetSkeleton(m_skeleton);

    m_lastNode = node.get();
    m_pendingNode = std::move(node);
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::AddClip(const std::string& name, const Animation* clip, float speed) {
    auto node = std::make_unique<ClipNode>(clip);
    node->SetName(name);
    node->SetSpeed(speed);
    if (m_skeleton) node->SetSkeleton(m_skeleton);

    m_lastNode = node.get();
    m_pendingNode = std::move(node);
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BeginBlend1D(const std::string& parameter) {
    // Add pending node if in a blend context
    if (!m_frameStack.empty()) {
        auto& frame = CurrentFrame();
        if (frame.state == BuilderState::Blend1D && m_pendingNode) {
            auto* blend1d = dynamic_cast<Blend1DNode*>(frame.node.get());
            if (blend1d) {
                blend1d->AddEntry(std::move(m_pendingNode), m_pendingThreshold);
            }
        }
    }

    PushFrame(BuilderState::Blend1D);
    auto& frame = CurrentFrame();
    frame.node = std::make_unique<Blend1DNode>(parameter);
    if (m_skeleton) frame.node->SetSkeleton(m_skeleton);
    m_lastNode = frame.node.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::At(float threshold) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    if (frame.state == BuilderState::Blend1D && m_pendingNode) {
        auto* blend1d = dynamic_cast<Blend1DNode*>(frame.node.get());
        if (blend1d) {
            blend1d->AddEntry(std::move(m_pendingNode), threshold);
        }
    }

    m_pendingThreshold = threshold;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::EndBlend1D() {
    if (m_frameStack.empty()) return *this;

    // Add any pending node
    auto& frame = CurrentFrame();
    if (frame.state == BuilderState::Blend1D && m_pendingNode) {
        auto* blend1d = dynamic_cast<Blend1DNode*>(frame.node.get());
        if (blend1d) {
            blend1d->AddEntry(std::move(m_pendingNode), m_pendingThreshold);
        }
    }

    m_pendingNode = PopFrame();
    m_lastNode = m_pendingNode.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BeginBlend2D(const std::string& paramX, const std::string& paramY) {
    PushFrame(BuilderState::Blend2D);
    auto& frame = CurrentFrame();
    frame.node = std::make_unique<Blend2DNode>(paramX, paramY);
    if (m_skeleton) frame.node->SetSkeleton(m_skeleton);
    m_lastNode = frame.node.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::At(float x, float y) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    if (frame.state == BuilderState::Blend2D && m_pendingNode) {
        auto* blend2d = dynamic_cast<Blend2DNode*>(frame.node.get());
        if (blend2d) {
            blend2d->AddPoint(std::move(m_pendingNode), m_pendingPosition);
        }
    }

    m_pendingPosition = glm::vec2(x, y);
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::EndBlend2D() {
    if (m_frameStack.empty()) return *this;

    auto& frame = CurrentFrame();
    if (frame.state == BuilderState::Blend2D && m_pendingNode) {
        auto* blend2d = dynamic_cast<Blend2DNode*>(frame.node.get());
        if (blend2d) {
            blend2d->AddPoint(std::move(m_pendingNode), m_pendingPosition);
        }
    }

    m_pendingNode = PopFrame();
    m_lastNode = m_pendingNode.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BeginAdditive() {
    PushFrame(BuilderState::Additive);
    auto& frame = CurrentFrame();
    frame.node = std::make_unique<AdditiveNode>();
    if (m_skeleton) frame.node->SetSkeleton(m_skeleton);
    m_lastNode = frame.node.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Base() {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();
    frame.state = BuilderState::AdditiveBase;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Additive(const std::string& weightParam) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    // First set base if we have a pending node and were in base state
    if (frame.state == BuilderState::AdditiveBase && m_pendingNode) {
        auto* additive = dynamic_cast<AdditiveNode*>(frame.node.get());
        if (additive) {
            additive->SetBaseNode(std::move(m_pendingNode));
        }
    }

    frame.state = BuilderState::AdditiveLayer;

    if (!weightParam.empty()) {
        auto* additive = dynamic_cast<AdditiveNode*>(frame.node.get());
        if (additive) {
            additive->SetWeightParameter(weightParam);
        }
    }

    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::EndAdditive() {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    auto* additive = dynamic_cast<AdditiveNode*>(frame.node.get());
    if (additive && m_pendingNode) {
        if (frame.state == BuilderState::AdditiveBase) {
            additive->SetBaseNode(std::move(m_pendingNode));
        } else if (frame.state == BuilderState::AdditiveLayer) {
            additive->SetAdditiveNode(std::move(m_pendingNode));
        }
    }

    m_pendingNode = PopFrame();
    m_lastNode = m_pendingNode.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BeginLayered() {
    PushFrame(BuilderState::Layered);
    auto& frame = CurrentFrame();
    frame.node = std::make_unique<LayeredNode>();
    if (m_skeleton) frame.node->SetSkeleton(m_skeleton);
    m_lastNode = frame.node.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BaseLayer() {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();
    frame.state = BuilderState::LayeredBase;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Layer(float weight, bool additive) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    auto* layered = dynamic_cast<LayeredNode*>(frame.node.get());
    if (!layered) return *this;

    // Add base or previous layer
    if (frame.state == BuilderState::LayeredBase && m_pendingNode) {
        layered->SetBaseLayer(std::move(m_pendingNode));
    } else if (frame.state == BuilderState::LayeredOverlay && m_pendingNode) {
        layered->AddLayer(std::move(m_pendingNode), nullptr, weight, additive);
    }

    frame.state = BuilderState::LayeredOverlay;
    frame.threshold = weight;  // Store weight temporarily
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::WithMask(std::shared_ptr<BlendMask> mask) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    if (frame.state == BuilderState::LayeredOverlay) {
        auto* layered = dynamic_cast<LayeredNode*>(frame.node.get());
        if (layered && layered->GetLayerCount() > 0) {
            layered->GetLayer(layered->GetLayerCount() - 1).mask = mask;
        }
    }

    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::WithMask(BlendMask::Preset preset) {
    return WithMask(BlendMask::CreateFromPreset(preset, m_skeleton));
}

BlendTreeBuilder& BlendTreeBuilder::EndLayered() {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    auto* layered = dynamic_cast<LayeredNode*>(frame.node.get());
    if (layered && m_pendingNode) {
        if (frame.state == BuilderState::LayeredBase) {
            layered->SetBaseLayer(std::move(m_pendingNode));
        } else if (frame.state == BuilderState::LayeredOverlay) {
            layered->AddLayer(std::move(m_pendingNode), nullptr, frame.threshold, false);
        }
    }

    m_pendingNode = PopFrame();
    m_lastNode = m_pendingNode.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::BeginStates() {
    PushFrame(BuilderState::States);
    auto& frame = CurrentFrame();
    frame.node = std::make_unique<StateSelectorNode>();
    if (m_skeleton) frame.node->SetSkeleton(m_skeleton);
    m_lastNode = frame.node.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::State(const std::string& name) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    // Add previous state if exists
    auto* selector = dynamic_cast<StateSelectorNode*>(frame.node.get());
    if (selector && frame.state == BuilderState::StateEntry && m_pendingNode) {
        selector->AddState(frame.stateName, std::move(m_pendingNode));
    }

    frame.state = BuilderState::StateEntry;
    frame.stateName = name;
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::DefaultState(const std::string& name) {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    auto* selector = dynamic_cast<StateSelectorNode*>(frame.node.get());
    if (selector) {
        selector->SetCurrentState(name, 0.0f);
    }

    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::EndStates() {
    if (m_frameStack.empty()) return *this;
    auto& frame = CurrentFrame();

    auto* selector = dynamic_cast<StateSelectorNode*>(frame.node.get());
    if (selector && frame.state == BuilderState::StateEntry && m_pendingNode) {
        selector->AddState(frame.stateName, std::move(m_pendingNode));
    }

    m_pendingNode = PopFrame();
    m_lastNode = m_pendingNode.get();
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Weight(float weight) {
    if (m_lastNode) {
        m_lastNode->SetWeight(weight);
    }
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Speed(float speed) {
    if (m_lastNode) {
        m_lastNode->SetSpeed(speed);
    }
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Loop(bool looping) {
    auto* clip = dynamic_cast<ClipNode*>(m_lastNode);
    if (clip) {
        clip->SetLooping(looping);
    }
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::RootMotion(bool enabled) {
    auto* clip = dynamic_cast<ClipNode*>(m_lastNode);
    if (clip) {
        clip->SetRootMotionEnabled(enabled);
    }
    return *this;
}

BlendTreeBuilder& BlendTreeBuilder::Sync(bool enabled) {
    auto* blend1d = dynamic_cast<Blend1DNode*>(m_lastNode);
    if (blend1d) {
        blend1d->SetSyncEnabled(enabled);
    }
    return *this;
}

std::unique_ptr<BlendNode> BlendTreeBuilder::Build() {
    // Close any remaining frames
    while (!m_frameStack.empty()) {
        m_pendingNode = PopFrame();
    }

    if (m_pendingNode) {
        if (!m_name.empty()) {
            m_pendingNode->SetName(m_name);
        }
        return std::move(m_pendingNode);
    }

    return nullptr;
}

std::unique_ptr<BlendTreeRuntime> BlendTreeBuilder::BuildRuntime() {
    auto runtime = std::make_unique<BlendTreeRuntime>();
    runtime->SetSkeleton(m_skeleton);
    runtime->SetRootTree(Build());
    return runtime;
}

std::vector<std::string> BlendTreeBuilder::Validate() const {
    std::vector<std::string> errors;

    if (!m_skeleton) {
        errors.push_back("No skeleton set");
    }

    return errors;
}

bool BlendTreeBuilder::IsValid() const {
    return Validate().empty();
}

BlendTreeBuilder::BuilderFrame& BlendTreeBuilder::CurrentFrame() {
    static BuilderFrame emptyFrame;
    return m_frameStack.empty() ? emptyFrame : m_frameStack.top();
}

void BlendTreeBuilder::PushFrame(BuilderState state) {
    BuilderFrame frame;
    frame.state = state;
    m_frameStack.push(std::move(frame));
}

std::unique_ptr<BlendNode> BlendTreeBuilder::PopFrame() {
    if (m_frameStack.empty()) return nullptr;

    auto node = std::move(m_frameStack.top().node);
    m_frameStack.pop();
    return node;
}

// =============================================================================
// LayerStackBuilder
// =============================================================================

LayerStackBuilder& LayerStackBuilder::SetSkeleton(Skeleton* skeleton) {
    m_skeleton = skeleton;
    m_stack->SetSkeleton(skeleton);
    return *this;
}

LayerStackBuilder& LayerStackBuilder::AddBaseLayer(const std::string& name, std::unique_ptr<BlendNode> tree) {
    auto layer = std::make_unique<AnimationLayer>(name);
    layer->SetBlendTree(std::move(tree));
    layer->SetBlendMode(AnimationLayer::BlendMode::Override);
    m_lastLayer = layer.get();
    m_stack->SetBaseLayer(std::move(layer));
    return *this;
}

LayerStackBuilder& LayerStackBuilder::AddLayer(const std::string& name, std::unique_ptr<BlendNode> tree,
                                                AnimationLayer::BlendMode mode, float weight) {
    auto layer = std::make_unique<AnimationLayer>(name);
    layer->SetBlendTree(std::move(tree));
    layer->SetBlendMode(mode);
    layer->SetWeight(weight);
    m_lastLayer = layer.get();
    m_stack->AddLayer(std::move(layer));
    return *this;
}

LayerStackBuilder& LayerStackBuilder::WithMask(std::shared_ptr<BlendMask> mask) {
    if (m_lastLayer) {
        m_lastLayer->SetMask(mask);
    }
    return *this;
}

LayerStackBuilder& LayerStackBuilder::WithMask(BlendMask::Preset preset) {
    return WithMask(BlendMask::CreateFromPreset(preset, m_skeleton));
}

LayerStackBuilder& LayerStackBuilder::SyncGroup(const std::string& name, const std::vector<std::string>& layerNames) {
    std::vector<size_t> indices;
    for (const auto& layerName : layerNames) {
        int idx = m_stack->GetLayerIndex(layerName);
        if (idx >= 0) {
            indices.push_back(static_cast<size_t>(idx));
        }
    }
    m_stack->CreateSyncGroup(name, indices);
    return *this;
}

std::unique_ptr<AnimationLayerStack> LayerStackBuilder::Build() {
    return std::move(m_stack);
}

// =============================================================================
// BlendTreeOptimizer
// =============================================================================

void BlendTreeOptimizer::Optimize(BlendNode* tree, const Options& options) {
    if (!tree) return;

    if (options.removeUnusedNodes) {
        RemoveZeroWeightNodes(tree);
    }

    if (options.foldConstantNodes) {
        FoldConstants(tree);
    }
}

void BlendTreeOptimizer::RemoveZeroWeightNodes(BlendNode* tree) {
    // This would recursively remove nodes with zero weight
    // Simplified implementation
}

void BlendTreeOptimizer::FoldConstants(BlendNode* tree) {
    // This would fold single-child nodes
    // Simplified implementation
}

std::vector<std::string> BlendTreeOptimizer::Validate(const BlendNode* tree) {
    std::vector<std::string> errors;

    if (!tree) {
        errors.push_back("Tree is null");
        return errors;
    }

    // Check for common issues
    if (auto* blend1d = dynamic_cast<const Blend1DNode*>(tree)) {
        if (blend1d->GetEntries().empty()) {
            errors.push_back("Blend1D node has no entries");
        }
    }

    if (auto* blend2d = dynamic_cast<const Blend2DNode*>(tree)) {
        if (blend2d->GetPoints().empty()) {
            errors.push_back("Blend2D node has no points");
        }
    }

    if (auto* clip = dynamic_cast<const ClipNode*>(tree)) {
        if (!clip->GetClip()) {
            errors.push_back("ClipNode has no animation clip");
        }
    }

    return errors;
}

} // namespace Nova
