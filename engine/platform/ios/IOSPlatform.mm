/**
 * @file IOSPlatform.mm
 * @brief iOS platform implementation using Objective-C++
 *
 * This file bridges between the C++ Nova3D engine and iOS frameworks
 * including UIKit, CoreLocation, Metal, and OpenGL ES.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CoreLocation/CoreLocation.h>
#import <QuartzCore/CAMetalLayer.h>
#import <sys/utsname.h>

#if __has_include(<CoreHaptics/CoreHaptics.h>)
#import <CoreHaptics/CoreHaptics.h>
#define NOVA_HAS_HAPTICS 1
#else
#define NOVA_HAS_HAPTICS 0
#endif

#include "IOSPlatform.hpp"
#include "IOSGLContext.hpp"
#include "MetalRenderer.hpp"
#include "IOSTouchInput.hpp"

namespace Nova {

// =============================================================================
// IOSLocationDelegate - Objective-C delegate for CoreLocation
// =============================================================================

} // namespace Nova

@interface NOVALocationDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, assign) Nova::IOSPlatform* platform;
@end

@implementation NOVALocationDelegate

- (void)locationManager:(CLLocationManager*)manager
     didUpdateLocations:(NSArray<CLLocation*>*)locations {
    CLLocation* location = locations.lastObject;
    if (location && self.platform) {
        self.platform->OnLocationUpdate(
            location.coordinate.latitude,
            location.coordinate.longitude,
            location.altitude,
            location.horizontalAccuracy,
            [location.timestamp timeIntervalSince1970]
        );
    }
}

- (void)locationManager:(CLLocationManager*)manager
       didFailWithError:(NSError*)error {
    if (self.platform) {
        self.platform->OnLocationError(static_cast<int>(error.code));
    }
}

- (void)locationManager:(CLLocationManager*)manager
didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    // Handle authorization status change
    NSLog(@"Nova3D: Location authorization status changed to %d", (int)status);
}

@end

namespace Nova {

// =============================================================================
// IOSPlatform Implementation
// =============================================================================

IOSPlatform::IOSPlatform() {
    m_touchInput = std::make_unique<IOSTouchInput>();
}

IOSPlatform::~IOSPlatform() {
    Shutdown();
}

bool IOSPlatform::Initialize() {
    @autoreleasepool {
        NSLog(@"Nova3D: Initializing iOS platform...");

        m_state = PlatformState::Starting;

        // Get main screen properties
        UpdateDisplayMetrics();

        // Initialize rendering based on selected API
        if (m_renderingAPI == IOSRenderingAPI::Metal) {
            InitializeMetal();
        } else {
            InitializeOpenGLES();
        }

        // Initialize location services
        InitializeLocationServices();

#if NOVA_HAS_HAPTICS
        // Initialize haptic engine if available
        if (@available(iOS 13.0, *)) {
            NSError* error = nil;
            CHHapticEngine* engine = [[CHHapticEngine alloc] initAndReturnError:&error];
            if (!error) {
                [engine startAndReturnError:&error];
                if (!error) {
                    m_hapticEngine = (__bridge_retained void*)engine;
                }
            }
        }
#endif

        m_state = PlatformState::Running;
        NSLog(@"Nova3D: iOS platform initialized successfully");
        return true;
    }
}

void IOSPlatform::Shutdown() {
    @autoreleasepool {
        NSLog(@"Nova3D: Shutting down iOS platform...");

        m_state = PlatformState::Terminating;

        // Cleanup location services
        CleanupLocationServices();

        // Cleanup rendering
        m_metalRenderer.reset();
        m_glContextWrapper.reset();

        if (m_glContext) {
            m_glContext = nullptr;
        }

        if (m_metalLayer) {
            m_metalLayer = nullptr;
        }

#if NOVA_HAS_HAPTICS
        if (m_hapticEngine) {
            CHHapticEngine* engine = (__bridge_transfer CHHapticEngine*)m_hapticEngine;
            [engine stopWithCompletionHandler:nil];
            m_hapticEngine = nullptr;
        }
#endif

        m_state = PlatformState::Unknown;
        NSLog(@"Nova3D: iOS platform shutdown complete");
    }
}

void IOSPlatform::ProcessEvents() {
    // iOS event processing is handled by the run loop
    // This method can be used for custom event polling if needed
}

void IOSPlatform::CreateWindow(int width, int height) {
    @autoreleasepool {
        // On iOS, the "window" is managed by the UIApplication
        // We just update our internal size tracking
        m_screenSize = glm::ivec2(width, height);
        m_framebufferSize = glm::ivec2(
            static_cast<int>(width * m_displayScale),
            static_cast<int>(height * m_displayScale)
        );

        NSLog(@"Nova3D: Window created %dx%d (framebuffer: %dx%d)",
              width, height, m_framebufferSize.x, m_framebufferSize.y);
    }
}

void IOSPlatform::SwapBuffers() {
    if (m_renderingAPI == IOSRenderingAPI::OpenGLES && m_glContextWrapper) {
        m_glContextWrapper->Present();
    }
    // Metal handles presentation through the drawable
}

glm::ivec2 IOSPlatform::GetScreenSize() const {
    return m_screenSize;
}

glm::ivec2 IOSPlatform::GetFramebufferSize() const {
    return m_framebufferSize;
}

float IOSPlatform::GetDisplayScale() const {
    return m_displayScale;
}

bool IOSPlatform::SupportsFeature(const std::string& feature) const {
    @autoreleasepool {
        if (feature == "metal") {
            return MTLCreateSystemDefaultDevice() != nil;
        }
        if (feature == "gps" || feature == "location") {
            return [CLLocationManager locationServicesEnabled];
        }
        if (feature == "haptics") {
#if NOVA_HAS_HAPTICS
            if (@available(iOS 13.0, *)) {
                return CHHapticEngine.capabilitiesForHardware.supportsHaptics;
            }
#endif
            return false;
        }
        if (feature == "arkit") {
            // Check for ARKit support
            return NSClassFromString(@"ARSession") != nil;
        }
        return false;
    }
}

void IOSPlatform::SetNativeView(void* view) {
    m_nativeView = view;
    UpdateDisplayMetrics();
}

void IOSPlatform::UpdateDisplayMetrics() {
    @autoreleasepool {
        UIScreen* mainScreen = [UIScreen mainScreen];
        CGRect bounds = mainScreen.bounds;
        CGFloat scale = mainScreen.scale;

        m_displayScale = static_cast<float>(scale);
        m_screenSize = glm::ivec2(
            static_cast<int>(bounds.size.width),
            static_cast<int>(bounds.size.height)
        );
        m_framebufferSize = glm::ivec2(
            static_cast<int>(bounds.size.width * scale),
            static_cast<int>(bounds.size.height * scale)
        );

        // Get safe area insets
        if (@available(iOS 11.0, *)) {
            UIWindow* window = UIApplication.sharedApplication.windows.firstObject;
            if (window) {
                UIEdgeInsets insets = window.safeAreaInsets;
                m_safeAreaInsets = glm::vec4(
                    insets.top,
                    insets.left,
                    insets.bottom,
                    insets.right
                );
            }
        }

        NSLog(@"Nova3D: Display metrics - Size: %dx%d, Scale: %.1f",
              m_screenSize.x, m_screenSize.y, m_displayScale);
    }
}

// =============================================================================
// Rendering Initialization
// =============================================================================

void IOSPlatform::InitializeOpenGLES() {
    @autoreleasepool {
        NSLog(@"Nova3D: Initializing OpenGL ES...");
        m_glContextWrapper = std::make_unique<IOSGLContext>();
        if (m_glContextWrapper->CreateContext()) {
            m_glContext = m_glContextWrapper->GetNativeContext();
            m_glContextWrapper->CreateFramebuffer(m_framebufferSize.x, m_framebufferSize.y);
            NSLog(@"Nova3D: OpenGL ES initialized successfully");
        } else {
            NSLog(@"Nova3D: Failed to initialize OpenGL ES");
        }
    }
}

void IOSPlatform::InitializeMetal() {
    @autoreleasepool {
        NSLog(@"Nova3D: Initializing Metal...");

        // Check Metal support
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            NSLog(@"Nova3D: Metal not supported, falling back to OpenGL ES");
            m_renderingAPI = IOSRenderingAPI::OpenGLES;
            InitializeOpenGLES();
            return;
        }

        m_metalRenderer = std::make_unique<MetalRenderer>();
        if (m_metalRenderer->Initialize()) {
            // Create CAMetalLayer if we have a native view
            if (m_nativeView) {
                UIView* view = (__bridge UIView*)m_nativeView;
                CAMetalLayer* metalLayer = [CAMetalLayer layer];
                metalLayer.device = device;
                metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                metalLayer.framebufferOnly = YES;
                metalLayer.frame = view.layer.frame;
                metalLayer.contentsScale = m_displayScale;
                [view.layer addSublayer:metalLayer];
                m_metalLayer = (__bridge void*)metalLayer;
            }
            NSLog(@"Nova3D: Metal initialized successfully");
        } else {
            NSLog(@"Nova3D: Failed to initialize Metal, falling back to OpenGL ES");
            m_metalRenderer.reset();
            m_renderingAPI = IOSRenderingAPI::OpenGLES;
            InitializeOpenGLES();
        }
    }
}

// =============================================================================
// Location Services
// =============================================================================

void IOSPlatform::InitializeLocationServices() {
    @autoreleasepool {
        CLLocationManager* manager = [[CLLocationManager alloc] init];
        NOVALocationDelegate* delegate = [[NOVALocationDelegate alloc] init];
        delegate.platform = this;

        manager.delegate = delegate;
        manager.desiredAccuracy = kCLLocationAccuracyBest;
        manager.distanceFilter = 1.0; // Update every 1 meter

        m_locationManager = (__bridge_retained void*)manager;
        // Keep delegate alive (stored in manager's delegate property)

        m_locationAvailable = [CLLocationManager locationServicesEnabled];
        NSLog(@"Nova3D: Location services initialized (available: %d)",
              m_locationAvailable.load());
    }
}

void IOSPlatform::CleanupLocationServices() {
    @autoreleasepool {
        StopLocationUpdates();
        if (m_locationManager) {
            CLLocationManager* manager = (__bridge_transfer CLLocationManager*)m_locationManager;
            manager.delegate = nil;
            m_locationManager = nullptr;
        }
    }
}

void IOSPlatform::RequestLocationPermission() {
    @autoreleasepool {
        if (m_locationManager) {
            CLLocationManager* manager = (__bridge CLLocationManager*)m_locationManager;
            CLAuthorizationStatus status;
            if (@available(iOS 14.0, *)) {
                status = manager.authorizationStatus;
            } else {
                status = [CLLocationManager authorizationStatus];
            }

            if (status == kCLAuthorizationStatusNotDetermined) {
                [manager requestWhenInUseAuthorization];
            }
        }
    }
}

void IOSPlatform::StartLocationUpdates() {
    @autoreleasepool {
        if (m_locationManager && !m_locationUpdatesActive) {
            CLLocationManager* manager = (__bridge CLLocationManager*)m_locationManager;
            [manager startUpdatingLocation];
            m_locationUpdatesActive = true;
            NSLog(@"Nova3D: Started location updates");
        }
    }
}

void IOSPlatform::StopLocationUpdates() {
    @autoreleasepool {
        if (m_locationManager && m_locationUpdatesActive) {
            CLLocationManager* manager = (__bridge CLLocationManager*)m_locationManager;
            [manager stopUpdatingLocation];
            m_locationUpdatesActive = false;
            NSLog(@"Nova3D: Stopped location updates");
        }
    }
}

GPSCoordinates IOSPlatform::GetCurrentLocation() const {
    return m_currentLocation;
}

bool IOSPlatform::IsLocationAvailable() const {
    return m_locationAvailable;
}

void IOSPlatform::OnLocationUpdate(double latitude, double longitude, double altitude,
                                    double accuracy, double timestamp) {
    m_currentLocation.latitude = latitude;
    m_currentLocation.longitude = longitude;
    m_currentLocation.altitude = altitude;
    m_currentLocation.accuracy = accuracy;
    m_currentLocation.timestamp = timestamp;
    m_currentLocation.valid = true;

    if (m_locationCallback) {
        m_locationCallback(m_currentLocation);
    }
}

void IOSPlatform::OnLocationError(int errorCode) {
    NSLog(@"Nova3D: Location error: %d", errorCode);
    m_currentLocation.valid = false;
}

// =============================================================================
// Touch Input
// =============================================================================

void IOSPlatform::HandleTouchBegan(float x, float y, int touchId) {
    if (m_touchInput) {
        m_touchInput->HandleTouchBegan(x, y, touchId);
    }
}

void IOSPlatform::HandleTouchMoved(float x, float y, int touchId) {
    if (m_touchInput) {
        m_touchInput->HandleTouchMoved(x, y, touchId);
    }
}

void IOSPlatform::HandleTouchEnded(float x, float y, int touchId) {
    if (m_touchInput) {
        m_touchInput->HandleTouchEnded(x, y, touchId);
    }
}

void IOSPlatform::HandleTouchCancelled(float x, float y, int touchId) {
    if (m_touchInput) {
        m_touchInput->HandleTouchCancelled(x, y, touchId);
    }
}

// =============================================================================
// App Lifecycle
// =============================================================================

void IOSPlatform::OnEnterBackground() {
    @autoreleasepool {
        NSLog(@"Nova3D: Entering background");
        m_state = PlatformState::Background;

        // Stop OpenGL rendering when backgrounded
        if (m_glContextWrapper) {
            m_glContextWrapper->Pause();
        }

        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

void IOSPlatform::OnEnterForeground() {
    @autoreleasepool {
        NSLog(@"Nova3D: Entering foreground");
        m_state = PlatformState::Foreground;

        // Resume OpenGL rendering
        if (m_glContextWrapper) {
            m_glContextWrapper->Resume();
        }

        // Update display metrics (orientation may have changed)
        UpdateDisplayMetrics();

        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

void IOSPlatform::OnMemoryWarning() {
    @autoreleasepool {
        NSLog(@"Nova3D: Memory warning received");

        if (m_memoryCallback) {
            m_memoryCallback();
        }
    }
}

void IOSPlatform::OnWillTerminate() {
    @autoreleasepool {
        NSLog(@"Nova3D: Application will terminate");
        m_state = PlatformState::Terminating;

        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

void IOSPlatform::OnDidBecomeActive() {
    @autoreleasepool {
        NSLog(@"Nova3D: Application did become active");
        m_state = PlatformState::Running;

        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

void IOSPlatform::OnWillResignActive() {
    @autoreleasepool {
        NSLog(@"Nova3D: Application will resign active");

        if (m_stateCallback) {
            m_stateCallback(m_state);
        }
    }
}

// =============================================================================
// Display Properties
// =============================================================================

glm::vec4 IOSPlatform::GetSafeAreaInsets() const {
    return m_safeAreaInsets;
}

int IOSPlatform::GetDeviceOrientation() const {
    @autoreleasepool {
        return static_cast<int>([UIDevice currentDevice].orientation);
    }
}

bool IOSPlatform::SupportsHaptics() const {
#if NOVA_HAS_HAPTICS
    if (@available(iOS 13.0, *)) {
        return CHHapticEngine.capabilitiesForHardware.supportsHaptics;
    }
#endif
    return false;
}

void IOSPlatform::TriggerHaptic(int type) {
#if NOVA_HAS_HAPTICS
    @autoreleasepool {
        if (@available(iOS 10.0, *)) {
            UIImpactFeedbackStyle style;
            switch (type) {
                case 0: style = UIImpactFeedbackStyleLight; break;
                case 1: style = UIImpactFeedbackStyleMedium; break;
                case 2: style = UIImpactFeedbackStyleHeavy; break;
                default: style = UIImpactFeedbackStyleMedium; break;
            }
            UIImpactFeedbackGenerator* generator =
                [[UIImpactFeedbackGenerator alloc] initWithStyle:style];
            [generator prepare];
            [generator impactOccurred];
        }
    }
#endif
}

// =============================================================================
// Platform Information
// =============================================================================

std::string IOSPlatform::GetOSVersion() const {
    @autoreleasepool {
        NSString* version = [[UIDevice currentDevice] systemVersion];
        return std::string([version UTF8String]);
    }
}

std::string IOSPlatform::GetDeviceModel() const {
    @autoreleasepool {
        struct utsname systemInfo;
        uname(&systemInfo);
        return std::string(systemInfo.machine);
    }
}

// =============================================================================
// Platform Factory
// =============================================================================

std::unique_ptr<Platform> CreatePlatform() {
    return std::make_unique<IOSPlatform>();
}

} // namespace Nova
