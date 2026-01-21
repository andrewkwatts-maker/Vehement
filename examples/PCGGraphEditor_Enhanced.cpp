/**
 * @file PCGGraphEditor_Enhanced.cpp
 * @brief Enhanced PCG Graph Editor with complete visual node editing
 *
 * Features:
 * - Visual node graph editing with bezier connections
 * - FastNoise2 integration for real noise generation
 * - ModernUI styling with glassmorphic effects
 * - Real-time preview window
 * - Support for real-world data nodes (SRTM, Sentinel-2, OSM, etc.)
 * - Asset placement nodes
 * - Comprehensive math and filter nodes
 */

#include "PCGGraphEditor.hpp"
#include "ModernUI.hpp"
#include "PCGNodeTypes.hpp"
#include "../engine/ui/EditorWidgets.hpp"
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <FastNoise/FastNoise.h>
#include <FastNoise/Generators/Perlin.h>
#include <FastNoise/Generators/Simplex.h>
#include <FastNoise/Generators/Cellular.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>

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

    // Bezier curve for smooth connections
    void DrawBezierConnection(ImDrawList* drawList, ImVec2 p1, ImVec2 p2, ImU32 color, float thickness = 3.0f) {
        float distance = std::abs(p2.x - p1.x);
        float offset = std::min(distance * 0.5f, 100.0f);

        ImVec2 cp1 = ImVec2(p1.x + offset, p1.y);
        ImVec2 cp2 = ImVec2(p2.x - offset, p2.y);

        drawList->AddBezierCubic(p1, cp1, cp2, p2, color, thickness);
    }

    // Get color for pin type
    ImU32 GetPinTypeColor(PCG::PinType type) {
        switch (type) {
            case PCG::PinType::Float:
                return IM_COL32(100, 200, 100, 255);  // Green
            case PCG::PinType::Vec2:
                return IM_COL32(255, 200, 100, 255);  // Orange
            case PCG::PinType::Vec3:
                return IM_COL32(200, 100, 255, 255);  // Purple
            case PCG::PinType::Color:
                return IM_COL32(255, 100, 150, 255);  // Pink
            case PCG::PinType::Noise:
                return IM_COL32(100, 150, 255, 255);  // Blue
            case PCG::PinType::Mask:
                return IM_COL32(200, 200, 200, 255);  // Gray
            case PCG::PinType::Terrain:
                return IM_COL32(150, 100, 50, 255);   // Brown
            case PCG::PinType::AssetList:
                return IM_COL32(255, 200, 50, 255);   // Gold
            default:
                return IM_COL32(150, 150, 150, 255);  // Default gray
        }
    }

    // Get category color
    ImU32 GetCategoryColor(PCG::NodeCategory category) {
        switch (category) {
            case PCG::NodeCategory::Input:
                return IM_COL32(80, 120, 200, 255);
            case PCG::NodeCategory::Noise:
                return IM_COL32(120, 80, 200, 255);
            case PCG::NodeCategory::Math:
                return IM_COL32(80, 200, 120, 255);
            case PCG::NodeCategory::Blend:
                return IM_COL32(200, 150, 80, 255);
            case PCG::NodeCategory::RealWorldData:
                return IM_COL32(200, 120, 80, 255);
            case PCG::NodeCategory::Terrain:
                return IM_COL32(150, 100, 50, 255);
            case PCG::NodeCategory::AssetPlacement:
                return IM_COL32(200, 180, 80, 255);
            case PCG::NodeCategory::Filter:
                return IM_COL32(180, 80, 200, 255);
            case PCG::NodeCategory::Output:
                return IM_COL32(200, 80, 80, 255);
            default:
                return IM_COL32(120, 120, 120, 255);
        }
    }

    // Generate preview texture using FastNoise2
    void GenerateNoisePreview(std::vector<float>& data, int width, int height) {
        // Create a Perlin noise generator
        auto fnSimplex = FastNoise::New<FastNoise::Simplex>();

        data.resize(width * height);
        std::vector<float> noiseData(width * height);

        // Generate noise
        fnSimplex->GenUniformGrid2D(noiseData.data(), 0, 0, width, height, 0.02f, 1337);

        // Normalize to 0-1 range
        for (int i = 0; i < width * height; ++i) {
            data[i] = (noiseData[i] + 1.0f) * 0.5f;
        }
    }
}

// =============================================================================
// PCGGraphEditor Implementation
// =============================================================================

PCGGraphEditor::PCGGraphEditor() = default;

PCGGraphEditor::~PCGGraphEditor() {
    Shutdown();
}

bool PCGGraphEditor::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing PCG Graph Editor (Enhanced)");

    // Create empty graph
    m_graph = std::make_unique<PCG::PCGGraph>();

    m_initialized = true;
    spdlog::info("PCG Graph Editor initialized successfully");
    return true;
}

void PCGGraphEditor::Shutdown() {
    spdlog::info("Shutting down PCG Graph Editor");
    m_initialized = false;
    m_graph.reset();
}

void PCGGraphEditor::Render(bool* isOpen) {
    if (!m_initialized) {
        Initialize();
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowSize(ImVec2(1400, 900), ImGuiCond_FirstUseEver);

    bool* pOpen = isOpen ? isOpen : nullptr;
    if (!ImGui::Begin("PCG Graph Editor", pOpen, windowFlags)) {
        ImGui::End();
        return;
    }

    RenderMenuBar();
    RenderToolbar();

    // Layout: Left palette | Center canvas | Right properties
    float leftPanelWidth = 250.0f;
    float rightPanelWidth = 320.0f;
    ImVec2 availSize = ImGui::GetContentRegionAvail();

    // Left panel - Node Palette
    if (m_showNodePalette) {
        ImGui::BeginChild("NodePalette", ImVec2(leftPanelWidth, -1), true);
        RenderNodePalette();
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Center - Canvas
    float canvasWidth = availSize.x - (m_showNodePalette ? leftPanelWidth + 8 : 0)
                       - (m_showProperties ? rightPanelWidth + 8 : 0);

    ImGui::BeginChild("Canvas", ImVec2(canvasWidth, -1), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderCanvas();
    ImGui::EndChild();

    // Right panel - Properties & Preview
    if (m_showProperties) {
        ImGui::SameLine();
        ImGui::BeginChild("PropertiesPanel", ImVec2(rightPanelWidth, -1), true);
        RenderPropertiesPanel();
        ImGui::EndChild();
    }

    // Context menus
    if (m_showCreateNodeMenu) {
        RenderNodeContextMenu();
    }

    ImGui::End();
}

void PCGGraphEditor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                NewGraph();
            }
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                std::string filePath;
                if (Nova::EditorWidgets::OpenFileDialog("Open PCG Graph", filePath, "*.pcg", nullptr)) {
                    LoadGraph(filePath);
                }
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_currentFilePath.empty()) {
                    SaveGraph(m_currentFilePath);
                }
            }
            if (ImGui::MenuItem("Save As...")) {
                std::string filePath;
                if (Nova::EditorWidgets::SaveFileDialog("Save PCG Graph", filePath, "*.pcg", "untitled.pcg")) {
                    SaveGraph(filePath);
                }
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Exit")) {
                // Close editor
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Delete Node", "Del", false, m_selectedNodeId >= 0)) {
                DeleteSelectedNode();
            }
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Frame All", "F")) {
                FrameAllNodes();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showNodePalette);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ImGui::MenuItem("Grid", nullptr, &m_showGrid);
            ModernUI::GradientSeparator();
            if (ImGui::MenuItem("Reset View")) {
                m_canvasOffset = glm::vec2(0.0f);
                m_canvasZoom = 1.0f;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Graph")) {
            if (ImGui::MenuItem("Execute", "F5")) {
                if (m_graph) {
                    PCG::PCGContext context;
                    context.position = glm::vec3(0.0f);
                    context.latitude = 40.7128;
                    context.longitude = -74.0060;
                    m_graph->Execute(context);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void PCGGraphEditor::RenderToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));

    if (ModernUI::GlowButton("Execute")) {
        if (m_graph) {
            PCG::PCGContext context;
            m_graph->Execute(context);
        }
    }

    ImGui::SameLine();
    ImGui::Text("Zoom: %.1f%%", m_canvasZoom * 100.0f);

    ImGui::SameLine();
    ModernUI::GradientSeparator(0.5f);

    ImGui::SameLine();
    if (ModernUI::GlowButton("+ Noise")) {
        CreateNode(PCG::NodeCategory::Noise, "Perlin");
    }

    ImGui::SameLine();
    if (ModernUI::GlowButton("+ Data")) {
        CreateNode(PCG::NodeCategory::RealWorldData, "SRTM");
    }

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ModernUI::GradientSeparator();
}

void PCGGraphEditor::RenderNodePalette() {
    ModernUI::GradientText("Node Palette");
    ModernUI::GradientSeparator();

    // Search bar
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search nodes...", searchBuffer, sizeof(searchBuffer));

    ImGui::Spacing();

    // Noise Nodes
    if (ModernUI::GradientHeader("Noise (FastNoise2)", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ModernUI::GlowSelectable("Perlin Noise")) {
            CreateNode(PCG::NodeCategory::Noise, "Perlin");
        }
        if (ModernUI::GlowSelectable("Simplex Noise")) {
            CreateNode(PCG::NodeCategory::Noise, "Simplex");
        }
        if (ModernUI::GlowSelectable("Voronoi/Cellular")) {
            CreateNode(PCG::NodeCategory::Noise, "Voronoi");
        }
        ImGui::TreePop();
    }

    // Math Nodes
    if (ModernUI::GradientHeader("Math Operations")) {
        if (ModernUI::GlowSelectable("Add")) CreateNode(PCG::NodeCategory::Math, "Add");
        if (ModernUI::GlowSelectable("Multiply")) CreateNode(PCG::NodeCategory::Math, "Multiply");
        if (ModernUI::GlowSelectable("Clamp")) CreateNode(PCG::NodeCategory::Math, "Clamp");
        if (ModernUI::GlowSelectable("Remap Range")) CreateNode(PCG::NodeCategory::Math, "Remap");
        ImGui::TreePop();
    }

    // Real-World Data
    if (ModernUI::GradientHeader("Real-World Data")) {
        if (ModernUI::GlowSelectable("SRTM Elevation")) CreateNode(PCG::NodeCategory::RealWorldData, "SRTM");
        if (ModernUI::GlowSelectable("Sentinel-2 RGB")) CreateNode(PCG::NodeCategory::RealWorldData, "Sentinel2");
        if (ModernUI::GlowSelectable("OSM Roads")) CreateNode(PCG::NodeCategory::RealWorldData, "OSMRoads");
        if (ModernUI::GlowSelectable("OSM Buildings")) CreateNode(PCG::NodeCategory::RealWorldData, "OSMBuildings");
        ImGui::TreePop();
    }

    // Asset Placement
    if (ModernUI::GradientHeader("Asset Placement")) {
        if (ModernUI::GlowSelectable("Point Scatter")) CreateNode(PCG::NodeCategory::AssetPlacement, "Scatter");
        if (ModernUI::GlowSelectable("Cluster")) CreateNode(PCG::NodeCategory::AssetPlacement, "Cluster");
        if (ModernUI::GlowSelectable("Along Curve")) CreateNode(PCG::NodeCategory::AssetPlacement, "AlongCurve");
        ImGui::TreePop();
    }
}

void PCGGraphEditor::RenderCanvas() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background with subtle gradient
    drawList->AddRectFilledMultiColor(
        canvasPos,
        canvasPos + canvasSize,
        IM_COL32(20, 25, 35, 255),
        IM_COL32(25, 30, 40, 255),
        IM_COL32(30, 35, 45, 255),
        IM_COL32(25, 30, 40, 255)
    );

    // Grid
    if (m_showGrid) {
        float gridStep = 64.0f * m_canvasZoom;
        ImVec2 offset = ToImVec2(m_canvasOffset * m_canvasZoom);

        for (float x = fmodf(offset.x, gridStep); x < canvasSize.x; x += gridStep) {
            drawList->AddLine(
                ImVec2(canvasPos.x + x, canvasPos.y),
                ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                IM_COL32(50, 55, 65, 100), 1.0f
            );
        }

        for (float y = fmodf(offset.y, gridStep); y < canvasSize.y; y += gridStep) {
            drawList->AddLine(
                ImVec2(canvasPos.x, canvasPos.y + y),
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                IM_COL32(50, 55, 65, 100), 1.0f
            );
        }
    }

    // Clip to canvas
    drawList->PushClipRect(canvasPos, canvasPos + canvasSize, true);

    // Draw connections first
    DrawConnections();

    // Draw pending connection
    if (m_isConnecting) {
        ImVec2 startPos = CanvasToScreen(m_connectionEndPos);
        ImVec2 endPos = ImGui::GetMousePos();
        DrawBezierConnection(drawList, startPos, endPos, IM_COL32(139, 127, 255, 200), 3.0f);
    }

    // Draw all nodes
    if (m_graph) {
        for (const auto& [id, node] : m_graph->GetNodes()) {
            DrawNode(node.get());
        }
    }

    drawList->PopClipRect();

    // Handle input
    HandleInput();
}

void PCGGraphEditor::DrawNode(PCG::PCGNode* node) {
    if (!node) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    glm::vec2 nodeCanvasPos = node->GetPosition();
    ImVec2 nodePos = CanvasToScreen(nodeCanvasPos);

    // Node dimensions
    const float nodeWidth = 200.0f;
    const float headerHeight = 35.0f;
    const float pinHeight = 24.0f;
    const float pinRadius = 7.0f;

    size_t maxPins = std::max(node->GetInputPins().size(), node->GetOutputPins().size());
    float nodeHeight = headerHeight + (maxPins * pinHeight) + 20.0f;

    ImVec2 nodeSize(nodeWidth, nodeHeight);
    ImVec2 nodeMax = nodePos + nodeSize;

    // Check if selected
    bool isSelected = (node->GetId() == m_selectedNodeId);

    // Node background (glassmorphic)
    ImU32 bgColor = isSelected
        ? IM_COL32(45, 50, 70, 240)
        : IM_COL32(35, 40, 55, 220);

    drawList->AddRectFilled(nodePos, nodeMax, bgColor, 8.0f);

    // Header with category color
    ImU32 headerColor = GetCategoryColor(node->GetCategory());
    drawList->AddRectFilled(nodePos, ImVec2(nodeMax.x, nodePos.y + headerHeight),
                           headerColor, 8.0f, ImDrawFlags_RoundCornersTop);

    // Node title
    drawList->AddText(ImVec2(nodePos.x + 10, nodePos.y + 8),
                     IM_COL32(255, 255, 255, 255),
                     node->GetName().c_str());

    // Border with glow for selected
    if (isSelected) {
        drawList->AddRect(nodePos - ImVec2(2, 2), nodeMax + ImVec2(2, 2),
                         IM_COL32(139, 127, 255, 200), 8.0f, 0, 3.0f);
    } else {
        drawList->AddRect(nodePos, nodeMax, IM_COL32(60, 65, 80, 255), 8.0f, 0, 1.5f);
    }

    // Draw input pins
    float yOffset = headerHeight + 10.0f;
    int pinIndex = 0;
    for (const auto& pin : node->GetInputPins()) {
        ImVec2 pinPos = ImVec2(nodePos.x, nodePos.y + yOffset);
        ImU32 pinColor = GetPinTypeColor(pin.type);

        // Pin circle
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        drawList->AddCircle(pinPos, pinRadius, IM_COL32(255, 255, 255, 100), 12, 1.5f);

        // Pin label
        drawList->AddText(ImVec2(pinPos.x + 15, pinPos.y - 8),
                         IM_COL32(200, 200, 200, 255), pin.name.c_str());

        // Hit test
        ImVec2 pinMin = pinPos - ImVec2(pinRadius + 5, pinRadius + 5);
        ImVec2 pinMax = pinPos + ImVec2(pinRadius + 5, pinRadius + 5);
        if (ImGui::IsMouseHoveringRect(pinMin, pinMax)) {
            m_hoveredPinNode = node->GetId();
            m_hoveredPinIndex = pinIndex;
            m_hoveredPinIsOutput = false;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (m_isConnecting) {
                    EndConnection(node->GetId(), pinIndex, true);
                }
            }
        }

        yOffset += pinHeight;
        pinIndex++;
    }

    // Draw output pins
    yOffset = headerHeight + 10.0f;
    pinIndex = 0;
    for (const auto& pin : node->GetOutputPins()) {
        ImVec2 pinPos = ImVec2(nodeMax.x, nodePos.y + yOffset);
        ImU32 pinColor = GetPinTypeColor(pin.type);

        // Pin circle
        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        drawList->AddCircle(pinPos, pinRadius, IM_COL32(255, 255, 255, 100), 12, 1.5f);

        // Pin label (right-aligned)
        ImVec2 textSize = ImGui::CalcTextSize(pin.name.c_str());
        drawList->AddText(ImVec2(pinPos.x - textSize.x - 15, pinPos.y - 8),
                         IM_COL32(200, 200, 200, 255), pin.name.c_str());

        // Hit test
        ImVec2 pinMin = pinPos - ImVec2(pinRadius + 5, pinRadius + 5);
        ImVec2 pinMax = pinPos + ImVec2(pinRadius + 5, pinRadius + 5);
        if (ImGui::IsMouseHoveringRect(pinMin, pinMax)) {
            m_hoveredPinNode = node->GetId();
            m_hoveredPinIndex = pinIndex;
            m_hoveredPinIsOutput = true;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (m_isConnecting) {
                    EndConnection(node->GetId(), pinIndex, false);
                } else {
                    BeginConnection(node->GetId(), pinIndex, true);
                }
            }
        }

        yOffset += pinHeight;
        pinIndex++;
    }

    // Node interaction
    ImVec2 headerMax = ImVec2(nodeMax.x, nodePos.y + headerHeight);
    if (ImGui::IsMouseHoveringRect(nodePos, headerMax)) {
        m_hoveredNodeId = node->GetId();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_selectedNodeId = node->GetId();
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_selectedNodeId == node->GetId()) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            glm::vec2 newPos = nodeCanvasPos + ToGlm(delta) / m_canvasZoom;
            node->SetPosition(newPos);
        }
    }
}

void PCGGraphEditor::DrawConnections() {
    if (!m_graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& [id, node] : m_graph->GetNodes()) {
        int inputIndex = 0;
        for (const auto& pin : node->GetInputPins()) {
            if (pin.isConnected) {
                auto* sourceNode = m_graph->GetNode(pin.connectedNodeId);
                if (sourceNode) {
                    const float headerHeight = 35.0f;
                    const float pinHeight = 24.0f;
                    const float nodeWidth = 200.0f;

                    ImVec2 targetPos = CanvasToScreen(glm::vec2(
                        node->GetPosition().x,
                        node->GetPosition().y + headerHeight + 10.0f + inputIndex * pinHeight
                    ));

                    ImVec2 sourcePos = CanvasToScreen(glm::vec2(
                        sourceNode->GetPosition().x + nodeWidth,
                        sourceNode->GetPosition().y + headerHeight + 10.0f + pin.connectedPinIndex * pinHeight
                    ));

                    ImU32 color = GetPinTypeColor(pin.type);
                    DrawBezierConnection(drawList, sourcePos, targetPos, color, 3.0f);
                }
            }
            inputIndex++;
        }
    }
}

void PCGGraphEditor::HandleInput() {
    bool isHovered = ImGui::IsWindowHovered();
    if (!isHovered) return;

    // Pan
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
        (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !m_showCreateNodeMenu)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_canvasOffset = m_canvasOffset + ToGlm(delta) / m_canvasZoom;
    }

    // Zoom
    float scroll = ImGui::GetIO().MouseWheel;
    if (scroll != 0.0f) {
        m_canvasZoom *= (scroll > 0) ? 1.1f : 0.9f;
        m_canvasZoom = std::clamp(m_canvasZoom, 0.25f, 3.0f);
    }

    // Context menu
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && m_hoveredNodeId == -1) {
        m_showCreateNodeMenu = true;
        m_createNodePos = ScreenToCanvas(ToGlm(ImGui::GetMousePos()));
    }

    // Cancel connection
    if (m_isConnecting && (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
        m_isConnecting = false;
    }

    // Delete
    if (m_selectedNodeId >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelectedNode();
    }

    if (!ImGui::IsAnyMouseDown()) {
        m_hoveredNodeId = -1;
        m_hoveredPinNode = -1;
    }
}

glm::vec2 PCGGraphEditor::ScreenToCanvas(const glm::vec2& screen) const {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    glm::vec2 relative = screen - glm::vec2(canvasPos.x, canvasPos.y);
    return (relative / m_canvasZoom) - m_canvasOffset;
}

glm::vec2 PCGGraphEditor::CanvasToScreen(const glm::vec2& canvas) const {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    glm::vec2 transformed = (canvas + m_canvasOffset) * m_canvasZoom;
    return glm::vec2(canvasPos.x, canvasPos.y) + transformed;
}

void PCGGraphEditor::RenderNodeContextMenu() {
    ImGui::OpenPopup("CreateNodeMenu");

    if (ImGui::BeginPopup("CreateNodeMenu")) {
        ModernUI::GradientText("Create Node");
        ModernUI::GradientSeparator();

        if (ImGui::BeginMenu("Noise")) {
            if (ImGui::MenuItem("Perlin")) CreateNode(PCG::NodeCategory::Noise, "Perlin");
            if (ImGui::MenuItem("Simplex")) CreateNode(PCG::NodeCategory::Noise, "Simplex");
            if (ImGui::MenuItem("Voronoi")) CreateNode(PCG::NodeCategory::Noise, "Voronoi");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Math")) {
            if (ImGui::MenuItem("Add")) CreateNode(PCG::NodeCategory::Math, "Add");
            if (ImGui::MenuItem("Multiply")) CreateNode(PCG::NodeCategory::Math, "Multiply");
            if (ImGui::MenuItem("Clamp")) CreateNode(PCG::NodeCategory::Math, "Clamp");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Real-World Data")) {
            if (ImGui::MenuItem("SRTM")) CreateNode(PCG::NodeCategory::RealWorldData, "SRTM");
            if (ImGui::MenuItem("Sentinel-2")) CreateNode(PCG::NodeCategory::RealWorldData, "Sentinel2");
            ImGui::EndMenu();
        }

        ImGui::EndPopup();
    } else {
        m_showCreateNodeMenu = false;
    }
}

void PCGGraphEditor::RenderPropertiesPanel() {
    ModernUI::GradientText("Properties");
    ModernUI::GradientSeparator();

    if (m_selectedNodeId < 0 || !m_graph) {
        ImGui::TextDisabled("No node selected");
    } else {
        auto* node = m_graph->GetNode(m_selectedNodeId);
        if (node) {
            ImGui::Text("Node: %s", node->GetName().c_str());
            ImGui::Text("Type: %s", node->GetTypeId().c_str());

            ModernUI::GradientSeparator();

            glm::vec2 pos = node->GetPosition();
            if (ImGui::DragFloat2("Position", &pos.x, 1.0f)) {
                node->SetPosition(pos);
            }

            ModernUI::GradientSeparator();

            if (ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& pin : node->GetInputPins()) {
                    ImGui::PushID(pin.name.c_str());
                    ImGui::Text("%s", pin.name.c_str());

                    if (!pin.isConnected) {
                        if (pin.type == PCG::PinType::Float) {
                            ImGui::DragFloat("##value", &pin.defaultFloat, 0.01f);
                        } else if (pin.type == PCG::PinType::Vec3) {
                            ImGui::DragFloat3("##value", &pin.defaultVec3.x, 0.01f);
                        }
                    } else {
                        ImGui::TextDisabled("Connected");
                    }

                    ImGui::PopID();
                }
            }
        }
    }

    ModernUI::GradientSeparator();

    // Preview section
    if (ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 previewSize(256, 256);
        ImVec2 previewPos = ImGui::GetCursorScreenPos();

        // Generate preview with FastNoise2
        std::vector<float> previewData;
        GenerateNoisePreview(previewData, 256, 256);

        // Render preview as grayscale
        for (int y = 0; y < 256; ++y) {
            for (int x = 0; x < 256; ++x) {
                float value = previewData[y * 256 + x];
                ImU32 gray = static_cast<ImU32>(value * 255.0f);
                ImU32 color = IM_COL32(gray, gray, gray, 255);

                if (x < 255 && y < 255) {
                    drawList->AddRectFilled(
                        ImVec2(previewPos.x + x, previewPos.y + y),
                        ImVec2(previewPos.x + x + 1, previewPos.y + y + 1),
                        color
                    );
                }
            }
        }

        drawList->AddRect(previewPos, previewPos + previewSize, IM_COL32(100, 105, 115, 255), 2.0f);

        ImGui::Dummy(previewSize);
    }
}

void PCGGraphEditor::CreateNode(PCG::NodeCategory category, const std::string& type) {
    if (!m_graph) return;

    std::unique_ptr<PCG::PCGNode> newNode;
    int nodeId = m_nextNodeId++;

    // Create node based on type
    if (type == "Position") {
        newNode = std::make_unique<PCG::PositionInputNode>(nodeId);
    } else if (type == "Perlin") {
        newNode = std::make_unique<PCG::PerlinNoiseNode>(nodeId);
    } else if (type == "Simplex") {
        newNode = std::make_unique<PCG::SimplexNoiseNode>(nodeId);
    } else if (type == "Voronoi") {
        newNode = std::make_unique<PCG::VoronoiNoiseNode>(nodeId);
    } else if (type == "Add") {
        newNode = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Add);
    } else if (type == "Multiply") {
        newNode = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Multiply);
    } else if (type == "Clamp") {
        newNode = std::make_unique<PCG::MathNode>(nodeId, PCG::MathNode::Operation::Clamp);
    } else if (type == "SRTM") {
        newNode = std::make_unique<PCG::SRTMElevationNode>(nodeId);
    } else if (type == "Sentinel2") {
        newNode = std::make_unique<PCG::Sentinel2Node>(nodeId);
    } else if (type == "OSMRoads") {
        newNode = std::make_unique<PCG::OSMRoadsNode>(nodeId);
    } else if (type == "Scatter") {
        newNode = std::make_unique<PCG::PointScatterNode>(nodeId);
    } else if (type == "Remap") {
        newNode = std::make_unique<PCG::RemapRangeNode>(nodeId);
    } else if (type == "Blend") {
        newNode = std::make_unique<PCG::BlendNode>(nodeId);
    } else {
        newNode = std::make_unique<PCG::PerlinNoiseNode>(nodeId);
    }

    if (newNode) {
        if (m_showCreateNodeMenu) {
            newNode->SetPosition(m_createNodePos);
            m_showCreateNodeMenu = false;
        } else {
            glm::vec2 centerPos = -m_canvasOffset + glm::vec2(400, 300) / m_canvasZoom;
            newNode->SetPosition(centerPos);
        }

        m_selectedNodeId = nodeId;
        m_graph->AddNode(std::move(newNode));
    }
}

void PCGGraphEditor::DeleteSelectedNode() {
    if (m_selectedNodeId < 0 || !m_graph) return;
    m_graph->RemoveNode(m_selectedNodeId);
    m_selectedNodeId = -1;
}

void PCGGraphEditor::BeginConnection(int nodeId, int pinIndex, bool isOutput) {
    m_isConnecting = true;
    m_connectionStartNode = nodeId;
    m_connectionStartPin = pinIndex;
    m_connectionStartIsOutput = isOutput;

    auto* node = m_graph->GetNode(nodeId);
    if (node) {
        const float headerHeight = 35.0f;
        const float pinHeight = 24.0f;
        const float nodeWidth = 200.0f;

        glm::vec2 nodePos = node->GetPosition();
        m_connectionEndPos = glm::vec2(
            nodePos.x + (isOutput ? nodeWidth : 0),
            nodePos.y + headerHeight + 10.0f + pinIndex * pinHeight
        );
    }
}

void PCGGraphEditor::EndConnection(int nodeId, int pinIndex, bool isInput) {
    if (!m_isConnecting || !m_graph) {
        m_isConnecting = false;
        return;
    }

    if (m_connectionStartIsOutput && isInput) {
        m_graph->ConnectPins(m_connectionStartNode, m_connectionStartPin, nodeId, pinIndex);
    } else if (!m_connectionStartIsOutput && !isInput) {
        m_graph->ConnectPins(nodeId, pinIndex, m_connectionStartNode, m_connectionStartPin);
    }

    m_isConnecting = false;
}

void PCGGraphEditor::DeleteConnection(int nodeId, int pinIndex) {
    if (m_graph) {
        m_graph->DisconnectPin(nodeId, pinIndex);
    }
}

void PCGGraphEditor::NewGraph() {
    spdlog::info("Creating new PCG graph");
    m_graph = std::make_unique<PCG::PCGGraph>();
    m_selectedNodeId = -1;
    m_nextNodeId = 1;
    m_canvasOffset = glm::vec2(0.0f);
    m_canvasZoom = 1.0f;
}

void PCGGraphEditor::FrameAllNodes() {
    if (!m_graph || m_graph->GetNodes().empty()) {
        m_canvasOffset = glm::vec2(0.0f);
        m_canvasZoom = 1.0f;
        return;
    }

    // Calculate bounding box of all nodes
    glm::vec2 minPos(std::numeric_limits<float>::max());
    glm::vec2 maxPos(std::numeric_limits<float>::lowest());

    const float nodeWidth = 200.0f;
    const float nodeHeight = 150.0f;  // Approximate node height

    for (const auto& [id, node] : m_graph->GetNodes()) {
        glm::vec2 pos = node->GetPosition();
        minPos = glm::min(minPos, pos);
        maxPos = glm::max(maxPos, pos + glm::vec2(nodeWidth, nodeHeight));
    }

    // Add padding
    const float padding = 50.0f;
    minPos -= glm::vec2(padding);
    maxPos += glm::vec2(padding);

    // Calculate center and size of bounding box
    glm::vec2 center = (minPos + maxPos) * 0.5f;
    glm::vec2 size = maxPos - minPos;

    // Get canvas size (approximate from ImGui window)
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    float canvasWidth = canvasSize.x > 0 ? canvasSize.x : 800.0f;
    float canvasHeight = canvasSize.y > 0 ? canvasSize.y : 600.0f;

    // Calculate zoom to fit all nodes
    float zoomX = canvasWidth / size.x;
    float zoomY = canvasHeight / size.y;
    m_canvasZoom = std::min(zoomX, zoomY);
    m_canvasZoom = std::clamp(m_canvasZoom, 0.25f, 2.0f);

    // Center the view on the nodes
    m_canvasOffset = -center + glm::vec2(canvasWidth, canvasHeight) * 0.5f / m_canvasZoom;

    spdlog::info("Framed {} nodes, zoom: {:.2f}", m_graph->GetNodes().size(), m_canvasZoom);
}

bool PCGGraphEditor::LoadGraph(const std::string& path) {
    spdlog::info("Loading PCG graph from: {}", path);

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open file: {}", path);
            return false;
        }

        nlohmann::json j;
        file >> j;

        // Create new graph
        m_graph = std::make_unique<PCG::PCGGraph>();
        m_selectedNodeId = -1;
        m_nextNodeId = 1;

        // Load nodes
        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& nodeJson : j["nodes"]) {
                int id = nodeJson.value("id", m_nextNodeId++);
                std::string type = nodeJson.value("type", "Perlin");
                std::string category = nodeJson.value("category", "Noise");
                float posX = nodeJson.value("posX", 0.0f);
                float posY = nodeJson.value("posY", 0.0f);

                // Create node based on type
                std::unique_ptr<PCG::PCGNode> node;
                if (type == "Perlin") {
                    node = std::make_unique<PCG::PerlinNoiseNode>(id);
                } else if (type == "Simplex") {
                    node = std::make_unique<PCG::SimplexNoiseNode>(id);
                } else if (type == "Voronoi") {
                    node = std::make_unique<PCG::VoronoiNoiseNode>(id);
                } else if (type == "Add" || type == "Multiply" || type == "Clamp") {
                    auto op = PCG::MathNode::Operation::Add;
                    if (type == "Multiply") op = PCG::MathNode::Operation::Multiply;
                    else if (type == "Clamp") op = PCG::MathNode::Operation::Clamp;
                    node = std::make_unique<PCG::MathNode>(id, op);
                } else if (type == "SRTM") {
                    node = std::make_unique<PCG::SRTMElevationNode>(id);
                } else if (type == "Sentinel2") {
                    node = std::make_unique<PCG::Sentinel2Node>(id);
                } else if (type == "OSMRoads") {
                    node = std::make_unique<PCG::OSMRoadsNode>(id);
                } else if (type == "Scatter") {
                    node = std::make_unique<PCG::PointScatterNode>(id);
                } else if (type == "Remap") {
                    node = std::make_unique<PCG::RemapRangeNode>(id);
                } else if (type == "Blend") {
                    node = std::make_unique<PCG::BlendNode>(id);
                } else if (type == "Position") {
                    node = std::make_unique<PCG::PositionInputNode>(id);
                } else {
                    node = std::make_unique<PCG::PerlinNoiseNode>(id);
                }

                node->SetPosition(glm::vec2(posX, posY));
                m_nextNodeId = std::max(m_nextNodeId, id + 1);
                m_graph->AddNode(std::move(node));
            }
        }

        // Load connections
        if (j.contains("connections") && j["connections"].is_array()) {
            for (const auto& connJson : j["connections"]) {
                int sourceNode = connJson.value("sourceNode", -1);
                int sourcePin = connJson.value("sourcePin", 0);
                int targetNode = connJson.value("targetNode", -1);
                int targetPin = connJson.value("targetPin", 0);

                if (sourceNode >= 0 && targetNode >= 0) {
                    m_graph->ConnectPins(sourceNode, sourcePin, targetNode, targetPin);
                }
            }
        }

        // Load view state
        if (j.contains("view")) {
            m_canvasOffset.x = j["view"].value("offsetX", 0.0f);
            m_canvasOffset.y = j["view"].value("offsetY", 0.0f);
            m_canvasZoom = j["view"].value("zoom", 1.0f);
        }

        m_currentFilePath = path;
        spdlog::info("Loaded PCG graph with {} nodes", m_graph->GetNodes().size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to load PCG graph: {}", e.what());
        return false;
    }
}

bool PCGGraphEditor::SaveGraph(const std::string& path) {
    spdlog::info("Saving PCG graph to: {}", path);

    if (!m_graph) {
        spdlog::error("No graph to save");
        return false;
    }

    try {
        nlohmann::json j;
        j["version"] = "1.0";

        // Save nodes
        nlohmann::json nodesArray = nlohmann::json::array();
        for (const auto& [id, node] : m_graph->GetNodes()) {
            nlohmann::json nodeJson;
            nodeJson["id"] = node->GetId();
            nodeJson["type"] = node->GetTypeId();
            nodeJson["name"] = node->GetName();
            nodeJson["category"] = static_cast<int>(node->GetCategory());
            nodeJson["posX"] = node->GetPosition().x;
            nodeJson["posY"] = node->GetPosition().y;
            nodesArray.push_back(nodeJson);
        }
        j["nodes"] = nodesArray;

        // Save connections
        nlohmann::json connectionsArray = nlohmann::json::array();
        for (const auto& [id, node] : m_graph->GetNodes()) {
            int inputIndex = 0;
            for (const auto& pin : node->GetInputPins()) {
                if (pin.isConnected) {
                    nlohmann::json connJson;
                    connJson["sourceNode"] = pin.connectedNodeId;
                    connJson["sourcePin"] = pin.connectedPinIndex;
                    connJson["targetNode"] = node->GetId();
                    connJson["targetPin"] = inputIndex;
                    connectionsArray.push_back(connJson);
                }
                inputIndex++;
            }
        }
        j["connections"] = connectionsArray;

        // Save view state
        j["view"]["offsetX"] = m_canvasOffset.x;
        j["view"]["offsetY"] = m_canvasOffset.y;
        j["view"]["zoom"] = m_canvasZoom;

        // Write to file
        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", path);
            return false;
        }

        file << j.dump(4);  // Pretty print with 4-space indent
        m_currentFilePath = path;
        spdlog::info("Saved PCG graph with {} nodes", m_graph->GetNodes().size());
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save PCG graph: {}", e.what());
        return false;
    }
}

// Placeholder implementations for base file compatibility
void PCGGraphEditor::RenderPinContextMenu() {}
void PCGGraphEditor::DrawConnection(const glm::vec2& start, const glm::vec2& end, bool isActive) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p1 = ToImVec2(CanvasToScreen(start));
    ImVec2 p2 = ToImVec2(CanvasToScreen(end));
    ImU32 color = isActive ? IM_COL32(139, 127, 255, 255) : IM_COL32(100, 100, 150, 200);
    DrawBezierConnection(drawList, p1, p2, color, isActive ? 4.0f : 3.0f);
}
