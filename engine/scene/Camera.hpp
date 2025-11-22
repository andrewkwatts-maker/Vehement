#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
 */
class Camera {
public:
    Camera();
    virtual ~Camera() = default;

    /**
     * @brief Set perspective projection
     */
    void SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);

    /**
     * @brief Set orthographic projection
     */
    void SetOrthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane);

    /**
     * @brief Set camera position and orientation
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
     */
    void SetRotation(float pitchDegrees, float yawDegrees);

    // Getters
    const glm::vec3& GetPosition() const { return m_position; }
    const glm::vec3& GetForward() const { return m_forward; }
    const glm::vec3& GetRight() const { return m_right; }
    const glm::vec3& GetUp() const { return m_up; }

    float GetPitch() const { return m_pitch; }
    float GetYaw() const { return m_yaw; }

    const glm::mat4& GetView() const { return m_viewMatrix; }
    const glm::mat4& GetProjection() const { return m_projectionMatrix; }
    glm::mat4 GetProjectionView() const { return m_projectionMatrix * m_viewMatrix; }
    glm::mat4 GetInverseProjectionView() const { return glm::inverse(GetProjectionView()); }

    ProjectionType GetProjectionType() const { return m_projectionType; }
    float GetFOV() const { return m_fov; }
    float GetAspectRatio() const { return m_aspectRatio; }
    float GetNearPlane() const { return m_nearPlane; }
    float GetFarPlane() const { return m_farPlane; }

    /**
     * @brief Convert screen position to world ray
     */
    glm::vec3 ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const;

    /**
     * @brief Convert world position to screen position
     */
    glm::vec2 WorldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const;

    /**
     * @brief Check if a point is in the camera frustum
     */
    bool IsInFrustum(const glm::vec3& point) const;

    /**
     * @brief Check if a sphere is in the camera frustum
     */
    bool IsInFrustum(const glm::vec3& center, float radius) const;

protected:
    void UpdateViewMatrix();
    void UpdateVectors();
    void UpdateFrustum();

    glm::vec3 m_position{0, 0, 5};
    glm::vec3 m_forward{0, 0, -1};
    glm::vec3 m_up{0, 1, 0};
    glm::vec3 m_right{1, 0, 0};
    glm::vec3 m_worldUp{0, 1, 0};

    float m_pitch = 0.0f;  // Degrees
    float m_yaw = -90.0f;  // Degrees

    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projectionMatrix{1.0f};

    ProjectionType m_projectionType = ProjectionType::Perspective;
    float m_fov = 45.0f;
    float m_aspectRatio = 16.0f / 9.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;

    // Frustum planes for culling
    glm::vec4 m_frustumPlanes[6];
};

} // namespace Nova
