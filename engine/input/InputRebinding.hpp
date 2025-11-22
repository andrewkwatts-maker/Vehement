#pragma once

#include "InputManager.hpp"
#include <array>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Nova {

// =============================================================================
// Gamepad Input Types
// =============================================================================

/**
 * @brief Gamepad button codes (following standard gamepad mapping)
 */
enum class GamepadButton : int {
    A = 0,              // Cross (PlayStation)
    B = 1,              // Circle
    X = 2,              // Square
    Y = 3,              // Triangle
    LeftBumper = 4,
    RightBumper = 5,
    Back = 6,           // Select/Share
    Start = 7,          // Options
    Guide = 8,          // Home/PS button
    LeftThumb = 9,      // L3
    RightThumb = 10,    // R3
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14,
    MaxButtons = 15
};

/**
 * @brief Gamepad axis codes
 */
enum class GamepadAxis : int {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,
    RightTrigger = 5,
    MaxAxes = 6
};

/**
 * @brief Input device type for per-device bindings
 */
enum class InputDevice : uint8_t {
    Keyboard,
    Mouse,
    Gamepad
};

// =============================================================================
// Extended Input Binding
// =============================================================================

/**
 * @brief Extended input binding that supports keyboard, mouse, and gamepad
 */
struct ExtendedBinding {
    InputDevice device = InputDevice::Keyboard;

    // For keyboard/mouse
    int keyOrButton = 0;
    ModifierFlags modifiers = ModifierFlags::None;

    // For gamepad
    GamepadButton gamepadButton = GamepadButton::A;
    GamepadAxis gamepadAxis = GamepadAxis::LeftX;
    float axisThreshold = 0.5f;     // For treating axis as button
    bool axisPositive = true;       // Positive or negative axis direction
    bool isAxisBinding = false;     // true if using axis as button

    // Factory methods
    static ExtendedBinding FromKey(Key key, ModifierFlags mods = ModifierFlags::None) {
        ExtendedBinding b;
        b.device = InputDevice::Keyboard;
        b.keyOrButton = static_cast<int>(key);
        b.modifiers = mods;
        return b;
    }

    static ExtendedBinding FromMouseButton(MouseButton button, ModifierFlags mods = ModifierFlags::None) {
        ExtendedBinding b;
        b.device = InputDevice::Mouse;
        b.keyOrButton = static_cast<int>(button);
        b.modifiers = mods;
        return b;
    }

    static ExtendedBinding FromGamepadButton(GamepadButton button) {
        ExtendedBinding b;
        b.device = InputDevice::Gamepad;
        b.gamepadButton = button;
        b.isAxisBinding = false;
        return b;
    }

    static ExtendedBinding FromGamepadAxis(GamepadAxis axis, bool positive, float threshold = 0.5f) {
        ExtendedBinding b;
        b.device = InputDevice::Gamepad;
        b.gamepadAxis = axis;
        b.axisPositive = positive;
        b.axisThreshold = threshold;
        b.isAxisBinding = true;
        return b;
    }

    bool operator==(const ExtendedBinding& other) const {
        if (device != other.device) return false;

        switch (device) {
            case InputDevice::Keyboard:
            case InputDevice::Mouse:
                return keyOrButton == other.keyOrButton && modifiers == other.modifiers;
            case InputDevice::Gamepad:
                if (isAxisBinding != other.isAxisBinding) return false;
                if (isAxisBinding) {
                    return gamepadAxis == other.gamepadAxis && axisPositive == other.axisPositive;
                }
                return gamepadButton == other.gamepadButton;
        }
        return false;
    }
};

// =============================================================================
// Action Definition
// =============================================================================

/**
 * @brief Defines an input action with bindings per device and metadata
 */
struct ActionDefinition {
    std::string name;
    std::string displayName;
    std::string category;           // For UI grouping (e.g., "Movement", "Combat")

    // Per-device bindings (allows different inputs for each device)
    std::vector<ExtendedBinding> keyboardBindings;
    std::vector<ExtendedBinding> mouseBindings;
    std::vector<ExtendedBinding> gamepadBindings;

    // Whether this action can be rebound by users
    bool rebindable = true;

    // For axis actions (movement, camera)
    bool isAxis = false;
    std::string positiveAction;     // For composite axis (e.g., "MoveRight")
    std::string negativeAction;     // For composite axis (e.g., "MoveLeft")
};

// =============================================================================
// Binding Conflict
// =============================================================================

/**
 * @brief Describes a binding conflict when rebinding
 */
struct BindingConflict {
    std::string existingAction;
    std::string newAction;
    ExtendedBinding binding;
    InputDevice device;

    std::string GetMessage() const {
        return "'" + binding.GetDisplayString() + "' is already bound to '" + existingAction + "'";
    }
};

// Helper method for ExtendedBinding display string
inline std::string ExtendedBinding_GetDisplayString(const ExtendedBinding& b) {
    std::string result;

    // Add modifiers for keyboard/mouse
    if (b.device == InputDevice::Keyboard || b.device == InputDevice::Mouse) {
        if (HasFlag(b.modifiers, ModifierFlags::Control)) result += "Ctrl+";
        if (HasFlag(b.modifiers, ModifierFlags::Shift)) result += "Shift+";
        if (HasFlag(b.modifiers, ModifierFlags::Alt)) result += "Alt+";
        if (HasFlag(b.modifiers, ModifierFlags::Super)) result += "Super+";
    }

    switch (b.device) {
        case InputDevice::Keyboard:
            result += KeyToString(static_cast<Key>(b.keyOrButton));
            break;
        case InputDevice::Mouse:
            result += MouseButtonToString(static_cast<MouseButton>(b.keyOrButton));
            break;
        case InputDevice::Gamepad:
            if (b.isAxisBinding) {
                static const char* axisNames[] = {
                    "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y",
                    "Left Trigger", "Right Trigger"
                };
                int idx = static_cast<int>(b.gamepadAxis);
                result += axisNames[idx < 6 ? idx : 0];
                result += b.axisPositive ? "+" : "-";
            } else {
                static const char* buttonNames[] = {
                    "A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Guide",
                    "L3", "R3", "D-Up", "D-Right", "D-Down", "D-Left"
                };
                int idx = static_cast<int>(b.gamepadButton);
                result += buttonNames[idx < 15 ? idx : 0];
            }
            break;
    }
    return result;
}

// Add display string method via extension
inline std::string ExtendedBinding::GetDisplayString() const {
    return ExtendedBinding_GetDisplayString(*this);
}

// =============================================================================
// Input Preset
// =============================================================================

/**
 * @brief A complete set of input bindings that can be saved/loaded
 */
struct InputPreset {
    std::string name;
    std::string description;
    std::unordered_map<std::string, ActionDefinition> actions;

    // Sensitivity settings
    float mouseSensitivity = 1.0f;
    float gamepadSensitivityX = 1.0f;
    float gamepadSensitivityY = 1.0f;
    float gamepadDeadzone = 0.15f;
    bool invertMouseY = false;
    bool invertGamepadY = false;
};

// =============================================================================
// Rebinding Listener
// =============================================================================

/**
 * @brief Callback interface for input rebinding UI
 */
class IRebindingListener {
public:
    virtual ~IRebindingListener() = default;

    // Called when waiting for user to press a key/button
    virtual void OnRebindStarted(const std::string& actionName, InputDevice device) = 0;

    // Called when binding is successfully captured
    virtual void OnRebindCompleted(const std::string& actionName, const ExtendedBinding& binding) = 0;

    // Called when rebinding is cancelled
    virtual void OnRebindCancelled(const std::string& actionName) = 0;

    // Called when a conflict is detected
    virtual void OnBindingConflict(const BindingConflict& conflict) = 0;
};

// =============================================================================
// Input Rebinding Manager
// =============================================================================

/**
 * @brief Manages input rebinding, persistence, and presets
 */
class InputRebinding {
public:
    static InputRebinding& Instance() {
        static InputRebinding instance;
        return instance;
    }

    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize with default actions
     * @param inputManager Reference to the main input manager
     */
    void Initialize(InputManager* inputManager) {
        m_inputManager = inputManager;
        RegisterDefaultActions();
    }

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown() {
        m_inputManager = nullptr;
        m_actions.clear();
        m_presets.clear();
    }

    // -------------------------------------------------------------------------
    // Action Registration
    // -------------------------------------------------------------------------

    /**
     * @brief Register a new action with default bindings
     */
    void RegisterAction(const ActionDefinition& action) {
        m_actions[action.name] = action;
        SyncToInputManager(action.name);
    }

    /**
     * @brief Register a simple action with just keyboard binding
     */
    void RegisterAction(const std::string& name, const std::string& displayName,
                       const std::string& category, Key defaultKey,
                       ModifierFlags mods = ModifierFlags::None) {
        ActionDefinition action;
        action.name = name;
        action.displayName = displayName;
        action.category = category;
        action.keyboardBindings.push_back(ExtendedBinding::FromKey(defaultKey, mods));
        RegisterAction(action);
    }

    /**
     * @brief Unregister an action
     */
    void UnregisterAction(const std::string& name) {
        m_actions.erase(name);
        if (m_inputManager) {
            m_inputManager->UnregisterAction(name);
        }
    }

    /**
     * @brief Get all registered actions
     */
    const std::unordered_map<std::string, ActionDefinition>& GetActions() const {
        return m_actions;
    }

    /**
     * @brief Get actions by category
     */
    std::vector<const ActionDefinition*> GetActionsByCategory(const std::string& category) const {
        std::vector<const ActionDefinition*> result;
        for (const auto& [name, action] : m_actions) {
            if (action.category == category) {
                result.push_back(&action);
            }
        }
        return result;
    }

    /**
     * @brief Get all unique categories
     */
    std::set<std::string> GetCategories() const {
        std::set<std::string> categories;
        for (const auto& [name, action] : m_actions) {
            if (!action.category.empty()) {
                categories.insert(action.category);
            }
        }
        return categories;
    }

    // -------------------------------------------------------------------------
    // Rebinding
    // -------------------------------------------------------------------------

    /**
     * @brief Start listening for input to rebind an action
     * @param actionName Action to rebind
     * @param device Device type to rebind
     * @param listener Callback listener for rebinding events
     */
    void StartRebinding(const std::string& actionName, InputDevice device,
                       IRebindingListener* listener = nullptr) {
        auto it = m_actions.find(actionName);
        if (it == m_actions.end() || !it->second.rebindable) {
            return;
        }

        m_rebindingAction = actionName;
        m_rebindingDevice = device;
        m_rebindingActive = true;
        m_rebindListener = listener;

        if (listener) {
            listener->OnRebindStarted(actionName, device);
        }
    }

    /**
     * @brief Cancel current rebinding operation
     */
    void CancelRebinding() {
        if (m_rebindingActive && m_rebindListener) {
            m_rebindListener->OnRebindCancelled(m_rebindingAction);
        }
        m_rebindingActive = false;
        m_rebindingAction.clear();
        m_rebindListener = nullptr;
    }

    /**
     * @brief Check if currently rebinding
     */
    bool IsRebinding() const { return m_rebindingActive; }

    /**
     * @brief Update rebinding state - call each frame
     */
    void Update() {
        if (!m_rebindingActive || !m_inputManager) return;

        std::optional<ExtendedBinding> capturedBinding = CaptureInput();
        if (capturedBinding) {
            CompleteRebinding(*capturedBinding);
        }
    }

    /**
     * @brief Directly set a binding for an action
     * @return Conflict if one exists, nullopt otherwise
     */
    std::optional<BindingConflict> SetBinding(const std::string& actionName,
                                               const ExtendedBinding& binding,
                                               bool removeConflicts = false) {
        auto it = m_actions.find(actionName);
        if (it == m_actions.end()) return std::nullopt;

        // Check for conflicts
        auto conflict = CheckConflict(actionName, binding);
        if (conflict && !removeConflicts) {
            return conflict;
        }

        // Remove conflict if requested
        if (conflict && removeConflicts) {
            RemoveBinding(conflict->existingAction, binding);
        }

        // Add the new binding
        ActionDefinition& action = it->second;
        switch (binding.device) {
            case InputDevice::Keyboard:
                action.keyboardBindings.push_back(binding);
                break;
            case InputDevice::Mouse:
                action.mouseBindings.push_back(binding);
                break;
            case InputDevice::Gamepad:
                action.gamepadBindings.push_back(binding);
                break;
        }

        SyncToInputManager(actionName);
        return std::nullopt;
    }

    /**
     * @brief Remove a specific binding from an action
     */
    void RemoveBinding(const std::string& actionName, const ExtendedBinding& binding) {
        auto it = m_actions.find(actionName);
        if (it == m_actions.end()) return;

        ActionDefinition& action = it->second;
        auto removeFrom = [&binding](std::vector<ExtendedBinding>& bindings) {
            bindings.erase(
                std::remove_if(bindings.begin(), bindings.end(),
                    [&binding](const ExtendedBinding& b) { return b == binding; }),
                bindings.end());
        };

        switch (binding.device) {
            case InputDevice::Keyboard:
                removeFrom(action.keyboardBindings);
                break;
            case InputDevice::Mouse:
                removeFrom(action.mouseBindings);
                break;
            case InputDevice::Gamepad:
                removeFrom(action.gamepadBindings);
                break;
        }

        SyncToInputManager(actionName);
    }

    /**
     * @brief Clear all bindings for an action on a specific device
     */
    void ClearBindings(const std::string& actionName, InputDevice device) {
        auto it = m_actions.find(actionName);
        if (it == m_actions.end()) return;

        ActionDefinition& action = it->second;
        switch (device) {
            case InputDevice::Keyboard:
                action.keyboardBindings.clear();
                break;
            case InputDevice::Mouse:
                action.mouseBindings.clear();
                break;
            case InputDevice::Gamepad:
                action.gamepadBindings.clear();
                break;
        }

        SyncToInputManager(actionName);
    }

    /**
     * @brief Check for binding conflicts
     */
    std::optional<BindingConflict> CheckConflict(const std::string& actionName,
                                                   const ExtendedBinding& binding) const {
        for (const auto& [name, action] : m_actions) {
            if (name == actionName) continue;

            const std::vector<ExtendedBinding>* bindings = nullptr;
            switch (binding.device) {
                case InputDevice::Keyboard:
                    bindings = &action.keyboardBindings;
                    break;
                case InputDevice::Mouse:
                    bindings = &action.mouseBindings;
                    break;
                case InputDevice::Gamepad:
                    bindings = &action.gamepadBindings;
                    break;
            }

            if (bindings) {
                for (const auto& b : *bindings) {
                    if (b == binding) {
                        BindingConflict conflict;
                        conflict.existingAction = name;
                        conflict.newAction = actionName;
                        conflict.binding = binding;
                        conflict.device = binding.device;
                        return conflict;
                    }
                }
            }
        }
        return std::nullopt;
    }

    // -------------------------------------------------------------------------
    // Presets
    // -------------------------------------------------------------------------

    /**
     * @brief Create a preset from current bindings
     */
    InputPreset CreatePreset(const std::string& name, const std::string& description = "") const {
        InputPreset preset;
        preset.name = name;
        preset.description = description;
        preset.actions = m_actions;
        preset.mouseSensitivity = m_mouseSensitivity;
        preset.gamepadSensitivityX = m_gamepadSensitivityX;
        preset.gamepadSensitivityY = m_gamepadSensitivityY;
        preset.gamepadDeadzone = m_gamepadDeadzone;
        preset.invertMouseY = m_invertMouseY;
        preset.invertGamepadY = m_invertGamepadY;
        return preset;
    }

    /**
     * @brief Apply a preset
     */
    void ApplyPreset(const InputPreset& preset) {
        m_actions = preset.actions;
        m_mouseSensitivity = preset.mouseSensitivity;
        m_gamepadSensitivityX = preset.gamepadSensitivityX;
        m_gamepadSensitivityY = preset.gamepadSensitivityY;
        m_gamepadDeadzone = preset.gamepadDeadzone;
        m_invertMouseY = preset.invertMouseY;
        m_invertGamepadY = preset.invertGamepadY;

        // Sync all actions to input manager
        for (const auto& [name, action] : m_actions) {
            SyncToInputManager(name);
        }
    }

    /**
     * @brief Register a built-in preset
     */
    void RegisterPreset(const InputPreset& preset) {
        m_presets[preset.name] = preset;
    }

    /**
     * @brief Get all available presets
     */
    const std::unordered_map<std::string, InputPreset>& GetPresets() const {
        return m_presets;
    }

    /**
     * @brief Reset to default bindings
     */
    void ResetToDefaults() {
        if (m_presets.count("Default")) {
            ApplyPreset(m_presets["Default"]);
        } else {
            RegisterDefaultActions();
        }
    }

    /**
     * @brief Reset a single action to default
     */
    void ResetActionToDefault(const std::string& actionName) {
        if (m_presets.count("Default")) {
            auto it = m_presets["Default"].actions.find(actionName);
            if (it != m_presets["Default"].actions.end()) {
                m_actions[actionName] = it->second;
                SyncToInputManager(actionName);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    /**
     * @brief Save bindings to JSON file
     */
    bool SaveBindings(const std::string& filepath) const {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "  \"version\": 1,\n";
        file << "  \"settings\": {\n";
        file << "    \"mouseSensitivity\": " << m_mouseSensitivity << ",\n";
        file << "    \"gamepadSensitivityX\": " << m_gamepadSensitivityX << ",\n";
        file << "    \"gamepadSensitivityY\": " << m_gamepadSensitivityY << ",\n";
        file << "    \"gamepadDeadzone\": " << m_gamepadDeadzone << ",\n";
        file << "    \"invertMouseY\": " << (m_invertMouseY ? "true" : "false") << ",\n";
        file << "    \"invertGamepadY\": " << (m_invertGamepadY ? "true" : "false") << "\n";
        file << "  },\n";
        file << "  \"actions\": {\n";

        bool firstAction = true;
        for (const auto& [name, action] : m_actions) {
            if (!firstAction) file << ",\n";
            firstAction = false;

            file << "    \"" << name << "\": {\n";
            file << "      \"keyboard\": [";
            WriteBindings(file, action.keyboardBindings);
            file << "],\n";
            file << "      \"mouse\": [";
            WriteBindings(file, action.mouseBindings);
            file << "],\n";
            file << "      \"gamepad\": [";
            WriteBindings(file, action.gamepadBindings);
            file << "]\n";
            file << "    }";
        }

        file << "\n  }\n";
        file << "}\n";

        return true;
    }

    /**
     * @brief Load bindings from JSON file
     */
    bool LoadBindings(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        // Simple JSON parsing (in production, use a proper JSON library)
        // This is a simplified parser for the expected format

        // Parse settings
        m_mouseSensitivity = ParseFloat(content, "mouseSensitivity", m_mouseSensitivity);
        m_gamepadSensitivityX = ParseFloat(content, "gamepadSensitivityX", m_gamepadSensitivityX);
        m_gamepadSensitivityY = ParseFloat(content, "gamepadSensitivityY", m_gamepadSensitivityY);
        m_gamepadDeadzone = ParseFloat(content, "gamepadDeadzone", m_gamepadDeadzone);
        m_invertMouseY = ParseBool(content, "invertMouseY", m_invertMouseY);
        m_invertGamepadY = ParseBool(content, "invertGamepadY", m_invertGamepadY);

        // Parse actions - for each registered action, look for saved bindings
        for (auto& [name, action] : m_actions) {
            ParseActionBindings(content, name, action);
            SyncToInputManager(name);
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // Sensitivity Settings
    // -------------------------------------------------------------------------

    float GetMouseSensitivity() const { return m_mouseSensitivity; }
    void SetMouseSensitivity(float sens) { m_mouseSensitivity = sens; }

    float GetGamepadSensitivityX() const { return m_gamepadSensitivityX; }
    void SetGamepadSensitivityX(float sens) { m_gamepadSensitivityX = sens; }

    float GetGamepadSensitivityY() const { return m_gamepadSensitivityY; }
    void SetGamepadSensitivityY(float sens) { m_gamepadSensitivityY = sens; }

    float GetGamepadDeadzone() const { return m_gamepadDeadzone; }
    void SetGamepadDeadzone(float dz) { m_gamepadDeadzone = dz; }

    bool GetInvertMouseY() const { return m_invertMouseY; }
    void SetInvertMouseY(bool invert) { m_invertMouseY = invert; }

    bool GetInvertGamepadY() const { return m_invertGamepadY; }
    void SetInvertGamepadY(bool invert) { m_invertGamepadY = invert; }

    // -------------------------------------------------------------------------
    // Gamepad State (for games that need direct gamepad access)
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a gamepad is connected
     */
    bool IsGamepadConnected(int gamepadId = 0) const {
        // Would check GLFW_JOYSTICK_* in real implementation
        return m_connectedGamepads.count(gamepadId) > 0;
    }

    /**
     * @brief Get gamepad button state
     */
    bool IsGamepadButtonDown(GamepadButton button, int gamepadId = 0) const {
        if (!IsGamepadConnected(gamepadId)) return false;
        int idx = static_cast<int>(button);
        return idx < 15 && m_gamepadButtons[gamepadId][idx];
    }

    /**
     * @brief Get gamepad axis value (-1 to 1)
     */
    float GetGamepadAxis(GamepadAxis axis, int gamepadId = 0) const {
        if (!IsGamepadConnected(gamepadId)) return 0.0f;
        int idx = static_cast<int>(axis);
        if (idx >= 6) return 0.0f;

        float value = m_gamepadAxes[gamepadId][idx];

        // Apply deadzone
        if (std::abs(value) < m_gamepadDeadzone) {
            return 0.0f;
        }

        // Rescale to 0-1 range after deadzone
        float sign = value > 0 ? 1.0f : -1.0f;
        value = (std::abs(value) - m_gamepadDeadzone) / (1.0f - m_gamepadDeadzone);
        return sign * value;
    }

    /**
     * @brief Update gamepad state - call each frame
     */
    void UpdateGamepads() {
        // In real implementation, would poll GLFW joysticks
        // For each connected joystick:
        //   - Get button states
        //   - Get axis values
        //   - Check for connection/disconnection events
    }

private:
    InputRebinding() = default;
    ~InputRebinding() = default;
    InputRebinding(const InputRebinding&) = delete;
    InputRebinding& operator=(const InputRebinding&) = delete;

    /**
     * @brief Register default game actions
     */
    void RegisterDefaultActions() {
        // Movement
        RegisterAction("MoveForward", "Move Forward", "Movement", Key::W);
        RegisterAction("MoveBackward", "Move Backward", "Movement", Key::S);
        RegisterAction("MoveLeft", "Move Left", "Movement", Key::A);
        RegisterAction("MoveRight", "Move Right", "Movement", Key::D);
        RegisterAction("Jump", "Jump", "Movement", Key::Space);
        RegisterAction("Crouch", "Crouch", "Movement", Key::LeftControl);
        RegisterAction("Sprint", "Sprint", "Movement", Key::LeftShift);

        // Combat
        ActionDefinition fire;
        fire.name = "Fire";
        fire.displayName = "Fire";
        fire.category = "Combat";
        fire.mouseBindings.push_back(ExtendedBinding::FromMouseButton(MouseButton::Left));
        fire.gamepadBindings.push_back(ExtendedBinding::FromGamepadAxis(GamepadAxis::RightTrigger, true, 0.3f));
        RegisterAction(fire);

        ActionDefinition aim;
        aim.name = "Aim";
        aim.displayName = "Aim Down Sights";
        aim.category = "Combat";
        aim.mouseBindings.push_back(ExtendedBinding::FromMouseButton(MouseButton::Right));
        aim.gamepadBindings.push_back(ExtendedBinding::FromGamepadAxis(GamepadAxis::LeftTrigger, true, 0.3f));
        RegisterAction(aim);

        RegisterAction("Reload", "Reload", "Combat", Key::R);
        RegisterAction("Melee", "Melee", "Combat", Key::V);
        RegisterAction("Grenade", "Grenade", "Combat", Key::G);

        // Interaction
        RegisterAction("Interact", "Interact", "Interaction", Key::E);
        RegisterAction("Use", "Use Item", "Interaction", Key::F);

        // UI
        RegisterAction("Pause", "Pause", "UI", Key::Escape);
        RegisterAction("Inventory", "Inventory", "UI", Key::Tab);
        RegisterAction("Map", "Map", "UI", Key::M);
        RegisterAction("Scoreboard", "Scoreboard", "UI", Key::Tab);

        // Quick slots
        for (int i = 0; i < 9; ++i) {
            std::string name = "Slot" + std::to_string(i + 1);
            std::string display = "Slot " + std::to_string(i + 1);
            RegisterAction(name, display, "Quick Slots", static_cast<Key>(static_cast<int>(Key::Num1) + i));
        }

        // Save as default preset
        m_presets["Default"] = CreatePreset("Default", "Default input bindings");
    }

    /**
     * @brief Sync action bindings to the InputManager
     */
    void SyncToInputManager(const std::string& actionName) {
        if (!m_inputManager) return;

        auto it = m_actions.find(actionName);
        if (it == m_actions.end()) return;

        const ActionDefinition& action = it->second;

        // Convert to InputManager bindings (keyboard and mouse only - InputManager doesn't handle gamepad)
        std::vector<InputBinding> bindings;

        for (const auto& kb : action.keyboardBindings) {
            bindings.push_back(InputBinding::FromKey(static_cast<Key>(kb.keyOrButton), kb.modifiers));
        }

        for (const auto& mb : action.mouseBindings) {
            bindings.push_back(InputBinding::FromMouseButton(static_cast<MouseButton>(mb.keyOrButton), mb.modifiers));
        }

        // Re-register with InputManager
        m_inputManager->UnregisterAction(actionName);
        if (!bindings.empty()) {
            for (const auto& binding : bindings) {
                m_inputManager->RegisterAction(actionName, binding);
            }
        }
    }

    /**
     * @brief Capture input during rebinding
     */
    std::optional<ExtendedBinding> CaptureInput() {
        if (!m_inputManager) return std::nullopt;

        // Check for cancel key (Escape)
        if (m_inputManager->IsKeyPressed(Key::Escape)) {
            CancelRebinding();
            return std::nullopt;
        }

        switch (m_rebindingDevice) {
            case InputDevice::Keyboard: {
                // Check all keys for press
                for (int k = 0; k < static_cast<int>(Key::MaxKeys); ++k) {
                    Key key = static_cast<Key>(k);
                    if (key == Key::Escape) continue; // Skip escape

                    if (m_inputManager->IsKeyPressed(key)) {
                        return ExtendedBinding::FromKey(key, m_inputManager->GetModifiers());
                    }
                }
                break;
            }
            case InputDevice::Mouse: {
                for (int b = 0; b < static_cast<int>(MouseButton::MaxButtons); ++b) {
                    MouseButton button = static_cast<MouseButton>(b);
                    if (m_inputManager->IsMouseButtonPressed(button)) {
                        return ExtendedBinding::FromMouseButton(button, m_inputManager->GetModifiers());
                    }
                }
                break;
            }
            case InputDevice::Gamepad: {
                // Check gamepad buttons
                for (int b = 0; b < static_cast<int>(GamepadButton::MaxButtons); ++b) {
                    if (IsGamepadButtonDown(static_cast<GamepadButton>(b))) {
                        return ExtendedBinding::FromGamepadButton(static_cast<GamepadButton>(b));
                    }
                }
                // Check gamepad axes
                for (int a = 0; a < static_cast<int>(GamepadAxis::MaxAxes); ++a) {
                    float value = GetGamepadAxis(static_cast<GamepadAxis>(a));
                    if (std::abs(value) > 0.7f) {
                        return ExtendedBinding::FromGamepadAxis(
                            static_cast<GamepadAxis>(a),
                            value > 0,
                            0.5f);
                    }
                }
                break;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Complete the rebinding process
     */
    void CompleteRebinding(const ExtendedBinding& binding) {
        auto conflict = SetBinding(m_rebindingAction, binding, false);

        if (conflict && m_rebindListener) {
            m_rebindListener->OnBindingConflict(*conflict);
            // Don't complete - wait for user to resolve conflict
            return;
        }

        if (m_rebindListener) {
            m_rebindListener->OnRebindCompleted(m_rebindingAction, binding);
        }

        m_rebindingActive = false;
        m_rebindingAction.clear();
        m_rebindListener = nullptr;
    }

    /**
     * @brief Write bindings array to file
     */
    void WriteBindings(std::ofstream& file, const std::vector<ExtendedBinding>& bindings) const {
        bool first = true;
        for (const auto& b : bindings) {
            if (!first) file << ", ";
            first = false;

            file << "{";
            file << "\"code\": " << b.keyOrButton << ", ";
            file << "\"modifiers\": " << static_cast<int>(b.modifiers);
            if (b.device == InputDevice::Gamepad) {
                file << ", \"button\": " << static_cast<int>(b.gamepadButton);
                file << ", \"axis\": " << static_cast<int>(b.gamepadAxis);
                file << ", \"axisPositive\": " << (b.axisPositive ? "true" : "false");
                file << ", \"axisThreshold\": " << b.axisThreshold;
                file << ", \"isAxisBinding\": " << (b.isAxisBinding ? "true" : "false");
            }
            file << "}";
        }
    }

    /**
     * @brief Parse a float value from JSON content
     */
    float ParseFloat(const std::string& content, const std::string& key, float defaultValue) const {
        std::string searchKey = "\"" + key + "\": ";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) return defaultValue;

        pos += searchKey.length();
        size_t endPos = content.find_first_of(",}\n", pos);
        if (endPos == std::string::npos) return defaultValue;

        try {
            return std::stof(content.substr(pos, endPos - pos));
        } catch (...) {
            return defaultValue;
        }
    }

    /**
     * @brief Parse a bool value from JSON content
     */
    bool ParseBool(const std::string& content, const std::string& key, bool defaultValue) const {
        std::string searchKey = "\"" + key + "\": ";
        size_t pos = content.find(searchKey);
        if (pos == std::string::npos) return defaultValue;

        pos += searchKey.length();
        return content.substr(pos, 4) == "true";
    }

    /**
     * @brief Parse action bindings from JSON content
     */
    void ParseActionBindings(const std::string& content, const std::string& actionName,
                            ActionDefinition& action) {
        // Simplified parsing - in production use a proper JSON library
        std::string actionKey = "\"" + actionName + "\":";
        size_t actionPos = content.find(actionKey);
        if (actionPos == std::string::npos) return;

        // Find the action's closing brace
        size_t braceCount = 0;
        size_t startPos = content.find('{', actionPos);
        if (startPos == std::string::npos) return;

        size_t endPos = startPos;
        for (size_t i = startPos; i < content.length(); ++i) {
            if (content[i] == '{') braceCount++;
            else if (content[i] == '}') {
                braceCount--;
                if (braceCount == 0) {
                    endPos = i;
                    break;
                }
            }
        }

        std::string actionContent = content.substr(startPos, endPos - startPos + 1);

        // Parse keyboard bindings
        action.keyboardBindings = ParseBindingArray(actionContent, "keyboard", InputDevice::Keyboard);
        action.mouseBindings = ParseBindingArray(actionContent, "mouse", InputDevice::Mouse);
        action.gamepadBindings = ParseBindingArray(actionContent, "gamepad", InputDevice::Gamepad);
    }

    /**
     * @brief Parse binding array from JSON
     */
    std::vector<ExtendedBinding> ParseBindingArray(const std::string& content,
                                                    const std::string& arrayName,
                                                    InputDevice device) const {
        std::vector<ExtendedBinding> bindings;

        std::string arrayKey = "\"" + arrayName + "\": [";
        size_t arrayPos = content.find(arrayKey);
        if (arrayPos == std::string::npos) return bindings;

        arrayPos += arrayKey.length();
        size_t endPos = content.find(']', arrayPos);
        if (endPos == std::string::npos) return bindings;

        std::string arrayContent = content.substr(arrayPos, endPos - arrayPos);

        // Find each binding object
        size_t pos = 0;
        while ((pos = arrayContent.find('{', pos)) != std::string::npos) {
            size_t objEnd = arrayContent.find('}', pos);
            if (objEnd == std::string::npos) break;

            std::string objContent = arrayContent.substr(pos, objEnd - pos + 1);

            ExtendedBinding binding;
            binding.device = device;
            binding.keyOrButton = static_cast<int>(ParseFloat(objContent, "code", 0));
            binding.modifiers = static_cast<ModifierFlags>(static_cast<int>(ParseFloat(objContent, "modifiers", 0)));

            if (device == InputDevice::Gamepad) {
                binding.gamepadButton = static_cast<GamepadButton>(static_cast<int>(ParseFloat(objContent, "button", 0)));
                binding.gamepadAxis = static_cast<GamepadAxis>(static_cast<int>(ParseFloat(objContent, "axis", 0)));
                binding.axisPositive = ParseBool(objContent, "axisPositive", true);
                binding.axisThreshold = ParseFloat(objContent, "axisThreshold", 0.5f);
                binding.isAxisBinding = ParseBool(objContent, "isAxisBinding", false);
            }

            bindings.push_back(binding);
            pos = objEnd + 1;
        }

        return bindings;
    }

    InputManager* m_inputManager = nullptr;

    // Action definitions
    std::unordered_map<std::string, ActionDefinition> m_actions;

    // Presets
    std::unordered_map<std::string, InputPreset> m_presets;

    // Rebinding state
    bool m_rebindingActive = false;
    std::string m_rebindingAction;
    InputDevice m_rebindingDevice = InputDevice::Keyboard;
    IRebindingListener* m_rebindListener = nullptr;

    // Sensitivity settings
    float m_mouseSensitivity = 1.0f;
    float m_gamepadSensitivityX = 1.0f;
    float m_gamepadSensitivityY = 1.0f;
    float m_gamepadDeadzone = 0.15f;
    bool m_invertMouseY = false;
    bool m_invertGamepadY = false;

    // Gamepad state
    std::set<int> m_connectedGamepads;
    std::array<std::array<bool, 15>, 4> m_gamepadButtons{};
    std::array<std::array<float, 6>, 4> m_gamepadAxes{};
};

// =============================================================================
// Convenience Functions
// =============================================================================

inline const char* GamepadButtonToString(GamepadButton button) {
    static const char* names[] = {
        "A", "B", "X", "Y",
        "Left Bumper", "Right Bumper",
        "Back", "Start", "Guide",
        "Left Stick", "Right Stick",
        "D-Pad Up", "D-Pad Right", "D-Pad Down", "D-Pad Left"
    };
    int idx = static_cast<int>(button);
    return (idx >= 0 && idx < 15) ? names[idx] : "Unknown";
}

inline const char* GamepadAxisToString(GamepadAxis axis) {
    static const char* names[] = {
        "Left Stick X", "Left Stick Y",
        "Right Stick X", "Right Stick Y",
        "Left Trigger", "Right Trigger"
    };
    int idx = static_cast<int>(axis);
    return (idx >= 0 && idx < 6) ? names[idx] : "Unknown";
}

inline const char* InputDeviceToString(InputDevice device) {
    switch (device) {
        case InputDevice::Keyboard: return "Keyboard";
        case InputDevice::Mouse: return "Mouse";
        case InputDevice::Gamepad: return "Gamepad";
    }
    return "Unknown";
}

} // namespace Nova
