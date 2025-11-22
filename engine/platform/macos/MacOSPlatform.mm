/**
 * @file MacOSPlatform.mm
 * @brief Full macOS platform implementation using Cocoa/AppKit
 *
 * Features:
 * - NSWindow creation and management
 * - Event handling
 * - Retina display support
 * - Menu bar integration
 * - Dock integration
 * - Native fullscreen (Mission Control)
 * - Core Location for GPS
 */

#include "MacOSPlatform.hpp"

#ifdef NOVA_PLATFORM_MACOS

#import <Cocoa/Cocoa.h>
#import <CoreLocation/CoreLocation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/ps/IOPSKeys.h>
#import <IOKit/ps/IOPowerSources.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <sys/sysctl.h>
#import <sys/types.h>
#import <mach/mach.h>
#import <mach/mach_host.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Nova {

// =============================================================================
// Objective-C Helper Classes
// =============================================================================

@interface NovaWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) Nova::MacOSPlatform* platform;
@end

@interface NovaApplicationDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, assign) Nova::MacOSPlatform* platform;
@end

@interface NovaLocationDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, assign) Nova::MacOSPlatform* platform;
@end

@interface NovaWindow : NSWindow
@end

@interface NovaView : NSView
@property (nonatomic, assign) Nova::MacOSPlatform* platform;
@end

// =============================================================================
// MacOSPlatform Implementation Structure
// =============================================================================

struct MacOSPlatform::Impl {
    NSWindow* window = nil;
    NovaView* view = nil;
    NovaWindowDelegate* windowDelegate = nil;
    NovaApplicationDelegate* appDelegate = nil;
    NovaLocationDelegate* locationDelegate = nil;
    CLLocationManager* locationManager = nil;
    NSOpenGLContext* glContext = nil;

    glm::ivec2 windowSize{1920, 1080};
    glm::ivec2 framebufferSize{1920, 1080};
    glm::ivec2 windowedSize{1920, 1080};
    NSPoint windowedPosition{100, 100};

    float displayScale = 1.0f;
    bool fullscreen = false;
    bool shouldClose = false;
    bool focused = true;

    std::string windowTitle = "Nova3D Engine";

    // Location
    GPSCoordinates lastLocation;
    LocationCallback locationCallback;
    LocationErrorCallback locationErrorCallback;
    bool locationUpdatesActive = false;

    // Lifecycle
    Platform::LifecycleCallbacks lifecycleCallbacks;

    // Cached info
    mutable std::string cachedDeviceModel;
    mutable std::string cachedOSVersion;
    mutable std::string cachedLocale;
    mutable uint64_t cachedTotalMemory = 0;
};

// =============================================================================
// Objective-C Implementation
// =============================================================================

@implementation NovaWindowDelegate

- (void)windowDidResize:(NSNotification*)notification {
    if (self.platform) {
        NSWindow* window = notification.object;
        NSRect contentRect = [window contentRectForFrameRect:window.frame];
        self.platform->OnWindowResize(static_cast<int>(contentRect.size.width),
                                       static_cast<int>(contentRect.size.height));
    }
}

- (void)windowDidMove:(NSNotification*)notification {
    (void)notification;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    if (self.platform) {
        self.platform->OnWindowFocus(true);
    }
    (void)notification;
}

- (void)windowDidResignKey:(NSNotification*)notification {
    if (self.platform) {
        self.platform->OnWindowFocus(false);
    }
    (void)notification;
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
    (void)notification;
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
    if (self.platform) {
        self.platform->OnFullscreenChange(true);
    }
    (void)notification;
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
    (void)notification;
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
    if (self.platform) {
        self.platform->OnFullscreenChange(false);
    }
    (void)notification;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    if (self.platform) {
        self.platform->OnWindowClose();
    }
    (void)sender;
    return NO;  // We handle closing ourselves
}

@end

@implementation NovaApplicationDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
}

- (void)applicationWillTerminate:(NSNotification*)notification {
    if (self.platform && self.platform->m_impl->lifecycleCallbacks.onTerminate) {
        self.platform->m_impl->lifecycleCallbacks.onTerminate();
    }
    (void)notification;
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
    if (self.platform && self.platform->m_impl->lifecycleCallbacks.onResume) {
        self.platform->m_impl->lifecycleCallbacks.onResume();
    }
    (void)notification;
}

- (void)applicationDidResignActive:(NSNotification*)notification {
    if (self.platform && self.platform->m_impl->lifecycleCallbacks.onPause) {
        self.platform->m_impl->lifecycleCallbacks.onPause();
    }
    (void)notification;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    return YES;
}

@end

@implementation NovaLocationDelegate

- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations {
    if (locations.count > 0 && self.platform) {
        CLLocation* location = locations.lastObject;

        GPSCoordinates coords;
        coords.latitude = location.coordinate.latitude;
        coords.longitude = location.coordinate.longitude;
        coords.altitude = location.altitude;
        coords.accuracy = static_cast<float>(location.horizontalAccuracy);
        coords.altitudeAccuracy = static_cast<float>(location.verticalAccuracy);
        coords.speed = static_cast<float>(location.speed);
        coords.bearing = static_cast<float>(location.course);
        coords.timestamp = static_cast<uint64_t>([location.timestamp timeIntervalSince1970] * 1000);
        coords.valid = (location.horizontalAccuracy >= 0);

        self.platform->OnLocationUpdate(coords);
    }
    (void)manager;
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
    if (self.platform) {
        self.platform->OnLocationError(static_cast<int>(error.code),
                                        [error.localizedDescription UTF8String]);
    }
    (void)manager;
}

- (void)locationManager:(CLLocationManager*)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    (void)manager;
    (void)status;
}

@end

@implementation NovaWindow

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}

@end

@implementation NovaView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    // Drawing handled by OpenGL/Metal
}

@end

// =============================================================================
// MacOSPlatform C++ Implementation
// =============================================================================

MacOSPlatform::MacOSPlatform()
    : m_impl(std::make_unique<Impl>()) {}

MacOSPlatform::~MacOSPlatform() {
    Shutdown();
}

bool MacOSPlatform::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_state = PlatformState::Starting;

    @autoreleasepool {
        // Ensure we have an NSApplication instance
        [NSApplication sharedApplication];

        // Set up application delegate
        m_impl->appDelegate = [[NovaApplicationDelegate alloc] init];
        m_impl->appDelegate.platform = this;
        [NSApp setDelegate:m_impl->appDelegate];

        // Set activation policy
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Create menu bar
        CreateMenuBar();

        // Finish launching
        [NSApp finishLaunching];

        // Activate the application
        [NSApp activateIgnoringOtherApps:YES];
    }

    m_initialized = true;
    m_state = PlatformState::Running;

    return true;
}

void MacOSPlatform::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_state = PlatformState::Terminating;

    @autoreleasepool {
        StopLocationUpdates();
        DestroyWindow();

        if (m_impl->appDelegate) {
            [NSApp setDelegate:nil];
            m_impl->appDelegate = nil;
        }
    }

    m_initialized = false;
    m_state = PlatformState::Unknown;
}

bool MacOSPlatform::CreateWindow(const WindowConfig& config) {
    @autoreleasepool {
        // Window style
        NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                       NSWindowStyleMaskClosable |
                                       NSWindowStyleMaskMiniaturizable;
        if (config.resizable) {
            styleMask |= NSWindowStyleMaskResizable;
        }

        // Calculate window rect
        NSRect contentRect = NSMakeRect(0, 0, config.width, config.height);

        // Create window
        m_impl->window = [[NovaWindow alloc] initWithContentRect:contentRect
                                                       styleMask:styleMask
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        if (!m_impl->window) {
            return false;
        }

        // Create content view
        m_impl->view = [[NovaView alloc] initWithFrame:contentRect];
        m_impl->view.platform = this;
        [m_impl->window setContentView:m_impl->view];

        // Set up window delegate
        m_impl->windowDelegate = [[NovaWindowDelegate alloc] init];
        m_impl->windowDelegate.platform = this;
        [m_impl->window setDelegate:m_impl->windowDelegate];

        // Configure window
        [m_impl->window setTitle:[NSString stringWithUTF8String:config.title.c_str()]];
        [m_impl->window setAcceptsMouseMovedEvents:YES];
        [m_impl->window setReleasedWhenClosed:NO];
        [m_impl->window center];

        // Enable fullscreen button
        [m_impl->window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

        // High DPI support
        if (config.highDPI) {
            [m_impl->view setWantsBestResolutionOpenGLSurface:YES];
        }

        // Store configuration
        m_impl->windowTitle = config.title;
        m_impl->windowSize = {config.width, config.height};
        m_impl->windowedSize = m_impl->windowSize;

        // Get actual backing size (for Retina)
        NSRect backingBounds = [m_impl->view convertRectToBacking:m_impl->view.bounds];
        m_impl->framebufferSize = {static_cast<int>(backingBounds.size.width),
                                   static_cast<int>(backingBounds.size.height)};
        m_impl->displayScale = m_impl->framebufferSize.x / static_cast<float>(m_impl->windowSize.x);

        // Show window
        if (config.visible) {
            [m_impl->window makeKeyAndOrderFront:nil];
        }

        // Enter fullscreen if requested
        if (config.fullscreen) {
            [m_impl->window toggleFullScreen:nil];
        }
    }

    return true;
}

void MacOSPlatform::DestroyWindow() {
    @autoreleasepool {
        if (m_impl->glContext) {
            [m_impl->glContext clearDrawable];
            m_impl->glContext = nil;
        }

        if (m_impl->window) {
            [m_impl->window setDelegate:nil];
            [m_impl->window close];
            m_impl->window = nil;
        }

        m_impl->windowDelegate = nil;
        m_impl->view = nil;
    }
}

bool MacOSPlatform::HasWindow() const noexcept {
    return m_impl->window != nil;
}

void MacOSPlatform::SwapBuffers() {
    if (m_impl->glContext) {
        [m_impl->glContext flushBuffer];
    }
}

glm::ivec2 MacOSPlatform::GetWindowSize() const {
    return m_impl->windowSize;
}

glm::ivec2 MacOSPlatform::GetFramebufferSize() const {
    return m_impl->framebufferSize;
}

float MacOSPlatform::GetDisplayScale() const {
    return m_impl->displayScale;
}

bool MacOSPlatform::IsFullscreen() const {
    return m_impl->fullscreen;
}

void MacOSPlatform::SetFullscreen(bool fullscreen) {
    if (m_impl->fullscreen == fullscreen || !m_impl->window) {
        return;
    }

    @autoreleasepool {
        [m_impl->window toggleFullScreen:nil];
    }
}

void MacOSPlatform::SetWindowTitle(const std::string& title) {
    m_impl->windowTitle = title;
    if (m_impl->window) {
        @autoreleasepool {
            [m_impl->window setTitle:[NSString stringWithUTF8String:title.c_str()]];
        }
    }
}

void MacOSPlatform::SetWindowSize(int width, int height) {
    if (m_impl->window && !m_impl->fullscreen) {
        @autoreleasepool {
            NSRect frame = [m_impl->window frame];
            NSRect contentRect = [m_impl->window contentRectForFrameRect:frame];
            contentRect.size = NSMakeSize(width, height);
            frame = [m_impl->window frameRectForContentRect:contentRect];
            [m_impl->window setFrame:frame display:YES animate:YES];
        }
    }
}

void* MacOSPlatform::GetNativeWindowHandle() const {
    return (__bridge void*)m_impl->window;
}

void* MacOSPlatform::GetNativeDisplayHandle() const {
    return nullptr;  // Not applicable on macOS
}

// =============================================================================
// Events
// =============================================================================

void MacOSPlatform::PollEvents() {
    @autoreleasepool {
        NSEvent* event;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:nil
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES])) {
            [NSApp sendEvent:event];
        }
    }
}

void MacOSPlatform::WaitEvents() {
    @autoreleasepool {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantFuture]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }

        // Process any remaining events
        PollEvents();
    }
}

void MacOSPlatform::WaitEventsTimeout(double timeout) {
    @autoreleasepool {
        NSDate* date = [NSDate dateWithTimeIntervalSinceNow:timeout];
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:date
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }

        PollEvents();
    }
}

bool MacOSPlatform::ShouldClose() const {
    return m_impl->shouldClose;
}

void MacOSPlatform::RequestClose() {
    m_impl->shouldClose = true;
}

// =============================================================================
// Callbacks from Obj-C
// =============================================================================

void MacOSPlatform::OnWindowResize(int width, int height) {
    m_impl->windowSize = {width, height};

    @autoreleasepool {
        NSRect backingBounds = [m_impl->view convertRectToBacking:m_impl->view.bounds];
        m_impl->framebufferSize = {static_cast<int>(backingBounds.size.width),
                                   static_cast<int>(backingBounds.size.height)};
        m_impl->displayScale = m_impl->framebufferSize.x / static_cast<float>(m_impl->windowSize.x);
    }
}

void MacOSPlatform::OnWindowFocus(bool focused) {
    m_impl->focused = focused;
    m_state = focused ? PlatformState::Foreground : PlatformState::Background;

    if (focused && m_impl->lifecycleCallbacks.onResume) {
        m_impl->lifecycleCallbacks.onResume();
    } else if (!focused && m_impl->lifecycleCallbacks.onPause) {
        m_impl->lifecycleCallbacks.onPause();
    }

    if (m_stateCallback) {
        m_stateCallback(m_state);
    }
}

void MacOSPlatform::OnWindowClose() {
    m_impl->shouldClose = true;
    if (m_impl->lifecycleCallbacks.onTerminate) {
        m_impl->lifecycleCallbacks.onTerminate();
    }
}

void MacOSPlatform::OnFullscreenChange(bool fullscreen) {
    m_impl->fullscreen = fullscreen;
}

void MacOSPlatform::OnLocationUpdate(const GPSCoordinates& coords) {
    m_impl->lastLocation = coords;
    if (m_impl->locationCallback) {
        m_impl->locationCallback(coords);
    }
    if (m_locationCallback) {
        m_locationCallback(coords);
    }
}

void MacOSPlatform::OnLocationError(int code, const std::string& message) {
    if (m_impl->locationErrorCallback) {
        m_impl->locationErrorCallback(code, message);
    }
}

void MacOSPlatform::CreateMenuBar() {
    @autoreleasepool {
        NSMenu* menuBar = [[NSMenu alloc] init];
        [NSApp setMainMenu:menuBar];

        // Application menu
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appMenuItem];

        NSMenu* appMenu = [[NSMenu alloc] init];
        [appMenuItem setSubmenu:appMenu];

        // About
        [appMenu addItemWithTitle:@"About Nova3D"
                           action:@selector(orderFrontStandardAboutPanel:)
                    keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];

        // Quit
        NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit"
                                                          action:@selector(terminate:)
                                                   keyEquivalent:@"q"];
        [appMenu addItem:quitItem];

        // Window menu
        NSMenuItem* windowMenuItem = [[NSMenuItem alloc] init];
        [menuBar addItem:windowMenuItem];

        NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
        [windowMenuItem setSubmenu:windowMenu];

        [windowMenu addItemWithTitle:@"Minimize"
                              action:@selector(performMiniaturize:)
                       keyEquivalent:@"m"];
        [windowMenu addItemWithTitle:@"Zoom"
                              action:@selector(performZoom:)
                       keyEquivalent:@""];
        [windowMenu addItem:[NSMenuItem separatorItem]];
        [windowMenu addItemWithTitle:@"Enter Full Screen"
                              action:@selector(toggleFullScreen:)
                       keyEquivalent:@"f"];

        [NSApp setWindowsMenu:windowMenu];
    }
}

// =============================================================================
// File System
// =============================================================================

std::string MacOSPlatform::GetDataPath() const {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,
                                                             NSUserDomainMask, YES);
        if (paths.count > 0) {
            return [[paths[0] stringByAppendingPathComponent:@"Nova3D"] UTF8String] + std::string("/");
        }
    }
    return "./";
}

std::string MacOSPlatform::GetCachePath() const {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,
                                                             NSUserDomainMask, YES);
        if (paths.count > 0) {
            return [[paths[0] stringByAppendingPathComponent:@"Nova3D"] UTF8String] + std::string("/");
        }
    }
    return "./Cache/";
}

std::string MacOSPlatform::GetDocumentsPath() const {
    @autoreleasepool {
        NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                             NSUserDomainMask, YES);
        if (paths.count > 0) {
            return [paths[0] UTF8String] + std::string("/");
        }
    }
    return "./";
}

std::string MacOSPlatform::GetBundlePath() const {
    @autoreleasepool {
        NSBundle* bundle = [NSBundle mainBundle];
        if (bundle) {
            return [[bundle bundlePath] UTF8String] + std::string("/");
        }
    }
    return "./";
}

std::string MacOSPlatform::GetAssetsPath() const {
    @autoreleasepool {
        NSBundle* bundle = [NSBundle mainBundle];
        if (bundle) {
            NSString* resourcePath = [bundle resourcePath];
            if (resourcePath) {
                return [resourcePath UTF8String] + std::string("/");
            }
        }
    }
    return GetBundlePath() + "Contents/Resources/";
}

std::vector<uint8_t> MacOSPlatform::ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};

    return buffer;
}

std::string MacOSPlatform::ReadFileAsString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool MacOSPlatform::WriteFile(const std::string& path, const std::vector<uint8_t>& data) {
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool MacOSPlatform::WriteFile(const std::string& path, const std::string& content) {
    fs::path filePath(path);
    if (filePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << content;
    return file.good();
}

bool MacOSPlatform::FileExists(const std::string& path) const {
    return fs::exists(path);
}

bool MacOSPlatform::IsDirectory(const std::string& path) const {
    return fs::is_directory(path);
}

bool MacOSPlatform::CreateDirectory(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec) || fs::exists(path);
}

bool MacOSPlatform::DeleteFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec) || !fs::exists(path);
}

std::vector<std::string> MacOSPlatform::ListFiles(const std::string& path, bool recursive) {
    std::vector<std::string> files;
    if (!fs::exists(path) || !fs::is_directory(path)) return files;

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
    } catch (const fs::filesystem_error&) {}

    return files;
}

// =============================================================================
// Permissions
// =============================================================================

void MacOSPlatform::RequestPermission(Permission permission, PermissionCallback callback) {
    if (callback) {
        callback(permission, PermissionResult::Granted);
    }
}

bool MacOSPlatform::HasPermission(Permission) const {
    return true;
}

PermissionResult MacOSPlatform::GetPermissionStatus(Permission) const {
    return PermissionResult::Granted;
}

void MacOSPlatform::OpenPermissionSettings() {
    @autoreleasepool {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy"]];
    }
}

// =============================================================================
// Location
// =============================================================================

bool MacOSPlatform::IsLocationAvailable() const {
    return [CLLocationManager locationServicesEnabled];
}

bool MacOSPlatform::IsLocationEnabled() const {
    CLAuthorizationStatus status = [CLLocationManager authorizationStatus];
    return status == kCLAuthorizationStatusAuthorized ||
           status == kCLAuthorizationStatusAuthorizedAlways;
}

void MacOSPlatform::StartLocationUpdates(const LocationConfig& config,
                                          LocationCallback callback,
                                          LocationErrorCallback errorCallback) {
    m_impl->locationCallback = std::move(callback);
    m_impl->locationErrorCallback = std::move(errorCallback);

    @autoreleasepool {
        if (!m_impl->locationManager) {
            m_impl->locationManager = [[CLLocationManager alloc] init];
            m_impl->locationDelegate = [[NovaLocationDelegate alloc] init];
            m_impl->locationDelegate.platform = this;
            m_impl->locationManager.delegate = m_impl->locationDelegate;
        }

        m_impl->locationManager.desiredAccuracy = config.desiredAccuracy < 10 ?
            kCLLocationAccuracyBest : kCLLocationAccuracyNearestTenMeters;
        m_impl->locationManager.distanceFilter = config.distanceFilter;

        [m_impl->locationManager requestAlwaysAuthorization];
        [m_impl->locationManager startUpdatingLocation];
    }

    m_impl->locationUpdatesActive = true;
}

void MacOSPlatform::StartLocationUpdates(LocationCallback callback) {
    StartLocationUpdates(LocationConfig{}, std::move(callback), nullptr);
}

void MacOSPlatform::StopLocationUpdates() {
    if (m_impl->locationManager) {
        @autoreleasepool {
            [m_impl->locationManager stopUpdatingLocation];
        }
    }
    m_impl->locationUpdatesActive = false;
    m_impl->locationCallback = nullptr;
    m_impl->locationErrorCallback = nullptr;
}

void MacOSPlatform::RequestSingleLocation(LocationCallback callback) {
    @autoreleasepool {
        if (!m_impl->locationManager) {
            m_impl->locationManager = [[CLLocationManager alloc] init];
        }
        [m_impl->locationManager requestLocation];
    }
    if (callback) {
        callback(m_impl->lastLocation);
    }
}

GPSCoordinates MacOSPlatform::GetLastKnownLocation() const {
    return m_impl->lastLocation;
}

// =============================================================================
// System Information
// =============================================================================

uint64_t MacOSPlatform::GetAvailableMemory() const {
    mach_port_t host = mach_host_self();
    vm_size_t pageSize;
    host_page_size(host, &pageSize);

    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vmStats), &count) == KERN_SUCCESS) {
        return static_cast<uint64_t>(vmStats.free_count) * pageSize;
    }
    return 0;
}

uint64_t MacOSPlatform::GetTotalMemory() const {
    if (m_impl->cachedTotalMemory == 0) {
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t memory = 0;
        size_t length = sizeof(memory);
        sysctl(mib, 2, &memory, &length, nullptr, 0);
        m_impl->cachedTotalMemory = memory;
    }
    return m_impl->cachedTotalMemory;
}

int MacOSPlatform::GetCPUCores() const {
    return static_cast<int>([[NSProcessInfo processInfo] processorCount]);
}

std::string MacOSPlatform::GetCPUArchitecture() const {
#if defined(__arm64__) || defined(__aarch64__)
    return "arm64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unknown";
#endif
}

bool MacOSPlatform::HasGPUCompute() const {
    return true;  // Metal is available on all modern Macs
}

std::string MacOSPlatform::GetDeviceModel() const {
    if (m_impl->cachedDeviceModel.empty()) {
        char model[256];
        size_t len = sizeof(model);
        if (sysctlbyname("hw.model", model, &len, nullptr, 0) == 0) {
            m_impl->cachedDeviceModel = model;
        } else {
            m_impl->cachedDeviceModel = "Mac";
        }
    }
    return m_impl->cachedDeviceModel;
}

std::string MacOSPlatform::GetOSVersion() const {
    if (m_impl->cachedOSVersion.empty()) {
        @autoreleasepool {
            NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
            m_impl->cachedOSVersion = "macOS " +
                std::to_string(version.majorVersion) + "." +
                std::to_string(version.minorVersion) + "." +
                std::to_string(version.patchVersion);
        }
    }
    return m_impl->cachedOSVersion;
}

std::string MacOSPlatform::GetDeviceId() const {
    @autoreleasepool {
        io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault,
            IOServiceMatching("IOPlatformExpertDevice"));
        if (platformExpert) {
            CFTypeRef serialNumberAsCFString = IORegistryEntryCreateCFProperty(
                platformExpert, CFSTR(kIOPlatformSerialNumberKey),
                kCFAllocatorDefault, 0);
            IOObjectRelease(platformExpert);
            if (serialNumberAsCFString) {
                NSString* serial = (__bridge_transfer NSString*)serialNumberAsCFString;
                return [serial UTF8String];
            }
        }
    }
    return "";
}

std::string MacOSPlatform::GetLocale() const {
    if (m_impl->cachedLocale.empty()) {
        @autoreleasepool {
            NSLocale* locale = [NSLocale currentLocale];
            m_impl->cachedLocale = [[locale localeIdentifier] UTF8String];
        }
    }
    return m_impl->cachedLocale;
}

int MacOSPlatform::GetTimezoneOffset() const {
    @autoreleasepool {
        NSTimeZone* tz = [NSTimeZone localTimeZone];
        return static_cast<int>([tz secondsFromGMT]);
    }
}

bool MacOSPlatform::HasHardwareFeature(const std::string& feature) const {
    if (feature == "metal") return true;
#if defined(__arm64__)
    if (feature == "neon" || feature == "NEON") return true;
#elif defined(__x86_64__)
    if (feature == "sse" || feature == "SSE") return true;
    if (feature == "avx" || feature == "AVX") {
        // Check via sysctl
        int hasAVX = 0;
        size_t len = sizeof(hasAVX);
        sysctlbyname("hw.optional.avx1_0", &hasAVX, &len, nullptr, 0);
        return hasAVX != 0;
    }
#endif
    return false;
}

// =============================================================================
// Battery/Network
// =============================================================================

float MacOSPlatform::GetBatteryLevel() const {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info) {
        CFArrayRef sources = IOPSCopyPowerSourcesList(info);
        if (sources && CFArrayGetCount(sources) > 0) {
            CFDictionaryRef source = IOPSGetPowerSourceDescription(info,
                CFArrayGetValueAtIndex(sources, 0));
            if (source) {
                CFNumberRef capacityRef = (CFNumberRef)CFDictionaryGetValue(source,
                    CFSTR(kIOPSCurrentCapacityKey));
                CFNumberRef maxCapacityRef = (CFNumberRef)CFDictionaryGetValue(source,
                    CFSTR(kIOPSMaxCapacityKey));

                if (capacityRef && maxCapacityRef) {
                    int capacity, maxCapacity;
                    CFNumberGetValue(capacityRef, kCFNumberIntType, &capacity);
                    CFNumberGetValue(maxCapacityRef, kCFNumberIntType, &maxCapacity);
                    CFRelease(sources);
                    CFRelease(info);
                    return static_cast<float>(capacity) / static_cast<float>(maxCapacity);
                }
            }
            CFRelease(sources);
        }
        CFRelease(info);
    }
    return -1.0f;  // No battery
}

bool MacOSPlatform::IsBatteryCharging() const {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (info) {
        CFArrayRef sources = IOPSCopyPowerSourcesList(info);
        if (sources && CFArrayGetCount(sources) > 0) {
            CFDictionaryRef source = IOPSGetPowerSourceDescription(info,
                CFArrayGetValueAtIndex(sources, 0));
            if (source) {
                CFStringRef state = (CFStringRef)CFDictionaryGetValue(source,
                    CFSTR(kIOPSPowerSourceStateKey));
                bool charging = CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo;
                CFRelease(sources);
                CFRelease(info);
                return charging;
            }
            CFRelease(sources);
        }
        CFRelease(info);
    }
    return false;
}

bool MacOSPlatform::IsNetworkAvailable() const {
    SCNetworkReachabilityRef reachability = SCNetworkReachabilityCreateWithName(nullptr, "www.apple.com");
    if (reachability) {
        SCNetworkReachabilityFlags flags;
        bool success = SCNetworkReachabilityGetFlags(reachability, &flags);
        CFRelease(reachability);
        if (success) {
            return (flags & kSCNetworkReachabilityFlagsReachable) != 0;
        }
    }
    return false;
}

bool MacOSPlatform::IsWifiConnected() const {
    // Would check via CoreWLAN
    return IsNetworkAvailable();
}

bool MacOSPlatform::IsCellularConnected() const {
    return false;  // Macs don't have cellular
}

// =============================================================================
// Lifecycle & Haptics
// =============================================================================

void MacOSPlatform::SetLifecycleCallbacks(LifecycleCallbacks callbacks) {
    m_impl->lifecycleCallbacks = std::move(callbacks);
    m_lifecycleCallbacks = m_impl->lifecycleCallbacks;
}

void MacOSPlatform::TriggerHaptic(HapticType /*type*/) {
    // Force Touch trackpad haptics via NSHapticFeedbackManager
    @autoreleasepool {
        [[NSHapticFeedbackManager defaultPerformer]
            performFeedbackPattern:NSHapticFeedbackPatternGeneric
            performanceTime:NSHapticFeedbackPerformanceTimeDefault];
    }
}

bool MacOSPlatform::HasHaptics() const {
    return true;  // Force Touch trackpad
}

// =============================================================================
// Platform Factory
// =============================================================================

std::unique_ptr<Platform> Platform::Create() {
    return std::make_unique<MacOSPlatform>();
}

PlatformType Platform::GetCurrentPlatform() noexcept {
    return PlatformType::macOS;
}

const char* Platform::GetPlatformName() noexcept {
    return "macOS";
}

bool Platform::IsDesktop() noexcept {
    return true;
}

bool Platform::IsMobile() noexcept {
    return false;
}

} // namespace Nova

#endif // NOVA_PLATFORM_MACOS
