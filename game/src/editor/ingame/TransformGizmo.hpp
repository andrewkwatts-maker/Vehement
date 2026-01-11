#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <memory>

namespace Nova {
    class Renderer;
    class Camera;
}

namespace Vehement {

/**
 * @brief Transform tool type
 */
enum class GizmoMode : uint8_t {
    None,
    Translate,  // Move objects
    Rotate,     // Rotate objects
    Scale       // Scale objects
};

/**
 * @brief Transform space
 */
enum class TransformSpace : uint8_t {
    World,      // World space coordinates
    Local       // Local object space
};

/**
 * @brief Gizmo axis being manipulated
 */
enum class GizmoAxis : uint8_t {
    None,
    X,
    Y,
    Z,
    XY,
    XZ,
    YZ,
    XYZ         // Center handle (all axes)
};

/**
 * @brief Transform Gizmo - Visual manipulation tool for objects
 *
 * Features:
 * - Translate (move) gizmo with X/Y/Z axes and planar handles
 * - Rotate gizmo with rotation circles
 * - Scale gizmo with uniform and per-axis scaling
 * - World and local space modes
 * - Mouse interaction and raycasting
 * - Visual feedback on hover and drag
 * - Snap to grid/rotation increments
 */
class TransformGizmo {
public:
    TransformGizmo();
    ~TransformGizmo();

    // Delete copy, allow move
    TransformGizmo(const TransformGizmo&) = delete;
    TransformGizmo& operator=(const TransformGizmo&) = delete;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the gizmo
     * @return true if successful
     */
    bool Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update gizmo state
     * @param deltaTime Time since last update
     * @param camera Camera for raycasting
     */
    void Update(float deltaTime, const Nova::Camera& camera);

    /**
     * @brief Render the gizmo
     * @param renderer Renderer
     * @param camera Camera for rendering
     */
    void Render(Nova::Renderer& renderer, const Nova::Camera& camera);

    // =========================================================================
    // Mode and Space
    // =========================================================================

    /**
     * @brief Set gizmo mode
     */
    void SetMode(GizmoMode mode);

    /**
     * @brief Get current mode
     */
    [[nodiscard]] GizmoMode GetMode() const noexcept { return m_mode; }

    /**
     * @brief Get mode name
     */
    [[nodiscard]] static const char* GetModeName(GizmoMode mode) noexcept;

    /**
     * @brief Set transform space
     */
    void SetSpace(TransformSpace space);

    /**
     * @brief Get current space
     */
    [[nodiscard]] TransformSpace GetSpace() const noexcept { return m_space; }

    /**
     * @brief Toggle between world and local space
     */
    void ToggleSpace();

    // =========================================================================
    // Position and Transform
    // =========================================================================

    /**
     * @brief Set gizmo position
     * @param position World position
     */
    void SetPosition(const glm::vec3& position);

    /**
     * @brief Get gizmo position
     */
    [[nodiscard]] const glm::vec3& GetPosition() const noexcept { return m_position; }

    /**
     * @brief Set gizmo rotation (for local space mode)
     * @param rotation Quaternion rotation
     */
    void SetRotation(const glm::quat& rotation);

    /**
     * @brief Get gizmo rotation
     */
    [[nodiscard]] const glm::quat& GetRotation() const noexcept { return m_rotation; }

    /**
     * @brief Set gizmo scale (visual size, not transform scale)
     * @param scale Scale factor
     */
    void SetGizmoScale(float scale);

    /**
     * @brief Get gizmo scale
     */
    [[nodiscard]] float GetGizmoScale() const noexcept { return m_gizmoScale; }

    // =========================================================================
    // Interaction
    // =========================================================================

    /**
     * @brief Handle mouse down event
     * @param screenPos Mouse position in screen space
     * @param camera Camera for raycasting
     * @return true if gizmo was clicked
     */
    bool OnMouseDown(const glm::vec2& screenPos, const Nova::Camera& camera);

    /**
     * @brief Handle mouse move event
     * @param screenPos Mouse position in screen space
     * @param camera Camera for raycasting
     */
    void OnMouseMove(const glm::vec2& screenPos, const Nova::Camera& camera);

    /**
     * @brief Handle mouse up event
     */
    void OnMouseUp();

    /**
     * @brief Check if gizmo is being dragged
     */
    [[nodiscard]] bool IsDragging() const noexcept { return m_isDragging; }

    /**
     * @brief Get the axis being dragged
     */
    [[nodiscard]] GizmoAxis GetDraggedAxis() const noexcept { return m_draggedAxis; }

    // =========================================================================
    // Transform Delta
    // =========================================================================

    /**
     * @brief Get translation delta since drag started
     */
    [[nodiscard]] const glm::vec3& GetTranslationDelta() const noexcept { return m_translationDelta; }

    /**
     * @brief Get rotation delta since drag started (in degrees)
     */
    [[nodiscard]] float GetRotationDelta() const noexcept { return m_rotationDelta; }

    /**
     * @brief Get scale delta since drag started
     */
    [[nodiscard]] const glm::vec3& GetScaleDelta() const noexcept { return m_scaleDelta; }

    /**
     * @brief Reset transform deltas
     */
    void ResetDeltas();

    // =========================================================================
    // Snapping
    // =========================================================================

    /**
     * @brief Enable/disable translation snapping
     */
    void SetTranslationSnapping(bool enabled);

    /**
     * @brief Set translation snap increment
     */
    void SetTranslationSnapIncrement(float increment);

    /**
     * @brief Enable/disable rotation snapping
     */
    void SetRotationSnapping(bool enabled);

    /**
     * @brief Set rotation snap increment (in degrees)
     */
    void SetRotationSnapIncrement(float increment);

    /**
     * @brief Enable/disable scale snapping
     */
    void SetScaleSnapping(bool enabled);

    /**
     * @brief Set scale snap increment
     */
    void SetScaleSnapIncrement(float increment);

    /**
     * @brief Check if snapping is enabled
     */
    [[nodiscard]] bool IsSnappingEnabled() const noexcept;

    // =========================================================================
    // Visibility
    // =========================================================================

    /**
     * @brief Set gizmo visibility
     */
    void SetVisible(bool visible);

    /**
     * @brief Check if gizmo is visible
     */
    [[nodiscard]] bool IsVisible() const noexcept { return m_visible; }

    /**
     * @brief Enable/disable specific axis
     */
    void SetAxisEnabled(GizmoAxis axis, bool enabled);

    /**
     * @brief Check if axis is enabled
     */
    [[nodiscard]] bool IsAxisEnabled(GizmoAxis axis) const;

private:
    // Raycasting
    GizmoAxis HitTest(const glm::vec2& screenPos, const Nova::Camera& camera);
    bool RayIntersectAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                          const glm::vec3& axisOrigin, const glm::vec3& axisDir,
                          float& t) const;
    bool RayIntersectPlane(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                           const glm::vec3& planePoint, const glm::vec3& planeNormal,
                           float& t) const;
    bool RayIntersectCircle(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                            const glm::vec3& circleCenter, const glm::vec3& circleNormal,
                            float radius, float& t) const;

    // Transform calculation
    void CalculateTranslationDelta(const glm::vec2& screenPos, const Nova::Camera& camera);
    void CalculateRotationDelta(const glm::vec2& screenPos, const Nova::Camera& camera);
    void CalculateScaleDelta(const glm::vec2& screenPos, const Nova::Camera& camera);

    // Snapping helpers
    float SnapValue(float value, float increment) const;
    glm::vec3 SnapVector(const glm::vec3& value, float increment) const;

    // Rendering helpers
    void RenderTranslateGizmo(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderRotateGizmo(Nova::Renderer& renderer, const Nova::Camera& camera);
    void RenderScaleGizmo(Nova::Renderer& renderer, const Nova::Camera& camera);

    void RenderAxis(Nova::Renderer& renderer, const Nova::Camera& camera,
                    const glm::vec3& direction, const glm::vec4& color,
                    GizmoAxis axis, float length);
    void RenderPlane(Nova::Renderer& renderer, const Nova::Camera& camera,
                     const glm::vec3& normal, const glm::vec4& color,
                     GizmoAxis axis, float size);
    void RenderCircle(Nova::Renderer& renderer, const Nova::Camera& camera,
                      const glm::vec3& normal, const glm::vec4& color,
                      GizmoAxis axis, float radius);

    // Color helpers
    glm::vec4 GetAxisColor(GizmoAxis axis, bool isHovered, bool isDragged) const;

    // State
    bool m_initialized = false;
    bool m_visible = true;

    // Mode and space
    GizmoMode m_mode = GizmoMode::Translate;
    TransformSpace m_space = TransformSpace::World;

    // Transform
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float m_gizmoScale = 1.0f;

    // Interaction state
    bool m_isDragging = false;
    GizmoAxis m_hoveredAxis = GizmoAxis::None;
    GizmoAxis m_draggedAxis = GizmoAxis::None;
    glm::vec2 m_dragStartScreenPos{0.0f};
    glm::vec3 m_dragStartPosition{0.0f};
    glm::quat m_dragStartRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_dragStartScale{1.0f};

    // Transform deltas
    glm::vec3 m_translationDelta{0.0f};
    float m_rotationDelta = 0.0f;
    glm::vec3 m_scaleDelta{0.0f};

    // Snapping
    bool m_translationSnapping = false;
    float m_translationSnapIncrement = 0.5f;
    bool m_rotationSnapping = false;
    float m_rotationSnapIncrement = 15.0f;
    bool m_scaleSnapping = false;
    float m_scaleSnapIncrement = 0.1f;

    // Axis enable flags
    bool m_axisEnabled[7] = {true, true, true, true, true, true, true};

    // Visual properties
    float m_axisLength = 1.5f;
    float m_axisThickness = 0.05f;
    float m_planeSize = 0.4f;
    float m_circleRadius = 1.2f;
    float m_centerSize = 0.15f;

    // Colors
    glm::vec4 m_colorX{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 m_colorY{0.0f, 1.0f, 0.0f, 1.0f};
    glm::vec4 m_colorZ{0.0f, 0.0f, 1.0f, 1.0f};
    glm::vec4 m_colorXY{1.0f, 1.0f, 0.0f, 0.6f};
    glm::vec4 m_colorXZ{1.0f, 0.0f, 1.0f, 0.6f};
    glm::vec4 m_colorYZ{0.0f, 1.0f, 1.0f, 0.6f};
    glm::vec4 m_colorXYZ{1.0f, 1.0f, 1.0f, 0.8f};
    glm::vec4 m_colorHover{1.0f, 1.0f, 0.0f, 1.0f};
};

} // namespace Vehement
