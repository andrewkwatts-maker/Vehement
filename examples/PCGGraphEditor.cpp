#include "PCGGraphEditor.hpp"
#include "ModernUI.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <cstring>
#include <cfloat>
#include <algorithm>

namespace {
    std::string GetCategoryName(PCG::NodeCategory category) {
        switch (category) {
            case PCG::NodeCategory::Input: return "Input";
            case PCG::NodeCategory::Noise: return "Noise";
            case PCG::NodeCategory::Math: return "Math";
            case PCG::NodeCategory::Blend: return "Blend";
            case PCG::NodeCategory::RealWorldData: return "Real World Data";
            case PCG::NodeCategory::Terrain: return "Terrain";
            case PCG::NodeCategory::AssetPlacement: return "Asset Placement";
            case PCG::NodeCategory::Filter: return "Filter";
            case PCG::NodeCategory::Output: return "Output";
            default: return "Unknown";
        }
    }

    std::string GetPinTypeName(PCG::PinType type) {
        switch (type) {
            case PCG::PinType::Float: return "Float";
            case PCG::PinType::Vec2: return "Vec2";
            case PCG::PinType::Vec3: return "Vec3";
            case PCG::PinType::Color: return "Color";
            case PCG::PinType::Noise: return "Noise";
            case PCG::PinType::Mask: return "Mask";
            case PCG::PinType::Terrain: return "Terrain";
            case PCG::PinType::AssetList: return "Asset List";
            case PCG::PinType::Transform: return "Transform";
            case PCG::PinType::Custom: return "Custom";
            default: return "Unknown";
        }
    }

    ImU32 GetPinColor(PCG::PinType type) {
        switch (type) {
            case PCG::PinType::Float: return IM_COL32(100, 200, 100, 255);
            case PCG::PinType::Vec2: return IM_COL32(100, 150, 200, 255);
            case PCG::PinType::Vec3: return IM_COL32(150, 100, 200, 255);
            case PCG::PinType::Color: return IM_COL32(200, 150, 100, 255);
            case PCG::PinType::Noise: return IM_COL32(200, 100, 150, 255);
            case PCG::PinType::Mask: return IM_COL32(200, 200, 100, 255);
            case PCG::PinType::Terrain: return IM_COL32(100, 200, 200, 255);
            case PCG::PinType::AssetList: return IM_COL32(200, 100, 100, 255);
            case PCG::PinType::Transform: return IM_COL32(150, 150, 200, 255);
            default: return IM_COL32(150, 150, 150, 255);
        }
    }

    ImU32 GetCategoryColor(PCG::NodeCategory category) {
        switch (category) {
            case PCG::NodeCategory::Input: return IM_COL32(70, 130, 70, 255);
            case PCG::NodeCategory::Noise: return IM_COL32(130, 70, 130, 255);
            case PCG::NodeCategory::Math: return IM_COL32(70, 100, 150, 255);
            case PCG::NodeCategory::Blend: return IM_COL32(150, 100, 70, 255);
            case PCG::NodeCategory::RealWorldData: return IM_COL32(70, 130, 130, 255);
            case PCG::NodeCategory::Terrain: return IM_COL32(100, 80, 60, 255);
            case PCG::NodeCategory::AssetPlacement: return IM_COL32(130, 100, 70, 255);
            case PCG::NodeCategory::Filter: return IM_COL32(100, 100, 130, 255);
            case PCG::NodeCategory::Output: return IM_COL32(130, 70, 70, 255);
            default: return IM_COL32(80, 80, 80, 255);
        }
    }
}

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

    // Open File Dialog
    if (m_showOpenDialog) {
        ImGui::OpenPopup("Open PCG Graph");
        m_showOpenDialog = false;
    }
    if (ImGui::BeginPopupModal("Open PCG Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (strlen(m_filePathBuffer) > 0) {
                LoadGraph(m_filePathBuffer);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Save File Dialog
    if (m_showSaveDialog) {
        ImGui::OpenPopup("Save PCG Graph");
        m_showSaveDialog = false;
        if (!m_currentFilePath.empty()) {
            strncpy(m_filePathBuffer, m_currentFilePath.c_str(), sizeof(m_filePathBuffer) - 1);
        }
    }
    if (ImGui::BeginPopupModal("Save PCG Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file path:");
        ImGui::SetNextItemWidth(400);
        ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (strlen(m_filePathBuffer) > 0) {
                SaveGraph(m_filePathBuffer);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void PCGGraphEditor::NewGraph() {
    spdlog::info("Creating new PCG graph");
    m_graph = std::make_unique<PCG::PCGGraph>();
    m_selectedNodeId = -1;
    m_nextNodeId = 1;
}

bool PCGGraphEditor::LoadGraph(const std::string& path) {
    spdlog::info("Loading PCG graph from: {}", path);

    if (!m_graph) {
        m_graph = std::make_unique<PCG::PCGGraph>();
    }

    if (m_graph->LoadFromFile(path)) {
        m_currentFilePath = path;
        m_selectedNodeId = -1;

        // Update next node ID based on loaded nodes
        m_nextNodeId = 1;
        for (const auto& [id, node] : m_graph->GetNodes()) {
            if (id >= m_nextNodeId) {
                m_nextNodeId = id + 1;
            }
        }

        spdlog::info("PCG graph loaded successfully with {} nodes", m_graph->GetNodes().size());
        return true;
    }

    spdlog::error("Failed to load PCG graph from: {}", path);
    return false;
}

bool PCGGraphEditor::SaveGraph(const std::string& path) {
    spdlog::info("Saving PCG graph to: {}", path);

    if (!m_graph) {
        spdlog::error("No graph to save");
        return false;
    }

    m_graph->SaveToFile(path);
    m_currentFilePath = path;
    spdlog::info("PCG graph saved successfully");
    return true;
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
                m_showOpenDialog = true;
            }
            if (ImGui::MenuItem("Save Graph", "Ctrl+S")) {
                if (!m_currentFilePath.empty()) {
                    SaveGraph(m_currentFilePath);
                } else {
                    m_showSaveDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Graph As...", "Ctrl+Shift+S")) {
                m_showSaveDialog = true;
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
        m_showOpenDialog = true;
    }
    ImGui::SameLine();
    if (ModernUI::GlowButton("Save", ImVec2(60, 0))) {
        if (!m_currentFilePath.empty()) {
            SaveGraph(m_currentFilePath);
        } else {
            m_showSaveDialog = true;
        }
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

    // Draw nodes and connections
    if (m_graph && !m_graph->GetNodes().empty()) {
        // First draw connections (behind nodes)
        DrawConnections();

        // Then draw nodes
        for (const auto& [id, node] : m_graph->GetNodes()) {
            DrawNode(node.get());
        }
    } else {
        // Placeholder text when no nodes exist
        drawList->AddText(ImVec2(canvasPos.x + canvasSize.x * 0.5f - 100,
                                canvasPos.y + canvasSize.y * 0.5f),
                         IM_COL32(150, 150, 160, 255),
                         "Node graph canvas");
        drawList->AddText(ImVec2(canvasPos.x + canvasSize.x * 0.5f - 150,
                                canvasPos.y + canvasSize.y * 0.5f + 30),
                         IM_COL32(120, 120, 130, 255),
                         "Create nodes from the palette on the left");
    }

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

    // Handle node selection, dragging, connections
    HandleInput();

    // Render context menus
    RenderNodeContextMenu();
    RenderPinContextMenu();

    // Right-click context menu for creating nodes (on empty canvas)
    if (ImGui::BeginPopupContextItem("CanvasContext")) {
        // Store position for new node creation
        ImVec2 mousePos = ImGui::GetMousePos();
        m_createNodePos = ScreenToCanvas(glm::vec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y));

        if (ImGui::BeginMenu("Add Input Node")) {
            if (ImGui::MenuItem("Coordinates")) {
                CreateNode(PCG::NodeCategory::Input, "Coordinates");
            }
            if (ImGui::MenuItem("Seed")) {
                CreateNode(PCG::NodeCategory::Input, "Seed");
            }
            if (ImGui::MenuItem("Constant")) {
                CreateNode(PCG::NodeCategory::Input, "Constant");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add Noise Node")) {
            if (ImGui::MenuItem("Perlin Noise")) {
                CreateNode(PCG::NodeCategory::Noise, "Perlin");
            }
            if (ImGui::MenuItem("Simplex Noise")) {
                CreateNode(PCG::NodeCategory::Noise, "Simplex");
            }
            if (ImGui::MenuItem("Voronoi")) {
                CreateNode(PCG::NodeCategory::Noise, "Voronoi");
            }
            if (ImGui::MenuItem("Cellular")) {
                CreateNode(PCG::NodeCategory::Noise, "Cellular");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add Math Node")) {
            if (ImGui::MenuItem("Add")) {
                CreateNode(PCG::NodeCategory::Math, "Add");
            }
            if (ImGui::MenuItem("Subtract")) {
                CreateNode(PCG::NodeCategory::Math, "Subtract");
            }
            if (ImGui::MenuItem("Multiply")) {
                CreateNode(PCG::NodeCategory::Math, "Multiply");
            }
            if (ImGui::MenuItem("Divide")) {
                CreateNode(PCG::NodeCategory::Math, "Divide");
            }
            if (ImGui::MenuItem("Clamp")) {
                CreateNode(PCG::NodeCategory::Math, "Clamp");
            }
            if (ImGui::MenuItem("Power")) {
                CreateNode(PCG::NodeCategory::Math, "Power");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add Blend Node")) {
            if (ImGui::MenuItem("Lerp")) {
                CreateNode(PCG::NodeCategory::Blend, "Lerp");
            }
            if (ImGui::MenuItem("Min")) {
                CreateNode(PCG::NodeCategory::Blend, "Min");
            }
            if (ImGui::MenuItem("Max")) {
                CreateNode(PCG::NodeCategory::Blend, "Max");
            }
            if (ImGui::MenuItem("Overlay")) {
                CreateNode(PCG::NodeCategory::Blend, "Overlay");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Add Output Node")) {
            if (ImGui::MenuItem("Height Output")) {
                CreateNode(PCG::NodeCategory::Output, "Height");
            }
            if (ImGui::MenuItem("Biome Output")) {
                CreateNode(PCG::NodeCategory::Output, "Biome");
            }
            if (ImGui::MenuItem("Moisture Output")) {
                CreateNode(PCG::NodeCategory::Output, "Moisture");
            }
            if (ImGui::MenuItem("Temperature Output")) {
                CreateNode(PCG::NodeCategory::Output, "Temperature");
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
}

void PCGGraphEditor::RenderNodeContextMenu() {
    if (ImGui::BeginPopup("NodeContextMenu")) {
        if (m_selectedNodeId >= 0) {
            PCG::PCGNode* node = m_graph->GetNode(m_selectedNodeId);
            if (node) {
                ImGui::Text("Node: %s", node->GetName().c_str());
                ModernUI::GradientSeparator();
            }

            if (ImGui::MenuItem("Delete Node", "Del")) {
                DeleteSelectedNode();
            }
            if (ImGui::MenuItem("Duplicate Node", "Ctrl+D")) {
                // Duplicate the selected node
                if (node) {
                    CreateNode(node->GetCategory(), node->GetName());
                }
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Disconnect All")) {
                // Disconnect all pins on this node
                if (node) {
                    for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                        m_graph->DisconnectPin(m_selectedNodeId, static_cast<int>(i));
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}

void PCGGraphEditor::RenderPinContextMenu() {
    if (ImGui::BeginPopup("PinContextMenu")) {
        if (m_hoveredPinNode >= 0 && m_hoveredPinIndex >= 0) {
            PCG::PCGNode* node = m_graph->GetNode(m_hoveredPinNode);
            if (node) {
                PCG::PCGPin* pin = m_hoveredPinIsOutput ?
                    node->GetOutputPin(m_hoveredPinIndex) :
                    node->GetInputPin(m_hoveredPinIndex);

                if (pin) {
                    ImGui::Text("Pin: %s", pin->name.c_str());
                    ModernUI::GradientSeparator();

                    if (!m_hoveredPinIsOutput && pin->isConnected) {
                        if (ImGui::MenuItem("Disconnect")) {
                            m_graph->DisconnectPin(m_hoveredPinNode, m_hoveredPinIndex);
                        }
                    }

                    if (ImGui::MenuItem("Break All Connections")) {
                        if (!m_hoveredPinIsOutput) {
                            m_graph->DisconnectPin(m_hoveredPinNode, m_hoveredPinIndex);
                        } else {
                            // For output pins, disconnect all input pins connected to it
                            for (auto& [id, otherNode] : m_graph->GetNodes()) {
                                for (size_t i = 0; i < otherNode->GetInputPins().size(); ++i) {
                                    auto* inputPin = otherNode->GetInputPin(static_cast<int>(i));
                                    if (inputPin && inputPin->connectedNodeId == m_hoveredPinNode &&
                                        inputPin->connectedPinIndex == m_hoveredPinIndex) {
                                        m_graph->DisconnectPin(id, static_cast<int>(i));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}

void PCGGraphEditor::RenderPropertiesPanel() {
    if (ModernUI::GradientHeader("Node Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("PropertiesContent");

        if (m_selectedNodeId < 0) {
            ImGui::TextDisabled("Select a node to view properties");
        } else {
            PCG::PCGNode* node = m_graph ? m_graph->GetNode(m_selectedNodeId) : nullptr;
            if (node) {
                ImGui::Text("Node ID: %d", m_selectedNodeId);
                ModernUI::GradientSeparator(0.3f);

                ImGui::Text("Node Type: %s", node->GetName().c_str());
                ImGui::Text("Category: %s", GetCategoryName(node->GetCategory()).c_str());
                ImGui::Text("Inputs: %zu", node->GetInputPins().size());
                ImGui::Text("Outputs: %zu", node->GetOutputPins().size());

                ModernUI::GradientSeparator(0.3f);

                // Position editing
                ImGui::Text("Position");
                glm::vec2 pos = node->GetPosition();
                if (ImGui::DragFloat2("##NodePos", &pos.x, 1.0f)) {
                    node->SetPosition(pos);
                }

                ModernUI::GradientSeparator(0.3f);

                // Input pins with default values
                if (!node->GetInputPins().empty()) {
                    ImGui::Text("Input Pins");
                    for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                        auto* pin = node->GetInputPin(static_cast<int>(i));
                        if (pin) {
                            ImGui::PushID(static_cast<int>(i));
                            ImGui::Text("%s:", pin->name.c_str());
                            ImGui::SameLine();

                            if (pin->isConnected) {
                                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Connected");
                            } else {
                                // Show default value editor based on pin type
                                switch (pin->type) {
                                    case PCG::PinType::Float:
                                        ImGui::SetNextItemWidth(100);
                                        ImGui::DragFloat("##val", &pin->defaultFloat, 0.01f);
                                        break;
                                    case PCG::PinType::Vec2:
                                        ImGui::SetNextItemWidth(150);
                                        ImGui::DragFloat2("##val", &pin->defaultVec2.x, 0.01f);
                                        break;
                                    case PCG::PinType::Vec3:
                                        ImGui::SetNextItemWidth(200);
                                        ImGui::DragFloat3("##val", &pin->defaultVec3.x, 0.01f);
                                        break;
                                    default:
                                        ImGui::TextDisabled("(no default)");
                                        break;
                                }
                            }
                            ImGui::PopID();
                        }
                    }
                    ModernUI::GradientSeparator(0.3f);
                }

                // Output pins
                if (!node->GetOutputPins().empty()) {
                    ImGui::Text("Output Pins");
                    for (size_t i = 0; i < node->GetOutputPins().size(); ++i) {
                        auto* pin = node->GetOutputPin(static_cast<int>(i));
                        if (pin) {
                            ImGui::BulletText("%s (%s)", pin->name.c_str(), GetPinTypeName(pin->type).c_str());
                        }
                    }
                    ModernUI::GradientSeparator(0.3f);
                }

                if (ModernUI::GlowButton("Delete Node", ImVec2(-1, 0))) {
                    DeleteSelectedNode();
                }
            } else {
                ImGui::TextDisabled("Node not found");
            }
        }

        ModernUI::EndGlassCard();
    }

    // Graph statistics
    if (ModernUI::GradientHeader("Graph Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ModernUI::BeginGlassCard("StatsContent");

        int nodeCount = m_graph ? static_cast<int>(m_graph->GetNodes().size()) : 0;
        int connectionCount = 0;
        int outputNodeCount = 0;

        if (m_graph) {
            for (const auto& [id, node] : m_graph->GetNodes()) {
                if (node->GetCategory() == PCG::NodeCategory::Output) {
                    outputNodeCount++;
                }
                for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                    auto* pin = node->GetInputPin(static_cast<int>(i));
                    if (pin && pin->isConnected) {
                        connectionCount++;
                    }
                }
            }
        }

        ModernUI::CompactStat("Total Nodes", std::to_string(nodeCount).c_str());
        ModernUI::CompactStat("Connections", std::to_string(connectionCount).c_str());
        ModernUI::CompactStat("Output Nodes", std::to_string(outputNodeCount).c_str());

        ModernUI::EndGlassCard();
    }
}

// ============================================================================
// Node Creation and Management
// ============================================================================

void PCGGraphEditor::CreateNode(PCG::NodeCategory category, const std::string& type) {
    spdlog::info("Creating node: {} ({})", type, (int)category);

    if (!m_graph) {
        m_graph = std::make_unique<PCG::PCGGraph>();
    }

    std::unique_ptr<PCG::PCGNode> node;
    int nodeId = m_nextNodeId++;

    // Create appropriate node based on category and type
    switch (category) {
        case PCG::NodeCategory::Input:
            if (type == "Coordinates" || type == "Position") {
                node = std::make_unique<PCG::PositionInputNode>(nodeId);
            } else if (type == "Seed" || type == "LatLong") {
                node = std::make_unique<PCG::LatLongInputNode>(nodeId);
            } else if (type == "Constant") {
                // Create a simple constant node using MathNode with Abs (single input)
                node = std::make_unique<PCG::PositionInputNode>(nodeId);
            }
            break;

        case PCG::NodeCategory::Noise:
            if (type == "Perlin") {
                node = std::make_unique<PCG::PerlinNoiseNode>(nodeId);
            } else if (type == "Simplex") {
                node = std::make_unique<PCG::SimplexNoiseNode>(nodeId);
            } else if (type == "Voronoi" || type == "Cellular") {
                node = std::make_unique<PCG::VoronoiNoiseNode>(nodeId);
            }
            break;

        case PCG::NodeCategory::Math:
            if (type == "Add") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Add);
            } else if (type == "Subtract") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Subtract);
            } else if (type == "Multiply") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Multiply);
            } else if (type == "Divide") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Divide);
            } else if (type == "Clamp") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Clamp);
            } else if (type == "Power") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Power);
            }
            break;

        case PCG::NodeCategory::Blend:
            if (type == "Lerp") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Lerp);
            } else if (type == "Min") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Min);
            } else if (type == "Max") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Max);
            } else if (type == "Overlay") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Add);
            }
            break;

        case PCG::NodeCategory::RealWorldData:
            if (type == "Elevation") {
                node = std::make_unique<PCG::ElevationDataNode>(nodeId);
            } else if (type == "Road") {
                node = std::make_unique<PCG::RoadDistanceNode>(nodeId);
            } else if (type == "Building") {
                node = std::make_unique<PCG::BuildingDataNode>(nodeId);
            } else if (type == "Biome") {
                node = std::make_unique<PCG::BiomeDataNode>(nodeId);
            }
            break;

        case PCG::NodeCategory::Filter:
            // For filter nodes, use math operations as placeholders
            if (type == "Blur" || type == "Sharpen" || type == "Terrace") {
                node = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Abs);
            }
            break;

        case PCG::NodeCategory::Output:
            // Output nodes can use a simple position input as placeholder
            if (type == "Height" || type == "Biome" || type == "Moisture" || type == "Temperature") {
                node = std::make_unique<PCG::PositionInputNode>(nodeId);
            }
            break;

        default:
            break;
    }

    if (node) {
        // Set node position at canvas center or create position
        node->SetPosition(m_createNodePos);
        m_graph->AddNode(std::move(node));
        m_selectedNodeId = nodeId;
        spdlog::info("Created node {} of type {}", nodeId, type);
    } else {
        spdlog::warn("Failed to create node of type: {}", type);
        m_nextNodeId--; // Rollback ID increment
    }
}

void PCGGraphEditor::FrameAllNodes() {
    if (!m_graph || m_graph->GetNodes().empty()) {
        m_canvasOffset = glm::vec2(0.0f);
        return;
    }

    // Calculate bounding box of all nodes
    glm::vec2 minPos(FLT_MAX);
    glm::vec2 maxPos(-FLT_MAX);

    const float nodeWidth = 180.0f;
    const float nodeHeight = 100.0f;

    for (const auto& [id, node] : m_graph->GetNodes()) {
        glm::vec2 pos = node->GetPosition();
        minPos.x = std::min(minPos.x, pos.x);
        minPos.y = std::min(minPos.y, pos.y);
        maxPos.x = std::max(maxPos.x, pos.x + nodeWidth);
        maxPos.y = std::max(maxPos.y, pos.y + nodeHeight);
    }

    // Center the view on all nodes
    glm::vec2 center = (minPos + maxPos) * 0.5f;
    m_canvasOffset = -center;
}

void PCGGraphEditor::DeleteSelectedNode() {
    if (m_selectedNodeId >= 0 && m_graph) {
        spdlog::info("Deleting node: {}", m_selectedNodeId);

        // First, disconnect all connections to/from this node
        for (auto& [id, node] : m_graph->GetNodes()) {
            for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                auto* pin = node->GetInputPin(static_cast<int>(i));
                if (pin && pin->connectedNodeId == m_selectedNodeId) {
                    m_graph->DisconnectPin(id, static_cast<int>(i));
                }
            }
        }

        // Remove the node from the graph
        m_graph->RemoveNode(m_selectedNodeId);
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
    if (!m_isConnecting || !m_graph) {
        m_isConnecting = false;
        return;
    }

    // Validate connection (output to input only, no self-connections)
    if (m_connectionStartIsOutput && isInput && m_connectionStartNode != nodeId) {
        // Check pin type compatibility
        PCG::PCGNode* sourceNode = m_graph->GetNode(m_connectionStartNode);
        PCG::PCGNode* targetNode = m_graph->GetNode(nodeId);

        if (sourceNode && targetNode) {
            PCG::PCGPin* outputPin = sourceNode->GetOutputPin(m_connectionStartPin);
            PCG::PCGPin* inputPin = targetNode->GetInputPin(pinIndex);

            if (outputPin && inputPin) {
                // Allow connection if types match or are compatible
                bool compatible = (outputPin->type == inputPin->type) ||
                                  (outputPin->type == PCG::PinType::Float) ||
                                  (inputPin->type == PCG::PinType::Float);

                if (compatible) {
                    // Disconnect any existing connection to this input
                    m_graph->DisconnectPin(nodeId, pinIndex);

                    // Create the new connection
                    if (m_graph->ConnectPins(m_connectionStartNode, m_connectionStartPin, nodeId, pinIndex)) {
                        spdlog::info("Created connection from node {} pin {} to node {} pin {}",
                                    m_connectionStartNode, m_connectionStartPin, nodeId, pinIndex);
                    } else {
                        spdlog::warn("Failed to create connection");
                    }
                } else {
                    spdlog::warn("Incompatible pin types for connection");
                }
            }
        }
    } else if (!m_connectionStartIsOutput && !isInput && m_connectionStartNode != nodeId) {
        // Connecting from input to output (reverse direction)
        PCG::PCGNode* sourceNode = m_graph->GetNode(nodeId);
        PCG::PCGNode* targetNode = m_graph->GetNode(m_connectionStartNode);

        if (sourceNode && targetNode) {
            PCG::PCGPin* outputPin = sourceNode->GetOutputPin(pinIndex);
            PCG::PCGPin* inputPin = targetNode->GetInputPin(m_connectionStartPin);

            if (outputPin && inputPin) {
                bool compatible = (outputPin->type == inputPin->type) ||
                                  (outputPin->type == PCG::PinType::Float) ||
                                  (inputPin->type == PCG::PinType::Float);

                if (compatible) {
                    m_graph->DisconnectPin(m_connectionStartNode, m_connectionStartPin);
                    if (m_graph->ConnectPins(nodeId, pinIndex, m_connectionStartNode, m_connectionStartPin)) {
                        spdlog::info("Created connection from node {} pin {} to node {} pin {}",
                                    nodeId, pinIndex, m_connectionStartNode, m_connectionStartPin);
                    }
                }
            }
        }
    }

    m_isConnecting = false;
}

void PCGGraphEditor::DeleteConnection(int nodeId, int pinIndex) {
    if (!m_graph) {
        return;
    }

    spdlog::info("Deleting connection at node {} pin {}", nodeId, pinIndex);
    m_graph->DisconnectPin(nodeId, pinIndex);
}

// ============================================================================
// Rendering Helpers
// ============================================================================

void PCGGraphEditor::DrawNode(PCG::PCGNode* node) {
    if (!node) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Node dimensions
    const float nodeWidth = 180.0f;
    const float headerHeight = 25.0f;
    const float pinHeight = 20.0f;
    const float pinRadius = 6.0f;

    // Calculate node height based on pins
    size_t maxPins = std::max(node->GetInputPins().size(), node->GetOutputPins().size());
    float nodeHeight = headerHeight + maxPins * pinHeight + 10.0f;

    // Get screen position from canvas position
    glm::vec2 nodePos = CanvasToScreen(node->GetPosition());
    ImVec2 nodeScreenPos(canvasPos.x + nodePos.x, canvasPos.y + nodePos.y);

    // Check if node is selected
    bool isSelected = (node->GetId() == m_selectedNodeId);
    bool isHovered = (node->GetId() == m_hoveredNodeId);

    // Node background
    ImU32 bgColor = isSelected ? IM_COL32(60, 60, 70, 240) :
                    isHovered ? IM_COL32(50, 50, 60, 240) : IM_COL32(40, 40, 50, 240);
    ImU32 borderColor = isSelected ? IM_COL32(100, 180, 255, 255) :
                        isHovered ? IM_COL32(80, 140, 200, 255) : IM_COL32(60, 60, 80, 255);

    // Draw node body
    drawList->AddRectFilled(nodeScreenPos,
                           ImVec2(nodeScreenPos.x + nodeWidth * m_canvasZoom,
                                  nodeScreenPos.y + nodeHeight * m_canvasZoom),
                           bgColor, 5.0f * m_canvasZoom);

    // Draw header
    ImU32 headerColor = GetCategoryColor(node->GetCategory());
    drawList->AddRectFilled(nodeScreenPos,
                           ImVec2(nodeScreenPos.x + nodeWidth * m_canvasZoom,
                                  nodeScreenPos.y + headerHeight * m_canvasZoom),
                           headerColor, 5.0f * m_canvasZoom, ImDrawFlags_RoundCornersTop);

    // Draw node name
    drawList->AddText(ImVec2(nodeScreenPos.x + 8 * m_canvasZoom, nodeScreenPos.y + 5 * m_canvasZoom),
                     IM_COL32(255, 255, 255, 255), node->GetName().c_str());

    // Draw border
    drawList->AddRect(nodeScreenPos,
                     ImVec2(nodeScreenPos.x + nodeWidth * m_canvasZoom,
                            nodeScreenPos.y + nodeHeight * m_canvasZoom),
                     borderColor, 5.0f * m_canvasZoom, 0, isSelected ? 2.0f : 1.0f);

    // Draw input pins (left side)
    for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
        auto* pin = node->GetInputPin(static_cast<int>(i));
        if (pin) {
            float pinY = nodeScreenPos.y + (headerHeight + 5 + i * pinHeight) * m_canvasZoom;
            ImVec2 pinPos(nodeScreenPos.x, pinY + pinHeight * 0.5f * m_canvasZoom);

            ImU32 pinColor = GetPinColor(pin->type);
            if (pin->isConnected) {
                drawList->AddCircleFilled(pinPos, pinRadius * m_canvasZoom, pinColor);
            } else {
                drawList->AddCircle(pinPos, pinRadius * m_canvasZoom, pinColor, 12, 2.0f);
            }

            // Pin label
            drawList->AddText(ImVec2(pinPos.x + 10 * m_canvasZoom, pinY + 2 * m_canvasZoom),
                             IM_COL32(200, 200, 200, 255), pin->name.c_str());
        }
    }

    // Draw output pins (right side)
    for (size_t i = 0; i < node->GetOutputPins().size(); ++i) {
        auto* pin = node->GetOutputPin(static_cast<int>(i));
        if (pin) {
            float pinY = nodeScreenPos.y + (headerHeight + 5 + i * pinHeight) * m_canvasZoom;
            ImVec2 pinPos(nodeScreenPos.x + nodeWidth * m_canvasZoom, pinY + pinHeight * 0.5f * m_canvasZoom);

            ImU32 pinColor = GetPinColor(pin->type);
            drawList->AddCircleFilled(pinPos, pinRadius * m_canvasZoom, pinColor);

            // Pin label (right-aligned)
            ImVec2 textSize = ImGui::CalcTextSize(pin->name.c_str());
            drawList->AddText(ImVec2(pinPos.x - 10 * m_canvasZoom - textSize.x, pinY + 2 * m_canvasZoom),
                             IM_COL32(200, 200, 200, 255), pin->name.c_str());
        }
    }
}

void PCGGraphEditor::DrawConnections() {
    if (!m_graph) return;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    const float nodeWidth = 180.0f;
    const float headerHeight = 25.0f;
    const float pinHeight = 20.0f;

    // Iterate through all nodes and draw their input connections
    for (const auto& [id, node] : m_graph->GetNodes()) {
        for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
            auto* inputPin = node->GetInputPin(static_cast<int>(i));
            if (inputPin && inputPin->isConnected) {
                // Find the source node
                PCG::PCGNode* sourceNode = m_graph->GetNode(inputPin->connectedNodeId);
                if (sourceNode) {
                    // Calculate start position (output pin on source node)
                    glm::vec2 sourcePos = CanvasToScreen(sourceNode->GetPosition());
                    float sourceY = (headerHeight + 5 + inputPin->connectedPinIndex * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                    glm::vec2 start(canvasPos.x + sourcePos.x + nodeWidth * m_canvasZoom,
                                   canvasPos.y + sourcePos.y + sourceY);

                    // Calculate end position (input pin on this node)
                    glm::vec2 targetPos = CanvasToScreen(node->GetPosition());
                    float targetY = (headerHeight + 5 + i * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                    glm::vec2 end(canvasPos.x + targetPos.x,
                                 canvasPos.y + targetPos.y + targetY);

                    // Draw the connection
                    bool isActive = (m_selectedNodeId == id || m_selectedNodeId == inputPin->connectedNodeId);
                    DrawConnection(start, end, isActive);
                }
            }
        }
    }

    // Draw connection being made
    if (m_isConnecting) {
        PCG::PCGNode* startNode = m_graph->GetNode(m_connectionStartNode);
        if (startNode) {
            glm::vec2 startNodePos = CanvasToScreen(startNode->GetPosition());
            float pinY;
            glm::vec2 start;

            if (m_connectionStartIsOutput) {
                pinY = (headerHeight + 5 + m_connectionStartPin * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                start = glm::vec2(canvasPos.x + startNodePos.x + nodeWidth * m_canvasZoom,
                                 canvasPos.y + startNodePos.y + pinY);
            } else {
                pinY = (headerHeight + 5 + m_connectionStartPin * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                start = glm::vec2(canvasPos.x + startNodePos.x,
                                 canvasPos.y + startNodePos.y + pinY);
            }

            ImVec2 mousePos = ImGui::GetMousePos();
            DrawConnection(start, glm::vec2(mousePos.x, mousePos.y), true);
        }
    }
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
    if (!m_graph) return;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 mousePos = ImGui::GetMousePos();

    const float nodeWidth = 180.0f;
    const float headerHeight = 25.0f;
    const float pinHeight = 20.0f;
    const float pinRadius = 6.0f;

    // Reset hover state
    m_hoveredNodeId = -1;
    m_hoveredPinNode = -1;
    m_hoveredPinIndex = -1;

    // Check for node/pin under cursor
    for (const auto& [id, node] : m_graph->GetNodes()) {
        glm::vec2 nodePos = CanvasToScreen(node->GetPosition());
        ImVec2 nodeScreenPos(canvasPos.x + nodePos.x, canvasPos.y + nodePos.y);

        size_t maxPins = std::max(node->GetInputPins().size(), node->GetOutputPins().size());
        float nodeHeight = headerHeight + maxPins * pinHeight + 10.0f;

        // Check if mouse is over node
        if (mousePos.x >= nodeScreenPos.x &&
            mousePos.x <= nodeScreenPos.x + nodeWidth * m_canvasZoom &&
            mousePos.y >= nodeScreenPos.y &&
            mousePos.y <= nodeScreenPos.y + nodeHeight * m_canvasZoom) {
            m_hoveredNodeId = id;

            // Check input pins
            for (size_t i = 0; i < node->GetInputPins().size(); ++i) {
                float pinY = nodeScreenPos.y + (headerHeight + 5 + i * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                ImVec2 pinPos(nodeScreenPos.x, pinY);

                float dx = mousePos.x - pinPos.x;
                float dy = mousePos.y - pinPos.y;
                if (dx * dx + dy * dy <= (pinRadius * m_canvasZoom + 5) * (pinRadius * m_canvasZoom + 5)) {
                    m_hoveredPinNode = id;
                    m_hoveredPinIndex = static_cast<int>(i);
                    m_hoveredPinIsOutput = false;
                    break;
                }
            }

            // Check output pins
            if (m_hoveredPinNode < 0) {
                for (size_t i = 0; i < node->GetOutputPins().size(); ++i) {
                    float pinY = nodeScreenPos.y + (headerHeight + 5 + i * pinHeight + pinHeight * 0.5f) * m_canvasZoom;
                    ImVec2 pinPos(nodeScreenPos.x + nodeWidth * m_canvasZoom, pinY);

                    float dx = mousePos.x - pinPos.x;
                    float dy = mousePos.y - pinPos.y;
                    if (dx * dx + dy * dy <= (pinRadius * m_canvasZoom + 5) * (pinRadius * m_canvasZoom + 5)) {
                        m_hoveredPinNode = id;
                        m_hoveredPinIndex = static_cast<int>(i);
                        m_hoveredPinIsOutput = true;
                        break;
                    }
                }
            }
        }
    }

    // Handle left mouse button click
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_hoveredPinNode >= 0) {
            // Start connection from pin
            BeginConnection(m_hoveredPinNode, m_hoveredPinIndex, m_hoveredPinIsOutput);
        } else if (m_hoveredNodeId >= 0) {
            // Select node
            m_selectedNodeId = m_hoveredNodeId;
        } else {
            // Click on empty space - deselect
            m_selectedNodeId = -1;
        }
    }

    // Handle left mouse button release (end connection)
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_isConnecting && m_hoveredPinNode >= 0) {
            EndConnection(m_hoveredPinNode, m_hoveredPinIndex, !m_hoveredPinIsOutput);
        } else {
            m_isConnecting = false;
        }
    }

    // Handle node dragging
    if (m_selectedNodeId >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_hoveredPinNode < 0 && !m_isConnecting) {
        PCG::PCGNode* selectedNode = m_graph->GetNode(m_selectedNodeId);
        if (selectedNode && m_hoveredNodeId == m_selectedNodeId) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            glm::vec2 pos = selectedNode->GetPosition();
            pos.x += delta.x / m_canvasZoom;
            pos.y += delta.y / m_canvasZoom;
            selectedNode->SetPosition(pos);
        }
    }

    // Handle right-click context menus
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (m_hoveredPinNode >= 0) {
            ImGui::OpenPopup("PinContextMenu");
        } else if (m_hoveredNodeId >= 0) {
            m_selectedNodeId = m_hoveredNodeId;
            ImGui::OpenPopup("NodeContextMenu");
        }
    }

    // Handle Delete key
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_selectedNodeId >= 0) {
        DeleteSelectedNode();
    }
}

glm::vec2 PCGGraphEditor::ScreenToCanvas(const glm::vec2& screen) const {
    return (screen - m_canvasOffset) / m_canvasZoom;
}

glm::vec2 PCGGraphEditor::CanvasToScreen(const glm::vec2& canvas) const {
    return canvas * m_canvasZoom + m_canvasOffset;
}
