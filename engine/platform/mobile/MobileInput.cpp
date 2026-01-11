/**
 * @file MobileInput.cpp
 * @brief Mobile input implementation
 */

#include "MobileInput.hpp"
#include <algorithm>
#include <cmath>

namespace Nova {

// =============================================================================
// Constructor/Destructor
// =============================================================================

MobileInput::MobileInput() {
    m_touches.reserve(10);
    m_previousTouches.reserve(10);
    m_gestureEvents.reserve(8);
}

MobileInput::~MobileInput() {
    Shutdown();
}

// =============================================================================
// Initialization
// =============================================================================

void MobileInput::Initialize() {
    if (m_initialized) return;

    m_lastUpdateTime = std::chrono::steady_clock::now();
    m_initialized = true;
}

void MobileInput::Shutdown() {
    if (!m_initialized) return;

    DisableAccelerometer();
    DisableGyroscope();
    DisableCompass();
    DisableDeviceMotion();

    m_touches.clear();
    m_previousTouches.clear();
    m_gestureEvents.clear();

    m_initialized = false;
}

// =============================================================================
// Update
// =============================================================================

void MobileInput::Update() {
    if (!m_initialized) return;

    // Clear per-frame state
    ClearFrameState();

    // Store previous touches for gesture detection
    m_previousTouches = m_touches;

    // Process gestures
    ProcessGestures();

    // Update timing
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

void MobileInput::ClearFrameState() {
    m_wasTapped = false;
    m_wasDoubleTapped = false;
    m_wasSwipe = false;
    m_swipeDirection = SwipeDirection::None;
    m_pinchScaleDelta = 0.0f;
    m_rotationDelta = 0.0f;
    m_panDelta = glm::vec2{0.0f};
    m_gestureEvents.clear();

    // Remove ended touches
    m_touches.erase(
        std::remove_if(m_touches.begin(), m_touches.end(),
            [](const Touch& t) { return t.HasEnded(); }),
        m_touches.end()
    );
}

// =============================================================================
// Touch Queries
// =============================================================================

const Touch* MobileInput::GetTouch(int index) const noexcept {
    if (index >= 0 && index < static_cast<int>(m_touches.size())) {
        return &m_touches[index];
    }
    return nullptr;
}

const Touch* MobileInput::GetTouchById(int64_t id) const noexcept {
    auto it = std::find_if(m_touches.begin(), m_touches.end(),
        [id](const Touch& t) { return t.id == id; });
    return (it != m_touches.end()) ? &(*it) : nullptr;
}

std::vector<const Touch*> MobileInput::GetNewTouches() const {
    std::vector<const Touch*> newTouches;
    for (const auto& touch : m_touches) {
        if (touch.IsNew()) {
            newTouches.push_back(&touch);
        }
    }
    return newTouches;
}

std::vector<const Touch*> MobileInput::GetEndedTouches() const {
    std::vector<const Touch*> endedTouches;
    for (const auto& touch : m_touches) {
        if (touch.HasEnded()) {
            endedTouches.push_back(&touch);
        }
    }
    return endedTouches;
}

const Touch* MobileInput::GetPrimaryTouch() const noexcept {
    return m_touches.empty() ? nullptr : &m_touches[0];
}

// =============================================================================
// Gesture Processing
// =============================================================================

void MobileInput::ProcessGestures() {
    DetectTap();
    DetectLongPress();
    DetectPinch();
    DetectRotation();
    DetectPan();
    DetectSwipe();
}

void MobileInput::DetectTap() {
    // Check for ended touches that qualify as taps
    for (const auto& touch : m_touches) {
        if (touch.phase != TouchPhase::Ended) continue;

        // Calculate movement and duration
        glm::vec2 delta = touch.GetTotalDelta();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        // Get duration (approximate from timestamp)
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration<float>(now - m_lastUpdateTime).count();

        if (distance < m_tapMaxMovement && duration < m_tapMaxDuration) {
            m_wasTapped = true;

            // Check for double tap
            uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();

            if (m_lastTapTime > 0) {
                float interval = (nowMs - m_lastTapTime) / 1000.0f;
                glm::vec2 tapDelta = touch.position - m_lastTapPosition;
                float tapDistance = std::sqrt(tapDelta.x * tapDelta.x + tapDelta.y * tapDelta.y);

                if (interval < m_doubleTapMaxInterval && tapDistance < m_tapMaxMovement * 2) {
                    m_wasDoubleTapped = true;

                    GestureEvent event;
                    event.type = GestureType::DoubleTap;
                    event.state = GestureState::Ended;
                    event.position = touch.position;
                    event.tapCount = 2;
                    m_gestureEvents.push_back(event);

                    if (m_gestureCallback) {
                        m_gestureCallback(event);
                    }
                }
            }

            m_lastTapTime = nowMs;
            m_lastTapPosition = touch.position;

            GestureEvent event;
            event.type = GestureType::Tap;
            event.state = GestureState::Ended;
            event.position = touch.position;
            event.tapCount = 1;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
    }
}

void MobileInput::DetectLongPress() {
    if (m_touches.size() != 1) {
        if (m_isLongPressing) {
            m_isLongPressing = false;

            GestureEvent event;
            event.type = GestureType::LongPress;
            event.state = GestureState::Ended;
            event.duration = m_longPressDuration;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
        return;
    }

    const Touch& touch = m_touches[0];

    if (touch.phase == TouchPhase::Began) {
        m_touchStartTime = touch.timestamp;
        m_touchStartPosition = touch.position;
        m_longPressDuration = 0.0f;
    }

    if (touch.phase == TouchPhase::Stationary || touch.phase == TouchPhase::Moved) {
        glm::vec2 delta = touch.position - m_touchStartPosition;
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

        if (distance < m_tapMaxMovement) {
            auto now = std::chrono::steady_clock::now();
            float duration = std::chrono::duration<float>(now - m_lastUpdateTime).count();
            m_longPressDuration += duration;

            if (m_longPressDuration >= m_longPressMinDuration) {
                bool wasLongPressing = m_isLongPressing;
                m_isLongPressing = true;

                GestureEvent event;
                event.type = GestureType::LongPress;
                event.state = wasLongPressing ? GestureState::Changed : GestureState::Began;
                event.position = touch.position;
                event.duration = m_longPressDuration;
                m_gestureEvents.push_back(event);

                if (m_gestureCallback) {
                    m_gestureCallback(event);
                }
            }
        } else {
            m_isLongPressing = false;
            m_longPressDuration = 0.0f;
        }
    }

    if (touch.HasEnded() && m_isLongPressing) {
        m_isLongPressing = false;

        GestureEvent event;
        event.type = GestureType::LongPress;
        event.state = GestureState::Ended;
        event.position = touch.position;
        event.duration = m_longPressDuration;
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    }
}

void MobileInput::DetectPinch() {
    if (m_touches.size() != 2) {
        if (m_isPinching) {
            m_isPinching = false;

            GestureEvent event;
            event.type = GestureType::Pinch;
            event.state = GestureState::Ended;
            event.scale = m_pinchScale;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
        return;
    }

    const Touch& touch1 = m_touches[0];
    const Touch& touch2 = m_touches[1];

    // Calculate current distance
    glm::vec2 delta = touch2.position - touch1.position;
    float currentDistance = std::sqrt(delta.x * delta.x + delta.y * delta.y);

    // Calculate center
    m_pinchCenter = (touch1.position + touch2.position) * 0.5f;

    if (!m_isPinching) {
        // Start pinch
        m_isPinching = true;
        m_initialPinchDistance = currentDistance;
        m_pinchScale = 1.0f;
        m_pinchScaleDelta = 0.0f;

        GestureEvent event;
        event.type = GestureType::Pinch;
        event.state = GestureState::Began;
        event.position = m_pinchCenter;
        event.scale = 1.0f;
        event.touchCount = 2;
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    } else {
        // Update pinch
        float prevScale = m_pinchScale;
        if (m_initialPinchDistance > 0.0f) {
            m_pinchScale = currentDistance / m_initialPinchDistance;
        }
        m_pinchScaleDelta = m_pinchScale - prevScale;

        GestureEvent event;
        event.type = GestureType::Pinch;
        event.state = GestureState::Changed;
        event.position = m_pinchCenter;
        event.scale = m_pinchScale;
        event.velocity_scale = m_pinchScaleDelta;
        event.touchCount = 2;
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    }
}

void MobileInput::DetectRotation() {
    if (m_touches.size() != 2) {
        if (m_isRotating) {
            m_isRotating = false;

            GestureEvent event;
            event.type = GestureType::Rotation;
            event.state = GestureState::Ended;
            event.rotation = m_rotationAngle;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
        return;
    }

    const Touch& touch1 = m_touches[0];
    const Touch& touch2 = m_touches[1];

    // Calculate current angle
    glm::vec2 delta = touch2.position - touch1.position;
    float currentAngle = std::atan2(delta.y, delta.x);

    if (!m_isRotating) {
        // Start rotation
        m_isRotating = true;
        m_initialRotationAngle = currentAngle;
        m_rotationAngle = 0.0f;
        m_rotationDelta = 0.0f;

        GestureEvent event;
        event.type = GestureType::Rotation;
        event.state = GestureState::Began;
        event.position = (touch1.position + touch2.position) * 0.5f;
        event.rotation = 0.0f;
        event.touchCount = 2;
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    } else {
        // Update rotation
        float prevAngle = m_rotationAngle;
        m_rotationAngle = currentAngle - m_initialRotationAngle;
        m_rotationDelta = m_rotationAngle - prevAngle;

        GestureEvent event;
        event.type = GestureType::Rotation;
        event.state = GestureState::Changed;
        event.position = (touch1.position + touch2.position) * 0.5f;
        event.rotation = m_rotationAngle;
        event.velocity_rotation = m_rotationDelta;
        event.touchCount = 2;
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    }
}

void MobileInput::DetectPan() {
    if (m_touches.empty()) {
        if (m_isPanning) {
            m_isPanning = false;
            m_panTranslation = glm::vec2{0.0f};

            GestureEvent event;
            event.type = GestureType::Pan;
            event.state = GestureState::Ended;
            event.velocity = m_panVelocity;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
        return;
    }

    // Calculate center of all touches
    glm::vec2 center{0.0f};
    for (const auto& touch : m_touches) {
        center += touch.position;
    }
    center /= static_cast<float>(m_touches.size());

    // Calculate previous center
    glm::vec2 prevCenter{0.0f};
    if (!m_previousTouches.empty()) {
        for (const auto& touch : m_previousTouches) {
            prevCenter += touch.position;
        }
        prevCenter /= static_cast<float>(m_previousTouches.size());
    } else {
        prevCenter = center;
    }

    m_panDelta = center - prevCenter;

    if (!m_isPanning && (std::abs(m_panDelta.x) > 1.0f || std::abs(m_panDelta.y) > 1.0f)) {
        m_isPanning = true;
        m_panTranslation = glm::vec2{0.0f};

        GestureEvent event;
        event.type = GestureType::Pan;
        event.state = GestureState::Began;
        event.position = center;
        event.touchCount = static_cast<int>(m_touches.size());
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    }

    if (m_isPanning) {
        m_panTranslation += m_panDelta;

        // Calculate velocity (simplified)
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - m_lastUpdateTime).count();
        if (dt > 0.0f) {
            m_panVelocity = m_panDelta / dt;
        }

        GestureEvent event;
        event.type = GestureType::Pan;
        event.state = GestureState::Changed;
        event.position = center;
        event.translation = m_panTranslation;
        event.velocity = m_panVelocity;
        event.touchCount = static_cast<int>(m_touches.size());
        m_gestureEvents.push_back(event);

        if (m_gestureCallback) {
            m_gestureCallback(event);
        }
    }
}

void MobileInput::DetectSwipe() {
    for (const auto& touch : m_touches) {
        if (touch.phase != TouchPhase::Ended) continue;

        glm::vec2 delta = touch.GetTotalDelta();

        // Calculate velocity
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - m_lastUpdateTime).count();
        glm::vec2 velocity = (dt > 0.0f) ? delta / dt : glm::vec2{0.0f};

        float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

        if (speed >= m_swipeMinVelocity) {
            m_wasSwipe = true;

            // Determine direction
            if (std::abs(velocity.x) > std::abs(velocity.y)) {
                m_swipeDirection = (velocity.x > 0) ? SwipeDirection::Right : SwipeDirection::Left;
            } else {
                m_swipeDirection = (velocity.y > 0) ? SwipeDirection::Down : SwipeDirection::Up;
            }

            GestureEvent event;
            event.type = GestureType::Swipe;
            event.state = GestureState::Ended;
            event.position = touch.position;
            event.velocity = velocity;
            event.direction = m_swipeDirection;
            m_gestureEvents.push_back(event);

            if (m_gestureCallback) {
                m_gestureCallback(event);
            }
        }
    }
}

// =============================================================================
// Touch Event Handling
// =============================================================================

void MobileInput::HandleTouchEvent(const Touch& touch) {
    switch (touch.phase) {
        case TouchPhase::Began: {
            // Add new touch
            m_touches.push_back(touch);

            if (m_touchBeganCallback) {
                m_touchBeganCallback(touch);
            }
            break;
        }

        case TouchPhase::Moved:
        case TouchPhase::Stationary: {
            // Update existing touch
            auto it = std::find_if(m_touches.begin(), m_touches.end(),
                [&touch](const Touch& t) { return t.id == touch.id; });

            if (it != m_touches.end()) {
                it->previousPosition = it->position;
                it->position = touch.position;
                it->phase = touch.phase;
                it->pressure = touch.pressure;
                it->force = touch.force;
                it->timestamp = touch.timestamp;

                if (m_touchMovedCallback) {
                    m_touchMovedCallback(*it);
                }
            }
            break;
        }

        case TouchPhase::Ended:
        case TouchPhase::Cancelled: {
            // Mark touch as ended
            auto it = std::find_if(m_touches.begin(), m_touches.end(),
                [&touch](const Touch& t) { return t.id == touch.id; });

            if (it != m_touches.end()) {
                it->phase = touch.phase;
                it->position = touch.position;
                it->timestamp = touch.timestamp;

                if (m_touchEndedCallback) {
                    m_touchEndedCallback(*it);
                }
            }
            break;
        }
    }
}

// =============================================================================
// Sensor Updates
// =============================================================================

void MobileInput::EnableAccelerometer(float /*interval*/) {
    m_accelerometerEnabled = true;
    // Platform-specific implementation would start updates here
}

void MobileInput::DisableAccelerometer() {
    m_accelerometerEnabled = false;
    m_accelerometer = AccelerometerData{};
}

bool MobileInput::IsAccelerometerAvailable() const noexcept {
    // Platform-specific check
    return true;  // Assume available on mobile
}

void MobileInput::EnableGyroscope(float /*interval*/) {
    m_gyroscopeEnabled = true;
}

void MobileInput::DisableGyroscope() {
    m_gyroscopeEnabled = false;
    m_gyroscope = GyroscopeData{};
}

bool MobileInput::IsGyroscopeAvailable() const noexcept {
    return true;
}

void MobileInput::EnableCompass() {
    m_compassEnabled = true;
}

void MobileInput::DisableCompass() {
    m_compassEnabled = false;
    m_compass = CompassData{};
}

bool MobileInput::IsCompassAvailable() const noexcept {
    return true;
}

void MobileInput::EnableDeviceMotion(float /*interval*/) {
    m_deviceMotionEnabled = true;
}

void MobileInput::DisableDeviceMotion() {
    m_deviceMotionEnabled = false;
    m_deviceMotion = DeviceMotion{};
}

bool MobileInput::IsDeviceMotionAvailable() const noexcept {
    return true;
}

void MobileInput::UpdateAccelerometer(const AccelerometerData& data) {
    if (m_accelerometerEnabled) {
        m_accelerometer = data;
    }
}

void MobileInput::UpdateGyroscope(const GyroscopeData& data) {
    if (m_gyroscopeEnabled) {
        m_gyroscope = data;
    }
}

void MobileInput::UpdateCompass(const CompassData& data) {
    if (m_compassEnabled) {
        m_compass = data;
    }
}

void MobileInput::UpdateDeviceMotion(const DeviceMotion& data) {
    if (m_deviceMotionEnabled) {
        m_deviceMotion = data;
    }
}

} // namespace Nova
