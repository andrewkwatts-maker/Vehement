#pragma once

/**
 * @file AndroidTouchInput.hpp
 * @brief Multi-touch input handling for Android
 *
 * Provides comprehensive touch input handling including:
 * - Multi-touch tracking
 * - Gesture recognition (tap, swipe, pinch, rotate)
 * - Touch state management
 * - Integration with Nova3D InputManager
 */

#include <android/input.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

namespace Nova {

// Forward declaration
class InputManager;

/**
 * @brief Touch action types
 */
enum class TouchAction {
    Down,       ///< Finger touched screen
    Up,         ///< Finger lifted from screen
    Move,       ///< Finger moved on screen
    Cancel,     ///< Touch cancelled (e.g., system gesture)
    PointerDown,///< Additional finger touched
    PointerUp   ///< Additional finger lifted
};

/**
 * @brief Recognized gesture types
 */
enum class GestureType {
    None,
    Tap,            ///< Single tap
    DoubleTap,      ///< Double tap
    LongPress,      ///< Long press (hold)
    Swipe,          ///< Single finger swipe
    SwipeLeft,      ///< Swipe to the left
    SwipeRight,     ///< Swipe to the right
    SwipeUp,        ///< Swipe upward
    SwipeDown,      ///< Swipe downward
    Pinch,          ///< Two finger pinch (zoom)
    Rotate,         ///< Two finger rotation
    Pan,            ///< Two finger pan
    ThreeFingerSwipe///< Three finger swipe
};

/**
 * @brief Individual touch point state
 */
struct TouchState {
    int32_t pointerId = -1;         ///< Unique pointer ID
    glm::vec2 position{0.0f};       ///< Current position in screen coordinates
    glm::vec2 startPosition{0.0f};  ///< Position when touch started
    glm::vec2 previousPosition{0.0f};///< Position from previous frame
    glm::vec2 delta{0.0f};          ///< Movement since last frame
    float pressure = 0.0f;          ///< Touch pressure (0-1)
    float size = 0.0f;              ///< Touch area size
    bool active = false;            ///< Is this touch currently active?
    bool justPressed = false;       ///< Did this touch just start this frame?
    bool justReleased = false;      ///< Did this touch just end this frame?
    int64_t startTime = 0;          ///< Timestamp when touch started (ms)
    int64_t lastUpdateTime = 0;     ///< Last update timestamp (ms)
};

/**
 * @brief Gesture event data
 */
struct GestureEvent {
    GestureType type = GestureType::None;
    glm::vec2 position{0.0f};       ///< Center position of gesture
    glm::vec2 startPosition{0.0f};  ///< Starting position
    glm::vec2 delta{0.0f};          ///< Movement delta
    glm::vec2 velocity{0.0f};       ///< Velocity at gesture end
    float scale = 1.0f;             ///< Pinch scale factor
    float rotation = 0.0f;          ///< Rotation angle in radians
    int fingerCount = 0;            ///< Number of fingers involved
    int64_t duration = 0;           ///< Duration in milliseconds
    bool isComplete = false;        ///< Is gesture complete?
};

/**
 * @brief Gesture recognition configuration
 */
struct GestureConfig {
    float tapMaxDistance = 20.0f;           ///< Max movement for tap (pixels)
    int64_t tapMaxDuration = 250;           ///< Max duration for tap (ms)
    int64_t doubleTapMaxInterval = 300;     ///< Max time between taps (ms)
    float doubleTapMaxDistance = 50.0f;     ///< Max distance between taps (pixels)
    int64_t longPressMinDuration = 500;     ///< Min duration for long press (ms)
    float swipeMinDistance = 100.0f;        ///< Min distance for swipe (pixels)
    float swipeMinVelocity = 500.0f;        ///< Min velocity for swipe (pixels/sec)
    float swipeMaxAngleDeviation = 30.0f;   ///< Max angle deviation (degrees)
    float pinchMinScale = 0.1f;             ///< Min scale change for pinch
    float rotateMinAngle = 10.0f;           ///< Min rotation angle (degrees)
};

/**
 * @brief Multi-touch input handler for Android
 *
 * Tracks multiple simultaneous touch points and recognizes common gestures.
 * Thread-safe for callback invocation.
 */
class AndroidTouchInput {
public:
    /**
     * @brief Touch event callback type
     */
    using TouchCallback = std::function<void(const TouchState& touch, TouchAction action)>;

    /**
     * @brief Gesture event callback type
     */
    using GestureCallback = std::function<void(const GestureEvent& gesture)>;

    AndroidTouchInput();
    ~AndroidTouchInput() = default;

    // Non-copyable, non-movable
    AndroidTouchInput(const AndroidTouchInput&) = delete;
    AndroidTouchInput& operator=(const AndroidTouchInput&) = delete;
    AndroidTouchInput(AndroidTouchInput&&) = delete;
    AndroidTouchInput& operator=(AndroidTouchInput&&) = delete;

    /**
     * @brief Process an Android motion event
     * @param event The input event from Android
     * @return 1 if event was handled, 0 otherwise
     */
    int32_t HandleMotionEvent(const AInputEvent* event);

    /**
     * @brief Process a touch event from JNI
     * @param action Touch action (from MotionEvent)
     * @param x X coordinate
     * @param y Y coordinate
     * @param pointerId Pointer ID
     * @param pressure Touch pressure
     */
    void HandleTouchEvent(int action, float x, float y, int pointerId, float pressure = 1.0f);

    /**
     * @brief Update gesture detection state
     * Called once per frame before processing input
     */
    void Update();

    /**
     * @brief Clear all touch states and reset gesture detection
     */
    void Reset();

    // -------------------------------------------------------------------------
    // Touch State Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of active touches
     */
    [[nodiscard]] int GetTouchCount() const { return m_activeTouchCount; }

    /**
     * @brief Check if any touch is active
     */
    [[nodiscard]] bool IsTouching() const { return m_activeTouchCount > 0; }

    /**
     * @brief Get touch state by index (0 = first touch)
     * @param index Touch index (0 to GetTouchCount()-1)
     * @return Touch state or nullptr if index is invalid
     */
    [[nodiscard]] const TouchState* GetTouch(int index) const;

    /**
     * @brief Get touch state by pointer ID
     * @param pointerId The Android pointer ID
     * @return Touch state or nullptr if not found
     */
    [[nodiscard]] const TouchState* GetTouchByPointerId(int32_t pointerId) const;

    /**
     * @brief Get the primary touch (first finger)
     * @return Touch state or nullptr if no touches
     */
    [[nodiscard]] const TouchState* GetPrimaryTouch() const;

    /**
     * @brief Get all active touches
     */
    [[nodiscard]] std::vector<const TouchState*> GetAllTouches() const;

    /**
     * @brief Check if screen was just tapped this frame
     */
    [[nodiscard]] bool WasTapped() const { return m_wasTapped; }

    /**
     * @brief Check if screen was double-tapped this frame
     */
    [[nodiscard]] bool WasDoubleTapped() const { return m_wasDoubleTapped; }

    /**
     * @brief Check if long press was detected this frame
     */
    [[nodiscard]] bool WasLongPressed() const { return m_wasLongPressed; }

    // -------------------------------------------------------------------------
    // Gesture Detection
    // -------------------------------------------------------------------------

    /**
     * @brief Get the current gesture type
     */
    [[nodiscard]] GestureType GetCurrentGesture() const { return m_currentGesture; }

    /**
     * @brief Get the current gesture event data
     */
    [[nodiscard]] const GestureEvent& GetGestureEvent() const { return m_gestureEvent; }

    /**
     * @brief Check if a pinch gesture is active
     */
    [[nodiscard]] bool IsPinching() const;

    /**
     * @brief Get current pinch scale (1.0 = no scale change)
     */
    [[nodiscard]] float GetPinchScale() const { return m_pinchScale; }

    /**
     * @brief Get current rotation angle in radians
     */
    [[nodiscard]] float GetRotationAngle() const { return m_rotationAngle; }

    /**
     * @brief Get swipe velocity (if swiping)
     */
    [[nodiscard]] glm::vec2 GetSwipeVelocity() const { return m_swipeVelocity; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Set gesture recognition configuration
     */
    void SetGestureConfig(const GestureConfig& config) { m_gestureConfig = config; }

    /**
     * @brief Get current gesture configuration
     */
    [[nodiscard]] const GestureConfig& GetGestureConfig() const { return m_gestureConfig; }

    /**
     * @brief Enable or disable gesture recognition
     */
    void SetGestureRecognitionEnabled(bool enabled) { m_gestureRecognitionEnabled = enabled; }

    /**
     * @brief Check if gesture recognition is enabled
     */
    [[nodiscard]] bool IsGestureRecognitionEnabled() const { return m_gestureRecognitionEnabled; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Set callback for touch events
     */
    void SetTouchCallback(TouchCallback callback) { m_touchCallback = std::move(callback); }

    /**
     * @brief Set callback for gesture events
     */
    void SetGestureCallback(GestureCallback callback) { m_gestureCallback = std::move(callback); }

    /**
     * @brief Clear all callbacks
     */
    void ClearCallbacks();

    // -------------------------------------------------------------------------
    // Integration with Nova3D InputManager
    // -------------------------------------------------------------------------

    /**
     * @brief Convert touch input to InputManager mouse events
     * @param input The InputManager to update
     *
     * Maps primary touch to mouse position and left button.
     * Useful for games that need mouse-like input.
     */
    void UpdateInputManager(InputManager& input);

private:
    // Touch handling
    void OnTouchDown(int32_t pointerId, float x, float y, float pressure, float size);
    void OnTouchUp(int32_t pointerId, float x, float y);
    void OnTouchMove(int32_t pointerId, float x, float y, float pressure, float size);
    void OnTouchCancel();

    // Gesture detection
    void UpdateGestureState();
    GestureType DetectGesture();
    void DetectTapGesture();
    void DetectSwipeGesture();
    void DetectPinchGesture();
    void DetectRotationGesture();
    void CompleteGesture(GestureType type);

    // Utility functions
    int64_t GetCurrentTimeMs() const;
    float CalculateDistance(const glm::vec2& a, const glm::vec2& b) const;
    float CalculateAngle(const glm::vec2& a, const glm::vec2& b) const;
    glm::vec2 CalculateVelocity(const TouchState& touch) const;

    // Touch state storage
    std::unordered_map<int32_t, TouchState> m_touches;
    std::vector<int32_t> m_touchOrder;  // Order of touch points
    int m_activeTouchCount = 0;

    // Gesture state
    GestureConfig m_gestureConfig;
    GestureType m_currentGesture = GestureType::None;
    GestureEvent m_gestureEvent;

    // Two-finger gesture tracking
    glm::vec2 m_initialPinchCenter{0.0f};
    float m_initialPinchDistance = 0.0f;
    float m_initialPinchAngle = 0.0f;
    float m_pinchScale = 1.0f;
    float m_rotationAngle = 0.0f;

    // Swipe tracking
    glm::vec2 m_swipeVelocity{0.0f};

    // Tap detection
    int64_t m_lastTapTime = 0;
    glm::vec2 m_lastTapPosition{0.0f};
    bool m_wasTapped = false;
    bool m_wasDoubleTapped = false;
    bool m_wasLongPressed = false;
    bool m_longPressTriggered = false;

    // State flags
    bool m_gestureRecognitionEnabled = true;
    bool m_gestureInProgress = false;

    // Callbacks
    TouchCallback m_touchCallback;
    GestureCallback m_gestureCallback;
};

} // namespace Nova
