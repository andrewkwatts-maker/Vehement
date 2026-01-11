#pragma once

#include <memory>
#include <string>
#include <map>
#include <functional>

// Forward declarations for all integrated editors
namespace Vehement {
    class ConfigEditor;
    class SDFModelEditor;
    class CampaignEditor;
    class MapEditor;
    class InGameEditor;
    class TriggerEditor;
    class ObjectEditor;
    class AIEditor;
    class StateMachineEditor;
    class BlendTreeEditor;
    class KeyframeEditor;
    class BoneAnimationEditor;
    class TalentTreeEditor;
    class TerrainEditor;
    class WorldTerrainEditor;
    class ScriptEditor;
    class LevelEditor;
    class Editor;
}

/**
 * @brief Asset type for routing to appropriate editor
 */
enum class AssetType {
    Unknown,
    Config,          // JSON configs
    SDFModel,        // SDF model files
    Campaign,        // Campaign definitions
    Map,             // Map files
    Trigger,         // Trigger definitions
    Animation,       // Animation files
    StateMachine,    // Animation state machines
    BlendTree,       // Blend trees
    TalentTree,      // Talent/skill trees
    Terrain,         // Terrain data
    Script,          // Script files
    Level            // Level data
};

/**
 * @brief Base interface for asset editors (optional - for editors that follow this pattern)
 */
class IAssetEditor {
public:
    virtual ~IAssetEditor() = default;
    virtual bool LoadAsset(const std::string& path) = 0;
    virtual bool SaveAsset(const std::string& path) = 0;
    virtual void RenderUI() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual const std::string& GetAssetPath() const = 0;
    virtual bool HasUnsavedChanges() const = 0;
};

/**
 * @brief Editor integration manager for StandaloneEditor
 *
 * This class manages integration of all existing editors into the standalone editor.
 * It handles:
 * - Lazy loading of editors
 * - Routing assets to appropriate editors
 * - Managing open editor windows
 * - Editor lifecycle
 */
class EditorIntegration {
public:
    EditorIntegration();
    ~EditorIntegration();

    /**
     * @brief Initialize the integration system
     */
    bool Initialize(Vehement::Editor* mainEditor);

    /**
     * @brief Shutdown and cleanup all editors
     */
    void Shutdown();

    /**
     * @brief Open asset in appropriate editor
     * @param path Full path to asset file
     * @param type Asset type (auto-detected if Unknown)
     * @return true if editor was opened successfully
     */
    bool OpenAssetInEditor(const std::string& path, AssetType type = AssetType::Unknown);

    /**
     * @brief Close editor for specific asset
     */
    void CloseEditor(const std::string& path);

    /**
     * @brief Close all open editors
     */
    void CloseAllEditors();

    /**
     * @brief Update all open editors
     */
    void Update(float deltaTime);

    /**
     * @brief Render UI for all open editors
     */
    void RenderUI();

    /**
     * @brief Get list of open editor paths
     */
    const std::vector<std::string>& GetOpenEditors() const { return m_openEditorPaths; }

    /**
     * @brief Check if an asset is currently open
     */
    bool IsAssetOpen(const std::string& path) const;

    /**
     * @brief Detect asset type from file extension/path
     */
    static AssetType DetectAssetType(const std::string& path);

    /**
     * @brief Get or create Config Editor
     */
    Vehement::ConfigEditor* GetConfigEditor();

    /**
     * @brief Get or create SDF Model Editor
     */
    Vehement::SDFModelEditor* GetSDFModelEditor();

    /**
     * @brief Get or create Campaign Editor
     */
    Vehement::CampaignEditor* GetCampaignEditor();

    /**
     * @brief Get or create Map Editor
     */
    Vehement::MapEditor* GetMapEditor();

    /**
     * @brief Get or create Terrain Editor
     */
    Vehement::TerrainEditor* GetTerrainEditor();

    /**
     * @brief Get or create State Machine Editor
     */
    Vehement::StateMachineEditor* GetStateMachineEditor();

    /**
     * @brief Get or create Talent Tree Editor
     */
    Vehement::TalentTreeEditor* GetTalentTreeEditor();

    /**
     * @brief Get or create Script Editor
     */
    Vehement::ScriptEditor* GetScriptEditor();

private:
    // Editor instances (lazy-loaded)
    std::unique_ptr<Vehement::ConfigEditor> m_configEditor;
    std::unique_ptr<Vehement::SDFModelEditor> m_sdfModelEditor;
    std::unique_ptr<Vehement::CampaignEditor> m_campaignEditor;
    std::unique_ptr<Vehement::MapEditor> m_mapEditor;
    std::unique_ptr<Vehement::TerrainEditor> m_terrainEditor;
    std::unique_ptr<Vehement::StateMachineEditor> m_stateMachineEditor;
    std::unique_ptr<Vehement::BlendTreeEditor> m_blendTreeEditor;
    std::unique_ptr<Vehement::TalentTreeEditor> m_talentTreeEditor;
    std::unique_ptr<Vehement::ScriptEditor> m_scriptEditor;
    std::unique_ptr<Vehement::KeyframeEditor> m_keyframeEditor;
    std::unique_ptr<Vehement::BoneAnimationEditor> m_boneAnimationEditor;
    std::unique_ptr<Vehement::TriggerEditor> m_triggerEditor;
    std::unique_ptr<Vehement::ObjectEditor> m_objectEditor;
    std::unique_ptr<Vehement::AIEditor> m_aiEditor;
    std::unique_ptr<Vehement::LevelEditor> m_levelEditor;
    std::unique_ptr<Vehement::WorldTerrainEditor> m_worldTerrainEditor;

    // Track which assets are open and which editor type
    std::map<std::string, AssetType> m_openEditors;
    std::vector<std::string> m_openEditorPaths;

    // Main editor reference
    Vehement::Editor* m_mainEditor = nullptr;
    bool m_initialized = false;

    // Helper to render specific editor
    void RenderEditorWindow(const std::string& path, AssetType type);
};
