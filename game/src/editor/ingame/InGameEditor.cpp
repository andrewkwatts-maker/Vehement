#include "InGameEditor.hpp"
#include "MapEditor.hpp"
#include "TriggerEditor.hpp"
#include "ObjectEditor.hpp"
#include "CampaignEditor.hpp"
#include "ScenarioSettings.hpp"
#include "AIEditor.hpp"
#include "panels/TerrainPanel.hpp"
#include "panels/ObjectPalette.hpp"
#include "panels/TriggerPanel.hpp"
#include "panels/PropertiesPanel.hpp"
#include "MapFile.hpp"
#include "CampaignFile.hpp"
#include "WorkshopIntegration.hpp"
#include "../../core/Game.hpp"

#include <imgui.h>
#include <chrono>
#include <functional>

namespace Vehement {

// Hash function for developer key verification
static uint64_t HashString(const std::string& str) {
    uint64_t hash = 0x5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<uint64_t>(c);
    }
    return hash;
}

InGameEditor::InGameEditor() = default;
InGameEditor::~InGameEditor() = default;

bool InGameEditor::Initialize(Nova::Engine& engine, Game& game) {
    if (m_initialized) {
        return true;
    }

    m_engine = &engine;
    m_game = &game;

    // Initialize sub-editors
    m_mapEditor = std::make_unique<MapEditor>();
    m_triggerEditor = std::make_unique<TriggerEditor>();
    m_objectEditor = std::make_unique<ObjectEditor>();
    m_campaignEditor = std::make_unique<CampaignEditor>();
    m_scenarioSettings = std::make_unique<ScenarioSettings>();
    m_aiEditor = std::make_unique<AIEditor>();

    if (!m_mapEditor->Initialize(*this) ||
        !m_triggerEditor->Initialize(*this) ||
        !m_objectEditor->Initialize(*this) ||
        !m_campaignEditor->Initialize(*this) ||
        !m_scenarioSettings->Initialize(*this) ||
        !m_aiEditor->Initialize(*this)) {
        return false;
    }

    // Initialize UI panels
    m_terrainPanel = std::make_unique<TerrainPanel>();
    m_objectPalette = std::make_unique<ObjectPalette>();
    m_triggerPanel = std::make_unique<TriggerPanel>();
    m_propertiesPanel = std::make_unique<PropertiesPanel>();

    m_terrainPanel->Initialize(*m_mapEditor);
    m_objectPalette->Initialize(*m_mapEditor);
    m_triggerPanel->Initialize(*m_triggerEditor);
    m_propertiesPanel->Initialize(*this);

    // Initialize permission system
    InitializePermissions();

    m_initialized = true;
    return true;
}

void InGameEditor::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Shutdown panels
    if (m_terrainPanel) m_terrainPanel->Shutdown();
    if (m_objectPalette) m_objectPalette->Shutdown();
    if (m_triggerPanel) m_triggerPanel->Shutdown();
    if (m_propertiesPanel) m_propertiesPanel->Shutdown();

    // Shutdown sub-editors
    if (m_mapEditor) m_mapEditor->Shutdown();
    if (m_triggerEditor) m_triggerEditor->Shutdown();
    if (m_objectEditor) m_objectEditor->Shutdown();
    if (m_campaignEditor) m_campaignEditor->Shutdown();
    if (m_scenarioSettings) m_scenarioSettings->Shutdown();
    if (m_aiEditor) m_aiEditor->Shutdown();

    m_initialized = false;
}

void InGameEditor::ToggleEditor() {
    if (IsInEditorMode()) {
        ExitEditorMode();
    } else {
        EnterEditorMode();
    }
}

void InGameEditor::EnterEditorMode() {
    if (m_state != EditorState::Disabled) {
        return;
    }

    m_previousState = m_state;
    m_state = EditorState::MapEditing;

    // Pause game simulation
    if (m_game) {
        m_game->SetPaused(true);
    }

    if (OnEditorEnter) {
        OnEditorEnter();
    }
}

void InGameEditor::ExitEditorMode() {
    if (m_state == EditorState::Disabled) {
        return;
    }

    // Prompt to save if unsaved changes
    if (m_hasUnsavedChanges) {
        // TODO: Show save prompt dialog
    }

    m_state = EditorState::Disabled;

    // Resume game simulation
    if (m_game) {
        m_game->SetPaused(false);
    }

    if (OnEditorExit) {
        OnEditorExit();
    }
}

void InGameEditor::SetState(EditorState state) {
    if (m_state == state) {
        return;
    }

    m_previousState = m_state;
    m_state = state;
}

void InGameEditor::Update(float deltaTime) {
    if (!m_initialized || m_state == EditorState::Disabled) {
        return;
    }

    // Update active sub-editor
    switch (m_state) {
        case EditorState::MapEditing:
            if (m_mapEditor) m_mapEditor->Update(deltaTime);
            break;
        case EditorState::TriggerEditing:
            if (m_triggerEditor) m_triggerEditor->Update(deltaTime);
            break;
        case EditorState::ObjectEditing:
            if (m_objectEditor) m_objectEditor->Update(deltaTime);
            break;
        case EditorState::CampaignEditing:
            if (m_campaignEditor) m_campaignEditor->Update(deltaTime);
            break;
        case EditorState::ScenarioConfig:
            if (m_scenarioSettings) m_scenarioSettings->Update(deltaTime);
            break;
        case EditorState::AIEditing:
            if (m_aiEditor) m_aiEditor->Update(deltaTime);
            break;
        case EditorState::Testing:
            // Game is running, editor is not updating
            break;
        default:
            break;
    }

    // Update panels
    if (m_showTerrainPanel && m_terrainPanel) m_terrainPanel->Update(deltaTime);
    if (m_showObjectPalette && m_objectPalette) m_objectPalette->Update(deltaTime);
    if (m_showTriggerPanel && m_triggerPanel) m_triggerPanel->Update(deltaTime);
    if (m_showPropertiesPanel && m_propertiesPanel) m_propertiesPanel->Update(deltaTime);
}

void InGameEditor::Render() {
    if (!m_initialized || m_state == EditorState::Disabled) {
        return;
    }

    // Render main editor UI
    RenderMenuBar();
    RenderToolbar();
    RenderPanels();
    RenderStatusBar();

    // Render dialogs
    if (m_showNewMapDialog) ShowNewMapDialog();
    if (m_showNewCampaignDialog) ShowNewCampaignDialog();
    if (m_showPublishDialog) ShowPublishDialog();
    if (m_showSettingsDialog) ShowSettingsDialog();

    // Render active sub-editor
    switch (m_state) {
        case EditorState::MapEditing:
            if (m_mapEditor) m_mapEditor->Render();
            break;
        case EditorState::TriggerEditing:
            if (m_triggerEditor) m_triggerEditor->Render();
            break;
        case EditorState::ObjectEditing:
            if (m_objectEditor) m_objectEditor->Render();
            break;
        case EditorState::CampaignEditing:
            if (m_campaignEditor) m_campaignEditor->Render();
            break;
        case EditorState::ScenarioConfig:
            if (m_scenarioSettings) m_scenarioSettings->Render();
            break;
        case EditorState::AIEditing:
            if (m_aiEditor) m_aiEditor->Render();
            break;
        default:
            break;
    }
}

void InGameEditor::ProcessInput() {
    if (!m_initialized || m_state == EditorState::Disabled) {
        return;
    }

    // Global shortcuts
    ImGuiIO& io = ImGui::GetIO();

    // Ctrl+S - Save
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (io.KeyShift) {
            // Save As
            m_showNewMapDialog = true;
        } else {
            SaveMap();
        }
    }

    // Ctrl+Z - Undo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (io.KeyShift) {
            Redo();
        } else {
            Undo();
        }
    }

    // Ctrl+Y - Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        Redo();
    }

    // F5 - Test map
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        if (IsTesting()) {
            StopTest();
        } else {
            StartTest();
        }
    }

    // Escape - Exit editor
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (IsTesting()) {
            StopTest();
        } else {
            ExitEditorMode();
        }
    }

    // Process input for active sub-editor
    switch (m_state) {
        case EditorState::MapEditing:
            if (m_mapEditor) m_mapEditor->ProcessInput();
            break;
        case EditorState::TriggerEditing:
            if (m_triggerEditor) m_triggerEditor->ProcessInput();
            break;
        case EditorState::ObjectEditing:
            if (m_objectEditor) m_objectEditor->ProcessInput();
            break;
        case EditorState::CampaignEditing:
            if (m_campaignEditor) m_campaignEditor->ProcessInput();
            break;
        case EditorState::ScenarioConfig:
            if (m_scenarioSettings) m_scenarioSettings->ProcessInput();
            break;
        case EditorState::AIEditing:
            if (m_aiEditor) m_aiEditor->ProcessInput();
            break;
        default:
            break;
    }
}

void InGameEditor::SetPermission(EditorPermission permission) {
    m_permission = permission;
}

bool InGameEditor::IsActionAllowed(const std::string& action) const {
    auto it = m_actionPermissions.find(action);
    if (it == m_actionPermissions.end()) {
        return true; // Unknown actions are allowed by default
    }
    return CheckPermission(it->second);
}

bool InGameEditor::UnlockDeveloperMode(const std::string& key) {
    if (HashString(key) == DEVELOPER_KEY_HASH) {
        m_permission = EditorPermission::Developer;
        return true;
    }
    return false;
}

bool InGameEditor::NewMap(const std::string& name, int width, int height) {
    if (!m_mapEditor) {
        return false;
    }

    if (!m_mapEditor->CreateNew(width, height)) {
        return false;
    }

    m_contentInfo = CustomContentInfo{};
    m_contentInfo.name = name;
    m_contentInfo.createdTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    m_contentInfo.modifiedTime = m_contentInfo.createdTime;

    m_currentPath.clear();
    m_hasUnsavedChanges = true;

    SetState(EditorState::MapEditing);
    return true;
}

bool InGameEditor::LoadMap(const std::string& path) {
    MapFile mapFile;
    if (!mapFile.Load(path)) {
        return false;
    }

    if (!m_mapEditor || !m_mapEditor->LoadFromFile(mapFile)) {
        return false;
    }

    if (m_triggerEditor) {
        m_triggerEditor->LoadFromFile(mapFile);
    }

    if (m_scenarioSettings) {
        m_scenarioSettings->LoadFromFile(mapFile);
    }

    m_contentInfo = mapFile.GetContentInfo();
    m_currentPath = path;
    m_hasUnsavedChanges = false;

    SetState(EditorState::MapEditing);

    if (OnMapLoaded) {
        OnMapLoaded(path);
    }

    return true;
}

bool InGameEditor::SaveMap() {
    if (m_currentPath.empty()) {
        m_showNewMapDialog = true;
        return false;
    }

    return SaveMapAs(m_currentPath);
}

bool InGameEditor::SaveMapAs(const std::string& path) {
    MapFile mapFile;

    // Update modification time
    m_contentInfo.modifiedTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    mapFile.SetContentInfo(m_contentInfo);

    if (m_mapEditor) {
        m_mapEditor->SaveToFile(mapFile);
    }

    if (m_triggerEditor) {
        m_triggerEditor->SaveToFile(mapFile);
    }

    if (m_scenarioSettings) {
        m_scenarioSettings->SaveToFile(mapFile);
    }

    if (!mapFile.Save(path)) {
        return false;
    }

    m_currentPath = path;
    m_hasUnsavedChanges = false;

    if (OnMapSaved) {
        OnMapSaved(path);
    }

    return true;
}

bool InGameEditor::NewCampaign(const std::string& name) {
    if (!m_campaignEditor) {
        return false;
    }

    if (!m_campaignEditor->CreateNew(name)) {
        return false;
    }

    m_contentInfo = CustomContentInfo{};
    m_contentInfo.name = name;
    m_contentInfo.createdTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    m_contentInfo.modifiedTime = m_contentInfo.createdTime;

    m_currentPath.clear();
    m_hasUnsavedChanges = true;

    SetState(EditorState::CampaignEditing);
    return true;
}

bool InGameEditor::LoadCampaign(const std::string& path) {
    CampaignFile campaignFile;
    if (!campaignFile.Load(path)) {
        return false;
    }

    if (!m_campaignEditor || !m_campaignEditor->LoadFromFile(campaignFile)) {
        return false;
    }

    m_contentInfo = campaignFile.GetContentInfo();
    m_currentPath = path;
    m_hasUnsavedChanges = false;

    SetState(EditorState::CampaignEditing);
    return true;
}

bool InGameEditor::SaveCampaign() {
    if (m_currentPath.empty() || !m_campaignEditor) {
        return false;
    }

    CampaignFile campaignFile;
    m_contentInfo.modifiedTime = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    campaignFile.SetContentInfo(m_contentInfo);
    m_campaignEditor->SaveToFile(campaignFile);

    if (!campaignFile.Save(m_currentPath)) {
        return false;
    }

    m_hasUnsavedChanges = false;
    return true;
}

bool InGameEditor::PublishToWorkshop(const WorkshopPublishSettings& settings) {
    if (m_workshopBusy) {
        return false;
    }

    m_workshopBusy = true;
    m_workshopProgress = 0.0f;

    // Save content first
    SaveMap();

    // Start async publish operation
    WorkshopIntegration::PublishAsync(
        m_currentPath,
        settings,
        [this](bool success, const std::string& workshopId) {
            m_workshopBusy = false;
            if (success) {
                m_contentInfo.isPublished = true;
                m_contentInfo.workshopId = workshopId;
                if (OnWorkshopPublished) {
                    OnWorkshopPublished(workshopId);
                }
            }
        },
        [this](float progress) {
            m_workshopProgress = progress;
        }
    );

    return true;
}

bool InGameEditor::UpdateWorkshopItem(const std::string& workshopId, const std::string& changeNotes) {
    if (m_workshopBusy) {
        return false;
    }

    m_workshopBusy = true;
    m_workshopProgress = 0.0f;

    SaveMap();

    WorkshopIntegration::UpdateAsync(
        workshopId,
        m_currentPath,
        changeNotes,
        [this](bool success, const std::string& id) {
            m_workshopBusy = false;
            if (success && OnWorkshopPublished) {
                OnWorkshopPublished(id);
            }
        },
        [this](float progress) {
            m_workshopProgress = progress;
        }
    );

    return true;
}

bool InGameEditor::DownloadFromWorkshop(const std::string& workshopId) {
    if (m_workshopBusy) {
        return false;
    }

    m_workshopBusy = true;
    m_workshopProgress = 0.0f;

    WorkshopIntegration::DownloadAsync(
        workshopId,
        [this](bool success, const std::string& localPath) {
            m_workshopBusy = false;
            if (success) {
                LoadMap(localPath);
                if (OnWorkshopDownloaded) {
                    OnWorkshopDownloaded(localPath);
                }
            }
        },
        [this](float progress) {
            m_workshopProgress = progress;
        }
    );

    return true;
}

void InGameEditor::StartTest() {
    if (m_state == EditorState::Testing) {
        return;
    }

    // Save current state
    m_previousState = m_state;

    // Apply map to game world
    if (m_mapEditor && m_game) {
        m_mapEditor->ApplyToWorld(m_game->GetWorld());
    }

    // Apply triggers
    if (m_triggerEditor && m_game) {
        m_triggerEditor->ApplyTriggers(m_game->GetWorld());
    }

    // Start game simulation
    m_state = EditorState::Testing;
    if (m_game) {
        m_game->SetPaused(false);
    }

    if (OnTestStart) {
        OnTestStart();
    }
}

void InGameEditor::StopTest() {
    if (m_state != EditorState::Testing) {
        return;
    }

    // Stop game simulation
    if (m_game) {
        m_game->SetPaused(true);
    }

    // Restore editor state
    m_state = m_previousState;

    // Reload map state
    if (m_mapEditor && m_game) {
        m_mapEditor->RestoreFromWorld(m_game->GetWorld());
    }

    if (OnTestStop) {
        OnTestStop();
    }
}

void InGameEditor::Undo() {
    switch (m_state) {
        case EditorState::MapEditing:
            if (m_mapEditor) m_mapEditor->Undo();
            break;
        case EditorState::TriggerEditing:
            if (m_triggerEditor) m_triggerEditor->Undo();
            break;
        case EditorState::ObjectEditing:
            if (m_objectEditor) m_objectEditor->Undo();
            break;
        case EditorState::CampaignEditing:
            if (m_campaignEditor) m_campaignEditor->Undo();
            break;
        default:
            break;
    }
}

void InGameEditor::Redo() {
    switch (m_state) {
        case EditorState::MapEditing:
            if (m_mapEditor) m_mapEditor->Redo();
            break;
        case EditorState::TriggerEditing:
            if (m_triggerEditor) m_triggerEditor->Redo();
            break;
        case EditorState::ObjectEditing:
            if (m_objectEditor) m_objectEditor->Redo();
            break;
        case EditorState::CampaignEditing:
            if (m_campaignEditor) m_campaignEditor->Redo();
            break;
        default:
            break;
    }
}

bool InGameEditor::CanUndo() const {
    switch (m_state) {
        case EditorState::MapEditing:
            return m_mapEditor && m_mapEditor->CanUndo();
        case EditorState::TriggerEditing:
            return m_triggerEditor && m_triggerEditor->CanUndo();
        case EditorState::ObjectEditing:
            return m_objectEditor && m_objectEditor->CanUndo();
        case EditorState::CampaignEditing:
            return m_campaignEditor && m_campaignEditor->CanUndo();
        default:
            return false;
    }
}

bool InGameEditor::CanRedo() const {
    switch (m_state) {
        case EditorState::MapEditing:
            return m_mapEditor && m_mapEditor->CanRedo();
        case EditorState::TriggerEditing:
            return m_triggerEditor && m_triggerEditor->CanRedo();
        case EditorState::ObjectEditing:
            return m_objectEditor && m_objectEditor->CanRedo();
        case EditorState::CampaignEditing:
            return m_campaignEditor && m_campaignEditor->CanRedo();
        default:
            return false;
    }
}

void InGameEditor::ClearHistory() {
    if (m_mapEditor) m_mapEditor->ClearHistory();
    if (m_triggerEditor) m_triggerEditor->ClearHistory();
    if (m_objectEditor) m_objectEditor->ClearHistory();
    if (m_campaignEditor) m_campaignEditor->ClearHistory();
}

void InGameEditor::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Map...", "Ctrl+N")) {
                m_showNewMapDialog = true;
            }
            if (ImGui::MenuItem("New Campaign...", "Ctrl+Shift+N")) {
                m_showNewCampaignDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open...", "Ctrl+O")) {
                // Show file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", false, !m_currentPath.empty())) {
                SaveMap();
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                m_showNewMapDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Publish to Workshop...", nullptr, false,
                               !m_currentPath.empty() && IsActionAllowed("publish"))) {
                m_showPublishDialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit Editor", "Escape")) {
                ExitEditorMode();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) {
                Undo();
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) {
                Redo();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            if (ImGui::MenuItem("Delete", "Del")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {}
            if (ImGui::MenuItem("Deselect", "Ctrl+D")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Terrain Panel", nullptr, &m_showTerrainPanel);
            ImGui::MenuItem("Object Palette", nullptr, &m_showObjectPalette);
            ImGui::MenuItem("Trigger Panel", nullptr, &m_showTriggerPanel);
            ImGui::MenuItem("Properties Panel", nullptr, &m_showPropertiesPanel);
            ImGui::MenuItem("Minimap", nullptr, &m_showMinimap);
            ImGui::MenuItem("Layer Panel", nullptr, &m_showLayerPanel);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                // Reset panel positions
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Editors")) {
            if (ImGui::MenuItem("Map Editor", "F1", m_state == EditorState::MapEditing)) {
                SetState(EditorState::MapEditing);
            }
            if (ImGui::MenuItem("Trigger Editor", "F2", m_state == EditorState::TriggerEditing)) {
                SetState(EditorState::TriggerEditing);
            }
            if (ImGui::MenuItem("Object Editor", "F3", m_state == EditorState::ObjectEditing,
                               IsActionAllowed("object_editor"))) {
                SetState(EditorState::ObjectEditing);
            }
            if (ImGui::MenuItem("Campaign Editor", "F4", m_state == EditorState::CampaignEditing)) {
                SetState(EditorState::CampaignEditing);
            }
            if (ImGui::MenuItem("Scenario Settings", "F6", m_state == EditorState::ScenarioConfig)) {
                SetState(EditorState::ScenarioConfig);
            }
            if (ImGui::MenuItem("AI Editor", "F7", m_state == EditorState::AIEditing)) {
                SetState(EditorState::AIEditing);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Test")) {
            if (ImGui::MenuItem("Test Map", "F5", m_state == EditorState::Testing)) {
                if (IsTesting()) {
                    StopTest();
                } else {
                    StartTest();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Test Settings...")) {
                // Show test settings
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) {}
            if (ImGui::MenuItem("Tutorials")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {
                m_showAboutDialog = true;
            }
            ImGui::EndMenu();
        }

        // Right-aligned items
        float rightOffset = ImGui::GetWindowWidth() - 200.0f;
        ImGui::SetCursorPosX(rightOffset);

        // Permission level indicator
        const char* permissionText = "Player";
        ImVec4 permissionColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        switch (m_permission) {
            case EditorPermission::Player:
                permissionText = "Player";
                permissionColor = ImVec4(0.3f, 0.7f, 0.3f, 1.0f);
                break;
            case EditorPermission::Modder:
                permissionText = "Modder";
                permissionColor = ImVec4(0.3f, 0.3f, 0.9f, 1.0f);
                break;
            case EditorPermission::Developer:
                permissionText = "Developer";
                permissionColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
                break;
        }
        ImGui::TextColored(permissionColor, "[%s]", permissionText);

        ImGui::EndMainMenuBar();
    }
}

void InGameEditor::RenderToolbar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::BeginViewportSideBar("##Toolbar", ImGui::GetMainViewport(), ImGuiDir_Up,
                                     40.0f, flags)) {
        ImGui::Spacing();

        // Mode buttons
        ImGui::SameLine();
        if (ImGui::Button("Map")) SetState(EditorState::MapEditing);
        ImGui::SameLine();
        if (ImGui::Button("Triggers")) SetState(EditorState::TriggerEditing);
        ImGui::SameLine();
        if (ImGui::Button("Objects")) SetState(EditorState::ObjectEditing);
        ImGui::SameLine();
        if (ImGui::Button("Campaign")) SetState(EditorState::CampaignEditing);
        ImGui::SameLine();
        if (ImGui::Button("Settings")) SetState(EditorState::ScenarioConfig);
        ImGui::SameLine();
        if (ImGui::Button("AI")) SetState(EditorState::AIEditing);

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Test button
        if (IsTesting()) {
            if (ImGui::Button("Stop Test (F5)")) {
                StopTest();
            }
        } else {
            if (ImGui::Button("Test (F5)")) {
                StartTest();
            }
        }

        ImGui::End();
    }
}

void InGameEditor::RenderPanels() {
    // Terrain Panel (left side)
    if (m_showTerrainPanel && m_state == EditorState::MapEditing && m_terrainPanel) {
        m_terrainPanel->Render();
    }

    // Object Palette (left side, below terrain)
    if (m_showObjectPalette && m_state == EditorState::MapEditing && m_objectPalette) {
        m_objectPalette->Render();
    }

    // Trigger Panel (for trigger editing mode)
    if (m_showTriggerPanel && m_state == EditorState::TriggerEditing && m_triggerPanel) {
        m_triggerPanel->Render();
    }

    // Properties Panel (right side)
    if (m_showPropertiesPanel && m_propertiesPanel) {
        m_propertiesPanel->Render();
    }

    // Minimap (bottom right)
    if (m_showMinimap && m_mapEditor) {
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220,
                                        ImGui::GetIO().DisplaySize.y - 220),
                                ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Minimap", &m_showMinimap)) {
            m_mapEditor->RenderMinimap();
        }
        ImGui::End();
    }
}

void InGameEditor::RenderStatusBar() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down,
                                     25.0f, flags)) {
        ImGui::Text("Status: ");
        ImGui::SameLine();

        if (m_hasUnsavedChanges) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Unsaved Changes");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Saved");
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Show current map info
        if (!m_contentInfo.name.empty()) {
            ImGui::Text("Map: %s", m_contentInfo.name.c_str());
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Workshop status
        if (m_workshopBusy) {
            ImGui::Text("Workshop: %.0f%%", m_workshopProgress * 100.0f);
        }

        ImGui::End();
    }
}

void InGameEditor::ShowNewMapDialog() {
    if (!ImGui::IsPopupOpen("New Map")) {
        ImGui::OpenPopup("New Map");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Map", &m_showNewMapDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char name[256] = "Untitled";
        static int width = 128;
        static int height = 128;

        ImGui::InputText("Map Name", name, sizeof(name));
        ImGui::SliderInt("Width", &width, 32, 512);
        ImGui::SliderInt("Height", &height, 32, 512);

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            NewMap(name, width, height);
            m_showNewMapDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewMapDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void InGameEditor::ShowNewCampaignDialog() {
    if (!ImGui::IsPopupOpen("New Campaign")) {
        ImGui::OpenPopup("New Campaign");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("New Campaign", &m_showNewCampaignDialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        static char name[256] = "Untitled Campaign";

        ImGui::InputText("Campaign Name", name, sizeof(name));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            NewCampaign(name);
            m_showNewCampaignDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showNewCampaignDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void InGameEditor::ShowPublishDialog() {
    if (!ImGui::IsPopupOpen("Publish to Workshop")) {
        ImGui::OpenPopup("Publish to Workshop");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Publish to Workshop", &m_showPublishDialog,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        static char title[256] = "";
        static char description[4096] = "";
        static char tags[256] = "";
        static char changeNotes[1024] = "";
        static bool isPublic = true;

        ImGui::InputText("Title", title, sizeof(title));
        ImGui::InputTextMultiline("Description", description, sizeof(description),
                                   ImVec2(400, 100));
        ImGui::InputText("Tags (comma separated)", tags, sizeof(tags));

        if (m_contentInfo.isPublished) {
            ImGui::InputTextMultiline("Change Notes", changeNotes, sizeof(changeNotes),
                                       ImVec2(400, 60));
        }

        ImGui::Checkbox("Public", &isPublic);

        ImGui::Separator();

        if (ImGui::Button("Publish", ImVec2(120, 0))) {
            WorkshopPublishSettings settings;
            settings.title = title;
            settings.description = description;
            settings.isPublic = isPublic;
            settings.changeNotes = changeNotes;

            // Parse tags
            std::string tagStr = tags;
            size_t pos = 0;
            while ((pos = tagStr.find(',')) != std::string::npos) {
                std::string tag = tagStr.substr(0, pos);
                // Trim whitespace
                tag.erase(0, tag.find_first_not_of(" \t"));
                tag.erase(tag.find_last_not_of(" \t") + 1);
                if (!tag.empty()) {
                    settings.tags.push_back(tag);
                }
                tagStr.erase(0, pos + 1);
            }
            if (!tagStr.empty()) {
                tagStr.erase(0, tagStr.find_first_not_of(" \t"));
                tagStr.erase(tagStr.find_last_not_of(" \t") + 1);
                if (!tagStr.empty()) {
                    settings.tags.push_back(tagStr);
                }
            }

            if (m_contentInfo.isPublished) {
                UpdateWorkshopItem(m_contentInfo.workshopId, changeNotes);
            } else {
                PublishToWorkshop(settings);
            }

            m_showPublishDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showPublishDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void InGameEditor::ShowSettingsDialog() {
    if (!ImGui::IsPopupOpen("Editor Settings")) {
        ImGui::OpenPopup("Editor Settings");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Editor Settings", &m_showSettingsDialog)) {
        // General settings
        if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
            static bool autoSave = true;
            static int autoSaveInterval = 5;
            ImGui::Checkbox("Auto-save", &autoSave);
            ImGui::SliderInt("Auto-save Interval (min)", &autoSaveInterval, 1, 30);
        }

        // Grid settings
        if (ImGui::CollapsingHeader("Grid")) {
            static bool showGrid = true;
            static float gridSize = 1.0f;
            ImGui::Checkbox("Show Grid", &showGrid);
            ImGui::SliderFloat("Grid Size", &gridSize, 0.25f, 4.0f);
        }

        // Camera settings
        if (ImGui::CollapsingHeader("Camera")) {
            static float cameraSpeed = 10.0f;
            static float zoomSpeed = 0.5f;
            ImGui::SliderFloat("Camera Speed", &cameraSpeed, 1.0f, 50.0f);
            ImGui::SliderFloat("Zoom Speed", &zoomSpeed, 0.1f, 2.0f);
        }

        ImGui::Separator();

        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showSettingsDialog = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void InGameEditor::InitializePermissions() {
    // Player can do basic map editing
    // Modder can modify objects
    // Developer can do everything

    m_actionPermissions["object_editor"] = EditorPermission::Modder;
    m_actionPermissions["modify_stats"] = EditorPermission::Modder;
    m_actionPermissions["modify_abilities"] = EditorPermission::Modder;
    m_actionPermissions["publish"] = EditorPermission::Player;
    m_actionPermissions["advanced_triggers"] = EditorPermission::Modder;
    m_actionPermissions["raw_script"] = EditorPermission::Developer;
    m_actionPermissions["debug_tools"] = EditorPermission::Developer;
}

bool InGameEditor::CheckPermission(EditorPermission required) const {
    return static_cast<int>(m_permission) >= static_cast<int>(required);
}

} // namespace Vehement
