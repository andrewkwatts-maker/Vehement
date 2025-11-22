#pragma once

#include "scene/Camera.hpp"

namespace Nova {

class InputManager;

/**
 * @brief Fly camera for free movement in 3D space
 */
class FlyCamera : public Camera {
public:
    FlyCamera();

    /**
     * @brief Update camera movement based on input
     */
    void Update(InputManager& input, float deltaTime);

    // Movement settings
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetLookSpeed(float speed) { m_lookSpeed = speed; }
    void SetSprintMultiplier(float mult) { m_sprintMultiplier = mult; }

    float GetMoveSpeed() const { return m_moveSpeed; }
    float GetLookSpeed() const { return m_lookSpeed; }

    /**
     * @brief Enable/disable camera controls
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

private:
    void ProcessKeyboardInput(InputManager& input, float deltaTime);
    void ProcessMouseInput(InputManager& input, float deltaTime);

    float m_moveSpeed = 10.0f;
    float m_lookSpeed = 0.1f;
    float m_sprintMultiplier = 2.5f;

    bool m_enabled = true;
};

} // namespace Nova
