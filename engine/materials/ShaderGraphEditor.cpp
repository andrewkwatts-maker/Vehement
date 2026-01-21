/**
 * @file ShaderGraphEditor.cpp
 * @brief Implementation of visual shader graph editor
 */

#include "ShaderGraphEditor.hpp"
#include "../graphics/PreviewRenderer.hpp"
#include "../graphics/Material.hpp"
#include "../graphics/Shader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>

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

ShaderGraphEditor::ShaderGraphEditor()
    : m_previewRenderer(std::make_unique<PreviewRenderer>())
    , m_previewMaterial(std::make_shared<Material>())
{
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

ShaderGraphEditor::~ShaderGraphEditor() {
    if (m_previewRenderer) {
        m_previewRenderer->Shutdown();
    }
}

void ShaderGraphEditor::Initialize() {
    if (m_previewRenderer) {
        m_previewRenderer->Initialize();

        // Configure for material preview mode
        PreviewSettings settings = PreviewSettings::MaterialPreview();
        settings.interaction.autoRotate = true;
        settings.thumbnailSize = m_previewSize;
        m_previewRenderer->SetSettings(settings);
    }
}

void ShaderGraphEditor::SetGraph(ShaderGraph* graph) {
    m_graph = graph;
    m_ownedGraph.reset();
    m_nodeVisuals.clear();
    m_links.clear();
    m_selectedNodes.clear();
    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview recompile

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
    m_ownedGraph = std::make_shared<ShaderGraph>();
    m_graph = m_ownedGraph.get();
    m_nodeVisuals.clear();
    m_links.clear();
    m_selectedNodes.clear();
    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview recompile

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

    // File dialogs
    if (m_showOpenDialog) {
        ImGui::OpenPopup("Open Shader Graph");
        m_showOpenDialog = false;
    }
    if (ImGui::BeginPopupModal("Open Shader Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file path:");
        ImGui::InputText("##OpenPath", m_filePathBuffer, sizeof(m_filePathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (LoadFromFile(m_filePathBuffer)) {
                m_currentFilePath = m_filePathBuffer;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showSaveDialog) {
        ImGui::OpenPopup("Save Shader Graph");
        m_showSaveDialog = false;
    }
    if (ImGui::BeginPopupModal("Save Shader Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter file path:");
        ImGui::InputText("##SavePath", m_filePathBuffer, sizeof(m_filePathBuffer));
        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (SaveToFile(m_filePathBuffer)) {
                m_currentFilePath = m_filePathBuffer;
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

void ShaderGraphEditor::DrawMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) NewGraph();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                m_showOpenDialog = true;
                std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_currentFilePath.empty()) {
                    SaveToFile(m_currentFilePath);
                } else {
                    m_showSaveDialog = true;
                    std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
                }
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                m_showSaveDialog = true;
                std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Shader...")) {
                if (m_graph && !m_compiledVS.empty() && !m_compiledFS.empty()) {
                    // Export to console for now
                    spdlog::info("=== Vertex Shader ===\n{}", m_compiledVS);
                    spdlog::info("=== Fragment Shader ===\n{}", m_compiledFS);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undoStack.empty())) Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redoStack.empty())) Redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, !m_selectedNodes.empty())) {
                // Copy then delete
                if (!m_selectedNodes.empty() && m_graph) {
                    m_clipboard = m_graph->ToJson();  // Simplified: copy entire graph for selected
                    DeleteSelected();
                }
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, !m_selectedNodes.empty())) {
                if (!m_selectedNodes.empty() && m_graph) {
                    m_clipboard = m_graph->ToJson();  // Simplified: copy entire graph
                }
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, !m_clipboard.empty())) {
                // Paste is complex - would need proper node duplication
                spdlog::info("Paste: clipboard has {} chars", m_clipboard.size());
            }
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
    ImGui::PushFont(nullptr);
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

    // Multi-selection editing
    if (m_selectedNodes.size() > 1) {
        ImGui::Text("Selected: %zu nodes", m_selectedNodes.size());
        ImGui::Separator();
        ImGui::TextDisabled("(Multiple nodes selected)");
        ImGui::TextDisabled("Common properties:");

        // Check if all selected nodes are of the same type
        bool sameType = true;
        std::string firstType;
        if (!m_selectedNodes.empty() && m_graph) {
            int idx = 0;
            for (auto& [nodeId, visual] : m_nodeVisuals) {
                if (idx < static_cast<int>(m_graph->GetNodes().size())) {
                    auto& node = m_graph->GetNodes()[idx];
                    if (std::find(m_selectedNodes.begin(), m_selectedNodes.end(), nodeId) != m_selectedNodes.end()) {
                        if (firstType.empty()) {
                            firstType = node->GetTypeName();
                        } else if (firstType != node->GetTypeName()) {
                            sameType = false;
                            break;
                        }
                    }
                }
                idx++;
            }
        }

        if (sameType && !firstType.empty()) {
            ImGui::Text("Type: %s", firstType.c_str());
        } else {
            ImGui::TextDisabled("Mixed types");
        }
        ImGui::Separator();
    }

    ImGui::Text("Material Settings");
    ImGui::Separator();

    if (m_graph) {
        // Domain
        const char* domains[] = {"Surface", "Post Process", "Decal", "UI", "Volume", "SDF"};
        int domain = static_cast<int>(m_graph->GetDomain());
        if (ImGui::Combo("Domain", &domain, domains, IM_ARRAYSIZE(domains))) {
            m_graph->SetDomain(static_cast<MaterialDomain>(domain));
            m_needsRecompile = true;
            m_graphDirty = true;  // Mark for preview update
        }

        // Blend mode
        const char* blendModes[] = {"Opaque", "Masked", "Translucent", "Additive", "Modulate"};
        int blendMode = static_cast<int>(m_graph->GetBlendMode());
        if (ImGui::Combo("Blend Mode", &blendMode, blendModes, IM_ARRAYSIZE(blendModes))) {
            m_graph->SetBlendMode(static_cast<BlendMode>(blendMode));
            m_needsRecompile = true;
            m_graphDirty = true;  // Mark for preview update
        }

        // Shading model
        const char* shadingModels[] = {"Unlit", "Default Lit", "Subsurface", "Clear Coat", "Hair", "Eye", "Cloth", "Two Sided Foliage"};
        int shadingModel = static_cast<int>(m_graph->GetShadingModel());
        if (ImGui::Combo("Shading Model", &shadingModel, shadingModels, IM_ARRAYSIZE(shadingModels))) {
            m_graph->SetShadingModel(static_cast<ShadingModel>(shadingModel));
            m_needsRecompile = true;
            m_graphDirty = true;  // Mark for preview update
        }

        // Two sided
        bool twoSided = m_graph->IsTwoSided();
        if (ImGui::Checkbox("Two Sided", &twoSided)) {
            m_graph->SetTwoSided(twoSided);
            m_graphDirty = true;  // Mark for preview update
        }
    }
}

void ShaderGraphEditor::DrawPreviewPanel() {
    if (!m_showPreview) return;

    ImGui::Text("Preview");
    ImGui::Separator();

    // Auto-compile checkbox
    ImGui::Checkbox("Auto Compile", &m_autoCompile);
    ImGui::SameLine();
    if (ImGui::Button("Compile Now")) {
        CompileGraphToShader();
    }

    // Auto-compile when graph is dirty
    if (m_autoCompile && m_graphDirty) {
        CompileGraphToShader();
        m_graphDirty = false;
    }

    // Preview mesh type
    const char* meshTypes[] = {"Sphere", "Cube", "Plane", "Cylinder", "Torus"};
    if (ImGui::Combo("Mesh", &m_previewMeshType, meshTypes, IM_ARRAYSIZE(meshTypes))) {
        // Update preview shape
        if (m_previewRenderer) {
            PreviewSettings& settings = m_previewRenderer->GetSettings();
            switch (m_previewMeshType) {
                case 0: settings.shape = PreviewShape::Sphere; break;
                case 1: settings.shape = PreviewShape::Cube; break;
                case 2: settings.shape = PreviewShape::Plane; break;
                case 3: settings.shape = PreviewShape::Cylinder; break;
                case 4: settings.shape = PreviewShape::Torus; break;
            }
        }
    }

    // Auto-rotation toggle
    bool autoRotate = m_previewRenderer ? m_previewRenderer->GetSettings().interaction.autoRotate : true;
    if (ImGui::Checkbox("Auto Rotate", &autoRotate)) {
        if (m_previewRenderer) {
            m_previewRenderer->GetSettings().interaction.autoRotate = autoRotate;
        }
    }

    // Manual rotation slider (disabled when auto-rotate is on)
    if (!autoRotate) {
        ImGui::SliderFloat("Rotation", &m_previewRotation, 0.0f, 360.0f);
    }

    // Preview size slider
    int previewSizeInt = m_previewSize;
    if (ImGui::SliderInt("Size", &previewSizeInt, 128, 512)) {
        m_previewSize = previewSizeInt;
    }

    ImGui::Separator();

    // Render the preview
    if (m_previewRenderer && m_previewRenderer->IsInitialized()) {
        // Update frame time for auto-rotation
        static float lastTime = 0.0f;
        float currentTime = ImGui::GetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        m_previewRenderer->Update(deltaTime);

        // Set the compiled material if available
        if (m_compiledShader && m_previewMaterial) {
            m_previewRenderer->SetMaterial(m_previewMaterial);
        }

        // Render to framebuffer
        m_previewRenderer->Render({m_previewSize, m_previewSize});

        // Get the texture ID and display it
        uint32_t textureID = m_previewRenderer->GetPreviewTextureID();
        m_previewTexture = textureID;

        // Display in ImGui with interactive area for orbit controls
        ImVec2 previewSizeVec(static_cast<float>(m_previewSize), static_cast<float>(m_previewSize));

        // Display the preview image
        ImGui::Image((ImTextureID)(intptr_t)textureID, previewSizeVec, ImVec2(0, 1), ImVec2(1, 0));

        // Handle mouse interaction for orbit controls
        if (ImGui::IsItemHovered()) {
            ImGuiIO& io = ImGui::GetIO();

            // Mouse drag for rotation
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                PreviewInputEvent event;
                event.type = PreviewInputEvent::Type::MouseDrag;
                event.position = glm::vec2(io.MousePos.x, io.MousePos.y);
                event.delta = glm::vec2(io.MouseDelta.x, io.MouseDelta.y);
                event.button = 0;
                m_previewRenderer->HandleInput(event);
            }

            // Mouse drag for pan (right button)
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                PreviewInputEvent event;
                event.type = PreviewInputEvent::Type::MouseDrag;
                event.position = glm::vec2(io.MousePos.x, io.MousePos.y);
                event.delta = glm::vec2(io.MouseDelta.x, io.MouseDelta.y);
                event.button = 1;
                m_previewRenderer->HandleInput(event);
            }

            // Scroll for zoom
            if (std::abs(io.MouseWheel) > 0.0f) {
                PreviewInputEvent event;
                event.type = PreviewInputEvent::Type::Scroll;
                event.scrollDelta = io.MouseWheel;
                m_previewRenderer->HandleInput(event);
            }
        }

        // Reset camera button
        if (ImGui::Button("Reset Camera")) {
            m_previewRenderer->ResetCamera();
        }
    } else {
        // Preview not initialized - show placeholder
        ImVec2 previewSizeVec(static_cast<float>(m_previewSize), static_cast<float>(m_previewSize));
        ImGui::Dummy(previewSizeVec);
        ImGui::TextDisabled("Preview not initialized");
        ImGui::TextDisabled("Call Initialize() after OpenGL context is ready");
    }

    // Show compile status
    if (!m_compileError.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Compile Error:");
        ImGui::TextWrapped("%s", m_compileError.c_str());
    } else if (m_compiledShader) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Shader compiled successfully");
    }
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
    m_graphDirty = true;  // Mark for preview update

    // Serialize node data for undo
    std::string nodeData = "{\"type\":\"" + typeName + "\",\"nodeId\":" + std::to_string(nodeId) +
                           ",\"x\":" + std::to_string(pos.x) + ",\"y\":" + std::to_string(pos.y) + "}";
    RecordAction(EditorAction::Type::CreateNode, nodeData);
}

bool ShaderGraphEditor::CanCreateLink(uint64_t srcNode, const std::string& srcPin,
                                       uint64_t dstNode, const std::string& dstPin) {
    if (srcNode == dstNode) return false;

    // Type compatibility checking
    if (m_graph) {
        // Find the source and destination nodes
        ShaderNode* sourceNode = nullptr;
        ShaderNode* destNode = nullptr;
        int idx = 0;
        for (auto& [nodeId, visual] : m_nodeVisuals) {
            if (idx < static_cast<int>(m_graph->GetNodes().size())) {
                if (nodeId == srcNode) {
                    sourceNode = m_graph->GetNodes()[idx].get();
                }
                if (nodeId == dstNode) {
                    destNode = m_graph->GetNodes()[idx].get();
                }
            }
            idx++;
        }

        if (sourceNode && destNode) {
            const ShaderPin* outPin = sourceNode->GetOutput(srcPin);
            const ShaderPin* inPin = destNode->GetInput(dstPin);

            if (outPin && inPin) {
                // Use the AreTypesCompatible function from ShaderGraph.hpp
                return AreTypesCompatible(outPin->type, inPin->type);
            }
        }
    }

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
    m_graphDirty = true;  // Mark for preview update

    RecordAction(EditorAction::Type::CreateLink, "");
}

void ShaderGraphEditor::DeleteLink(uint64_t linkId) {
    m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
                                  [linkId](const NodeLink& l) { return l.id == linkId; }),
                  m_links.end());
    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview update
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

    // Apply undo based on action type
    switch (action.type) {
        case EditorAction::Type::CreateNode:
            // Undo node creation by removing the node
            if (!action.data.empty() && m_graph) {
                // Parse nodeId from JSON data: {"type":"...","nodeId":123,...}
                size_t idPos = action.data.find("\"nodeId\":");
                if (idPos != std::string::npos) {
                    uint64_t nodeId = std::stoull(action.data.substr(idPos + 9));
                    DeleteLinksForNode(nodeId);
                    m_nodeVisuals.erase(nodeId);
                }
            }
            break;
        case EditorAction::Type::DeleteNode:
        case EditorAction::Type::MoveNode:
        case EditorAction::Type::CreateLink:
        case EditorAction::Type::DeleteLink:
        case EditorAction::Type::ChangeProperty:
            // Restore state from action.data (JSON serialized state)
            // Full implementation would deserialize and restore the previous state
            break;
    }

    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview update
}

void ShaderGraphEditor::Redo() {
    if (m_redoStack.empty()) return;

    auto action = m_redoStack.back();
    m_redoStack.pop_back();
    m_undoStack.push_back(action);

    // Re-apply the action based on type
    switch (action.type) {
        case EditorAction::Type::CreateNode:
        case EditorAction::Type::DeleteNode:
        case EditorAction::Type::MoveNode:
        case EditorAction::Type::CreateLink:
        case EditorAction::Type::DeleteLink:
        case EditorAction::Type::ChangeProperty:
            // Restore state from action.data (JSON serialized state)
            if (!action.data.empty() && m_graph) {
                // Full undo/redo would deserialize action.data and restore graph state
                // For now, mark as dirty to trigger recompile
            }
            break;
    }

    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview update
}

void ShaderGraphEditor::DeleteSelected() {
    if (!m_graph) return;

    // Build a mapping from visual IDs to graph node indices
    std::vector<uint64_t> visualIds;
    for (const auto& [id, visual] : m_nodeVisuals) {
        visualIds.push_back(id);
    }
    std::sort(visualIds.begin(), visualIds.end());

    // Collect graph node IDs to remove (process in reverse to avoid index shifting)
    std::vector<NodeId> nodesToRemove;
    for (uint64_t visualId : m_selectedNodes) {
        auto it = std::find(visualIds.begin(), visualIds.end(), visualId);
        if (it != visualIds.end()) {
            size_t index = std::distance(visualIds.begin(), it);
            if (index < m_graph->GetNodes().size()) {
                nodesToRemove.push_back(m_graph->GetNodes()[index]->GetId());
            }
        }
        DeleteLinksForNode(visualId);
        m_nodeVisuals.erase(visualId);
    }

    // Remove nodes from the actual graph
    for (NodeId graphNodeId : nodesToRemove) {
        m_graph->RemoveNode(graphNodeId);
    }

    m_selectedNodes.clear();
    m_needsRecompile = true;
    m_graphDirty = true;  // Mark for preview update
}

void ShaderGraphEditor::DuplicateSelected() {
    if (!m_graph || m_selectedNodes.empty()) return;

    // Build a mapping from visual IDs to graph node indices
    std::vector<uint64_t> visualIds;
    for (const auto& [id, visual] : m_nodeVisuals) {
        visualIds.push_back(id);
    }
    std::sort(visualIds.begin(), visualIds.end());

    // Offset for duplicated nodes
    const ImVec2 duplicateOffset(50.0f, 50.0f);

    // Store new node IDs for selection
    std::vector<uint64_t> newNodeIds;

    for (uint64_t visualId : m_selectedNodes) {
        auto visualIt = m_nodeVisuals.find(visualId);
        if (visualIt == m_nodeVisuals.end()) continue;

        // Find corresponding graph node index
        auto it = std::find(visualIds.begin(), visualIds.end(), visualId);
        if (it == visualIds.end()) continue;

        size_t index = std::distance(visualIds.begin(), it);
        if (index >= m_graph->GetNodes().size()) continue;

        // Get the original node's type and create a duplicate
        const auto& originalNode = m_graph->GetNodes()[index];
        auto duplicateNode = ShaderNodeFactory::Instance().Create(originalNode->GetTypeName());
        if (!duplicateNode) continue;

        // Add to graph
        m_graph->AddNode(duplicateNode);

        // Create visual data with offset position
        uint64_t newVisualId = GetNextId();
        m_nodeVisuals[newVisualId] = NodeVisualData{
            ImVec2(visualIt->second.position.x + duplicateOffset.x,
                   visualIt->second.position.y + duplicateOffset.y)
        };

        newNodeIds.push_back(newVisualId);
    }

    // Clear old selection and select new nodes
    for (auto& [id, visual] : m_nodeVisuals) {
        visual.selected = false;
    }
    m_selectedNodes.clear();

    for (uint64_t newId : newNodeIds) {
        m_nodeVisuals[newId].selected = true;
        m_selectedNodes.push_back(newId);
    }

    m_needsRecompile = true;
    m_graphDirty = true;
    RecordAction(EditorAction::Type::CreateNode, "");
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
    if (!m_graph) return;

    // Build a mapping from visual IDs to graph node indices
    std::vector<uint64_t> visualIds;
    for (const auto& [id, visual] : m_nodeVisuals) {
        visualIds.push_back(id);
    }
    std::sort(visualIds.begin(), visualIds.end());

    // First, disconnect all inputs in the graph
    for (auto& node : m_graph->GetNodes()) {
        node->DisconnectAll();
    }

    // Now reconnect based on m_links
    for (const auto& link : m_links) {
        // Find source node index
        auto srcIt = std::find(visualIds.begin(), visualIds.end(), link.sourceNodeId);
        auto dstIt = std::find(visualIds.begin(), visualIds.end(), link.destNodeId);

        if (srcIt == visualIds.end() || dstIt == visualIds.end()) continue;

        size_t srcIndex = std::distance(visualIds.begin(), srcIt);
        size_t dstIndex = std::distance(visualIds.begin(), dstIt);

        if (srcIndex >= m_graph->GetNodes().size() ||
            dstIndex >= m_graph->GetNodes().size()) continue;

        auto& srcNode = m_graph->GetNodes()[srcIndex];
        auto& dstNode = m_graph->GetNodes()[dstIndex];

        // Connect in the ShaderGraph
        dstNode->Connect(link.destPin, srcNode, link.sourcePin);
    }
}

bool ShaderGraphEditor::CompileGraphToShader() {
    if (!m_graph) {
        m_compileError = "No graph to compile";
        return false;
    }

    try {
        // Compile the graph to get shader source
        std::string compiledSource = m_graph->Compile();

        // Store the compiled sources
        m_compiledVS = compiledSource;
        m_compiledFS = compiledSource;

        // Create or update the shader object
        if (!m_compiledShader) {
            m_compiledShader = std::make_shared<Shader>();
        }

        // Attempt to load the shader from the compiled source
        // The ShaderGraph compiles to a fragment shader; we'll use a default vertex shader
        // for preview rendering
        static const char* PREVIEW_VERTEX_SHADER = R"(
#version 450 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    mat3 TBN;
} vs_out;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = uNormalMatrix * aNormal;
    vs_out.TexCoords = aTexCoords;

    vec3 T = normalize(uNormalMatrix * aTangent);
    vec3 B = normalize(uNormalMatrix * aBitangent);
    vec3 N = normalize(vs_out.Normal);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = uProjection * uView * worldPos;
}
)";

        // The compiled fragment shader should come from the graph
        // For now, use the compiled output directly as the fragment shader
        // The graph Compile() method should produce a complete fragment shader
        bool shaderLoaded = m_compiledShader->LoadFromSource(PREVIEW_VERTEX_SHADER, m_compiledFS);

        if (!shaderLoaded) {
            m_compileError = "Failed to compile shader from graph output";
            m_compiledShader.reset();
            return false;
        }

        // Update the preview material with the new shader
        UpdatePreviewMaterial();

        m_compileError.clear();
        m_needsRecompile = false;

        // Invoke callback if set
        if (m_compiledCallback) {
            m_compiledCallback(m_compiledVS, m_compiledFS);
        }

        return true;

    } catch (const std::exception& e) {
        m_compileError = std::string("Compilation error: ") + e.what();
        m_compiledShader.reset();
        return false;
    }
}

void ShaderGraphEditor::UpdatePreviewMaterial() {
    if (!m_previewMaterial || !m_compiledShader) {
        return;
    }

    // Set the shader on the material
    m_previewMaterial->SetShader(m_compiledShader);

    // Set default PBR properties for preview
    m_previewMaterial->SetAlbedo(glm::vec3(0.8f, 0.8f, 0.8f));
    m_previewMaterial->SetMetallic(0.0f);
    m_previewMaterial->SetRoughness(0.5f);
    m_previewMaterial->SetAO(1.0f);
    m_previewMaterial->SetEmissive(glm::vec3(0.0f));

    // If the graph has material settings, apply them
    if (m_graph) {
        // Apply two-sided setting
        m_previewMaterial->SetTwoSided(m_graph->IsTwoSided());

        // Apply transparency based on blend mode
        BlendMode blendMode = m_graph->GetBlendMode();
        m_previewMaterial->SetTransparent(blendMode == BlendMode::Translucent ||
                                          blendMode == BlendMode::Additive ||
                                          blendMode == BlendMode::Modulate);
    }
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
    std::string jsonContent = buffer.str();

    // Try to load the graph from JSON
    auto loadedGraph = ShaderGraph::FromJson(jsonContent);
    if (!loadedGraph) {
        return false;
    }

    m_ownedGraph = std::move(loadedGraph);
    m_graph = m_ownedGraph.get();

    // Reinitialize visual data for the loaded nodes
    m_nodeVisuals.clear();
    m_links.clear();
    m_selectedNodes.clear();
    m_needsRecompile = true;
    m_graphDirty = true;

    if (m_graph) {
        // Initialize visual data for nodes based on their stored positions
        for (const auto& node : m_graph->GetNodes()) {
            uint64_t id = GetNextId();
            m_nodeVisuals[id] = NodeVisualData{
                ImVec2(node->GetPosition().x, node->GetPosition().y)
            };
        }

        // Reconstruct visual links from node connections
        int nodeIdx = 0;
        for (const auto& node : m_graph->GetNodes()) {
            auto nodeVisIt = m_nodeVisuals.begin();
            std::advance(nodeVisIt, nodeIdx);
            uint64_t nodeId = nodeVisIt->first;

            for (const auto& input : node->GetInputs()) {
                if (input.IsConnected()) {
                    auto connectedNode = input.connectedNode.lock();
                    if (connectedNode) {
                        // Find the visual ID for the connected node
                        int srcNodeIdx = 0;
                        for (const auto& srcNode : m_graph->GetNodes()) {
                            if (srcNode.get() == connectedNode.get()) {
                                auto srcVisIt = m_nodeVisuals.begin();
                                std::advance(srcVisIt, srcNodeIdx);
                                uint64_t srcNodeId = srcVisIt->first;

                                NodeLink link;
                                link.id = GetNextId();
                                link.sourceNodeId = srcNodeId;
                                link.sourcePin = input.connectedPinName;
                                link.destNodeId = nodeId;
                                link.destPin = input.name;
                                m_links.push_back(link);
                                break;
                            }
                            srcNodeIdx++;
                        }
                    }
                }
            }
            nodeIdx++;
        }
    }

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
            // Store the selected path and invoke callback if set
            m_selectedPath = mat.path;
            if (m_onMaterialSelected) {
                m_onMaterialSelected(mat.path);
            }
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
    // Scan for .material.json files in the given directory
    try {
        // Use C++17 filesystem to iterate through directory
        namespace fs = std::filesystem;

        if (!fs::exists(path) || !fs::is_directory(path)) {
            return;
        }

        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (!entry.is_regular_file()) continue;

            std::string filename = entry.path().filename().string();
            std::string extension = entry.path().extension().string();

            // Check for .material.json or .mat.json files
            if (filename.ends_with(".material.json") || filename.ends_with(".mat.json")) {
                // Extract material name from filename
                std::string materialName = entry.path().stem().string();
                // Remove extra extension (.material or .mat)
                size_t dotPos = materialName.rfind('.');
                if (dotPos != std::string::npos) {
                    materialName = materialName.substr(0, dotPos);
                }

                // Determine category from parent directory
                std::string category = "Default";
                if (entry.path().has_parent_path()) {
                    category = entry.path().parent_path().filename().string();
                }

                // Add to library (avoid duplicates)
                bool exists = false;
                for (const auto& mat : m_materials) {
                    if (mat.path == entry.path().string()) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    AddMaterial(materialName, category, entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        // Filesystem error, ignore
        (void)e;
    }
}

// ============================================================================
// ShaderParameterInspector
// ============================================================================

void ShaderParameterInspector::Draw(ShaderGraph* graph) {
    if (!graph) return;

    ImGui::Text("Material Parameters");
    ImGui::Separator();

    // Iterate over all nodes and find parameter nodes
    for (const auto& node : graph->GetNodes()) {
        if (node->GetCategory() != NodeCategory::Parameter) continue;

        // Handle ParameterNode specifically
        if (auto* paramNode = dynamic_cast<ParameterNode*>(node.get())) {
            const std::string& paramName = paramNode->GetParameterName();
            ShaderDataType paramType = paramNode->GetParameterType();

            ImGui::PushID(paramName.c_str());

            switch (paramType) {
                case ShaderDataType::Float: {
                    float value = 0.0f;
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<float>(it->second)) {
                        value = std::get<float>(it->second);
                    }
                    if (ImGui::DragFloat(paramName.c_str(), &value, 0.01f)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                case ShaderDataType::Vec2: {
                    glm::vec2 value(0.0f);
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<glm::vec2>(it->second)) {
                        value = std::get<glm::vec2>(it->second);
                    }
                    if (ImGui::DragFloat2(paramName.c_str(), &value.x, 0.01f)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                case ShaderDataType::Vec3: {
                    glm::vec3 value(0.0f);
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<glm::vec3>(it->second)) {
                        value = std::get<glm::vec3>(it->second);
                    }
                    if (ImGui::DragFloat3(paramName.c_str(), &value.x, 0.01f)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                case ShaderDataType::Vec4: {
                    glm::vec4 value(0.0f);
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<glm::vec4>(it->second)) {
                        value = std::get<glm::vec4>(it->second);
                    }
                    if (ImGui::ColorEdit4(paramName.c_str(), &value.x)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                case ShaderDataType::Int: {
                    int value = 0;
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<int>(it->second)) {
                        value = std::get<int>(it->second);
                    }
                    if (ImGui::DragInt(paramName.c_str(), &value)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                case ShaderDataType::Bool: {
                    bool value = false;
                    auto it = m_modifiedValues.find(paramName);
                    if (it != m_modifiedValues.end() && std::holds_alternative<bool>(it->second)) {
                        value = std::get<bool>(it->second);
                    }
                    if (ImGui::Checkbox(paramName.c_str(), &value)) {
                        m_modifiedValues[paramName] = value;
                    }
                    break;
                }
                default:
                    ImGui::TextDisabled("%s (unsupported type)", paramName.c_str());
                    break;
            }

            ImGui::PopID();
        }
        // Handle other parameter-category nodes (FloatConstant, VectorConstant, ColorConstant)
        else if (auto* floatNode = dynamic_cast<FloatConstantNode*>(node.get())) {
            ImGui::PushID(static_cast<int>(node->GetId()));
            float value = floatNode->GetValue();
            if (ImGui::DragFloat(node->GetDisplayName().c_str(), &value, 0.01f)) {
                floatNode->SetValue(value);
            }
            ImGui::PopID();
        }
        else if (auto* vecNode = dynamic_cast<VectorConstantNode*>(node.get())) {
            ImGui::PushID(static_cast<int>(node->GetId()));
            glm::vec4 value = vecNode->GetValue();
            if (ImGui::DragFloat4(node->GetDisplayName().c_str(), &value.x, 0.01f)) {
                vecNode->SetValue(value);
            }
            ImGui::PopID();
        }
        else if (auto* colorNode = dynamic_cast<ColorConstantNode*>(node.get())) {
            ImGui::PushID(static_cast<int>(node->GetId()));
            glm::vec4 value = colorNode->GetColor();
            if (ImGui::ColorEdit4(node->GetDisplayName().c_str(), &value.x)) {
                colorNode->SetColor(value);
            }
            ImGui::PopID();
        }
    }

    // Also show graph-level parameters
    const auto& params = graph->GetParameters();
    if (!params.empty()) {
        ImGui::Separator();
        ImGui::Text("Exposed Parameters");

        for (const auto& param : params) {
            ImGui::PushID(param.name.c_str());

            switch (param.type) {
                case ShaderDataType::Float: {
                    float value = param.defaultValue.index() == 0 ?
                                  std::get<float>(param.defaultValue) : 0.0f;
                    ImGui::SliderFloat(param.displayName.c_str(), &value, param.minValue, param.maxValue);
                    break;
                }
                case ShaderDataType::Vec3: {
                    glm::vec3 value = std::holds_alternative<glm::vec3>(param.defaultValue) ?
                                      std::get<glm::vec3>(param.defaultValue) : glm::vec3(0.0f);
                    ImGui::ColorEdit3(param.displayName.c_str(), &value.x);
                    break;
                }
                case ShaderDataType::Vec4: {
                    glm::vec4 value = std::holds_alternative<glm::vec4>(param.defaultValue) ?
                                      std::get<glm::vec4>(param.defaultValue) : glm::vec4(0.0f);
                    ImGui::ColorEdit4(param.displayName.c_str(), &value.x);
                    break;
                }
                default:
                    ImGui::TextDisabled("%s", param.displayName.c_str());
                    break;
            }

            ImGui::PopID();
        }
    }
}

} // namespace Nova
