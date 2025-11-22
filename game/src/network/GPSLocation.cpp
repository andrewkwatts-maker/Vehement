#include "GPSLocation.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <nlohmann/json.hpp>

// Include engine logger if available
#if __has_include("../../engine/core/Logger.hpp")
#include "../../engine/core/Logger.hpp"
#define GPS_LOG_INFO(...) NOVA_LOG_INFO(__VA_ARGS__)
#define GPS_LOG_WARN(...) NOVA_LOG_WARN(__VA_ARGS__)
#define GPS_LOG_ERROR(...) NOVA_LOG_ERROR(__VA_ARGS__)
#else
#include <iostream>
#define GPS_LOG_INFO(...) std::cout << "[GPS] " << __VA_ARGS__ << std::endl
#define GPS_LOG_WARN(...) std::cerr << "[GPS WARN] " << __VA_ARGS__ << std::endl
#define GPS_LOG_ERROR(...) std::cerr << "[GPS ERROR] " << __VA_ARGS__ << std::endl
#endif

namespace Vehement {

// ==================== TownInfo ====================

std::string TownInfo::GenerateTownId(const std::string& name,
                                      const std::string& countryCode,
                                      const std::string& postalCode) {
    std::string id;

    // Convert name to lowercase and replace spaces/special chars with dashes
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '-') {
            if (!id.empty() && id.back() != '-') {
                id += '-';
            }
        }
    }

    // Remove trailing dashes
    while (!id.empty() && id.back() == '-') {
        id.pop_back();
    }

    // Add country code
    if (!countryCode.empty()) {
        std::string lowerCountry = countryCode;
        std::transform(lowerCountry.begin(), lowerCountry.end(), lowerCountry.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        id += "-" + lowerCountry;
    }

    // Add postal code if available
    if (!postalCode.empty()) {
        id += "-" + postalCode;
    }

    return id;
}

// ==================== GPSLocation ====================

GPSLocation& GPSLocation::Instance() {
    static GPSLocation instance;
    return instance;
}

void GPSLocation::Initialize(const GeocodingConfig& config) {
    m_geocodingConfig = config;

    // Register default stub provider if no providers registered
    if (m_providers.empty()) {
        RegisterProvider(std::make_unique<StubLocationProvider>());
    }

    GPS_LOG_INFO("GPSLocation initialized");
}

void GPSLocation::RegisterProvider(std::unique_ptr<ILocationProvider> provider) {
    if (provider) {
        GPS_LOG_INFO("Registered location provider: " + provider->GetName());
        m_providers.push_back(std::move(provider));
    }
}

void GPSLocation::GetCurrentLocation(LocationCallback callback,
                                      LocationErrorCallback errorCallback) {
    if (!callback) {
        return;
    }

    // Check for manual override
    if (m_useManualLocation) {
        m_lastLocation = m_manualLocation;
        m_status = Status::Success;
        callback(m_manualLocation);
        return;
    }

    // Try providers in order
    if (m_providers.empty()) {
        m_status = Status::Failed;
        if (errorCallback) {
            errorCallback(Status::Failed, "No location providers available");
        }
        return;
    }

    m_status = Status::Requesting;
    TryNextProvider(0, callback, errorCallback);
}

void GPSLocation::TryNextProvider(size_t index, LocationCallback callback,
                                   LocationErrorCallback errorCallback) {
    if (index >= m_providers.size()) {
        // All providers failed
        m_status = Status::Failed;
        if (errorCallback) {
            errorCallback(Status::Failed, "All location providers failed");
        }
        return;
    }

    auto& provider = m_providers[index];
    if (!provider->IsAvailable()) {
        // Skip unavailable providers
        TryNextProvider(index + 1, callback, errorCallback);
        return;
    }

    GPS_LOG_INFO("Trying location provider: " + provider->GetName());

    provider->RequestLocation([this, index, callback, errorCallback](std::optional<GPSCoordinates> coords) {
        if (coords && coords->IsValid()) {
            m_lastLocation = *coords;
            m_status = Status::Success;
            GPS_LOG_INFO("Location acquired: " + std::to_string(coords->latitude) + ", " +
                        std::to_string(coords->longitude));
            callback(*coords);
        } else {
            // Try next provider
            TryNextProvider(index + 1, callback, errorCallback);
        }
    });
}

void GPSLocation::GetTownFromCoordinates(GPSCoordinates coords, TownCallback callback) {
    if (!callback) {
        return;
    }

    if (!coords.IsValid()) {
        callback(CreateDefaultTown(coords));
        return;
    }

    // In a real implementation, this would make an HTTP request to a geocoding API
    // For the stub, we generate a reasonable default based on coordinates

    // Stub implementation: create a synthetic town based on coordinates
    TownInfo town;

    // Round coordinates to create "grid" of towns
    double latGrid = std::round(coords.latitude * 10) / 10;
    double lonGrid = std::round(coords.longitude * 10) / 10;

    // Generate synthetic town name
    std::string latDir = (latGrid >= 0) ? "N" : "S";
    std::string lonDir = (lonGrid >= 0) ? "E" : "W";

    std::ostringstream nameStream;
    nameStream << "Town " << latDir << std::abs(static_cast<int>(latGrid * 10))
               << lonDir << std::abs(static_cast<int>(lonGrid * 10));
    town.townName = nameStream.str();

    // Generate ID
    std::ostringstream idStream;
    idStream << "town-" << static_cast<int>(std::abs(latGrid * 100)) << "-"
             << static_cast<int>(std::abs(lonGrid * 100));
    town.townId = idStream.str();

    // Determine rough region based on coordinates
    if (coords.latitude >= -45 && coords.latitude <= -10 &&
        coords.longitude >= 110 && coords.longitude <= 155) {
        town.country = "Australia";
        town.countryCode = "AU";
        town.region = "Unknown State";
    } else if (coords.latitude >= 24 && coords.latitude <= 50 &&
               coords.longitude >= -125 && coords.longitude <= -66) {
        town.country = "United States";
        town.countryCode = "US";
        town.region = "Unknown State";
    } else if (coords.latitude >= 49 && coords.latitude <= 60 &&
               coords.longitude >= -10 && coords.longitude <= 2) {
        town.country = "United Kingdom";
        town.countryCode = "GB";
        town.region = "Unknown Region";
    } else {
        town.country = "Unknown";
        town.countryCode = "XX";
        town.region = "Unknown";
    }

    town.center = coords;
    town.radiusKm = 5.0f;

    // Re-generate proper ID
    town.townId = TownInfo::GenerateTownId(town.townName, town.countryCode, "");

    m_lastTown = town;

    GPS_LOG_INFO("Geocoded location to: " + town.townName + " (" + town.townId + ")");
    callback(town);
}

void GPSLocation::GetLocationFromIP(LocationCallback callback) {
    if (!callback) {
        return;
    }

    // Find and use IP geolocation provider
    for (auto& provider : m_providers) {
        if (provider->GetName() == "IPGeolocation" && provider->IsAvailable()) {
            provider->RequestLocation([callback](std::optional<GPSCoordinates> coords) {
                if (coords && coords->IsValid()) {
                    callback(*coords);
                } else {
                    // Fallback to default location
                    callback({-37.8136, 144.9631}); // Melbourne
                }
            });
            return;
        }
    }

    // No IP provider available, use stub
    GPS_LOG_WARN("No IP geolocation provider, using default location");
    callback({-37.8136, 144.9631}); // Melbourne as default
}

void GPSLocation::SetManualLocation(GPSCoordinates coords) {
    m_useManualLocation = true;
    m_manualLocation = coords;
    m_lastLocation = coords;
    GPS_LOG_INFO("Manual location set: " + std::to_string(coords.latitude) + ", " +
                std::to_string(coords.longitude));
}

bool GPSLocation::IsLocationAvailable() const {
    for (const auto& provider : m_providers) {
        if (provider->IsAvailable()) {
            return true;
        }
    }
    return m_useManualLocation;
}

void GPSLocation::ClearCache() {
    m_lastLocation = {};
    m_lastTown = {};
    m_useManualLocation = false;
    GPS_LOG_INFO("Location cache cleared");
}

TownInfo GPSLocation::CreateDefaultTown(GPSCoordinates coords) {
    TownInfo town;
    town.townId = "unknown-town";
    town.townName = "Unknown Town";
    town.country = "Unknown";
    town.countryCode = "XX";
    town.center = coords.IsValid() ? coords : GPSCoordinates{0, 0};
    town.radiusKm = 10.0f;
    return town;
}

void GPSLocation::ParseGeocodingResponse(const std::string& json, TownCallback callback) {
    try {
        auto data = nlohmann::json::parse(json);

        TownInfo town;

        // Parse OpenStreetMap Nominatim response format
        if (data.contains("address")) {
            auto& addr = data["address"];

            town.townName = addr.value("city",
                           addr.value("town",
                           addr.value("village",
                           addr.value("municipality", "Unknown"))));

            town.region = addr.value("state", addr.value("county", ""));
            town.country = addr.value("country", "Unknown");
            town.countryCode = addr.value("country_code", "XX");

            // Convert country code to uppercase
            std::transform(town.countryCode.begin(), town.countryCode.end(),
                          town.countryCode.begin(), ::toupper);

            town.postalCode = addr.value("postcode", "");
        }

        if (data.contains("lat") && data.contains("lon")) {
            town.center.latitude = std::stod(data["lat"].get<std::string>());
            town.center.longitude = std::stod(data["lon"].get<std::string>());
        }

        town.townId = TownInfo::GenerateTownId(town.townName, town.countryCode, town.postalCode);
        town.radiusKm = 5.0f;

        m_lastTown = town;
        callback(town);

    } catch (const std::exception& e) {
        GPS_LOG_ERROR("Failed to parse geocoding response: " + std::string(e.what()));
        callback(CreateDefaultTown(m_lastLocation));
    }
}

std::string GPSLocation::SanitizeTownName(const std::string& name) {
    std::string sanitized;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-') {
            sanitized += c;
        }
    }
    return sanitized;
}

// ==================== StubLocationProvider ====================

StubLocationProvider::StubLocationProvider(GPSCoordinates defaultLocation)
    : m_location(defaultLocation) {
}

void StubLocationProvider::RequestLocation(
    std::function<void(std::optional<GPSCoordinates>)> callback) {
    if (callback) {
        // Simulate slight delay in real scenario, but return immediately in stub
        callback(m_location);
    }
}

// ==================== IPGeolocationProvider ====================

IPGeolocationProvider::IPGeolocationProvider(const Config& config)
    : m_config(config) {
}

void IPGeolocationProvider::RequestLocation(
    std::function<void(std::optional<GPSCoordinates>)> callback) {
    if (!callback) {
        return;
    }

    // In a real implementation, this would make an HTTP request to ip-api.com or similar
    // For the stub, we return a default location

    // Stub: Return Melbourne coordinates as if detected from IP
    callback(GPSCoordinates{-37.8136, 144.9631});
}

bool IPGeolocationProvider::IsAvailable() const {
    // In real implementation, check for network connectivity
    return true;
}

// ==================== PlatformLocationProvider ====================

// Forward declare platform service creation
namespace Nova { namespace Platform {
    std::unique_ptr<ILocationService> CreateLocationService();
}}

class PlatformLocationProvider::Impl {
public:
    std::unique_ptr<Nova::Platform::ILocationService> service;
    std::function<void(GPSCoordinates)> continuousCallback;
    bool mockDetected = false;
    bool rejectMock = false;

    Impl() {
        service = Nova::Platform::CreateLocationService();
    }

    GPSCoordinates ConvertLocation(const Nova::Platform::LocationData& data) {
        GPSCoordinates coords;
        coords.latitude = data.coordinate.latitude;
        coords.longitude = data.coordinate.longitude;
        return coords;
    }
};

PlatformLocationProvider::PlatformLocationProvider()
    : m_impl(std::make_unique<Impl>()) {
    GPS_LOG_INFO("PlatformLocationProvider created");
}

PlatformLocationProvider::~PlatformLocationProvider() {
    StopContinuousUpdates();
}

void PlatformLocationProvider::RequestLocation(
    std::function<void(std::optional<GPSCoordinates>)> callback) {

    if (!callback) return;

    if (!m_impl->service) {
        GPS_LOG_WARN("No platform location service available");
        callback(std::nullopt);
        return;
    }

    m_impl->service->RequestSingleUpdate(
        [this, callback](const Nova::Platform::LocationData& location) {
            if (location.IsValid()) {
                // Check for mock location
                if (location.isMockLocation) {
                    m_impl->mockDetected = true;
                    if (m_impl->rejectMock) {
                        GPS_LOG_WARN("Rejecting mock location");
                        callback(std::nullopt);
                        return;
                    }
                }

                GPSCoordinates coords = m_impl->ConvertLocation(location);
                GPS_LOG_INFO("Platform location received: " + std::to_string(coords.latitude) +
                            ", " + std::to_string(coords.longitude));
                callback(coords);
            } else {
                callback(std::nullopt);
            }
        },
        [callback](Nova::Platform::LocationError error, const std::string& message) {
            GPS_LOG_ERROR("Platform location error: " + message);
            callback(std::nullopt);
        }
    );
}

bool PlatformLocationProvider::IsAvailable() const {
    return m_impl->service && m_impl->service->AreLocationServicesEnabled();
}

std::string PlatformLocationProvider::GetName() const {
    if (m_impl->service) {
        return "Platform (" + m_impl->service->GetServiceName() + ")";
    }
    return "Platform (unavailable)";
}

bool PlatformLocationProvider::RequestPermission(bool alwaysAccess) {
    if (!m_impl->service) return false;
    return m_impl->service->RequestPermission(alwaysAccess);
}

bool PlatformLocationProvider::HasPermission() const {
    if (!m_impl->service) return false;
    return m_impl->service->HasPermission();
}

void PlatformLocationProvider::StartContinuousUpdates(
    std::function<void(GPSCoordinates)> callback) {

    if (!m_impl->service || !callback) return;

    m_impl->continuousCallback = callback;

    m_impl->service->StartUpdates([this](const Nova::Platform::LocationData& location) {
        if (!location.IsValid()) return;

        if (location.isMockLocation) {
            m_impl->mockDetected = true;
            if (m_impl->rejectMock) return;
        }

        if (m_impl->continuousCallback) {
            GPSCoordinates coords = m_impl->ConvertLocation(location);
            m_impl->continuousCallback(coords);
        }
    });

    GPS_LOG_INFO("Platform continuous updates started");
}

void PlatformLocationProvider::StopContinuousUpdates() {
    if (m_impl->service && m_impl->service->IsUpdating()) {
        m_impl->service->StopUpdates();
        GPS_LOG_INFO("Platform continuous updates stopped");
    }
    m_impl->continuousCallback = nullptr;
}

bool PlatformLocationProvider::IsUpdating() const {
    return m_impl->service && m_impl->service->IsUpdating();
}

bool PlatformLocationProvider::IsMockLocationDetected() const {
    return m_impl->mockDetected;
}

void PlatformLocationProvider::SetRejectMockLocations(bool reject) {
    m_impl->rejectMock = reject;
    if (m_impl->service) {
        m_impl->service->SetRejectMockLocations(reject);
    }
}

std::string PlatformLocationProvider::GetPlatformServiceName() const {
    if (m_impl->service) {
        return m_impl->service->GetServiceName();
    }
    return "None";
}

} // namespace Vehement

// ==================== Platform Service Factory ====================

namespace Nova {
namespace Platform {

std::unique_ptr<ILocationService> CreateLocationService() {
#if defined(__ANDROID__)
    return std::make_unique<AndroidLocationService>();
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        return std::make_unique<IOSLocationService>();
    #else
        return std::make_unique<MacOSLocationService>();
    #endif
#elif defined(_WIN32)
    return std::make_unique<WindowsLocationService>();
#elif defined(__linux__)
    return std::make_unique<LinuxLocationService>();
#else
    // Fallback - return nullptr or a stub implementation
    return nullptr;
#endif
}

// LocationServiceManager implementation
LocationServiceManager& LocationServiceManager::Instance() {
    static LocationServiceManager instance;
    return instance;
}

void LocationServiceManager::Initialize() {
    if (m_service) return;
    m_service = CreateLocationService();
}

void LocationServiceManager::Shutdown() {
    m_service.reset();
}

} // namespace Platform
} // namespace Nova
