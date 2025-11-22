#include "input/InputManager.hpp"

#include <GLFW/glfw3.h>

namespace Nova {

InputManager::InputManager() = default;

InputManager::~InputManager() = default;

void InputManager::Initialize(GLFWwindow* window) {
    m_window = window;

    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, KeyCallbackGLFW);
    glfwSetMouseButtonCallback(window, MouseButtonCallbackGLFW);
    glfwSetCursorPosCallback(window, CursorPosCallbackGLFW);
    glfwSetScrollCallback(window, ScrollCallbackGLFW);

    // Get initial mouse position
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_mousePosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    m_lastMousePosition = m_mousePosition;
}

void InputManager::Update() {
    // Reset pressed/released states
    for (auto& key : m_keys) {
        key.pressed = false;
        key.released = false;
    }
    for (auto& button : m_mouseButtons) {
        button.pressed = false;
        button.released = false;
    }

    // Calculate mouse delta
    m_mouseDelta = m_mousePosition - m_lastMousePosition;
    m_lastMousePosition = m_mousePosition;

    // Reset scroll
    m_scrollDelta = 0.0f;
}

bool InputManager::IsKeyDown(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].down;
}

bool InputManager::IsKeyPressed(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].pressed;
}

bool InputManager::IsKeyReleased(Key key) const {
    size_t index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].released;
}

bool InputManager::IsAnyKeyDown() const {
    for (const auto& key : m_keys) {
        if (key.down) return true;
    }
    return false;
}

bool InputManager::IsMouseButtonDown(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].down;
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].pressed;
}

bool InputManager::IsMouseButtonReleased(MouseButton button) const {
    size_t index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].released;
}

glm::vec2 InputManager::GetMousePosition() const {
    return m_mousePosition;
}

glm::vec2 InputManager::GetMouseDelta() const {
    return m_mouseDelta;
}

float InputManager::GetScrollDelta() const {
    return m_scrollDelta;
}

void InputManager::SetMousePosition(const glm::vec2& position) {
    glfwSetCursorPos(m_window, position.x, position.y);
    m_mousePosition = position;
    m_lastMousePosition = position;
}

void InputManager::SetCursorLocked(bool locked) {
    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    if (locked) {
        m_firstMouse = true;
    }
}

void InputManager::SetCursorVisible(bool visible) {
    if (!m_cursorLocked) {
        glfwSetInputMode(m_window, GLFW_CURSOR,
                         visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

void InputManager::KeyCallbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input || key < 0 || key >= static_cast<int>(Key::MaxKeys)) return;

    auto& state = input->m_keys[key];
    if (action == GLFW_PRESS) {
        state.down = true;
        state.pressed = true;
    } else if (action == GLFW_RELEASE) {
        state.down = false;
        state.released = true;
    }

    if (input->m_keyCallback) {
        input->m_keyCallback(static_cast<Key>(key), action != GLFW_RELEASE);
    }
}

void InputManager::MouseButtonCallbackGLFW(GLFWwindow* window, int button, int action, int mods) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input || button < 0 || button >= static_cast<int>(MouseButton::MaxButtons)) return;

    auto& state = input->m_mouseButtons[button];
    if (action == GLFW_PRESS) {
        state.down = true;
        state.pressed = true;
    } else if (action == GLFW_RELEASE) {
        state.down = false;
        state.released = true;
    }

    if (input->m_mouseButtonCallback) {
        input->m_mouseButtonCallback(static_cast<MouseButton>(button), action == GLFW_PRESS);
    }
}

void InputManager::CursorPosCallbackGLFW(GLFWwindow* window, double x, double y) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    glm::vec2 newPos(static_cast<float>(x), static_cast<float>(y));

    if (input->m_firstMouse && input->m_cursorLocked) {
        input->m_lastMousePosition = newPos;
        input->m_firstMouse = false;
    }

    input->m_mousePosition = newPos;

    if (input->m_mouseMoveCallback) {
        input->m_mouseMoveCallback(newPos);
    }
}

void InputManager::ScrollCallbackGLFW(GLFWwindow* window, double x, double y) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input) return;

    input->m_scrollDelta = static_cast<float>(y);

    if (input->m_scrollCallback) {
        input->m_scrollCallback(static_cast<float>(y));
    }
}

} // namespace Nova
