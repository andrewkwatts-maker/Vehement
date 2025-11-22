/**
 * @file IOSLocation.mm
 * @brief iOS Core Location service implementation
 */

#import "IOSLocation.hpp"
#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>
#import <UIKit/UIKit.h>

namespace Nova {
namespace Platform {

// Objective-C delegate class
@interface NovaLocationDelegate : NSObject <CLLocationManagerDelegate>
@property (nonatomic, assign) IOSLocationService* service;
@end

@implementation NovaLocationDelegate

- (void)locationManager:(CLLocationManager *)manager
     didUpdateLocations:(NSArray<CLLocation *> *)locations {
    if (!_service || locations.count == 0) return;

    CLLocation* location = locations.lastObject;

    BOOL isSimulated = NO;
    if (@available(iOS 15.0, *)) {
        isSimulated = location.sourceInformation.isSimulatedBySoftware;
    }

    _service->OnLocationUpdate(
        location.coordinate.latitude,
        location.coordinate.longitude,
        location.altitude,
        location.horizontalAccuracy,
        location.verticalAccuracy,
        location.speed,
        location.course,
        static_cast<int64_t>([location.timestamp timeIntervalSince1970] * 1000),
        isSimulated
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
    if (@available(iOS 14.0, *)) {
        status = manager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    _service->OnAuthorizationChange(static_cast<int>(status));
}

// Legacy authorization callback for iOS < 14
- (void)locationManager:(CLLocationManager *)manager
didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    if (!_service) return;
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

- (void)locationManager:(CLLocationManager *)manager
              didVisit:(CLVisit *)visit {
    if (!_service) return;

    int64_t arrivalTime = visit.arrivalDate == [NSDate distantPast] ? 0 :
        static_cast<int64_t>([visit.arrivalDate timeIntervalSince1970] * 1000);
    int64_t departureTime = visit.departureDate == [NSDate distantFuture] ? 0 :
        static_cast<int64_t>([visit.departureDate timeIntervalSince1970] * 1000);

    _service->OnVisit(
        visit.coordinate.latitude,
        visit.coordinate.longitude,
        arrivalTime,
        departureTime
    );
}

- (void)locationManager:(CLLocationManager *)manager
       didUpdateHeading:(CLHeading *)newHeading {
    if (!_service) return;

    _service->OnHeadingUpdate(
        newHeading.magneticHeading,
        newHeading.trueHeading,
        newHeading.headingAccuracy
    );
}

- (void)locationManager:(CLLocationManager *)manager
        didRangeBeacons:(NSArray<CLBeacon *> *)beacons
               inRegion:(CLBeaconRegion *)region {
    if (!_service) return;

    for (CLBeacon* beacon in beacons) {
        NSString* uuid = beacon.proximityUUID.UUIDString;
        _service->OnBeaconRanged(
            [uuid UTF8String],
            beacon.major.intValue,
            beacon.minor.intValue,
            beacon.accuracy,
            static_cast<int>(beacon.proximity)
        );
    }
}

@end

// C++ implementation

IOSLocationService::IOSLocationService() {
    @autoreleasepool {
        m_locationManager = [[CLLocationManager alloc] init];
        m_delegate = [[NovaLocationDelegate alloc] init];
        m_delegate.service = this;
        m_locationManager.delegate = m_delegate;

        // Default settings
        m_locationManager.desiredAccuracy = kCLLocationAccuracyBest;
        m_locationManager.distanceFilter = kCLDistanceFilterNone;

        NSLog(@"IOSLocationService initialized");
    }
}

IOSLocationService::~IOSLocationService() {
    StopUpdates();
    StopSignificantLocationChanges();
    StopMonitoringAllRegions();
    StopVisitMonitoring();
    StopHeadingUpdates();

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

    NSLog(@"IOSLocationService destroyed");
}

bool IOSLocationService::RequestPermission(bool alwaysAccess) {
    @autoreleasepool {
        if (!m_locationManager) return false;

        if (alwaysAccess) {
            [m_locationManager requestAlwaysAuthorization];
        } else {
            [m_locationManager requestWhenInUseAuthorization];
        }

        NSLog(@"Requested location permission (always=%d)", alwaysAccess);
        return true;
    }
}

bool IOSLocationService::HasPermission() const {
    CLAuthorizationStatus status;
    if (@available(iOS 14.0, *)) {
        status = m_locationManager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    return status == kCLAuthorizationStatusAuthorizedWhenInUse ||
           status == kCLAuthorizationStatusAuthorizedAlways;
}

LocationAuthorizationStatus IOSLocationService::GetAuthorizationStatus() const {
    CLAuthorizationStatus status;
    if (@available(iOS 14.0, *)) {
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
            return LocationAuthorizationStatus::AuthorizedAlways;
        case kCLAuthorizationStatusAuthorizedWhenInUse:
            return LocationAuthorizationStatus::AuthorizedWhenInUse;
        default:
            return LocationAuthorizationStatus::NotDetermined;
    }
}

void IOSLocationService::SetAuthorizationCallback(AuthorizationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authCallback = std::move(callback);
}

void IOSLocationService::StartUpdates(LocationCallback callback) {
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

void IOSLocationService::StopUpdates() {
    if (!m_updating) return;

    @autoreleasepool {
        [m_locationManager stopUpdatingLocation];
    }

    m_updating = false;
    NSLog(@"Location updates stopped");
}

bool IOSLocationService::IsUpdating() const {
    return m_updating;
}

void IOSLocationService::RequestSingleUpdate(LocationCallback callback,
                                               LocationErrorCallback errorCallback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
        m_errorCallback = std::move(errorCallback);
    }

    @autoreleasepool {
        UpdateAccuracySettings();
        [m_locationManager requestLocation];
    }

    NSLog(@"Single location update requested");
}

LocationData IOSLocationService::GetLastKnown() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_lastLocation.IsValid()) {
        return m_lastLocation;
    }

    // Try to get from system
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

bool IOSLocationService::IsHighAccuracyAvailable() const {
    return [CLLocationManager locationServicesEnabled] && HasPermission();
}

void IOSLocationService::SetDesiredAccuracy(LocationAccuracy accuracy) {
    m_desiredAccuracy = accuracy;
    if (m_updating) {
        UpdateAccuracySettings();
    }
}

LocationAccuracy IOSLocationService::GetDesiredAccuracy() const {
    return m_desiredAccuracy;
}

void IOSLocationService::SetDistanceFilter(double meters) {
    m_distanceFilter = meters;
    @autoreleasepool {
        m_locationManager.distanceFilter = meters > 0 ? meters : kCLDistanceFilterNone;
    }
}

void IOSLocationService::SetUpdateInterval(int64_t milliseconds) {
    // iOS doesn't support explicit update intervals
    // The system determines update frequency based on accuracy and distance filter
    // We can influence this indirectly through pausesLocationUpdatesAutomatically
    @autoreleasepool {
        m_locationManager.pausesLocationUpdatesAutomatically = (milliseconds > 5000);
    }
}

bool IOSLocationService::IsBackgroundLocationAvailable() const {
    CLAuthorizationStatus status;
    if (@available(iOS 14.0, *)) {
        status = m_locationManager.authorizationStatus;
    } else {
        status = [CLLocationManager authorizationStatus];
    }

    return status == kCLAuthorizationStatusAuthorizedAlways;
}

void IOSLocationService::SetBackgroundUpdatesEnabled(bool enable) {
    m_backgroundEnabled = enable;
    @autoreleasepool {
        m_locationManager.allowsBackgroundLocationUpdates = enable;

        if (enable) {
            // Keep running in background
            m_locationManager.pausesLocationUpdatesAutomatically = NO;
        }
    }
}

void IOSLocationService::StartSignificantLocationChanges(LocationCallback callback) {
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

void IOSLocationService::StopSignificantLocationChanges() {
    if (!m_significantChanges) return;

    @autoreleasepool {
        [m_locationManager stopMonitoringSignificantLocationChanges];
    }

    m_significantChanges = false;
    NSLog(@"Significant location changes monitoring stopped");
}

bool IOSLocationService::IsGeofencingSupported() const {
    return [CLLocationManager isMonitoringAvailableForClass:[CLCircularRegion class]];
}

bool IOSLocationService::StartMonitoringRegion(const GeofenceRegion& region,
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

void IOSLocationService::StopMonitoringRegion(const std::string& identifier) {
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

void IOSLocationService::StopMonitoringAllRegions() {
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

std::vector<GeofenceRegion> IOSLocationService::GetMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredRegions;
}

bool IOSLocationService::IsActivityRecognitionAvailable() const {
    return [CMMotionActivityManager isActivityAvailable];
}

void IOSLocationService::StartActivityUpdates(ActivityCallback callback) {
    // iOS doesn't have activity recognition in CoreLocation
    // We estimate based on speed or use CoreMotion
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activityCallback = std::move(callback);
    }

    // For simplicity, we'll estimate activity from speed in location updates
    // A more complete implementation would use CMMotionActivityManager
    NSLog(@"Activity updates started (speed-based estimation)");
}

void IOSLocationService::StopActivityUpdates() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activityCallback = nullptr;
    }
    NSLog(@"Activity updates stopped");
}

std::string IOSLocationService::GetServiceName() const {
    return "iOS Core Location";
}

bool IOSLocationService::AreLocationServicesEnabled() const {
    return [CLLocationManager locationServicesEnabled];
}

void IOSLocationService::OpenLocationSettings() {
    @autoreleasepool {
        NSURL* url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
        if (url && [[UIApplication sharedApplication] canOpenURL:url]) {
            [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
        }
    }
}

bool IOSLocationService::AreMockLocationsAllowed() const {
    // iOS 15+ can detect simulated locations
    // Prior versions can't detect mock locations reliably
    return true;
}

void IOSLocationService::SetRejectMockLocations(bool reject) {
    m_rejectMockLocations = reject;
}

void IOSLocationService::SetErrorCallback(LocationErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = std::move(callback);
}

std::string IOSLocationService::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// === iOS-specific features ===

void IOSLocationService::StartVisitMonitoring(
    std::function<void(const LocationData&, bool isDeparture)> callback) {
    if (m_visitMonitoring) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_visitCallback = std::move(callback);
    }

    @autoreleasepool {
        [m_locationManager startMonitoringVisits];
    }

    m_visitMonitoring = true;
    NSLog(@"Visit monitoring started");
}

void IOSLocationService::StopVisitMonitoring() {
    if (!m_visitMonitoring) return;

    @autoreleasepool {
        [m_locationManager stopMonitoringVisits];
    }

    m_visitMonitoring = false;
    NSLog(@"Visit monitoring stopped");
}

void IOSLocationService::StartHeadingUpdates(
    std::function<void(double heading, double accuracy)> callback) {
    if (m_headingUpdates) return;

    @autoreleasepool {
        if (![CLLocationManager headingAvailable]) {
            NSLog(@"Heading not available");
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_headingCallback = std::move(callback);
    }

    @autoreleasepool {
        [m_locationManager startUpdatingHeading];
    }

    m_headingUpdates = true;
    NSLog(@"Heading updates started");
}

void IOSLocationService::StopHeadingUpdates() {
    if (!m_headingUpdates) return;

    @autoreleasepool {
        [m_locationManager stopUpdatingHeading];
    }

    m_headingUpdates = false;
    NSLog(@"Heading updates stopped");
}

void IOSLocationService::StartBeaconRanging(const std::string& uuid,
    std::function<void(const std::string& uuid, int major, int minor, double distance)> callback) {

    @autoreleasepool {
        NSUUID* beaconUUID = [[NSUUID alloc] initWithUUIDString:
            [NSString stringWithUTF8String:uuid.c_str()]];

        if (!beaconUUID) {
            NSLog(@"Invalid beacon UUID: %s", uuid.c_str());
            return;
        }

        CLBeaconRegion* region;
        if (@available(iOS 13.0, *)) {
            CLBeaconIdentityConstraint* constraint = [[CLBeaconIdentityConstraint alloc]
                initWithUUID:beaconUUID];
            region = [[CLBeaconRegion alloc] initWithBeaconIdentityConstraint:constraint
                identifier:[NSString stringWithUTF8String:uuid.c_str()]];
            [m_locationManager startRangingBeaconsSatisfyingConstraint:constraint];
        } else {
            region = [[CLBeaconRegion alloc] initWithProximityUUID:beaconUUID
                identifier:[NSString stringWithUTF8String:uuid.c_str()]];
            [m_locationManager startRangingBeaconsInRegion:region];
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_beaconCallbacks[uuid] = std::move(callback);
    }

    NSLog(@"Beacon ranging started for UUID: %s", uuid.c_str());
}

void IOSLocationService::StopBeaconRanging(const std::string& uuid) {
    @autoreleasepool {
        NSUUID* beaconUUID = [[NSUUID alloc] initWithUUIDString:
            [NSString stringWithUTF8String:uuid.c_str()]];

        if (!beaconUUID) return;

        if (@available(iOS 13.0, *)) {
            CLBeaconIdentityConstraint* constraint = [[CLBeaconIdentityConstraint alloc]
                initWithUUID:beaconUUID];
            [m_locationManager stopRangingBeaconsSatisfyingConstraint:constraint];
        } else {
            CLBeaconRegion* region = [[CLBeaconRegion alloc] initWithProximityUUID:beaconUUID
                identifier:[NSString stringWithUTF8String:uuid.c_str()]];
            [m_locationManager stopRangingBeaconsInRegion:region];
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_beaconCallbacks.erase(uuid);
    }

    NSLog(@"Beacon ranging stopped for UUID: %s", uuid.c_str());
}

// === Delegate callbacks ===

void IOSLocationService::OnLocationUpdate(double latitude, double longitude, double altitude,
                                            double hAccuracy, double vAccuracy, double speed,
                                            double course, int64_t timestamp, bool isSimulated) {
    // Check for mock/simulated location
    if (m_rejectMockLocations && isSimulated) {
        NSLog(@"Rejecting simulated location");
        if (m_errorCallback) {
            m_errorCallback(LocationError::MockLocationDetected, "Simulated location detected");
        }
        return;
    }

    LocationData data;
    data.coordinate.latitude = latitude;
    data.coordinate.longitude = longitude;
    data.altitude = altitude;
    data.horizontalAccuracy = hAccuracy;
    data.verticalAccuracy = vAccuracy;
    data.speed = speed >= 0 ? speed : -1;
    data.course = course >= 0 ? course : -1;
    data.timestamp = timestamp;
    data.isMockLocation = isSimulated;
    data.provider = "CoreLocation";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastLocation = data;
    }

    // Notify callbacks
    LocationCallback locationCallback;
    LocationCallback significantCallback;
    ActivityCallback activityCallback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        locationCallback = m_locationCallback;
        significantCallback = m_significantCallback;
        activityCallback = m_activityCallback;
    }

    if (locationCallback) {
        locationCallback(data);
    }
    if (significantCallback) {
        significantCallback(data);
    }

    // Estimate activity from speed
    if (activityCallback && speed >= 0) {
        ActivityData activity;
        activity.type = EstimateActivityFromSpeed(speed);
        activity.confidence = 0.7f; // Speed-based estimation is moderately confident
        activity.timestamp = timestamp;
        activityCallback(activity);
    }
}

void IOSLocationService::OnAuthorizationChange(int status) {
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
                authStatus = LocationAuthorizationStatus::AuthorizedAlways;
                break;
            case kCLAuthorizationStatusAuthorizedWhenInUse:
                authStatus = LocationAuthorizationStatus::AuthorizedWhenInUse;
                break;
            default:
                authStatus = LocationAuthorizationStatus::NotDetermined;
                break;
        }
        callback(authStatus);
    }
}

void IOSLocationService::OnLocationError(int errorCode, const std::string& message) {
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

void IOSLocationService::OnRegionEnter(const std::string& identifier) {
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

void IOSLocationService::OnRegionExit(const std::string& identifier) {
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

void IOSLocationService::OnVisit(double latitude, double longitude,
                                   int64_t arrivalTime, int64_t departureTime) {
    NSLog(@"Visit at (%.6f, %.6f)", latitude, longitude);

    std::function<void(const LocationData&, bool)> callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_visitCallback;
    }

    if (callback) {
        LocationData data;
        data.coordinate.latitude = latitude;
        data.coordinate.longitude = longitude;
        data.timestamp = departureTime > 0 ? departureTime : arrivalTime;
        data.provider = "Visit";

        bool isDeparture = (departureTime > 0);
        callback(data, isDeparture);
    }
}

void IOSLocationService::OnHeadingUpdate(double magneticHeading, double trueHeading,
                                           double accuracy) {
    std::function<void(double, double)> callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_headingCallback;
    }

    if (callback) {
        double heading = (trueHeading >= 0) ? trueHeading : magneticHeading;
        callback(heading, accuracy);
    }
}

void IOSLocationService::OnBeaconRanged(const std::string& uuid, int major, int minor,
                                          double accuracy, int proximity) {
    std::function<void(const std::string&, int, int, double)> callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_beaconCallbacks.find(uuid);
        if (it != m_beaconCallbacks.end()) {
            callback = it->second;
        }
    }

    if (callback) {
        callback(uuid, major, minor, accuracy);
    }
}

// === Private helpers ===

void IOSLocationService::UpdateAccuracySettings() {
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

ActivityType IOSLocationService::EstimateActivityFromSpeed(double speed) const {
    if (speed < 0) return ActivityType::Unknown;
    if (speed < 0.5) return ActivityType::Stationary;  // < 1.8 km/h
    if (speed < 2.5) return ActivityType::Walking;     // < 9 km/h
    if (speed < 5.0) return ActivityType::Running;     // < 18 km/h
    if (speed < 12.0) return ActivityType::Cycling;    // < 43 km/h
    return ActivityType::Automotive;
}

} // namespace Platform
} // namespace Nova
