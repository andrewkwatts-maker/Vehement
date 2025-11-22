#pragma once

#include "BoneAnimationEditor.hpp"
#include "KeyframeEditor.hpp"
#include "AnimationTimeline.hpp"
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <functional>

namespace Nova {
    class Mesh;
    class Shader;
    class Framebuffer;
    class Camera;
}

namespace Vehement {

/**
 * @brief Skeleton rendering style
 */
enum class SkeletonRenderStyle {
    Lines,          // Simple lines between bones
    Bones,          // Bone shapes (octahedron)
    Spheres,        // Spheres at joints
    Custom          // Custom bone mesh
};

/**
 * @brief Background style
 */
enum class BackgroundStyle {
    SolidColor,
    Gradient,
    Checkerboard,
    Grid,
    Skybox,
    Image
};

/**
 * @brief Lighting preset
 */
enum class LightingPreset {
    Default,
    Studio,
    Outdoor,
    Dramatic,
    Flat,
    Custom
};

/**
 * @brief Camera preset
 */
enum class CameraPreset {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Perspective,
    Custom
};

/**
 * @brief Render settings
 */
struct PreviewRenderSettings {
    // Skeleton
    SkeletonRenderStyle skeletonStyle = SkeletonRenderStyle::Bones;
    bool showSkeleton = true;
    bool showBoneNames = false;
    bool showJoints = true;
    float boneThickness = 2.0f;
    glm::vec4 boneColor{0.5f, 0.7f, 1.0f, 1.0f};
    glm::vec4 selectedBoneColor{1.0f, 0.8f, 0.0f, 1.0f};
    glm::vec4 jointColor{0.8f, 0.8f, 1.0f, 1.0f};

    // Mesh
    bool showMesh = true;
    bool showWireframe = false;
    float meshOpacity = 1.0f;
    bool showNormals = false;
    float normalLength = 0.1f;

    // Ground plane
    bool showGroundPlane = true;
    glm::vec4 groundColor{0.3f, 0.3f, 0.35f, 1.0f};
    float groundSize = 10.0f;
    bool showGroundGrid = true;
    float gridSpacing = 1.0f;

    // Background
    BackgroundStyle backgroundStyle = BackgroundStyle::Gradient;
    glm::vec4 backgroundColor{0.15f, 0.15f, 0.2f, 1.0f};
    glm::vec4 backgroundGradientTop{0.2f, 0.2f, 0.3f, 1.0f};
    glm::vec4 backgroundGradientBottom{0.1f, 0.1f, 0.15f, 1.0f};

    // Lighting
    LightingPreset lightingPreset = LightingPreset::Studio;
    glm::vec3 lightDirection{-0.5f, 1.0f, 0.5f};
    glm::vec4 lightColor{1.0f, 0.98f, 0.95f, 1.0f};
    float lightIntensity = 1.0f;
    glm::vec4 ambientColor{0.2f, 0.2f, 0.25f, 1.0f};

    // Effects
    bool showShadows = true;
    bool antialiasing = true;
    int samples = 4;
};

/**
 * @brief Camera controls
 */
struct PreviewCamera {
    glm::vec3 position{0.0f, 1.5f, 5.0f};
    glm::vec3 target{0.0f, 1.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Orbit controls
    float distance = 5.0f;
    float azimuth = 0.0f;
    float elevation = 15.0f;
    glm::vec3 orbitCenter{0.0f, 1.0f, 0.0f};

    // Constraints
    float minDistance = 0.5f;
    float maxDistance = 50.0f;
    float minElevation = -89.0f;
    float maxElevation = 89.0f;
};

/**
 * @brief Animation 3D preview
 *
 * Features:
 * - Skeleton rendering
 * - Mesh rendering
 * - Ground plane
 * - Camera controls
 * - Lighting options
 * - Background options
 */
class AnimationPreview {
public:
    struct Config {
        int viewportWidth = 800;
        int viewportHeight = 600;
        bool enableMSAA = true;
        int msaaSamples = 4;
    };

    AnimationPreview();
    ~AnimationPreview();

    /**
     * @brief Initialize the preview
     */
    bool Initialize(const Config& config = {});

    /**
     * @brief Shutdown preview
     */
    void Shutdown();

    /**
     * @brief Set bone animation editor
     */
    void SetBoneEditor(BoneAnimationEditor* editor) { m_boneEditor = editor; }

    /**
     * @brief Set keyframe editor
     */
    void SetKeyframeEditor(KeyframeEditor* editor) { m_keyframeEditor = editor; }

    /**
     * @brief Set timeline
     */
    void SetTimeline(AnimationTimeline* timeline) { m_timeline = timeline; }

    // =========================================================================
    // Mesh Loading
    // =========================================================================

    /**
     * @brief Load character mesh
     */
    bool LoadMesh(const std::string& meshPath);

    /**
     * @brief Unload current mesh
     */
    void UnloadMesh();

    /**
     * @brief Check if mesh is loaded
     */
    [[nodiscard]] bool HasMesh() const { return m_mesh != nullptr; }

    /**
     * @brief Set mesh visibility
     */
    void SetMeshVisible(bool visible) { m_settings.showMesh = visible; }

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Update preview
     */
    void Update(float deltaTime);

    /**
     * @brief Render preview to framebuffer
     */
    void Render();

    /**
     * @brief Get rendered texture ID for display
     */
    [[nodiscard]] unsigned int GetRenderedTextureId() const;

    /**
     * @brief Resize viewport
     */
    void Resize(int width, int height);

    /**
     * @brief Get viewport size
     */
    [[nodiscard]] glm::ivec2 GetViewportSize() const {
        return {m_config.viewportWidth, m_config.viewportHeight};
    }

    // =========================================================================
    // Render Settings
    // =========================================================================

    /**
     * @brief Get render settings
     */
    [[nodiscard]] PreviewRenderSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const PreviewRenderSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Apply settings
     */
    void ApplySettings(const PreviewRenderSettings& settings);

    /**
     * @brief Reset to default settings
     */
    void ResetSettings();

    /**
     * @brief Apply lighting preset
     */
    void ApplyLightingPreset(LightingPreset preset);

    /**
     * @brief Apply background preset
     */
    void ApplyBackgroundPreset(BackgroundStyle style);

    // =========================================================================
    // Camera Controls
    // =========================================================================

    /**
     * @brief Get camera
     */
    [[nodiscard]] PreviewCamera& GetCamera() { return m_camera; }
    [[nodiscard]] const PreviewCamera& GetCamera() const { return m_camera; }

    /**
     * @brief Apply camera preset
     */
    void ApplyCameraPreset(CameraPreset preset);

    /**
     * @brief Orbit camera
     */
    void OrbitCamera(float deltaAzimuth, float deltaElevation);

    /**
     * @brief Pan camera
     */
    void PanCamera(float deltaX, float deltaY);

    /**
     * @brief Zoom camera
     */
    void ZoomCamera(float delta);

    /**
     * @brief Reset camera
     */
    void ResetCamera();

    /**
     * @brief Focus on skeleton
     */
    void FocusOnSkeleton();

    /**
     * @brief Focus on bone
     */
    void FocusOnBone(const std::string& boneName);

    // =========================================================================
    // Mouse Interaction
    // =========================================================================

    /**
     * @brief Handle mouse down
     */
    void OnMouseDown(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse up
     */
    void OnMouseUp(const glm::vec2& position, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& position, const glm::vec2& delta);

    /**
     * @brief Handle mouse wheel
     */
    void OnMouseWheel(float delta);

    /**
     * @brief Pick bone at screen position
     */
    [[nodiscard]] std::string PickBoneAtScreen(const glm::vec2& screenPos);

    // =========================================================================
    // Screenshot
    // =========================================================================

    /**
     * @brief Capture screenshot
     */
    bool CaptureScreenshot(const std::string& filePath);

    /**
     * @brief Capture thumbnail
     */
    bool CaptureThumbnail(const std::string& filePath, int width = 128, int height = 128);

    // =========================================================================
    // Animation Playback
    // =========================================================================

    /**
     * @brief Set animation time for preview
     */
    void SetAnimationTime(float time);

    /**
     * @brief Enable auto-update from timeline
     */
    void SetAutoUpdateFromTimeline(bool enabled) { m_autoUpdateFromTimeline = enabled; }

    // =========================================================================
    // Gizmos
    // =========================================================================

    /**
     * @brief Show transform gizmo for selected bone
     */
    void SetShowGizmo(bool show) { m_showGizmo = show; }

    /**
     * @brief Get show gizmo
     */
    [[nodiscard]] bool GetShowGizmo() const { return m_showGizmo; }

    /**
     * @brief Set gizmo mode
     */
    void SetGizmoMode(GizmoMode mode) { m_gizmoMode = mode; }

    /**
     * @brief Get gizmo mode
     */
    [[nodiscard]] GizmoMode GetGizmoMode() const { return m_gizmoMode; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    std::function<void(const std::string&)> OnBoneClicked;
    std::function<void()> OnViewChanged;

private:
    void RenderSkeleton();
    void RenderMesh();
    void RenderGroundPlane();
    void RenderBackground();
    void RenderGizmo();
    void UpdateCameraMatrices();
    glm::vec3 ScreenToWorld(const glm::vec2& screenPos, float depth) const;
    glm::vec2 WorldToScreen(const glm::vec3& worldPos) const;

    Config m_config;
    PreviewRenderSettings m_settings;
    PreviewCamera m_camera;

    // References
    BoneAnimationEditor* m_boneEditor = nullptr;
    KeyframeEditor* m_keyframeEditor = nullptr;
    AnimationTimeline* m_timeline = nullptr;

    // Rendering resources
    std::unique_ptr<Nova::Framebuffer> m_framebuffer;
    std::unique_ptr<Nova::Shader> m_skeletonShader;
    std::unique_ptr<Nova::Shader> m_meshShader;
    std::unique_ptr<Nova::Shader> m_gridShader;
    Nova::Mesh* m_mesh = nullptr;

    // Camera matrices
    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projectionMatrix{1.0f};
    glm::mat4 m_viewProjectionMatrix{1.0f};

    // Mouse state
    bool m_isOrbiting = false;
    bool m_isPanning = false;
    glm::vec2 m_lastMousePos{0.0f};

    // Gizmo
    bool m_showGizmo = true;
    GizmoMode m_gizmoMode = GizmoMode::Rotate;
    bool m_isManipulatingGizmo = false;

    // Auto update
    bool m_autoUpdateFromTimeline = true;

    bool m_initialized = false;
};

} // namespace Vehement
