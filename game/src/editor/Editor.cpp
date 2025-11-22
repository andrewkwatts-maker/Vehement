#include "Editor.hpp"
#include "ConfigEditor.hpp"
#include "WorldView.hpp"
#include "TileInspector.hpp"
#include "PCGPanel.hpp"
#include "LocationCrafter.hpp"
#include "ScriptEditor.hpp"
#include "AssetBrowser.hpp"
#include "Hierarchy.hpp"
#include "Inspector.hpp"
#include "Console.hpp"
#include "Toolbar.hpp"

#include "../../engine/core/Engine.hpp"
#include "../../engine/core/Window.hpp"
#include "../../engine/core/Logger.hpp"
#include "../../engine/input/InputManager.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <fstream>
#include <nlohmann/json.hpp>

namespace Vehement {

Editor::Editor() = default;

Editor::~Editor() {
    if (m_initialized) {
        Shutdown();
    }
}

bool Editor::Initialize(Nova::Engine& engine, const EditorConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_engine = &engine;
    m_config = config;
    m_mode = Mode::Integrated;

    // Get GLFW window from engine
    m_window = static_cast<GLFWwindow*>(engine.GetWindow().GetNativeWindow());
    if (!m_window) {
        Nova::Logger::Error("[Editor] Failed to get GLFW window from engine");
        return false;
    }

    // Initialize ImGui
    if (!InitImGui(m_window)) {
        Nova::Logger::Error("[Editor] Failed to initialize ImGui");
        return false;
    }

    // Apply theme
    ApplyTheme(m_config.theme);

    // Create panels
    m_configEditor = std::make_unique<ConfigEditor>(this);
    m_worldView = std::make_unique<WorldView>(this);
    m_tileInspector = std::make_unique<TileInspector>(this);
    m_pcgPanel = std::make_unique<PCGPanel>(this);
    m_locationCrafter = std::make_unique<LocationCrafter>(this);
    m_scriptEditor = std::make_unique<ScriptEditor>(this);
    m_assetBrowser = std::make_unique<AssetBrowser>(this);
    m_hierarchy = std::make_unique<Hierarchy>(this);
    m_inspector = std::make_unique<Inspector>(this);
    m_console = std::make_unique<Console>(this);
    m_toolbar = std::make_unique<Toolbar>(this);

    // Register default shortcuts
    RegisterDefaultShortcuts();

    // Try to load saved layout
    LoadLayout(m_config.layoutPath);

    m_initialized = true;
    Nova::Logger::Info("[Editor] Initialized successfully");
    return true;
}

bool Editor::InitializeStandalone(const EditorConfig& config) {
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_mode = Mode::Standalone;

    // Initialize GLFW
    if (!glfwInit()) {
        Nova::Logger::Error("[Editor] Failed to initialize GLFW");
        return false;
    }

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    m_window = glfwCreateWindow(1920, 1080, "Nova3D Editor", nullptr, nullptr);
    if (!m_window) {
        Nova::Logger::Error("[Editor] Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    m_ownsWindow = true;
    glfwMakeContextCurrent(m_window);

    if (m_config.enableVsync) {
        glfwSwapInterval(1);
    }

    // Initialize ImGui
    if (!InitImGui(m_window)) {
        Nova::Logger::Error("[Editor] Failed to initialize ImGui");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Apply theme
    ApplyTheme(m_config.theme);

    // Create panels
    m_configEditor = std::make_unique<ConfigEditor>(this);
    m_worldView = std::make_unique<WorldView>(this);
    m_tileInspector = std::make_unique<TileInspector>(this);
    m_pcgPanel = std::make_unique<PCGPanel>(this);
    m_locationCrafter = std::make_unique<LocationCrafter>(this);
    m_scriptEditor = std::make_unique<ScriptEditor>(this);
    m_assetBrowser = std::make_unique<AssetBrowser>(this);
    m_hierarchy = std::make_unique<Hierarchy>(this);
    m_inspector = std::make_unique<Inspector>(this);
    m_console = std::make_unique<Console>(this);
    m_toolbar = std::make_unique<Toolbar>(this);

    // Register default shortcuts
    RegisterDefaultShortcuts();

    // Try to load saved layout
    LoadLayout(m_config.layoutPath);

    m_initialized = true;
    Nova::Logger::Info("[Editor] Initialized in standalone mode");
    return true;
}

void Editor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Save layout before shutdown
    SaveLayout(m_config.layoutPath);

    // Destroy panels
    m_toolbar.reset();
    m_console.reset();
    m_inspector.reset();
    m_hierarchy.reset();
    m_assetBrowser.reset();
    m_scriptEditor.reset();
    m_locationCrafter.reset();
    m_pcgPanel.reset();
    m_tileInspector.reset();
    m_worldView.reset();
    m_configEditor.reset();

    // Shutdown ImGui
    ShutdownImGui();

    // Destroy window if we own it
    if (m_ownsWindow && m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    m_initialized = false;
    Nova::Logger::Info("[Editor] Shutdown complete");
}

bool Editor::InitImGui(GLFWwindow* window) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    return true;
}

void Editor::ShutdownImGui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(m_imguiContext);
    m_imguiContext = nullptr;
}

int Editor::Run() {
    if (!m_initialized || m_mode != Mode::Standalone) {
        return -1;
    }

    m_running = true;
    double lastTime = glfwGetTime();

    while (m_running && !glfwWindowShouldClose(m_window)) {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        glfwPollEvents();
        ProcessInput();
        Update(deltaTime);
        Render();

        glfwSwapBuffers(m_window);
    }

    return 0;
}

void Editor::Update(float deltaTime) {
    if (!m_initialized) return;

    // Check autosave
    CheckAutosave(deltaTime);

    // Update panels
    if (m_worldView) m_worldView->Update(deltaTime);
    if (m_configEditor) m_configEditor->Update(deltaTime);
}

void Editor::Render() {
    if (!m_initialized) return;

    // Start new ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render dockspace and UI
    BeginDockspace();
    RenderMenuBar();
    if (m_toolbar) m_toolbar->Render();
    RenderPanels();
    RenderStatusBar();
    EndDockspace();

    // Render dialogs
    if (m_showAboutDialog) ShowAboutDialog();
    if (m_showShortcutsDialog) ShowShortcutsDialog();
    if (m_showSettingsDialog) ShowSettingsDialog();
    if (m_showNewProjectDialog) ShowNewProjectDialog();
    if (m_showOpenProjectDialog) ShowOpenProjectDialog();
    if (m_showSaveAsDialog) ShowSaveAsDialog();

    // Demo window for debugging
    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }

    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle viewports
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void Editor::ProcessInput() {
    ProcessShortcuts();
}

void Editor::BeginDockspace() {
    static bool opt_fullscreen = true;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();

    if (opt_fullscreen) {
        ImGui::PopStyleVar(2);
    }

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        m_dockspaceId = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(m_dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags);

        // Setup default layout on first frame
        if (m_firstFrame) {
            m_firstFrame = false;
            ResetLayout();
        }
    }
}

void Editor::EndDockspace() {
    ImGui::End();
}

void Editor::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        RenderFileMenu();
        RenderEditMenu();
        RenderViewMenu();
        RenderToolsMenu();
        RenderHelpMenu();
        ImGui::EndMenuBar();
    }
}

void Editor::RenderFileMenu() {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project", "Ctrl+N")) {
            m_showNewProjectDialog = true;
        }
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
            m_showOpenProjectDialog = true;
        }
        if (ImGui::MenuItem("Save", "Ctrl+S", false, HasOpenProject())) {
            SaveProject();
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, HasOpenProject())) {
            m_showSaveAsDialog = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close Project", nullptr, false, HasOpenProject())) {
            CloseProject();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            m_running = false;
            if (m_window) {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
        }
        ImGui::EndMenu();
    }
}

void Editor::RenderEditMenu() {
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
            Undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
            Redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cut", "Ctrl+X")) {
            // TODO: Implement cut
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            // TODO: Implement copy
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V")) {
            // TODO: Implement paste
        }
        if (ImGui::MenuItem("Delete", "Delete")) {
            // TODO: Implement delete
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A")) {
            // TODO: Implement select all
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Settings...")) {
            m_showSettingsDialog = true;
        }
        ImGui::EndMenu();
    }
}

void Editor::RenderViewMenu() {
    if (ImGui::BeginMenu("View")) {
        if (ImGui::BeginMenu("Panels")) {
            ImGui::MenuItem("Config Editor", nullptr, &m_showConfigEditor);
            ImGui::MenuItem("World View", nullptr, &m_showWorldView);
            ImGui::MenuItem("Tile Inspector", nullptr, &m_showTileInspector);
            ImGui::MenuItem("PCG Panel", nullptr, &m_showPCGPanel);
            ImGui::MenuItem("Location Crafter", nullptr, &m_showLocationCrafter);
            ImGui::MenuItem("Script Editor", nullptr, &m_showScriptEditor);
            ImGui::MenuItem("Asset Browser", nullptr, &m_showAssetBrowser);
            ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
            ImGui::MenuItem("Console", nullptr, &m_showConsole);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            ResetLayout();
        }
        if (ImGui::MenuItem("Save Layout")) {
            SaveLayout(m_config.layoutPath);
        }
        if (ImGui::MenuItem("Load Layout")) {
            LoadLayout(m_config.layoutPath);
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &m_showDemoWindow);
        ImGui::EndMenu();
    }
}

void Editor::RenderToolsMenu() {
    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Play", "F5")) {
            if (OnPlay) OnPlay();
        }
        if (ImGui::MenuItem("Pause", "F6")) {
            if (OnPause) OnPause();
        }
        if (ImGui::MenuItem("Stop", "F7")) {
            if (OnStop) OnStop();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Build All Configs")) {
            // TODO: Implement
        }
        if (ImGui::MenuItem("Validate All Configs")) {
            // TODO: Implement
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Generate World")) {
            m_showPCGPanel = true;
        }
        if (ImGui::MenuItem("Export World")) {
            // TODO: Implement
        }
        ImGui::EndMenu();
    }
}

void Editor::RenderHelpMenu() {
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Keyboard Shortcuts", "F1")) {
            m_showShortcutsDialog = true;
        }
        if (ImGui::MenuItem("Documentation")) {
            // TODO: Open docs
        }
        ImGui::Separator();
        if (ImGui::MenuItem("About Nova3D Editor")) {
            m_showAboutDialog = true;
        }
        ImGui::EndMenu();
    }
}

void Editor::RenderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - 25));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 25));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Project status
        if (HasOpenProject()) {
            ImGui::Text("Project: %s", m_projectPath.c_str());
            if (m_hasUnsavedChanges) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "*");
            }
        } else {
            ImGui::Text("No project open");
        }

        // Right-aligned info
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void Editor::RenderPanels() {
    if (m_showConfigEditor && m_configEditor) {
        m_configEditor->Render();
    }
    if (m_showWorldView && m_worldView) {
        m_worldView->Render();
    }
    if (m_showTileInspector && m_tileInspector) {
        m_tileInspector->Render();
    }
    if (m_showPCGPanel && m_pcgPanel) {
        m_pcgPanel->Render();
    }
    if (m_showLocationCrafter && m_locationCrafter) {
        m_locationCrafter->Render();
    }
    if (m_showScriptEditor && m_scriptEditor) {
        m_scriptEditor->Render();
    }
    if (m_showAssetBrowser && m_assetBrowser) {
        m_assetBrowser->Render();
    }
    if (m_showHierarchy && m_hierarchy) {
        m_hierarchy->Render();
    }
    if (m_showInspector && m_inspector) {
        m_inspector->Render();
    }
    if (m_showConsole && m_console) {
        m_console->Render();
    }
}

void Editor::ShowNewProjectDialog() {
    ImGui::OpenPopup("New Project");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Project", &m_showNewProjectDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char projectPath[512] = "";
        static char projectName[256] = "NewProject";

        ImGui::InputText("Project Name", projectName, sizeof(projectName));
        ImGui::InputText("Location", projectPath, sizeof(projectPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // TODO: File dialog
        }

        ImGui::Separator();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            std::string fullPath = std::string(projectPath) + "/" + projectName;
            if (NewProject(fullPath)) {
                m_showNewProjectDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewProjectDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::ShowOpenProjectDialog() {
    ImGui::OpenPopup("Open Project");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Open Project", &m_showOpenProjectDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char projectPath[512] = "";

        ImGui::InputText("Project File", projectPath, sizeof(projectPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // TODO: File dialog
        }

        ImGui::Separator();
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (OpenProject(projectPath)) {
                m_showOpenProjectDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showOpenProjectDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::ShowSaveAsDialog() {
    ImGui::OpenPopup("Save Project As");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Save Project As", &m_showSaveAsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char savePath[512] = "";

        ImGui::InputText("Save Location", savePath, sizeof(savePath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            // TODO: File dialog
        }

        ImGui::Separator();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (SaveProjectAs(savePath)) {
                m_showSaveAsDialog = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSaveAsDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::ShowAboutDialog() {
    ImGui::OpenPopup("About Nova3D Editor");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About Nova3D Editor", &m_showAboutDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Nova3D / Vehement2 Editor");
        ImGui::Separator();
        ImGui::Text("Version: 1.0.0");
        ImGui::Text("Engine: Nova3D");
        ImGui::Text("Built with Dear ImGui %s", IMGUI_VERSION);
        ImGui::Separator();
        ImGui::Text("A comprehensive editor for creating and managing");
        ImGui::Text("game content, world generation, and configurations.");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_showAboutDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::ShowShortcutsDialog() {
    ImGui::OpenPopup("Keyboard Shortcuts");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("Keyboard Shortcuts", &m_showShortcutsDialog)) {
        ImGui::Text("Editor Shortcuts:");
        ImGui::Separator();

        if (ImGui::BeginTable("shortcuts", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("Shortcut");
            ImGui::TableHeadersRow();

            for (const auto& shortcut : m_shortcuts) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", shortcut.description.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", shortcut.action.c_str());
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showShortcutsDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::ShowSettingsDialog() {
    ImGui::OpenPopup("Settings");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("Settings", &m_showSettingsDialog)) {
        if (ImGui::BeginTabBar("SettingsTabs")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::Checkbox("Enable VSync", &m_config.enableVsync);
                ImGui::SliderInt("Target FPS", &m_config.targetFPS, 30, 144);
                ImGui::SliderFloat("Autosave Interval (s)", &m_config.autosaveInterval, 60.0f, 600.0f);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Appearance")) {
                ImGui::Text("Theme Colors:");
                ImGui::ColorEdit4("Window Background", &m_config.theme.windowBg.x);
                ImGui::ColorEdit4("Frame Background", &m_config.theme.frameBg.x);
                ImGui::ColorEdit4("Button", &m_config.theme.button.x);
                ImGui::ColorEdit4("Accent", &m_config.theme.accent.x);
                ImGui::ColorEdit4("Text", &m_config.theme.text.x);

                if (ImGui::Button("Apply Theme")) {
                    ApplyTheme(m_config.theme);
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Shortcuts")) {
                ImGui::Text("Customize keyboard shortcuts:");
                // TODO: Shortcut customization
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_showSettingsDialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showSettingsDialog = false;
        }
        ImGui::EndPopup();
    }
}

void Editor::RegisterDefaultShortcuts() {
    m_shortcuts.clear();

    // File shortcuts
    RegisterShortcut({GLFW_KEY_N, GLFW_MOD_CONTROL, "Ctrl+N", "New Project", [this]() { m_showNewProjectDialog = true; }});
    RegisterShortcut({GLFW_KEY_O, GLFW_MOD_CONTROL, "Ctrl+O", "Open Project", [this]() { m_showOpenProjectDialog = true; }});
    RegisterShortcut({GLFW_KEY_S, GLFW_MOD_CONTROL, "Ctrl+S", "Save Project", [this]() { SaveProject(); }});
    RegisterShortcut({GLFW_KEY_S, GLFW_MOD_CONTROL | GLFW_MOD_SHIFT, "Ctrl+Shift+S", "Save As", [this]() { m_showSaveAsDialog = true; }});

    // Edit shortcuts
    RegisterShortcut({GLFW_KEY_Z, GLFW_MOD_CONTROL, "Ctrl+Z", "Undo", [this]() { Undo(); }});
    RegisterShortcut({GLFW_KEY_Y, GLFW_MOD_CONTROL, "Ctrl+Y", "Redo", [this]() { Redo(); }});

    // Tools shortcuts
    RegisterShortcut({GLFW_KEY_F5, 0, "F5", "Play", [this]() { if (OnPlay) OnPlay(); }});
    RegisterShortcut({GLFW_KEY_F6, 0, "F6", "Pause", [this]() { if (OnPause) OnPause(); }});
    RegisterShortcut({GLFW_KEY_F7, 0, "F7", "Stop", [this]() { if (OnStop) OnStop(); }});

    // Help
    RegisterShortcut({GLFW_KEY_F1, 0, "F1", "Show Shortcuts", [this]() { m_showShortcutsDialog = true; }});
}

void Editor::RegisterShortcut(const KeyboardShortcut& shortcut) {
    m_shortcuts.push_back(shortcut);
}

void Editor::UnregisterShortcut(const std::string& action) {
    m_shortcuts.erase(
        std::remove_if(m_shortcuts.begin(), m_shortcuts.end(),
            [&action](const KeyboardShortcut& s) { return s.action == action; }),
        m_shortcuts.end());
}

void Editor::ProcessShortcuts() {
    if (!m_window) return;

    for (const auto& shortcut : m_shortcuts) {
        bool keyPressed = glfwGetKey(m_window, shortcut.key) == GLFW_PRESS;
        bool modsMatch = true;

        if (shortcut.modifiers & GLFW_MOD_CONTROL) {
            modsMatch &= glfwGetKey(m_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(m_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        }
        if (shortcut.modifiers & GLFW_MOD_SHIFT) {
            modsMatch &= glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                        glfwGetKey(m_window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        }
        if (shortcut.modifiers & GLFW_MOD_ALT) {
            modsMatch &= glfwGetKey(m_window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                        glfwGetKey(m_window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        }

        if (keyPressed && modsMatch && shortcut.callback) {
            shortcut.callback();
        }
    }
}

void Editor::CheckAutosave(float deltaTime) {
    if (!HasOpenProject() || !m_hasUnsavedChanges) return;

    m_autosaveTimer += deltaTime;
    if (m_autosaveTimer >= m_config.autosaveInterval) {
        m_autosaveTimer = 0.0f;
        SaveProject();
        if (m_console) {
            m_console->Log("Project autosaved", Console::LogLevel::Info);
        }
    }
}

void Editor::ExecuteCommand(std::unique_ptr<EditorCommand> command) {
    if (!command) return;

    command->Execute();
    m_undoStack.push(std::move(command));

    // Clear redo stack when new command is executed
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }

    // Limit undo history
    while (m_undoStack.size() > MAX_UNDO_HISTORY) {
        std::stack<std::unique_ptr<EditorCommand>> temp;
        while (m_undoStack.size() > 1) {
            temp.push(std::move(m_undoStack.top()));
            m_undoStack.pop();
        }
        m_undoStack.pop();  // Remove oldest
        while (!temp.empty()) {
            m_undoStack.push(std::move(temp.top()));
            temp.pop();
        }
    }

    MarkDirty();
}

void Editor::Undo() {
    if (m_undoStack.empty()) return;

    auto command = std::move(m_undoStack.top());
    m_undoStack.pop();

    command->Undo();
    m_redoStack.push(std::move(command));
    MarkDirty();
}

void Editor::Redo() {
    if (m_redoStack.empty()) return;

    auto command = std::move(m_redoStack.top());
    m_redoStack.pop();

    command->Execute();
    m_undoStack.push(std::move(command));
    MarkDirty();
}

void Editor::ClearHistory() {
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();
}

bool Editor::NewProject(const std::string& path) {
    CloseProject();
    m_projectPath = path;
    m_hasUnsavedChanges = false;

    if (OnProjectNew) OnProjectNew();
    if (m_console) {
        m_console->Log("Created new project: " + path, Console::LogLevel::Info);
    }
    return true;
}

bool Editor::OpenProject(const std::string& path) {
    CloseProject();

    // Load project file
    std::ifstream file(path);
    if (!file.is_open()) {
        if (m_console) {
            m_console->Log("Failed to open project: " + path, Console::LogLevel::Error);
        }
        return false;
    }

    m_projectPath = path;
    m_hasUnsavedChanges = false;

    if (OnProjectOpen) OnProjectOpen(path);
    if (m_console) {
        m_console->Log("Opened project: " + path, Console::LogLevel::Info);
    }
    return true;
}

bool Editor::SaveProject() {
    if (!HasOpenProject()) return false;

    // TODO: Implement actual save logic
    m_hasUnsavedChanges = false;

    if (OnProjectSave) OnProjectSave();
    if (m_console) {
        m_console->Log("Project saved: " + m_projectPath, Console::LogLevel::Info);
    }
    return true;
}

bool Editor::SaveProjectAs(const std::string& path) {
    m_projectPath = path;
    return SaveProject();
}

void Editor::CloseProject() {
    if (!HasOpenProject()) return;

    // TODO: Check for unsaved changes

    ClearHistory();
    m_projectPath.clear();
    m_hasUnsavedChanges = false;

    if (OnProjectClose) OnProjectClose();
}

bool Editor::SaveLayout(const std::string& path) {
    // ImGui handles layout saving through ini file
    ImGui::SaveIniSettingsToDisk(path.c_str());
    return true;
}

bool Editor::LoadLayout(const std::string& path) {
    ImGui::LoadIniSettingsFromDisk(path.c_str());
    return true;
}

void Editor::ResetLayout() {
    ImGuiID dockspace_id = m_dockspaceId;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // Create dock layout
    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
    ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_down = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
    ImGuiID dock_left_down = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.5f, nullptr, &dock_left);

    // Dock windows
    ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
    ImGui::DockBuilderDockWindow("Asset Browser", dock_left_down);
    ImGui::DockBuilderDockWindow("World View", dock_main_id);
    ImGui::DockBuilderDockWindow("Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Config Editor", dock_right);
    ImGui::DockBuilderDockWindow("Tile Inspector", dock_right);
    ImGui::DockBuilderDockWindow("Console", dock_down);
    ImGui::DockBuilderDockWindow("Script Editor", dock_down);
    ImGui::DockBuilderDockWindow("PCG Panel", dock_down);
    ImGui::DockBuilderDockWindow("Location Crafter", dock_down);

    ImGui::DockBuilderFinish(dockspace_id);
}

void Editor::ApplyTheme(const EditorTheme& theme) {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = theme.windowRounding;
    style.FrameRounding = theme.frameRounding;
    style.GrabRounding = theme.grabRounding;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(theme.windowBg.x, theme.windowBg.y, theme.windowBg.z, theme.windowBg.w);
    colors[ImGuiCol_TitleBg] = ImVec4(theme.titleBg.x, theme.titleBg.y, theme.titleBg.z, theme.titleBg.w);
    colors[ImGuiCol_TitleBgActive] = ImVec4(theme.titleBgActive.x, theme.titleBgActive.y, theme.titleBgActive.z, theme.titleBgActive.w);
    colors[ImGuiCol_FrameBg] = ImVec4(theme.frameBg.x, theme.frameBg.y, theme.frameBg.z, theme.frameBg.w);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(theme.frameBgHovered.x, theme.frameBgHovered.y, theme.frameBgHovered.z, theme.frameBgHovered.w);
    colors[ImGuiCol_FrameBgActive] = ImVec4(theme.frameBgActive.x, theme.frameBgActive.y, theme.frameBgActive.z, theme.frameBgActive.w);
    colors[ImGuiCol_Button] = ImVec4(theme.button.x, theme.button.y, theme.button.z, theme.button.w);
    colors[ImGuiCol_ButtonHovered] = ImVec4(theme.buttonHovered.x, theme.buttonHovered.y, theme.buttonHovered.z, theme.buttonHovered.w);
    colors[ImGuiCol_ButtonActive] = ImVec4(theme.buttonActive.x, theme.buttonActive.y, theme.buttonActive.z, theme.buttonActive.w);
    colors[ImGuiCol_Header] = ImVec4(theme.header.x, theme.header.y, theme.header.z, theme.header.w);
    colors[ImGuiCol_HeaderHovered] = ImVec4(theme.headerHovered.x, theme.headerHovered.y, theme.headerHovered.z, theme.headerHovered.w);
    colors[ImGuiCol_HeaderActive] = ImVec4(theme.headerActive.x, theme.headerActive.y, theme.headerActive.z, theme.headerActive.w);
    colors[ImGuiCol_Tab] = ImVec4(theme.tab.x, theme.tab.y, theme.tab.z, theme.tab.w);
    colors[ImGuiCol_TabHovered] = ImVec4(theme.tabHovered.x, theme.tabHovered.y, theme.tabHovered.z, theme.tabHovered.w);
    colors[ImGuiCol_TabActive] = ImVec4(theme.tabActive.x, theme.tabActive.y, theme.tabActive.z, theme.tabActive.w);
    colors[ImGuiCol_Text] = ImVec4(theme.text.x, theme.text.y, theme.text.z, theme.text.w);
    colors[ImGuiCol_TextDisabled] = ImVec4(theme.textDisabled.x, theme.textDisabled.y, theme.textDisabled.z, theme.textDisabled.w);

    m_config.theme = theme;
}

} // namespace Vehement
