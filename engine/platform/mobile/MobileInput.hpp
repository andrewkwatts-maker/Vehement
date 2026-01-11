#pragma once

/**
 * @file MobileInput.hpp
 * @brief Unified mobile input handling (touch, gestures, sensors)
 *
 * Provides a platform-agnostic input interface for mobile platforms
 * (iOS, Android). Includes touch handling, gesture recognition, and
 * sensor access (accelerometer, gyroscope, compass).
 */

#include <glm/glm.hpp>
#include <array>
#include <vector>
#include <functional>
#include <cstdint>
#include <chrono>

namespace Nova {

// =============================================================================
// Touch Input
// =============================================================================

/**
 * @brief Touch phase (lifecycle state)
 */
enum class TouchPhase {
    Began,      // Touch just started
    Moved,      // Touch moved
    Stationary, // Touch hasn't moved
    Ended,      // Touch ended (finger lifted)
    Cancelled   // Touch cancelled (interrupted)
};

/**
 * @brief Touch type
 */
enum class TouchType {
    Direct,     // Direct finger touch
    Indirect,   // Indirect (e.g., Apple Pencil)
    Stylus,     // Stylus/pen input
    Pencil      // Apple Pencil specifically
};

/**
 * @brief Individual touch point data
 */
struct Touch {
    int64_t id = 0;                         // Unique identifier for this touch
    glm::vec2 position{0.0f};               // Current position in points
    glm::vec2 previousPosition{0.0f};       // Previous position
    glm::vec2 startPosition{0.0f};          // Initial touch position
    TouchPhase phase = TouchPhase::Began;
    TouchType type = TouchType::Direct;
    float pressure = 1.0f;                  // 0.0 to 1.0 (if supported)
    float radius = 0.0f;                    // Touch radius in points
    float radiusTolerance = 0.0f;           // Radius uncertainty
    float force = 0.0f;                     // Force/pressure (3D Touch/Force Touch)
    float maxForce = 1.0f;                  // Maximum possible force
    float azimuthAngle = 0.0f;              // Stylus azimuth (radians)
    float altitudeAngle = 0.0f;             // Stylus altitude (radians)
    uint64_t timestamp = 0;                 // Touch timestamp in milliseconds
    int tapCount = 0;                       // Number of taps (for tap detection)

    /**
     * @brief Get movement delta since last frame
     */
    [[nodiscard]] glm::vec2 GetDelta() const noexcept {
        return position - previousPosition;
    }

    /**
     * @brief Get total movement from start
     */
    [[nodiscard]] glm::vec2 GetTotalDelta() const noexcept {
        return position - startPosition;
    }

    /**
     * @brief Check if this is a new touch
     */
    [[nodiscard]] bool IsNew() const noexcept {
        return phase == TouchPhase::Began;
    }

    /**
     * @brief Check if touch has ended
     */
    [[nodiscard]] bool HasEnded() const noexcept {
        return phase == TouchPhase::Ended || phase == TouchPhase::Cancelled;
    }
};

// =============================================================================
// Gesture Recognition
// =============================================================================

/**
 * @brief Gesture types
 */
enum class GestureType {
    None,
    Tap,
    DoubleTap,
    LongPress,
    Pan,
    Pinch,
    Rotation,
    Swipe,
    EdgeSwipe  // Swipe from screen edge
};

/**
 * @brief Gesture state
 */
enum class GestureState {
    Possible,   // Gesture may be recognized
    Began,      // Gesture started
    Changed,    // Gesture updated
    Ended,      // Gesture completed
    Cancelled,  // Gesture cancelled
    Failed      // Gesture not recognized
};

/**
 * @brief Swipe direction
 */
enum class SwipeDirection {
    None = 0,
    Left = 1 << 0,
    Right = 1 << 1,
    Up = 1 << 2,
    Down = 1 << 3
};

inline SwipeDirection operator|(SwipeDirection a, SwipeDirection b) noexcept {
    return static_cast<SwipeDirection>(static_cast<int>(a) | static_cast<int>(b));
}

inline bool HasDirection(SwipeDirection flags, SwipeDirection dir) noexcept {
    return (static_cast<int>(flags) & static_cast<int>(dir)) != 0;
}

/**
 * @brief Gesture event data
 */
struct GestureEvent {
    GestureType type = GestureType::None;
    GestureState state = GestureState::Possible;

    // Common properties
    glm::vec2 position{0.0f};        // Center position
    glm::vec2 velocity{0.0f};        // Velocity in points/second
    int touchCount = 0;              // Number of touches involved

    // Tap-specific
    int tapCount = 0;

    // Pan-specific
    glm::vec2 translation{0.0f};     // Total translation

    // Pinch-specific
    float scale = 1.0f;              // Current scale factor
    float velocity_scale = 0.0f;     // Scale velocity

    // Rotation-specific
    float rotation = 0.0f;           // Rotation in radians
    float velocity_rotation = 0.0f;  // Rotation velocity

    // Swipe-specific
    SwipeDirection direction = SwipeDirection::None;

    // Long press-specific
    float duration = 0.0f;           // Press duration in seconds
};

// =============================================================================
// Sensor Data
// =============================================================================

/**
 * @brief Accelerometer data
 */
struct AccelerometerData {
    glm::vec3 acceleration{0.0f};    // Raw acceleration (g)
    glm::vec3 gravity{0.0f, -1.0f, 0.0f};  // Gravity vector
    glm::vec3 userAcceleration{0.0f}; // User acceleration (sans gravity)
    uint64_t timestamp = 0;
    bool available = false;
};

/**
 * @brief Gyroscope data
 */
struct GyroscopeData {
    glm::vec3 rotationRate{0.0f};    // Rotation rate (rad/s)
    uint64_t timestamp = 0;
    bool available = false;
};

/**
 * @brief Magnetometer/Compass data
 */
struct CompassData {
    float heading = 0.0f;            // Magnetic north (degrees)
    float trueHeading = 0.0f;        // True north (degrees)
    float accuracy = 0.0f;           // Heading accuracy
    glm::vec3 magneticField{0.0f};   // Raw magnetic field (microteslas)
    uint64_t timestamp = 0;
    bool available = false;
};

/**
 * @brief Device motion (fused sensor data)
 */
struct DeviceMotion {
    glm::quat attitude{1.0f, 0.0f, 0.0f, 0.0f};  // Device orientation
    glm::vec3 rotationRate{0.0f};
    glm::vec3 gravity{0.0f, -1.0f, 0.0f};
    glm::vec3 userAcceleration{0.0f};
    glm::vec3 magneticField{0.0f};
    uint64_t timestamp = 0;
    bool available = false;
};

// =============================================================================
// Mobile Input Class
// =============================================================================

/**
 * @brief Unified mobile input handler
 *
 * Provides touch input, gesture recognition, and sensor access.
 * Works across iOS and Android.
 */
class MobileInput {
public:
    MobileInput();
    ~MobileInput();

    // Non-copyable
    MobileInput(const MobileInput&) = delete;
    MobileInput& operator=(const MobileInput&) = delete;

    /**
     * @brief Initialize mobile input
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    /**
     * @brief Update input state (call at start of each frame)
     */
    void Update();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // -------------------------------------------------------------------------
    // Touch Input
    // -------------------------------------------------------------------------

    /**
     * @brief Get all active touches
     */
    [[nodiscard]] const std::vector<Touch>& GetActiveTouches() const noexcept {
        return m_touches;
    }

    /**
     * @brief Check if screen is being touched
     */
    [[nodiscard]] bool IsTouching() const noexcept {
        return !m_touches.empty();
    }

    /**
     * @brief Get number of active touches
     */
    [[nodiscard]] int GetTouchCount() const noexcept {
        return static_cast<int>(m_touches.size());
    }

    /**
     * @brief Get touch by index
     * @param index Touch index (0 to GetTouchCount()-1)
     * @return Pointer to touch or nullptr if invalid index
     */
    [[nodiscard]] const Touch* GetTouch(int index) const noexcept;

    /**
     * @brief Get touch by ID
     * @param id Touch identifier
     * @return Pointer to touch or nullptr if not found
     */
    [[nodiscard]] const Touch* GetTouchById(int64_t id) const noexcept;

    /**
     * @brief Get new touches this frame (phase == Began)
     */
    [[nodiscard]] std::vector<const Touch*> GetNewTouches() const;

    /**
     * @brief Get ended touches this frame
     */
    [[nodiscard]] std::vector<const Touch*> GetEndedTouches() const;

    /**
     * @brief Get the primary touch (first touch)
     */
    [[nodiscard]] const Touch* GetPrimaryTouch() const noexcept;

    // -------------------------------------------------------------------------
    // Gesture Recognition
    // -------------------------------------------------------------------------

    /**
     * @brief Check if tap occurred this frame
     */
    [[nodiscard]] bool WasTapped() const noexcept { return m_wasTapped; }

    /**
     * @brief Check if double tap occurred this frame
     */
    [[nodiscard]] bool WasDoubleTapped() const noexcept { return m_wasDoubleTapped; }

    /**
     * @brief Check if long press is active
     */
    [[nodiscard]] bool IsLongPressing() const noexcept { return m_isLongPressing; }

    /**
     * @brief Get long press duration
     */
    [[nodiscard]] float GetLongPressDuration() const noexcept { return m_longPressDuration; }

    /**
     * @brief Check if pinch gesture is active
     */
    [[nodiscard]] bool IsPinching() const noexcept { return m_isPinching; }

    /**
     * @brief Get pinch center point
     */
    [[nodiscard]] glm::vec2 GetPinchCenter() const noexcept { return m_pinchCenter; }

    /**
     * @brief Get current pinch scale (1.0 = no scale)
     */
    [[nodiscard]] float GetPinchScale() const noexcept { return m_pinchScale; }

    /**
     * @brief Get pinch scale delta this frame
     */
    [[nodiscard]] float GetPinchScaleDelta() const noexcept { return m_pinchScaleDelta; }

    /**
     * @brief Check if rotation gesture is active
     */
    [[nodiscard]] bool IsRotating() const noexcept { return m_isRotating; }

    /**
     * @brief Get rotation angle (radians)
     */
    [[nodiscard]] float GetRotationAngle() const noexcept { return m_rotationAngle; }

    /**
     * @brief Get rotation delta this frame
     */
    [[nodiscard]] float GetRotationDelta() const noexcept { return m_rotationDelta; }

    /**
     * @brief Check if panning
     */
    [[nodiscard]] bool IsPanning() const noexcept { return m_isPanning; }

    /**
     * @brief Get pan translation
     */
    [[nodiscard]] glm::vec2 GetPanDelta() const noexcept { return m_panDelta; }

    /**
     * @brief Get pan velocity
     */
    [[nodiscard]] glm::vec2 GetPanVelocity() const noexcept { return m_panVelocity; }

    /**
     * @brief Get total pan translation
     */
    [[nodiscard]] glm::vec2 GetPanTranslation() const noexcept { return m_panTranslation; }

    /**
     * @brief Check if swipe occurred this frame
     */
    [[nodiscard]] bool WasSwiped() const noexcept { return m_wasSwipe; }

    /**
     * @brief Get swipe direction
     */
    [[nodiscard]] SwipeDirection GetSwipeDirection() const noexcept { return m_swipeDirection; }

    /**
     * @brief Get all gesture events this frame
     */
    [[nodiscard]] const std::vector<GestureEvent>& GetGestureEvents() const noexcept {
        return m_gestureEvents;
    }

    // -------------------------------------------------------------------------
    // Gesture Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Set tap detection threshold
     */
    void SetTapThreshold(float maxMovement, float maxDuration) noexcept {
        m_tapMaxMovement = maxMovement;
        m_tapMaxDuration = maxDuration;
    }

    /**
     * @brief Set double tap threshold
     */
    void SetDoubleTapThreshold(float maxInterval) noexcept {
        m_doubleTapMaxInterval = maxInterval;
    }

    /**
     * @brief Set long press threshold
     */
    void SetLongPressThreshold(float minDuration) noexcept {
        m_longPressMinDuration = minDuration;
    }

    /**
     * @brief Set swipe threshold
     */
    void SetSwipeThreshold(float minVelocity) noexcept {
        m_swipeMinVelocity = minVelocity;
    }

    // -------------------------------------------------------------------------
    // Sensors
    // -------------------------------------------------------------------------

    /**
     * @brief Enable accelerometer updates
     * @param interval Update interval in seconds
     */
    void EnableAccelerometer(float interval = 0.016f);

    /**
     * @brief Disable accelerometer
     */
    void DisableAccelerometer();

    /**
     * @brief Check if accelerometer is available
     */
    [[nodiscard]] bool IsAccelerometerAvailable() const noexcept;

    /**
     * @brief Get accelerometer data
     */
    [[nodiscard]] glm::vec3 GetAccelerometer() const noexcept {
        return m_accelerometer.acceleration;
    }

    /**
     * @brief Get full accelerometer data
     */
    [[nodiscard]] const AccelerometerData& GetAccelerometerData() const noexcept {
        return m_accelerometer;
    }

    /**
     * @brief Enable gyroscope updates
     */
    void EnableGyroscope(float interval = 0.016f);

    /**
     * @brief Disable gyroscope
     */
    void DisableGyroscope();

    /**
     * @brief Check if gyroscope is available
     */
    [[nodiscard]] bool IsGyroscopeAvailable() const noexcept;

    /**
     * @brief Get gyroscope rotation rate
     */
    [[nodiscard]] glm::vec3 GetGyroscope() const noexcept {
        return m_gyroscope.rotationRate;
    }

    /**
     * @brief Get full gyroscope data
     */
    [[nodiscard]] const GyroscopeData& GetGyroscopeData() const noexcept {
        return m_gyroscope;
    }

    /**
     * @brief Enable compass/magnetometer
     */
    void EnableCompass();

    /**
     * @brief Disable compass
     */
    void DisableCompass();

    /**
     * @brief Check if compass is available
     */
    [[nodiscard]] bool IsCompassAvailable() const noexcept;

    /**
     * @brief Get compass heading (degrees, 0-360)
     */
    [[nodiscard]] float GetCompassHeading() const noexcept {
        return m_compass.heading;
    }

    /**
     * @brief Get true north heading
     */
    [[nodiscard]] float GetTrueHeading() const noexcept {
        return m_compass.trueHeading;
    }

    /**
     * @brief Get full compass data
     */
    [[nodiscard]] const CompassData& GetCompassData() const noexcept {
        return m_compass;
    }

    /**
     * @brief Enable device motion (fused sensors)
     */
    void EnableDeviceMotion(float interval = 0.016f);

    /**
     * @brief Disable device motion
     */
    void DisableDeviceMotion();

    /**
     * @brief Check if device motion is available
     */
    [[nodiscard]] bool IsDeviceMotionAvailable() const noexcept;

    /**
     * @brief Get device motion data
     */
    [[nodiscard]] const DeviceMotion& GetDeviceMotion() const noexcept {
        return m_deviceMotion;
    }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using TouchCallback = std::function<void(const Touch&)>;
    using GestureCallback = std::function<void(const GestureEvent&)>;

    void SetTouchBeganCallback(TouchCallback cb) { m_touchBeganCallback = std::move(cb); }
    void SetTouchMovedCallback(TouchCallback cb) { m_touchMovedCallback = std::move(cb); }
    void SetTouchEndedCallback(TouchCallback cb) { m_touchEndedCallback = std::move(cb); }
    void SetGestureCallback(GestureCallback cb) { m_gestureCallback = std::move(cb); }

    // -------------------------------------------------------------------------
    // Platform Integration (called by platform layer)
    // -------------------------------------------------------------------------

    /**
     * @brief Handle touch event from platform
     */
    void HandleTouchEvent(const Touch& touch);

    /**
     * @brief Update sensor data from platform
     */
    void UpdateAccelerometer(const AccelerometerData& data);
    void UpdateGyroscope(const GyroscopeData& data);
    void UpdateCompass(const CompassData& data);
    void UpdateDeviceMotion(const DeviceMotion& data);

private:
    void ProcessGestures();
    void DetectTap();
    void DetectLongPress();
    void DetectPinch();
    void DetectRotation();
    void DetectPan();
    void DetectSwipe();
    void ClearFrameState();

    bool m_initialized = false;

    // Touch state
    std::vector<Touch> m_touches;
    std::vector<Touch> m_previousTouches;

    // Gesture state
    std::vector<GestureEvent> m_gestureEvents;
    bool m_wasTapped = false;
    bool m_wasDoubleTapped = false;
    bool m_isLongPressing = false;
    float m_longPressDuration = 0.0f;
    bool m_isPinching = false;
    glm::vec2 m_pinchCenter{0.0f};
    float m_pinchScale = 1.0f;
    float m_pinchScaleDelta = 0.0f;
    float m_initialPinchDistance = 0.0f;
    bool m_isRotating = false;
    float m_rotationAngle = 0.0f;
    float m_rotationDelta = 0.0f;
    float m_initialRotationAngle = 0.0f;
    bool m_isPanning = false;
    glm::vec2 m_panDelta{0.0f};
    glm::vec2 m_panVelocity{0.0f};
    glm::vec2 m_panTranslation{0.0f};
    bool m_wasSwipe = false;
    SwipeDirection m_swipeDirection = SwipeDirection::None;

    // Tap detection
    uint64_t m_lastTapTime = 0;
    glm::vec2 m_lastTapPosition{0.0f};

    // Gesture thresholds
    float m_tapMaxMovement = 10.0f;         // Max movement for tap (points)
    float m_tapMaxDuration = 0.3f;          // Max duration for tap (seconds)
    float m_doubleTapMaxInterval = 0.3f;    // Max interval between taps
    float m_longPressMinDuration = 0.5f;    // Min duration for long press
    float m_swipeMinVelocity = 500.0f;      // Min velocity for swipe (points/sec)

    // Long press tracking
    uint64_t m_touchStartTime = 0;
    glm::vec2 m_touchStartPosition{0.0f};

    // Sensor data
    AccelerometerData m_accelerometer;
    GyroscopeData m_gyroscope;
    CompassData m_compass;
    DeviceMotion m_deviceMotion;

    bool m_accelerometerEnabled = false;
    bool m_gyroscopeEnabled = false;
    bool m_compassEnabled = false;
    bool m_deviceMotionEnabled = false;

    // Callbacks
    TouchCallback m_touchBeganCallback;
    TouchCallback m_touchMovedCallback;
    TouchCallback m_touchEndedCallback;
    GestureCallback m_gestureCallback;

    // Timing
    std::chrono::steady_clock::time_point m_lastUpdateTime;
};

} // namespace Nova
