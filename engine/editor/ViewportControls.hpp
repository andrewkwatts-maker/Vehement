/**
 * @file ViewportControls.hpp
 * @brief Comprehensive viewport navigation and control system for the Nova3D/Vehement editor
 *
 * Provides professional-grade viewport navigation with multiple camera modes,
 * overlay rendering, grid display, camera bookmarks, and orientation gizmo.
 *
 * Navigation Controls:
 * - Alt+LMB: Orbit around focus point
 * - Alt+MMB: Pan camera
 * - Alt+RMB: Dolly/zoom
 * - Mouse wheel: Zoom
 * - F: Frame selection
 * - Home: Reset view
 * - Numpad: Orthographic views (1=front, 3=right, 7=top)
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <cstdint>
#include <string>

namespace Nova {

// Forward declarations
class Camera;
class SceneNode;
class InputManager;
class Shader;
class Mesh;

/**
 * @brief Camera navigation mode
 */
enum class CameraMode {
    Orbit,          ///< Orbit around focus point (Maya-style)
    Fly,            ///< First-person fly camera (WASD)
    Pan,            ///< 2D pan/zoom (top-down)
    Turntable,      ///< Rotate around Y axis only
    Walkthrough     ///< Ground-constrained navigation
};

/**
 * @brief Viewport overlay flags for debug visualization
 */
enum class ViewportOverlay : uint32_t {
    None            = 0,
    Grid            = 1 << 0,       ///< World grid
    Gizmos          = 1 << 1,       ///< Transform gizmos
    Selection       = 1 << 2,       ///< Selection highlight
    Wireframe       = 1 << 3,       ///< Wireframe overlay
    Normals         = 1 << 4,       ///< Normal vectors
    BoundingBoxes   = 1 << 5,       ///< Object bounding boxes
    LightIcons      = 1 << 6,       ///< Light source icons
    CameraIcons     = 1 << 7,       ///< Camera icons
    SDFBounds       = 1 << 8,       ///< SDF field bounds
    Octree          = 1 << 9,       ///< Octree structure
    CollisionShapes = 1 << 10,      ///< Collision geometry

    // Common combinations
    Default         = Grid | Gizmos | Selection,
    All             = 0xFFFFFFFF
};

// Enable bitwise operations for ViewportOverlay
inline ViewportOverlay operator|(ViewportOverlay a, ViewportOverlay b) {
    return static_cast<ViewportOverlay>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ViewportOverlay operator&(ViewportOverlay a, ViewportOverlay b) {
    return static_cast<ViewportOverlay>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline ViewportOverlay operator~(ViewportOverlay a) {
    return static_cast<ViewportOverlay>(~static_cast<uint32_t>(a));
}

inline ViewportOverlay& operator|=(ViewportOverlay& a, ViewportOverlay b) {
    return a = a | b;
}

inline ViewportOverlay& operator&=(ViewportOverlay& a, ViewportOverlay b) {
    return a = a & b;
}

inline bool HasOverlay(ViewportOverlay flags, ViewportOverlay check) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(check)) != 0;
}

/**
 * @brief Render visualization mode
 */
enum class RenderMode {
    Shaded,             ///< Full shading with lighting
    Unlit,              ///< No lighting applied
    Wireframe,          ///< Wireframe only
    ShadedWireframe,    ///< Shaded with wireframe overlay
    SDFDistance,        ///< Visualize SDF distance field
    Normals,            ///< Normal visualization (RGB = XYZ)
    UVs,                ///< UV coordinate visualization
    Overdraw,           ///< Overdraw heat map
    LODColors           ///< LOD level color coding
};

/**
 * @brief Preset orthographic view directions
 */
enum class OrthoView {
    Front,              ///< Front view (-Z)
    Back,               ///< Back view (+Z)
    Left,               ///< Left view (-X)
    Right,              ///< Right view (+X)
    Top,                ///< Top view (+Y looking down)
    Bottom,             ///< Bottom view (-Y looking up)
    Perspective         ///< Return to perspective view
};

/**
 * @brief Viewport display and control settings
 */
struct ViewportSettings {
    CameraMode mode = CameraMode::Orbit;
    ViewportOverlay overlays = ViewportOverlay::Default;
    RenderMode renderMode = RenderMode::Shaded;

    // Camera projection settings
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;
    float fieldOfView = 45.0f;
    float orthoSize = 10.0f;        ///< Orthographic viewport size

    // Rendering toggles
    bool enablePostProcessing = true;
    bool enableShadows = true;
    bool enableGI = true;
    bool enableAA = true;
    bool enableSSAO = true;

    // Navigation settings
    float orbitSpeed = 0.3f;        ///< Degrees per pixel
    float panSpeed = 0.01f;         ///< Units per pixel
    float zoomSpeed = 0.1f;         ///< Multiplier per scroll notch
    float flySpeed = 10.0f;         ///< Units per second
    float flySprintMultiplier = 2.5f;
    float minZoomDistance = 0.1f;
    float maxZoomDistance = 10000.0f;

    // Walkthrough settings
    float groundHeight = 0.0f;      ///< Y level for ground constraint
    float eyeHeight = 1.7f;         ///< Height above ground

    // Grid settings
    float gridSize = 1.0f;          ///< Primary grid cell size
    int gridSubdivisions = 10;      ///< Subdivisions per cell
    float gridExtent = 100.0f;      ///< Grid visible range
    glm::vec4 gridColor{0.4f, 0.4f, 0.4f, 0.5f};
    glm::vec4 gridSubdivColor{0.3f, 0.3f, 0.3f, 0.25f};
    glm::vec4 gridAxisXColor{0.8f, 0.2f, 0.2f, 0.8f};  ///< X axis (red)
    glm::vec4 gridAxisYColor{0.2f, 0.8f, 0.2f, 0.8f};  ///< Y axis (green)
    glm::vec4 gridAxisZColor{0.2f, 0.2f, 0.8f, 0.8f};  ///< Z axis (blue)

    // Smooth motion settings
    bool enableSmoothOrbit = true;
    bool enableSmoothZoom = true;
    float smoothingFactor = 10.0f;  ///< Smoothing responsiveness
};

/**
 * @brief Camera bookmark for saving/restoring camera positions
 */
struct CameraBookmark {
    glm::vec3 position{0.0f};
    glm::vec3 focusPoint{0.0f};
    float pitch = 0.0f;
    float yaw = -90.0f;
    float distance = 5.0f;
    bool isOrthographic = false;
    float orthoSize = 10.0f;
    std::string name;
    bool isValid = false;
};

/**
 * @brief Result of orientation gizmo interaction
 */
struct OrientationGizmoResult {
    bool wasClicked = false;        ///< True if gizmo was clicked
    OrthoView clickedFace = OrthoView::Perspective; ///< Which face was clicked
    bool isHovered = false;         ///< True if hovering over gizmo
};

/**
 * @brief Viewport Controls - Professional 3D viewport navigation system
 *
 * Features:
 * - Multiple camera modes (Orbit, Fly, Pan, Turntable, Walkthrough)
 * - Maya-style Alt+mouse navigation
 * - Smooth camera interpolation
 * - Frame selection (F key)
 * - Numpad orthographic views
 * - Camera bookmarks (save/restore)
 * - Infinite adaptive grid
 * - Orientation gizmo (view cube)
 * - Configurable overlays and render modes
 *
 * Usage:
 *   ViewportControls controls;
 *   controls.Initialize();
 *   controls.Attach(&camera);
 *
 *   // In update loop:
 *   controls.Update(deltaTime, input, screenSize);
 *
 *   // Frame selected objects:
 *   controls.FrameSelection(selectionBounds);
 *
 *   // Render viewport overlays:
 *   controls.RenderGrid(camera);
 *   controls.RenderOrientationGizmo(camera, screenSize);
 */
class ViewportControls {
public:
    ViewportControls();
    ~ViewportControls();

    // Non-copyable
    ViewportControls(const ViewportControls&) = delete;
    ViewportControls& operator=(const ViewportControls&) = delete;
    ViewportControls(ViewportControls&&) noexcept;
    ViewportControls& operator=(ViewportControls&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize OpenGL resources for rendering
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if controls are initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Camera Attachment
    // =========================================================================

    /**
     * @brief Attach to a camera to control
     * @param camera Camera to control (must outlive ViewportControls)
     */
    void Attach(Camera* camera);

    /**
     * @brief Detach from current camera
     */
    void Detach();

    /**
     * @brief Get attached camera
     */
    [[nodiscard]] Camera* GetCamera() const { return m_camera; }

    /**
     * @brief Check if a camera is attached
     */
    [[nodiscard]] bool HasCamera() const { return m_camera != nullptr; }

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update camera based on input
     * @param deltaTime Time since last update in seconds
     * @param input Input manager for mouse/keyboard state
     * @param screenSize Current viewport dimensions
     */
    void Update(float deltaTime, const InputManager& input, const glm::vec2& screenSize);

    /**
     * @brief Update with manual mouse state
     * @param deltaTime Time since last update
     * @param mousePos Current mouse position
     * @param mouseDelta Mouse movement since last frame
     * @param scrollDelta Mouse wheel delta
     * @param lmbDown Left mouse button state
     * @param mmbDown Middle mouse button state
     * @param rmbDown Right mouse button state
     * @param altDown Alt key state
     * @param shiftDown Shift key state
     * @param ctrlDown Control key state
     * @param screenSize Viewport dimensions
     */
    void Update(float deltaTime,
                const glm::vec2& mousePos,
                const glm::vec2& mouseDelta,
                float scrollDelta,
                bool lmbDown, bool mmbDown, bool rmbDown,
                bool altDown, bool shiftDown, bool ctrlDown,
                const glm::vec2& screenSize);

    // =========================================================================
    // Camera Mode
    // =========================================================================

    /**
     * @brief Set camera navigation mode
     */
    void SetMode(CameraMode mode);

    /**
     * @brief Get current camera mode
     */
    [[nodiscard]] CameraMode GetMode() const { return m_settings.mode; }

    /**
     * @brief Cycle to next camera mode
     */
    void CycleMode();

    // =========================================================================
    // Focus and Framing
    // =========================================================================

    /**
     * @brief Set the orbit focus point
     * @param point World-space position to orbit around
     */
    void SetFocusPoint(const glm::vec3& point);

    /**
     * @brief Get current focus point
     */
    [[nodiscard]] const glm::vec3& GetFocusPoint() const { return m_focusPoint; }

    /**
     * @brief Frame the camera to view given bounds
     * @param bounds Vector of world-space positions defining the bounding region
     * @param padding Additional padding multiplier (1.0 = tight fit)
     */
    void FrameSelection(const std::vector<glm::vec3>& bounds, float padding = 1.2f);

    /**
     * @brief Frame camera on axis-aligned bounding box
     * @param minBounds AABB minimum corner
     * @param maxBounds AABB maximum corner
     * @param padding Additional padding multiplier
     */
    void FrameBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds, float padding = 1.2f);

    /**
     * @brief Focus camera on a scene node
     * @param node Scene node to focus on
     * @param frameSelection If true, adjust distance to frame the object
     */
    void FocusOnObject(SceneNode* node, bool frameSelection = true);

    /**
     * @brief Reset camera to default view
     */
    void ResetView();

    /**
     * @brief Set the default view parameters
     * @param position Default camera position
     * @param target Default look-at target
     */
    void SetDefaultView(const glm::vec3& position, const glm::vec3& target);

    // =========================================================================
    // Orthographic Views
    // =========================================================================

    /**
     * @brief Switch to orthographic view
     * @param view The orthographic view direction
     * @param animate If true, smoothly transition to the view
     */
    void SetOrthoView(OrthoView view, bool animate = true);

    /**
     * @brief Get current orthographic view (or Perspective if in perspective)
     */
    [[nodiscard]] OrthoView GetCurrentOrthoView() const { return m_currentOrthoView; }

    /**
     * @brief Check if currently in orthographic mode
     */
    [[nodiscard]] bool IsOrthographic() const { return m_isOrthographic; }

    /**
     * @brief Toggle between perspective and last orthographic view
     */
    void TogglePerspective();

    // =========================================================================
    // Camera Bookmarks
    // =========================================================================

    /**
     * @brief Save current camera state to a bookmark slot
     * @param slot Bookmark slot (0-9)
     * @param name Optional name for the bookmark
     */
    void SaveBookmark(int slot, const std::string& name = "");

    /**
     * @brief Restore camera state from a bookmark slot
     * @param slot Bookmark slot (0-9)
     * @param animate If true, smoothly transition to the bookmark
     * @return true if bookmark was valid and restored
     */
    bool RestoreBookmark(int slot, bool animate = true);

    /**
     * @brief Clear a bookmark slot
     * @param slot Bookmark slot to clear
     */
    void ClearBookmark(int slot);

    /**
     * @brief Get bookmark data
     * @param slot Bookmark slot
     * @return Bookmark data (check isValid)
     */
    [[nodiscard]] const CameraBookmark& GetBookmark(int slot) const;

    /**
     * @brief Check if a bookmark slot is valid
     */
    [[nodiscard]] bool IsBookmarkValid(int slot) const;

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Get viewport settings
     */
    [[nodiscard]] ViewportSettings& GetSettings() { return m_settings; }
    [[nodiscard]] const ViewportSettings& GetSettings() const { return m_settings; }

    /**
     * @brief Set viewport settings
     */
    void SetSettings(const ViewportSettings& settings) { m_settings = settings; }

    /**
     * @brief Set enabled overlays
     */
    void SetOverlays(ViewportOverlay overlays) { m_settings.overlays = overlays; }

    /**
     * @brief Toggle an overlay
     */
    void ToggleOverlay(ViewportOverlay overlay);

    /**
     * @brief Check if overlay is enabled
     */
    [[nodiscard]] bool IsOverlayEnabled(ViewportOverlay overlay) const;

    /**
     * @brief Set render mode
     */
    void SetRenderMode(RenderMode mode) { m_settings.renderMode = mode; }

    /**
     * @brief Get render mode
     */
    [[nodiscard]] RenderMode GetRenderMode() const { return m_settings.renderMode; }

    /**
     * @brief Cycle to next render mode
     */
    void CycleRenderMode();

    // =========================================================================
    // Navigation State
    // =========================================================================

    /**
     * @brief Check if currently navigating (any mouse drag)
     */
    [[nodiscard]] bool IsNavigating() const { return m_isNavigating; }

    /**
     * @brief Check if currently orbiting
     */
    [[nodiscard]] bool IsOrbiting() const { return m_isOrbiting; }

    /**
     * @brief Check if currently panning
     */
    [[nodiscard]] bool IsPanning() const { return m_isPanning; }

    /**
     * @brief Check if currently zooming
     */
    [[nodiscard]] bool IsZooming() const { return m_isZooming; }

    /**
     * @brief Get orbit distance from focus point
     */
    [[nodiscard]] float GetOrbitDistance() const { return m_orbitDistance; }

    /**
     * @brief Set orbit distance
     */
    void SetOrbitDistance(float distance);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render the infinite world grid
     * @param camera Camera for view/projection matrices
     */
    void RenderGrid(const Camera& camera);

    /**
     * @brief Render grid with explicit matrices
     * @param view View matrix
     * @param projection Projection matrix
     * @param cameraPosition Camera world position
     */
    void RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                    const glm::vec3& cameraPosition);

    /**
     * @brief Render the orientation gizmo (view cube)
     * @param camera Camera for current orientation
     * @param screenSize Viewport dimensions
     * @param mousePos Current mouse position (for hover detection)
     * @return Interaction result (hover state, clicked face)
     */
    OrientationGizmoResult RenderOrientationGizmo(const Camera& camera,
                                                   const glm::vec2& screenSize,
                                                   const glm::vec2& mousePos);

    /**
     * @brief Set orientation gizmo size
     * @param size Size in pixels
     */
    void SetOrientationGizmoSize(float size) { m_orientationGizmoSize = size; }

    /**
     * @brief Set orientation gizmo position
     * @param position Screen position for gizmo center (normalized 0-1)
     */
    void SetOrientationGizmoPosition(const glm::vec2& position) { m_orientationGizmoPosition = position; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using CameraModeChangedCallback = std::function<void(CameraMode newMode)>;
    using ViewChangedCallback = std::function<void()>;
    using OrthoViewChangedCallback = std::function<void(OrthoView view)>;

    void SetOnCameraModeChanged(CameraModeChangedCallback callback) {
        m_onCameraModeChanged = std::move(callback);
    }

    void SetOnViewChanged(ViewChangedCallback callback) {
        m_onViewChanged = std::move(callback);
    }

    void SetOnOrthoViewChanged(OrthoViewChangedCallback callback) {
        m_onOrthoViewChanged = std::move(callback);
    }

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get human-readable name for camera mode
     */
    [[nodiscard]] static const char* GetCameraModeName(CameraMode mode);

    /**
     * @brief Get human-readable name for render mode
     */
    [[nodiscard]] static const char* GetRenderModeName(RenderMode mode);

    /**
     * @brief Get human-readable name for ortho view
     */
    [[nodiscard]] static const char* GetOrthoViewName(OrthoView view);

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Input processing
    void ProcessOrbitMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                          bool lmbDown, bool mmbDown, bool rmbDown, bool altDown, bool shiftDown);
    void ProcessFlyMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                        bool lmbDown, bool rmbDown, bool shiftDown,
                        bool wDown, bool aDown, bool sDown, bool dDown,
                        bool qDown, bool eDown);
    void ProcessPanMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                        bool lmbDown, bool mmbDown);
    void ProcessTurntableMode(float deltaTime, const glm::vec2& mouseDelta, float scrollDelta,
                              bool lmbDown, bool mmbDown, bool rmbDown, bool altDown);
    void ProcessWalkthroughMode(float deltaTime, const glm::vec2& mouseDelta,
                                bool wDown, bool aDown, bool sDown, bool dDown, bool shiftDown);

    // Camera updates
    void UpdateOrbitCamera(float deltaTime);
    void ApplySmoothMotion(float deltaTime);
    void ApplyCameraTransform();
    void UpdateOrthoProjection();

    // Animation
    void StartCameraAnimation(const glm::vec3& targetPosition, const glm::vec3& targetFocus,
                              float targetPitch, float targetYaw, float duration = 0.3f);
    void UpdateCameraAnimation(float deltaTime);

    // Grid rendering
    bool InitializeGridResources();
    void DestroyGridResources();
    void GenerateInfiniteGrid(const glm::vec3& cameraPos);

    // Orientation gizmo rendering
    bool InitializeOrientationGizmoResources();
    void DestroyOrientationGizmoResources();
    void RenderOrientationGizmoFace(const glm::mat4& mvp, const glm::vec3& normal,
                                    const glm::vec4& color, bool highlighted);
    OrthoView HitTestOrientationGizmo(const glm::vec2& mousePos, const glm::vec2& screenSize,
                                       const glm::mat4& viewRotation);

    // Utility
    glm::vec3 GetOrthoViewDirection(OrthoView view) const;
    glm::vec3 GetOrthoViewUp(OrthoView view) const;
    float CalculateFrameDistance(const glm::vec3& boundsSize, float fov) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    // State
    bool m_initialized = false;
    Camera* m_camera = nullptr;
    ViewportSettings m_settings;

    // Orbit mode state
    glm::vec3 m_focusPoint{0.0f};
    float m_orbitDistance = 5.0f;
    float m_orbitPitch = 0.0f;      // Degrees
    float m_orbitYaw = -90.0f;      // Degrees

    // Smooth motion targets
    glm::vec3 m_targetFocusPoint{0.0f};
    float m_targetOrbitDistance = 5.0f;
    float m_targetOrbitPitch = 0.0f;
    float m_targetOrbitYaw = -90.0f;

    // Navigation state
    bool m_isNavigating = false;
    bool m_isOrbiting = false;
    bool m_isPanning = false;
    bool m_isZooming = false;

    // Orthographic state
    bool m_isOrthographic = false;
    OrthoView m_currentOrthoView = OrthoView::Perspective;
    OrthoView m_lastOrthoView = OrthoView::Front;

    // Animation state
    bool m_isAnimating = false;
    float m_animationTime = 0.0f;
    float m_animationDuration = 0.3f;
    glm::vec3 m_animStartPosition{0.0f};
    glm::vec3 m_animStartFocus{0.0f};
    float m_animStartPitch = 0.0f;
    float m_animStartYaw = 0.0f;
    float m_animStartDistance = 0.0f;
    glm::vec3 m_animTargetPosition{0.0f};
    glm::vec3 m_animTargetFocus{0.0f};
    float m_animTargetPitch = 0.0f;
    float m_animTargetYaw = 0.0f;
    float m_animTargetDistance = 0.0f;
    bool m_animTargetOrtho = false;
    float m_animTargetOrthoSize = 10.0f;

    // Default view
    glm::vec3 m_defaultPosition{0.0f, 5.0f, 10.0f};
    glm::vec3 m_defaultTarget{0.0f, 0.0f, 0.0f};

    // Bookmarks
    static constexpr int MAX_BOOKMARKS = 10;
    std::array<CameraBookmark, MAX_BOOKMARKS> m_bookmarks;

    // Fly mode state
    glm::vec3 m_flyVelocity{0.0f};

    // Orientation gizmo
    float m_orientationGizmoSize = 100.0f;
    glm::vec2 m_orientationGizmoPosition{0.92f, 0.12f}; // Top-right corner

    // OpenGL resources - Grid
    uint32_t m_gridVAO = 0;
    uint32_t m_gridVBO = 0;
    size_t m_gridVertexCount = 0;
    std::unique_ptr<Shader> m_gridShader;
    static constexpr size_t MAX_GRID_VERTICES = 16384;

    // OpenGL resources - Orientation gizmo
    uint32_t m_gizmoVAO = 0;
    uint32_t m_gizmoVBO = 0;
    uint32_t m_gizmoEBO = 0;
    std::unique_ptr<Shader> m_gizmoShader;
    std::unique_ptr<Mesh> m_gizmoCubeMesh;
    std::unique_ptr<Mesh> m_gizmoConeMesh;

    // Callbacks
    CameraModeChangedCallback m_onCameraModeChanged;
    ViewChangedCallback m_onViewChanged;
    OrthoViewChangedCallback m_onOrthoViewChanged;
};

/**
 * @brief Get the string name for a CameraMode
 */
inline const char* CameraModeToString(CameraMode mode) {
    return ViewportControls::GetCameraModeName(mode);
}

/**
 * @brief Get the string name for a RenderMode
 */
inline const char* RenderModeToString(RenderMode mode) {
    return ViewportControls::GetRenderModeName(mode);
}

/**
 * @brief Get the string name for an OrthoView
 */
inline const char* OrthoViewToString(OrthoView view) {
    return ViewportControls::GetOrthoViewName(view);
}

} // namespace Nova
