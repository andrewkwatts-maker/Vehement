/**
 * @file LinuxLocation.cpp
 * @brief Linux desktop location service implementation
 */

#include "LinuxLocation.hpp"

// GeoClue2 D-Bus interface (optional)
#ifdef HAVE_GEOCLUE
#include <gio/gio.h>
#endif

// GPSD client library (optional)
#ifdef HAVE_GPSD
#include <gps.h>
#endif

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdlib>

// HTTP client for IP geolocation (simple implementation)
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace Nova {
namespace Platform {

// GeoClue2 D-Bus interface name
static const char* GEOCLUE_BUS_NAME = "org.freedesktop.GeoClue2";
static const char* GEOCLUE_MANAGER_PATH = "/org/freedesktop/GeoClue2/Manager";
static const char* GEOCLUE_MANAGER_INTERFACE = "org.freedesktop.GeoClue2.Manager";
static const char* GEOCLUE_CLIENT_INTERFACE = "org.freedesktop.GeoClue2.Client";
static const char* GEOCLUE_LOCATION_INTERFACE = "org.freedesktop.GeoClue2.Location";

LinuxLocationService::LinuxLocationService() {
    std::cout << "[LinuxLocation] Service created" << std::endl;

    // Try to initialize GeoClue2 first
    if (InitializeGeoClue()) {
        m_activeProvider = ProviderType::GeoClue2;
        std::cout << "[LinuxLocation] Using GeoClue2 provider" << std::endl;
    }
    // Fall back to GPSD
    else if (InitializeGPSD()) {
        m_activeProvider = ProviderType::GPSD;
        std::cout << "[LinuxLocation] Using GPSD provider" << std::endl;
    }
    // Fall back to IP geolocation
    else {
        m_activeProvider = ProviderType::IPBased;
        std::cout << "[LinuxLocation] Using IP-based geolocation" << std::endl;
    }

    m_initialized = true;
}

LinuxLocationService::~LinuxLocationService() {
    StopUpdates();
    ShutdownGeoClue();
    ShutdownGPSD();
    std::cout << "[LinuxLocation] Service destroyed" << std::endl;
}

bool LinuxLocationService::InitializeGeoClue() {
#ifdef HAVE_GEOCLUE
    GError* error = nullptr;

    // Connect to system bus
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (!connection) {
        if (error) {
            std::cerr << "[LinuxLocation] Failed to connect to D-Bus: " << error->message << std::endl;
            g_error_free(error);
        }
        return false;
    }
    m_dbusConnection = connection;

    // Create a client
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        GEOCLUE_BUS_NAME,
        GEOCLUE_MANAGER_PATH,
        GEOCLUE_MANAGER_INTERFACE,
        "CreateClient",
        nullptr,
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (!result) {
        if (error) {
            std::cerr << "[LinuxLocation] Failed to create GeoClue client: " << error->message << std::endl;
            g_error_free(error);
        }
        return false;
    }

    const gchar* clientPath;
    g_variant_get(result, "(&o)", &clientPath);
    std::cout << "[LinuxLocation] GeoClue client created: " << clientPath << std::endl;

    // Store client path for later use
    m_geoClueClient = g_strdup(clientPath);
    g_variant_unref(result);

    return true;
#else
    return false;
#endif
}

void LinuxLocationService::ShutdownGeoClue() {
#ifdef HAVE_GEOCLUE
    if (m_geoClueClient) {
        g_free(m_geoClueClient);
        m_geoClueClient = nullptr;
    }
    if (m_dbusConnection) {
        g_object_unref(m_dbusConnection);
        m_dbusConnection = nullptr;
    }
#endif
}

bool LinuxLocationService::InitializeGPSD() {
#ifdef HAVE_GPSD
    struct gps_data_t* gpsData = new struct gps_data_t;

    if (gps_open(m_gpsdHost.c_str(), std::to_string(m_gpsdPort).c_str(), gpsData) != 0) {
        std::cerr << "[LinuxLocation] Failed to connect to GPSD at "
                  << m_gpsdHost << ":" << m_gpsdPort << std::endl;
        delete gpsData;
        return false;
    }

    gps_stream(gpsData, WATCH_ENABLE | WATCH_JSON, nullptr);
    m_gpsdData = gpsData;
    m_gpsdConnected = true;

    std::cout << "[LinuxLocation] Connected to GPSD at "
              << m_gpsdHost << ":" << m_gpsdPort << std::endl;
    return true;
#else
    return false;
#endif
}

void LinuxLocationService::ShutdownGPSD() {
#ifdef HAVE_GPSD
    if (m_gpsdData) {
        struct gps_data_t* gpsData = static_cast<struct gps_data_t*>(m_gpsdData);
        gps_stream(gpsData, WATCH_DISABLE, nullptr);
        gps_close(gpsData);
        delete gpsData;
        m_gpsdData = nullptr;
        m_gpsdConnected = false;
    }
#endif
}

bool LinuxLocationService::RequestPermission(bool alwaysAccess) {
    // Linux desktop typically doesn't have the same permission model
    // GeoClue2 uses its own authorization mechanism
    std::cout << "[LinuxLocation] Permission request (Linux uses GeoClue authorization)" << std::endl;

    AuthorizationCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callback = m_authCallback;
    }

    if (callback) {
        callback(LocationAuthorizationStatus::AuthorizedAlways);
    }

    return true;
}

bool LinuxLocationService::HasPermission() const {
    // On Linux, we assume permission is granted if the service is available
    return IsGeoClueAvailable() || IsGPSDAvailable() || true; // IP fallback always available
}

LocationAuthorizationStatus LinuxLocationService::GetAuthorizationStatus() const {
    if (HasPermission()) {
        return LocationAuthorizationStatus::AuthorizedAlways;
    }
    return LocationAuthorizationStatus::Denied;
}

void LinuxLocationService::SetAuthorizationCallback(AuthorizationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authCallback = std::move(callback);
}

void LinuxLocationService::StartUpdates(LocationCallback callback) {
    if (m_updating) {
        std::cout << "[LinuxLocation] Already receiving updates" << std::endl;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
    }

    m_stopRequested = false;
    m_updating = true;

    // Start update thread based on active provider
    m_updateThread = std::thread([this]() {
        while (!m_stopRequested) {
            switch (m_activeProvider) {
                case ProviderType::GeoClue2:
                    GeoClueUpdateThread();
                    break;
                case ProviderType::GPSD:
                    GPSDUpdateThread();
                    break;
                case ProviderType::IPBased:
                    IPGeolocationUpdate();
                    break;
                case ProviderType::Manual:
                    if (m_useManualLocation) {
                        LocationCallback cb;
                        {
                            std::lock_guard<std::mutex> lock(m_mutex);
                            cb = m_locationCallback;
                            m_lastLocation = m_manualLocation;
                        }
                        if (cb) cb(m_manualLocation);
                        CheckGeofences(m_manualLocation);
                    }
                    break;
            }

            // Wait for next update interval
            std::unique_lock<std::mutex> lock(m_mutex);
            m_updateCondition.wait_for(lock, std::chrono::milliseconds(m_updateInterval),
                [this]() { return m_stopRequested.load(); });
        }
    });

    std::cout << "[LinuxLocation] Location updates started" << std::endl;
}

void LinuxLocationService::StopUpdates() {
    if (!m_updating) return;

    m_stopRequested = true;
    m_updateCondition.notify_all();

    if (m_updateThread.joinable()) {
        m_updateThread.join();
    }

    m_updating = false;
    std::cout << "[LinuxLocation] Location updates stopped" << std::endl;
}

bool LinuxLocationService::IsUpdating() const {
    return m_updating;
}

void LinuxLocationService::RequestSingleUpdate(LocationCallback callback,
                                                 LocationErrorCallback errorCallback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
        m_errorCallback = std::move(errorCallback);
    }

    // Perform a single update based on active provider
    switch (m_activeProvider) {
        case ProviderType::GeoClue2:
            GeoClueUpdateThread();
            break;
        case ProviderType::GPSD:
            GPSDUpdateThread();
            break;
        case ProviderType::IPBased:
            IPGeolocationUpdate();
            break;
        case ProviderType::Manual:
            if (m_useManualLocation) {
                LocationCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    cb = m_locationCallback;
                }
                if (cb) cb(m_manualLocation);
            }
            break;
    }
}

LocationData LinuxLocationService::GetLastKnown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastLocation;
}

bool LinuxLocationService::IsHighAccuracyAvailable() const {
    return IsGPSDAvailable(); // GPS hardware provides best accuracy
}

void LinuxLocationService::SetDesiredAccuracy(LocationAccuracy accuracy) {
    m_desiredAccuracy = accuracy;

#ifdef HAVE_GEOCLUE
    if (m_dbusConnection && m_geoClueClient) {
        // Set accuracy level in GeoClue2
        guint32 accuracyLevel;
        switch (accuracy) {
            case LocationAccuracy::Best:
            case LocationAccuracy::BestForNavigation:
            case LocationAccuracy::NearestTenMeters:
                accuracyLevel = 8; // EXACT
                break;
            case LocationAccuracy::HundredMeters:
                accuracyLevel = 6; // STREET
                break;
            case LocationAccuracy::Kilometer:
                accuracyLevel = 4; // NEIGHBORHOOD
                break;
            case LocationAccuracy::ThreeKilometers:
                accuracyLevel = 2; // CITY
                break;
            default:
                accuracyLevel = 6;
                break;
        }

        GDBusConnection* connection = static_cast<GDBusConnection*>(m_dbusConnection);
        const gchar* clientPath = static_cast<const gchar*>(m_geoClueClient);

        g_dbus_connection_call_sync(
            connection,
            GEOCLUE_BUS_NAME,
            clientPath,
            "org.freedesktop.DBus.Properties",
            "Set",
            g_variant_new("(ssv)",
                GEOCLUE_CLIENT_INTERFACE,
                "RequestedAccuracyLevel",
                g_variant_new_uint32(accuracyLevel)
            ),
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            nullptr
        );
    }
#endif
}

LocationAccuracy LinuxLocationService::GetDesiredAccuracy() const {
    return m_desiredAccuracy;
}

void LinuxLocationService::SetDistanceFilter(double meters) {
    m_distanceFilter = meters;
}

void LinuxLocationService::SetUpdateInterval(int64_t milliseconds) {
    m_updateInterval = milliseconds;
}

bool LinuxLocationService::IsBackgroundLocationAvailable() const {
    return true; // Desktop apps can always run in background
}

void LinuxLocationService::SetBackgroundUpdatesEnabled(bool enable) {
    // No-op on Linux desktop - apps always run in background
}

void LinuxLocationService::StartSignificantLocationChanges(LocationCallback callback) {
    // Use regular updates with larger distance filter
    SetDistanceFilter(500.0); // 500m threshold for "significant" change
    StartUpdates(std::move(callback));
}

void LinuxLocationService::StopSignificantLocationChanges() {
    StopUpdates();
    SetDistanceFilter(0.0);
}

bool LinuxLocationService::IsGeofencingSupported() const {
    return true; // Software geofencing is always available
}

bool LinuxLocationService::StartMonitoringRegion(const GeofenceRegion& region,
                                                   GeofenceCallback callback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks[region.identifier] = std::move(callback);
        m_monitoredRegions.push_back(region);
        m_regionState[region.identifier] = false; // Initially outside
    }

    std::cout << "[LinuxLocation] Started monitoring region: " << region.identifier << std::endl;
    return true;
}

void LinuxLocationService::StopMonitoringRegion(const std::string& identifier) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks.erase(identifier);
        m_regionState.erase(identifier);
        m_monitoredRegions.erase(
            std::remove_if(m_monitoredRegions.begin(), m_monitoredRegions.end(),
                [&identifier](const GeofenceRegion& r) {
                    return r.identifier == identifier;
                }),
            m_monitoredRegions.end());
    }

    std::cout << "[LinuxLocation] Stopped monitoring region: " << identifier << std::endl;
}

void LinuxLocationService::StopMonitoringAllRegions() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_geofenceCallbacks.clear();
    m_regionState.clear();
    m_monitoredRegions.clear();
}

std::vector<GeofenceRegion> LinuxLocationService::GetMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredRegions;
}

bool LinuxLocationService::IsActivityRecognitionAvailable() const {
    return false; // Not available on desktop Linux
}

void LinuxLocationService::StartActivityUpdates(ActivityCallback callback) {
    // Not implemented for Linux desktop
}

void LinuxLocationService::StopActivityUpdates() {
    // Not implemented for Linux desktop
}

std::string LinuxLocationService::GetServiceName() const {
    switch (m_activeProvider) {
        case ProviderType::GeoClue2: return "Linux GeoClue2";
        case ProviderType::GPSD: return "Linux GPSD";
        case ProviderType::IPBased: return "Linux IP Geolocation";
        case ProviderType::Manual: return "Linux Manual Location";
        default: return "Linux Location Service";
    }
}

bool LinuxLocationService::AreLocationServicesEnabled() const {
    return IsGeoClueAvailable() || IsGPSDAvailable() || true;
}

void LinuxLocationService::OpenLocationSettings() {
    // Try to open GNOME settings or KDE settings
    int result = system("gnome-control-center privacy 2>/dev/null || "
                        "systemsettings5 kcm_privacy 2>/dev/null || "
                        "xdg-open /etc/geoclue/geoclue.conf 2>/dev/null");
    (void)result;
}

bool LinuxLocationService::AreMockLocationsAllowed() const {
    return true; // No restriction on Linux desktop
}

void LinuxLocationService::SetRejectMockLocations(bool reject) {
    m_rejectMockLocations = reject;
}

void LinuxLocationService::SetErrorCallback(LocationErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = std::move(callback);
}

std::string LinuxLocationService::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

// === Linux-specific features ===

void LinuxLocationService::SetPreferredProvider(ProviderType type) {
    m_preferredProvider = type;

    switch (type) {
        case ProviderType::GeoClue2:
            if (IsGeoClueAvailable()) {
                m_activeProvider = ProviderType::GeoClue2;
            }
            break;
        case ProviderType::GPSD:
            if (IsGPSDAvailable() || InitializeGPSD()) {
                m_activeProvider = ProviderType::GPSD;
            }
            break;
        case ProviderType::IPBased:
            m_activeProvider = ProviderType::IPBased;
            break;
        case ProviderType::Manual:
            m_activeProvider = ProviderType::Manual;
            break;
    }
}

LinuxLocationService::ProviderType LinuxLocationService::GetActiveProvider() const {
    return m_activeProvider;
}

void LinuxLocationService::SetManualLocation(const LocationData& location) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_manualLocation = location;
    m_useManualLocation = true;
    m_lastLocation = location;
}

void LinuxLocationService::ConfigureGPSD(const std::string& host, int port) {
    m_gpsdHost = host;
    m_gpsdPort = port;

    // Reconnect if already connected
    if (m_gpsdConnected) {
        ShutdownGPSD();
        InitializeGPSD();
    }
}

void LinuxLocationService::ConfigureIPGeolocation(const std::string& apiUrl,
                                                    const std::string& apiKey) {
    m_ipApiUrl = apiUrl;
    m_ipApiKey = apiKey;
}

bool LinuxLocationService::IsGeoClueAvailable() const {
#ifdef HAVE_GEOCLUE
    return m_dbusConnection != nullptr && m_geoClueClient != nullptr;
#else
    return false;
#endif
}

bool LinuxLocationService::IsGPSDAvailable() const {
    return m_gpsdConnected;
}

// === Private implementation ===

void LinuxLocationService::GeoClueUpdateThread() {
#ifdef HAVE_GEOCLUE
    if (!m_dbusConnection || !m_geoClueClient) {
        return;
    }

    GError* error = nullptr;
    GDBusConnection* connection = static_cast<GDBusConnection*>(m_dbusConnection);
    const gchar* clientPath = static_cast<const gchar*>(m_geoClueClient);

    // Start client
    g_dbus_connection_call_sync(
        connection,
        GEOCLUE_BUS_NAME,
        clientPath,
        GEOCLUE_CLIENT_INTERFACE,
        "Start",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (error) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = error->message;
        g_error_free(error);
        return;
    }

    // Get location object path
    GVariant* result = g_dbus_connection_call_sync(
        connection,
        GEOCLUE_BUS_NAME,
        clientPath,
        "org.freedesktop.DBus.Properties",
        "Get",
        g_variant_new("(ss)", GEOCLUE_CLIENT_INTERFACE, "Location"),
        G_VARIANT_TYPE("(v)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (!result || error) {
        if (error) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = error->message;
            g_error_free(error);
        }
        return;
    }

    GVariant* locationVariant;
    g_variant_get(result, "(v)", &locationVariant);
    const gchar* locationPath = g_variant_get_string(locationVariant, nullptr);

    // Get location properties
    GVariant* props = g_dbus_connection_call_sync(
        connection,
        GEOCLUE_BUS_NAME,
        locationPath,
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", GEOCLUE_LOCATION_INTERFACE),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        &error
    );

    if (!props || error) {
        g_variant_unref(locationVariant);
        g_variant_unref(result);
        if (error) {
            g_error_free(error);
        }
        return;
    }

    LocationData data;
    GVariant* propsDict;
    g_variant_get(props, "(@a{sv})", &propsDict);

    GVariant* value;
    if ((value = g_variant_lookup_value(propsDict, "Latitude", G_VARIANT_TYPE_DOUBLE))) {
        data.coordinate.latitude = g_variant_get_double(value);
        g_variant_unref(value);
    }
    if ((value = g_variant_lookup_value(propsDict, "Longitude", G_VARIANT_TYPE_DOUBLE))) {
        data.coordinate.longitude = g_variant_get_double(value);
        g_variant_unref(value);
    }
    if ((value = g_variant_lookup_value(propsDict, "Altitude", G_VARIANT_TYPE_DOUBLE))) {
        data.altitude = g_variant_get_double(value);
        g_variant_unref(value);
    }
    if ((value = g_variant_lookup_value(propsDict, "Accuracy", G_VARIANT_TYPE_DOUBLE))) {
        data.horizontalAccuracy = g_variant_get_double(value);
        g_variant_unref(value);
    }
    if ((value = g_variant_lookup_value(propsDict, "Speed", G_VARIANT_TYPE_DOUBLE))) {
        data.speed = g_variant_get_double(value);
        g_variant_unref(value);
    }
    if ((value = g_variant_lookup_value(propsDict, "Heading", G_VARIANT_TYPE_DOUBLE))) {
        data.course = g_variant_get_double(value);
        g_variant_unref(value);
    }

    data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    data.provider = "GeoClue2";

    g_variant_unref(propsDict);
    g_variant_unref(props);
    g_variant_unref(locationVariant);
    g_variant_unref(result);

    // Check distance filter
    bool shouldReport = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_distanceFilter > 0 && m_lastLocation.IsValid()) {
            double distance = m_lastLocation.coordinate.DistanceTo(data.coordinate);
            shouldReport = (distance >= m_distanceFilter);
        }
        if (shouldReport) {
            m_lastLocation = data;
        }
    }

    if (shouldReport) {
        LocationCallback callback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            callback = m_locationCallback;
        }
        if (callback) {
            callback(data);
        }
        CheckGeofences(data);
    }
#endif
}

void LinuxLocationService::GPSDUpdateThread() {
#ifdef HAVE_GPSD
    if (!m_gpsdData) return;

    struct gps_data_t* gpsData = static_cast<struct gps_data_t*>(m_gpsdData);

    if (gps_waiting(gpsData, 1000)) {
        if (gps_read(gpsData, nullptr, 0) == -1) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_lastError = "Error reading from GPSD";
            return;
        }

        if (gpsData->fix.mode >= MODE_2D) {
            LocationData data;
            data.coordinate.latitude = gpsData->fix.latitude;
            data.coordinate.longitude = gpsData->fix.longitude;

            if (gpsData->fix.mode >= MODE_3D) {
                data.altitude = gpsData->fix.altitude;
                data.verticalAccuracy = gpsData->fix.epv;
            }

            data.horizontalAccuracy = gpsData->fix.eph;
            data.speed = gpsData->fix.speed;
            data.course = gpsData->fix.track;
            data.timestamp = static_cast<int64_t>(gpsData->fix.time.tv_sec * 1000 +
                                                   gpsData->fix.time.tv_nsec / 1000000);
            data.provider = "GPSD";

            // Check distance filter
            bool shouldReport = true;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_distanceFilter > 0 && m_lastLocation.IsValid()) {
                    double distance = m_lastLocation.coordinate.DistanceTo(data.coordinate);
                    shouldReport = (distance >= m_distanceFilter);
                }
                if (shouldReport) {
                    m_lastLocation = data;
                }
            }

            if (shouldReport) {
                LocationCallback callback;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    callback = m_locationCallback;
                }
                if (callback) {
                    callback(data);
                }
                CheckGeofences(data);
            }
        }
    }
#endif
}

void LinuxLocationService::IPGeolocationUpdate() {
    // Simple HTTP request to IP geolocation API
    // In production, use a proper HTTP library like libcurl

    try {
        // Parse URL
        std::string url = m_ipApiUrl;
        std::string host;
        std::string path = "/json";
        int port = 80;

        size_t protocolEnd = url.find("://");
        if (protocolEnd != std::string::npos) {
            url = url.substr(protocolEnd + 3);
        }

        size_t pathStart = url.find('/');
        if (pathStart != std::string::npos) {
            path = url.substr(pathStart);
            host = url.substr(0, pathStart);
        } else {
            host = url;
        }

        size_t portStart = host.find(':');
        if (portStart != std::string::npos) {
            port = std::stoi(host.substr(portStart + 1));
            host = host.substr(0, portStart);
        }

        // Create socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct hostent* server = gethostbyname(host.c_str());
        if (!server) {
            close(sock);
            throw std::runtime_error("Failed to resolve host: " + host);
        }

        struct sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        std::memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
        serverAddr.sin_port = htons(port);

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
            close(sock);
            throw std::runtime_error("Failed to connect to " + host);
        }

        // Send HTTP request
        std::ostringstream request;
        request << "GET " << path << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "User-Agent: NovaEngine/1.0\r\n";
        request << "Accept: application/json\r\n";
        request << "Connection: close\r\n\r\n";

        std::string requestStr = request.str();
        if (send(sock, requestStr.c_str(), requestStr.length(), 0) < 0) {
            close(sock);
            throw std::runtime_error("Failed to send request");
        }

        // Receive response
        std::string response;
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }
        close(sock);

        // Parse response (find JSON body)
        size_t jsonStart = response.find("\r\n\r\n");
        if (jsonStart == std::string::npos) {
            throw std::runtime_error("Invalid HTTP response");
        }
        std::string json = response.substr(jsonStart + 4);

        // Simple JSON parsing for ip-api.com format
        LocationData data;

        auto extractDouble = [&json](const std::string& key) -> double {
            size_t pos = json.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0.0;
            pos = json.find(':', pos);
            if (pos == std::string::npos) return 0.0;
            pos++;
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
            size_t end = pos;
            while (end < json.length() && (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
            if (end > pos) {
                return std::stod(json.substr(pos, end - pos));
            }
            return 0.0;
        };

        data.coordinate.latitude = extractDouble("lat");
        data.coordinate.longitude = extractDouble("lon");
        data.horizontalAccuracy = 5000.0; // IP geolocation is not very accurate (~5km)
        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        data.provider = "IP Geolocation";

        if (data.coordinate.IsValid()) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastLocation = data;
            }

            LocationCallback callback;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                callback = m_locationCallback;
            }
            if (callback) {
                callback(data);
            }
            CheckGeofences(data);
        }

    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = std::string("IP geolocation error: ") + e.what();
        std::cerr << "[LinuxLocation] " << m_lastError << std::endl;
    }
}

void LinuxLocationService::CheckGeofences(const LocationData& location) {
    std::vector<std::pair<GeofenceRegion, GeofenceCallback>> toNotify;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& region : m_monitoredRegions) {
            bool isInside = region.ContainsPoint(location.coordinate);
            bool wasInside = m_regionState[region.identifier];

            if (isInside != wasInside) {
                m_regionState[region.identifier] = isInside;

                auto it = m_geofenceCallbacks.find(region.identifier);
                if (it != m_geofenceCallbacks.end()) {
                    toNotify.emplace_back(region, it->second);
                }
            }
        }
    }

    // Notify outside lock to prevent deadlocks
    for (const auto& [region, callback] : toNotify) {
        bool isInside = region.ContainsPoint(location.coordinate);
        GeofenceEvent event = isInside ? GeofenceEvent::Enter : GeofenceEvent::Exit;
        callback(region, event);
    }
}

} // namespace Platform
} // namespace Nova
