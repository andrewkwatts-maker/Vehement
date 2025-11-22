#include "core/Window.hpp"
#include "config/Config.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace Nova {

Window::~Window() {
    Destroy();
}

Window::Window(Window&& other) noexcept
    : m_window(std::exchange(other.m_window, nullptr))
    , m_size(other.m_size)
    , m_framebufferSize(other.m_framebufferSize)
    , m_windowedSize(other.m_windowedSize)
    , m_windowedPos(other.m_windowedPos)
    , m_title(std::move(other.m_title))
    , m_fullscreen(other.m_fullscreen)
    , m_vsync(other.m_vsync)
    , m_focused(other.m_focused)
    , m_callbacks(std::move(other.m_callbacks))
{
    // Update the user pointer to point to this object
    if (m_window) {
        glfwSetWindowUserPointer(m_window, this);
    }
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Destroy();  // Clean up existing window

        m_window = std::exchange(other.m_window, nullptr);
        m_size = other.m_size;
        m_framebufferSize = other.m_framebufferSize;
        m_windowedSize = other.m_windowedSize;
        m_windowedPos = other.m_windowedPos;
        m_title = std::move(other.m_title);
        m_fullscreen = other.m_fullscreen;
        m_vsync = other.m_vsync;
        m_focused = other.m_focused;
        m_callbacks = std::move(other.m_callbacks);

        // Update the user pointer to point to this object
        if (m_window) {
            glfwSetWindowUserPointer(m_window, this);
        }
    }
    return *this;
}

bool Window::Create() {
    auto& config = Config::Instance();

    CreateParams params;
    params.width = config.Get("window.width", 1920);
    params.height = config.Get("window.height", 1080);
    params.title = config.Get("window.title", std::string("Nova3D Engine"));
    params.fullscreen = config.Get("window.fullscreen", false);
    params.samples = config.Get("window.samples", 4);
    params.vsync = config.Get("window.vsync", true);

    return Create(params);
}

bool Window::Create(const CreateParams& params) {
    m_size = glm::ivec2(params.width, params.height);
    m_windowedSize = m_size;
    m_title = params.title;
    m_fullscreen = params.fullscreen;
    m_vsync = params.vsync;

    // Set OpenGL version hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Multisampling
    if (params.samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, params.samples);
    }

    // Window hints
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

#ifdef NOVA_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWmonitor* monitor = nullptr;
    if (m_fullscreen) {
        monitor = glfwGetPrimaryMonitor();
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            m_size = glm::ivec2(mode->width, mode->height);
        }
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
    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
    m_framebufferSize = glm::ivec2(fbWidth, fbHeight);

    // Set VSync
    SetVSync(m_vsync);

    // Center window if not fullscreen
    if (!m_fullscreen) {
        if (GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor()) {
            if (const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor)) {
                const int posX = (mode->width - m_size.x) / 2;
                const int posY = (mode->height - m_size.y) / 2;
                glfwSetWindowPos(m_window, posX, posY);
            }
        }
    }

    spdlog::info("Created window: {}x{} ({})", m_size.x, m_size.y,
                 m_fullscreen ? "fullscreen" : "windowed");

    return true;
}

bool Window::Create(int width, int height, std::string_view title,
                    bool fullscreen, int samples) {
    CreateParams params;
    params.width = width;
    params.height = height;
    params.title = std::string(title);
    params.fullscreen = fullscreen;
    params.samples = samples;
    return Create(params);
}

void Window::Destroy() noexcept {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

bool Window::ShouldClose() const noexcept {
    return m_window != nullptr && glfwWindowShouldClose(m_window) != 0;
}

void Window::Close() noexcept {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void Window::SwapBuffers() noexcept {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

void Window::SetTitle(std::string_view title) {
    m_title = std::string(title);
    if (m_window) {
        glfwSetWindowTitle(m_window, m_title.c_str());
    }
}

void Window::SetVSync(bool enabled) noexcept {
    m_vsync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void Window::SetFullscreen(bool fullscreen) {
    if (fullscreen == m_fullscreen) {
        return;
    }

    if (fullscreen) {
        // Save current window position and size
        glfwGetWindowPos(m_window, &m_windowedPos.x, &m_windowedPos.y);
        m_windowedSize = m_size;

        // Switch to fullscreen
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (const GLFWvidmode* mode = glfwGetVideoMode(monitor)) {
            glfwSetWindowMonitor(m_window, monitor, 0, 0,
                                mode->width, mode->height, mode->refreshRate);
        }
    } else {
        // Switch back to windowed
        glfwSetWindowMonitor(m_window, nullptr,
                            m_windowedPos.x, m_windowedPos.y,
                            m_windowedSize.x, m_windowedSize.y, 0);
    }

    m_fullscreen = fullscreen;
}

void Window::SetCursorMode(CursorMode mode) noexcept {
    if (m_window) {
        glfwSetInputMode(m_window, GLFW_CURSOR, static_cast<int>(mode));
    }
}

void Window::SetupCallbacks() {
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    glfwSetWindowSizeCallback(m_window, WindowSizeCallback);
    glfwSetWindowFocusCallback(m_window, WindowFocusCallback);
    glfwSetWindowCloseCallback(m_window, WindowCloseCallback);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        self->m_framebufferSize = glm::ivec2(width, height);
        glViewport(0, 0, width, height);
    }
}

void Window::WindowSizeCallback(GLFWwindow* window, int width, int height) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        self->m_size = glm::ivec2(width, height);
        if (self->m_callbacks.onResize) {
            self->m_callbacks.onResize(width, height);
        }
    }
}

void Window::WindowFocusCallback(GLFWwindow* window, int focused) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        self->m_focused = (focused != 0);
        if (self->m_callbacks.onFocus) {
            self->m_callbacks.onFocus(focused != 0);
        }
    }
}

void Window::WindowCloseCallback(GLFWwindow* window) {
    if (auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window))) {
        if (self->m_callbacks.onClose) {
            self->m_callbacks.onClose();
        }
    }
}

} // namespace Nova
