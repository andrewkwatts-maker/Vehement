#include "StandaloneEditorIntegration.hpp"
#include "game/src/editor/ConfigEditor.hpp"
#include "game/src/editor/sdf/SDFModelEditor.hpp"
#include "game/src/editor/ingame/CampaignEditor.hpp"
#include "game/src/editor/ingame/MapEditor.hpp"
#include "game/src/editor/ingame/TriggerEditor.hpp"
#include "game/src/editor/ingame/ObjectEditor.hpp"
#include "game/src/editor/ingame/AIEditor.hpp"
#include "game/src/editor/ingame/InGameEditor.hpp"
#include "game/src/editor/animation/StateMachineEditor.hpp"
#include "game/src/editor/animation/BlendTreeEditor.hpp"
#include "game/src/editor/animation/KeyframeEditor.hpp"
#include "game/src/editor/animation/BoneAnimationEditor.hpp"
#include "game/src/editor/race/TalentTreeEditor.hpp"
#include "game/src/editor/terrain/TerrainEditor.hpp"
#include "game/src/editor/terrain/WorldTerrainEditor.hpp"
#include "game/src/editor/ScriptEditor.hpp"
#include "game/src/editor/LevelEditor.hpp"
#include "game/src/editor/Editor.hpp"

#include <spdlog/spdlog.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

EditorIntegration::EditorIntegration() = default;
EditorIntegration::~EditorIntegration() = default;

bool EditorIntegration::Initialize(Vehement::Editor* mainEditor) {
    if (m_initialized) {
        return true;
    }

    m_mainEditor = mainEditor;
    m_initialized = true;

    spdlog::info("EditorIntegration initialized");
    return true;
}

void EditorIntegration::Shutdown() {
    CloseAllEditors();

    // Cleanup all editor instances
    m_configEditor.reset();
    m_sdfModelEditor.reset();
    m_campaignEditor.reset();
    m_mapEditor.reset();
    m_terrainEditor.reset();
    m_stateMachineEditor.reset();
    m_blendTreeEditor.reset();
    m_talentTreeEditor.reset();
    m_scriptEditor.reset();
    m_keyframeEditor.reset();
    m_boneAnimationEditor.reset();
    m_triggerEditor.reset();
    m_objectEditor.reset();
    m_aiEditor.reset();
    m_levelEditor.reset();
    m_worldTerrainEditor.reset();

    m_initialized = false;
    spdlog::info("EditorIntegration shut down");
}

AssetType EditorIntegration::DetectAssetType(const std::string& path) {
    fs::path p(path);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    std::string filename = p.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

    // Check by extension and filename patterns
    if (ext == ".json") {
        // Inspect filename to determine JSON type
        if (filename.find("campaign") != std::string::npos) {
            return AssetType::Campaign;
        }
        if (filename.find("map") != std::string::npos) {
            return AssetType::Map;
        }
        if (filename.find("trigger") != std::string::npos) {
            return AssetType::Trigger;
        }
        if (filename.find("talent") != std::string::npos || filename.find("skill") != std::string::npos) {
            return AssetType::TalentTree;
        }
        if (filename.find("statemachine") != std::string::npos) {
            return AssetType::StateMachine;
        }
        if (filename.find("blendtree") != std::string::npos) {
            return AssetType::BlendTree;
        }
        // Default JSON files to config editor
        return AssetType::Config;
    }

    if (ext == ".sdf" || ext == ".sdfmodel") {
        return AssetType::SDFModel;
    }

    if (ext == ".map" || ext == ".tmx") {
        return AssetType::Map;
    }

    if (ext == ".campaign") {
        return AssetType::Campaign;
    }

    if (ext == ".anim" || ext == ".animation") {
        return AssetType::Animation;
    }

    if (ext == ".statemachine" || ext == ".asm") {
        return AssetType::StateMachine;
    }

    if (ext == ".blendtree") {
        return AssetType::BlendTree;
    }

    if (ext == ".talent" || ext == ".tree") {
        return AssetType::TalentTree;
    }

    if (ext == ".terrain" || ext == ".heightmap") {
        return AssetType::Terrain;
    }

    if (ext == ".lua" || ext == ".py" || ext == ".js" || ext == ".cpp" || ext == ".hpp") {
        return AssetType::Script;
    }

    if (ext == ".level" || ext == ".scene") {
        return AssetType::Level;
    }

    return AssetType::Unknown;
}

bool EditorIntegration::OpenAssetInEditor(const std::string& path, AssetType type) {
    if (!m_initialized) {
        spdlog::error("EditorIntegration not initialized");
        return false;
    }

    // Auto-detect type if unknown
    if (type == AssetType::Unknown) {
        type = DetectAssetType(path);
        if (type == AssetType::Unknown) {
            spdlog::warn("Could not determine asset type for: {}", path);
            return false;
        }
    }

    // Check if already open
    if (IsAssetOpen(path)) {
        spdlog::info("Asset already open: {}", path);
        return true;
    }

    // Mark as open
    m_openEditors[path] = type;
    m_openEditorPaths.push_back(path);

    spdlog::info("Opening asset in editor: {} (type: {})", path, static_cast<int>(type));

    // Load asset into appropriate editor
    bool success = false;
    try {
        switch (type) {
            case AssetType::Config: {
                auto* editor = GetConfigEditor();
                if (editor) {
                    editor->SelectConfig(path);
                    success = true;
                }
                break;
            }

            case AssetType::SDFModel: {
                auto* editor = GetSDFModelEditor();
                if (editor) {
                    success = editor->LoadModel(path);
                }
                break;
            }

            case AssetType::Campaign: {
                auto* editor = GetCampaignEditor();
                if (editor) {
                    // Campaign editor uses a different loading mechanism
                    // success = editor->LoadFromFile(...);
                    success = true; // Placeholder
                }
                break;
            }

            case AssetType::Map: {
                auto* editor = GetMapEditor();
                if (editor) {
                    // success = editor->LoadFromFile(...);
                    success = true; // Placeholder
                }
                break;
            }

            case AssetType::Terrain: {
                auto* editor = GetTerrainEditor();
                if (editor) {
                    success = true;
                }
                break;
            }

            case AssetType::StateMachine: {
                auto* editor = GetStateMachineEditor();
                if (editor) {
                    success = editor->LoadStateMachine(path);
                }
                break;
            }

            case AssetType::TalentTree: {
                auto* editor = GetTalentTreeEditor();
                if (editor) {
                    success = editor->LoadTree(path);
                }
                break;
            }

            case AssetType::Script: {
                auto* editor = GetScriptEditor();
                if (editor) {
                    // success = editor->LoadScript(path);
                    success = true; // Placeholder
                }
                break;
            }

            default:
                spdlog::warn("No editor available for asset type: {}", static_cast<int>(type));
                break;
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception opening asset: {}", e.what());
        success = false;
    }

    if (!success) {
        CloseEditor(path);
    }

    return success;
}

void EditorIntegration::CloseEditor(const std::string& path) {
    auto it = m_openEditors.find(path);
    if (it != m_openEditors.end()) {
        m_openEditors.erase(it);

        auto pathIt = std::find(m_openEditorPaths.begin(), m_openEditorPaths.end(), path);
        if (pathIt != m_openEditorPaths.end()) {
            m_openEditorPaths.erase(pathIt);
        }

        spdlog::info("Closed editor for: {}", path);
    }
}

void EditorIntegration::CloseAllEditors() {
    m_openEditors.clear();
    m_openEditorPaths.clear();
    spdlog::info("Closed all editors");
}

bool EditorIntegration::IsAssetOpen(const std::string& path) const {
    return m_openEditors.find(path) != m_openEditors.end();
}

void EditorIntegration::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update all active editors
    if (m_configEditor) m_configEditor->Update(deltaTime);
    if (m_sdfModelEditor) m_sdfModelEditor->Update(deltaTime);
    if (m_campaignEditor) m_campaignEditor->Update(deltaTime);
    if (m_mapEditor) m_mapEditor->Update(deltaTime);
    if (m_terrainEditor) m_terrainEditor->Update(deltaTime);
    if (m_stateMachineEditor) m_stateMachineEditor->Update(deltaTime);
    if (m_talentTreeEditor) m_talentTreeEditor->Update(deltaTime);
}

void EditorIntegration::RenderUI() {
    if (!m_initialized) {
        return;
    }

    // Render editor windows for all open assets
    for (const auto& path : m_openEditorPaths) {
        auto it = m_openEditors.find(path);
        if (it != m_openEditors.end()) {
            RenderEditorWindow(path, it->second);
        }
    }
}

void EditorIntegration::RenderEditorWindow(const std::string& path, AssetType type) {
    fs::path p(path);
    std::string windowTitle = "Editor: " + p.filename().string();

    bool isOpen = true;
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;

    if (ImGui::Begin(windowTitle.c_str(), &isOpen, flags)) {
        // Render appropriate editor content
        switch (type) {
            case AssetType::Config:
                if (m_configEditor) m_configEditor->Render();
                break;

            case AssetType::SDFModel:
                if (m_sdfModelEditor) m_sdfModelEditor->RenderUI();
                break;

            case AssetType::Campaign:
                if (m_campaignEditor) m_campaignEditor->Render();
                break;

            case AssetType::Map:
                if (m_mapEditor) m_mapEditor->Render();
                break;

            case AssetType::Terrain:
                if (m_terrainEditor) m_terrainEditor->RenderUI();
                break;

            case AssetType::StateMachine:
                if (m_stateMachineEditor) m_stateMachineEditor->RenderUI();
                break;

            case AssetType::TalentTree:
                if (m_talentTreeEditor) m_talentTreeEditor->RenderUI();
                break;

            case AssetType::Script:
                if (m_scriptEditor) m_scriptEditor->RenderUI();
                break;

            default:
                ImGui::Text("Editor not yet implemented for this asset type");
                break;
        }
    }
    ImGui::End();

    // Close editor if window was closed
    if (!isOpen) {
        CloseEditor(path);
    }
}

// Editor getters with lazy initialization
Vehement::ConfigEditor* EditorIntegration::GetConfigEditor() {
    if (!m_configEditor && m_mainEditor) {
        m_configEditor = std::make_unique<Vehement::ConfigEditor>(m_mainEditor);
        spdlog::info("ConfigEditor created");
    }
    return m_configEditor.get();
}

Vehement::SDFModelEditor* EditorIntegration::GetSDFModelEditor() {
    if (!m_sdfModelEditor) {
        m_sdfModelEditor = std::make_unique<Vehement::SDFModelEditor>();
        if (m_mainEditor) {
            m_sdfModelEditor->Initialize(m_mainEditor);
        }
        spdlog::info("SDFModelEditor created");
    }
    return m_sdfModelEditor.get();
}

Vehement::CampaignEditor* EditorIntegration::GetCampaignEditor() {
    if (!m_campaignEditor) {
        m_campaignEditor = std::make_unique<Vehement::CampaignEditor>();
        // Initialize if needed
        spdlog::info("CampaignEditor created");
    }
    return m_campaignEditor.get();
}

Vehement::MapEditor* EditorIntegration::GetMapEditor() {
    if (!m_mapEditor) {
        m_mapEditor = std::make_unique<Vehement::MapEditor>();
        // Initialize if needed
        spdlog::info("MapEditor created");
    }
    return m_mapEditor.get();
}

Vehement::TerrainEditor* EditorIntegration::GetTerrainEditor() {
    if (!m_terrainEditor) {
        m_terrainEditor = std::make_unique<Vehement::TerrainEditor>();
        // Initialize if needed
        spdlog::info("TerrainEditor created");
    }
    return m_terrainEditor.get();
}

Vehement::StateMachineEditor* EditorIntegration::GetStateMachineEditor() {
    if (!m_stateMachineEditor) {
        m_stateMachineEditor = std::make_unique<Vehement::StateMachineEditor>();
        m_stateMachineEditor->Initialize();
        spdlog::info("StateMachineEditor created");
    }
    return m_stateMachineEditor.get();
}

Vehement::TalentTreeEditor* EditorIntegration::GetTalentTreeEditor() {
    if (!m_talentTreeEditor) {
        m_talentTreeEditor = std::make_unique<Vehement::TalentTreeEditor>();
        m_talentTreeEditor->Initialize();
        spdlog::info("TalentTreeEditor created");
    }
    return m_talentTreeEditor.get();
}

Vehement::ScriptEditor* EditorIntegration::GetScriptEditor() {
    if (!m_scriptEditor) {
        m_scriptEditor = std::make_unique<Vehement::ScriptEditor>();
        // Initialize if needed
        spdlog::info("ScriptEditor created");
    }
    return m_scriptEditor.get();
}
