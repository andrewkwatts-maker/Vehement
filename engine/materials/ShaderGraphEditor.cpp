/**
 * @file ShaderGraphEditor.cpp
 * @brief Implementation of visual shader graph editor
 */

#include "ShaderGraphEditor.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Nova {

// ============================================================================
// MiniMap
// ============================================================================

void MiniMap::Draw(const ImVec2& editorSize, const ImVec2& canvasOffset,
                   const std::vector<std::pair<ImVec2, ImVec2>>& nodeBounds) {
    if (!m_enabled || nodeBounds.empty()) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();

    // Position in bottom-right corner
    ImVec2 mapPos(windowPos.x + editorSize.x - m_size.x - m_padding,
                  windowPos.y + editorSize.y - m_size.y - m_padding);

    // Background
    drawList->AddRectFilled(mapPos, ImVec2(mapPos.x + m_size.x, mapPos.y + m_size.y),
                            IM_COL32(30, 30, 30, 200), 4.0f);
    drawList->AddRect(mapPos, ImVec2(mapPos.x + m_size.x, mapPos.y + m_size.y),
                      IM_COL32(60, 60, 60, 255), 4.0f);

    // Calculate bounds of all nodes
    ImVec2 minBounds(FLT_MAX, FLT_MAX);
    ImVec2 maxBounds(-FLT_MAX, -FLT_MAX);
    for (const auto& [nodeMin, nodeMax] : nodeBounds) {
        minBounds.x = std::min(minBounds.x, nodeMin.x);
        minBounds.y = std::min(minBounds.y, nodeMin.y);
        maxBounds.x = std::max(maxBounds.x, nodeMax.x);
        maxBounds.y = std::max(maxBounds.y, nodeMax.y);
    }

    // Add padding
    float padding = 50.0f;
    minBounds.x -= padding;
    minBounds.y -= padding;
    maxBounds.x += padding;
    maxBounds.y += padding;

    // Scale factor
    float scaleX = (m_size.x - 10) / (maxBounds.x - minBounds.x);
    float scaleY = (m_size.y - 10) / (maxBounds.y - minBounds.y);
    float scale = std::min(scaleX, scaleY);

    // Draw nodes as small rectangles
    for (const auto& [nodeMin, nodeMax] : nodeBounds) {
        ImVec2 rectMin((nodeMin.x - minBounds.x) * scale + mapPos.x + 5,
                       (nodeMin.y - minBounds.y) * scale + mapPos.y + 5);
        ImVec2 rectMax((nodeMax.x - minBounds.x) * scale + mapPos.x + 5,
                       (nodeMax.y - minBounds.y) * scale + mapPos.y + 5);
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(100, 150, 200, 200));
    }

    // Draw viewport rectangle
    ImVec2 viewMin((-canvasOffset.x - minBounds.x) * scale + mapPos.x + 5,
                   (-canvasOffset.y - minBounds.y) * scale + mapPos.y + 5);
    ImVec2 viewMax = ImVec2(viewMin.x + editorSize.x * scale,
                            viewMin.y + editorSize.y * scale);
    drawList->AddRect(viewMin, viewMax, IM_COL32(255, 200, 100, 255), 2.0f);
}

// ============================================================================
// ShaderGraphEditor
// ============================================================================

ShaderGraphEditor::ShaderGraphEditor() {
    // Register node infos for palette
    m_nodeInfos = {
        // Input
        {"Material Output", "MaterialOutput", "Final material output", NodeCategory::Output},
        {"Texture Coordinates", "TexCoord", "UV coordinates", NodeCategory::Input},
        {"World Position", "WorldPosition", "World space position", NodeCategory::Input},
        {"World Normal", "WorldNormal", "World space normal", NodeCategory::Input},
        {"Vertex Color", "VertexColor", "Vertex color", NodeCategory::Input},
        {"View Direction", "ViewDirection", "Camera view direction", NodeCategory::Input},
        {"Time", "Time", "Game time values", NodeCategory::Input},
        {"Screen Position", "ScreenPosition", "Screen space position", NodeCategory::Input},

        // Parameters
        {"Float", "FloatConstant", "Constant float value", NodeCategory::Parameter},
        {"Vector2", "VectorConstant", "Constant vec2 value", NodeCategory::Parameter},
        {"Vector3", "VectorConstant", "Constant vec3 value", NodeCategory::Parameter},
        {"Color", "ColorConstant", "Color picker", NodeCategory::Parameter},
        {"Parameter", "Parameter", "Exposed material parameter", NodeCategory::Parameter},

        // Texture
        {"Texture 2D", "Texture2D", "Sample 2D texture", NodeCategory::Texture},
        {"Normal Map", "NormalMap", "Sample and decode normal map", NodeCategory::Texture},
        {"Texture Cube", "TextureCube", "Sample cubemap texture", NodeCategory::Texture},

        // Math Basic
        {"Add", "Add", "A + B", NodeCategory::MathBasic},
        {"Subtract", "Subtract", "A - B", NodeCategory::MathBasic},
        {"Multiply", "Multiply", "A * B", NodeCategory::MathBasic},
        {"Divide", "Divide", "A / B", NodeCategory::MathBasic},
        {"One Minus", "OneMinus", "1 - A", NodeCategory::MathBasic},
        {"Abs", "Abs", "Absolute value", NodeCategory::MathBasic},
        {"Negate", "Negate", "-A", NodeCategory::MathBasic},
        {"Min", "Min", "Minimum of A and B", NodeCategory::MathBasic},
        {"Max", "Max", "Maximum of A and B", NodeCategory::MathBasic},
        {"Clamp", "Clamp", "Clamp between min and max", NodeCategory::MathBasic},
        {"Saturate", "Saturate", "Clamp to 0-1", NodeCategory::MathBasic},
        {"Floor", "Floor", "Round down", NodeCategory::MathBasic},
        {"Ceil", "Ceil", "Round up", NodeCategory::MathBasic},
        {"Round", "Round", "Round to nearest", NodeCategory::MathBasic},
        {"Frac", "Frac", "Fractional part", NodeCategory::MathBasic},
        {"Mod", "Mod", "Modulo operation", NodeCategory::MathBasic},

        // Math Advanced
        {"Power", "Power", "A ^ B", NodeCategory::MathAdvanced},
        {"Sqrt", "Sqrt", "Square root", NodeCategory::MathAdvanced},
        {"Log", "Log", "Natural logarithm", NodeCategory::MathAdvanced},
        {"Exp", "Exp", "e ^ A", NodeCategory::MathAdvanced},

        // Math Trig
        {"Sin", "Sin", "Sine", NodeCategory::MathTrig},
        {"Cos", "Cos", "Cosine", NodeCategory::MathTrig},
        {"Tan", "Tan", "Tangent", NodeCategory::MathTrig},
        {"ASin", "Asin", "Arc sine", NodeCategory::MathTrig},
        {"ACos", "Acos", "Arc cosine", NodeCategory::MathTrig},
        {"ATan", "Atan", "Arc tangent", NodeCategory::MathTrig},
        {"ATan2", "Atan2", "Two-argument arc tangent", NodeCategory::MathTrig},

        // Math Vector
        {"Dot Product", "Dot", "Dot product", NodeCategory::MathVector},
        {"Cross Product", "Cross", "Cross product", NodeCategory::MathVector},
        {"Normalize", "Normalize", "Normalize vector", NodeCategory::MathVector},
        {"Length", "Length", "Vector length", NodeCategory::MathVector},
        {"Distance", "Distance", "Distance between points", NodeCategory::MathVector},
        {"Reflect", "Reflect", "Reflect vector", NodeCategory::MathVector},
        {"Refract", "Refract", "Refract vector", NodeCategory::MathVector},

        // Interpolation
        {"Lerp", "Lerp", "Linear interpolation", NodeCategory::MathInterpolation},
        {"SmoothStep", "SmoothStep", "Smooth Hermite interpolation", NodeCategory::MathInterpolation},
        {"Step", "Step", "Step function", NodeCategory::MathInterpolation},
        {"InverseLerp", "InverseLerp", "Inverse linear interpolation", NodeCategory::MathInterpolation},
        {"Remap", "Remap", "Remap value range", NodeCategory::MathInterpolation},

        // Utility
        {"Swizzle", "Swizzle", "Rearrange components", NodeCategory::Utility},
        {"Split", "Split", "Split vector components", NodeCategory::Utility},
        {"Combine", "Combine", "Combine into vector", NodeCategory::Utility},
        {"Append", "Append", "Append component", NodeCategory::Utility},
        {"DDX", "DDX", "Derivative in X", NodeCategory::Utility},
        {"DDY", "DDY", "Derivative in Y", NodeCategory::Utility},

        // Logic
        {"If", "If", "Conditional branch", NodeCategory::Logic},
        {"Compare", "Compare", "Compare values", NodeCategory::Logic},

        // Color
        {"Blend", "Blend", "Blend colors", NodeCategory::Color},
        {"HSV", "HSV", "Adjust hue/saturation/value", NodeCategory::Color},
        {"RGB to HSV", "RGBToHSV", "Convert RGB to HSV", NodeCategory::Color},
        {"HSV to RGB", "HSVToRGB", "Convert HSV to RGB", NodeCategory::Color},
        {"Contrast", "Contrast", "Adjust contrast", NodeCategory::Color},
        {"Posterize", "Posterize", "Reduce color levels", NodeCategory::Color},
        {"Grayscale", "Grayscale", "Convert to grayscale", NodeCategory::Color},

        // Noise
        {"Value Noise", "ValueNoise", "Simple value noise", NodeCategory::Noise},
        {"Perlin Noise", "PerlinNoise", "Classic Perlin noise", NodeCategory::Noise},
        {"Simplex Noise", "SimplexNoise", "Simplex gradient noise", NodeCategory::Noise},
        {"Worley Noise", "WorleyNoise", "Cellular/Worley noise", NodeCategory::Noise},
        {"Voronoi", "Voronoi", "Voronoi cell noise", NodeCategory::Noise},
        {"FBM", "FBM", "Fractal Brownian motion", NodeCategory::Noise},
        {"Turbulence", "Turbulence", "Turbulence noise", NodeCategory::Noise},
        {"Gradient Noise", "GradientNoise", "Gradient noise with direction", NodeCategory::Noise},

        // Pattern
        {"Checkerboard", "Checkerboard", "Checker pattern", NodeCategory::Pattern},
        {"Brick", "Brick", "Brick pattern", NodeCategory::Pattern},
        {"Gradient", "Gradient", "Gradient patterns", NodeCategory::Pattern},
        {"Polar Coordinates", "PolarCoordinates", "Convert to polar", NodeCategory::Pattern},
        {"Triplanar", "Triplanar", "Triplanar projection", NodeCategory::Pattern},
        {"Tiling Offset", "TilingOffset", "Tile and offset UVs", NodeCategory::Pattern},
        {"Rotate UV", "RotateUV", "Rotate UVs", NodeCategory::Pattern},
        {"Parallax", "Parallax", "Parallax occlusion mapping", NodeCategory::Pattern},

        // SDF
        {"SDF Sphere", "SDFSphere", "Sphere signed distance", NodeCategory::Pattern},
        {"SDF Box", "SDFBox", "Box signed distance", NodeCategory::Pattern},
        {"SDF Union", "SDFUnion", "Union of SDFs", NodeCategory::Pattern},
        {"SDF Subtract", "SDFSubtract", "Subtraction of SDFs", NodeCategory::Pattern},
        {"SDF Intersect", "SDFIntersect", "Intersection of SDFs", NodeCategory::Pattern},
        {"SDF Smooth Union", "SDFSmoothUnion", "Smooth union of SDFs", NodeCategory::Pattern},
    };
}

void ShaderGraphEditor::SetGraph(ShaderGraph* graph) {
    m_graph = graph;
    m_ownedGraph.reset();
    m_nodeVisuals.clear();
    m_links.clear();
    m_selectedNodes.clear();
    m_needsRecompile = true;

    if (m_graph) {
        // Initialize visual data for nodes
        float x = 100.0f;
        float y = 100.0f;
        for (const auto& node : m_graph->GetNodes()) {
            uint64_t id = GetNextId();
            m_nodeVisuals[id] = NodeVisualData{ImVec2(x, y)};
            x += 250.0f;
            if (x > 1000.0f) {
                x = 100.0f;
                y += 200.0f;
            }
        }
    }
}

void ShaderGraphEditor::NewGraph() {
    m_ownedGraph = std::make_unique<ShaderGraph>();
    m_graph = m_ownedGraph.get();
    m_nodeVisuals.clear();
    m_links.clear();
    m_selectedNodes.clear();
    m_needsRecompile = true;

    // Add default material output node
    AddNodeAtPosition("MaterialOutput", ImVec2(600, 300));
}

void ShaderGraphEditor::Draw() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::BeginChild("ShaderGraphEditor", ImVec2(0, 0), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        DrawMenuBar();
        DrawToolbar();

        // Main content area
        ImGui::BeginGroup();
        {
            // Left panel - Node palette
            if (m_showPalette) {
                ImGui::BeginChild("NodePalette", ImVec2(200, 0), true);
                DrawNodePalette();
                ImGui::EndChild();
                ImGui::SameLine();
            }

            // Center - Node canvas
            ImGui::BeginChild("NodeCanvas", ImVec2(m_showProperties ? -250.0f : 0.0f, m_showShaderCode ? -200.0f : 0.0f),
                              false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            DrawNodeCanvas();
            ImGui::EndChild();

            // Right panel - Properties
            if (m_showProperties) {
                ImGui::SameLine();
                ImGui::BeginChild("Properties", ImVec2(250, 0), true);
                DrawPropertyPanel();
                ImGui::EndChild();
            }
        }
        ImGui::EndGroup();

        // Bottom panel - Shader output
        if (m_showShaderCode) {
            ImGui::BeginChild("ShaderOutput", ImVec2(0, 200), true);
            DrawShaderOutput();
            ImGui::EndChild();
        }

        HandleShortcuts();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void ShaderGraphEditor::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) NewGraph();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) { /* TODO: File dialog */ }
            if (ImGui::MenuItem("Save", "Ctrl+S")) { /* TODO: Save */ }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) { /* TODO: Save as */ }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Shader...")) { /* TODO: Export */ }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) { /* TODO */ }
            if (ImGui::MenuItem("Copy", "Ctrl+C")) { /* TODO */ }
            if (ImGui::MenuItem("Paste", "Ctrl+V")) { /* TODO */ }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) DuplicateSelected();
            if (ImGui::MenuItem("Delete", "Delete")) DeleteSelected();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) SelectAll();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Node Palette", nullptr, &m_showPalette);
            ImGui::MenuItem("Properties", nullptr, &m_showProperties);
            ImGui::MenuItem("Preview", nullptr, &m_showPreview);
            ImGui::MenuItem("Shader Code", nullptr, &m_showShaderCode);
            ImGui::Separator();
            if (ImGui::MenuItem("Frame All", "F")) FrameAll();
            if (ImGui::MenuItem("Frame Selected", "Shift+F")) FrameSelected();
            ImGui::Separator();
            bool miniMapEnabled = m_miniMap.IsEnabled();
            if (ImGui::MenuItem("Mini Map", nullptr, &miniMapEnabled)) {
                m_miniMap.SetEnabled(miniMapEnabled);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Node")) {
            if (ImGui::MenuItem("Compile", "F5")) CompileShader();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void ShaderGraphEditor::DrawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    if (ImGui::Button("New")) NewGraph();
    ImGui::SameLine();
    if (ImGui::Button("Compile")) CompileShader();
    ImGui::SameLine();

    ImGui::Separator();
    ImGui::SameLine();

    if (ImGui::Button("Undo")) Undo();
    ImGui::SameLine();
    if (ImGui::Button("Redo")) Redo();
    ImGui::SameLine();

    ImGui::Separator();
    ImGui::SameLine();

    ImGui::Text("Zoom: %.0f%%", m_zoom * 100.0f);
    ImGui::SameLine();
    if (ImGui::Button("100%")) m_zoom = 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("Fit")) FrameAll();

    ImGui::PopStyleVar(2);
    ImGui::Separator();
}

void ShaderGraphEditor::DrawNodeCanvas() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(25, 25, 28, 255));

    // Grid
    float gridSize = 32.0f * m_zoom;
    for (float x = fmodf(m_canvasOffset.x, gridSize); x < canvasSize.x; x += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                          ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                          IM_COL32(40, 40, 45, 255));
    }
    for (float y = fmodf(m_canvasOffset.y, gridSize); y < canvasSize.y; y += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                          IM_COL32(40, 40, 45, 255));
    }

    // Handle input
    ImGui::InvisibleButton("canvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvasHovered = ImGui::IsItemHovered();

    // Panning
    if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        m_canvasOffset.x += ImGui::GetIO().MouseDelta.x;
        m_canvasOffset.y += ImGui::GetIO().MouseDelta.y;
    }

    // Zooming
    if (canvasHovered && fabsf(ImGui::GetIO().MouseWheel) > 0.0f) {
        float zoomDelta = ImGui::GetIO().MouseWheel * 0.1f;
        float oldZoom = m_zoom;
        m_zoom = std::clamp(m_zoom + zoomDelta, 0.1f, 3.0f);

        // Zoom towards mouse position
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 mouseCanvas((mousePos.x - canvasPos.x - m_canvasOffset.x) / oldZoom,
                           (mousePos.y - canvasPos.y - m_canvasOffset.y) / oldZoom);
        m_canvasOffset.x = mousePos.x - canvasPos.x - mouseCanvas.x * m_zoom;
        m_canvasOffset.y = mousePos.y - canvasPos.y - mouseCanvas.y * m_zoom;
    }

    // Context menu
    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_showContextMenu = true;
        m_contextMenuPos = ScreenToCanvas(ImGui::GetMousePos());
        m_contextMenuSearch.clear();
    }

    // Draw links
    DrawLinks();

    // Draw nodes
    if (m_graph) {
        std::vector<std::pair<ImVec2, ImVec2>> nodeBounds;
        int idx = 0;
        for (auto& [nodeId, visual] : m_nodeVisuals) {
            if (idx < static_cast<int>(m_graph->GetNodes().size())) {
                auto& node = m_graph->GetNodes()[idx];
                ImVec2 screenPos = CanvasToScreen(visual.position);
                nodeBounds.push_back({visual.position,
                                      ImVec2(visual.position.x + visual.size.x,
                                             visual.position.y + visual.size.y)});
                DrawNode(node.get(), visual, nodeId);
            }
            idx++;
        }

        // Mini-map
        m_miniMap.Draw(canvasSize, m_canvasOffset, nodeBounds);
    }

    // Draw pending link
    if (m_isLinking) {
        DrawPendingLink();
    }

    // Box selection
    if (m_isBoxSelecting) {
        ImVec2 min(std::min(m_boxSelectStart.x, m_boxSelectEnd.x),
                   std::min(m_boxSelectStart.y, m_boxSelectEnd.y));
        ImVec2 max(std::max(m_boxSelectStart.x, m_boxSelectEnd.x),
                   std::max(m_boxSelectStart.y, m_boxSelectEnd.y));
        drawList->AddRectFilled(min, max, IM_COL32(100, 150, 200, 50));
        drawList->AddRect(min, max, IM_COL32(100, 150, 200, 200));
    }

    // Context menu
    DrawContextMenu();
}

void ShaderGraphEditor::DrawNode(ShaderNode* node, NodeVisualData& visual, uint64_t nodeId) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 nodePos = CanvasToScreen(visual.position);
    ImVec2 nodeSize = ImVec2(visual.size.x * m_zoom, visual.size.y * m_zoom);

    // Node colors
    ImU32 headerColor = COLOR_MATH;
    switch (node->GetCategory()) {
        case NodeCategory::Input: headerColor = COLOR_INPUT; break;
        case NodeCategory::Output: headerColor = COLOR_OUTPUT; break;
        case NodeCategory::Parameter: headerColor = COLOR_PARAMETER; break;
        case NodeCategory::Texture: headerColor = COLOR_TEXTURE; break;
        case NodeCategory::MathBasic:
        case NodeCategory::MathAdvanced:
        case NodeCategory::MathTrig: headerColor = COLOR_MATH; break;
        case NodeCategory::MathVector: headerColor = COLOR_VECTOR; break;
        case NodeCategory::MathInterpolation: headerColor = COLOR_MATH; break;
        case NodeCategory::Utility: headerColor = COLOR_UTILITY; break;
        case NodeCategory::Noise: headerColor = COLOR_NOISE; break;
        case NodeCategory::Pattern: headerColor = COLOR_PATTERN; break;
        case NodeCategory::Color: headerColor = COLOR_VECTOR; break;
        case NodeCategory::Logic: headerColor = COLOR_UTILITY; break;
    }

    // Selection highlight
    if (visual.selected) {
        drawList->AddRect(ImVec2(nodePos.x - 2, nodePos.y - 2),
                          ImVec2(nodePos.x + nodeSize.x + 2, nodePos.y + nodeSize.y + 2),
                          IM_COL32(255, 200, 100, 255), 6.0f, 0, 2.0f);
    }

    // Node background
    drawList->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y),
                            IM_COL32(45, 45, 48, 240), 4.0f);

    // Header
    float headerHeight = 24.0f * m_zoom;
    drawList->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + headerHeight),
                            headerColor, 4.0f, ImDrawFlags_RoundCornersTop);

    // Title
    ImGui::PushFont(nullptr); // TODO: Use proper font
    const char* title = node->GetName().c_str();
    ImVec2 textSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2(nodePos.x + (nodeSize.x - textSize.x) * 0.5f,
                             nodePos.y + (headerHeight - textSize.y) * 0.5f),
                      IM_COL32(255, 255, 255, 255), title);
    ImGui::PopFont();

    // Border
    drawList->AddRect(nodePos, ImVec2(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y),
                      IM_COL32(60, 60, 65, 255), 4.0f);

    // Draw pins
    DrawNodePins(node, nodePos, nodeId);

    // Handle node interaction
    ImGui::SetCursorScreenPos(nodePos);
    ImGui::InvisibleButton(("node_" + std::to_string(nodeId)).c_str(), nodeSize);

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (!ImGui::GetIO().KeyCtrl) {
            for (auto& [id, v] : m_nodeVisuals) v.selected = false;
            m_selectedNodes.clear();
        }
        visual.selected = true;
        m_selectedNodes.push_back(nodeId);
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        visual.position.x += delta.x / m_zoom;
        visual.position.y += delta.y / m_zoom;
    }
}

void ShaderGraphEditor::DrawNodePins(ShaderNode* node, const ImVec2& nodePos, uint64_t nodeId) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float pinRadius = 6.0f * m_zoom;
    float pinSpacing = 20.0f * m_zoom;
    float headerHeight = 24.0f * m_zoom;

    // Input pins
    float y = nodePos.y + headerHeight + pinSpacing;
    for (const auto& input : node->GetInputs()) {
        ImVec2 pinPos(nodePos.x, y);
        ImU32 pinColor = GetTypeColor(input.type);

        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        drawList->AddCircle(pinPos, pinRadius, IM_COL32(30, 30, 30, 255), 12, 2.0f);

        // Pin label
        drawList->AddText(ImVec2(pinPos.x + pinRadius + 4, y - 7 * m_zoom),
                          IM_COL32(200, 200, 200, 255), input.displayName.c_str());

        // Handle pin click for linking
        ImGui::SetCursorScreenPos(ImVec2(pinPos.x - pinRadius, pinPos.y - pinRadius));
        if (ImGui::InvisibleButton(("pin_in_" + std::to_string(nodeId) + "_" + input.name).c_str(),
                                   ImVec2(pinRadius * 2, pinRadius * 2))) {
            if (m_isLinking && m_linkFromOutput) {
                // Complete link
                if (CanCreateLink(m_linkSourceNode, m_linkSourcePin, nodeId, input.name)) {
                    CreateLink(m_linkSourceNode, m_linkSourcePin, nodeId, input.name);
                }
                m_isLinking = false;
            } else if (!m_isLinking) {
                // Start link from input
                m_isLinking = true;
                m_linkSourceNode = nodeId;
                m_linkSourcePin = input.name;
                m_linkFromOutput = false;
            }
        }

        y += pinSpacing;
    }

    // Output pins
    y = nodePos.y + headerHeight + pinSpacing;
    float nodeWidth = m_nodeVisuals[nodeId].size.x * m_zoom;
    for (const auto& output : node->GetOutputs()) {
        ImVec2 pinPos(nodePos.x + nodeWidth, y);
        ImU32 pinColor = GetTypeColor(output.type);

        drawList->AddCircleFilled(pinPos, pinRadius, pinColor);
        drawList->AddCircle(pinPos, pinRadius, IM_COL32(30, 30, 30, 255), 12, 2.0f);

        // Pin label
        ImVec2 textSize = ImGui::CalcTextSize(output.displayName.c_str());
        drawList->AddText(ImVec2(pinPos.x - pinRadius - 4 - textSize.x, y - 7 * m_zoom),
                          IM_COL32(200, 200, 200, 255), output.displayName.c_str());

        // Handle pin click for linking
        ImGui::SetCursorScreenPos(ImVec2(pinPos.x - pinRadius, pinPos.y - pinRadius));
        if (ImGui::InvisibleButton(("pin_out_" + std::to_string(nodeId) + "_" + output.name).c_str(),
                                   ImVec2(pinRadius * 2, pinRadius * 2))) {
            if (m_isLinking && !m_linkFromOutput) {
                // Complete link
                if (CanCreateLink(nodeId, output.name, m_linkSourceNode, m_linkSourcePin)) {
                    CreateLink(nodeId, output.name, m_linkSourceNode, m_linkSourcePin);
                }
                m_isLinking = false;
            } else if (!m_isLinking) {
                // Start link from output
                m_isLinking = true;
                m_linkSourceNode = nodeId;
                m_linkSourcePin = output.name;
                m_linkFromOutput = true;
            }
        }

        y += pinSpacing;
    }
}

void ShaderGraphEditor::DrawLinks() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& link : m_links) {
        auto srcIt = m_nodeVisuals.find(link.sourceNodeId);
        auto dstIt = m_nodeVisuals.find(link.destNodeId);
        if (srcIt == m_nodeVisuals.end() || dstIt == m_nodeVisuals.end()) continue;

        // Calculate pin positions (simplified)
        ImVec2 srcPos = CanvasToScreen(srcIt->second.position);
        ImVec2 dstPos = CanvasToScreen(dstIt->second.position);

        float nodeWidth = srcIt->second.size.x * m_zoom;
        srcPos.x += nodeWidth;
        srcPos.y += 44.0f * m_zoom; // Approximate pin position

        dstPos.y += 44.0f * m_zoom;

        // Bezier curve
        float tangent = std::abs(dstPos.x - srcPos.x) * 0.5f;
        tangent = std::max(tangent, 50.0f * m_zoom);

        drawList->AddBezierCubic(srcPos,
                                  ImVec2(srcPos.x + tangent, srcPos.y),
                                  ImVec2(dstPos.x - tangent, dstPos.y),
                                  dstPos,
                                  IM_COL32(200, 200, 200, 200), 2.0f);
    }
}

void ShaderGraphEditor::DrawPendingLink() {
    if (!m_isLinking) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto srcIt = m_nodeVisuals.find(m_linkSourceNode);
    if (srcIt == m_nodeVisuals.end()) return;

    ImVec2 srcPos = CanvasToScreen(srcIt->second.position);
    if (m_linkFromOutput) {
        srcPos.x += srcIt->second.size.x * m_zoom;
    }
    srcPos.y += 44.0f * m_zoom;

    ImVec2 mousePos = ImGui::GetMousePos();

    float tangent = std::abs(mousePos.x - srcPos.x) * 0.5f;
    tangent = std::max(tangent, 50.0f);

    if (m_linkFromOutput) {
        drawList->AddBezierCubic(srcPos,
                                  ImVec2(srcPos.x + tangent, srcPos.y),
                                  ImVec2(mousePos.x - tangent, mousePos.y),
                                  mousePos,
                                  IM_COL32(255, 200, 100, 200), 2.0f);
    } else {
        drawList->AddBezierCubic(mousePos,
                                  ImVec2(mousePos.x + tangent, mousePos.y),
                                  ImVec2(srcPos.x - tangent, srcPos.y),
                                  srcPos,
                                  IM_COL32(255, 200, 100, 200), 2.0f);
    }

    // Cancel on right click or escape
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_isLinking = false;
    }
}

void ShaderGraphEditor::DrawContextMenu() {
    if (m_showContextMenu) {
        ImGui::OpenPopup("NodeContextMenu");
        m_showContextMenu = false;
    }

    if (ImGui::BeginPopup("NodeContextMenu")) {
        ImGui::InputText("##search", &m_contextMenuSearch[0], 256);

        std::string searchLower = m_contextMenuSearch;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        NodeCategory lastCategory = NodeCategory::Input;
        bool firstCategory = true;

        for (const auto& info : m_nodeInfos) {
            // Filter by search
            if (!searchLower.empty()) {
                std::string nameLower = info.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(searchLower) == std::string::npos) continue;
            }

            // Category separator
            if (info.category != lastCategory || firstCategory) {
                if (!firstCategory) ImGui::Separator();
                ImGui::TextDisabled("%s", GetCategoryIcon(info.category));
                ImGui::SameLine();
                switch (info.category) {
                    case NodeCategory::Input: ImGui::TextDisabled("Input"); break;
                    case NodeCategory::Output: ImGui::TextDisabled("Output"); break;
                    case NodeCategory::Parameter: ImGui::TextDisabled("Parameter"); break;
                    case NodeCategory::Texture: ImGui::TextDisabled("Texture"); break;
                    case NodeCategory::MathBasic: ImGui::TextDisabled("Math Basic"); break;
                    case NodeCategory::MathAdvanced: ImGui::TextDisabled("Math Advanced"); break;
                    case NodeCategory::MathTrig: ImGui::TextDisabled("Math Trig"); break;
                    case NodeCategory::MathVector: ImGui::TextDisabled("Math Vector"); break;
                    case NodeCategory::MathInterpolation: ImGui::TextDisabled("Interpolation"); break;
                    case NodeCategory::Utility: ImGui::TextDisabled("Utility"); break;
                    case NodeCategory::Logic: ImGui::TextDisabled("Logic"); break;
                    case NodeCategory::Color: ImGui::TextDisabled("Color"); break;
                    case NodeCategory::Noise: ImGui::TextDisabled("Noise"); break;
                    case NodeCategory::Pattern: ImGui::TextDisabled("Pattern"); break;
                }
                lastCategory = info.category;
                firstCategory = false;
            }

            if (ImGui::MenuItem(info.name.c_str())) {
                AddNodeAtPosition(info.typeName, m_contextMenuPos);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", info.description.c_str());
            }
        }

        ImGui::EndPopup();
    }
}

void ShaderGraphEditor::DrawNodePalette() {
    ImGui::Text("Node Palette");
    ImGui::Separator();

    static char searchBuf[256] = "";
    ImGui::InputTextWithHint("##palettesearch", "Search...", searchBuf, 256);

    std::string searchLower = searchBuf;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    NodeCategory lastCategory = static_cast<NodeCategory>(-1);

    for (const auto& info : m_nodeInfos) {
        // Filter
        if (!searchLower.empty()) {
            std::string nameLower = info.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.find(searchLower) == std::string::npos) continue;
        }

        // Category header
        if (info.category != lastCategory) {
            const char* catName = "";
            switch (info.category) {
                case NodeCategory::Input: catName = "Input"; break;
                case NodeCategory::Output: catName = "Output"; break;
                case NodeCategory::Parameter: catName = "Parameter"; break;
                case NodeCategory::Texture: catName = "Texture"; break;
                case NodeCategory::MathBasic: catName = "Math Basic"; break;
                case NodeCategory::MathAdvanced: catName = "Math Advanced"; break;
                case NodeCategory::MathTrig: catName = "Math Trig"; break;
                case NodeCategory::MathVector: catName = "Math Vector"; break;
                case NodeCategory::MathInterpolation: catName = "Interpolation"; break;
                case NodeCategory::Utility: catName = "Utility"; break;
                case NodeCategory::Logic: catName = "Logic"; break;
                case NodeCategory::Color: catName = "Color"; break;
                case NodeCategory::Noise: catName = "Noise"; break;
                case NodeCategory::Pattern: catName = "Pattern"; break;
            }

            if (lastCategory != static_cast<NodeCategory>(-1)) {
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx(catName, ImGuiTreeNodeFlags_DefaultOpen)) {
                lastCategory = info.category;
            } else {
                lastCategory = info.category;
                continue;
            }
        }

        // Node button
        if (ImGui::Selectable(info.name.c_str())) {
            AddNodeAtPosition(info.typeName, ImVec2(400, 300));
        }

        // Drag and drop source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("NODE_TYPE", info.typeName.c_str(), info.typeName.size() + 1);
            ImGui::Text("Create %s", info.name.c_str());
            ImGui::EndDragDropSource();
        }
    }

    if (lastCategory != static_cast<NodeCategory>(-1)) {
        ImGui::TreePop();
    }
}

void ShaderGraphEditor::DrawPropertyPanel() {
    ImGui::Text("Properties");
    ImGui::Separator();

    if (m_selectedNodes.empty()) {
        ImGui::TextDisabled("No node selected");
        return;
    }

    // Show properties for first selected node
    // TODO: Multi-selection editing

    ImGui::Text("Material Settings");
    ImGui::Separator();

    if (m_graph) {
        // Domain
        const char* domains[] = {"Surface", "Post Process", "Decal", "UI", "Volume", "SDF"};
        int domain = static_cast<int>(m_graph->GetDomain());
        if (ImGui::Combo("Domain", &domain, domains, IM_ARRAYSIZE(domains))) {
            m_graph->SetDomain(static_cast<MaterialDomain>(domain));
            m_needsRecompile = true;
        }

        // Blend mode
        const char* blendModes[] = {"Opaque", "Masked", "Translucent", "Additive", "Modulate"};
        int blendMode = static_cast<int>(m_graph->GetBlendMode());
        if (ImGui::Combo("Blend Mode", &blendMode, blendModes, IM_ARRAYSIZE(blendModes))) {
            m_graph->SetBlendMode(static_cast<BlendMode>(blendMode));
            m_needsRecompile = true;
        }

        // Shading model
        const char* shadingModels[] = {"Unlit", "Default Lit", "Subsurface", "Clear Coat", "Hair", "Eye", "Cloth", "Two Sided Foliage"};
        int shadingModel = static_cast<int>(m_graph->GetShadingModel());
        if (ImGui::Combo("Shading Model", &shadingModel, shadingModels, IM_ARRAYSIZE(shadingModels))) {
            m_graph->SetShadingModel(static_cast<ShadingModel>(shadingModel));
            m_needsRecompile = true;
        }

        // Two sided
        bool twoSided = m_graph->IsTwoSided();
        if (ImGui::Checkbox("Two Sided", &twoSided)) {
            m_graph->SetTwoSided(twoSided);
        }
    }
}

void ShaderGraphEditor::DrawPreviewPanel() {
    if (!m_showPreview) return;

    ImGui::Text("Preview");
    ImGui::Separator();

    // Preview mesh type
    const char* meshTypes[] = {"Sphere", "Cube", "Plane", "Cylinder"};
    ImGui::Combo("Mesh", &m_previewMeshType, meshTypes, IM_ARRAYSIZE(meshTypes));

    // Rotation
    ImGui::SliderFloat("Rotation", &m_previewRotation, 0.0f, 360.0f);

    // Preview image would go here (requires texture rendering)
    ImVec2 previewSize(200, 200);
    ImGui::Image((ImTextureID)(intptr_t)m_previewTexture, previewSize);
}

void ShaderGraphEditor::DrawShaderOutput() {
    ImGui::Text("Generated Shader Code");
    ImGui::SameLine();
    if (ImGui::Button("Copy VS")) {
        ImGui::SetClipboardText(m_compiledVS.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy FS")) {
        ImGui::SetClipboardText(m_compiledFS.c_str());
    }

    ImGui::Separator();

    if (!m_compileError.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", m_compileError.c_str());
    }

    if (ImGui::BeginTabBar("ShaderTabs")) {
        if (ImGui::BeginTabItem("Vertex Shader")) {
            ImGui::InputTextMultiline("##vs", &m_compiledVS[0], m_compiledVS.size(),
                                      ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Fragment Shader")) {
            ImGui::InputTextMultiline("##fs", &m_compiledFS[0], m_compiledFS.size(),
                                      ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void ShaderGraphEditor::HandleShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelected();
    }

    // Ctrl shortcuts
    if (ImGui::GetIO().KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_N)) NewGraph();
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
        if (ImGui::IsKeyPressed(ImGuiKey_A)) SelectAll();
        if (ImGui::IsKeyPressed(ImGuiKey_D)) DuplicateSelected();
    }

    // Frame shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        if (ImGui::GetIO().KeyShift) FrameSelected();
        else FrameAll();
    }

    // Compile
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        CompileShader();
    }
}

void ShaderGraphEditor::AddNodeAtPosition(const std::string& typeName, const ImVec2& pos) {
    if (!m_graph) return;

    auto node = ShaderNodeFactory::Instance().Create(typeName);
    if (!node) return;

    uint64_t nodeId = GetNextId();
    m_nodeVisuals[nodeId] = NodeVisualData{pos};

    m_graph->AddNode(node);
    m_needsRecompile = true;

    RecordAction(EditorAction::Type::CreateNode, ""); // TODO: Serialize
}

bool ShaderGraphEditor::CanCreateLink(uint64_t srcNode, const std::string& srcPin,
                                       uint64_t dstNode, const std::string& dstPin) {
    if (srcNode == dstNode) return false;

    // TODO: Type compatibility checking
    return true;
}

void ShaderGraphEditor::CreateLink(uint64_t srcNode, const std::string& srcPin,
                                    uint64_t dstNode, const std::string& dstPin) {
    NodeLink link;
    link.id = GetNextId();
    link.sourceNodeId = srcNode;
    link.sourcePin = srcPin;
    link.destNodeId = dstNode;
    link.destPin = dstPin;
    m_links.push_back(link);

    UpdateNodeConnections();
    m_needsRecompile = true;

    RecordAction(EditorAction::Type::CreateLink, "");
}

void ShaderGraphEditor::DeleteLink(uint64_t linkId) {
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                  [linkId](const NodeLink& l) { return l.id == linkId; }),
                  m_links.end());
    m_needsRecompile = true;
}

void ShaderGraphEditor::DeleteLinksForNode(uint64_t nodeId) {
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                  [nodeId](const NodeLink& l) {
                                      return l.sourceNodeId == nodeId || l.destNodeId == nodeId;
                                  }),
                  m_links.end());
}

void ShaderGraphEditor::CompileShader() {
    if (!m_graph) {
        m_compileError = "No graph to compile";
        return;
    }

    try {
        std::string compiled = m_graph->Compile();
        // Split compiled output into VS and FS (simplified - just use same for both for now)
        m_compiledVS = compiled;
        m_compiledFS = compiled;
        m_compileError.clear();
        m_needsRecompile = false;

        if (m_compiledCallback) {
            m_compiledCallback(m_compiledVS, m_compiledFS);
        }
    } catch (const std::exception& e) {
        m_compileError = e.what();
    }
}

void ShaderGraphEditor::Undo() {
    if (m_undoStack.empty()) return;

    auto action = m_undoStack.back();
    m_undoStack.pop_back();
    m_redoStack.push_back(action);

    // TODO: Apply undo
    m_needsRecompile = true;
}

void ShaderGraphEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto action = m_redoStack.back();
    m_redoStack.pop_back();
    m_undoStack.push_back(action);

    // TODO: Apply redo
    m_needsRecompile = true;
}

void ShaderGraphEditor::DeleteSelected() {
    for (uint64_t nodeId : m_selectedNodes) {
        DeleteLinksForNode(nodeId);
        m_nodeVisuals.erase(nodeId);
    }
    m_selectedNodes.clear();

    // TODO: Remove from graph
    m_needsRecompile = true;
}

void ShaderGraphEditor::DuplicateSelected() {
    // TODO: Implement duplication
}

void ShaderGraphEditor::SelectAll() {
    m_selectedNodes.clear();
    for (auto& [id, visual] : m_nodeVisuals) {
        visual.selected = true;
        m_selectedNodes.push_back(id);
    }
}

void ShaderGraphEditor::ClearSelection() {
    for (auto& [id, visual] : m_nodeVisuals) {
        visual.selected = false;
    }
    m_selectedNodes.clear();
}

void ShaderGraphEditor::FrameAll() {
    if (m_nodeVisuals.empty()) return;

    ImVec2 min(FLT_MAX, FLT_MAX);
    ImVec2 max(-FLT_MAX, -FLT_MAX);

    for (const auto& [id, visual] : m_nodeVisuals) {
        min.x = std::min(min.x, visual.position.x);
        min.y = std::min(min.y, visual.position.y);
        max.x = std::max(max.x, visual.position.x + visual.size.x);
        max.y = std::max(max.y, visual.position.y + visual.size.y);
    }

    ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    m_canvasOffset.x = canvasSize.x * 0.5f - center.x * m_zoom;
    m_canvasOffset.y = canvasSize.y * 0.5f - center.y * m_zoom;
}

void ShaderGraphEditor::FrameSelected() {
    if (m_selectedNodes.empty()) {
        FrameAll();
        return;
    }

    ImVec2 min(FLT_MAX, FLT_MAX);
    ImVec2 max(-FLT_MAX, -FLT_MAX);

    for (uint64_t id : m_selectedNodes) {
        auto it = m_nodeVisuals.find(id);
        if (it == m_nodeVisuals.end()) continue;

        min.x = std::min(min.x, it->second.position.x);
        min.y = std::min(min.y, it->second.position.y);
        max.x = std::max(max.x, it->second.position.x + it->second.size.x);
        max.y = std::max(max.y, it->second.position.y + it->second.size.y);
    }

    ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    m_canvasOffset.x = canvasSize.x * 0.5f - center.x * m_zoom;
    m_canvasOffset.y = canvasSize.y * 0.5f - center.y * m_zoom;
}

uint64_t ShaderGraphEditor::GetNextId() {
    return m_nextId++;
}

ImVec2 ShaderGraphEditor::ScreenToCanvas(const ImVec2& screenPos) {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    return ImVec2((screenPos.x - canvasPos.x - m_canvasOffset.x) / m_zoom,
                  (screenPos.y - canvasPos.y - m_canvasOffset.y) / m_zoom);
}

ImVec2 ShaderGraphEditor::CanvasToScreen(const ImVec2& canvasPos) {
    ImVec2 screenOrigin = ImGui::GetCursorScreenPos();
    return ImVec2(canvasPos.x * m_zoom + m_canvasOffset.x + screenOrigin.x,
                  canvasPos.y * m_zoom + m_canvasOffset.y + screenOrigin.y);
}

ImU32 ShaderGraphEditor::GetTypeColor(ShaderDataType type) {
    switch (type) {
        case ShaderDataType::Float: return IM_COL32(150, 150, 150, 255);
        case ShaderDataType::Vec2: return IM_COL32(150, 200, 100, 255);
        case ShaderDataType::Vec3: return IM_COL32(250, 200, 100, 255);
        case ShaderDataType::Vec4: return IM_COL32(100, 200, 250, 255);
        case ShaderDataType::Int: return IM_COL32(100, 200, 150, 255);
        case ShaderDataType::Bool: return IM_COL32(200, 100, 100, 255);
        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4: return IM_COL32(200, 150, 200, 255);
        case ShaderDataType::Sampler2D:
        case ShaderDataType::SamplerCube:
        case ShaderDataType::Sampler3D: return IM_COL32(250, 150, 100, 255);
        default: return IM_COL32(150, 150, 150, 255);
    }
}

const char* ShaderGraphEditor::GetCategoryIcon(NodeCategory category) {
    switch (category) {
        case NodeCategory::Input: return "[I]";
        case NodeCategory::Output: return "[O]";
        case NodeCategory::Parameter: return "[P]";
        case NodeCategory::Texture: return "[T]";
        case NodeCategory::MathBasic:
        case NodeCategory::MathAdvanced:
        case NodeCategory::MathTrig: return "[M]";
        case NodeCategory::MathVector: return "[V]";
        case NodeCategory::MathInterpolation: return "[~]";
        case NodeCategory::Utility: return "[U]";
        case NodeCategory::Logic: return "[?]";
        case NodeCategory::Color: return "[C]";
        case NodeCategory::Noise: return "[N]";
        case NodeCategory::Pattern: return "[#]";
        default: return "[ ]";
    }
}

void ShaderGraphEditor::RecordAction(EditorAction::Type type, const std::string& data) {
    EditorAction action{type, data};
    m_undoStack.push_back(action);
    m_redoStack.clear();

    if (m_undoStack.size() > MAX_UNDO_STACK) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void ShaderGraphEditor::UpdateNodeConnections() {
    // TODO: Update actual node connections from m_links
}

bool ShaderGraphEditor::SaveToFile(const std::string& path) {
    if (!m_graph) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << m_graph->ToJson();
    return true;
}

bool ShaderGraphEditor::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();

    m_ownedGraph = std::make_unique<ShaderGraph>();
    // TODO: Parse JSON and reconstruct graph
    m_graph = m_ownedGraph.get();

    return true;
}

// ============================================================================
// MaterialLibrary
// ============================================================================

void MaterialLibrary::Draw() {
    ImGui::Text("Material Library");
    ImGui::Separator();

    ImGui::InputTextWithHint("##libsearch", "Search...", &m_searchFilter[0], 256);

    for (const auto& mat : m_materials) {
        if (!m_searchFilter.empty()) {
            std::string nameLower = mat.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            std::string filterLower = m_searchFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            if (nameLower.find(filterLower) == std::string::npos) continue;
        }

        if (ImGui::Selectable(mat.name.c_str())) {
            // TODO: Load material
        }
    }
}

void MaterialLibrary::AddMaterial(const std::string& name, const std::string& category,
                                   const std::string& jsonPath) {
    m_materials.push_back({name, category, jsonPath, 0});
}

std::optional<std::string> MaterialLibrary::GetMaterialPath(const std::string& name) const {
    for (const auto& mat : m_materials) {
        if (mat.name == name) return mat.path;
    }
    return std::nullopt;
}

void MaterialLibrary::ScanDirectory(const std::string& path) {
    // TODO: Scan for .material.json files
}

// ============================================================================
// ShaderParameterInspector
// ============================================================================

void ShaderParameterInspector::Draw(ShaderGraph* graph) {
    if (!graph) return;

    ImGui::Text("Material Parameters");
    ImGui::Separator();

    // TODO: Iterate over parameter nodes and show editors
}

} // namespace Nova
