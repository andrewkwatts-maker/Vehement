#include "scene/FlyCamera.hpp"
#include "input/InputManager.hpp"
#include "config/Config.hpp"

#include <algorithm>

namespace Nova {

FlyCamera::FlyCamera() {
    // Load settings from config
    auto& config = Config::Instance();
    m_moveSpeed = config.Get("camera.move_speed", 10.0f);
    m_lookSpeed = config.Get("camera.look_speed", 0.1f);
    m_sprintMultiplier = config.Get("camera.sprint_multiplier", 2.5f);
    m_minSpeed = config.Get("camera.min_speed", 1.0f);
    m_maxSpeed = config.Get("camera.max_speed", 100.0f);

    // Store defaults for reset
    m_defaultPosition = m_position;
    m_defaultPitch = m_pitch;
    m_defaultYaw = m_yaw;
}

void FlyCamera::Update(InputManager& input, float deltaTime) {
    if (!m_enabled) {
        return;
    }

    ProcessMouseInput(input, deltaTime);
    ProcessKeyboardInput(input, deltaTime);
}

void FlyCamera::ProcessKeyboardInput(InputManager& input, float deltaTime) {
    float speed = m_moveSpeed * deltaTime;

    // Sprint modifier
    if (input.IsKeyDown(Key::LeftShift)) {
        speed *= m_sprintMultiplier;
    }

    // Accumulate movement direction
    glm::vec3 moveDir(0.0f);

    if (input.IsKeyDown(Key::W)) {
        moveDir += m_forward;
    }
    if (input.IsKeyDown(Key::S)) {
        moveDir -= m_forward;
    }
    if (input.IsKeyDown(Key::A)) {
        moveDir -= m_right;
    }
    if (input.IsKeyDown(Key::D)) {
        moveDir += m_right;
    }
    if (input.IsKeyDown(Key::E) || input.IsKeyDown(Key::Space)) {
        moveDir += m_worldUp;
    }
    if (input.IsKeyDown(Key::Q) || input.IsKeyDown(Key::LeftControl)) {
        moveDir -= m_worldUp;
    }

    // Apply movement if any direction pressed
    if (glm::dot(moveDir, moveDir) > 0.0f) {
        m_position += glm::normalize(moveDir) * speed;
        MarkViewDirty();
    }
}

void FlyCamera::ProcessMouseInput(InputManager& input, float deltaTime) {
    // Only rotate when right mouse button is held
    if (!input.IsMouseButtonDown(MouseButton::Right)) {
        return;
    }

    glm::vec2 mouseDelta = input.GetMouseDelta();

    // Use squared length for early exit (avoids sqrt)
    if (glm::dot(mouseDelta, mouseDelta) < 0.0001f) {
        return;
    }

    m_yaw += mouseDelta.x * m_lookSpeed;
    m_pitch -= mouseDelta.y * m_lookSpeed;

    // Clamp pitch to prevent gimbal lock
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    UpdateVectors();
    MarkViewDirty();
}

void FlyCamera::Reset() {
    m_position = m_defaultPosition;
    m_pitch = m_defaultPitch;
    m_yaw = m_defaultYaw;

    UpdateVectors();
    MarkViewDirty();
}

void FlyCamera::SetSpeedBounds(float min, float max) {
    m_minSpeed = min;
    m_maxSpeed = max;
    m_moveSpeed = glm::clamp(m_moveSpeed, m_minSpeed, m_maxSpeed);
}

void FlyCamera::AdjustSpeed(float factor) {
    m_moveSpeed = glm::clamp(m_moveSpeed * factor, m_minSpeed, m_maxSpeed);
}

} // namespace Nova
