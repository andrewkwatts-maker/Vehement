/**
 * @file ViewController.mm
 * @brief Main view controller for Vehement 2 iOS app
 *
 * This file provides an alternative standalone view controller implementation
 * that can be used with storyboards or programmatic UI setup.
 */

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>

#include "engine/platform/ios/IOSAppDelegate.hpp"
#include "engine/platform/ios/IOSPlatform.hpp"

// =============================================================================
// Vehement2ViewController Interface
// =============================================================================

@interface Vehement2ViewController : UIViewController <CLLocationManagerDelegate>

@property (nonatomic, strong) MTKView* metalView;
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, strong) CLLocationManager* locationManager;
@property (nonatomic, strong) CMMotionManager* motionManager;

// Touch tracking
@property (nonatomic, assign) int nextTouchId;
@property (nonatomic, strong) NSMutableDictionary<NSValue*, NSNumber*>* touchIdMap;

// UI Elements
@property (nonatomic, strong) UIView* hudOverlay;
@property (nonatomic, strong) UILabel* locationLabel;
@property (nonatomic, strong) UILabel* fpsLabel;
@property (nonatomic, strong) UIActivityIndicatorView* loadingIndicator;

// State
@property (nonatomic, assign) BOOL isInitialized;
@property (nonatomic, assign) int frameCount;
@property (nonatomic, assign) CFTimeInterval lastFPSUpdate;

@end

// =============================================================================
// Vehement2ViewController Implementation
// =============================================================================

@implementation Vehement2ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];
    self.isInitialized = NO;
    self.frameCount = 0;
    self.lastFPSUpdate = CACurrentMediaTime();

    // Setup touch tracking
    self.nextTouchId = 0;
    self.touchIdMap = [NSMutableDictionary dictionary];

    // Setup Metal view
    [self setupMetalView];

    // Setup HUD overlay
    [self setupHUD];

    // Setup location manager
    [self setupLocationManager];

    // Setup motion manager (for device orientation)
    [self setupMotionManager];

    // Initialize Nova3D
    [self initializeEngine];

    NSLog(@"Vehement2: ViewController loaded");
}

- (void)setupMetalView {
    // Create Metal view
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"Vehement2: Metal is not supported on this device");
        return;
    }

    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds device:device];
    self.metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    self.metalView.sampleCount = 1;
    self.metalView.preferredFramesPerSecond = 60;
    self.metalView.enableSetNeedsDisplay = NO;
    self.metalView.paused = YES;  // We'll use our own display link

    self.metalView.multipleTouchEnabled = YES;
    self.metalView.userInteractionEnabled = YES;

    [self.view addSubview:self.metalView];

    // Set native view in platform
    Nova::IOSBridge::SetNativeView((__bridge void*)self.metalView);

    NSLog(@"Vehement2: Metal view created with device: %@", device.name);
}

- (void)setupHUD {
    // Create HUD overlay
    self.hudOverlay = [[UIView alloc] initWithFrame:self.view.bounds];
    self.hudOverlay.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.hudOverlay.userInteractionEnabled = NO;
    self.hudOverlay.backgroundColor = [UIColor clearColor];
    [self.view addSubview:self.hudOverlay];

    // FPS label (top-left)
    self.fpsLabel = [[UILabel alloc] init];
    self.fpsLabel.font = [UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightMedium];
    self.fpsLabel.textColor = [UIColor greenColor];
    self.fpsLabel.text = @"FPS: --";
    self.fpsLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.hudOverlay addSubview:self.fpsLabel];

    // Location label (top-right)
    self.locationLabel = [[UILabel alloc] init];
    self.locationLabel.font = [UIFont monospacedSystemFontOfSize:10 weight:UIFontWeightRegular];
    self.locationLabel.textColor = [UIColor cyanColor];
    self.locationLabel.text = @"GPS: Waiting...";
    self.locationLabel.textAlignment = NSTextAlignmentRight;
    self.locationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.hudOverlay addSubview:self.locationLabel];

    // Loading indicator (center)
    self.loadingIndicator = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    self.loadingIndicator.color = [UIColor whiteColor];
    self.loadingIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    [self.hudOverlay addSubview:self.loadingIndicator];
    [self.loadingIndicator startAnimating];

    // Layout constraints
    UILayoutGuide* safeArea = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [self.fpsLabel.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:8],
        [self.fpsLabel.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor constant:8],

        [self.locationLabel.topAnchor constraintEqualToAnchor:safeArea.topAnchor constant:8],
        [self.locationLabel.trailingAnchor constraintEqualToAnchor:safeArea.trailingAnchor constant:-8],

        [self.loadingIndicator.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [self.loadingIndicator.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
    ]];
}

- (void)setupLocationManager {
    self.locationManager = [[CLLocationManager alloc] init];
    self.locationManager.delegate = self;
    self.locationManager.desiredAccuracy = kCLLocationAccuracyBest;
    self.locationManager.distanceFilter = 1.0;  // Update every 1 meter
    self.locationManager.allowsBackgroundLocationUpdates = YES;
    self.locationManager.pausesLocationUpdatesAutomatically = NO;

    // Request permission
    CLAuthorizationStatus status;
    if (@available(iOS 14.0, *)) {
        status = self.locationManager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    if (status == kCLAuthorizationStatusNotDetermined) {
        [self.locationManager requestWhenInUseAuthorization];
    } else if (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
               status == kCLAuthorizationStatusAuthorizedAlways) {
        [self.locationManager startUpdatingLocation];
    }
}

- (void)setupMotionManager {
    self.motionManager = [[CMMotionManager alloc] init];

    if (self.motionManager.deviceMotionAvailable) {
        self.motionManager.deviceMotionUpdateInterval = 1.0 / 60.0;
        [self.motionManager startDeviceMotionUpdates];
    }
}

- (void)initializeEngine {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        BOOL success = Nova::IOSBridge::InitializeEngine();

        dispatch_async(dispatch_get_main_queue(), ^{
            [self.loadingIndicator stopAnimating];
            self.loadingIndicator.hidden = YES;

            if (success) {
                self.isInitialized = YES;
                [self startDisplayLink];
                NSLog(@"Vehement2: Engine initialized successfully");
            } else {
                [self showErrorAlert:@"Failed to initialize game engine"];
            }
        });
    });
}

- (void)showErrorAlert:(NSString*)message {
    UIAlertController* alert = [UIAlertController alertControllerWithTitle:@"Error"
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

- (void)startDisplayLink {
    if (self.displayLink) return;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    Nova::IOSBridge::StartGameLoop();
    NSLog(@"Vehement2: Display link started");
}

- (void)stopDisplayLink {
    if (!self.displayLink) return;

    [self.displayLink invalidate];
    self.displayLink = nil;

    Nova::IOSBridge::StopGameLoop();
    NSLog(@"Vehement2: Display link stopped");
}

- (void)render:(CADisplayLink*)displayLink {
    @autoreleasepool {
        if (!self.isInitialized) return;

        // Process frame
        Nova::IOSBridge::ProcessFrame();

        // Update FPS counter
        self.frameCount++;
        CFTimeInterval now = CACurrentMediaTime();
        CFTimeInterval elapsed = now - self.lastFPSUpdate;

        if (elapsed >= 1.0) {
            double fps = self.frameCount / elapsed;
            self.fpsLabel.text = [NSString stringWithFormat:@"FPS: %.1f", fps];
            self.frameCount = 0;
            self.lastFPSUpdate = now;
        }
    }
}

#pragma mark - View Lifecycle

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];

    if (self.isInitialized) {
        [self startDisplayLink];
    }
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self stopDisplayLink];
}

- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];

    CGFloat scale = [UIScreen mainScreen].scale;
    CGSize size = self.metalView.bounds.size;

    self.metalView.drawableSize = CGSizeMake(size.width * scale, size.height * scale);

    Nova::IOSPlatform* platform = Nova::IOSBridge::GetPlatform();
    if (platform) {
        platform->CreateWindow(size.width, size.height);
    }
}

#pragma mark - Status Bar and Orientation

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return UIRectEdgeAll;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)shouldAutorotate {
    return YES;
}

#pragma mark - Touch Handling

- (int)touchIdForTouch:(UITouch*)touch {
    NSValue* key = [NSValue valueWithPointer:(__bridge const void*)touch];
    NSNumber* touchId = self.touchIdMap[key];
    if (!touchId) {
        touchId = @(self.nextTouchId++);
        self.touchIdMap[key] = touchId;
    }
    return touchId.intValue;
}

- (void)removeTouchId:(UITouch*)touch {
    NSValue* key = [NSValue valueWithPointer:(__bridge const void*)touch];
    [self.touchIdMap removeObjectForKey:key];
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self.metalView];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchBegan(location.x, location.y, touchId);
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self.metalView];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchMoved(location.x, location.y, touchId);
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self.metalView];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchEnded(location.x, location.y, touchId);
        [self removeTouchId:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self.metalView];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchCancelled(location.x, location.y, touchId);
        [self removeTouchId:touch];
    }
}

#pragma mark - CLLocationManagerDelegate

- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations {
    CLLocation* location = locations.lastObject;

    // Update UI
    self.locationLabel.text = [NSString stringWithFormat:@"GPS: %.6f, %.6f (%.0fm)",
                               location.coordinate.latitude,
                               location.coordinate.longitude,
                               location.horizontalAccuracy];

    // Update platform
    Nova::IOSPlatform* platform = Nova::IOSBridge::GetPlatform();
    if (platform) {
        platform->OnLocationUpdate(
            location.coordinate.latitude,
            location.coordinate.longitude,
            location.altitude,
            location.horizontalAccuracy,
            [location.timestamp timeIntervalSince1970]
        );
    }
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
    NSLog(@"Vehement2: Location error: %@", error.localizedDescription);
    self.locationLabel.text = @"GPS: Error";
}

- (void)locationManager:(CLLocationManager*)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    if (status == kCLAuthorizationStatusAuthorizedWhenInUse ||
        status == kCLAuthorizationStatusAuthorizedAlways) {
        [self.locationManager startUpdatingLocation];
    }
}

#pragma mark - Memory Management

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    Nova::IOSBridge::OnMemoryWarning();
    NSLog(@"Vehement2: Memory warning received");
}

- (void)dealloc {
    [self stopDisplayLink];
    [self.locationManager stopUpdatingLocation];
    [self.motionManager stopDeviceMotionUpdates];
}

@end
