#include "StandaloneEditor.hpp"
#include "AssetBrowser.hpp"
#include "EditorCommand.hpp"
#include "ModernUI.hpp"
#include "WorldMapEditor.hpp"
#include "LocalMapEditor.hpp"
#include "PCGGraphEditor.hpp"
#include "core/Engine.hpp"
#include "core/Window.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/debug/DebugDraw.hpp"
#include "input/InputManager.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>

// STB Image for heightmap import/export (implementation is in Texture.cpp)
#include <stb_image.h>
#include <stb_image_write.h>
#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#undef GetCurrentDirectory
#endif


StandaloneEditor::StandaloneEditor() = default;
StandaloneEditor::~StandaloneEditor() = default;

bool StandaloneEditor::Initialize() {
    if (m_initialized) {
        return true;
    }

    spdlog::info("Initializing Standalone Editor");

    // Enable ImGui docking
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Apply custom theme
    ApplyEditorTheme();

    // Setup default panel layout
    SetupDefaultLayout();

    // Create default map (spherical Earth world)
    m_worldType = WorldType::Spherical;
    m_worldRadius = 6371.0f; // Earth radius
    NewWorldMap();

    // Initialize command history for undo/redo
    m_commandHistory = std::make_unique<CommandHistory>();

    // Initialize asset browser
    m_assetBrowser = std::make_unique<AssetBrowser>();
    if (!m_assetBrowser->Initialize(m_assetDirectory)) {
        spdlog::warn("AssetBrowser initialization failed, but continuing with editor startup");
    }

    m_initialized = true;
    spdlog::info("Standalone Editor initialized");
    return true;
}

void StandaloneEditor::SetupDefaultLayout() {
    m_panelLayouts.clear();

    // Default layout:
    // - Viewport: Center (fills remaining space)
    // - Tools: Left side
    // - Content Browser: Bottom
    // - Details: Right side
    // - Material Editor: Floating (hidden by default)
    // - Engine Stats: Floating (shown if debug enabled)

    m_panelLayouts.push_back({PanelID::Viewport, DockZone::Center, 1.0f, true});
    m_panelLayouts.push_back({PanelID::Tools, DockZone::Left, 1.0f, true});
    m_panelLayouts.push_back({PanelID::ContentBrowser, DockZone::Bottom, 1.0f, true});
    m_panelLayouts.push_back({PanelID::Details, DockZone::Right, 1.0f, true});
    m_panelLayouts.push_back({PanelID::MaterialEditor, DockZone::Floating, 1.0f, false});
    m_panelLayouts.push_back({PanelID::EngineStats, DockZone::Floating, 1.0f, false});
}

void StandaloneEditor::ApplyEditorTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Inspired by mystical dark theme with gold/purple/blue accents
    // Base colors - deep dark with slight warmth
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.10f, 0.10f, 0.14f, 0.90f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.10f, 0.10f, 0.15f, 0.95f);

    // Borders - subtle gold glow
    colors[ImGuiCol_Border]                 = ImVec4(0.60f, 0.50f, 0.20f, 0.40f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);

    // Frame backgrounds - nested panels
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.20f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.25f, 0.20f, 0.35f, 0.90f);  // Purple tint
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.30f, 0.25f, 0.45f, 0.95f);

    // Title bars - gold gradient feeling
    colors[ImGuiCol_TitleBg]                = ImVec4(0.15f, 0.12f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.35f, 0.28f, 0.12f, 1.00f);  // Gold
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.12f, 0.10f, 0.08f, 0.85f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.10f, 0.10f, 0.14f, 0.90f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.40f, 0.35f, 0.20f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.55f, 0.48f, 0.25f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.70f, 0.60f, 0.30f, 1.00f);

    // Check marks and sliders - cyan accent
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.80f, 0.82f, 1.00f);  // Cyan
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.50f, 0.45f, 0.25f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.75f, 0.65f, 0.35f, 1.00f);

    // Buttons - purple/blue gradient feeling
    colors[ImGuiCol_Button]                 = ImVec4(0.25f, 0.20f, 0.35f, 0.80f);  // Purple
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.28f, 0.50f, 0.90f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.45f, 0.35f, 0.65f, 1.00f);

    // Headers - collapsible sections
    colors[ImGuiCol_Header]                 = ImVec4(0.28f, 0.23f, 0.38f, 0.75f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.35f, 0.28f, 0.48f, 0.85f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.42f, 0.33f, 0.58f, 0.95f);

    // Separators
    colors[ImGuiCol_Separator]              = ImVec4(0.50f, 0.45f, 0.25f, 0.30f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.65f, 0.58f, 0.32f, 0.50f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.80f, 0.70f, 0.40f, 0.70f);

    // Resize grip
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.40f, 0.35f, 0.20f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.55f, 0.48f, 0.25f, 0.60f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.70f, 0.60f, 0.30f, 0.90f);

    // Tabs
    colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.15f, 0.12f, 0.90f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.38f, 0.30f, 0.15f, 0.95f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.35f, 0.28f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.12f, 0.10f, 0.85f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.25f, 0.20f, 0.10f, 0.90f);

    // Table
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.20f, 0.18f, 0.15f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.45f, 0.40f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.30f, 0.27f, 0.18f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    // Text selection
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.45f, 0.35f, 0.60f, 0.45f);

    // Drag and drop
    colors[ImGuiCol_DragDropTarget]         = ImVec4(0.00f, 0.80f, 0.82f, 0.90f);  // Cyan

    // Nav highlight
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.65f, 0.55f, 0.30f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

    // Modal
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.10f, 0.15f, 0.60f);

    // Style adjustments - more compact
    style.WindowPadding                     = ImVec2(8, 8);
    style.FramePadding                      = ImVec2(5, 3);
    style.CellPadding                       = ImVec2(4, 2);
    style.ItemSpacing                       = ImVec2(8, 4);
    style.ItemInnerSpacing                  = ImVec2(4, 4);
    style.TouchExtraPadding                 = ImVec2(0, 0);
    style.IndentSpacing                     = 18;
    style.ScrollbarSize                     = 14;
    style.GrabMinSize                       = 10;

    // Borders and rounding - subtle glow effect
    style.WindowBorderSize                  = 1;
    style.ChildBorderSize                   = 1;
    style.PopupBorderSize                   = 1;
    style.FrameBorderSize                   = 1;
    style.TabBorderSize                     = 0;

    style.WindowRounding                    = 6;
    style.ChildRounding                     = 4;
    style.FrameRounding                     = 4;
    style.PopupRounding                     = 4;
    style.ScrollbarRounding                 = 8;
    style.GrabRounding                      = 4;
    style.LogSliderDeadzone                 = 4;
    style.TabRounding                       = 4;

    // Window title alignment
    style.WindowTitleAlign                  = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition          = ImGuiDir_Right;
    style.ColorButtonPosition               = ImGuiDir_Right;

    // Misc
    style.Alpha                             = 1.0f;
    style.DisabledAlpha                     = 0.5f;
    style.AntiAliasedLines                  = true;
    style.AntiAliasedLinesUseTex            = true;
    style.AntiAliasedFill                   = true;
}

void StandaloneEditor::Shutdown() {
    spdlog::info("Shutting down Standalone Editor");
    m_initialized = false;
}

void StandaloneEditor::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update camera
    auto& input = Nova::Engine::Instance().GetInput();

    // Camera rotation with arrow keys
    if (input.IsKeyDown(Nova::Key::Left)) {
        m_cameraAngle -= deltaTime * 90.0f;
    }
    if (input.IsKeyDown(Nova::Key::Right)) {
        m_cameraAngle += deltaTime * 90.0f;
    }

    // Camera zoom with Page Up/Down
    if (input.IsKeyDown(Nova::Key::PageUp)) {
        m_cameraDistance = std::max(5.0f, m_cameraDistance - deltaTime * 20.0f);
    }
    if (input.IsKeyDown(Nova::Key::PageDown)) {
        m_cameraDistance = std::min(100.0f, m_cameraDistance + deltaTime * 20.0f);
    }

    // Calculate camera position
    float rad = glm::radians(m_cameraAngle);
    m_editorCameraPos.x = m_cameraDistance * cos(rad);
    m_editorCameraPos.z = m_cameraDistance * sin(rad);
    m_editorCameraPos.y = m_cameraDistance * 0.7f;

    // Tool keyboard shortcuts (only when not typing in text fields)
    if (!ImGui::GetIO().WantTextInput) {
        // Help shortcut
        if (input.IsKeyPressed(Nova::Key::F1)) {
            m_showControlsDialog = true;
        }

        // File menu shortcuts
        if (input.IsKeyDown(Nova::Key::LeftControl) || input.IsKeyDown(Nova::Key::RightControl)) {
            if (input.IsKeyDown(Nova::Key::LeftShift) || input.IsKeyDown(Nova::Key::RightShift)) {
                // Ctrl+Shift+N - New World Map
                if (input.IsKeyPressed(Nova::Key::N)) {
                    NewWorldMap();
                }
                // Ctrl+Shift+S - Save As
                if (input.IsKeyPressed(Nova::Key::S)) {
                    m_showSaveMapDialog = true;
                }
            } else {
                // Ctrl+N - New Local Map
                if (input.IsKeyPressed(Nova::Key::N)) {
                    m_showNewMapDialog = true;
                }
                // Ctrl+O - Open Map
                if (input.IsKeyPressed(Nova::Key::O)) {
                    m_showLoadMapDialog = true;
                }
                // Ctrl+S - Save Map
                if (input.IsKeyPressed(Nova::Key::S)) {
                    if (!m_currentMapPath.empty()) {
                        SaveMap(m_currentMapPath);
                    } else {
                        m_showSaveMapDialog = true;
                    }
                }

                // Edit menu shortcuts
                // Ctrl+Z - Undo
                if (input.IsKeyPressed(Nova::Key::Z)) {
                    if (m_commandHistory && m_commandHistory->CanUndo()) {
                        m_commandHistory->Undo();
                    }
                }
                // Ctrl+Y - Redo
                if (input.IsKeyPressed(Nova::Key::Y)) {
                    if (m_commandHistory && m_commandHistory->CanRedo()) {
                        m_commandHistory->Redo();
                    }
                }
                // Ctrl+X - Cut
                if (input.IsKeyPressed(Nova::Key::X)) {
                    if (m_selectedObjectIndex >= 0) {
                        CopySelectedObjects();
                        DeleteSelectedObjects();
                    }
                }
                // Ctrl+C - Copy
                if (input.IsKeyPressed(Nova::Key::C)) {
                    if (m_selectedObjectIndex >= 0) {
                        CopySelectedObjects();
                    }
                }
                // Ctrl+V - Paste
                if (input.IsKeyPressed(Nova::Key::V)) {
                    spdlog::warn("Paste not yet implemented");
                }
                // Ctrl+A - Select All
                if (input.IsKeyPressed(Nova::Key::A)) {
                    SelectAllObjects();
                }
            }
        }

        // Edit mode shortcuts
        if (input.IsKeyPressed(Nova::Key::Q)) {
            SetEditMode(EditMode::ObjectSelect);
            m_transformTool = TransformTool::None;
        }
        if (input.IsKeyPressed(Nova::Key::Num1)) {
            SetEditMode(EditMode::TerrainPaint);
        }
        if (input.IsKeyPressed(Nova::Key::Num2)) {
            SetEditMode(EditMode::TerrainSculpt);
        }

        // Transform tool shortcuts (only in ObjectSelect mode)
        if (m_editMode == EditMode::ObjectSelect) {
            if (input.IsKeyPressed(Nova::Key::W)) {
                SetTransformTool((m_transformTool == TransformTool::Move) ? TransformTool::None : TransformTool::Move);
            }
            if (input.IsKeyPressed(Nova::Key::E)) {
                SetTransformTool((m_transformTool == TransformTool::Rotate) ? TransformTool::None : TransformTool::Rotate);
            }
            if (input.IsKeyPressed(Nova::Key::R)) {
                SetTransformTool((m_transformTool == TransformTool::Scale) ? TransformTool::None : TransformTool::Scale);
            }
            // Toggle grid snapping with G key (or hold Ctrl while dragging)
            if (input.IsKeyPressed(Nova::Key::G)) {
                m_snapToGrid = !m_snapToGrid;
                spdlog::info("Grid snapping: {}", m_snapToGrid ? "ON" : "OFF");
            }
            // Hold Ctrl for temporary snap override during drag
            if (m_gizmoDragging) {
                bool ctrlHeld = input.IsKeyDown(Nova::Key::LeftControl) || input.IsKeyDown(Nova::Key::RightControl);
                m_snapToGridEnabled = ctrlHeld ? !m_snapToGrid : false; // Toggle behavior when Ctrl held
            }
        }

        // Brush size adjustment
        if (m_editMode == EditMode::TerrainPaint || m_editMode == EditMode::TerrainSculpt) {
            if (input.IsKeyPressed(Nova::Key::LeftBracket)) {
                m_brushSize = std::max(1, m_brushSize - 1);
            }
            if (input.IsKeyPressed(Nova::Key::RightBracket)) {
                m_brushSize = std::min(20, m_brushSize + 1);
            }
        }
    }
    
    // Terrain painting with mouse
    if (m_editMode == EditMode::TerrainPaint) {
        if (input.IsMouseButtonDown(Nova::MouseButton::Left)) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                auto mousePos = input.GetMousePosition();
                // Convert screen position to terrain coordinates using ray-plane intersection
                glm::vec3 rayDir = ScreenToWorldRay(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));
                glm::vec3 rayOrigin = m_currentCamera ? m_currentCamera->GetPosition() : m_editorCameraPos;

                // Intersect ray with terrain plane (Y=0 for flat terrain)
                float planeY = 0.0f;
                if (std::abs(rayDir.y) > 0.0001f) {
                    float t = (planeY - rayOrigin.y) / rayDir.y;
                    if (t > 0.0f) {
                        glm::vec3 hitPoint = rayOrigin + rayDir * t;
                        // Convert world position to terrain grid coordinates
                        int terrainX = static_cast<int>(std::floor(hitPoint.x / m_gridSize + m_mapWidth * 0.5f));
                        int terrainY = static_cast<int>(std::floor(hitPoint.z / m_gridSize + m_mapHeight * 0.5f));
                        if (terrainX >= 0 && terrainX < m_mapWidth && terrainY >= 0 && terrainY < m_mapHeight) {
                            PaintTerrain(terrainX, terrainY);
                        }
                    }
                }
            }
        }
    }

    // Terrain sculpting with mouse
    if (m_editMode == EditMode::TerrainSculpt) {
        auto convertMouseToTerrain = [this](const glm::vec2& mousePos, int& outX, int& outY) -> bool {
            glm::vec3 rayDir = ScreenToWorldRay(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));
            glm::vec3 rayOrigin = m_currentCamera ? m_currentCamera->GetPosition() : m_editorCameraPos;

            // Intersect ray with terrain plane (Y=0 for flat terrain)
            float planeY = 0.0f;
            if (std::abs(rayDir.y) > 0.0001f) {
                float t = (planeY - rayOrigin.y) / rayDir.y;
                if (t > 0.0f) {
                    glm::vec3 hitPoint = rayOrigin + rayDir * t;
                    outX = static_cast<int>(std::floor(hitPoint.x / m_gridSize + m_mapWidth * 0.5f));
                    outY = static_cast<int>(std::floor(hitPoint.z / m_gridSize + m_mapHeight * 0.5f));
                    return (outX >= 0 && outX < m_mapWidth && outY >= 0 && outY < m_mapHeight);
                }
            }
            return false;
        };

        if (input.IsMouseButtonDown(Nova::MouseButton::Left)) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                auto mousePos = input.GetMousePosition();
                // Positive strength for raising
                float strength = 1.0f * m_brushStrength;
                int terrainX, terrainY;
                if (convertMouseToTerrain(mousePos, terrainX, terrainY)) {
                    SculptTerrain(terrainX, terrainY, strength);
                }
            }
        }
        // Lower terrain with right mouse button
        if (input.IsMouseButtonDown(Nova::MouseButton::Right)) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                auto mousePos = input.GetMousePosition();
                // Negative strength for lowering
                float strength = -1.0f * m_brushStrength;
                int terrainX, terrainY;
                if (convertMouseToTerrain(mousePos, terrainX, terrainY)) {
                    SculptTerrain(terrainX, terrainY, strength);
                }
            }
        }
    }

    // Object selection with mouse click
    if (m_editMode == EditMode::ObjectSelect) {
        // Only handle object selection if not dragging gizmo
        if (input.IsMouseButtonPressed(Nova::MouseButton::Left) && !m_gizmoDragging) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                auto mousePos = input.GetMousePosition();
                SelectObjectAtScreenPos(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));
            }
        }
        // Clear selection with Escape or Delete
        if (input.IsKeyPressed(Nova::Key::Escape)) {
            ClearSelection();
        }
        if (input.IsKeyPressed(Nova::Key::Delete)) {
            DeleteSelectedObjects();
        }

        // Update gizmo interaction
        UpdateGizmoInteraction(deltaTime);
    }
}

void StandaloneEditor::RenderUI() {
    // Create main dockspace window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // Create DockSpace
    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    
    // Setup default docking layout on first run
    SetupDefaultDockLayout();
    
    // Render menu bar
    RenderMenuBar();
    
    ImGui::End();
    
    // Now render each panel as a separate window that can dock
    for (auto& layout : m_panelLayouts) {
        if (layout.isVisible) {
            RenderPanelWindow(layout);
        }
    }
    
    // Dialogs (always on top)
    if (m_showNewMapDialog) ShowNewMapDialog();
    if (m_showLoadMapDialog) ShowLoadMapDialog();
    if (m_showSaveMapDialog) ShowSaveMapDialog();
    if (m_showAboutDialog) ShowAboutDialog();
    if (m_showControlsDialog) ShowControlsDialog();
}


void StandaloneEditor::SetupDefaultDockLayout() {
    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
    
    // Only set up layout if it hasn't been initialized
    static bool first_time = true;
    if (first_time) {
        first_time = false;
        
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);
        
        // Split the dockspace
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
        ImGuiID dock_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
        
        // Dock windows to their default locations
        ImGui::DockBuilderDockWindow("Tools", dock_left);
        ImGui::DockBuilderDockWindow("Details", dock_right);
        ImGui::DockBuilderDockWindow("Content Browser", dock_bottom);
        ImGui::DockBuilderDockWindow("Viewport", dockspace_id); // Center
        
        ImGui::DockBuilderFinish(dockspace_id);
    }
}


void StandaloneEditor::RenderPanelWindow(PanelLayout& layout) {
    const char* name = GetPanelName(layout.id);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    
    if (ImGui::Begin(name, &layout.isVisible, flags)) {
        RenderPanelContent(layout.id);
    }
    ImGui::End();
}

void StandaloneEditor::RenderMenuBar() {
    // Render menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("New")) {
                if (ImGui::MenuItem("World Map", "Ctrl+Shift+N")) {
                    NewWorldMap();
                }
                if (ImGui::MenuItem("Local Map", "Ctrl+N")) {
                    m_showNewMapDialog = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Open Map", "Ctrl+O")) {
                m_showLoadMapDialog = true;
            }
            if (ImGui::MenuItem("Save Map", "Ctrl+S")) {
                if (!m_currentMapPath.empty()) {
                    SaveMap(m_currentMapPath);
                } else {
                    m_showSaveMapDialog = true;
                }
            }
            if (ImGui::MenuItem("Save Map As", "Ctrl+Shift+S")) {
                m_showSaveMapDialog = true;
            }
            ImGui::Separator();

            // Import/Export submenu
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = OpenNativeFileDialog("Image Files (*.png;*.jpg;*.tga;*.bmp)\0*.png;*.jpg;*.tga;*.bmp\0All Files\0*.*\0", "Import Heightmap");
                    if (!path.empty()) {
                        ImportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("Heightmap...")) {
                    std::string path = SaveNativeFileDialog("PNG Image (*.png)\0*.png\0All Files\0*.*\0", "Export Heightmap", ".png");
                    if (!path.empty()) {
                        ExportHeightmap(path);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            // Recent files
            if (ImGui::BeginMenu("Recent Files")) {
                if (m_recentFiles.empty()) {
                    ImGui::MenuItem("(No recent files)", nullptr, false, false);
                } else {
                    for (const auto& recentFile : m_recentFiles) {
                        if (ImGui::MenuItem(recentFile.c_str())) {
                            LoadMap(recentFile);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent Files")) {
                        ClearRecentFiles();
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Exit Editor", "Alt+F4")) {
                Nova::Engine::Instance().RequestShutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            bool canUndo = m_commandHistory && m_commandHistory->CanUndo();
            bool canRedo = m_commandHistory && m_commandHistory->CanRedo();

            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                if (m_commandHistory) {
                    m_commandHistory->Undo();
                }
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
                if (m_commandHistory) {
                    m_commandHistory->Redo();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
                DeleteSelectedObjects();
            }
            if (ImGui::MenuItem("Copy", "Ctrl+C", false, m_selectedObjectIndex >= 0)) {
                CopySelectedObjects();
            }
            if (ImGui::MenuItem("Paste", "Ctrl+V", false, false)) {
                // Clipboard paste not implemented yet
                spdlog::warn("Paste not yet implemented");
            }
            if (ImGui::MenuItem("Delete", "Del", false, m_selectedObjectIndex >= 0)) {
                DeleteSelectedObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                SelectAllObjects();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Map Properties")) {
                m_showMapPropertiesDialog = true;
            }
            if (ImGui::MenuItem("Preferences...")) {
                m_showSettingsDialog = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            // Panel visibility toggles
            ImGui::Text("Panels");
            ImGui::Separator();

            // Viewport is always visible (grayed out)
            ImGui::BeginDisabled();
            bool viewportVisible = true;
            ImGui::MenuItem("Viewport", nullptr, &viewportVisible);
            ImGui::EndDisabled();

            // Details Panel
            if (ImGui::MenuItem("Details Panel", nullptr, &m_showDetailsPanel)) {
                // Update panel layout visibility
                for (auto& layout : m_panelLayouts) {
                    if (layout.id == PanelID::Details) {
                        layout.isVisible = m_showDetailsPanel;
                        break;
                    }
                }
            }

            // Tools Panel
            if (ImGui::MenuItem("Tools Panel", nullptr, &m_showToolsPanel)) {
                for (auto& layout : m_panelLayouts) {
                    if (layout.id == PanelID::Tools) {
                        layout.isVisible = m_showToolsPanel;
                        break;
                    }
                }
            }

            // Content Browser
            if (ImGui::MenuItem("Content Browser", nullptr, &m_showContentBrowser)) {
                for (auto& layout : m_panelLayouts) {
                    if (layout.id == PanelID::ContentBrowser) {
                        layout.isVisible = m_showContentBrowser;
                        break;
                    }
                }
            }

            // Material Editor
            if (ImGui::MenuItem("Material Editor", nullptr, &m_showMaterialEditor)) {
                for (auto& layout : m_panelLayouts) {
                    if (layout.id == PanelID::MaterialEditor) {
                        layout.isVisible = m_showMaterialEditor;
                        break;
                    }
                }
            }

            // World Map Editor (if spherical world)
            if (m_worldType == WorldType::Spherical) {
                ImGui::MenuItem("World Map Editor", nullptr, &m_showWorldMapEditor);
            }

            // PCG Graph Editor
            ImGui::MenuItem("PCG Graph Editor", nullptr, &m_showPCGEditor);

            ImGui::Separator();

            // Rendering Options submenu
            if (ImGui::BeginMenu("Rendering Options")) {
                ImGui::MenuItem("Show Grid", nullptr, &m_showGrid);
                ImGui::MenuItem("Show Gizmos", nullptr, &m_showGizmos);
                ImGui::MenuItem("Show Wireframe", nullptr, &m_showWireframe);

                // Show Spherical Grid only for spherical worlds
                if (m_worldType == WorldType::Spherical) {
                    ImGui::MenuItem("Show Spherical Grid", nullptr, &m_showSphericalGrid);
                }

                ImGui::MenuItem("Show Normals", nullptr, &m_showNormals);
                ImGui::Separator();
                ImGui::MenuItem("Snap to Grid", nullptr, &m_snapToGrid);
                ImGui::EndMenu();
            }

            ImGui::Separator();

            // Camera submenu
            if (ImGui::BeginMenu("Camera")) {
                if (ImGui::MenuItem("Reset Camera")) {
                    // Reset to default camera position
                    m_editorCameraPos = m_defaultCameraPos;
                    m_editorCameraTarget = m_defaultCameraTarget;
                    m_cameraDistance = 30.0f;
                    m_cameraAngle = 45.0f;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Top View", nullptr, false)) {
                    // Set orthographic top-down view
                    m_editorCameraPos = glm::vec3(m_mapWidth * m_gridSize / 2.0f, 50.0f, m_mapHeight * m_gridSize / 2.0f);
                    m_editorCameraTarget = glm::vec3(m_mapWidth * m_gridSize / 2.0f, 0.0f, m_mapHeight * m_gridSize / 2.0f);
                    m_cameraAngle = 90.0f;
                }

                if (ImGui::MenuItem("Front View", nullptr, false)) {
                    // Set orthographic front view
                    m_editorCameraPos = glm::vec3(m_mapWidth * m_gridSize / 2.0f, 15.0f, m_mapHeight * m_gridSize + 30.0f);
                    m_editorCameraTarget = glm::vec3(m_mapWidth * m_gridSize / 2.0f, 0.0f, m_mapHeight * m_gridSize / 2.0f);
                    m_cameraAngle = 0.0f;
                }

                if (ImGui::MenuItem("Free Camera", nullptr, false)) {
                    // Set perspective free-look camera
                    m_editorCameraPos = m_defaultCameraPos;
                    m_editorCameraTarget = m_defaultCameraTarget;
                    m_cameraAngle = 45.0f;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            // Edit Mode Tools
            if (ImGui::MenuItem("Object Select", "Q", m_editMode == EditMode::ObjectSelect)) {
                SetEditMode(EditMode::ObjectSelect);
                m_transformTool = TransformTool::None;
            }

            ImGui::Separator();
            ImGui::Text("Transform Tools");

            if (ImGui::MenuItem("Move", "W", m_transformTool == TransformTool::Move)) {
                SetTransformTool(TransformTool::Move);
            }
            if (ImGui::MenuItem("Rotate", "E", m_transformTool == TransformTool::Rotate)) {
                SetTransformTool(TransformTool::Rotate);
            }
            if (ImGui::MenuItem("Scale", "R", m_transformTool == TransformTool::Scale)) {
                SetTransformTool(TransformTool::Scale);
            }

            ImGui::Separator();
            ImGui::Text("Terrain Tools");

            if (ImGui::MenuItem("Terrain Paint", "1", m_editMode == EditMode::TerrainPaint)) {
                SetEditMode(EditMode::TerrainPaint);
            }
            if (ImGui::MenuItem("Terrain Sculpt", "2", m_editMode == EditMode::TerrainSculpt)) {
                SetEditMode(EditMode::TerrainSculpt);
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Tool Settings")) {
                ImGui::Text("Brush Settings");
                ImGui::Separator();

                ImGui::SliderInt("Brush Size", &m_brushSize, 1, 100);
                ImGui::SliderFloat("Brush Strength", &m_brushStrength, 0.1f, 10.0f);

                ImGui::Spacing();
                ImGui::Text("Brush Falloff");
                if (ImGui::RadioButton("Linear", m_brushFalloff == BrushFalloff::Linear)) {
                    m_brushFalloff = BrushFalloff::Linear;
                }
                if (ImGui::RadioButton("Smooth", m_brushFalloff == BrushFalloff::Smooth)) {
                    m_brushFalloff = BrushFalloff::Smooth;
                }
                if (ImGui::RadioButton("Spherical", m_brushFalloff == BrushFalloff::Spherical)) {
                    m_brushFalloff = BrushFalloff::Spherical;
                }

                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Material Editor", nullptr, m_editMode == EditMode::MaterialEdit)) {
                SetEditMode(EditMode::MaterialEdit);
                // Show material editor as floating window
                for (auto& layout : m_panelLayouts) {
                    if (layout.id == PanelID::MaterialEditor) {
                        layout.isVisible = true;
                        break;
                    }
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Controls", "F1")) {
                m_showControlsDialog = true;
            }
            if (ImGui::MenuItem("About")) {
                m_showAboutDialog = true;
            }
            ImGui::EndMenu();
        }

        // Stats display on the right side of menu bar
        float menuBarHeight = ImGui::GetFrameHeight();
        float statsWidth = 200.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - statsWidth - 10);

        // FPS display (always visible)
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // Expandable stats dropdown
        ImGui::SameLine();
        if (ImGui::BeginMenu("Stats")) {
            ImGui::Text("Performance");
            ImGui::Separator();
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::Separator();
            ImGui::Text("Debug Overlays");
            static bool showDebugOverlay = false;
            static bool showProfiler = false;
            static bool showMemory = false;
            ImGui::Checkbox("Show Debug Overlay", &showDebugOverlay);
            ImGui::Checkbox("Show Profiler", &showProfiler);
            ImGui::Checkbox("Show Memory Stats", &showMemory);

            ImGui::Separator();
            ImGui::Text("Time Distribution");
            static bool showRenderTime = false;
            static bool showUpdateTime = false;
            static bool showPhysicsTime = false;
            ImGui::Checkbox("Render Time", &showRenderTime);
            ImGui::Checkbox("Update Time", &showUpdateTime);
            ImGui::Checkbox("Physics Time", &showPhysicsTime);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void StandaloneEditor::Render3D(Nova::Renderer& renderer, const Nova::Camera& camera) {
    if (!m_initialized) {
        return;
    }

    // Store camera reference for ray casting
    m_currentCamera = &camera;

    auto& debugDraw = renderer.GetDebugDraw();

    // Draw grid if enabled
    if (m_showGrid) {
        debugDraw.AddGrid(m_mapWidth, m_gridSize, glm::vec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    // Draw terrain tiles (simple colored quads for now)
    for (int y = 0; y < m_mapHeight; ++y) {
        for (int x = 0; x < m_mapWidth; ++x) {
            int index = y * m_mapWidth + x;
            float height = m_terrainHeights[index];

            glm::vec3 pos(x * m_gridSize, height, y * m_gridSize);
            glm::vec4 color(0.2f, 0.6f, 0.2f, 1.0f); // Green for grass

            // Simple tile visualization
            debugDraw.AddAABB(pos, glm::vec3(m_gridSize * 0.45f, 0.05f, m_gridSize * 0.45f), color);
        }
    }

    // Draw selected object bounds
    if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size())) {
        auto& obj = m_sceneObjects[m_selectedObjectIndex];

        // Calculate AABB from bounding box
        glm::vec3 aabbMin = obj.position + obj.boundingBoxMin * obj.scale;
        glm::vec3 aabbMax = obj.position + obj.boundingBoxMax * obj.scale;

        // Draw selection outline (yellow bounding box)
        debugDraw.AddAABB(aabbMin, aabbMax, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    // Render transform gizmo if in ObjectSelect mode with an active tool
    if (m_editMode == EditMode::ObjectSelect &&
        m_selectedObjectIndex >= 0 &&
        m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size()) &&
        m_transformTool != TransformTool::None) {

        RenderTransformGizmo(debugDraw);
    }
}

void StandaloneEditor::ProcessInput() {
    // Input processing will be handled in Update for now
}

void StandaloneEditor::SetEditMode(EditMode mode) {
    m_editMode = mode;
    spdlog::info("Editor mode changed to: {}", static_cast<int>(mode));
}

void StandaloneEditor::SetTransformTool(TransformTool tool) {
    // Can only use transform tools in ObjectSelect mode
    if (m_editMode == EditMode::ObjectSelect) {
        m_transformTool = tool;
        spdlog::info("Transform tool changed to: {}", static_cast<int>(tool));
    }
}

void StandaloneEditor::NewMap(int width, int height) {
    spdlog::info("Creating new map: {}x{}", width, height);

    m_mapWidth = width;
    m_mapHeight = height;

    // Initialize terrain data
    int numTiles = width * height;
    m_terrainTiles.resize(numTiles, 0);  // 0 = grass
    m_terrainHeights.resize(numTiles, 0.0f);  // Flat terrain

    m_currentMapPath.clear();
    spdlog::info("New map created");
}
void StandaloneEditor::NewWorldMap() {    spdlog::info("Creating new spherical world map with radius {} km", m_worldRadius);    m_worldType = WorldType::Spherical;    m_mapWidth = 360;    m_mapHeight = 180;    int numTiles = m_mapWidth * m_mapHeight;    m_terrainTiles.resize(numTiles, 0);    m_terrainHeights.resize(numTiles, 0.0f);    float surfaceArea = 4.0f * 3.14159f * m_worldRadius * m_worldRadius;    spdlog::info("Spherical world created: Radius {} km, Surface {:.0f} km²", m_worldRadius, surfaceArea);    m_currentMapPath.clear();}void StandaloneEditor::NewLocalMap(int width, int height) {    spdlog::info("Creating new local map: {}x{}", width, height);    m_worldType = WorldType::Flat;    m_mapWidth = width;    m_mapHeight = height;    int numTiles = width * height;    m_terrainTiles.resize(numTiles, 0);    m_terrainHeights.resize(numTiles, 0.0f);    m_currentMapPath.clear();    spdlog::info("Local map created");}

bool StandaloneEditor::LoadMap(const std::string& path) {
    spdlog::info("Loading map from: {}", path);

    try {
        // Read JSON file
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open map file: {}", path);
            return false;
        }

        nlohmann::json mapJson;
        file >> mapJson;
        file.close();

        // Validate basic structure
        if (!mapJson.contains("version")) {
            spdlog::error("Map file missing version field");
            return false;
        }

        std::string version = mapJson["version"];
        spdlog::info("Loading map version: {}", version);

        // Clear existing scene
        m_sceneObjects.clear();
        m_selectedObjectIndex = -1;
        m_selectedObjectIndices.clear();

        // Load camera
        if (mapJson.contains("camera")) {
            auto& camera = mapJson["camera"];
            if (camera.contains("position")) {
                m_editorCameraPos.x = camera["position"][0];
                m_editorCameraPos.y = camera["position"][1];
                m_editorCameraPos.z = camera["position"][2];
            }
            if (camera.contains("distance")) {
                m_cameraDistance = camera["distance"];
            }
            if (camera.contains("angle")) {
                m_cameraAngle = camera["angle"];
            }
        }

        // Load terrain
        if (mapJson.contains("terrain")) {
            auto& terrain = mapJson["terrain"];

            if (terrain.contains("size")) {
                m_mapWidth = terrain["size"][0];
                m_mapHeight = terrain["size"][1];
            }

            if (terrain.contains("worldType")) {
                std::string worldType = terrain["worldType"];
                m_worldType = (worldType == "Spherical") ? WorldType::Spherical : WorldType::Flat;
            }

            if (terrain.contains("worldRadius")) {
                m_worldRadius = terrain["worldRadius"];
            }

            if (terrain.contains("heights")) {
                m_terrainHeights.clear();
                for (const auto& h : terrain["heights"]) {
                    m_terrainHeights.push_back(h);
                }
            } else {
                int numTiles = m_mapWidth * m_mapHeight;
                m_terrainHeights.resize(numTiles, 0.0f);
            }

            if (terrain.contains("tiles")) {
                m_terrainTiles.clear();
                for (const auto& t : terrain["tiles"]) {
                    m_terrainTiles.push_back(t);
                }
            } else {
                int numTiles = m_mapWidth * m_mapHeight;
                m_terrainTiles.resize(numTiles, 0);
            }

            if (terrain.contains("heightmapPath")) {
                std::string heightmapPath = terrain["heightmapPath"];
                spdlog::info("Terrain heightmap reference: {}", heightmapPath);
            }

            m_terrainMeshDirty = true;
        }

        // Load objects
        if (mapJson.contains("objects")) {
            for (const auto& obj : mapJson["objects"]) {
                SceneObject sceneObj;

                if (obj.contains("name")) {
                    sceneObj.name = obj["name"];
                }

                if (obj.contains("transform")) {
                    auto& transform = obj["transform"];

                    if (transform.contains("position")) {
                        sceneObj.position.x = transform["position"][0];
                        sceneObj.position.y = transform["position"][1];
                        sceneObj.position.z = transform["position"][2];
                    }

                    if (transform.contains("rotation")) {
                        sceneObj.rotation.x = transform["rotation"][0];
                        sceneObj.rotation.y = transform["rotation"][1];
                        sceneObj.rotation.z = transform["rotation"][2];
                    }

                    if (transform.contains("scale")) {
                        sceneObj.scale.x = transform["scale"][0];
                        sceneObj.scale.y = transform["scale"][1];
                        sceneObj.scale.z = transform["scale"][2];
                    }
                }

                if (obj.contains("boundingBox")) {
                    auto& bbox = obj["boundingBox"];
                    if (bbox.contains("min")) {
                        sceneObj.boundingBoxMin.x = bbox["min"][0];
                        sceneObj.boundingBoxMin.y = bbox["min"][1];
                        sceneObj.boundingBoxMin.z = bbox["min"][2];
                    }
                    if (bbox.contains("max")) {
                        sceneObj.boundingBoxMax.x = bbox["max"][0];
                        sceneObj.boundingBoxMax.y = bbox["max"][1];
                        sceneObj.boundingBoxMax.z = bbox["max"][2];
                    }
                }

                m_sceneObjects.push_back(sceneObj);
            }

            spdlog::info("Loaded {} scene objects", m_sceneObjects.size());
        }

        // Load lighting
        if (mapJson.contains("lighting")) {
            auto& lighting = mapJson["lighting"];

            if (lighting.contains("ambientColor")) {
                m_mapAmbientLight.r = lighting["ambientColor"][0];
                m_mapAmbientLight.g = lighting["ambientColor"][1];
                m_mapAmbientLight.b = lighting["ambientColor"][2];
            }

            if (lighting.contains("sunColor")) {
                m_mapDirectionalLight.r = lighting["sunColor"][0];
                m_mapDirectionalLight.g = lighting["sunColor"][1];
                m_mapDirectionalLight.b = lighting["sunColor"][2];
            }
        }

        // Load map properties
        if (mapJson.contains("name")) {
            m_mapName = mapJson["name"];
        }

        if (mapJson.contains("description")) {
            m_mapDescription = mapJson["description"];
        }

        // Update current map path and add to recent files
        m_currentMapPath = path;
        AddToRecentFiles(path);

        spdlog::info("Map loaded successfully from: {}", path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error loading map: {}", e.what());
        return false;
    }
}

bool StandaloneEditor::SaveMap(const std::string& path) {
    spdlog::info("Saving map to: {}", path);

    try {
        nlohmann::json mapJson;

        // Set version
        mapJson["version"] = "1.0";

        // Save map name and description
        mapJson["name"] = m_mapName;
        mapJson["description"] = m_mapDescription;

        // Save camera
        mapJson["camera"] = {
            {"position", {m_editorCameraPos.x, m_editorCameraPos.y, m_editorCameraPos.z}},
            {"distance", m_cameraDistance},
            {"angle", m_cameraAngle}
        };

        // Save terrain
        nlohmann::json terrain;
        terrain["size"] = {m_mapWidth, m_mapHeight};
        terrain["worldType"] = (m_worldType == WorldType::Spherical) ? "Spherical" : "Flat";

        if (m_worldType == WorldType::Spherical) {
            terrain["worldRadius"] = m_worldRadius;
        }

        terrain["heightScale"] = 10.0f;

        // Save terrain heights
        terrain["heights"] = nlohmann::json::array();
        for (const auto& h : m_terrainHeights) {
            terrain["heights"].push_back(h);
        }

        // Save terrain tiles
        terrain["tiles"] = nlohmann::json::array();
        for (const auto& t : m_terrainTiles) {
            terrain["tiles"].push_back(t);
        }

        mapJson["terrain"] = terrain;

        // Save objects
        nlohmann::json objects = nlohmann::json::array();
        int objIndex = 0;
        for (const auto& obj : m_sceneObjects) {
            nlohmann::json objJson;

            objJson["id"] = "obj_" + std::to_string(objIndex++);
            objJson["name"] = obj.name;
            objJson["type"] = "generic";

            objJson["transform"] = {
                {"position", {obj.position.x, obj.position.y, obj.position.z}},
                {"rotation", {obj.rotation.x, obj.rotation.y, obj.rotation.z}},
                {"scale", {obj.scale.x, obj.scale.y, obj.scale.z}}
            };

            objJson["boundingBox"] = {
                {"min", {obj.boundingBoxMin.x, obj.boundingBoxMin.y, obj.boundingBoxMin.z}},
                {"max", {obj.boundingBoxMax.x, obj.boundingBoxMax.y, obj.boundingBoxMax.z}}
            };

            objects.push_back(objJson);
        }
        mapJson["objects"] = objects;

        // Save lighting
        mapJson["lighting"] = {
            {"ambientColor", {m_mapAmbientLight.r, m_mapAmbientLight.g, m_mapAmbientLight.b}},
            {"sunColor", {m_mapDirectionalLight.r, m_mapDirectionalLight.g, m_mapDirectionalLight.b}},
            {"sunDirection", {-0.5f, -1.0f, -0.5f}}
        };

        // Create parent directories if needed
        std::filesystem::path filePath(path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }

        // Write to file with indentation for readability
        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", path);
            return false;
        }

        file << mapJson.dump(2);
        file.close();

        // Update current map path and add to recent files
        m_currentMapPath = path;
        AddToRecentFiles(path);

        spdlog::info("Map saved successfully to: {}", path);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error saving map: {}", e.what());
        return false;
    }
}

// UI Rendering Functions

void StandaloneEditor::RenderAssetBrowser() {
    ImGui::Begin("Asset Browser", &m_showAssetBrowser);

    ImGui::Text("Assets Directory: %s", m_assetDirectory.c_str());
    ImGui::Separator();

    // Simple folder tree
    if (ImGui::TreeNode("Textures")) {
        ImGui::Selectable("grass.png");
        ImGui::Selectable("dirt.png");
        ImGui::Selectable("stone.png");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Models")) {
        ImGui::Selectable("tree.fbx");
        ImGui::Selectable("rock.fbx");
        ImGui::Selectable("building.fbx");
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Materials")) {
        ImGui::Selectable("grass_material.mat");
        ImGui::Selectable("stone_material.mat");
        ImGui::TreePop();
    }

    ImGui::End();
}

void StandaloneEditor::RenderTerrainPanel() {
    ImGui::Begin("Terrain Editor", &m_showTerrainPanel);

    ImGui::Text("Terrain Tools");
    ImGui::Separator();

    ImGui::Text("Brush Type:");
    if (ImGui::RadioButton("Grass", m_selectedBrush == TerrainBrush::Grass)) m_selectedBrush = TerrainBrush::Grass;
    if (ImGui::RadioButton("Dirt", m_selectedBrush == TerrainBrush::Dirt)) m_selectedBrush = TerrainBrush::Dirt;
    if (ImGui::RadioButton("Stone", m_selectedBrush == TerrainBrush::Stone)) m_selectedBrush = TerrainBrush::Stone;
    if (ImGui::RadioButton("Sand", m_selectedBrush == TerrainBrush::Sand)) m_selectedBrush = TerrainBrush::Sand;
    if (ImGui::RadioButton("Water", m_selectedBrush == TerrainBrush::Water)) m_selectedBrush = TerrainBrush::Water;

    ImGui::Separator();
    ImGui::Text("Sculpting:");
    if (ImGui::RadioButton("Raise", m_selectedBrush == TerrainBrush::Raise)) m_selectedBrush = TerrainBrush::Raise;
    if (ImGui::RadioButton("Lower", m_selectedBrush == TerrainBrush::Lower)) m_selectedBrush = TerrainBrush::Lower;

    ImGui::Separator();
    ImGui::SliderInt("Brush Size", &m_brushSize, 1, 10);
    ImGui::SliderFloat("Strength", &m_brushStrength, 0.1f, 2.0f);

    ImGui::End();
}

void StandaloneEditor::RenderObjectPanel() {
    ImGui::Begin("Object Placement", &m_showObjectPanel);

    ImGui::Text("Place Objects");
    ImGui::Separator();

    if (ImGui::TreeNode("Nature")) {
        if (ImGui::Selectable("Tree")) {}
        if (ImGui::Selectable("Rock")) {}
        if (ImGui::Selectable("Bush")) {}
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Buildings")) {
        if (ImGui::Selectable("House")) {}
        if (ImGui::Selectable("Tower")) {}
        if (ImGui::Selectable("Wall")) {}
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Units")) {
        if (ImGui::Selectable("Worker")) {}
        if (ImGui::Selectable("Soldier")) {}
        ImGui::TreePop();
    }

    ImGui::End();
}

void StandaloneEditor::RenderMaterialPanel() {
    ImGui::Begin("Material Editor", &m_showMaterialPanel);

    ImGui::Text("Material Properties");
    ImGui::Separator();

    ImGui::Text("(Material editor UI goes here)");

    ImGui::End();
}

void StandaloneEditor::RenderPropertiesPanel() {
    ImGui::Begin("Properties", &m_showPropertiesPanel);

    if (m_selectedObjectIndex >= 0) {
        ImGui::Text("Selected Object");
        ImGui::Separator();

        ImGui::DragFloat3("Position", &m_selectedObjectPosition.x, 0.1f);
        ImGui::DragFloat3("Rotation", &m_selectedObjectRotation.x, 1.0f);
        ImGui::DragFloat3("Scale", &m_selectedObjectScale.x, 0.1f);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
    }

    ImGui::End();
}

void StandaloneEditor::RenderToolsPanel() {
    ImGui::Begin("Tools", &m_showPropertiesPanel);

    ImGui::Text("Edit Tools");
    ImGui::Separator();

    if (ImGui::Button("Select\n[Q]", ImVec2(90, 50))) {
        SetEditMode(EditMode::ObjectSelect);
    }
    ImGui::SameLine();
    if (ImGui::Button("Move\n[W]", ImVec2(90, 50))) {
        // Set move tool
    }

    if (ImGui::Button("Rotate\n[E]", ImVec2(90, 50))) {
        // Set rotate tool
    }
    ImGui::SameLine();
    if (ImGui::Button("Scale\n[R]", ImVec2(90, 50))) {
        // Set scale tool
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Terrain Tools");
    ImGui::Separator();

    if (ImGui::Button("Paint\n[1]", ImVec2(90, 50))) {
        SetEditMode(EditMode::TerrainPaint);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sculpt\n[2]", ImVec2(90, 50))) {
        SetEditMode(EditMode::TerrainSculpt);
    }

    if (ImGui::Button("Smooth\n[3]", ImVec2(90, 50))) {
        // Set smooth tool
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten\n[4]", ImVec2(90, 50))) {
        // Set flatten tool
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Placement Tools");
    ImGui::Separator();

    if (ImGui::Button("Place Object\n[5]", ImVec2(90, 50))) {
        SetEditMode(EditMode::ObjectPlace);
    }
    ImGui::SameLine();
    if (ImGui::Button("Paint Foliage\n[6]", ImVec2(90, 50))) {
        // Set foliage paint tool
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Brush Settings");
    ImGui::Separator();

    ImGui::SliderInt("Size", &m_brushSize, 1, 20);
    ImGui::SliderFloat("Strength", &m_brushStrength, 0.1f, 5.0f);
    ImGui::Checkbox("Snap to Grid", &m_snapToGrid);

    ImGui::End();
}

void StandaloneEditor::RenderContentBrowser() {
    ImGui::Begin("Content Browser");

    ImGui::BeginChild("ContentToolbar", ImVec2(0, 30), ImGuiChildFlags_Borders);
    if (ImGui::Button("Import")) {}
    ImGui::SameLine();
    if (ImGui::Button("New Folder")) {}
    ImGui::SameLine();
    ImGui::Text("Path: /Assets/");
    ImGui::EndChild();

    ImGui::BeginChild("ContentArea", ImVec2(0, 0), ImGuiChildFlags_Borders);

    // Folder/File tree on left
    ImGui::BeginChild("Folders", ImVec2(200, 0), ImGuiChildFlags_Borders);
    ImGui::Text("Folders");
    ImGui::Separator();

    if (ImGui::TreeNode("Assets")) {
        if (ImGui::TreeNode("Textures")) {
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Materials")) {
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Models")) {
            if (ImGui::TreeNode("Trees")) {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Rocks")) {
                ImGui::TreePop();
            }
            if (ImGui::TreeNode("Buildings")) {
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Sounds")) {
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Scripts")) {
            ImGui::TreePop();
        }
        ImGui::TreePop();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Content grid on right
    ImGui::BeginChild("ContentGrid", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGui::Text("Content");
    ImGui::Separator();

    // Display items as grid of thumbnails
    float thumbnailSize = 64.0f;
    float cellSize = thumbnailSize + 10.0f;
    int columns = (int)((ImGui::GetContentRegionAvail().x) / cellSize);
    if (columns < 1) columns = 1;

    const char* items[] = {
        "tree_oak.fbx", "tree_pine.fbx", "rock_01.fbx", "rock_02.fbx",
        "grass_texture.png", "dirt_texture.png", "building_01.fbx", "wall_01.fbx"
    };

    for (int i = 0; i < 8; i++) {
        ImGui::BeginGroup();
        ImGui::Button("##thumb", ImVec2(thumbnailSize, thumbnailSize));
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            spdlog::info("Double-clicked: {}", items[i]);
        }
        ImGui::Text("%s", items[i]);
        ImGui::EndGroup();

        if ((i + 1) % columns != 0) ImGui::SameLine();
    }

    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::End();
}

void StandaloneEditor::RenderDetailsPanel() {
    ImGui::Begin("Details");

    if (m_selectedObjectIndex >= 0) {
        ImGui::Text("Selected Object Properties");
        ImGui::Separator();

        ImGui::Text("Transform");
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Location", &m_selectedObjectPosition.x, 0.1f);
            ImGui::DragFloat3("Rotation", &m_selectedObjectRotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &m_selectedObjectScale.x, 0.01f);
        }

        if (ImGui::CollapsingHeader("Rendering")) {
            static bool castShadows = true;
            static bool receiveShadows = true;
            static int renderLayer = 0;

            ImGui::Checkbox("Cast Shadows", &castShadows);
            ImGui::Checkbox("Receive Shadows", &receiveShadows);
            ImGui::SliderInt("Render Layer", &renderLayer, 0, 31);
        }

        if (ImGui::CollapsingHeader("Physics")) {
            static bool enablePhysics = false;
            static float mass = 1.0f;

            ImGui::Checkbox("Enable Physics", &enablePhysics);
            if (enablePhysics) {
                ImGui::DragFloat("Mass", &mass, 0.1f, 0.1f, 1000.0f);
            }
        }

        if (ImGui::CollapsingHeader("Tags & Layers")) {
            static char tag[64] = "Default";
            ImGui::InputText("Tag", tag, 64);
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
        ImGui::Separator();

        ImGui::Text("Scene Settings");
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float ambientIntensity = 0.2f;
            static float skyboxRotation = 0.0f;

            ImGui::SliderFloat("Ambient Intensity", &ambientIntensity, 0.0f, 1.0f);
            ImGui::SliderFloat("Skybox Rotation", &skyboxRotation, 0.0f, 360.0f);
        }

        if (ImGui::CollapsingHeader("Lighting")) {
            static glm::vec3 sunDirection(-0.5f, -1.0f, -0.5f);
            static glm::vec3 sunColor(1.0f, 0.95f, 0.9f);

            ImGui::DragFloat3("Sun Direction", &sunDirection.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun Color", &sunColor.x);
        }
    }

    ImGui::End();
}

void StandaloneEditor::RenderViewportControls() {
    // Viewport controls as overlay within viewport window
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    ImGui::SetCursorPos(ImVec2(windowSize.x - 220, 10));
    ImGui::BeginChild("ViewportControls", ImVec2(210, 0), ImGuiChildFlags_Borders);

    ImGui::Text("Viewport");
    ImGui::Separator();

    // Projection mode
    static int projectionMode = 0;
    ImGui::Text("Projection:");
    ImGui::RadioButton("Perspective", &projectionMode, 0); ImGui::SameLine();
    ImGui::RadioButton("Orthographic", &projectionMode, 1);

    ImGui::Separator();

    // View mode
    static int viewMode = 0;
    ImGui::Text("View:");
    if (ImGui::Button("Free")) viewMode = 0;
    ImGui::SameLine();
    if (ImGui::Button("Top (Z)")) viewMode = 1;
    if (ImGui::Button("Front (Y)")) viewMode = 2;
    ImGui::SameLine();
    if (ImGui::Button("Right (X)")) viewMode = 3;

    ImGui::Separator();

    // Render mode
    static int renderMode = 0;
    ImGui::Text("Shading:");
    ImGui::RadioButton("Lit", &renderMode, 0);
    ImGui::RadioButton("Unlit", &renderMode, 1);
    ImGui::RadioButton("Wireframe", &renderMode, 2);

    ImGui::Separator();

    // Viewport options
    ImGui::Checkbox("Grid", &m_showGrid);
    ImGui::Checkbox("Gizmos", &m_showGizmos);

    ImGui::EndChild();
}

void StandaloneEditor::RenderStatusBar() {
    auto& window = Nova::Engine::Instance().GetWindow();
    ImVec2 windowSize(static_cast<float>(window.GetWidth()), 25.0f);

    ImGui::SetNextWindowPos(ImVec2(0, window.GetHeight() - 25.0f));
    ImGui::SetNextWindowSize(windowSize);

    ImGui::Begin("StatusBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    ImGui::Text("Map: %dx%d | Mode: %s | Camera: (%.1f, %.1f, %.1f)",
        m_mapWidth, m_mapHeight,
        m_editMode == EditMode::TerrainPaint ? "Paint" :
        m_editMode == EditMode::TerrainSculpt ? "Sculpt" :
        m_editMode == EditMode::ObjectPlace ? "Place" :
        m_editMode == EditMode::ObjectSelect ? "Select" : "None",
        m_editorCameraPos.x, m_editorCameraPos.y, m_editorCameraPos.z);

    ImGui::End();
}

// Dialog Functions

void StandaloneEditor::ShowNewMapDialog() {
    ImGui::OpenPopup("New Map");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("New Map", &m_showNewMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static bool initialized = false;
        static int worldTypeIndex = 1;
        static int width = 64;
        static int height = 64;
        static float planetRadius = 6371.0f;
        if (!initialized) {
            worldTypeIndex = (m_worldType == WorldType::Spherical) ? 1 : 0;
            planetRadius = m_worldRadius;
            initialized = true;
        }
        ModernUI::GradientHeader("World Type");
        bool isFlatWorld = (worldTypeIndex == 0);
        bool isSphericalWorld = (worldTypeIndex == 1);
        if (ImGui::RadioButton("Flat World", isFlatWorld)) worldTypeIndex = 0;
        ImGui::SameLine();
        ImGui::TextDisabled("Traditional flat map");
        if (ImGui::RadioButton("Spherical World", isSphericalWorld)) worldTypeIndex = 1;
        ImGui::SameLine();
        ImGui::TextDisabled("Planet surface");
        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();
        if (worldTypeIndex == 1) {
            ModernUI::GradientHeader("Planet Settings");
            ImGui::Text("World Radius:");
            ImGui::SetNextItemWidth(200.0f);
            ImGui::InputFloat("##Radius", &planetRadius, 100.0f, 1000.0f, "%.0f km");
            planetRadius = std::max(100.0f, std::min(planetRadius, 100000.0f));
            ImGui::Spacing();
            ImGui::Text("Presets:");
            if (ModernUI::GlowButton("Earth", ImVec2(80, 0))) planetRadius = 6371.0f;
            ImGui::SameLine();
            if (ModernUI::GlowButton("Mars", ImVec2(80, 0))) planetRadius = 3390.0f;
            ImGui::SameLine();
            if (ModernUI::GlowButton("Moon", ImVec2(80, 0))) planetRadius = 1737.0f;
        } else {
            ModernUI::GradientHeader("Map Dimensions");
            ImGui::InputInt("Width (chunks)", &width);
            width = std::max(1, std::min(width, 512));
            ImGui::InputInt("Height (chunks)", &height);
            height = std::max(1, std::min(height, 512));
            ImGui::Text("Total chunks: %d", width * height);
        }
        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();
        float buttonWidth = 120.0f;
        float spacing = 10.0f;
        float totalWidth = buttonWidth * 2 + spacing;
        float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        if (ModernUI::GlowButton("Create", ImVec2(buttonWidth, 0))) {
            if (worldTypeIndex == 1) {
                m_worldType = WorldType::Spherical;
                m_worldRadius = planetRadius;
                NewWorldMap();
            } else {
                m_worldType = WorldType::Flat;
                NewLocalMap(width, height);
            }
            m_showNewMapDialog = false;
            initialized = false;
        }
        ImGui::SameLine(0, spacing);
        if (ModernUI::GlowButton("Cancel", ImVec2(buttonWidth, 0))) {
            m_showNewMapDialog = false;
            initialized = false;
        }
        ImGui::EndPopup();
    }
}

void StandaloneEditor::ShowLoadMapDialog() {
    ImGui::OpenPopup("Load Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Load Map", &m_showLoadMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Select a map file to load:");
        ImGui::Separator();

        // Simple file list
        for (const auto& file : m_recentFiles) {
            if (ImGui::Selectable(file.c_str())) {
                LoadMap(file);
                m_showLoadMapDialog = false;
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showLoadMapDialog = false;
        }

        ImGui::EndPopup();
    }
}

void StandaloneEditor::ShowSaveMapDialog() {
    ImGui::OpenPopup("Save Map");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Map", &m_showSaveMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char filename[256] = "untitled.map";

        ImGui::InputText("Filename", filename, sizeof(filename));

        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            SaveMap(filename);
            m_showSaveMapDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSaveMapDialog = false;
        }

        ImGui::EndPopup();
    }
}

void StandaloneEditor::ShowAboutDialog() {
    ImGui::OpenPopup("About Editor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About Editor", &m_showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Nova3D RTS Editor");
        ImGui::Text("Version 1.0");
        ImGui::Separator();
        ImGui::Text("A standalone level editor for creating");
        ImGui::Text("custom maps and scenarios.");
        ImGui::Separator();

        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showAboutDialog = false;
        }

        ImGui::EndPopup();
    }
}

void StandaloneEditor::ShowControlsDialog() {
    ImGui::OpenPopup("Editor Controls");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Editor Controls", &m_showControlsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Keyboard Shortcuts");
        ImGui::Separator();

        // Edit Modes
        ImGui::Text("Edit Modes:");
        ImGui::BulletText("Q - Object Select Mode");
        ImGui::BulletText("1 - Terrain Paint Mode");
        ImGui::BulletText("2 - Terrain Sculpt Mode");

        ImGui::Spacing();

        // Transform Tools
        ImGui::Text("Transform Tools (in Object Select mode):");
        ImGui::BulletText("W - Toggle Move Tool");
        ImGui::BulletText("E - Toggle Rotate Tool");
        ImGui::BulletText("R - Toggle Scale Tool");

        ImGui::Spacing();

        // Camera Controls
        ImGui::Text("Camera Controls:");
        ImGui::BulletText("Arrow Keys - Rotate Camera");
        ImGui::BulletText("Page Up/Down - Zoom In/Out");

        ImGui::Spacing();

        // Brush Controls
        ImGui::Text("Brush Controls (in Terrain modes):");
        ImGui::BulletText("[ - Decrease Brush Size");
        ImGui::BulletText("] - Increase Brush Size");

        ImGui::Spacing();

        // Selection Controls
        ImGui::Text("Selection Controls:");
        ImGui::BulletText("Left Click - Select Object");
        ImGui::BulletText("Escape - Clear Selection");
        ImGui::BulletText("Delete - Delete Selected Objects");

        ImGui::Spacing();

        // File Operations
        ImGui::Text("File Operations:");
        ImGui::BulletText("Ctrl+N - New Map");
        ImGui::BulletText("Ctrl+O - Open Map");
        ImGui::BulletText("Ctrl+S - Save Map");
        ImGui::BulletText("Ctrl+Shift+S - Save Map As");

        ImGui::Spacing();

        // Help
        ImGui::Text("Help:");
        ImGui::BulletText("F1 - Show this dialog");

        ImGui::Separator();

        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showControlsDialog = false;
        }

        ImGui::EndPopup();
    }
}

// Terrain editing functions

float StandaloneEditor::CalculateBrushFalloff(float distance, float radius) {
    if (distance >= radius) return 0.0f;

    float t = distance / radius; // 0 to 1

    switch (m_brushFalloff) {
        case BrushFalloff::Linear:
            return 1.0f - t;

        case BrushFalloff::Smooth:
            return 1.0f - (t * t * (3.0f - 2.0f * t)); // Smoothstep

        case BrushFalloff::Spherical:
            return sqrt(1.0f - t * t); // Circular falloff

        default:
            return 1.0f - t;
    }
}

void StandaloneEditor::PaintTerrain(int x, int y) {
    if (x < 0 || x >= m_mapWidth || y < 0 || y >= m_mapHeight) return;

    int newValue = static_cast<int>(m_selectedBrush);

    // Apply brush with falloff
    for (int by = -m_brushSize; by <= m_brushSize; by++) {
        for (int bx = -m_brushSize; bx <= m_brushSize; bx++) {
            int tx = x + bx;
            int ty = y + by;

            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            float distance = sqrt(bx*bx + by*by);
            if (distance > m_brushSize) continue;

            // Calculate falloff
            float falloff = CalculateBrushFalloff(distance, static_cast<float>(m_brushSize));

            // Apply paint with probability based on falloff
            if (falloff > 0.5f) {
                int index = ty * m_mapWidth + tx;
                int oldValue = m_terrainTiles[index];

                // Only create command if value actually changes
                if (oldValue != newValue) {
                    // Create undo command
                    auto command = std::make_unique<TerrainPaintCommand>(
                        m_terrainTiles, index, oldValue, newValue
                    );

                    m_commandHistory->ExecuteCommand(std::move(command));
                }
            }
        }
    }

    // Mark terrain as dirty for rendering updates
    m_terrainMeshDirty = true;

    spdlog::debug("Painted terrain at ({}, {}) with brush size {}", x, y, m_brushSize);
}

void StandaloneEditor::SculptTerrain(int x, int y, float strength) {
    if (x < 0 || x >= m_mapWidth || y < 0 || y >= m_mapHeight) return;

    // Create a single command to store all height changes
    auto command = std::make_unique<TerrainSculptCommand>(m_terrainHeights);

    // Apply height changes with brush
    for (int by = -m_brushSize; by <= m_brushSize; by++) {
        for (int bx = -m_brushSize; bx <= m_brushSize; bx++) {
            int tx = x + bx;
            int ty = y + by;

            if (tx < 0 || tx >= m_mapWidth || ty < 0 || ty >= m_mapHeight) continue;

            float distance = sqrt(bx*bx + by*by);
            if (distance > m_brushSize) continue;

            float falloff = CalculateBrushFalloff(distance, static_cast<float>(m_brushSize));

            int index = ty * m_mapWidth + tx;
            float oldHeight = m_terrainHeights[index];
            float heightChange = strength * falloff * 0.1f; // Scale factor
            float newHeight = oldHeight + heightChange;

            // Clamp to min/max height
            newHeight = std::max(m_minHeight, std::min(newHeight, m_maxHeight));

            // Only add to command if height actually changes
            if (std::abs(newHeight - oldHeight) > 0.001f) {
                command->AddHeightChange(index, oldHeight, newHeight);
            }
        }
    }

    // Only execute command if there were any changes
    if (command->GetDescription().find("0 tiles") == std::string::npos) {
        m_commandHistory->ExecuteCommand(std::move(command));
        // Mark terrain as dirty for rendering updates
        m_terrainMeshDirty = true;
    }

    spdlog::debug("Sculpted terrain at ({}, {}) with strength {} and brush size {}", x, y, strength, m_brushSize);
}

// Object editing functions (stubs for now)

void StandaloneEditor::PlaceObject(const glm::vec3& position, const std::string& objectType) {
    // Create a new scene object
    SceneObject newObject;
    newObject.name = objectType + "_" + std::to_string(m_sceneObjects.size());
    newObject.position = position;
    newObject.rotation = glm::vec3(0.0f);
    newObject.scale = glm::vec3(1.0f);

    // Set default bounding box based on object type
    if (objectType == "Cube" || objectType == "cube") {
        newObject.boundingBoxMin = glm::vec3(-0.5f);
        newObject.boundingBoxMax = glm::vec3(0.5f);
    } else if (objectType == "Sphere" || objectType == "sphere") {
        newObject.boundingBoxMin = glm::vec3(-0.5f);
        newObject.boundingBoxMax = glm::vec3(0.5f);
    } else if (objectType == "Plane" || objectType == "plane") {
        newObject.boundingBoxMin = glm::vec3(-5.0f, -0.01f, -5.0f);
        newObject.boundingBoxMax = glm::vec3(5.0f, 0.01f, 5.0f);
    } else {
        // Default bounding box for unknown types
        newObject.boundingBoxMin = glm::vec3(-0.5f);
        newObject.boundingBoxMax = glm::vec3(0.5f);
    }

    // Add to scene
    m_sceneObjects.push_back(newObject);

    // Select the newly placed object
    SelectObjectByIndex(static_cast<int>(m_sceneObjects.size()) - 1, false);

    spdlog::info("Placed object '{}' at ({:.2f}, {:.2f}, {:.2f})",
                 newObject.name, position.x, position.y, position.z);
}

void StandaloneEditor::SelectObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    if (m_sceneObjects.empty()) {
        spdlog::info("No objects in scene to select");
        return;
    }

    int closestObjectIndex = -1;
    float closestDistance = std::numeric_limits<float>::max();

    // Test ray against all scene objects
    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        const auto& obj = m_sceneObjects[i];

        // Calculate world-space AABB for this object
        glm::vec3 aabbMin = obj.position + (obj.boundingBoxMin * obj.scale);
        glm::vec3 aabbMax = obj.position + (obj.boundingBoxMax * obj.scale);

        // Test ray intersection with AABB
        float distance;
        if (RayIntersectsAABB(rayOrigin, rayDir, aabbMin, aabbMax, distance)) {
            // Found an intersection - check if it's closer than previous hits
            if (distance < closestDistance) {
                closestDistance = distance;
                closestObjectIndex = static_cast<int>(i);
            }
        }
    }

    // Select the closest object if we found one
    if (closestObjectIndex >= 0) {
        SelectObjectByIndex(closestObjectIndex, false);
        spdlog::info("Selected object '{}' at distance {:.2f}",
                    m_sceneObjects[closestObjectIndex].name, closestDistance);
    } else {
        ClearSelection();
        spdlog::info("No object hit by ray");
    }
}

void StandaloneEditor::TransformSelectedObject() {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Apply the transform values from the UI to the selected object
    obj.position = m_selectedObjectPosition;
    obj.rotation = m_selectedObjectRotation;
    obj.scale = m_selectedObjectScale;

    spdlog::debug("Transformed object '{}': pos=({:.2f}, {:.2f}, {:.2f}), rot=({:.2f}, {:.2f}, {:.2f}), scale=({:.2f}, {:.2f}, {:.2f})",
                  obj.name,
                  obj.position.x, obj.position.y, obj.position.z,
                  obj.rotation.x, obj.rotation.y, obj.rotation.z,
                  obj.scale.x, obj.scale.y, obj.scale.z);
}

void StandaloneEditor::DeleteSelectedObject() {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        spdlog::warn("No object selected for deletion");
        return;
    }

    // Log the deletion
    spdlog::info("Deleting object: {}", m_sceneObjects[m_selectedObjectIndex].name);

    // Remove the object from the scene
    m_sceneObjects.erase(m_sceneObjects.begin() + m_selectedObjectIndex);

    // Clear selection since the object no longer exists
    ClearSelection();
}

// ========================================
// Docking System Implementation
// ========================================

const char* StandaloneEditor::GetPanelName(PanelID panel) {
    switch (panel) {
        case PanelID::Viewport: return "Viewport";
        case PanelID::Tools: return "Tools";
        case PanelID::ContentBrowser: return "Content Browser";
        case PanelID::Details: return "Details";
        case PanelID::MaterialEditor: return "Material Editor";
        case PanelID::EngineStats: return "Engine Stats";
        default: return "Unknown";
    }
}

void StandaloneEditor::RenderPanelContent(PanelID panel) {
    switch (panel) {
        case PanelID::Viewport:
            RenderViewportControls();
            break;
        case PanelID::Tools:
            RenderUnifiedToolsPanel();
            break;
        case PanelID::ContentBrowser:
            RenderUnifiedContentBrowser();
            break;
        case PanelID::Details:
            RenderDetailsContent();
            break;
        case PanelID::MaterialEditor:
            RenderMaterialEditorContent();
            break;
        case PanelID::EngineStats:
            RenderEngineStatsContent();
            break;
    }
}

void StandaloneEditor::RenderUnifiedToolsPanel() {
    // Current Tool Indicator
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
    ImGui::BeginChild("ToolIndicator", ImVec2(0, 60), true);

    ImGui::Text("Current Mode:");
    ImGui::SameLine();

    // Show current edit mode with color
    const char* modeName = "None";
    ImVec4 modeColor(0.5f, 0.5f, 0.5f, 1.0f);

    switch (m_editMode) {
        case EditMode::ObjectSelect:
            modeName = "Object Select";
            modeColor = ImVec4(0.0f, 0.8f, 0.82f, 1.0f);  // Cyan
            break;
        case EditMode::TerrainPaint:
            modeName = "Terrain Paint";
            modeColor = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);  // Green
            break;
        case EditMode::TerrainSculpt:
            modeName = "Terrain Sculpt";
            modeColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);  // Orange
            break;
        case EditMode::ObjectPlace:
            modeName = "Object Place";
            modeColor = ImVec4(0.6f, 0.4f, 0.8f, 1.0f);  // Purple
            break;
        case EditMode::MaterialEdit:
            modeName = "Material Edit";
            modeColor = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);  // Gold
            break;
        default:
            break;
    }

    ImGui::TextColored(modeColor, "%s", modeName);

    // Show current transform tool if applicable
    if (m_editMode == EditMode::ObjectSelect && m_transformTool != TransformTool::None) {
        ImGui::Text("Tool:");
        ImGui::SameLine();

        const char* toolName = "None";
        switch (m_transformTool) {
            case TransformTool::Move: toolName = "Move"; break;
            case TransformTool::Rotate: toolName = "Rotate"; break;
            case TransformTool::Scale: toolName = "Scale"; break;
            default: break;
        }

        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", toolName);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Edit Tools
    if (ImGui::CollapsingHeader("Edit Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        float buttonWidth = ImGui::GetContentRegionAvail().x;

        // Object Select button with highlight
        bool isSelectMode = m_editMode == EditMode::ObjectSelect;
        if (isSelectMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.62f, 1.0f));
        }
        if (ImGui::Button("Object Select [Q]", ImVec2(buttonWidth, 35))) {
            SetEditMode(EditMode::ObjectSelect);
            m_transformTool = TransformTool::None;
        }
        if (isSelectMode) {
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Text("Transform Tools:");

        float smallButtonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

        // Move button with highlight
        bool isMoveActive = m_transformTool == TransformTool::Move;
        if (isMoveActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.62f, 1.0f));
        }
        if (ImGui::Button("Move [W]", ImVec2(smallButtonWidth, 35))) {
            SetTransformTool((m_transformTool == TransformTool::Move) ? TransformTool::None : TransformTool::Move);
        }
        if (isMoveActive) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Rotate button with highlight
        bool isRotateActive = m_transformTool == TransformTool::Rotate;
        if (isRotateActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.62f, 1.0f));
        }
        if (ImGui::Button("Rotate [E]", ImVec2(smallButtonWidth, 35))) {
            SetTransformTool((m_transformTool == TransformTool::Rotate) ? TransformTool::None : TransformTool::Rotate);
        }
        if (isRotateActive) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Scale button with highlight
        bool isScaleActive = m_transformTool == TransformTool::Scale;
        if (isScaleActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.62f, 1.0f));
        }
        if (ImGui::Button("Scale [R]", ImVec2(smallButtonWidth, 35))) {
            SetTransformTool((m_transformTool == TransformTool::Scale) ? TransformTool::None : TransformTool::Scale);
        }
        if (isScaleActive) {
            ImGui::PopStyleColor();
        }
    }

    // Terrain Tools
    if (ImGui::CollapsingHeader("Terrain Tools", ImGuiTreeNodeFlags_DefaultOpen)) {
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

        // Paint button with highlight
        bool isPaintMode = m_editMode == EditMode::TerrainPaint;
        if (isPaintMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        }
        if (ImGui::Button("Paint [1]", ImVec2(buttonWidth, 35))) {
            SetEditMode(EditMode::TerrainPaint);
        }
        if (isPaintMode) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Sculpt button with highlight
        bool isSculptMode = m_editMode == EditMode::TerrainSculpt;
        if (isSculptMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.1f, 1.0f));
        }
        if (ImGui::Button("Sculpt [2]", ImVec2(buttonWidth, 35))) {
            SetEditMode(EditMode::TerrainSculpt);
        }
        if (isSculptMode) {
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Text("Brush Type:");
        if (ImGui::RadioButton("Grass", m_selectedBrush == TerrainBrush::Grass)) m_selectedBrush = TerrainBrush::Grass;
        if (ImGui::RadioButton("Dirt", m_selectedBrush == TerrainBrush::Dirt)) m_selectedBrush = TerrainBrush::Dirt;
        if (ImGui::RadioButton("Stone", m_selectedBrush == TerrainBrush::Stone)) m_selectedBrush = TerrainBrush::Stone;
        if (ImGui::RadioButton("Sand", m_selectedBrush == TerrainBrush::Sand)) m_selectedBrush = TerrainBrush::Sand;
        if (ImGui::RadioButton("Water", m_selectedBrush == TerrainBrush::Water)) m_selectedBrush = TerrainBrush::Water;
    }

    // Brush Settings
    if (ImGui::CollapsingHeader("Brush Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Brush Size", &m_brushSize, 1, 100);
        ImGui::SliderFloat("Brush Strength", &m_brushStrength, 0.1f, 10.0f);

        ImGui::Spacing();
        ImGui::Text("Brush Falloff:");
        if (ImGui::RadioButton("Linear", m_brushFalloff == BrushFalloff::Linear)) {
            m_brushFalloff = BrushFalloff::Linear;
        }
        if (ImGui::RadioButton("Smooth", m_brushFalloff == BrushFalloff::Smooth)) {
            m_brushFalloff = BrushFalloff::Smooth;
        }
        if (ImGui::RadioButton("Spherical", m_brushFalloff == BrushFalloff::Spherical)) {
            m_brushFalloff = BrushFalloff::Spherical;
        }

        ImGui::Spacing();
        ImGui::Checkbox("Snap to Grid", &m_snapToGrid);
    }

    // Placement Tools
    if (ImGui::CollapsingHeader("Placement Tools")) {
        float buttonWidth = ImGui::GetContentRegionAvail().x;

        bool isPlaceMode = m_editMode == EditMode::ObjectPlace;
        if (isPlaceMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.3f, 0.6f, 1.0f));
        }
        if (ImGui::Button("Place Object", ImVec2(buttonWidth, 35))) {
            SetEditMode(EditMode::ObjectPlace);
        }
        if (isPlaceMode) {
            ImGui::PopStyleColor();
        }
    }
}

void StandaloneEditor::RenderUnifiedContentBrowser() {
    if (!m_assetBrowser) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "AssetBrowser not initialized");
        return;
    }

    // Toolbar with navigation and actions
    ImGui::BeginChild("ContentToolbar", ImVec2(0, 35), ImGuiChildFlags_Borders);

    // Navigation buttons
    if (ImGui::Button("<")) {
        m_assetBrowser->NavigateBack();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Back");
    }

    ImGui::SameLine();
    if (ImGui::Button(">")) {
        m_assetBrowser->NavigateForward();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Forward");
    }

    ImGui::SameLine();
    if (ImGui::Button("^")) {
        m_assetBrowser->NavigateToParent();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Up");
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Action buttons
    if (ImGui::Button("Refresh")) {
        m_assetBrowser->Refresh();
    }
    ImGui::SameLine();

    static char newFolderName[128] = "";
    static bool showNewFolderPopup = false;

    if (ImGui::Button("New Folder")) {
        showNewFolderPopup = true;
        memset(newFolderName, 0, sizeof(newFolderName));
    }

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Current path display
    std::string currentDir = m_assetBrowser->GetCurrentDirectory();
    ImGui::Text("Path: %s", currentDir.c_str());

    ImGui::EndChild();

    // New folder popup
    if (showNewFolderPopup) {
        ImGui::OpenPopup("New Folder");
    }

    if (ImGui::BeginPopupModal("New Folder", &showNewFolderPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        ImGui::InputText("##foldername", newFolderName, sizeof(newFolderName));

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(newFolderName) > 0) {
                if (m_assetBrowser->CreateFolder(newFolderName)) {
                    spdlog::info("Created folder: {}", newFolderName);
                }
            }
            showNewFolderPopup = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewFolderPopup = false;
        }

        ImGui::EndPopup();
    }

    // Search bar
    ImGui::BeginChild("SearchBar", ImVec2(0, 30), ImGuiChildFlags_Borders);
    static char searchBuffer[256] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search assets...", searchBuffer, sizeof(searchBuffer))) {
        m_assetBrowser->SetSearchFilter(searchBuffer);
    }
    ImGui::EndChild();

    ImGui::BeginChild("ContentArea", ImVec2(0, 0), ImGuiChildFlags_None);

    // Left side: Directory tree
    ImGui::BeginChild("Folders", ImVec2(200, 0), ImGuiChildFlags_Borders);
    ImGui::Text("Folders");
    ImGui::Separator();

    // Render directory tree
    const auto& dirTree = m_assetBrowser->GetDirectoryTree();
    std::string rootDir = m_assetBrowser->GetRootDirectory();

    if (ImGui::TreeNodeEx(rootDir.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        // Render subdirectories recursively
        for (const auto& dir : dirTree) {
            if (ImGui::Selectable(dir.name.c_str())) {
                m_assetBrowser->NavigateToDirectory(dir.path);
            }
        }
        ImGui::TreePop();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: Content grid with thumbnails
    ImGui::BeginChild("ContentGrid", ImVec2(0, 0), ImGuiChildFlags_Borders);

    ImGui::Text("Content");
    ImGui::Separator();

    // Get filtered assets
    auto assets = m_assetBrowser->GetFilteredAssets();

    // Display as grid
    float thumbnailSize = 80.0f;
    float cellSize = thumbnailSize + 30.0f;
    int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellSize));

    int itemIndex = 0;
    static std::string contextMenuPath = "";
    static bool showContextMenu = false;

    for (const auto& asset : assets) {
        ImGui::PushID(itemIndex);

        ImGui::BeginGroup();

        // Get thumbnail or draw colored placeholder
        auto& thumbnailCache = m_assetBrowser->GetThumbnailCache();
        ImTextureID thumbnail = thumbnailCache.GetThumbnail(asset.path, asset.type);
        ImVec4 typeColor = thumbnailCache.GetTypeColor(asset.type);

        // Draw thumbnail button with colored background
        ImGui::PushStyleColor(ImGuiCol_Button, typeColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(typeColor.x * 1.2f, typeColor.y * 1.2f, typeColor.z * 1.2f, typeColor.w));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(typeColor.x * 1.4f, typeColor.y * 1.4f, typeColor.z * 1.4f, typeColor.w));

        bool clicked = ImGui::Button("##thumb", ImVec2(thumbnailSize, thumbnailSize));

        ImGui::PopStyleColor(3);

        // Handle double-click for directories
        if (asset.isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_assetBrowser->NavigateToDirectory(asset.path);
        }

        // Handle single click for files
        if (!asset.isDirectory && clicked) {
            m_assetBrowser->SetSelectedAsset(asset.path);
            spdlog::info("Selected asset: {}", asset.path);
        }

        // Context menu on right-click
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            contextMenuPath = asset.path;
            showContextMenu = true;
        }

        // Display asset name (truncate if too long)
        std::string displayName = asset.name;
        if (displayName.length() > 15) {
            displayName = displayName.substr(0, 12) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (thumbnailSize - textSize.x) * 0.5f);
        ImGui::TextWrapped("%s", displayName.c_str());

        // Show tooltip with full name
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", asset.name.c_str());
            ImGui::Text("Type: %s", asset.type.c_str());
            if (!asset.isDirectory) {
                ImGui::Text("Size: %llu bytes", asset.fileSize);
            }
            ImGui::EndTooltip();
        }

        ImGui::EndGroup();
        ImGui::PopID();

        // Layout in columns
        if ((itemIndex + 1) % columns != 0 && itemIndex < (int)assets.size() - 1) {
            ImGui::SameLine();
        }

        itemIndex++;
    }

    // Context menu
    if (showContextMenu) {
        ImGui::OpenPopup("AssetContextMenu");
        showContextMenu = false;
    }

    static char renameBuffer[256] = "";
    static bool showRenamePopup = false;

    if (ImGui::BeginPopup("AssetContextMenu")) {
        ImGui::Text("Asset: %s", contextMenuPath.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Rename")) {
            // Extract filename from path
            size_t lastSlash = contextMenuPath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                contextMenuPath.substr(lastSlash + 1) : contextMenuPath;
            strncpy(renameBuffer, filename.c_str(), sizeof(renameBuffer) - 1);
            showRenamePopup = true;
        }

        if (ImGui::MenuItem("Delete")) {
            if (m_assetBrowser->DeleteAsset(contextMenuPath)) {
                spdlog::info("Deleted asset: {}", contextMenuPath);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh")) {
            m_assetBrowser->Refresh();
        }

        ImGui::EndPopup();
    }

    // Rename popup
    if (showRenamePopup) {
        ImGui::OpenPopup("Rename Asset");
    }

    if (ImGui::BeginPopupModal("Rename Asset", &showRenamePopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name:");
        ImGui::InputText("##rename", renameBuffer, sizeof(renameBuffer));

        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            if (strlen(renameBuffer) > 0) {
                // Build new path
                size_t lastSlash = contextMenuPath.find_last_of("/\\");
                std::string newPath = (lastSlash != std::string::npos) ?
                    contextMenuPath.substr(0, lastSlash + 1) + renameBuffer : renameBuffer;

                if (m_assetBrowser->RenameAsset(contextMenuPath, newPath)) {
                    spdlog::info("Renamed asset: {} -> {}", contextMenuPath, newPath);
                }
            }
            showRenamePopup = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenamePopup = false;
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::EndChild();
}

void StandaloneEditor::RenderDetailsContent() {
    if (m_selectedObjectIndex >= 0) {
        ImGui::Text("Selected Object");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &m_selectedObjectPosition.x, 0.1f);
            ImGui::DragFloat3("Rotation", &m_selectedObjectRotation.x, 1.0f);
            ImGui::DragFloat3("Scale", &m_selectedObjectScale.x, 0.01f);
        }

        if (ImGui::CollapsingHeader("Rendering")) {
            static bool castShadows = true;
            static bool receiveShadows = true;
            ImGui::Checkbox("Cast Shadows", &castShadows);
            ImGui::Checkbox("Receive Shadows", &receiveShadows);
        }

        if (ImGui::CollapsingHeader("Physics")) {
            static bool enablePhysics = false;
            static float mass = 1.0f;
            ImGui::Checkbox("Enable Physics", &enablePhysics);
            if (enablePhysics) {
                ImGui::DragFloat("Mass", &mass, 0.1f);
            }
        }
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No object selected");
        ImGui::Separator();

        if (ImGui::CollapsingHeader("Scene Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            static float ambientIntensity = 0.2f;
            ImGui::SliderFloat("Ambient", &ambientIntensity, 0.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Lighting")) {
            static float sunDirection[3] = {-0.5f, -1.0f, -0.5f};
            ImGui::DragFloat3("Sun Dir", sunDirection, 0.01f, -1.0f, 1.0f);
        }
    }
}

void StandaloneEditor::RenderMaterialEditorContent() {
    ImGui::Text("Material Editor");
    ImGui::Separator();
    ImGui::TextWrapped("Material editor UI goes here...");
}

void StandaloneEditor::RenderEngineStatsContent() {
    ImGui::Text("Engine Statistics");
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Frame: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

// ========================================
// Edit Menu and Map Properties Implementation
// ========================================


// ========================================
// New Object Selection and Manipulation System
// ========================================

void StandaloneEditor::SelectObjectAtScreenPos(int x, int y) {
    // Generate ray from screen position
    glm::vec3 rayDir = ScreenToWorldRay(x, y);

    // Ray origin is the camera position
    glm::vec3 rayOrigin;
    if (m_currentCamera) {
        rayOrigin = m_currentCamera->GetPosition();
    } else {
        rayOrigin = m_editorCameraPos;
    }

    spdlog::info("Ray-casting from screen position ({}, {}) - Origin: ({:.2f}, {:.2f}, {:.2f}), Dir: ({:.2f}, {:.2f}, {:.2f})",
                x, y,
                rayOrigin.x, rayOrigin.y, rayOrigin.z,
                rayDir.x, rayDir.y, rayDir.z);

    // Perform ray-object intersection
    SelectObject(rayOrigin, rayDir);
}

void StandaloneEditor::SelectObjectByIndex(int index, bool addToSelection) {
    if (index < 0 || index >= static_cast<int>(m_sceneObjects.size())) {
        spdlog::warn("Invalid object index: {}", index);
        return;
    }
    
    if (addToSelection) {
        // Multi-selection mode
        m_isMultiSelectMode = true;
        auto it = std::find(m_selectedObjectIndices.begin(), m_selectedObjectIndices.end(), index);
        if (it == m_selectedObjectIndices.end()) {
            m_selectedObjectIndices.push_back(index);
        }
    } else {
        // Single selection mode
        m_selectedObjectIndices.clear();
        m_selectedObjectIndices.push_back(index);
        m_isMultiSelectMode = false;
    }
    
    m_selectedObjectIndex = index;
    
    // Update transform values
    auto& obj = m_sceneObjects[index];
    m_selectedObjectPosition = obj.position;
    m_selectedObjectRotation = obj.rotation;
    m_selectedObjectScale = obj.scale;
    
    spdlog::info("Selected object: {}", obj.name);
}

void StandaloneEditor::ClearSelection() {
    m_selectedObjectIndex = -1;
    m_selectedObjectIndices.clear();
    m_isMultiSelectMode = false;
    spdlog::info("Selection cleared");
}

void StandaloneEditor::DeleteSelectedObjects() {
    if (m_selectedObjectIndices.empty() && m_selectedObjectIndex < 0) {
        spdlog::warn("No objects selected for deletion");
        return;
    }
    
    // Collect indices to delete
    std::vector<int> indicesToDelete;
    if (!m_selectedObjectIndices.empty()) {
        indicesToDelete = m_selectedObjectIndices;
    } else if (m_selectedObjectIndex >= 0) {
        indicesToDelete.push_back(m_selectedObjectIndex);
    }
    
    // Sort in descending order to avoid index shifting issues
    std::sort(indicesToDelete.rbegin(), indicesToDelete.rend());
    
    // Delete objects
    for (int index : indicesToDelete) {
        if (index >= 0 && index < static_cast<int>(m_sceneObjects.size())) {
            spdlog::info("Deleting object: {}", m_sceneObjects[index].name);
            m_sceneObjects.erase(m_sceneObjects.begin() + index);
        }
    }
    
    // Clear selection
    ClearSelection();
}

// ========================================
// Transform Gizmo Rendering
// ========================================

void StandaloneEditor::RenderSelectionOutline() {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    // Note: The actual outline rendering is done in Render3D() which has access to DebugDraw.
    // This function exists to update selection-related state that may be needed for UI or other purposes.

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Keep the transform values in sync with the selected object
    // This ensures the Details panel shows current values
    m_selectedObjectPosition = obj.position;
    m_selectedObjectRotation = obj.rotation;
    m_selectedObjectScale = obj.scale;
}

void StandaloneEditor::RenderTransformGizmo(Nova::DebugDraw& debugDraw) {
    if (m_selectedObjectIndex < 0) {
        return;
    }

    if (m_editMode != EditMode::ObjectSelect) {
        return;
    }

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    switch (m_transformTool) {
        case TransformTool::Move:
            RenderMoveGizmo(debugDraw, obj.position);
            break;
        case TransformTool::Rotate:
            RenderRotateGizmo(debugDraw, obj.position, obj.rotation);
            break;
        case TransformTool::Scale:
            RenderScaleGizmo(debugDraw, obj.position, obj.scale);
            break;
        case TransformTool::None:
        default:
            break;
    }
}

void StandaloneEditor::RenderMoveGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position) {
    // Render move arrows (X/Y/Z) using DebugDraw
    float arrowLength = 2.0f;
    float arrowHeadSize = 0.3f;

    // Get colors with hover/drag feedback
    GizmoAxis activeAxis = m_gizmoDragging ? m_dragAxis : m_hoveredAxis;
    glm::vec4 xColor = GetGizmoAxisColor(GizmoAxis::X, m_hoveredAxis, m_dragAxis);
    glm::vec4 yColor = GetGizmoAxisColor(GizmoAxis::Y, m_hoveredAxis, m_dragAxis);
    glm::vec4 zColor = GetGizmoAxisColor(GizmoAxis::Z, m_hoveredAxis, m_dragAxis);
    glm::vec4 centerColor = GetGizmoAxisColor(GizmoAxis::Center, m_hoveredAxis, m_dragAxis);

    // X axis - Red arrow (or highlighted)
    debugDraw.AddArrow(position, position + glm::vec3(arrowLength, 0, 0), xColor, arrowHeadSize);

    // Y axis - Green arrow (or highlighted)
    debugDraw.AddArrow(position, position + glm::vec3(0, arrowLength, 0), yColor, arrowHeadSize);

    // Z axis - Blue arrow (or highlighted)
    debugDraw.AddArrow(position, position + glm::vec3(0, 0, arrowLength), zColor, arrowHeadSize);

    // Add small sphere at origin for better visibility
    debugDraw.AddSphere(position, 0.15f, centerColor, 8);
}

void StandaloneEditor::RenderRotateGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position, const glm::vec3& rotation) {
    // Render rotation circles using DebugDraw
    float circleRadius = 1.5f;
    int segments = 32; // Higher segment count for smoother circles

    // Get colors with hover/drag feedback
    glm::vec4 xColor = GetGizmoAxisColor(GizmoAxis::X, m_hoveredAxis, m_dragAxis);
    glm::vec4 yColor = GetGizmoAxisColor(GizmoAxis::Y, m_hoveredAxis, m_dragAxis);
    glm::vec4 zColor = GetGizmoAxisColor(GizmoAxis::Z, m_hoveredAxis, m_dragAxis);
    glm::vec4 centerColor = GetGizmoAxisColor(GizmoAxis::Center, m_hoveredAxis, m_dragAxis);

    // X axis rotation - Red circle (YZ plane) (or highlighted)
    debugDraw.AddCircle(position, circleRadius, glm::vec3(1.0f, 0.0f, 0.0f), xColor, segments);

    // Y axis rotation - Green circle (XZ plane) (or highlighted)
    debugDraw.AddCircle(position, circleRadius, glm::vec3(0.0f, 1.0f, 0.0f), yColor, segments);

    // Z axis rotation - Blue circle (XY plane) (or highlighted)
    debugDraw.AddCircle(position, circleRadius, glm::vec3(0.0f, 0.0f, 1.0f), zColor, segments);

    // Add small sphere at center for better visibility
    debugDraw.AddSphere(position, 0.15f, centerColor, 8);
}

void StandaloneEditor::RenderScaleGizmo(Nova::DebugDraw& debugDraw, const glm::vec3& position, const glm::vec3& scale) {
    // Render scale handles using lines and boxes
    float lineLength = 2.0f;
    float handleSize = 0.2f;

    // X axis - Red line with box handle at end
    glm::vec3 xEnd = position + glm::vec3(lineLength, 0.0f, 0.0f);
    debugDraw.AddLine(position, xEnd, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    glm::mat4 xTransform = glm::translate(glm::mat4(1.0f), xEnd);
    debugDraw.AddBox(xTransform, glm::vec3(handleSize), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    // Y axis - Green line with box handle at end
    glm::vec3 yEnd = position + glm::vec3(0.0f, lineLength, 0.0f);
    debugDraw.AddLine(position, yEnd, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    glm::mat4 yTransform = glm::translate(glm::mat4(1.0f), yEnd);
    debugDraw.AddBox(yTransform, glm::vec3(handleSize), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    // Z axis - Blue line with box handle at end
    glm::vec3 zEnd = position + glm::vec3(0.0f, 0.0f, lineLength);
    debugDraw.AddLine(position, zEnd, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    glm::mat4 zTransform = glm::translate(glm::mat4(1.0f), zEnd);
    debugDraw.AddBox(zTransform, glm::vec3(handleSize), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    // Add small sphere at center for uniform scaling handle
    debugDraw.AddSphere(position, 0.15f, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 8);
}

// ========================================
// Helper Methods
// ========================================

glm::vec3 StandaloneEditor::ScreenToWorldRay(int screenX, int screenY) {
    // Use the camera's ScreenToWorldRay method if available
    if (m_currentCamera) {
        auto& window = Nova::Engine::Instance().GetWindow();
        glm::vec2 screenPos(static_cast<float>(screenX), static_cast<float>(screenY));
        glm::vec2 screenSize(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        return m_currentCamera->ScreenToWorldRay(screenPos, screenSize);
    }

    // Fallback: Manual implementation using editor camera
    auto& window = Nova::Engine::Instance().GetWindow();
    float screenWidth = static_cast<float>(window.GetWidth());
    float screenHeight = static_cast<float>(window.GetHeight());

    // Convert screen coordinates to NDC (Normalized Device Coordinates)
    float x = (2.0f * screenX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * screenY) / screenHeight;  // Flip Y axis

    // Create view and projection matrices using editor camera
    glm::vec3 cameraPos = m_editorCameraPos;
    glm::vec3 cameraTarget = m_editorCameraTarget;
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, up);
    float aspectRatio = screenWidth / screenHeight;
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

    // Unproject: screen -> NDC -> clip -> eye -> world
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(projection) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::vec3(glm::inverse(view) * rayEye);
    return glm::normalize(rayWorld);
}

bool StandaloneEditor::RayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                          const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                                          float& distance) {
    // Ray-AABB intersection using slab method
    glm::vec3 invDir = 1.0f / rayDir;
    
    float tmin = (aabbMin.x - rayOrigin.x) * invDir.x;
    float tmax = (aabbMax.x - rayOrigin.x) * invDir.x;
    
    if (tmin > tmax) std::swap(tmin, tmax);
    
    float tymin = (aabbMin.y - rayOrigin.y) * invDir.y;
    float tymax = (aabbMax.y - rayOrigin.y) * invDir.y;
    
    if (tymin > tymax) std::swap(tymin, tymax);
    
    if ((tmin > tymax) || (tymin > tmax)) {
        return false;
    }
    
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;
    
    float tzmin = (aabbMin.z - rayOrigin.z) * invDir.z;
    float tzmax = (aabbMax.z - rayOrigin.z) * invDir.z;
    
    if (tzmin > tzmax) std::swap(tzmin, tzmax);
    
    if ((tmin > tzmax) || (tzmin > tmax)) {
        return false;
    }
    
    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;
    
    // Only return positive distances (in front of ray origin)
    distance = tmin >= 0.0f ? tmin : tmax;
    return distance >= 0.0f;
}

bool StandaloneEditor::RayIntersectsSphere(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                           const glm::vec3& sphereCenter, float radius, float& distance) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDir, rayDir);
    float b = 2.0f * glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return false;
    }

    distance = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (distance < 0) {
        distance = (-b + std::sqrt(discriminant)) / (2.0f * a);
    }

    return distance >= 0;
}

bool StandaloneEditor::RayIntersectsCylinder(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                             const glm::vec3& cylinderStart, const glm::vec3& cylinderEnd,
                                             float radius, float& distance) {
    glm::vec3 axis = glm::normalize(cylinderEnd - cylinderStart);
    float cylinderLength = glm::length(cylinderEnd - cylinderStart);

    // Check if ray is close enough to the cylinder axis
    float minDist = RayToLineDistance(rayOrigin, rayDir, cylinderStart, axis);

    if (minDist > radius) {
        return false;
    }

    // Project intersection point onto cylinder axis to check bounds
    glm::vec3 nearestPoint = rayOrigin + rayDir * glm::dot(cylinderStart - rayOrigin, rayDir);
    float t = glm::dot(nearestPoint - cylinderStart, axis);

    if (t < 0 || t > cylinderLength) {
        return false;
    }

    distance = glm::length(nearestPoint - rayOrigin);
    return true;
}

float StandaloneEditor::RayToLineDistance(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                          const glm::vec3& linePoint, const glm::vec3& lineDir) {
    glm::vec3 w = rayOrigin - linePoint;
    float a = glm::dot(rayDir, rayDir);
    float b = glm::dot(rayDir, lineDir);
    float c = glm::dot(lineDir, lineDir);
    float d = glm::dot(rayDir, w);
    float e = glm::dot(lineDir, w);

    float denominator = a * c - b * b;
    if (std::abs(denominator) < 0.0001f) {
        // Lines are parallel
        return glm::length(glm::cross(w, lineDir)) / glm::length(lineDir);
    }

    float s = (b * e - c * d) / denominator;
    float t = (a * e - b * d) / denominator;

    glm::vec3 p1 = rayOrigin + s * rayDir;
    glm::vec3 p2 = linePoint + t * lineDir;

    return glm::length(p2 - p1);
}

// ========================================
// Gizmo Interaction Implementations
// ========================================

void StandaloneEditor::UpdateGizmoInteraction(float deltaTime) {
    if (!m_currentCamera) {
        return;
    }

    if (m_editMode != EditMode::ObjectSelect || m_transformTool == TransformTool::None) {
        return;
    }

    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    auto& input = Nova::Engine::Instance().GetInput();

    // Don't interact with gizmo if ImGui wants mouse
    if (ImGui::GetIO().WantCaptureMouse) {
        m_gizmoDragging = false;
        m_hoveredAxis = GizmoAxis::None;
        return;
    }

    glm::vec2 mousePos = input.GetMousePosition();
    glm::vec2 mouseDelta = input.GetMouseDelta();
    bool mousePressed = input.IsMouseButtonDown(Nova::MouseButton::Left);

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Calculate ray from mouse position
    glm::vec3 rayDir = ScreenToWorldRay(static_cast<int>(mousePos.x), static_cast<int>(mousePos.y));
    glm::vec3 rayOrigin = m_editorCameraPos;

    // Start dragging
    if (mousePressed && !m_gizmoDragging) {
        GizmoAxis hitAxis = GizmoAxis::None;

        switch (m_transformTool) {
            case TransformTool::Move:
                hitAxis = RaycastMoveGizmo(rayOrigin, rayDir, obj.position);
                break;
            case TransformTool::Rotate:
                hitAxis = RaycastRotateGizmo(rayOrigin, rayDir, obj.position);
                break;
            case TransformTool::Scale:
                hitAxis = RaycastScaleGizmo(rayOrigin, rayDir, obj.position);
                break;
            default:
                break;
        }

        if (hitAxis != GizmoAxis::None) {
            m_gizmoDragging = true;
            m_dragAxis = hitAxis;
            m_dragStartMousePos = mousePos;
            m_dragStartObjectPos = obj.position;
            m_dragStartObjectRot = obj.rotation;
            m_dragStartObjectScale = obj.scale;
            m_dragStartDistance = glm::length(obj.position - rayOrigin);

            // Set up drag plane normal for translation
            if (m_transformTool == TransformTool::Move) {
                switch (hitAxis) {
                    case GizmoAxis::X:
                        m_dragPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
                        break;
                    case GizmoAxis::Y:
                        m_dragPlaneNormal = glm::vec3(0.0f, 0.0f, 1.0f);
                        break;
                    case GizmoAxis::Z:
                        m_dragPlaneNormal = glm::vec3(1.0f, 0.0f, 0.0f);
                        break;
                    default:
                        m_dragPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);
                        break;
                }
            }
        }
    }

    // Continue dragging
    if (m_gizmoDragging && mousePressed) {
        switch (m_transformTool) {
            case TransformTool::Move:
                ApplyMoveTransform(m_dragAxis, rayOrigin, rayDir);
                break;
            case TransformTool::Rotate:
                ApplyRotateTransform(m_dragAxis, mouseDelta);
                break;
            case TransformTool::Scale:
                ApplyScaleTransform(m_dragAxis, mouseDelta);
                break;
            default:
                break;
        }
    }

    // Stop dragging
    if (!mousePressed && m_gizmoDragging) {
        m_gizmoDragging = false;
        m_dragAxis = GizmoAxis::None;

        // Add to undo/redo history if transform actually changed
        bool positionChanged = (obj.position != m_dragStartObjectPos);
        bool rotationChanged = (obj.rotation != m_dragStartObjectRot);
        bool scaleChanged = (obj.scale != m_dragStartObjectScale);

        if (positionChanged || rotationChanged || scaleChanged) {
            spdlog::debug("Recording transform change to undo history");

            // Store the final transform values
            m_selectedObjectPosition = obj.position;
            m_selectedObjectRotation = obj.rotation;
            m_selectedObjectScale = obj.scale;

            // Note: Full undo/redo command creation requires ObjectTransformCommand
            // which uses a different object data structure. For now, log the change.
            spdlog::info("Transform recorded - Pos: ({:.2f}, {:.2f}, {:.2f}) -> ({:.2f}, {:.2f}, {:.2f})",
                        m_dragStartObjectPos.x, m_dragStartObjectPos.y, m_dragStartObjectPos.z,
                        obj.position.x, obj.position.y, obj.position.z);
        }
    }

    // Update hovered axis when not dragging
    if (!m_gizmoDragging) {
        switch (m_transformTool) {
            case TransformTool::Move:
                m_hoveredAxis = RaycastMoveGizmo(rayOrigin, rayDir, obj.position);
                break;
            case TransformTool::Rotate:
                m_hoveredAxis = RaycastRotateGizmo(rayOrigin, rayDir, obj.position);
                break;
            case TransformTool::Scale:
                m_hoveredAxis = RaycastScaleGizmo(rayOrigin, rayDir, obj.position);
                break;
            default:
                m_hoveredAxis = GizmoAxis::None;
                break;
        }
    }
}

StandaloneEditor::GizmoAxis StandaloneEditor::RaycastMoveGizmo(const glm::vec3& rayOrigin,
                                                                const glm::vec3& rayDir,
                                                                const glm::vec3& gizmoPos) {
    float arrowLength = 2.0f;
    float hitRadius = 0.15f;  // Larger hit area for easier selection
    float closestDist = std::numeric_limits<float>::max();
    GizmoAxis closestAxis = GizmoAxis::None;

    // Check X axis (red arrow)
    {
        glm::vec3 axisEnd = gizmoPos + glm::vec3(arrowLength, 0, 0);
        float dist = RayToLineDistance(rayOrigin, rayDir, gizmoPos, glm::vec3(1, 0, 0));

        // Check if within arrow length
        glm::vec3 rayPoint = rayOrigin + rayDir * glm::dot(gizmoPos - rayOrigin, rayDir);
        float t = glm::dot(rayPoint - gizmoPos, glm::vec3(1, 0, 0));

        if (dist < hitRadius && t >= 0 && t <= arrowLength && dist < closestDist) {
            closestDist = dist;
            closestAxis = GizmoAxis::X;
        }
    }

    // Check Y axis (green arrow)
    {
        glm::vec3 axisEnd = gizmoPos + glm::vec3(0, arrowLength, 0);
        float dist = RayToLineDistance(rayOrigin, rayDir, gizmoPos, glm::vec3(0, 1, 0));

        glm::vec3 rayPoint = rayOrigin + rayDir * glm::dot(gizmoPos - rayOrigin, rayDir);
        float t = glm::dot(rayPoint - gizmoPos, glm::vec3(0, 1, 0));

        if (dist < hitRadius && t >= 0 && t <= arrowLength && dist < closestDist) {
            closestDist = dist;
            closestAxis = GizmoAxis::Y;
        }
    }

    // Check Z axis (blue arrow)
    {
        glm::vec3 axisEnd = gizmoPos + glm::vec3(0, 0, arrowLength);
        float dist = RayToLineDistance(rayOrigin, rayDir, gizmoPos, glm::vec3(0, 0, 1));

        glm::vec3 rayPoint = rayOrigin + rayDir * glm::dot(gizmoPos - rayOrigin, rayDir);
        float t = glm::dot(rayPoint - gizmoPos, glm::vec3(0, 0, 1));

        if (dist < hitRadius && t >= 0 && t <= arrowLength && dist < closestDist) {
            closestDist = dist;
            closestAxis = GizmoAxis::Z;
        }
    }

    // Check center sphere
    {
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, gizmoPos, 0.15f, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::Center;
            }
        }
    }

    return closestAxis;
}

StandaloneEditor::GizmoAxis StandaloneEditor::RaycastRotateGizmo(const glm::vec3& rayOrigin,
                                                                  const glm::vec3& rayDir,
                                                                  const glm::vec3& gizmoPos) {
    float circleRadius = 1.5f;
    float hitThickness = 0.15f;
    float closestDist = std::numeric_limits<float>::max();
    GizmoAxis closestAxis = GizmoAxis::None;

    // For each rotation circle, check if ray is close to the circle perimeter
    // This is a simplified approach - checking distance to circle center and comparing to radius

    // Check X axis circle (YZ plane)
    {
        glm::vec3 planeNormal(1, 0, 0);
        float denom = glm::dot(rayDir, planeNormal);
        if (std::abs(denom) > 0.0001f) {
            float t = glm::dot(gizmoPos - rayOrigin, planeNormal) / denom;
            if (t >= 0) {
                glm::vec3 hitPoint = rayOrigin + rayDir * t;
                float distToCenter = glm::length(hitPoint - gizmoPos);
                if (std::abs(distToCenter - circleRadius) < hitThickness && t < closestDist) {
                    closestDist = t;
                    closestAxis = GizmoAxis::X;
                }
            }
        }
    }

    // Check Y axis circle (XZ plane)
    {
        glm::vec3 planeNormal(0, 1, 0);
        float denom = glm::dot(rayDir, planeNormal);
        if (std::abs(denom) > 0.0001f) {
            float t = glm::dot(gizmoPos - rayOrigin, planeNormal) / denom;
            if (t >= 0) {
                glm::vec3 hitPoint = rayOrigin + rayDir * t;
                float distToCenter = glm::length(hitPoint - gizmoPos);
                if (std::abs(distToCenter - circleRadius) < hitThickness && t < closestDist) {
                    closestDist = t;
                    closestAxis = GizmoAxis::Y;
                }
            }
        }
    }

    // Check Z axis circle (XY plane)
    {
        glm::vec3 planeNormal(0, 0, 1);
        float denom = glm::dot(rayDir, planeNormal);
        if (std::abs(denom) > 0.0001f) {
            float t = glm::dot(gizmoPos - rayOrigin, planeNormal) / denom;
            if (t >= 0) {
                glm::vec3 hitPoint = rayOrigin + rayDir * t;
                float distToCenter = glm::length(hitPoint - gizmoPos);
                if (std::abs(distToCenter - circleRadius) < hitThickness && t < closestDist) {
                    closestDist = t;
                    closestAxis = GizmoAxis::Z;
                }
            }
        }
    }

    // Check center sphere
    {
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, gizmoPos, 0.15f, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::Center;
            }
        }
    }

    return closestAxis;
}

StandaloneEditor::GizmoAxis StandaloneEditor::RaycastScaleGizmo(const glm::vec3& rayOrigin,
                                                                 const glm::vec3& rayDir,
                                                                 const glm::vec3& gizmoPos) {
    float lineLength = 2.0f;
    float handleSize = 0.2f;
    float hitRadius = 0.15f;
    float closestDist = std::numeric_limits<float>::max();
    GizmoAxis closestAxis = GizmoAxis::None;

    // Check X axis handle
    {
        glm::vec3 handlePos = gizmoPos + glm::vec3(lineLength, 0, 0);
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, handlePos, handleSize, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::X;
            }
        }
    }

    // Check Y axis handle
    {
        glm::vec3 handlePos = gizmoPos + glm::vec3(0, lineLength, 0);
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, handlePos, handleSize, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::Y;
            }
        }
    }

    // Check Z axis handle
    {
        glm::vec3 handlePos = gizmoPos + glm::vec3(0, 0, lineLength);
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, handlePos, handleSize, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::Z;
            }
        }
    }

    // Check center sphere for uniform scaling
    {
        float dist;
        if (RayIntersectsSphere(rayOrigin, rayDir, gizmoPos, 0.15f, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestAxis = GizmoAxis::Center;
            }
        }
    }

    return closestAxis;
}

void StandaloneEditor::ApplyMoveTransform(GizmoAxis axis, const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Calculate intersection with drag plane
    float denom = glm::dot(rayDir, m_dragPlaneNormal);
    if (std::abs(denom) < 0.0001f) {
        return;  // Ray parallel to plane
    }

    float t = glm::dot(m_dragStartObjectPos - rayOrigin, m_dragPlaneNormal) / denom;
    if (t < 0) {
        return;  // Intersection behind camera
    }

    glm::vec3 hitPoint = rayOrigin + rayDir * t;
    glm::vec3 delta = hitPoint - m_dragStartObjectPos;

    // Constrain movement to selected axis
    glm::vec3 movement(0.0f);
    switch (axis) {
        case GizmoAxis::X:
            movement.x = delta.x;
            break;
        case GizmoAxis::Y:
            movement.y = delta.y;
            break;
        case GizmoAxis::Z:
            movement.z = delta.z;
            break;
        case GizmoAxis::Center:
            movement = delta;  // Free movement
            break;
        default:
            break;
    }

    obj.position = m_dragStartObjectPos + movement;

    // Apply grid snapping if enabled
    if (m_snapToGrid || m_snapToGridEnabled) {
        float gridSize = m_gridSize;
        obj.position.x = std::round(obj.position.x / gridSize) * gridSize;
        obj.position.y = std::round(obj.position.y / gridSize) * gridSize;
        obj.position.z = std::round(obj.position.z / gridSize) * gridSize;
    }
}

void StandaloneEditor::ApplyRotateTransform(GizmoAxis axis, const glm::vec2& mouseDelta) {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Convert mouse movement to rotation angle
    float rotationSpeed = 0.5f;  // Degrees per pixel
    float angle = (mouseDelta.x + mouseDelta.y) * rotationSpeed;

    glm::vec3 rotationDelta(0.0f);
    switch (axis) {
        case GizmoAxis::X:
            rotationDelta.x = angle;
            break;
        case GizmoAxis::Y:
            rotationDelta.y = angle;
            break;
        case GizmoAxis::Z:
            rotationDelta.z = angle;
            break;
        default:
            break;
    }

    obj.rotation = m_dragStartObjectRot + rotationDelta;

    // Apply angle snapping if enabled
    if (m_snapToGridEnabled) {
        obj.rotation.x = std::round(obj.rotation.x / m_snapAngle) * m_snapAngle;
        obj.rotation.y = std::round(obj.rotation.y / m_snapAngle) * m_snapAngle;
        obj.rotation.z = std::round(obj.rotation.z / m_snapAngle) * m_snapAngle;
    }
}

void StandaloneEditor::ApplyScaleTransform(GizmoAxis axis, const glm::vec2& mouseDelta) {
    if (m_selectedObjectIndex < 0 || m_selectedObjectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return;
    }

    auto& obj = m_sceneObjects[m_selectedObjectIndex];

    // Convert mouse movement to scale factor
    float scaleSpeed = 0.01f;
    float scaleDelta = (mouseDelta.x + mouseDelta.y) * scaleSpeed;

    glm::vec3 scale = m_dragStartObjectScale;

    switch (axis) {
        case GizmoAxis::X:
            scale.x += scaleDelta;
            break;
        case GizmoAxis::Y:
            scale.y += scaleDelta;
            break;
        case GizmoAxis::Z:
            scale.z += scaleDelta;
            break;
        case GizmoAxis::Center:
            // Uniform scaling
            scale += glm::vec3(scaleDelta);
            break;
        default:
            break;
    }

    // Clamp scale to prevent negative or zero values
    scale.x = std::max(0.1f, scale.x);
    scale.y = std::max(0.1f, scale.y);
    scale.z = std::max(0.1f, scale.z);

    obj.scale = scale;
}

glm::vec4 StandaloneEditor::GetGizmoAxisColor(GizmoAxis axis, GizmoAxis hoveredAxis, GizmoAxis draggedAxis) {
    // If this axis is being dragged, use yellow highlight
    if (axis == draggedAxis) {
        return glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    }

    // If this axis is being hovered, use white highlight
    if (axis == hoveredAxis) {
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    // Default colors based on axis
    switch (axis) {
        case GizmoAxis::X:
            return glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
        case GizmoAxis::Y:
            return glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
        case GizmoAxis::Z:
            return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);  // Blue
        case GizmoAxis::Center:
            return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // White
        default:
            return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);  // Gray
    }
}

// ========================================
// Debug Overlay Implementations
// ========================================

void StandaloneEditor::RenderDebugOverlay() {
    if (!m_showDebugOverlay) return;

    float currentFPS = ImGui::GetIO().Framerate;
    float currentFrameTime = 1000.0f / currentFPS;

    m_fpsHistory.push_back(currentFPS);
    m_frameTimeHistory.push_back(currentFrameTime);

    if (m_fpsHistory.size() > m_historyMaxSize) {
        m_fpsHistory.erase(m_fpsHistory.begin());
    }
    if (m_frameTimeHistory.size() > m_historyMaxSize) {
        m_frameTimeHistory.erase(m_frameTimeHistory.begin());
    }

    ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 250), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Debug Overlay", &m_showDebugOverlay, flags)) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.82f, 1.0f), "Performance Metrics");
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", currentFPS);

        if (!m_fpsHistory.empty()) {
            ImGui::PlotLines("##FPS", m_fpsHistory.data(), (int)m_fpsHistory.size(),
                           0, "FPS History", 0.0f, 144.0f, ImVec2(320, 60));
        }

        ImGui::Spacing();
        ImGui::Text("Frame Time: %.3f ms", currentFrameTime);

        if (!m_frameTimeHistory.empty()) {
            ImGui::PlotLines("##FrameTime", m_frameTimeHistory.data(), (int)m_frameTimeHistory.size(),
                           0, "Frame Time (ms)", 0.0f, 33.3f, ImVec2(320, 60));
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Map Size: %dx%d", m_mapWidth, m_mapHeight);
        ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)",
                   m_editorCameraPos.x, m_editorCameraPos.y, m_editorCameraPos.z);

        if (m_selectedObjectIndex >= 0) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.82f, 1.0f), "Selected Object");
            ImGui::Text("Position: (%.1f, %.1f, %.1f)",
                       m_selectedObjectPosition.x, m_selectedObjectPosition.y, m_selectedObjectPosition.z);
        }
    }
    ImGui::End();
}

void StandaloneEditor::RenderProfiler() {
    if (!m_showProfiler) return;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 360, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 280), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Profiler", &m_showProfiler, flags)) {
        ImGui::TextColored(ImVec4(0.65f, 0.55f, 0.30f, 1.0f), "CPU Timing");
        ImGui::Separator();

        // Placeholder values - will be replaced with actual profiling data
        float updateTime = 3.1f;
        float renderTime = 8.2f;
        float totalTime = 16.7f;

        ImGui::Text("Update");
        ImGui::SameLine(120);
        ImGui::ProgressBar(updateTime / 16.7f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.2f ms", updateTime);

        ImGui::Text("Render");
        ImGui::SameLine(120);
        ImGui::ProgressBar(renderTime / 16.7f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.2f ms", renderTime);

        ImGui::Separator();
        ImGui::Text("Total");
        ImGui::SameLine(120);
        ImGui::ProgressBar(totalTime / 33.3f, ImVec2(-1, 0), "");
        ImGui::SameLine();
        ImGui::Text("%.2f ms", totalTime);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Average (60 frames)");
        ImGui::Text("  Update: 3.05 ms");
        ImGui::Text("  Render: 8.15 ms");
        ImGui::Text("  Total:  16.52 ms");

        ImGui::Spacing();
        ImGui::Text("Min / Max");
        ImGui::Text("  Update: 2.8 / 4.2 ms");
        ImGui::Text("  Render: 7.1 / 12.5 ms");
        ImGui::Text("  Total:  14.2 / 22.1 ms");
    }
    ImGui::End();
}

void StandaloneEditor::RenderMemoryStats() {
    if (!m_showMemoryStats) return;

    ImGui::SetNextWindowPos(ImVec2(10, ImGui::GetIO().DisplaySize.y - 240), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 230), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Memory Stats", &m_showMemoryStats, flags)) {
        ImGui::TextColored(ImVec4(0.45f, 0.35f, 0.65f, 1.0f), "Memory Usage");
        ImGui::Separator();

        // Calculate actual terrain memory usage
        size_t terrainTilesBytes = m_terrainTiles.size() * sizeof(int);
        size_t terrainHeightsBytes = m_terrainHeights.size() * sizeof(float);
        float terrainDataMB = (terrainTilesBytes + terrainHeightsBytes) / (1024.0f * 1024.0f);

        // Placeholder values for other memory categories
        float textureMemMB = 156.0f;
        float meshMemMB = 45.0f;
        float totalVRAM = 201.0f + terrainDataMB;
        float maxVRAM = 8192.0f;
        float systemRAM = 2100.0f;
        float maxSystemRAM = 16000.0f;

        ImGui::Text("Texture Memory");
        ImGui::ProgressBar(textureMemMB / maxVRAM, ImVec2(-1, 0));
        ImGui::SameLine();
        ImGui::Text("%.0f MB", textureMemMB);

        ImGui::Spacing();
        ImGui::Text("Mesh Memory");
        ImGui::ProgressBar(meshMemMB / maxVRAM, ImVec2(-1, 0));
        ImGui::SameLine();
        ImGui::Text("%.0f MB", meshMemMB);

        ImGui::Spacing();
        ImGui::Text("Terrain Data");
        ImGui::ProgressBar(terrainDataMB / maxVRAM, ImVec2(-1, 0));
        ImGui::SameLine();
        ImGui::Text("%.2f MB (%d tiles)", terrainDataMB, (int)m_terrainTiles.size());

        ImGui::Separator();
        ImGui::Text("Total VRAM");
        ImGui::ProgressBar(totalVRAM / maxVRAM, ImVec2(-1, 0));
        ImGui::SameLine();
        ImGui::Text("%.0f / %.0f MB", totalVRAM, maxVRAM);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("System RAM");
        ImGui::ProgressBar(systemRAM / maxSystemRAM, ImVec2(-1, 0));
        ImGui::SameLine();
        ImGui::Text("%.1f / %.0f GB", systemRAM / 1000.0f, maxSystemRAM / 1000.0f);
    }
    ImGui::End();
}

void StandaloneEditor::RenderTimeDistribution() {
    if (!m_showRenderTime && !m_showUpdateTime && !m_showPhysicsTime) return;

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 260, ImGui::GetIO().DisplaySize.y - 180), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 170), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::Begin("Time Distribution", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.98f, 1.0f), "Frame Breakdown");
        ImGui::Separator();

        // Placeholder values - will be replaced with actual profiling data
        float renderMs = 8.2f;
        float updateMs = 3.1f;
        float physicsMs = 1.3f;
        float totalMs = renderMs + updateMs + physicsMs;

        float renderPercent = (renderMs / totalMs) * 100.0f;
        float updatePercent = (updateMs / totalMs) * 100.0f;
        float physicsPercent = (physicsMs / totalMs) * 100.0f;

        auto GetTimeColor = [](float ms) -> ImVec4 {
            if (ms < 16.0f) return ImVec4(0.0f, 0.8f, 0.2f, 1.0f);      // Green - good
            else if (ms < 33.0f) return ImVec4(0.9f, 0.9f, 0.0f, 1.0f); // Yellow - warning
            else return ImVec4(0.9f, 0.1f, 0.1f, 1.0f);                 // Red - bad
        };

        if (m_showRenderTime) {
            ImGui::TextColored(GetTimeColor(renderMs), "Render:");
            ImGui::SameLine(80);
            ImGui::Text("%.1f ms (%.0f%%)", renderMs, renderPercent);
        }

        if (m_showUpdateTime) {
            ImGui::TextColored(GetTimeColor(updateMs), "Update:");
            ImGui::SameLine(80);
            ImGui::Text("%.1f ms (%.0f%%)", updateMs, updatePercent);
        }

        if (m_showPhysicsTime) {
            ImGui::TextColored(GetTimeColor(physicsMs), "Physics:");
            ImGui::SameLine(80);
            ImGui::Text("%.1f ms (%.0f%%)", physicsMs, physicsPercent);
        }

        ImGui::Separator();
        ImGui::TextColored(GetTimeColor(totalMs), "Total:");
        ImGui::SameLine(80);
        ImGui::Text("%.1f ms", totalMs);
    }
    ImGui::End();
}
void StandaloneEditor::ShowMapPropertiesDialog() {
    ImGui::OpenPopup("Map Properties");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Map Properties", &m_showMapPropertiesDialog, ImGuiWindowFlags_AlwaysAutoResize)) {

        // Static variables to preserve dialog state
        static char nameBuf[256];
        static WorldType worldType = WorldType::Flat;
        static float worldRadius = 6371.0f;
        static int mapWidth = 64;
        static int mapHeight = 64;
        static float minHeight = -100.0f;
        static float maxHeight = 8848.0f;
        static bool initialized = false;

        // Initialize from current values
        if (!initialized) {
            strncpy(nameBuf, m_mapName.c_str(), sizeof(nameBuf) - 1);
            worldType = m_worldType;
            worldRadius = m_worldRadius;
            mapWidth = m_mapWidth;
            mapHeight = m_mapHeight;
            minHeight = m_minHeight;
            maxHeight = m_maxHeight;
            initialized = true;
        }

        // ===== Map Name Section =====
        ModernUI::GradientHeader("Map Information");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(450);
        ImGui::InputText("Map Name", nameBuf, sizeof(nameBuf));

        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        // ===== World Type Section =====
        ModernUI::GradientHeader("World Type");
        ImGui::Spacing();

        bool isFlatWorld = (worldType == WorldType::Flat);
        bool isSphericalWorld = (worldType == WorldType::Spherical);

        if (ImGui::RadioButton("Flat World", isFlatWorld)) {
            worldType = WorldType::Flat;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Traditional flat map)");

        if (ImGui::RadioButton("Spherical World", isSphericalWorld)) {
            worldType = WorldType::Spherical;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Planet surface)");

        // ===== Spherical World Settings (conditional) =====
        if (worldType == WorldType::Spherical) {
            ImGui::Spacing();
            ImGui::Indent(20.0f);

            ImGui::SetNextItemWidth(200);
            ImGui::InputFloat("World Radius (km)", &worldRadius, 100.0f, 1000.0f, "%.0f");

            // Clamp radius to reasonable values
            if (worldRadius < 100.0f) worldRadius = 100.0f;
            if (worldRadius > 100000.0f) worldRadius = 100000.0f;

            ImGui::Spacing();
            ImGui::Text("Presets:");
            ImGui::SameLine();

            if (ModernUI::GlowButton("Earth", ImVec2(80, 0))) {
                worldRadius = 6371.0f;
            }
            ImGui::SameLine();

            if (ModernUI::GlowButton("Mars", ImVec2(80, 0))) {
                worldRadius = 3390.0f;
            }
            ImGui::SameLine();

            if (ModernUI::GlowButton("Moon", ImVec2(80, 0))) {
                worldRadius = 1737.0f;
            }

            ImGui::Unindent(20.0f);
        }

        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        // ===== Map Dimensions Section =====
        ModernUI::GradientHeader("Map Dimensions");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Width (chunks)", &mapWidth);
        if (mapWidth < 1) mapWidth = 1;
        if (mapWidth > 512) mapWidth = 512;

        ImGui::SetNextItemWidth(200);
        ImGui::InputInt("Height (chunks)", &mapHeight);
        if (mapHeight < 1) mapHeight = 1;
        if (mapHeight > 512) mapHeight = 512;

        ImGui::TextDisabled("Total chunks: %d", mapWidth * mapHeight);

        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();

        // ===== Terrain Settings Section =====
        ModernUI::GradientHeader("Terrain Settings");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(200);
        ImGui::InputFloat("Min Height (m)", &minHeight, 10.0f, 100.0f, "%.0f");

        ImGui::SetNextItemWidth(200);
        ImGui::InputFloat("Max Height (m)", &maxHeight, 10.0f, 100.0f, "%.0f");

        // Ensure min < max
        if (minHeight >= maxHeight) {
            minHeight = maxHeight - 100.0f;
        }

        ImGui::TextDisabled("Height range: %.0f meters", maxHeight - minHeight);

        ImGui::Spacing();
        ModernUI::GradientSeparator();
        ImGui::Spacing();
        ImGui::Spacing();

        // ===== Action Buttons =====
        float buttonWidth = 120.0f;
        float totalWidth = buttonWidth * 2 + ImGui::GetStyle().ItemSpacing.x;
        float offsetX = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

        if (ModernUI::GlowButton("Apply", ImVec2(buttonWidth, 0))) {
            // Check if dimensions changed (would require terrain regeneration)
            bool dimensionsChanged = (mapWidth != m_mapWidth) || (mapHeight != m_mapHeight);
            bool worldTypeChanged = (worldType != m_worldType) ||
                                   (worldType == WorldType::Spherical && worldRadius != m_worldRadius);

            // Apply changes to member variables
            m_mapName = nameBuf;
            m_worldType = worldType;
            m_worldRadius = worldRadius;
            m_mapWidth = mapWidth;
            m_mapHeight = mapHeight;
            m_minHeight = minHeight;
            m_maxHeight = maxHeight;

            // Regenerate terrain if needed
            if (dimensionsChanged) {
                spdlog::info("Map dimensions changed: {}x{} chunks - regenerating terrain", mapWidth, mapHeight);
                m_terrainTiles.resize(mapWidth * mapHeight, 0);
                m_terrainHeights.resize(mapWidth * mapHeight, 0.0f);

                // Initialize terrain with default values
                std::fill(m_terrainTiles.begin(), m_terrainTiles.end(), 0);  // Default to grass tiles
                std::fill(m_terrainHeights.begin(), m_terrainHeights.end(), 0.0f);  // Flat terrain

                // Mark terrain mesh as dirty to trigger visual regeneration
                m_terrainMeshDirty = true;

                // Reset camera to view new terrain
                m_editorCameraTarget = glm::vec3(mapWidth * m_gridSize * 0.5f, 0.0f, mapHeight * m_gridSize * 0.5f);
                m_editorCameraPos = m_editorCameraTarget + glm::vec3(0.0f, 20.0f, 20.0f);

                spdlog::info("Terrain regenerated with {} tiles", mapWidth * mapHeight);
            }

            if (worldTypeChanged) {
                if (worldType == WorldType::Spherical) {
                    spdlog::info("World type changed to Spherical (radius: {} km)", worldRadius);

                    // Initialize spherical world geometry
                    m_worldCenter = glm::vec3(0.0f);
                    m_showSphericalGrid = true;

                    // Position camera to view the sphere
                    float cameraDistance = worldRadius * 2.5f;
                    m_editorCameraPos = glm::vec3(cameraDistance, cameraDistance * 0.5f, cameraDistance);
                    m_editorCameraTarget = m_worldCenter;

                    spdlog::info("Spherical world initialized - center: (0,0,0), radius: {} km", worldRadius);
                } else {
                    spdlog::info("World type changed to Flat");

                    // Initialize flat world geometry
                    m_showSphericalGrid = false;
                    m_worldCenter = glm::vec3(0.0f);

                    // Position camera for flat terrain view
                    m_editorCameraTarget = glm::vec3(m_mapWidth * m_gridSize * 0.5f, 0.0f, m_mapHeight * m_gridSize * 0.5f);
                    m_editorCameraPos = m_editorCameraTarget + glm::vec3(0.0f, 20.0f, 20.0f);

                    spdlog::info("Flat world initialized - terrain size: {}x{}", m_mapWidth, m_mapHeight);
                }
            }

            m_showMapPropertiesDialog = false;
            initialized = false;

            spdlog::info("Map properties applied successfully");
        }

        ImGui::SameLine();

        if (ModernUI::GlowButton("Cancel", ImVec2(buttonWidth, 0))) {
            // Discard all changes
            m_showMapPropertiesDialog = false;
            initialized = false;
            spdlog::debug("Map properties dialog cancelled");
        }

        ImGui::EndPopup();
    }
}
// Additional menu action functions to be appended to StandaloneEditor.cpp

// ============================================================================
// Recent Files Management
// ============================================================================

void StandaloneEditor::LoadRecentFiles() {
    m_recentFiles.clear();

    // Determine config file path
    std::filesystem::path configPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        configPath = std::filesystem::path(appData) / "Nova3D" / "editor_recent_files.json";
    } else {
        configPath = "editor_recent_files.json";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        configPath = std::filesystem::path(home) / ".config" / "Nova3D" / "editor_recent_files.json";
    } else {
        configPath = "editor_recent_files.json";
    }
#endif

    if (!std::filesystem::exists(configPath)) {
        spdlog::debug("Recent files config not found: {}", configPath.string());
        return;
    }

    try {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            spdlog::warn("Could not open recent files config: {}", configPath.string());
            return;
        }

        nlohmann::json j;
        file >> j;

        if (j.contains("recent_files") && j["recent_files"].is_array()) {
            for (const auto& item : j["recent_files"]) {
                if (item.is_string()) {
                    std::string path = item.get<std::string>();
                    // Only add if file still exists
                    if (std::filesystem::exists(path)) {
                        m_recentFiles.push_back(path);
                    }
                }
            }
        }

        spdlog::info("Loaded {} recent files from {}", m_recentFiles.size(), configPath.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load recent files: {}", e.what());
    }
}

void StandaloneEditor::SaveRecentFiles() {
    // Determine config file path
    std::filesystem::path configPath;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        configPath = std::filesystem::path(appData) / "Nova3D";
    } else {
        configPath = ".";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        configPath = std::filesystem::path(home) / ".config" / "Nova3D";
    } else {
        configPath = ".";
    }
#endif

    // Create directory if it doesn't exist
    try {
        if (!std::filesystem::exists(configPath)) {
            std::filesystem::create_directories(configPath);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to create config directory: {}", e.what());
        return;
    }

    configPath /= "editor_recent_files.json";

    try {
        nlohmann::json j;
        j["recent_files"] = m_recentFiles;

        std::ofstream file(configPath);
        if (!file.is_open()) {
            spdlog::error("Could not open recent files config for writing: {}", configPath.string());
            return;
        }

        file << j.dump(2);  // Pretty print with 2-space indent
        spdlog::debug("Saved {} recent files to {}", m_recentFiles.size(), configPath.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to save recent files: {}", e.what());
    }
}

void StandaloneEditor::AddToRecentFiles(const std::string& path) {
    // Remove if already exists
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }

    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), path);

    // Limit to 10 recent files
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }

    SaveRecentFiles();
    spdlog::info("Added to recent files: {}", path);
}

void StandaloneEditor::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
    spdlog::info("Recent files list cleared");
}

// ============================================================================
// Import/Export Functions
// ============================================================================

bool StandaloneEditor::ImportHeightmap(const std::string& path) {
    spdlog::info("Importing heightmap from: {}", path);

    // Load the image using stb_image (grayscale mode)
    int width, height, channels;
    unsigned char* imageData = stbi_load(path.c_str(), &width, &height, &channels, 1);

    if (!imageData) {
        spdlog::error("Failed to load heightmap image: {}", path);
        return false;
    }

    spdlog::info("Loaded heightmap: {}x{} pixels, {} channels", width, height, channels);

    // Resize terrain to match heightmap dimensions
    m_mapWidth = width;
    m_mapHeight = height;
    int numTiles = width * height;

    m_terrainTiles.resize(numTiles, 0);  // Default to grass tiles
    m_terrainHeights.resize(numTiles, 0.0f);

    // Height mapping parameters (adjust these as needed)
    const float minHeight = 0.0f;
    const float maxHeight = 100.0f;

    // Convert pixel values to height values
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int pixelIndex = y * width + x;
            int terrainIndex = y * width + x;

            // Get grayscale value (0-255)
            unsigned char pixelValue = imageData[pixelIndex];

            // Map to height range: 0-255 -> minHeight-maxHeight
            float normalizedValue = pixelValue / 255.0f;
            float height = normalizedValue * (maxHeight - minHeight) + minHeight;

            m_terrainHeights[terrainIndex] = height;
        }
    }

    // Free the image data
    stbi_image_free(imageData);

    // Mark terrain mesh as dirty to trigger regeneration
    m_terrainMeshDirty = true;

    spdlog::info("Successfully imported heightmap: {} vertices", numTiles);
    return true;
}

bool StandaloneEditor::ExportHeightmap(const std::string& path) {
    spdlog::info("Exporting heightmap to: {}", path);

    // Validate terrain data exists
    if (m_terrainHeights.empty() || m_mapWidth <= 0 || m_mapHeight <= 0) {
        spdlog::error("No terrain data to export");
        return false;
    }

    int numPixels = m_mapWidth * m_mapHeight;
    if (static_cast<int>(m_terrainHeights.size()) != numPixels) {
        spdlog::error("Terrain height data size mismatch: expected {}, got {}",
                     numPixels, m_terrainHeights.size());
        return false;
    }

    // Find the actual min/max heights in the terrain
    float actualMin = m_terrainHeights[0];
    float actualMax = m_terrainHeights[0];
    for (float height : m_terrainHeights) {
        if (height < actualMin) actualMin = height;
        if (height > actualMax) actualMax = height;
    }

    spdlog::info("Terrain height range: {} to {}", actualMin, actualMax);

    // Allocate pixel buffer for grayscale image
    std::vector<unsigned char> pixelData(numPixels);

    // Normalize heights to 0-255 range
    float heightRange = actualMax - actualMin;
    if (heightRange < 0.001f) {
        // Flat terrain - use middle gray
        spdlog::warn("Terrain is flat, exporting as middle gray (128)");
        std::fill(pixelData.begin(), pixelData.end(), 128);
    } else {
        for (int i = 0; i < numPixels; ++i) {
            float normalizedHeight = (m_terrainHeights[i] - actualMin) / heightRange;
            pixelData[i] = static_cast<unsigned char>(normalizedHeight * 255.0f);
        }
    }

    // Write PNG file using stb_image_write
    // Parameters: filename, width, height, channels (1=grayscale), data, stride_in_bytes
    int result = stbi_write_png(path.c_str(), m_mapWidth, m_mapHeight, 1,
                                pixelData.data(), m_mapWidth);

    if (!result) {
        spdlog::error("Failed to write heightmap PNG: {}", path);
        return false;
    }

    spdlog::info("Successfully exported heightmap: {}x{} pixels to {}",
                m_mapWidth, m_mapHeight, path);
    return true;
}

// ============================================================================
// Selection and Clipboard Operations
// ============================================================================

void StandaloneEditor::SelectAllObjects() {
    spdlog::info("Select All Objects");
    m_selectedObjectIndices.clear();

    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        m_selectedObjectIndices.push_back(static_cast<int>(i));
    }

    m_isMultiSelectMode = true;

    if (!m_sceneObjects.empty()) {
        m_selectedObjectIndex = 0;
        auto& obj = m_sceneObjects[0];
        m_selectedObjectPosition = obj.position;
        m_selectedObjectRotation = obj.rotation;
        m_selectedObjectScale = obj.scale;
    }

    spdlog::info("Selected {} objects", m_selectedObjectIndices.size());
}

void StandaloneEditor::CopySelectedObjects() {
    // Clear previous clipboard contents
    m_clipboard.clear();

    // Handle multi-selection
    if (!m_selectedObjectIndices.empty()) {
        for (int index : m_selectedObjectIndices) {
            if (index >= 0 && index < static_cast<int>(m_sceneObjects.size())) {
                m_clipboard.push_back(m_sceneObjects[index]);
            }
        }
        spdlog::info("Copied {} objects to clipboard", m_clipboard.size());
    }
    // Handle single selection
    else if (m_selectedObjectIndex >= 0 && m_selectedObjectIndex < static_cast<int>(m_sceneObjects.size())) {
        m_clipboard.push_back(m_sceneObjects[m_selectedObjectIndex]);
        spdlog::info("Copied object '{}' to clipboard", m_sceneObjects[m_selectedObjectIndex].name);
    } else {
        spdlog::warn("No object selected for copy");
    }
}

void StandaloneEditor::PasteObjects() {
    if (m_clipboard.empty()) {
        spdlog::warn("Clipboard is empty - nothing to paste");
        return;
    }

    // Clear current selection
    ClearSelection();

    // Offset for pasted objects to make them visible
    glm::vec3 pasteOffset(1.0f, 0.0f, 1.0f);

    // Paste each object from clipboard
    for (const auto& clipboardObj : m_clipboard) {
        SceneObject newObj = clipboardObj;

        // Generate unique name
        newObj.name = clipboardObj.name + "_copy_" + std::to_string(m_sceneObjects.size());

        // Offset position so pasted objects don't overlap originals
        newObj.position += pasteOffset;

        // Add to scene
        m_sceneObjects.push_back(newObj);

        // Add to selection
        m_selectedObjectIndices.push_back(static_cast<int>(m_sceneObjects.size()) - 1);
    }

    // Set primary selection to first pasted object
    if (!m_selectedObjectIndices.empty()) {
        m_selectedObjectIndex = m_selectedObjectIndices[0];
        auto& obj = m_sceneObjects[m_selectedObjectIndex];
        m_selectedObjectPosition = obj.position;
        m_selectedObjectRotation = obj.rotation;
        m_selectedObjectScale = obj.scale;
    }

    m_isMultiSelectMode = (m_selectedObjectIndices.size() > 1);

    spdlog::info("Pasted {} objects from clipboard", m_clipboard.size());
}

#ifdef _WIN32
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {0};
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    
    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    char filename[MAX_PATH] = {0};
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
    
    return "";
}
#else
// Fallback for non-Windows platforms
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}
#endif
