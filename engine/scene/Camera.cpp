#include "scene/Camera.hpp"
#include "config/Config.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace Nova {

Camera::Camera() {
    // Load defaults from config
    auto& config = Config::Instance();
    m_fov = config.Get("camera.fov", 45.0f);
    m_nearPlane = config.Get("camera.near_plane", 0.1f);
    m_farPlane = config.Get("camera.far_plane", 1000.0f);

    UpdateVectors();
    // Initial projection setup
    SetPerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
}

void Camera::SetPerspective(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    m_projectionType = ProjectionType::Perspective;
    m_fov = fovDegrees;
    m_aspectRatio = aspectRatio;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;

    m_projectionMatrix = glm::perspective(
        glm::radians(fovDegrees),
        aspectRatio,
        nearPlane,
        farPlane
    );

    MarkProjectionDirty();
}

void Camera::SetOrthographic(float left, float right, float bottom, float top,
                              float nearPlane, float farPlane) {
    m_projectionType = ProjectionType::Orthographic;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;

    m_projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    MarkProjectionDirty();
}

void Camera::LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    m_position = position;
    m_worldUp = up;

    m_forward = glm::normalize(target - position);
    m_right = glm::normalize(glm::cross(m_forward, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_forward));

    // Calculate pitch and yaw from forward vector
    m_pitch = glm::degrees(std::asin(m_forward.y));
    m_yaw = glm::degrees(std::atan2(m_forward.z, m_forward.x));

    MarkViewDirty();
}

void Camera::SetPosition(const glm::vec3& position) {
    m_position = position;
    MarkViewDirty();
}

void Camera::SetRotation(const glm::vec3& eulerDegrees) {
    m_pitch = eulerDegrees.x;
    m_yaw = eulerDegrees.y;

    // Clamp pitch to prevent gimbal lock
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    UpdateVectors();
    MarkViewDirty();
}

void Camera::SetRotation(float pitchDegrees, float yawDegrees) {
    m_pitch = glm::clamp(pitchDegrees, -89.0f, 89.0f);
    m_yaw = yawDegrees;

    UpdateVectors();
    MarkViewDirty();
}

void Camera::MarkViewDirty() {
    m_viewDirty = true;
    m_projectionViewDirty = true;
    m_frustumDirty = true;
}

void Camera::MarkProjectionDirty() {
    m_projectionViewDirty = true;
    m_frustumDirty = true;
}

const glm::mat4& Camera::GetView() const {
    if (m_viewDirty) {
        UpdateViewMatrix();
    }
    return m_viewMatrix;
}

const glm::mat4& Camera::GetProjectionView() const {
    if (m_projectionViewDirty) {
        UpdateProjectionView();
    }
    return m_projectionViewMatrix;
}

const glm::mat4& Camera::GetInverseProjectionView() const {
    if (m_projectionViewDirty) {
        UpdateProjectionView();
    }
    return m_inverseProjectionViewMatrix;
}

void Camera::UpdateViewMatrix() const {
    m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, m_up);
    m_viewDirty = false;
}

void Camera::UpdateProjectionView() const {
    if (m_viewDirty) {
        UpdateViewMatrix();
    }
    m_projectionViewMatrix = m_projectionMatrix * m_viewMatrix;
    m_inverseProjectionViewMatrix = glm::inverse(m_projectionViewMatrix);
    m_projectionViewDirty = false;
}

void Camera::UpdateVectors() {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    forward.y = std::sin(glm::radians(m_pitch));
    forward.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));

    m_forward = glm::normalize(forward);
    m_right = glm::normalize(glm::cross(m_forward, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

void Camera::UpdateFrustum() const {
    if (!m_frustumDirty) {
        return;
    }

    const glm::mat4& vp = GetProjectionView();

    // Extract frustum planes from view-projection matrix
    // Left
    m_frustumPlanes[0] = glm::vec4(
        vp[0][3] + vp[0][0],
        vp[1][3] + vp[1][0],
        vp[2][3] + vp[2][0],
        vp[3][3] + vp[3][0]
    );
    // Right
    m_frustumPlanes[1] = glm::vec4(
        vp[0][3] - vp[0][0],
        vp[1][3] - vp[1][0],
        vp[2][3] - vp[2][0],
        vp[3][3] - vp[3][0]
    );
    // Bottom
    m_frustumPlanes[2] = glm::vec4(
        vp[0][3] + vp[0][1],
        vp[1][3] + vp[1][1],
        vp[2][3] + vp[2][1],
        vp[3][3] + vp[3][1]
    );
    // Top
    m_frustumPlanes[3] = glm::vec4(
        vp[0][3] - vp[0][1],
        vp[1][3] - vp[1][1],
        vp[2][3] - vp[2][1],
        vp[3][3] - vp[3][1]
    );
    // Near
    m_frustumPlanes[4] = glm::vec4(
        vp[0][3] + vp[0][2],
        vp[1][3] + vp[1][2],
        vp[2][3] + vp[2][2],
        vp[3][3] + vp[3][2]
    );
    // Far
    m_frustumPlanes[5] = glm::vec4(
        vp[0][3] - vp[0][2],
        vp[1][3] - vp[1][2],
        vp[2][3] - vp[2][2],
        vp[3][3] - vp[3][2]
    );

    // Normalize planes
    for (auto& plane : m_frustumPlanes) {
        float length = glm::length(glm::vec3(plane));
        if (length > 0.0f) {
            plane /= length;
        }
    }

    m_frustumDirty = false;
}

glm::vec3 Camera::ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
    // Convert to NDC
    float x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * screenPos.y) / screenSize.y;

    // Unproject
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(m_projectionMatrix) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::vec3(glm::inverse(GetView()) * rayEye);
    return glm::normalize(rayWorld);
}

glm::vec2 Camera::WorldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const {
    glm::vec4 clipPos = GetProjectionView() * glm::vec4(worldPos, 1.0f);

    if (clipPos.w <= 0.0f) {
        return glm::vec2(-1.0f);  // Behind camera
    }

    glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

    return glm::vec2(
        (ndc.x + 1.0f) * 0.5f * screenSize.x,
        (1.0f - ndc.y) * 0.5f * screenSize.y
    );
}

bool Camera::IsInFrustum(const glm::vec3& point) const {
    UpdateFrustum();

    for (const auto& plane : m_frustumPlanes) {
        float distance = glm::dot(glm::vec3(plane), point) + plane.w;
        if (distance < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Camera::IsInFrustum(const glm::vec3& center, float radius) const {
    UpdateFrustum();

    for (const auto& plane : m_frustumPlanes) {
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

} // namespace Nova
