/**
 * @file IOSAppDelegate.mm
 * @brief iOS Application Delegate and View Controller implementation
 *
 * This file contains the Objective-C implementation of the iOS app delegate,
 * view controller, and Metal/OpenGL view setup. It bridges the iOS lifecycle
 * to the Nova3D engine.
 */

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>

#include "IOSAppDelegate.hpp"
#include "IOSPlatform.hpp"
#include "../Platform.hpp"
#include "../../core/Engine.hpp"

// =============================================================================
// C++ Bridge Implementation
// =============================================================================

namespace Nova {
namespace IOSBridge {

static std::unique_ptr<IOSPlatform> s_platform;
static bool s_engineInitialized = false;
static bool s_gameLoopRunning = false;

bool InitializeEngine() {
    if (s_engineInitialized) return true;

    NSLog(@"Nova3D: Initializing engine from iOS bridge...");

    // Create iOS platform
    s_platform = std::make_unique<IOSPlatform>();
    if (!s_platform->Initialize()) {
        NSLog(@"Nova3D: Failed to initialize iOS platform");
        return false;
    }

    // Initialize engine
    Engine& engine = Engine::Instance();
    Engine::InitParams params;
    params.enableImGui = true;
    params.enableDebugDraw = true;

    if (!engine.Initialize(params)) {
        NSLog(@"Nova3D: Failed to initialize engine");
        return false;
    }

    s_engineInitialized = true;
    NSLog(@"Nova3D: Engine initialized successfully");
    return true;
}

void ShutdownEngine() {
    if (!s_engineInitialized) return;

    NSLog(@"Nova3D: Shutting down engine from iOS bridge...");

    s_gameLoopRunning = false;

    Engine::Instance().RequestShutdown();

    if (s_platform) {
        s_platform->Shutdown();
        s_platform.reset();
    }

    s_engineInitialized = false;
    NSLog(@"Nova3D: Engine shutdown complete");
}

IOSPlatform* GetPlatform() {
    return s_platform.get();
}

Engine* GetEngine() {
    return s_engineInitialized ? &Engine::Instance() : nullptr;
}

void OnEnterBackground() {
    if (s_platform) {
        s_platform->OnEnterBackground();
    }
}

void OnEnterForeground() {
    if (s_platform) {
        s_platform->OnEnterForeground();
    }
}

void OnMemoryWarning() {
    if (s_platform) {
        s_platform->OnMemoryWarning();
    }
}

void OnWillTerminate() {
    if (s_platform) {
        s_platform->OnWillTerminate();
    }
}

void StartGameLoop() {
    s_gameLoopRunning = true;
}

void StopGameLoop() {
    s_gameLoopRunning = false;
}

void ProcessFrame() {
    if (!s_engineInitialized || !s_gameLoopRunning) return;

    // Process platform events
    if (s_platform) {
        s_platform->ProcessEvents();
    }

    // The engine's Run() method handles the game loop on desktop,
    // but on iOS we need manual frame processing through display link
}

void SetNativeView(void* view) {
    if (s_platform) {
        s_platform->SetNativeView(view);
    }
}

void SetMetalLayer(void* layer) {
    // Metal layer is set through the platform's native view
}

void HandleTouchBegan(float x, float y, int touchId) {
    if (s_platform) {
        s_platform->HandleTouchBegan(x, y, touchId);
    }
}

void HandleTouchMoved(float x, float y, int touchId) {
    if (s_platform) {
        s_platform->HandleTouchMoved(x, y, touchId);
    }
}

void HandleTouchEnded(float x, float y, int touchId) {
    if (s_platform) {
        s_platform->HandleTouchEnded(x, y, touchId);
    }
}

void HandleTouchCancelled(float x, float y, int touchId) {
    if (s_platform) {
        s_platform->HandleTouchCancelled(x, y, touchId);
    }
}

} // namespace IOSBridge
} // namespace Nova

// =============================================================================
// Nova Game View (Metal/OpenGL ES)
// =============================================================================

@interface NovaGameView : UIView
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, strong) CAMetalLayer* metalLayer;
@property (nonatomic, strong) EAGLContext* glContext;
@property (nonatomic, assign) BOOL useMetal;
@property (nonatomic, assign) int nextTouchId;
@property (nonatomic, strong) NSMutableDictionary<NSValue*, NSNumber*>* touchIdMap;
@end

@implementation NovaGameView

+ (Class)layerClass {
    // Check if Metal is available
    if (MTLCreateSystemDefaultDevice()) {
        return [CAMetalLayer class];
    }
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder {
    self = [super initWithCoder:coder];
    if (self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit {
    self.multipleTouchEnabled = YES;
    self.userInteractionEnabled = YES;
    self.nextTouchId = 0;
    self.touchIdMap = [NSMutableDictionary dictionary];

    // Setup rendering layer
    if ([self.layer isKindOfClass:[CAMetalLayer class]]) {
        self.useMetal = YES;
        [self setupMetal];
    } else {
        self.useMetal = NO;
        [self setupOpenGLES];
    }

    // Set native view in platform
    Nova::IOSBridge::SetNativeView((__bridge void*)self);
}

- (void)setupMetal {
    self.metalLayer = (CAMetalLayer*)self.layer;
    self.metalLayer.device = MTLCreateSystemDefaultDevice();
    self.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    self.metalLayer.framebufferOnly = YES;
    self.metalLayer.contentsScale = [UIScreen mainScreen].scale;

    NSLog(@"Nova3D: Metal rendering setup complete");
}

- (void)setupOpenGLES {
    CAEAGLLayer* eaglLayer = (CAEAGLLayer*)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
    eaglLayer.contentsScale = [UIScreen mainScreen].scale;

    self.glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!self.glContext) {
        self.glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    }

    [EAGLContext setCurrentContext:self.glContext];

    NSLog(@"Nova3D: OpenGL ES rendering setup complete");
}

- (void)startDisplayLink {
    if (self.displayLink) return;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    Nova::IOSBridge::StartGameLoop();
    NSLog(@"Nova3D: Display link started");
}

- (void)stopDisplayLink {
    if (!self.displayLink) return;

    [self.displayLink invalidate];
    self.displayLink = nil;

    Nova::IOSBridge::StopGameLoop();
    NSLog(@"Nova3D: Display link stopped");
}

- (void)render:(CADisplayLink*)displayLink {
    @autoreleasepool {
        Nova::IOSBridge::ProcessFrame();

        // Present frame
        if (self.useMetal) {
            // Metal rendering is handled through the renderer
        } else {
            // OpenGL ES presentation
            if (self.glContext) {
                [EAGLContext setCurrentContext:self.glContext];
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                [self.glContext presentRenderbuffer:GL_RENDERBUFFER];
            }
        }
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];

    CGFloat scale = [UIScreen mainScreen].scale;
    CGSize size = self.bounds.size;

    if (self.useMetal) {
        self.metalLayer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
    }

    // Update platform with new size
    Nova::IOSPlatform* platform = Nova::IOSBridge::GetPlatform();
    if (platform) {
        platform->CreateWindow(size.width, size.height);
    }
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
// Nova View Controller
// =============================================================================

@interface NovaViewController : UIViewController
@property (nonatomic, strong) NovaGameView* gameView;
@end

@implementation NovaViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    // Create game view
    self.gameView = [[NovaGameView alloc] initWithFrame:self.view.bounds];
    self.gameView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.gameView];

    NSLog(@"Nova3D: View controller loaded");
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

@end

// =============================================================================
// Nova App Delegate
// =============================================================================

@interface NovaAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow* window;
@end

@implementation NovaAppDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions {
    NSLog(@"Nova3D: Application did finish launching");

    // Create window
    self.window = [[UIWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];

    // Initialize engine
    if (!Nova::IOSBridge::InitializeEngine()) {
        NSLog(@"Nova3D: Failed to initialize engine!");
        return NO;
    }

    // Create view controller
    NovaViewController* viewController = [[NovaViewController alloc] init];
    self.window.rootViewController = viewController;
    [self.window makeKeyAndVisible];

    return YES;
}

- (void)applicationWillResignActive:(UIApplication*)application {
    NSLog(@"Nova3D: Application will resign active");
    Nova::IOSBridge::GetPlatform()->OnWillResignActive();
}

- (void)applicationDidEnterBackground:(UIApplication*)application {
    NSLog(@"Nova3D: Application did enter background");
    Nova::IOSBridge::OnEnterBackground();
}

- (void)applicationWillEnterForeground:(UIApplication*)application {
    NSLog(@"Nova3D: Application will enter foreground");
    Nova::IOSBridge::OnEnterForeground();
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
    NSLog(@"Nova3D: Application did become active");
    Nova::IOSBridge::GetPlatform()->OnDidBecomeActive();
}

- (void)applicationWillTerminate:(UIApplication*)application {
    NSLog(@"Nova3D: Application will terminate");
    Nova::IOSBridge::OnWillTerminate();
    Nova::IOSBridge::ShutdownEngine();
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication*)application {
    NSLog(@"Nova3D: Memory warning received");
    Nova::IOSBridge::OnMemoryWarning();
}

@end

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([NovaAppDelegate class]));
    }
}
