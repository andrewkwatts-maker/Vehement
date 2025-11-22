#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Nova {

/**
 * @brief Cursor mode options for SetCursorMode
 */
enum class CursorMode : int {
    Normal = 0x00034001,   // GLFW_CURSOR_NORMAL
    Hidden = 0x00034002,   // GLFW_CURSOR_HIDDEN
    Disabled = 0x00034003  // GLFW_CURSOR_DISABLED
};

/**
 * @brief Window management class
 *
 * Handles window creation, resizing, and related events.
 * Uses GLFW for cross-platform window management.
 * Supports RAII - window is destroyed when object goes out of scope.
 */
class Window {
public:
    /**
     * @brief Window event callbacks
     */
    struct Callbacks {
        std::function<void(int, int)> onResize;
        std::function<void(bool)> onFocus;
        std::function<void()> onClose;
    };

    /**
     * @brief Window creation parameters
     */
    struct CreateParams {
        int width = 1920;
        int height = 1080;
        std::string title = "Nova3D Engine";
        bool fullscreen = false;
        int samples = 4;
        bool vsync = true;
    };

    Window() noexcept = default;
    ~Window();

    // Delete copy operations - window handle cannot be copied
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Enable move operations for flexibility
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    /**
     * @brief Create the window using config settings
     * @return true if creation succeeded
     */
    [[nodiscard]] bool Create();

    /**
     * @brief Create window with explicit parameters
     * @param params Window creation parameters
     * @return true if creation succeeded
     */
    [[nodiscard]] bool Create(const CreateParams& params);

    /**
     * @brief Create window with explicit parameters (legacy overload)
     */
    [[nodiscard]] bool Create(int width, int height, std::string_view title,
                              bool fullscreen = false, int samples = 4);

    /**
     * @brief Close and destroy the window
     */
    void Destroy() noexcept;

    /**
     * @brief Check if window should close
     */
    [[nodiscard]] bool ShouldClose() const noexcept;

    /**
     * @brief Check if window is valid (created and not destroyed)
     */
    [[nodiscard]] bool IsValid() const noexcept { return m_window != nullptr; }

    /**
     * @brief Request window close
     */
    void Close() noexcept;

    /**
     * @brief Swap front and back buffers
     */
    void SwapBuffers() noexcept;

    /**
     * @brief Set window title
     * @param title New window title
     */
    void SetTitle(std::string_view title);

    /**
     * @brief Set VSync mode
     * @param enabled true to enable VSync
     */
    void SetVSync(bool enabled) noexcept;

    /**
     * @brief Toggle fullscreen mode
     * @param fullscreen true for fullscreen, false for windowed
     */
    void SetFullscreen(bool fullscreen);

    /**
     * @brief Set cursor mode
     * @param mode Cursor visibility/capture mode
     */
    void SetCursorMode(CursorMode mode) noexcept;

    /**
     * @brief Get the GLFW window handle
     */
    [[nodiscard]] GLFWwindow* GetHandle() const noexcept { return m_window; }

    /**
     * @brief Get window dimensions
     */
    [[nodiscard]] glm::ivec2 GetSize() const noexcept { return m_size; }
    [[nodiscard]] int GetWidth() const noexcept { return m_size.x; }
    [[nodiscard]] int GetHeight() const noexcept { return m_size.y; }

    /**
     * @brief Get framebuffer dimensions (may differ on high-DPI displays)
     */
    [[nodiscard]] glm::ivec2 GetFramebufferSize() const noexcept { return m_framebufferSize; }

    /**
     * @brief Get aspect ratio (returns 1.0f if height is zero)
     */
    [[nodiscard]] float GetAspectRatio() const noexcept {
        return m_size.y > 0 ? static_cast<float>(m_size.x) / static_cast<float>(m_size.y) : 1.0f;
    }

    /**
     * @brief Get DPI scale factor
     */
    [[nodiscard]] float GetDPIScale() const noexcept {
        return m_size.x > 0 ? static_cast<float>(m_framebufferSize.x) / static_cast<float>(m_size.x) : 1.0f;
    }

    /**
     * @brief Check if window is fullscreen
     */
    [[nodiscard]] bool IsFullscreen() const noexcept { return m_fullscreen; }

    /**
     * @brief Check if window has focus
     */
    [[nodiscard]] bool HasFocus() const noexcept { return m_focused; }

    /**
     * @brief Check if VSync is enabled
     */
    [[nodiscard]] bool IsVSyncEnabled() const noexcept { return m_vsync; }

    /**
     * @brief Set event callbacks
     * @param callbacks Callback functions for window events
     */
    void SetCallbacks(Callbacks callbacks) noexcept { m_callbacks = std::move(callbacks); }

private:
    // GLFW callbacks - static to interface with C API
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowFocusCallback(GLFWwindow* window, int focused);
    static void WindowCloseCallback(GLFWwindow* window);

    void SetupCallbacks();

    GLFWwindow* m_window = nullptr;
    glm::ivec2 m_size{1920, 1080};
    glm::ivec2 m_framebufferSize{1920, 1080};
    glm::ivec2 m_windowedSize{1920, 1080};  // Saved for fullscreen toggle
    glm::ivec2 m_windowedPos{100, 100};
    std::string m_title = "Nova3D Engine";
    bool m_fullscreen = false;
    bool m_vsync = true;
    bool m_focused = true;
    Callbacks m_callbacks;
};

} // namespace Nova
