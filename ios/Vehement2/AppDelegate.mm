/**
 * @file AppDelegate.mm
 * @brief iOS Application Delegate for Vehement 2
 *
 * This is the game-specific app delegate that integrates with the Nova3D engine
 * and sets up the Vehement 2 game.
 */

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

// Include Nova3D iOS headers
#include "engine/platform/ios/IOSAppDelegate.hpp"
#include "engine/platform/ios/IOSPlatform.hpp"
#include "engine/core/Engine.hpp"

// Include game headers
#include "Vehement2/Vehement2Game.hpp"

// Forward declaration of view controller
@class Vehement2ViewController;

// =============================================================================
// Vehement2 App Delegate
// =============================================================================

@interface Vehement2AppDelegate : UIResponder <UIApplicationDelegate, UNUserNotificationCenterDelegate>
@property (nonatomic, strong) UIWindow* window;
@property (nonatomic, strong) Vehement2ViewController* viewController;
@end

// =============================================================================
// Vehement2 Game View
// =============================================================================

@interface Vehement2GameView : UIView
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, assign) int nextTouchId;
@property (nonatomic, strong) NSMutableDictionary<NSValue*, NSNumber*>* touchIdMap;
@end

@implementation Vehement2GameView

+ (Class)layerClass {
    return [CAMetalLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setupView];
    }
    return self;
}

- (void)setupView {
    self.multipleTouchEnabled = YES;
    self.userInteractionEnabled = YES;
    self.nextTouchId = 0;
    self.touchIdMap = [NSMutableDictionary dictionary];
    self.backgroundColor = [UIColor blackColor];

    // Setup Metal layer
    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    metalLayer.device = MTLCreateSystemDefaultDevice();
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.framebufferOnly = YES;
    metalLayer.contentsScale = [UIScreen mainScreen].scale;

    // Set the native view in the platform
    Nova::IOSBridge::SetNativeView((__bridge void*)self);

    NSLog(@"Vehement2: Game view initialized");
}

- (void)startDisplayLink {
    if (self.displayLink) return;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    Nova::IOSBridge::StartGameLoop();
    NSLog(@"Vehement2: Display link started at 60 FPS");
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
        Nova::IOSBridge::ProcessFrame();
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];

    CGFloat scale = [UIScreen mainScreen].scale;
    CGSize size = self.bounds.size;

    CAMetalLayer* metalLayer = (CAMetalLayer*)self.layer;
    metalLayer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);

    Nova::IOSPlatform* platform = Nova::IOSBridge::GetPlatform();
    if (platform) {
        platform->CreateWindow(size.width, size.height);
    }

    NSLog(@"Vehement2: View resized to %.0fx%.0f (%.0fx%.0f pixels)",
          size.width, size.height, size.width * scale, size.height * scale);
}

// Touch handling
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
        CGPoint location = [touch locationInView:self];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchBegan(location.x, location.y, touchId);
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchMoved(location.x, location.y, touchId);
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchEnded(location.x, location.y, touchId);
        [self removeTouchId:touch];
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    for (UITouch* touch in touches) {
        CGPoint location = [touch locationInView:self];
        int touchId = [self touchIdForTouch:touch];
        Nova::IOSBridge::HandleTouchCancelled(location.x, location.y, touchId);
        [self removeTouchId:touch];
    }
}

- (void)dealloc {
    [self stopDisplayLink];
}

@end

// =============================================================================
// Vehement2 View Controller
// =============================================================================

@interface Vehement2ViewController : UIViewController
@property (nonatomic, strong) Vehement2GameView* gameView;
@end

@implementation Vehement2ViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    self.view.backgroundColor = [UIColor blackColor];

    // Create game view
    self.gameView = [[Vehement2GameView alloc] initWithFrame:self.view.bounds];
    self.gameView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.gameView];

    NSLog(@"Vehement2: View controller loaded");
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self.gameView startDisplayLink];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self.gameView stopDisplayLink];
}

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

@end

// =============================================================================
// Vehement2 App Delegate Implementation
// =============================================================================

@implementation Vehement2AppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    NSLog(@"Vehement2: Application launching...");

    // Request notification permissions
    [self requestNotificationPermissions];

    // Create window
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
    self.window.backgroundColor = [UIColor blackColor];

    // Initialize Nova3D engine
    if (!Nova::IOSBridge::InitializeEngine()) {
        NSLog(@"Vehement2: FATAL - Failed to initialize engine!");
        return NO;
    }

    // Create view controller
    self.viewController = [[Vehement2ViewController alloc] init];
    self.window.rootViewController = self.viewController;
    [self.window makeKeyAndVisible];

    // Initialize the Vehement2 game
    // Vehement2::Game::Instance().Initialize();

    NSLog(@"Vehement2: Application launched successfully");
    return YES;
}

- (void)requestNotificationPermissions {
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    center.delegate = self;
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL granted, NSError* _Nullable error) {
        if (granted) {
            NSLog(@"Vehement2: Notification permissions granted");
        } else {
            NSLog(@"Vehement2: Notification permissions denied");
        }
    }];
}

- (void)applicationWillResignActive:(UIApplication*)application {
    NSLog(@"Vehement2: Will resign active");
    Nova::IOSBridge::GetPlatform()->OnWillResignActive();
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
    NSLog(@"Vehement2: Did enter background");
    Nova::IOSBridge::OnEnterBackground();

    // Save game state when entering background
    // Vehement2::Game::Instance().SaveState();
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
    NSLog(@"Vehement2: Will enter foreground");
    Nova::IOSBridge::OnEnterForeground();
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
    NSLog(@"Vehement2: Did become active");
    Nova::IOSBridge::GetPlatform()->OnDidBecomeActive();

    // Sync with server when becoming active
    // Vehement2::Game::Instance().SyncWithServer();
}

- (void)applicationWillTerminate:(UIApplication*)application {
    NSLog(@"Vehement2: Will terminate");
    Nova::IOSBridge::OnWillTerminate();

    // Save game state before termination
    // Vehement2::Game::Instance().SaveState();

    Nova::IOSBridge::ShutdownEngine();
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
    NSLog(@"Vehement2: Memory warning!");
    Nova::IOSBridge::OnMemoryWarning();

    // Free cached resources
    // Vehement2::Game::Instance().OnMemoryWarning();
}

// UNUserNotificationCenterDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler {
    // Show notification even when app is in foreground
    completionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionSound);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
didReceiveNotificationResponse:(UNNotificationResponse*)response
         withCompletionHandler:(void (^)(void))completionHandler {
    // Handle notification tap
    NSString* identifier = response.notification.request.identifier;
    NSLog(@"Vehement2: Notification tapped: %@", identifier);

    // Handle game-specific notifications
    // Vehement2::Game::Instance().HandleNotification(identifier.UTF8String);

    completionHandler();
}

@end

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([Vehement2AppDelegate class]));
    }
}
