#include "AndroidTouchInput.hpp"
#include "AndroidPlatform.hpp"

#include <algorithm>
#include <cmath>

namespace Nova {

AndroidTouchInput::AndroidTouchInput() {
    m_touches.reserve(10);  // Pre-allocate for typical max touches
    m_touchOrder.reserve(10);
}

// -------------------------------------------------------------------------
// Event Handling
// -------------------------------------------------------------------------

int32_t AndroidTouchInput::HandleMotionEvent(const AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t action = AMotionEvent_getAction(event);
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t actionIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                          AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    size_t pointerCount = AMotionEvent_getPointerCount(event);

    switch (actionMasked) {
        case AMOTION_EVENT_ACTION_DOWN: {
            // First finger touched
            int32_t pointerId = AMotionEvent_getPointerId(event, 0);
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            float pressure = AMotionEvent_getPressure(event, 0);
            float size = AMotionEvent_getSize(event, 0);
            OnTouchDown(pointerId, x, y, pressure, size);
            break;
        }

        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            // Additional finger touched
            int32_t pointerId = AMotionEvent_getPointerId(event, actionIndex);
            float x = AMotionEvent_getX(event, actionIndex);
            float y = AMotionEvent_getY(event, actionIndex);
            float pressure = AMotionEvent_getPressure(event, actionIndex);
            float size = AMotionEvent_getSize(event, actionIndex);
            OnTouchDown(pointerId, x, y, pressure, size);
            break;
        }

        case AMOTION_EVENT_ACTION_UP: {
            // Last finger lifted
            int32_t pointerId = AMotionEvent_getPointerId(event, 0);
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            OnTouchUp(pointerId, x, y);
            break;
        }

        case AMOTION_EVENT_ACTION_POINTER_UP: {
            // One of multiple fingers lifted
            int32_t pointerId = AMotionEvent_getPointerId(event, actionIndex);
            float x = AMotionEvent_getX(event, actionIndex);
            float y = AMotionEvent_getY(event, actionIndex);
            OnTouchUp(pointerId, x, y);
            break;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            // One or more fingers moved
            for (size_t i = 0; i < pointerCount; ++i) {
                int32_t pointerId = AMotionEvent_getPointerId(event, i);
                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                float pressure = AMotionEvent_getPressure(event, i);
                float size = AMotionEvent_getSize(event, i);
                OnTouchMove(pointerId, x, y, pressure, size);
            }
            break;
        }

        case AMOTION_EVENT_ACTION_CANCEL: {
            OnTouchCancel();
            break;
        }

        default:
            return 0;
    }

    // Update gesture detection
    if (m_gestureRecognitionEnabled) {
        UpdateGestureState();
    }

    return 1;
}

void AndroidTouchInput::HandleTouchEvent(int action, float x, float y, int pointerId, float pressure) {
    // Map Android MotionEvent actions to our handling
    switch (action) {
        case 0:  // ACTION_DOWN
        case 5:  // ACTION_POINTER_DOWN
            OnTouchDown(pointerId, x, y, pressure, 1.0f);
            break;

        case 1:  // ACTION_UP
        case 6:  // ACTION_POINTER_UP
            OnTouchUp(pointerId, x, y);
            break;

        case 2:  // ACTION_MOVE
            OnTouchMove(pointerId, x, y, pressure, 1.0f);
            break;

        case 3:  // ACTION_CANCEL
            OnTouchCancel();
            break;
    }

    if (m_gestureRecognitionEnabled) {
        UpdateGestureState();
    }
}

void AndroidTouchInput::OnTouchDown(int32_t pointerId, float x, float y, float pressure, float size) {
    int64_t currentTime = GetCurrentTimeMs();

    TouchState& touch = m_touches[pointerId];
    touch.pointerId = pointerId;
    touch.position = glm::vec2(x, y);
    touch.startPosition = touch.position;
    touch.previousPosition = touch.position;
    touch.delta = glm::vec2(0.0f);
    touch.pressure = pressure;
    touch.size = size;
    touch.active = true;
    touch.justPressed = true;
    touch.justReleased = false;
    touch.startTime = currentTime;
    touch.lastUpdateTime = currentTime;

    // Track touch order
    m_touchOrder.push_back(pointerId);
    m_activeTouchCount++;

    // Reset long press tracking for new primary touch
    if (m_activeTouchCount == 1) {
        m_longPressTriggered = false;
    }

    // Initialize pinch/rotation tracking when second finger touches
    if (m_activeTouchCount == 2) {
        auto touches = GetAllTouches();
        if (touches.size() >= 2) {
            m_initialPinchCenter = (touches[0]->position + touches[1]->position) * 0.5f;
            m_initialPinchDistance = CalculateDistance(touches[0]->position, touches[1]->position);
            m_initialPinchAngle = CalculateAngle(
                touches[1]->position - touches[0]->position,
                glm::vec2(1.0f, 0.0f)
            );
            m_pinchScale = 1.0f;
            m_rotationAngle = 0.0f;
        }
    }

    // Invoke callback
    if (m_touchCallback) {
        m_touchCallback(touch, TouchAction::Down);
    }

    NOVA_LOGD("Touch down: id=%d pos=(%.1f, %.1f) count=%d",
              pointerId, x, y, m_activeTouchCount);
}

void AndroidTouchInput::OnTouchUp(int32_t pointerId, float x, float y) {
    auto it = m_touches.find(pointerId);
    if (it == m_touches.end() || !it->second.active) {
        return;
    }

    TouchState& touch = it->second;
    touch.position = glm::vec2(x, y);
    touch.active = false;
    touch.justReleased = true;
    touch.lastUpdateTime = GetCurrentTimeMs();

    // Check for tap
    if (m_activeTouchCount == 1 && !m_longPressTriggered) {
        int64_t duration = touch.lastUpdateTime - touch.startTime;
        float distance = CalculateDistance(touch.position, touch.startPosition);

        if (duration < m_gestureConfig.tapMaxDuration &&
            distance < m_gestureConfig.tapMaxDistance) {

            // Check for double tap
            int64_t timeSinceLastTap = touch.lastUpdateTime - m_lastTapTime;
            float distanceFromLastTap = CalculateDistance(touch.position, m_lastTapPosition);

            if (timeSinceLastTap < m_gestureConfig.doubleTapMaxInterval &&
                distanceFromLastTap < m_gestureConfig.doubleTapMaxDistance) {
                m_wasDoubleTapped = true;
                CompleteGesture(GestureType::DoubleTap);
            } else {
                m_wasTapped = true;
                CompleteGesture(GestureType::Tap);
            }

            m_lastTapTime = touch.lastUpdateTime;
            m_lastTapPosition = touch.position;
        }
    }

    // Calculate final velocity for swipe detection
    m_swipeVelocity = CalculateVelocity(touch);

    // Remove from touch order
    auto orderIt = std::find(m_touchOrder.begin(), m_touchOrder.end(), pointerId);
    if (orderIt != m_touchOrder.end()) {
        m_touchOrder.erase(orderIt);
    }
    m_activeTouchCount--;

    // Invoke callback
    if (m_touchCallback) {
        m_touchCallback(touch, TouchAction::Up);
    }

    NOVA_LOGD("Touch up: id=%d pos=(%.1f, %.1f) count=%d",
              pointerId, x, y, m_activeTouchCount);
}

void AndroidTouchInput::OnTouchMove(int32_t pointerId, float x, float y, float pressure, float size) {
    auto it = m_touches.find(pointerId);
    if (it == m_touches.end() || !it->second.active) {
        return;
    }

    TouchState& touch = it->second;
    touch.previousPosition = touch.position;
    touch.position = glm::vec2(x, y);
    touch.delta = touch.position - touch.previousPosition;
    touch.pressure = pressure;
    touch.size = size;
    touch.lastUpdateTime = GetCurrentTimeMs();

    // Update pinch/rotation for two-finger gestures
    if (m_activeTouchCount == 2) {
        auto touches = GetAllTouches();
        if (touches.size() >= 2 && m_initialPinchDistance > 0.0f) {
            float currentDistance = CalculateDistance(touches[0]->position, touches[1]->position);
            m_pinchScale = currentDistance / m_initialPinchDistance;

            float currentAngle = CalculateAngle(
                touches[1]->position - touches[0]->position,
                glm::vec2(1.0f, 0.0f)
            );
            m_rotationAngle = currentAngle - m_initialPinchAngle;
        }
    }

    // Invoke callback
    if (m_touchCallback) {
        m_touchCallback(touch, TouchAction::Move);
    }
}

void AndroidTouchInput::OnTouchCancel() {
    // Cancel all active touches
    for (auto& [id, touch] : m_touches) {
        if (touch.active) {
            touch.active = false;
            touch.justReleased = true;

            if (m_touchCallback) {
                m_touchCallback(touch, TouchAction::Cancel);
            }
        }
    }

    m_touchOrder.clear();
    m_activeTouchCount = 0;
    m_currentGesture = GestureType::None;
    m_gestureInProgress = false;
    m_longPressTriggered = false;

    NOVA_LOGD("Touch cancel");
}

// -------------------------------------------------------------------------
// Frame Update
// -------------------------------------------------------------------------

void AndroidTouchInput::Update() {
    // Clear per-frame flags
    m_wasTapped = false;
    m_wasDoubleTapped = false;
    m_wasLongPressed = false;

    // Clear justPressed/justReleased flags
    for (auto& [id, touch] : m_touches) {
        touch.justPressed = false;
        touch.justReleased = false;
        touch.delta = glm::vec2(0.0f);
    }

    // Clean up inactive touches
    for (auto it = m_touches.begin(); it != m_touches.end(); ) {
        if (!it->second.active) {
            it = m_touches.erase(it);
        } else {
            ++it;
        }
    }

    // Check for long press
    if (m_activeTouchCount == 1 && !m_longPressTriggered) {
        const TouchState* touch = GetPrimaryTouch();
        if (touch) {
            int64_t duration = GetCurrentTimeMs() - touch->startTime;
            float distance = CalculateDistance(touch->position, touch->startPosition);

            if (duration >= m_gestureConfig.longPressMinDuration &&
                distance < m_gestureConfig.tapMaxDistance) {
                m_wasLongPressed = true;
                m_longPressTriggered = true;
                CompleteGesture(GestureType::LongPress);
            }
        }
    }

    // Update ongoing gesture tracking
    if (m_gestureRecognitionEnabled && m_activeTouchCount > 0) {
        DetectGesture();
    } else if (m_activeTouchCount == 0) {
        m_currentGesture = GestureType::None;
        m_gestureInProgress = false;
    }
}

void AndroidTouchInput::Reset() {
    m_touches.clear();
    m_touchOrder.clear();
    m_activeTouchCount = 0;
    m_currentGesture = GestureType::None;
    m_gestureEvent = GestureEvent{};
    m_pinchScale = 1.0f;
    m_rotationAngle = 0.0f;
    m_swipeVelocity = glm::vec2(0.0f);
    m_wasTapped = false;
    m_wasDoubleTapped = false;
    m_wasLongPressed = false;
    m_longPressTriggered = false;
    m_gestureInProgress = false;
}

// -------------------------------------------------------------------------
// Touch State Queries
// -------------------------------------------------------------------------

const TouchState* AndroidTouchInput::GetTouch(int index) const {
    if (index < 0 || index >= static_cast<int>(m_touchOrder.size())) {
        return nullptr;
    }

    auto it = m_touches.find(m_touchOrder[index]);
    return (it != m_touches.end() && it->second.active) ? &it->second : nullptr;
}

const TouchState* AndroidTouchInput::GetTouchByPointerId(int32_t pointerId) const {
    auto it = m_touches.find(pointerId);
    return (it != m_touches.end() && it->second.active) ? &it->second : nullptr;
}

const TouchState* AndroidTouchInput::GetPrimaryTouch() const {
    return GetTouch(0);
}

std::vector<const TouchState*> AndroidTouchInput::GetAllTouches() const {
    std::vector<const TouchState*> result;
    result.reserve(m_activeTouchCount);

    for (int32_t pointerId : m_touchOrder) {
        auto it = m_touches.find(pointerId);
        if (it != m_touches.end() && it->second.active) {
            result.push_back(&it->second);
        }
    }

    return result;
}

bool AndroidTouchInput::IsPinching() const {
    return m_activeTouchCount >= 2 &&
           std::abs(m_pinchScale - 1.0f) > m_gestureConfig.pinchMinScale;
}

// -------------------------------------------------------------------------
// Gesture Detection
// -------------------------------------------------------------------------

void AndroidTouchInput::UpdateGestureState() {
    // This is called after each touch event
    // Real-time gesture updates can be done here
}

GestureType AndroidTouchInput::DetectGesture() {
    if (m_activeTouchCount == 0) {
        return GestureType::None;
    }

    if (m_activeTouchCount == 1) {
        DetectSwipeGesture();
    } else if (m_activeTouchCount == 2) {
        DetectPinchGesture();
        DetectRotationGesture();
    } else if (m_activeTouchCount >= 3) {
        // Three finger gesture detection could go here
    }

    return m_currentGesture;
}

void AndroidTouchInput::DetectTapGesture() {
    // Tap detection is handled in OnTouchUp for better timing
}

void AndroidTouchInput::DetectSwipeGesture() {
    const TouchState* touch = GetPrimaryTouch();
    if (!touch) return;

    float distance = CalculateDistance(touch->position, touch->startPosition);
    glm::vec2 velocity = CalculateVelocity(*touch);
    float speed = glm::length(velocity);

    if (distance >= m_gestureConfig.swipeMinDistance &&
        speed >= m_gestureConfig.swipeMinVelocity) {

        glm::vec2 direction = glm::normalize(touch->position - touch->startPosition);

        // Determine swipe direction
        GestureType swipeType = GestureType::Swipe;
        float absX = std::abs(direction.x);
        float absY = std::abs(direction.y);

        if (absX > absY) {
            swipeType = (direction.x > 0) ? GestureType::SwipeRight : GestureType::SwipeLeft;
        } else {
            swipeType = (direction.y > 0) ? GestureType::SwipeDown : GestureType::SwipeUp;
        }

        m_currentGesture = swipeType;
        m_gestureInProgress = true;

        m_gestureEvent.type = swipeType;
        m_gestureEvent.position = touch->position;
        m_gestureEvent.startPosition = touch->startPosition;
        m_gestureEvent.delta = touch->position - touch->startPosition;
        m_gestureEvent.velocity = velocity;
        m_gestureEvent.fingerCount = 1;
    }
}

void AndroidTouchInput::DetectPinchGesture() {
    if (m_activeTouchCount < 2) return;

    float scaleDelta = std::abs(m_pinchScale - 1.0f);
    if (scaleDelta >= m_gestureConfig.pinchMinScale) {
        m_currentGesture = GestureType::Pinch;
        m_gestureInProgress = true;

        auto touches = GetAllTouches();
        if (touches.size() >= 2) {
            m_gestureEvent.type = GestureType::Pinch;
            m_gestureEvent.position = (touches[0]->position + touches[1]->position) * 0.5f;
            m_gestureEvent.scale = m_pinchScale;
            m_gestureEvent.fingerCount = 2;
        }

        if (m_gestureCallback) {
            m_gestureCallback(m_gestureEvent);
        }
    }
}

void AndroidTouchInput::DetectRotationGesture() {
    if (m_activeTouchCount < 2) return;

    float rotationDegrees = glm::degrees(std::abs(m_rotationAngle));
    if (rotationDegrees >= m_gestureConfig.rotateMinAngle) {
        m_currentGesture = GestureType::Rotate;
        m_gestureInProgress = true;

        auto touches = GetAllTouches();
        if (touches.size() >= 2) {
            m_gestureEvent.type = GestureType::Rotate;
            m_gestureEvent.position = (touches[0]->position + touches[1]->position) * 0.5f;
            m_gestureEvent.rotation = m_rotationAngle;
            m_gestureEvent.fingerCount = 2;
        }

        if (m_gestureCallback) {
            m_gestureCallback(m_gestureEvent);
        }
    }
}

void AndroidTouchInput::CompleteGesture(GestureType type) {
    const TouchState* touch = GetPrimaryTouch();
    if (!touch && type != GestureType::Pinch && type != GestureType::Rotate) {
        // For single-finger gestures, use the last known touch
        if (!m_touches.empty()) {
            touch = &m_touches.begin()->second;
        }
    }

    m_gestureEvent.type = type;
    m_gestureEvent.isComplete = true;

    if (touch) {
        m_gestureEvent.position = touch->position;
        m_gestureEvent.startPosition = touch->startPosition;
        m_gestureEvent.delta = touch->position - touch->startPosition;
        m_gestureEvent.velocity = m_swipeVelocity;
        m_gestureEvent.duration = GetCurrentTimeMs() - touch->startTime;
        m_gestureEvent.fingerCount = m_activeTouchCount > 0 ? m_activeTouchCount : 1;
    }

    if (m_gestureCallback) {
        m_gestureCallback(m_gestureEvent);
    }

    NOVA_LOGD("Gesture complete: type=%d pos=(%.1f, %.1f)",
              static_cast<int>(type),
              m_gestureEvent.position.x, m_gestureEvent.position.y);
}

// -------------------------------------------------------------------------
// InputManager Integration
// -------------------------------------------------------------------------

void AndroidTouchInput::UpdateInputManager(InputManager& input) {
    // This would map touch to mouse-like input
    // Implementation depends on Nova3D InputManager interface

    // Example mapping:
    // - Primary touch position -> mouse position
    // - Touch down/up -> left mouse button
    // - Pinch scale -> scroll wheel

    // The actual implementation would need access to InputManager internals
    // or use a dedicated mobile input interface
}

// -------------------------------------------------------------------------
// Utility Functions
// -------------------------------------------------------------------------

int64_t AndroidTouchInput::GetCurrentTimeMs() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

float AndroidTouchInput::CalculateDistance(const glm::vec2& a, const glm::vec2& b) const {
    return glm::length(b - a);
}

float AndroidTouchInput::CalculateAngle(const glm::vec2& a, const glm::vec2& b) const {
    return std::atan2(a.y * b.x - a.x * b.y, a.x * b.x + a.y * b.y);
}

glm::vec2 AndroidTouchInput::CalculateVelocity(const TouchState& touch) const {
    int64_t duration = touch.lastUpdateTime - touch.startTime;
    if (duration <= 0) {
        return glm::vec2(0.0f);
    }

    float seconds = static_cast<float>(duration) / 1000.0f;
    return (touch.position - touch.startPosition) / seconds;
}

void AndroidTouchInput::ClearCallbacks() {
    m_touchCallback = nullptr;
    m_gestureCallback = nullptr;
}

} // namespace Nova
