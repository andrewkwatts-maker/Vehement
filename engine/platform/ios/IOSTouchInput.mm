/**
 * @file IOSTouchInput.mm
 * @brief Touch input and gesture recognition implementation for iOS
 */

#include "IOSTouchInput.hpp"
#include <cmath>
#include <algorithm>

namespace Nova {

IOSTouchInput::IOSTouchInput() {
    m_lastTapTime = std::chrono::steady_clock::now();
}

IOSTouchInput::~IOSTouchInput() = default;

// =============================================================================
// Touch Event Handlers
// =============================================================================

void IOSTouchInput::HandleTouchBegan(float x, float y, int touchId) {
    auto now = std::chrono::steady_clock::now();

    TouchState touch;
    touch.touchId = touchId;
    touch.position = glm::vec2(x, y);
    touch.previousPosition = touch.position;
    touch.startPosition = touch.position;
    touch.delta = glm::vec2(0.0f);
    touch.phase = TouchPhase::Began;
    touch.startTime = now;
    touch.lastUpdateTime = now;
    touch.active = true;

    m_touches[touchId] = touch;
    m_activeTouchIds.push_back(touchId);

    // Initialize gestures based on touch count
    int touchCount = static_cast<int>(m_activeTouchIds.size());

    if (touchCount == 1) {
        // Single touch - could be tap, long press, or pan
        m_longPressActive = false;
        m_panActive = false;
        m_selectionBoxActive = false;
    } else if (touchCount == 2) {
        // Two touches - could be pinch or rotation
        m_initialPinchDistance = CalculateTouchDistance();
        m_currentPinchDistance = m_initialPinchDistance;
        m_initialRotationAngle = CalculateTouchAngle();
        m_currentRotationAngle = m_initialRotationAngle;
        m_pinchActive = false;
        m_rotationActive = false;

        // Cancel any single-touch gesture
        m_panActive = false;
        m_longPressActive = false;
        m_selectionBoxActive = false;
    }

    m_gestureState.touchCount = touchCount;
}

void IOSTouchInput::HandleTouchMoved(float x, float y, int touchId) {
    auto it = m_touches.find(touchId);
    if (it == m_touches.end()) return;

    TouchState& touch = it->second;
    touch.previousPosition = touch.position;
    touch.position = glm::vec2(x, y);
    touch.delta = touch.position - touch.previousPosition;
    touch.phase = TouchPhase::Moved;
    touch.lastUpdateTime = std::chrono::steady_clock::now();
}

void IOSTouchInput::HandleTouchEnded(float x, float y, int touchId) {
    auto it = m_touches.find(touchId);
    if (it == m_touches.end()) return;

    TouchState& touch = it->second;
    touch.previousPosition = touch.position;
    touch.position = glm::vec2(x, y);
    touch.delta = touch.position - touch.previousPosition;
    touch.phase = TouchPhase::Ended;
    touch.active = false;

    // Remove from active list
    m_activeTouchIds.erase(
        std::remove(m_activeTouchIds.begin(), m_activeTouchIds.end(), touchId),
        m_activeTouchIds.end()
    );

    // Check for tap
    auto duration = std::chrono::steady_clock::now() - touch.startTime;
    float durationSec = std::chrono::duration<float>(duration).count();
    float distance = glm::length(touch.position - touch.startPosition);

    if (durationSec < m_config.tapMaxDuration && distance < m_config.tapMaxDistance) {
        DetectDoubleTap();
        if (!m_doubleTapOccurred) {
            m_tapOccurred = true;
            m_lastTapPosition = touch.position;
            m_lastTapTime = std::chrono::steady_clock::now();
            m_consecutiveTaps++;

            if (m_tapCallback) {
                m_tapCallback(touch.position);
            }
        }
    }

    // End selection box if active
    if (m_selectionBoxActive && m_activeTouchIds.empty()) {
        m_selectionBoxEnd = touch.position;
        m_rtsCommand.type = RTSTouchCommand::Type::SelectMultiple;
        m_rtsCommand.selectionStart = m_selectionBoxStart;
        m_rtsCommand.selectionEnd = m_selectionBoxEnd;
        m_rtsCommand.isActive = false;
        m_selectionBoxActive = false;
    }

    // Clean up touch
    m_touches.erase(it);

    // Reset gesture state if no touches remaining
    if (m_activeTouchIds.empty()) {
        m_pinchActive = false;
        m_rotationActive = false;
        m_panActive = false;
        m_longPressActive = false;
        m_gestureState = GestureState();
    }

    m_gestureState.touchCount = static_cast<int>(m_activeTouchIds.size());
}

void IOSTouchInput::HandleTouchCancelled(float x, float y, int touchId) {
    // Treat cancelled like ended but don't trigger tap
    auto it = m_touches.find(touchId);
    if (it != m_touches.end()) {
        it->second.phase = TouchPhase::Cancelled;
        it->second.active = false;
        m_touches.erase(it);
    }

    m_activeTouchIds.erase(
        std::remove(m_activeTouchIds.begin(), m_activeTouchIds.end(), touchId),
        m_activeTouchIds.end()
    );

    // Reset gestures
    m_pinchActive = false;
    m_rotationActive = false;
    m_panActive = false;
    m_longPressActive = false;
    m_selectionBoxActive = false;

    if (m_activeTouchIds.empty()) {
        m_gestureState = GestureState();
    }

    m_gestureState.touchCount = static_cast<int>(m_activeTouchIds.size());
}

// =============================================================================
// Update
// =============================================================================

void IOSTouchInput::Update(float deltaTime) {
    ClearFrameFlags();

    int touchCount = static_cast<int>(m_activeTouchIds.size());

    if (touchCount == 0) {
        return;
    }

    if (touchCount == 1) {
        DetectLongPress(deltaTime);
        DetectPan();
    } else if (touchCount == 2) {
        DetectPinch();
        DetectRotation();

        // Two-finger pan (for camera)
        if (!m_pinchActive && !m_rotationActive) {
            glm::vec2 center = CalculateTouchCenter();
            if (m_gestureState.type == GestureType::Pan) {
                m_gestureState.delta = center - m_gestureState.position;
            }
            m_gestureState.position = center;
        }
    }

    UpdateRTSCommands();
}

void IOSTouchInput::Reset() {
    m_touches.clear();
    m_activeTouchIds.clear();
    m_gestureState = GestureState();
    m_rtsCommand = RTSTouchCommand();

    m_tapOccurred = false;
    m_doubleTapOccurred = false;
    m_longPressStarted = false;
    m_longPressActive = false;
    m_pinchActive = false;
    m_rotationActive = false;
    m_panActive = false;
    m_selectionBoxActive = false;
}

// =============================================================================
// Touch State Queries
// =============================================================================

int IOSTouchInput::GetTouchCount() const {
    return static_cast<int>(m_activeTouchIds.size());
}

const TouchState* IOSTouchInput::GetTouch(int index) const {
    if (index < 0 || index >= static_cast<int>(m_activeTouchIds.size())) {
        return nullptr;
    }
    return GetTouchById(m_activeTouchIds[index]);
}

const TouchState* IOSTouchInput::GetTouchById(int touchId) const {
    auto it = m_touches.find(touchId);
    return it != m_touches.end() ? &it->second : nullptr;
}

bool IOSTouchInput::IsGestureActive(GestureType type) const {
    return m_gestureState.type == type && m_gestureState.inProgress;
}

bool IOSTouchInput::GetSelectionBox(glm::vec2& outMin, glm::vec2& outMax) const {
    if (!m_selectionBoxActive) {
        return false;
    }

    outMin = glm::min(m_selectionBoxStart, m_selectionBoxEnd);
    outMax = glm::max(m_selectionBoxStart, m_selectionBoxEnd);
    return true;
}

void IOSTouchInput::SetScreenSize(int width, int height) {
    m_screenWidth = std::max(1, width);
    m_screenHeight = std::max(1, height);
}

// =============================================================================
// Gesture Detection
// =============================================================================

void IOSTouchInput::DetectTap() {
    // Tap detection is handled in HandleTouchEnded
}

void IOSTouchInput::DetectDoubleTap() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastTap = std::chrono::duration<float>(now - m_lastTapTime).count();

    if (m_consecutiveTaps > 0 && timeSinceLastTap < m_config.doubleTapMaxInterval) {
        m_doubleTapOccurred = true;
        m_tapOccurred = false;  // Override single tap
        m_consecutiveTaps = 0;

        if (m_doubleTapCallback) {
            m_doubleTapCallback(m_lastTapPosition);
        }
    }
}

void IOSTouchInput::DetectLongPress(float deltaTime) {
    if (m_activeTouchIds.empty()) return;

    const TouchState* touch = GetTouch(0);
    if (!touch) return;

    // Check if we've moved too much
    float distance = glm::length(touch->position - touch->startPosition);
    if (distance > m_config.longPressMaxMovement) {
        m_longPressActive = false;
        return;
    }

    // Check duration
    auto duration = std::chrono::steady_clock::now() - touch->startTime;
    float durationSec = std::chrono::duration<float>(duration).count();

    if (!m_longPressActive && durationSec >= m_config.longPressMinDuration) {
        m_longPressActive = true;
        m_longPressStarted = true;
        m_gestureState.type = GestureType::LongPress;
        m_gestureState.position = touch->position;
        m_gestureState.recognized = true;

        if (m_longPressCallback) {
            m_longPressCallback(touch->position);
        }
    }
}

void IOSTouchInput::DetectPinch() {
    if (m_activeTouchIds.size() < 2) return;

    float currentDistance = CalculateTouchDistance();
    float distanceChange = std::abs(currentDistance - m_initialPinchDistance);

    // Start pinch if distance changed enough
    if (!m_pinchActive && distanceChange > m_config.pinchMinDistance) {
        m_pinchActive = true;
        m_gestureState.type = GestureType::Pinch;
        m_gestureState.inProgress = true;
    }

    if (m_pinchActive) {
        float previousScale = m_gestureState.scale;
        m_gestureState.scale = currentDistance / m_initialPinchDistance;
        m_gestureState.scaleDelta = m_gestureState.scale - previousScale;
        m_gestureState.position = CalculateTouchCenter();

        if (m_pinchCallback) {
            m_pinchCallback(m_gestureState.scale, m_gestureState.scaleDelta, m_gestureState.position);
        }
    }
}

void IOSTouchInput::DetectRotation() {
    if (m_activeTouchIds.size() < 2) return;

    float currentAngle = CalculateTouchAngle();
    float angleChange = currentAngle - m_initialRotationAngle;

    // Normalize angle to -PI to PI
    while (angleChange > M_PI) angleChange -= 2.0f * M_PI;
    while (angleChange < -M_PI) angleChange += 2.0f * M_PI;

    // Start rotation if angle changed enough
    if (!m_rotationActive && std::abs(angleChange) > m_config.rotationMinAngle) {
        m_rotationActive = true;
        if (m_gestureState.type != GestureType::Pinch) {
            m_gestureState.type = GestureType::Rotation;
        }
        m_gestureState.inProgress = true;
    }

    if (m_rotationActive) {
        float previousRotation = m_gestureState.rotation;
        m_gestureState.rotation = currentAngle - m_initialRotationAngle;
        m_gestureState.rotationDelta = m_gestureState.rotation - previousRotation;
        m_gestureState.position = CalculateTouchCenter();

        if (m_rotationCallback) {
            m_rotationCallback(m_gestureState.rotation, m_gestureState.rotationDelta, m_gestureState.position);
        }
    }

    m_currentRotationAngle = currentAngle;
}

void IOSTouchInput::DetectPan() {
    if (m_activeTouchIds.empty()) return;
    if (m_longPressActive) return;  // Don't pan during long press

    const TouchState* touch = GetTouch(0);
    if (!touch) return;

    float distance = glm::length(touch->position - touch->startPosition);

    // Start pan if moved enough
    if (!m_panActive && distance > m_config.tapMaxDistance) {
        m_panActive = true;
        m_panStartPosition = touch->position;
        m_gestureState.type = GestureType::Pan;
        m_gestureState.inProgress = true;

        // Start selection box if enabled
        if (m_selectionBoxEnabled) {
            m_selectionBoxActive = true;
            m_selectionBoxStart = touch->startPosition;
        }
    }

    if (m_panActive) {
        m_gestureState.delta = touch->delta;
        m_gestureState.position = touch->position;

        if (m_selectionBoxActive) {
            m_selectionBoxEnd = touch->position;
        }

        if (m_panCallback) {
            m_panCallback(touch->delta, touch->position);
        }
    }
}

void IOSTouchInput::DetectSwipe() {
    // Swipe detection happens at touch end
    // Implementation would check velocity and direction
}

// =============================================================================
// RTS Command Generation
// =============================================================================

void IOSTouchInput::UpdateRTSCommands() {
    int touchCount = static_cast<int>(m_activeTouchIds.size());

    // Single tap - select or move
    if (m_tapOccurred && !m_doubleTapOccurred) {
        m_rtsCommand.type = RTSTouchCommand::Type::SelectUnit;
        m_rtsCommand.screenPosition = m_lastTapPosition;
        m_rtsCommand.isActive = true;
    }

    // Long press - context menu
    if (m_longPressStarted) {
        m_rtsCommand.type = RTSTouchCommand::Type::OpenContextMenu;
        m_rtsCommand.screenPosition = m_gestureState.position;
        m_rtsCommand.isActive = true;
    }

    // Two-finger gestures for camera
    if (touchCount == 2) {
        if (m_pinchActive) {
            m_rtsCommand.type = RTSTouchCommand::Type::CameraZoom;
            m_rtsCommand.zoomDelta = m_gestureState.scaleDelta;
            m_rtsCommand.screenPosition = m_gestureState.position;
            m_rtsCommand.isActive = true;
        }

        if (m_rotationActive) {
            m_rtsCommand.type = RTSTouchCommand::Type::CameraRotate;
            m_rtsCommand.rotationDelta = m_gestureState.rotationDelta;
            m_rtsCommand.screenPosition = m_gestureState.position;
            m_rtsCommand.isActive = true;
        }

        // If neither pinch nor rotation, it's a camera pan
        if (!m_pinchActive && !m_rotationActive && m_gestureState.delta != glm::vec2(0.0f)) {
            m_rtsCommand.type = RTSTouchCommand::Type::CameraPan;
            m_rtsCommand.screenPosition = m_gestureState.position;
            m_rtsCommand.isActive = true;
        }
    }

    // Selection box
    if (m_selectionBoxActive) {
        m_rtsCommand.type = RTSTouchCommand::Type::SelectMultiple;
        m_rtsCommand.selectionStart = m_selectionBoxStart;
        m_rtsCommand.selectionEnd = m_selectionBoxEnd;
        m_rtsCommand.isActive = true;
    }
}

// =============================================================================
// InputManager Integration
// =============================================================================

void IOSTouchInput::UpdateInputManager(InputManager& input) {
    // Touch input maps to mouse-like controls for compatibility:
    // - Single touch = left mouse button + position
    // - Two-finger tap = right mouse button
    // - Pinch = scroll wheel (zoom)

    // This method would update the InputManager with virtual mouse events
    // based on touch gestures, allowing existing mouse-based game code
    // to work with touch input.

    // The actual implementation depends on InputManager's interface
    // for programmatic input injection.
}

// =============================================================================
// Helper Methods
// =============================================================================

glm::vec2 IOSTouchInput::CalculateTouchCenter() const {
    if (m_activeTouchIds.empty()) {
        return glm::vec2(0.0f);
    }

    glm::vec2 center(0.0f);
    for (int touchId : m_activeTouchIds) {
        auto it = m_touches.find(touchId);
        if (it != m_touches.end()) {
            center += it->second.position;
        }
    }
    return center / static_cast<float>(m_activeTouchIds.size());
}

float IOSTouchInput::CalculateTouchDistance() const {
    if (m_activeTouchIds.size() < 2) {
        return 0.0f;
    }

    const TouchState* touch1 = GetTouchById(m_activeTouchIds[0]);
    const TouchState* touch2 = GetTouchById(m_activeTouchIds[1]);

    if (!touch1 || !touch2) {
        return 0.0f;
    }

    return glm::length(touch2->position - touch1->position);
}

float IOSTouchInput::CalculateTouchAngle() const {
    if (m_activeTouchIds.size() < 2) {
        return 0.0f;
    }

    const TouchState* touch1 = GetTouchById(m_activeTouchIds[0]);
    const TouchState* touch2 = GetTouchById(m_activeTouchIds[1]);

    if (!touch1 || !touch2) {
        return 0.0f;
    }

    glm::vec2 delta = touch2->position - touch1->position;
    return std::atan2(delta.y, delta.x);
}

void IOSTouchInput::ClearFrameFlags() {
    m_tapOccurred = false;
    m_doubleTapOccurred = false;
    m_longPressStarted = false;
    m_gestureState.delta = glm::vec2(0.0f);
    m_gestureState.scaleDelta = 0.0f;
    m_gestureState.rotationDelta = 0.0f;

    // Clear RTS command if processed
    if (!m_rtsCommand.isActive) {
        m_rtsCommand = RTSTouchCommand();
    }
}

} // namespace Nova
