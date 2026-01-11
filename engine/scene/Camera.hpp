#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace Nova {

/**
 * @brief Camera projection type
 */
enum class ProjectionType {
    Perspective,
    Orthographic
};

/**
 * @brief Camera class for 3D viewing
 *
 * Provides view and projection matrix management with frustum culling support.
 * Uses dirty flags to cache computed matrices for optimal performance.
 */
class Camera {
public:
    Camera();
    virtual ~Camera() = default;

    // Non-copyable but movable
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    Camera(Camera&&) noexcept = default;
    Camera& operator=(Camera&&) noexcept = default;

    /**
     * @brief Set perspective projection
     * @param fovDegrees Field of view in degrees
     * @param aspectRatio Width/height ratio
     * @param nearPlane Near clipping plane distance
     * @param farPlane Far clipping plane distance
     */
    void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);

    /**
     * @brief Set orthographic projection
     */
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

    /**
     * @brief Set camera position and orientation using look-at
     * @param position Camera world position
     * @param target Point to look at
     * @param up World up vector
     */
    void LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));

    /**
     * @brief Set camera position
     */
    void SetPosition(const glm::vec3& position);

    /**
     * @brief Set camera rotation (Euler angles in degrees)
     */
    void SetRotation(const glm::vec3& eulerDegrees);

    /**
     * @brief Set camera rotation from pitch and yaw
     * @param pitchDegrees Vertical angle (-89 to 89 degrees)
     * @param yawDegrees Horizontal angle
     */
    void SetRotation(float pitchDegrees, float yawDegrees);

    // Getters - Position and orientation
    [[nodiscard]] const glm::vec3& GetPosition() const { return m_position; }
    [[nodiscard]] const glm::vec3& GetForward() const { return m_forward; }
    [[nodiscard]] const glm::vec3& GetRight() const { return m_right; }
    [[nodiscard]] const glm::vec3& GetUp() const { return m_up; }
    [[nodiscard]] float GetPitch() const { return m_pitch; }
    [[nodiscard]] float GetYaw() const { return m_yaw; }

    // Getters - Matrices (cached with dirty flags)
    [[nodiscard]] const glm::mat4& GetView() const;
    [[nodiscard]] const glm::mat4& GetProjection() const { return m_projectionMatrix; }
    [[nodiscard]] const glm::mat4& GetProjectionView() const;
    [[nodiscard]] const glm::mat4& GetInverseProjectionView() const;

    // Getters - Projection parameters
    [[nodiscard]] ProjectionType GetProjectionType() const { return m_projectionType; }
    [[nodiscard]] float GetFOV() const { return m_fov; }
    [[nodiscard]] float GetAspectRatio() const { return m_aspectRatio; }
    [[nodiscard]] float GetNearPlane() const { return m_nearPlane; }
    [[nodiscard]] float GetFarPlane() const { return m_farPlane; }

    /**
     * @brief Convert screen position to world ray
     * @param screenPos Screen coordinates (pixels)
     * @param screenSize Screen dimensions (pixels)
     * @return Normalized direction vector in world space
     */
    [[nodiscard]] glm::vec3 ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const;

    /**
     * @brief Convert world position to screen position
     * @param worldPos Position in world space
     * @param screenSize Screen dimensions (pixels)
     * @return Screen coordinates, or (-1, -1) if behind camera
     */
    [[nodiscard]] glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const;

    /**
     * @brief Check if a point is in the camera frustum
     */
    [[nodiscard]] bool IsInFrustum(const glm::vec3& point) const;

    /**
     * @brief Check if a sphere is in the camera frustum
     * @param center Sphere center in world space
     * @param radius Sphere radius
     */
    [[nodiscard]] bool IsInFrustum(const glm::vec3& center, float radius) const;

protected:
    void UpdateViewMatrix() const;
    void UpdateVectors();
    void UpdateFrustum() const;
    void UpdateProjectionView() const;

    void MarkViewDirty();
    void MarkProjectionDirty();

    glm::vec3 m_position{0, 0, 5};
    glm::vec3 m_forward{0, 0, -1};
    glm::vec3 m_up{0, 1, 0};
    glm::vec3 m_right{1, 0, 0};
    glm::vec3 m_worldUp{0, 1, 0};

    float m_pitch = 0.0f;  // Degrees
    float m_yaw = -90.0f;  // Degrees

    // Cached matrices with dirty flags
    mutable glm::mat4 m_viewMatrix{1.0f};
    mutable glm::mat4 m_projectionMatrix{1.0f};
    mutable glm::mat4 m_projectionViewMatrix{1.0f};
    mutable glm::mat4 m_inverseProjectionViewMatrix{1.0f};

    mutable bool m_viewDirty = true;
    mutable bool m_projectionViewDirty = true;
    mutable bool m_frustumDirty = true;

    ProjectionType m_projectionType = ProjectionType::Perspective;
    float m_fov = 45.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    // Frustum planes for culling (left, right, bottom, top, near, far)
    mutable std::array<glm::vec4, 6> m_frustumPlanes;
};

} // namespace Nova
