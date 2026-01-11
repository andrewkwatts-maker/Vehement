#pragma once

/**
 * @file IOSAppDelegate.hpp
 * @brief C++ interface declarations for iOS App Delegate
 *
 * This header provides the C++ interface to the iOS application lifecycle.
 * The actual Objective-C implementation is in IOSAppDelegate.mm.
 */

namespace Nova {

// Forward declarations
class IOSPlatform;
class Engine;

/**
 * @brief Bridge functions between Objective-C and C++ for app lifecycle
 */
namespace IOSBridge {

/**
 * @brief Initialize the Nova3D engine from the app delegate
 * @return true if initialization succeeded
 */
bool InitializeEngine();

/**
 * @brief Shutdown the Nova3D engine
 */
void ShutdownEngine();

/**
 * @brief Get the iOS platform instance
 */
IOSPlatform* GetPlatform();

/**
 * @brief Get the engine instance
 */
Engine* GetEngine();

/**
 * @brief Called when the app enters background
 */
void OnEnterBackground();

/**
 * @brief Called when the app enters foreground
 */
void OnEnterForeground();

/**
 * @brief Called when the app receives a memory warning
 */
void OnMemoryWarning();

/**
 * @brief Called when the app will terminate
 */
void OnWillTerminate();

/**
 * @brief Start the game loop
 */
void StartGameLoop();

/**
 * @brief Stop the game loop
 */
void StopGameLoop();

/**
 * @brief Process one frame of the game loop
 */
void ProcessFrame();

/**
 * @brief Set the native view for rendering
 * @param view UIView pointer
 */
void SetNativeView(void* view);

/**
 * @brief Set the Metal layer for rendering
 * @param layer CAMetalLayer pointer
 */
void SetMetalLayer(void* layer);

/**
 * @brief Handle touch events
 */
void HandleTouchBegan(float x, float y, int touchId);
void HandleTouchMoved(float x, float y, int touchId);
void HandleTouchEnded(float x, float y, int touchId);
void HandleTouchCancelled(float x, float y, int touchId);

} // namespace IOSBridge

} // namespace Nova
