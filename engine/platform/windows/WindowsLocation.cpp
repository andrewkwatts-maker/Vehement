/**
 * @file WindowsLocation.cpp
 * @brief Windows location service implementation
 */

#include "WindowsLocation.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// WinRT headers for location (Windows 10+)
#ifdef WINRT_ENABLED
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Geolocation.h>
#include <winrt/Windows.Devices.Geolocation.Geofencing.h>
using namespace winrt;
using namespace Windows::Devices::Geolocation;
using namespace Windows::Devices::Geolocation::Geofencing;
#endif
#endif

#include <iostream>
#include <sstream>
#include <chrono>

namespace Nova {
namespace Platform {

WindowsLocationService::WindowsLocationService() {
    std::cout << "[WindowsLocation] Service created" << std::endl;
    InitializeWinRT();
}

WindowsLocationService::~WindowsLocationService() {
    StopUpdates();
    ShutdownWinRT();
    std::cout << "[WindowsLocation] Service destroyed" << std::endl;
}

void WindowsLocationService::InitializeWinRT() {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        winrt::init_apartment();
        Geolocator geolocator;
        m_geolocator = new Geolocator(geolocator);
        m_initialized = true;
        std::cout << "[WindowsLocation] WinRT initialized" << std::endl;
    } catch (const winrt::hresult_error& ex) {
        std::wcerr << L"[WindowsLocation] WinRT init failed: " << ex.message().c_str() << std::endl;
        m_useIPFallback = true;
    }
#else
    // Fallback to IP geolocation on older Windows or when WinRT not available
    m_useIPFallback = true;
    m_initialized = true;
    std::cout << "[WindowsLocation] Using IP geolocation fallback" << std::endl;
#endif
}

void WindowsLocationService::ShutdownWinRT() {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    if (m_geolocator) {
        delete static_cast<Geolocator*>(m_geolocator);
        m_geolocator = nullptr;
    }
#endif
}

bool WindowsLocationService::RequestPermission(bool alwaysAccess) {
    std::cout << "[WindowsLocation] Permission requested" << std::endl;

#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        if (geolocator) {
            auto status = geolocator->RequestAccessAsync().get();

            AuthorizationCallback callback;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                callback = m_authCallback;
            }

            if (callback) {
                LocationAuthorizationStatus authStatus;
                switch (status) {
                    case GeolocationAccessStatus::Allowed:
                        authStatus = LocationAuthorizationStatus::AuthorizedAlways;
                        break;
                    case GeolocationAccessStatus::Denied:
                        authStatus = LocationAuthorizationStatus::Denied;
                        break;
                    default:
                        authStatus = LocationAuthorizationStatus::NotDetermined;
                        break;
                }
                callback(authStatus);
            }
            return status == GeolocationAccessStatus::Allowed;
        }
    } catch (...) {
        // Fall through
    }
#endif

    // IP fallback always has permission
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

bool WindowsLocationService::HasPermission() const {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        auto status = Geolocator::RequestAccessAsync().get();
        return status == GeolocationAccessStatus::Allowed;
    } catch (...) {
        return true; // Assume permission for IP fallback
    }
#else
    return true;
#endif
}

LocationAuthorizationStatus WindowsLocationService::GetAuthorizationStatus() const {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        auto status = Geolocator::RequestAccessAsync().get();
        switch (status) {
            case GeolocationAccessStatus::Allowed:
                return LocationAuthorizationStatus::AuthorizedAlways;
            case GeolocationAccessStatus::Denied:
                return LocationAuthorizationStatus::Denied;
            default:
                return LocationAuthorizationStatus::NotDetermined;
        }
    } catch (...) {
        // Fall through
    }
#endif
    return LocationAuthorizationStatus::AuthorizedAlways;
}

void WindowsLocationService::SetAuthorizationCallback(AuthorizationCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_authCallback = std::move(callback);
}

void WindowsLocationService::StartUpdates(LocationCallback callback) {
    if (m_updating) {
        std::cout << "[WindowsLocation] Already receiving updates" << std::endl;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
    }

    m_stopRequested = false;
    m_updating = true;

    m_updateThread = std::thread([this]() {
        while (!m_stopRequested) {
            if (m_useIPFallback) {
                IPGeolocationFallback();
            } else {
                UpdateThread();
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            m_updateCondition.wait_for(lock, std::chrono::milliseconds(m_updateInterval),
                [this]() { return m_stopRequested.load(); });
        }
    });

    std::cout << "[WindowsLocation] Location updates started" << std::endl;
}

void WindowsLocationService::StopUpdates() {
    if (!m_updating) return;

    m_stopRequested = true;
    m_updateCondition.notify_all();

    if (m_updateThread.joinable()) {
        m_updateThread.join();
    }

    m_updating = false;
    std::cout << "[WindowsLocation] Location updates stopped" << std::endl;
}

bool WindowsLocationService::IsUpdating() const {
    return m_updating;
}

void WindowsLocationService::RequestSingleUpdate(LocationCallback callback,
                                                   LocationErrorCallback errorCallback) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_locationCallback = std::move(callback);
        m_errorCallback = std::move(errorCallback);
    }

    if (m_useIPFallback) {
        IPGeolocationFallback();
    } else {
        UpdateThread();
    }
}

LocationData WindowsLocationService::GetLastKnown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastLocation;
}

bool WindowsLocationService::IsHighAccuracyAvailable() const {
    return !m_useIPFallback && HasPermission();
}

void WindowsLocationService::SetDesiredAccuracy(LocationAccuracy accuracy) {
    m_desiredAccuracy = accuracy;

#if defined(_WIN32) && defined(WINRT_ENABLED)
    if (m_geolocator) {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        switch (accuracy) {
            case LocationAccuracy::Best:
            case LocationAccuracy::BestForNavigation:
            case LocationAccuracy::NearestTenMeters:
                geolocator->DesiredAccuracy(PositionAccuracy::High);
                break;
            default:
                geolocator->DesiredAccuracy(PositionAccuracy::Default);
                break;
        }
    }
#endif
}

LocationAccuracy WindowsLocationService::GetDesiredAccuracy() const {
    return m_desiredAccuracy;
}

void WindowsLocationService::SetDistanceFilter(double meters) {
    m_distanceFilter = meters;

#if defined(_WIN32) && defined(WINRT_ENABLED)
    if (m_geolocator) {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        geolocator->MovementThreshold(meters);
    }
#endif
}

void WindowsLocationService::SetUpdateInterval(int64_t milliseconds) {
    m_updateInterval = milliseconds;

#if defined(_WIN32) && defined(WINRT_ENABLED)
    if (m_geolocator) {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        geolocator->ReportInterval(static_cast<uint32_t>(milliseconds));
    }
#endif
}

bool WindowsLocationService::IsBackgroundLocationAvailable() const {
    return true; // Desktop apps can run in background
}

void WindowsLocationService::SetBackgroundUpdatesEnabled(bool enable) {
    // No-op on Windows desktop
}

void WindowsLocationService::StartSignificantLocationChanges(LocationCallback callback) {
    SetDistanceFilter(500.0);
    StartUpdates(std::move(callback));
}

void WindowsLocationService::StopSignificantLocationChanges() {
    StopUpdates();
    SetDistanceFilter(0.0);
}

bool WindowsLocationService::IsGeofencingSupported() const {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    return true;
#else
    return true; // Software geofencing always available
#endif
}

bool WindowsLocationService::StartMonitoringRegion(const GeofenceRegion& region,
                                                     GeofenceCallback callback) {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        BasicGeoposition position;
        position.Latitude = region.center.latitude;
        position.Longitude = region.center.longitude;
        position.Altitude = 0;

        Geocircle circle(position, region.radiusMeters);

        auto states = static_cast<MonitoredGeofenceStates>(0);
        if (region.notifyOnEntry) {
            states = states | MonitoredGeofenceStates::Entered;
        }
        if (region.notifyOnExit) {
            states = states | MonitoredGeofenceStates::Exited;
        }

        Geofence geofence(winrt::to_hstring(region.identifier), circle, states,
            false, Windows::Foundation::TimeSpan{0});

        GeofenceMonitor::Current().Geofences().Append(geofence);
    } catch (const winrt::hresult_error& ex) {
        std::wcerr << L"[WindowsLocation] Failed to add geofence: "
                   << ex.message().c_str() << std::endl;
        // Fall through to software implementation
    }
#endif

    // Software geofencing
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_geofenceCallbacks[region.identifier] = std::move(callback);
        m_monitoredRegions.push_back(region);
        m_regionState[region.identifier] = false;
    }

    std::cout << "[WindowsLocation] Started monitoring region: " << region.identifier << std::endl;
    return true;
}

void WindowsLocationService::StopMonitoringRegion(const std::string& identifier) {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        auto& geofences = GeofenceMonitor::Current().Geofences();
        for (uint32_t i = 0; i < geofences.Size(); ++i) {
            if (winrt::to_string(geofences.GetAt(i).Id()) == identifier) {
                geofences.RemoveAt(i);
                break;
            }
        }
    } catch (...) {
        // Fall through
    }
#endif

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
}

void WindowsLocationService::StopMonitoringAllRegions() {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        GeofenceMonitor::Current().Geofences().Clear();
    } catch (...) {
        // Fall through
    }
#endif

    std::lock_guard<std::mutex> lock(m_mutex);
    m_geofenceCallbacks.clear();
    m_regionState.clear();
    m_monitoredRegions.clear();
}

std::vector<GeofenceRegion> WindowsLocationService::GetMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_monitoredRegions;
}

bool WindowsLocationService::IsActivityRecognitionAvailable() const {
    return false; // Not available on Windows
}

void WindowsLocationService::StartActivityUpdates(ActivityCallback callback) {
    // Not implemented
}

void WindowsLocationService::StopActivityUpdates() {
    // Not implemented
}

std::string WindowsLocationService::GetServiceName() const {
    if (m_useIPFallback) {
        return "Windows IP Geolocation";
    }
    return "Windows Location Service";
}

bool WindowsLocationService::AreLocationServicesEnabled() const {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        if (geolocator) {
            return geolocator->LocationStatus() != PositionStatus::Disabled;
        }
    } catch (...) {
        // Fall through
    }
#endif
    return true; // IP fallback always available
}

void WindowsLocationService::OpenLocationSettings() {
#ifdef _WIN32
    // Open Windows Settings > Privacy > Location
    ShellExecuteA(nullptr, "open", "ms-settings:privacy-location", nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

bool WindowsLocationService::AreMockLocationsAllowed() const {
    return true; // No restriction on Windows
}

void WindowsLocationService::SetRejectMockLocations(bool reject) {
    m_rejectMockLocations = reject;
}

void WindowsLocationService::SetErrorCallback(LocationErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = std::move(callback);
}

std::string WindowsLocationService::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void WindowsLocationService::UpdateThread() {
#if defined(_WIN32) && defined(WINRT_ENABLED)
    try {
        Geolocator* geolocator = static_cast<Geolocator*>(m_geolocator);
        if (!geolocator) return;

        auto asyncOp = geolocator->GetGeopositionAsync();
        auto geoposition = asyncOp.get();
        auto coord = geoposition.Coordinate();

        LocationData data;
        data.coordinate.latitude = coord.Point().Position().Latitude;
        data.coordinate.longitude = coord.Point().Position().Longitude;
        data.altitude = coord.Point().Position().Altitude;

        if (coord.Accuracy()) {
            data.horizontalAccuracy = coord.Accuracy().Value();
        }
        if (coord.AltitudeAccuracy()) {
            data.verticalAccuracy = coord.AltitudeAccuracy().Value();
        }
        if (coord.Speed()) {
            data.speed = coord.Speed().Value();
        }
        if (coord.Heading()) {
            data.course = coord.Heading().Value();
        }

        data.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            coord.Timestamp().time_since_epoch()).count();
        data.provider = "Windows Location";

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
    } catch (const winrt::hresult_error& ex) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = winrt::to_string(ex.message());

        LocationErrorCallback errorCallback;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            errorCallback = m_errorCallback;
        }
        if (errorCallback) {
            errorCallback(LocationError::Unknown, m_lastError);
        }
    }
#else
    IPGeolocationFallback();
#endif
}

void WindowsLocationService::IPGeolocationFallback() {
#ifdef _WIN32
    try {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }

        // Create socket
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("Socket creation failed");
        }

        // Resolve host
        struct addrinfo hints = {}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo("ip-api.com", "80", &hints, &result) != 0) {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Failed to resolve host");
        }

        // Connect
        if (connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
            freeaddrinfo(result);
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("Connection failed");
        }
        freeaddrinfo(result);

        // Set timeout
        DWORD timeout = 5000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));

        // Send request
        const char* request =
            "GET /json HTTP/1.1\r\n"
            "Host: ip-api.com\r\n"
            "User-Agent: NovaEngine/1.0\r\n"
            "Accept: application/json\r\n"
            "Connection: close\r\n\r\n";

        send(sock, request, static_cast<int>(strlen(request)), 0);

        // Receive response
        std::string response;
        char buffer[4096];
        int bytesRead;
        while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }

        closesocket(sock);
        WSACleanup();

        // Parse JSON
        size_t jsonStart = response.find("\r\n\r\n");
        if (jsonStart == std::string::npos) {
            throw std::runtime_error("Invalid response");
        }
        std::string json = response.substr(jsonStart + 4);

        auto extractDouble = [&json](const std::string& key) -> double {
            size_t pos = json.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0.0;
            pos = json.find(':', pos);
            if (pos == std::string::npos) return 0.0;
            pos++;
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
            size_t end = pos;
            while (end < json.length() && (isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
            if (end > pos) {
                return std::stod(json.substr(pos, end - pos));
            }
            return 0.0;
        };

        LocationData data;
        data.coordinate.latitude = extractDouble("lat");
        data.coordinate.longitude = extractDouble("lon");
        data.horizontalAccuracy = 5000.0;
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
        std::cerr << "[WindowsLocation] " << m_lastError << std::endl;
    }
#endif
}

void WindowsLocationService::CheckGeofences(const LocationData& location) {
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

    for (const auto& [region, callback] : toNotify) {
        bool isInside = region.ContainsPoint(location.coordinate);
        GeofenceEvent event = isInside ? GeofenceEvent::Enter : GeofenceEvent::Exit;
        callback(region, event);
    }
}

} // namespace Platform
} // namespace Nova
