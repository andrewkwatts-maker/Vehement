/**
 * @file MacOSLocation.mm
 * @brief macOS Core Location service implementation
 */

#import "MacOSLocation.hpp"
#import <CoreLocation/CoreLocation.h>
#import <AppKit/AppKit.h>

namespace Nova {
namespace Platform {

// Objective-C delegate class
@interface NovaMacLocationDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, assign) MacOSLocationService* service;
@end

@implementation NovaMacLocationDelegate

- (void)locationManager:(CLLocationManager *)manager
     didUpdateLocations:(NSArray<CLLocation *> *)locations {
    if (!_service || locations.count == 0) return;

    CLLocation* location = locations.lastObject;

    _service->OnLocationUpdate(
        location.coordinate.latitude,
        location.coordinate.longitude,
        location.altitude,
        location.horizontalAccuracy,
        location.verticalAccuracy,
        location.speed,
        location.course,
        static_cast<int64_t>([location.timestamp timeIntervalSince1970] * 1000)
    );
}

- (void)locationManager:(CLLocationManager *)manager
       didFailWithError:(NSError *)error {
    if (!_service) return;

    int errorCode = static_cast<int>(error.code);
    std::string message = [error.localizedDescription UTF8String] ?: "Unknown error";
    _service->OnLocationError(errorCode, message);
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager {
    if (!_service) return;

    CLAuthorizationStatus status;
    if (@available(macOS 11.0, *)) {
        status = manager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    _service->OnAuthorizationChange(static_cast<int>(status));
}

- (void)locationManager:(CLLocationManager *)manager
         didEnterRegion:(CLRegion *)region {
    if (!_service) return;
    _service->OnRegionEnter([region.identifier UTF8String]);
}

- (void)locationManager:(CLLocationManager *)manager
          didExitRegion:(CLRegion *)region {
    if (!_service) return;
    _service->OnRegionExit([region.identifier UTF8String]);
}

- (void)locationManager:(CLLocationManager *)manager
monitoringDidFailForRegion:(CLRegion *)region
              withError:(NSError *)error {
    if (!_service) return;
    std::string message = [error.localizedDescription UTF8String] ?: "Region monitoring failed";
    _service->OnLocationError(static_cast<int>(error.code), message);
}

@end

// C++ implementation

MacOSLocationService::MacOSLocationService() {
    @autoreleasepool {
        m_locationManager = [[CLLocationManager alloc] init];
        m_delegate = [[NovaMacLocationDelegate alloc] init];
        m_delegate.service = this;
        m_locationManager.delegate = m_delegate;

        // Default settings
        m_locationManager.desiredAccuracy = kCLLocationAccuracyBest;
        m_locationManager.distanceFilter = kCLDistanceFilterNone;

        NSLog(@"MacOSLocationService initialized");
    }
}

MacOSLocationService::~MacOSLocationService() {
    StopUpdates();
    StopSignificantLocationChanges();
    StopMonitoringAllRegions();

    @autoreleasepool {
        if (m_delegate) {
            m_delegate.service = nullptr;
            m_delegate = nil;
        }
        if (m_locationManager) {
            m_locationManager.delegate = nil;
            m_locationManager = nil;
        }
    }

    NSLog(@"MacOSLocationService destroyed");
}

bool MacOSLocationService::RequestPermission(bool alwaysAccess) {
    @autoreleasepool {
        if (!m_locationManager) return false;

        // macOS 10.15+ requires explicit authorization request
        if (@available(macOS 10.15, *)) {
            if (alwaysAccess) {
                [m_locationManager requestAlwaysAuthorization];
            } else {
                // macOS doesn't have WhenInUse, only Always
                [m_locationManager requestAlwaysAuthorization];
            }
        } else {
            // Older macOS - just start updates, which prompts for permission
            [m_locationManager startUpdatingLocation];
            [m_locationManager stopUpdatingLocation];
        }

        NSLog(@"Requested location permission");
        return true;
    }
}

bool MacOSLocationService::HasPermission() const {
    CLAuthorizationStatus status;
    if (@available(macOS 11.0, *)) {
        status = m_locationManager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    return status == kCLAuthorizationStatusAuthorizedAlways ||
           status == kCLAuthorizationStatusAuthorized;
}

LocationAuthorizationStatus MacOSLocationService::GetAuthorizationStatus() const {
    CLAuthorizationStatus status;
    if (@available(macOS 11.0, *)) {
        status = m_locationManager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    switch (status) {
        case kCLAuthorizationStatusNotDetermined:
            return LocationAuthorizationStatus::NotDetermined;
        case kCLAuthorizationStatusRestricted:
            return LocationAuthorizationStatus::Restricted;
        case kCLAuthorizationStatusDenied:
            return LocationAuthorizationStatus::Denied;
        case kCLAuthorizationStatusAuthorizedAlways:
        case kCLAuthorizationStatusAuthorized:
            return LocationAuthorizationStatus::AuthorizedAlways;
        default:
            return LocationAuthorizationStatus::NotDetermined;
    }
}

void MacOSLocationService::SetAuthorizationCallback(AuthorizationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authCallback = std::move(callback);
}

void MacOSLocationService::StartUpdates(LocationCallback callback) {
    if (m_updating) {
        NSLog(@"Already receiving location updates");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
    }

    @autoreleasepool {
        UpdateAccuracySettings();
        [m_locationManager startUpdatingLocation];
    }

    m_updating = true;
    NSLog(@"Location updates started");
}

void MacOSLocationService::StopUpdates() {
    if (!m_updating) return;

    @autoreleasepool {
        [m_locationManager stopUpdatingLocation];
    }

    m_updating = false;
    NSLog(@"Location updates stopped");
}

bool MacOSLocationService::IsUpdating() const {
    return m_updating;
}

void MacOSLocationService::RequestSingleUpdate(LocationCallback callback,
                                                 LocationErrorCallback errorCallback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
        m_errorCallback = std::move(errorCallback);
    }

    @autoreleasepool {
        UpdateAccuracySettings();
        // macOS doesn't have requestLocation, so we start/stop updates
        [m_locationManager startUpdatingLocation];
        // The delegate will receive the update and we'll stop after first location
    }

    NSLog(@"Single location update requested");
}

LocationData MacOSLocationService::GetLastKnown() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_lastLocation.IsValid()) {
        return m_lastLocation;
    }

    @autoreleasepool {
        CLLocation* location = m_locationManager.location;
        if (location) {
            LocationData data;
            data.coordinate.latitude = location.coordinate.latitude;
            data.coordinate.longitude = location.coordinate.longitude;
            data.altitude = location.altitude;
            data.horizontalAccuracy = location.horizontalAccuracy;
            data.verticalAccuracy = location.verticalAccuracy;
            data.speed = location.speed;
            data.course = location.course;
            data.timestamp = static_cast<int64_t>([location.timestamp timeIntervalSince1970] * 1000);
            data.provider = "CoreLocation";
            return data;
        }
    }

    return LocationData{};
}

bool MacOSLocationService::IsHighAccuracyAvailable() const {
    return [CLLocationManager locationServicesEnabled] && HasPermission();
}

void MacOSLocationService::SetDesiredAccuracy(LocationAccuracy accuracy) {
    m_desiredAccuracy = accuracy;
    if (m_updating) {
        UpdateAccuracySettings();
    }
}

LocationAccuracy MacOSLocationService::GetDesiredAccuracy() const {
    return m_desiredAccuracy;
}

void MacOSLocationService::SetDistanceFilter(double meters) {
    m_distanceFilter = meters;
    @autoreleasepool {
        m_locationManager.distanceFilter = meters > 0 ? meters : kCLDistanceFilterNone;
    }
}

void MacOSLocationService::SetUpdateInterval(int64_t milliseconds) {
    // macOS Core Location doesn't support explicit update intervals
}

bool MacOSLocationService::IsBackgroundLocationAvailable() const {
    return true; // Desktop apps can always run in background
}

void MacOSLocationService::SetBackgroundUpdatesEnabled(bool enable) {
    // No-op on macOS desktop
}

void MacOSLocationService::StartSignificantLocationChanges(LocationCallback callback) {
    if (m_significantChanges) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_significantCallback = std::move(callback);
    }

    @autoreleasepool {
        if ([CLLocationManager significantLocationChangeMonitoringAvailable]) {
            [m_locationManager startMonitoringSignificantLocationChanges];
            m_significantChanges = true;
            NSLog(@"Significant location changes monitoring started");
        } else {
            NSLog(@"Significant location changes not available");
        }
    }
}

void MacOSLocationService::StopSignificantLocationChanges() {
    if (!m_significantChanges) return;

    @autoreleasepool {
        [m_locationManager stopMonitoringSignificantLocationChanges];
    }

    m_significantChanges = false;
    NSLog(@"Significant location changes monitoring stopped");
}

bool MacOSLocationService::IsGeofencingSupported() const {
    return [CLLocationManager isMonitoringAvailableForClass:[CLCircularRegion class]];
}

bool MacOSLocationService::StartMonitoringRegion(const GeofenceRegion& region,
                                                   GeofenceCallback callback) {
    if (!IsGeofencingSupported()) {
        NSLog(@"Geofencing not supported");
        return false;
    }

    @autoreleasepool {
        CLLocationCoordinate2D center = CLLocationCoordinate2DMake(
            region.center.latitude, region.center.longitude);

        CLCircularRegion* clRegion = [[CLCircularRegion alloc]
            initWithCenter:center
            radius:region.radiusMeters
            identifier:[NSString stringWithUTF8String:region.identifier.c_str()]];

        clRegion.notifyOnEntry = region.notifyOnEntry;
        clRegion.notifyOnExit = region.notifyOnExit;

        [m_locationManager startMonitoringForRegion:clRegion];
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks[region.identifier] = std::move(callback);
        m_monitoredRegions.push_back(region);
    }

    NSLog(@"Started monitoring region: %s", region.identifier.c_str());
    return true;
}

void MacOSLocationService::StopMonitoringRegion(const std::string& identifier) {
    @autoreleasepool {
        NSString* regionId = [NSString stringWithUTF8String:identifier.c_str()];

        for (CLRegion* region in m_locationManager.monitoredRegions) {
            if ([region.identifier isEqualToString:regionId]) {
                [m_locationManager stopMonitoringForRegion:region];
                break;
            }
        }
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

    NSLog(@"Stopped monitoring region: %s", identifier.c_str());
}

void MacOSLocationService::StopMonitoringAllRegions() {
    @autoreleasepool {
        for (CLRegion* region in [m_locationManager.monitoredRegions copy]) {
            [m_locationManager stopMonitoringForRegion:region];
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks.clear();
        m_monitoredRegions.clear();
    }

    NSLog(@"Stopped monitoring all regions");
}

std::vector<GeofenceRegion> MacOSLocationService::GetMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredRegions;
}

bool MacOSLocationService::IsActivityRecognitionAvailable() const {
    return false; // Not available on macOS
}

void MacOSLocationService::StartActivityUpdates(ActivityCallback callback) {
    // Not implemented for macOS
}

void MacOSLocationService::StopActivityUpdates() {
    // Not implemented for macOS
}

std::string MacOSLocationService::GetServiceName() const {
    return "macOS Core Location";
}

bool MacOSLocationService::AreLocationServicesEnabled() const {
    return [CLLocationManager locationServicesEnabled];
}

void MacOSLocationService::OpenLocationSettings() {
    @autoreleasepool {
        // Open System Preferences > Security & Privacy > Privacy > Location Services
        NSURL* url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices"];
        [[NSWorkspace sharedWorkspace] openURL:url];
    }
}

bool MacOSLocationService::AreMockLocationsAllowed() const {
    return true;
}

void MacOSLocationService::SetRejectMockLocations(bool reject) {
    m_rejectMockLocations = reject;
}

void MacOSLocationService::SetErrorCallback(LocationErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = std::move(callback);
}

std::string MacOSLocationService::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// === Delegate callbacks ===

void MacOSLocationService::OnLocationUpdate(double latitude, double longitude, double altitude,
                                              double hAccuracy, double vAccuracy, double speed,
                                              double course, int64_t timestamp) {
    LocationData data;
    data.coordinate.latitude = latitude;
    data.coordinate.longitude = longitude;
    data.altitude = altitude;
    data.horizontalAccuracy = hAccuracy;
    data.verticalAccuracy = vAccuracy;
    data.speed = speed >= 0 ? speed : -1;
    data.course = course >= 0 ? course : -1;
    data.timestamp = timestamp;
    data.provider = "CoreLocation";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastLocation = data;
    }

    LocationCallback locationCallback;
    LocationCallback significantCallback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        locationCallback = m_locationCallback;
        significantCallback = m_significantCallback;
    }

    if (locationCallback) {
        locationCallback(data);
    }
    if (significantCallback) {
        significantCallback(data);
    }
}

void MacOSLocationService::OnAuthorizationChange(int status) {
    NSLog(@"Authorization status changed: %d", status);

    AuthorizationCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_authCallback;
    }

    if (callback) {
        LocationAuthorizationStatus authStatus;
        switch (status) {
            case kCLAuthorizationStatusNotDetermined:
                authStatus = LocationAuthorizationStatus::NotDetermined;
                break;
            case kCLAuthorizationStatusRestricted:
                authStatus = LocationAuthorizationStatus::Restricted;
                break;
            case kCLAuthorizationStatusDenied:
                authStatus = LocationAuthorizationStatus::Denied;
                break;
            case kCLAuthorizationStatusAuthorizedAlways:
            case kCLAuthorizationStatusAuthorized:
                authStatus = LocationAuthorizationStatus::AuthorizedAlways;
                break;
            default:
                authStatus = LocationAuthorizationStatus::NotDetermined;
                break;
        }
        callback(authStatus);
    }
}

void MacOSLocationService::OnLocationError(int errorCode, const std::string& message) {
    NSLog(@"Location error: %d - %s", errorCode, message.c_str());

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
            case kCLErrorDenied:
                error = LocationError::PermissionDenied;
                break;
            case kCLErrorLocationUnknown:
                error = LocationError::Unknown;
                break;
            case kCLErrorNetwork:
                error = LocationError::NetworkUnavailable;
                break;
            default:
                error = LocationError::Unknown;
                break;
        }
        callback(error, message);
    }
}

void MacOSLocationService::OnRegionEnter(const std::string& identifier) {
    NSLog(@"Entered region: %s", identifier.c_str());

    GeofenceCallback callback;
    GeofenceRegion region;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_geofenceCallbacks.find(identifier);
        if (it != m_geofenceCallbacks.end()) {
            callback = it->second;
        }
        for (const auto& r : m_monitoredRegions) {
            if (r.identifier == identifier) {
                region = r;
                break;
            }
        }
    }

    if (callback) {
        callback(region, GeofenceEvent::Enter);
    }
}

void MacOSLocationService::OnRegionExit(const std::string& identifier) {
    NSLog(@"Exited region: %s", identifier.c_str());

    GeofenceCallback callback;
    GeofenceRegion region;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_geofenceCallbacks.find(identifier);
        if (it != m_geofenceCallbacks.end()) {
            callback = it->second;
        }
        for (const auto& r : m_monitoredRegions) {
            if (r.identifier == identifier) {
                region = r;
                break;
            }
        }
    }

    if (callback) {
        callback(region, GeofenceEvent::Exit);
    }
}

void MacOSLocationService::UpdateAccuracySettings() {
    @autoreleasepool {
        CLLocationAccuracy accuracy;
        switch (m_desiredAccuracy) {
            case LocationAccuracy::BestForNavigation:
                accuracy = kCLLocationAccuracyBestForNavigation;
                break;
            case LocationAccuracy::Best:
                accuracy = kCLLocationAccuracyBest;
                break;
            case LocationAccuracy::NearestTenMeters:
                accuracy = kCLLocationAccuracyNearestTenMeters;
                break;
            case LocationAccuracy::HundredMeters:
                accuracy = kCLLocationAccuracyHundredMeters;
                break;
            case LocationAccuracy::Kilometer:
                accuracy = kCLLocationAccuracyKilometer;
                break;
            case LocationAccuracy::ThreeKilometers:
                accuracy = kCLLocationAccuracyThreeKilometers;
                break;
            default:
                accuracy = kCLLocationAccuracyBest;
                break;
        }
        m_locationManager.desiredAccuracy = accuracy;
    }
}

} // namespace Platform
} // namespace Nova
