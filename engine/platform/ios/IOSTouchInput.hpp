#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>

namespace Nova {

// Forward declaration
class InputManager;

/**
 * @brief Touch phase enumeration
 */
enum class TouchPhase {
    Began,
    Moved,
    Stationary,
    Ended,
    Cancelled
};

/**
 * @brief Individual touch state
 */
struct TouchState {
    int touchId = -1;
    glm::vec2 position{0.0f};
    glm::vec2 previousPosition{0.0f};
    glm::vec2 startPosition{0.0f};
    glm::vec2 delta{0.0f};
    TouchPhase phase = TouchPhase::Ended;
    float pressure = 1.0f;
    float radius = 1.0f;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdateTime;
    bool active = false;
};

/**
 * @brief Gesture types recognized by the touch system
 */
enum class GestureType {
    None,
    Tap,
    DoubleTap,
    LongPress,
    Pan,
    Pinch,
    Rotation,
    Swipe
};

/**
 * @brief Swipe direction
 */
enum class SwipeDirection {
    None,
    Left,
    Right,
    Up,
    Down
};

/**
 * @brief Gesture state data
 */
struct GestureState {
    GestureType type = GestureType::None;
    glm::vec2 position{0.0f};           // Center position
    glm::vec2 delta{0.0f};              // Movement delta
    float scale = 1.0f;                 // Pinch scale factor
    float scaleDelta = 0.0f;            // Scale change this frame
    float rotation = 0.0f;              // Rotation angle in radians
    float rotationDelta = 0.0f;         // Rotation change this frame
    SwipeDirection swipeDirection = SwipeDirection::None;
    float velocity = 0.0f;              // Gesture velocity
    int touchCount = 0;                 // Number of touches in gesture
    bool inProgress = false;            // Gesture is ongoing
    bool recognized = false;            // Gesture was recognized
};

/**
 * @brief Configuration for gesture recognition
 */
struct GestureConfig {
    // Tap configuration
    float tapMaxDistance = 20.0f;           // Max movement to still be a tap
    float tapMaxDuration = 0.3f;            // Max duration for tap in seconds
    float doubleTapMaxInterval = 0.3f;      // Max time between taps for double tap

    // Long press configuration
    float longPressMinDuration = 0.5f;      // Min duration for long press
    float longPressMaxMovement = 10.0f;     // Max movement during long press

    // Pinch configuration
    float pinchMinDistance = 10.0f;         // Min finger distance change to start pinch

    // Rotation configuration
    float rotationMinAngle = 0.1f;          // Min rotation angle to start (radians)

    // Swipe configuration
    float swipeMinDistance = 50.0f;         // Min distance for swipe
    float swipeMinVelocity = 100.0f;        // Min velocity for swipe
    float swipeMaxDuration = 0.5f;          // Max duration for swipe
};

/**
 * @brief RTS-specific touch commands
 */
struct RTSTouchCommand {
    enum class Type {
        None,
        SelectUnit,         // Single tap on unit
        SelectMultiple,     // Drag to select box
        MoveUnits,          // Tap on ground with selection
        AttackMove,         // Long press then tap
        CameraRotate,       // Two-finger rotate
        CameraZoom,         // Pinch gesture
        CameraPan,          // Two-finger pan
        OpenContextMenu,    // Long press on unit
        Deselect            // Tap on empty space
    };

    Type type = Type::None;
    glm::vec2 screenPosition{0.0f};
    glm::vec2 worldPosition{0.0f};       // Set by game code
    glm::vec2 selectionStart{0.0f};
    glm::vec2 selectionEnd{0.0f};
    float zoomDelta = 0.0f;
    float rotationDelta = 0.0f;
    bool isActive = false;
};

/**
 * @brief iOS touch input handler with gesture recognition
 *
 * Handles multi-touch input and recognizes common gestures:
 * - Tap, Double Tap, Long Press
 * - Pan, Pinch (zoom), Rotation
 * - Swipe gestures
 *
 * Also provides RTS-specific touch command translation for camera
 * control and unit selection/movement.
 */
class IOSTouchInput {
public:
    IOSTouchInput();
    ~IOSTouchInput();

    // =========================================================================
    // Touch Event Handlers (called from platform layer)
    // =========================================================================

    void HandleTouchBegan(float x, float y, int touchId);
    void HandleTouchMoved(float x, float y, int touchId);
    void HandleTouchEnded(float x, float y, int touchId);
    void HandleTouchCancelled(float x, float y, int touchId);

    // =========================================================================
    // Update
    // =========================================================================

    /**
     * @brief Update gesture recognition (call each frame)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Reset all touch and gesture state
     */
    void Reset();

    // =========================================================================
    // Touch State Queries
    // =========================================================================

    /**
     * @brief Get number of active touches
     */
    [[nodiscard]] int GetTouchCount() const;

    /**
     * @brief Get touch state by index (0 to touchCount-1)
     */
    [[nodiscard]] const TouchState* GetTouch(int index) const;

    /**
     * @brief Get touch state by touch ID
     */
    [[nodiscard]] const TouchState* GetTouchById(int touchId) const;

    /**
     * @brief Get all active touches
     */
    [[nodiscard]] const std::unordered_map<int, TouchState>& GetAllTouches() const { return m_touches; }

    /**
     * @brief Check if any touch is active
     */
    [[nodiscard]] bool HasActiveTouch() const { return !m_activeTouchIds.empty(); }

    // =========================================================================
    // Gesture State Queries
    // =========================================================================

    /**
     * @brief Get current gesture state
     */
    [[nodiscard]] const GestureState& GetGestureState() const { return m_gestureState; }

    /**
     * @brief Check if specific gesture is active
     */
    [[nodiscard]] bool IsGestureActive(GestureType type) const;

    /**
     * @brief Check if a tap occurred this frame
     */
    [[nodiscard]] bool WasTapped() const { return m_tapOccurred; }

    /**
     * @brief Check if a double tap occurred this frame
     */
    [[nodiscard]] bool WasDoubleTapped() const { return m_doubleTapOccurred; }

    /**
     * @brief Check if long press started this frame
     */
    [[nodiscard]] bool LongPressStarted() const { return m_longPressStarted; }

    /**
     * @brief Get tap position (valid if WasTapped() returns true)
     */
    [[nodiscard]] glm::vec2 GetTapPosition() const { return m_lastTapPosition; }

    // =========================================================================
    // RTS Touch Commands
    // =========================================================================

    /**
     * @brief Get current RTS touch command
     */
    [[nodiscard]] const RTSTouchCommand& GetRTSCommand() const { return m_rtsCommand; }

    /**
     * @brief Check if RTS command is pending
     */
    [[nodiscard]] bool HasRTSCommand() const { return m_rtsCommand.type != RTSTouchCommand::Type::None; }

    /**
     * @brief Clear the current RTS command (call after processing)
     */
    void ClearRTSCommand() { m_rtsCommand = RTSTouchCommand(); }

    /**
     * @brief Enable/disable selection box mode
     */
    void SetSelectionBoxEnabled(bool enabled) { m_selectionBoxEnabled = enabled; }
    [[nodiscard]] bool IsSelectionBoxEnabled() const { return m_selectionBoxEnabled; }

    /**
     * @brief Get selection box bounds (valid during drag selection)
     */
    [[nodiscard]] bool GetSelectionBox(glm::vec2& outMin, glm::vec2& outMax) const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set gesture configuration
     */
    void SetGestureConfig(const GestureConfig& config) { m_config = config; }

    /**
     * @brief Get gesture configuration
     */
    [[nodiscard]] const GestureConfig& GetGestureConfig() const { return m_config; }

    /**
     * @brief Set screen size (for coordinate normalization)
     */
    void SetScreenSize(int width, int height);

    /**
     * @brief Set display scale (for Retina displays)
     */
    void SetDisplayScale(float scale) { m_displayScale = scale; }

    // =========================================================================
    // InputManager Integration
    // =========================================================================

    /**
     * @brief Translate touch input to InputManager actions
     * @param input InputManager to update
     */
    void UpdateInputManager(InputManager& input);

    // =========================================================================
    // Callbacks
    // =========================================================================

    using TapCallback = std::function<void(const glm::vec2& position)>;
    using DoubleTapCallback = std::function<void(const glm::vec2& position)>;
    using LongPressCallback = std::function<void(const glm::vec2& position)>;
    using PinchCallback = std::function<void(float scale, float delta, const glm::vec2& center)>;
    using RotationCallback = std::function<void(float rotation, float delta, const glm::vec2& center)>;
    using PanCallback = std::function<void(const glm::vec2& delta, const glm::vec2& position)>;

    void SetTapCallback(TapCallback callback) { m_tapCallback = std::move(callback); }
    void SetDoubleTapCallback(DoubleTapCallback callback) { m_doubleTapCallback = std::move(callback); }
    void SetLongPressCallback(LongPressCallback callback) { m_longPressCallback = std::move(callback); }
    void SetPinchCallback(PinchCallback callback) { m_pinchCallback = std::move(callback); }
    void SetRotationCallback(RotationCallback callback) { m_rotationCallback = std::move(callback); }
    void SetPanCallback(PanCallback callback) { m_panCallback = std::move(callback); }

private:
    // Gesture detection methods
    void DetectTap();
    void DetectDoubleTap();
    void DetectLongPress(float deltaTime);
    void DetectPinch();
    void DetectRotation();
    void DetectPan();
    void DetectSwipe();

    // RTS command generation
    void UpdateRTSCommands();

    // Helper methods
    glm::vec2 CalculateTouchCenter() const;
    float CalculateTouchDistance() const;
    float CalculateTouchAngle() const;
    void ClearFrameFlags();

    // Touch state
    std::unordered_map<int, TouchState> m_touches;
    std::vector<int> m_activeTouchIds;

    // Gesture state
    GestureState m_gestureState;
    GestureConfig m_config;

    // Tap detection
    bool m_tapOccurred = false;
    bool m_doubleTapOccurred = false;
    bool m_longPressStarted = false;
    bool m_longPressActive = false;
    glm::vec2 m_lastTapPosition{0.0f};
    std::chrono::steady_clock::time_point m_lastTapTime;
    int m_consecutiveTaps = 0;

    // Pinch/Rotation state
    float m_initialPinchDistance = 0.0f;
    float m_currentPinchDistance = 0.0f;
    float m_initialRotationAngle = 0.0f;
    float m_currentRotationAngle = 0.0f;
    bool m_pinchActive = false;
    bool m_rotationActive = false;

    // Pan state
    glm::vec2 m_panStartPosition{0.0f};
    bool m_panActive = false;

    // RTS state
    RTSTouchCommand m_rtsCommand;
    bool m_selectionBoxEnabled = false;
    bool m_selectionBoxActive = false;
    glm::vec2 m_selectionBoxStart{0.0f};
    glm::vec2 m_selectionBoxEnd{0.0f};

    // Screen properties
    int m_screenWidth = 1;
    int m_screenHeight = 1;
    float m_displayScale = 1.0f;

    // Callbacks
    TapCallback m_tapCallback;
    DoubleTapCallback m_doubleTapCallback;
    LongPressCallback m_longPressCallback;
    PinchCallback m_pinchCallback;
    RotationCallback m_rotationCallback;
    PanCallback m_panCallback;
};

} // namespace Nova
