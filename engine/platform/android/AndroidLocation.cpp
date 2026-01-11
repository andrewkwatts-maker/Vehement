/**
 * @file AndroidLocation.cpp
 * @brief Android GPS/Location service implementation
 */

#include "AndroidLocation.hpp"
#include <android/log.h>
#include <chrono>

#define LOG_TAG "NovaLocation"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Nova {
namespace Platform {

// Static members
JavaVM* AndroidLocationService::s_javaVM = nullptr;
jobject AndroidLocationService::s_activity = nullptr;

// JNI class path for our helper class
static const char* LOCATION_SERVICE_CLASS = "com/nova/engine/location/NovaLocationService";

// Android detected activity types
static constexpr int DETECTED_ACTIVITY_IN_VEHICLE = 0;
static constexpr int DETECTED_ACTIVITY_ON_BICYCLE = 1;
static constexpr int DETECTED_ACTIVITY_ON_FOOT = 2;
static constexpr int DETECTED_ACTIVITY_STILL = 3;
static constexpr int DETECTED_ACTIVITY_UNKNOWN = 4;
static constexpr int DETECTED_ACTIVITY_TILTING = 5;
static constexpr int DETECTED_ACTIVITY_WALKING = 7;
static constexpr int DETECTED_ACTIVITY_RUNNING = 8;

// Location request priorities
static constexpr int PRIORITY_HIGH_ACCURACY = 100;
static constexpr int PRIORITY_BALANCED_POWER_ACCURACY = 102;
static constexpr int PRIORITY_LOW_POWER = 104;
static constexpr int PRIORITY_PASSIVE = 105;

// Geofence transition types
static constexpr int GEOFENCE_TRANSITION_ENTER = 1;
static constexpr int GEOFENCE_TRANSITION_EXIT = 2;
static constexpr int GEOFENCE_TRANSITION_DWELL = 4;

AndroidLocationService::AndroidLocationService() {
    LOGI("AndroidLocationService created");
    InitializeJNI();
}

AndroidLocationService::~AndroidLocationService() {
    LOGI("AndroidLocationService destroyed");
    StopUpdates();
    StopSignificantLocationChanges();
    StopMonitoringAllRegions();
    StopActivityUpdates();
    CleanupJNI();
}

void AndroidLocationService::SetJavaVM(JavaVM* vm) {
    s_javaVM = vm;
}

void AndroidLocationService::SetActivity(jobject activity) {
    JNIEnv* env = nullptr;
    if (s_javaVM && s_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        if (s_activity) {
            env->DeleteGlobalRef(s_activity);
        }
        s_activity = activity ? env->NewGlobalRef(activity) : nullptr;
    }
}

JavaVM* AndroidLocationService::GetJavaVM() {
    return s_javaVM;
}

jobject AndroidLocationService::GetActivity() {
    return s_activity;
}

JNIEnv* AndroidLocationService::GetJNIEnv() const {
    JNIEnv* env = nullptr;
    if (s_javaVM) {
        int status = s_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (s_javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                LOGE("Failed to attach thread to JVM");
                return nullptr;
            }
        }
    }
    return env;
}

void AndroidLocationService::InitializeJNI() {
    JNIEnv* env = GetJNIEnv();
    if (!env) {
        LOGE("Failed to get JNI environment");
        return;
    }

    // Find our Java location service class
    jclass localClass = env->FindClass(LOCATION_SERVICE_CLASS);
    if (!localClass) {
        LOGW("Location service class not found, using fallback");
        return;
    }

    m_locationServiceClass = reinterpret_cast<jclass>(env->NewGlobalRef(localClass));
    env->DeleteLocalRef(localClass);

    // Get method IDs
    m_requestPermissionMethod = env->GetMethodID(m_locationServiceClass,
        "requestPermission", "(Z)V");
    m_hasPermissionMethod = env->GetMethodID(m_locationServiceClass,
        "hasPermission", "()Z");
    m_startUpdatesMethod = env->GetMethodID(m_locationServiceClass,
        "startUpdates", "(IJF)V");
    m_stopUpdatesMethod = env->GetMethodID(m_locationServiceClass,
        "stopUpdates", "()V");
    m_requestSingleUpdateMethod = env->GetMethodID(m_locationServiceClass,
        "requestSingleUpdate", "(I)V");
    m_getLastKnownMethod = env->GetMethodID(m_locationServiceClass,
        "getLastKnownLocation", "()Landroid/location/Location;");
    m_setAccuracyMethod = env->GetMethodID(m_locationServiceClass,
        "setAccuracy", "(I)V");
    m_setDistanceFilterMethod = env->GetMethodID(m_locationServiceClass,
        "setDistanceFilter", "(F)V");
    m_setIntervalMethod = env->GetMethodID(m_locationServiceClass,
        "setInterval", "(J)V");
    m_addGeofenceMethod = env->GetMethodID(m_locationServiceClass,
        "addGeofence", "(Ljava/lang/String;DDFIZZI)V");
    m_removeGeofenceMethod = env->GetMethodID(m_locationServiceClass,
        "removeGeofence", "(Ljava/lang/String;)V");
    m_startActivityRecognitionMethod = env->GetMethodID(m_locationServiceClass,
        "startActivityRecognition", "()V");
    m_stopActivityRecognitionMethod = env->GetMethodID(m_locationServiceClass,
        "stopActivityRecognition", "()V");
    m_isLocationEnabledMethod = env->GetMethodID(m_locationServiceClass,
        "isLocationEnabled", "()Z");
    m_openSettingsMethod = env->GetMethodID(m_locationServiceClass,
        "openLocationSettings", "()V");
    m_isMockLocationMethod = env->GetMethodID(m_locationServiceClass,
        "isMockLocation", "(Landroid/location/Location;)Z");

    // Create instance
    jmethodID constructor = env->GetMethodID(m_locationServiceClass, "<init>",
        "(Landroid/app/Activity;J)V");
    if (constructor && s_activity) {
        jobject localInstance = env->NewObject(m_locationServiceClass, constructor,
            s_activity, reinterpret_cast<jlong>(this));
        if (localInstance) {
            m_locationServiceInstance = env->NewGlobalRef(localInstance);
            env->DeleteLocalRef(localInstance);
            m_initialized = true;
            LOGI("Location service initialized successfully");
        }
    }
}

void AndroidLocationService::CleanupJNI() {
    JNIEnv* env = GetJNIEnv();
    if (env) {
        if (m_locationServiceInstance) {
            env->DeleteGlobalRef(m_locationServiceInstance);
            m_locationServiceInstance = nullptr;
        }
        if (m_locationServiceClass) {
            env->DeleteGlobalRef(m_locationServiceClass);
            m_locationServiceClass = nullptr;
        }
    }
    m_initialized = false;
}

bool AndroidLocationService::RequestPermission(bool alwaysAccess) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_requestPermissionMethod) {
        LOGE("Cannot request permission: JNI not initialized");
        return false;
    }

    env->CallVoidMethod(m_locationServiceInstance, m_requestPermissionMethod,
        static_cast<jboolean>(alwaysAccess));

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Exception while requesting permission");
        return false;
    }

    LOGI("Permission request initiated (alwaysAccess=%d)", alwaysAccess);
    return true;
}

bool AndroidLocationService::HasPermission() const {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_hasPermissionMethod) {
        return false;
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, m_hasPermissionMethod);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return result == JNI_TRUE;
}

LocationAuthorizationStatus AndroidLocationService::GetAuthorizationStatus() const {
    if (!m_initialized) {
        return LocationAuthorizationStatus::NotDetermined;
    }

    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance) {
        return LocationAuthorizationStatus::NotDetermined;
    }

    // Check permission status
    jmethodID getStatusMethod = env->GetMethodID(m_locationServiceClass,
        "getAuthorizationStatus", "()I");

    if (!getStatusMethod) {
        // Fallback to simple check
        return HasPermission() ? LocationAuthorizationStatus::AuthorizedWhenInUse
                               : LocationAuthorizationStatus::NotDetermined;
    }

    jint status = env->CallIntMethod(m_locationServiceInstance, getStatusMethod);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return LocationAuthorizationStatus::NotDetermined;
    }

    switch (status) {
        case 0: return LocationAuthorizationStatus::NotDetermined;
        case 1: return LocationAuthorizationStatus::Restricted;
        case 2: return LocationAuthorizationStatus::Denied;
        case 3: return LocationAuthorizationStatus::AuthorizedAlways;
        case 4: return LocationAuthorizationStatus::AuthorizedWhenInUse;
        default: return LocationAuthorizationStatus::NotDetermined;
    }
}

void AndroidLocationService::SetAuthorizationCallback(AuthorizationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authCallback = std::move(callback);
}

void AndroidLocationService::StartUpdates(LocationCallback callback) {
    if (m_updating) {
        LOGW("Already receiving location updates");
        return;
    }

    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_startUpdatesMethod) {
        LOGE("Cannot start updates: JNI not initialized");
        if (m_errorCallback) {
            m_errorCallback(LocationError::NotSupported, "Location service not initialized");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
    }

    int priority = GetPriorityFromAccuracy(m_desiredAccuracy);

    env->CallVoidMethod(m_locationServiceInstance, m_startUpdatesMethod,
        static_cast<jint>(priority),
        static_cast<jlong>(m_updateInterval),
        static_cast<jfloat>(m_distanceFilter));

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Exception while starting location updates");
        if (m_errorCallback) {
            m_errorCallback(LocationError::Unknown, "Failed to start location updates");
        }
        return;
    }

    m_updating = true;
    LOGI("Location updates started (priority=%d, interval=%lld, filter=%.1f)",
         priority, static_cast<long long>(m_updateInterval), m_distanceFilter);
}

void AndroidLocationService::StopUpdates() {
    if (!m_updating) {
        return;
    }

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_stopUpdatesMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_stopUpdatesMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    m_updating = false;
    LOGI("Location updates stopped");
}

bool AndroidLocationService::IsUpdating() const {
    return m_updating;
}

void AndroidLocationService::RequestSingleUpdate(LocationCallback callback,
                                                   LocationErrorCallback errorCallback) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_requestSingleUpdateMethod) {
        if (errorCallback) {
            errorCallback(LocationError::NotSupported, "Location service not initialized");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
        m_errorCallback = std::move(errorCallback);
    }

    int priority = GetPriorityFromAccuracy(m_desiredAccuracy);
    env->CallVoidMethod(m_locationServiceInstance, m_requestSingleUpdateMethod,
        static_cast<jint>(priority));

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (m_errorCallback) {
            m_errorCallback(LocationError::Unknown, "Failed to request single location update");
        }
    }

    LOGI("Single location update requested");
}

LocationData AndroidLocationService::GetLastKnown() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_lastLocation.IsValid()) {
        return m_lastLocation;
    }

    // Try to get from system
    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_getLastKnownMethod) {
        jobject location = env->CallObjectMethod(m_locationServiceInstance, m_getLastKnownMethod);
        if (location && !env->ExceptionCheck()) {
            LocationData data = const_cast<AndroidLocationService*>(this)->ConvertLocation(env, location);
            env->DeleteLocalRef(location);
            return data;
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    return LocationData{};
}

bool AndroidLocationService::IsHighAccuracyAvailable() const {
    return HasPermission() && AreLocationServicesEnabled();
}

void AndroidLocationService::SetDesiredAccuracy(LocationAccuracy accuracy) {
    m_desiredAccuracy = accuracy;

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_setAccuracyMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_setAccuracyMethod,
            static_cast<jint>(GetPriorityFromAccuracy(accuracy)));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

LocationAccuracy AndroidLocationService::GetDesiredAccuracy() const {
    return m_desiredAccuracy;
}

void AndroidLocationService::SetDistanceFilter(double meters) {
    m_distanceFilter = meters;

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_setDistanceFilterMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_setDistanceFilterMethod,
            static_cast<jfloat>(meters));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

void AndroidLocationService::SetUpdateInterval(int64_t milliseconds) {
    m_updateInterval = milliseconds;

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_setIntervalMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_setIntervalMethod,
            static_cast<jlong>(milliseconds));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

bool AndroidLocationService::IsBackgroundLocationAvailable() const {
    // Check Android version and permission
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance) {
        return false;
    }

    jmethodID method = env->GetMethodID(m_locationServiceClass,
        "isBackgroundLocationAvailable", "()Z");
    if (!method) {
        return false;
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return result == JNI_TRUE;
}

void AndroidLocationService::SetBackgroundUpdatesEnabled(bool enable) {
    m_backgroundEnabled = enable;

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance) {
        jmethodID method = env->GetMethodID(m_locationServiceClass,
            "setBackgroundUpdatesEnabled", "(Z)V");
        if (method) {
            env->CallVoidMethod(m_locationServiceInstance, method,
                static_cast<jboolean>(enable));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
    }
}

void AndroidLocationService::StartSignificantLocationChanges(LocationCallback callback) {
    if (m_significantChanges) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_significantCallback = std::move(callback);
    }

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance) {
        jmethodID method = env->GetMethodID(m_locationServiceClass,
            "startSignificantLocationChanges", "()V");
        if (method) {
            env->CallVoidMethod(m_locationServiceInstance, method);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                return;
            }
            m_significantChanges = true;
            LOGI("Significant location changes monitoring started");
        }
    }
}

void AndroidLocationService::StopSignificantLocationChanges() {
    if (!m_significantChanges) {
        return;
    }

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance) {
        jmethodID method = env->GetMethodID(m_locationServiceClass,
            "stopSignificantLocationChanges", "()V");
        if (method) {
            env->CallVoidMethod(m_locationServiceInstance, method);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }
    }

    m_significantChanges = false;
    LOGI("Significant location changes monitoring stopped");
}

bool AndroidLocationService::IsGeofencingSupported() const {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance) {
        return false;
    }

    jmethodID method = env->GetMethodID(m_locationServiceClass,
        "isGeofencingSupported", "()Z");
    if (!method) {
        return true; // Assume supported if we can't check
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }

    return result == JNI_TRUE;
}

bool AndroidLocationService::StartMonitoringRegion(const GeofenceRegion& region,
                                                     GeofenceCallback callback) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_addGeofenceMethod) {
        return false;
    }

    jstring jId = env->NewStringUTF(region.identifier.c_str());

    int transitionTypes = 0;
    if (region.notifyOnEntry) transitionTypes |= GEOFENCE_TRANSITION_ENTER;
    if (region.notifyOnExit) transitionTypes |= GEOFENCE_TRANSITION_EXIT;
    if (region.notifyOnDwell) transitionTypes |= GEOFENCE_TRANSITION_DWELL;

    env->CallVoidMethod(m_locationServiceInstance, m_addGeofenceMethod,
        jId,
        static_cast<jdouble>(region.center.latitude),
        static_cast<jdouble>(region.center.longitude),
        static_cast<jfloat>(region.radiusMeters),
        static_cast<jint>(transitionTypes),
        static_cast<jboolean>(region.notifyOnDwell),
        static_cast<jint>(region.dwellTimeMs));

    env->DeleteLocalRef(jId);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Failed to add geofence: %s", region.identifier.c_str());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks[region.identifier] = std::move(callback);
        m_monitoredRegions.push_back(region);
    }

    LOGI("Started monitoring geofence: %s", region.identifier.c_str());
    return true;
}

void AndroidLocationService::StopMonitoringRegion(const std::string& identifier) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_removeGeofenceMethod) {
        return;
    }

    jstring jId = env->NewStringUTF(identifier.c_str());
    env->CallVoidMethod(m_locationServiceInstance, m_removeGeofenceMethod, jId);
    env->DeleteLocalRef(jId);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks.erase(identifier);
        m_monitoredRegions.erase(
            std::remove_if(m_monitoredRegions.begin(), m_monitoredRegions.end(),
                [&identifier](const GeofenceRegion& r) {
                    return r.identifier == identifier;
                }),
            m_monitoredRegions.end());
    }

    LOGI("Stopped monitoring geofence: %s", identifier.c_str());
}

void AndroidLocationService::StopMonitoringAllRegions() {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& region : m_monitoredRegions) {
            ids.push_back(region.identifier);
        }
    }

    for (const auto& id : ids) {
        StopMonitoringRegion(id);
    }
}

std::vector<GeofenceRegion> AndroidLocationService::GetMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredRegions;
}

bool AndroidLocationService::IsActivityRecognitionAvailable() const {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance) {
        return false;
    }

    jmethodID method = env->GetMethodID(m_locationServiceClass,
        "isActivityRecognitionAvailable", "()Z");
    if (!method) {
        return false;
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return result == JNI_TRUE;
}

void AndroidLocationService::StartActivityUpdates(ActivityCallback callback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activityCallback = std::move(callback);
    }

    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_startActivityRecognitionMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_startActivityRecognitionMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            LOGE("Failed to start activity recognition");
        } else {
            LOGI("Activity recognition started");
        }
    }
}

void AndroidLocationService::StopActivityUpdates() {
    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_stopActivityRecognitionMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_stopActivityRecognitionMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        LOGI("Activity recognition stopped");
    }
}

std::string AndroidLocationService::GetServiceName() const {
    return "Android FusedLocationProvider";
}

bool AndroidLocationService::AreLocationServicesEnabled() const {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance || !m_isLocationEnabledMethod) {
        return false;
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, m_isLocationEnabledMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }

    return result == JNI_TRUE;
}

void AndroidLocationService::OpenLocationSettings() {
    JNIEnv* env = GetJNIEnv();
    if (env && m_locationServiceInstance && m_openSettingsMethod) {
        env->CallVoidMethod(m_locationServiceInstance, m_openSettingsMethod);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
}

bool AndroidLocationService::AreMockLocationsAllowed() const {
    JNIEnv* env = GetJNIEnv();
    if (!env || !m_locationServiceInstance) {
        return true; // Assume allowed if can't check
    }

    jmethodID method = env->GetMethodID(m_locationServiceClass,
        "areMockLocationsAllowed", "()Z");
    if (!method) {
        return true;
    }

    jboolean result = env->CallBooleanMethod(m_locationServiceInstance, method);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }

    return result == JNI_TRUE;
}

void AndroidLocationService::SetRejectMockLocations(bool reject) {
    m_rejectMockLocations = reject;
}

void AndroidLocationService::SetErrorCallback(LocationErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = std::move(callback);
}

std::string AndroidLocationService::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// === JNI Callbacks ===

void AndroidLocationService::OnLocationUpdate(JNIEnv* env, jobject location) {
    if (!location) {
        return;
    }

    LocationData data = ConvertLocation(env, location);

    // Check for mock location if rejection is enabled
    if (m_rejectMockLocations && data.isMockLocation) {
        LOGW("Rejecting mock location");
        if (m_errorCallback) {
            m_errorCallback(LocationError::MockLocationDetected, "Mock location detected and rejected");
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastLocation = data;
    }

    // Notify callbacks
    LocationCallback callback;
    LocationCallback significantCallback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_locationCallback;
        significantCallback = m_significantCallback;
    }

    if (callback) {
        callback(data);
    }
    if (significantCallback) {
        significantCallback(data);
    }
}

void AndroidLocationService::OnPermissionResult(bool granted, bool fineLocation) {
    LOGI("Permission result: granted=%d, fineLocation=%d", granted, fineLocation);

    AuthorizationCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_authCallback;
    }

    if (callback) {
        LocationAuthorizationStatus status;
        if (!granted) {
            status = LocationAuthorizationStatus::Denied;
        } else if (fineLocation) {
            status = m_backgroundEnabled ? LocationAuthorizationStatus::AuthorizedAlways
                                         : LocationAuthorizationStatus::AuthorizedWhenInUse;
        } else {
            status = LocationAuthorizationStatus::AuthorizedWhenInUse;
        }
        callback(status);
    }
}

void AndroidLocationService::OnGeofenceEvent(const std::string& regionId, int transitionType) {
    GeofenceCallback callback;
    GeofenceRegion region;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_geofenceCallbacks.find(regionId);
        if (it != m_geofenceCallbacks.end()) {
            callback = it->second;
        }
        for (const auto& r : m_monitoredRegions) {
            if (r.identifier == regionId) {
                region = r;
                break;
            }
        }
    }

    if (callback) {
        GeofenceEvent event;
        switch (transitionType) {
            case GEOFENCE_TRANSITION_ENTER:
                event = GeofenceEvent::Enter;
                break;
            case GEOFENCE_TRANSITION_EXIT:
                event = GeofenceEvent::Exit;
                break;
            case GEOFENCE_TRANSITION_DWELL:
                event = GeofenceEvent::Dwell;
                break;
            default:
                return;
        }
        callback(region, event);
    }
}

void AndroidLocationService::OnActivityUpdate(int activityType, int confidence) {
    ActivityCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_activityCallback;
    }

    if (callback) {
        ActivityData data;
        data.type = ConvertActivityType(activityType);
        data.confidence = static_cast<float>(confidence) / 100.0f;
        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        callback(data);
    }
}

void AndroidLocationService::OnLocationError(int errorCode, const std::string& message) {
    LOGE("Location error: %d - %s", errorCode, message.c_str());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = message;
    }

    LocationErrorCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_errorCallback;
    }

    if (callback) {
        LocationError error;
        switch (errorCode) {
            case 1: error = LocationError::PermissionDenied; break;
            case 2: error = LocationError::LocationDisabled; break;
            case 3: error = LocationError::NetworkUnavailable; break;
            case 4: error = LocationError::Timeout; break;
            default: error = LocationError::Unknown; break;
        }
        callback(error, message);
    }
}

// === Helper Methods ===

LocationData AndroidLocationService::ConvertLocation(JNIEnv* env, jobject location) {
    LocationData data;

    jclass locationClass = env->GetObjectClass(location);

    // Get latitude
    jmethodID getLatitude = env->GetMethodID(locationClass, "getLatitude", "()D");
    data.coordinate.latitude = env->CallDoubleMethod(location, getLatitude);

    // Get longitude
    jmethodID getLongitude = env->GetMethodID(locationClass, "getLongitude", "()D");
    data.coordinate.longitude = env->CallDoubleMethod(location, getLongitude);

    // Get altitude
    jmethodID hasAltitude = env->GetMethodID(locationClass, "hasAltitude", "()Z");
    if (env->CallBooleanMethod(location, hasAltitude)) {
        jmethodID getAltitude = env->GetMethodID(locationClass, "getAltitude", "()D");
        data.altitude = env->CallDoubleMethod(location, getAltitude);
    }

    // Get accuracy
    jmethodID hasAccuracy = env->GetMethodID(locationClass, "hasAccuracy", "()Z");
    if (env->CallBooleanMethod(location, hasAccuracy)) {
        jmethodID getAccuracy = env->GetMethodID(locationClass, "getAccuracy", "()F");
        data.horizontalAccuracy = env->CallFloatMethod(location, getAccuracy);
    }

    // Get vertical accuracy (API 26+)
    jmethodID hasVerticalAccuracy = env->GetMethodID(locationClass, "hasVerticalAccuracy", "()Z");
    if (hasVerticalAccuracy && env->CallBooleanMethod(location, hasVerticalAccuracy)) {
        jmethodID getVerticalAccuracy = env->GetMethodID(locationClass,
            "getVerticalAccuracyMeters", "()F");
        if (getVerticalAccuracy) {
            data.verticalAccuracy = env->CallFloatMethod(location, getVerticalAccuracy);
        }
    }

    // Get speed
    jmethodID hasSpeed = env->GetMethodID(locationClass, "hasSpeed", "()Z");
    if (env->CallBooleanMethod(location, hasSpeed)) {
        jmethodID getSpeed = env->GetMethodID(locationClass, "getSpeed", "()F");
        data.speed = env->CallFloatMethod(location, getSpeed);
    }

    // Get bearing
    jmethodID hasBearing = env->GetMethodID(locationClass, "hasBearing", "()Z");
    if (env->CallBooleanMethod(location, hasBearing)) {
        jmethodID getBearing = env->GetMethodID(locationClass, "getBearing", "()F");
        data.course = env->CallFloatMethod(location, getBearing);
    }

    // Get timestamp
    jmethodID getTime = env->GetMethodID(locationClass, "getTime", "()J");
    data.timestamp = env->CallLongMethod(location, getTime);

    // Get provider
    jmethodID getProvider = env->GetMethodID(locationClass, "getProvider", "()Ljava/lang/String;");
    jstring provider = static_cast<jstring>(env->CallObjectMethod(location, getProvider));
    if (provider) {
        const char* providerStr = env->GetStringUTFChars(provider, nullptr);
        if (providerStr) {
            data.provider = providerStr;
            env->ReleaseStringUTFChars(provider, providerStr);
        }
        env->DeleteLocalRef(provider);
    }

    // Check mock location
    if (m_isMockLocationMethod) {
        data.isMockLocation = env->CallBooleanMethod(m_locationServiceInstance,
            m_isMockLocationMethod, location) == JNI_TRUE;
    }

    env->DeleteLocalRef(locationClass);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return data;
}

int AndroidLocationService::GetPriorityFromAccuracy(LocationAccuracy accuracy) const {
    switch (accuracy) {
        case LocationAccuracy::BestForNavigation:
        case LocationAccuracy::Best:
        case LocationAccuracy::NearestTenMeters:
            return PRIORITY_HIGH_ACCURACY;
        case LocationAccuracy::HundredMeters:
            return PRIORITY_BALANCED_POWER_ACCURACY;
        case LocationAccuracy::Kilometer:
        case LocationAccuracy::ThreeKilometers:
            return PRIORITY_LOW_POWER;
        case LocationAccuracy::Passive:
            return PRIORITY_PASSIVE;
        default:
            return PRIORITY_BALANCED_POWER_ACCURACY;
    }
}

ActivityType AndroidLocationService::ConvertActivityType(int androidType) const {
    switch (androidType) {
        case DETECTED_ACTIVITY_IN_VEHICLE:
            return ActivityType::Automotive;
        case DETECTED_ACTIVITY_ON_BICYCLE:
            return ActivityType::Cycling;
        case DETECTED_ACTIVITY_ON_FOOT:
        case DETECTED_ACTIVITY_WALKING:
            return ActivityType::Walking;
        case DETECTED_ACTIVITY_RUNNING:
            return ActivityType::Running;
        case DETECTED_ACTIVITY_STILL:
            return ActivityType::Stationary;
        default:
            return ActivityType::Unknown;
    }
}

// === JNI Native Methods ===

extern "C" {

JNIEXPORT void JNICALL
Java_com_nova_engine_location_NovaLocationService_nativeOnLocationUpdate(
    JNIEnv* env, jobject thiz, jlong nativePtr, jobject location) {

    auto* service = reinterpret_cast<AndroidLocationService*>(nativePtr);
    if (service) {
        service->OnLocationUpdate(env, location);
    }
}

JNIEXPORT void JNICALL
Java_com_nova_engine_location_NovaLocationService_nativeOnPermissionResult(
    JNIEnv* env, jobject thiz, jlong nativePtr, jboolean granted, jboolean fineLocation) {

    auto* service = reinterpret_cast<AndroidLocationService*>(nativePtr);
    if (service) {
        service->OnPermissionResult(granted == JNI_TRUE, fineLocation == JNI_TRUE);
    }
}

JNIEXPORT void JNICALL
Java_com_nova_engine_location_NovaLocationService_nativeOnGeofenceEvent(
    JNIEnv* env, jobject thiz, jlong nativePtr, jstring regionId, jint transitionType) {

    auto* service = reinterpret_cast<AndroidLocationService*>(nativePtr);
    if (service && regionId) {
        const char* id = env->GetStringUTFChars(regionId, nullptr);
        if (id) {
            service->OnGeofenceEvent(id, transitionType);
            env->ReleaseStringUTFChars(regionId, id);
        }
    }
}

JNIEXPORT void JNICALL
Java_com_nova_engine_location_NovaLocationService_nativeOnActivityUpdate(
    JNIEnv* env, jobject thiz, jlong nativePtr, jint activityType, jint confidence) {

    auto* service = reinterpret_cast<AndroidLocationService*>(nativePtr);
    if (service) {
        service->OnActivityUpdate(activityType, confidence);
    }
}

JNIEXPORT void JNICALL
Java_com_nova_engine_location_NovaLocationService_nativeOnError(
    JNIEnv* env, jobject thiz, jlong nativePtr, jint errorCode, jstring message) {

    auto* service = reinterpret_cast<AndroidLocationService*>(nativePtr);
    if (service && message) {
        const char* msg = env->GetStringUTFChars(message, nullptr);
        if (msg) {
            service->OnLocationError(errorCode, msg);
            env->ReleaseStringUTFChars(message, msg);
        }
    }
}

} // extern "C"

} // namespace Platform
} // namespace Nova
