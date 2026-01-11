#include "PCGGraphEditor.hpp"
#include "ModernUI.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>

PCGGraphEditor::PCGGraphEditor() = default;
PCGGraphEditor::~PCGGraphEditor() = default;

bool PCGGraphEditor::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing PCG Graph Editor");

    // Create empty graph
    m_graph = std::make_unique<PCG::PCGGraph>();

    m_initialized = true;
    spdlog::info("PCG Graph Editor initialized successfully");
    return true;
}

void PCGGraphEditor::Shutdown() {
    spdlog::info("Shutting down PCG Graph Editor");
    m_initialized = false;
}

void PCGGraphEditor::Render(bool* isOpen) {
    if (!m_initialized) {
        return;
    }

    // Main window
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    bool windowOpen = true;
    if (isOpen) {
        windowOpen = *isOpen;
    }

    if (ImGui::Begin("PCG Graph Editor", isOpen ? isOpen : &windowOpen, windowFlags)) {
        RenderMenuBar();
        RenderToolbar();

        // Main content area - split into panels
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);

        // Left panel - Node Palette
        ImGui::BeginChild("NodePalette", ImVec2(250, 0), true);
        if (m_showNodePalette) {
            RenderNodePalette();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // Center panel - Canvas
        ImGui::BeginChild("Canvas", ImVec2(0, 0), true);
        RenderCanvas();
        ImGui::EndChild();

        ImGui::SameLine();

        // Right panel - Properties
        ImGui::BeginChild("Properties", ImVec2(300, 0), true);
        if (m_showProperties) {
            RenderPropertiesPanel();
        }
        ImGui::EndChild();

        ImGui::EndChild();
    }
    ImGui::End();
}

void PCGGraphEditor::NewGraph() {
    spdlog::info("Creating new PCG graph");
    m_graph = std::make_unique<PCG::PCGGraph>();
    m_selectedNodeId = -1;
    m_nextNodeId = 1;
}

bool PCGGraphEditor::LoadGraph(const std::string& path) {
    spdlog::info("Loading PCG graph from: {}", path);
    // TODO: Implement graph loading
    return false;
}

bool PCGGraphEditor::SaveGraph(const std::string& path) {
    spdlog::info("Saving PCG graph to: {}", path);
    // TODO: Implement graph saving
    return false;
}

// ============================================================================
// UI Rendering
// ============================================================================

void PCGGraphEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Graph", "Ctrl+N")) {
                NewGraph();
            }
            if (ImGui::MenuItem("Open Graph...", "Ctrl+O")) {
                // TODO: Open file dialog
            }
            if (ImGui::MenuItem("Save Graph", "Ctrl+S")) {
                // TODO: Save current graph
            }
            if (ImGui::MenuItem("Save Graph As...", "Ctrl+Shift+S")) {
                // TODO: Save file dialog
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Close", "Ctrl+W")) {
                // Will close window
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Delete Selected", "Del", false, m_selectedNodeId >= 0)) {
                if (m_selectedNodeId >= 0) {
                    DeleteSelectedNode();
                }
            }
            if (ImGui::MenuItem("Select All", "Ctrl+A", false, false)) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showNodePalette);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ImGui::MenuItem("Grid", nullptr, &m_showGrid);
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Reset Zoom")) {
                m_canvasZoom = 1.0f;
            }
            if (ImGui::MenuItem("Center View")) {
                m_canvasOffset = glm::vec2(0.0f);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void PCGGraphEditor::RenderToolbar() {
    ModernUI::BeginGlassCard("Toolbar", ImVec2(0, 40));

    if (ModernUI::GlowButton("New", ImVec2(60, 0))) {
        NewGraph();
    }
    ImGui::SameLine();
    if (ModernUI::GlowButton("Load", ImVec2(60, 0))) {
        // TODO: Load dialog
    }
    ImGui::SameLine();
    if (ModernUI::GlowButton("Save", ImVec2(60, 0))) {
        // TODO: Save current graph
    }

    ImGui::SameLine();
    ModernUI::GradientSeparator();
    ImGui::SameLine();

    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##Zoom", &m_canvasZoom, 0.1f, 3.0f, "%.2fx");

    ImGui::SameLine();
    ImGui::Text("Nodes: %d", m_nextNodeId - 1);

    ModernUI::EndGlassCard();
}

void PCGGraphEditor::RenderNodePalette() {
    if (ModernUI::GradientHeader("Node Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PaletteContent");

        // Input Nodes
        if (ModernUI::GradientHeader("Input Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Coordinates", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Input, "Coordinates");
            }
            if (ModernUI::GlowButton("Seed", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Input, "Seed");
            }
            if (ModernUI::GlowButton("Constant", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Input, "Constant");
            }
        }

        // Noise Nodes
        if (ModernUI::GradientHeader("Noise Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Perlin Noise", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Noise, "Perlin");
            }
            if (ModernUI::GlowButton("Simplex Noise", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Noise, "Simplex");
            }
            if (ModernUI::GlowButton("Cellular Noise", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Noise, "Cellular");
            }
            if (ModernUI::GlowButton("Voronoi", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Noise, "Voronoi");
            }
        }

        // Math Nodes
        if (ModernUI::GradientHeader("Math Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Add", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Add");
            }
            if (ModernUI::GlowButton("Subtract", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Subtract");
            }
            if (ModernUI::GlowButton("Multiply", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Multiply");
            }
            if (ModernUI::GlowButton("Divide", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Divide");
            }
            if (ModernUI::GlowButton("Clamp", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Clamp");
            }
            if (ModernUI::GlowButton("Power", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Math, "Power");
            }
        }

        // Blend Nodes
        if (ModernUI::GradientHeader("Blend Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Lerp", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Blend, "Lerp");
            }
            if (ModernUI::GlowButton("Overlay", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Blend, "Overlay");
            }
            if (ModernUI::GlowButton("Min", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Blend, "Min");
            }
            if (ModernUI::GlowButton("Max", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Blend, "Max");
            }
        }

        // Filter Nodes
        if (ModernUI::GradientHeader("Filter Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Blur", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Filter, "Blur");
            }
            if (ModernUI::GlowButton("Sharpen", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Filter, "Sharpen");
            }
            if (ModernUI::GlowButton("Terrace", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Filter, "Terrace");
            }
        }

        // Output Nodes
        if (ModernUI::GradientHeader("Output Nodes", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ModernUI::GlowButton("Height Output", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Output, "Height");
            }
            if (ModernUI::GlowButton("Biome Output", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Output, "Biome");
            }
            if (ModernUI::GlowButton("Moisture Output", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Output, "Moisture");
            }
            if (ModernUI::GlowButton("Temperature Output", ImVec2(-1, 0))) {
                CreateNode(PCG::NodeCategory::Output, "Temperature");
            }
        }

        ModernUI::EndGlassCard();
    }
}

void PCGGraphEditor::RenderCanvas() {
    ModernUI::GradientHeader("Node Graph");

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw background
    drawList->AddRectFilled(canvasPos,
                           ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                           IM_COL32(10, 10, 15, 255));

    // Draw grid if enabled
    if (m_showGrid) {
        float gridSize = 64.0f * m_canvasZoom;
        ImU32 gridColor = IM_COL32(40, 40, 50, 128);

        // Vertical lines
        for (float x = fmodf(m_canvasOffset.x * m_canvasZoom, gridSize); x < canvasSize.x; x += gridSize) {
            drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                            gridColor);
        }

        // Horizontal lines
        for (float y = fmodf(m_canvasOffset.y * m_canvasZoom, gridSize); y < canvasSize.y; y += gridSize) {
            drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                            gridColor);
        }
    }

    // Placeholder for nodes - draw some example nodes
    drawList->AddText(ImVec2(canvasPos.x + canvasSize.x * 0.5f - 100,
                            canvasPos.y + canvasSize.y * 0.5f),
                     IM_COL32(150, 150, 160, 255),
                     "Node graph canvas");
    drawList->AddText(ImVec2(canvasPos.x + canvasSize.x * 0.5f - 150,
                            canvasPos.y + canvasSize.y * 0.5f + 30),
                     IM_COL32(120, 120, 130, 255),
                     "Create nodes from the palette on the left");

    // Handle input for panning
    ImGui::InvisibleButton("Canvas", canvasSize);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_canvasOffset.x += delta.x / m_canvasZoom;
        m_canvasOffset.y += delta.y / m_canvasZoom;
    }

    // Handle zoom with mouse wheel
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_canvasZoom *= (1.0f + wheel * 0.1f);
            m_canvasZoom = std::clamp(m_canvasZoom, 0.1f, 3.0f);
        }
    }

    // Right-click context menu for creating nodes
    if (ImGui::BeginPopupContextItem("CanvasContext")) {
        if (ImGui::BeginMenu("Add Node")) {
            if (ImGui::MenuItem("Perlin Noise")) {
                CreateNode(PCG::NodeCategory::Noise, "Perlin");
            }
            if (ImGui::MenuItem("Add")) {
                CreateNode(PCG::NodeCategory::Math, "Add");
            }
            if (ImGui::MenuItem("Height Output")) {
                CreateNode(PCG::NodeCategory::Output, "Height");
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void PCGGraphEditor::RenderNodeContextMenu() {
    // TODO: Implement node context menu
}

void PCGGraphEditor::RenderPinContextMenu() {
    // TODO: Implement pin context menu
}

void PCGGraphEditor::RenderPropertiesPanel() {
    if (ModernUI::GradientHeader("Node Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PropertiesContent");

        if (m_selectedNodeId < 0) {
            ImGui::TextDisabled("Select a node to view properties");
        } else {
            ImGui::Text("Node ID: %d", m_selectedNodeId);
            ModernUI::GradientSeparator(0.3f);

            // Placeholder properties
            ImGui::Text("Node Type: Unknown");
            ImGui::Text("Inputs: 0");
            ImGui::Text("Outputs: 0");

            ModernUI::GradientSeparator(0.3f);

            ImGui::Text("Parameters");
            // TODO: Add node-specific parameters here

            ModernUI::GradientSeparator(0.3f);

            if (ModernUI::GlowButton("Delete Node", ImVec2(-1, 0))) {
                DeleteSelectedNode();
            }
        }

        ModernUI::EndGlassCard();
    }

    // Graph statistics
    if (ModernUI::GradientHeader("Graph Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("StatsContent");

        ModernUI::CompactStat("Total Nodes", std::to_string(m_nextNodeId - 1).c_str());
        ModernUI::CompactStat("Connections", "0");
        ModernUI::CompactStat("Output Nodes", "0");

        ModernUI::EndGlassCard();
    }
}

// ============================================================================
// Node Creation and Management
// ============================================================================

void PCGGraphEditor::CreateNode(PCG::NodeCategory category, const std::string& type) {
    spdlog::info("Creating node: {} ({})", type, (int)category);

    // TODO: Implement actual node creation in the graph
    // For now, just increment node ID counter
    m_nextNodeId++;

    // Create node at canvas center
    m_createNodePos = m_canvasOffset;
}

void PCGGraphEditor::DeleteSelectedNode() {
    if (m_selectedNodeId >= 0) {
        spdlog::info("Deleting node: {}", m_selectedNodeId);
        // TODO: Implement node deletion from graph
        m_selectedNodeId = -1;
    }
}

// ============================================================================
// Connection Handling
// ============================================================================

void PCGGraphEditor::BeginConnection(int nodeId, int pinIndex, bool isOutput) {
    m_isConnecting = true;
    m_connectionStartNode = nodeId;
    m_connectionStartPin = pinIndex;
    m_connectionStartIsOutput = isOutput;
}

void PCGGraphEditor::EndConnection(int nodeId, int pinIndex, bool isInput) {
    if (!m_isConnecting) {
        return;
    }

    // Validate connection (output to input only)
    if (m_connectionStartIsOutput && isInput) {
        spdlog::info("Creating connection from node {} pin {} to node {} pin {}",
                    m_connectionStartNode, m_connectionStartPin, nodeId, pinIndex);
        // TODO: Create connection in graph
    }

    m_isConnecting = false;
}

void PCGGraphEditor::DeleteConnection(int nodeId, int pinIndex) {
    spdlog::info("Deleting connection at node {} pin {}", nodeId, pinIndex);
    // TODO: Implement connection deletion
}

// ============================================================================
// Rendering Helpers
// ============================================================================

void PCGGraphEditor::DrawNode(PCG::PCGNode* node) {
    // TODO: Implement node rendering
}

void PCGGraphEditor::DrawConnections() {
    // TODO: Implement connection rendering
}

void PCGGraphEditor::DrawConnection(const glm::vec2& start, const glm::vec2& end, bool isActive) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Calculate Bezier control points for smooth curve
    float distance = std::abs(end.x - start.x);
    float controlOffset = std::min(distance * 0.5f, 100.0f);

    ImVec2 p1(start.x, start.y);
    ImVec2 p2(start.x + controlOffset, start.y);
    ImVec2 p3(end.x - controlOffset, end.y);
    ImVec2 p4(end.x, end.y);

    ImU32 color = isActive ?
        IM_COL32(0, 200, 210, 255) :  // Cyan for active
        IM_COL32(120, 120, 140, 255);  // Gray for normal

    drawList->AddBezierCubic(p1, p2, p3, p4, color, 2.0f);
}

// ============================================================================
// Input Handling
// ============================================================================

void PCGGraphEditor::HandleInput() {
    // TODO: Implement input handling for node selection, dragging, etc.
}

glm::vec2 PCGGraphEditor::ScreenToCanvas(const glm::vec2& screen) const {
    return (screen - m_canvasOffset) / m_canvasZoom;
}

glm::vec2 PCGGraphEditor::CanvasToScreen(const glm::vec2& canvas) const {
    return canvas * m_canvasZoom + m_canvasOffset;
}
