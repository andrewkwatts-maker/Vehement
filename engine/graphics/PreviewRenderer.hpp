#pragma once

#include "Framebuffer.hpp"
#include "Mesh.hpp"
#include "Material.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "../sdf/SDFModel.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <functional>
#include <optional>
#include <variant>

namespace Nova {

// =============================================================================
// Preview Mode Enumeration
// =============================================================================

/**
 * @brief Preview rendering modes
 *
 * Determines how the preview content is rendered and what type of content
 * is being displayed.
 */
enum class PreviewMode : uint8_t {
    Material,   ///< Material preview on standard geometry
    Mesh,       ///< Mesh preview with default/custom material
    SDF,        ///< SDF model preview with raymarching
    Texture,    ///< 2D texture preview
    Animation,  ///< Animated mesh/SDF preview
    Custom      ///< Custom rendering callback
};

// =============================================================================
// Preview Shape (for material previews)
// =============================================================================

/**
 * @brief Standard shapes for material/mesh preview
 */
enum class PreviewShape : uint8_t {
    Sphere,
    Cube,
    Plane,
    Cylinder,
    Torus,
    Custom      ///< Use custom mesh
};

// =============================================================================
// Preview Lighting Configuration
// =============================================================================

/**
 * @brief Light configuration for preview rendering
 */
struct PreviewLight {
    glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    bool enabled = true;
};

/**
 * @brief Environment configuration for preview
 */
struct PreviewEnvironment {
    glm::vec4 backgroundColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
    glm::vec4 ambientColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
    std::shared_ptr<Texture> environmentMap;    ///< Optional environment cubemap
    float environmentIntensity = 1.0f;
    bool showGrid = true;
    float gridSize = 1.0f;
    glm::vec4 gridColor = glm::vec4(0.3f, 0.3f, 0.3f, 1.0f);
};

// =============================================================================
// Preview Camera Configuration
// =============================================================================

/**
 * @brief Camera configuration for preview rendering
 */
struct PreviewCamera {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool orthographic = false;
    float orthoSize = 2.0f;

    /**
     * @brief Get view matrix
     */
    [[nodiscard]] glm::mat4 GetViewMatrix() const {
        return glm::lookAt(position, target, up);
    }

    /**
     * @brief Get projection matrix
     */
    [[nodiscard]] glm::mat4 GetProjectionMatrix(float aspectRatio) const {
        if (orthographic) {
            float halfSize = orthoSize * 0.5f;
            return glm::ortho(-halfSize * aspectRatio, halfSize * aspectRatio,
                            -halfSize, halfSize, nearPlane, farPlane);
        }
        return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    }
};

// =============================================================================
// Preview Interaction Settings
// =============================================================================

/**
 * @brief Interaction settings for preview
 */
struct PreviewInteraction {
    bool enableRotation = true;
    bool enableZoom = true;
    bool enablePan = true;
    bool autoRotate = false;
    float autoRotateSpeed = 1.0f;       ///< Radians per second
    float rotationSensitivity = 0.01f;
    float zoomSensitivity = 0.1f;
    float panSensitivity = 0.01f;
    float minDistance = 0.5f;
    float maxDistance = 20.0f;
};

// =============================================================================
// Preview Settings (Unified Configuration)
// =============================================================================

/**
 * @brief Complete preview settings structure
 *
 * This unified structure replaces the various settings found in
 * MaterialGraphPreviewRenderer, BuildingUIRenderer, and TemplateRenderer.
 */
struct PreviewSettings {
    PreviewMode mode = PreviewMode::Mesh;
    PreviewShape shape = PreviewShape::Sphere;
    PreviewCamera camera;
    PreviewLight mainLight;
    PreviewLight fillLight = {
        glm::normalize(glm::vec3(-1.0f, 0.5f, -0.5f)),
        glm::vec3(0.5f, 0.5f, 0.6f),
        0.3f,
        true
    };
    PreviewEnvironment environment;
    PreviewInteraction interaction;

    int thumbnailSize = 256;
    bool antialiasing = true;
    int msaaSamples = 4;
    bool hdr = false;
    float exposure = 1.0f;

    /**
     * @brief Create default settings for material preview
     */
    [[nodiscard]] static PreviewSettings MaterialPreview() {
        PreviewSettings settings;
        settings.mode = PreviewMode::Material;
        settings.shape = PreviewShape::Sphere;
        settings.camera.position = glm::vec3(0.0f, 0.0f, 2.5f);
        settings.environment.showGrid = false;
        return settings;
    }

    /**
     * @brief Create default settings for mesh preview
     */
    [[nodiscard]] static PreviewSettings MeshPreview() {
        PreviewSettings settings;
        settings.mode = PreviewMode::Mesh;
        settings.camera.position = glm::vec3(2.0f, 1.5f, 2.0f);
        settings.camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
        settings.environment.showGrid = true;
        return settings;
    }

    /**
     * @brief Create default settings for SDF preview
     */
    [[nodiscard]] static PreviewSettings SDFPreview() {
        PreviewSettings settings;
        settings.mode = PreviewMode::SDF;
        settings.camera.position = glm::vec3(0.0f, 0.0f, 3.0f);
        settings.environment.showGrid = true;
        settings.interaction.autoRotate = true;
        return settings;
    }

    /**
     * @brief Create default settings for texture preview
     */
    [[nodiscard]] static PreviewSettings TexturePreview() {
        PreviewSettings settings;
        settings.mode = PreviewMode::Texture;
        settings.camera.orthographic = true;
        settings.camera.orthoSize = 2.0f;
        settings.camera.position = glm::vec3(0.0f, 0.0f, 1.0f);
        settings.environment.showGrid = false;
        settings.environment.backgroundColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        settings.interaction.enableRotation = false;
        return settings;
    }
};

// =============================================================================
// Preview Input Event
// =============================================================================

/**
 * @brief Input event for preview interaction
 *
 * Simplified input event structure for preview-specific interactions.
 */
struct PreviewInputEvent {
    enum class Type : uint8_t {
        MouseDown,
        MouseUp,
        MouseMove,
        MouseDrag,
        Scroll,
        KeyDown,
        KeyUp
    };

    Type type = Type::MouseMove;
    glm::vec2 position{0.0f};       ///< Mouse position in preview coordinates
    glm::vec2 delta{0.0f};          ///< Mouse movement delta
    int button = 0;                  ///< Mouse button (0=left, 1=right, 2=middle)
    float scrollDelta = 0.0f;        ///< Scroll wheel delta
    int key = 0;                     ///< Key code
    bool shift = false;              ///< Shift modifier
    bool ctrl = false;               ///< Ctrl modifier
    bool alt = false;                ///< Alt modifier
};

// =============================================================================
// Custom Render Callback
// =============================================================================

/**
 * @brief Callback type for custom rendering
 */
using CustomRenderCallback = std::function<void(
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const glm::ivec2& size
)>;

// =============================================================================
// Preview Content (Variant for different content types)
// =============================================================================

/**
 * @brief Content that can be rendered in preview
 */
using PreviewContent = std::variant<
    std::shared_ptr<Mesh>,
    std::shared_ptr<Material>,
    std::shared_ptr<SDFModel>,
    std::shared_ptr<Texture>,
    CustomRenderCallback
>;

// =============================================================================
// PreviewRenderer Class
// =============================================================================

/**
 * @brief Unified preview renderer for materials, meshes, SDFs, and textures
 *
 * This class consolidates the preview rendering functionality previously
 * scattered across MaterialGraphPreviewRenderer, BuildingUIRenderer, and
 * TemplateRenderer into a single, flexible, and maintainable implementation.
 *
 * Features:
 * - Multiple preview modes (Material, Mesh, SDF, Texture, Animation, Custom)
 * - Configurable camera with orbit controls
 * - Multiple lighting options
 * - Grid and environment rendering
 * - Thumbnail generation
 * - Interactive rotation and zoom
 * - Auto-rotation support
 *
 * Usage Example:
 * @code
 * PreviewRenderer renderer;
 * renderer.Initialize();
 *
 * // Set up for material preview
 * renderer.SetSettings(PreviewSettings::MaterialPreview());
 * renderer.SetMaterial(myMaterial);
 *
 * // Render to screen
 * renderer.Render({512, 512});
 *
 * // Or generate thumbnail
 * auto thumbnail = renderer.RenderToTexture(256);
 * @endcode
 */
class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();

    // Non-copyable, movable
    PreviewRenderer(const PreviewRenderer&) = delete;
    PreviewRenderer& operator=(const PreviewRenderer&) = delete;
    PreviewRenderer(PreviewRenderer&&) noexcept;
    PreviewRenderer& operator=(PreviewRenderer&&) noexcept;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize renderer resources
     *
     * Must be called before any rendering operations.
     * Creates internal framebuffers, shaders, and primitive meshes.
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if renderer is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Set preview settings
     */
    void SetSettings(const PreviewSettings& settings);

    /**
     * @brief Get current settings
     */
    [[nodiscard]] const PreviewSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Get mutable settings for modification
     */
    [[nodiscard]] PreviewSettings& GetSettings() { return m_settings; }

    // =========================================================================
    // Content Setters
    // =========================================================================

    /**
     * @brief Set mesh to preview
     */
    void SetMesh(std::shared_ptr<Mesh> mesh);

    /**
     * @brief Set material to preview
     *
     * When mode is Material, this material is applied to the preview shape.
     * When mode is Mesh, this overrides the mesh's default material.
     */
    void SetMaterial(std::shared_ptr<Material> material);

    /**
     * @brief Set SDF model to preview
     */
    void SetSDF(std::shared_ptr<SDFModel> sdf);

    /**
     * @brief Set texture to preview (for Texture mode)
     */
    void SetTexture(std::shared_ptr<Texture> texture);

    /**
     * @brief Set custom render callback
     */
    void SetCustomRenderer(CustomRenderCallback callback);

    /**
     * @brief Clear all content
     */
    void ClearContent();

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render preview to current framebuffer
     *
     * @param size Viewport size in pixels
     */
    void Render(const glm::ivec2& size);

    /**
     * @brief Render preview to texture
     *
     * @param size Texture size (square)
     * @return Generated texture with preview rendering
     */
    [[nodiscard]] std::shared_ptr<Texture> RenderToTexture(int size);

    /**
     * @brief Render preview to texture with specific dimensions
     *
     * @param width Texture width
     * @param height Texture height
     * @return Generated texture with preview rendering
     */
    [[nodiscard]] std::shared_ptr<Texture> RenderToTexture(int width, int height);

    /**
     * @brief Get the internal framebuffer texture ID (for ImGui)
     *
     * @return OpenGL texture ID of the color attachment
     */
    [[nodiscard]] uint32_t GetPreviewTextureID() const;

    // =========================================================================
    // Interaction
    // =========================================================================

    /**
     * @brief Handle input event for preview interaction
     *
     * Processes mouse/keyboard events for camera orbit, zoom, and pan.
     *
     * @param event Input event to process
     * @return true if event was consumed
     */
    bool HandleInput(const PreviewInputEvent& event);

    /**
     * @brief Update preview state (call each frame)
     *
     * Handles auto-rotation and animation updates.
     *
     * @param deltaTime Time since last update in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Reset camera to default position
     */
    void ResetCamera();

    /**
     * @brief Focus camera on content bounds
     */
    void FocusOnContent();

    // =========================================================================
    // Camera Control
    // =========================================================================

    /**
     * @brief Orbit camera around target
     *
     * @param deltaYaw Horizontal rotation in radians
     * @param deltaPitch Vertical rotation in radians
     */
    void OrbitCamera(float deltaYaw, float deltaPitch);

    /**
     * @brief Zoom camera (move towards/away from target)
     *
     * @param delta Zoom amount (positive = closer)
     */
    void ZoomCamera(float delta);

    /**
     * @brief Pan camera (move target and camera together)
     *
     * @param delta Pan amount in screen space
     */
    void PanCamera(const glm::vec2& delta);

    /**
     * @brief Set camera distance from target
     *
     * @param distance Distance in world units
     */
    void SetCameraDistance(float distance);

    /**
     * @brief Get current camera distance from target
     */
    [[nodiscard]] float GetCameraDistance() const;

    // =========================================================================
    // Animation
    // =========================================================================

    /**
     * @brief Set animation time for animated previews
     *
     * @param time Animation time in seconds
     */
    void SetAnimationTime(float time) { m_animationTime = time; }

    /**
     * @brief Get current animation time
     */
    [[nodiscard]] float GetAnimationTime() const { return m_animationTime; }

    /**
     * @brief Set animation playback state
     */
    void SetAnimationPlaying(bool playing) { m_animationPlaying = playing; }

    /**
     * @brief Check if animation is playing
     */
    [[nodiscard]] bool IsAnimationPlaying() const { return m_animationPlaying; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Callback when preview needs update (content changed)
     */
    std::function<void()> OnPreviewUpdated;

private:
    // Internal rendering methods
    void RenderInternal(const glm::ivec2& size);
    void RenderMesh(const glm::mat4& view, const glm::mat4& projection);
    void RenderMaterial(const glm::mat4& view, const glm::mat4& projection);
    void RenderSDF(const glm::mat4& view, const glm::mat4& projection);
    void RenderTexture(const glm::mat4& view, const glm::mat4& projection);
    void RenderGrid(const glm::mat4& view, const glm::mat4& projection);
    void RenderBackground();

    // Shader management
    void CreateShaders();
    void CreatePrimitiveMeshes();

    // Framebuffer management
    void EnsureFramebuffer(int width, int height);

    // Camera helpers
    void UpdateCameraFromOrbit();
    [[nodiscard]] glm::vec3 GetContentCenter() const;
    [[nodiscard]] float GetContentRadius() const;

    // State
    bool m_initialized = false;
    PreviewSettings m_settings;

    // Content
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Material> m_material;
    std::shared_ptr<SDFModel> m_sdfModel;
    std::shared_ptr<Texture> m_texture;
    CustomRenderCallback m_customRenderer;

    // Primitive meshes for shape preview
    std::unique_ptr<Mesh> m_sphereMesh;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::unique_ptr<Mesh> m_planeMesh;
    std::unique_ptr<Mesh> m_cylinderMesh;
    std::unique_ptr<Mesh> m_torusMesh;
    std::unique_ptr<Mesh> m_gridMesh;
    std::unique_ptr<Mesh> m_quadMesh;  // For texture preview

    // Shaders
    std::unique_ptr<Shader> m_pbrShader;
    std::unique_ptr<Shader> m_gridShader;
    std::unique_ptr<Shader> m_textureShader;
    std::unique_ptr<Shader> m_sdfShader;

    // Framebuffer
    std::unique_ptr<Framebuffer> m_framebuffer;
    int m_framebufferWidth = 0;
    int m_framebufferHeight = 0;

    // Default material for preview
    std::shared_ptr<Material> m_defaultMaterial;

    // Camera orbit state
    float m_orbitYaw = 0.0f;
    float m_orbitPitch = 0.3f;
    float m_orbitDistance = 3.0f;

    // Interaction state
    bool m_isDragging = false;
    int m_dragButton = 0;
    glm::vec2 m_lastMousePos{0.0f};

    // Animation state
    float m_animationTime = 0.0f;
    bool m_animationPlaying = false;
    float m_autoRotateAngle = 0.0f;
};

// =============================================================================
// Backward Compatibility Wrappers
// =============================================================================

/**
 * @brief Legacy wrapper for MaterialGraphPreviewRenderer
 *
 * Provides backward compatibility for code using the old MaterialGraphPreviewRenderer
 * interface. New code should use PreviewRenderer directly.
 *
 * @deprecated Use PreviewRenderer instead
 */
class [[deprecated("Use PreviewRenderer instead")]] MaterialGraphPreviewRendererWrapper {
public:
    MaterialGraphPreviewRendererWrapper();
    ~MaterialGraphPreviewRendererWrapper();

    void Initialize();
    void Render(std::shared_ptr<class MaterialGraph> graph);
    void Resize(int width, int height);

    // Legacy settings
    enum class PreviewShape {
        Sphere,
        Cube,
        Plane,
        Cylinder,
        Torus
    };

    PreviewShape previewShape = PreviewShape::Sphere;
    float lightIntensity = 1.0f;
    glm::vec3 lightColor{1.0f};
    float rotation = 0.0f;
    bool autoRotate = true;

    [[nodiscard]] unsigned int GetPreviewTexture() const;

private:
    std::unique_ptr<PreviewRenderer> m_renderer;
    int m_width = 512;
    int m_height = 512;
};

/**
 * @brief Legacy wrapper for BuildingUIRenderer preview functionality
 *
 * @deprecated Use PreviewRenderer instead
 */
class [[deprecated("Use PreviewRenderer instead")]] BuildingPreviewRendererWrapper {
public:
    BuildingPreviewRendererWrapper();
    ~BuildingPreviewRendererWrapper();

    void Initialize();
    void RenderBuildingPreview(std::shared_ptr<Mesh> buildingMesh,
                                std::shared_ptr<Material> material);
    void Resize(int width, int height);

    [[nodiscard]] unsigned int GetPreviewTexture() const;

private:
    std::unique_ptr<PreviewRenderer> m_renderer;
    int m_width = 256;
    int m_height = 256;
};

/**
 * @brief Legacy wrapper for TemplateRenderer preview functionality
 *
 * @deprecated Use PreviewRenderer instead
 */
class [[deprecated("Use PreviewRenderer instead")]] TemplatePreviewRendererWrapper {
public:
    TemplatePreviewRendererWrapper();
    ~TemplatePreviewRendererWrapper();

    void Initialize();
    void RenderPreview(std::shared_ptr<Texture> texture);
    void Resize(int width, int height);

    [[nodiscard]] unsigned int GetPreviewTexture() const;

private:
    std::unique_ptr<PreviewRenderer> m_renderer;
    int m_width = 256;
    int m_height = 256;
};

} // namespace Nova
