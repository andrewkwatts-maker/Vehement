#pragma once

#include "scene/Camera.hpp"

namespace Nova {

class InputManager;

/**
 * @brief Fly camera for free movement in 3D space
 *
 * Provides WASD + mouse look controls for navigating a 3D scene.
 * Supports sprint mode and configurable movement/look speeds.
 */
class FlyCamera : public Camera {
public:
    FlyCamera();

    /**
     * @brief Update camera movement based on input
     * @param input The input manager to read from
     * @param deltaTime Time since last update in seconds
     */
    void Update(InputManager& input, float deltaTime);

    // Movement settings
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetLookSpeed(float speed) { m_lookSpeed = speed; }
    void SetSprintMultiplier(float mult) { m_sprintMultiplier = mult; }

    [[nodiscard]] float GetMoveSpeed() const { return m_moveSpeed; }
    [[nodiscard]] float GetLookSpeed() const { return m_lookSpeed; }
    [[nodiscard]] float GetSprintMultiplier() const { return m_sprintMultiplier; }

    /**
     * @brief Enable/disable camera controls
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Reset camera to default position and rotation
     */
    void Reset();

    /**
     * @brief Set the movement speed bounds
     * @param min Minimum movement speed
     * @param max Maximum movement speed
     */
    void SetSpeedBounds(float min, float max);

    /**
     * @brief Adjust movement speed by a factor (for scroll wheel)
     * @param factor Multiplier to apply to current speed
     */
    void AdjustSpeed(float factor);

private:
    void ProcessKeyboardInput(InputManager& input, float deltaTime);
    void ProcessMouseInput(InputManager& input, float deltaTime);

    float m_moveSpeed = 10.0f;
    float m_lookSpeed = 0.1f;
    float m_sprintMultiplier = 2.5f;

    float m_minSpeed = 1.0f;
    float m_maxSpeed = 100.0f;

    bool m_enabled = true;

    // Default values for reset
    glm::vec3 m_defaultPosition{0, 0, 5};
    float m_defaultPitch = 0.0f;
    float m_defaultYaw = -90.0f;
};

} // namespace Nova
