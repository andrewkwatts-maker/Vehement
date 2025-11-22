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
    UpdateViewMatrix();
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

    UpdateFrustum();
}

void Camera::SetOrthographic(float left, float right, float bottom, float top,
                              float nearPlane, float farPlane) {
    m_projectionType = ProjectionType::Orthographic;
    m_nearPlane = nearPlane;
    m_farPlane = farPlane;

    m_projectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    UpdateFrustum();
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

    UpdateViewMatrix();
}

void Camera::SetPosition(const glm::vec3& position) {
    m_position = position;
    UpdateViewMatrix();
}

void Camera::SetRotation(const glm::vec3& eulerDegrees) {
    m_pitch = eulerDegrees.x;
    m_yaw = eulerDegrees.y;

    // Clamp pitch
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::SetRotation(float pitchDegrees, float yawDegrees) {
    m_pitch = glm::clamp(pitchDegrees, -89.0f, 89.0f);
    m_yaw = yawDegrees;

    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::UpdateViewMatrix() {
    m_viewMatrix = glm::lookAt(m_position, m_position + m_forward, m_up);
    UpdateFrustum();
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

void Camera::UpdateFrustum() {
    glm::mat4 vp = m_projectionMatrix * m_viewMatrix;

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
        plane /= length;
    }
}

glm::vec3 Camera::ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& screenSize) const {
    // Convert to NDC
    float x = (2.0f * screenPos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * screenPos.y) / screenSize.y;

    // Unproject
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(m_projectionMatrix) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::vec3(glm::inverse(m_viewMatrix) * rayEye);
    return glm::normalize(rayWorld);
}

glm::vec2 Camera::WorldToScreen(const glm::vec3& worldPos, const glm::vec2& screenSize) const {
    glm::vec4 clipPos = m_projectionMatrix * m_viewMatrix * glm::vec4(worldPos, 1.0f);

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
    for (const auto& plane : m_frustumPlanes) {
        float distance = glm::dot(glm::vec3(plane), point) + plane.w;
        if (distance < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Camera::IsInFrustum(const glm::vec3& center, float radius) const {
    for (const auto& plane : m_frustumPlanes) {
        float distance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (distance < -radius) {
            return false;
        }
    }
    return true;
}

} // namespace Nova
