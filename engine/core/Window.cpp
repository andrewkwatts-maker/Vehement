#include "core/Window.hpp"
#include "config/Config.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace Nova {

Window::Window() = default;

Window::~Window() {
    Destroy();
}

bool Window::Create() {
    auto& config = Config::Instance();

    int width = config.Get("window.width", 1920);
    int height = config.Get("window.height", 1080);
    std::string title = config.Get("window.title", std::string("Nova3D Engine"));
    bool fullscreen = config.Get("window.fullscreen", false);
    int samples = config.Get("window.samples", 4);
    m_vsync = config.Get("window.vsync", true);

    return Create(width, height, title, fullscreen, samples);
}

bool Window::Create(int width, int height, const std::string& title,
                    bool fullscreen, int samples) {
    m_size = glm::ivec2(width, height);
    m_windowedSize = m_size;
    m_title = title;
    m_fullscreen = fullscreen;

    // Set OpenGL version hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Multisampling
    if (samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, samples);
    }

    // Window hints
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

#ifdef NOVA_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWmonitor* monitor = nullptr;
    if (fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        m_size = glm::ivec2(mode->width, mode->height);
    }

    m_window = glfwCreateWindow(m_size.x, m_size.y, m_title.c_str(), monitor, nullptr);
    if (!m_window) {
        spdlog::critical("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(m_window);

    // Store this pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);
    SetupCallbacks();

    // Get actual framebuffer size
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
    m_framebufferSize = glm::ivec2(fbWidth, fbHeight);

    // Set VSync
    SetVSync(m_vsync);

    // Center window if not fullscreen
    if (!fullscreen) {
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        glfwSetWindowPos(m_window,
            (mode->width - m_size.x) / 2,
            (mode->height - m_size.y) / 2);
    }

    spdlog::info("Created window: {}x{} ({})", m_size.x, m_size.y,
                 fullscreen ? "fullscreen" : "windowed");

    return true;
}

void Window::Destroy() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

bool Window::ShouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void Window::Close() {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void Window::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void Window::SetTitle(const std::string& title) {
    m_title = title;
    if (m_window) {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

void Window::SetVSync(bool enabled) {
    m_vsync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::SetFullscreen(bool fullscreen) {
    if (fullscreen == m_fullscreen) return;

    if (fullscreen) {
        // Save current window position and size
        glfwGetWindowPos(m_window, &m_windowedPos.x, &m_windowedPos.y);
        m_windowedSize = m_size;

        // Switch to fullscreen
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0,
                            mode->width, mode->height, mode->refreshRate);
    } else {
        // Switch back to windowed
        glfwSetWindowMonitor(m_window, nullptr,
                            m_windowedPos.x, m_windowedPos.y,
                            m_windowedSize.x, m_windowedSize.y, 0);
    }

    m_fullscreen = fullscreen;
}

void Window::SetCursorMode(int mode) {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, mode);
    }
}

void Window::SetupCallbacks() {
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    glfwSetWindowSizeCallback(m_window, WindowSizeCallback);
    glfwSetWindowFocusCallback(m_window, WindowFocusCallback);
    glfwSetWindowCloseCallback(m_window, WindowCloseCallback);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_framebufferSize = glm::ivec2(width, height);
        glViewport(0, 0, width, height);
    }
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_size = glm::ivec2(width, height);
        if (self->m_callbacks.onResize) {
            self->m_callbacks.onResize(width, height);
        }
    }
}

void Window::WindowFocusCallback(GLFWwindow* window, int focused) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_focused = focused;
        if (self->m_callbacks.onFocus) {
            self->m_callbacks.onFocus(focused);
        }
    }
}

void Window::WindowCloseCallback(GLFWwindow* window) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_callbacks.onClose) {
        self->m_callbacks.onClose();
    }
}

} // namespace Nova
