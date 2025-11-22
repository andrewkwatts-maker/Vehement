#pragma once

#include <string>
#include <functional>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Nova {

/**
 * @brief Window management class
 *
 * Handles window creation, resizing, and related events.
 * Uses GLFW for cross-platform window management.
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

    Window();
    ~Window();

    // Delete copy/move
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /**
     * @brief Create the window using config settings
     * @return true if creation succeeded
     */
    bool Create();

    /**
     * @brief Create window with explicit parameters
     */
    bool Create(int width, int height, const std::string& title,
                bool fullscreen = false, int samples = 4);

    /**
     * @brief Close and destroy the window
     */
    void Destroy();

    /**
     * @brief Check if window should close
     */
    bool ShouldClose() const;

    /**
     * @brief Request window close
     */
    void Close();

    /**
     * @brief Swap front and back buffers
     */
    void SwapBuffers();

    /**
     * @brief Set window title
     */
    void SetTitle(const std::string& title);

    /**
     * @brief Set VSync mode
     */
    void SetVSync(bool enabled);

    /**
     * @brief Toggle fullscreen mode
     */
    void SetFullscreen(bool fullscreen);

    /**
     * @brief Set cursor mode (normal, hidden, disabled)
     */
    void SetCursorMode(int mode);

    /**
     * @brief Get the GLFW window handle
     */
    GLFWwindow* GetHandle() const { return m_window; }

    /**
     * @brief Get window dimensions
     */
    glm::ivec2 GetSize() const { return m_size; }
    int GetWidth() const { return m_size.x; }
    int GetHeight() const { return m_size.y; }

    /**
     * @brief Get framebuffer dimensions (may differ on high-DPI displays)
     */
    glm::ivec2 GetFramebufferSize() const { return m_framebufferSize; }

    /**
     * @brief Get aspect ratio
     */
    float GetAspectRatio() const {
        return m_size.y > 0 ? static_cast<float>(m_size.x) / m_size.y : 1.0f;
    }

    /**
     * @brief Check if window is fullscreen
     */
    bool IsFullscreen() const { return m_fullscreen; }

    /**
     * @brief Check if window has focus
     */
    bool HasFocus() const { return m_focused; }

    /**
     * @brief Set event callbacks
     */
    void SetCallbacks(const Callbacks& callbacks) { m_callbacks = callbacks; }

private:
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
