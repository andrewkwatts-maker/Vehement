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
 * @brief Snap target types for object snapping
 */
enum class SnapTargetType : uint8_t {
    None        = 0,
    Vertex      = 1 << 0,           ///< Snap to mesh vertices
    Edge        = 1 << 1,           ///< Snap to edge midpoints/closest points
    Face        = 1 << 2,           ///< Snap to face centers
    BoundingBox = 1 << 3,           ///< Snap to bounding box corners/centers
    GridPoint   = 1 << 4,           ///< Snap to grid intersection points
    All         = 0xFF              ///< Snap to all types
};

// Enable bitwise operations for SnapTargetType
inline SnapTargetType operator|(SnapTargetType a, SnapTargetType b) {
    return static_cast<SnapTargetType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SnapTargetType operator&(SnapTargetType a, SnapTargetType b) {
    return static_cast<SnapTargetType>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasSnapTarget(SnapTargetType flags, SnapTargetType check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

/**
 * @brief Result of a snap operation
 */
struct SnapResult {
    glm::vec3 position{0.0f};           ///< Snapped world position
    glm::vec3 normal{0.0f, 1.0f, 0.0f}; ///< Surface normal at snap point (if applicable)
    SnapTargetType type = SnapTargetType::None; ///< Type of snap target hit
    float distance = 0.0f;              ///< Distance from original position
    bool didSnap = false;               ///< Whether snapping occurred

    [[nodiscard]] bool IsValid() const { return didSnap; }
};

/**
 * @brief World-space snapping configuration
 */
struct WorldSnapConfig {
    // Grid snapping
    bool gridSnapEnabled = false;           ///< Enable world-space grid snapping
    float gridSize = 1.0f;                  ///< Primary grid cell size
    int gridSubdivisions = 4;               ///< Number of subdivisions per cell
    float snapDistance = 0.5f;              ///< Distance threshold for snapping

    // Object snapping
    bool objectSnapEnabled = false;         ///< Enable snap-to-object
    SnapTargetType snapTargets = SnapTargetType::All; ///< Which snap targets to use
    float objectSnapDistance = 0.3f;        ///< Distance threshold for object snapping
    int maxSnapCandidates = 100;            ///< Maximum objects to consider for snapping

    // Rotation snapping
    bool worldAxisRotationSnap = false;     ///< Snap rotation to world axes
    float worldRotationSnapAngle = 90.0f;   ///< World rotation snap angle (degrees)

    // Scale snapping
    bool roundScaleSnap = false;            ///< Snap scale to round values
    float scaleSnapIncrement = 0.25f;       ///< Scale snap increment (e.g., 0.25, 0.5, 1.0)

    // Visual settings
    bool showGrid = false;                  ///< Display grid in viewport
    bool showSnapIndicators = true;         ///< Show visual feedback when snapping
    glm::vec4 gridColor{0.5f, 0.5f, 0.5f, 0.3f};       ///< Primary grid line color
    glm::vec4 gridSubdivColor{0.4f, 0.4f, 0.4f, 0.15f}; ///< Subdivision grid color
    glm::vec4 snapIndicatorColor{1.0f, 1.0f, 0.0f, 0.8f}; ///< Snap indicator color

    // Override settings
    bool ctrlOverridesSnap = true;          ///< Holding Ctrl disables snapping
};

/**
 * @brief Snap target point for object snapping
 */
struct SnapPoint {
    glm::vec3 position;
    glm::vec3 normal;
    SnapTargetType type;
    uint64_t objectId = 0;              ///< ID of the object this snap point belongs to

    SnapPoint() = default;
    SnapPoint(const glm::vec3& pos, const glm::vec3& norm, SnapTargetType t, uint64_t id = 0)
        : position(pos), normal(norm), type(t), objectId(id) {}
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

    // =========================================================================
    // World-Space Snapping
    // =========================================================================

    /**
     * @brief Enable or disable world-space grid snapping
     * @param enabled True to enable world-space grid snapping
     */
    void SetWorldSnapEnabled(bool enabled) { m_worldSnap.gridSnapEnabled = enabled; }

    /**
     * @brief Check if world-space snapping is enabled
     */
    [[nodiscard]] bool IsWorldSnapEnabled() const { return m_worldSnap.gridSnapEnabled; }

    /**
     * @brief Set the world grid size
     * @param size Grid cell size in world units
     */
    void SetGridSize(float size) { m_worldSnap.gridSize = size; }

    /**
     * @brief Get the current grid size
     */
    [[nodiscard]] float GetGridSize() const { return m_worldSnap.gridSize; }

    /**
     * @brief Set the number of grid subdivisions
     * @param subdivisions Number of subdivisions per grid cell
     */
    void SetGridSubdivisions(int subdivisions) { m_worldSnap.gridSubdivisions = subdivisions; }

    /**
     * @brief Get the number of grid subdivisions
     */
    [[nodiscard]] int GetGridSubdivisions() const { return m_worldSnap.gridSubdivisions; }

    /**
     * @brief Set the world snap configuration
     */
    void SetWorldSnapConfig(const WorldSnapConfig& config) { m_worldSnap = config; }

    /**
     * @brief Get the world snap configuration
     */
    [[nodiscard]] const WorldSnapConfig& GetWorldSnapConfig() const { return m_worldSnap; }

    /**
     * @brief Get mutable world snap configuration
     */
    [[nodiscard]] WorldSnapConfig& GetWorldSnapConfig() { return m_worldSnap; }

    /**
     * @brief Enable or disable object snapping
     */
    void SetObjectSnapEnabled(bool enabled) { m_worldSnap.objectSnapEnabled = enabled; }

    /**
     * @brief Check if object snapping is enabled
     */
    [[nodiscard]] bool IsObjectSnapEnabled() const { return m_worldSnap.objectSnapEnabled; }

    /**
     * @brief Set which snap target types to use
     */
    void SetSnapTargets(SnapTargetType targets) { m_worldSnap.snapTargets = targets; }

    /**
     * @brief Get current snap target types
     */
    [[nodiscard]] SnapTargetType GetSnapTargets() const { return m_worldSnap.snapTargets; }

    /**
     * @brief Enable or disable grid display
     */
    void SetGridVisible(bool visible) { m_worldSnap.showGrid = visible; }

    /**
     * @brief Check if grid is visible
     */
    [[nodiscard]] bool IsGridVisible() const { return m_worldSnap.showGrid; }

    /**
     * @brief Snap a position to the world grid
     * @param position World-space position to snap
     * @return Snapped position aligned to grid
     */
    [[nodiscard]] glm::vec3 SnapToGrid(const glm::vec3& position) const;

    /**
     * @brief Snap a position to the nearest grid intersection
     * @param position World-space position
     * @return Snapped position at grid intersection point
     */
    [[nodiscard]] glm::vec3 SnapToGridIntersection(const glm::vec3& position) const;

    /**
     * @brief Snap a position to nearby objects
     * @param position World-space position to snap
     * @param snapPoints Available snap points from scene objects
     * @return Snap result with closest snap point
     */
    [[nodiscard]] SnapResult SnapToObject(const glm::vec3& position,
                                          const std::vector<SnapPoint>& snapPoints) const;

    /**
     * @brief Find snap points from a mesh
     * @param mesh The mesh to extract snap points from
     * @param transform World transform of the mesh
     * @param objectId Unique ID of the mesh object
     * @param targets Which snap target types to extract
     * @return Vector of snap points
     */
    [[nodiscard]] static std::vector<SnapPoint> GetMeshSnapPoints(
        const Mesh& mesh,
        const glm::mat4& transform,
        uint64_t objectId,
        SnapTargetType targets = SnapTargetType::All);

    /**
     * @brief Find snap points from a bounding box
     * @param boundsMin AABB minimum corner
     * @param boundsMax AABB maximum corner
     * @param transform World transform
     * @param objectId Unique ID of the object
     * @return Vector of snap points (corners, edge midpoints, face centers)
     */
    [[nodiscard]] static std::vector<SnapPoint> GetBoundsSnapPoints(
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax,
        const glm::mat4& transform,
        uint64_t objectId);

    /**
     * @brief Snap rotation to world axes
     * @param rotation Current rotation quaternion
     * @return Rotation snapped to nearest world axis alignment
     */
    [[nodiscard]] glm::quat SnapRotationToWorldAxes(const glm::quat& rotation) const;

    /**
     * @brief Snap scale to round values
     * @param scale Current scale vector
     * @return Scale with values snapped to nearest increment
     */
    [[nodiscard]] glm::vec3 SnapScaleToRoundValues(const glm::vec3& scale) const;

    /**
     * @brief Render the world grid
     * @param camera Active camera for view/projection
     */
    void RenderGrid(const Camera& camera);

    /**
     * @brief Render the world grid with explicit matrices
     * @param view View matrix
     * @param projection Projection matrix
     * @param cameraPosition Camera world position
     */
    void RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                    const glm::vec3& cameraPosition);

    /**
     * @brief Render snap indicators at active snap points
     * @param camera Active camera
     * @param activeSnap Current snap result to visualize
     */
    void RenderSnapIndicator(const Camera& camera, const SnapResult& activeSnap);

    /**
     * @brief Set Ctrl key state for snap override
     * @param pressed True if Ctrl is held down
     */
    void SetCtrlPressed(bool pressed) { m_ctrlPressed = pressed; }

    /**
     * @brief Check if snapping is currently active (considering overrides)
     */
    [[nodiscard]] bool IsSnappingActive() const;

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

    // World-space snapping helpers
    glm::vec3 ApplyWorldSnap(const glm::vec3& worldPosition);
    float SnapToNearestGridLine(float value, float gridSize) const;
    glm::vec3 FindClosestGridIntersection(const glm::vec3& position) const;

    // Grid rendering helpers
    void InitializeGridResources();
    void DestroyGridResources();
    void GenerateGridLines(std::vector<float>& vertices,
                           const glm::vec3& cameraPos,
                           float gridExtent) const;

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
    WorldSnapConfig m_worldSnap;
    float m_screenSize = 100.0f;     // Target screen size in pixels
    float m_baseScale = 1.0f;
    float m_handleLength = 1.0f;
    float m_handleRadius = 0.08f;
    float m_planeSize = 0.25f;
    float m_rotateRadius = 0.9f;
    float m_scaleBoxSize = 0.1f;

    // World snap state
    bool m_ctrlPressed = false;
    SnapResult m_activeSnapResult;

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

    // Grid rendering resources
    uint32_t m_gridVAO = 0;
    uint32_t m_gridVBO = 0;
    size_t m_gridVertexCount = 0;
    std::unique_ptr<Shader> m_gridShader;
    static constexpr size_t MAX_GRID_VERTICES = 8192;
    static constexpr float GRID_FADE_START = 20.0f;    // Distance at which grid starts fading
    static constexpr float GRID_FADE_END = 100.0f;     // Distance at which grid fully fades

    // Snap indicator resources
    uint32_t m_snapIndicatorVAO = 0;
    uint32_t m_snapIndicatorVBO = 0;

    // Callback
    TransformCallback m_onTransformChanged;
};

} // namespace Nova
