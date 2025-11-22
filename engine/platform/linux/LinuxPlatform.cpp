/**
 * @file LinuxPlatform.cpp
 * @brief Linux platform implementation
 */

#include "LinuxPlatform.hpp"

#ifdef NOVA_PLATFORM_LINUX

#include <GLFW/glfw3.h>

// X11 native access (only if building with X11 support)
#if defined(GLFW_EXPOSE_NATIVE_X11)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

// Wayland native access
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <ctime>
#include <clocale>
#include <dirent.h>
#include <cmath>

namespace fs = std::filesystem;

namespace Nova {

// =============================================================================
// Static Methods
// =============================================================================

bool LinuxPlatform::IsWaylandSession() noexcept {
    const char* session = std::getenv("XDG_SESSION_TYPE");
    if (session && std::strcmp(session, "wayland") == 0) {
        return true;
    }
    // Also check for WAYLAND_DISPLAY
    return std::getenv("WAYLAND_DISPLAY") != nullptr;
}

bool LinuxPlatform::IsX11Session() noexcept {
    const char* session = std::getenv("XDG_SESSION_TYPE");
    if (session && std::strcmp(session, "x11") == 0) {
        return true;
    }
    // Also check for DISPLAY
    return std::getenv("DISPLAY") != nullptr && !IsWaylandSession();
}

// =============================================================================
// Constructor/Destructor
// =============================================================================

LinuxPlatform::LinuxPlatform() {
    m_state = PlatformState::Unknown;
}

LinuxPlatform::~LinuxPlatform() {
    Shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool LinuxPlatform::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_state = PlatformState::Starting;

    // Set GLFW error callback
    glfwSetErrorCallback(ErrorCallback);

    // Initialize GLFW
    if (!glfwInit()) {
        return false;
    }
    m_glfwInitialized = true;

    // Set platform-specific hints
    if (IsWaylandSession()) {
        SetWaylandHints();
    } else {
        SetX11Hints();
    }

    m_initialized = true;
    m_state = PlatformState::Running;

    return true;
}

void LinuxPlatform::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_state = PlatformState::Terminating;

    StopLocationUpdates();
    DestroyWindow();

    if (m_glfwInitialized) {
        glfwTerminate();
        m_glfwInitialized = false;
    }

    m_initialized = false;
    m_state = PlatformState::Unknown;
}

// =============================================================================
// Window Management
// =============================================================================

bool LinuxPlatform::CreateWindow(const WindowConfig& config) {
    if (m_window) {
        DestroyWindow();
    }

    // OpenGL context hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // Window hints
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, config.floating ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, config.highDPI ? GLFW_TRUE : GLFW_FALSE);

    // Multisampling
    if (config.samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, config.samples);
    }

    // Select monitor for fullscreen
    GLFWmonitor* monitor = nullptr;
    if (config.fullscreen) {
        if (config.monitor.has_value()) {
            int monitorCount;
            GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
            if (config.monitor.value() < monitorCount) {
                monitor = monitors[config.monitor.value()];
            }
        }
        if (!monitor) {
            monitor = glfwGetPrimaryMonitor();
        }
    }

    // Create window
    m_window = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        monitor,
        nullptr
    );

    if (!m_window) {
        return false;
    }

    // Store initial state
    m_title = config.title;
    m_fullscreen = config.fullscreen;
    m_windowedSize = {config.width, config.height};

    // Make context current
    glfwMakeContextCurrent(m_window);

    // VSync
    glfwSwapInterval(config.vsync ? 1 : 0);

    // Set size limits
    if (config.minWidth > 0 || config.minHeight > 0 ||
        config.maxWidth > 0 || config.maxHeight > 0) {
        glfwSetWindowSizeLimits(
            m_window,
            config.minWidth > 0 ? config.minWidth : GLFW_DONT_CARE,
            config.minHeight > 0 ? config.minHeight : GLFW_DONT_CARE,
            config.maxWidth > 0 ? config.maxWidth : GLFW_DONT_CARE,
            config.maxHeight > 0 ? config.maxHeight : GLFW_DONT_CARE
        );
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Setup callbacks
    SetupCallbacks();

    // Get initial sizes
    glfwGetWindowSize(m_window, &m_windowSize.x, &m_windowSize.y);
    glfwGetFramebufferSize(m_window, &m_framebufferSize.x, &m_framebufferSize.y);
    UpdateDisplayScale();

    return true;
}

void LinuxPlatform::DestroyWindow() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

void LinuxPlatform::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

glm::ivec2 LinuxPlatform::GetWindowSize() const {
    return m_windowSize;
}

glm::ivec2 LinuxPlatform::GetFramebufferSize() const {
    return m_framebufferSize;
}

float LinuxPlatform::GetDisplayScale() const {
    return m_displayScale;
}

void LinuxPlatform::SetFullscreen(bool fullscreen) {
    if (!m_window || m_fullscreen == fullscreen) {
        return;
    }

    if (fullscreen) {
        // Save windowed state
        glfwGetWindowPos(m_window, &m_windowedPos.x, &m_windowedPos.y);
        glfwGetWindowSize(m_window, &m_windowedSize.x, &m_windowedSize.y);

        // Get primary monitor
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        // Set fullscreen
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        // Restore windowed state
        glfwSetWindowMonitor(m_window, nullptr, m_windowedPos.x, m_windowedPos.y,
                            m_windowedSize.x, m_windowedSize.y, 0);
    }

    m_fullscreen = fullscreen;
}

void LinuxPlatform::SetWindowTitle(const std::string& title) {
    m_title = title;
    if (m_window) {
        glfwSetWindowTitle(m_window, title.c_str());
    }
}

void LinuxPlatform::SetWindowSize(int width, int height) {
    if (m_window && !m_fullscreen) {
        glfwSetWindowSize(m_window, width, height);
    }
}

void* LinuxPlatform::GetNativeWindowHandle() const {
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (m_window && IsX11Session()) {
        return reinterpret_cast<void*>(glfwGetX11Window(m_window));
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (m_window && IsWaylandSession()) {
        return glfwGetWaylandWindow(m_window);
    }
#endif
    return nullptr;
}

void* LinuxPlatform::GetNativeDisplayHandle() const {
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (IsX11Session()) {
        return glfwGetX11Display();
    }
#endif
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (IsWaylandSession()) {
        return glfwGetWaylandDisplay();
    }
#endif
    return nullptr;
}

// =============================================================================
// Input/Events
// =============================================================================

void LinuxPlatform::PollEvents() {
    glfwPollEvents();
}

void LinuxPlatform::WaitEvents() {
    glfwWaitEvents();
}

void LinuxPlatform::WaitEventsTimeout(double timeout) {
    glfwWaitEventsTimeout(timeout);
}

bool LinuxPlatform::ShouldClose() const {
    return m_window && glfwWindowShouldClose(m_window);
}

void LinuxPlatform::RequestClose() {
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

// =============================================================================
// File System (XDG paths)
// =============================================================================

std::string LinuxPlatform::GetXDGPath(const char* envVar, const char* defaultSubpath) const {
    const char* xdgPath = std::getenv(envVar);
    if (xdgPath && xdgPath[0] != '\0') {
        return std::string(xdgPath) + "/Nova3D/";
    }

    // Fallback to home directory
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }

    if (home) {
        return std::string(home) + "/" + defaultSubpath + "/Nova3D/";
    }

    return "./";
}

std::string LinuxPlatform::GetDataPath() const {
    return GetXDGPath("XDG_DATA_HOME", ".local/share");
}

std::string LinuxPlatform::GetCachePath() const {
    return GetXDGPath("XDG_CACHE_HOME", ".cache");
}

std::string LinuxPlatform::GetDocumentsPath() const {
    // Check XDG user dirs
    const char* home = std::getenv("HOME");
    if (home) {
        // Try to read user-dirs.dirs
        std::string userDirsPath = std::string(home) + "/.config/user-dirs.dirs";
        std::ifstream file(userDirsPath);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("XDG_DOCUMENTS_DIR") == 0) {
                    size_t start = line.find('"');
                    size_t end = line.rfind('"');
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        std::string path = line.substr(start + 1, end - start - 1);
                        // Replace $HOME
                        size_t homePos = path.find("$HOME");
                        if (homePos != std::string::npos) {
                            path.replace(homePos, 5, home);
                        }
                        return path + "/";
                    }
                }
            }
        }
        return std::string(home) + "/Documents/";
    }
    return "./";
}

std::string LinuxPlatform::GetBundlePath() const {
    char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        std::string exePath(path);
        size_t lastSlash = exePath.rfind('/');
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
    return "./";
}

std::string LinuxPlatform::GetAssetsPath() const {
    return GetBundlePath() + "assets/";
}

std::vector<uint8_t> LinuxPlatform::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return {};
    }

    return buffer;
}

std::string LinuxPlatform::ReadFileAsString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool LinuxPlatform::WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    // Create parent directories if needed
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool LinuxPlatform::WriteFile(const std::string& path, const std::string& content) {
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << content;
    return file.good();
}

bool LinuxPlatform::FileExists(const std::string& path) const {
    return fs::exists(path);
}

bool LinuxPlatform::IsDirectory(const std::string& path) const {
    return fs::is_directory(path);
}

bool LinuxPlatform::CreateDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec) || fs::exists(path);
}

bool LinuxPlatform::DeleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec) || !fs::exists(path);
}

std::vector<std::string> LinuxPlatform::ListFiles(const std::string& path, bool recursive) {
    std::vector<std::string> files;

    if (!fs::exists(path) || !fs::is_directory(path)) {
        return files;
    }

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error&) {
        // Permission denied or other error
    }

    return files;
}

// =============================================================================
// Permissions (Desktop - mostly no-ops)
// =============================================================================

void LinuxPlatform::RequestPermission(Permission permission, PermissionCallback callback) {
    // On desktop Linux, most permissions are granted by default
    // or handled by the system (e.g., polkit for location)
    if (callback) {
        callback(permission, PermissionResult::Granted);
    }
}

bool LinuxPlatform::HasPermission(Permission /*permission*/) const {
    // Desktop Linux doesn't have a permission system like mobile
    return true;
}

PermissionResult LinuxPlatform::GetPermissionStatus(Permission /*permission*/) const {
    return PermissionResult::Granted;
}

void LinuxPlatform::OpenPermissionSettings() {
    // Try to open GNOME Settings or KDE System Settings
    system("xdg-open gnome-control-center &>/dev/null || systemsettings5 &>/dev/null &");
}

// =============================================================================
// GPS/Location (Stub - would use GeoClue2 D-Bus API)
// =============================================================================

bool LinuxPlatform::IsLocationAvailable() const {
    // Check if GeoClue2 service is available
    // This would require D-Bus communication in a full implementation
    return fs::exists("/usr/share/dbus-1/services/org.freedesktop.GeoClue2.service") ||
           fs::exists("/usr/lib/systemd/user/geoclue-agent.service");
}

bool LinuxPlatform::IsLocationEnabled() const {
    return IsLocationAvailable();
}

void LinuxPlatform::StartLocationUpdates(const LocationConfig& /*config*/,
                                          LocationCallback callback,
                                          LocationErrorCallback errorCallback) {
    m_locationCallback = std::move(callback);
    m_locationErrorCallback = std::move(errorCallback);
    m_locationUpdatesActive = true;

    // In a full implementation, this would:
    // 1. Connect to GeoClue2 via D-Bus
    // 2. Create a client
    // 3. Start receiving location updates

    if (m_locationErrorCallback) {
        m_locationErrorCallback(1, "GeoClue2 integration not implemented");
    }
}

void LinuxPlatform::StartLocationUpdates(LocationCallback callback) {
    StartLocationUpdates(LocationConfig{}, std::move(callback), nullptr);
}

void LinuxPlatform::StopLocationUpdates() {
    m_locationUpdatesActive = false;
    m_locationCallback = nullptr;
    m_locationErrorCallback = nullptr;
}

void LinuxPlatform::RequestSingleLocation(LocationCallback callback) {
    // Same as StartLocationUpdates but for a single update
    if (callback) {
        callback(m_lastLocation);
    }
}

GPSCoordinates LinuxPlatform::GetLastKnownLocation() const {
    return m_lastLocation;
}

// =============================================================================
// System Information
// =============================================================================

uint64_t LinuxPlatform::GetAvailableMemory() const {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<uint64_t>(info.freeram) * info.mem_unit;
    }
    return 0;
}

uint64_t LinuxPlatform::GetTotalMemory() const {
    if (m_cachedTotalMemory == 0) {
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            m_cachedTotalMemory = static_cast<uint64_t>(info.totalram) * info.mem_unit;
        }
    }
    return m_cachedTotalMemory;
}

int LinuxPlatform::GetCPUCores() const {
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
}

std::string LinuxPlatform::GetCPUArchitecture() const {
    struct utsname info;
    if (uname(&info) == 0) {
        return info.machine;
    }
    return "unknown";
}

bool LinuxPlatform::HasGPUCompute() const {
    // Check for OpenCL or CUDA support
    return fs::exists("/dev/nvidia0") ||
           fs::exists("/dev/dri/renderD128") ||
           fs::exists("/usr/lib/x86_64-linux-gnu/libOpenCL.so.1");
}

std::string LinuxPlatform::GetDeviceModel() const {
    if (m_cachedHostname.empty()) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            m_cachedHostname = hostname;
        } else {
            m_cachedHostname = "Linux Desktop";
        }
    }
    return m_cachedHostname;
}

std::string LinuxPlatform::GetOSVersion() const {
    if (m_cachedOSVersion.empty()) {
        // Try to read from /etc/os-release
        std::ifstream osRelease("/etc/os-release");
        if (osRelease.is_open()) {
            std::string line;
            std::string prettyName;
            while (std::getline(osRelease, line)) {
                if (line.find("PRETTY_NAME=") == 0) {
                    size_t start = line.find('"');
                    size_t end = line.rfind('"');
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        prettyName = line.substr(start + 1, end - start - 1);
                        break;
                    }
                }
            }
            if (!prettyName.empty()) {
                m_cachedOSVersion = prettyName;
                return m_cachedOSVersion;
            }
        }

        // Fallback to uname
        struct utsname info;
        if (uname(&info) == 0) {
            m_cachedOSVersion = std::string(info.sysname) + " " + info.release;
        } else {
            m_cachedOSVersion = "Linux";
        }
    }
    return m_cachedOSVersion;
}

std::string LinuxPlatform::GetDeviceId() const {
    // Try to read machine-id
    std::ifstream machineId("/etc/machine-id");
    if (machineId.is_open()) {
        std::string id;
        std::getline(machineId, id);
        return id;
    }

    // Fallback to /var/lib/dbus/machine-id
    std::ifstream dbusId("/var/lib/dbus/machine-id");
    if (dbusId.is_open()) {
        std::string id;
        std::getline(dbusId, id);
        return id;
    }

    return "";
}

std::string LinuxPlatform::GetLocale() const {
    const char* locale = std::setlocale(LC_ALL, nullptr);
    if (locale) {
        std::string loc(locale);
        // Convert locale format (e.g., "en_US.UTF-8" to "en-US")
        size_t dotPos = loc.find('.');
        if (dotPos != std::string::npos) {
            loc = loc.substr(0, dotPos);
        }
        size_t underscorePos = loc.find('_');
        if (underscorePos != std::string::npos) {
            loc[underscorePos] = '-';
        }
        return loc;
    }
    return "en-US";
}

int LinuxPlatform::GetTimezoneOffset() const {
    time_t now = time(nullptr);
    struct tm local_tm;
    struct tm utc_tm;
    localtime_r(&now, &local_tm);
    gmtime_r(&now, &utc_tm);

    time_t local_time = mktime(&local_tm);
    time_t utc_time = mktime(&utc_tm);

    return static_cast<int>(difftime(local_time, utc_time));
}

bool LinuxPlatform::HasHardwareFeature(const std::string& feature) const {
    // Check /proc/cpuinfo for CPU features
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("flags") == 0 || line.find("Features") == 0) {
                return line.find(feature) != std::string::npos;
            }
        }
    }
    return false;
}

// =============================================================================
// Battery Status
// =============================================================================

float LinuxPlatform::GetBatteryLevel() const {
    // Try to read from /sys/class/power_supply/
    const std::string batteryPath = "/sys/class/power_supply/BAT0/capacity";
    std::ifstream file(batteryPath);
    if (file.is_open()) {
        int capacity;
        file >> capacity;
        return static_cast<float>(capacity) / 100.0f;
    }

    // Try BAT1
    std::ifstream file1("/sys/class/power_supply/BAT1/capacity");
    if (file1.is_open()) {
        int capacity;
        file1 >> capacity;
        return static_cast<float>(capacity) / 100.0f;
    }

    return -1.0f;  // No battery or not available
}

bool LinuxPlatform::IsBatteryCharging() const {
    const std::string statusPath = "/sys/class/power_supply/BAT0/status";
    std::ifstream file(statusPath);
    if (file.is_open()) {
        std::string status;
        std::getline(file, status);
        return status == "Charging" || status == "Full";
    }
    return false;
}

// =============================================================================
// Network Status
// =============================================================================

bool LinuxPlatform::IsNetworkAvailable() const {
    // Check if any non-loopback interface is up
    // Simple check: try to read from /sys/class/net/
    for (const auto& entry : fs::directory_iterator("/sys/class/net/")) {
        std::string ifname = entry.path().filename().string();
        if (ifname == "lo") continue;  // Skip loopback

        std::ifstream operstate(entry.path() / "operstate");
        if (operstate.is_open()) {
            std::string state;
            operstate >> state;
            if (state == "up") {
                return true;
            }
        }
    }
    return false;
}

bool LinuxPlatform::IsWifiConnected() const {
    // Check for wireless interfaces
    for (const auto& entry : fs::directory_iterator("/sys/class/net/")) {
        std::string ifname = entry.path().filename().string();
        if (ifname.find("wl") == 0 || ifname.find("wifi") == 0) {
            std::ifstream operstate(entry.path() / "operstate");
            if (operstate.is_open()) {
                std::string state;
                operstate >> state;
                if (state == "up") {
                    return true;
                }
            }
        }
    }
    return false;
}

bool LinuxPlatform::IsCellularConnected() const {
    // Check for mobile broadband interfaces (usually wwanX or wwan0)
    for (const auto& entry : fs::directory_iterator("/sys/class/net/")) {
        std::string ifname = entry.path().filename().string();
        if (ifname.find("wwan") == 0) {
            std::ifstream operstate(entry.path() / "operstate");
            if (operstate.is_open()) {
                std::string state;
                operstate >> state;
                if (state == "up") {
                    return true;
                }
            }
        }
    }
    return false;
}

// =============================================================================
// App Lifecycle
// =============================================================================

void LinuxPlatform::SetLifecycleCallbacks(LifecycleCallbacks callbacks) {
    m_lifecycleCallbacks = std::move(callbacks);
}

// =============================================================================
// Haptics (not supported on desktop)
// =============================================================================

void LinuxPlatform::TriggerHaptic(HapticType /*type*/) {
    // Haptic feedback not available on desktop Linux
}

// =============================================================================
// Linux-Specific
// =============================================================================

Display* LinuxPlatform::GetX11Display() const {
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (IsX11Session()) {
        return glfwGetX11Display();
    }
#endif
    return nullptr;
}

unsigned long LinuxPlatform::GetX11Window() const {
#if defined(GLFW_EXPOSE_NATIVE_X11)
    if (m_window && IsX11Session()) {
        return glfwGetX11Window(m_window);
    }
#endif
    return 0;
}

wl_display* LinuxPlatform::GetWaylandDisplay() const {
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
    if (IsWaylandSession()) {
        return glfwGetWaylandDisplay();
    }
#endif
    return nullptr;
}

void LinuxPlatform::SetWaylandHints() {
    // Wayland-specific window hints
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
}

void LinuxPlatform::SetX11Hints() {
    // X11-specific window hints
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
}

// =============================================================================
// GLFW Callbacks
// =============================================================================

void LinuxPlatform::ErrorCallback(int error, const char* description) {
    // Log error - in production, use proper logging
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void LinuxPlatform::WindowSizeCallback(GLFWwindow* window, int width, int height) {
    auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->m_windowSize = {width, height};
        platform->UpdateDisplayScale();
    }
}

void LinuxPlatform::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->m_framebufferSize = {width, height};
        platform->UpdateDisplayScale();
    }
}

void LinuxPlatform::WindowFocusCallback(GLFWwindow* window, int focused) {
    auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->m_focused = (focused == GLFW_TRUE);

        // Trigger lifecycle callbacks
        if (focused) {
            platform->m_state = PlatformState::Foreground;
            if (platform->m_lifecycleCallbacks.onResume) {
                platform->m_lifecycleCallbacks.onResume();
            }
        } else {
            platform->m_state = PlatformState::Background;
            if (platform->m_lifecycleCallbacks.onPause) {
                platform->m_lifecycleCallbacks.onPause();
            }
        }

        if (platform->m_stateCallback) {
            platform->m_stateCallback(platform->m_state);
        }
    }
}

void LinuxPlatform::WindowCloseCallback(GLFWwindow* window) {
    auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->m_state = PlatformState::Terminating;
        if (platform->m_lifecycleCallbacks.onTerminate) {
            platform->m_lifecycleCallbacks.onTerminate();
        }
        if (platform->m_stateCallback) {
            platform->m_stateCallback(platform->m_state);
        }
    }
}

void LinuxPlatform::WindowIconifyCallback(GLFWwindow* window, int iconified) {
    auto* platform = static_cast<LinuxPlatform*>(glfwGetWindowUserPointer(window));
    if (platform) {
        platform->m_iconified = (iconified == GLFW_TRUE);
    }
}

void LinuxPlatform::SetupCallbacks() {
    if (!m_window) return;

    glfwSetWindowSizeCallback(m_window, WindowSizeCallback);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    glfwSetWindowFocusCallback(m_window, WindowFocusCallback);
    glfwSetWindowCloseCallback(m_window, WindowCloseCallback);
    glfwSetWindowIconifyCallback(m_window, WindowIconifyCallback);
}

void LinuxPlatform::UpdateDisplayScale() {
    if (m_windowSize.x > 0) {
        m_displayScale = static_cast<float>(m_framebufferSize.x) / static_cast<float>(m_windowSize.x);
    }
}

// =============================================================================
// GPS Coordinate Methods
// =============================================================================

double GPSCoordinates::DistanceTo(const GPSCoordinates& other) const noexcept {
    constexpr double EARTH_RADIUS = 6371000.0;  // meters

    double lat1 = latitude * M_PI / 180.0;
    double lat2 = other.latitude * M_PI / 180.0;
    double dLat = (other.latitude - latitude) * M_PI / 180.0;
    double dLon = (other.longitude - longitude) * M_PI / 180.0;

    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return EARTH_RADIUS * c;
}

float GPSCoordinates::BearingTo(const GPSCoordinates& other) const noexcept {
    double lat1 = latitude * M_PI / 180.0;
    double lat2 = other.latitude * M_PI / 180.0;
    double dLon = (other.longitude - longitude) * M_PI / 180.0;

    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) -
               std::sin(lat1) * std::cos(lat2) * std::cos(dLon);

    double bearing = std::atan2(y, x) * 180.0 / M_PI;
    return static_cast<float>(std::fmod(bearing + 360.0, 360.0));
}

// =============================================================================
// Platform Factory Implementation
// =============================================================================

std::unique_ptr<Platform> Platform::Create() {
    return std::make_unique<LinuxPlatform>();
}

PlatformType Platform::GetCurrentPlatform() noexcept {
    return PlatformType::Linux;
}

const char* Platform::GetPlatformName() noexcept {
    return "Linux";
}

bool Platform::IsDesktop() noexcept {
    return true;
}

bool Platform::IsMobile() noexcept {
    return false;
}

} // namespace Nova

#endif // NOVA_PLATFORM_LINUX
