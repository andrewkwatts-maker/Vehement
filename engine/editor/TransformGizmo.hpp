/**
 * @file TransformGizmo.hpp
 * @brief Transform manipulation gizmo for the Nova3D/Vehement editor
 *
 * Provides interactive 3D gizmos for translating, rotating, and scaling
 * objects in the scene. Supports axis/plane handles, snapping, and
 * screen-space sizing for consistent visibility.
 */

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace Nova {

// Forward declarations
class Camera;
class Shader;
class Mesh;
class InputManager;

/**
 * @brief Gizmo operation mode
 */
enum class GizmoMode {
    Translate,   ///< Move objects along axes/planes
    Rotate,      ///< Rotate objects around axes
    Scale        ///< Scale objects along axes/uniformly
};

/**
 * @brief Coordinate space for gizmo operations
 */
enum class GizmoSpace {
    World,       ///< Use world axes
    Local        ///< Use object's local axes
};

/**
 * @brief Individual axis or plane identifier
 */
enum class GizmoAxis : uint8_t {
    None = 0,
    X = 1,         ///< X axis (red)
    Y = 2,         ///< Y axis (green)
    Z = 4,         ///< Z axis (blue)
    XY = 3,        ///< XY plane handle
    XZ = 5,        ///< XZ plane handle
    YZ = 6,        ///< YZ plane handle
    XYZ = 7,       ///< Uniform scale / screen-aligned
    View = 8       ///< View-aligned rotation (rotate mode only)
};

/**
 * @brief Result of a gizmo interaction
 */
struct GizmoResult {
    bool isActive = false;          ///< True if gizmo is being manipulated
    bool wasModified = false;       ///< True if transform changed this frame
    glm::vec3 translationDelta{0};  ///< Translation change (translate mode)
    glm::quat rotationDelta{1, 0, 0, 0}; ///< Rotation change (rotate mode)
    glm::vec3 scaleDelta{1};        ///< Scale multiplier (scale mode)
    GizmoAxis activeAxis = GizmoAxis::None; ///< Currently active axis/plane
};

/**
 * @brief Snapping configuration
 */
struct GizmoSnapping {
    bool enabled = false;
    float translateSnap = 1.0f;     ///< Grid snap distance
    float rotateSnap = 15.0f;       ///< Angle snap in degrees
    float scaleSnap = 0.1f;         ///< Scale snap increment
};

/**
 * @brief Individual handle component of a gizmo
 */
struct GizmoHandle {
    GizmoAxis axis = GizmoAxis::None;
    glm::vec3 direction{0};         ///< Direction for axis handles
    glm::vec4 color{1};             ///< Base color
    glm::vec4 highlightColor{1};    ///< Color when hovered/active
    float hitRadius = 0.0f;         ///< Radius for hit testing
    bool isHovered = false;
    bool isActive = false;
};

/**
 * @brief Transform manipulation gizmo for 3D editing
 *
 * Provides visual handles for translating, rotating, and scaling objects.
 * Features:
 * - Three operation modes: Translate, Rotate, Scale
 * - Axis handles (X=red, Y=green, Z=blue)
 * - Plane handles for 2-axis movement (translate mode)
 * - Center cube for uniform scaling (scale mode)
 * - View-aligned rotation ring (rotate mode)
 * - Screen-space sizing for consistent visibility
 * - Highlight on hover
 * - Configurable snapping
 *
 * Usage:
 *   gizmo.SetTransform(selectedObject->GetWorldTransform());
 *   gizmo.SetMode(GizmoMode::Translate);
 *   GizmoResult result = gizmo.Update(camera, input, screenSize);
 *   if (result.wasModified) {
 *       selectedObject->Translate(result.translationDelta);
 *   }
 *   gizmo.Render(camera);
 */
class TransformGizmo {
public:
    TransformGizmo();
    ~TransformGizmo();

    // Non-copyable
    TransformGizmo(const TransformGizmo&) = delete;
    TransformGizmo& operator=(const TransformGizmo&) = delete;
    TransformGizmo(TransformGizmo&&) noexcept;
    TransformGizmo& operator=(TransformGizmo&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize OpenGL resources
     * @return true if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Cleanup OpenGL resources
     */
    void Shutdown();

    /**
     * @brief Check if gizmo is initialized
     */
    [[nodiscard]] bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the gizmo operation mode
     */
    void SetMode(GizmoMode mode) { m_mode = mode; }

    /**
     * @brief Get the current operation mode
     */
    [[nodiscard]] GizmoMode GetMode() const { return m_mode; }

    /**
     * @brief Set the coordinate space
     */
    void SetSpace(GizmoSpace space) { m_space = space; }

    /**
     * @brief Get the current coordinate space
     */
    [[nodiscard]] GizmoSpace GetSpace() const { return m_space; }

    /**
     * @brief Toggle between world and local space
     */
    void ToggleSpace() {
        m_space = (m_space == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World;
    }

    /**
     * @brief Set gizmo visibility
     */
    void SetVisible(bool visible) { m_visible = visible; }

    /**
     * @brief Check if gizmo is visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    /**
     * @brief Enable or disable the gizmo
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

    /**
     * @brief Check if gizmo is enabled
     */
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Configure snapping behavior
     */
    void SetSnapping(const GizmoSnapping& snapping) { m_snapping = snapping; }

    /**
     * @brief Get current snapping configuration
     */
    [[nodiscard]] const GizmoSnapping& GetSnapping() const { return m_snapping; }

    /**
     * @brief Set snap values directly
     */
    void SetSnapValues(float translate, float rotate, float scale);

    /**
     * @brief Enable or disable snapping
     */
    void SetSnapEnabled(bool enabled) { m_snapping.enabled = enabled; }

    /**
     * @brief Set the screen-space size of the gizmo in pixels
     */
    void SetScreenSize(float size) { m_screenSize = size; }

    /**
     * @brief Get the screen-space size
     */
    [[nodiscard]] float GetScreenSize() const { return m_screenSize; }

    /**
     * @brief Set the base scale multiplier
     */
    void SetScale(float scale) { m_baseScale = scale; }

    /**
     * @brief Get the base scale multiplier
     */
    [[nodiscard]] float GetScale() const { return m_baseScale; }

    // =========================================================================
    // Transform Management
    // =========================================================================

    /**
     * @brief Set the transform to manipulate
     * @param position World position
     * @param rotation World rotation (optional, for local space)
     */
    void SetTransform(const glm::vec3& position, const glm::quat& rotation = glm::quat(1, 0, 0, 0));

    /**
     * @brief Set transform from 4x4 matrix
     */
    void SetTransform(const glm::mat4& transform);

    /**
     * @brief Get the current gizmo position
     */
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }

    /**
     * @brief Get the current gizmo rotation
     */
    [[nodiscard]] const glm::quat& GetRotation() const { return m_rotation; }

    /**
     * @brief Get the transform matrix
     */
    [[nodiscard]] glm::mat4 GetTransformMatrix() const;

    // =========================================================================
    // Interaction
    // =========================================================================

    /**
     * @brief Update gizmo state and handle input
     * @param camera Active camera for view/projection
     * @param input Input manager for mouse state
     * @param screenSize Current viewport dimensions
     * @return Result containing any transform changes
     */
    GizmoResult Update(const Camera& camera, const InputManager& input,
                       const glm::vec2& screenSize);

    /**
     * @brief Alternative update using raw mouse state
     * @param camera Active camera
     * @param mousePos Mouse position in pixels
     * @param mouseDown Left mouse button state
     * @param screenSize Viewport dimensions
     * @return Result containing any transform changes
     */
    GizmoResult Update(const Camera& camera, const glm::vec2& mousePos,
                       bool mouseDown, const glm::vec2& screenSize);

    /**
     * @brief Check if gizmo is currently being manipulated
     */
    [[nodiscard]] bool IsActive() const { return m_isActive; }

    /**
     * @brief Check if mouse is hovering over any handle
     */
    [[nodiscard]] bool IsHovered() const { return m_hoveredAxis != GizmoAxis::None; }

    /**
     * @brief Get the currently hovered axis
     */
    [[nodiscard]] GizmoAxis GetHoveredAxis() const { return m_hoveredAxis; }

    /**
     * @brief Get the currently active axis (during manipulation)
     */
    [[nodiscard]] GizmoAxis GetActiveAxis() const { return m_activeAxis; }

    /**
     * @brief Cancel current manipulation and reset to initial state
     */
    void CancelManipulation();

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Render the gizmo
     * @param camera Active camera for view/projection matrices
     */
    void Render(const Camera& camera);

    /**
     * @brief Render the gizmo with explicit matrices
     * @param view View matrix
     * @param projection Projection matrix
     * @param cameraPosition Camera world position for screen-space scaling
     */
    void Render(const glm::mat4& view, const glm::mat4& projection,
                const glm::vec3& cameraPosition);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using TransformCallback = std::function<void(const glm::vec3&, const glm::quat&, const glm::vec3&)>;

    /**
     * @brief Set callback for transform changes
     * @param callback Function receiving (translation, rotation, scale) deltas
     */
    void SetOnTransformChanged(TransformCallback callback) {
        m_onTransformChanged = std::move(callback);
    }

    // =========================================================================
    // Colors
    // =========================================================================

    /**
     * @brief Set custom axis colors
     */
    void SetAxisColors(const glm::vec4& xColor, const glm::vec4& yColor, const glm::vec4& zColor);

    /**
     * @brief Set highlight color intensity multiplier
     */
    void SetHighlightIntensity(float intensity) { m_highlightIntensity = intensity; }

    /**
     * @brief Set inactive (non-hovered) alpha value
     */
    void SetInactiveAlpha(float alpha) { m_inactiveAlpha = alpha; }

private:
    // =========================================================================
    // Internal Methods
    // =========================================================================

    // Shader initialization
    bool CreateShaders();
    void DestroyShaders();

    // Mesh generation
    bool CreateMeshes();
    void DestroyMeshes();
    void CreateTranslateMeshes();
    void CreateRotateMeshes();
    void CreateScaleMeshes();

    // Hit testing
    GizmoAxis HitTest(const Camera& camera, const glm::vec2& mousePos,
                      const glm::vec2& screenSize);
    bool RayAxisTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                     const glm::vec3& axisOrigin, const glm::vec3& axisDir,
                     float length, float radius, float& outDistance);
    bool RayPlaneTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                      const glm::vec3& planeOrigin, const glm::vec3& planeNormal,
                      float& outDistance, glm::vec3& outHitPoint);
    bool RayTorusTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                      const glm::vec3& center, const glm::vec3& normal,
                      float majorRadius, float minorRadius, float& outDistance);
    bool RaySphereTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                       const glm::vec3& center, float radius, float& outDistance);

    // Manipulation
    void BeginManipulation(const Camera& camera, const glm::vec2& mousePos,
                           const glm::vec2& screenSize);
    GizmoResult ContinueManipulation(const Camera& camera, const glm::vec2& mousePos,
                                     const glm::vec2& screenSize);
    void EndManipulation();

    // Translation handling
    glm::vec3 ComputeTranslation(const Camera& camera, const glm::vec2& mousePos,
                                 const glm::vec2& screenSize);

    // Rotation handling
    glm::quat ComputeRotation(const Camera& camera, const glm::vec2& mousePos,
                              const glm::vec2& screenSize);

    // Scale handling
    glm::vec3 ComputeScale(const Camera& camera, const glm::vec2& mousePos,
                           const glm::vec2& screenSize);

    // Snapping
    float ApplySnap(float value, float snapInterval);
    glm::vec3 ApplyTranslationSnap(const glm::vec3& translation);
    float ApplyRotationSnap(float angleDegrees);
    glm::vec3 ApplyScaleSnap(const glm::vec3& scale);

    // Rendering helpers
    void RenderTranslateGizmo(const glm::mat4& view, const glm::mat4& projection, float scale);
    void RenderRotateGizmo(const glm::mat4& view, const glm::mat4& projection, float scale);
    void RenderScaleGizmo(const glm::mat4& view, const glm::mat4& projection, float scale);
    void RenderAxis(const glm::mat4& mvp, const glm::vec3& direction, const glm::vec4& color,
                    float length, bool highlighted);

    glm::vec4 GetAxisColor(GizmoAxis axis, bool highlighted, bool active) const;
    float ComputeScreenScale(const Camera& camera) const;
    glm::mat4 GetGizmoOrientation() const;

    // Ray casting
    glm::vec3 ScreenToWorldRay(const Camera& camera, const glm::vec2& screenPos,
                               const glm::vec2& screenSize) const;

    // =========================================================================
    // Member Variables
    // =========================================================================

    // State
    bool m_initialized = false;
    bool m_visible = true;
    bool m_enabled = true;
    GizmoMode m_mode = GizmoMode::Translate;
    GizmoSpace m_space = GizmoSpace::World;

    // Transform
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_scale{1.0f};

    // Interaction state
    bool m_isActive = false;
    GizmoAxis m_hoveredAxis = GizmoAxis::None;
    GizmoAxis m_activeAxis = GizmoAxis::None;

    // Manipulation state (during drag)
    glm::vec2 m_dragStartMouse{0.0f};
    glm::vec3 m_dragStartPosition{0.0f};
    glm::quat m_dragStartRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_dragStartScale{1.0f};
    glm::vec3 m_dragPlaneNormal{0.0f, 1.0f, 0.0f};
    glm::vec3 m_dragStartHitPoint{0.0f};
    float m_dragStartAngle = 0.0f;
    float m_accumulatedRotation = 0.0f;
    glm::vec3 m_lastTranslation{0.0f};
    glm::quat m_lastRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_lastScale{1.0f};

    // Configuration
    GizmoSnapping m_snapping;
    float m_screenSize = 100.0f;     // Target screen size in pixels
    float m_baseScale = 1.0f;
    float m_handleLength = 1.0f;
    float m_handleRadius = 0.08f;
    float m_planeSize = 0.25f;
    float m_rotateRadius = 0.9f;
    float m_scaleBoxSize = 0.1f;

    // Colors
    glm::vec4 m_xAxisColor{0.95f, 0.25f, 0.25f, 1.0f};  // Red
    glm::vec4 m_yAxisColor{0.25f, 0.95f, 0.25f, 1.0f};  // Green
    glm::vec4 m_zAxisColor{0.25f, 0.25f, 0.95f, 1.0f};  // Blue
    glm::vec4 m_viewAxisColor{0.9f, 0.9f, 0.9f, 0.8f};  // White (view-aligned)
    glm::vec4 m_centerColor{0.9f, 0.9f, 0.9f, 1.0f};    // Center cube/sphere
    float m_highlightIntensity = 1.4f;
    float m_inactiveAlpha = 0.6f;

    // OpenGL resources
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_lineShader;

    // Meshes for different gizmo parts
    std::unique_ptr<Mesh> m_arrowMesh;       // Translate arrow
    std::unique_ptr<Mesh> m_planeMesh;       // Translate plane
    std::unique_ptr<Mesh> m_torusMesh;       // Rotate ring
    std::unique_ptr<Mesh> m_circleMesh;      // Rotate circle outline
    std::unique_ptr<Mesh> m_scaleCubeMesh;   // Scale cube
    std::unique_ptr<Mesh> m_scaleLineMesh;   // Scale line
    std::unique_ptr<Mesh> m_centerCubeMesh;  // Center cube for uniform scale
    std::unique_ptr<Mesh> m_coneMesh;        // Arrow head

    // Line rendering (for circles, etc.)
    uint32_t m_lineVAO = 0;
    uint32_t m_lineVBO = 0;
    static constexpr size_t MAX_LINE_VERTICES = 1024;

    // Callback
    TransformCallback m_onTransformChanged;
};

} // namespace Nova
