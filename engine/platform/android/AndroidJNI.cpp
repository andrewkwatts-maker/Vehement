/**
 * @file AndroidJNI.cpp
 * @brief JNI bridge for Java/Kotlin interop with Nova3D Engine
 *
 * Provides the native interface between Kotlin/Java Android code and
 * the C++ Nova3D engine. Handles:
 * - Engine initialization and lifecycle
 * - Rendering callbacks
 * - Touch input forwarding
 * - GPS location updates
 */

#include "AndroidPlatform.hpp"
#include "AndroidGLES.hpp"
#include "VulkanRenderer.hpp"
#include "AndroidTouchInput.hpp"

#include <jni.h>
#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>

#include <memory>
#include <mutex>

namespace {

// Global state for JNI callbacks
struct JNIState {
    JavaVM* javaVM = nullptr;
    jobject nativeLibObject = nullptr;
    jmethodID requestLocationPermissionMethod = nullptr;
    jmethodID startLocationUpdatesMethod = nullptr;
    jmethodID stopLocationUpdatesMethod = nullptr;
    jmethodID showToastMethod = nullptr;
    jmethodID vibrateMethod = nullptr;

    ANativeWindow* nativeWindow = nullptr;
    bool engineInitialized = false;
    bool surfaceCreated = false;

    std::mutex mutex;
};

JNIState g_jniState;

// User-provided callbacks
std::function<bool()> g_onStartup;
std::function<void(float)> g_onUpdate;
std::function<void()> g_onRender;
std::function<void()> g_onShutdown;
std::function<void()> g_onPause;
std::function<void()> g_onResume;

// Helper to get JNIEnv for current thread
JNIEnv* GetJNIEnv() {
    JNIEnv* env = nullptr;
    if (g_jniState.javaVM) {
        int status = g_jniState.javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            g_jniState.javaVM->AttachCurrentThread(&env, nullptr);
        }
    }
    return env;
}

} // anonymous namespace

// -------------------------------------------------------------------------
// JNI Native Methods
// -------------------------------------------------------------------------

extern "C" {

/**
 * @brief Called when the native library is loaded
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jniState.javaVM = vm;

    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    Nova::NOVA_LOGI("JNI_OnLoad: Native library loaded");
    return JNI_VERSION_1_6;
}

/**
 * @brief Called when the native library is unloaded
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    Nova::NOVA_LOGI("JNI_OnUnload: Native library unloading");

    std::lock_guard<std::mutex> lock(g_jniState.mutex);

    if (g_jniState.nativeLibObject) {
        JNIEnv* env = GetJNIEnv();
        if (env) {
            env->DeleteGlobalRef(g_jniState.nativeLibObject);
        }
        g_jniState.nativeLibObject = nullptr;
    }

    g_jniState.javaVM = nullptr;
}

/**
 * @brief Initialize the native engine
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param assetManager Android AssetManager for loading assets from APK
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_init(JNIEnv* env, jobject obj, jobject assetManager) {
    Nova::NOVA_LOGI("NativeLib_init called");

    std::lock_guard<std::mutex> lock(g_jniState.mutex);

    // Store global reference to NativeLib object
    if (g_jniState.nativeLibObject) {
        env->DeleteGlobalRef(g_jniState.nativeLibObject);
    }
    g_jniState.nativeLibObject = env->NewGlobalRef(obj);

    // Cache method IDs for callbacks to Java
    jclass nativeLibClass = env->GetObjectClass(obj);
    if (nativeLibClass) {
        g_jniState.requestLocationPermissionMethod =
            env->GetMethodID(nativeLibClass, "requestLocationPermission", "()V");
        g_jniState.startLocationUpdatesMethod =
            env->GetMethodID(nativeLibClass, "startLocationUpdates", "()V");
        g_jniState.stopLocationUpdatesMethod =
            env->GetMethodID(nativeLibClass, "stopLocationUpdates", "()V");
        g_jniState.showToastMethod =
            env->GetMethodID(nativeLibClass, "showToast", "(Ljava/lang/String;)V");
        g_jniState.vibrateMethod =
            env->GetMethodID(nativeLibClass, "vibrate", "(J)V");
    }

    // Note: Full initialization happens in surfaceCreated when we have the window
    g_jniState.engineInitialized = false;
    Nova::NOVA_LOGI("NativeLib_init complete");
}

/**
 * @brief Called when the rendering surface is created
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param surface The Android Surface
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_surfaceCreated(JNIEnv* env, jobject obj, jobject surface) {
    Nova::NOVA_LOGI("NativeLib_surfaceCreated called");

    std::lock_guard<std::mutex> lock(g_jniState.mutex);

    // Get native window from surface
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        Nova::NOVA_LOGE("Failed to get native window from surface");
        return;
    }

    g_jniState.nativeWindow = window;

    // Initialize or update the platform with the new window
    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();

    if (!g_jniState.engineInitialized) {
        // First time initialization - would need ANativeActivity from somewhere
        // For now, just set the window
        platform.SetNativeWindow(window);
        g_jniState.engineInitialized = true;

        // Call user startup callback
        if (g_onStartup) {
            if (!g_onStartup()) {
                Nova::NOVA_LOGE("User startup callback failed");
            }
        }
    } else {
        // Surface recreated (e.g., after rotation)
        platform.SetNativeWindow(window);
    }

    g_jniState.surfaceCreated = true;
    Nova::NOVA_LOGI("NativeLib_surfaceCreated complete");
}

/**
 * @brief Called when the rendering surface is destroyed
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_surfaceDestroyed(JNIEnv* env, jobject obj) {
    Nova::NOVA_LOGI("NativeLib_surfaceDestroyed called");

    std::lock_guard<std::mutex> lock(g_jniState.mutex);

    g_jniState.surfaceCreated = false;

    if (g_jniState.nativeWindow) {
        Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
        platform.SetNativeWindow(nullptr);

        ANativeWindow_release(g_jniState.nativeWindow);
        g_jniState.nativeWindow = nullptr;
    }

    Nova::NOVA_LOGI("NativeLib_surfaceDestroyed complete");
}

/**
 * @brief Called when the surface dimensions change
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param width New width in pixels
 * @param height New height in pixels
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_resize(JNIEnv* env, jobject obj, jint width, jint height) {
    Nova::NOVA_LOGI("NativeLib_resize: %dx%d", width, height);

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.OnSurfaceChanged(width, height);
}

/**
 * @brief Called every frame to update and render
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_step(JNIEnv* env, jobject obj) {
    if (!g_jniState.surfaceCreated) {
        return;
    }

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();

    if (!platform.IsReady()) {
        return;
    }

    // Get touch input and update it
    Nova::AndroidTouchInput* touchInput = platform.GetTouchInput();
    if (touchInput) {
        touchInput->Update();
    }

    // Calculate delta time (simplified - should use proper timing)
    static auto lastTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;

    // Cap delta time to prevent large jumps
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // User update callback
    if (g_onUpdate) {
        g_onUpdate(deltaTime);
    }

    // Begin frame rendering
    if (platform.GetActiveBackend() == Nova::AndroidGraphicsBackend::Vulkan) {
        Nova::VulkanRenderer* vulkan = platform.GetVulkanRenderer();
        if (vulkan && vulkan->BeginFrame()) {
            // User render callback
            if (g_onRender) {
                g_onRender();
            }
            vulkan->EndFrame();
        }
    } else {
        Nova::AndroidGLES* gles = platform.GetGLES();
        if (gles && gles->IsValid()) {
            gles->MakeCurrent();

            // User render callback
            if (g_onRender) {
                g_onRender();
            }

            gles->SwapBuffers();
        }
    }
}

/**
 * @brief Handle touch input events
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param action Touch action (ACTION_DOWN, ACTION_UP, ACTION_MOVE, etc.)
 * @param x X coordinate
 * @param y Y coordinate
 * @param pointerId Pointer ID for multi-touch
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_touch(JNIEnv* env, jobject obj,
                                   jint action, jfloat x, jfloat y, jint pointerId) {
    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    Nova::AndroidTouchInput* touchInput = platform.GetTouchInput();

    if (touchInput) {
        touchInput->HandleTouchEvent(action, x, y, pointerId);
    }
}

/**
 * @brief Handle touch input with pressure
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param action Touch action
 * @param x X coordinate
 * @param y Y coordinate
 * @param pointerId Pointer ID
 * @param pressure Touch pressure (0-1)
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_touchWithPressure(JNIEnv* env, jobject obj,
                                               jint action, jfloat x, jfloat y,
                                               jint pointerId, jfloat pressure) {
    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    Nova::AndroidTouchInput* touchInput = platform.GetTouchInput();

    if (touchInput) {
        touchInput->HandleTouchEvent(action, x, y, pointerId, pressure);
    }
}

/**
 * @brief Called when the activity is paused
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_pause(JNIEnv* env, jobject obj) {
    Nova::NOVA_LOGI("NativeLib_pause called");

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.OnPause();

    if (g_onPause) {
        g_onPause();
    }
}

/**
 * @brief Called when the activity is resumed
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_resume(JNIEnv* env, jobject obj) {
    Nova::NOVA_LOGI("NativeLib_resume called");

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.OnResume();

    if (g_onResume) {
        g_onResume();
    }
}

/**
 * @brief Called when the activity is destroyed
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_destroy(JNIEnv* env, jobject obj) {
    Nova::NOVA_LOGI("NativeLib_destroy called");

    if (g_onShutdown) {
        g_onShutdown();
    }

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.OnDestroy();

    std::lock_guard<std::mutex> lock(g_jniState.mutex);
    g_jniState.engineInitialized = false;
}

/**
 * @brief Set GPS location from Java location service
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param latitude Latitude in degrees
 * @param longitude Longitude in degrees
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_setLocation(JNIEnv* env, jobject obj,
                                         jdouble latitude, jdouble longitude) {
    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.SetLocation(latitude, longitude);
}

/**
 * @brief Set GPS location with full data
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param latitude Latitude in degrees
 * @param longitude Longitude in degrees
 * @param altitude Altitude in meters
 * @param accuracy Accuracy in meters
 * @param timestamp Timestamp in milliseconds
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_setLocationFull(JNIEnv* env, jobject obj,
                                             jdouble latitude, jdouble longitude,
                                             jdouble altitude, jfloat accuracy,
                                             jlong timestamp) {
    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.SetLocation(latitude, longitude, altitude, accuracy, timestamp);
}

/**
 * @brief Called when location permission result is received
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 * @param granted true if permission was granted
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_onLocationPermissionResult(JNIEnv* env, jobject obj,
                                                        jboolean granted) {
    Nova::NOVA_LOGI("Location permission %s", granted ? "granted" : "denied");
    // The platform tracks permission state internally
}

/**
 * @brief Called when low memory warning is received
 *
 * @param env JNI environment
 * @param obj The NativeLib object
 */
JNIEXPORT void JNICALL
Java_com_vehement2_NativeLib_onLowMemory(JNIEnv* env, jobject obj) {
    Nova::NOVA_LOGW("Low memory warning received");

    Nova::AndroidPlatform& platform = Nova::AndroidPlatform::Instance();
    platform.OnLowMemory();
}

} // extern "C"

// -------------------------------------------------------------------------
// C++ API for calling Java methods
// -------------------------------------------------------------------------

namespace Nova {

/**
 * @brief Request location permission from Java side
 */
void RequestLocationPermissionFromJava() {
    JNIEnv* env = GetJNIEnv();
    if (env && g_jniState.nativeLibObject && g_jniState.requestLocationPermissionMethod) {
        env->CallVoidMethod(g_jniState.nativeLibObject, g_jniState.requestLocationPermissionMethod);
    }
}

/**
 * @brief Start location updates via Java
 */
void StartLocationUpdatesFromJava() {
    JNIEnv* env = GetJNIEnv();
    if (env && g_jniState.nativeLibObject && g_jniState.startLocationUpdatesMethod) {
        env->CallVoidMethod(g_jniState.nativeLibObject, g_jniState.startLocationUpdatesMethod);
    }
}

/**
 * @brief Stop location updates via Java
 */
void StopLocationUpdatesFromJava() {
    JNIEnv* env = GetJNIEnv();
    if (env && g_jniState.nativeLibObject && g_jniState.stopLocationUpdatesMethod) {
        env->CallVoidMethod(g_jniState.nativeLibObject, g_jniState.stopLocationUpdatesMethod);
    }
}

/**
 * @brief Show a toast message
 * @param message Message to display
 */
void ShowToast(const std::string& message) {
    JNIEnv* env = GetJNIEnv();
    if (env && g_jniState.nativeLibObject && g_jniState.showToastMethod) {
        jstring jMessage = env->NewStringUTF(message.c_str());
        env->CallVoidMethod(g_jniState.nativeLibObject, g_jniState.showToastMethod, jMessage);
        env->DeleteLocalRef(jMessage);
    }
}

/**
 * @brief Trigger device vibration
 * @param durationMs Duration in milliseconds
 */
void Vibrate(int64_t durationMs) {
    JNIEnv* env = GetJNIEnv();
    if (env && g_jniState.nativeLibObject && g_jniState.vibrateMethod) {
        env->CallVoidMethod(g_jniState.nativeLibObject, g_jniState.vibrateMethod,
                           static_cast<jlong>(durationMs));
    }
}

/**
 * @brief Set application callbacks for the engine
 */
void SetAndroidCallbacks(
    std::function<bool()> onStartup,
    std::function<void(float)> onUpdate,
    std::function<void()> onRender,
    std::function<void()> onShutdown,
    std::function<void()> onPause,
    std::function<void()> onResume)
{
    g_onStartup = std::move(onStartup);
    g_onUpdate = std::move(onUpdate);
    g_onRender = std::move(onRender);
    g_onShutdown = std::move(onShutdown);
    g_onPause = std::move(onPause);
    g_onResume = std::move(onResume);
}

} // namespace Nova
