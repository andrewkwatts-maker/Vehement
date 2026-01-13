/**
 * @file IEditorTool.hpp
 * @brief Pure virtual interface for all editor tools in the Vehement SDF Engine
 *
 * Provides a unified interface for editor tools including:
 * - TransformGizmo for object manipulation
 * - SDFSculptTool for SDF sculpting
 * - Selection tools, measurement tools, navigation tools
 * - Custom user-defined tools via plugin system
 *
 * Features:
 * - Input event handling with priority and exclusivity
 * - Tool state management with activation/deactivation lifecycle
 * - Visual rendering of tool overlays (3D and 2D)
 * - Settings persistence via JSON
 * - Tool factory pattern for registration and lookup
 * - Category-based organization
 */

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

// Forward declarations for ImGui types
struct ImDrawList;

namespace Nova {

// Forward declarations
class Camera;
class Scene;
class SceneNode;
class IRenderContext;

// =============================================================================
// Tool Category
// =============================================================================

/**
 * @brief Category classification for editor tools
 *
 * Used for organizing tools in the UI and determining tool behavior priorities.
 */
enum class ToolCategory : uint8_t {
    Transform,      ///< Object manipulation tools (gizmo, move, rotate, scale)
    Sculpt,         ///< SDF sculpting tools (brushes, modifiers)
    Selection,      ///< Selection tools (box select, lasso, paint select)
    Measurement,    ///< Measurement tools (distance, angle, area)
    Navigation,     ///< Camera navigation tools (orbit, pan, zoom, fly)
    Annotation,     ///< Annotation tools (comments, markers, guides)
    Custom          ///< User-defined custom tools
};

/**
 * @brief Convert tool category to display string
 */
constexpr const char* ToolCategoryToString(ToolCategory category) noexcept {
    switch (category) {
        case ToolCategory::Transform:   return "Transform";
        case ToolCategory::Sculpt:      return "Sculpt";
        case ToolCategory::Selection:   return "Selection";
        case ToolCategory::Measurement: return "Measurement";
        case ToolCategory::Navigation:  return "Navigation";
        case ToolCategory::Annotation:  return "Annotation";
        case ToolCategory::Custom:      return "Custom";
        default:                        return "Unknown";
    }
}

/**
 * @brief Get icon for tool category (Material Design Icons codepoint or name)
 */
constexpr const char* ToolCategoryIcon(ToolCategory category) noexcept {
    switch (category) {
        case ToolCategory::Transform:   return "\xef\x87\x9a";  // move icon
        case ToolCategory::Sculpt:      return "\xef\x8c\x82";  // brush icon
        case ToolCategory::Selection:   return "\xef\x84\x85";  // cursor icon
        case ToolCategory::Measurement: return "\xef\x95\x8f";  // ruler icon
        case ToolCategory::Navigation:  return "\xef\x8b\xac";  // eye icon
        case ToolCategory::Annotation:  return "\xef\x88\xa8";  // comment icon
        case ToolCategory::Custom:      return "\xef\x8c\x94";  // extension icon
        default:                        return "\xef\x8b\xab";  // help icon
    }
}

// =============================================================================
// Tool Input Event
// =============================================================================

/**
 * @brief Key modifier flags for input events
 */
enum class ToolKeyModifiers : uint8_t {
    None  = 0,
    Ctrl  = 1 << 0,
    Shift = 1 << 1,
    Alt   = 1 << 2,
    Super = 1 << 3   // Windows/Command key
};

inline ToolKeyModifiers operator|(ToolKeyModifiers a, ToolKeyModifiers b) {
    return static_cast<ToolKeyModifiers>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline ToolKeyModifiers operator&(ToolKeyModifiers a, ToolKeyModifiers b) {
    return static_cast<ToolKeyModifiers>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline bool HasModifier(ToolKeyModifiers flags, ToolKeyModifiers check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

/**
 * @brief Mouse button identifiers
 */
enum class MouseButton : int8_t {
    None   = -1,
    Left   = 0,
    Right  = 1,
    Middle = 2,
    Extra1 = 3,
    Extra2 = 4
};

/**
 * @brief Input event structure for tool interaction
 *
 * Contains all relevant information about user input for tools to process.
 */
struct ToolInputEvent {
    /**
     * @brief Type of input event
     */
    enum class Type : uint8_t {
        MouseDown,      ///< Mouse button pressed
        MouseUp,        ///< Mouse button released
        MouseMove,      ///< Mouse moved (button state unchanged)
        MouseDrag,      ///< Mouse moved while button held
        MouseDoubleClick, ///< Double-click detected
        KeyDown,        ///< Keyboard key pressed
        KeyUp,          ///< Keyboard key released
        KeyRepeat,      ///< Keyboard key held (auto-repeat)
        Scroll,         ///< Mouse wheel scrolled
        TouchBegin,     ///< Touch input began (mobile/tablet)
        TouchMove,      ///< Touch position changed
        TouchEnd        ///< Touch input ended
    };

    Type type = Type::MouseMove;

    // Mouse state
    glm::vec2 mousePos{0.0f};       ///< Current mouse position in screen pixels
    glm::vec2 mouseDelta{0.0f};     ///< Mouse movement since last event
    glm::vec2 mousePosNormalized{0.0f}; ///< Mouse position normalized to [0,1]
    MouseButton button = MouseButton::None; ///< Mouse button for button events
    uint8_t clickCount = 0;          ///< Number of clicks (for double-click detection)

    // Keyboard state
    int key = 0;                     ///< Key code (GLFW key codes)
    int scancode = 0;                ///< Platform-specific scancode
    char character = '\0';           ///< Unicode character if text input

    // Modifiers (valid for all event types)
    ToolKeyModifiers modifiers = ToolKeyModifiers::None;

    // Scroll state
    float scrollDelta = 0.0f;        ///< Vertical scroll amount
    float scrollDeltaX = 0.0f;       ///< Horizontal scroll amount

    // Pressure (for tablets/touch)
    float pressure = 1.0f;           ///< Pen/touch pressure [0, 1]
    float tiltX = 0.0f;              ///< Pen tilt X [-1, 1]
    float tiltY = 0.0f;              ///< Pen tilt Y [-1, 1]

    // Viewport context
    glm::vec2 viewportSize{0.0f};    ///< Size of the viewport in pixels
    glm::vec2 viewportPos{0.0f};     ///< Position of viewport on screen

    // Ray casting (computed from mouse position)
    glm::vec3 rayOrigin{0.0f};       ///< World-space ray origin
    glm::vec3 rayDirection{0.0f, 0.0f, -1.0f}; ///< World-space ray direction

    // Timing
    double timestamp = 0.0;          ///< Event timestamp (seconds since app start)
    float deltaTime = 0.0f;          ///< Time since last frame

    /**
     * @brief Check if this is a mouse button event
     */
    [[nodiscard]] bool IsMouseButton() const {
        return type == Type::MouseDown || type == Type::MouseUp || type == Type::MouseDoubleClick;
    }

    /**
     * @brief Check if this is a keyboard event
     */
    [[nodiscard]] bool IsKeyboard() const {
        return type == Type::KeyDown || type == Type::KeyUp || type == Type::KeyRepeat;
    }

    /**
     * @brief Check if Ctrl modifier is held
     */
    [[nodiscard]] bool IsCtrlHeld() const {
        return HasModifier(modifiers, ToolKeyModifiers::Ctrl);
    }

    /**
     * @brief Check if Shift modifier is held
     */
    [[nodiscard]] bool IsShiftHeld() const {
        return HasModifier(modifiers, ToolKeyModifiers::Shift);
    }

    /**
     * @brief Check if Alt modifier is held
     */
    [[nodiscard]] bool IsAltHeld() const {
        return HasModifier(modifiers, ToolKeyModifiers::Alt);
    }

    /**
     * @brief Serialize to JSON (for input recording/replay)
     */
    [[nodiscard]] nlohmann::json ToJson() const;

    /**
     * @brief Deserialize from JSON
     */
    static ToolInputEvent FromJson(const nlohmann::json& json);
};

// =============================================================================
// Tool Result
// =============================================================================

/**
 * @brief Result of tool input processing
 *
 * Tools return this to indicate how input was handled.
 */
struct ToolInputResult {
    bool consumed = false;           ///< True if input was fully handled
    bool wantCapture = false;        ///< True if tool wants mouse capture
    bool needsRedraw = false;        ///< True if viewport needs redraw
    std::string statusMessage;       ///< Optional status bar message

    /**
     * @brief Create a result indicating input was consumed
     */
    static ToolInputResult Consumed() {
        ToolInputResult result;
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    /**
     * @brief Create a result indicating input was not handled
     */
    static ToolInputResult NotHandled() {
        return ToolInputResult{};
    }
};

// =============================================================================
// Render Context for Tools
// =============================================================================

/**
 * @brief Rendering context passed to tools for 3D overlay rendering
 */
struct ToolRenderContext {
    const Camera* camera = nullptr;       ///< Active camera
    IRenderContext* renderContext = nullptr; ///< Low-level render context
    glm::mat4 viewMatrix{1.0f};           ///< View matrix
    glm::mat4 projectionMatrix{1.0f};     ///< Projection matrix
    glm::mat4 viewProjectionMatrix{1.0f}; ///< Combined VP matrix
    glm::vec3 cameraPosition{0.0f};       ///< Camera world position
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f}; ///< Camera forward direction
    glm::vec2 viewportSize{0.0f};         ///< Viewport dimensions
    float nearPlane = 0.1f;               ///< Near clip plane
    float farPlane = 1000.0f;             ///< Far clip plane
    float deltaTime = 0.0f;               ///< Frame delta time
    bool isOrthographic = false;          ///< True if using orthographic projection
};

// =============================================================================
// Tool Context
// =============================================================================

/**
 * @brief Context information provided to tools during operation
 */
struct ToolContext {
    Scene* scene = nullptr;               ///< Current scene
    std::vector<SceneNode*> selection;    ///< Currently selected objects
    const Camera* camera = nullptr;       ///< Active camera
    glm::vec2 viewportSize{0.0f};         ///< Viewport dimensions
    float deltaTime = 0.0f;               ///< Frame delta time

    /**
     * @brief Check if there's a valid selection
     */
    [[nodiscard]] bool HasSelection() const { return !selection.empty(); }

    /**
     * @brief Get the first selected object (or nullptr)
     */
    [[nodiscard]] SceneNode* GetPrimarySelection() const {
        return selection.empty() ? nullptr : selection.front();
    }
};

// =============================================================================
// IEditorTool Interface
// =============================================================================

/**
 * @brief Pure virtual interface for all editor tools
 *
 * All editor tools (TransformGizmo, SDFSculptTool, selection tools, etc.)
 * should implement this interface to integrate with the editor's tool system.
 *
 * Tool Lifecycle:
 * 1. Tool is created and registered with EditorToolRegistry
 * 2. User selects tool (via hotkey or UI)
 * 3. Activate() is called
 * 4. OnInput() receives events, Update() called each frame
 * 5. Render() and RenderOverlay() called for visualization
 * 6. User switches to different tool
 * 7. Deactivate() is called
 *
 * Example implementation:
 * @code
 * class MyCustomTool : public IEditorTool {
 * public:
 *     void Activate() override {
 *         m_active = true;
 *         // Initialize tool state
 *     }
 *
 *     void Deactivate() override {
 *         m_active = false;
 *         // Cleanup tool state
 *     }
 *
 *     ToolInputResult OnInput(const ToolInputEvent& event, const ToolContext& ctx) override {
 *         if (event.type == ToolInputEvent::Type::MouseDown) {
 *             // Handle click
 *             return ToolInputResult::Consumed();
 *         }
 *         return ToolInputResult::NotHandled();
 *     }
 *     // ... other methods
 * };
 * @endcode
 */
class IEditorTool {
public:
    virtual ~IEditorTool() = default;

    // =========================================================================
    // Tool Identification
    // =========================================================================

    /**
     * @brief Get the unique identifier for this tool
     *
     * Should be a stable, unique string like "nova.transform.move" or
     * "nova.sculpt.add". Used for serialization and lookup.
     */
    [[nodiscard]] virtual std::string_view GetId() const = 0;

    /**
     * @brief Get human-readable display name
     *
     * Shown in UI elements like toolbar tooltips and tool selection menus.
     */
    [[nodiscard]] virtual std::string_view GetName() const = 0;

    /**
     * @brief Get icon identifier or Unicode glyph
     *
     * Can be a Material Design Icons codepoint or custom icon name.
     */
    [[nodiscard]] virtual std::string_view GetIcon() const = 0;

    /**
     * @brief Get tooltip text for the tool
     *
     * Shown when hovering over the tool button.
     */
    [[nodiscard]] virtual std::string_view GetTooltip() const = 0;

    /**
     * @brief Get the tool category
     */
    [[nodiscard]] virtual ToolCategory GetCategory() const = 0;

    /**
     * @brief Get keyboard shortcut string (e.g., "W", "Ctrl+G")
     *
     * Return empty string if no shortcut is assigned.
     */
    [[nodiscard]] virtual std::string_view GetShortcut() const = 0;

    /**
     * @brief Get tool priority within category
     *
     * Higher values are shown first. Default tools should use 100.
     */
    [[nodiscard]] virtual int GetPriority() const { return 100; }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Called when tool becomes the active tool
     *
     * Initialize any tool state, cursors, overlays here.
     */
    virtual void Activate() = 0;

    /**
     * @brief Called when tool is deactivated (another tool selected)
     *
     * Cleanup temporary state, release resources here.
     * Should not throw exceptions.
     */
    virtual void Deactivate() = 0;

    /**
     * @brief Check if tool is currently active
     */
    [[nodiscard]] virtual bool IsActive() const = 0;

    /**
     * @brief Check if tool is available (enabled) given current context
     *
     * Tools can disable themselves based on scene state, selection, etc.
     * @param ctx Current tool context
     * @return true if tool should be enabled
     */
    [[nodiscard]] virtual bool IsAvailable(const ToolContext& ctx) const {
        (void)ctx;
        return true;
    }

    // =========================================================================
    // Input Handling
    // =========================================================================

    /**
     * @brief Process input event
     *
     * Called for each input event when tool is active. Tool should return
     * whether it consumed the event (preventing other handlers from receiving it).
     *
     * @param event The input event
     * @param ctx Current tool context
     * @return Result indicating how input was handled
     */
    [[nodiscard]] virtual ToolInputResult OnInput(const ToolInputEvent& event,
                                                   const ToolContext& ctx) = 0;

    /**
     * @brief Check if tool wants exclusive input capture
     *
     * When true, all input goes to this tool even if cursor leaves viewport.
     * Useful during drag operations.
     */
    [[nodiscard]] virtual bool WantsExclusiveInput() const = 0;

    /**
     * @brief Get cursor to display while tool is active
     *
     * Return empty string for default cursor, or cursor name like "crosshair",
     * "move", "rotate", etc.
     */
    [[nodiscard]] virtual std::string_view GetCursor() const { return ""; }

    /**
     * @brief Get cursor to display while hovering over specific element
     *
     * Tools can return different cursors based on what's under the mouse.
     * @param event Current input state
     * @param ctx Tool context
     * @return Cursor name or empty for default
     */
    [[nodiscard]] virtual std::string_view GetCursorAt(const ToolInputEvent& event,
                                                        const ToolContext& ctx) const {
        (void)event;
        (void)ctx;
        return GetCursor();
    }

    // =========================================================================
    // Update and Render
    // =========================================================================

    /**
     * @brief Update tool state
     *
     * Called once per frame while tool is active. Use for animations,
     * time-based effects, etc.
     *
     * @param deltaTime Time since last frame in seconds
     * @param ctx Tool context
     */
    virtual void Update(float deltaTime, const ToolContext& ctx) = 0;

    /**
     * @brief Render 3D tool overlays
     *
     * Called during 3D rendering pass. Draw gizmos, guides, previews here.
     *
     * @param renderCtx Rendering context with camera and matrices
     */
    virtual void Render(const ToolRenderContext& renderCtx) = 0;

    /**
     * @brief Render 2D overlay using ImGui draw list
     *
     * Called after 3D rendering. Draw screen-space UI elements like
     * selection rectangles, measurement labels, etc.
     *
     * @param drawList ImGui draw list for the viewport
     * @param ctx Tool context
     */
    virtual void RenderOverlay(ImDrawList* drawList, const ToolContext& ctx) = 0;

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Show tool-specific settings UI
     *
     * Called to render ImGui controls for tool settings.
     * Should be displayed in a properties panel or tool options bar.
     */
    virtual void ShowSettings() = 0;

    /**
     * @brief Load tool settings from JSON
     *
     * @param settings JSON object containing tool settings
     */
    virtual void LoadSettings(const nlohmann::json& settings) = 0;

    /**
     * @brief Save tool settings to JSON
     *
     * @return JSON object containing current tool settings
     */
    [[nodiscard]] virtual nlohmann::json SaveSettings() const = 0;

    /**
     * @brief Reset tool settings to defaults
     */
    virtual void ResetSettings() {}

    // =========================================================================
    // Optional Capabilities
    // =========================================================================

    /**
     * @brief Check if tool supports undo/redo
     */
    [[nodiscard]] virtual bool SupportsUndo() const { return false; }

    /**
     * @brief Check if tool can work with current selection
     *
     * @param selection Currently selected objects
     * @return true if tool can operate on selection
     */
    [[nodiscard]] virtual bool CanOperateOn(const std::vector<SceneNode*>& selection) const {
        (void)selection;
        return true;
    }

    /**
     * @brief Get help text for tool usage
     *
     * Detailed description shown in help panel.
     */
    [[nodiscard]] virtual std::string_view GetHelpText() const { return ""; }

    /**
     * @brief Get tool version string
     */
    [[nodiscard]] virtual std::string_view GetVersion() const { return "1.0"; }

protected:
    IEditorTool() = default;
};

// =============================================================================
// Tool Factory
// =============================================================================

/**
 * @brief Factory function type for creating tools
 */
using ToolFactoryFunc = std::function<std::unique_ptr<IEditorTool>()>;

/**
 * @brief Tool registration information
 */
struct ToolRegistration {
    std::string id;                      ///< Unique tool ID
    std::string name;                    ///< Display name
    ToolCategory category;               ///< Tool category
    ToolFactoryFunc factory;             ///< Factory function
    int priority = 100;                  ///< Sort priority
    std::string shortcut;                ///< Default keyboard shortcut

    /**
     * @brief Create a tool instance
     */
    [[nodiscard]] std::unique_ptr<IEditorTool> Create() const {
        return factory ? factory() : nullptr;
    }
};

// =============================================================================
// EditorToolRegistry
// =============================================================================

/**
 * @brief Registry for editor tools
 *
 * Manages tool registration, lookup, and lifecycle. Singleton pattern ensures
 * consistent tool management across the editor.
 *
 * Usage:
 * @code
 * // Register a tool
 * EditorToolRegistry::Instance().Register<MyTool>("mytool", "My Tool", ToolCategory::Custom);
 *
 * // Get available tools
 * auto tools = EditorToolRegistry::Instance().GetToolsByCategory(ToolCategory::Transform);
 *
 * // Create tool instance
 * auto tool = EditorToolRegistry::Instance().CreateTool("mytool");
 * @endcode
 */
class EditorToolRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static EditorToolRegistry& Instance();

    // Delete copy/move for singleton
    EditorToolRegistry(const EditorToolRegistry&) = delete;
    EditorToolRegistry& operator=(const EditorToolRegistry&) = delete;
    EditorToolRegistry(EditorToolRegistry&&) = delete;
    EditorToolRegistry& operator=(EditorToolRegistry&&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    /**
     * @brief Register a tool type
     *
     * @tparam T Tool class (must derive from IEditorTool)
     * @param id Unique tool identifier
     * @param name Display name
     * @param category Tool category
     * @param priority Sort priority (higher = first)
     * @param shortcut Default keyboard shortcut
     * @return true if registration succeeded
     */
    template<typename T>
    bool Register(const std::string& id,
                  const std::string& name,
                  ToolCategory category,
                  int priority = 100,
                  const std::string& shortcut = "") {
        static_assert(std::is_base_of_v<IEditorTool, T>,
                      "Tool type must derive from IEditorTool");

        ToolRegistration reg;
        reg.id = id;
        reg.name = name;
        reg.category = category;
        reg.priority = priority;
        reg.shortcut = shortcut;
        reg.factory = []() { return std::make_unique<T>(); };

        return RegisterTool(std::move(reg));
    }

    /**
     * @brief Register a tool with custom factory
     *
     * @param registration Tool registration info
     * @return true if registration succeeded
     */
    bool RegisterTool(ToolRegistration registration);

    /**
     * @brief Unregister a tool
     *
     * @param id Tool identifier
     * @return true if tool was unregistered
     */
    bool Unregister(const std::string& id);

    /**
     * @brief Check if a tool is registered
     *
     * @param id Tool identifier
     */
    [[nodiscard]] bool IsRegistered(const std::string& id) const;

    // =========================================================================
    // Tool Creation
    // =========================================================================

    /**
     * @brief Create a tool instance by ID
     *
     * @param id Tool identifier
     * @return Tool instance or nullptr if not found
     */
    [[nodiscard]] std::unique_ptr<IEditorTool> CreateTool(const std::string& id) const;

    /**
     * @brief Create all tools in a category
     *
     * @param category Tool category
     * @return Vector of tool instances
     */
    [[nodiscard]] std::vector<std::unique_ptr<IEditorTool>> CreateToolsInCategory(
        ToolCategory category) const;

    // =========================================================================
    // Lookup
    // =========================================================================

    /**
     * @brief Get registration info for a tool
     *
     * @param id Tool identifier
     * @return Registration info or nullopt if not found
     */
    [[nodiscard]] std::optional<ToolRegistration> GetRegistration(const std::string& id) const;

    /**
     * @brief Get all registered tool IDs
     */
    [[nodiscard]] std::vector<std::string> GetAllToolIds() const;

    /**
     * @brief Get tool IDs in a category (sorted by priority)
     *
     * @param category Tool category
     * @return Vector of tool IDs
     */
    [[nodiscard]] std::vector<std::string> GetToolIdsByCategory(ToolCategory category) const;

    /**
     * @brief Get all registrations (sorted by category then priority)
     */
    [[nodiscard]] std::vector<ToolRegistration> GetAllRegistrations() const;

    /**
     * @brief Get registrations in a category (sorted by priority)
     */
    [[nodiscard]] std::vector<ToolRegistration> GetRegistrationsByCategory(
        ToolCategory category) const;

    /**
     * @brief Find tool ID by keyboard shortcut
     *
     * @param shortcut Shortcut string (e.g., "W", "Ctrl+G")
     * @return Tool ID or empty string if not found
     */
    [[nodiscard]] std::string FindToolByShortcut(const std::string& shortcut) const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get total number of registered tools
     */
    [[nodiscard]] size_t GetToolCount() const;

    /**
     * @brief Get number of tools in a category
     */
    [[nodiscard]] size_t GetToolCountInCategory(ToolCategory category) const;

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Save registry state to JSON (for debugging)
     */
    [[nodiscard]] nlohmann::json ToJson() const;

private:
    EditorToolRegistry() = default;
    ~EditorToolRegistry() = default;

    std::unordered_map<std::string, ToolRegistration> m_registrations;
};

// =============================================================================
// Tool Manager
// =============================================================================

/**
 * @brief Manages the active editor tool and tool switching
 *
 * Handles tool activation/deactivation, input routing, and persistence.
 */
class EditorToolManager {
public:
    EditorToolManager();
    ~EditorToolManager();

    // Non-copyable, movable
    EditorToolManager(const EditorToolManager&) = delete;
    EditorToolManager& operator=(const EditorToolManager&) = delete;
    EditorToolManager(EditorToolManager&&) noexcept;
    EditorToolManager& operator=(EditorToolManager&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the tool manager
     *
     * Creates default tools and loads saved tool settings.
     */
    void Initialize();

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========================================================================
    // Tool Selection
    // =========================================================================

    /**
     * @brief Set the active tool by ID
     *
     * @param toolId Tool identifier
     * @return true if tool was activated
     */
    bool SetActiveTool(const std::string& toolId);

    /**
     * @brief Set the active tool by category (first tool in category)
     *
     * @param category Tool category
     * @return true if tool was activated
     */
    bool SetActiveToolByCategory(ToolCategory category);

    /**
     * @brief Get the currently active tool
     */
    [[nodiscard]] IEditorTool* GetActiveTool() const { return m_activeTool.get(); }

    /**
     * @brief Get the ID of the active tool
     */
    [[nodiscard]] const std::string& GetActiveToolId() const { return m_activeToolId; }

    /**
     * @brief Push a temporary tool (e.g., for modifier key held)
     *
     * @param toolId Tool to temporarily activate
     * @return true if tool was pushed
     */
    bool PushTemporaryTool(const std::string& toolId);

    /**
     * @brief Pop the temporary tool, restoring previous
     */
    void PopTemporaryTool();

    /**
     * @brief Check if there's a temporary tool active
     */
    [[nodiscard]] bool HasTemporaryTool() const { return !m_toolStack.empty(); }

    /**
     * @brief Cycle to next tool in current category
     *
     * @param forward Direction to cycle
     */
    void CycleToolInCategory(bool forward = true);

    // =========================================================================
    // Input and Update
    // =========================================================================

    /**
     * @brief Process input event
     *
     * Routes input to active tool.
     *
     * @param event Input event
     * @param ctx Tool context
     * @return Result of input processing
     */
    [[nodiscard]] ToolInputResult ProcessInput(const ToolInputEvent& event,
                                                const ToolContext& ctx);

    /**
     * @brief Update active tool
     *
     * @param deltaTime Frame delta time
     * @param ctx Tool context
     */
    void Update(float deltaTime, const ToolContext& ctx);

    /**
     * @brief Render active tool overlays
     *
     * @param renderCtx Render context
     */
    void Render(const ToolRenderContext& renderCtx);

    /**
     * @brief Render active tool 2D overlay
     *
     * @param drawList ImGui draw list
     * @param ctx Tool context
     */
    void RenderOverlay(ImDrawList* drawList, const ToolContext& ctx);

    // =========================================================================
    // Settings
    // =========================================================================

    /**
     * @brief Load all tool settings from JSON
     *
     * @param settings JSON object with tool settings
     */
    void LoadAllSettings(const nlohmann::json& settings);

    /**
     * @brief Save all tool settings to JSON
     *
     * @return JSON object with all tool settings
     */
    [[nodiscard]] nlohmann::json SaveAllSettings() const;

    /**
     * @brief Get default tool ID (shown on startup)
     */
    [[nodiscard]] const std::string& GetDefaultToolId() const { return m_defaultToolId; }

    /**
     * @brief Set default tool ID
     */
    void SetDefaultToolId(const std::string& id) { m_defaultToolId = id; }

    // =========================================================================
    // Callbacks
    // =========================================================================

    using ToolChangedCallback = std::function<void(const std::string& oldId,
                                                    const std::string& newId)>;

    /**
     * @brief Register callback for tool changes
     *
     * @param callback Function to call on tool change
     * @return Callback ID for unregistering
     */
    uint64_t RegisterToolChangedCallback(ToolChangedCallback callback);

    /**
     * @brief Unregister a callback
     *
     * @param callbackId ID from RegisterToolChangedCallback
     */
    void UnregisterCallback(uint64_t callbackId);

private:
    void NotifyToolChanged(const std::string& oldId, const std::string& newId);

    std::unique_ptr<IEditorTool> m_activeTool;
    std::string m_activeToolId;
    std::string m_defaultToolId = "nova.transform.select";

    // Tool stack for temporary tools
    struct ToolStackEntry {
        std::unique_ptr<IEditorTool> tool;
        std::string id;
    };
    std::vector<ToolStackEntry> m_toolStack;

    // Cached tool settings
    std::unordered_map<std::string, nlohmann::json> m_toolSettings;

    // Callbacks
    struct CallbackEntry {
        uint64_t id;
        ToolChangedCallback callback;
    };
    std::vector<CallbackEntry> m_callbacks;
    uint64_t m_nextCallbackId = 1;

    bool m_initialized = false;
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert ToolInputEvent::Type to string
 */
constexpr const char* ToolInputEventTypeToString(ToolInputEvent::Type type) noexcept {
    switch (type) {
        case ToolInputEvent::Type::MouseDown:       return "MouseDown";
        case ToolInputEvent::Type::MouseUp:         return "MouseUp";
        case ToolInputEvent::Type::MouseMove:       return "MouseMove";
        case ToolInputEvent::Type::MouseDrag:       return "MouseDrag";
        case ToolInputEvent::Type::MouseDoubleClick: return "MouseDoubleClick";
        case ToolInputEvent::Type::KeyDown:         return "KeyDown";
        case ToolInputEvent::Type::KeyUp:           return "KeyUp";
        case ToolInputEvent::Type::KeyRepeat:       return "KeyRepeat";
        case ToolInputEvent::Type::Scroll:          return "Scroll";
        case ToolInputEvent::Type::TouchBegin:      return "TouchBegin";
        case ToolInputEvent::Type::TouchMove:       return "TouchMove";
        case ToolInputEvent::Type::TouchEnd:        return "TouchEnd";
        default:                                    return "Unknown";
    }
}

/**
 * @brief Convert MouseButton to string
 */
constexpr const char* MouseButtonToString(MouseButton button) noexcept {
    switch (button) {
        case MouseButton::None:   return "None";
        case MouseButton::Left:   return "Left";
        case MouseButton::Right:  return "Right";
        case MouseButton::Middle: return "Middle";
        case MouseButton::Extra1: return "Extra1";
        case MouseButton::Extra2: return "Extra2";
        default:                  return "Unknown";
    }
}

} // namespace Nova
