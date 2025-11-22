#include "scene/FlyCamera.hpp"
#include "input/InputManager.hpp"
#include "config/Config.hpp"

namespace Nova {

FlyCamera::FlyCamera() {
    // Load settings from config
    auto& config = Config::Instance();
    m_moveSpeed = config.Get("camera.move_speed", 10.0f);
    m_lookSpeed = config.Get("camera.look_speed", 0.1f);
}

void FlyCamera::Update(InputManager& input, float deltaTime) {
    if (!m_enabled) return;

    ProcessMouseInput(input, deltaTime);
    ProcessKeyboardInput(input, deltaTime);
}

void FlyCamera::ProcessKeyboardInput(InputManager& input, float deltaTime) {
    float speed = m_moveSpeed * deltaTime;

    // Sprint
    if (input.IsKeyDown(Key::LeftShift)) {
        speed *= m_sprintMultiplier;
    }

    // Movement
    glm::vec3 movement(0.0f);

    if (input.IsKeyDown(Key::W)) {
        movement += m_forward * speed;
    }
    if (input.IsKeyDown(Key::S)) {
        movement -= m_forward * speed;
    }
    if (input.IsKeyDown(Key::A)) {
        movement -= m_right * speed;
    }
    if (input.IsKeyDown(Key::D)) {
        movement += m_right * speed;
    }
    if (input.IsKeyDown(Key::E) || input.IsKeyDown(Key::Space)) {
        movement += m_worldUp * speed;
    }
    if (input.IsKeyDown(Key::Q) || input.IsKeyDown(Key::LeftControl)) {
        movement -= m_worldUp * speed;
    }

    if (glm::length(movement) > 0.0f) {
        m_position += movement;
        UpdateViewMatrix();
    }
}

void FlyCamera::ProcessMouseInput(InputManager& input, float deltaTime) {
    // Only rotate when right mouse button is held
    if (!input.IsMouseButtonDown(MouseButton::Right)) {
        return;
    }

    glm::vec2 mouseDelta = input.GetMouseDelta();

    if (glm::length(mouseDelta) < 0.001f) {
        return;
    }

    m_yaw += mouseDelta.x * m_lookSpeed;
    m_pitch -= mouseDelta.y * m_lookSpeed;

    // Clamp pitch to prevent gimbal lock
    m_pitch = glm::clamp(m_pitch, -89.0f, 89.0f);

    UpdateVectors();
    UpdateViewMatrix();
}

} // namespace Nova
