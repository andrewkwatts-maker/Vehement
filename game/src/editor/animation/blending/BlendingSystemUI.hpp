#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class Skeleton;
class Animation;
class BlendNode;
class BlendMask;
class AnimationLayerStack;
class BlendTreeRuntime;
struct AnimationPose;
}

namespace Vehement {

class Editor;

namespace WebEditor {
class WebView;
class JSBridge;
}

/**
 * @brief Main UI for the animation blending system
 *
 * Provides a comprehensive interface for:
 * - Layer stack management
 * - Blend tree editing per layer
 * - Parameter controls
 * - Live skeleton preview
 * - Mask editing
 */
class BlendingSystemUI {
public:
    /**
     * @brief Configuration for the UI
     */
    struct Config {
        int windowWidth = 1200;
        int windowHeight = 800;
        float previewPanelWidth = 400.0f;
        float layerPanelWidth = 250.0f;
        float parameterPanelHeight = 200.0f;
        bool showPreview = true;
        bool showParameters = true;
        bool showLayers = true;
    };

    /**
     * @brief Parameter display info
     */
    struct ParameterInfo {
        std::string name;
        float value = 0.0f;
        float minValue = -1.0f;
        float maxValue = 1.0f;
        bool isSmooth = false;
        float smoothSpeed = 10.0f;
    };

    /**
     * @brief Layer display info
     */
    struct LayerInfo {
        std::string name;
        float weight = 1.0f;
        bool enabled = true;
        bool muted = false;
        bool solo = false;
        std::string blendMode;
        std::string maskName;
    };

    explicit BlendingSystemUI(Editor* editor);
    ~BlendingSystemUI();

    // Non-copyable
    BlendingSystemUI(const BlendingSystemUI&) = delete;
    BlendingSystemUI& operator=(const BlendingSystemUI&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Initialize(const Config& config = {});
    void Shutdown();
    void Update(float deltaTime);
    void Render();

    // =========================================================================
    // Data Binding
    // =========================================================================

    /**
     * @brief Set the blend tree runtime to edit
     */
    void SetRuntime(Nova::BlendTreeRuntime* runtime);

    /**
     * @brief Set the layer stack to edit
     */
    void SetLayerStack(Nova::AnimationLayerStack* stack);

    /**
     * @brief Set skeleton for preview
     */
    void SetSkeleton(Nova::Skeleton* skeleton);

    /**
     * @brief Get current runtime
     */
    [[nodiscard]] Nova::BlendTreeRuntime* GetRuntime() const { return m_runtime; }

    // =========================================================================
    // Preview
    // =========================================================================

    /**
     * @brief Enable live preview
     */
    void SetPreviewEnabled(bool enabled);
    [[nodiscard]] bool IsPreviewEnabled() const { return m_previewEnabled; }

    /**
     * @brief Set preview playback speed
     */
    void SetPreviewSpeed(float speed) { m_previewSpeed = speed; }

    /**
     * @brief Play/pause preview
     */
    void PlayPreview();
    void PausePreview();
    void ResetPreview();

    /**
     * @brief Get current preview pose
     */
    [[nodiscard]] const Nova::AnimationPose* GetPreviewPose() const;

    // =========================================================================
    // Parameters
    // =========================================================================

    /**
     * @brief Get all parameters
     */
    [[nodiscard]] std::vector<ParameterInfo> GetParameters() const;

    /**
     * @brief Set parameter value from UI
     */
    void SetParameter(const std::string& name, float value);

    /**
     * @brief Add a new parameter
     */
    void AddParameter(const std::string& name, float defaultValue = 0.0f);

    /**
     * @brief Remove a parameter
     */
    void RemoveParameter(const std::string& name);

    // =========================================================================
    // Layers
    // =========================================================================

    /**
     * @brief Get all layers
     */
    [[nodiscard]] std::vector<LayerInfo> GetLayers() const;

    /**
     * @brief Set layer weight
     */
    void SetLayerWeight(size_t index, float weight);

    /**
     * @brief Set layer enabled
     */
    void SetLayerEnabled(size_t index, bool enabled);

    /**
     * @brief Set layer muted
     */
    void SetLayerMuted(size_t index, bool muted);

    /**
     * @brief Solo a layer
     */
    void SoloLayer(size_t index);

    /**
     * @brief Clear solo
     */
    void ClearSolo();

    /**
     * @brief Reorder layers
     */
    void MoveLayer(size_t fromIndex, size_t toIndex);

    // =========================================================================
    // Blend Tree Editing
    // =========================================================================

    /**
     * @brief Open blend tree editor for a layer
     */
    void EditLayerTree(size_t layerIndex);

    /**
     * @brief Open mask editor for a layer
     */
    void EditLayerMask(size_t layerIndex);

    // =========================================================================
    // Sub-editors
    // =========================================================================

    /**
     * @brief Open 1D blend space editor
     */
    void OpenBlendSpace1DEditor(const std::string& blendSpaceId);

    /**
     * @brief Open 2D blend space editor
     */
    void OpenBlendSpace2DEditor(const std::string& blendSpaceId);

    /**
     * @brief Open mask editor
     */
    void OpenMaskEditor(const std::string& maskId);

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Load blend configuration from file
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Save blend configuration to file
     */
    bool SaveToFile(const std::string& path);

    /**
     * @brief Create new blend tree
     */
    void NewBlendTree();

    // =========================================================================
    // Visibility
    // =========================================================================

    void SetVisible(bool visible) { m_visible = visible; }
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    void SetPreviewPanelVisible(bool visible);
    void SetLayerPanelVisible(bool visible);
    void SetParameterPanelVisible(bool visible);

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void()> OnBlendTreeChanged;
    std::function<void(const std::string&, float)> OnParameterChanged;
    std::function<void(size_t)> OnLayerSelected;
    std::function<void()> OnSaveRequested;

private:
    void SetupJSBridge();
    void UpdatePreview(float deltaTime);
    void SyncUIState();
    void NotifyBlendTreeChanged();

    Editor* m_editor = nullptr;
    Config m_config;
    bool m_visible = true;
    bool m_initialized = false;

    // Web view
    std::unique_ptr<WebEditor::WebView> m_webView;
    std::unique_ptr<WebEditor::JSBridge> m_bridge;

    // Data
    Nova::BlendTreeRuntime* m_runtime = nullptr;
    Nova::AnimationLayerStack* m_layerStack = nullptr;
    Nova::Skeleton* m_skeleton = nullptr;

    // Preview state
    bool m_previewEnabled = true;
    bool m_previewPlaying = false;
    float m_previewSpeed = 1.0f;
    float m_previewTime = 0.0f;
    std::unique_ptr<Nova::AnimationPose> m_previewPose;

    // Selection
    int m_selectedLayerIndex = -1;

    // Dirty flag
    bool m_isDirty = false;
};

} // namespace Vehement
