# Nova3D iOS Platform Support

This directory contains the iOS platform implementation for the Nova3D engine,
providing support for Metal and OpenGL ES rendering, touch input with gesture
recognition, GPS/location services, and iOS lifecycle management.

## Features

### Rendering
- **Metal Renderer**: High-performance rendering using Apple's Metal API
- **OpenGL ES 3.0**: Fallback rendering for broader device compatibility
- **Retina Display**: Full support for high-DPI displays with proper scaling
- **CAMetalLayer/CAEAGLLayer**: Native layer integration for smooth rendering

### Input
- **Multi-touch**: Full multi-touch support with touch tracking
- **Gesture Recognition**: Built-in gesture detection:
  - Tap / Double Tap
  - Long Press
  - Pan (drag)
  - Pinch (zoom)
  - Rotation
  - Swipe
- **RTS Controls**: Touch-to-RTS command translation:
  - Single tap: Select unit
  - Drag: Selection box
  - Two-finger pan: Camera movement
  - Pinch: Camera zoom
  - Two-finger rotate: Camera rotation
  - Long press: Context menu

### Location Services
- **CoreLocation Integration**: GPS support for location-based games
- **Background Updates**: Optional background location tracking
- **Permission Handling**: Proper iOS permission request flow

### Lifecycle
- **App State Management**: Proper handling of iOS app states
- **Memory Warnings**: Respond to low memory conditions
- **Background/Foreground**: Pause/resume rendering appropriately

## Files

| File | Description |
|------|-------------|
| `IOSPlatform.hpp/mm` | Main iOS platform implementation |
| `IOSGLContext.hpp/mm` | OpenGL ES context and framebuffer management |
| `MetalRenderer.hpp/mm` | Metal rendering pipeline |
| `IOSTouchInput.hpp/mm` | Touch input and gesture recognition |
| `IOSAppDelegate.hpp/mm` | iOS app delegate and view controller |

## Building

### Using Xcode Project

Open `ios/Vehement2.xcodeproj` in Xcode and build directly.

### Using CMake

```bash
# Create build directory
mkdir build-ios && cd build-ios

# Configure for iOS device (arm64)
cmake -G Xcode \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
      -DPLATFORM=OS64 \
      -DDEPLOYMENT_TARGET=14.0 \
      ..

# Build
cmake --build . --config Release

# For iOS Simulator (Intel Mac)
cmake -G Xcode \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
      -DPLATFORM=SIMULATOR64 \
      ..

# For iOS Simulator (Apple Silicon Mac)
cmake -G Xcode \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
      -DPLATFORM=SIMULATORARM64 \
      ..
```

## Usage

### Initializing the Platform

```cpp
#include "engine/platform/ios/IOSPlatform.hpp"

// Create and initialize
auto platform = std::make_unique<Nova::IOSPlatform>();

// Choose rendering API (call before Initialize)
platform->SetRenderingAPI(Nova::IOSRenderingAPI::Metal);

if (!platform->Initialize()) {
    // Handle error
}

// Set callbacks
platform->SetLocationUpdateCallback([](const Nova::GPSCoordinates& coords) {
    // Handle location update
});

platform->SetMemoryWarningCallback([]() {
    // Free non-essential resources
});
```

### Handling Touch Input

```cpp
#include "engine/platform/ios/IOSTouchInput.hpp"

Nova::IOSTouchInput touchInput;

// Set callbacks
touchInput.SetTapCallback([](const glm::vec2& pos) {
    // Handle tap
});

touchInput.SetPinchCallback([](float scale, float delta, const glm::vec2& center) {
    // Handle zoom
});

// In update loop
touchInput.Update(deltaTime);

// Get RTS commands
const auto& rtsCommand = touchInput.GetRTSCommand();
if (rtsCommand.type == Nova::RTSTouchCommand::Type::CameraZoom) {
    camera.Zoom(rtsCommand.zoomDelta);
}
```

### Using Metal Renderer

```cpp
#include "engine/platform/ios/MetalRenderer.hpp"

Nova::MetalRenderer renderer;
renderer.Initialize();

// Create pipeline from shader source
renderer.CreatePipeline("basic",
    metalVertexShaderSource,
    metalFragmentShaderSource);

// In render loop
renderer.BeginFrame();
renderer.SetPipeline("basic");
renderer.SetVertexBuffer(vertexBuffer, 0, 0);
renderer.Draw(MTLPrimitiveTypeTriangle, 0, vertexCount);
renderer.EndFrame();
```

### Location Services

```cpp
// Request permission first
platform->RequestLocationPermission();

// Start updates
platform->StartLocationUpdates();

// Get current location
auto location = platform->GetCurrentLocation();
if (location.valid) {
    double lat = location.latitude;
    double lon = location.longitude;
}

// Stop when not needed
platform->StopLocationUpdates();
```

## Requirements

- iOS 14.0 or later
- Xcode 15.0 or later
- Metal-capable device (for Metal rendering)
- OpenGL ES 3.0 capable device (for OpenGL ES fallback)

## Info.plist Requirements

Add these keys to your Info.plist for location and camera features:

```xml
<key>NSLocationWhenInUseUsageDescription</key>
<string>Your description for location usage</string>

<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
<string>Your description for background location</string>

<key>NSCameraUsageDescription</key>
<string>Your description for camera usage</string>

<key>UIBackgroundModes</key>
<array>
    <string>location</string>
</array>
```

## License

Part of the Nova3D Engine. See main LICENSE file for details.
