#include "DockingSystem.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <algorithm>
#include <cmath>

// JSON support for layout persistence
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// Global Dock Space Instance
// ============================================================================

static std::unique_ptr<DockSpace> g_dockSpace;

DockSpace& GetDockSpace() {
    if (!g_dockSpace) {
        g_dockSpace = std::make_unique<DockSpace>();
    }
    return *g_dockSpace;
}

void SetDockSpace(std::unique_ptr<DockSpace> dockSpace) {
    g_dockSpace = std::move(dockSpace);
}

// ============================================================================
// DockNode Implementation
// ============================================================================

DockNode::DockNode() : id(0) {}

DockNode::DockNode(uint64_t nodeId) : id(nodeId) {}

EditorPanel* DockNode::GetActivePanel() const {
    if (panels.empty()) return nullptr;
    int index = std::clamp(activeTabIndex, 0, static_cast<int>(panels.size()) - 1);
    return panels[index];
}

void DockNode::SetActivePanel(int index) {
    if (index >= 0 && index < static_cast<int>(panels.size())) {
        activeTabIndex = index;
    }
}

void DockNode::SetActivePanel(EditorPanel* panel) {
    int index = FindPanelIndex(panel);
    if (index >= 0) {
        activeTabIndex = index;
    }
}

int DockNode::FindPanelIndex(EditorPanel* panel) const {
    for (size_t i = 0; i < panels.size(); ++i) {
        if (panels[i] == panel) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void DockNode::AddPanel(EditorPanel* panel) {
    if (panel && FindPanelIndex(panel) < 0) {
        panels.push_back(panel);
        activeTabIndex = static_cast<int>(panels.size()) - 1;
    }
}

bool DockNode::RemovePanel(EditorPanel* panel) {
    auto it = std::find(panels.begin(), panels.end(), panel);
    if (it != panels.end()) {
        int removedIndex = static_cast<int>(std::distance(panels.begin(), it));
        panels.erase(it);

        // Adjust active tab index
        if (panels.empty()) {
            activeTabIndex = 0;
        } else if (activeTabIndex >= static_cast<int>(panels.size())) {
            activeTabIndex = static_cast<int>(panels.size()) - 1;
        } else if (removedIndex < activeTabIndex) {
            activeTabIndex--;
        }
        return true;
    }
    return false;
}

void DockNode::CalculateChildBounds() {
    if (!IsSplit() || !firstChild || !secondChild) return;

    if (splitDirection == SplitDirection::Horizontal) {
        float splitX = bounds.x + bounds.width * splitRatio;
        firstChild->bounds = DockRect(bounds.x, bounds.y,
                                       bounds.width * splitRatio, bounds.height);
        secondChild->bounds = DockRect(splitX, bounds.y,
                                        bounds.width * (1.0f - splitRatio), bounds.height);
    } else {
        float splitY = bounds.y + bounds.height * splitRatio;
        firstChild->bounds = DockRect(bounds.x, bounds.y,
                                       bounds.width, bounds.height * splitRatio);
        secondChild->bounds = DockRect(bounds.x, splitY,
                                        bounds.width, bounds.height * (1.0f - splitRatio));
    }
}

DockNode::Ptr DockNode::GetSibling(const Ptr& child) const {
    if (firstChild == child) return secondChild;
    if (secondChild == child) return firstChild;
    return nullptr;
}

bool DockNode::IsAncestorOf(const Ptr& node) const {
    if (!node) return false;

    Ptr current = node->GetParent();
    while (current) {
        if (current.get() == this) return true;
        current = current->GetParent();
    }
    return false;
}

int DockNode::GetDepth() const {
    int depth = 0;
    Ptr current = parent.lock();
    while (current) {
        depth++;
        current = current->GetParent();
    }
    return depth;
}

void DockNode::CollectLeafNodes(std::vector<Ptr>& outLeaves) {
    if (IsLeaf()) {
        // We need to get shared_ptr to this, but we don't have it here
        // This will be called from DockSpace which has the shared_ptr
        return;
    }

    if (firstChild) {
        if (firstChild->IsLeaf()) {
            outLeaves.push_back(firstChild);
        } else {
            firstChild->CollectLeafNodes(outLeaves);
        }
    }

    if (secondChild) {
        if (secondChild->IsLeaf()) {
            outLeaves.push_back(secondChild);
        } else {
            secondChild->CollectLeafNodes(outLeaves);
        }
    }
}

DockNode::Ptr DockNode::FindNodeById(uint64_t nodeId) {
    if (id == nodeId) {
        // Return self as shared_ptr - this requires external tracking
        return nullptr; // Caller should use DockSpace::FindNodeById
    }

    if (firstChild) {
        if (firstChild->id == nodeId) return firstChild;
        auto found = firstChild->FindNodeById(nodeId);
        if (found) return found;
    }

    if (secondChild) {
        if (secondChild->id == nodeId) return secondChild;
        auto found = secondChild->FindNodeById(nodeId);
        if (found) return found;
    }

    return nullptr;
}

DockNode::Ptr DockNode::FindNodeByPanel(EditorPanel* panel) {
    if (IsLeaf()) {
        if (FindPanelIndex(panel) >= 0) {
            // Return self - but we need shared_ptr from outside
            return nullptr;
        }
    }

    if (firstChild) {
        if (firstChild->IsLeaf() && firstChild->FindPanelIndex(panel) >= 0) {
            return firstChild;
        }
        auto found = firstChild->FindNodeByPanel(panel);
        if (found) return found;
    }

    if (secondChild) {
        if (secondChild->IsLeaf() && secondChild->FindPanelIndex(panel) >= 0) {
            return secondChild;
        }
        auto found = secondChild->FindNodeByPanel(panel);
        if (found) return found;
    }

    return nullptr;
}

// ============================================================================
// DockSpace Implementation
// ============================================================================

DockSpace::DockSpace() = default;

DockSpace::~DockSpace() {
    Shutdown();
}

bool DockSpace::Initialize(const DockRect& workArea, const Config& config) {
    m_config = config;
    m_workArea = workArea;
    m_rootNode = CreateNode();
    m_rootNode->bounds = workArea;
    m_initialized = true;
    return true;
}

void DockSpace::Shutdown() {
    m_rootNode.reset();
    m_floatingNodes.clear();
    m_initialized = false;
}

void DockSpace::SetWorkArea(const DockRect& workArea) {
    m_workArea = workArea;
    RecalculateBounds();
}

// =========================================================================
// Panel Management
// =========================================================================

DockNode::Ptr DockSpace::AddPanel(EditorPanel* panel, DockPosition position,
                                   DockNode* relativeTo) {
    if (!panel || !m_initialized) return nullptr;

    // Find target node
    DockNode::Ptr targetNode;
    if (relativeTo) {
        targetNode = FindNodeById(relativeTo->id);
    }
    if (!targetNode) {
        targetNode = m_rootNode;
    }

    // Handle floating
    if (position == DockPosition::Floating) {
        auto floatNode = CreateNode();
        floatNode->isFloating = true;
        floatNode->floatingPos = glm::vec2(100, 100);
        floatNode->floatingSize = panel->GetConfig().defaultSize;
        floatNode->bounds = DockRect(floatNode->floatingPos, floatNode->floatingSize);
        floatNode->AddPanel(panel);
        m_floatingNodes.push_back(floatNode);

        if (OnPanelDocked) OnPanelDocked(panel, floatNode.get());
        if (OnLayoutChanged) OnLayoutChanged();
        return floatNode;
    }

    // Dock to existing node
    auto resultNode = DockPanelToNode(panel, targetNode, position);

    RecalculateBounds();

    if (OnPanelDocked) OnPanelDocked(panel, resultNode.get());
    if (OnLayoutChanged) OnLayoutChanged();

    return resultNode;
}

bool DockSpace::RemovePanel(EditorPanel* panel) {
    if (!panel) return false;

    // Check floating nodes
    for (auto it = m_floatingNodes.begin(); it != m_floatingNodes.end(); ++it) {
        if ((*it)->RemovePanel(panel)) {
            if ((*it)->IsEmpty()) {
                m_floatingNodes.erase(it);
            }
            if (OnPanelUndocked) OnPanelUndocked(panel);
            if (OnLayoutChanged) OnLayoutChanged();
            return true;
        }
    }

    // Check docked nodes
    auto node = FindNodeByPanel(panel);
    if (node) {
        node->RemovePanel(panel);
        RemoveEmptyNodes();
        RecalculateBounds();

        if (OnPanelUndocked) OnPanelUndocked(panel);
        if (OnLayoutChanged) OnLayoutChanged();
        return true;
    }

    return false;
}

bool DockSpace::MovePanel(EditorPanel* panel, DockPosition position,
                          DockNode* relativeTo) {
    if (!panel) return false;

    // Remove from current location
    RemovePanel(panel);

    // Add to new location
    AddPanel(panel, position, relativeTo);
    return true;
}

bool DockSpace::IsPanelDocked(EditorPanel* panel) const {
    return FindNodeByPanel(const_cast<EditorPanel*>(panel)) != nullptr;
}

DockNode::Ptr DockSpace::FindNodeByPanel(EditorPanel* panel) {
    if (!panel || !m_rootNode) return nullptr;

    // Check floating nodes first
    for (auto& floatNode : m_floatingNodes) {
        if (floatNode->FindPanelIndex(panel) >= 0) {
            return floatNode;
        }
    }

    // Search in tree
    std::function<DockNode::Ptr(DockNode::Ptr)> findInTree;
    findInTree = [&](DockNode::Ptr node) -> DockNode::Ptr {
        if (!node) return nullptr;

        if (node->IsLeaf()) {
            if (node->FindPanelIndex(panel) >= 0) {
                return node;
            }
            return nullptr;
        }

        auto found = findInTree(node->firstChild);
        if (found) return found;
        return findInTree(node->secondChild);
    };

    return findInTree(m_rootNode);
}

std::vector<EditorPanel*> DockSpace::GetAllPanels() const {
    std::vector<EditorPanel*> result;

    std::function<void(const DockNode::Ptr&)> collectPanels;
    collectPanels = [&](const DockNode::Ptr& node) {
        if (!node) return;

        if (node->IsLeaf()) {
            for (auto* panel : node->panels) {
                result.push_back(panel);
            }
        } else {
            collectPanels(node->firstChild);
            collectPanels(node->secondChild);
        }
    };

    collectPanels(m_rootNode);

    for (const auto& floatNode : m_floatingNodes) {
        for (auto* panel : floatNode->panels) {
            result.push_back(panel);
        }
    }

    return result;
}

// =========================================================================
// Node Operations
// =========================================================================

DockNode::Ptr DockSpace::SplitNode(DockNode* node, DockPosition direction,
                                    float ratio) {
    if (!node || !node->IsLeaf()) return nullptr;

    auto nodePtr = FindNodeById(node->id);
    if (!nodePtr) return nullptr;

    // Create two new children
    auto firstChild = CreateNode();
    auto secondChild = CreateNode();

    // Move existing panels to first child
    firstChild->panels = std::move(nodePtr->panels);
    firstChild->activeTabIndex = nodePtr->activeTabIndex;

    // Set up the split
    switch (direction) {
        case DockPosition::Left:
        case DockPosition::Right:
            nodePtr->splitDirection = SplitDirection::Horizontal;
            break;
        case DockPosition::Top:
        case DockPosition::Bottom:
            nodePtr->splitDirection = SplitDirection::Vertical;
            break;
        default:
            return nullptr;
    }

    nodePtr->splitRatio = ratio;
    nodePtr->panels.clear();

    // Assign children based on direction
    if (direction == DockPosition::Left || direction == DockPosition::Top) {
        nodePtr->firstChild = secondChild;  // New panel goes first
        nodePtr->secondChild = firstChild;  // Existing content goes second
    } else {
        nodePtr->firstChild = firstChild;   // Existing content goes first
        nodePtr->secondChild = secondChild; // New panel goes second
    }

    firstChild->parent = nodePtr;
    secondChild->parent = nodePtr;

    RecalculateBounds();

    if (OnLayoutChanged) OnLayoutChanged();

    // Return the new empty node
    return (direction == DockPosition::Left || direction == DockPosition::Top)
               ? secondChild
               : firstChild;
}

DockNode::Ptr DockSpace::FindNodeById(uint64_t id) {
    if (!m_rootNode) return nullptr;

    if (m_rootNode->id == id) return m_rootNode;

    std::function<DockNode::Ptr(DockNode::Ptr)> findInTree;
    findInTree = [&](DockNode::Ptr node) -> DockNode::Ptr {
        if (!node) return nullptr;
        if (node->id == id) return node;

        if (node->firstChild) {
            if (node->firstChild->id == id) return node->firstChild;
            auto found = findInTree(node->firstChild);
            if (found) return found;
        }

        if (node->secondChild) {
            if (node->secondChild->id == id) return node->secondChild;
            auto found = findInTree(node->secondChild);
            if (found) return found;
        }

        return nullptr;
    };

    auto found = findInTree(m_rootNode);
    if (found) return found;

    // Check floating nodes
    for (auto& floatNode : m_floatingNodes) {
        if (floatNode->id == id) return floatNode;
    }

    return nullptr;
}

std::vector<DockNode::Ptr> DockSpace::GetLeafNodes() const {
    std::vector<DockNode::Ptr> result;

    std::function<void(const DockNode::Ptr&)> collectLeaves;
    collectLeaves = [&](const DockNode::Ptr& node) {
        if (!node) return;

        if (node->IsLeaf()) {
            result.push_back(node);
        } else {
            collectLeaves(node->firstChild);
            collectLeaves(node->secondChild);
        }
    };

    collectLeaves(m_rootNode);
    return result;
}

std::vector<DockNode::Ptr> DockSpace::GetFloatingNodes() const {
    return m_floatingNodes;
}

// =========================================================================
// Drag & Drop
// =========================================================================

void DockSpace::BeginDrag(EditorPanel* panel, const glm::vec2& mousePos) {
    if (!panel) return;

    m_dragInfo = DockDragInfo();
    m_dragInfo.panel = panel;
    m_dragInfo.sourceNode = FindNodeByPanel(panel);
    m_dragInfo.currentPos = mousePos;
    m_dragInfo.state = DockDragState::Dragging;
    m_dragInfo.detached = false;

    // Calculate drag offset (panel title position to mouse)
    if (m_dragInfo.sourceNode) {
        m_dragInfo.dragOffset = mousePos - m_dragInfo.sourceNode->bounds.GetPos();
    }
}

void DockSpace::UpdateDrag(const glm::vec2& mousePos) {
    if (m_dragInfo.state == DockDragState::None) return;

    m_dragInfo.currentPos = mousePos;

    // Check if we should detach
    if (!m_dragInfo.detached && m_dragInfo.sourceNode) {
        float dragDistance = glm::length(mousePos - (m_dragInfo.sourceNode->bounds.GetPos() + m_dragInfo.dragOffset));
        if (dragDistance > 20.0f) {
            m_dragInfo.detached = true;
        }
    }

    if (!m_dragInfo.detached) {
        m_dragInfo.state = DockDragState::Dragging;
        return;
    }

    // Find best drop zone
    m_dragInfo.hoveredZone = FindBestDropZone(mousePos);

    // Update drag state based on hovered zone
    if (!m_dragInfo.hoveredZone.isValid || !m_dragInfo.hoveredZone.targetNode) {
        m_dragInfo.state = m_config.allowFloating
                             ? DockDragState::PreviewFloating
                             : DockDragState::Dragging;
    } else {
        switch (m_dragInfo.hoveredZone.position) {
            case DockPosition::Left:   m_dragInfo.state = DockDragState::PreviewLeft;   break;
            case DockPosition::Right:  m_dragInfo.state = DockDragState::PreviewRight;  break;
            case DockPosition::Top:    m_dragInfo.state = DockDragState::PreviewTop;    break;
            case DockPosition::Bottom: m_dragInfo.state = DockDragState::PreviewBottom; break;
            case DockPosition::Center: m_dragInfo.state = DockDragState::PreviewCenter; break;
            default: m_dragInfo.state = DockDragState::Dragging; break;
        }
    }
}

bool DockSpace::EndDrag() {
    if (m_dragInfo.state == DockDragState::None) return false;

    bool docked = false;

    if (m_dragInfo.detached && m_dragInfo.panel) {
        // Remove from source
        if (m_dragInfo.sourceNode) {
            m_dragInfo.sourceNode->RemovePanel(m_dragInfo.panel);
        }

        if (m_dragInfo.hoveredZone.isValid && m_dragInfo.hoveredZone.targetNode) {
            // Dock to target
            DockPanelToNode(m_dragInfo.panel, m_dragInfo.hoveredZone.targetNode,
                           m_dragInfo.hoveredZone.position);
            docked = true;
        } else if (m_config.allowFloating) {
            // Create floating window
            auto floatNode = CreateNode();
            floatNode->isFloating = true;
            floatNode->floatingPos = m_dragInfo.currentPos - m_dragInfo.dragOffset;
            floatNode->floatingSize = m_dragInfo.panel->GetConfig().defaultSize;
            floatNode->bounds = DockRect(floatNode->floatingPos, floatNode->floatingSize);
            floatNode->AddPanel(m_dragInfo.panel);
            m_floatingNodes.push_back(floatNode);
            docked = true;
        }

        RemoveEmptyNodes();
        RecalculateBounds();

        if (OnLayoutChanged) OnLayoutChanged();
    }

    m_dragInfo = DockDragInfo();
    return docked;
}

void DockSpace::CancelDrag() {
    m_dragInfo = DockDragInfo();
}

// =========================================================================
// Resize Handles
// =========================================================================

bool DockSpace::IsOverSplitter(const glm::vec2& mousePos, DockNode::Ptr& outNode) const {
    std::function<bool(const DockNode::Ptr&)> checkNode;
    checkNode = [&](const DockNode::Ptr& node) -> bool {
        if (!node || node->IsLeaf()) return false;

        // Calculate splitter rect
        DockRect splitterRect;
        if (node->splitDirection == SplitDirection::Horizontal) {
            float splitX = node->bounds.x + node->bounds.width * node->splitRatio;
            splitterRect = DockRect(
                splitX - m_config.splitterSize * 0.5f,
                node->bounds.y,
                m_config.splitterSize,
                node->bounds.height
            );
        } else {
            float splitY = node->bounds.y + node->bounds.height * node->splitRatio;
            splitterRect = DockRect(
                node->bounds.x,
                splitY - m_config.splitterSize * 0.5f,
                node->bounds.width,
                m_config.splitterSize
            );
        }

        if (splitterRect.Contains(mousePos)) {
            outNode = std::const_pointer_cast<DockNode>(node);
            return true;
        }

        if (checkNode(node->firstChild)) return true;
        if (checkNode(node->secondChild)) return true;
        return false;
    };

    return checkNode(m_rootNode);
}

void DockSpace::BeginResize(DockNode* node, const glm::vec2& mousePos) {
    if (!node || node->IsLeaf()) return;

    m_isResizing = true;
    m_resizeNode = FindNodeById(node->id);
    m_resizeStartPos = mousePos;
    m_resizeStartRatio = node->splitRatio;
}

void DockSpace::UpdateResize(const glm::vec2& mousePos) {
    if (!m_isResizing || !m_resizeNode) return;

    glm::vec2 delta = mousePos - m_resizeStartPos;
    float newRatio = m_resizeStartRatio;

    if (m_resizeNode->splitDirection == SplitDirection::Horizontal) {
        float deltaRatio = delta.x / m_resizeNode->bounds.width;
        newRatio = m_resizeStartRatio + deltaRatio;
    } else {
        float deltaRatio = delta.y / m_resizeNode->bounds.height;
        newRatio = m_resizeStartRatio + deltaRatio;
    }

    // Clamp to ensure minimum sizes
    float minRatio = m_config.minNodeSize / std::max(
        m_resizeNode->splitDirection == SplitDirection::Horizontal
            ? m_resizeNode->bounds.width
            : m_resizeNode->bounds.height,
        1.0f);
    float maxRatio = 1.0f - minRatio;

    m_resizeNode->splitRatio = std::clamp(newRatio, minRatio, maxRatio);
    RecalculateBounds();
}

void DockSpace::EndResize() {
    if (m_isResizing && OnLayoutChanged) {
        OnLayoutChanged();
    }
    m_isResizing = false;
    m_resizeNode.reset();
}

void DockSpace::ResetSplitter(DockNode* node) {
    if (node && node->IsSplit()) {
        node->splitRatio = 0.5f;
        RecalculateBounds();
        if (OnLayoutChanged) OnLayoutChanged();
    }
}

// =========================================================================
// Rendering
// =========================================================================

void DockSpace::Render() {
    if (!m_initialized || !m_rootNode) return;

    // Update hovered splitter
    glm::vec2 mousePos(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
    m_hoveredSplitterNode.reset();
    if (!IsDragging() && !m_isResizing) {
        IsOverSplitter(mousePos, m_hoveredSplitterNode);
    }

    // Handle resize input
    if (m_hoveredSplitterNode && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        BeginResize(m_hoveredSplitterNode.get(), mousePos);
    }

    if (m_isResizing) {
        UpdateResize(mousePos);
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            EndResize();
        }
    }

    // Handle double-click to reset splitter
    if (m_hoveredSplitterNode && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ResetSplitter(m_hoveredSplitterNode.get());
    }

    // Render docked nodes
    RenderNode(m_rootNode);

    // Render splitters
    RenderSplitters();

    // Render floating windows
    RenderFloatingWindows();

    // Render drop preview during drag
    if (IsDragging() && m_dragInfo.detached) {
        RenderDropPreview();
    }

    // Update cursor based on hover/resize state
    if (m_isResizing || m_hoveredSplitterNode) {
        if ((m_isResizing ? m_resizeNode : m_hoveredSplitterNode)->splitDirection == SplitDirection::Horizontal) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        } else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
    }
}

void DockSpace::RenderDropPreview() {
    if (!m_config.showDropPreview) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    if (m_dragInfo.hoveredZone.isValid && m_dragInfo.hoveredZone.targetNode) {
        // Draw preview rectangle
        DockRect previewRect = m_dragInfo.hoveredZone.bounds;
        ImU32 previewColor = EditorTheme::ToImU32(m_config.dropPreviewColor);

        drawList->AddRectFilled(
            ImVec2(previewRect.x, previewRect.y),
            ImVec2(previewRect.x + previewRect.width, previewRect.y + previewRect.height),
            previewColor
        );

        // Draw border
        glm::vec4 borderColor = m_config.dropPreviewColor;
        borderColor.w = 0.8f;
        drawList->AddRect(
            ImVec2(previewRect.x, previewRect.y),
            ImVec2(previewRect.x + previewRect.width, previewRect.y + previewRect.height),
            EditorTheme::ToImU32(borderColor),
            0.0f, 0, 2.0f
        );
    }

    // Draw floating panel preview
    if (m_dragInfo.state == DockDragState::PreviewFloating) {
        DockRect floatRect(
            m_dragInfo.currentPos - m_dragInfo.dragOffset,
            m_dragInfo.panel->GetConfig().defaultSize
        );

        glm::vec4 floatColor = m_config.dropPreviewColor;
        floatColor.w = 0.5f;

        drawList->AddRectFilled(
            ImVec2(floatRect.x, floatRect.y),
            ImVec2(floatRect.x + floatRect.width, floatRect.y + floatRect.height),
            EditorTheme::ToImU32(floatColor)
        );
    }
}

void DockSpace::RenderSplitters() {
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    std::function<void(const DockNode::Ptr&)> renderSplitter;
    renderSplitter = [&](const DockNode::Ptr& node) {
        if (!node || node->IsLeaf()) return;

        bool isHovered = (m_hoveredSplitterNode == node);
        bool isResizing = (m_isResizing && m_resizeNode == node);

        glm::vec4 color = (isHovered || isResizing)
                             ? m_config.splitterHoverColor
                             : m_config.splitterColor;

        DockRect splitterRect;
        if (node->splitDirection == SplitDirection::Horizontal) {
            float splitX = node->bounds.x + node->bounds.width * node->splitRatio;
            splitterRect = DockRect(
                splitX - m_config.splitterSize * 0.5f,
                node->bounds.y,
                m_config.splitterSize,
                node->bounds.height
            );
        } else {
            float splitY = node->bounds.y + node->bounds.height * node->splitRatio;
            splitterRect = DockRect(
                node->bounds.x,
                splitY - m_config.splitterSize * 0.5f,
                node->bounds.width,
                m_config.splitterSize
            );
        }

        drawList->AddRectFilled(
            ImVec2(splitterRect.x, splitterRect.y),
            ImVec2(splitterRect.x + splitterRect.width, splitterRect.y + splitterRect.height),
            EditorTheme::ToImU32(color)
        );

        renderSplitter(node->firstChild);
        renderSplitter(node->secondChild);
    };

    renderSplitter(m_rootNode);
}

void DockSpace::RenderFloatingWindows() {
    for (auto it = m_floatingNodes.begin(); it != m_floatingNodes.end(); ) {
        auto& floatNode = *it;

        if (floatNode->panels.empty()) {
            it = m_floatingNodes.erase(it);
            continue;
        }

        EditorPanel* activePanel = floatNode->GetActivePanel();
        if (!activePanel) {
            ++it;
            continue;
        }

        std::string windowTitle = activePanel->GetTitle() + "###float_" + std::to_string(floatNode->id);

        ImGui::SetNextWindowPos(ImVec2(floatNode->floatingPos.x, floatNode->floatingPos.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(floatNode->floatingSize.x, floatNode->floatingSize.y), ImGuiCond_FirstUseEver);

        bool windowOpen = true;
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

        if (ImGui::Begin(windowTitle.c_str(), &windowOpen, flags)) {
            // Update floating position and size
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            floatNode->floatingPos = glm::vec2(pos.x, pos.y);
            floatNode->floatingSize = glm::vec2(size.x, size.y);
            floatNode->bounds = DockRect(floatNode->floatingPos, floatNode->floatingSize);

            // Render tabs if multiple panels
            if (floatNode->panels.size() > 1) {
                RenderTabBar(floatNode);
            }

            // Render active panel content
            if (activePanel->IsVisible()) {
                activePanel->Render();
            }
        }
        ImGui::End();

        if (!windowOpen) {
            // Close the floating window
            for (auto* panel : floatNode->panels) {
                if (OnPanelClosed) OnPanelClosed(panel);
            }
            it = m_floatingNodes.erase(it);
            if (OnLayoutChanged) OnLayoutChanged();
        } else {
            ++it;
        }
    }
}

void DockSpace::RenderNode(DockNode::Ptr node) {
    if (!node) return;

    if (node->IsLeaf()) {
        if (node->panels.empty()) return;

        // Create a child window for this dock node
        std::string childId = "##dock_" + std::to_string(node->id);

        ImGui::SetNextWindowPos(ImVec2(node->bounds.x, node->bounds.y));
        ImGui::SetNextWindowSize(ImVec2(node->bounds.width, node->bounds.height));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin(childId.c_str(), nullptr, flags)) {
            // Render tab bar if multiple panels
            float tabBarHeight = 0.0f;
            if (node->panels.size() > 1) {
                RenderTabBar(node);
                tabBarHeight = m_config.tabHeight;
            } else if (!node->panels.empty()) {
                // Single panel - show title bar
                auto& theme = EditorTheme::Instance();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorTheme::ToImVec4(theme.GetColors().panelHeader));

                ImGui::BeginChild("##header", ImVec2(0, m_config.tabHeight), ImGuiChildFlags_None);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(node->panels[0]->GetTitle().c_str());
                ImGui::EndChild();

                ImGui::PopStyleColor();
                tabBarHeight = m_config.tabHeight;
            }

            // Render active panel
            EditorPanel* activePanel = node->GetActivePanel();
            if (activePanel) {
                float contentHeight = node->bounds.height - tabBarHeight;
                ImGui::BeginChild("##content", ImVec2(0, contentHeight), ImGuiChildFlags_None);
                RenderPanelContent(node, activePanel);
                ImGui::EndChild();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    } else {
        // Recurse into children
        RenderNode(node->firstChild);
        RenderNode(node->secondChild);
    }
}

void DockSpace::RenderTabBar(DockNode::Ptr node) {
    if (!node || node->panels.empty()) return;

    auto& theme = EditorTheme::Instance();
    std::string tabBarId = "##tabs_" + std::to_string(node->id);

    ImGui::PushStyleColor(ImGuiCol_Tab, EditorTheme::ToImVec4(theme.GetColors().tab));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, EditorTheme::ToImVec4(theme.GetColors().tabHovered));
    ImGui::PushStyleColor(ImGuiCol_TabActive, EditorTheme::ToImVec4(theme.GetColors().tabActive));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocused, EditorTheme::ToImVec4(theme.GetColors().tabUnfocused));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, EditorTheme::ToImVec4(theme.GetColors().tabActive));

    if (ImGui::BeginTabBar(tabBarId.c_str(), ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (size_t i = 0; i < node->panels.size(); ++i) {
            EditorPanel* panel = node->panels[i];
            bool isActive = (static_cast<int>(i) == node->activeTabIndex);

            std::string tabLabel = panel->GetTitle();
            if (panel->IsDirty()) tabLabel += " *";
            tabLabel += "###tab_" + std::to_string(reinterpret_cast<uintptr_t>(panel));

            ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
            bool tabOpen = true;

            if (ImGui::BeginTabItem(tabLabel.c_str(), m_config.allowCloseTabs ? &tabOpen : nullptr, tabFlags)) {
                node->activeTabIndex = static_cast<int>(i);
                ImGui::EndTabItem();
            }

            // Handle tab close
            if (!tabOpen) {
                node->RemovePanel(panel);
                if (OnPanelClosed) OnPanelClosed(panel);
                if (OnLayoutChanged) OnLayoutChanged();
                break; // Iterator invalidated
            }

            // Handle tab drag start
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                glm::vec2 mousePos(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
                if (!IsDragging()) {
                    BeginDrag(panel, mousePos);
                }
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::PopStyleColor(5);
}

void DockSpace::RenderPanelContent(DockNode::Ptr node, EditorPanel* panel) {
    (void)node;
    if (!panel) return;

    // The panel renders itself using ImGui
    // We just need to call Update and let it render
    panel->Render();
}

// =========================================================================
// Layout Persistence
// =========================================================================

bool DockSpace::SaveLayout(const std::string& path) const {
    try {
        DockLayout layout = GetCurrentLayout();

        nlohmann::json j;
        j["name"] = layout.name;
        j["rootNodeId"] = layout.rootNodeId;
        j["workAreaPos"] = {layout.workAreaPos.x, layout.workAreaPos.y};
        j["workAreaSize"] = {layout.workAreaSize.x, layout.workAreaSize.y};

        nlohmann::json nodesArray = nlohmann::json::array();
        for (const auto& nodeLayout : layout.nodes) {
            nlohmann::json nodeJson;
            nodeJson["id"] = nodeLayout.id;
            nodeJson["parentId"] = nodeLayout.parentId;
            nodeJson["splitDirection"] = static_cast<int>(nodeLayout.splitDirection);
            nodeJson["splitRatio"] = nodeLayout.splitRatio;
            nodeJson["panelIds"] = nodeLayout.panelIds;
            nodeJson["activeTabIndex"] = nodeLayout.activeTabIndex;
            nodeJson["isFloating"] = nodeLayout.isFloating;
            nodeJson["floatingPos"] = {nodeLayout.floatingPos.x, nodeLayout.floatingPos.y};
            nodeJson["floatingSize"] = {nodeLayout.floatingSize.x, nodeLayout.floatingSize.y};
            nodeJson["bounds"] = {nodeLayout.bounds.x, nodeLayout.bounds.y,
                                  nodeLayout.bounds.width, nodeLayout.bounds.height};
            nodesArray.push_back(nodeJson);
        }
        j["nodes"] = nodesArray;

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool DockSpace::LoadLayout(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        DockLayout layout;
        layout.name = j.value("name", "");
        layout.rootNodeId = j.value("rootNodeId", 0ULL);

        if (j.contains("workAreaPos")) {
            layout.workAreaPos.x = j["workAreaPos"][0];
            layout.workAreaPos.y = j["workAreaPos"][1];
        }
        if (j.contains("workAreaSize")) {
            layout.workAreaSize.x = j["workAreaSize"][0];
            layout.workAreaSize.y = j["workAreaSize"][1];
        }

        for (const auto& nodeJson : j["nodes"]) {
            DockLayout::NodeLayout nodeLayout;
            nodeLayout.id = nodeJson.value("id", 0ULL);
            nodeLayout.parentId = nodeJson.value("parentId", 0ULL);
            nodeLayout.splitDirection = static_cast<SplitDirection>(nodeJson.value("splitDirection", 0));
            nodeLayout.splitRatio = nodeJson.value("splitRatio", 0.5f);
            nodeLayout.activeTabIndex = nodeJson.value("activeTabIndex", 0);
            nodeLayout.isFloating = nodeJson.value("isFloating", false);

            if (nodeJson.contains("panelIds")) {
                for (const auto& panelId : nodeJson["panelIds"]) {
                    nodeLayout.panelIds.push_back(panelId.get<std::string>());
                }
            }

            if (nodeJson.contains("floatingPos")) {
                nodeLayout.floatingPos.x = nodeJson["floatingPos"][0];
                nodeLayout.floatingPos.y = nodeJson["floatingPos"][1];
            }
            if (nodeJson.contains("floatingSize")) {
                nodeLayout.floatingSize.x = nodeJson["floatingSize"][0];
                nodeLayout.floatingSize.y = nodeJson["floatingSize"][1];
            }
            if (nodeJson.contains("bounds")) {
                nodeLayout.bounds.x = nodeJson["bounds"][0];
                nodeLayout.bounds.y = nodeJson["bounds"][1];
                nodeLayout.bounds.width = nodeJson["bounds"][2];
                nodeLayout.bounds.height = nodeJson["bounds"][3];
            }

            layout.nodes.push_back(nodeLayout);
        }

        return ApplyLayout(layout);
    } catch (...) {
        return false;
    }
}

DockLayout DockSpace::GetCurrentLayout() const {
    DockLayout layout;
    layout.name = "current";
    layout.workAreaPos = m_workArea.GetPos();
    layout.workAreaSize = m_workArea.GetSize();
    layout.rootNodeId = m_rootNode ? m_rootNode->id : 0;

    if (m_rootNode) {
        SerializeNode(m_rootNode, layout);
    }

    for (const auto& floatNode : m_floatingNodes) {
        SerializeNode(floatNode, layout);
    }

    return layout;
}

bool DockSpace::ApplyLayout(const DockLayout& layout) {
    // Build panel map from registry
    std::unordered_map<std::string, EditorPanel*> panelMap;
    auto allPanels = PanelRegistry::Instance().GetAll();
    for (auto& panel : allPanels) {
        panelMap[panel->GetID()] = panel.get();
    }

    // Clear current layout
    ClearLayout();

    // Build node map
    std::unordered_map<uint64_t, DockLayout::NodeLayout> nodeLayoutMap;
    for (const auto& nodeLayout : layout.nodes) {
        nodeLayoutMap[nodeLayout.id] = nodeLayout;
    }

    // Reconstruct nodes
    std::unordered_map<uint64_t, DockNode::Ptr> nodeMap;

    std::function<DockNode::Ptr(uint64_t)> buildNode;
    buildNode = [&](uint64_t nodeId) -> DockNode::Ptr {
        if (nodeMap.count(nodeId)) return nodeMap[nodeId];

        auto it = nodeLayoutMap.find(nodeId);
        if (it == nodeLayoutMap.end()) return nullptr;

        const auto& nodeLayout = it->second;
        auto node = std::make_shared<DockNode>(nodeLayout.id);
        nodeMap[nodeId] = node;

        node->splitDirection = nodeLayout.splitDirection;
        node->splitRatio = nodeLayout.splitRatio;
        node->activeTabIndex = nodeLayout.activeTabIndex;
        node->isFloating = nodeLayout.isFloating;
        node->floatingPos = nodeLayout.floatingPos;
        node->floatingSize = nodeLayout.floatingSize;
        node->bounds = nodeLayout.bounds;

        // Add panels
        for (const auto& panelId : nodeLayout.panelIds) {
            auto panelIt = panelMap.find(panelId);
            if (panelIt != panelMap.end()) {
                node->AddPanel(panelIt->second);
            }
        }

        // Update next node ID
        m_nextNodeId = std::max(m_nextNodeId, nodeLayout.id + 1);

        return node;
    };

    // Build all nodes first
    for (const auto& nodeLayout : layout.nodes) {
        buildNode(nodeLayout.id);
    }

    // Set up parent-child relationships
    for (const auto& nodeLayout : layout.nodes) {
        auto node = nodeMap[nodeLayout.id];
        if (!node) continue;

        if (nodeLayout.parentId != 0 && nodeMap.count(nodeLayout.parentId)) {
            node->parent = nodeMap[nodeLayout.parentId];
        }

        // Find children by looking for nodes with this node as parent
        for (const auto& otherLayout : layout.nodes) {
            if (otherLayout.parentId == nodeLayout.id) {
                auto childNode = nodeMap[otherLayout.id];
                if (!node->firstChild) {
                    node->firstChild = childNode;
                } else {
                    node->secondChild = childNode;
                }
            }
        }
    }

    // Set root node
    if (nodeMap.count(layout.rootNodeId)) {
        m_rootNode = nodeMap[layout.rootNodeId];
    }

    // Collect floating nodes
    m_floatingNodes.clear();
    for (const auto& [id, node] : nodeMap) {
        if (node->isFloating) {
            m_floatingNodes.push_back(node);
        }
    }

    RecalculateBounds();

    if (OnLayoutChanged) OnLayoutChanged();
    return true;
}

void DockSpace::ClearLayout() {
    m_rootNode = CreateNode();
    m_rootNode->bounds = m_workArea;
    m_floatingNodes.clear();

    if (OnLayoutChanged) OnLayoutChanged();
}

// =========================================================================
// Preset Layouts
// =========================================================================

void DockSpace::CreateDefaultLayout() {
    ClearLayout();

    // Split root horizontally for left panel
    auto leftSplit = SplitNode(m_rootNode.get(), DockPosition::Left, 0.2f);

    // The right side is now m_rootNode->secondChild
    // Split it horizontally for right panel
    auto rightArea = m_rootNode->secondChild;
    if (rightArea && rightArea->IsLeaf()) {
        // Split for right panel
        auto rightNode = FindNodeById(rightArea->id);
        if (rightNode) {
            auto rightSplit = SplitNode(rightNode.get(), DockPosition::Right, 0.75f);
        }
    }

    // Now split the center area vertically for bottom panel
    auto centerArea = m_rootNode->secondChild ? m_rootNode->secondChild->firstChild : nullptr;
    if (centerArea && centerArea->IsLeaf()) {
        auto centerNode = FindNodeById(centerArea->id);
        if (centerNode) {
            SplitNode(centerNode.get(), DockPosition::Bottom, 0.7f);
        }
    }

    RecalculateBounds();
    if (OnLayoutChanged) OnLayoutChanged();
}

void DockSpace::CreateCompactLayout() {
    ClearLayout();
    // Single panel area - all panels as tabs in root
    RecalculateBounds();
    if (OnLayoutChanged) OnLayoutChanged();
}

void DockSpace::CreateWideLayout() {
    ClearLayout();

    // Split for narrow left panel
    SplitNode(m_rootNode.get(), DockPosition::Left, 0.15f);

    // Split for narrow right panel
    auto centerArea = m_rootNode->secondChild;
    if (centerArea && centerArea->IsLeaf()) {
        auto centerNode = FindNodeById(centerArea->id);
        if (centerNode) {
            SplitNode(centerNode.get(), DockPosition::Right, 0.85f);
        }
    }

    // Split for small bottom panel
    auto mainArea = m_rootNode->secondChild ? m_rootNode->secondChild->firstChild : nullptr;
    if (mainArea && mainArea->IsLeaf()) {
        auto mainNode = FindNodeById(mainArea->id);
        if (mainNode) {
            SplitNode(mainNode.get(), DockPosition::Bottom, 0.8f);
        }
    }

    RecalculateBounds();
    if (OnLayoutChanged) OnLayoutChanged();
}

// =========================================================================
// Internal Methods
// =========================================================================

uint64_t DockSpace::GenerateNodeId() {
    return m_nextNodeId++;
}

DockNode::Ptr DockSpace::CreateNode() {
    auto node = std::make_shared<DockNode>(GenerateNodeId());
    return node;
}

void DockSpace::RecalculateBounds() {
    if (m_rootNode) {
        RecalculateNodeBounds(m_rootNode, m_workArea);
    }
}

void DockSpace::RecalculateNodeBounds(DockNode::Ptr node, const DockRect& bounds) {
    if (!node) return;

    node->bounds = bounds;

    if (node->IsSplit()) {
        node->CalculateChildBounds();
        RecalculateNodeBounds(node->firstChild, node->firstChild->bounds);
        RecalculateNodeBounds(node->secondChild, node->secondChild->bounds);
    }
}

void DockSpace::CollectDropZones(DockNode::Ptr node, const glm::vec2& mousePos,
                                  std::vector<DockDropZone>& zones) {
    if (!node) return;

    if (node->IsLeaf() && !node->IsEmpty()) {
        const float zoneSize = m_config.dropZoneSize;
        const DockRect& bounds = node->bounds;

        // Left zone
        DockDropZone leftZone;
        leftZone.bounds = DockRect(bounds.x, bounds.y, zoneSize, bounds.height);
        leftZone.position = DockPosition::Left;
        leftZone.targetNode = node;
        leftZone.isValid = bounds.Contains(mousePos);
        zones.push_back(leftZone);

        // Right zone
        DockDropZone rightZone;
        rightZone.bounds = DockRect(bounds.x + bounds.width - zoneSize, bounds.y, zoneSize, bounds.height);
        rightZone.position = DockPosition::Right;
        rightZone.targetNode = node;
        rightZone.isValid = bounds.Contains(mousePos);
        zones.push_back(rightZone);

        // Top zone
        DockDropZone topZone;
        topZone.bounds = DockRect(bounds.x + zoneSize, bounds.y, bounds.width - zoneSize * 2, zoneSize);
        topZone.position = DockPosition::Top;
        topZone.targetNode = node;
        topZone.isValid = bounds.Contains(mousePos);
        zones.push_back(topZone);

        // Bottom zone
        DockDropZone bottomZone;
        bottomZone.bounds = DockRect(bounds.x + zoneSize, bounds.y + bounds.height - zoneSize,
                                     bounds.width - zoneSize * 2, zoneSize);
        bottomZone.position = DockPosition::Bottom;
        bottomZone.targetNode = node;
        bottomZone.isValid = bounds.Contains(mousePos);
        zones.push_back(bottomZone);

        // Center zone (tab)
        DockDropZone centerZone;
        float centerInset = zoneSize;
        centerZone.bounds = DockRect(bounds.x + centerInset, bounds.y + centerInset,
                                     bounds.width - centerInset * 2, bounds.height - centerInset * 2);
        centerZone.position = DockPosition::Center;
        centerZone.targetNode = node;
        centerZone.isValid = bounds.Contains(mousePos);
        zones.push_back(centerZone);
    } else if (node->IsSplit()) {
        CollectDropZones(node->firstChild, mousePos, zones);
        CollectDropZones(node->secondChild, mousePos, zones);
    }
}

DockDropZone DockSpace::FindBestDropZone(const glm::vec2& mousePos) {
    std::vector<DockDropZone> zones;
    CollectDropZones(m_rootNode, mousePos, zones);

    // Also add root drop zones if root is empty
    if (m_rootNode && m_rootNode->IsLeaf() && m_rootNode->IsEmpty()) {
        DockDropZone centerZone;
        centerZone.bounds = m_rootNode->bounds;
        centerZone.position = DockPosition::Center;
        centerZone.targetNode = m_rootNode;
        centerZone.isValid = true;
        zones.push_back(centerZone);
    }

    // Find the zone containing the mouse position
    for (auto& zone : zones) {
        if (zone.bounds.Contains(mousePos) && zone.isValid) {
            // Calculate preview bounds based on position
            const DockRect& targetBounds = zone.targetNode->bounds;
            switch (zone.position) {
                case DockPosition::Left:
                    zone.bounds = targetBounds.GetLeftHalf(0.5f);
                    break;
                case DockPosition::Right:
                    zone.bounds = targetBounds.GetRightHalf(0.5f);
                    break;
                case DockPosition::Top:
                    zone.bounds = targetBounds.GetTopHalf(0.5f);
                    break;
                case DockPosition::Bottom:
                    zone.bounds = targetBounds.GetBottomHalf(0.5f);
                    break;
                case DockPosition::Center:
                    zone.bounds = targetBounds;
                    break;
                default:
                    break;
            }
            return zone;
        }
    }

    // No valid zone found
    DockDropZone invalid;
    invalid.isValid = false;
    return invalid;
}

void DockSpace::RemoveEmptyNodes() {
    std::function<void(DockNode::Ptr)> removeEmpty;
    removeEmpty = [&](DockNode::Ptr node) {
        if (!node) return;

        if (node->IsSplit()) {
            removeEmpty(node->firstChild);
            removeEmpty(node->secondChild);

            // Check if children became empty
            bool firstEmpty = node->firstChild && node->firstChild->IsLeaf() && node->firstChild->IsEmpty();
            bool secondEmpty = node->secondChild && node->secondChild->IsLeaf() && node->secondChild->IsEmpty();

            if (firstEmpty && secondEmpty) {
                // Both children empty - make this a leaf
                node->splitDirection = SplitDirection::None;
                node->firstChild.reset();
                node->secondChild.reset();
            } else if (firstEmpty) {
                // Replace this node with second child's content
                CollapseEmptyNode(node);
            } else if (secondEmpty) {
                // Replace this node with first child's content
                CollapseEmptyNode(node);
            }
        }
    };

    removeEmpty(m_rootNode);
}

void DockSpace::CollapseEmptyNode(DockNode::Ptr node) {
    if (!node || !node->IsSplit()) return;

    bool firstEmpty = node->firstChild && node->firstChild->IsLeaf() && node->firstChild->IsEmpty();
    DockNode::Ptr keepChild = firstEmpty ? node->secondChild : node->firstChild;

    if (!keepChild) return;

    // Copy keep child's properties to this node
    node->splitDirection = keepChild->splitDirection;
    node->splitRatio = keepChild->splitRatio;
    node->panels = keepChild->panels;
    node->activeTabIndex = keepChild->activeTabIndex;

    if (keepChild->IsSplit()) {
        node->firstChild = keepChild->firstChild;
        node->secondChild = keepChild->secondChild;

        if (node->firstChild) node->firstChild->parent = node;
        if (node->secondChild) node->secondChild->parent = node;
    } else {
        node->firstChild.reset();
        node->secondChild.reset();
    }
}

DockNode::Ptr DockSpace::DockPanelToNode(EditorPanel* panel, DockNode::Ptr targetNode,
                                          DockPosition position) {
    if (!panel || !targetNode) return nullptr;

    // Center position means add as tab
    if (position == DockPosition::Center) {
        targetNode->AddPanel(panel);
        return targetNode;
    }

    // Need to split the target node
    if (targetNode->IsLeaf()) {
        // Split and add panel to new child
        auto firstChild = CreateNode();
        auto secondChild = CreateNode();

        // Move existing panels to appropriate child
        bool newPanelFirst = (position == DockPosition::Left || position == DockPosition::Top);

        if (newPanelFirst) {
            firstChild->AddPanel(panel);
            secondChild->panels = std::move(targetNode->panels);
            secondChild->activeTabIndex = targetNode->activeTabIndex;
        } else {
            firstChild->panels = std::move(targetNode->panels);
            firstChild->activeTabIndex = targetNode->activeTabIndex;
            secondChild->AddPanel(panel);
        }

        // Set up split
        targetNode->splitDirection = (position == DockPosition::Left || position == DockPosition::Right)
                                        ? SplitDirection::Horizontal
                                        : SplitDirection::Vertical;
        targetNode->splitRatio = 0.5f;
        targetNode->panels.clear();
        targetNode->firstChild = firstChild;
        targetNode->secondChild = secondChild;
        firstChild->parent = targetNode;
        secondChild->parent = targetNode;

        return newPanelFirst ? firstChild : secondChild;
    }

    return nullptr;
}

void DockSpace::SerializeNode(const DockNode::Ptr& node, DockLayout& layout) const {
    if (!node) return;

    DockLayout::NodeLayout nodeLayout;
    nodeLayout.id = node->id;
    nodeLayout.parentId = node->GetParent() ? node->GetParent()->id : 0;
    nodeLayout.splitDirection = node->splitDirection;
    nodeLayout.splitRatio = node->splitRatio;
    nodeLayout.activeTabIndex = node->activeTabIndex;
    nodeLayout.isFloating = node->isFloating;
    nodeLayout.floatingPos = node->floatingPos;
    nodeLayout.floatingSize = node->floatingSize;
    nodeLayout.bounds = node->bounds;

    for (auto* panel : node->panels) {
        nodeLayout.panelIds.push_back(panel->GetID());
    }

    layout.nodes.push_back(nodeLayout);

    if (node->firstChild) SerializeNode(node->firstChild, layout);
    if (node->secondChild) SerializeNode(node->secondChild, layout);
}

DockNode::Ptr DockSpace::DeserializeNode(const DockLayout::NodeLayout& nodeLayout,
                                          const std::unordered_map<std::string, EditorPanel*>& panelMap) {
    auto node = std::make_shared<DockNode>(nodeLayout.id);
    node->splitDirection = nodeLayout.splitDirection;
    node->splitRatio = nodeLayout.splitRatio;
    node->activeTabIndex = nodeLayout.activeTabIndex;
    node->isFloating = nodeLayout.isFloating;
    node->floatingPos = nodeLayout.floatingPos;
    node->floatingSize = nodeLayout.floatingSize;
    node->bounds = nodeLayout.bounds;

    for (const auto& panelId : nodeLayout.panelIds) {
        auto it = panelMap.find(panelId);
        if (it != panelMap.end()) {
            node->AddPanel(it->second);
        }
    }

    return node;
}

} // namespace Nova
