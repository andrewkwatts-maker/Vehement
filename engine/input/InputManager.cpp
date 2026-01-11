#include "input/InputManager.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace Nova {

InputManager::InputManager() {
    // Reserve space for changed key/button tracking to avoid allocations during gameplay
    m_changedKeys.reserve(16);
    m_changedMouseButtons.reserve(8);
}

InputManager::~InputManager() {
    Shutdown();
}

void InputManager::Initialize(GLFWwindow* window) {
    if (m_window) {
        Shutdown();
    }

    m_window = window;
    if (!m_window) {
        return;
    }

    // Store this pointer for GLFW callbacks
    glfwSetWindowUserPointer(window, this);

    // Register GLFW callbacks
    glfwSetKeyCallback(window, KeyCallbackGLFW);
    glfwSetMouseButtonCallback(window, MouseButtonCallbackGLFW);
    glfwSetCursorPosCallback(window, CursorPosCallbackGLFW);
    glfwSetScrollCallback(window, ScrollCallbackGLFW);

    // Get initial mouse position
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_mousePosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    m_lastMousePosition = m_mousePosition;

    // Reset all state
    m_keys.fill({});
    m_mouseButtons.fill({});
    m_activeKeyCount = 0;
    m_changedKeys.clear();
    m_changedMouseButtons.clear();
    m_firstMouse = true;
}

void InputManager::Shutdown() {
    if (!m_window) {
        return;
    }

    // Clear GLFW callbacks
    glfwSetKeyCallback(m_window, nullptr);
    glfwSetMouseButtonCallback(m_window, nullptr);
    glfwSetCursorPosCallback(m_window, nullptr);
    glfwSetScrollCallback(m_window, nullptr);

    // Clear window user pointer if it still points to us
    if (glfwGetWindowUserPointer(m_window) == this) {
        glfwSetWindowUserPointer(m_window, nullptr);
    }

    m_window = nullptr;

    // Clear all state
    m_keys.fill({});
    m_mouseButtons.fill({});
    m_activeKeyCount = 0;
    m_changedKeys.clear();
    m_changedMouseButtons.clear();

    // Clear callbacks
    ClearCallbacks();
    ClearActions();
}

void InputManager::Update() {
    // Reset pressed/released states only for keys that changed
    // This is more efficient than iterating all 400 keys every frame
    for (int keyIndex : m_changedKeys) {
        m_keys[keyIndex].pressed = false;
        m_keys[keyIndex].released = false;
    }
    m_changedKeys.clear();

    for (int buttonIndex : m_changedMouseButtons) {
        m_mouseButtons[buttonIndex].pressed = false;
        m_mouseButtons[buttonIndex].released = false;
    }
    m_changedMouseButtons.clear();

    // Calculate mouse delta
    m_mouseDelta = m_mousePosition - m_lastMousePosition;
    m_lastMousePosition = m_mousePosition;

    // Reset scroll deltas
    m_scrollDeltaY = 0.0f;
    m_scrollDeltaX = 0.0f;
}

// -----------------------------------------------------------------------------
// Keyboard State Queries
// -----------------------------------------------------------------------------

bool InputManager::IsKeyDown(Key key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].down;
}

bool InputManager::IsKeyPressed(Key key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].pressed;
}

bool InputManager::IsKeyReleased(Key key) const noexcept {
    const auto index = static_cast<size_t>(key);
    return index < m_keys.size() && m_keys[index].released;
}

bool InputManager::IsAnyKeyDown() const noexcept {
    // Use tracked count instead of iterating all keys
    return m_activeKeyCount > 0;
}

// -----------------------------------------------------------------------------
// Modifier Key Helpers
// -----------------------------------------------------------------------------

bool InputManager::IsShiftDown() const noexcept {
    return IsKeyDown(Key::LeftShift) || IsKeyDown(Key::RightShift);
}

bool InputManager::IsControlDown() const noexcept {
    return IsKeyDown(Key::LeftControl) || IsKeyDown(Key::RightControl);
}

bool InputManager::IsAltDown() const noexcept {
    return IsKeyDown(Key::LeftAlt) || IsKeyDown(Key::RightAlt);
}

bool InputManager::IsSuperDown() const noexcept {
    return IsKeyDown(Key::LeftSuper) || IsKeyDown(Key::RightSuper);
}

ModifierFlags InputManager::GetModifiers() const noexcept {
    ModifierFlags flags = ModifierFlags::None;
    if (IsShiftDown()) flags = flags | ModifierFlags::Shift;
    if (IsControlDown()) flags = flags | ModifierFlags::Control;
    if (IsAltDown()) flags = flags | ModifierFlags::Alt;
    if (IsSuperDown()) flags = flags | ModifierFlags::Super;
    return flags;
}

// -----------------------------------------------------------------------------
// Mouse Button State Queries
// -----------------------------------------------------------------------------

bool InputManager::IsMouseButtonDown(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].down;
}

bool InputManager::IsMouseButtonPressed(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].pressed;
}

bool InputManager::IsMouseButtonReleased(MouseButton button) const noexcept {
    const auto index = static_cast<size_t>(button);
    return index < m_mouseButtons.size() && m_mouseButtons[index].released;
}

// -----------------------------------------------------------------------------
// Mouse Position and Movement
// -----------------------------------------------------------------------------

glm::vec2 InputManager::GetMousePosition() const noexcept {
    return m_mousePosition;
}

glm::vec2 InputManager::GetMouseDelta() const noexcept {
    return m_mouseDelta;
}

float InputManager::GetScrollDelta() const noexcept {
    return m_scrollDeltaY;
}

float InputManager::GetScrollDeltaX() const noexcept {
    return m_scrollDeltaX;
}

// -----------------------------------------------------------------------------
// Mouse Control
// -----------------------------------------------------------------------------

void InputManager::SetMousePosition(const glm::vec2& position) {
    if (!m_window) return;

    glfwSetCursorPos(m_window, static_cast<double>(position.x), static_cast<double>(position.y));
    m_mousePosition = position;
    m_lastMousePosition = position;
}

void InputManager::SetCursorLocked(bool locked) {
    if (!m_window) return;

    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    if (locked) {
        m_firstMouse = true;
        m_cursorVisible = false;
    } else {
        m_cursorVisible = true;
    }
}

void InputManager::SetCursorVisible(bool visible) {
    if (!m_window || m_cursorLocked) return;

    m_cursorVisible = visible;
    glfwSetInputMode(m_window, GLFW_CURSOR,
                     visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

// -----------------------------------------------------------------------------
// Action Mapping System
// -----------------------------------------------------------------------------

void InputManager::RegisterAction(const std::string& actionName, std::initializer_list<InputBinding> bindings) {
    m_actionBindings[actionName] = std::vector<InputBinding>(bindings);
}

void InputManager::RegisterAction(const std::string& actionName, InputBinding binding) {
    m_actionBindings[actionName] = { binding };
}

void InputManager::UnregisterAction(const std::string& actionName) {
    m_actionBindings.erase(actionName);
}

void InputManager::ClearActions() {
    m_actionBindings.clear();
}

bool InputManager::IsBindingDown(const InputBinding& binding) const noexcept {
    // Check modifier requirements first
    if (binding.requiredModifiers != ModifierFlags::None) {
        ModifierFlags current = GetModifiers();
        if ((current & binding.requiredModifiers) != binding.requiredModifiers) {
            return false;
        }
    }

    if (binding.type == InputBinding::Type::Key) {
        return IsKeyDown(static_cast<Key>(binding.code));
    } else {
        return IsMouseButtonDown(static_cast<MouseButton>(binding.code));
    }
}

bool InputManager::IsBindingPressed(const InputBinding& binding) const noexcept {
    // Check modifier requirements first
    if (binding.requiredModifiers != ModifierFlags::None) {
        ModifierFlags current = GetModifiers();
        if ((current & binding.requiredModifiers) != binding.requiredModifiers) {
            return false;
        }
    }

    if (binding.type == InputBinding::Type::Key) {
        return IsKeyPressed(static_cast<Key>(binding.code));
    } else {
        return IsMouseButtonPressed(static_cast<MouseButton>(binding.code));
    }
}

bool InputManager::IsBindingReleased(const InputBinding& binding) const noexcept {
    if (binding.type == InputBinding::Type::Key) {
        return IsKeyReleased(static_cast<Key>(binding.code));
    } else {
        return IsMouseButtonReleased(static_cast<MouseButton>(binding.code));
    }
}

bool InputManager::IsActionDown(const std::string& actionName) const {
    auto it = m_actionBindings.find(actionName);
    if (it == m_actionBindings.end()) {
        return false;
    }

    for (const auto& binding : it->second) {
        if (IsBindingDown(binding)) {
            return true;
        }
    }
    return false;
}

bool InputManager::IsActionPressed(const std::string& actionName) const {
    auto it = m_actionBindings.find(actionName);
    if (it == m_actionBindings.end()) {
        return false;
    }

    for (const auto& binding : it->second) {
        if (IsBindingPressed(binding)) {
            return true;
        }
    }
    return false;
}

bool InputManager::IsActionReleased(const std::string& actionName) const {
    auto it = m_actionBindings.find(actionName);
    if (it == m_actionBindings.end()) {
        return false;
    }

    for (const auto& binding : it->second) {
        if (IsBindingReleased(binding)) {
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Axis Input Helpers
// -----------------------------------------------------------------------------

float InputManager::GetAxis(Key negative, Key positive) const noexcept {
    float value = 0.0f;
    if (IsKeyDown(negative)) value -= 1.0f;
    if (IsKeyDown(positive)) value += 1.0f;
    return value;
}

glm::vec2 InputManager::GetMovementVector(bool useWASD) const noexcept {
    glm::vec2 movement{0.0f};

    if (useWASD) {
        if (IsKeyDown(Key::W)) movement.y += 1.0f;
        if (IsKeyDown(Key::S)) movement.y -= 1.0f;
        if (IsKeyDown(Key::A)) movement.x -= 1.0f;
        if (IsKeyDown(Key::D)) movement.x += 1.0f;
    } else {
        if (IsKeyDown(Key::Up)) movement.y += 1.0f;
        if (IsKeyDown(Key::Down)) movement.y -= 1.0f;
        if (IsKeyDown(Key::Left)) movement.x -= 1.0f;
        if (IsKeyDown(Key::Right)) movement.x += 1.0f;
    }

    // Normalize diagonal movement
    const float lengthSq = movement.x * movement.x + movement.y * movement.y;
    if (lengthSq > 1.0f) {
        const float invLength = 1.0f / std::sqrt(lengthSq);
        movement.x *= invLength;
        movement.y *= invLength;
    }

    return movement;
}

// -----------------------------------------------------------------------------
// Callbacks
// -----------------------------------------------------------------------------

void InputManager::ClearCallbacks() {
    m_keyCallback = nullptr;
    m_mouseButtonCallback = nullptr;
    m_mouseMoveCallback = nullptr;
    m_scrollCallback = nullptr;
}

// -----------------------------------------------------------------------------
// GLFW Callback Handlers
// -----------------------------------------------------------------------------

void InputManager::KeyCallbackGLFW(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;

    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input || key < 0 || key >= static_cast<int>(Key::MaxKeys)) {
        return;
    }

    auto& state = input->m_keys[key];

    if (action == GLFW_PRESS) {
        if (!state.down) {
            input->m_activeKeyCount++;
        }
        state.down = true;
        state.pressed = true;
        input->m_changedKeys.push_back(key);
    } else if (action == GLFW_RELEASE) {
        if (state.down) {
            input->m_activeKeyCount--;
        }
        state.down = false;
        state.released = true;
        input->m_changedKeys.push_back(key);
    }
    // GLFW_REPEAT is intentionally ignored - we track held state separately

    if (input->m_keyCallback) {
        input->m_keyCallback(static_cast<Key>(key), action != GLFW_RELEASE);
    }
}

void InputManager::MouseButtonCallbackGLFW(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;

    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input || button < 0 || button >= static_cast<int>(MouseButton::MaxButtons)) {
        return;
    }

    auto& state = input->m_mouseButtons[button];

    if (action == GLFW_PRESS) {
        state.down = true;
        state.pressed = true;
        input->m_changedMouseButtons.push_back(button);
    } else if (action == GLFW_RELEASE) {
        state.down = false;
        state.released = true;
        input->m_changedMouseButtons.push_back(button);
    }

    if (input->m_mouseButtonCallback) {
        input->m_mouseButtonCallback(static_cast<MouseButton>(button), action == GLFW_PRESS);
    }
}

void InputManager::CursorPosCallbackGLFW(GLFWwindow* window, double x, double y) {
    auto* input = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (!input) {
        return;
    }

    const glm::vec2 newPos(static_cast<float>(x), static_cast<float>(y));

    // Handle first mouse movement when cursor is locked to avoid large delta
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
    if (!input) {
        return;
    }

    // Accumulate scroll in case multiple events occur before Update()
    input->m_scrollDeltaY += static_cast<float>(y);
    input->m_scrollDeltaX += static_cast<float>(x);

    if (input->m_scrollCallback) {
        input->m_scrollCallback(static_cast<float>(y), static_cast<float>(x));
    }
}

} // namespace Nova
