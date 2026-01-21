#include "VisualScriptEditor.hpp"
#include "StandardNodes.hpp"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstring>
#include <unordered_set>
#include <cfloat>

namespace Nova {
namespace VisualScript {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {
    ImVec2 ToImVec2(const glm::vec2& v) { return ImVec2(v.x, v.y); }
    glm::vec2 ToGlm(const ImVec2& v) { return glm::vec2(v.x, v.y); }

    ImVec2 operator+(const ImVec2& a, const ImVec2& b) {
        return ImVec2(a.x + b.x, a.y + b.y);
    }

    ImVec2 operator-(const ImVec2& a, const ImVec2& b) {
        return ImVec2(a.x - b.x, a.y - b.y);
    }

    ImVec2 operator*(const ImVec2& v, float s) {
        return ImVec2(v.x * s, v.y * s);
    }

    // Bezier curve helper for connections
    void DrawBezierCurve(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, ImU32 color, float thickness) {
        float distance = std::abs(p2.x - p1.x);
        float offset = std::min(distance * 0.5f, 100.0f);

        // Add minimum offset for vertical connections
        if (offset < 25.0f) offset = 25.0f;

        ImVec2 cp1 = ImVec2(p1.x + offset, p1.y);
        ImVec2 cp2 = ImVec2(p2.x - offset, p2.y);

        drawList->AddBezierCubic(p1, cp1, cp2, p2, color, thickness);
    }

    // Convert glm color to ImU32
    ImU32 GlmColorToImU32(const glm::vec4& color) {
        return IM_COL32(
            static_cast<int>(color.r * 255),
            static_cast<int>(color.g * 255),
            static_cast<int>(color.b * 255),
            static_cast<int>(color.a * 255)
        );
    }
}

// =============================================================================
// VisualScriptEditor Implementation
// =============================================================================

VisualScriptEditor::VisualScriptEditor() {
    NewGraph("Untitled");
}

void VisualScriptEditor::Render() {
    // Main layout with docking
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    ImGui::Begin("Visual Script Editor", nullptr, windowFlags);

    // Handle keyboard shortcuts first
    HandleKeyboardShortcuts();

    RenderMenuBar();

    // Create layout regions
    float leftPanelWidth = 250.0f;
    float rightPanelWidth = 300.0f;
    float bottomPanelHeight = 150.0f;

    ImVec2 availSize = ImGui::GetContentRegionAvail();

    // Left panel - Node Palette
    if (m_showNodePalette) {
        ImGui::BeginChild("NodePalette", ImVec2(leftPanelWidth, availSize.y - bottomPanelHeight), true);
        RenderNodePalette();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center - Canvas
    float canvasWidth = availSize.x - (m_showNodePalette ? leftPanelWidth + 8 : 0)
                       - (m_showPropertyInspector ? rightPanelWidth + 8 : 0);

    ImGui::BeginChild("Canvas", ImVec2(canvasWidth, availSize.y - bottomPanelHeight),
                     true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderCanvas();
    ImGui::EndChild();

    // Right panel - Property Inspector and Binding Browser
    if (m_showPropertyInspector || m_showBindingBrowser) {
        ImGui::SameLine();
        ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, availSize.y - bottomPanelHeight), true);

        if (m_showPropertyInspector) {
            if (ImGui::CollapsingHeader("Property Inspector", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderPropertyInspector();
            }
        }

        if (m_showBindingBrowser) {
            if (ImGui::CollapsingHeader("Binding Browser", ImGuiTreeNodeFlags_DefaultOpen)) {
                RenderBindingBrowser();
            }
        }

        // Variables panel
        if (ImGui::CollapsingHeader("Variables")) {
            RenderVariablesPanel();
        }

        ImGui::EndChild();
    }

    // Bottom panel - Warnings
    if (m_showWarnings) {
        ImGui::BeginChild("Warnings", ImVec2(0, bottomPanelHeight), true);
        RenderWarningsPanel();
        ImGui::EndChild();
    }

    // Context menu
    if (m_showContextMenu) {
        RenderContextMenu();
    }

    ImGui::End();
}

void VisualScriptEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                NewGraph();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // Would trigger file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_currentFilepath.empty()) {
                    SaveGraph(m_currentFilepath);
                }
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                // Would trigger file dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Close")) {
                m_graph = nullptr;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                SelectAll();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedNodes.empty())) {
                CopySelected();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
                ImVec2 canvasCenter = ImGui::GetWindowSize() * 0.5f;
                glm::vec2 pastePos = ToGlm(canvasCenter - m_canvasOffset) / m_canvasZoom;
                PasteAtPosition(pastePos);
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, !m_selectedNodes.empty())) {
                DuplicateSelected();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del", false, !m_selectedNodes.empty())) {
                DeleteSelected();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showNodePalette);
            ImGui::MenuItem("Property Inspector", nullptr, &m_showPropertyInspector);
            ImGui::MenuItem("Binding Browser", nullptr, &m_showBindingBrowser);
            ImGui::MenuItem("Warnings", nullptr, &m_showWarnings);
            ImGui::Separator();
            if (ImGui::MenuItem("Frame Selected", "F", false, !m_selectedNodes.empty())) {
                FrameSelected();
            }
            if (ImGui::MenuItem("Frame All", "Shift+F")) {
                FrameAll();
            }
            if (ImGui::MenuItem("Reset View", "Home")) {
                m_canvasOffset = ImVec2(0, 0);
                m_canvasZoom = 1.0f;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Graph")) {
            if (ImGui::MenuItem("Validate")) {
                if (m_graph) {
                    std::vector<std::string> errors;
                    m_graph->Validate(errors);
                }
            }
            if (ImGui::MenuItem("Refresh Bindings")) {
                if (m_graph) {
                    m_graph->UpdateBindingStates(BindingRegistry::Instance());
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void VisualScriptEditor::RenderNodePalette() {
    ImGui::Text("Nodes");
    ImGui::Separator();

    // Search bar
    ImGui::SetNextItemWidth(-1);
    bool searchChanged = ImGui::InputTextWithHint("##NodeSearch", "Search nodes...",
        m_nodeSearchBuffer, sizeof(m_nodeSearchBuffer));

    ImGui::Spacing();

    auto& factory = NodeFactory::Instance();

    // If searching, show filtered results
    if (m_nodeSearchBuffer[0] != '\0') {
        auto results = factory.SearchNodes(m_nodeSearchBuffer);
        for (const auto& info : results) {
            if (ImGui::Selectable(info.displayName.c_str())) {
                ImVec2 canvasCenter = ImGui::GetWindowSize() * 0.5f;
                CreateNodeAtPosition(info.typeId, ToGlm(canvasCenter - m_canvasOffset));
            }
            if (ImGui::IsItemHovered() && !info.description.empty()) {
                ImGui::SetTooltip("%s", info.description.c_str());
            }
        }
    } else {
        // Show by category
        const char* categories[] = {
            "Binding", "Event", "Asset", "Flow", "Math", "Logic", "Data",
            "Material", "Animation", "AI", "Audio", "Physics", "Custom"
        };
        NodeCategory catEnums[] = {
            NodeCategory::Binding, NodeCategory::Event, NodeCategory::Asset,
            NodeCategory::Flow, NodeCategory::Math, NodeCategory::Logic,
            NodeCategory::Data, NodeCategory::Material, NodeCategory::Animation,
            NodeCategory::AI, NodeCategory::Audio, NodeCategory::Physics,
            NodeCategory::Custom
        };

        for (int i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i) {
            auto types = factory.GetNodeTypesByCategory(catEnums[i]);
            if (types.empty()) continue;

            if (ImGui::TreeNode(categories[i])) {
                for (const auto& typeId : types) {
                    auto* info = factory.GetNodeInfo(typeId);
                    if (info) {
                        if (ImGui::Selectable(info->displayName.c_str())) {
                            ImVec2 canvasCenter = ImGui::GetWindowSize() * 0.5f;
                            CreateNodeAtPosition(typeId, ToGlm(canvasCenter - m_canvasOffset));
                        }
                        if (ImGui::IsItemHovered() && !info->description.empty()) {
                            ImGui::SetTooltip("%s", info->description.c_str());
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
    }
}

void VisualScriptEditor::RenderCanvas() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(canvasPos, canvasPos + canvasSize, IM_COL32(30, 30, 30, 255));

    // Grid
    float gridStep = m_style.gridSize * m_canvasZoom;
    for (float x = fmodf(m_canvasOffset.x, gridStep); x < canvasSize.x; x += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
            IM_COL32(50, 50, 50, 255)
        );
    }
    for (float y = fmodf(m_canvasOffset.y, gridStep); y < canvasSize.y; y += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
            IM_COL32(50, 50, 50, 255)
        );
    }

    // Clip to canvas
    drawList->PushClipRect(canvasPos, canvasPos + canvasSize, true);

    // Render connections
    RenderConnections();

    // Render pending connection
    if (m_isDraggingConnection) {
        RenderPendingConnection();
    }

    // Render nodes
    if (m_graph) {
        for (const auto& node : m_graph->GetNodes()) {
            RenderNode(node);
        }
    }

    drawList->PopClipRect();

    // Render box selection overlay
    RenderBoxSelection();

    // Handle canvas interactions
    if (ImGui::IsWindowHovered()) {
        // Pan with middle mouse or right mouse (when not showing context menu)
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
            (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !m_showContextMenu)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_canvasOffset = m_canvasOffset + delta;
        }

        // Zoom with scroll wheel (zoom towards mouse position)
        float scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 mouseCanvasPos = mousePos - canvasPos - m_canvasOffset;

            float oldZoom = m_canvasZoom;
            m_canvasZoom *= (scroll > 0) ? 1.1f : 0.9f;
            m_canvasZoom = std::clamp(m_canvasZoom, 0.25f, 4.0f);

            // Adjust offset to keep mouse position stable during zoom
            float zoomRatio = m_canvasZoom / oldZoom;
            m_canvasOffset = m_canvasOffset - mouseCanvasPos * (zoomRatio - 1.0f);
        }

        // Right-click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            m_showContextMenu = true;
            m_contextMenuPos = ImGui::GetMousePos();
        }

        // Box selection - start on left click in empty space
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            if (!ctrl) {
                ClearSelection();
            }
            m_isBoxSelecting = true;
            m_boxSelectStart = ImGui::GetMousePos();
            m_boxSelectEnd = m_boxSelectStart;
        }

        // Box selection - update while dragging
        if (m_isBoxSelecting && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            m_boxSelectEnd = ImGui::GetMousePos();
        }

        // Box selection - complete on mouse release
        if (m_isBoxSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_isBoxSelecting = false;

            // Calculate selection rectangle in canvas coordinates
            ImVec2 minPt = ImVec2(std::min(m_boxSelectStart.x, m_boxSelectEnd.x),
                                  std::min(m_boxSelectStart.y, m_boxSelectEnd.y));
            ImVec2 maxPt = ImVec2(std::max(m_boxSelectStart.x, m_boxSelectEnd.x),
                                  std::max(m_boxSelectStart.y, m_boxSelectEnd.y));

            // Only process if the selection rectangle has some size
            if ((maxPt.x - minPt.x) > 5 || (maxPt.y - minPt.y) > 5) {
                // Select nodes within the rectangle
                if (m_graph) {
                    bool ctrl = ImGui::GetIO().KeyCtrl;
                    for (const auto& node : m_graph->GetNodes()) {
                        ImVec2 nodePos = canvasPos + m_canvasOffset +
                            ToImVec2(node->GetPosition()) * m_canvasZoom;
                        float nodeWidth = 180.0f * m_canvasZoom;
                        float nodeHeight = 100.0f * m_canvasZoom;  // Approximate

                        // Check if node overlaps with selection rectangle
                        if (nodePos.x < maxPt.x && nodePos.x + nodeWidth > minPt.x &&
                            nodePos.y < maxPt.y && nodePos.y + nodeHeight > minPt.y) {
                            SelectNode(node, ctrl);
                        }
                    }
                }
            }
        }
    }
}

void VisualScriptEditor::RenderNode(NodePtr node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Calculate node position on canvas
    ImVec2 nodePos = canvasPos + m_canvasOffset + ToImVec2(node->GetPosition()) * m_canvasZoom;

    // Node dimensions
    float nodeWidth = 180.0f * m_canvasZoom;
    float headerHeight = 25.0f * m_canvasZoom;
    float portSpacing = 22.0f * m_canvasZoom;

    size_t maxPorts = std::max(node->GetInputPorts().size(), node->GetOutputPorts().size());
    float nodeHeight = headerHeight + maxPorts * portSpacing + 10.0f * m_canvasZoom;

    ImVec2 nodeSize(nodeWidth, nodeHeight);

    // Background (check if node is in multi-select list)
    bool isSelected = IsNodeSelected(node);
    ImVec4 bgColor = isSelected ? m_style.nodeSelected : m_style.nodeBackground;
    drawList->AddRectFilled(nodePos, nodePos + nodeSize, ImGui::ColorConvertFloat4ToU32(bgColor),
                           m_style.nodeRounding * m_canvasZoom);

    // Header
    drawList->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeWidth, nodePos.y + headerHeight),
                           ImGui::ColorConvertFloat4ToU32(m_style.nodeHeader),
                           m_style.nodeRounding * m_canvasZoom, ImDrawFlags_RoundCornersTop);

    // Title
    ImGui::PushFont(nullptr);  // Would use a bold font here
    ImVec2 textPos = ImVec2(nodePos.x + 8 * m_canvasZoom, nodePos.y + 5 * m_canvasZoom);
    drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), node->GetDisplayName().c_str());
    ImGui::PopFont();

    // Border
    ImU32 borderColor = isSelected ? IM_COL32(100, 150, 255, 255) : IM_COL32(60, 60, 60, 255);
    drawList->AddRect(nodePos, nodePos + nodeSize, borderColor,
                     m_style.nodeRounding * m_canvasZoom, 0, isSelected ? 2.0f : 1.0f);

    // Render ports
    float inputY = headerHeight + 8 * m_canvasZoom;
    float outputY = headerHeight + 8 * m_canvasZoom;

    for (const auto& port : node->GetInputPorts()) {
        RenderPort(port, nodePos, inputY);
        inputY += portSpacing;
    }

    for (const auto& port : node->GetOutputPorts()) {
        ImVec2 portPos = ImVec2(nodePos.x + nodeWidth, nodePos.y + outputY);

        // Port circle
        ImU32 portColor = GetPortColor(port->GetType());
        if (port->GetType() == PortType::Binding) {
            portColor = GetBindingStateColor(port->GetBindingRef().state);
        }
        drawList->AddCircleFilled(portPos, m_style.portRadius * m_canvasZoom, portColor);

        // Port label (right-aligned)
        const char* label = port->GetDisplayName().c_str();
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(portPos.x - textSize.x - 12 * m_canvasZoom, portPos.y - textSize.y * 0.5f);
        drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), label);

        // Port interaction
        ImVec2 portMin = ImVec2(portPos.x - 10, portPos.y - 10);
        ImVec2 portMax = ImVec2(portPos.x + 10, portPos.y + 10);
        if (ImGui::IsMouseHoveringRect(portMin, portMax)) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (m_isDraggingConnection) {
                    EndConnection(port);
                } else {
                    BeginConnection(port);
                }
            }
        }

        outputY += portSpacing;
    }

    // Node interaction - selection and dragging
    ImVec2 nodeMin = nodePos;
    ImVec2 nodeMax = nodePos + nodeSize;
    if (ImGui::IsMouseHoveringRect(nodeMin, nodeMax)) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            bool ctrl = ImGui::GetIO().KeyCtrl;
            bool shift = ImGui::GetIO().KeyShift;

            if (ctrl || shift) {
                // Ctrl+click or Shift+click: toggle selection
                SelectNode(node, true);
            } else if (!IsNodeSelected(node)) {
                // Regular click on unselected node: clear and select only this one
                SelectNode(node, false);
            }
            // If already selected and no modifier, keep current selection (for dragging multiple)

            // Start multi-node drag
            m_isDraggingNodes = true;
            m_dragStartPositions.clear();
            for (const auto& selectedNode : m_selectedNodes) {
                m_dragStartPositions[selectedNode->GetId()] = selectedNode->GetPosition();
            }
        }

        // Drag selected nodes
        if (m_isDraggingNodes && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && IsNodeSelected(node)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            glm::vec2 deltaGlm = ToGlm(delta) / m_canvasZoom;

            // Move all selected nodes
            for (const auto& selectedNode : m_selectedNodes) {
                glm::vec2 newPos = selectedNode->GetPosition() + deltaGlm;
                selectedNode->SetPosition(newPos);
            }
            m_isDirty = true;
        }
    }

    // End drag on mouse release
    if (m_isDraggingNodes && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_isDraggingNodes = false;
        m_dragStartPositions.clear();
    }
}

void VisualScriptEditor::RenderPort(PortPtr port, const ImVec2& nodePos, float& yOffset) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 portPos = ImVec2(nodePos.x, nodePos.y + yOffset);

    // Port circle
    ImU32 portColor = GetPortColor(port->GetType());
    if (port->GetType() == PortType::Binding) {
        portColor = GetBindingStateColor(port->GetBindingRef().state);
    }
    drawList->AddCircleFilled(portPos, m_style.portRadius * m_canvasZoom, portColor);

    // Port label
    ImVec2 textPos = ImVec2(portPos.x + 12 * m_canvasZoom, portPos.y - ImGui::GetFontSize() * 0.5f);
    drawList->AddText(textPos, IM_COL32(200, 200, 200, 255), port->GetDisplayName().c_str());

    // Port interaction
    ImVec2 portMin = ImVec2(portPos.x - 10, portPos.y - 10);
    ImVec2 portMax = ImVec2(portPos.x + 10, portPos.y + 10);
    if (ImGui::IsMouseHoveringRect(portMin, portMax)) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (m_isDraggingConnection) {
                EndConnection(port);
            } else {
                BeginConnection(port);
            }
        }
    }
}

ImU32 VisualScriptEditor::GetPortColor(PortType type) {
    switch (type) {
        case PortType::Flow:
            return ImGui::ColorConvertFloat4ToU32(m_style.flowPortColor);
        case PortType::Data:
            return ImGui::ColorConvertFloat4ToU32(m_style.dataPortColor);
        case PortType::Event:
            return ImGui::ColorConvertFloat4ToU32(m_style.eventPortColor);
        case PortType::Binding:
            return ImGui::ColorConvertFloat4ToU32(m_style.bindingPortColor);
    }
    return IM_COL32(128, 128, 128, 255);
}

void VisualScriptEditor::RenderConnections() {
    if (!m_graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    for (const auto& conn : m_graph->GetConnections()) {
        auto sourcePort = conn->GetSource();
        auto targetPort = conn->GetTarget();
        if (!sourcePort || !targetPort) continue;

        auto sourceNode = sourcePort->GetOwner();
        auto targetNode = targetPort->GetOwner();
        if (!sourceNode || !targetNode) continue;

        // Calculate port positions (simplified - should match RenderNode logic)
        float nodeWidth = 180.0f * m_canvasZoom;
        ImVec2 sourceNodePos = canvasPos + m_canvasOffset +
            ToImVec2(sourceNode->GetPosition()) * m_canvasZoom;
        ImVec2 targetNodePos = canvasPos + m_canvasOffset +
            ToImVec2(targetNode->GetPosition()) * m_canvasZoom;

        // Find port indices
        int sourceIdx = 0, targetIdx = 0;
        for (int i = 0; i < static_cast<int>(sourceNode->GetOutputPorts().size()); ++i) {
            if (sourceNode->GetOutputPorts()[i] == sourcePort) {
                sourceIdx = i;
                break;
            }
        }
        for (int i = 0; i < static_cast<int>(targetNode->GetInputPorts().size()); ++i) {
            if (targetNode->GetInputPorts()[i] == targetPort) {
                targetIdx = i;
                break;
            }
        }

        float headerHeight = 25.0f * m_canvasZoom;
        float portSpacing = 22.0f * m_canvasZoom;

        ImVec2 p1 = ImVec2(
            sourceNodePos.x + nodeWidth,
            sourceNodePos.y + headerHeight + 8 * m_canvasZoom + sourceIdx * portSpacing
        );
        ImVec2 p2 = ImVec2(
            targetNodePos.x,
            targetNodePos.y + headerHeight + 8 * m_canvasZoom + targetIdx * portSpacing
        );

        ImU32 color = GlmColorToImU32(conn->GetColor());
        DrawBezierCurve(drawList, p1, p2, color, m_style.connectionThickness);
    }
}

void VisualScriptEditor::RenderPendingConnection() {
    if (!m_connectionStartPort) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Get start position from port
    auto node = m_connectionStartPort->GetOwner();
    if (!node) return;

    ImVec2 nodePos = canvasPos + m_canvasOffset + ToImVec2(node->GetPosition()) * m_canvasZoom;
    float nodeWidth = 180.0f * m_canvasZoom;
    float headerHeight = 25.0f * m_canvasZoom;
    float portSpacing = 22.0f * m_canvasZoom;

    // Find port index
    int portIdx = 0;
    bool isOutput = (m_connectionStartPort->GetDirection() == PortDirection::Output);
    if (isOutput) {
        for (int i = 0; i < static_cast<int>(node->GetOutputPorts().size()); ++i) {
            if (node->GetOutputPorts()[i] == m_connectionStartPort) {
                portIdx = i;
                break;
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(node->GetInputPorts().size()); ++i) {
            if (node->GetInputPorts()[i] == m_connectionStartPort) {
                portIdx = i;
                break;
            }
        }
    }

    ImVec2 startPos = ImVec2(
        nodePos.x + (isOutput ? nodeWidth : 0),
        nodePos.y + headerHeight + 8 * m_canvasZoom + portIdx * portSpacing
    );

    ImVec2 endPos = ImGui::GetMousePos();

    ImU32 color = GetPortColor(m_connectionStartPort->GetType());
    DrawBezierCurve(drawList, startPos, endPos, color, m_style.connectionThickness);
}

void VisualScriptEditor::RenderPropertyInspector() {
    if (!m_selectedNode) {
        ImGui::TextDisabled("No node selected");
        return;
    }

    ImGui::Text("Type: %s", m_selectedNode->GetTypeId().c_str());
    ImGui::Separator();

    // Display name
    static char nameBuffer[256];
    strncpy(nameBuffer, m_selectedNode->GetDisplayName().c_str(), sizeof(nameBuffer) - 1);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        m_selectedNode->SetDisplayName(nameBuffer);
        m_isDirty = true;
    }

    // Position
    glm::vec2 pos = m_selectedNode->GetPosition();
    if (ImGui::DragFloat2("Position", &pos.x)) {
        m_selectedNode->SetPosition(pos);
        m_isDirty = true;
    }

    ImGui::Separator();

    // Input ports with binding info
    if (ImGui::TreeNode("Input Ports")) {
        for (const auto& port : m_selectedNode->GetInputPorts()) {
            ImGui::PushID(port->GetName().c_str());

            ImGui::Text("%s", port->GetDisplayName().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", port->GetDataType().c_str());

            if (port->GetType() == PortType::Binding) {
                const auto& ref = port->GetBindingRef();
                RenderBindingState(ref);
            }

            ImGui::PopID();
        }
        ImGui::TreePop();
    }

    // Output ports
    if (ImGui::TreeNode("Output Ports")) {
        for (const auto& port : m_selectedNode->GetOutputPorts()) {
            ImGui::Text("%s", port->GetDisplayName().c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", port->GetDataType().c_str());
        }
        ImGui::TreePop();
    }
}

void VisualScriptEditor::RenderBindingBrowser() {
    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##BindingSearch", "Search bindings...",
        m_bindingSearchBuffer, sizeof(m_bindingSearchBuffer));

    ImGui::Spacing();

    auto& registry = BindingRegistry::Instance();

    std::vector<const BindableProperty*> properties;
    if (m_bindingSearchBuffer[0] != '\0') {
        properties = registry.Search(m_bindingSearchBuffer);
    } else {
        // Show by category
        auto categories = registry.GetCategories();
        for (const auto& cat : categories) {
            if (ImGui::TreeNode(cat.c_str())) {
                auto catProps = registry.GetByCategory(cat);
                for (const auto* prop : catProps) {
                    RenderBindableProperty(*prop);
                }
                ImGui::TreePop();
            }
        }
        return;
    }

    // Display search results
    for (const auto* prop : properties) {
        RenderBindableProperty(*prop);
    }
}

void VisualScriptEditor::RenderBindableProperty(const BindableProperty& prop) {
    ImGui::PushID(prop.id.c_str());

    // Icon based on binding state
    if (prop.isHardLinked) {
        ImGui::TextColored(m_style.hardBindingColor, "[H]");
    } else if (prop.isLooseLinked) {
        ImGui::TextColored(m_style.looseBindingColor, "[L]");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[?]");
    }
    ImGui::SameLine();

    if (ImGui::Selectable(prop.displayName.c_str())) {
        // Could create a GetProperty node with this binding
        if (m_selectedNode) {
            // Set binding on selected node if applicable
        }
    }

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("ID: %s", prop.id.c_str());
        ImGui::Text("Type: %s", prop.typeName.c_str());
        ImGui::Text("Source: %s", prop.sourceId.c_str());
        if (!prop.description.empty()) {
            ImGui::TextWrapped("%s", prop.description.c_str());
        }
        ImGui::EndTooltip();
    }

    ImGui::PopID();
}

void VisualScriptEditor::RenderWarningsPanel() {
    ImGui::Text("Warnings & Errors");
    ImGui::Separator();

    if (!m_graph) {
        ImGui::TextDisabled("No graph loaded");
        return;
    }

    // Get all bindings with issues
    auto looseBindings = m_graph->GetLooseBindings();
    auto brokenBindings = m_graph->GetBrokenBindings();

    if (brokenBindings.empty() && looseBindings.empty()) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "No issues found");
        return;
    }

    // Broken bindings (errors)
    for (const auto& ref : brokenBindings) {
        ImGui::TextColored(m_style.brokenBindingColor, "[ERROR]");
        ImGui::SameLine();
        ImGui::Text("%s: %s", ref.path.c_str(), ref.warningMessage.c_str());
    }

    // Loose bindings (warnings)
    for (const auto& ref : looseBindings) {
        ImGui::TextColored(m_style.looseBindingColor, "[WARNING]");
        ImGui::SameLine();
        ImGui::Text("%s: %s", ref.path.c_str(), ref.warningMessage.c_str());
    }
}

void VisualScriptEditor::RenderContextMenu() {
    if (ImGui::BeginPopup("CanvasContextMenu")) {
        ImGui::Text("Create Node");
        ImGui::Separator();

        auto& factory = NodeFactory::Instance();

        // Quick access to common nodes
        if (ImGui::MenuItem("Get Property")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("GetProperty", pos);
        }
        if (ImGui::MenuItem("Set Property")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("SetProperty", pos);
        }
        if (ImGui::MenuItem("On Property Changed")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("OnPropertyChanged", pos);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Asset Reference")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("AssetReference", pos);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Publish Event")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("PublishEvent", pos);
        }
        if (ImGui::MenuItem("Subscribe Event")) {
            glm::vec2 pos = ToGlm(m_contextMenuPos - ImGui::GetCursorScreenPos() - m_canvasOffset);
            CreateNodeAtPosition("SubscribeEvent", pos);
        }

        ImGui::EndPopup();
        m_showContextMenu = false;
    } else {
        ImGui::OpenPopup("CanvasContextMenu");
    }
}

void VisualScriptEditor::RenderBindingState(const BindingReference& ref) {
    ImVec4 color;
    const char* stateText;

    switch (ref.state) {
        case BindingState::HardBinding:
            color = m_style.hardBindingColor;
            stateText = "Hard Bound";
            break;
        case BindingState::LooseBinding:
            color = m_style.looseBindingColor;
            stateText = "Loose Bound";
            break;
        case BindingState::BrokenBinding:
            color = m_style.brokenBindingColor;
            stateText = "Broken";
            break;
        default:
            color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            stateText = "Unbound";
            break;
    }

    ImGui::Indent();
    ImGui::TextColored(color, "%s: %s", ref.path.c_str(), stateText);
    if (!ref.warningMessage.empty()) {
        ImGui::TextWrapped("%s", ref.warningMessage.c_str());
    }
    ImGui::Unindent();
}

ImU32 VisualScriptEditor::GetBindingStateColor(BindingState state) {
    switch (state) {
        case BindingState::HardBinding:
            return ImGui::ColorConvertFloat4ToU32(m_style.hardBindingColor);
        case BindingState::LooseBinding:
            return ImGui::ColorConvertFloat4ToU32(m_style.looseBindingColor);
        case BindingState::BrokenBinding:
            return ImGui::ColorConvertFloat4ToU32(m_style.brokenBindingColor);
        default:
            return IM_COL32(128, 128, 128, 255);
    }
}

void VisualScriptEditor::NewGraph(const std::string& name) {
    PushUndoState();
    m_graph = std::make_shared<Graph>(name);
    m_selectedNode = nullptr;
    m_currentFilepath.clear();
    m_isDirty = false;

    if (m_onGraphChanged) {
        m_onGraphChanged(m_graph);
    }
}

void VisualScriptEditor::LoadGraph(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return;

        nlohmann::json json;
        file >> json;

        PushUndoState();
        m_graph = Graph::Deserialize(json);
        m_graph->UpdateBindingStates(BindingRegistry::Instance());
        m_selectedNode = nullptr;
        m_currentFilepath = filepath;
        m_isDirty = false;

        if (m_onGraphChanged) {
            m_onGraphChanged(m_graph);
        }
    } catch (const std::exception& e) {
        // Handle error
    }
}

void VisualScriptEditor::SaveGraph(const std::string& filepath) {
    if (!m_graph) return;

    try {
        nlohmann::json json = m_graph->Serialize();

        std::ofstream file(filepath);
        file << json.dump(2);

        m_currentFilepath = filepath;
        m_isDirty = false;
    } catch (const std::exception& e) {
        // Handle error
    }
}

void VisualScriptEditor::SetGraph(GraphPtr graph) {
    PushUndoState();
    m_graph = graph;
    m_selectedNode = nullptr;

    if (m_graph) {
        m_graph->UpdateBindingStates(BindingRegistry::Instance());
    }

    if (m_onGraphChanged) {
        m_onGraphChanged(m_graph);
    }
}

void VisualScriptEditor::SelectNode(NodePtr node, bool addToSelection) {
    if (!node) {
        if (!addToSelection) {
            ClearSelection();
        }
        return;
    }

    if (addToSelection) {
        // Toggle selection if already selected
        auto it = std::find(m_selectedNodes.begin(), m_selectedNodes.end(), node);
        if (it != m_selectedNodes.end()) {
            m_selectedNodes.erase(it);
            m_selectedNode = m_selectedNodes.empty() ? nullptr : m_selectedNodes.back();
        } else {
            m_selectedNodes.push_back(node);
            m_selectedNode = node;
        }
    } else {
        m_selectedNodes.clear();
        m_selectedNodes.push_back(node);
        m_selectedNode = node;
    }
}

void VisualScriptEditor::ClearSelection() {
    m_selectedNode = nullptr;
    m_selectedNodes.clear();
}

void VisualScriptEditor::SelectAll() {
    if (!m_graph) return;
    m_selectedNodes = m_graph->GetNodes();
    m_selectedNode = m_selectedNodes.empty() ? nullptr : m_selectedNodes.back();
}

bool VisualScriptEditor::IsNodeSelected(NodePtr node) const {
    return std::find(m_selectedNodes.begin(), m_selectedNodes.end(), node) != m_selectedNodes.end();
}

void VisualScriptEditor::CreateNodeAtPosition(const std::string& typeId, const glm::vec2& position) {
    if (!m_graph) return;

    auto& factory = NodeFactory::Instance();
    auto node = factory.Create(typeId);
    if (node) {
        PushUndoState();
        node->SetPosition(position);
        m_graph->AddNode(node);
        SelectNode(node);
        m_isDirty = true;
    }
}

void VisualScriptEditor::BeginConnection(PortPtr port) {
    m_isDraggingConnection = true;
    m_connectionStartPort = port;
}

void VisualScriptEditor::EndConnection(PortPtr port) {
    if (!m_connectionStartPort || !port || !m_graph) {
        CancelConnection();
        return;
    }

    // Determine source and target
    PortPtr source = m_connectionStartPort;
    PortPtr target = port;
    if (source->GetDirection() == PortDirection::Input) {
        std::swap(source, target);
    }

    if (source->CanConnectTo(*target)) {
        PushUndoState();
        m_graph->Connect(source, target);
        m_isDirty = true;
    }

    CancelConnection();
}

void VisualScriptEditor::CancelConnection() {
    m_isDraggingConnection = false;
    m_connectionStartPort = nullptr;
}

void VisualScriptEditor::PushUndoState() {
    if (!m_graph) return;

    m_undoStack.push_back(m_graph->Serialize());
    if (m_undoStack.size() > m_maxUndoSteps) {
        m_undoStack.erase(m_undoStack.begin());
    }

    m_redoStack.clear();
}

void VisualScriptEditor::Undo() {
    if (m_undoStack.empty() || !m_graph) return;

    m_redoStack.push_back(m_graph->Serialize());
    m_graph = Graph::Deserialize(m_undoStack.back());
    m_undoStack.pop_back();
    m_selectedNode = nullptr;
    m_selectedNodes.clear();
}

void VisualScriptEditor::Redo() {
    if (m_redoStack.empty()) return;

    m_undoStack.push_back(m_graph->Serialize());
    m_graph = Graph::Deserialize(m_redoStack.back());
    m_redoStack.pop_back();
    m_selectedNode = nullptr;
    m_selectedNodes.clear();
}

// =============================================================================
// Copy/Paste/Delete Operations
// =============================================================================

void VisualScriptEditor::CopySelected() {
    if (m_selectedNodes.empty()) return;

    m_clipboard = nlohmann::json::object();
    m_clipboard["nodes"] = nlohmann::json::array();
    m_clipboard["connections"] = nlohmann::json::array();

    // Track copied node IDs for connection filtering
    std::unordered_set<std::string> copiedNodeIds;
    for (const auto& node : m_selectedNodes) {
        copiedNodeIds.insert(node->GetId());
        m_clipboard["nodes"].push_back(node->Serialize());
    }

    // Copy connections between selected nodes
    if (m_graph) {
        for (const auto& conn : m_graph->GetConnections()) {
            auto sourceNode = conn->GetSource()->GetOwner();
            auto targetNode = conn->GetTarget()->GetOwner();
            if (sourceNode && targetNode) {
                bool sourceSelected = copiedNodeIds.count(sourceNode->GetId()) > 0;
                bool targetSelected = copiedNodeIds.count(targetNode->GetId()) > 0;
                if (sourceSelected && targetSelected) {
                    nlohmann::json connJson;
                    connJson["sourceNode"] = sourceNode->GetId();
                    connJson["sourcePort"] = conn->GetSource()->GetName();
                    connJson["targetNode"] = targetNode->GetId();
                    connJson["targetPort"] = conn->GetTarget()->GetName();
                    m_clipboard["connections"].push_back(connJson);
                }
            }
        }
    }
}

void VisualScriptEditor::PasteAtPosition(const glm::vec2& position) {
    if (!m_graph || m_clipboard.empty() || !m_clipboard.contains("nodes")) return;

    PushUndoState();

    // Calculate offset from original positions
    glm::vec2 minPos(FLT_MAX, FLT_MAX);
    for (const auto& nodeJson : m_clipboard["nodes"]) {
        if (nodeJson.contains("position")) {
            auto pos = nodeJson["position"];
            minPos.x = std::min(minPos.x, pos[0].get<float>());
            minPos.y = std::min(minPos.y, pos[1].get<float>());
        }
    }
    glm::vec2 offset = position - minPos;

    // Map old IDs to new nodes
    std::unordered_map<std::string, NodePtr> idToNewNode;
    auto& factory = NodeFactory::Instance();

    m_selectedNodes.clear();

    for (const auto& nodeJson : m_clipboard["nodes"]) {
        std::string typeId = nodeJson.value("typeId", "");
        auto node = factory.Create(typeId);
        if (node) {
            node->Deserialize(nodeJson);

            // Generate new ID to avoid conflicts
            std::string oldId = node->GetId();

            // Apply position offset
            glm::vec2 newPos = node->GetPosition() + offset;
            node->SetPosition(newPos);

            m_graph->AddNode(node);
            idToNewNode[oldId] = node;
            m_selectedNodes.push_back(node);
        }
    }

    // Recreate connections between pasted nodes
    if (m_clipboard.contains("connections")) {
        for (const auto& connJson : m_clipboard["connections"]) {
            std::string sourceNodeId = connJson.value("sourceNode", "");
            std::string sourcePortName = connJson.value("sourcePort", "");
            std::string targetNodeId = connJson.value("targetNode", "");
            std::string targetPortName = connJson.value("targetPort", "");

            auto srcIt = idToNewNode.find(sourceNodeId);
            auto tgtIt = idToNewNode.find(targetNodeId);

            if (srcIt != idToNewNode.end() && tgtIt != idToNewNode.end()) {
                auto sourcePort = srcIt->second->GetOutputPort(sourcePortName);
                auto targetPort = tgtIt->second->GetInputPort(targetPortName);
                if (sourcePort && targetPort) {
                    m_graph->Connect(sourcePort, targetPort);
                }
            }
        }
    }

    m_selectedNode = m_selectedNodes.empty() ? nullptr : m_selectedNodes.back();
    m_isDirty = true;
}

void VisualScriptEditor::DuplicateSelected() {
    CopySelected();
    if (!m_selectedNodes.empty()) {
        // Offset duplicates slightly from originals
        glm::vec2 offset(30.0f, 30.0f);
        glm::vec2 firstNodePos = m_selectedNodes[0]->GetPosition() + offset;
        PasteAtPosition(firstNodePos);
    }
}

void VisualScriptEditor::DeleteSelected() {
    if (!m_graph || m_selectedNodes.empty()) return;

    PushUndoState();

    for (const auto& node : m_selectedNodes) {
        m_graph->RemoveNode(node);
    }

    ClearSelection();
    m_isDirty = true;
}

void VisualScriptEditor::FrameSelected() {
    if (m_selectedNodes.empty()) {
        FrameAll();
        return;
    }

    // Calculate bounding box of selected nodes
    glm::vec2 minPos(FLT_MAX, FLT_MAX);
    glm::vec2 maxPos(-FLT_MAX, -FLT_MAX);

    for (const auto& node : m_selectedNodes) {
        glm::vec2 pos = node->GetPosition();
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos + glm::vec2(180.0f, 100.0f));  // Node size estimate
    }

    // Center the view on the selection
    glm::vec2 center = (minPos + maxPos) * 0.5f;
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    m_canvasOffset = ImVec2(canvasSize.x * 0.5f - center.x * m_canvasZoom,
                            canvasSize.y * 0.5f - center.y * m_canvasZoom);
}

void VisualScriptEditor::FrameAll() {
    if (!m_graph || m_graph->GetNodes().empty()) return;

    // Calculate bounding box of all nodes
    glm::vec2 minPos(FLT_MAX, FLT_MAX);
    glm::vec2 maxPos(-FLT_MAX, -FLT_MAX);

    for (const auto& node : m_graph->GetNodes()) {
        glm::vec2 pos = node->GetPosition();
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos + glm::vec2(180.0f, 100.0f));
    }

    // Center the view and adjust zoom to fit
    glm::vec2 size = maxPos - minPos;
    glm::vec2 center = (minPos + maxPos) * 0.5f;

    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    float zoomX = canvasSize.x / (size.x + 100.0f);  // Add padding
    float zoomY = canvasSize.y / (size.y + 100.0f);
    m_canvasZoom = std::clamp(std::min(zoomX, zoomY), 0.25f, 2.0f);

    m_canvasOffset = ImVec2(canvasSize.x * 0.5f - center.x * m_canvasZoom,
                            canvasSize.y * 0.5f - center.y * m_canvasZoom);
}

// =============================================================================
// Keyboard Shortcuts
// =============================================================================

void VisualScriptEditor::HandleKeyboardShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    // Only handle shortcuts when the editor window is focused
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

    bool ctrl = io.KeyCtrl;
    bool shift = io.KeyShift;

    // File operations
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        NewGraph();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (shift || m_currentFilepath.empty()) {
            // Save As - would trigger file dialog
        } else {
            SaveGraph(m_currentFilepath);
        }
    }

    // Edit operations
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (shift) {
            Redo();
        } else {
            Undo();
        }
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        Redo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAll();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelected();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        // Paste at canvas center
        ImVec2 canvasCenter = ImGui::GetWindowSize() * 0.5f;
        glm::vec2 pastePos = ToGlm(canvasCenter - m_canvasOffset) / m_canvasZoom;
        PasteAtPosition(pastePos);
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        DuplicateSelected();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        DeleteSelected();
    }

    // View operations
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        if (shift) {
            FrameAll();
        } else {
            FrameSelected();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        m_canvasOffset = ImVec2(0, 0);
        m_canvasZoom = 1.0f;
    }

    // Cancel operations
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (m_isDraggingConnection) {
            CancelConnection();
        } else if (m_isBoxSelecting) {
            m_isBoxSelecting = false;
        } else {
            ClearSelection();
        }
    }
}

// =============================================================================
// Box Selection
// =============================================================================

void VisualScriptEditor::RenderBoxSelection() {
    if (!m_isBoxSelecting) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Calculate rectangle bounds
    ImVec2 minPt = ImVec2(std::min(m_boxSelectStart.x, m_boxSelectEnd.x),
                          std::min(m_boxSelectStart.y, m_boxSelectEnd.y));
    ImVec2 maxPt = ImVec2(std::max(m_boxSelectStart.x, m_boxSelectEnd.x),
                          std::max(m_boxSelectStart.y, m_boxSelectEnd.y));

    // Draw selection rectangle
    drawList->AddRectFilled(minPt, maxPt, IM_COL32(100, 150, 255, 50));
    drawList->AddRect(minPt, maxPt, IM_COL32(100, 150, 255, 200), 0, 0, 1.5f);
}

// =============================================================================
// Variables Panel
// =============================================================================

void VisualScriptEditor::RenderVariablesPanel() {
    if (!m_graph) {
        ImGui::TextDisabled("No graph loaded");
        return;
    }

    ImGui::Text("Graph Variables");
    ImGui::Separator();

    auto varNames = m_graph->GetVariableNames();

    if (varNames.empty()) {
        ImGui::TextDisabled("No variables defined");
    } else {
        for (const auto& name : varNames) {
            ImGui::PushID(name.c_str());

            if (ImGui::TreeNode(name.c_str())) {
                // Show variable info and allow creating Get/Set nodes
                if (ImGui::Button("Get")) {
                    auto node = NodeFactory::Instance().Create("GetVariable");
                    if (node) {
                        if (auto getVarNode = std::dynamic_pointer_cast<GetVariableNode>(node)) {
                            getVarNode->SetVariableName(name);
                        }
                        glm::vec2 pos = m_selectedNode ?
                            m_selectedNode->GetPosition() + glm::vec2(200, 0) :
                            glm::vec2(100, 100);
                        node->SetPosition(pos);
                        m_graph->AddNode(node);
                        m_isDirty = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Set")) {
                    auto node = NodeFactory::Instance().Create("SetVariable");
                    if (node) {
                        if (auto setVarNode = std::dynamic_pointer_cast<SetVariableNode>(node)) {
                            setVarNode->SetVariableName(name);
                        }
                        glm::vec2 pos = m_selectedNode ?
                            m_selectedNode->GetPosition() + glm::vec2(200, 0) :
                            glm::vec2(100, 100);
                        node->SetPosition(pos);
                        m_graph->AddNode(node);
                        m_isDirty = true;
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    ImGui::Separator();

    // Add new variable
    static char newVarName[128] = "";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
    ImGui::InputTextWithHint("##NewVar", "New variable name", newVarName, sizeof(newVarName));
    ImGui::SameLine();
    if (ImGui::Button("Add") && newVarName[0] != '\0') {
        m_graph->SetVariable(newVarName, std::any{});
        newVarName[0] = '\0';
        m_isDirty = true;
    }
}

// =============================================================================
// VisualScriptEditorWindow Implementation
// =============================================================================

VisualScriptEditorWindow::VisualScriptEditorWindow(const std::string& title)
    : m_title(title) {
}

void VisualScriptEditorWindow::Render() {
    if (!m_isOpen) return;

    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(m_title.c_str(), &m_isOpen)) {
        m_editor.Render();
    }
    ImGui::End();
}

} // namespace VisualScript
} // namespace Nova
