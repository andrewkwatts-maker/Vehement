#include "EntityEventEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>

namespace Vehement {

EntityEventEditor::EntityEventEditor() = default;
EntityEventEditor::~EntityEventEditor() = default;

void EntityEventEditor::Initialize(Editor* editor, const Config& config) {
    m_editor = editor;
    m_config = config;

    InitializeCategoryStyles();

    // Register builtin nodes
    Nova::EventNodeFactory::Instance().RegisterBuiltinNodes();

    m_initialized = true;
}

void EntityEventEditor::InitializeCategoryStyles() {
    m_categoryStyles = {
        {Nova::EventNodeCategory::EventTrigger, "Event Triggers", "E", {0.8f, 0.2f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::EventCustom, "Custom Events", "C", {0.8f, 0.4f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::FlowControl, "Flow Control", "F", {0.4f, 0.4f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityState, "Entity State", "S", {0.2f, 0.6f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityMesh, "Mesh", "M", {0.6f, 0.4f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::EntityAnimation, "Animation", "A", {0.8f, 0.6f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::EntityComponent, "Components", "K", {0.4f, 0.8f, 0.4f, 1.0f}},
        {Nova::EventNodeCategory::EntityMovement, "Movement", "V", {0.2f, 0.8f, 0.6f, 1.0f}},
        {Nova::EventNodeCategory::Combat, "Combat", "X", {0.9f, 0.3f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::World, "World", "W", {0.3f, 0.7f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::Terrain, "Terrain", "T", {0.5f, 0.4f, 0.3f, 1.0f}},
        {Nova::EventNodeCategory::Math, "Math", "+", {0.7f, 0.7f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::Logic, "Logic", "&", {0.5f, 0.5f, 0.7f, 1.0f}},
        {Nova::EventNodeCategory::Comparison, "Comparison", "=", {0.6f, 0.6f, 0.6f, 1.0f}},
        {Nova::EventNodeCategory::Variables, "Variables", "$", {0.3f, 0.6f, 0.9f, 1.0f}},
        {Nova::EventNodeCategory::Arrays, "Arrays", "[]", {0.4f, 0.5f, 0.7f, 1.0f}},
        {Nova::EventNodeCategory::Python, "Python", "Py", {0.3f, 0.5f, 0.8f, 1.0f}},
        {Nova::EventNodeCategory::Debug, "Debug", "D", {0.9f, 0.9f, 0.2f, 1.0f}},
        {Nova::EventNodeCategory::UI, "UI", "U", {0.6f, 0.3f, 0.6f, 1.0f}},
    };
}

// =========================================================================
// Graph Management
// =========================================================================

EntityEventGraph* EntityEventEditor::CreateGraph(const std::string& name, const std::string& entityType, const std::string& entityId) {
    auto graph = std::make_unique<EntityEventGraph>();
    graph->name = name;
    graph->entityType = entityType;
    graph->entityId = entityId;

    EntityEventGraph* ptr = graph.get();
    m_graphs.push_back(std::move(graph));
    m_currentGraph = ptr;

    return ptr;
}

bool EntityEventEditor::LoadGraph(const std::string& path) {
    // TODO: Implement JSON loading
    return false;
}

bool EntityEventEditor::SaveGraph(const std::string& path) {
    if (!m_currentGraph) return false;
    // TODO: Implement JSON saving
    m_currentGraph->modified = false;
    return true;
}

void EntityEventEditor::SetCurrentGraph(EntityEventGraph* graph) {
    m_currentGraph = graph;
    ClearSelection();
    ResetView();
}

void EntityEventEditor::CloseGraph(EntityEventGraph* graph) {
    auto it = std::find_if(m_graphs.begin(), m_graphs.end(),
        [graph](const auto& g) { return g.get() == graph; });

    if (it != m_graphs.end()) {
        if (m_currentGraph == graph) {
            m_currentGraph = nullptr;
        }
        m_graphs.erase(it);
    }
}

// =========================================================================
// Node Operations
// =========================================================================

Nova::EventNode::Ptr EntityEventEditor::AddNode(const std::string& typeName, const glm::vec2& position) {
    if (!m_currentGraph) return nullptr;

    auto node = Nova::EventNodeFactory::Instance().Create(typeName);
    if (!node) return nullptr;

    m_currentGraph->graph.AddNode(node);

    glm::vec2 snappedPos = m_config.snapToGrid ? SnapToGrid(position) : position;
    CreateNodeVisual(node->GetId(), snappedPos);

    m_currentGraph->modified = true;

    if (OnNodeAdded) OnNodeAdded(node);
    if (OnGraphModified) OnGraphModified();

    return node;
}

void EntityEventEditor::RemoveSelectedNodes() {
    if (!m_currentGraph) return;

    for (uint64_t nodeId : m_selectedNodes) {
        m_currentGraph->graph.RemoveNode(nodeId);

        // Remove visual
        auto& visuals = m_currentGraph->nodeVisuals;
        visuals.erase(std::remove_if(visuals.begin(), visuals.end(),
            [nodeId](const EventNodeVisual& v) { return v.nodeId == nodeId; }),
            visuals.end());

        if (OnNodeRemoved) OnNodeRemoved(nodeId);
    }

    m_selectedNodes.clear();
    m_currentGraph->modified = true;

    if (OnSelectionChanged) OnSelectionChanged();
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::DuplicateSelectedNodes() {
    if (!m_currentGraph || m_selectedNodes.empty()) return;

    std::vector<Nova::EventNode::Ptr> newNodes;
    glm::vec2 offset{50.0f, 50.0f};

    for (uint64_t nodeId : m_selectedNodes) {
        auto* node = m_currentGraph->graph.GetNode(nodeId);
        auto* visual = GetNodeVisual(nodeId);
        if (node && visual) {
            auto newNode = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
            if (newNode) {
                m_currentGraph->graph.AddNode(newNode);
                CreateNodeVisual(newNode->GetId(), visual->position + offset);
                newNodes.push_back(newNode);
            }
        }
    }

    // Select new nodes
    ClearSelection();
    for (auto& node : newNodes) {
        SelectNode(node->GetId(), true);
    }

    m_currentGraph->modified = true;
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::CopySelectedNodes() {
    m_clipboard.clear();

    if (!m_currentGraph) return;

    for (uint64_t nodeId : m_selectedNodes) {
        auto* node = m_currentGraph->graph.GetNode(nodeId);
        auto* visual = GetNodeVisual(nodeId);
        if (node && visual) {
            auto copy = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
            if (copy) {
                m_clipboard.emplace_back(copy, visual->position);
            }
        }
    }
}

void EntityEventEditor::PasteNodes(const glm::vec2& position) {
    if (!m_currentGraph || m_clipboard.empty()) return;

    // Calculate center of clipboard nodes
    glm::vec2 center{0.0f};
    for (const auto& [node, pos] : m_clipboard) {
        center += pos;
    }
    center /= static_cast<float>(m_clipboard.size());

    // Paste with offset from center to target position
    ClearSelection();
    for (const auto& [node, pos] : m_clipboard) {
        auto newNode = Nova::EventNodeFactory::Instance().Create(node->GetTypeName());
        if (newNode) {
            m_currentGraph->graph.AddNode(newNode);
            glm::vec2 newPos = position + (pos - center);
            CreateNodeVisual(newNode->GetId(), newPos);
            SelectNode(newNode->GetId(), true);
        }
    }

    m_currentGraph->modified = true;
    if (OnGraphModified) OnGraphModified();
}

void EntityEventEditor::CutSelectedNodes() {
    CopySelectedNodes();
    RemoveSelectedNodes();
}

// =========================================================================
// Selection
// =========================================================================

void EntityEventEditor::SelectNode(uint64_t nodeId, bool addToSelection) {
    if (!addToSelection) {
        for (uint64_t id : m_selectedNodes) {
            if (auto* visual = GetNodeVisual(id)) {
                visual->selected = false;
            }
        }
        m_selectedNodes.clear();
    }

    m_selectedNodes.insert(nodeId);
    if (auto* visual = GetNodeVisual(nodeId)) {
        visual->selected = true;
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::DeselectNode(uint64_t nodeId) {
    m_selectedNodes.erase(nodeId);
    if (auto* visual = GetNodeVisual(nodeId)) {
        visual->selected = false;
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::ClearSelection() {
    for (uint64_t id : m_selectedNodes) {
        if (auto* visual = GetNodeVisual(id)) {
            visual->selected = false;
        }
    }
    m_selectedNodes.clear();

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::SelectAllNodes() {
    if (!m_currentGraph) return;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        visual.selected = true;
        m_selectedNodes.insert(visual.nodeId);
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

void EntityEventEditor::BoxSelectNodes(const glm::vec2& start, const glm::vec2& end) {
    if (!m_currentGraph) return;

    glm::vec2 minPos = glm::min(start, end);
    glm::vec2 maxPos = glm::max(start, end);

    for (auto& visual : m_currentGraph->nodeVisuals) {
        bool inBox = visual.position.x + visual.size.x >= minPos.x &&
                     visual.position.x <= maxPos.x &&
                     visual.position.y + visual.size.y >= minPos.y &&
                     visual.position.y <= maxPos.y;

        if (inBox) {
            visual.selected = true;
            m_selectedNodes.insert(visual.nodeId);
        }
    }

    if (OnSelectionChanged) OnSelectionChanged();
}

// =========================================================================
// Connections
// =========================================================================

void EntityEventEditor::StartConnection(uint64_t nodeId, const std::string& pinName, bool isOutput) {
    m_isConnecting = true;
    m_connectionStartNode = nodeId;
    m_connectionStartPin = pinName;
    m_connectionStartIsOutput = isOutput;
}

bool EntityEventEditor::CompleteConnection(uint64_t nodeId, const std::string& pinName) {
    if (!m_isConnecting || !m_currentGraph) {
        CancelConnection();
        return false;
    }

    // Validate connection direction
    if (m_connectionStartIsOutput) {
        // Output to input
        if (m_currentGraph->graph.Connect(m_connectionStartNode, m_connectionStartPin, nodeId, pinName)) {
            m_currentGraph->modified = true;
            if (OnGraphModified) OnGraphModified();
            CancelConnection();
            return true;
        }
    } else {
        // Input to output (reversed)
        if (m_currentGraph->graph.Connect(nodeId, pinName, m_connectionStartNode, m_connectionStartPin)) {
            m_currentGraph->modified = true;
            if (OnGraphModified) OnGraphModified();
            CancelConnection();
            return true;
        }
    }

    CancelConnection();
    return false;
}

void EntityEventEditor::CancelConnection() {
    m_isConnecting = false;
    m_connectionStartNode = 0;
    m_connectionStartPin.clear();
}

void EntityEventEditor::RemoveConnection(uint64_t fromNode, const std::string& fromPin, uint64_t toNode, const std::string& toPin) {
    if (!m_currentGraph) return;

    m_currentGraph->graph.Disconnect(fromNode, fromPin, toNode, toPin);
    m_currentGraph->modified = true;

    if (OnGraphModified) OnGraphModified();
}

// =========================================================================
// View Control
// =========================================================================

void EntityEventEditor::Pan(const glm::vec2& delta) {
    m_viewOffset += delta;
}

void EntityEventEditor::Zoom(float delta, const glm::vec2& center) {
    float oldScale = m_viewScale;
    m_viewScale = std::clamp(m_viewScale + delta * 0.1f, 0.1f, 3.0f);

    // Zoom toward center
    float scaleFactor = m_viewScale / oldScale;
    m_viewOffset = center - (center - m_viewOffset) * scaleFactor;
}

void EntityEventEditor::ResetView() {
    m_viewOffset = glm::vec2{0.0f};
    m_viewScale = 1.0f;
}

void EntityEventEditor::FrameAll() {
    if (!m_currentGraph || m_currentGraph->nodeVisuals.empty()) return;

    glm::vec2 minPos{std::numeric_limits<float>::max()};
    glm::vec2 maxPos{std::numeric_limits<float>::lowest()};

    for (const auto& visual : m_currentGraph->nodeVisuals) {
        minPos = glm::min(minPos, visual.position);
        maxPos = glm::max(maxPos, visual.position + visual.size);
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    glm::vec2 size = maxPos - minPos;

    m_viewScale = std::min(m_canvasSize.x / (size.x + 100.0f), m_canvasSize.y / (size.y + 100.0f));
    m_viewScale = std::clamp(m_viewScale, 0.1f, 1.0f);
    m_viewOffset = m_canvasSize * 0.5f - center * m_viewScale;
}

void EntityEventEditor::FrameSelected() {
    if (!m_currentGraph || m_selectedNodes.empty()) return;

    glm::vec2 minPos{std::numeric_limits<float>::max()};
    glm::vec2 maxPos{std::numeric_limits<float>::lowest()};

    for (uint64_t nodeId : m_selectedNodes) {
        if (auto* visual = GetNodeVisual(nodeId)) {
            minPos = glm::min(minPos, visual->position);
            maxPos = glm::max(maxPos, visual->position + visual->size);
        }
    }

    glm::vec2 center = (minPos + maxPos) * 0.5f;
    m_viewOffset = m_canvasSize * 0.5f - center * m_viewScale;
}

// =========================================================================
// Compilation
// =========================================================================

std::string EntityEventEditor::CompileToPython() {
    if (!m_currentGraph) return "";

    std::string code = m_currentGraph->graph.CompileToPython();

    if (OnCompiled) OnCompiled(code);

    return code;
}

bool EntityEventEditor::ValidateGraph(std::vector<std::string>& errors) {
    if (!m_currentGraph) {
        errors.push_back("No graph loaded");
        return false;
    }

    return m_currentGraph->graph.Validate(errors);
}

bool EntityEventEditor::ExportToPython(const std::string& path) {
    std::string code = CompileToPython();
    if (code.empty()) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "# Auto-generated from visual event graph\n";
    file << "# Entity: " << m_currentGraph->entityId << "\n";
    file << "# Graph: " << m_currentGraph->name << "\n\n";
    file << code;

    return true;
}

// =========================================================================
// Templates
// =========================================================================

void EntityEventEditor::LoadTemplate(const std::string& templateName) {
    // TODO: Load predefined templates
}

bool EntityEventEditor::SaveAsTemplate(const std::string& templateName) {
    // TODO: Save current graph as template
    return false;
}

std::vector<std::string> EntityEventEditor::GetAvailableTemplates() const {
    return {
        "Empty",
        "Basic Unit Events",
        "Combat Unit",
        "Resource Gatherer",
        "Building Construction",
        "Hero Abilities",
        "Spawner",
        "Patrol Unit"
    };
}

// =========================================================================
// UI Rendering
// =========================================================================

void EntityEventEditor::Render() {
    if (!m_initialized) return;

    ImGui::Begin("Entity Event Editor", nullptr, ImGuiWindowFlags_MenuBar);

    RenderToolbar();

    // Split into panels
    float panelWidth = 250.0f;

    // Left panel - Node palette
    if (m_showNodePalette) {
        ImGui::BeginChild("NodePalette", ImVec2(panelWidth, 0), true);
        RenderNodePalette();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center - Canvas
    ImGui::BeginChild("Canvas", ImVec2(-(m_showPropertyPanel ? panelWidth : 0) - (m_showCodePreview ? 300 : 0), 0), true, ImGuiWindowFlags_NoScrollbar);
    m_canvasPos = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    m_canvasSize = glm::vec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);

    RenderCanvas();
    ImGui::EndChild();

    // Right panel - Property panel
    if (m_showPropertyPanel) {
        ImGui::SameLine();
        ImGui::BeginChild("Properties", ImVec2(panelWidth, 0), true);
        RenderPropertyPanel();
        ImGui::EndChild();
    }

    // Code preview panel
    if (m_showCodePreview) {
        ImGui::SameLine();
        ImGui::BeginChild("CodePreview", ImVec2(300, 0), true);
        RenderCodePreview();
        ImGui::EndChild();
    }

    // Context menu
    if (m_showContextMenu) {
        RenderContextMenu();
    }

    ImGui::End();
}

void EntityEventEditor::RenderToolbar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Graph")) {
                CreateGraph("New Graph", "unit", "");
            }
            if (ImGui::MenuItem("Open...")) {}
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveGraph();
            }
            if (ImGui::MenuItem("Export Python...")) {
                // TODO: File dialog
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) { CutSelectedNodes(); }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) { CopySelectedNodes(); }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) { PasteNodes(ScreenToCanvas(m_canvasSize * 0.5f)); }
            if (ImGui::MenuItem("Delete", "Del")) { RemoveSelectedNodes(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) { SelectAllNodes(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showNodePalette);
            ImGui::MenuItem("Properties", nullptr, &m_showPropertyPanel);
            ImGui::MenuItem("Code Preview", nullptr, &m_showCodePreview);
            ImGui::Separator();
            if (ImGui::MenuItem("Frame All", "F")) { FrameAll(); }
            if (ImGui::MenuItem("Frame Selected")) { FrameSelected(); }
            if (ImGui::MenuItem("Reset View")) { ResetView(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Templates")) {
            for (const auto& tmpl : GetAvailableTemplates()) {
                if (ImGui::MenuItem(tmpl.c_str())) {
                    LoadTemplate(tmpl);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Toolbar buttons
    if (ImGui::Button("Compile")) {
        CompileToPython();
    }
    ImGui::SameLine();

    std::vector<std::string> errors;
    if (ImGui::Button("Validate")) {
        if (ValidateGraph(errors)) {
            // Show success
        }
    }

    ImGui::SameLine();
    ImGui::Text("Scale: %.1f%%", m_viewScale * 100.0f);

    if (m_currentGraph) {
        ImGui::SameLine();
        ImGui::Text("| %s%s", m_currentGraph->name.c_str(), m_currentGraph->modified ? "*" : "");
    }
}

void EntityEventEditor::RenderCanvas() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(
        ImVec2(m_canvasPos.x, m_canvasPos.y),
        ImVec2(m_canvasPos.x + m_canvasSize.x, m_canvasPos.y + m_canvasSize.y),
        ImColor(m_config.backgroundColor.r, m_config.backgroundColor.g, m_config.backgroundColor.b, m_config.backgroundColor.a)
    );

    // Grid
    if (m_config.showGrid) {
        RenderGrid();
    }

    // Connections
    RenderConnections();

    // Pending connection
    if (m_isConnecting) {
        RenderPendingConnection();
    }

    // Nodes
    RenderNodes();

    // Selection box
    if (m_isBoxSelecting) {
        RenderSelectionBox();
    }

    // Minimap
    if (m_config.showMinimap) {
        RenderMinimap();
    }

    // Handle input
    ProcessInput();
}

void EntityEventEditor::RenderGrid() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float gridSize = m_config.gridSize * m_viewScale;
    ImU32 gridColor = ImColor(m_config.gridColor.r, m_config.gridColor.g, m_config.gridColor.b, m_config.gridColor.a);

    glm::vec2 offset = glm::mod(m_viewOffset, glm::vec2(gridSize));

    for (float x = offset.x; x < m_canvasSize.x; x += gridSize) {
        drawList->AddLine(
            ImVec2(m_canvasPos.x + x, m_canvasPos.y),
            ImVec2(m_canvasPos.x + x, m_canvasPos.y + m_canvasSize.y),
            gridColor
        );
    }

    for (float y = offset.y; y < m_canvasSize.y; y += gridSize) {
        drawList->AddLine(
            ImVec2(m_canvasPos.x, m_canvasPos.y + y),
            ImVec2(m_canvasPos.x + m_canvasSize.x, m_canvasPos.y + y),
            gridColor
        );
    }
}

void EntityEventEditor::RenderNodes() {
    if (!m_currentGraph) return;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        auto* node = m_currentGraph->graph.GetNode(visual.nodeId);
        if (node) {
            RenderNode(visual, node);
        }
    }
}

void EntityEventEditor::RenderNode(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    glm::vec2 screenSize = visual.size * m_viewScale;

    // Skip if not visible
    if (screenPos.x + screenSize.x < m_canvasPos.x || screenPos.x > m_canvasPos.x + m_canvasSize.x ||
        screenPos.y + screenSize.y < m_canvasPos.y || screenPos.y > m_canvasPos.y + m_canvasSize.y) {
        return;
    }

    // Node body
    ImU32 bodyColor = visual.selected ?
        ImColor(0.25f, 0.25f, 0.35f, 1.0f) :
        ImColor(0.18f, 0.18f, 0.22f, 1.0f);

    float rounding = 4.0f * m_viewScale;

    drawList->AddRectFilled(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
        bodyColor, rounding
    );

    // Header
    RenderNodeHeader(visual, node);

    // Selection outline
    if (visual.selected) {
        drawList->AddRect(
            ImVec2(screenPos.x, screenPos.y),
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImColor(0.4f, 0.6f, 1.0f, 1.0f), rounding, 0, 2.0f * m_viewScale
        );
    }

    // Hover outline
    if (visual.nodeId == m_hoveredNode) {
        drawList->AddRect(
            ImVec2(screenPos.x, screenPos.y),
            ImVec2(screenPos.x + screenSize.x, screenPos.y + screenSize.y),
            ImColor(0.6f, 0.6f, 0.6f, 0.5f), rounding
        );
    }

    // Pins
    RenderNodePins(visual, node);
}

void EntityEventEditor::RenderNodeHeader(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    glm::vec2 screenSize = visual.size * m_viewScale;
    float headerHeight = 24.0f * m_viewScale;
    float rounding = 4.0f * m_viewScale;

    glm::vec4 headerColor = GetCategoryColor(node->GetCategory());

    drawList->AddRectFilled(
        ImVec2(screenPos.x, screenPos.y),
        ImVec2(screenPos.x + screenSize.x, screenPos.y + headerHeight),
        ImColor(headerColor.r, headerColor.g, headerColor.b, headerColor.a),
        rounding, ImDrawFlags_RoundCornersTop
    );

    // Title
    float fontSize = 14.0f * m_viewScale;
    drawList->AddText(
        ImVec2(screenPos.x + 8.0f * m_viewScale, screenPos.y + 4.0f * m_viewScale),
        ImColor(1.0f, 1.0f, 1.0f, 1.0f),
        node->GetDisplayName().c_str()
    );
}

void EntityEventEditor::RenderNodePins(EventNodeVisual& visual, Nova::EventNode* node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 screenPos = CanvasToScreen(visual.position);
    float pinRadius = m_config.pinRadius * m_viewScale;
    float yOffset = 32.0f * m_viewScale;
    float pinSpacing = 20.0f * m_viewScale;

    // Flow inputs
    for (const auto& pin : node->GetFlowInputs()) {
        glm::vec2 pinPos = screenPos + glm::vec2(0, yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && !m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, false, isHovered);

        // Pin name
        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f * m_viewScale, pinPos.y - pinRadius),
            ImColor(0.8f, 0.8f, 0.8f, 1.0f),
            pin.name.c_str()
        );

        yOffset += pinSpacing;
    }

    // Data inputs
    for (const auto& pin : node->GetDataInputs()) {
        glm::vec2 pinPos = screenPos + glm::vec2(0, yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && !m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, false, isHovered);

        drawList->AddText(
            ImVec2(pinPos.x + pinRadius + 4.0f * m_viewScale, pinPos.y - pinRadius),
            ImColor(0.8f, 0.8f, 0.8f, 1.0f),
            pin.name.c_str()
        );

        yOffset += pinSpacing;
    }

    // Flow outputs
    yOffset = 32.0f * m_viewScale;
    float rightX = screenPos.x + visual.size.x * m_viewScale;

    for (const auto& pin : node->GetFlowOutputs()) {
        glm::vec2 pinPos = glm::vec2(rightX, screenPos.y + yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, true, isHovered);

        yOffset += pinSpacing;
    }

    // Data outputs
    for (const auto& pin : node->GetDataOutputs()) {
        glm::vec2 pinPos = glm::vec2(rightX, screenPos.y + yOffset);
        bool isHovered = (m_hoveredNode == visual.nodeId && m_hoveredPin == pin.name && m_hoveredPinIsOutput);
        RenderPin(pinPos, pin, true, isHovered);

        yOffset += pinSpacing;
    }

    // Update node size based on pins
    float totalPins = static_cast<float>(std::max(
        node->GetFlowInputs().size() + node->GetDataInputs().size(),
        node->GetFlowOutputs().size() + node->GetDataOutputs().size()
    ));
    visual.size.y = std::max(60.0f, 32.0f + totalPins * 20.0f + 10.0f);
}

void EntityEventEditor::RenderPin(const glm::vec2& pos, const Nova::EventPin& pin, bool isOutput, bool isHovered) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float radius = m_config.pinRadius * m_viewScale;

    // Color based on pin type
    glm::vec4 color;
    if (pin.kind == Nova::EventPinKind::Flow) {
        color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        // Color based on data type
        switch (pin.dataType) {
            case Nova::EventDataType::Bool: color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f); break;
            case Nova::EventDataType::Int: color = glm::vec4(0.2f, 0.8f, 0.8f, 1.0f); break;
            case Nova::EventDataType::Float: color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f); break;
            case Nova::EventDataType::String: color = glm::vec4(0.8f, 0.2f, 0.8f, 1.0f); break;
            case Nova::EventDataType::Vec3: color = glm::vec4(0.8f, 0.8f, 0.2f, 1.0f); break;
            case Nova::EventDataType::Entity: color = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f); break;
            default: color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f); break;
        }
    }

    if (isHovered) {
        color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Draw pin
    if (pin.kind == Nova::EventPinKind::Flow) {
        // Triangle for flow pins
        ImVec2 points[3];
        if (isOutput) {
            points[0] = ImVec2(pos.x - radius, pos.y - radius);
            points[1] = ImVec2(pos.x + radius, pos.y);
            points[2] = ImVec2(pos.x - radius, pos.y + radius);
        } else {
            points[0] = ImVec2(pos.x + radius, pos.y - radius);
            points[1] = ImVec2(pos.x - radius, pos.y);
            points[2] = ImVec2(pos.x + radius, pos.y + radius);
        }
        drawList->AddTriangleFilled(points[0], points[1], points[2], ImColor(color.r, color.g, color.b, color.a));
    } else {
        // Circle for data pins
        bool connected = pin.connectedNodeId != 0;
        if (connected) {
            drawList->AddCircleFilled(ImVec2(pos.x, pos.y), radius, ImColor(color.r, color.g, color.b, color.a));
        } else {
            drawList->AddCircle(ImVec2(pos.x, pos.y), radius, ImColor(color.r, color.g, color.b, color.a), 12, 2.0f);
        }
    }
}

void EntityEventEditor::RenderConnections() {
    if (!m_currentGraph) return;

    for (const auto& conn : m_currentGraph->graph.GetConnections()) {
        auto* fromVisual = GetNodeVisual(conn.fromNode);
        auto* toVisual = GetNodeVisual(conn.toNode);
        auto* fromNode = m_currentGraph->graph.GetNode(conn.fromNode);
        auto* toNode = m_currentGraph->graph.GetNode(conn.toNode);

        if (fromVisual && toVisual && fromNode && toNode) {
            glm::vec2 start = GetPinPosition(*fromVisual, fromNode, conn.fromPin, true);
            glm::vec2 end = GetPinPosition(*toVisual, toNode, conn.toPin, false);

            // Determine color based on connection type
            glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};

            // Check if it's a flow connection
            for (const auto& pin : fromNode->GetFlowOutputs()) {
                if (pin.name == conn.fromPin) {
                    color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                    break;
                }
            }

            RenderConnectionWire(start, end, color, m_config.connectionThickness);
        }
    }
}

void EntityEventEditor::RenderConnectionWire(const glm::vec2& start, const glm::vec2& end, const glm::vec4& color, float thickness) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Bezier curve
    float dx = std::abs(end.x - start.x);
    float tangentLength = std::max(50.0f * m_viewScale, dx * 0.5f);

    glm::vec2 cp1 = start + glm::vec2(tangentLength, 0);
    glm::vec2 cp2 = end - glm::vec2(tangentLength, 0);

    drawList->AddBezierCubic(
        ImVec2(start.x, start.y),
        ImVec2(cp1.x, cp1.y),
        ImVec2(cp2.x, cp2.y),
        ImVec2(end.x, end.y),
        ImColor(color.r, color.g, color.b, color.a),
        thickness * m_viewScale
    );
}

void EntityEventEditor::RenderPendingConnection() {
    if (!m_currentGraph) return;

    auto* visual = GetNodeVisual(m_connectionStartNode);
    auto* node = m_currentGraph->graph.GetNode(m_connectionStartNode);

    if (visual && node) {
        glm::vec2 start = GetPinPosition(*visual, node, m_connectionStartPin, m_connectionStartIsOutput);
        glm::vec2 end = m_connectionEndPos;

        if (!m_connectionStartIsOutput) {
            std::swap(start, end);
        }

        RenderConnectionWire(start, end, glm::vec4(1.0f, 1.0f, 1.0f, 0.5f), m_config.connectionThickness);
    }
}

void EntityEventEditor::RenderSelectionBox() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 start = CanvasToScreen(m_boxSelectStart);
    glm::vec2 end = CanvasToScreen(m_boxSelectEnd);

    drawList->AddRectFilled(
        ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
        ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
        ImColor(m_config.selectionColor.r, m_config.selectionColor.g, m_config.selectionColor.b, m_config.selectionColor.a)
    );

    drawList->AddRect(
        ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
        ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
        ImColor(0.4f, 0.6f, 1.0f, 1.0f)
    );
}

void EntityEventEditor::RenderMinimap() {
    // TODO: Implement minimap
}

void EntityEventEditor::RenderNodePalette() {
    ImGui::Text("Node Palette");
    ImGui::Separator();

    ImGui::InputText("##Search", &m_nodeSearchFilter[0], 256);
    ImGui::Separator();

    for (const auto& style : m_categoryStyles) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(style.color.r, style.color.g, style.color.b, 0.5f));

        if (ImGui::CollapsingHeader(style.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            auto nodes = Nova::EventNodeFactory::Instance().GetNodesInCategory(style.category);

            for (const auto& nodeName : nodes) {
                // Filter by search
                if (!m_nodeSearchFilter.empty()) {
                    std::string lowerName = nodeName;
                    std::string lowerFilter = m_nodeSearchFilter;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
                    if (lowerName.find(lowerFilter) == std::string::npos) continue;
                }

                if (ImGui::Selectable(nodeName.c_str())) {
                    // Add node at center of canvas
                    glm::vec2 center = ScreenToCanvas(m_canvasSize * 0.5f + m_canvasPos);
                    AddNode(nodeName, center);
                }

                // Drag source for drag-and-drop
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("EVENT_NODE", nodeName.c_str(), nodeName.size() + 1);
                    ImGui::Text("Create %s", nodeName.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }

        ImGui::PopStyleColor();
    }
}

void EntityEventEditor::RenderPropertyPanel() {
    ImGui::Text("Properties");
    ImGui::Separator();

    if (m_selectedNodes.empty()) {
        ImGui::TextDisabled("No node selected");
        return;
    }

    if (m_selectedNodes.size() > 1) {
        ImGui::Text("%zu nodes selected", m_selectedNodes.size());
        return;
    }

    uint64_t nodeId = *m_selectedNodes.begin();
    if (!m_currentGraph) return;

    auto* node = m_currentGraph->graph.GetNode(nodeId);
    if (!node) return;

    ImGui::Text("Type: %s", node->GetTypeName().c_str());
    ImGui::Text("Display: %s", node->GetDisplayName().c_str());
    ImGui::Separator();

    // Data input default values
    ImGui::Text("Inputs:");
    for (const auto& pin : node->GetDataInputs()) {
        if (pin.connectedNodeId != 0) {
            ImGui::TextDisabled("%s: Connected", pin.name.c_str());
        } else {
            // Show editable default value based on type
            switch (pin.dataType) {
                case Nova::EventDataType::Bool: {
                    bool val = std::get<bool>(pin.defaultValue);
                    if (ImGui::Checkbox(pin.name.c_str(), &val)) {
                        // TODO: Update pin default value
                    }
                    break;
                }
                case Nova::EventDataType::Int: {
                    int val = std::get<int>(pin.defaultValue);
                    if (ImGui::DragInt(pin.name.c_str(), &val)) {
                        // TODO: Update pin default value
                    }
                    break;
                }
                case Nova::EventDataType::Float: {
                    float val = std::get<float>(pin.defaultValue);
                    if (ImGui::DragFloat(pin.name.c_str(), &val, 0.1f)) {
                        // TODO: Update pin default value
                    }
                    break;
                }
                case Nova::EventDataType::String: {
                    std::string val = std::get<std::string>(pin.defaultValue);
                    char buffer[256];
                    strncpy(buffer, val.c_str(), sizeof(buffer));
                    if (ImGui::InputText(pin.name.c_str(), buffer, sizeof(buffer))) {
                        // TODO: Update pin default value
                    }
                    break;
                }
                default:
                    ImGui::Text("%s: %s", pin.name.c_str(), "N/A");
                    break;
            }
        }
    }
}

void EntityEventEditor::RenderCodePreview() {
    ImGui::Text("Python Code Preview");
    ImGui::Separator();

    std::string code = CompileToPython();

    ImGui::BeginChild("CodeScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(code.c_str());
    ImGui::EndChild();
}

void EntityEventEditor::RenderContextMenu() {
    if (ImGui::BeginPopup("NodeContextMenu")) {
        ImGui::InputText("##Filter", &m_contextMenuFilter[0], 256);
        ImGui::Separator();

        for (const auto& style : m_categoryStyles) {
            if (ImGui::BeginMenu(style.name.c_str())) {
                auto nodes = Nova::EventNodeFactory::Instance().GetNodesInCategory(style.category);

                for (const auto& nodeName : nodes) {
                    if (ImGui::MenuItem(nodeName.c_str())) {
                        AddNode(nodeName, m_contextMenuPos);
                    }
                }

                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    } else {
        m_showContextMenu = false;
    }
}

void EntityEventEditor::ProcessInput() {
    ImGuiIO& io = ImGui::GetIO();

    bool canvasHovered = ImGui::IsWindowHovered();
    glm::vec2 mousePos = glm::vec2(io.MousePos.x, io.MousePos.y);
    glm::vec2 canvasMousePos = ScreenToCanvas(mousePos);

    // Update connection end position
    if (m_isConnecting) {
        m_connectionEndPos = mousePos;
    }

    // Find hovered node and pin
    m_hoveredNode = 0;
    m_hoveredPin.clear();

    if (m_currentGraph && canvasHovered) {
        for (auto& visual : m_currentGraph->nodeVisuals) {
            glm::vec2 screenPos = CanvasToScreen(visual.position);
            glm::vec2 screenSize = visual.size * m_viewScale;

            if (mousePos.x >= screenPos.x && mousePos.x <= screenPos.x + screenSize.x &&
                mousePos.y >= screenPos.y && mousePos.y <= screenPos.y + screenSize.y) {
                m_hoveredNode = visual.nodeId;

                // Check pins
                auto* node = m_currentGraph->graph.GetNode(visual.nodeId);
                if (node) {
                    // Check input pins
                    for (const auto& pin : node->GetFlowInputs()) {
                        glm::vec2 pinPos = GetPinPosition(visual, node, pin.name, false);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = false;
                        }
                    }
                    for (const auto& pin : node->GetDataInputs()) {
                        glm::vec2 pinPos = GetPinPosition(visual, node, pin.name, false);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = false;
                        }
                    }
                    // Check output pins
                    for (const auto& pin : node->GetFlowOutputs()) {
                        glm::vec2 pinPos = GetPinPosition(visual, node, pin.name, true);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = true;
                        }
                    }
                    for (const auto& pin : node->GetDataOutputs()) {
                        glm::vec2 pinPos = GetPinPosition(visual, node, pin.name, true);
                        if (glm::distance(mousePos, pinPos) < m_config.pinRadius * m_viewScale * 2.0f) {
                            m_hoveredPin = pin.name;
                            m_hoveredPinIsOutput = true;
                        }
                    }
                }
                break;
            }
        }
    }

    // Mouse button handling
    if (canvasHovered) {
        // Left click
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!m_hoveredPin.empty()) {
                // Start or complete connection
                if (m_isConnecting) {
                    CompleteConnection(m_hoveredNode, m_hoveredPin);
                } else {
                    StartConnection(m_hoveredNode, m_hoveredPin, m_hoveredPinIsOutput);
                }
            } else if (m_hoveredNode != 0) {
                // Select node
                if (!io.KeyCtrl && m_selectedNodes.find(m_hoveredNode) == m_selectedNodes.end()) {
                    ClearSelection();
                }
                SelectNode(m_hoveredNode, io.KeyCtrl);

                // Start dragging
                m_isDraggingNodes = true;
                m_dragStartPos = canvasMousePos;
                m_dragStartPositions.clear();
                for (uint64_t id : m_selectedNodes) {
                    if (auto* v = GetNodeVisual(id)) {
                        m_dragStartPositions[id] = v->position;
                    }
                }
            } else {
                // Cancel connection or start box select
                if (m_isConnecting) {
                    CancelConnection();
                } else {
                    ClearSelection();
                    m_isBoxSelecting = true;
                    m_boxSelectStart = canvasMousePos;
                    m_boxSelectEnd = canvasMousePos;
                }
            }
        }

        // Left drag
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            if (m_isDraggingNodes) {
                glm::vec2 delta = canvasMousePos - m_dragStartPos;
                for (uint64_t id : m_selectedNodes) {
                    if (auto* v = GetNodeVisual(id)) {
                        v->position = m_dragStartPositions[id] + delta;
                        if (m_config.snapToGrid) {
                            v->position = SnapToGrid(v->position);
                        }
                    }
                }
                if (m_currentGraph) m_currentGraph->modified = true;
            } else if (m_isBoxSelecting) {
                m_boxSelectEnd = canvasMousePos;
            }
        }

        // Left release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (m_isBoxSelecting) {
                BoxSelectNodes(m_boxSelectStart, m_boxSelectEnd);
                m_isBoxSelecting = false;
            }
            m_isDraggingNodes = false;
        }

        // Middle click for panning
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            m_isPanning = true;
            m_panStartPos = mousePos;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            if (m_isPanning) {
                glm::vec2 delta = mousePos - m_panStartPos;
                Pan(delta);
                m_panStartPos = mousePos;
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
            m_isPanning = false;
        }

        // Right click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (m_hoveredNode != 0) {
                // Node context menu
            } else {
                m_showContextMenu = true;
                m_contextMenuPos = canvasMousePos;
                ImGui::OpenPopup("NodeContextMenu");
            }
        }

        // Scroll zoom
        if (io.MouseWheel != 0.0f) {
            Zoom(io.MouseWheel, mousePos);
        }

        // Drag drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EVENT_NODE")) {
                std::string nodeName(static_cast<const char*>(payload->Data));
                AddNode(nodeName, canvasMousePos);
            }
            ImGui::EndDragDropTarget();
        }
    }

    // Keyboard shortcuts
    if (canvasHovered && !io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) CopySelectedNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) PasteNodes(canvasMousePos);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) CutSelectedNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) SelectAllNodes();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) DuplicateSelectedNodes();
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) RemoveSelectedNodes();
        if (ImGui::IsKeyPressed(ImGuiKey_F)) FrameSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (m_isConnecting) CancelConnection();
            else ClearSelection();
        }
    }
}

void EntityEventEditor::Update(float deltaTime) {
    // Animation updates, etc.
}

// =========================================================================
// Helper Functions
// =========================================================================

glm::vec2 EntityEventEditor::ScreenToCanvas(const glm::vec2& screen) const {
    return (screen - m_canvasPos - m_viewOffset) / m_viewScale;
}

glm::vec2 EntityEventEditor::CanvasToScreen(const glm::vec2& canvas) const {
    return canvas * m_viewScale + m_viewOffset + m_canvasPos;
}

EventNodeVisual* EntityEventEditor::GetNodeVisual(uint64_t nodeId) {
    if (!m_currentGraph) return nullptr;

    for (auto& visual : m_currentGraph->nodeVisuals) {
        if (visual.nodeId == nodeId) {
            return &visual;
        }
    }
    return nullptr;
}

EventNodeVisual& EntityEventEditor::CreateNodeVisual(uint64_t nodeId, const glm::vec2& position) {
    EventNodeVisual visual;
    visual.nodeId = nodeId;
    visual.position = position;
    visual.size = glm::vec2(m_config.nodeWidth, 100.0f);

    m_currentGraph->nodeVisuals.push_back(visual);
    return m_currentGraph->nodeVisuals.back();
}

glm::vec2 EntityEventEditor::GetPinPosition(const EventNodeVisual& visual, Nova::EventNode* node, const std::string& pinName, bool isOutput) {
    glm::vec2 screenPos = CanvasToScreen(visual.position);
    float yOffset = 32.0f * m_viewScale;
    float pinSpacing = 20.0f * m_viewScale;

    if (isOutput) {
        float rightX = screenPos.x + visual.size.x * m_viewScale;

        for (const auto& pin : node->GetFlowOutputs()) {
            if (pin.name == pinName) {
                return glm::vec2(rightX, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }

        for (const auto& pin : node->GetDataOutputs()) {
            if (pin.name == pinName) {
                return glm::vec2(rightX, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }
    } else {
        for (const auto& pin : node->GetFlowInputs()) {
            if (pin.name == pinName) {
                return glm::vec2(screenPos.x, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }

        for (const auto& pin : node->GetDataInputs()) {
            if (pin.name == pinName) {
                return glm::vec2(screenPos.x, screenPos.y + yOffset);
            }
            yOffset += pinSpacing;
        }
    }

    return screenPos;
}

glm::vec4 EntityEventEditor::GetCategoryColor(Nova::EventNodeCategory category) const {
    for (const auto& style : m_categoryStyles) {
        if (style.category == category) {
            return style.color;
        }
    }
    return glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
}

glm::vec2 EntityEventEditor::SnapToGrid(const glm::vec2& position) const {
    return glm::vec2(
        std::round(position.x / m_config.gridSize) * m_config.gridSize,
        std::round(position.y / m_config.gridSize) * m_config.gridSize
    );
}

} // namespace Vehement
